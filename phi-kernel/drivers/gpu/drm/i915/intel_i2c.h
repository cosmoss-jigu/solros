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

#define CREATE_TRACE_POINTS
#include "gmbus_trace.h"

#define	GMBUS_I2C_TIMEOUT_ERR	-1
#define GMBUS_I2C_SACK_TO_ERR	-2
#define GMBUS_I2C_INTERRUPTED	-3
#define GMBUS_I2C_INVALID_CMD	-4

#define DBOX_BASE               0x08007C0000ULL
#define COMMON_MMIO_BOX_SIZE    (1<<16)

#define GMBUS0			0x5100 /* clock/port select */
#define   GMBUS_RATE_100KHZ	(0<<8)
#define   GMBUS_RATE_50KHZ	(1<<8)
#define   GMBUS_RATE_400KHZ	(2<<8) /* reserved on Pineview */
#define   GMBUS_RATE_1MHZ	(3<<8) /* reserved on Pineview */
#define   GMBUS_HOLD_EXT	(1<<7) /* 300ns hold time, rsvd on Pineview */
#define   GMBUS_PORT_DISABLED	0
#define   GMBUS_PORT_SSC	1
#define   GMBUS_PORT_VGADDC	2
#define   GMBUS_PORT_PANEL	3
#define   GMBUS_PORT_DPC	4 /* HDMIC */
#define   GMBUS_PORT_DPB	5 /* SDVO, HDMIB */
				  /* 6 reserved */
#define   GMBUS_PORT_DPD	7 /* HDMID */
#define   GMBUS_NUM_PORTS       8
#define GMBUS1			0x5104 /* command/status */
#define   GMBUS_SW_CLR_INT	(1<<31)
#define   GMBUS_SW_RDY		(1<<30)
#define   GMBUS_ENT		(1<<29) /* enable timeout */
#define   GMBUS_CYCLE_NONE	(0<<25)
#define   GMBUS_CYCLE_WAIT	(1<<25)
#define   GMBUS_CYCLE_INDEX	(2<<25)
#define   GMBUS_CYCLE_STOP	(4<<25)
#define   GMBUS_BYTE_COUNT_SHIFT 16
#define   GMBUS_SLAVE_INDEX_SHIFT 8
#define   GMBUS_SLAVE_ADDR_SHIFT 1
#define   GMBUS_SLAVE_READ	(1<<0)
#define   GMBUS_SLAVE_WRITE	(0<<0)
#define GMBUS2			0x5108 /* status */
#define   GMBUS_INUSE		(1<<15)
#define   GMBUS_HW_WAIT_PHASE	(1<<14)
#define   GMBUS_STALL_TIMEOUT	(1<<13)
#define   GMBUS_INT		(1<<12)
#define   GMBUS_HW_RDY		(1<<11)
#define   GMBUS_SATOER		(1<<10)
#define   GMBUS_ACTIVE		(1<<9)
#define GMBUS3			0x510c /* data buffer bytes 3-0 */
#define GMBUS4			0x5110 /* interrupt mask (Pineview+) */
#define   GMBUS_SLAVE_TIMEOUT_EN (1<<4)
#define   GMBUS_NAK_EN		(1<<3)
#define   GMBUS_IDLE_EN		(1<<2)
#define   GMBUS_HW_WAIT_EN	(1<<1)
#define   GMBUS_HW_RDY_EN	(1<<0)
#define GMBUS5			0x5120 /* byte index */
#define   GMBUS_2BYTE_INDEX_EN	(1<<31)

#define ZERO_OFFSET 0

#define DBOX_ADAK_CTRL_REG	0x600
#define DBOX_SW_FLAG_REG	0x2440

#ifndef CONFIG_X86_EARLYMIC

#define I915_GMBUS_READ8(reg) 	I915_READ8(reg)
#define I915_GMBUS_WRITE8(reg, val) 	I915_WRITE8(reg, val)

#define I915_GMBUS_READ16(reg) 	I915_READ16(reg)
#define I915_GMBUS_WRITE16(reg, val) I915_WRITE16(reg, val)

#define I915_GMBUS_READ16_NOTRACE(reg) 	I915_READ16_NOTRACE(reg)
#define I915_GMBUS_WRITE16_NOTRACE(reg, val)	I915_WRITE16_NOTRACE(reg, val)

#define I915_GMBUS_READ(reg)		I915_READ(reg)
#define I915_GMBUS_WRITE(reg, val)	I915_WRITE(reg, val)

#define I915_GMBUS_READ_NOTRACE(reg)		I915_READ_NOTRACE(reg)
#define I915_GMBUS_WRITE_NOTRACE(reg, val)	I915_WRITE_NOTRACE(reg, val)

#define I915_GMBUS_WRITE64(reg, val)	I915_WRITE64(reg, val)
#define I915_GMBUS_READ64(reg)	I915_READ64(reg)

#else

#define __gmbus_read(x, y) \
static inline u##x gmbus_read##x(void __iomem *dbox_mmio_base, uint32_t reg) { \
	u##x val = 0; \
	val = read##y(dbox_mmio_base + reg); \
	trace_gmbus_reg_rw(false, reg, val, sizeof(val)); \
	return val; \
}

__gmbus_read(8, b)
__gmbus_read(16, w)
__gmbus_read(32, l)
__gmbus_read(64, q)
#undef __gmbus_read

#define __gmbus_write(x, y) \
static inline void gmbus_write##x(void __iomem *dbox_mmio_base, uint32_t reg, u##x val) { \
	trace_gmbus_reg_rw(true, reg, val, sizeof(val)); \
	write##y(val, dbox_mmio_base + reg); \
}
__gmbus_write(8, b)
__gmbus_write(16, w)
__gmbus_write(32, l)
__gmbus_write(64, q)
#undef __gmbus_write


#define I915_GMBUS_READ8(reg)	gmbus_read8(dbox_mmio_base, (reg))
#define I915_GMBUS_WRITE8(reg, val)	gmbus_write8(dbox_mmio_base, (reg), (val))

#define I915_GMBUS_READ16(reg)	gmbus_read16(dbox_mmio_base, (reg))
#define I915_GMBUS_WRITE16(reg, val)	gmbus_write16(dbox_mmio_base, (reg), (val))

#define I915_GMBUS_READ16_NOTRACE(reg)	readw(dbox_mmio_base + (reg))
#define I915_GMBUS_WRITE16_NOTRACE(reg, val)	writew(val, dbox_mmio_base + (reg))

#define I915_GMBUS_READ(reg)		gmbus_read32(dbox_mmio_base, (reg))
#define I915_GMBUS_WRITE(reg, val)	gmbus_write32(dbox_mmio_base, (reg), (val))

#define I915_GMBUS_READ_NOTRACE(reg)		readl(dbox_mmio_base + (reg))
#define I915_GMBUS_WRITE_NOTRACE(reg, val)	writel(val, dbox_mmio_base + (reg))

#define I915_GMBUS_READ64(reg)	gmbus_read64(dbox_mmio_base, (reg))
#define I915_GMBUS_WRITE64(reg, val)	gmbus_write64(dbox_mmio_base, (reg), (val))

#endif


