#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
#ifndef _SCSI_IOCTL_H
#define _SCSI_IOCTL_H 

#define SCSI_IOCTL_SEND_COMMAND 1
#define SCSI_IOCTL_TEST_UNIT_READY 2
#define SCSI_IOCTL_BENCHMARK_COMMAND 3
#define SCSI_IOCTL_SYNC 4			/* Request synchronous parameters */
#define SCSI_IOCTL_START_UNIT 5
#define SCSI_IOCTL_STOP_UNIT 6

#ifdef MY_ABC_HERE
/* kernel defined other scsi ioctl in scsi.h. If you want add new scsi ioctl,
 * you must check scsi.h, too */
#define SD_IOCTL_IDLE 4746
#define SD_IOCTL_SUPPORT_SLEEP 4747
#endif /* MY_ABC_HERE */

#ifdef MY_DEF_HERE
#define SD_IOCTL_SASHOST_DISK_LED 4755
#endif /* MY_DEF_HERE */

/* The door lock/unlock constants are compatible with Sun constants for
   the cdrom */
#define SCSI_IOCTL_DOORLOCK 0x5380		/* lock the eject mechanism */
#define SCSI_IOCTL_DOORUNLOCK 0x5381		/* unlock the mechanism	  */

#define	SCSI_REMOVAL_PREVENT	1
#define	SCSI_REMOVAL_ALLOW	0

#ifdef __KERNEL__

struct scsi_device;

/*
 * Structures used for scsi_ioctl et al.
 */

typedef struct scsi_ioctl_command {
	unsigned int inlen;
	unsigned int outlen;
	unsigned char data[0];
} Scsi_Ioctl_Command;

typedef struct scsi_idlun {
	__u32 dev_id;
	__u32 host_unique_id;
} Scsi_Idlun;

/* Fibre Channel WWN, port_id struct */
typedef struct scsi_fctargaddress {
	__u32 host_port_id;
	unsigned char host_wwn[8]; // include NULL term.
} Scsi_FCTargAddress;

int scsi_ioctl_block_when_processing_errors(struct scsi_device *sdev,
		int cmd, bool ndelay);
extern int scsi_ioctl(struct scsi_device *, int, void __user *);

#endif /* __KERNEL__ */
#endif /* _SCSI_IOCTL_H */
