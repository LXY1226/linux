#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
#include <linux/console.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/screen_info.h>
#include <linux/usb/ch9.h>
#include <linux/pci_regs.h>
#include <linux/pci_ids.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/fcntl.h>
#include <asm/setup.h>
#include <xen/hvc-console.h>
#include <asm/pci-direct.h>
#include <asm/fixmap.h>
#include <asm/intel-mid.h>
#include <asm/pgtable.h>
#include <linux/usb/ehci_def.h>
#include <linux/efi.h>
#include <asm/efi.h>
#include <asm/pci_x86.h>

/* Simple VGA output */
#define VGABASE		(__ISA_IO_base + 0xb8000)

static int max_ypos = 25, max_xpos = 80;
static int current_ypos = 25, current_xpos;

static void early_vga_write(struct console *con, const char *str, unsigned n)
{
	char c;
	int  i, k, j;

	while ((c = *str++) != '\0' && n-- > 0) {
		if (current_ypos >= max_ypos) {
			/* scroll 1 line up */
			for (k = 1, j = 0; k < max_ypos; k++, j++) {
				for (i = 0; i < max_xpos; i++) {
					writew(readw(VGABASE+2*(max_xpos*k+i)),
					       VGABASE + 2*(max_xpos*j + i));
				}
			}
			for (i = 0; i < max_xpos; i++)
				writew(0x720, VGABASE + 2*(max_xpos*j + i));
			current_ypos = max_ypos-1;
		}
#ifdef CONFIG_KGDB_KDB
		if (c == '\b') {
			if (current_xpos > 0)
				current_xpos--;
		} else if (c == '\r') {
			current_xpos = 0;
		} else
#endif
		if (c == '\n') {
			current_xpos = 0;
			current_ypos++;
		} else if (c != '\r')  {
			writew(((0x7 << 8) | (unsigned short) c),
			       VGABASE + 2*(max_xpos*current_ypos +
						current_xpos++));
			if (current_xpos >= max_xpos) {
				current_xpos = 0;
				current_ypos++;
			}
		}
	}
}

static struct console early_vga_console = {
	.name =		"earlyvga",
	.write =	early_vga_write,
	.flags =	CON_PRINTBUFFER,
	.index =	-1,
};

/* Serial functions loosely based on a similar package from Klaus P. Gerlicher */

static unsigned long early_serial_base = 0x3f8;  /* ttyS0 */

#define XMTRDY          0x20
#if defined(MY_DEF_HERE) || defined(MY_DEF_HERE)
#define TEMT		0x40
#define THRE		XMTRDY
#define BOTH_EMPTY 	(TEMT | THRE)
#endif /* MY_DEF_HERE || MY_DEF_HERE */

#define DLAB		0x80

#define TXR             0       /*  Transmit register (WRITE) */
#define RXR             0       /*  Receive register  (READ)  */
#define IER             1       /*  Interrupt Enable          */
#define IIR             2       /*  Interrupt ID              */
#define FCR             2       /*  FIFO control              */
#define LCR             3       /*  Line control              */
#define MCR             4       /*  Modem control             */
#define LSR             5       /*  Line Status               */
#define MSR             6       /*  Modem Status              */
#define DLL             0       /*  Divisor Latch Low         */
#define DLH             1       /*  Divisor latch High        */

static unsigned int io_serial_in(unsigned long addr, int offset)
{
	return inb(addr + offset);
}

static void io_serial_out(unsigned long addr, int offset, int value)
{
	outb(value, addr + offset);
}

static unsigned int (*serial_in)(unsigned long addr, int offset) = io_serial_in;
static void (*serial_out)(unsigned long addr, int offset, int value) = io_serial_out;

static int early_serial_putc(unsigned char ch)
{
	unsigned timeout = 0xffff;

	while ((serial_in(early_serial_base, LSR) & XMTRDY) == 0 && --timeout)
		cpu_relax();
	serial_out(early_serial_base, TXR, ch);
	return timeout ? 0 : -1;
}

static void early_serial_write(struct console *con, const char *s, unsigned n)
{
	while (*s && n-- > 0) {
		if (*s == '\n')
			early_serial_putc('\r');
		early_serial_putc(*s);
		s++;
	}
}

static __init void early_serial_hw_init(unsigned divisor)
{
	unsigned char c;

	serial_out(early_serial_base, LCR, 0x3);	/* 8n1 */
	serial_out(early_serial_base, IER, 0);	/* no interrupt */
	serial_out(early_serial_base, FCR, 0);	/* no fifo */
	serial_out(early_serial_base, MCR, 0x3);	/* DTR + RTS */

	c = serial_in(early_serial_base, LCR);
	serial_out(early_serial_base, LCR, c | DLAB);
	serial_out(early_serial_base, DLL, divisor & 0xff);
	serial_out(early_serial_base, DLH, (divisor >> 8) & 0xff);
	serial_out(early_serial_base, LCR, c & ~DLAB);
}

#define DEFAULT_BAUD 9600

static __init void early_serial_init(char *s)
{
	unsigned divisor;
	unsigned long baud = DEFAULT_BAUD;
	char *e;

	if (*s == ',')
		++s;

	if (*s) {
		unsigned port;
		if (!strncmp(s, "0x", 2)) {
			early_serial_base = simple_strtoul(s, &e, 16);
		} else {
			static const int __initconst bases[] = { 0x3f8, 0x2f8 };

			if (!strncmp(s, "ttyS", 4))
				s += 4;
			port = simple_strtoul(s, &e, 10);
			if (port > 1 || s == e)
				port = 0;
			early_serial_base = bases[port];
		}
		s += strcspn(s, ",");
		if (*s == ',')
			s++;
	}

	if (*s) {
		baud = simple_strtoull(s, &e, 0);

		if (baud == 0 || s == e)
			baud = DEFAULT_BAUD;
	}

	/* Convert from baud to divisor value */
	divisor = 115200 / baud;

	/* These will always be IO based ports */
	serial_in = io_serial_in;
	serial_out = io_serial_out;

	/* Set up the HW */
	early_serial_hw_init(divisor);
}

#ifdef CONFIG_PCI
static void mem32_serial_out(unsigned long addr, int offset, int value)
{
	u32 __iomem *vaddr = (u32 __iomem *)addr;
	/* shift implied by pointer type */
	writel(value, vaddr + offset);
}

static unsigned int mem32_serial_in(unsigned long addr, int offset)
{
	u32 __iomem *vaddr = (u32 __iomem *)addr;
	/* shift implied by pointer type */
	return readl(vaddr + offset);
}

#if defined(MY_DEF_HERE) || defined(MY_DEF_HERE)
static void early_serial_hw_deinit(void)
{
	unsigned long timeout_jiffies = jiffies + msecs_to_jiffies(2000);
	while ((serial_in(early_serial_base, LSR) & BOTH_EMPTY) != BOTH_EMPTY) {
		if (time_after(jiffies, timeout_jiffies)) {
			break;
		}
	}
	serial_out(early_serial_base, IER, 0);	/* no interrupt */
	serial_out(early_serial_base, FCR, 0);	/* no fifo */
}
#endif /* MY_DEF_HERE || MY_DEF_HERE */

static struct console early_serial_console = {
	.name =		"earlyser",
	.write =	early_serial_write,
	.flags =	CON_PRINTBUFFER,
	.index =	-1,
#if defined(MY_DEF_HERE) || defined(MY_DEF_HERE)
	.pcimapaddress = 0,
	.pcimapsize = 0,
	.deinit = early_serial_hw_deinit,
#endif /* MY_DEF_HERE  || MY_DEF_HERE*/
};


#ifdef MY_DEF_HERE
static __init void early_mmio_serial_init(char *s)
{
        unsigned divisor;
        unsigned long addr;
        unsigned long baud = 115200;         /* Default baud 115200 */
        unsigned long base_clock = 1843200;  /* Default clock 1.84M */
        char *e;

        if (*s == ',')
                ++s;

        if (!strncmp(s, "0x", 2)) {
                addr = simple_strtoul(s, &e, 16);
        }

        s = e;

        if (*s == ',')
                ++s;

        baud = simple_strtoul(s, &e, 10);

        s = e;

        if (*s == ',')
                ++s;

        base_clock = simple_strtoul(s, &e, 10);


        early_serial_base = (unsigned long)early_ioremap(addr, 0x10);


        serial_in = mem32_serial_in;
        serial_out = mem32_serial_out;

        early_serial_console.pcimapaddress = (void __iomem *)early_serial_base;
        early_serial_console.pcimapsize = 0x10;

        divisor = (base_clock / 16) / baud;

        early_serial_hw_init(divisor);
}

/*
 * early_pcifull_serial_init()
 *
 * This function is invoked when the early_printk param starts with "pciserial"
 * The rest of the param should be ",B:D.F,baud" where B, D & F describe the
 * location of a PCI device that must be a UART device.
 */
static __init void early_pcifull_serial_init(char *s)
{
	unsigned divisor;
	unsigned long baud = DEFAULT_BAUD;
	u8 bus, slot, func;
	u8 htype, secondbus;
	u32 classcode, bar0;
	u16 cmdreg;
	char *e;

	/*
	 * First, part the param to get the BDF values
	 */
	if (*s == ',')
		++s;

	if (*s == 0)
		return;

	bus = (u8)simple_strtoul(s, &e, 16);
	s = e;
	if (*s != ':')
		return;
	++s;
	slot = (u8)simple_strtoul(s, &e, 16);
	s = e;
	if (*s != '.')
		return;
	++s;
	func = (u8)simple_strtoul(s, &e, 16);
	s = e;

	htype = read_pci_config_byte(bus, slot, func, PCI_HEADER_TYPE);
	while((htype & 0x7F) == PCI_HEADER_TYPE_BRIDGE ||
		  (htype & 0x7F) == PCI_HEADER_TYPE_CARDBUS ){

		secondbus = read_pci_config_byte(bus, slot, func, PCI_SECONDARY_BUS);
		if(secondbus == 0xFF)
			return;
		bus = secondbus;

		if (*s != ',')
			return;
		++s;

		slot = (u8)simple_strtoul(s, &e, 16);
		s = e;
		if (*s != '.')
			return;
		++s;

		func = (u8)simple_strtoul(s, &e, 16);
		s = e;

		htype = read_pci_config_byte(bus, slot, func, PCI_HEADER_TYPE);
	}

	if ((htype & 0x7F) != PCI_HEADER_TYPE_NORMAL)
		return ;

	/* A baud might be following */
	if (*s == ',')
		s++;

	/*
	 * Second, find the device from the BDF
	 */
	cmdreg = read_pci_config(bus, slot, func, PCI_COMMAND);
	classcode = read_pci_config(bus, slot, func, PCI_CLASS_REVISION);
	bar0 = read_pci_config(bus, slot, func, PCI_BASE_ADDRESS_0);

	/*
	 * Determine if it is IO or memory mapped
	 */
	if (bar0 & 0x01) {
		/* it is IO mapped */
		serial_in = io_serial_in;
		serial_out = io_serial_out;
		early_serial_base = bar0&0xfffffffc;
		write_pci_config(bus, slot, func, PCI_COMMAND,
						cmdreg|PCI_COMMAND_IO);
	} else {
		/* It is memory mapped - assume 32-bit alignment */
		serial_in = mem32_serial_in;
		serial_out = mem32_serial_out;
		/* WARNING! assuming the address is always in the first 4G */
		early_serial_base =
			(unsigned long)early_ioremap(bar0 & 0xfffffff0, 0x10);
		early_serial_console.pcimapaddress = (void __iomem *)early_serial_base;
		/* base on pci spec with serial console */
		early_serial_console.pcimapsize = 0x10;
		write_pci_config(bus, slot, func, PCI_COMMAND,
						cmdreg|PCI_COMMAND_MEMORY);
	}

	/*
	 * Lastly, initalize the hardware
	 */
	if (*s) {
		if (strcmp(s, "nocfg") == 0)
			/* Sometimes, we want to leave the UART alone
			 * and assume the BIOS has set it up correctly.
			 * "nocfg" tells us this is the case, and we
			 * should do no more setup.
			 */
			return;
		if (kstrtoul(s, 0, &baud) < 0 || baud == 0)
			baud = DEFAULT_BAUD;
	}

	/* Convert from baud to divisor value */
	divisor = 115200 / baud;

	/* Set up the HW */
	early_serial_hw_init(divisor);
}
#endif /* MY_DEF_HERE */

/*
 * early_pci_serial_init()
 *
 * This function is invoked when the early_printk param starts with "pciserial"
 * The rest of the param should be ",B:D.F,baud" where B, D & F describe the
 * location of a PCI device that must be a UART device.
 */
static __init void early_pci_serial_init(char *s)
{
	unsigned divisor;
	unsigned long baud = DEFAULT_BAUD;
	u8 bus, slot, func;
	u32 classcode, bar0;
	u16 cmdreg;
	char *e;


	/*
	 * First, part the param to get the BDF values
	 */
	if (*s == ',')
		++s;

	if (*s == 0)
		return;

	bus = (u8)simple_strtoul(s, &e, 16);
	s = e;
	if (*s != ':')
		return;
	++s;
	slot = (u8)simple_strtoul(s, &e, 16);
	s = e;
	if (*s != '.')
		return;
	++s;
	func = (u8)simple_strtoul(s, &e, 16);
	s = e;

	/* A baud might be following */
	if (*s == ',')
		s++;

	/*
	 * Second, find the device from the BDF
	 */
	cmdreg = read_pci_config(bus, slot, func, PCI_COMMAND);
	classcode = read_pci_config(bus, slot, func, PCI_CLASS_REVISION);
	bar0 = read_pci_config(bus, slot, func, PCI_BASE_ADDRESS_0);

	/*
	 * Verify it is a UART type device
	 */
#ifdef MY_DEF_HERE
#else
	if (((classcode >> 16 != PCI_CLASS_COMMUNICATION_MODEM) &&
	     (classcode >> 16 != PCI_CLASS_COMMUNICATION_SERIAL)) ||
	   (((classcode >> 8) & 0xff) != 0x02)) /* 16550 I/F at BAR0 */
		return;

#endif
	/*
	 * Determine if it is IO or memory mapped
	 */
	if (bar0 & 0x01) {
		/* it is IO mapped */
		serial_in = io_serial_in;
		serial_out = io_serial_out;
		early_serial_base = bar0&0xfffffffc;
		write_pci_config(bus, slot, func, PCI_COMMAND,
						cmdreg|PCI_COMMAND_IO);
	} else {
		/* It is memory mapped - assume 32-bit alignment */
		serial_in = mem32_serial_in;
		serial_out = mem32_serial_out;
		/* WARNING! assuming the address is always in the first 4G */
		early_serial_base =
			(unsigned long)early_ioremap(bar0 & 0xfffffff0, 0x10);
		write_pci_config(bus, slot, func, PCI_COMMAND,
						cmdreg|PCI_COMMAND_MEMORY);
	}

#ifdef MY_DEF_HERE
	early_serial_console.pcimapaddress = (void __iomem *)early_serial_base;
	/* base on pci spec with serial console */
	early_serial_console.pcimapsize = 0x10;
#endif
	/*
	 * Lastly, initalize the hardware
	 */
	if (*s) {
		if (strcmp(s, "nocfg") == 0)
			/* Sometimes, we want to leave the UART alone
			 * and assume the BIOS has set it up correctly.
			 * "nocfg" tells us this is the case, and we
			 * should do no more setup.
			 */
			return;
		if (kstrtoul(s, 0, &baud) < 0 || baud == 0)
			baud = DEFAULT_BAUD;
	}

	/* Convert from baud to divisor value */
	divisor = 115200 / baud;

	/* Set up the HW */
	early_serial_hw_init(divisor);
}
#endif

#ifdef MY_DEF_HERE
static __init void apl_serial_hw_init(unsigned divisor)
{
	//
	// Configure baud rate
	//
	serial_out(early_serial_base, LCR, DLAB);
	serial_out(early_serial_base, DLL, divisor & 0xff);
	serial_out(early_serial_base, DLH, (divisor >> 8) & 0xff);

	//
	// Configure Line control and switch back to bank 0
	//
	serial_out(early_serial_base, LCR, 0x3 & 0x1f);

	//
	// Enable and reset FIFOs
	//
	serial_out(early_serial_base, FCR, 1);

	//
	// Put Modem Control Register(MCR) into its reset state of 0x00.
	//
	serial_out(early_serial_base, MCR, 1);
}

#define EARLY_PRINTK_APL_BUS 0
#define EARLY_PRINTK_APL_SLOT 24
#define EARLY_PRINTK_APL_FUNC 2
/*
 * early_apl_serial_init()
 *
 * This function is invoked when the early_printk param starts with "apl"
 * The rest of the param should be ",B:D.F,baud" where B, D & F describe the
 * location of a PCI device that must be a UART device.
 */
static __init void early_apl_serial_init(void)
{
	unsigned divisor;
	unsigned long baud = 115200;
	u32 classcode, bar0;
	u16 cmdreg;

	/*
	 * Second, find the device from the BDF
	 */
	cmdreg = read_pci_config(EARLY_PRINTK_APL_BUS, EARLY_PRINTK_APL_SLOT, EARLY_PRINTK_APL_FUNC, PCI_COMMAND);
	classcode = read_pci_config(EARLY_PRINTK_APL_BUS, EARLY_PRINTK_APL_SLOT, EARLY_PRINTK_APL_FUNC, PCI_CLASS_REVISION);
	bar0 = read_pci_config(EARLY_PRINTK_APL_BUS, EARLY_PRINTK_APL_SLOT, EARLY_PRINTK_APL_FUNC, PCI_BASE_ADDRESS_0);

	/*
	 * Determine if it is IO or memory mapped
	 */
	/* It is memory mapped - assume 32-bit alignment */
	serial_in = mem32_serial_in;
	serial_out = mem32_serial_out;

	/* WARNING! assuming the address is always in the first 4G */
	early_serial_base =
		(unsigned long)early_ioremap(bar0 & 0xfffffff0, 0x10);
	write_pci_config(EARLY_PRINTK_APL_BUS, EARLY_PRINTK_APL_SLOT, EARLY_PRINTK_APL_FUNC, PCI_COMMAND,
			cmdreg|PCI_COMMAND_MEMORY);

	early_serial_console.pcimapaddress = (void __iomem *)early_serial_base;
	early_serial_console.pcimapsize = 0x10;

	/* Convert from baud to divisor value */
	divisor = 115200 / baud;

	/* Set up the HW */
	apl_serial_hw_init(divisor);
}
#endif /* MY_DEF_HERE */

static void early_console_register(struct console *con, int keep_early)
{
	if (con->index != -1) {
		printk(KERN_CRIT "ERROR: earlyprintk= %s already used\n",
		       con->name);
		return;
	}
	early_console = con;
	if (keep_early)
		early_console->flags &= ~CON_BOOT;
	else
		early_console->flags |= CON_BOOT;
	register_console(early_console);
}

#ifdef MY_DEF_HERE
int __init setup_early_printk(char *buf)
#else
static int __init setup_early_printk(char *buf)
#endif /* MY_DEF_HERE */
{
	int keep;

	if (!buf)
		return 0;

	if (early_console)
		return 0;

	keep = (strstr(buf, "keep") != NULL);

	while (*buf != '\0') {
		if (!strncmp(buf, "serial", 6)) {
			buf += 6;
			early_serial_init(buf);
			early_console_register(&early_serial_console, keep);
			if (!strncmp(buf, ",ttyS", 5))
				buf += 5;
		}
		if (!strncmp(buf, "ttyS", 4)) {
			early_serial_init(buf + 4);
			early_console_register(&early_serial_console, keep);
		}
#ifdef CONFIG_PCI
		if (!strncmp(buf, "pciserial", 9)) {
			early_pci_serial_init(buf + 9);
			early_console_register(&early_serial_console, keep);
			buf += 9; /* Keep from match the above "serial" */
		}
#endif
#ifdef MY_DEF_HERE
		if (!strncmp(buf, "mmio", 4)) {
			early_mmio_serial_init(buf + 4);
			early_console_register(&early_serial_console, keep);
			buf += 4; /* Keep from match the above "serial" */
		}

		if (!strncmp(buf, "pcifull", 7)) {
			early_pcifull_serial_init(buf + 7);
			early_console_register(&early_serial_console, keep);
			buf += 7; /* Keep from match the above "serial" */
		}
#endif /* MY_DEF_HERE */
		if (!strncmp(buf, "vga", 3) &&
		    boot_params.screen_info.orig_video_isVGA == 1) {
			max_xpos = boot_params.screen_info.orig_video_cols;
			max_ypos = boot_params.screen_info.orig_video_lines;
			current_ypos = boot_params.screen_info.orig_y;
			early_console_register(&early_vga_console, keep);
		}
#ifdef CONFIG_EARLY_PRINTK_DBGP
		if (!strncmp(buf, "dbgp", 4) && !early_dbgp_init(buf + 4))
			early_console_register(&early_dbgp_console, keep);
#endif
#ifdef CONFIG_HVC_XEN
		if (!strncmp(buf, "xen", 3))
			early_console_register(&xenboot_console, keep);
#endif
#ifdef CONFIG_EARLY_PRINTK_EFI
		if (!strncmp(buf, "efi", 3))
			early_console_register(&early_efi_console, keep);
#endif
#ifdef MY_DEF_HERE
		if (!strncmp(buf, "apl", 3)) {
			early_apl_serial_init();
			early_console_register(&early_serial_console, keep);
		}
#endif /* MY_DEF_HERE */

		buf++;
	}
	return 0;
}

#ifdef MY_DEF_HERE
EXPORT_SYMBOL(setup_early_printk);
#endif /* MY_DEF_HERE */
early_param("earlyprintk", setup_early_printk);
