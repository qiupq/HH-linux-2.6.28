/*
 * drivers/video/s3c/s3cfb_a070vw04.c
 *
 * $Id: s3cfb_lte480wv.c,v 1.12 2008/06/05 02:13:24 jsgood Exp $
 *
 * Copyright (C) 2008 Jinsung Yang <jsgood.yang@samsung.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	S3C Frame Buffer Driver
 *	based on skeletonfb.c, sa1100fb.h, s3c2410fb.c
 */

#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-lcd.h>

#include "s3cfb.h"
#include <asm-arm/arch-s3c2410/gpio.h>

#define S3C_FB_HFP   40              /* front porch */
#define S3C_FB_HSW   128             /* Hsync width */
#define S3C_FB_HBP   88              /* Back porch */

#define S3C_FB_VFP   1               /* front porch */
#define S3C_FB_VSW   3               /* Vsync width */
#define S3C_FB_VBP   20		     /* Back porch */ 

#define S3C_FB_HRES		800	/* horizon pixel  x resolition */
#define S3C_FB_VRES		480	/* line cnt       y resolution */

#define S3C_FB_HRES_VIRTUAL	800	/* horizon pixel  x resolition */
#define S3C_FB_VRES_VIRTUAL	480	/* line cnt       y resolution */

#define S3C_FB_HRES_OSD		800	/* horizon pixel  x resolition */
#define S3C_FB_VRES_OSD		480	/* line cnt       y resolution */

#define S3C_FB_VFRAME_FREQ     	60	/* frame rate freq */

#define S3C_FB_PIXEL_CLOCK	(S3C_FB_VFRAME_FREQ * (S3C_FB_HFP + S3C_FB_HSW + S3C_FB_HBP + S3C_FB_HRES) * (S3C_FB_VFP + S3C_FB_VSW + S3C_FB_VBP + S3C_FB_VRES))

#define LCD_CS		S3C_GPL8	//S3C_GPM0
#define LCD_SCL		S3C_GPL10	//S3C_GPM1
#define LCD_SDA		S3C_GPL9	//S3C_GPM2

static void set_lcd_power(int val)
{
    if(val > 0) {
	/* Display On */
	s3cfb_init_hw();
        s3cfb_start_lcd();
    } else {
	/* Direct Off */
	s3cfb_stop_lcd();
    }
}

#define WAITTIME    (10 * HZ / 1000)    // 10ms
static int old_display_brightness = S3C_FB_MIN_BACKLIGHT_LEVEL;

static void __set_brightness(int val)
{
    int channel = 1;  // must use channel-1
    int usec = 0;       // don't care value 
    unsigned long tcnt=1000;
    unsigned long tcmp=0;

    if(val == S3C_FB_MAX_BACKLIGHT_LEVEL)
	gpio_direction_output(S3C_GPF15, 1);
    else if(val == S3C_FB_MIN_BACKLIGHT_LEVEL)
	gpio_direction_output(S3C_GPF15, 0);
    else {
	tcmp = val * 10;
#if defined(CONFIG_S3C6410_PWM) && defined(CONFIG_PWM)
	s3c6410_timer_setup (channel, usec, tcnt, tcmp);
#endif
    }
}

static void set_brightness(int val)
{
	int old_val = old_display_brightness;

	if(val < 0) val = 0;
	if(val > S3C_FB_MAX_BACKLIGHT_LEVEL) val = S3C_FB_MAX_BACKLIGHT_LEVEL;

	if(val > old_val) {
	    while((++old_val) < val) {
		__set_brightness(old_val);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(WAITTIME); 
	    }
	} else {
	    while((--old_val) > val) {
		__set_brightness(old_val);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(WAITTIME);
	    }
	}

	__set_brightness(val);
	old_display_brightness = val;	
}

static void set_backlight_power(int val)
{
    if(val > 0)
	__set_brightness(old_display_brightness);
    else
	__set_brightness(S3C_FB_MIN_BACKLIGHT_LEVEL);
}

static void s3cfb_set_fimd_info(void)
{
	s3c_fimd.vidcon1 = S3C_VIDCON1_IHSYNC_INVERT | S3C_VIDCON1_IVSYNC_INVERT | S3C_VIDCON1_IVDEN_NORMAL | S3C_VIDCON1_IVCLK_RISE_EDGE;
	s3c_fimd.vidtcon0 = S3C_VIDTCON0_VBPD(S3C_FB_VBP - 1) | S3C_VIDTCON0_VFPD(S3C_FB_VFP - 1) | S3C_VIDTCON0_VSPW(S3C_FB_VSW - 1);
	s3c_fimd.vidtcon1 = S3C_VIDTCON1_HBPD(S3C_FB_HBP - 1) | S3C_VIDTCON1_HFPD(S3C_FB_HFP - 1) | S3C_VIDTCON1_HSPW(S3C_FB_HSW - 1);
	s3c_fimd.vidtcon2 = S3C_VIDTCON2_LINEVAL(S3C_FB_VRES - 1) | S3C_VIDTCON2_HOZVAL(S3C_FB_HRES - 1);

	s3c_fimd.vidosd0a = S3C_VIDOSDxA_OSD_LTX_F(0) | S3C_VIDOSDxA_OSD_LTY_F(0);
	s3c_fimd.vidosd0b = S3C_VIDOSDxB_OSD_RBX_F(S3C_FB_HRES - 1) | S3C_VIDOSDxB_OSD_RBY_F(S3C_FB_VRES - 1);

	s3c_fimd.vidosd1a = S3C_VIDOSDxA_OSD_LTX_F(0) | S3C_VIDOSDxA_OSD_LTY_F(0);
	s3c_fimd.vidosd1b = S3C_VIDOSDxB_OSD_RBX_F(S3C_FB_HRES_OSD - 1) | S3C_VIDOSDxB_OSD_RBY_F(S3C_FB_VRES_OSD - 1);

	s3c_fimd.width = S3C_FB_HRES;
	s3c_fimd.height = S3C_FB_VRES;
	s3c_fimd.xres = S3C_FB_HRES;
	s3c_fimd.yres = S3C_FB_VRES;

#if defined(CONFIG_FB_S3C_VIRTUAL_SCREEN)
	s3c_fimd.xres_virtual = S3C_FB_HRES_VIRTUAL;
	s3c_fimd.yres_virtual = S3C_FB_VRES_VIRTUAL;
#else
	s3c_fimd.xres_virtual = S3C_FB_HRES;
	s3c_fimd.yres_virtual = S3C_FB_VRES;
#endif

	s3c_fimd.osd_width = S3C_FB_HRES_OSD;
	s3c_fimd.osd_height = S3C_FB_VRES_OSD;
	s3c_fimd.osd_xres = S3C_FB_HRES_OSD;
	s3c_fimd.osd_yres = S3C_FB_VRES_OSD;

	s3c_fimd.osd_xres_virtual = S3C_FB_HRES_OSD;
	s3c_fimd.osd_yres_virtual = S3C_FB_VRES_OSD;

	s3c_fimd.pixclock = S3C_FB_PIXEL_CLOCK;

	s3c_fimd.hsync_len = S3C_FB_HSW;
	s3c_fimd.vsync_len = S3C_FB_VSW;
	s3c_fimd.left_margin = S3C_FB_HFP;
	s3c_fimd.upper_margin = S3C_FB_VFP;
	s3c_fimd.right_margin = S3C_FB_HBP;
	s3c_fimd.lower_margin = S3C_FB_VBP;

	s3c_fimd.set_lcd_power = set_lcd_power;
	s3c_fimd.set_backlight_power = set_backlight_power;
	s3c_fimd.set_brightness = set_brightness;
	s3c_fimd.backlight_min = S3C_FB_MIN_BACKLIGHT_LEVEL;
	s3c_fimd.backlight_max = S3C_FB_MAX_BACKLIGHT_LEVEL; 
}

void s3cfb_init_hw(void)
{
	printk(KERN_INFO "LCD TYPE :: A070VW04 will be initialized\n");

	s3cfb_set_fimd_info();
	s3cfb_set_gpio();
	lcd_init_hw();
	set_brightness(old_display_brightness);
}

static void us_delay(unsigned int dly)
{
    udelay(dly);
}

static void write_bit(unsigned char  val)
{
    gpio_set_value(LCD_SCL, 0);	    // SCL = 0
    gpio_set_value(LCD_SDA, val);	    // SDA = val
    us_delay(1000);
    gpio_set_value(LCD_SCL, 1);	    // SCL = 1
    us_delay(1000);
}

static unsigned int read_bit()
{
    gpio_set_value(LCD_SCL, 0);	    // SCL = 0
    unsigned int data = s3c2410_gpio_getpin(LCD_SDA);	// SDA 
    us_delay(1000);
    gpio_set_value(LCD_SCL, 1);	    // SCL = 1
    us_delay(1000);
    return data;
}

static void write_reg(unsigned char addr, unsigned short data)
{
    unsigned char  i;
    gpio_set_value(LCD_CS, 0);    // CSB pin of LCD = 0
    /* transfer the register address bits (4 bit) */
    for(i=0; i<4; i++) {
	if(addr & 0x8)
	    write_bit(1);
	else
	    write_bit(0);
	addr <<= 1;
    }
    /* transfer the write mode bit (1 bit) */
    write_bit(0);
    /* transfer the data bits (11 bits) */
    write_bit(0);
    for(i=0; i<10; i++) {
        if(data & 0x200) 
            write_bit(1);
        else 
            write_bit(0);
        data <<= 1;
    }
    gpio_set_value(LCD_CS, 1);    // CSB pin of LCD = 1
}

static unsigned int read_reg(unsigned char addr)
{
    unsigned char  i;
    unsigned int data = 0x0;
    gpio_set_value(LCD_CS, 0);    // CSB pin of LCD = 0
    /* transfer the register address bits (4 bit) */
    for(i=0; i<4; i++) {
	if(addr & 0x8)
	    write_bit(1);
	else
	    write_bit(0);
	addr <<= 1;
    }
    /* transfer the read or read mode (1 bit) */
    write_bit(1);
    /* transfer the data (11 bits) */
    write_bit(0);
    for(i=0; i<10; i++) {
	data |= (read_bit() << i);
    }
    gpio_set_value(LCD_CS, 1);    // CSB pin of LCD = 1

    return data;
}

void lcd_init_hw(void)
{
    gpio_direction_output(LCD_CS, 1);	// GPL8 <---> CSB pin of LCD
    gpio_direction_output(LCD_SCL, 1);// GPL9 <---> SCL pin of LCD 
    gpio_direction_output(LCD_SDA, 1);	// XEINT14 <---> SDA pin of LCD
    gpio_direction_output(S3C_GPF14, 1);// GPF14 <---> POWER pin of LCD backlight
    write_reg(0x0, 0x2d3);
    write_reg(0x1, 0x181);
    write_reg(0x4, 0x19f);
}

