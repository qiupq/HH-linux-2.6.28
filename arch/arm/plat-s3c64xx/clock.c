/* linux/arch/arm/plat-s3c64xx/clock.c
 *
 * Copyright (c) 2004-2005 Samsung Electronics
 *
 *
 * S3C64XX Core clock control support
 *
 * Based on, and code from linux/arch/arm/mach-versatile/clock.c
 **
 **  Copyright (C) 2004 ARM Limited.
 **  Written by Deep Blue Solutions Limited.
 *
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/sysdev.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

//#include <asm/arch/regs-clock.h>
#include <asm/arch/regs-gpio.h>

#include <asm/plat-s3c64xx/clock.h>
#include <asm/plat-s3c24xx/cpu.h>
#include <asm/arch-s3c2410/regs-s3c-clock.h>

#define ARM_PLL_CON 	S3C_APLL_CON
#define ARM_CLK_DIV	S3C_CLK_DIV0

#define ARM_DIV_RATIO_BIT		0
#define ARM_DIV_MASK			(0xf<<ARM_DIV_RATIO_BIT)
#define HCLK_DIV_RATIO_BIT		9
#define HCLK_DIV_MASK			(0x7<<HCLK_DIV_RATIO_BIT)

#define READ_ARM_DIV    		((__raw_readl(ARM_CLK_DIV)&ARM_DIV_MASK) + 1)
#define PLL_CALC_VAL(MDIV,PDIV,SDIV)	((1<<31)|(MDIV)<<16 |(PDIV)<<8 |(SDIV))
#define GET_ARM_CLOCK(baseclk)		s3c6400_get_pll(__raw_readl(S3C_APLL_CON),baseclk)

#define MHZ				1000*1000
#define INIT_XTAL			12 * MHZ

/* enable and disable calls for use with the clk struct */
static const u32 s3c_cpu_clock_table[][6] = {
	{532*MHZ, 266, 3, 1, 0, 0},
	{266*MHZ, 266, 3, 1, 0, 1},
	{133*MHZ, 266, 3, 1, 0, 3},
};

/* clock information */

static LIST_HEAD(clocks);

DEFINE_MUTEX(clocks_mutex);

/* enable and disable calls for use with the clk struct */

static int clk_null_enable(struct clk *clk, int enable)
{
	return 0;
}

/* Clock API calls */

struct clk *clk_get(struct device *dev, const char *id)
{
	struct clk *p;
	struct clk *clk = ERR_PTR(-ENOENT);
	int idno;

	if (dev == NULL || dev->bus != &platform_bus_type)
		idno = -1;
	else
		idno = to_platform_device(dev)->id;

	mutex_lock(&clocks_mutex);

	list_for_each_entry(p, &clocks, list) {
		if (p->id == idno &&
		    strcmp(id, p->name) == 0 &&
		    try_module_get(p->owner)) {
			clk = p;
			break;
		}
	}

	/* check for the case where a device was supplied, but the
	 * clock that was being searched for is not device specific */

	if (IS_ERR(clk)) {
		list_for_each_entry(p, &clocks, list) {
			if (p->id == -1 && strcmp(id, p->name) == 0 &&
			    try_module_get(p->owner)) {
				clk = p;
				break;
			}
		}
	}

	mutex_unlock(&clocks_mutex);
	return clk;
}

void clk_put(struct clk *clk)
{
	module_put(clk->owner);
}

int clk_enable(struct clk *clk)
{
	if (IS_ERR(clk) || clk == NULL)
		return -EINVAL;

	clk_enable(clk->parent);

	mutex_lock(&clocks_mutex);

	if ((clk->usage++) == 0)
		(clk->enable)(clk, 1);

	mutex_unlock(&clocks_mutex);
	return 0;
}

void clk_disable(struct clk *clk)
{
	if (IS_ERR(clk) || clk == NULL)
		return;

	mutex_lock(&clocks_mutex);

	if ((--clk->usage) == 0)
		(clk->enable)(clk, 0);

	mutex_unlock(&clocks_mutex);
	clk_disable(clk->parent);
}


unsigned long clk_get_rate(struct clk *clk)
{
	if (IS_ERR(clk))
		return 0;

	if (clk->rate != 0)
		return clk->rate;

	if (clk->get_rate != NULL)
		return (clk->get_rate)(clk);

	if (clk->parent != NULL)
		return clk_get_rate(clk->parent);

	return clk->rate;
}

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (!IS_ERR(clk) && clk->round_rate)
		return (clk->round_rate)(clk, rate);

	return rate;
}

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret;

	if (IS_ERR(clk))
		return -EINVAL;

	mutex_lock(&clocks_mutex);
	ret = (clk->set_rate)(clk, rate);
	mutex_unlock(&clocks_mutex);

	return ret;
}

struct clk *clk_get_parent(struct clk *clk)
{
	return clk->parent;
}

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	int ret = 0;

	if (IS_ERR(clk))
		return -EINVAL;

	mutex_lock(&clocks_mutex);

	if (clk->set_parent)
		ret = (clk->set_parent)(clk, parent);

	mutex_unlock(&clocks_mutex);

	return ret;
}

EXPORT_SYMBOL(clk_get);
EXPORT_SYMBOL(clk_put);
EXPORT_SYMBOL(clk_enable);
EXPORT_SYMBOL(clk_disable);
EXPORT_SYMBOL(clk_get_rate);
EXPORT_SYMBOL(clk_round_rate);
EXPORT_SYMBOL(clk_set_rate);
EXPORT_SYMBOL(clk_get_parent);
EXPORT_SYMBOL(clk_set_parent);


void inline s3c_clk_enable (uint clocks, uint enable, ulong gate_reg)
{
	unsigned long clkcon;
	unsigned long flags;

	local_irq_save(flags);

	clkcon = __raw_readl(gate_reg);
	clkcon &= ~clocks;

	if (enable)
		clkcon |= clocks;

	__raw_writel(clkcon, gate_reg);

	local_irq_restore(flags);
}

unsigned long s3c_fclk_get_rate(void)
{
	unsigned long apll_con;
	unsigned long clk_div0_tmp;
	unsigned long m = 0;
	unsigned long p = 0;
	unsigned long s = 0;
	unsigned long ret;

	apll_con = __raw_readl(S3C_APLL_CON);
	clk_div0_tmp = __raw_readl(S3C_CLK_DIV0) & 0xf;

	m = (apll_con >> 16) & 0x3ff;
	p = (apll_con >> 8) & 0x3f;
	s = apll_con & 0x3;

	ret = (m * (INIT_XTAL / (p * (1 << s))));

	return (ret / (clk_div0_tmp + 1));
}

unsigned long s3c_fclk_round_rate(struct clk *clk, unsigned long rate)
{
	u32 iter;

        for(iter = 1 ; iter < ARRAY_SIZE(s3c_cpu_clock_table) ; iter++){
                if(rate > s3c_cpu_clock_table[iter][0])
                        return s3c_cpu_clock_table[iter-1][0];
        }

        return s3c_cpu_clock_table[ARRAY_SIZE(s3c_cpu_clock_table) - 1][0];
}

int s3c_fclk_set_rate(struct clk *clk, unsigned long rate)
{
	u32 ret = -EINVAL;
	u32 round_tmp;
	u32 iter;
	u32 apll_con_tmp;
	u32 clk_div0_tmp;

	round_tmp = s3c_fclk_round_rate(clk,rate);

	if(round_tmp == (int)s3c_fclk_get_rate())
		return 0;

	for (iter = 0 ; iter < ARRAY_SIZE(s3c_cpu_clock_table) ; iter++){
		if(round_tmp == s3c_cpu_clock_table[iter][0])
			break;
	}

	clk_div0_tmp = __raw_readl(ARM_CLK_DIV) & ~(ARM_DIV_MASK);
	clk_div0_tmp |= s3c_cpu_clock_table[iter][5];

	__raw_writel(clk_div0_tmp, ARM_CLK_DIV);

	if (__raw_readl(ARM_CLK_DIV) == clk_div0_tmp)
		ret = 0;

	clk->rate = s3c_cpu_clock_table[iter][0];

	return ret;
}

/* base clocks */

struct clk clk_xtal = {
	.name		= "xtal",
	.id		= -1,
	.rate		= 0,
	.parent		= NULL,
	.ctrlbit	= 0,
};

struct clk clk_mpll = {
	.name		= "mpll",
	.id		= -1,
};


struct clk clk_epll = {
	.name		= "epll",
	.id		= -1,
	.rate		= 0,
	.parent		= NULL,
	.ctrlbit	= 0,

};

struct clk clk_f = {
	.name		= "fclk",
	.id		= -1,
	.rate		= 0,
	.parent		= &clk_mpll,
	.ctrlbit	= 0,
	.set_rate	= s3c_fclk_set_rate,
	.round_rate	= s3c_fclk_round_rate,
};

struct clk clk_h = {
	.name		= "hclk",
	.id		= -1,
	.rate		= 0,
	.parent		= NULL,
	.ctrlbit	= 0,
};

struct clk clk_p = {
	.name		= "pclk",
	.id		= -1,
	.rate		= 0,
	.parent		= NULL,
	.ctrlbit	= 0,
};


struct clk clk_hx2 = {
	.name		= "hclkx2",
	.id		= -1,
	.rate		= 0,
	.parent		= NULL,
	.ctrlbit	= 0,
};

struct clk clk_s = {
	.name		= "sclk",
	.id		= -1,
	.rate		= 0,
	.parent		= NULL,
	.ctrlbit	= 0,
};

struct clk clk_u = {
	.name		= "uclk",
	.id		= -1,
	.rate		= 0,
	.parent		= NULL,
	.ctrlbit	= 0,
};

struct clk clk_48m = {
	.name		= "clk48m",
	.id		= -1,
	.rate		= 48*1000*1000,
	.parent		= NULL,
	.ctrlbit	= 0,
};

struct clk clk_27m = {
	.name		= "clk27m",
	.id		= -1,
	.rate		= 27*1000*1000,
	.parent		= NULL,
	.ctrlbit	= 0,
};



/* initialise the clock system */

int s3c64xx_register_clock(struct clk *clk)
{
	clk->owner = THIS_MODULE;

	if (clk->enable == NULL)
		clk->enable = clk_null_enable;

	/* add to the list of available clocks */

	mutex_lock(&clocks_mutex);
	list_add(&clk->list, &clocks);
	mutex_unlock(&clocks_mutex);

	return 0;
}

int s3c64xx_register_clocks(struct clk **clks, int nr_clks)
{
	int fails = 0;

	for (; nr_clks > 0; nr_clks--, clks++) {
		if (s3c64xx_register_clock(*clks) < 0)
			fails++;
	}

	return fails;
}

/* initalise all the clocks */

int __init s3c64xx_setup_clocks(unsigned long xtal,
				unsigned long fclk,
				unsigned long hclk,
				unsigned long pclk)
{
	printk(KERN_INFO "S3C64XX Clocks, (c) 2007 Samssung Electronics\n");

	/* initialise the main system clocks */

	clk_xtal.rate = xtal;
	clk_mpll.rate = fclk;
	clk_h.rate = hclk;
	clk_p.rate = pclk;
	clk_f.rate = fclk;

	/* assume uart clocks are correctly setup */

	/* register our clocks */

	if (s3c64xx_register_clock(&clk_xtal) < 0)
		printk(KERN_ERR "failed to register master xtal\n");

	if (s3c64xx_register_clock(&clk_mpll) < 0)
		printk(KERN_ERR "failed to register mpll clock\n");

	if (s3c64xx_register_clock(&clk_f) < 0)
		printk(KERN_ERR "failed to register cpu fclk\n");

	if (s3c64xx_register_clock(&clk_h) < 0)
		printk(KERN_ERR "failed to register cpu hclk\n");

	if (s3c64xx_register_clock(&clk_p) < 0)
		printk(KERN_ERR "failed to register cpu pclk\n");

	if (s3c64xx_register_clock(&clk_epll) < 0)
		printk(KERN_ERR "failed to register epll clock\n");

	/* register hclkx2  */
	if (s3c64xx_register_clock(&clk_hx2) < 0)
		printk(KERN_ERR "failed to register cpu hclkx2\n");

	if (s3c64xx_register_clock(&clk_s) < 0)
		printk(KERN_ERR "failed to register cpu pclk\n");

	if (s3c64xx_register_clock(&clk_u) < 0)
		printk(KERN_ERR "failed to register cpu uclk\n");

	if (s3c64xx_register_clock(&clk_48m) < 0)
		printk(KERN_ERR "failed to register cpu uclk\n");

	if (s3c64xx_register_clock(&clk_27m) < 0)
		printk(KERN_ERR "failed to register cpu uclk\n");

	return 0;
}

