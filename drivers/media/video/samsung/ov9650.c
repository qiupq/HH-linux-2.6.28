/*
 *  Copyright (C) 2004 Samsung Electronics 
 *             SW.LEE <hitchcar@samsung.com>
 *            - based on Russell King : pcf8583.c
 * 	      - added  smdk24a0, smdk2440
 *            - added  poseidon (s3c24a0+wavecom)	
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Driver for FIMC2.x Camera Decoder 
 *
 */

//#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/i2c-id.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/arch/regs-gpio.h>
#include <asm/io.h>
//#define CAMIF_DEBUG

//#include <asm/arch/registers.h>
#include "../s3c_camif.h"
#include "ov9650.h"

static const char *sensor_version =
    "$Id: ov9650.c, hhtech $";


static struct i2c_driver sensor_driver;

/* This is an abstract CIS sensor for MSDMA input. */

camif_cis_t msdma_input = {
        itu_fmt:       CAMIF_ITU601,
        order422:      CAMIF_YCRYCB,		/* YCRYCB */
        camclk:        32000000, 		/* No effect */
        source_x:      640,       
        source_y:      480,
        win_hor_ofst:  0,
        win_ver_ofst:  0,
        win_hor_ofst2: 0,
        win_ver_ofst2: 0,
        polarity_pclk: 0,
	polarity_vsync:1,
        polarity_href: 0,
        reset_type:CAMIF_EX_RESET_AH, /* Ref board has inverted signal */
        reset_udelay: 20000,
};

camif_cis_t interlace_input = {
        itu_fmt:       CAMIF_ITU601,
        order422:      CAMIF_YCBYCR,		/* YCRYCB */
        camclk:        32000000, 		/* No effect */
        source_x:      720,       
        source_y:      243,
        win_hor_ofst:  0,
        win_ver_ofst:  0,
        win_hor_ofst2: 0,
        win_ver_ofst2: 0,
        polarity_pclk: 1,
	polarity_vsync:0,
        polarity_href: 0,
        reset_type:CAMIF_EX_RESET_AH, /* Ref board has inverted signal */
        reset_udelay: 20000,
};

static camif_cis_t data = {
        itu_fmt:       CAMIF_ITU601,
        order422:      CAMIF_YCRYCB,		 /* YCRYCB */
        camclk:        32000000,		 /* No effect */
        source_x:      640,
        source_y:      480,
        win_hor_ofst:  0,
        win_ver_ofst:  0,
        win_hor_ofst2: 0,
        win_ver_ofst2: 0,
        polarity_pclk: 1,
	polarity_vsync:0,
        polarity_href: 0,
        reset_type:CAMIF_EX_RESET_AH, 		/* Ref board has inverted signal */
        reset_udelay: 20000,
};
//s5k3xa_t s5k3aa_regs_mirror[S5K3AA_REGS];

//extern camif_cis_t* get_initialized_cis();
camif_cis_t* get_initialized_cis() {
	if(data.init_sensor == 0) return NULL;
	return &data;

}
EXPORT(get_initialized_cis);
#if 1	//not used,bxl@hhtech
//bxl@hhtech add
static void ov9650_power_on(void)
{
	writel((readl(S3C_GPNCON)&~(0x3<<30))|(0x1<<30),S3C_GPNCON);	//gpn15 -> output
	writel(readl(S3C_GPNPU)&~(0x3<<30),S3C_GPNPU);
	writel(readl(S3C_GPNDAT)|(1<<15),S3C_GPNDAT);
}
#endif
#define CAM_ID 0x60

static unsigned short ignore[] = { I2C_CLIENT_END };
static unsigned short normal_addr[] = { (CAM_ID >> 1), I2C_CLIENT_END };
static unsigned short *forces[] = { NULL };

static struct i2c_client_address_data addr_data = {
      normal_i2c:normal_addr,
      //normal_i2c_range:ignore,
      probe:ignore,
      //probe_range:ignore,
      ignore:ignore,
      //ignore_range:ignore,
      forces:forces,
};


unsigned char sensor_read(struct i2c_client *client, unsigned char subaddr)
{
	int ret;
	unsigned char buf[1];
	struct i2c_msg msg = { client->addr, 0, 1, buf };
	buf[0] = subaddr;

	ret = i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	if (ret == -EIO) {
		printk(" I2C write Error \n");
		return -EIO;
	}

	msg.flags = I2C_M_RD;
	ret = i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;

	return buf[0];
}


static int
sensor_write(struct i2c_client *client,
	     unsigned char subaddr, unsigned char val)
{
	unsigned char buf[2];
	struct i2c_msg msg = { client->addr, 0, 2, buf };

	buf[0] = subaddr;
	buf[1] = val;

	return i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
}

void inline sensor_init(struct i2c_client *sam_client)
{
	int i, count;
	unsigned int pid,vid,tmp;
	count = (sizeof(ov9650_reg_VGA)/sizeof(ov9650_reg_VGA[0]));
#if 1
	pid = sensor_read(sam_client, 0x0a);
	vid = sensor_read(sam_client, 0x0b);
	printk("pid: 0x%02x, vid: 0x%02x\n", pid, vid);
#endif
	for (i = 0; i < count; i++) {
		if (ov9650_reg_VGA[i].subaddr == CHIP_DELAY) {
			mdelay(ov9650_reg_VGA[i].value);
			mdelay(10);
		} else {
			mdelay(10);
			if((sensor_write(sam_client,
				ov9650_reg_VGA[i].subaddr, ov9650_reg_VGA[i].value))<0)
			printk("write error %d\n",i);
		}
	}
}

static int
ov9650_attach(struct i2c_adapter *adap, int addr, unsigned short flags,
	      int kind)
{
	struct i2c_client *c;

	c = kmalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return -ENOMEM;

	memset(c, 0, sizeof(struct i2c_client));	    

	strcpy(c->name, "OV9650");
	//c->id = sensor_driver.id;
	//c->flags = I2C_CLIENT_ALLOW_USE;
	c->addr = addr;
	c->adapter = adap;
	c->driver = &sensor_driver;
	c->data = &data;
	data.sensor = c;
	
	camif_register_cis(c);
	return i2c_attach_client(c);
}

static int sensor_attach_adapter(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &addr_data, ov9650_attach);
}

static int sensor_detach(struct i2c_client *client)
{
	i2c_detach_client(client);
	camif_unregister_cis(client);
	return 0;
}

/* Purpose:
    This fucntion only for SXGA Camera : ov9650
*/
static int change_sensor_size(struct i2c_client *client, int size)
{
	int i, count;

	switch (size) {
	case SENSOR_VGA:
		count = (sizeof(ov9650_reg_VGA)/sizeof(ov9650_reg_VGA[0]));
		for (i = 0; i < count; i++) {
			if (ov9650_reg_VGA[i].subaddr == CHIP_DELAY) {
				mdelay(ov9650_reg_VGA[i].value);
				mdelay(10);
			} else {
				mdelay(10);
				sensor_write(client, ov9650_reg_VGA[i].subaddr,
					ov9650_reg_VGA[i].value);
			}
		}
		break;

	case SENSOR_SXGA:
		count = (sizeof(ov9650_reg_SXGA)/sizeof(ov9650_reg_SXGA[0]));
		for (i = 0; i < count; i++) {
			if (ov9650_reg_SXGA[i].subaddr == CHIP_DELAY) {
				mdelay(ov9650_reg_SXGA[i].value);
				mdelay(10);
			} else {
				mdelay(10);
				sensor_write(client, ov9650_reg_SXGA[i].subaddr,
					ov9650_reg_SXGA[i].value);
			}
		}
		break;
	case SENSOR_QVGA:   //may not use?? bxl@hhtech
		count = (sizeof(ov9650_reg_QVGA)/sizeof(ov9650_reg_QVGA[0]));
		for (i = 0; i < count; i++) {
			if (ov9650_reg_QVGA[i].subaddr == CHIP_DELAY) {
				mdelay(ov9650_reg_QVGA[i].value);
				mdelay(10);
			} else {
				mdelay(10);
				sensor_write(client, ov9650_reg_QVGA[i].subaddr,
					ov9650_reg_QVGA[i].value);
			}
		}
		break;
	default:
		panic("ov9650.c: unexpect value \n");
	}

	return 0;
}

static int change_sensor_wb(struct i2c_client *client, int type)
{
      //not implement yet! bxl@hhtech 
       return 0;
}

static int
sensor_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	switch (cmd) {
	case SENSOR_INIT:
		sensor_init(client);
		printk(KERN_INFO "External Camera initialized\n");
		break;
		
	case USER_ADD:
		//MOD_INC_USE_COUNT;
		break;
		
	case USER_EXIT:
		//MOD_DEC_USE_COUNT;
		break;
		
	case SENSOR_VGA:
		change_sensor_size(client, SENSOR_VGA);
		break;
		
	case SENSOR_SXGA:
		change_sensor_size(client, SENSOR_SXGA);
		break;
		
	case SENSOR_QVGA:
		change_sensor_size(client, SENSOR_QVGA);
		break;
/* Todo
	case SENSOR_BRIGHTNESS:
		change_sensor_setting();
		break;
*/
	case SENSOR_WB:
        	change_sensor_wb(client, arg);
        	break;
	
	default:
		panic("ov9650.c : Unexpect Sensor Command \n");
		break;
	}
	
	return 0;
}

static struct i2c_driver sensor_driver = {
	.driver = {
		.name = "ov9650",
	},
      //name:"S5K3XA",
      .id = I2C_DRIVERID_OV9650,
      //flags:I2C_DF_NOTIFY,
      .attach_adapter = sensor_attach_adapter,
      .detach_client = sensor_detach,
      .command = sensor_command
};

static __init int camif_sensor_init(void)
{
	ov9650_power_on();	//bxl@hhtech
	return i2c_add_driver(&sensor_driver);
}


static __exit void camif_sensor_exit(void)
{
	i2c_del_driver(&sensor_driver);
}

module_init(camif_sensor_init)
module_exit(camif_sensor_exit)

MODULE_AUTHOR("bxl@hhtech");
MODULE_DESCRIPTION("I2C Client Driver For OV9650 V4L2 Driver");
MODULE_LICENSE("GPL");

