#
# Makefile for the Linux Kernel SOC specific device drivers.
#

obj-$(CONFIG_SOC_BRCMSTB)	+= brcmstb/
obj-$(CONFIG_MACH_DOVE)		+= dove/
obj-$(CONFIG_ARCH_MEDIATEK)	+= mediatek/
obj-$(CONFIG_ARCH_QCOM)		+= qcom/
obj-$(CONFIG_ARCH_ROCKCHIP)		+= rockchip/
obj-$(CONFIG_ARCH_SUNXI)	+= sunxi/
obj-$(CONFIG_ARCH_TEGRA)	+= tegra/
obj-$(CONFIG_SOC_TI)		+= ti/
obj-$(CONFIG_PLAT_VERSATILE)	+= versatile/
ifeq ($(CONFIG_SYNO_LSP_RTD129X),y)
obj-$(CONFIG_ARCH_RTD129X)	+= realtek/
endif # CONFIG_SYNO_LSP_RTD129X
ifeq ($(CONFIG_SYNO_LSP_RTD1619), y)
obj-$(CONFIG_RTK_PLATFORM)      += realtek/
endif # CONFIG_SYNO_LSP_RTD1619
