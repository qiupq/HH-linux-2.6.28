/*  
    Copyright (C) 2004 Samsung Electronics 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/signal.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/kmod.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <asm/io.h>
#include <asm/page.h>
//#include <asm/irq.h>
#include <asm/semaphore.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-camif.h>

#include <linux/mm.h>

//#define CAMIF_DEBUG 

//#include "s3c_camif.h"
#include "s3c_camif_fsm.h"
#include "videodev2_s3c.h"
//#include <linux/videodev.h>
#include <linux/videodev2.h>
#include <media/v4l2-dev.h>

#define S3C_V4L2_SUPPORT	1

/* Codec and Preview minor number : V4L2 compliance */
#define CODEC_MINOR 		12
#define PREVIEW_MINOR 		13
#define CAMIF_NUM  		2

#define FSM_Drived
#undef FSM_Drived

#if (defined (CONFIG_VIDEO_ADV7180)/* || defined (CONFIG_VIDEO_SAA7113)*/)
#define TOMTOM_INTERLACE_MODE
#define SW_IPC
#endif

static int preview_msdma_start_flag;	// For SW_IPC

camif_cfg_t fimc[CAMIF_NUM];

/*-------------  For changing size from VGA to SXGA -------------*/
typedef int (*camif_handle) (camif_cfg_t * cfg);
static camif_handle camif_enter_c = camif_enter_c_4fsm;
static camif_handle camif_start_c = camif_start_preview;
static int pre_src_x, pre_src_y, pre_ver, pre_hor;
/*--------------------------------------------------------------*/
extern camif_cis_t msdma_input;

#ifdef TOMTOM_INTERLACE_MODE
extern camif_cis_t interlace_input;
#endif

static const char *driver_version =
    "$Id: s3c_camera_driver.c,v 1.5 2007/10/11 07:48:54 yreom Exp $";
extern const char *camif_version;
extern const char *fsm_version;

extern void camif_start_c_with_p (camif_cfg_t *cfg, camif_cfg_t *other);
extern ssize_t camif_stop_codec(camif_cfg_t *cfg);
extern ssize_t camif_pc_stop(camif_cfg_t *cfg);
extern int camif_codec_msdma_start(camif_cfg_t * cfg);
extern camif_cis_t* get_initialized_cis();
extern int camif_preview_msdma_start(camif_cfg_t * cfg);
/*--------------- For V4L2 -----------------*/
static int camif_s_v4l2(camif_cfg_t *cfg);
/*------------------------------------------*/

camif_cfg_t *get_camif(int nr)
{
	camif_cfg_t *ret = NULL;
	
	switch (nr) {
		case CODEC_MINOR:
			ret = &fimc[0];
			break;
			
		case PREVIEW_MINOR:
			ret = &fimc[1];
			break;
			
		default:
			printk("s3c_camera_drvier.c : Unknow Minor Number \n");
	}
	return ret;
}


static int camif_start_codec(camif_cfg_t * cfg)
{
	int ret = 0;
	
#if defined(CONFIG_CPU_S3C2412)
	ret = camif_check_preview_in_CODEC(cfg);
	
	switch (ret) {
	case 0:		/* Play alone */
		DPRINTK("Start Alone \n");
		camif_start_c(cfg);
		cfg->cis->status |= C_WORKING;
		break;
		
	case -ERESTARTSYS:	/* Busy , retry */
		printk(KERN_INFO "Error:Retry \n");
		break;
		
	case 1:
		printk(KERN_INFO "Ask preview for callback\n");
		ret = camif_callback_start(cfg);
		if (ret < 0) {
			printk(KERN_INFO "Busy RESTART \n");
			return ret;	/* Busy, retry */
		}
		break;
	}
#else
	camif_reset(CAMIF_RESET,0); /* FIFO Count goes to zero */
	cfg->capture_enable = CAMIF_DMA_ON;
	camif_capture_start(cfg);
	cfg->status = CAMIF_STARTED;
	cfg->fsm = CAMIF_1st_INT;
	cfg->perf.frames = 0;

#if (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
	if(cfg->input_channel == MSDMA_FROM_CODEC){
		camif_codec_msdma_start(cfg);
	}
#endif
#endif
	return ret;
}

	
ssize_t camif_write(struct file * f, const char *b, size_t c,
		    loff_t * offset)
{
	camif_cfg_t *cfg;

	int ret = 0;			/* return value */
	cfg = get_camif(MINOR(f->f_dentry->d_inode->i_rdev));
	
	switch (*b) {
	case 'O':
		if (cfg->dma_type & CAMIF_PREVIEW) {
#if defined(CONFIG_CPU_S3C2412)
			if (cfg->cis->status & C_WORKING) {
				camif_start_c_with_p(cfg,
						     get_camif
						     (CODEC_MINOR));
			} else 
#endif
				camif_start_preview(cfg);
			
		} else {
			ret = camif_start_codec(cfg);
			if (ret < 0)
				ret = 1;	/* Error and neet to retry */
		}
		break;
		
	case 'X':
		if (cfg->dma_type & CAMIF_PREVIEW) {
			camif_stop_preview(cfg);
			cfg->cis->status |= P_NOT_WORKING;
		} else {	/* Codec */
			cfg->cis->status &= ~C_WORKING;
			camif_stop_codec(cfg);
		}
		break;

#if defined(CONFIG_CPU_S3C2443)
	case 'P':
		if (cfg->dma_type & CAMIF_PREVIEW) {
			camif_start_preview(cfg);
			camif_start_postprocessing(cfg);
			return 0;
		}
		else {
			return -EFAULT;
		}
#endif
	default:
		panic("s3c_camera_driver.c: camif_write() - Unexpected Parameter\n");
	}

	return ret;
}

ssize_t camif_p_read(struct file * file, char *buf, size_t count,
		     loff_t * pos)
{
	camif_cfg_t *cfg = NULL;
	size_t end;

	cfg = get_camif(MINOR(file->f_dentry->d_inode->i_rdev));
	
#ifdef FSM_Drived
	cfg->status = CAMIF_STARTED;
	
	if (wait_event_interruptible
	    (cfg->waitq, cfg->status == CAMIF_INT_HAPPEN))
		return -ERESTARTSYS;

	cfg->status = CAMIF_STOPPED;
#endif
	end = min_t(size_t, cfg->pp_totalsize / cfg->pp_num, count);
	if (copy_to_user(buf, camif_g_frame(cfg), end))
		return -EFAULT;
	return end;
}

ssize_t
camif_c_read(struct file *file, char *buf, size_t count, loff_t * pos)
{
	camif_cfg_t *cfg = NULL;
	size_t end;

	/* cfg = file->private_data; */
	cfg = get_camif(MINOR(file->f_dentry->d_inode->i_rdev));

#if 0
	if (file->f_flags & O_NONBLOCK) {
		printk(KERN_ERR "Don't Support NON_BLOCK \n");
	}
#endif

	/* Change the below wait_event_interruptible func */
	if (wait_event_interruptible
	    (cfg->waitq, cfg->status == CAMIF_INT_HAPPEN))
		return -ERESTARTSYS;
	
	cfg->status = CAMIF_STOPPED;
	end = min_t(size_t, cfg->pp_totalsize / cfg->pp_num, count);
	if (copy_to_user(buf, camif_g_frame(cfg), end))
		return -EFAULT;
	return end;
}


static irqreturn_t camif_f_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	int ret;
	camif_cfg_t *cfg = (camif_cfg_t *) dev_id;

	DPRINTK("camif_f_irq !!!  \n"); 
#ifdef CONFIG_CPU_S3C2443
	free_irq(IRQ_EINT0, cfg);
	cfg->check_even_odd = FIELD_ODD;
	
	ret = camif_start_codec(cfg);
#elif (defined(CONFIG_CPU_S3C6400)||defined(CONFIG_CPU_S3C6410))
	if(cfg->status == CAMIF_STOPPED) {
		set_irq_type(IRQ_EINT14, IRQT_FALLING);
		cfg->check_even_odd = FIELD_ODD;
		wake_up_interruptible(&cfg->waitq);
		
	} else {
		switch(cfg->check_even_odd) {
			case FIELD_ODD:
				set_irq_type(IRQ_EINT14, IRQT_RISING);
				cfg->check_even_odd = FIELD_EVEN;
				break;
			case FIELD_EVEN:
				set_irq_type(IRQ_EINT14, IRQT_FALLING);
				cfg->check_even_odd = FIELD_ODD;
				break;
			default:
				break;
		}

	}
#endif
	return IRQ_HANDLED;
}


static irqreturn_t camif_c_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	camif_cfg_t *cfg = (camif_cfg_t *) dev_id;
	camif_cfg_t *cfg_preview = NULL;
	u32 i;
	int val; 
	
#if (defined(CONFIG_CPU_S3C6400)||defined(CONFIG_CPU_S3C6410))
	s3c_gpio_setpin(S3C_GPN15, 1);
#endif

	clear_camif_irq(irq);
	camif_g_fifo_status(cfg);
	camif_g_frame_num(cfg);
#if (defined(CONFIG_CPU_S3C6400)||defined(CONFIG_CPU_S3C6410))
	s3c_gpio_setpin(S3C_GPN15, 0);
#endif

#ifdef SW_IPC
	if(preview_msdma_start_flag == 1) {
		cfg_preview = get_camif(PREVIEW_MINOR);
		switch(cfg->now_frame_num) {
			case 0:
			case 2:
				break;
			case 1:
				i = __raw_readl(S3C_CICOYSA1);
				__raw_writel(i, S3C_MSPRY0SA);
				i += (cfg_preview->cis->source_x * cfg_preview->cis->source_y *2);
				__raw_writel(i, S3C_MSPRY0END);
				camif_preview_msdma_start(cfg_preview);
				break;
			case 3:
				i = __raw_readl(S3C_CICOYSA3);
				__raw_writel(i, S3C_MSPRY0SA);
				i += (cfg_preview->cis->source_x * cfg_preview->cis->source_y *2);
				__raw_writel(i, S3C_MSPRY0END);
				camif_preview_msdma_start(cfg_preview);
				break;
			default:
				printk("xxx\n");
				break;
		}
	}
		
#endif

	if (camif_enter_c(cfg) == INSTANT_SKIP)
		return IRQ_HANDLED; 
	wake_up_interruptible(&cfg->waitq);

	return IRQ_HANDLED;
}

static irqreturn_t camif_p_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	camif_cfg_t *cfg = (camif_cfg_t *) dev_id;
	int val;
	
	clear_camif_irq(irq);
	camif_g_fifo_status(cfg);
	camif_g_frame_num(cfg);
	wake_up_interruptible(&cfg->waitq);
#ifdef FSM_Drived
	if (camif_enter_p_4fsm(cfg) == INSTANT_SKIP)
		return IRQ_HANDLED; 
	wake_up_interruptible(&cfg->waitq);
#endif
	return IRQ_HANDLED;
}

static void camif_release_irq(camif_cfg_t * cfg)
{
	int i;
	disable_irq(cfg->irq);
	free_irq(cfg->irq, cfg);

#if (defined(CONFIG_CPU_S3C6400)||defined(CONFIG_CPU_S3C6410)) && defined(TOMTOM_INTERLACE_MODE)
	if (cfg->dma_type & CAMIF_CODEC) {
		i = __raw_readl(S3C_EINTMASK);
		i |= (1<<14);
		__raw_writel(i, S3C_EINTMASK);

		i = __raw_readl(S3C_EINTPEND);
		if(i&(1<<14))
			__raw_writel(1<<14, S3C_EINTPEND);
		disable_irq(IRQ_EINT14);
		free_irq(IRQ_EINT14, cfg);
	}
#endif
}

static int camif_irq_request(camif_cfg_t * cfg)
{
	int ret = 0;
	
	if (cfg->dma_type & CAMIF_CODEC) {
		if ((ret = request_irq(cfg->irq, camif_c_irq,
				       SA_INTERRUPT, cfg->shortname,
				       cfg))) {
			printk("request_irq(CAM_C) failed.\n");
		}
	}
	if (cfg->dma_type & CAMIF_PREVIEW) {
		if ((ret = request_irq(cfg->irq, camif_p_irq,
				       SA_INTERRUPT, cfg->shortname,
				       cfg))) {
			printk("request_irq(CAM_P) failed.\n");
		}
	}
	return 0;
}

#ifndef TOMTOM_INTERLACE_MODE
static void camif_init_sensor(camif_cfg_t * cfg)
{
	camif_cis_t *cis = cfg->cis;
	camif_cis_t *initialized_cis;
	
	if (!cis->sensor) {
		initialized_cis = get_initialized_cis();
		
		if(initialized_cis == NULL) {
			printk(KERN_ERR "s3c_camera_driver.c : An I2C Client for CIS sensor isn't registered\n");
			return -1;
		}
		cis = cfg->cis = initialized_cis;
		cfg->input_channel = 0;
		cfg->cis->user++;
	}
	
	if (!cis->init_sensor) {
		//      camif_reset(cis->reset_type, cis->reset_udelay);
		/* Aftering sending reset cmd to sensor,
		   we wait for some time
		 */
		cis->sensor->driver->command(cis->sensor, SENSOR_INIT, NULL);
		cis->init_sensor = 1;	/*sensor init done */
		
#if defined(CONFIG_VIDEO_SAMSUNG_S5K3BA)
		cis->sensor->driver->command(cis->sensor, SENSOR_VGA, NULL);
	        cis->source_x = 640;
	        cis->source_y = 480;
#endif
	}
	cis->sensor->driver->command(cis->sensor, USER_ADD, NULL);
}

#else
static void camif_init_sensor(camif_cfg_t * cfg)
{
	camif_cis_t *cis = cfg->cis;
	camif_cis_t *initialized_cis;
	int tmp;
	
	if (!cis->sensor) {
		initialized_cis = get_initialized_cis();
		
		if(initialized_cis == NULL) {
			printk(KERN_ERR "s3c_camera_driver.c : An I2C Client for CIS sensor isn't registered\n");
			return -1;
		}
		cis = cfg->cis = initialized_cis;
		cfg->input_channel = 0;
		cfg->cis->user++;
	}
	
	if (!cis->init_sensor) {
		cis->sensor->driver->command(cis->sensor, USER_ADD, NULL);	// Reset ADV7180
		
		tmp = 0; //Composite
		cis->sensor->driver->command(cis->sensor, DECODER_SET_NORM, &tmp);

		tmp = 0; // Pin  37 output is Vsync signal
		cis->sensor->driver->command(cis->sensor, DECODER_SET_GPIO, &tmp); 
		cis->init_sensor = 1;	/*sensor init done */
		/* For Debugging */
		cis->sensor->driver->command(cis->sensor, DECODER_GET_STATUS, &tmp);
		printk("ADV7180 status = 0x%x\n", tmp);		
	}
	
}

#endif

 int camif_open(struct inode *inode, struct file *file)
{
	int err;
	camif_cfg_t *cfg = get_camif(MINOR(inode->i_rdev));

	if (!cfg->cis) {
		printk(KERN_ERR "s3c_camera_driver.c : An object for a CIS is missing \n");
		printk(KERN_ERR " using msdma_input as a default CIS data structure !!!\n");
		cfg->cis = &msdma_input;
		sema_init(&cfg->cis->lock,1);		/* global lock for both Codec and Preview */
		cfg->cis->status |= P_NOT_WORKING;	/* Default Value */
	}

	if (cfg->dma_type & CAMIF_PREVIEW) {
		//if (down_interruptible(&cfg->cis->lock))
		//	return -ERESTARTSYS;
		if (cfg->dma_type & CAMIF_PREVIEW) {
			cfg->cis->status &= ~P_NOT_WORKING;
		}
		up(&cfg->cis->lock);
	}
	
	err = video_exclusive_open(inode, file);
	cfg->cis->user++;
	cfg->status = CAMIF_STOPPED;
	
	if (err < 0)
		return err;
	if (file->f_flags & O_NONCAP) {
		printk("Don't Support Non-capturing open !!\n");
		return 0;
	}
	file->private_data = cfg;

	camif_irq_request(cfg);
	camif_init_sensor(cfg);

	/* For setting up V4L2 */
	camif_s_v4l2(cfg);
	
	return 0;
}



int camif_release(struct inode *inode, struct file *file)
{
	int i;
	camif_cfg_t *cfg = get_camif(MINOR(inode->i_rdev));

	DPRINTK(" cfg->status 0x%0X cfg->cis->status 0x%0X \n", cfg->status,cfg->cis->status );
#if 0 
	printk("S3C_CICOSTATUS is 0x%x\n",readl(S3C_CICOSTATUS));
	for(i=0;i<79;i++){
		printk("offset 0x%x,data is 0x%x\n",i*4,readl(S3C_CISRCFMT+i*4));
	}
#endif

	
	if (cfg->dma_type & CAMIF_PREVIEW) {
		////if (down_interruptible(&cfg->cis->lock))
		////	return -ERESTARTSYS;
		cfg->cis->status &= ~PWANT2START;
		cfg->cis->status |= P_NOT_WORKING;
		camif_stop_preview(cfg);
		up(&cfg->cis->lock);
	} else {
		cfg->cis->status &= ~CWANT2START;	/* No need semaphore */
		camif_stop_codec(cfg);
	}
	
	camif_dynamic_close(cfg);
	camif_release_irq(cfg);
	video_exclusive_release(inode, file);

	if(cfg->cis->sensor == NULL) {
		DPRINTK("A CIS sensor for MSDMA has been used.. \n");
	}
	else {
		cfg->cis->sensor->driver->command(cfg->cis->sensor, USER_EXIT, NULL);
	}
	cfg->cis->user--;
	cfg->status = CAMIF_STOPPED;
	return 0;
}

/* 1. 3AA camera
 * Purpose: change input image size from VGA to SXGA
 * <1>.Change the status of external module camera 
 *   -- Samsung SXGA Camera needs 1 second for changing from VGA to SXGA
 * <2>.For SXGA Camera, set win_hor_ofst,win_ver_ofst to zero 
 *
 * 2. 3BA camera
 * Purpose: change input image size from SVGA to UXGA
 * <1>.Change the status of external module camera 
 *   -- Samsung UXGA Camera needs 1 second for changing from SVGA to UXGA
 * <2>.For UXGA Camera, set win_hor_ofst,win_ver_ofst to zero 
 */
 

static void camif_2_pic_mode(camif_cfg_t * cfg)
{
	camif_cis_t *cis = cfg->cis;

	/* Stop All fimc engine */
	camif_pc_stop(cfg);
	
	/* Back up */
	pre_src_x = cis->source_x;
	pre_src_y = cis->source_y;
	pre_hor = cis->win_hor_ofst;
	pre_ver = cis->win_ver_ofst;

#if (defined(CONFIG_VIDEO_SAMSUNG_S5K3AA) || defined(CONFIG_VIDEO_OV9650)) 
	printk(KERN_INFO "Resolution of this camera changed into SXGA(1280x1024) mode. \n");

	cis->sensor->driver->command(cis->sensor, SENSOR_SXGA, NULL);
	cis->source_x = 1280;
	cis->source_y = 1024;

#elif defined(CONFIG_VIDEO_SAMSUNG_S5K3BA)
	printk(KERN_INFO "Resolution of this camera changed into UXGA(1600x1200) mode. \n");

	cis->sensor->driver->command(cis->sensor, SENSOR_VGA, NULL);
	cis->source_x = 640;
	cis->source_y = 480;
#else
//#error you must select a samsung CIS module.
#endif
	cis->win_hor_ofst = 0;
	cis->win_ver_ofst = 0;
	
	/* change source fmt of fimc 2.x */
	camif_source_fmt(cis);
}

static void camif_2_camcoder_mode(camif_cfg_t * cfg)
{
	camif_cis_t *cis = cfg->cis;
	camif_pc_stop(cfg);
	
#if (defined(CONFIG_VIDEO_SAMSUNG_S5K3AA) || defined(CONFIG_VIDEO_OV9650)) 
	printk(KERN_INFO "External Camera changed BACK to VGA(640x480) mode(default mode). \n");
	cis->sensor->driver->command(cis->sensor, SENSOR_VGA, NULL);

#elif defined(CONFIG_VIDEO_SAMSUNG_S5K3BA)	
	printk(KERN_INFO "External Camera changed BACK to SVGA(800x600) mode(default mode). \n");
	cis->sensor->driver->command(cis->sensor, SENSOR_UXGA, NULL);
#else
//#error you must select a samsung CIS module.
#endif

	/* Back up */
	cis->source_x = pre_src_x;
	cis->source_y = pre_src_y;
	cis->win_hor_ofst = pre_hor;
	cis->win_ver_ofst = pre_ver;

	/* change source fmt of fimc 2.x */
	camif_source_fmt(cis);
}

/* To saving the codec pingpong memory,
    use only one pingpong
 */
static void codec_uses_1pp(camif_cfg_t * cfg)
{
	if (!cfg->dma_type & CAMIF_CODEC) {
		printk("NOT expected uses 1pp.. \n");
		return;
	}

	cfg->pp_num = 1;
	camif_enter_c = camif_enter_1fsm;
	camif_start_c = camif_start_1fsm;
}

static void codec_uses_2pp(camif_cfg_t * cfg)
{
	if (!cfg->dma_type & CAMIF_CODEC) {
		printk("NOT expected uses 1pp.. \n");
		return;
	}

	cfg->pp_num = 2;
	camif_enter_c = camif_enter_2fsm;
	camif_start_c = camif_start_2fsm;
}


static void codec_uses_4pp(camif_cfg_t * cfg)
{
	camif_cis_t *cis = cfg->cis;

	if (!cfg->dma_type & CAMIF_CODEC) {
		//panic("unexpected condition\n");
		return;
	}
	cfg->pp_num = 4;
	camif_enter_c = camif_enter_c_4fsm;
	camif_start_c = camif_start_preview;
}


	
/* To parse the user command line */
static void fimc_config(camif_cfg_t *cfg, camif_param_t param)
{
	cfg->target_x = param.dst_x;
	cfg->target_y = param.dst_y;

	switch (param.src_fmt) {
		case 16:
			cfg->src_fmt = CAMIF_RGB16;
			break;
			
		case 24:
			cfg->src_fmt = CAMIF_RGB24;
			break;

		case 420:
			cfg->src_fmt = CAMIF_YCBCR420;
			break;
			
		case 422:
		default:
			cfg->src_fmt = CAMIF_YCBCR422;
			break;
	}

	switch (param.dst_fmt) {
		case 16:
			cfg->dst_fmt = CAMIF_RGB16;
			break;
			
		case 24:
			cfg->dst_fmt = CAMIF_RGB24;
			break;
			
		case 420:
			cfg->dst_fmt = CAMIF_YCBCR420;
			break;
			
		case 422:
			cfg->dst_fmt = CAMIF_YCBCR422;
			break;
			
		default:
			panic("s3c_camera_driver.c : Invalid BPP on destination \n");
			break;
	}
	
	switch (param.flip) {
		case 3:
			cfg->flip = CAMIF_FLIP_MIRROR;
			break;
		case 1:
			cfg->flip = CAMIF_FLIP_X;
			break;
		case 2:
			cfg->flip = CAMIF_FLIP_Y;
			break;
		case 0:
		default:
			cfg->flip = CAMIF_FLIP;
			break;
	}


#if (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
	/* 2007-05-11 by YROM   PIP for S3C6400 */
	switch(param.input_channel){
		case 2: 
			cfg->input_channel = MSDMA_FROM_PREVIEW;
			cfg->cis->source_x = param.src_x;
			cfg->cis->source_y = param.src_y;
			break;
		case 1: 
			cfg->cis->user--;   /* CIS will be replaced with a CIS for MSDMA */
			
			cfg->cis = &msdma_input;
			cfg->cis->user++;
			cfg->input_channel = MSDMA_FROM_CODEC;
			cfg->cis->source_x = param.src_x;
			cfg->cis->source_y = param.src_y;
			break;
		case 0:
		default:
			cfg->input_channel = CAMERA_INPUT;
			break;
	}

	switch(param.output_channel){
		case 1: 
			cfg->output_channel = CAMIF_OUT_FIFO;
			break;
		case 0:
		default:
			cfg->output_channel = CAMIF_OUT_PP;
			break;
	}
#endif
	cfg->cis->win_hor_ofst = param.h_offset;
	cfg->cis->win_ver_ofst = param.v_offset;
	cfg->cis->win_hor_ofst2 = param.h_offset2;
	cfg->cis->win_ver_ofst2 = param.v_offset2;


}

#define ZOOM_AT_A_TIME_IN_PIXELS 	30
#define ZOOM_IN_MAX	640

static int camif_make_underzoom_out_check(camif_cfg_t *cfg) {

	if(cfg->sc.zoom_in_cnt > 0) {
		cfg->sc.zoom_in_cnt--;
		return 1;
	}
	else {
                printk("Invalid ZOOM-OUT : This ZOOM-OUT on preview scaler already comes to the MINIMUM !!! \n");
		return 0;
	}
}

static int camif_make_overzoom_in_check(camif_cfg_t *cfg) {

	if(((cfg->sc.modified_src_x-(cfg->cis->win_hor_ofst+ZOOM_AT_A_TIME_IN_PIXELS+cfg->cis->win_hor_ofst2+ZOOM_AT_A_TIME_IN_PIXELS))/cfg->sc.prehratio)>ZOOM_IN_MAX) {
                printk("Invalid ZOOM-IN : This ZOOM-IN on preview scaler already comes to the MAXIMUM !!! \n");
		return 0;
	}
	cfg->sc.zoom_in_cnt++;
	return 1;
}
	
static int camif_cis_control(camif_cfg_t *cfg, unsigned int cmd, int arg) 
{
	cfg->cis->sensor->driver->command(cfg->cis->sensor, cmd, arg);
	return 0;
}

static int camif_change_zoom(camif_cfg_t *cfg)
{	
	int ret = 0;
	camif_dynamic_close(cfg);
	
	camif_stop_preview(cfg);
	
	if (camif_setup_fimc_controller(cfg)) {
		printk(" camif_change_zoom() - CMD_SENSOR_ZOOMIN(OUT): Error Happens \n");
		ret = -1;
	}
	
	camif_start_preview(cfg);
	return ret;
}


int
camif_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	    unsigned long arg)
{
	int ret = 0;
	camif_cfg_t *cfg = file->private_data;
	camif_param_t par;

	switch (cmd) {
		case CMD_CAMERA_INIT:
			if (copy_from_user(&par, (camif_param_t *) arg, sizeof(camif_param_t)))
				return -EFAULT;
			
			fimc_config(cfg, par);
			if (camif_setup_fimc_controller(cfg)) {
				printk(" CMD_CAMERA_INIT : Some Errors Happen in camif_setup_fimc_controller() \n");
				ret = -1;
			}
			camif_change_flip(cfg);
			break;
			
			   
		case CMD_SENSOR_ZOOMIN:
			if(camif_make_overzoom_in_check(cfg)) {
				
				cfg->cis->win_hor_ofst += ZOOM_AT_A_TIME_IN_PIXELS;
				cfg->cis->win_ver_ofst += ZOOM_AT_A_TIME_IN_PIXELS;
				cfg->cis->win_hor_ofst2 += ZOOM_AT_A_TIME_IN_PIXELS;
				cfg->cis->win_ver_ofst2 += ZOOM_AT_A_TIME_IN_PIXELS;
				
				if (camif_change_zoom(cfg)) {
					printk(" CMD_SENSOR_ZOOM-IN: Error Happens \n");
					ret = -1;
				}
			}
			else {
				ret = -1;
			}
			break;
			
		case CMD_SENSOR_ZOOMOUT:
			if(camif_make_underzoom_out_check(cfg)) {
				
				cfg->cis->win_hor_ofst -= ZOOM_AT_A_TIME_IN_PIXELS;
				cfg->cis->win_ver_ofst -= ZOOM_AT_A_TIME_IN_PIXELS;
				cfg->cis->win_hor_ofst2 -= ZOOM_AT_A_TIME_IN_PIXELS;
				cfg->cis->win_ver_ofst2 -= ZOOM_AT_A_TIME_IN_PIXELS;
				
				if (camif_change_zoom(cfg)) {
					printk(" CMD_SENSOR_ZOOM-OUT: Error Happens \n");
					ret = -1;
				}
			}
			else {
				ret = -1;
			}
			break;
			
		case CMD_SENSOR_MIRROR:
			if (copy_from_user(&par, (camif_param_t *) arg, sizeof(camif_param_t)))
				return -EFAULT;
			
			switch(par.flip) {
				case 5:
					cfg->flip = CAMIF_FLIP_ROTATE_270;
					break;
				case 4:
					cfg->flip = CAMIF_ROTATE_90;
					break;
				case 3:
					cfg->flip = CAMIF_FLIP_MIRROR;
					break;
				case 2:
					cfg->flip = CAMIF_FLIP_Y;
					break;
				case 1:
					cfg->flip = CAMIF_FLIP_X;
					break;
				case 0:
				default:
					cfg->flip = CAMIF_FLIP;
					break;
			}
			camif_change_flip(cfg);
			break;
			
		case CMD_SENSOR_AF:
		case CMD_SENSOR_WB:
			if (copy_from_user(&par, (camif_param_t *) arg, sizeof(camif_param_t)))
				return -EFAULT;
			camif_cis_control(cfg, SENSOR_WB, par.awb);
			break;

		/* Todo
		case CMD_SENSOR_BRIGHTNESS:
			if (copy_from_user(&par, (camif_param_t *) arg, sizeof(camif_param_t)))
				return -EFAULT;
			cfg->cis->sensor->driver->command(cfg->cis->sensor, SENSOR_BRIGHTNESS, NULL);
			break;
		*/
			
		case CMD_PICTURE_MODE:
			camif_2_pic_mode(cfg);
			codec_uses_2pp(cfg);
			break;

		case CMD_CAMCODER_MODE:
			camif_2_camcoder_mode(cfg);
			codec_uses_4pp(cfg);
			break;
			
		case CMD_SENSOR_IMAGE_EFFECT:
			if (copy_from_user(&par, (camif_param_t *) arg, sizeof(camif_param_t)))
				return -EFAULT;
				
			switch(par.effect) {
				case 5:
					cfg->effect = CAMIF_SILHOUETTE;
					break;
				case 4:
					cfg->effect = CAMIF_EMBOSSING;
					break;
				case 3:
					cfg->effect = CAMIF_ART_FREEZE;
					break;
				case 2:
					cfg->effect = CAMIF_NEGATIVE;
					break;
				case 1:
					cfg->effect = CAMIF_ARBITRARY_CB_CR;
					break;
				case 0:
				default:
					cfg->effect = CAMIF_BYPASS;
					break;
			}
			
			camif_change_image_effect(cfg);
			break;

		case CMD_POSTPROCESSING_INIT:
			if (copy_from_user(&par, (camif_param_t *) arg, sizeof(camif_param_t)))
				return -EFAULT;

			cfg->cis->source_x = par.src_x;
			cfg->cis->source_y = par.src_y;
			
			switch(par.src_fmt) {
				default:
				case 420:
					cfg->input_channel = CAMIF_YCBCR420;
					break;
			
				case 422:
					cfg->input_channel = CAMIF_YCBCR422;
					break;
			}
			
			fimc_config(cfg, par);

			if (camif_setup_fimc_controller(cfg)) {
				printk("CMD_POSTPROCESSING_INIT : Error Happens \n");
				ret = -1;
			}
			camif_change_flip(cfg);
			break;
			   
		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}


/* ------------------------------------------ V4L2 SUPPORT ----------------------------------------------*/
/* ------------- In CODEC and preview path, v4l2_input supported by S3C FIMC controller ------------------*/
static struct v4l2_input fimc_inputs[] = {
	{
		.index		= 0,
		.name		= "S3C FIMC camera input",
		.type		= V4L2_INPUT_TYPE_CAMERA,
		.audioset	= 1,
		.tuner		= 0, /* ignored */
		.std		= V4L2_STD_PAL_BG|V4L2_STD_NTSC_M,
		.status		= 0,
	}, 
	{
		.index		= 1,
		.name		= "Memory input (MSDMA)",
		.type		= V4L2_INPUT_TYPE_MSDMA,
		.audioset	= 2,
		.tuner		= 0,
		.std		= V4L2_STD_PAL_BG|V4L2_STD_NTSC_M,
		.status		= 0,
	},
	{
		.index		= 2,
		.name		= "Interlace signal input ",
		.type		= V4L2_INPUT_TYPE_INTERLACE,
		.audioset	= 2,
		.tuner		= 0,
		.std		= V4L2_STD_PAL_BG|V4L2_STD_NTSC_M,
		.status		= 0,
	} 
};

/* ------------ In CODEC and preview path, v4l2_output supported by S3C FIMC controller ----------------*/
static struct v4l2_output fimc_outputs[] = {
	{
		.index		= 0,
		.name		= "Pingpong memory",
		.type		= 0,
		.audioset	= 0,
		.modulator	= 0, 
		.std		= 0,
	}, 
	{
		.index		= 1,
		.name		= "LCD FIFO_OUT",
		.type		= 0,
		.audioset	= 0,
		.modulator	= 0,
		.std		= 0,
	} 
};


/* 
  Codec_formats/Preview_format[0] must be same to initial value of 
  preview_init_param/codec_init_param 
*/

/* ------------------ In CODEC path, v4l2_fmtdesc supported by S3C FIMC controller------------------------*/
const struct v4l2_fmtdesc fimc_codec_formats[] = {
	{
		.index    = 0,
		.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.description = "16 bpp RGB, le",
		.pixelformat   = V4L2_PIX_FMT_RGB565,
		.flags    = FORMAT_FLAGS_PACKED,
	},
	{
		.index    = 1,
		.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags    = FORMAT_FLAGS_PACKED,
		.description = "32 bpp RGB, le",
		.pixelformat   = V4L2_PIX_FMT_BGR32,
	},
	{
		.index     = 2,
		.type      = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags     = FORMAT_FLAGS_PLANAR,
		.description = "4:2:2, planar, Y-Cb-Cr",
		.pixelformat = V4L2_PIX_FMT_YUV422P,

	},
	{
		.index    = 3,
		.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags    = FORMAT_FLAGS_PLANAR,
		.description     = "4:2:0, planar, Y-Cb-Cr",
		.pixelformat   = V4L2_PIX_FMT_YUV420,
	}
};


/* 
   FIMC V4L2_PIX_FMT_RGB565 is not same to that of V4L2spec 
   and so we need image convert to FIMC V4l2_PIX_FMT_RGB565.
*/
/* ------------------ In preview path, v4l2_fmtdesc supported by S3C FIMC controller------------------------*/
const struct v4l2_fmtdesc fimc_preview_formats[] = {
	{
		.index    = 0,
		.type     = V4L2_BUF_TYPE_VIDEO_OVERLAY,
		.description = "16 bpp RGB, le",
		.pixelformat   = V4L2_PIX_FMT_RGB565,
		.flags    = FORMAT_FLAGS_PACKED,
	},
	{
		.index    = 1,
		.type     = V4L2_BUF_TYPE_VIDEO_OVERLAY,
		.flags    = FORMAT_FLAGS_PACKED,
		.description = "32 bpp RGB, le",
		.pixelformat   = V4L2_PIX_FMT_BGR32,
	},
	{
		.index     = 2,
		.type      = V4L2_BUF_TYPE_VIDEO_OVERLAY,
		.flags     = FORMAT_FLAGS_PLANAR,
		.description = "4:2:2, planar, Y-Cb-Cr",
		.pixelformat = V4L2_PIX_FMT_YUV422P,

	},
	{
		.index    = 3,
		.type     = V4L2_BUF_TYPE_VIDEO_OVERLAY,
		.flags    = FORMAT_FLAGS_PLANAR,
		.description     = "4:2:0, planar, Y-Cb-Cr",
		.pixelformat   = V4L2_PIX_FMT_YUV420,
	}
};


#define NUMBER_OF_PREVIEW_FORMATS  	ARRAY_SIZE(fimc_preview_formats)
#define NUMBER_OF_CODEC_FORMATS	        ARRAY_SIZE(fimc_codec_formats)
#define NUMBER_OF_INPUTS	        ARRAY_SIZE(fimc_inputs)
#define NUMBER_OF_OUTPUTS	        ARRAY_SIZE(fimc_outputs)



/* 
 * This function and v4l2 structure made for V4L2 API functions 
 *     App <--> v4l2 <--> logical param <--> hardware
 */
static v4l2_t camif_get_v4l2(camif_cfg_t *cfg)
{	
	return cfg->v2;
}


/*
** Gives the depth of a video4linux2 fourcc aka pixel format in bits.
*/
static int pixfmt2depth(int pixfmt, int *fmtptr)
{
	int fmt=CAMIF_YCBCR420;
	int depth=12;    
	
	switch (pixfmt) {
		case V4L2_PIX_FMT_RGB565:
		case V4L2_PIX_FMT_RGB565X:
			fmt = CAMIF_RGB16;
			depth = 16;
			break;
		case V4L2_PIX_FMT_BGR24: /* Not tested */
		case V4L2_PIX_FMT_RGB24:
			fmt = CAMIF_RGB24;
			depth = 24;
			break;
		case V4L2_PIX_FMT_BGR32:
		case V4L2_PIX_FMT_RGB32:
			fmt = CAMIF_RGB24;
			depth = 32;
			break;
		case V4L2_PIX_FMT_GREY:	/* Not tested  */
			fmt = CAMIF_YCBCR420;
			depth = 8;
			break;
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_YUV422P:
			fmt = CAMIF_YCBCR422;
			depth = 16;
			break;
		case V4L2_PIX_FMT_YUV420:
			fmt = CAMIF_YCBCR420;
			depth = 12;
			
			break;
		}
	
	if (fmtptr) *fmtptr = fmt;		
	return depth;
}


static int camif_s_v4l2_pixfmt(camif_cfg_t *cfg, int depth, int fourcc) {
	
	/* To define v4l2_format used currently */
	cfg->v2.frmbuf.fmt.width    = cfg->target_x;
	cfg->v2.frmbuf.fmt.height   = cfg->target_y;
	cfg->v2.frmbuf.fmt.field    = V4L2_FIELD_NONE;
	cfg->v2.frmbuf.fmt.pixelformat = fourcc;
		
	cfg->v2.frmbuf.fmt.bytesperline= cfg->v2.frmbuf.fmt.width*depth >> 3;
	cfg->v2.frmbuf.fmt.sizeimage = cfg->v2.frmbuf.fmt.height * cfg->v2.frmbuf.fmt.bytesperline;
	
	return 0;	
}


static int camif_s_input(camif_cfg_t *cfg, int index)
{	
	cfg->v2.input = &fimc_inputs[index];
	
	if(cfg->v2.input->type == V4L2_INPUT_TYPE_MSDMA)
		if(cfg->dma_type & CAMIF_PREVIEW)
			cfg->input_channel = MSDMA_FROM_PREVIEW;
		else if(cfg->dma_type & CAMIF_CODEC)
		cfg->input_channel = MSDMA_FROM_CODEC;
	else
			return -EINVAL;
	else
		cfg->input_channel = CAMERA_INPUT;
	
	return 0;
}


static int camif_s_output(camif_cfg_t *cfg, int index)
{
	cfg->v2.output = &fimc_outputs[index];
	return 0;
}


static int camif_s_v4l2(camif_cfg_t *cfg)
{
	int depth, fourcc;
	int default_num = 0;	/* default */

	if ( !(cfg->v2.status&CAMIF_V4L2_INIT)) {
		
		/* To define v4l2_fmtsdesc */
		if (cfg->dma_type == CAMIF_CODEC)
			cfg->v2.fmtdesc = fimc_codec_formats;
		else
			cfg->v2.fmtdesc = fimc_preview_formats;

		fourcc = cfg->v2.fmtdesc[default_num].pixelformat;
		depth = pixfmt2depth(fourcc, NULL);
		
		camif_s_v4l2_pixfmt(cfg, depth, fourcc);

		camif_s_input(cfg, default_num);
		camif_s_output(cfg, default_num);

		/* Write the Status of v4l2 machine */
		cfg->v2.status |= CAMIF_V4L2_INIT;
	}
	
	return 0;
}


static int camif_g_fmt(camif_cfg_t *cfg, struct v4l2_format *f)
{
	int size = sizeof(struct v4l2_pix_format);

	switch (f->type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			memset(&f->fmt.pix, 0, size);
			memcpy(&f->fmt.pix, &cfg->v2.frmbuf.fmt, size);
			return 0;
#if 0		
		case V4L2_BUF_TYPE_VIDEO_OVERLAY:
			memset(&f->fmt.pix, 0, size);
			memcpy(&f->fmt.pix, &cfg->v2.frmbuf.fmt, size);
			return 0;
#endif		
		default:
			return -EINVAL;
	}
}


/* Copy v4l2 parameter into other element of camif_cfg_t */
static int camif_convert_into_camif_cfg_t(camif_cfg_t *cfg, int f)
{
	int pixfmt;
	cfg->target_x = cfg->v2.frmbuf.fmt.width;
	cfg->target_y = cfg->v2.frmbuf.fmt.height;
	pixfmt2depth(cfg->v2.frmbuf.fmt.pixelformat, &pixfmt);
	cfg->dst_fmt = pixfmt;
	//camif_setup_fimc_controller(cfg);

	return 0;
}


static int camif_s_fmt(camif_cfg_t *cfg, struct v4l2_format *f)
{
	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	{
		/* update our state informations */
		//mutex_lock(&cfg->cis->lock);
		cfg->v2.frmbuf.fmt          = f->fmt.pix;
		cfg->v2.status       |= CAMIF_v4L2_DIRTY;
		//camif_setup_fimc_controller(cfg);
		cfg->v2.status      &= ~CAMIF_v4L2_DIRTY; /* dummy ? */
		//mutex_unlock(&cfg->cis->lock);
		
		camif_convert_into_camif_cfg_t(cfg, 1);
		return 0;
	}
#if 0
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	{
		/* update our state informations */
		//mutex_lock(&cfg->cis->lock);
		cfg->v2.frmbuf.fmt          = f->fmt.pix;
		cfg->v2.status       |= CAMIF_v4L2_DIRTY;
		//camif_setup_fimc_controller(cfg);
		cfg->v2.status      &= ~CAMIF_v4L2_DIRTY; /* dummy ? */
		//mutex_unlock(&cfg->cis->lock);

		camif_convert_into_camif_cfg_t(cfg, 1);	
		return 0;
	}
#endif
	default:
		return -EINVAL;
	}

}



/* To parse the user command line */
static int camif_set_v4l2_control(camif_cfg_t *cfg, struct v4l2_control *ctrl)
{
	switch (ctrl->id) {
		case V4L2_CID_ORIGINAL:		
		case V4L2_CID_ARBITRARY:
		case V4L2_CID_NEGATIVE:
		case V4L2_CID_EMBOSSING:
		case V4L2_CID_ART_FREEZE:
		case V4L2_CID_SILHOUETTE:
			cfg->effect = ctrl->value;
			camif_change_image_effect(cfg);
			break;	
		
		case V4L2_CID_HFLIP:
			cfg->flip = CAMIF_FLIP_X;
			camif_change_flip(cfg);
			break;
			
		case V4L2_CID_VFLIP:
			cfg->flip = CAMIF_FLIP_Y;
			camif_change_flip(cfg);
			break;
			
		case V4L2_CID_ROTATE_90:
			cfg->flip = CAMIF_ROTATE_90;
			camif_change_flip(cfg);
			break;
			
		case V4L2_CID_ROTATE_180:
			cfg->flip = CAMIF_FLIP_MIRROR;
			camif_change_flip(cfg);
			break;
			
		case V4L2_CID_ROTATE_270:						
			cfg->flip = CAMIF_FLIP_ROTATE_270;
			camif_change_flip(cfg);
			break;
			
		case V4L2_CID_ROTATE_BYPASS:						
			cfg->flip = CAMIF_FLIP;
			camif_change_flip(cfg);
			break;

		case V4L2_CID_ZOOMIN:
			if(camif_make_overzoom_in_check(cfg)) {
				cfg->cis->win_hor_ofst += ZOOM_AT_A_TIME_IN_PIXELS;
				cfg->cis->win_ver_ofst += ZOOM_AT_A_TIME_IN_PIXELS;
				cfg->cis->win_hor_ofst2 += ZOOM_AT_A_TIME_IN_PIXELS;
				cfg->cis->win_ver_ofst2 += ZOOM_AT_A_TIME_IN_PIXELS;
				camif_change_zoom(cfg);
			}
			break;
			
		case V4L2_CID_ZOOMOUT:
			if(camif_make_underzoom_out_check(cfg)) {
				cfg->cis->win_hor_ofst -= ZOOM_AT_A_TIME_IN_PIXELS;
				cfg->cis->win_ver_ofst -= ZOOM_AT_A_TIME_IN_PIXELS;
				cfg->cis->win_hor_ofst2 -= ZOOM_AT_A_TIME_IN_PIXELS;
				cfg->cis->win_ver_ofst2 -= ZOOM_AT_A_TIME_IN_PIXELS;
				camif_change_zoom(cfg);
			}
			break;
			
		case V4L2_CID_CONTRAST:
		case V4L2_CID_AUTO_WHITE_BALANCE:
			camif_cis_control(cfg, SENSOR_WB, ctrl->value);
			break;
			
		default:
			printk("camif_set_v4l2_control.c : Invalid control id = %d \n", ctrl->id);
			return -1;
	}
	return 0;

}

/* Refer ioctl of  videodeX.c  and bttv-driver.c */
int camif_do_ioctl(struct inode *inode, struct file *file,unsigned int cmd, void * arg)
{
	camif_cfg_t *cfg = file->private_data;
	int ret = 0;

	switch (cmd) {
        case VIDIOC_QUERYCAP:	/* CODEC path & Preview path */
		{
			struct v4l2_capability *cap = arg;
			DPRINTK("C&P: VIDIOC_QUERYCAP \n");
                                                                                                      
			strcpy(cap->driver, "S3C FIMC Camera driver");
			strlcpy(cap->card, cfg->v->name, sizeof(cap->card));
			sprintf(cap->bus_info, "FIMC AHB Bus");
			cap->version = 0;
			cap->capabilities = cfg->v->type2;
			
			return 0;
		}

	case VIDIOC_G_FBUF:		/* Preview path ONLY */
		{	
			struct v4l2_framebuffer *fb = arg;
			DPRINTK("P: VIDIOC_G_FBUF \n");

			*fb = cfg->v2.frmbuf;
			
			fb->base = cfg->v2.frmbuf.base;
			fb->capability = V4L2_FBUF_CAP_LIST_CLIPPING;
			
			fb->fmt.pixelformat  = cfg->v2.frmbuf.fmt.pixelformat;
			fb->fmt.width = cfg->v2.frmbuf.fmt.width;
			fb->fmt.height = cfg->v2.frmbuf.fmt.height;
			fb->fmt.bytesperline = cfg->v2.frmbuf.fmt.bytesperline;

			return 0;
		}
	
	case VIDIOC_S_FBUF:		/* Preview path ONLY */
		{	
			int i, depth;
			struct v4l2_framebuffer *fb = arg;
			__u32 printformat = __cpu_to_le32(fb->fmt.pixelformat);

			DPRINTK("P: VIDIOC_S_FBUF \n");
			
			DPRINTK(
			"VIDIOC_S_FBUF - base=0x%p, size=%dx%d, bpl=%d, fmt=0x%x (%4.4s)\n",
			fb->base, fb->fmt.width, fb->fmt.height,
			fb->fmt.bytesperline, fb->fmt.pixelformat,
			(char *) &printformat);

			for (i = 0; i < NUMBER_OF_PREVIEW_FORMATS; i++)
				if (fimc_preview_formats[i].pixelformat == fb->fmt.pixelformat) {
					break;
				}
			
				
			if (i == NUMBER_OF_PREVIEW_FORMATS) {
				DPRINTK(
					"VIDIOC_S_FBUF - format=0x%x (%4.4s) not allowed\n",
					fb->fmt.pixelformat,
					(char *) &printformat);
				return -EINVAL;
			}

			cfg->v2.frmbuf.base  = fb->base;
			cfg->v2.frmbuf.flags = fb->flags;
			cfg->v2.frmbuf.capability = fb->capability;
			
			cfg->target_x = fb->fmt.width;
			cfg->target_y = fb->fmt.height;

			depth = pixfmt2depth(fb->fmt.pixelformat, &(cfg->dst_fmt));
			camif_s_v4l2_pixfmt(cfg, depth, fb->fmt.pixelformat);
			
			//mutex_lock(&cfg->cis->lock);
			#ifndef TOMTOM_INTERLACE_MODE
			ret = camif_setup_fimc_controller(cfg);
			#endif
			//mutex_unlock(&cfg->cis->lock);

			return ret;
		}
	
	case VIDIOC_G_FMT:		/* CODEC path ONLY */
		{	
			DPRINTK("C: VIDIOC_G_FMT \n");
			struct v4l2_format *f = arg;
			ret = camif_g_fmt(cfg, f);
			return ret;
		}
	
	case VIDIOC_S_FMT:		/* CODEC path ONLY */
		{	
			DPRINTK("C: VIDIOC_S_FMT \n");
			
			struct v4l2_format *f = arg;
			ret = camif_s_fmt(cfg, f);
			if(ret != 0) {
				printk("camif_s_fmt() failed !\n");
				return -EINVAL;
			}
			ret = camif_setup_fimc_controller(cfg);
			return ret;
		}

	case VIDIOC_ENUM_FMT:		/* CODEC path ONLY */
		{
			struct v4l2_fmtdesc *f = arg;
			enum v4l2_buf_type type = f->type;
			int index = f->index;

			DPRINTK("C: VIDIOC_ENUM_FMT : index = %d\n", index);
			
			if (index >= NUMBER_OF_CODEC_FORMATS) 
				return -EINVAL;
			
			switch (f->type) {
				case V4L2_BUF_TYPE_VIDEO_CAPTURE:
					break;
				case V4L2_BUF_TYPE_VIDEO_OVERLAY:
				default:
					return -EINVAL;
			}
			memset(f, 0, sizeof(*f));
			memcpy(f, cfg->v2.fmtdesc+index, sizeof(*f));
			return 0;
		}
	
	case VIDIOC_OVERLAY:		/* Preview path ONLY */
		{
			int on = *(int *)arg;
			
			DPRINTK("P: VIDIOC_OVERLAY on:%d\n", on);
			if (on != 0) {
				ret = camif_start_preview(cfg);
			} else {
				ret = camif_stop_preview(cfg);
			}
			return ret;
		}	
			
	case VIDIOC_S_CTRL:		/* Preview path ONLY */
		{
			struct v4l2_control *ctrl = arg;
			DPRINTK("P: VIDIOC_S_CTRL \n");
			ret = camif_set_v4l2_control(cfg, ctrl);
			return ret;
		}
			
	case VIDIOC_STREAMON:		/* CODEC path ONLY */
		{
			int val;
			DPRINTK("C: VIDIOC_STREAMON \n");
#ifdef TOMTOM_INTERLACE_MODE
			#ifdef CONFIG_CPU_S3C2443
			set_irq_type(IRQ_EINT0, IRQT_LOW);
			ret = request_irq(IRQ_EINT0, camif_f_irq,
				       SA_INTERRUPT, "s3c_field",
				       cfg);
			#elif 1
			s3c_gpio_cfgpin(S3C_GPN15, S3C_GPN15_OUTP);
			s3c_gpio_setpin(S3C_GPN15, 0);

			val = readl(S3C_EINTFLTCON1);
			val |= (0x1<<31);		
			val |= (0x1<<30);			
			val |= (0x1f<<24);				
			writel(val, S3C_EINTFLTCON1);	
			set_irq_type(IRQ_EINT14, IRQT_RISING);
			ret = request_irq(IRQ_EINT14, camif_f_irq,
				       SA_INTERRUPT, "s3c_field",
				       cfg);

			#else
			ret = camif_start_codec(cfg);
			#endif

			interruptible_sleep_on(&cfg->waitq);
			ret = camif_start_codec(cfg);
#else
			ret = camif_start_codec(cfg);
#endif
			return ret;
		}
			
	case VIDIOC_STREAMOFF:		/* CODEC path ONLY */
		{
			printk("C: VIDIOC_STREAMOFF \n");
			cfg->cis->status &= ~C_WORKING;
			ret = camif_stop_codec(cfg);
			return ret;
		}
		
	case VIDIOC_G_INPUT:		/* CODEC path & Preview path */
		{
			u32 *i = arg;
			DPRINTK("C&P: VIDIOC_G_INPUT \n");
			*i = cfg->v2.input->index;
			return 0;
		}
	
	case VIDIOC_S_INPUT:		/* CODEC path & Preview path */
		{
			int index = *((int *)arg);
			DPRINTK("C&P: VIDIOC_S_INPUT \n");
			
			//if (index != 0) 
			//	return -EINVAL;
			
			if (index >= NUMBER_OF_INPUTS) {
				return -EINVAL;
			}
			else {
				camif_s_input(cfg, index);
				return 0;
			}
		}
	
	case VIDIOC_G_OUTPUT:		/* CODEC path & Preview path */
		{
			u32 *i = arg;
			printk("VIDIOC_G_OUTPUT \n");
			*i = cfg->v2.output->index;
			return 0;
		}
	
	case VIDIOC_S_OUTPUT:		/* CODEC path & Preview path */
		{
			int index = *((int *)arg);
			printk("C&P: VIDIOC_S_OUTPUT \n");

/*********************************************			
			if (index != 0) 
				return -EINVAL;
**********************************************/

			if (index >= NUMBER_OF_OUTPUTS) {
				return -EINVAL;
			}
			else {
				camif_s_output(cfg, index);
				return 0;
			}
		}

	case VIDIOC_ENUMINPUT:		/* CODEC path & Preview path */
		{
			struct v4l2_input *i = arg;
			DPRINTK("C&P: VIDIOC_ENUMINPUT : index = %d\n", i->index);
			
			if ((i->index) >= NUMBER_OF_INPUTS) {
				return -EINVAL;
			}
			memcpy(i, &fimc_inputs[i->index], sizeof(struct v4l2_input));
			return 0;
		}
	
	case VIDIOC_ENUMOUTPUT:		/* CODEC path & Preview path */
		{
			struct v4l2_output *i = arg;
			DPRINTK("C&P: VIDIOC_ENUMOUTPUT : index = %d\n", i->index);
			
			if ((i->index) >= NUMBER_OF_OUTPUTS) {
				return -EINVAL;
			}
			memcpy(i, &fimc_outputs[i->index], sizeof(struct v4l2_output));
			return 0;
		}
	
	case VIDIOC_S_MSDMA: 		/* CODEC path ONLY */
		{	
			struct v4l2_msdma_format *f = arg;
			DPRINTK("C/P: VIDIOC_S_MSDMA \n");
			
			switch(f->input_path) {
				case V4L2_MSDMA_PREVIEW: 
					cfg->cis->user--;   /* CIS will be replaced with a CIS for MSDMA */
			
					cfg->cis = &msdma_input;
					cfg->cis->user++;
					cfg->input_channel = MSDMA_FROM_PREVIEW;
					break;
					
				case V4L2_MSDMA_CODEC: 
					cfg->cis->user--;   /* CIS will be replaced with a CIS for MSDMA */
			
					cfg->cis = &msdma_input;
					cfg->cis->user++;
					cfg->input_channel = MSDMA_FROM_CODEC;
					break;
					
				default:
					cfg->input_channel = CAMERA_INPUT;
					break;
			}

			cfg->cis->source_x = f->width;
			cfg->cis->source_y = f->height;
			pixfmt2depth(f->pixelformat, &cfg->src_fmt);

			cfg->cis->win_hor_ofst = 0;
			cfg->cis->win_ver_ofst = 0;
			cfg->cis->win_hor_ofst2 = 0;
			cfg->cis->win_ver_ofst2 = 0;

			ret = camif_setup_fimc_controller(cfg);
			switch(f->input_path) {
				case V4L2_MSDMA_PREVIEW:
					ret = camif_start_preview(cfg);
					break;

				case V4L2_MSDMA_CODEC:
					ret = camif_start_codec(cfg);
					break;
					
				default:
					break;	
					
			}			
			return ret;
		}
	
	case VIDIOC_MSDMA_START:	/* CODEC path ONLY */
		{
			struct v4l2_msdma_format *f = arg;
			DPRINTK("C/P: VIDIOC_MSDMA_START \n");
			if(cfg->input_channel == MSDMA_FROM_PREVIEW){
				cfg->msdma_status = 1;	//start
				#ifndef SW_IPC
				camif_preview_msdma_start(cfg);
				#else
				preview_msdma_start_flag = 1;
				#endif
			}
			#if 0
			interruptible_sleep_on(&cfg->waitq);
			#endif
			return ret;
		}
			
	case VIDIOC_MSDMA_STOP:		/* CODEC path ONLY */
		{
			struct v4l2_msdma_format *f = arg;
			DPRINTK("C/P: VIDIOC_MSDMA_STOP \n");
			cfg->cis->status &= ~C_WORKING;
			cfg->msdma_status = 0;	//stop
			#ifdef SW_IPC
			interruptible_sleep_on(&cfg->waitq);
			preview_msdma_start_flag = 0;
			#endif
			switch(f->input_path) {
				case V4L2_MSDMA_PREVIEW:
					ret = camif_stop_preview(cfg);
					break;

				case V4L2_MSDMA_CODEC:
			ret = camif_stop_codec(cfg);
					break;
					
				default:
					break;						
			}
			return ret;
		}	
	
	case VIDIOC_S_CAMERA_START:
		{
			return 0;
		}	
	
	case VIDIOC_S_CAMERA_STOP:
		{
			return 0;
		}
	case VIDIOC_S_INTERLACE_MODE:
		{
			struct v4l2_interlace_format *f = arg;
			DPRINTK("C/P: VIDIOC_S_INTERLACE_MODE \n");

		#ifdef TOMTOM_INTERLACE_MODE
			if(cfg->dma_type & CAMIF_CODEC) {
				cfg->cis->user--;   /* CIS will be replaced with a interlace_input mode  */
				cfg->cis = &interlace_input;
				cfg->cis->user++;

				cfg->cis->source_x = f->width;
				cfg->cis->source_y = f->height;
				cfg->cis->win_hor_ofst = 0;
				cfg->cis->win_ver_ofst = 0;
				cfg->cis->win_hor_ofst2 = 0;
				cfg->cis->win_ver_ofst2 = 0;

				camif_source_fmt(cfg->cis);
				camif_win_offset(cfg->cis);
			}
			
	
			cfg->interlace_capture = 1;
			cfg->check_even_odd = 0;
		#endif
		
			return 0;
		}
	default:	
		{	
			DPRINTK("C&P: v4l_compat_translate_ioctl() called \n");
			/* For v4l compatability */
			v4l_compat_translate_ioctl(inode, file, cmd, arg, camif_do_ioctl);
			break;
		}
	} /* End of Switch  */

	return ret;
}

static int camif_ioctl_v4l2(struct inode *inode, struct file *file, unsigned int cmd,
		    unsigned long arg)
{
	return video_usercopy(inode, file, cmd, arg, camif_do_ioctl);
}


/*---------------------------------------------------------------------------------------*/


int camif_p_mmap(struct file* filp, struct vm_area_struct *vma) {

	camif_cfg_t *cfg = filp->private_data;
	
	unsigned long pageFrameNo, size;
	
	size = vma->vm_end - vma->vm_start;

	// page frame number of the address for a source RGB frame to be stored at. 
	pageFrameNo = __phys_to_pfn(cfg->pp_phys_buf);
	
	if(size > RGB_MEM) {
		printk("The size of RGB_MEM mapping is too big!\n");
		return -EINVAL;
	}
	
	if((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED)) {
		printk("Writable RGB_MEM mapping must be shared !\n");
		return -EINVAL;
	}
	
	if(remap_pfn_range(vma, vma->vm_start, pageFrameNo, size, vma->vm_page_prot))
		return -EINVAL;
	
	return 0;
}


int camif_c_mmap(struct file* filp, struct vm_area_struct *vma) {

	camif_cfg_t *cfg = filp->private_data;
	
	unsigned long pageFrameNo, size;
	
	size = vma->vm_end - vma->vm_start;

	// page frame number of the address for a source YCbCr frame to be stored at. 
	pageFrameNo = __phys_to_pfn(cfg->pp_phys_buf);
	
	if(size > YUV_MEM) {
		printk("The size of YUV_MEM mapping is too big!\n");
		return -EINVAL;
	}
	
	if((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED)) {
		printk("Writable YUV_MEM mapping must be shared !\n");
		return -EINVAL;
	}
	
	if(remap_pfn_range(vma, vma->vm_start, pageFrameNo, size, vma->vm_page_prot))
		return -EINVAL;
	
	return 0;
}




 struct file_operations camif_c_fops = {
	.owner = THIS_MODULE,
	.open = camif_open,
	.release = camif_release,

#if S3C_V4L2_SUPPORT
	.ioctl = camif_ioctl_v4l2,
#else
	.ioctl = camif_ioctl,
#endif
	.read = camif_c_read,
	.write = camif_write,
	.mmap = camif_c_mmap,
};

 struct file_operations camif_p_fops = {
	.owner = THIS_MODULE,
	.open = camif_open,
	.release = camif_release,

#if S3C_V4L2_SUPPORT
	.ioctl = camif_ioctl_v4l2,
#else
	.ioctl = camif_ioctl,
#endif
	.read = camif_p_read,
	.write = camif_write,
	.mmap = camif_p_mmap,
};

void camif_vdev_release (struct video_device *vdev) {
	kfree(vdev);
}

 struct video_device codec_template = {
	.owner = THIS_MODULE,
	.name = "CODEC_CAMIF",
	.type = VID_TYPE_OVERLAY | VID_TYPE_CAPTURE | VID_TYPE_CLIPPING | VID_TYPE_SCALES,
	.type2 = V4L2_CAP_VIDEO_OVERLAY|V4L2_CAP_VIDEO_CAPTURE,		/* V4L2 */
	.fops = &camif_c_fops,
	.release  = camif_vdev_release,
	.minor = CODEC_MINOR,
};

 struct video_device preview_template = {
	.owner = THIS_MODULE,
	.name = "PREVIEW_CAMIF",
	.type = VID_TYPE_OVERLAY | VID_TYPE_CAPTURE | VID_TYPE_CLIPPING | VID_TYPE_SCALES,
	.type2 = V4L2_CAP_VIDEO_OVERLAY|V4L2_CAP_VIDEO_CAPTURE,		/* V4L2 */
	.fops = &camif_p_fops,
	.release  = camif_vdev_release,
	.minor = PREVIEW_MINOR,
};

static int preview_init(camif_cfg_t * cfg)
{
	char name[16] = "CAMIF_PREVIEW";
	
	preview_template.fops = &camif_p_fops;	//bxl@hhtech
	memset(cfg, 0, sizeof(camif_cfg_t));
	
	// Default value : These target-x,y values will be changed 
	cfg->target_x = 320;//640;
	cfg->target_y = 240;//480;
	
#ifdef TOMTOM_INTERLACE_MODE
	cfg->pp_num = 1;
#else
	cfg->pp_num = 4;
#endif
	cfg->dma_type = CAMIF_PREVIEW;

	/* Default INPUT type & channel */
	cfg->input_channel = CAMERA_INPUT;
	cfg->src_fmt = CAMIF_YCBCR422;

	/* Default OUTPUT type & channel */
	cfg->output_channel = CAMIF_OUT_PP;
	cfg->dst_fmt = CAMIF_RGB16;
	
	cfg->flip = CAMIF_FLIP_Y;
	cfg->v = &preview_template;
	init_MUTEX(&cfg->v->lock);
#if defined(CONFIG_CPU_S3C2443)
	cfg->irq = IRQ_CAM_P;
#elif (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
	cfg->irq = IRQ_CAMIF_P;
#endif
	strcpy(cfg->shortname, name);
	init_waitqueue_head(&cfg->waitq);
	cfg->status = CAMIF_STOPPED;
	
	/* To get the handle of CODEC */
	cfg->other = get_camif(PREVIEW_MINOR);
	
	return cfg->status;
}

static int codec_init(camif_cfg_t * cfg)
{
	char name[16] = "CAMIF_CODEC";
	
	codec_template.fops = &camif_c_fops;
	memset(cfg, 0, sizeof(camif_cfg_t));
	
	// Default value : These target-x,y values will be changed 
	cfg->target_x = 640;
	cfg->target_y = 486;
#ifdef TOMTOM_INTERLACE_MODE
	cfg->pp_num = 4;
#else
	cfg->pp_num = 4;
#endif
	cfg->dma_type = CAMIF_CODEC;

	/* Default INPUT type & channel */
	cfg->src_fmt = CAMIF_YCBCR422;
	cfg->input_channel = CAMERA_INPUT;

	/* Default OUTPUT type & channel */
	cfg->dst_fmt = CAMIF_YCBCR420;
	cfg->output_channel = CAMIF_OUT_PP;
	
	cfg->flip = CAMIF_FLIP_X;
	cfg->v = &codec_template;
	init_MUTEX(&cfg->v->lock);
	
#if defined(CONFIG_CPU_S3C2443)
	cfg->irq = IRQ_CAM_C;
#elif (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
	cfg->irq = IRQ_CAMIF_C;
#endif	
	strcpy(cfg->shortname, name);
	init_waitqueue_head(&cfg->waitq);
	
	cfg->status = CAMIF_STOPPED;

	/* To get the handle of PREVIEW */
	cfg->other = get_camif(CODEC_MINOR);
	
	return cfg->status;
}

static void camif_init(void)
{
	camif_setup_sensor();
}


static void print_version(void)
{

	printk(KERN_INFO "FIMC built:" __DATE__ " " __TIME__
	       "\n%s\n%s\n%s\n", camif_version, driver_version, fsm_version);
}


static int camif_m_in(void)
{
	int ret = 0;
	camif_cfg_t *cfg;

	camif_init();
	cfg = get_camif(CODEC_MINOR);
	codec_init(cfg);
	if (video_register_device(cfg->v, VFL_TYPE_GRABBER, CODEC_MINOR) != 0) {
		DPRINTK("s3c_camera_driver.c : Couldn't register this codec driver.\n");
		return 0;
	}
	
	cfg = get_camif(PREVIEW_MINOR);
	preview_init(cfg);
	if (video_register_device(cfg->v, VFL_TYPE_GRABBER, PREVIEW_MINOR) != 0) {
		DPRINTK("s3c_camera_driver.c : Couldn't register this preview driver.\n");
		return 0;
	}
#ifdef CONFIG_CPU_S3C2443 
	s3c_gpio_cfgpin(S3C_GPF7, S3C_GPF7_OUTP);
	s3c_gpio_setpin(S3C_GPF7, 0);
#endif

	print_version();
	return ret;
}

static void unconfig_device(camif_cfg_t * cfg)
{
	video_unregister_device(cfg->v);
	camif_hw_close(cfg);
	memset(cfg, 0, sizeof(camif_cfg_t));
}

static void camif_m_out(void)
{				/* module out */
	camif_cfg_t *cfg;

	cfg = get_camif(CODEC_MINOR);
	unconfig_device(cfg);
	
	cfg = get_camif(PREVIEW_MINOR);
	unconfig_device(cfg);
	return;
}

void camif_register_cis(struct i2c_client *ptr)
{
	camif_cfg_t *cfg;

	cfg = get_camif(CODEC_MINOR);
	cfg->cis = (camif_cis_t *) (ptr->data);

	cfg = get_camif(PREVIEW_MINOR);
	cfg->cis = (camif_cis_t *) (ptr->data);

	sema_init(&cfg->cis->lock, 1);		/* global lock for both Codec and Preview */
	cfg->cis->status |= P_NOT_WORKING;	/* Default Value */
	camif_hw_open(cfg->cis);
}

void camif_unregister_cis(struct i2c_client *ptr)
{
	camif_cis_t *cis;

	cis = (camif_cis_t *) (ptr->data);
	cis->init_sensor = 0;	/* need to modify */
}

module_init(camif_m_in);
module_exit(camif_m_out);

EXPORT_SYMBOL(camif_register_cis);
EXPORT_SYMBOL(camif_unregister_cis);

MODULE_AUTHOR("Yeom, Youngran <yeom@samsung.com>");
MODULE_DESCRIPTION("s3c_camera_driver for FIMC3.X Camera interface");
MODULE_LICENSE("GPL");

