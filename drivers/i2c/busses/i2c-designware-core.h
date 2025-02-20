#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Synopsys DesignWare I2C adapter driver (master only).
 *
 * Based on the TI DAVINCI I2C adapter driver.
 *
 * Copyright (C) 2006 Texas Instruments.
 * Copyright (C) 2007 MontaVista Software Inc.
 * Copyright (C) 2009 Provigent Ltd.
 *
 * ----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * ----------------------------------------------------------------------------
 *
 */


#define DW_IC_CON_MASTER		0x1
#define DW_IC_CON_SPEED_STD		0x2
#define DW_IC_CON_SPEED_FAST		0x4
#define DW_IC_CON_10BITADDR_MASTER	0x10
#define DW_IC_CON_RESTART_EN		0x20
#define DW_IC_CON_SLAVE_DISABLE		0x40


/**
 * struct dw_i2c_dev - private i2c-designware data
 * @dev: driver model device node
 * @base: IO registers pointer
 * @cmd_complete: tx completion indicator
 * @lock: protect this struct and IO registers
 * @clk: input reference clock
 * @cmd_err: run time hadware error code
 * @msgs: points to an array of messages currently being transfered
 * @msgs_num: the number of elements in msgs
 * @msg_write_idx: the element index of the current tx message in the msgs
 *	array
 * @tx_buf_len: the length of the current tx buffer
 * @tx_buf: the current tx buffer
 * @msg_read_idx: the element index of the current rx message in the msgs
 *	array
 * @rx_buf_len: the length of the current rx buffer
 * @rx_buf: the current rx buffer
 * @msg_err: error status of the current transfer
 * @status: i2c master status, one of STATUS_*
 * @abort_source: copy of the TX_ABRT_SOURCE register
 * @irq: interrupt number for the i2c master
 * @adapter: i2c subsystem adapter node
 * @tx_fifo_depth: depth of the hardware tx fifo
 * @rx_fifo_depth: depth of the hardware rx fifo
 * @rx_outstanding: current master-rx elements in tx fifo
 * @ss_hcnt: standard speed HCNT value
 * @ss_lcnt: standard speed LCNT value
 * @fs_hcnt: fast speed HCNT value
 * @fs_lcnt: fast speed LCNT value
 * @acquire_lock: function to acquire a hardware lock on the bus
 * @release_lock: function to release a hardware lock on the bus
 * @pm_runtime_disabled: true if pm runtime is disabled
 *
 * HCNT and LCNT parameters can be used if the platform knows more accurate
 * values than the one computed based only on the input clock frequency.
 * Leave them to be %0 if not used.
 */
struct dw_i2c_dev {
	struct device		*dev;
	void __iomem		*base;
	struct completion	cmd_complete;
	struct mutex		lock;
	struct clk		*clk;
	u32			(*get_clk_rate_khz) (struct dw_i2c_dev *dev);
	struct dw_pci_controller *controller;
	int			cmd_err;
	struct i2c_msg		*msgs;
	int			msgs_num;
	int			msg_write_idx;
	u32			tx_buf_len;
	u8			*tx_buf;
	int			msg_read_idx;
	u32			rx_buf_len;
	u8			*rx_buf;
	int			msg_err;
	unsigned int		status;
	u32			abort_source;
	int			irq;
	u32			accessor_flags;
	struct i2c_adapter	adapter;
	u32			functionality;
	u32			master_cfg;
	unsigned int		tx_fifo_depth;
	unsigned int		rx_fifo_depth;
	int			rx_outstanding;
	u32			sda_hold_time;
	u32			sda_falling_time;
	u32			scl_falling_time;
	u16			ss_hcnt;
	u16			ss_lcnt;
	u16			fs_hcnt;
	u16			fs_lcnt;
	int			(*acquire_lock)(struct dw_i2c_dev *dev);
	void			(*release_lock)(struct dw_i2c_dev *dev);
	bool			pm_runtime_disabled;
#if defined(MY_DEF_HERE)
	struct i2c_bus_recovery_info rinfo;
	char		proc_dir_name[15];
	struct proc_dir_entry * i2c_proc_dir;
	void __iomem *iomux_base;
	void __iomem *gpio_base;
#endif /* MY_DEF_HERE */
};

#define ACCESS_SWAP		0x00000001
#define ACCESS_16BIT		0x00000002
#define ACCESS_INTR_MASK	0x00000004

#if defined(MY_DEF_HERE)
#define AMD_IOMUX_BASE 0xFED80D00
#define AMD_IOMUX_BASE_SIZE 0x100
#define AMD_GPIO_BASE 0xFED81500
#define AMD_GPIO_BASE_SIZE 0x400
#define SMB_CLK_DELAY_TIME_MS 42
#define AMD_PIN_STS_OFFSET 16

static unsigned long delay_try_cnt = 0;
static unsigned long delay_suc_cnt = 0;
static unsigned long pulse_try_cnt = 0;
static unsigned long pulse_suc_cnt = 0;
#endif /* MY_DEF_HERE */

extern int i2c_dw_init(struct dw_i2c_dev *dev);
extern void i2c_dw_disable(struct dw_i2c_dev *dev);
extern void i2c_dw_disable_int(struct dw_i2c_dev *dev);
extern u32 i2c_dw_read_comp_param(struct dw_i2c_dev *dev);
extern int i2c_dw_probe(struct dw_i2c_dev *dev);
#if defined(MY_DEF_HERE)
extern int syno_dw_i2c_get_sda(struct i2c_adapter *adap);
extern int syno_dw_i2c_get_scl(struct i2c_adapter *adap);
extern void syno_dw_i2c_set_scl(struct i2c_adapter *adap, int val);
extern int syno_dw_delay_recovery(struct i2c_adapter *adap);
#endif /* MY_DEF_HERE */

#if IS_ENABLED(CONFIG_I2C_DESIGNWARE_BAYTRAIL)
extern int i2c_dw_eval_lock_support(struct dw_i2c_dev *dev);
#else
static inline int i2c_dw_eval_lock_support(struct dw_i2c_dev *dev) { return 0; }
#endif
