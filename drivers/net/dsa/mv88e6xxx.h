#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * net/dsa/mv88e6xxx.h - Marvell 88e6xxx switch chip support
 * Copyright (c) 2008 Marvell Semiconductor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MV88E6XXX_H
#define __MV88E6XXX_H

#include <linux/if_vlan.h>
#if defined(MY_ABC_HERE)
#include <linux/gpio/consumer.h>
#endif /* MY_ABC_HERE */

#ifndef UINT64_MAX
#define UINT64_MAX		(u64)(~((u64)0))
#endif

#define SMI_CMD			0x00
#define SMI_CMD_BUSY		BIT(15)
#define SMI_CMD_CLAUSE_22	BIT(12)
#define SMI_CMD_OP_22_WRITE	((1 << 10) | SMI_CMD_BUSY | SMI_CMD_CLAUSE_22)
#define SMI_CMD_OP_22_READ	((2 << 10) | SMI_CMD_BUSY | SMI_CMD_CLAUSE_22)
#define SMI_CMD_OP_45_WRITE_ADDR	((0 << 10) | SMI_CMD_BUSY)
#define SMI_CMD_OP_45_WRITE_DATA	((1 << 10) | SMI_CMD_BUSY)
#define SMI_CMD_OP_45_READ_DATA		((2 << 10) | SMI_CMD_BUSY)
#define SMI_CMD_OP_45_READ_DATA_INC	((3 << 10) | SMI_CMD_BUSY)
#define SMI_DATA		0x01

#if defined(MY_ABC_HERE)
/* Fiber/SERDES Registers are located at SMI address F, page 1 */
#define REG_FIBER_SERDES	0x0f
#define PAGE_FIBER_SERDES	0x01
#endif /* MY_ABC_HERE */

#if defined(MY_ABC_HERE)
#define REG_PORT_BASE_LEGACY 0x10
#define REG_PORT_BASE_PERIDOT  0
#define REG_PORT(p)		(REG_PORT_BASE + (p))
#else /* MY_ABC_HERE */
#define REG_PORT(p)		(0x10 + (p))
#endif /* MY_ABC_HERE */
#define PORT_STATUS		0x00
#define PORT_STATUS_PAUSE_EN	BIT(15)
#define PORT_STATUS_MY_PAUSE	BIT(14)
#define PORT_STATUS_HD_FLOW	BIT(13)
#define PORT_STATUS_PHY_DETECT	BIT(12)
#define PORT_STATUS_LINK	BIT(11)
#define PORT_STATUS_DUPLEX	BIT(10)
#define PORT_STATUS_SPEED_MASK	0x0300
#define PORT_STATUS_SPEED_10	0x0000
#define PORT_STATUS_SPEED_100	0x0100
#define PORT_STATUS_SPEED_1000	0x0200
#define PORT_STATUS_EEE		BIT(6) /* 6352 */
#define PORT_STATUS_AM_DIS	BIT(6) /* 6165 */
#define PORT_STATUS_MGMII	BIT(6) /* 6185 */
#define PORT_STATUS_TX_PAUSED	BIT(5)
#define PORT_STATUS_FLOW_CTRL	BIT(4)
#if defined(MY_ABC_HERE)
#define PORT_STATUS_CMODE_MASK	0x0f
#define PORT_STATUS_CMODE_100BASE_X	0x8
#define PORT_STATUS_CMODE_1000BASE_X	0x9
#define PORT_STATUS_CMODE_SGMII		0xa
#endif /* MY_ABC_HERE */
#define PORT_PCS_CTRL		0x01
#define PORT_PCS_CTRL_RGMII_DELAY_RXCLK	BIT(15)
#define PORT_PCS_CTRL_RGMII_DELAY_TXCLK	BIT(14)
#if defined(MY_ABC_HERE)
#define PORT_PCS_CTRL_FORCE_SPEED	BIT(13)
#endif /* MY_ABC_HERE */
#define PORT_PCS_CTRL_FC		BIT(7)
#define PORT_PCS_CTRL_FORCE_FC		BIT(6)
#define PORT_PCS_CTRL_LINK_UP		BIT(5)
#define PORT_PCS_CTRL_FORCE_LINK	BIT(4)
#define PORT_PCS_CTRL_DUPLEX_FULL	BIT(3)
#define PORT_PCS_CTRL_FORCE_DUPLEX	BIT(2)
#define PORT_PCS_CTRL_10		0x00
#define PORT_PCS_CTRL_100		0x01
#define PORT_PCS_CTRL_1000		0x02
#define PORT_PCS_CTRL_UNFORCED		0x03
#define PORT_PAUSE_CTRL		0x02
#define PORT_SWITCH_ID		0x03
#if defined(MY_ABC_HERE)
#define PORT_SWITCH_ID_PROD_NUM_6085	0x04a
#define PORT_SWITCH_ID_PROD_NUM_6095	0x095
#define PORT_SWITCH_ID_PROD_NUM_6131	0x106
#define PORT_SWITCH_ID_PROD_NUM_6320	0x115
#define PORT_SWITCH_ID_PROD_NUM_6123	0x121
#define PORT_SWITCH_ID_PROD_NUM_6161	0x161
#define PORT_SWITCH_ID_PROD_NUM_6165	0x165
#define PORT_SWITCH_ID_PROD_NUM_6171	0x171
#define PORT_SWITCH_ID_PROD_NUM_6172	0x172
#define PORT_SWITCH_ID_PROD_NUM_6175	0x175
#define PORT_SWITCH_ID_PROD_NUM_6176	0x176
#define PORT_SWITCH_ID_PROD_NUM_6185	0x1a7
#define PORT_SWITCH_ID_PROD_NUM_6240	0x240
#define PORT_SWITCH_ID_PROD_NUM_6321	0x310
#if defined(MY_ABC_HERE)
#define PORT_SWITCH_ID_PROD_NUM_6341	0x341
#else /* MY_ABC_HERE */
#define PORT_SWITCH_ID_PROD_NUM_6341	0x340
#endif /* MY_ABC_HERE */
#define PORT_SWITCH_ID_PROD_NUM_6352	0x352
#define PORT_SWITCH_ID_PROD_NUM_6350	0x371
#define PORT_SWITCH_ID_PROD_NUM_6351	0x375
#if defined(MY_ABC_HERE)
#define PORT_SWITCH_ID_PROD_NUM_6190	0x190
#define PORT_SWITCH_ID_PROD_NUM_6290	0x290
#define PORT_SWITCH_ID_PROD_NUM_6390	0x390
#endif /* MY_ABC_HERE */
#else /* MY_ABC_HERE */
#define PORT_SWITCH_ID_PROD_NUM_MASK	0xfff0
#define PORT_SWITCH_ID_REV_MASK		0x000f
#define PORT_SWITCH_ID_6031	0x0310
#define PORT_SWITCH_ID_6035	0x0350
#define PORT_SWITCH_ID_6046	0x0480
#define PORT_SWITCH_ID_6061	0x0610
#define PORT_SWITCH_ID_6065	0x0650
#define PORT_SWITCH_ID_6085	0x04a0
#define PORT_SWITCH_ID_6092	0x0970
#define PORT_SWITCH_ID_6095	0x0950
#define PORT_SWITCH_ID_6096	0x0980
#define PORT_SWITCH_ID_6097	0x0990
#define PORT_SWITCH_ID_6108	0x1070
#define PORT_SWITCH_ID_6121	0x1040
#define PORT_SWITCH_ID_6122	0x1050
#define PORT_SWITCH_ID_6123	0x1210
#define PORT_SWITCH_ID_6123_A1	0x1212
#define PORT_SWITCH_ID_6123_A2	0x1213
#define PORT_SWITCH_ID_6131	0x1060
#define PORT_SWITCH_ID_6131_B2	0x1066
#define PORT_SWITCH_ID_6152	0x1a40
#define PORT_SWITCH_ID_6155	0x1a50
#define PORT_SWITCH_ID_6161	0x1610
#define PORT_SWITCH_ID_6161_A1	0x1612
#define PORT_SWITCH_ID_6161_A2	0x1613
#define PORT_SWITCH_ID_6165	0x1650
#define PORT_SWITCH_ID_6165_A1	0x1652
#define PORT_SWITCH_ID_6165_A2	0x1653
#define PORT_SWITCH_ID_6171	0x1710
#define PORT_SWITCH_ID_6172	0x1720
#define PORT_SWITCH_ID_6175	0x1750
#define PORT_SWITCH_ID_6176	0x1760
#define PORT_SWITCH_ID_6182	0x1a60
#define PORT_SWITCH_ID_6185	0x1a70
#define PORT_SWITCH_ID_6240	0x2400
#define PORT_SWITCH_ID_6320	0x1150
#define PORT_SWITCH_ID_6320_A1	0x1151
#define PORT_SWITCH_ID_6320_A2	0x1152
#define PORT_SWITCH_ID_6321	0x3100
#define PORT_SWITCH_ID_6321_A1	0x3101
#define PORT_SWITCH_ID_6321_A2	0x3102
#define PORT_SWITCH_ID_6350	0x3710
#define PORT_SWITCH_ID_6351	0x3750
#define PORT_SWITCH_ID_6352	0x3520
#define PORT_SWITCH_ID_6352_A0	0x3521
#define PORT_SWITCH_ID_6352_A1	0x3522
#endif /* MY_ABC_HERE */
#define PORT_CONTROL		0x04
#define PORT_CONTROL_USE_CORE_TAG	BIT(15)
#define PORT_CONTROL_DROP_ON_LOCK	BIT(14)
#define PORT_CONTROL_EGRESS_UNMODIFIED	(0x0 << 12)
#define PORT_CONTROL_EGRESS_UNTAGGED	(0x1 << 12)
#define PORT_CONTROL_EGRESS_TAGGED	(0x2 << 12)
#define PORT_CONTROL_EGRESS_ADD_TAG	(0x3 << 12)
#define PORT_CONTROL_HEADER		BIT(11)
#define PORT_CONTROL_IGMP_MLD_SNOOP	BIT(10)
#define PORT_CONTROL_DOUBLE_TAG		BIT(9)
#define PORT_CONTROL_FRAME_MODE_NORMAL		(0x0 << 8)
#define PORT_CONTROL_FRAME_MODE_DSA		(0x1 << 8)
#define PORT_CONTROL_FRAME_MODE_PROVIDER	(0x2 << 8)
#define PORT_CONTROL_FRAME_ETHER_TYPE_DSA	(0x3 << 8)
#define PORT_CONTROL_DSA_TAG		BIT(8)
#define PORT_CONTROL_VLAN_TUNNEL	BIT(7)
#define PORT_CONTROL_TAG_IF_BOTH	BIT(6)
#define PORT_CONTROL_USE_IP		BIT(5)
#define PORT_CONTROL_USE_TAG		BIT(4)
#define PORT_CONTROL_FORWARD_UNKNOWN_MC	BIT(3)
#define PORT_CONTROL_FORWARD_UNKNOWN	BIT(2)
#define PORT_CONTROL_STATE_MASK		0x03
#define PORT_CONTROL_STATE_DISABLED	0x00
#define PORT_CONTROL_STATE_BLOCKING	0x01
#define PORT_CONTROL_STATE_LEARNING	0x02
#define PORT_CONTROL_STATE_FORWARDING	0x03
#define PORT_CONTROL_1		0x05
#if defined(MY_ABC_HERE)
#define PORT_CONTROL_1_FID_11_4_MASK	(0xff << 0)
#endif /* MY_ABC_HERE */
#define PORT_BASE_VLAN		0x06
#if defined(MY_ABC_HERE)
#define PORT_BASE_VLAN_FID_3_0_MASK	(0xf << 12)
#endif /* MY_ABC_HERE */
#define PORT_DEFAULT_VLAN	0x07
#define PORT_DEFAULT_VLAN_MASK	0xfff
#define PORT_CONTROL_2		0x08
#define PORT_CONTROL_2_IGNORE_FCS	BIT(15)
#define PORT_CONTROL_2_VTU_PRI_OVERRIDE	BIT(14)
#define PORT_CONTROL_2_SA_PRIO_OVERRIDE	BIT(13)
#define PORT_CONTROL_2_DA_PRIO_OVERRIDE	BIT(12)
#define PORT_CONTROL_2_JUMBO_1522	(0x00 << 12)
#define PORT_CONTROL_2_JUMBO_2048	(0x01 << 12)
#define PORT_CONTROL_2_JUMBO_10240	(0x02 << 12)
#define PORT_CONTROL_2_8021Q_MASK	(0x03 << 10)
#define PORT_CONTROL_2_8021Q_DISABLED	(0x00 << 10)
#define PORT_CONTROL_2_8021Q_FALLBACK	(0x01 << 10)
#define PORT_CONTROL_2_8021Q_CHECK	(0x02 << 10)
#define PORT_CONTROL_2_8021Q_SECURE	(0x03 << 10)
#define PORT_CONTROL_2_DISCARD_TAGGED	BIT(9)
#define PORT_CONTROL_2_DISCARD_UNTAGGED	BIT(8)
#define PORT_CONTROL_2_MAP_DA		BIT(7)
#define PORT_CONTROL_2_DEFAULT_FORWARD	BIT(6)
#define PORT_CONTROL_2_FORWARD_UNKNOWN	BIT(6)
#define PORT_CONTROL_2_EGRESS_MONITOR	BIT(5)
#define PORT_CONTROL_2_INGRESS_MONITOR	BIT(4)
#define PORT_RATE_CONTROL	0x09
#define PORT_RATE_CONTROL_2	0x0a
#define PORT_ASSOC_VECTOR	0x0b
#define PORT_ASSOC_VECTOR_HOLD_AT_1		BIT(15)
#define PORT_ASSOC_VECTOR_INT_AGE_OUT		BIT(14)
#define PORT_ASSOC_VECTOR_LOCKED_PORT		BIT(13)
#define PORT_ASSOC_VECTOR_IGNORE_WRONG		BIT(12)
#define PORT_ASSOC_VECTOR_REFRESH_LOCKED	BIT(11)
#define PORT_ATU_CONTROL	0x0c
#define PORT_PRI_OVERRIDE	0x0d
#define PORT_ETH_TYPE		0x0f
#define PORT_IN_DISCARD_LO	0x10
#define PORT_IN_DISCARD_HI	0x11
#define PORT_IN_FILTERED	0x12
#define PORT_OUT_FILTERED	0x13
#define PORT_TAG_REGMAP_0123	0x18
#define PORT_TAG_REGMAP_4567	0x19

#define REG_GLOBAL		0x1b
#define GLOBAL_STATUS		0x00
#define GLOBAL_STATUS_PPU_STATE BIT(15) /* 6351 and 6171 */
/* Two bits for 6165, 6185 etc */
#define GLOBAL_STATUS_PPU_MASK		(0x3 << 14)
#define GLOBAL_STATUS_PPU_DISABLED_RST	(0x0 << 14)
#define GLOBAL_STATUS_PPU_INITIALIZING	(0x1 << 14)
#define GLOBAL_STATUS_PPU_DISABLED	(0x2 << 14)
#define GLOBAL_STATUS_PPU_POLLING	(0x3 << 14)
#define GLOBAL_MAC_01		0x01
#define GLOBAL_MAC_23		0x02
#define GLOBAL_MAC_45		0x03
#define GLOBAL_ATU_FID		0x01	/* 6097 6165 6351 6352 */
#define GLOBAL_VTU_FID		0x02	/* 6097 6165 6351 6352 */
#define GLOBAL_VTU_FID_MASK	0xfff
#define GLOBAL_VTU_SID		0x03	/* 6097 6165 6351 6352 */
#define GLOBAL_VTU_SID_MASK	0x3f
#define GLOBAL_CONTROL		0x04
#define GLOBAL_CONTROL_SW_RESET		BIT(15)
#define GLOBAL_CONTROL_PPU_ENABLE	BIT(14)
#define GLOBAL_CONTROL_DISCARD_EXCESS	BIT(13) /* 6352 */
#define GLOBAL_CONTROL_SCHED_PRIO	BIT(11) /* 6152 */
#define GLOBAL_CONTROL_MAX_FRAME_1632	BIT(10) /* 6152 */
#define GLOBAL_CONTROL_RELOAD_EEPROM	BIT(9)	/* 6152 */
#define GLOBAL_CONTROL_DEVICE_EN	BIT(7)
#define GLOBAL_CONTROL_STATS_DONE_EN	BIT(6)
#define GLOBAL_CONTROL_VTU_PROBLEM_EN	BIT(5)
#define GLOBAL_CONTROL_VTU_DONE_EN	BIT(4)
#define GLOBAL_CONTROL_ATU_PROBLEM_EN	BIT(3)
#define GLOBAL_CONTROL_ATU_DONE_EN	BIT(2)
#define GLOBAL_CONTROL_TCAM_EN		BIT(1)
#define GLOBAL_CONTROL_EEPROM_DONE_EN	BIT(0)
#define GLOBAL_VTU_OP		0x05
#define GLOBAL_VTU_OP_BUSY	BIT(15)
#define GLOBAL_VTU_OP_FLUSH_ALL		((0x01 << 12) | GLOBAL_VTU_OP_BUSY)
#define GLOBAL_VTU_OP_VTU_LOAD_PURGE	((0x03 << 12) | GLOBAL_VTU_OP_BUSY)
#define GLOBAL_VTU_OP_VTU_GET_NEXT	((0x04 << 12) | GLOBAL_VTU_OP_BUSY)
#define GLOBAL_VTU_OP_STU_LOAD_PURGE	((0x05 << 12) | GLOBAL_VTU_OP_BUSY)
#define GLOBAL_VTU_OP_STU_GET_NEXT	((0x06 << 12) | GLOBAL_VTU_OP_BUSY)
#define GLOBAL_VTU_VID		0x06
#define GLOBAL_VTU_VID_MASK	0xfff
#define GLOBAL_VTU_VID_VALID	BIT(12)
#define GLOBAL_VTU_DATA_0_3	0x07
#define GLOBAL_VTU_DATA_4_7	0x08
#define GLOBAL_VTU_DATA_8_11	0x09
#define GLOBAL_VTU_STU_DATA_MASK		0x03
#define GLOBAL_VTU_DATA_MEMBER_TAG_UNMODIFIED	0x00
#define GLOBAL_VTU_DATA_MEMBER_TAG_UNTAGGED	0x01
#define GLOBAL_VTU_DATA_MEMBER_TAG_TAGGED	0x02
#define GLOBAL_VTU_DATA_MEMBER_TAG_NON_MEMBER	0x03
#define GLOBAL_STU_DATA_PORT_STATE_DISABLED	0x00
#define GLOBAL_STU_DATA_PORT_STATE_BLOCKING	0x01
#define GLOBAL_STU_DATA_PORT_STATE_LEARNING	0x02
#define GLOBAL_STU_DATA_PORT_STATE_FORWARDING	0x03
#define GLOBAL_ATU_CONTROL	0x0a
#define GLOBAL_ATU_CONTROL_LEARN2ALL	BIT(3)
#define GLOBAL_ATU_OP		0x0b
#define GLOBAL_ATU_OP_BUSY	BIT(15)
#define GLOBAL_ATU_OP_NOP		(0 << 12)
#define GLOBAL_ATU_OP_FLUSH_MOVE_ALL		((1 << 12) | GLOBAL_ATU_OP_BUSY)
#define GLOBAL_ATU_OP_FLUSH_MOVE_NON_STATIC	((2 << 12) | GLOBAL_ATU_OP_BUSY)
#define GLOBAL_ATU_OP_LOAD_DB		((3 << 12) | GLOBAL_ATU_OP_BUSY)
#define GLOBAL_ATU_OP_GET_NEXT_DB	((4 << 12) | GLOBAL_ATU_OP_BUSY)
#define GLOBAL_ATU_OP_FLUSH_MOVE_ALL_DB		((5 << 12) | GLOBAL_ATU_OP_BUSY)
#define GLOBAL_ATU_OP_FLUSH_MOVE_NON_STATIC_DB ((6 << 12) | GLOBAL_ATU_OP_BUSY)
#define GLOBAL_ATU_OP_GET_CLR_VIOLATION	  ((7 << 12) | GLOBAL_ATU_OP_BUSY)
#define GLOBAL_ATU_DATA		0x0c
#define GLOBAL_ATU_DATA_TRUNK			BIT(15)
#define GLOBAL_ATU_DATA_TRUNK_ID_MASK		0x00f0
#define GLOBAL_ATU_DATA_TRUNK_ID_SHIFT		4
#define GLOBAL_ATU_DATA_PORT_VECTOR_MASK	0x3ff0
#define GLOBAL_ATU_DATA_PORT_VECTOR_SHIFT	4
#define GLOBAL_ATU_DATA_STATE_MASK		0x0f
#define GLOBAL_ATU_DATA_STATE_UNUSED		0x00
#define GLOBAL_ATU_DATA_STATE_UC_MGMT		0x0d
#define GLOBAL_ATU_DATA_STATE_UC_STATIC		0x0e
#define GLOBAL_ATU_DATA_STATE_UC_PRIO_OVER	0x0f
#define GLOBAL_ATU_DATA_STATE_MC_NONE_RATE	0x05
#define GLOBAL_ATU_DATA_STATE_MC_STATIC		0x07
#define GLOBAL_ATU_DATA_STATE_MC_MGMT		0x0e
#define GLOBAL_ATU_DATA_STATE_MC_PRIO_OVER	0x0f
#define GLOBAL_ATU_MAC_01	0x0d
#define GLOBAL_ATU_MAC_23	0x0e
#define GLOBAL_ATU_MAC_45	0x0f
#define GLOBAL_IP_PRI_0		0x10
#define GLOBAL_IP_PRI_1		0x11
#define GLOBAL_IP_PRI_2		0x12
#define GLOBAL_IP_PRI_3		0x13
#define GLOBAL_IP_PRI_4		0x14
#define GLOBAL_IP_PRI_5		0x15
#define GLOBAL_IP_PRI_6		0x16
#define GLOBAL_IP_PRI_7		0x17
#define GLOBAL_IEEE_PRI		0x18
#define GLOBAL_CORE_TAG_TYPE	0x19
#define GLOBAL_MONITOR_CONTROL	0x1a
#define GLOBAL_MONITOR_CONTROL_INGRESS_SHIFT	12
#define GLOBAL_MONITOR_CONTROL_EGRESS_SHIFT	8
#define GLOBAL_MONITOR_CONTROL_ARP_SHIFT	4
#define GLOBAL_MONITOR_CONTROL_MIRROR_SHIFT	0
#define GLOBAL_MONITOR_CONTROL_ARP_DISABLED	(0xf0)
#define GLOBAL_CONTROL_2	0x1c
#define GLOBAL_CONTROL_2_NO_CASCADE		0xe000
#define GLOBAL_CONTROL_2_MULTIPLE_CASCADE	0xf000

#define GLOBAL_STATS_OP		0x1d
#define GLOBAL_STATS_OP_BUSY	BIT(15)
#define GLOBAL_STATS_OP_NOP		(0 << 12)
#define GLOBAL_STATS_OP_FLUSH_ALL	((1 << 12) | GLOBAL_STATS_OP_BUSY)
#define GLOBAL_STATS_OP_FLUSH_PORT	((2 << 12) | GLOBAL_STATS_OP_BUSY)
#define GLOBAL_STATS_OP_READ_CAPTURED	((4 << 12) | GLOBAL_STATS_OP_BUSY)
#define GLOBAL_STATS_OP_CAPTURE_PORT	((5 << 12) | GLOBAL_STATS_OP_BUSY)
#define GLOBAL_STATS_OP_HIST_RX		((1 << 10) | GLOBAL_STATS_OP_BUSY)
#define GLOBAL_STATS_OP_HIST_TX		((2 << 10) | GLOBAL_STATS_OP_BUSY)
#define GLOBAL_STATS_OP_HIST_RX_TX	((3 << 10) | GLOBAL_STATS_OP_BUSY)
#if defined(MY_ABC_HERE)
#define GLOBAL_STATS_OP_BANK_1	BIT(9)
#endif /* MY_ABC_HERE */
#define GLOBAL_STATS_COUNTER_32	0x1e
#define GLOBAL_STATS_COUNTER_01	0x1f

#define REG_GLOBAL2		0x1c
#define GLOBAL2_INT_SOURCE	0x00
#define GLOBAL2_INT_MASK	0x01
#define GLOBAL2_MGMT_EN_2X	0x02
#define GLOBAL2_MGMT_EN_0X	0x03
#define GLOBAL2_FLOW_CONTROL	0x04
#define GLOBAL2_SWITCH_MGMT	0x05
#define GLOBAL2_SWITCH_MGMT_USE_DOUBLE_TAG_DATA	BIT(15)
#define GLOBAL2_SWITCH_MGMT_PREVENT_LOOPS	BIT(14)
#define GLOBAL2_SWITCH_MGMT_FLOW_CONTROL_MSG	BIT(13)
#define GLOBAL2_SWITCH_MGMT_FORCE_FLOW_CTRL_PRI	BIT(7)
#define GLOBAL2_SWITCH_MGMT_RSVD2CPU		BIT(3)
#define GLOBAL2_DEVICE_MAPPING	0x06
#define GLOBAL2_DEVICE_MAPPING_UPDATE		BIT(15)
#define GLOBAL2_DEVICE_MAPPING_TARGET_SHIFT	8
#define GLOBAL2_DEVICE_MAPPING_PORT_MASK	0x0f
#define GLOBAL2_TRUNK_MASK	0x07
#define GLOBAL2_TRUNK_MASK_UPDATE		BIT(15)
#define GLOBAL2_TRUNK_MASK_NUM_SHIFT		12
#define GLOBAL2_TRUNK_MAPPING	0x08
#define GLOBAL2_TRUNK_MAPPING_UPDATE		BIT(15)
#define GLOBAL2_TRUNK_MAPPING_ID_SHIFT		11
#define GLOBAL2_INGRESS_OP	0x09
#define GLOBAL2_INGRESS_DATA	0x0a
#define GLOBAL2_PVT_ADDR	0x0b
#define GLOBAL2_PVT_DATA	0x0c
#define GLOBAL2_SWITCH_MAC	0x0d
#define GLOBAL2_SWITCH_MAC_BUSY BIT(15)
#define GLOBAL2_ATU_STATS	0x0e
#define GLOBAL2_PRIO_OVERRIDE	0x0f
#define GLOBAL2_PRIO_OVERRIDE_FORCE_SNOOP	BIT(7)
#define GLOBAL2_PRIO_OVERRIDE_SNOOP_SHIFT	4
#define GLOBAL2_PRIO_OVERRIDE_FORCE_ARP		BIT(3)
#define GLOBAL2_PRIO_OVERRIDE_ARP_SHIFT		0
#define GLOBAL2_EEPROM_OP	0x14
#define GLOBAL2_EEPROM_OP_BUSY		BIT(15)
#define GLOBAL2_EEPROM_OP_WRITE		((3 << 12) | GLOBAL2_EEPROM_OP_BUSY)
#define GLOBAL2_EEPROM_OP_READ		((4 << 12) | GLOBAL2_EEPROM_OP_BUSY)
#define GLOBAL2_EEPROM_OP_LOAD		BIT(11)
#define GLOBAL2_EEPROM_OP_WRITE_EN	BIT(10)
#define GLOBAL2_EEPROM_OP_ADDR_MASK	0xff
#define GLOBAL2_EEPROM_DATA	0x15
#define GLOBAL2_PTP_AVB_OP	0x16
#define GLOBAL2_PTP_AVB_DATA	0x17
#define GLOBAL2_SMI_OP		0x18
#define GLOBAL2_SMI_OP_BUSY		BIT(15)
#define GLOBAL2_SMI_OP_CLAUSE_22	BIT(12)
#define GLOBAL2_SMI_OP_22_WRITE		((1 << 10) | GLOBAL2_SMI_OP_BUSY | \
					 GLOBAL2_SMI_OP_CLAUSE_22)
#define GLOBAL2_SMI_OP_22_READ		((2 << 10) | GLOBAL2_SMI_OP_BUSY | \
					 GLOBAL2_SMI_OP_CLAUSE_22)
#define GLOBAL2_SMI_OP_45_WRITE_ADDR	((0 << 10) | GLOBAL2_SMI_OP_BUSY)
#define GLOBAL2_SMI_OP_45_WRITE_DATA	((1 << 10) | GLOBAL2_SMI_OP_BUSY)
#define GLOBAL2_SMI_OP_45_READ_DATA	((2 << 10) | GLOBAL2_SMI_OP_BUSY)
#define GLOBAL2_SMI_DATA	0x19
#define GLOBAL2_SCRATCH_MISC	0x1a
#define GLOBAL2_SCRATCH_BUSY		BIT(15)
#define GLOBAL2_SCRATCH_REGISTER_SHIFT	8
#define GLOBAL2_SCRATCH_VALUE_MASK	0xff
#define GLOBAL2_WDOG_CONTROL	0x1b
#define GLOBAL2_QOS_WEIGHT	0x1c
#define GLOBAL2_MISC		0x1d

#if defined(MY_ABC_HERE)
#define MV88E6XXX_N_FID		4096

/* List of supported models */
enum mv88e6xxx_model {
	MV88E6085,
	MV88E6095,
	MV88E6123,
	MV88E6131,
	MV88E6161,
	MV88E6165,
	MV88E6171,
	MV88E6172,
	MV88E6175,
	MV88E6176,
	MV88E6185,
	MV88E6240,
	MV88E6320,
	MV88E6321,
	MV88E6341,
	MV88E6350,
	MV88E6351,
	MV88E6352,
#if defined(MY_ABC_HERE)
	MV88E6190,
	MV88E6290,
	MV88E6390,
#endif /* MY_ABC_HERE */
};

enum mv88e6xxx_family {
	MV88E6XXX_FAMILY_NONE,
	MV88E6XXX_FAMILY_6065,	/* 6031 6035 6061 6065 */
	MV88E6XXX_FAMILY_6095,	/* 6092 6095 */
	MV88E6XXX_FAMILY_6097,	/* 6046 6085 6096 6097 */
	MV88E6XXX_FAMILY_6165,	/* 6123 6161 6165 */
	MV88E6XXX_FAMILY_6185,	/* 6108 6121 6122 6131 6152 6155 6182 6185 */
	MV88E6XXX_FAMILY_6320,	/* 6320 6321 */
	MV88E6XXX_FAMILY_6351,	/* 6171 6175 6350 6351 */
	MV88E6XXX_FAMILY_6352,	/* 6172 6176 6240 6341 6352 */
#if defined(MY_ABC_HERE)
	MV88E6XXX_FAMILY_6390,	/* 6190 6190X 6290 6390 6390X*/
#endif /* MY_ABC_HERE */
};

enum mv88e6xxx_cap {
	/* Address Translation Unit.
	 * The ATU is used to lookup and learn MAC addresses. See GLOBAL_ATU_OP.
	 */
	MV88E6XXX_CAP_ATU,

	/* Energy Efficient Ethernet.
	 */
	MV88E6XXX_CAP_EEE,

	/* EEPROM Command and Data registers.
	 * See GLOBAL2_EEPROM_OP and GLOBAL2_EEPROM_DATA.
	 */
	MV88E6XXX_CAP_EEPROM,

	/* Port State Filtering for 802.1D Spanning Tree.
	 * See PORT_CONTROL_STATE_* values in the PORT_CONTROL register.
	 */
	MV88E6XXX_CAP_PORTSTATE,

	/* PHY Polling Unit.
	 * See GLOBAL_CONTROL_PPU_ENABLE and GLOBAL_STATUS_PPU_POLLING.
	 */
	MV88E6XXX_CAP_PPU,
	MV88E6XXX_CAP_PPU_ACTIVE,

	/* SMI PHY Command and Data registers.
	 * This requires an indirect access to PHY registers through
	 * GLOBAL2_SMI_OP, otherwise direct access to PHY registers is done.
	 */
	MV88E6XXX_CAP_SMI_PHY,

	/* Per VLAN Spanning Tree Unit (STU).
	 * The Port State database, if present, is accessed through VTU
	 * operations and dedicated SID registers. See GLOBAL_VTU_SID.
	 */
	MV88E6XXX_CAP_STU,

	/* Switch MAC/WoL/WoF register.
	 * This requires an indirect access to set the switch MAC address
	 * through GLOBAL2_SWITCH_MAC, otherwise GLOBAL_MAC_01, GLOBAL_MAC_23,
	 * and GLOBAL_MAC_45 are used with a direct access.
	 */
	MV88E6XXX_CAP_SWITCH_MAC_WOL_WOF,

	/* Internal temperature sensor.
	 * Available from any enabled port's PHY register 26, page 6.
	 */
	MV88E6XXX_CAP_TEMP,
	MV88E6XXX_CAP_TEMP_LIMIT,

	/* In-chip Port Based VLANs.
	 * Each port VLANTable register (see PORT_BASE_VLAN) is used to restrict
	 * the output (or egress) ports to which it is allowed to send frames.
	 */
	MV88E6XXX_CAP_VLANTABLE,

	/* VLAN Table Unit.
	 * The VTU is used to program 802.1Q VLANs. See GLOBAL_VTU_OP.
	 */
	MV88E6XXX_CAP_VTU,

	/* Switch internal PHY SMI address conversion.
	 * Most of switch internal PHY SMI address are equal to switch port ID
	 * However Some switch internal PHY SMI addressis are (0x10 + port ID)
	 */
	MV88E6XXX_PHY_ADDR_CONVERT,
};

/* Bitmask of capabilities */
#define MV88E6XXX_FLAG_ATU		BIT(MV88E6XXX_CAP_ATU)
#define MV88E6XXX_FLAG_EEE		BIT(MV88E6XXX_CAP_EEE)
#define MV88E6XXX_FLAG_EEPROM		BIT(MV88E6XXX_CAP_EEPROM)
#define MV88E6XXX_FLAG_PORTSTATE	BIT(MV88E6XXX_CAP_PORTSTATE)
#define MV88E6XXX_FLAG_PPU		BIT(MV88E6XXX_CAP_PPU)
#define MV88E6XXX_FLAG_PPU_ACTIVE	BIT(MV88E6XXX_CAP_PPU_ACTIVE)
#define MV88E6XXX_FLAG_SMI_PHY		BIT(MV88E6XXX_CAP_SMI_PHY)
#define MV88E6XXX_FLAG_STU		BIT(MV88E6XXX_CAP_STU)
#define MV88E6XXX_FLAG_SWITCH_MAC	BIT(MV88E6XXX_CAP_SWITCH_MAC_WOL_WOF)
#define MV88E6XXX_FLAG_TEMP		BIT(MV88E6XXX_CAP_TEMP)
#define MV88E6XXX_FLAG_TEMP_LIMIT	BIT(MV88E6XXX_CAP_TEMP_LIMIT)
#define MV88E6XXX_FLAG_VLANTABLE	BIT(MV88E6XXX_CAP_VLANTABLE)
#define MV88E6XXX_FLAG_VTU		BIT(MV88E6XXX_CAP_VTU)
#define MV88E6XXX_FLAG_PHY_ADDR		BIT(MV88E6XXX_PHY_ADDR_CONVERT)


#define MV88E6XXX_FLAGS_FAMILY_6095	\
	(MV88E6XXX_FLAG_ATU |		\
	 MV88E6XXX_FLAG_PPU |		\
	 MV88E6XXX_FLAG_VLANTABLE |	\
	 MV88E6XXX_FLAG_VTU)

#define MV88E6XXX_FLAGS_FAMILY_6097	\
	(MV88E6XXX_FLAG_ATU |		\
	 MV88E6XXX_FLAG_PPU |		\
	 MV88E6XXX_FLAG_STU |		\
	 MV88E6XXX_FLAG_VLANTABLE |	\
	 MV88E6XXX_FLAG_VTU)

#define MV88E6XXX_FLAGS_FAMILY_6165	\
	(MV88E6XXX_FLAG_STU |		\
	 MV88E6XXX_FLAG_SWITCH_MAC |	\
	 MV88E6XXX_FLAG_TEMP |		\
	 MV88E6XXX_FLAG_VTU)

#define MV88E6XXX_FLAGS_FAMILY_6185	\
	(MV88E6XXX_FLAG_ATU |		\
	 MV88E6XXX_FLAG_PPU |		\
	 MV88E6XXX_FLAG_VLANTABLE |	\
	 MV88E6XXX_FLAG_VTU)

#define MV88E6XXX_FLAGS_FAMILY_6320	\
	(MV88E6XXX_FLAG_ATU |		\
	 MV88E6XXX_FLAG_EEE |		\
	 MV88E6XXX_FLAG_EEPROM |	\
	 MV88E6XXX_FLAG_PORTSTATE |	\
	 MV88E6XXX_FLAG_PPU_ACTIVE |	\
	 MV88E6XXX_FLAG_SMI_PHY |	\
	 MV88E6XXX_FLAG_SWITCH_MAC |	\
	 MV88E6XXX_FLAG_TEMP |		\
	 MV88E6XXX_FLAG_TEMP_LIMIT |	\
	 MV88E6XXX_FLAG_VLANTABLE |	\
	 MV88E6XXX_FLAG_VTU)

#define MV88E6XXX_FLAGS_FAMILY_6351	\
	(MV88E6XXX_FLAG_ATU |		\
	 MV88E6XXX_FLAG_PORTSTATE |	\
	 MV88E6XXX_FLAG_PPU_ACTIVE |	\
	 MV88E6XXX_FLAG_SMI_PHY |	\
	 MV88E6XXX_FLAG_STU |		\
	 MV88E6XXX_FLAG_SWITCH_MAC |	\
	 MV88E6XXX_FLAG_TEMP |		\
	 MV88E6XXX_FLAG_VLANTABLE |	\
	 MV88E6XXX_FLAG_VTU)

#define MV88E6XXX_FLAGS_FAMILY_6352	\
	(MV88E6XXX_FLAG_ATU |		\
	 MV88E6XXX_FLAG_EEE |		\
	 MV88E6XXX_FLAG_EEPROM |	\
	 MV88E6XXX_FLAG_PORTSTATE |	\
	 MV88E6XXX_FLAG_PPU_ACTIVE |	\
	 MV88E6XXX_FLAG_SMI_PHY |	\
	 MV88E6XXX_FLAG_STU |		\
	 MV88E6XXX_FLAG_SWITCH_MAC |	\
	 MV88E6XXX_FLAG_TEMP |		\
	 MV88E6XXX_FLAG_TEMP_LIMIT |	\
	 MV88E6XXX_FLAG_VLANTABLE |	\
	 MV88E6XXX_FLAG_VTU)

#if defined(MY_ABC_HERE)
#define MV88E6XXX_FLAGS_FAMILY_6390	\
	(MV88E6XXX_FLAG_ATU |		\
	 MV88E6XXX_FLAG_EEE |		\
	 MV88E6XXX_FLAG_EEPROM |	\
	 MV88E6XXX_FLAG_PORTSTATE |	\
	 MV88E6XXX_FLAG_PPU_ACTIVE |	\
	 MV88E6XXX_FLAG_SMI_PHY |	\
	 MV88E6XXX_FLAG_STU |		\
	 MV88E6XXX_FLAG_SWITCH_MAC |	\
	 MV88E6XXX_FLAG_TEMP |		\
	 MV88E6XXX_FLAG_TEMP_LIMIT |	\
	 MV88E6XXX_FLAG_VLANTABLE |	\
	 MV88E6XXX_FLAG_VTU)

#endif /* MY_ABC_HERE */
struct mv88e6xxx_info {
	enum mv88e6xxx_family family;
	u16 prod_num;
	const char *name;
	unsigned int num_databases;
	unsigned int num_ports;
	unsigned long flags;
};
#else /* MY_ABC_HERE */
struct mv88e6xxx_switch_id {
	u16 id;
	char *name;
};
#endif /* MY_ABC_HERE */

struct mv88e6xxx_atu_entry {
	u16	fid;
	u8	state;
	bool	trunk;
	u16	portv_trunkid;
	u8	mac[ETH_ALEN];
};

struct mv88e6xxx_vtu_stu_entry {
	/* VTU only */
	u16	vid;
	u16	fid;

	/* VTU and STU */
	u8	sid;
	bool	valid;
	u8	data[DSA_MAX_PORTS];
};

#if defined(MY_ABC_HERE)
struct mv88e6xxx_priv_port {
	struct net_device *bridge_dev;
};
#endif /* MY_ABC_HERE */

struct mv88e6xxx_priv_state {
#if defined(MY_ABC_HERE)
	const struct mv88e6xxx_info *info;

	/* The dsa_switch this private structure is related to */
	struct dsa_switch *ds;

	/* The device this structure is associated to */
	struct device *dev;
#endif /* MY_ABC_HERE */

	/* When using multi-chip addressing, this mutex protects
	 * access to the indirect access registers.  (In single-chip
	 * mode, this mutex is effectively useless.)
	 */
	struct mutex	smi_mutex;

#if defined(MY_ABC_HERE)
	/* The MII bus and the address on the bus that is used to
	 * communication with the switch
	 */
	struct mii_bus *bus;
	int sw_addr;
#endif /* MY_ABC_HERE */
#if (defined(CONFIG_NET_DSA_MV88E6XXX_NEED_PPU) && !defined(MY_ABC_HERE)) || defined(MY_ABC_HERE)
	/* Handles automatic disabling and re-enabling of the PHY
	 * polling unit.
	 */
	struct mutex		ppu_mutex;
	int			ppu_disabled;
	struct work_struct	ppu_work;
	struct timer_list	ppu_timer;
#endif /* MY_ABC_HERE */

	/* This mutex serialises access to the statistics unit.
	 * Hold this mutex over snapshot + dump sequences.
	 */
	struct mutex	stats_mutex;

	/* This mutex serializes phy access for chips with
	 * indirect phy addressing. It is unused for chips
	 * with direct phy access.
	 */
	struct mutex	phy_mutex;

	/* This mutex serializes eeprom access for chips with
	 * eeprom support.
	 */
	struct mutex eeprom_mutex;

#if defined(MY_ABC_HERE)
	struct mv88e6xxx_priv_port	ports[DSA_MAX_PORTS];
#else /* MY_ABC_HERE */
	int		id; /* switch product id */
	int		num_ports;	/* number of switch ports */
#endif /* MY_ABC_HERE */

#if defined(MY_ABC_HERE)
	/* A switch may have a GPIO line tied to its reset pin. Parse
	 * this from the device tree, and use it before performing
	 * switch soft reset.
	 */
	struct gpio_desc *reset;
#else /* MY_ABC_HERE */
	unsigned long port_state_update_mask;
	u8 port_state[DSA_MAX_PORTS];
#endif /* MY_ABC_HERE */

#if defined(MY_ABC_HERE)
	/* set to size of eeprom if supported by the switch */
	int		eeprom_len;

	/* Device node for the MDIO bus */
	struct device_node *mdio_np;

	/* And the MDIO bus itself */
	struct mii_bus *mdio_bus;
};

enum stat_type {
	BANK0,
	BANK1,
	PORT,
#else /* MY_ABC_HERE */
	struct work_struct bridge_work;
#endif /* MY_ABC_HERE */
};

struct mv88e6xxx_hw_stat {
	char string[ETH_GSTRING_LEN];
	int sizeof_stat;
	int reg;
#if defined(MY_ABC_HERE)
	enum stat_type type;
#endif /* MY_ABC_HERE */
};

#if defined(MY_ABC_HERE)
static inline bool mv88e6xxx_has(struct mv88e6xxx_priv_state *ps,
				 unsigned long flags)
{
	return (ps->info->flags & flags) == flags;
}
#else /* MY_ABC_HERE */
int mv88e6xxx_switch_reset(struct dsa_switch *ds, bool ppu_active);
char *mv88e6xxx_lookup_name(struct device *host_dev, int sw_addr,
			    const struct mv88e6xxx_switch_id *table,
			    unsigned int num);
int mv88e6xxx_setup_ports(struct dsa_switch *ds);
int mv88e6xxx_setup_common(struct dsa_switch *ds);
int mv88e6xxx_setup_global(struct dsa_switch *ds);
int mv88e6xxx_reg_read(struct dsa_switch *ds, int addr, int reg);
int mv88e6xxx_reg_write(struct dsa_switch *ds, int addr, int reg, u16 val);
int mv88e6xxx_set_addr_direct(struct dsa_switch *ds, u8 *addr);
int mv88e6xxx_set_addr_indirect(struct dsa_switch *ds, u8 *addr);
int mv88e6xxx_phy_read(struct dsa_switch *ds, int port, int regnum);
int mv88e6xxx_phy_write(struct dsa_switch *ds, int port, int regnum, u16 val);
int mv88e6xxx_phy_read_indirect(struct dsa_switch *ds, int port, int regnum);
int mv88e6xxx_phy_write_indirect(struct dsa_switch *ds, int port, int regnum,
				 u16 val);
void mv88e6xxx_ppu_state_init(struct dsa_switch *ds);
int mv88e6xxx_phy_read_ppu(struct dsa_switch *ds, int addr, int regnum);
int mv88e6xxx_phy_write_ppu(struct dsa_switch *ds, int addr,
			    int regnum, u16 val);
void mv88e6xxx_get_strings(struct dsa_switch *ds, int port, uint8_t *data);
void mv88e6xxx_get_ethtool_stats(struct dsa_switch *ds, int port,
				 uint64_t *data);
int mv88e6xxx_get_sset_count(struct dsa_switch *ds);
int mv88e6xxx_get_sset_count_basic(struct dsa_switch *ds);
void mv88e6xxx_adjust_link(struct dsa_switch *ds, int port,
			   struct phy_device *phydev);
int mv88e6xxx_get_regs_len(struct dsa_switch *ds, int port);
void mv88e6xxx_get_regs(struct dsa_switch *ds, int port,
			struct ethtool_regs *regs, void *_p);
int mv88e6xxx_get_temp(struct dsa_switch *ds, int *temp);
int mv88e6xxx_get_temp_limit(struct dsa_switch *ds, int *temp);
int mv88e6xxx_set_temp_limit(struct dsa_switch *ds, int temp);
int mv88e6xxx_get_temp_alarm(struct dsa_switch *ds, bool *alarm);
int mv88e6xxx_eeprom_load_wait(struct dsa_switch *ds);
int mv88e6xxx_eeprom_busy_wait(struct dsa_switch *ds);
int mv88e6xxx_phy_read_indirect(struct dsa_switch *ds, int addr, int regnum);
int mv88e6xxx_phy_write_indirect(struct dsa_switch *ds, int addr, int regnum,
				 u16 val);
int mv88e6xxx_get_eee(struct dsa_switch *ds, int port, struct ethtool_eee *e);
int mv88e6xxx_set_eee(struct dsa_switch *ds, int port,
		      struct phy_device *phydev, struct ethtool_eee *e);
int mv88e6xxx_port_bridge_join(struct dsa_switch *ds, int port, u32 members);
int mv88e6xxx_port_bridge_leave(struct dsa_switch *ds, int port, u32 members);
int mv88e6xxx_port_stp_update(struct dsa_switch *ds, int port, u8 state);
int mv88e6xxx_port_vlan_prepare(struct dsa_switch *ds, int port,
				const struct switchdev_obj_port_vlan *vlan,
				struct switchdev_trans *trans);
int mv88e6xxx_port_vlan_add(struct dsa_switch *ds, int port,
			    const struct switchdev_obj_port_vlan *vlan,
			    struct switchdev_trans *trans);
int mv88e6xxx_port_vlan_del(struct dsa_switch *ds, int port,
			    const struct switchdev_obj_port_vlan *vlan);
int mv88e6xxx_port_pvid_get(struct dsa_switch *ds, int port, u16 *vid);
int mv88e6xxx_vlan_getnext(struct dsa_switch *ds, u16 *vid,
			   unsigned long *ports, unsigned long *untagged);
int mv88e6xxx_port_fdb_prepare(struct dsa_switch *ds, int port,
			       const struct switchdev_obj_port_fdb *fdb,
			       struct switchdev_trans *trans);
int mv88e6xxx_port_fdb_add(struct dsa_switch *ds, int port,
			   const struct switchdev_obj_port_fdb *fdb,
			   struct switchdev_trans *trans);
int mv88e6xxx_port_fdb_del(struct dsa_switch *ds, int port,
			   const struct switchdev_obj_port_fdb *fdb);
int mv88e6xxx_port_fdb_dump(struct dsa_switch *ds, int port,
			    struct switchdev_obj_port_fdb *fdb,
			    int (*cb)(struct switchdev_obj *obj));
int mv88e6xxx_phy_page_read(struct dsa_switch *ds, int port, int page, int reg);
int mv88e6xxx_phy_page_write(struct dsa_switch *ds, int port, int page,
			     int reg, int val);

extern struct dsa_switch_driver mv88e6131_switch_driver;
extern struct dsa_switch_driver mv88e6123_61_65_switch_driver;
extern struct dsa_switch_driver mv88e6352_switch_driver;
extern struct dsa_switch_driver mv88e6171_switch_driver;

#define REG_READ(addr, reg)						\
	({								\
		int __ret;						\
									\
		__ret = mv88e6xxx_reg_read(ds, addr, reg);		\
		if (__ret < 0)						\
			return __ret;					\
		__ret;							\
	})

#define REG_WRITE(addr, reg, val)					\
	({								\
		int __ret;						\
									\
		__ret = mv88e6xxx_reg_write(ds, addr, reg, val);	\
		if (__ret < 0)						\
			return __ret;					\
	})


#endif /* MY_ABC_HERE */

#endif
