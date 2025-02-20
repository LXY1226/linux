#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * SAS Transport Layer for MPT (Message Passing Technology) based controllers
 *
 * This code is based on drivers/scsi/mpt3sas/mpt3sas_transport.c
 * Copyright (C) 2012-2014  LSI Corporation
 * Copyright (C) 2013-2014 Avago Technologies
 *  (mailto: MPT-FusionLinux.pdl@avagotech.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * NO WARRANTY
 * THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
 * LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
 * solely responsible for determining the appropriateness of using and
 * distributing the Program and assumes all risks associated with its
 * exercise of rights under this Agreement, including but not limited to
 * the risks and costs of program errors, damage to or loss of data,
 * programs or equipment, and unavailability or interruption of operations.

 * DISCLAIMER OF LIABILITY
 * NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/pci.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_sas.h>
#include <scsi/scsi_dbg.h>

#include "mpt3sas_base.h"

/**
 * _transport_sas_node_find_by_sas_address - sas node search
 * @ioc: per adapter object
 * @sas_address: sas address of expander or sas host
 * Context: Calling function should acquire ioc->sas_node_lock.
 *
 * Search for either hba phys or expander device based on handle, then returns
 * the sas_node object.
 */
static struct _sas_node *
_transport_sas_node_find_by_sas_address(struct MPT3SAS_ADAPTER *ioc,
	u64 sas_address)
{
	if (ioc->sas_hba.sas_address == sas_address)
		return &ioc->sas_hba;
	else
		return mpt3sas_scsih_expander_find_by_sas_address(ioc,
		    sas_address);
}

/**
 * _transport_convert_phy_link_rate -
 * @link_rate: link rate returned from mpt firmware
 *
 * Convert link_rate from mpi fusion into sas_transport form.
 */
static enum sas_linkrate
_transport_convert_phy_link_rate(u8 link_rate)
{
	enum sas_linkrate rc;

	switch (link_rate) {
	case MPI2_SAS_NEG_LINK_RATE_1_5:
		rc = SAS_LINK_RATE_1_5_GBPS;
		break;
	case MPI2_SAS_NEG_LINK_RATE_3_0:
		rc = SAS_LINK_RATE_3_0_GBPS;
		break;
	case MPI2_SAS_NEG_LINK_RATE_6_0:
		rc = SAS_LINK_RATE_6_0_GBPS;
		break;
	case MPI25_SAS_NEG_LINK_RATE_12_0:
		rc = SAS_LINK_RATE_12_0_GBPS;
		break;
	case MPI2_SAS_NEG_LINK_RATE_PHY_DISABLED:
		rc = SAS_PHY_DISABLED;
		break;
	case MPI2_SAS_NEG_LINK_RATE_NEGOTIATION_FAILED:
		rc = SAS_LINK_RATE_FAILED;
		break;
	case MPI2_SAS_NEG_LINK_RATE_PORT_SELECTOR:
		rc = SAS_SATA_PORT_SELECTOR;
		break;
	case MPI2_SAS_NEG_LINK_RATE_SMP_RESET_IN_PROGRESS:
		rc = SAS_PHY_RESET_IN_PROGRESS;
		break;

	default:
	case MPI2_SAS_NEG_LINK_RATE_SATA_OOB_COMPLETE:
	case MPI2_SAS_NEG_LINK_RATE_UNKNOWN_LINK_RATE:
		rc = SAS_LINK_RATE_UNKNOWN;
		break;
	}
	return rc;
}

/**
 * _transport_set_identify - set identify for phys and end devices
 * @ioc: per adapter object
 * @handle: device handle
 * @identify: sas identify info
 *
 * Populates sas identify info.
 *
 * Returns 0 for success, non-zero for failure.
 */
static int
_transport_set_identify(struct MPT3SAS_ADAPTER *ioc, u16 handle,
	struct sas_identify *identify)
{
	Mpi2SasDevicePage0_t sas_device_pg0;
	Mpi2ConfigReply_t mpi_reply;
	u32 device_info;
	u32 ioc_status;

	if (ioc->shost_recovery || ioc->pci_error_recovery) {
		pr_info(MPT3SAS_FMT "%s: host reset in progress!\n",
		    __func__, ioc->name);
		return -EFAULT;
	}

	if ((mpt3sas_config_get_sas_device_pg0(ioc, &mpi_reply, &sas_device_pg0,
	    MPI2_SAS_DEVICE_PGAD_FORM_HANDLE, handle))) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return -ENXIO;
	}

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		pr_err(MPT3SAS_FMT
			"handle(0x%04x), ioc_status(0x%04x)\nfailure at %s:%d/%s()!\n",
			ioc->name, handle, ioc_status,
		     __FILE__, __LINE__, __func__);
		return -EIO;
	}

	memset(identify, 0, sizeof(struct sas_identify));
	device_info = le32_to_cpu(sas_device_pg0.DeviceInfo);

	/* sas_address */
	identify->sas_address = le64_to_cpu(sas_device_pg0.SASAddress);

	/* phy number of the parent device this device is linked to */
	identify->phy_identifier = sas_device_pg0.PhyNum;

	/* device_type */
	switch (device_info & MPI2_SAS_DEVICE_INFO_MASK_DEVICE_TYPE) {
	case MPI2_SAS_DEVICE_INFO_NO_DEVICE:
		identify->device_type = SAS_PHY_UNUSED;
		break;
	case MPI2_SAS_DEVICE_INFO_END_DEVICE:
		identify->device_type = SAS_END_DEVICE;
		break;
	case MPI2_SAS_DEVICE_INFO_EDGE_EXPANDER:
		identify->device_type = SAS_EDGE_EXPANDER_DEVICE;
		break;
	case MPI2_SAS_DEVICE_INFO_FANOUT_EXPANDER:
		identify->device_type = SAS_FANOUT_EXPANDER_DEVICE;
		break;
	}

	/* initiator_port_protocols */
	if (device_info & MPI2_SAS_DEVICE_INFO_SSP_INITIATOR)
		identify->initiator_port_protocols |= SAS_PROTOCOL_SSP;
	if (device_info & MPI2_SAS_DEVICE_INFO_STP_INITIATOR)
		identify->initiator_port_protocols |= SAS_PROTOCOL_STP;
	if (device_info & MPI2_SAS_DEVICE_INFO_SMP_INITIATOR)
		identify->initiator_port_protocols |= SAS_PROTOCOL_SMP;
	if (device_info & MPI2_SAS_DEVICE_INFO_SATA_HOST)
		identify->initiator_port_protocols |= SAS_PROTOCOL_SATA;

	/* target_port_protocols */
	if (device_info & MPI2_SAS_DEVICE_INFO_SSP_TARGET)
		identify->target_port_protocols |= SAS_PROTOCOL_SSP;
	if (device_info & MPI2_SAS_DEVICE_INFO_STP_TARGET)
		identify->target_port_protocols |= SAS_PROTOCOL_STP;
	if (device_info & MPI2_SAS_DEVICE_INFO_SMP_TARGET)
		identify->target_port_protocols |= SAS_PROTOCOL_SMP;
	if (device_info & MPI2_SAS_DEVICE_INFO_SATA_DEVICE)
		identify->target_port_protocols |= SAS_PROTOCOL_SATA;

	return 0;
}

/**
 * mpt3sas_transport_done -  internal transport layer callback handler.
 * @ioc: per adapter object
 * @smid: system request message index
 * @msix_index: MSIX table index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 *
 * Callback handler when sending internal generated transport cmds.
 * The callback index passed is `ioc->transport_cb_idx`
 *
 * Return 1 meaning mf should be freed from _base_interrupt
 *        0 means the mf is freed from this function.
 */
u8
mpt3sas_transport_done(struct MPT3SAS_ADAPTER *ioc, u16 smid, u8 msix_index,
	u32 reply)
{
	MPI2DefaultReply_t *mpi_reply;

	mpi_reply =  mpt3sas_base_get_reply_virt_addr(ioc, reply);
	if (ioc->transport_cmds.status == MPT3_CMD_NOT_USED)
		return 1;
	if (ioc->transport_cmds.smid != smid)
		return 1;
	ioc->transport_cmds.status |= MPT3_CMD_COMPLETE;
	if (mpi_reply) {
		memcpy(ioc->transport_cmds.reply, mpi_reply,
		    mpi_reply->MsgLength*4);
		ioc->transport_cmds.status |= MPT3_CMD_REPLY_VALID;
	}
	ioc->transport_cmds.status &= ~MPT3_CMD_PENDING;
	complete(&ioc->transport_cmds.done);
	return 1;
}

/* report manufacture request structure */
struct rep_manu_request {
	u8 smp_frame_type;
	u8 function;
	u8 reserved;
	u8 request_length;
};

/* report manufacture reply structure */
struct rep_manu_reply {
	u8 smp_frame_type; /* 0x41 */
	u8 function; /* 0x01 */
	u8 function_result;
	u8 response_length;
	u16 expander_change_count;
	u8 reserved0[2];
	u8 sas_format;
	u8 reserved2[3];
	u8 vendor_id[SAS_EXPANDER_VENDOR_ID_LEN];
	u8 product_id[SAS_EXPANDER_PRODUCT_ID_LEN];
	u8 product_rev[SAS_EXPANDER_PRODUCT_REV_LEN];
	u8 component_vendor_id[SAS_EXPANDER_COMPONENT_VENDOR_ID_LEN];
	u16 component_id;
	u8 component_revision_id;
	u8 reserved3;
	u8 vendor_specific[8];
};

/**
 * transport_expander_report_manufacture - obtain SMP report_manufacture
 * @ioc: per adapter object
 * @sas_address: expander sas address
 * @edev: the sas_expander_device object
 *
 * Fills in the sas_expander_device object when SMP port is created.
 *
 * Returns 0 for success, non-zero for failure.
 */
static int
_transport_expander_report_manufacture(struct MPT3SAS_ADAPTER *ioc,
	u64 sas_address, struct sas_expander_device *edev)
{
	Mpi2SmpPassthroughRequest_t *mpi_request;
	Mpi2SmpPassthroughReply_t *mpi_reply;
	struct rep_manu_reply *manufacture_reply;
	struct rep_manu_request *manufacture_request;
	int rc;
	u16 smid;
	u32 ioc_state;
	unsigned long timeleft;
	void *psge;
	u8 issue_reset = 0;
	void *data_out = NULL;
	dma_addr_t data_out_dma;
	dma_addr_t data_in_dma;
	size_t data_in_sz;
	size_t data_out_sz;
	u16 wait_state_count;

	if (ioc->shost_recovery || ioc->pci_error_recovery) {
		pr_info(MPT3SAS_FMT "%s: host reset in progress!\n",
		    __func__, ioc->name);
		return -EFAULT;
	}

	mutex_lock(&ioc->transport_cmds.mutex);

	if (ioc->transport_cmds.status != MPT3_CMD_NOT_USED) {
		pr_err(MPT3SAS_FMT "%s: transport_cmds in use\n",
		    ioc->name, __func__);
		rc = -EAGAIN;
		goto out;
	}
	ioc->transport_cmds.status = MPT3_CMD_PENDING;

	wait_state_count = 0;
	ioc_state = mpt3sas_base_get_iocstate(ioc, 1);
	while (ioc_state != MPI2_IOC_STATE_OPERATIONAL) {
		if (wait_state_count++ == 10) {
			pr_err(MPT3SAS_FMT
			    "%s: failed due to ioc not operational\n",
			    ioc->name, __func__);
			rc = -EFAULT;
			goto out;
		}
		ssleep(1);
		ioc_state = mpt3sas_base_get_iocstate(ioc, 1);
		pr_info(MPT3SAS_FMT
			"%s: waiting for operational state(count=%d)\n",
			ioc->name, __func__, wait_state_count);
	}
	if (wait_state_count)
		pr_info(MPT3SAS_FMT "%s: ioc is operational\n",
		    ioc->name, __func__);

	smid = mpt3sas_base_get_smid(ioc, ioc->transport_cb_idx);
	if (!smid) {
		pr_err(MPT3SAS_FMT "%s: failed obtaining a smid\n",
		    ioc->name, __func__);
		rc = -EAGAIN;
		goto out;
	}

	rc = 0;
	mpi_request = mpt3sas_base_get_msg_frame(ioc, smid);
	ioc->transport_cmds.smid = smid;

	data_out_sz = sizeof(struct rep_manu_request);
	data_in_sz = sizeof(struct rep_manu_reply);
	data_out = pci_alloc_consistent(ioc->pdev, data_out_sz + data_in_sz,
	    &data_out_dma);

	if (!data_out) {
		pr_err("failure at %s:%d/%s()!\n", __FILE__,
		    __LINE__, __func__);
		rc = -ENOMEM;
		mpt3sas_base_free_smid(ioc, smid);
		goto out;
	}

	data_in_dma = data_out_dma + sizeof(struct rep_manu_request);

	manufacture_request = data_out;
	manufacture_request->smp_frame_type = 0x40;
	manufacture_request->function = 1;
	manufacture_request->reserved = 0;
	manufacture_request->request_length = 0;

	memset(mpi_request, 0, sizeof(Mpi2SmpPassthroughRequest_t));
	mpi_request->Function = MPI2_FUNCTION_SMP_PASSTHROUGH;
	mpi_request->PhysicalPort = 0xFF;
	mpi_request->SASAddress = cpu_to_le64(sas_address);
	mpi_request->RequestDataLength = cpu_to_le16(data_out_sz);
	psge = &mpi_request->SGL;

	ioc->build_sg(ioc, psge, data_out_dma, data_out_sz, data_in_dma,
	    data_in_sz);

	dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		"report_manufacture - send to sas_addr(0x%016llx)\n",
		ioc->name, (unsigned long long)sas_address));
	init_completion(&ioc->transport_cmds.done);
	mpt3sas_base_put_smid_default(ioc, smid);
	timeleft = wait_for_completion_timeout(&ioc->transport_cmds.done,
	    10*HZ);

	if (!(ioc->transport_cmds.status & MPT3_CMD_COMPLETE)) {
		pr_err(MPT3SAS_FMT "%s: timeout\n",
		    ioc->name, __func__);
		_debug_dump_mf(mpi_request,
		    sizeof(Mpi2SmpPassthroughRequest_t)/4);
		if (!(ioc->transport_cmds.status & MPT3_CMD_RESET))
			issue_reset = 1;
		goto issue_host_reset;
	}

	dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		"report_manufacture - complete\n", ioc->name));

	if (ioc->transport_cmds.status & MPT3_CMD_REPLY_VALID) {
		u8 *tmp;

		mpi_reply = ioc->transport_cmds.reply;

		dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		    "report_manufacture - reply data transfer size(%d)\n",
		    ioc->name, le16_to_cpu(mpi_reply->ResponseDataLength)));

		if (le16_to_cpu(mpi_reply->ResponseDataLength) !=
		    sizeof(struct rep_manu_reply))
			goto out;

		manufacture_reply = data_out + sizeof(struct rep_manu_request);
		strncpy(edev->vendor_id, manufacture_reply->vendor_id,
		     SAS_EXPANDER_VENDOR_ID_LEN);
		strncpy(edev->product_id, manufacture_reply->product_id,
		     SAS_EXPANDER_PRODUCT_ID_LEN);
		strncpy(edev->product_rev, manufacture_reply->product_rev,
		     SAS_EXPANDER_PRODUCT_REV_LEN);
		edev->level = manufacture_reply->sas_format & 1;
		if (edev->level) {
			strncpy(edev->component_vendor_id,
			    manufacture_reply->component_vendor_id,
			     SAS_EXPANDER_COMPONENT_VENDOR_ID_LEN);
			tmp = (u8 *)&manufacture_reply->component_id;
			edev->component_id = tmp[0] << 8 | tmp[1];
			edev->component_revision_id =
			    manufacture_reply->component_revision_id;
		}
	} else
		dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		    "report_manufacture - no reply\n", ioc->name));

 issue_host_reset:
	if (issue_reset)
		mpt3sas_base_hard_reset_handler(ioc, CAN_SLEEP,
		    FORCE_BIG_HAMMER);
 out:
	ioc->transport_cmds.status = MPT3_CMD_NOT_USED;
	if (data_out)
		pci_free_consistent(ioc->pdev, data_out_sz + data_in_sz,
		    data_out, data_out_dma);

	mutex_unlock(&ioc->transport_cmds.mutex);
	return rc;
}


/**
 * _transport_delete_port - helper function to removing a port
 * @ioc: per adapter object
 * @mpt3sas_port: mpt3sas per port object
 *
 * Returns nothing.
 */
static void
_transport_delete_port(struct MPT3SAS_ADAPTER *ioc,
	struct _sas_port *mpt3sas_port)
{
	u64 sas_address = mpt3sas_port->remote_identify.sas_address;
	enum sas_device_type device_type =
	    mpt3sas_port->remote_identify.device_type;

	dev_printk(KERN_INFO, &mpt3sas_port->port->dev,
	    "remove: sas_addr(0x%016llx)\n",
	    (unsigned long long) sas_address);

	ioc->logging_level |= MPT_DEBUG_TRANSPORT;
	if (device_type == SAS_END_DEVICE)
		mpt3sas_device_remove_by_sas_address(ioc, sas_address);
	else if (device_type == SAS_EDGE_EXPANDER_DEVICE ||
	    device_type == SAS_FANOUT_EXPANDER_DEVICE)
		mpt3sas_expander_remove(ioc, sas_address);
	ioc->logging_level &= ~MPT_DEBUG_TRANSPORT;
}

/**
 * _transport_delete_phy - helper function to removing single phy from port
 * @ioc: per adapter object
 * @mpt3sas_port: mpt3sas per port object
 * @mpt3sas_phy: mpt3sas per phy object
 *
 * Returns nothing.
 */
static void
_transport_delete_phy(struct MPT3SAS_ADAPTER *ioc,
	struct _sas_port *mpt3sas_port, struct _sas_phy *mpt3sas_phy)
{
	u64 sas_address = mpt3sas_port->remote_identify.sas_address;

	dev_printk(KERN_INFO, &mpt3sas_phy->phy->dev,
	    "remove: sas_addr(0x%016llx), phy(%d)\n",
	    (unsigned long long) sas_address, mpt3sas_phy->phy_id);

	list_del(&mpt3sas_phy->port_siblings);
	mpt3sas_port->num_phys--;
	sas_port_delete_phy(mpt3sas_port->port, mpt3sas_phy->phy);
	mpt3sas_phy->phy_belongs_to_port = 0;
}

/**
 * _transport_add_phy - helper function to adding single phy to port
 * @ioc: per adapter object
 * @mpt3sas_port: mpt3sas per port object
 * @mpt3sas_phy: mpt3sas per phy object
 *
 * Returns nothing.
 */
static void
_transport_add_phy(struct MPT3SAS_ADAPTER *ioc, struct _sas_port *mpt3sas_port,
	struct _sas_phy *mpt3sas_phy)
{
	u64 sas_address = mpt3sas_port->remote_identify.sas_address;

	dev_printk(KERN_INFO, &mpt3sas_phy->phy->dev,
	    "add: sas_addr(0x%016llx), phy(%d)\n", (unsigned long long)
	    sas_address, mpt3sas_phy->phy_id);

	list_add_tail(&mpt3sas_phy->port_siblings, &mpt3sas_port->phy_list);
	mpt3sas_port->num_phys++;
	sas_port_add_phy(mpt3sas_port->port, mpt3sas_phy->phy);
	mpt3sas_phy->phy_belongs_to_port = 1;
}

/**
 * _transport_add_phy_to_an_existing_port - adding new phy to existing port
 * @ioc: per adapter object
 * @sas_node: sas node object (either expander or sas host)
 * @mpt3sas_phy: mpt3sas per phy object
 * @sas_address: sas address of device/expander were phy needs to be added to
 *
 * Returns nothing.
 */
static void
_transport_add_phy_to_an_existing_port(struct MPT3SAS_ADAPTER *ioc,
	struct _sas_node *sas_node, struct _sas_phy *mpt3sas_phy,
	u64 sas_address)
{
	struct _sas_port *mpt3sas_port;
	struct _sas_phy *phy_srch;

	if (mpt3sas_phy->phy_belongs_to_port == 1)
		return;

	list_for_each_entry(mpt3sas_port, &sas_node->sas_port_list,
	    port_list) {
		if (mpt3sas_port->remote_identify.sas_address !=
		    sas_address)
			continue;
		list_for_each_entry(phy_srch, &mpt3sas_port->phy_list,
		    port_siblings) {
			if (phy_srch == mpt3sas_phy)
				return;
		}
		_transport_add_phy(ioc, mpt3sas_port, mpt3sas_phy);
			return;
	}

}

/**
 * _transport_del_phy_from_an_existing_port - delete phy from existing port
 * @ioc: per adapter object
 * @sas_node: sas node object (either expander or sas host)
 * @mpt3sas_phy: mpt3sas per phy object
 *
 * Returns nothing.
 */
static void
_transport_del_phy_from_an_existing_port(struct MPT3SAS_ADAPTER *ioc,
	struct _sas_node *sas_node, struct _sas_phy *mpt3sas_phy)
{
	struct _sas_port *mpt3sas_port, *next;
	struct _sas_phy *phy_srch;

	if (mpt3sas_phy->phy_belongs_to_port == 0)
		return;

	list_for_each_entry_safe(mpt3sas_port, next, &sas_node->sas_port_list,
	    port_list) {
		list_for_each_entry(phy_srch, &mpt3sas_port->phy_list,
		    port_siblings) {
			if (phy_srch != mpt3sas_phy)
				continue;

			if (mpt3sas_port->num_phys == 1)
				_transport_delete_port(ioc, mpt3sas_port);
			else
				_transport_delete_phy(ioc, mpt3sas_port,
				    mpt3sas_phy);
			return;
		}
	}
}

/**
 * _transport_sanity_check - sanity check when adding a new port
 * @ioc: per adapter object
 * @sas_node: sas node object (either expander or sas host)
 * @sas_address: sas address of device being added
 *
 * See the explanation above from _transport_delete_duplicate_port
 */
static void
_transport_sanity_check(struct MPT3SAS_ADAPTER *ioc, struct _sas_node *sas_node,
	u64 sas_address)
{
	int i;

	for (i = 0; i < sas_node->num_phys; i++) {
		if (sas_node->phy[i].remote_identify.sas_address != sas_address)
			continue;
		if (sas_node->phy[i].phy_belongs_to_port == 1)
			_transport_del_phy_from_an_existing_port(ioc, sas_node,
			    &sas_node->phy[i]);
	}
}

/**
 * mpt3sas_transport_port_add - insert port to the list
 * @ioc: per adapter object
 * @handle: handle of attached device
 * @sas_address: sas address of parent expander or sas host
 * Context: This function will acquire ioc->sas_node_lock.
 *
 * Adding new port object to the sas_node->sas_port_list.
 *
 * Returns mpt3sas_port.
 */
struct _sas_port *
mpt3sas_transport_port_add(struct MPT3SAS_ADAPTER *ioc, u16 handle,
	u64 sas_address)
{
	struct _sas_phy *mpt3sas_phy, *next;
	struct _sas_port *mpt3sas_port;
	unsigned long flags;
	struct _sas_node *sas_node;
	struct sas_rphy *rphy;
	struct _sas_device *sas_device = NULL;
	int i;
	struct sas_port *port;

	mpt3sas_port = kzalloc(sizeof(struct _sas_port),
	    GFP_KERNEL);
	if (!mpt3sas_port) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return NULL;
	}

	INIT_LIST_HEAD(&mpt3sas_port->port_list);
	INIT_LIST_HEAD(&mpt3sas_port->phy_list);
	spin_lock_irqsave(&ioc->sas_node_lock, flags);
	sas_node = _transport_sas_node_find_by_sas_address(ioc, sas_address);
	spin_unlock_irqrestore(&ioc->sas_node_lock, flags);

	if (!sas_node) {
		pr_err(MPT3SAS_FMT
			"%s: Could not find parent sas_address(0x%016llx)!\n",
			ioc->name, __func__, (unsigned long long)sas_address);
		goto out_fail;
	}

	if ((_transport_set_identify(ioc, handle,
	    &mpt3sas_port->remote_identify))) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out_fail;
	}

	if (mpt3sas_port->remote_identify.device_type == SAS_PHY_UNUSED) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out_fail;
	}

	_transport_sanity_check(ioc, sas_node,
	    mpt3sas_port->remote_identify.sas_address);

	for (i = 0; i < sas_node->num_phys; i++) {
		if (sas_node->phy[i].remote_identify.sas_address !=
		    mpt3sas_port->remote_identify.sas_address)
			continue;
		list_add_tail(&sas_node->phy[i].port_siblings,
		    &mpt3sas_port->phy_list);
		mpt3sas_port->num_phys++;
	}

	if (!mpt3sas_port->num_phys) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out_fail;
	}

	port = sas_port_alloc_num(sas_node->parent_dev);
	if ((sas_port_add(port))) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out_fail;
	}

	list_for_each_entry(mpt3sas_phy, &mpt3sas_port->phy_list,
	    port_siblings) {
		if ((ioc->logging_level & MPT_DEBUG_TRANSPORT))
			dev_printk(KERN_INFO, &port->dev,
				"add: handle(0x%04x), sas_addr(0x%016llx), phy(%d)\n",
				handle, (unsigned long long)
			    mpt3sas_port->remote_identify.sas_address,
			    mpt3sas_phy->phy_id);
		sas_port_add_phy(port, mpt3sas_phy->phy);
		mpt3sas_phy->phy_belongs_to_port = 1;
	}

	mpt3sas_port->port = port;
	if (mpt3sas_port->remote_identify.device_type == SAS_END_DEVICE)
		rphy = sas_end_device_alloc(port);
	else
		rphy = sas_expander_alloc(port,
		    mpt3sas_port->remote_identify.device_type);

	rphy->identify = mpt3sas_port->remote_identify;

	if (mpt3sas_port->remote_identify.device_type == SAS_END_DEVICE) {
		sas_device = mpt3sas_get_sdev_by_addr(ioc,
				    mpt3sas_port->remote_identify.sas_address);
		if (!sas_device) {
			dfailprintk(ioc, printk(MPT3SAS_FMT
				"failure at %s:%d/%s()!\n",
				ioc->name, __FILE__, __LINE__, __func__));
			goto out_fail;
		}
		sas_device->pend_sas_rphy_add = 1;
	}

	if ((sas_rphy_add(rphy))) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
	}

	if (mpt3sas_port->remote_identify.device_type == SAS_END_DEVICE) {
		sas_device->pend_sas_rphy_add = 0;
		sas_device_put(sas_device);
	}

	if ((ioc->logging_level & MPT_DEBUG_TRANSPORT))
		dev_printk(KERN_INFO, &rphy->dev,
			"add: handle(0x%04x), sas_addr(0x%016llx)\n",
			handle, (unsigned long long)
		    mpt3sas_port->remote_identify.sas_address);
	mpt3sas_port->rphy = rphy;
	spin_lock_irqsave(&ioc->sas_node_lock, flags);
	list_add_tail(&mpt3sas_port->port_list, &sas_node->sas_port_list);
	spin_unlock_irqrestore(&ioc->sas_node_lock, flags);

	/* fill in report manufacture */
	if (mpt3sas_port->remote_identify.device_type ==
	    MPI2_SAS_DEVICE_INFO_EDGE_EXPANDER ||
	    mpt3sas_port->remote_identify.device_type ==
	    MPI2_SAS_DEVICE_INFO_FANOUT_EXPANDER)
		_transport_expander_report_manufacture(ioc,
		    mpt3sas_port->remote_identify.sas_address,
		    rphy_to_expander_device(rphy));
	return mpt3sas_port;

 out_fail:
	list_for_each_entry_safe(mpt3sas_phy, next, &mpt3sas_port->phy_list,
	    port_siblings)
		list_del(&mpt3sas_phy->port_siblings);
	kfree(mpt3sas_port);
	return NULL;
}

/**
 * mpt3sas_transport_port_remove - remove port from the list
 * @ioc: per adapter object
 * @sas_address: sas address of attached device
 * @sas_address_parent: sas address of parent expander or sas host
 * Context: This function will acquire ioc->sas_node_lock.
 *
 * Removing object and freeing associated memory from the
 * ioc->sas_port_list.
 *
 * Return nothing.
 */
void
mpt3sas_transport_port_remove(struct MPT3SAS_ADAPTER *ioc, u64 sas_address,
	u64 sas_address_parent)
{
	int i;
	unsigned long flags;
	struct _sas_port *mpt3sas_port, *next;
	struct _sas_node *sas_node;
	u8 found = 0;
	struct _sas_phy *mpt3sas_phy, *next_phy;

	spin_lock_irqsave(&ioc->sas_node_lock, flags);
	sas_node = _transport_sas_node_find_by_sas_address(ioc,
	    sas_address_parent);
	if (!sas_node) {
		spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
		return;
	}
	list_for_each_entry_safe(mpt3sas_port, next, &sas_node->sas_port_list,
	    port_list) {
		if (mpt3sas_port->remote_identify.sas_address != sas_address)
			continue;
		found = 1;
		list_del(&mpt3sas_port->port_list);
		goto out;
	}
 out:
	if (!found) {
		spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
		return;
	}

	for (i = 0; i < sas_node->num_phys; i++) {
		if (sas_node->phy[i].remote_identify.sas_address == sas_address)
			memset(&sas_node->phy[i].remote_identify, 0 ,
			    sizeof(struct sas_identify));
	}

	spin_unlock_irqrestore(&ioc->sas_node_lock, flags);

	list_for_each_entry_safe(mpt3sas_phy, next_phy,
	    &mpt3sas_port->phy_list, port_siblings) {
		if ((ioc->logging_level & MPT_DEBUG_TRANSPORT))
			dev_printk(KERN_INFO, &mpt3sas_port->port->dev,
			    "remove: sas_addr(0x%016llx), phy(%d)\n",
			    (unsigned long long)
			    mpt3sas_port->remote_identify.sas_address,
			    mpt3sas_phy->phy_id);
		mpt3sas_phy->phy_belongs_to_port = 0;
		sas_port_delete_phy(mpt3sas_port->port, mpt3sas_phy->phy);
		list_del(&mpt3sas_phy->port_siblings);
	}
	sas_port_delete(mpt3sas_port->port);
	kfree(mpt3sas_port);
}

/**
 * mpt3sas_transport_add_host_phy - report sas_host phy to transport
 * @ioc: per adapter object
 * @mpt3sas_phy: mpt3sas per phy object
 * @phy_pg0: sas phy page 0
 * @parent_dev: parent device class object
 *
 * Returns 0 for success, non-zero for failure.
 */
int
mpt3sas_transport_add_host_phy(struct MPT3SAS_ADAPTER *ioc, struct _sas_phy
	*mpt3sas_phy, Mpi2SasPhyPage0_t phy_pg0, struct device *parent_dev)
{
	struct sas_phy *phy;
	int phy_index = mpt3sas_phy->phy_id;


	INIT_LIST_HEAD(&mpt3sas_phy->port_siblings);
	phy = sas_phy_alloc(parent_dev, phy_index);
	if (!phy) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return -1;
	}
	if ((_transport_set_identify(ioc, mpt3sas_phy->handle,
	    &mpt3sas_phy->identify))) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		sas_phy_free(phy);
		return -1;
	}
	phy->identify = mpt3sas_phy->identify;
	mpt3sas_phy->attached_handle = le16_to_cpu(phy_pg0.AttachedDevHandle);
	if (mpt3sas_phy->attached_handle)
		_transport_set_identify(ioc, mpt3sas_phy->attached_handle,
		    &mpt3sas_phy->remote_identify);
	phy->identify.phy_identifier = mpt3sas_phy->phy_id;
	phy->negotiated_linkrate = _transport_convert_phy_link_rate(
	    phy_pg0.NegotiatedLinkRate & MPI2_SAS_NEG_LINK_RATE_MASK_PHYSICAL);
	phy->minimum_linkrate_hw = _transport_convert_phy_link_rate(
	    phy_pg0.HwLinkRate & MPI2_SAS_HWRATE_MIN_RATE_MASK);
	phy->maximum_linkrate_hw = _transport_convert_phy_link_rate(
	    phy_pg0.HwLinkRate >> 4);
	phy->minimum_linkrate = _transport_convert_phy_link_rate(
	    phy_pg0.ProgrammedLinkRate & MPI2_SAS_PRATE_MIN_RATE_MASK);
	phy->maximum_linkrate = _transport_convert_phy_link_rate(
	    phy_pg0.ProgrammedLinkRate >> 4);

	if ((sas_phy_add(phy))) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		sas_phy_free(phy);
		return -1;
	}
	if ((ioc->logging_level & MPT_DEBUG_TRANSPORT))
		dev_printk(KERN_INFO, &phy->dev,
		    "add: handle(0x%04x), sas_addr(0x%016llx)\n"
		    "\tattached_handle(0x%04x), sas_addr(0x%016llx)\n",
		    mpt3sas_phy->handle, (unsigned long long)
		    mpt3sas_phy->identify.sas_address,
		    mpt3sas_phy->attached_handle,
		    (unsigned long long)
		    mpt3sas_phy->remote_identify.sas_address);
	mpt3sas_phy->phy = phy;
	return 0;
}


/**
 * mpt3sas_transport_add_expander_phy - report expander phy to transport
 * @ioc: per adapter object
 * @mpt3sas_phy: mpt3sas per phy object
 * @expander_pg1: expander page 1
 * @parent_dev: parent device class object
 *
 * Returns 0 for success, non-zero for failure.
 */
int
mpt3sas_transport_add_expander_phy(struct MPT3SAS_ADAPTER *ioc, struct _sas_phy
	*mpt3sas_phy, Mpi2ExpanderPage1_t expander_pg1,
	struct device *parent_dev)
{
	struct sas_phy *phy;
	int phy_index = mpt3sas_phy->phy_id;

	INIT_LIST_HEAD(&mpt3sas_phy->port_siblings);
	phy = sas_phy_alloc(parent_dev, phy_index);
	if (!phy) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return -1;
	}
	if ((_transport_set_identify(ioc, mpt3sas_phy->handle,
	    &mpt3sas_phy->identify))) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		sas_phy_free(phy);
		return -1;
	}
	phy->identify = mpt3sas_phy->identify;
	mpt3sas_phy->attached_handle =
	    le16_to_cpu(expander_pg1.AttachedDevHandle);
	if (mpt3sas_phy->attached_handle)
		_transport_set_identify(ioc, mpt3sas_phy->attached_handle,
		    &mpt3sas_phy->remote_identify);
	phy->identify.phy_identifier = mpt3sas_phy->phy_id;
	phy->negotiated_linkrate = _transport_convert_phy_link_rate(
	    expander_pg1.NegotiatedLinkRate &
	    MPI2_SAS_NEG_LINK_RATE_MASK_PHYSICAL);
	phy->minimum_linkrate_hw = _transport_convert_phy_link_rate(
	    expander_pg1.HwLinkRate & MPI2_SAS_HWRATE_MIN_RATE_MASK);
	phy->maximum_linkrate_hw = _transport_convert_phy_link_rate(
	    expander_pg1.HwLinkRate >> 4);
	phy->minimum_linkrate = _transport_convert_phy_link_rate(
	    expander_pg1.ProgrammedLinkRate & MPI2_SAS_PRATE_MIN_RATE_MASK);
	phy->maximum_linkrate = _transport_convert_phy_link_rate(
	    expander_pg1.ProgrammedLinkRate >> 4);

	if ((sas_phy_add(phy))) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		sas_phy_free(phy);
		return -1;
	}
	if ((ioc->logging_level & MPT_DEBUG_TRANSPORT))
		dev_printk(KERN_INFO, &phy->dev,
		    "add: handle(0x%04x), sas_addr(0x%016llx)\n"
		    "\tattached_handle(0x%04x), sas_addr(0x%016llx)\n",
		    mpt3sas_phy->handle, (unsigned long long)
		    mpt3sas_phy->identify.sas_address,
		    mpt3sas_phy->attached_handle,
		    (unsigned long long)
		    mpt3sas_phy->remote_identify.sas_address);
	mpt3sas_phy->phy = phy;
	return 0;
}

/**
 * mpt3sas_transport_update_links - refreshing phy link changes
 * @ioc: per adapter object
 * @sas_address: sas address of parent expander or sas host
 * @handle: attached device handle
 * @phy_numberv: phy number
 * @link_rate: new link rate
 *
 * Returns nothing.
 */
void
mpt3sas_transport_update_links(struct MPT3SAS_ADAPTER *ioc,
	u64 sas_address, u16 handle, u8 phy_number, u8 link_rate)
{
	unsigned long flags;
	struct _sas_node *sas_node;
	struct _sas_phy *mpt3sas_phy;

	if (ioc->shost_recovery || ioc->pci_error_recovery)
		return;

	spin_lock_irqsave(&ioc->sas_node_lock, flags);
	sas_node = _transport_sas_node_find_by_sas_address(ioc, sas_address);
	if (!sas_node) {
		spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
		return;
	}

	mpt3sas_phy = &sas_node->phy[phy_number];
	mpt3sas_phy->attached_handle = handle;
	spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
	if (handle && (link_rate >= MPI2_SAS_NEG_LINK_RATE_1_5)) {
		_transport_set_identify(ioc, handle,
		    &mpt3sas_phy->remote_identify);
		_transport_add_phy_to_an_existing_port(ioc, sas_node,
		    mpt3sas_phy, mpt3sas_phy->remote_identify.sas_address);
#ifdef MY_DEF_HERE
	} else {
		mpt3sas_phy->attached_handle = (u16)0;
		memset(&mpt3sas_phy->remote_identify, 0 , sizeof(struct
		    sas_identify));
		_transport_del_phy_from_an_existing_port(ioc, sas_node, mpt3sas_phy);
	}
#else /* MY_DEF_HERE */
	} else
		memset(&mpt3sas_phy->remote_identify, 0 , sizeof(struct
		    sas_identify));
#endif /* MY_DEF_HERE */

	if (mpt3sas_phy->phy)
		mpt3sas_phy->phy->negotiated_linkrate =
		    _transport_convert_phy_link_rate(link_rate);

	if ((ioc->logging_level & MPT_DEBUG_TRANSPORT))
		dev_printk(KERN_INFO, &mpt3sas_phy->phy->dev,
		    "refresh: parent sas_addr(0x%016llx),\n"
		    "\tlink_rate(0x%02x), phy(%d)\n"
		    "\tattached_handle(0x%04x), sas_addr(0x%016llx)\n",
		    (unsigned long long)sas_address,
		    link_rate, phy_number, handle, (unsigned long long)
		    mpt3sas_phy->remote_identify.sas_address);
}

static inline void *
phy_to_ioc(struct sas_phy *phy)
{
	struct Scsi_Host *shost = dev_to_shost(phy->dev.parent);
	return shost_priv(shost);
}

static inline void *
rphy_to_ioc(struct sas_rphy *rphy)
{
	struct Scsi_Host *shost = dev_to_shost(rphy->dev.parent->parent);
	return shost_priv(shost);
}

/* report phy error log structure */
struct phy_error_log_request {
	u8 smp_frame_type; /* 0x40 */
	u8 function; /* 0x11 */
	u8 allocated_response_length;
	u8 request_length; /* 02 */
	u8 reserved_1[5];
	u8 phy_identifier;
	u8 reserved_2[2];
};

/* report phy error log reply structure */
struct phy_error_log_reply {
	u8 smp_frame_type; /* 0x41 */
	u8 function; /* 0x11 */
	u8 function_result;
	u8 response_length;
	__be16 expander_change_count;
	u8 reserved_1[3];
	u8 phy_identifier;
	u8 reserved_2[2];
	__be32 invalid_dword;
	__be32 running_disparity_error;
	__be32 loss_of_dword_sync;
	__be32 phy_reset_problem;
};

/**
 * _transport_get_expander_phy_error_log - return expander counters
 * @ioc: per adapter object
 * @phy: The sas phy object
 *
 * Returns 0 for success, non-zero for failure.
 *
 */
static int
_transport_get_expander_phy_error_log(struct MPT3SAS_ADAPTER *ioc,
	struct sas_phy *phy)
{
	Mpi2SmpPassthroughRequest_t *mpi_request;
	Mpi2SmpPassthroughReply_t *mpi_reply;
	struct phy_error_log_request *phy_error_log_request;
	struct phy_error_log_reply *phy_error_log_reply;
	int rc;
	u16 smid;
	u32 ioc_state;
	unsigned long timeleft;
	void *psge;
	u8 issue_reset = 0;
	void *data_out = NULL;
	dma_addr_t data_out_dma;
	u32 sz;
	u16 wait_state_count;

	if (ioc->shost_recovery || ioc->pci_error_recovery) {
		pr_info(MPT3SAS_FMT "%s: host reset in progress!\n",
		    __func__, ioc->name);
		return -EFAULT;
	}

	mutex_lock(&ioc->transport_cmds.mutex);

	if (ioc->transport_cmds.status != MPT3_CMD_NOT_USED) {
		pr_err(MPT3SAS_FMT "%s: transport_cmds in use\n",
		    ioc->name, __func__);
		rc = -EAGAIN;
		goto out;
	}
	ioc->transport_cmds.status = MPT3_CMD_PENDING;

	wait_state_count = 0;
	ioc_state = mpt3sas_base_get_iocstate(ioc, 1);
	while (ioc_state != MPI2_IOC_STATE_OPERATIONAL) {
		if (wait_state_count++ == 10) {
			pr_err(MPT3SAS_FMT
			    "%s: failed due to ioc not operational\n",
			    ioc->name, __func__);
			rc = -EFAULT;
			goto out;
		}
		ssleep(1);
		ioc_state = mpt3sas_base_get_iocstate(ioc, 1);
		pr_info(MPT3SAS_FMT
			"%s: waiting for operational state(count=%d)\n",
			ioc->name, __func__, wait_state_count);
	}
	if (wait_state_count)
		pr_info(MPT3SAS_FMT "%s: ioc is operational\n",
		    ioc->name, __func__);

	smid = mpt3sas_base_get_smid(ioc, ioc->transport_cb_idx);
	if (!smid) {
		pr_err(MPT3SAS_FMT "%s: failed obtaining a smid\n",
		    ioc->name, __func__);
		rc = -EAGAIN;
		goto out;
	}

	mpi_request = mpt3sas_base_get_msg_frame(ioc, smid);
	ioc->transport_cmds.smid = smid;

	sz = sizeof(struct phy_error_log_request) +
	    sizeof(struct phy_error_log_reply);
	data_out = pci_alloc_consistent(ioc->pdev, sz, &data_out_dma);
	if (!data_out) {
		pr_err("failure at %s:%d/%s()!\n", __FILE__,
		    __LINE__, __func__);
		rc = -ENOMEM;
		mpt3sas_base_free_smid(ioc, smid);
		goto out;
	}

	rc = -EINVAL;
	memset(data_out, 0, sz);
	phy_error_log_request = data_out;
	phy_error_log_request->smp_frame_type = 0x40;
	phy_error_log_request->function = 0x11;
	phy_error_log_request->request_length = 2;
	phy_error_log_request->allocated_response_length = 0;
	phy_error_log_request->phy_identifier = phy->number;

	memset(mpi_request, 0, sizeof(Mpi2SmpPassthroughRequest_t));
	mpi_request->Function = MPI2_FUNCTION_SMP_PASSTHROUGH;
	mpi_request->PhysicalPort = 0xFF;
	mpi_request->VF_ID = 0; /* TODO */
	mpi_request->VP_ID = 0;
	mpi_request->SASAddress = cpu_to_le64(phy->identify.sas_address);
	mpi_request->RequestDataLength =
	    cpu_to_le16(sizeof(struct phy_error_log_request));
	psge = &mpi_request->SGL;

	ioc->build_sg(ioc, psge, data_out_dma,
		sizeof(struct phy_error_log_request),
	    data_out_dma + sizeof(struct phy_error_log_request),
	    sizeof(struct phy_error_log_reply));

	dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		"phy_error_log - send to sas_addr(0x%016llx), phy(%d)\n",
		ioc->name, (unsigned long long)phy->identify.sas_address,
		phy->number));
	init_completion(&ioc->transport_cmds.done);
	mpt3sas_base_put_smid_default(ioc, smid);
	timeleft = wait_for_completion_timeout(&ioc->transport_cmds.done,
	    10*HZ);

	if (!(ioc->transport_cmds.status & MPT3_CMD_COMPLETE)) {
		pr_err(MPT3SAS_FMT "%s: timeout\n",
		    ioc->name, __func__);
		_debug_dump_mf(mpi_request,
		    sizeof(Mpi2SmpPassthroughRequest_t)/4);
		if (!(ioc->transport_cmds.status & MPT3_CMD_RESET))
			issue_reset = 1;
		goto issue_host_reset;
	}

	dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		"phy_error_log - complete\n", ioc->name));

	if (ioc->transport_cmds.status & MPT3_CMD_REPLY_VALID) {

		mpi_reply = ioc->transport_cmds.reply;

		dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		    "phy_error_log - reply data transfer size(%d)\n",
		    ioc->name, le16_to_cpu(mpi_reply->ResponseDataLength)));

		if (le16_to_cpu(mpi_reply->ResponseDataLength) !=
		    sizeof(struct phy_error_log_reply))
			goto out;

		phy_error_log_reply = data_out +
		    sizeof(struct phy_error_log_request);

		dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		    "phy_error_log - function_result(%d)\n",
		    ioc->name, phy_error_log_reply->function_result));

		phy->invalid_dword_count =
		    be32_to_cpu(phy_error_log_reply->invalid_dword);
		phy->running_disparity_error_count =
		    be32_to_cpu(phy_error_log_reply->running_disparity_error);
		phy->loss_of_dword_sync_count =
		    be32_to_cpu(phy_error_log_reply->loss_of_dword_sync);
		phy->phy_reset_problem_count =
		    be32_to_cpu(phy_error_log_reply->phy_reset_problem);
		rc = 0;
	} else
		dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		    "phy_error_log - no reply\n", ioc->name));

 issue_host_reset:
	if (issue_reset)
		mpt3sas_base_hard_reset_handler(ioc, CAN_SLEEP,
		    FORCE_BIG_HAMMER);
 out:
	ioc->transport_cmds.status = MPT3_CMD_NOT_USED;
	if (data_out)
		pci_free_consistent(ioc->pdev, sz, data_out, data_out_dma);

	mutex_unlock(&ioc->transport_cmds.mutex);
	return rc;
}

/**
 * _transport_get_linkerrors - return phy counters for both hba and expanders
 * @phy: The sas phy object
 *
 * Returns 0 for success, non-zero for failure.
 *
 */
static int
_transport_get_linkerrors(struct sas_phy *phy)
{
	struct MPT3SAS_ADAPTER *ioc = phy_to_ioc(phy);
	unsigned long flags;
	Mpi2ConfigReply_t mpi_reply;
	Mpi2SasPhyPage1_t phy_pg1;

	spin_lock_irqsave(&ioc->sas_node_lock, flags);
	if (_transport_sas_node_find_by_sas_address(ioc,
	    phy->identify.sas_address) == NULL) {
		spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&ioc->sas_node_lock, flags);

	if (phy->identify.sas_address != ioc->sas_hba.sas_address)
		return _transport_get_expander_phy_error_log(ioc, phy);

	/* get hba phy error logs */
	if ((mpt3sas_config_get_phy_pg1(ioc, &mpi_reply, &phy_pg1,
		    phy->number))) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return -ENXIO;
	}

	if (mpi_reply.IOCStatus || mpi_reply.IOCLogInfo)
		pr_info(MPT3SAS_FMT
			"phy(%d), ioc_status (0x%04x), loginfo(0x%08x)\n",
			ioc->name, phy->number,
			le16_to_cpu(mpi_reply.IOCStatus),
		    le32_to_cpu(mpi_reply.IOCLogInfo));

	phy->invalid_dword_count = le32_to_cpu(phy_pg1.InvalidDwordCount);
	phy->running_disparity_error_count =
	    le32_to_cpu(phy_pg1.RunningDisparityErrorCount);
	phy->loss_of_dword_sync_count =
	    le32_to_cpu(phy_pg1.LossDwordSynchCount);
	phy->phy_reset_problem_count =
	    le32_to_cpu(phy_pg1.PhyResetProblemCount);
	return 0;
}

/**
 * _transport_get_enclosure_identifier -
 * @phy: The sas phy object
 *
 * Obtain the enclosure logical id for an expander.
 * Returns 0 for success, non-zero for failure.
 */
static int
_transport_get_enclosure_identifier(struct sas_rphy *rphy, u64 *identifier)
{
	struct MPT3SAS_ADAPTER *ioc = rphy_to_ioc(rphy);
	struct _sas_device *sas_device;
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = __mpt3sas_get_sdev_by_addr(ioc,
	    rphy->identify.sas_address);
	if (sas_device) {
		*identifier = sas_device->enclosure_logical_id;
		rc = 0;
		sas_device_put(sas_device);
	} else {
		*identifier = 0;
		rc = -ENXIO;
	}

	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	return rc;
}

/**
 * _transport_get_bay_identifier -
 * @phy: The sas phy object
 *
 * Returns the slot id for a device that resides inside an enclosure.
 */
static int
_transport_get_bay_identifier(struct sas_rphy *rphy)
{
	struct MPT3SAS_ADAPTER *ioc = rphy_to_ioc(rphy);
	struct _sas_device *sas_device;
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = __mpt3sas_get_sdev_by_addr(ioc,
	    rphy->identify.sas_address);
	if (sas_device) {
		rc = sas_device->slot;
		sas_device_put(sas_device);
	} else {
		rc = -ENXIO;
	}
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	return rc;
}

/* phy control request structure */
struct phy_control_request {
	u8 smp_frame_type; /* 0x40 */
	u8 function; /* 0x91 */
	u8 allocated_response_length;
	u8 request_length; /* 0x09 */
	u16 expander_change_count;
	u8 reserved_1[3];
	u8 phy_identifier;
	u8 phy_operation;
	u8 reserved_2[13];
	u64 attached_device_name;
	u8 programmed_min_physical_link_rate;
	u8 programmed_max_physical_link_rate;
	u8 reserved_3[6];
};

/* phy control reply structure */
struct phy_control_reply {
	u8 smp_frame_type; /* 0x41 */
	u8 function; /* 0x11 */
	u8 function_result;
	u8 response_length;
};

#define SMP_PHY_CONTROL_LINK_RESET	(0x01)
#define SMP_PHY_CONTROL_HARD_RESET	(0x02)
#define SMP_PHY_CONTROL_DISABLE		(0x03)

/**
 * _transport_expander_phy_control - expander phy control
 * @ioc: per adapter object
 * @phy: The sas phy object
 *
 * Returns 0 for success, non-zero for failure.
 *
 */
static int
_transport_expander_phy_control(struct MPT3SAS_ADAPTER *ioc,
	struct sas_phy *phy, u8 phy_operation)
{
	Mpi2SmpPassthroughRequest_t *mpi_request;
	Mpi2SmpPassthroughReply_t *mpi_reply;
	struct phy_control_request *phy_control_request;
	struct phy_control_reply *phy_control_reply;
	int rc;
	u16 smid;
	u32 ioc_state;
	unsigned long timeleft;
	void *psge;
	u32 sgl_flags;
	u8 issue_reset = 0;
	void *data_out = NULL;
	dma_addr_t data_out_dma;
	u32 sz;
	u16 wait_state_count;

	if (ioc->shost_recovery || ioc->pci_error_recovery) {
		pr_info(MPT3SAS_FMT "%s: host reset in progress!\n",
		    __func__, ioc->name);
		return -EFAULT;
	}

	mutex_lock(&ioc->transport_cmds.mutex);

	if (ioc->transport_cmds.status != MPT3_CMD_NOT_USED) {
		pr_err(MPT3SAS_FMT "%s: transport_cmds in use\n",
		    ioc->name, __func__);
		rc = -EAGAIN;
		goto out;
	}
	ioc->transport_cmds.status = MPT3_CMD_PENDING;

	wait_state_count = 0;
	ioc_state = mpt3sas_base_get_iocstate(ioc, 1);
	while (ioc_state != MPI2_IOC_STATE_OPERATIONAL) {
		if (wait_state_count++ == 10) {
			pr_err(MPT3SAS_FMT
			    "%s: failed due to ioc not operational\n",
			    ioc->name, __func__);
			rc = -EFAULT;
			goto out;
		}
		ssleep(1);
		ioc_state = mpt3sas_base_get_iocstate(ioc, 1);
		pr_info(MPT3SAS_FMT
			"%s: waiting for operational state(count=%d)\n",
			ioc->name, __func__, wait_state_count);
	}
	if (wait_state_count)
		pr_info(MPT3SAS_FMT "%s: ioc is operational\n",
		    ioc->name, __func__);

	smid = mpt3sas_base_get_smid(ioc, ioc->transport_cb_idx);
	if (!smid) {
		pr_err(MPT3SAS_FMT "%s: failed obtaining a smid\n",
		    ioc->name, __func__);
		rc = -EAGAIN;
		goto out;
	}

	mpi_request = mpt3sas_base_get_msg_frame(ioc, smid);
	ioc->transport_cmds.smid = smid;

	sz = sizeof(struct phy_control_request) +
	    sizeof(struct phy_control_reply);
	data_out = pci_alloc_consistent(ioc->pdev, sz, &data_out_dma);
	if (!data_out) {
		pr_err("failure at %s:%d/%s()!\n", __FILE__,
		    __LINE__, __func__);
		rc = -ENOMEM;
		mpt3sas_base_free_smid(ioc, smid);
		goto out;
	}

	rc = -EINVAL;
	memset(data_out, 0, sz);
	phy_control_request = data_out;
	phy_control_request->smp_frame_type = 0x40;
	phy_control_request->function = 0x91;
	phy_control_request->request_length = 9;
	phy_control_request->allocated_response_length = 0;
	phy_control_request->phy_identifier = phy->number;
	phy_control_request->phy_operation = phy_operation;
	phy_control_request->programmed_min_physical_link_rate =
	    phy->minimum_linkrate << 4;
	phy_control_request->programmed_max_physical_link_rate =
	    phy->maximum_linkrate << 4;

	memset(mpi_request, 0, sizeof(Mpi2SmpPassthroughRequest_t));
	mpi_request->Function = MPI2_FUNCTION_SMP_PASSTHROUGH;
	mpi_request->PhysicalPort = 0xFF;
	mpi_request->VF_ID = 0; /* TODO */
	mpi_request->VP_ID = 0;
	mpi_request->SASAddress = cpu_to_le64(phy->identify.sas_address);
	mpi_request->RequestDataLength =
	    cpu_to_le16(sizeof(struct phy_error_log_request));
	psge = &mpi_request->SGL;

	/* WRITE sgel first */
	sgl_flags = (MPI2_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI2_SGE_FLAGS_END_OF_BUFFER | MPI2_SGE_FLAGS_HOST_TO_IOC);
	sgl_flags = sgl_flags << MPI2_SGE_FLAGS_SHIFT;
	ioc->base_add_sg_single(psge, sgl_flags |
	    sizeof(struct phy_control_request), data_out_dma);

	/* incr sgel */
	psge += ioc->sge_size;

	/* READ sgel last */
	sgl_flags = (MPI2_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI2_SGE_FLAGS_LAST_ELEMENT | MPI2_SGE_FLAGS_END_OF_BUFFER |
	    MPI2_SGE_FLAGS_END_OF_LIST);
	sgl_flags = sgl_flags << MPI2_SGE_FLAGS_SHIFT;
	ioc->base_add_sg_single(psge, sgl_flags |
	    sizeof(struct phy_control_reply), data_out_dma +
	    sizeof(struct phy_control_request));

	dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		"phy_control - send to sas_addr(0x%016llx), phy(%d), opcode(%d)\n",
		ioc->name, (unsigned long long)phy->identify.sas_address,
		phy->number, phy_operation));
	init_completion(&ioc->transport_cmds.done);
	mpt3sas_base_put_smid_default(ioc, smid);
	timeleft = wait_for_completion_timeout(&ioc->transport_cmds.done,
	    10*HZ);

	if (!(ioc->transport_cmds.status & MPT3_CMD_COMPLETE)) {
		pr_err(MPT3SAS_FMT "%s: timeout\n",
		    ioc->name, __func__);
		_debug_dump_mf(mpi_request,
		    sizeof(Mpi2SmpPassthroughRequest_t)/4);
		if (!(ioc->transport_cmds.status & MPT3_CMD_RESET))
			issue_reset = 1;
		goto issue_host_reset;
	}

	dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		"phy_control - complete\n", ioc->name));

	if (ioc->transport_cmds.status & MPT3_CMD_REPLY_VALID) {

		mpi_reply = ioc->transport_cmds.reply;

		dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		    "phy_control - reply data transfer size(%d)\n",
		    ioc->name, le16_to_cpu(mpi_reply->ResponseDataLength)));

		if (le16_to_cpu(mpi_reply->ResponseDataLength) !=
		    sizeof(struct phy_control_reply))
			goto out;

		phy_control_reply = data_out +
		    sizeof(struct phy_control_request);

		dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		    "phy_control - function_result(%d)\n",
		    ioc->name, phy_control_reply->function_result));

		rc = 0;
	} else
		dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		    "phy_control - no reply\n", ioc->name));

 issue_host_reset:
	if (issue_reset)
		mpt3sas_base_hard_reset_handler(ioc, CAN_SLEEP,
		    FORCE_BIG_HAMMER);
 out:
	ioc->transport_cmds.status = MPT3_CMD_NOT_USED;
	if (data_out)
		pci_free_consistent(ioc->pdev, sz, data_out, data_out_dma);

	mutex_unlock(&ioc->transport_cmds.mutex);
	return rc;
}

/**
 * _transport_phy_reset -
 * @phy: The sas phy object
 * @hard_reset:
 *
 * Returns 0 for success, non-zero for failure.
 */
static int
_transport_phy_reset(struct sas_phy *phy, int hard_reset)
{
	struct MPT3SAS_ADAPTER *ioc = phy_to_ioc(phy);
	Mpi2SasIoUnitControlReply_t mpi_reply;
	Mpi2SasIoUnitControlRequest_t mpi_request;
	unsigned long flags;

	spin_lock_irqsave(&ioc->sas_node_lock, flags);
	if (_transport_sas_node_find_by_sas_address(ioc,
	    phy->identify.sas_address) == NULL) {
		spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&ioc->sas_node_lock, flags);

	/* handle expander phys */
	if (phy->identify.sas_address != ioc->sas_hba.sas_address)
		return _transport_expander_phy_control(ioc, phy,
		    (hard_reset == 1) ? SMP_PHY_CONTROL_HARD_RESET :
		    SMP_PHY_CONTROL_LINK_RESET);

	/* handle hba phys */
	memset(&mpi_request, 0, sizeof(Mpi2SasIoUnitControlRequest_t));
	mpi_request.Function = MPI2_FUNCTION_SAS_IO_UNIT_CONTROL;
	mpi_request.Operation = hard_reset ?
	    MPI2_SAS_OP_PHY_HARD_RESET : MPI2_SAS_OP_PHY_LINK_RESET;
	mpi_request.PhyNum = phy->number;

	if ((mpt3sas_base_sas_iounit_control(ioc, &mpi_reply, &mpi_request))) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return -ENXIO;
	}

	if (mpi_reply.IOCStatus || mpi_reply.IOCLogInfo)
		pr_info(MPT3SAS_FMT
		"phy(%d), ioc_status(0x%04x), loginfo(0x%08x)\n",
		ioc->name, phy->number, le16_to_cpu(mpi_reply.IOCStatus),
		    le32_to_cpu(mpi_reply.IOCLogInfo));

	return 0;
}

/**
 * _transport_phy_enable - enable/disable phys
 * @phy: The sas phy object
 * @enable: enable phy when true
 *
 * Only support sas_host direct attached phys.
 * Returns 0 for success, non-zero for failure.
 */
static int
_transport_phy_enable(struct sas_phy *phy, int enable)
{
	struct MPT3SAS_ADAPTER *ioc = phy_to_ioc(phy);
	Mpi2SasIOUnitPage1_t *sas_iounit_pg1 = NULL;
	Mpi2SasIOUnitPage0_t *sas_iounit_pg0 = NULL;
	Mpi2ConfigReply_t mpi_reply;
	u16 ioc_status;
	u16 sz;
	int rc = 0;
	unsigned long flags;
	int i, discovery_active;

	spin_lock_irqsave(&ioc->sas_node_lock, flags);
	if (_transport_sas_node_find_by_sas_address(ioc,
	    phy->identify.sas_address) == NULL) {
		spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&ioc->sas_node_lock, flags);

	/* handle expander phys */
	if (phy->identify.sas_address != ioc->sas_hba.sas_address)
		return _transport_expander_phy_control(ioc, phy,
		    (enable == 1) ? SMP_PHY_CONTROL_LINK_RESET :
		    SMP_PHY_CONTROL_DISABLE);

	/* handle hba phys */

	/* read sas_iounit page 0 */
	sz = offsetof(Mpi2SasIOUnitPage0_t, PhyData) + (ioc->sas_hba.num_phys *
	    sizeof(Mpi2SasIOUnit0PhyData_t));
	sas_iounit_pg0 = kzalloc(sz, GFP_KERNEL);
	if (!sas_iounit_pg0) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = -ENOMEM;
		goto out;
	}
	if ((mpt3sas_config_get_sas_iounit_pg0(ioc, &mpi_reply,
	    sas_iounit_pg0, sz))) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = -ENXIO;
		goto out;
	}
	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = -EIO;
		goto out;
	}

	/* unable to enable/disable phys when when discovery is active */
	for (i = 0, discovery_active = 0; i < ioc->sas_hba.num_phys ; i++) {
		if (sas_iounit_pg0->PhyData[i].PortFlags &
		    MPI2_SASIOUNIT0_PORTFLAGS_DISCOVERY_IN_PROGRESS) {
			pr_err(MPT3SAS_FMT "discovery is active on " \
			    "port = %d, phy = %d: unable to enable/disable "
			    "phys, try again later!\n", ioc->name,
			    sas_iounit_pg0->PhyData[i].Port, i);
			discovery_active = 1;
		}
	}

	if (discovery_active) {
		rc = -EAGAIN;
		goto out;
	}

	/* read sas_iounit page 1 */
	sz = offsetof(Mpi2SasIOUnitPage1_t, PhyData) + (ioc->sas_hba.num_phys *
	    sizeof(Mpi2SasIOUnit1PhyData_t));
	sas_iounit_pg1 = kzalloc(sz, GFP_KERNEL);
	if (!sas_iounit_pg1) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = -ENOMEM;
		goto out;
	}
	if ((mpt3sas_config_get_sas_iounit_pg1(ioc, &mpi_reply,
	    sas_iounit_pg1, sz))) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = -ENXIO;
		goto out;
	}
	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = -EIO;
		goto out;
	}

	/* copy Port/PortFlags/PhyFlags from page 0 */
	for (i = 0; i < ioc->sas_hba.num_phys ; i++) {
		sas_iounit_pg1->PhyData[i].Port =
		    sas_iounit_pg0->PhyData[i].Port;
		sas_iounit_pg1->PhyData[i].PortFlags =
		    (sas_iounit_pg0->PhyData[i].PortFlags &
		    MPI2_SASIOUNIT0_PORTFLAGS_AUTO_PORT_CONFIG);
		sas_iounit_pg1->PhyData[i].PhyFlags =
		    (sas_iounit_pg0->PhyData[i].PhyFlags &
		    (MPI2_SASIOUNIT0_PHYFLAGS_ZONING_ENABLED +
		    MPI2_SASIOUNIT0_PHYFLAGS_PHY_DISABLED));
	}

	if (enable)
		sas_iounit_pg1->PhyData[phy->number].PhyFlags
		    &= ~MPI2_SASIOUNIT1_PHYFLAGS_PHY_DISABLE;
	else
		sas_iounit_pg1->PhyData[phy->number].PhyFlags
		    |= MPI2_SASIOUNIT1_PHYFLAGS_PHY_DISABLE;

	mpt3sas_config_set_sas_iounit_pg1(ioc, &mpi_reply, sas_iounit_pg1, sz);

	/* link reset */
	if (enable)
		_transport_phy_reset(phy, 0);

 out:
	kfree(sas_iounit_pg1);
	kfree(sas_iounit_pg0);
	return rc;
}

/**
 * _transport_phy_speed - set phy min/max link rates
 * @phy: The sas phy object
 * @rates: rates defined in sas_phy_linkrates
 *
 * Only support sas_host direct attached phys.
 * Returns 0 for success, non-zero for failure.
 */
static int
_transport_phy_speed(struct sas_phy *phy, struct sas_phy_linkrates *rates)
{
	struct MPT3SAS_ADAPTER *ioc = phy_to_ioc(phy);
	Mpi2SasIOUnitPage1_t *sas_iounit_pg1 = NULL;
	Mpi2SasPhyPage0_t phy_pg0;
	Mpi2ConfigReply_t mpi_reply;
	u16 ioc_status;
	u16 sz;
	int i;
	int rc = 0;
	unsigned long flags;

	spin_lock_irqsave(&ioc->sas_node_lock, flags);
	if (_transport_sas_node_find_by_sas_address(ioc,
	    phy->identify.sas_address) == NULL) {
		spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&ioc->sas_node_lock, flags);

	if (!rates->minimum_linkrate)
		rates->minimum_linkrate = phy->minimum_linkrate;
	else if (rates->minimum_linkrate < phy->minimum_linkrate_hw)
		rates->minimum_linkrate = phy->minimum_linkrate_hw;

	if (!rates->maximum_linkrate)
		rates->maximum_linkrate = phy->maximum_linkrate;
	else if (rates->maximum_linkrate > phy->maximum_linkrate_hw)
		rates->maximum_linkrate = phy->maximum_linkrate_hw;

	/* handle expander phys */
	if (phy->identify.sas_address != ioc->sas_hba.sas_address) {
		phy->minimum_linkrate = rates->minimum_linkrate;
		phy->maximum_linkrate = rates->maximum_linkrate;
		return _transport_expander_phy_control(ioc, phy,
		    SMP_PHY_CONTROL_LINK_RESET);
	}

	/* handle hba phys */

	/* sas_iounit page 1 */
	sz = offsetof(Mpi2SasIOUnitPage1_t, PhyData) + (ioc->sas_hba.num_phys *
	    sizeof(Mpi2SasIOUnit1PhyData_t));
	sas_iounit_pg1 = kzalloc(sz, GFP_KERNEL);
	if (!sas_iounit_pg1) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = -ENOMEM;
		goto out;
	}
	if ((mpt3sas_config_get_sas_iounit_pg1(ioc, &mpi_reply,
	    sas_iounit_pg1, sz))) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = -ENXIO;
		goto out;
	}
	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = -EIO;
		goto out;
	}

	for (i = 0; i < ioc->sas_hba.num_phys; i++) {
		if (phy->number != i) {
			sas_iounit_pg1->PhyData[i].MaxMinLinkRate =
			    (ioc->sas_hba.phy[i].phy->minimum_linkrate +
			    (ioc->sas_hba.phy[i].phy->maximum_linkrate << 4));
		} else {
			sas_iounit_pg1->PhyData[i].MaxMinLinkRate =
			    (rates->minimum_linkrate +
			    (rates->maximum_linkrate << 4));
		}
	}

	if (mpt3sas_config_set_sas_iounit_pg1(ioc, &mpi_reply, sas_iounit_pg1,
	    sz)) {
		pr_err(MPT3SAS_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = -ENXIO;
		goto out;
	}

	/* link reset */
	_transport_phy_reset(phy, 0);

	/* read phy page 0, then update the rates in the sas transport phy */
	if (!mpt3sas_config_get_phy_pg0(ioc, &mpi_reply, &phy_pg0,
	    phy->number)) {
		phy->minimum_linkrate = _transport_convert_phy_link_rate(
		    phy_pg0.ProgrammedLinkRate & MPI2_SAS_PRATE_MIN_RATE_MASK);
		phy->maximum_linkrate = _transport_convert_phy_link_rate(
		    phy_pg0.ProgrammedLinkRate >> 4);
		phy->negotiated_linkrate = _transport_convert_phy_link_rate(
		    phy_pg0.NegotiatedLinkRate &
		    MPI2_SAS_NEG_LINK_RATE_MASK_PHYSICAL);
	}

 out:
	kfree(sas_iounit_pg1);
	return rc;
}

/**
 * _transport_smp_handler - transport portal for smp passthru
 * @shost: shost object
 * @rphy: sas transport rphy object
 * @req:
 *
 * This used primarily for smp_utils.
 * Example:
 *           smp_rep_general /sys/class/bsg/expander-5:0
 */
static int
_transport_smp_handler(struct Scsi_Host *shost, struct sas_rphy *rphy,
	struct request *req)
{
	struct MPT3SAS_ADAPTER *ioc = shost_priv(shost);
	Mpi2SmpPassthroughRequest_t *mpi_request;
	Mpi2SmpPassthroughReply_t *mpi_reply;
	int rc;
	u16 smid;
	u32 ioc_state;
	unsigned long timeleft;
	void *psge;
	u8 issue_reset = 0;
	dma_addr_t dma_addr_in = 0;
	dma_addr_t dma_addr_out = 0;
	dma_addr_t pci_dma_in = 0;
	dma_addr_t pci_dma_out = 0;
	void *pci_addr_in = NULL;
	void *pci_addr_out = NULL;
	u16 wait_state_count;
	struct request *rsp = req->next_rq;
	struct bio_vec bvec;
	struct bvec_iter iter;

	if (!rsp) {
		pr_err(MPT3SAS_FMT "%s: the smp response space is missing\n",
			ioc->name, __func__);
		return -EINVAL;
	}

	if (ioc->shost_recovery || ioc->pci_error_recovery) {
		pr_info(MPT3SAS_FMT "%s: host reset in progress!\n",
		    __func__, ioc->name);
		return -EFAULT;
	}

	rc = mutex_lock_interruptible(&ioc->transport_cmds.mutex);
	if (rc)
		return rc;

	if (ioc->transport_cmds.status != MPT3_CMD_NOT_USED) {
		pr_err(MPT3SAS_FMT "%s: transport_cmds in use\n", ioc->name,
		    __func__);
		rc = -EAGAIN;
		goto out;
	}
	ioc->transport_cmds.status = MPT3_CMD_PENDING;

	/* Check if the request is split across multiple segments */
	if (bio_multiple_segments(req->bio)) {
		u32 offset = 0;

		/* Allocate memory and copy the request */
		pci_addr_out = pci_alloc_consistent(ioc->pdev,
		    blk_rq_bytes(req), &pci_dma_out);
		if (!pci_addr_out) {
			pr_info(MPT3SAS_FMT "%s(): PCI Addr out = NULL\n",
			    ioc->name, __func__);
			rc = -ENOMEM;
			goto out;
		}

		bio_for_each_segment(bvec, req->bio, iter) {
			memcpy(pci_addr_out + offset,
			    page_address(bvec.bv_page) + bvec.bv_offset,
			    bvec.bv_len);
			offset += bvec.bv_len;
		}
	} else {
		dma_addr_out = pci_map_single(ioc->pdev, bio_data(req->bio),
		    blk_rq_bytes(req), PCI_DMA_BIDIRECTIONAL);
		if (pci_dma_mapping_error(ioc->pdev, dma_addr_out)) {
			pr_info(MPT3SAS_FMT "%s(): DMA Addr out = NULL\n",
			    ioc->name, __func__);
			rc = -ENOMEM;
			goto free_pci;
		}
	}

	/* Check if the response needs to be populated across
	 * multiple segments */
	if (bio_multiple_segments(rsp->bio)) {
		pci_addr_in = pci_alloc_consistent(ioc->pdev, blk_rq_bytes(rsp),
		    &pci_dma_in);
		if (!pci_addr_in) {
			pr_info(MPT3SAS_FMT "%s(): PCI Addr in = NULL\n",
			    ioc->name, __func__);
			rc = -ENOMEM;
			goto unmap;
		}
	} else {
		dma_addr_in =  pci_map_single(ioc->pdev, bio_data(rsp->bio),
		    blk_rq_bytes(rsp), PCI_DMA_BIDIRECTIONAL);
		if (pci_dma_mapping_error(ioc->pdev, dma_addr_in)) {
			pr_info(MPT3SAS_FMT "%s(): DMA Addr in = NULL\n",
			    ioc->name, __func__);
			rc = -ENOMEM;
			goto unmap;
		}
	}

	wait_state_count = 0;
	ioc_state = mpt3sas_base_get_iocstate(ioc, 1);
	while (ioc_state != MPI2_IOC_STATE_OPERATIONAL) {
		if (wait_state_count++ == 10) {
			pr_err(MPT3SAS_FMT
			    "%s: failed due to ioc not operational\n",
			    ioc->name, __func__);
			rc = -EFAULT;
			goto unmap;
		}
		ssleep(1);
		ioc_state = mpt3sas_base_get_iocstate(ioc, 1);
		pr_info(MPT3SAS_FMT
			"%s: waiting for operational state(count=%d)\n",
			ioc->name, __func__, wait_state_count);
	}
	if (wait_state_count)
		pr_info(MPT3SAS_FMT "%s: ioc is operational\n",
		    ioc->name, __func__);

	smid = mpt3sas_base_get_smid(ioc, ioc->transport_cb_idx);
	if (!smid) {
		pr_err(MPT3SAS_FMT "%s: failed obtaining a smid\n",
		    ioc->name, __func__);
		rc = -EAGAIN;
		goto unmap;
	}

	rc = 0;
	mpi_request = mpt3sas_base_get_msg_frame(ioc, smid);
	ioc->transport_cmds.smid = smid;

	memset(mpi_request, 0, sizeof(Mpi2SmpPassthroughRequest_t));
	mpi_request->Function = MPI2_FUNCTION_SMP_PASSTHROUGH;
	mpi_request->PhysicalPort = 0xFF;
	mpi_request->SASAddress = (rphy) ?
	    cpu_to_le64(rphy->identify.sas_address) :
	    cpu_to_le64(ioc->sas_hba.sas_address);
	mpi_request->RequestDataLength = cpu_to_le16(blk_rq_bytes(req) - 4);
	psge = &mpi_request->SGL;

	if (bio_multiple_segments(req->bio))
		ioc->build_sg(ioc, psge, pci_dma_out, (blk_rq_bytes(req) - 4),
		    pci_dma_in, (blk_rq_bytes(rsp) + 4));
	else
		ioc->build_sg(ioc, psge, dma_addr_out, (blk_rq_bytes(req) - 4),
		    dma_addr_in, (blk_rq_bytes(rsp) + 4));

	dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		"%s - sending smp request\n", ioc->name, __func__));

	init_completion(&ioc->transport_cmds.done);
	mpt3sas_base_put_smid_default(ioc, smid);
	timeleft = wait_for_completion_timeout(&ioc->transport_cmds.done,
	    10*HZ);

	if (!(ioc->transport_cmds.status & MPT3_CMD_COMPLETE)) {
		pr_err(MPT3SAS_FMT "%s : timeout\n",
		    __func__, ioc->name);
		_debug_dump_mf(mpi_request,
		    sizeof(Mpi2SmpPassthroughRequest_t)/4);
		if (!(ioc->transport_cmds.status & MPT3_CMD_RESET))
			issue_reset = 1;
		goto issue_host_reset;
	}

	dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		"%s - complete\n", ioc->name, __func__));

	if (ioc->transport_cmds.status & MPT3_CMD_REPLY_VALID) {

		mpi_reply = ioc->transport_cmds.reply;

		dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		    "%s - reply data transfer size(%d)\n",
		    ioc->name, __func__,
		    le16_to_cpu(mpi_reply->ResponseDataLength)));

		memcpy(req->sense, mpi_reply, sizeof(*mpi_reply));
		req->sense_len = sizeof(*mpi_reply);
		req->resid_len = 0;
		rsp->resid_len -=
		    le16_to_cpu(mpi_reply->ResponseDataLength);

		/* check if the resp needs to be copied from the allocated
		 * pci mem */
		if (bio_multiple_segments(rsp->bio)) {
			u32 offset = 0;
			u32 bytes_to_copy =
			    le16_to_cpu(mpi_reply->ResponseDataLength);
			bio_for_each_segment(bvec, rsp->bio, iter) {
				if (bytes_to_copy <= bvec.bv_len) {
					memcpy(page_address(bvec.bv_page) +
					    bvec.bv_offset, pci_addr_in +
					    offset, bytes_to_copy);
					break;
				} else {
					memcpy(page_address(bvec.bv_page) +
					    bvec.bv_offset, pci_addr_in +
					    offset, bvec.bv_len);
					bytes_to_copy -= bvec.bv_len;
				}
				offset += bvec.bv_len;
			}
		}
	} else {
		dtransportprintk(ioc, pr_info(MPT3SAS_FMT
		    "%s - no reply\n", ioc->name, __func__));
		rc = -ENXIO;
	}

 issue_host_reset:
	if (issue_reset) {
		mpt3sas_base_hard_reset_handler(ioc, CAN_SLEEP,
		    FORCE_BIG_HAMMER);
		rc = -ETIMEDOUT;
	}

 unmap:
	if (dma_addr_out)
		pci_unmap_single(ioc->pdev, dma_addr_out, blk_rq_bytes(req),
		    PCI_DMA_BIDIRECTIONAL);
	if (dma_addr_in)
		pci_unmap_single(ioc->pdev, dma_addr_in, blk_rq_bytes(rsp),
		    PCI_DMA_BIDIRECTIONAL);

 free_pci:
	if (pci_addr_out)
		pci_free_consistent(ioc->pdev, blk_rq_bytes(req), pci_addr_out,
		    pci_dma_out);

	if (pci_addr_in)
		pci_free_consistent(ioc->pdev, blk_rq_bytes(rsp), pci_addr_in,
		    pci_dma_in);

 out:
	ioc->transport_cmds.status = MPT3_CMD_NOT_USED;
	mutex_unlock(&ioc->transport_cmds.mutex);
	return rc;
}

struct sas_function_template mpt3sas_transport_functions = {
	.get_linkerrors		= _transport_get_linkerrors,
	.get_enclosure_identifier = _transport_get_enclosure_identifier,
	.get_bay_identifier	= _transport_get_bay_identifier,
	.phy_reset		= _transport_phy_reset,
	.phy_enable		= _transport_phy_enable,
	.set_phy_speed		= _transport_phy_speed,
	.smp_handler		= _transport_smp_handler,
};

struct scsi_transport_template *mpt3sas_transport_template;
