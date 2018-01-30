#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#include <linux/stat.h>

#include <linux/delay.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/gpio.h>

#define ARM_LE		0
#define INT_LE		1

/* ltc3714 voltage table */
static const unsigned int voltage_table[32] = {
	1750, 1700, 1650, 1600, 1550, 1500, 1450, 1400,
	1350, 1300, 1250, 1200, 1150, 1100, 1050, 1000,
	975, 950, 925, 900, 875, 850, 825, 800,
	775, 750, 725, 700, 675, 650, 625, 600,
};

/* frequency voltage matching table */
static const unsigned int frequency_match[][3] = {
/* frequency, Mathced VDD ARM voltage , Matched VDD INT*/
	{667000, 1200, 1300},
	{333000, 1100, 1200},
	{222000, 1050, 1200},
	{133000, 1000, 1200},
	{66000, 1000, 1000},
};

/* LTC3714 Setting Routine */
static int ltc3714_gpio_setting(void)
{
	gpio_direction_output(S3C_GPN11, 0);
	gpio_direction_output(S3C_GPN12, 0);
	gpio_direction_output(S3C_GPN13, 0);
	gpio_direction_output(S3C_GPN14, 0);
	gpio_direction_output(S3C_GPN15, 0);
	gpio_direction_output(S3C_GPL8, 0);
	gpio_direction_output(S3C_GPL9, 0);
	gpio_direction_output(S3C_GPL10, 0);

	gpio_pullup(S3C_GPN11, 0x0);
	gpio_pullup(S3C_GPN12, 0x0);
	gpio_pullup(S3C_GPN13, 0x0);
	gpio_pullup(S3C_GPN14, 0x0);
	gpio_pullup(S3C_GPN15, 0x0);
	gpio_pullup(S3C_GPL8, 0x0);
	gpio_pullup(S3C_GPL9, 0x0);
	gpio_pullup(S3C_GPL10, 0x0);

	return 0;
}

static int set_ltc3714(unsigned int pwr, unsigned int index)
{
	int position = 0;

	int voltage = frequency_match[index][pwr + 1];

	if(voltage > voltage_table[0] || voltage < voltage_table[31]) {
		printk("[ERROR]: voltage value over limits!!!");
		return -EINVAL;
	}

	if(voltage > voltage_table[16]) { // 1750 ~ 1000 mV
		for(position = 15; position >= 0; position --) {
			if(voltage_table[position] == voltage) break;
		}

	}
	else if(voltage >= voltage_table[31]) {	//975 ~ 600 mV
		for(position = 31; position >= 16; position --) {
			if(voltage_table[position] == voltage) break;
		}
	}
	else {
		printk("[error]: Can't find adquate voltage table list value\n");
		return -EINVAL;
	}

	position &=0x1f;

	gpio_set_value(S3C_GPN11,(position >> 0)&0x1);
	gpio_set_value(S3C_GPN12,(position >> 1)&0x1);
	gpio_set_value(S3C_GPN13,(position >> 2)&0x1);
	gpio_set_value(S3C_GPN14,(position >> 3)&0x1);
	gpio_set_value(S3C_GPN15,(position >> 4)&0x1);

	if(pwr == ARM_LE) {
		gpio_set_value(S3C_GPL8, 1);
		udelay(10);
		gpio_set_value(S3C_GPL8, 0);
	} else if(pwr == INT_LE) {
		gpio_set_value(S3C_GPL10, 1);
		udelay(10);
		gpio_set_value(S3C_GPL10, 0);
	} else {
		printk("[error]: set_power, check mode [pwr] value\n");
		return -EINVAL;
	}

	return 0;
}

static int find_voltage(int freq)
{
	int index = 0;

	if(freq > frequency_match[0][0]){
		printk(KERN_ERR "frequecy is over then support frequency\n");
		return 0;
	}

	for(index = 0 ; index < ARRAY_SIZE(frequency_match) ; index++){
		if(freq >= frequency_match[index][0])
			return index;
	}

	printk("Cannot find matched voltage on table\n");

	return 0;
}

int set_power(unsigned int freq)
{
	int index;

	index = find_voltage(freq);

	set_ltc3714(ARM_LE, index);

	return 0;
}

EXPORT_SYMBOL(set_power);

int ltc3714_init(void)
{
	ltc3714_gpio_setting();
	set_power(532000);
	gpio_set_value(S3C_GPL9, 1);
}

EXPORT_SYMBOL(ltc3714_init);
