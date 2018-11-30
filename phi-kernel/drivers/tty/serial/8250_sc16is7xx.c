/* * Copyright (c) Intel Corporation (2011).
*
* Disclaimer: The codes contained in these modules may be specific to the
* Intel Software Development Platform codenamed: Knights Ferry, and the 
* Intel product codenamed: Knights Corner, and are not backward compatible 
* with other Intel products. Additionally, Intel will NOT support the codes 
* or instruction set in future products.
*
* Intel offers no warranty of any kind regarding the code.  This code is
* licensed on an "AS IS" basis and Intel is not obligated to provide any support,
* assistance, installation, training, or other services of any kind.  Intel is 
* also not obligated to provide any updates, enhancements or extensions.  Intel 
* specifically disclaims any warranty of merchantability, non-infringement, 
* fitness for any particular purpose, and any other warranty.
*
* Further, Intel disclaims all liability of any kind, including but not
* limited to liability for infringement of any proprietary rights, relating
* to the use of the code, even if Intel is notified of the possibility of
* such liability.  Except as expressly stated in an Intel license agreement
* provided with this code and agreed upon with Intel, no license, express
* or implied, by estoppel or otherwise, to any intellectual property rights
* is granted herein.
*/

#include <linux/i2c.h>
#include <linux/console.h>
#include "8250.h"
#ifdef	CONFIG_KDB
#include <linux/kdb.h>
#endif

/* Easier for compile time module, we don't actually need a UPIO_I2C */
#ifndef SC16IS_COMPILETIME

#if 0
static char *test_regs_wr[] = {
	"UART_TX/DLL",
	"UART_IER/DLM",
	"UART_FCR",
	"UART_LCR",
	"UART_MCR",
	"UART_BAD",
	"UART_BAD",
	"UART_SCR"
};

static char *test_regs_rd[] = {
	"UART_RX/DLL",
	"UART_IER/DLM",
	"UART_IIR",
	"UART_LCR",
	"UART_MCR",
	"UART_LSR",
	"UART_MSR",
	"UART_SCR"
};
#endif

unsigned int sc16is_serial_in(struct uart_port *port, int offset)
{
	struct i2c_client *client = (struct i2c_client *)port->private_data;
	s32 value = -EAGAIN;

	do {
		value = i2c_smbus_read_byte_data(client, (u8)offset << port->regshift);
	} while (0);
	//} while (value == -EAGAIN);

	if (value < 0) {
		//panic("serial_in bad %s (%d), %d\n", test_regs_rd[offset], offset, value);
		//printk(KERN_INFO "serial_in bad %s (%d), %d\n", test_regs_rd[offset], offset, value);
	} else {
		//printk(KERN_INFO "serial_in %s (%d), 0x%x\n", test_regs_rd[offset], offset, value);
	}
	if (value == (-EAGAIN) && offset == 2)
		return 1;

	return (value & 0xFF);
}

void sc16is_serial_out(struct uart_port *port, int offset, int value)
{
	struct i2c_client *client = (struct i2c_client *)port->private_data;
	s32 val = -EAGAIN;

	do {
		val = i2c_smbus_write_byte_data(client, (u8)offset << port->regshift, (u8)value);
	} while(0);
	//	} while(val == -EAGAIN);

	if (val < 0)   {
		//panic("serial_out %s (%d) = 0x%x\n", test_regs_wr[offset], offset, value);
		//printk(KERN_INFO "serial_out %s (%d) = 0x%x\n", test_regs_wr[offset], offset, value);
	}
	//printk(KERN_INFO "serial_out %s (%d) = 0x%x\n", test_regs_wr[offset], offset, value);
}
#endif

extern struct uart_port kdb_uart_port;
extern int kdb_serial_line;

static int __devinit sc16is7xx_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct uart_port port;
	int error = 0;
	memset(&port, 0, sizeof(struct uart_port));
	/* Setup the port data structure */
#ifdef SC16IS_COMPILETIME
	/* TBD if we do this at all */
	port.iotype = UPIO_I2C;
#else
#define UPIO_SC16IS 0xff
	port.iotype = UPIO_SC16IS;
#endif
	port.serial_in = &sc16is_serial_in;
	port.serial_out = &sc16is_serial_out;
	port.irq = 0;
	port.uartclk = 1843200;
	port.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_NO_TXEN_TEST; /* TODO: need to study flags */
	port.fifosize = 64;
	port.regshift = 3;
	port.private_data = client;
	port.iobase = 0x800000000UL;
	port.mapbase = 0x800000000UL;
	port.membase = (char *)0x800000000UL;
	port.timeout = 0xff;
	//if (early_serial_setup(&port) < 0) {
	//	printk(KERN_INFO "BAW fail 1\n");
	//}
	error = serial8250_register_port(&port);
	printk(KERN_INFO "8250 UART MIC Port error %d\n", error);

#ifdef	CONFIG_KDB

	/*
	 * Remember the line number of the first serial
	 * console.  We'll make this the kdb serial console too.
	 */
	//printk("Configuring KDB\n");

//	if (co && kdb_serial_line == -1) {
		kdb_serial_line = port.line;
		kdb_serial.io_type = port.iotype;
		switch (port.iotype) {
		case SERIAL_IO_MEM:
#ifdef  SERIAL_IO_MEM32
		case SERIAL_IO_MEM32:
#endif
			kdb_serial.iobase = (unsigned long)(port.membase);
			kdb_serial.ioreg_shift = port.regshift;
			break;
		default:
			kdb_serial.iobase = port.iobase;
			kdb_serial.ioreg_shift = port.regshift;
			break;
		}
		//printk("  kdb_serial.io_type = %d\n", kdb_serial.io_type);
		//printk("  kdb_serial.iobase = 0x%lx\n", kdb_serial.iobase);
		//printk("  kdb_serial.ioreg_shift = %d\n", kdb_serial.ioreg_shift);

	memcpy(&kdb_uart_port, &port, sizeof(port));
//	}
#endif	/* CONFIG_KDB */

	return error;
}

static const struct i2c_device_id sc16is7xx_id[] = {
	{ "sc16is7xx", 0 },
	{ "sc16is740", 0 },
	{ }
};

extern int __devinit sc16is7xx_probe(struct i2c_client *client,
		const struct i2c_device_id *id);

static struct i2c_driver sc16is7xx_driver = {
	.driver = {
		.name	= "sc16is7xx",
	},
	.probe		= sc16is7xx_probe,
	.remove		= NULL, /* TODO: */
	.id_table	= sc16is7xx_id,
};

static int __init sc16is7xx_init(void)
{
	int error = i2c_add_driver(&sc16is7xx_driver);
	printk(KERN_INFO "sc16is7xx_init adding driver %d\n", error);
	return error;
}

module_init(sc16is7xx_init);

static void __exit sc16is7xx_exit(void)
{
	i2c_del_driver(&sc16is7xx_driver);
}
module_exit(sc16is7xx_exit);
