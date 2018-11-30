/* * Copyright (c) 2010 - 2012 Intel Corporation.
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
/*
 * Copyright (c) 2006 Dave Airlie <airlied@linux.ie>
 * Copyright © 2006-2008,2010 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	Eric Anholt <eric@anholt.net>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include "drmP.h"
#include "drm.h"
#include "intel_drv.h"
#include "intel_i2c.h"
#include "i915_drm.h"
#include "i915_drv.h"

/* Intel GPIO access functions */

#define I2C_RISEFALL_TIME 20
#define DELAY_75_USECS 75

static void __iomem *dbox_mmio_base;
static int gmbus_i2c_adap_id[GMBUS_NUM_PORTS];
static void intel_dealloc_gmbus(struct intel_gmbus *gmbus);

atomic_t exception_flag = ATOMIC_INIT(0);

static inline struct intel_gmbus *
to_intel_gmbus(struct i2c_adapter *i2c)
{
	return container_of(i2c, struct intel_gmbus, adapter);
}

struct intel_gpio {
	struct i2c_adapter adapter;
	struct i2c_algo_bit_data algo;
	struct drm_i915_private *dev_priv;
	u32 reg;
};

void
intel_i2c_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	if (HAS_PCH_SPLIT(dev))
		I915_GMBUS_WRITE(PCH_GMBUS0, 0);
	else
		I915_GMBUS_WRITE(GMBUS0, 0);
}

static void intel_i2c_quirk_set(struct drm_i915_private *dev_priv, bool enable)
{
	u32 val;

	/* When using bit bashing for I2C, this bit needs to be set to 1 */
	if (!IS_PINEVIEW(dev_priv->dev))
		return;

	val = I915_GMBUS_READ(DSPCLK_GATE_D);
	if (enable)
		val |= DPCUNIT_CLOCK_GATE_DISABLE;
	else
		val &= ~DPCUNIT_CLOCK_GATE_DISABLE;
	I915_GMBUS_WRITE(DSPCLK_GATE_D, val);
}

static u32 get_reserved(struct intel_gpio *gpio)
{
	struct drm_i915_private *dev_priv = gpio->dev_priv;
	struct drm_device *dev = dev_priv->dev;
	u32 reserved = 0;

	/* On most chips, these bits must be preserved in software. */
	if (!IS_I830(dev) && !IS_845G(dev))
		reserved = I915_GMBUS_READ_NOTRACE(gpio->reg) &
					     (GPIO_DATA_PULLUP_DISABLE |
					      GPIO_CLOCK_PULLUP_DISABLE);

	return reserved;
}

static int get_clock(void *data)
{
	struct intel_gpio *gpio = data;
	struct drm_i915_private *dev_priv = gpio->dev_priv;
	u32 reserved = get_reserved(gpio);
	I915_GMBUS_WRITE_NOTRACE(gpio->reg, reserved | GPIO_CLOCK_DIR_MASK);
	I915_GMBUS_WRITE_NOTRACE(gpio->reg, reserved);
	return (I915_GMBUS_READ_NOTRACE(gpio->reg) & GPIO_CLOCK_VAL_IN) != 0;
}

static int get_data(void *data)
{
	struct intel_gpio *gpio = data;
	struct drm_i915_private *dev_priv = gpio->dev_priv;
	u32 reserved = get_reserved(gpio);
	I915_GMBUS_WRITE_NOTRACE(gpio->reg, reserved | GPIO_DATA_DIR_MASK);
	I915_GMBUS_WRITE_NOTRACE(gpio->reg, reserved);
	return (I915_GMBUS_READ_NOTRACE(gpio->reg) & GPIO_DATA_VAL_IN) != 0;
}

static void set_clock(void *data, int state_high)
{
	struct intel_gpio *gpio = data;
	struct drm_i915_private *dev_priv = gpio->dev_priv;
	u32 reserved = get_reserved(gpio);
	u32 clock_bits;

	if (state_high)
		clock_bits = GPIO_CLOCK_DIR_IN | GPIO_CLOCK_DIR_MASK;
	else
		clock_bits = GPIO_CLOCK_DIR_OUT | GPIO_CLOCK_DIR_MASK |
			GPIO_CLOCK_VAL_MASK;

	I915_GMBUS_WRITE_NOTRACE(gpio->reg, reserved | clock_bits);
	POSTING_READ(gpio->reg);
}

static void set_data(void *data, int state_high)
{
	struct intel_gpio *gpio = data;
	struct drm_i915_private *dev_priv = gpio->dev_priv;
	u32 reserved = get_reserved(gpio);
	u32 data_bits;

	if (state_high)
		data_bits = GPIO_DATA_DIR_IN | GPIO_DATA_DIR_MASK;
	else
		data_bits = GPIO_DATA_DIR_OUT | GPIO_DATA_DIR_MASK |
			GPIO_DATA_VAL_MASK;

	I915_GMBUS_WRITE_NOTRACE(gpio->reg, reserved | data_bits);
	POSTING_READ(gpio->reg);
}

static struct i2c_adapter *
intel_gpio_create(struct drm_i915_private *dev_priv, u32 pin)
{
	static const int map_pin_to_reg[] = {
		0,
		GPIOB,
		GPIOA,
		GPIOC,
		GPIOD,
		GPIOE,
		0,
		GPIOF,
	};
	struct intel_gpio *gpio;

	if (pin >= ARRAY_SIZE(map_pin_to_reg) || !map_pin_to_reg[pin])
		return NULL;

	gpio = kzalloc(sizeof(struct intel_gpio), GFP_KERNEL);
	if (gpio == NULL)
		return NULL;

	gpio->reg = map_pin_to_reg[pin];
	if (HAS_PCH_SPLIT(dev_priv->dev))
		gpio->reg += PCH_GPIOA - GPIOA;
	gpio->dev_priv = dev_priv;

	snprintf(gpio->adapter.name, sizeof(gpio->adapter.name),
		 "i915 GPIO%c", "?BACDE?F"[pin]);
	gpio->adapter.owner = THIS_MODULE;
	gpio->adapter.algo_data	= &gpio->algo;
	gpio->adapter.dev.parent = &dev_priv->dev->pdev->dev;
	gpio->algo.setsda = set_data;
	gpio->algo.setscl = set_clock;
	gpio->algo.getsda = get_data;
	gpio->algo.getscl = get_clock;
	gpio->algo.udelay = I2C_RISEFALL_TIME;
	gpio->algo.timeout = usecs_to_jiffies(2200);
	gpio->algo.data = gpio;

	if (i2c_bit_add_bus(&gpio->adapter))
		goto out_free;

	return &gpio->adapter;

out_free:
	kfree(gpio);
	return NULL;
}

static int
intel_i2c_quirk_xfer(struct drm_i915_private *dev_priv,
		     struct i2c_adapter *adapter,
		     struct i2c_msg *msgs,
		     int num)
{
	struct intel_gpio *gpio = container_of(adapter,
					       struct intel_gpio,
					       adapter);
	int ret;

	intel_i2c_reset(dev_priv->dev);

	intel_i2c_quirk_set(dev_priv, true);
	set_data(gpio, 1);
	set_clock(gpio, 1);
	udelay(I2C_RISEFALL_TIME);

	ret = adapter->algo->master_xfer(adapter, msgs, num);

	set_data(gpio, 1);
	set_clock(gpio, 1);
	intel_i2c_quirk_set(dev_priv, false);

	return ret;
}

/**
 * dummy_read_dbox_regs() - Reads two DBOX registers to recover from
 * corruption of DBOX registers caused due to clock ratio changes.
 * HSD # 4118169.
 */
static inline void
dummy_read_dbox_regs()
{
	I915_GMBUS_READ(DBOX_ADAK_CTRL_REG);
	I915_GMBUS_READ(DBOX_SW_FLAG_REG);
}

/**
 * release_gmbus_hw_mutex() - Releases the GMBUS HW mutex.
 * @reg_offset - In certain architectures (with gen 5 and 6 render engines).
 *		the GMBUS registers are at a different offsets from the base.
 *		reg_offset is the difference in offsets.
 */
static inline void 
release_gmbus_hw_mutex(int reg_offset)
{
	/* Read two DBOX registers to recover from any corruption of DBOX */
	dummy_read_dbox_regs();
	/* Release the HW mutex by setting the INUSE bit (15) in GMBUS2 reg */
	I915_GMBUS_WRITE(GMBUS2 + reg_offset, GMBUS_INUSE);
}

/**
 * gmbus_reset() - Resets the GMBUS HW Engine.
 * @reg_offset - In certain architectures (with gen 5 and 6 render engines).
 *		the GMBUS registers are at a different offsets from the base.
 *		reg_offset is the difference in offsets.
 */
static inline void
gmbus_reset(int reg_offset)
{
	/* Toggle the Software Clear Interrupt bit. This has the effect
	 * of resetting the GMBUS controller and also clearing the
	 * BUS_ERROR if raised by the slave's NAK.
	 */
	I915_GMBUS_WRITE(GMBUS1 + reg_offset, GMBUS_SW_CLR_INT);
	I915_GMBUS_WRITE(GMBUS1 + reg_offset, 0);
}

/*
 * gmbus_xfer_wait_for_hw_rdy() - Wait for HW_RDY bit to be set after initiating
 *					a GMBUS I2C read or write.
 * @reg_offset - In certain architectures (with gen 5 and 6 render engines).
 *		the GMBUS registers are at a different offsets from the base.
 *		reg_offset is the difference in offsets.
 */
static int
gmbus_xfer_wait_for_hw_rdy(int reg_offset)
{
	uint16_t count = 0;
	uint32_t reg_val;

	do {
		/* Read two DBOX registers to recover from any corruption of
		   DBOX registers which can cause a register read to return 0
		   even though the register contains non-zero value.
		*/
		if (count == 499)
			dummy_read_dbox_regs();

		reg_val = I915_GMBUS_READ(GMBUS2 + reg_offset);
		if (reg_val & (GMBUS_STALL_TIMEOUT | GMBUS_SATOER))
			return GMBUS_I2C_SACK_TO_ERR;
		if (reg_val & GMBUS_HW_RDY)
			return 0;
		udelay(DELAY_75_USECS);
	} while (++count < 500);  /* Timeout set at approx 37.5 ms */
	return GMBUS_I2C_TIMEOUT_ERR;
}

/*
 * gmbus_start_i2c_xfer() - Initiate a GMBUS I2C transfer by writing to the 
 *				GMBUS command register.
 * @reg_offset - In certain architectures (with gen 5 and 6 render engines).
 *		the GMBUS registers are at a different offsets from the base.
 *		reg_offset is the difference in offsets.
 * @byte_count - No of bytes to be transferred in the cycle.
 * @addr       - Slave device address.
 * @flags      - GMBUS_SLAVE_READ / GMBUS_SLAVE_WRITE for a read/write transfer
 */
static inline void 
gmbus_start_i2c_xfer(int reg_offset, uint16_t byte_count, uint16_t addr, 
								uint32_t flags)
{
	I915_GMBUS_WRITE(GMBUS1 + reg_offset, (GMBUS_SW_RDY | GMBUS_ENT |
					GMBUS_CYCLE_INDEX | GMBUS_CYCLE_WAIT | 
					(byte_count << GMBUS_BYTE_COUNT_SHIFT) |
					(addr << GMBUS_SLAVE_ADDR_SHIFT) | 
					flags));
}

/*
 * gmbus_stop_i2c_xfer() - End the I2C transaction by sending a STOP bit. 
 * 		This is accomplished by writing to the GMBUS command register.
 * @reg_offset - In certain architectures (with gen 5 and 6 render engines).
 *		the GMBUS registers are at a different offsets from the base.
 *		reg_offset is the difference in offsets.
 */
static inline void
gmbus_stop_i2c_xfer(int reg_offset)
{
	I915_GMBUS_WRITE(GMBUS1 + reg_offset,(GMBUS_SW_RDY | GMBUS_CYCLE_STOP));
	if (wait_for_atomic(!(I915_GMBUS_READ(GMBUS2 + reg_offset) & 
						GMBUS_ACTIVE), 5))
		printk(KERN_ERR "GMBUS timed out after sending a stop bit\n");
}

/*
 * GMBUS I2C read/write transfer function.
 */
static int
gmbus_xfer_read_write(uint16_t flags, uint16_t len, uint8_t *buf, uint16_t addr,
								 int reg_offset)
{
	int ret;
	uint8_t first_loop_iter = 1;
	uint16_t count, byte_count = 0;
	uint32_t val, loop;

	while (len > 0) {
		/* byte count (no of bytes to be transferred in a GMBUS cycle)
		 * is a 9-bit field (max 511) in the GMBUS command register.
		 */
		byte_count = min(len, 511);
		len -= byte_count;

		/* If Machine Check Exception handler wants to use the GMBUS, 
		 * it sets this flag. Release GMBUS as soon as possible.
		 */
		if (atomic_read(&exception_flag) == 1)
			return GMBUS_I2C_INTERRUPTED;

		if (flags & I2C_M_RD) { /* GMBUS read transaction */
			gmbus_start_i2c_xfer(reg_offset, byte_count, addr, 
							GMBUS_SLAVE_READ);
			do {
				loop = 0;
				ret=gmbus_xfer_wait_for_hw_rdy(reg_offset);
				if (ret != 0)
					return ret;
				val = I915_GMBUS_READ(GMBUS3 + reg_offset);
				do {
					*buf++ = val & 0xff;
					val >>= 8;
				} while (--byte_count && ++loop < 4);
				/* If exception handler sets this flag and 
				 * there are pending bytes to be transferred,
				 * send a stop bit and exit without completing
				 * the full transfer to release the GMBUS. 
				 * If the flag is set but no pending bytes, 
				 * we'll give up the GMBUS as we are done.
				 */
				if ((atomic_read(&exception_flag) == 1) && 
							(len || byte_count)) {
					gmbus_stop_i2c_xfer(reg_offset);
					return GMBUS_I2C_INTERRUPTED;
				}
			} while (byte_count);
		}
		else {  /* GMBUS write transaction */
			count = byte_count;
			while (byte_count) {
				val = loop = 0;
				do {
					val |= *buf++ << (8 * loop);
				} while (--byte_count && ++loop < 4);
				I915_GMBUS_WRITE(GMBUS3 + reg_offset, val);
				if (first_loop_iter) {
					gmbus_start_i2c_xfer(reg_offset, count,
						addr, GMBUS_SLAVE_WRITE);
					first_loop_iter = 0;
				}
				ret=gmbus_xfer_wait_for_hw_rdy(reg_offset);
				if (ret != 0)
					return ret;
				if ((atomic_read(&exception_flag) == 1) && 
							(len || byte_count)) {
					gmbus_stop_i2c_xfer(reg_offset);
					return GMBUS_I2C_INTERRUPTED;
				}
			}
		}
	}
	return 0;
}

/*
 * GMBUS I2C transfer function. Can be invoked via i2c_transfer().
 */
static int
gmbus_xfer(struct i2c_adapter *adapter,
	   struct i2c_msg *msgs,
	   int num)
{
	struct intel_gmbus *bus = container_of(adapter,
					       struct intel_gmbus,
					       adapter);
	struct drm_i915_private *dev_priv = adapter->algo_data;
	int i, reg_offset, num_msgs=0, ret;

	if (bus->force_bit)
		return intel_i2c_quirk_xfer(dev_priv,
					    bus->force_bit, msgs, num);
	if (dev_priv)
		reg_offset = HAS_PCH_SPLIT(dev_priv->dev) ? 
						PCH_GMBUS0 - GMBUS0 : 0;
	else
		reg_offset = 0;

	I915_GMBUS_WRITE(GMBUS0 + reg_offset, bus->reg0);
	I915_GMBUS_WRITE(GMBUS5 + reg_offset, 0);

	/* Check if the GMBUS is already in use by reading the GMBUS HW mutex.
	 * If mutex is free, the read returns GMBUS not in use and also 
	 * acquires the HW mutex.
	 */
	if (wait_for(!(I915_GMBUS_READ(GMBUS2+reg_offset) & GMBUS_INUSE), 100))
		return GMBUS_I2C_TIMEOUT_ERR;

	/* Reset the GMBUS before every transaction */
	gmbus_reset(reg_offset);

	for (i = 0; i < num; i++) {
		if (msgs[i].len == 0 || msgs[i].buf == NULL)
			continue;
		num_msgs++;
		ret = gmbus_xfer_read_write(msgs[i].flags, msgs[i].len, 
					msgs[i].buf, msgs[i].addr, reg_offset);
		if (ret == GMBUS_I2C_TIMEOUT_ERR) {
			goto timeout;
		}
		else if (ret == GMBUS_I2C_SACK_TO_ERR) {
			goto clear_err;
		}
		else if (ret == GMBUS_I2C_INTERRUPTED) {
			printk(KERN_INFO "GMBUS I2C transaction interrupted\n");
			release_gmbus_hw_mutex(reg_offset);
			return GMBUS_I2C_INTERRUPTED;
		}
		if (i + 1 < num) {
			if (wait_for_atomic(I915_GMBUS_READ(GMBUS2 + reg_offset)
						 & GMBUS_HW_WAIT_PHASE, 10))
				goto timeout;
		}
		else {
			gmbus_stop_i2c_xfer(reg_offset);
		}
	}

	goto done;

clear_err:
	printk(KERN_ERR "Slave Ack time-out Err \n");
	gmbus_reset(reg_offset);
	release_gmbus_hw_mutex(reg_offset);
	return GMBUS_I2C_SACK_TO_ERR;
done:
	/* Mark the GMBUS interface as disabled. We will re-enable it at the
	 * start of the next xfer, till then let it sleep.
	 */
	I915_GMBUS_WRITE(GMBUS0 + reg_offset, 0);
	release_gmbus_hw_mutex(reg_offset);
	return num_msgs;

timeout:
	gmbus_reset(reg_offset);
	release_gmbus_hw_mutex(reg_offset);
#ifndef CONFIG_X86_EARLYMIC
	printk(KERN_INFO "GMBUS timed out, falling back to bit banging on pin %d [%s]\n",
		 bus->reg0 & 0xff, bus->adapter.name);
	I915_GMBUS_WRITE(GMBUS0 + reg_offset, 0);

	/* Hardware may not support GMBUS over these pins? Try GPIO bitbanging instead. */
	bus->force_bit = intel_gpio_create(dev_priv, bus->reg0 & 0xff);
	if (!bus->force_bit)
		return -ENOMEM;

	return intel_i2c_quirk_xfer(dev_priv, bus->force_bit, msgs, num);
#else
	printk(KERN_ERR "GMBUS timed out \n");
	return GMBUS_I2C_TIMEOUT_ERR;
#endif
}

static u32 gmbus_func(struct i2c_adapter *adapter)
{
	struct intel_gmbus *bus = container_of(adapter,
					       struct intel_gmbus,
					       adapter);

	if (bus->force_bit)
		bus->force_bit->algo->functionality(bus->force_bit);

	return (I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL |
		/* I2C_FUNC_10BIT_ADDR | */
		I2C_FUNC_SMBUS_READ_BLOCK_DATA |
		I2C_FUNC_SMBUS_BLOCK_PROC_CALL);
}

static const struct i2c_algorithm gmbus_algorithm = {
	.master_xfer	= gmbus_xfer,
	.functionality	= gmbus_func
};

static int 
intel_alloc_init_gmbus(const char *names[GMBUS_NUM_PORTS], 
			struct device *parent, void *algo_data, 
			uint32_t gmbus_rate, struct intel_gmbus **gmbus_ptr)
{
	int i, ret;
	struct intel_gmbus *gmbus;

	*gmbus_ptr = NULL;
	gmbus = kcalloc(sizeof(struct intel_gmbus),GMBUS_NUM_PORTS,GFP_KERNEL);
	if (!gmbus)
		return -ENOMEM;

	for (i = 0; i < GMBUS_NUM_PORTS; i++) {
		struct intel_gmbus *bus = &gmbus[i];

		bus->adapter.owner = THIS_MODULE;
		bus->adapter.class = I2C_CLASS_DDC;
		snprintf(bus->adapter.name,
			 sizeof(bus->adapter.name),
			 "i915 gmbus %s",
			 names[i]);

		bus->adapter.dev.parent = parent;
		bus->adapter.algo_data = algo_data;

		bus->adapter.algo = &gmbus_algorithm;
		ret = i2c_add_adapter(&bus->adapter);
		if (ret)
			goto err;
		bus->reg0 = i | gmbus_rate | GMBUS_HOLD_EXT;
		bus->force_bit = NULL;
	}

	*gmbus_ptr = gmbus;
	return 0;

err:
	while (--i) {
		struct intel_gmbus *bus = &gmbus[i];
		i2c_del_adapter(&bus->adapter);
	}
	kfree(gmbus);
	return ret;
}

/**
 * intel_gmbus_setup - instantiate all Intel i2c GMBuses
 * @dev: DRM device
 */
int intel_setup_gmbus(struct drm_device *dev)
{
	static const char *names[GMBUS_NUM_PORTS] = {
		"disabled",
		"ssc",
		"vga",
		"panel",
		"dpc",
		"dpb",
		"reserved",
		"dpd",
	};
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret, i;

	ret = intel_alloc_init_gmbus(names, &dev->pdev->dev, 
					dev_priv, GMBUS_RATE_100KHZ, 
					&dev_priv->gmbus);
	if (ret != 0)
		return ret;

	for (i = 0; i < GMBUS_NUM_PORTS; i++) {
		struct intel_gmbus *bus = &dev_priv->gmbus[i];
		/* XXX force bit banging until GMBUS is fully debugged */
		bus->force_bit = intel_gpio_create(dev_priv, i);
	}

	intel_i2c_reset(dev_priv->dev);
	return ret;
}

static int __devinit intel_gmbus_probe(struct platform_device* pdev)
{
	static const char *names[GMBUS_NUM_PORTS] = {
		"disabled",
		"dvoa",
		"dvob",
		"dvoc",
		"reserved",
		"reserved",
		"reserved",
		"reserved",
	};
	int ret, i;
	struct intel_gmbus *gmbus;

	ret = intel_alloc_init_gmbus(names, NULL, NULL, GMBUS_RATE_100KHZ,
								&gmbus);
	if (ret != 0) {
		printk(KERN_ERR "GMBUS allocation failed \n");
		return ret;
	}
	dbox_mmio_base = ioremap(DBOX_BASE, COMMON_MMIO_BOX_SIZE);
	if (!dbox_mmio_base) {
		intel_dealloc_gmbus(gmbus);
		printk(KERN_ERR "DBOX MMIO failed \n");
		return -EIO;
	}
	for (i = 0; i < GMBUS_NUM_PORTS; i++)
		gmbus_i2c_adap_id[i] = i2c_adapter_id(&gmbus[i].adapter);
	printk(KERN_INFO "GMBUS setup successful \n");
	gmbus_reset(ZERO_OFFSET);
	platform_set_drvdata(pdev, gmbus);
	return 0;
}

void intel_gmbus_set_speed(struct i2c_adapter *adapter, int speed)
{
	struct intel_gmbus *bus = to_intel_gmbus(adapter);

	/* speed:
	 * 0x0 = 100 KHz
	 * 0x1 = 50 KHz
	 * 0x2 = 400 KHz
	 * 0x3 = 1000 Khz
	 */
	bus->reg0 = (bus->reg0 & ~(0x3 << 8)) | (speed << 8);
}

void intel_gmbus_force_bit(struct i2c_adapter *adapter, bool force_bit)
{
	struct intel_gmbus *bus = to_intel_gmbus(adapter);

	if (force_bit) {
		if (bus->force_bit == NULL) {
			struct drm_i915_private *dev_priv = adapter->algo_data;
			bus->force_bit = intel_gpio_create(dev_priv,
							   bus->reg0 & 0xff);
		}
	} else {
		if (bus->force_bit) {
			i2c_del_adapter(bus->force_bit);
			kfree(bus->force_bit);
			bus->force_bit = NULL;
		}
	}
}

/* 
 * Release all the resources allocated to GMBUS.
 */
static void intel_dealloc_gmbus(struct intel_gmbus *gmbus)
{
	int i;	
	for (i = 0; i < GMBUS_NUM_PORTS; i++) {
		struct intel_gmbus *bus = &gmbus[i];
		if (bus->force_bit) {
			i2c_del_adapter(bus->force_bit);
			kfree(bus->force_bit);
		}
		i2c_del_adapter(&bus->adapter);
	}
	kfree(gmbus);
}

void intel_teardown_gmbus(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (dev_priv->gmbus == NULL)
		return;
	intel_dealloc_gmbus(dev_priv->gmbus);
	dev_priv->gmbus = NULL;
}

static int __devexit intel_gmbus_remove(struct platform_device* pdev)
{
	struct intel_gmbus *gmbus = platform_get_drvdata(pdev);

	iounmap(dbox_mmio_base);
	if (gmbus != NULL)
		intel_dealloc_gmbus(gmbus);
	return 0;
}

/**
 * gmbus_i2c_read() - Read from an i2c slave device using gmbus hw engine.
 * @port - which port the device is connected to. 1 - DVOA, 2 - DVOB, 3 - DVOC
 * @tgt  - 7-bit slave address of the device.
 *	   This is NOT the read or write address of the device.
 *         Example : If 0b0001001 is the 7-bit slave address, 
 *                   read address is 0b00010011, write address is 0b00010010
 * @index - index of the slave device register to read from.
 * @buf - address of the buffer to read into.
 * @len - no of bytes to read.
 *
 * @return - no of bytes successfully transferred.
 *           errno for invalid args or unsuccessful transfer.
 */
int 
gmbus_i2c_read(uint8_t port, uint8_t tgt, uint8_t index, 
						uint8_t *buf, uint16_t len)
{
	int ret;
	struct i2c_msg mesg;
	struct i2c_adapter *adap_ptr;

	if ((buf == NULL) || (len == 0))
	   return -EINVAL;

	if ((port == 0) || (port >= GMBUS_NUM_PORTS))
	   return -EINVAL;

	mesg.addr = tgt | (index << 7);
	mesg.flags = I2C_M_RD;
	mesg.len = len;
	mesg.buf = buf;
  
	adap_ptr = i2c_get_adapter(gmbus_i2c_adap_id[port]);
	ret = i2c_transfer(adap_ptr, &mesg, 1);

	if (ret == 1) {
		if ((*(uint32_t *)buf == 0xe0e0e0e0) || (*(uint32_t *)buf == 0xe0e0e0e2)) {
			printk(KERN_INFO "%s : Invalid data returned from slave: 0x%x due to invalid command: 0x%x \n", __func__, tgt, index);
			return GMBUS_I2C_INVALID_CMD;
		}
		return len;
	} else {
		return ret;
	}
}
EXPORT_SYMBOL(gmbus_i2c_read);

/**
 * gmbus_i2c_write() - Write to an i2c slave device using gmbus hw engine.
 *                     This function cannot be used in exception context as it
 *		       may sleep. Use gmbus_i2c_write_atomic() instead.
 * @port - which port the device is connected to. 1 - DVOA, 2 - DVOB, 3 - DVOC
 * @tgt  - 7-bit slave address of the device. This is NOT the read or write 
 *         address of the device.
 *         Example : If 0b0001001 is the 7-bit slave address, 
 *                   read address is 0b00010011, write address is 0b00010010
 * @index - index of the slave device register to write to.
 * @buf - address of the data buffer to write.
 * @len - no of bytes to write.
 *
 * @return - no of bytes successfully transferred.
 *           errno for invalid args or unsuccessful transfer.
 */
int 
gmbus_i2c_write(uint8_t port, uint8_t tgt, uint8_t index, 
						uint8_t *buf, uint16_t len)
{
	int ret;
	struct i2c_msg mesg;
	struct i2c_adapter *adap_ptr;

	if ((buf == NULL) || (len == 0))
   		return -EINVAL;

	if ((port == 0) || (port >= GMBUS_NUM_PORTS))
   		return -EINVAL;

	mesg.addr = tgt | (index << 7);
	mesg.flags = 0;
	mesg.len = len;
	mesg.buf = buf;

	adap_ptr = i2c_get_adapter(gmbus_i2c_adap_id[port]);
	ret = i2c_transfer(adap_ptr, &mesg, 1);

	if (ret == 1)
		return len;
	else
		return ret;
}
EXPORT_SYMBOL(gmbus_i2c_write);

/**
 * gmbus_i2c_write_atomic() - Write to an i2c slave device using gmbus hw 
 *			      engine. This function can be called in the
 *			      atomic context. WARNING: This function may 
 *			      interrupt a current I2C transaction in progress.
 * @port - which port the device is connected to. 1 - DVOA, 2 - DVOB, 3 - DVOC
 * @tgt  - 7-bit slave address of the device. This is NOT the read or write 
 *         address of the device.
 *         Example : If 0b0001001 is the 7-bit slave address, 
 *                   read address is 0b00010011, write address is 0b00010010
 * @index - index of the slave device register to write to.
 * @buf - address of the data buffer to write.
 * @len - no of bytes to write.
 *
 * @return - 0 for a successful transfer.
 *           errno for an unsuccessful transfer.
 */
int 
gmbus_i2c_write_atomic(uint8_t port, uint8_t tgt, uint8_t index, 
						uint8_t *buf, uint16_t len)
{
	uint8_t loop = 0;
	uint16_t addr = tgt | (index << 7);
	uint16_t byte_count = len;
	uint32_t val = 0;
	int ret = 0;

	/* Set the flag so other thread accessing the GMBUS know that there 
	 * has been an exception and the exception handler wants to use 
 	 * the GMBUS to send an I2C message.
	 */
	atomic_set(&exception_flag, 1);

	/* Check if the GMBUS HW mutex is acquired by another thread/process. 
	 * If not acquired, the read returns not in use and will also acquire
	 * the HW mutex.
	 * Any thread that is using the GMBUS keeps checking the exception_flag
	 * frequently. If the flag is set, it releases the GMBUS ASAP. 
	 */
	if (wait_for_atomic(!(I915_GMBUS_READ(GMBUS2) & GMBUS_INUSE), 100)) {
		printk(KERN_ERR "Time out trying to acquire the GMBUS \n");
		return GMBUS_I2C_TIMEOUT_ERR;
	}

	gmbus_reset(ZERO_OFFSET);
	/* Use DVO Port B, 100 KHz Bus rate */
	I915_GMBUS_WRITE(GMBUS0, 0x2);
	do {
		val |= *buf++ << (8 * loop);
	} while (--byte_count && ++loop < 4);

	I915_GMBUS_WRITE(GMBUS3, val);
	gmbus_start_i2c_xfer(ZERO_OFFSET, len, addr, GMBUS_SLAVE_WRITE);
	ret = gmbus_xfer_wait_for_hw_rdy(ZERO_OFFSET);
	if (ret == 0)
		gmbus_stop_i2c_xfer(ZERO_OFFSET);
	release_gmbus_hw_mutex(ZERO_OFFSET);
	atomic_set(&exception_flag, 0);
	return ret;
}
EXPORT_SYMBOL(gmbus_i2c_write_atomic);

static struct platform_device intel_gmbus_device = {
	.name = "i915-gmbus"
};

static struct platform_driver intel_gmbus_driver = {
	.probe   = intel_gmbus_probe,
	.remove  = __devexit_p(intel_gmbus_remove),
	.driver  = {
		.name = "i915-gmbus",
	},
};

static int __init gmbus_init_module(void)
{
	int ret;
	ret = platform_device_register(&intel_gmbus_device);
	if (ret != 0)
		return ret;
	ret = platform_driver_register(&intel_gmbus_driver);
	return ret;
}

static void __exit gmbus_exit_module(void)
{
	platform_driver_unregister(&intel_gmbus_driver);
	platform_device_unregister(&intel_gmbus_device);
}

MODULE_ALIAS("platform:i915-gmbus-i2c");
MODULE_LICENSE("GPL");

module_init(gmbus_init_module);
module_exit(gmbus_exit_module);
