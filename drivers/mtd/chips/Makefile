#
# linux/drivers/chips/Makefile
#

obj-$(CONFIG_MTD)		+= chipreg.o
obj-$(CONFIG_MTD_CFI)		+= cfi_probe.o
obj-$(CONFIG_MTD_CFI_UTIL)	+= cfi_util.o
obj-$(CONFIG_MTD_CFI_STAA)	+= cfi_cmdset_0020.o
obj-$(CONFIG_MTD_CFI_AMDSTD)	+= cfi_cmdset_0002.o
obj-$(CONFIG_MTD_CFI_INTELEXT)	+= cfi_cmdset_0001.o
obj-$(CONFIG_MTD_GEN_PROBE)	+= gen_probe.o
obj-$(CONFIG_MTD_JEDECPROBE)	+= jedec_probe.o
obj-$(CONFIG_MTD_RAM)		+= map_ram.o
obj-$(CONFIG_MTD_ROM)		+= map_rom.o
obj-$(CONFIG_MTD_ABSENT)	+= map_absent.o
ifeq ($(CONFIG_SYNO_LSP_RTD129X), y)
obj-$(CONFIG_MTD_RTK_SFC)	+= rtk_sfc.o
endif # CONFIG_SYNO_LSP_RTD129X
ifeq ($(CONFIG_SYNO_LSP_RTD1619), y)
obj-$(CONFIG_MTD_RTK_SFC)	+= rtk_sfc_1619.o
obj-$(CONFIG_MTD_RTK_SFC_PSTORE)+= rtk_sfc_pstore.o
endif # CONFIG_SYNO_LSP_RTD1619
