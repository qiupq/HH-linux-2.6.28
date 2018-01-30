/*
 *   Copyright (C) 2004 Samsung Electronics 
 *   
 * This file is subject to the terms and conditions of the GNU General Public
 * License 2. See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <asm/io.h>
#include <asm/semaphore.h>
#include <asm/hardware.h>
#include <asm/uaccess.h>

#include <asm/arch/map.h>
#include <asm/arch/regs-camif.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-gpioj.h>
#include <asm/arch/regs-lcd.h>
#include <asm/arch/regs-s3c6410-clock.h>

#include "s3c_camif.h"
#include <linux/videodev.h>

//#define CAMIF_DEBUG

#if (defined (CONFIG_VIDEO_ADV7180)/* || defined (CONFIG_VIDEO_SAA7113)*/)
#define TOMTOM_INTERLACE_MODE
#define SW_IPC
#endif

static dma_addr_t shared_buffer_addr; 

static int camif_dma_burst(camif_cfg_t *);
static int camif_setup_scaler(camif_cfg_t *);
static int camif_setup_intput_path(camif_cfg_t *);
static int camif_setup_msdma_input(camif_cfg_t *);
static int camif_setup_camera_input(camif_cfg_t *);
static int camif_setup_output_path(camif_cfg_t *);
static int camif_setup_lcd_fifo_output(camif_cfg_t *);
static int camif_setup_memory_output(camif_cfg_t *);


const char *camif_version =
        "$Id: s3c_camif.c,v 1.5 2007/10/11 07:49:25 yreom Exp $";

#define CAMDIV_val     20

int s3c_camif_set_clock (unsigned int camclk)
{
	unsigned int camclk_div, val, hclkcon;
	struct clk *src_clk = clk_get(NULL, "hclkx2");

	printk(KERN_INFO "External camera clock is set to %dHz\n", camclk);

	camclk_div = clk_get_rate(src_clk) / camclk;
	printk("Parent clk = %ld, CAMDIV = %d\n", clk_get_rate(src_clk), camclk_div);

	// CAMIF HCLK Enable
	hclkcon = __raw_readl(S3C_HCLK_GATE);
	hclkcon |= S3C_CLKCON_HCLK_CAMIF;
	__raw_writel(hclkcon, S3C_HCLK_GATE);

	/* CAMCLK Enable */
	val = readl(S3C_SCLK_GATE);
	val |= S3C_CLKCON_SCLK_CAM;
	writel(val, S3C_SCLK_GATE);

	val = readl(S3C_CLK_DIV0);
	val &= ~(0xf<<CAMDIV_val);
	writel(val, S3C_CLK_DIV0);
	
	if(camclk_div > 16) {
	    printk("Can't set to %dHZ, set to %dHZ instead!!!\n", 
			camclk, clk_get_rate(src_clk)/16);
	    camclk_div = 16;
	}
	/* CAM CLK DIVider Ratio = (EPLL clk)/(camclk_div) */
	val |= ((camclk_div - 1) << CAMDIV_val);
	writel(val, S3C_CLK_DIV0);
	val = readl(S3C_CLK_DIV0);

	return 0;
}

void s3c_camif_disable_clock (void)
{
	unsigned int val;

	val = readl(S3C_SCLK_GATE);
	val &= ~S3C_CLKCON_SCLK_CAM;
	writel(val, S3C_SCLK_GATE);
}

static int camif_malloc(camif_cfg_t *cfg)
{
	unsigned int t_size = 0;
	unsigned int area = 0;
	

	if(cfg->interlace_capture)
		area = cfg->target_x * cfg->target_y * 2;
	else
		area = cfg->target_x * cfg->target_y;

	if(cfg->dma_type & CAMIF_CODEC) {
		if (cfg->dst_fmt & CAMIF_YCBCR420) 
			t_size = area * 3 / 2;  /* CAMIF_YCBCR420 */
		
		else if (cfg->dst_fmt & CAMIF_YCBCR422) 
			t_size = area * 2;     /* CAMIF_YCBCR422 */ 
		
		else if (cfg->dst_fmt & CAMIF_RGB16) 
			t_size = area * 2;     /*  2 bytes per one pixel*/
		
		else if (cfg->dst_fmt & CAMIF_RGB24) 
			t_size = area * 4;     /* 4 bytes per one pixel */
		
		else 
			printk(KERN_INFO "CODEC: Invalid format !! \n");
		
		
		t_size = t_size * cfg->pp_num;
		printk(KERN_INFO "CODEC: Memory required : 0x%08X bytes\n",t_size);
		
#ifndef C_DEDICATED_MEM
		cfg->pp_virt_buf = consistent_alloc(GFP_KERNEL, t_size, &cfg->pp_phys_buf);
	//	memset(cfg->pp_virt_buf, 0, t_size);
#else
		printk(KERN_INFO "CODEC: Using Dedicated High RAM\n");
		cfg->pp_phys_buf = PHYS_OFFSET + (MEM_SIZE - RESERVE_MEM);
		cfg->pp_virt_buf = ioremap_nocache(cfg->pp_phys_buf, YUV_MEM);
		shared_buffer_addr = cfg->pp_virt_buf;
#endif
		if ( !cfg->pp_virt_buf ) {
			printk(KERN_ERR "s3c_camif.c : Failed to request YCbCr: size of memory %08x \n",t_size);
			return -ENOMEM;
		}
		cfg->pp_totalsize = t_size;
		return 0;
	}
	if ( cfg->dma_type & CAMIF_PREVIEW ) {
	
		if (cfg->dst_fmt & CAMIF_RGB16) {
			t_size = area * 2;      /*  2 bytes per two pixel*/
		}
		else if (cfg->dst_fmt & CAMIF_RGB24) {
			t_size = area * 4;      /* 4 bytes per one pixel */
		}
		else {
			printk(KERN_ERR "Error[s3c_camif.c]: Invalid format !\n");
		}
			
		t_size = t_size * cfg->pp_num;
		printk(KERN_INFO "Preview: Memory required : 0x%08X bytes\n",t_size);
		
#ifndef P_DEDICATED_MEM
		cfg->pp_virt_buf = consistent_alloc(GFP_KERNEL, t_size, &cfg->pp_phys_buf);
		//memset(cfg->pp_virt_buf, 0, t_size);
#else
		printk(KERN_INFO "Preview: Using Dedicated High RAM\n");

		if(cfg->interlace_capture == 1) {
			cfg->pp_phys_buf = PHYS_OFFSET + (MEM_SIZE - RESERVE_MEM);
			cfg->pp_virt_buf = shared_buffer_addr;
		} else {
		cfg->pp_phys_buf = PHYS_OFFSET + (MEM_SIZE - RESERVE_MEM ) + YUV_MEM;
		cfg->pp_virt_buf = ioremap_nocache(cfg->pp_phys_buf, RGB_MEM);
		}
		
#endif
		if ( !cfg->pp_virt_buf ) { 
			printk(KERN_ERR "s3c_camif.c: Failed to request RGB: size of memory %08x\n",t_size);
			return -ENOMEM;
		}
		cfg->pp_totalsize = t_size;
		return 0;
	}

	return 0;		/* Never come. */
}


static int camif_demalloc(camif_cfg_t *cfg)
{
#if defined(P_DEDICATED_MEM) || defined(C_DEDICATED_MEM)
	iounmap(cfg->pp_virt_buf);
	cfg->pp_virt_buf = 0;
#else
	if ( cfg->pp_virt_buf ) {
		consistent_free(cfg->pp_virt_buf,cfg->pp_totalsize,cfg->pp_phys_buf);
		cfg->pp_virt_buf = 0;
	}
#endif
	return 0;
}

/* 
 * advise a person to use this func in ISR 
 * index value indicates the next frame count to be used 
 */
int camif_g_frame_num(camif_cfg_t *cfg)
{
	int index = 0;
	int value = 0; 

	if (cfg->dma_type & CAMIF_CODEC ) {
		value = readl(S3C_CICOSTATUS); 
		index = (value>>26) & 0x03;
//		DPRINTK("CAMIF_CODEC frame %d \n", index);
	}
	else {
		assert(cfg->dma_type & CAMIF_PREVIEW );
		
#if defined(CONFIG_CPU_S3C2412)
		value = readl(S3C_CICOSTATUS); 
		index = (value>>26) & 0x03;
		DPRINTK("CAMIF_PREVIEW frame %d  0x%08X \n", index, readl(S3C_CICOSTATUS));
#else
		value = readl(S3C_CIPRSTATUS); 
		index = (value>>26) & 0x03;
//		DPRINTK("CAMIF_PREVIEW frame %d  0x%08X \n", index, readl(S3C_CIPRSTATUS));
#endif
	}
	
	cfg->now_frame_num = (index + 2) % 4;    /* When 4 PingPong */
	return index; /* meaningless */
}

static int camif_pp_codec_rgb(camif_cfg_t *cfg);
static int camif_pp_preview_msdma(camif_cfg_t *cfg);

static int camif_pp_codec(camif_cfg_t *cfg)
{
	u32 i, cbcr_size = 0;    /* Cb,Cr size */
	u32 one_p_size;
	u32 one_line_size;
	u32 area = 0;

	if(cfg->interlace_capture)
		area = cfg->target_x * cfg->target_y * 2;
	else
		area = cfg->target_x * cfg->target_y;
	
	if (cfg->dst_fmt & CAMIF_YCBCR420) {
		cbcr_size = area /4;
	}
	else if(cfg->dst_fmt & CAMIF_YCBCR422){
		cbcr_size = area /2;
	}
	else if((cfg->dst_fmt & CAMIF_RGB16) || (cfg->dst_fmt & CAMIF_RGB24)){ 
		camif_pp_codec_rgb(cfg);
		return 0;
	}
	else {
		printk("s3c_camif.c : Invalid format No - %d \n", cfg->dst_fmt);	
	}  
	
	switch ( cfg->pp_num ) {
		case 1 :
			for ( i =0 ; i < 4; i=i+1) {
				cfg->img_buf[i].virt_y = cfg->pp_virt_buf;
				cfg->img_buf[i].phys_y = cfg->pp_phys_buf;
				cfg->img_buf[i].virt_cb = cfg->pp_virt_buf + area; 
				cfg->img_buf[i].phys_cb = cfg->pp_phys_buf + area;
				cfg->img_buf[i].virt_cr = cfg->pp_virt_buf + area + cbcr_size;
				cfg->img_buf[i].phys_cr = cfg->pp_phys_buf + area + cbcr_size;
				writel(cfg->img_buf[i].phys_y, S3C_CICOYSA(i));
				writel(cfg->img_buf[i].phys_cb, S3C_CICOCBSA(i));
				writel(cfg->img_buf[i].phys_cr, S3C_CICOCRSA(i));
			}
			break;
			
		case 2:
#define  TRY   (( i%2 ) ? 1 :0)
			if(cfg->interlace_capture)
				one_p_size = cfg->target_x * 2;
			else
			one_p_size = area + 2*cbcr_size;
			for (i = 0; i < 4  ; i++) {
				cfg->img_buf[i].virt_y = cfg->pp_virt_buf + TRY * one_p_size;
				cfg->img_buf[i].phys_y = cfg->pp_phys_buf + TRY * one_p_size;
				cfg->img_buf[i].virt_cb = cfg->pp_virt_buf + area + TRY * one_p_size;
				cfg->img_buf[i].phys_cb = cfg->pp_phys_buf + area + TRY * one_p_size;
				cfg->img_buf[i].virt_cr = cfg->pp_virt_buf + area + cbcr_size + TRY * one_p_size;
				cfg->img_buf[i].phys_cr = cfg->pp_phys_buf + area + cbcr_size + TRY * one_p_size;
				writel(cfg->img_buf[i].phys_y, S3C_CICOYSA(i));
				writel(cfg->img_buf[i].phys_cb, S3C_CICOCBSA(i));
				writel(cfg->img_buf[i].phys_cr, S3C_CICOCRSA(i));
			}
			break;
			
		case 4: 
			if(cfg->interlace_capture) {
				one_line_size = cfg->target_x * 2;
				one_p_size = area + 2*cbcr_size;
				for (i = 0; i < 4 ; i++) {
					cfg->img_buf[i].virt_y = cfg->pp_virt_buf + (i/2) * one_p_size + (i%2) * one_line_size;
					cfg->img_buf[i].phys_y = cfg->pp_phys_buf + (i/2) * one_p_size + (i%2) * one_line_size;
					cfg->img_buf[i].virt_cb = cfg->pp_virt_buf + area + (i/2) * one_p_size + (i%2) * one_line_size;
					cfg->img_buf[i].phys_cb = cfg->pp_phys_buf + area + (i/2) * one_p_size + (i%2) * one_line_size;
					cfg->img_buf[i].virt_cr = cfg->pp_virt_buf + area + cbcr_size + (i/2) * one_p_size + (i%2) * one_line_size;
					cfg->img_buf[i].phys_cr = cfg->pp_phys_buf + area + cbcr_size + (i/2) * one_p_size + (i%2) * one_line_size;
					writel(cfg->img_buf[i].phys_y, S3C_CICOYSA(i));
					writel(cfg->img_buf[i].phys_cb, S3C_CICOCBSA(i));
					writel(cfg->img_buf[i].phys_cr, S3C_CICOCRSA(i));
				}
			} else {
			one_p_size = area + 2*cbcr_size;
			for (i = 0; i < 4 ; i++) {
				cfg->img_buf[i].virt_y = cfg->pp_virt_buf + i * one_p_size;
				cfg->img_buf[i].phys_y = cfg->pp_phys_buf + i * one_p_size;
				cfg->img_buf[i].virt_cb = cfg->pp_virt_buf + area + i * one_p_size;
				cfg->img_buf[i].phys_cb = cfg->pp_phys_buf + area + i * one_p_size;
				cfg->img_buf[i].virt_cr = cfg->pp_virt_buf + area + cbcr_size + i * one_p_size;
				cfg->img_buf[i].phys_cr = cfg->pp_phys_buf + area + cbcr_size + i * one_p_size;
				writel(cfg->img_buf[i].phys_y, S3C_CICOYSA(i));
				writel(cfg->img_buf[i].phys_cb, S3C_CICOCBSA(i));
				writel(cfg->img_buf[i].phys_cr, S3C_CICOCRSA(i));
			}
			}
			break;
			
		default:
			printk("s3c_camif.c : Invalid PingPong Number %d \n", cfg->pp_num);
	}
	return 0;
}

/* RGB Buffer Allocation */
static int camif_pp_preview(camif_cfg_t *cfg)
{
	int i;
	u32 cbcr_size = 0;    /* Cb,Cr size */
	u32 area = cfg->target_x * cfg->target_y;

#if defined(CONFIG_CPU_S3C2443) || defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410)
	if(cfg->input_channel) {
		camif_pp_preview_msdma(cfg);
		return 0;
	}
#endif
	if (cfg->dst_fmt & CAMIF_YCBCR420) {
		cbcr_size = area /4;
		/* To Do */
	}
	else if(cfg->dst_fmt & CAMIF_YCBCR422){
		cbcr_size = area /2;
		/* To Do */
	}
	else if(cfg->dst_fmt & CAMIF_RGB24)  {
		area = area * 4 ;
	}
	else if (cfg->dst_fmt & CAMIF_RGB16) {
		area = area * 2;
	}  
	else {
		printk("s3c_camif.c : Invalid format No - %d \n", cfg->dst_fmt);	
	}  
		
	switch ( cfg->pp_num ) {
		case 1:
			for ( i = 0; i < 4 ; i++ ) {
				cfg->img_buf[i].virt_rgb = cfg->pp_virt_buf ;
				cfg->img_buf[i].phys_rgb = cfg->pp_phys_buf ;
#if defined(CONFIG_CPU_S3C2412)
				writel(cfg->img_buf[i].phys_rgb, S3C_CICOYSA(i));
#elif defined(CONFIG_CPU_S3C2443)
				writel(cfg->img_buf[i].phys_rgb, S3C_CIPRCLRSA(i));
#elif (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
				writel(cfg->img_buf[i].phys_rgb, S3C_CIPRYSA(i));
#endif
			}
			break;
		case 2:
			for ( i = 0; i < 4 ; i++) {
				cfg->img_buf[i].virt_rgb = cfg->pp_virt_buf + TRY * area;
				cfg->img_buf[i].phys_rgb = cfg->pp_phys_buf + TRY * area;
#if defined(CONFIG_CPU_S3C2412)
				writel(cfg->img_buf[i].phys_rgb, S3C_CICOYSA(i));
#elif defined(CONFIG_CPU_S3C2443)
				writel(cfg->img_buf[i].phys_rgb, S3C_CIPRCLRSA(i));
#elif (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
				writel(cfg->img_buf[i].phys_rgb, S3C_CIPRYSA(i));
#endif
			}
			break;
		case 4:
			for ( i = 0; i < 4 ; i++) {
				cfg->img_buf[i].virt_rgb = cfg->pp_virt_buf + i * area;
				cfg->img_buf[i].phys_rgb = cfg->pp_phys_buf + i * area;
#if defined(CONFIG_CPU_S3C2412)
				writel(cfg->img_buf[i].phys_rgb, S3C_CICOYSA(i));
#elif defined(CONFIG_CPU_S3C2443)
				writel(cfg->img_buf[i].phys_rgb, S3C_CIPRCLRSA(i));
#elif (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
				writel(cfg->img_buf[i].phys_rgb, S3C_CIPRYSA(i));
#endif
			}
			break;
		default:
			printk("s3c_camif.c : Invalid PingPong Number %d \n", cfg->pp_num);
	}
	return 0;
}

/* RGB Buffer Allocation For CODEC path */
static int camif_pp_codec_rgb(camif_cfg_t *cfg)
{
	int i;
	u32 val;
	u32 area = cfg->target_x * cfg->target_y;

	if(cfg->dst_fmt & CAMIF_RGB24)  
		area = area * 4 ;
	else {
		assert (cfg->dst_fmt & CAMIF_RGB16);
		area = area * 2;
	}  

	if(cfg->input_channel == MSDMA_FROM_CODEC){  // Use MSDMA
		val = readl(S3C_VIDW00ADD0B0);
		for ( i = 0; i < 4 ; i++ ) {
			writel(val, S3C_CICOYSA(i));
		}
	}
	else{ 					// Do NOT use MSDMA 
		switch ( cfg->pp_num ) {
			case 1:
				for ( i = 0; i < 4 ; i++ ) {
					cfg->img_buf[i].virt_rgb = cfg->pp_virt_buf ;
					cfg->img_buf[i].phys_rgb = cfg->pp_phys_buf ;
					writel(cfg->img_buf[i].phys_rgb, S3C_CICOYSA(i));
				}
				break;
			case 2:
				for ( i = 0; i < 4 ; i++) {
					cfg->img_buf[i].virt_rgb = cfg->pp_virt_buf + TRY * area;
					cfg->img_buf[i].phys_rgb = cfg->pp_phys_buf + TRY * area;
					writel(cfg->img_buf[i].phys_rgb, S3C_CICOYSA(i));
				}
				break;
			case 4:
				for ( i = 0; i < 4 ; i++) {
					cfg->img_buf[i].virt_rgb = cfg->pp_virt_buf + i * area;
					cfg->img_buf[i].phys_rgb = cfg->pp_phys_buf + i * area;
					writel(cfg->img_buf[i].phys_rgb, S3C_CICOYSA(i));
				}
				break;
			default:
				printk("s3c_camif.c : Invalid PingPong Number %d \n",cfg->pp_num);
				panic(" s3c_camif.c : Halt !\n");
		}
	}
	return 0;
}

#if defined(CONFIG_CPU_S3C2443) || (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
/* RGB Buffer Allocation into frame buffer */
static int camif_pp_preview_msdma(camif_cfg_t *cfg)
{
	int i;
	
	u32 cbcr_size = 0;    /* Cb+Cr size */
	u32 one_p_size, val;
	
	u32 area = cfg->cis->source_x * cfg->cis->source_y;

	#if defined(CONFIG_CPU_S3C2443)
	val = readl(S3C_VIDW01ADD0);
	#elif (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
	val = readl(S3C_VIDW01ADD0B0);
	#endif
	
	if(!((cfg->dst_fmt & CAMIF_RGB16) || (cfg->dst_fmt & CAMIF_RGB24)))
		printk("camif_pp_preview_msdma() - Invalid Format\n");
	
	for ( i = 0; i < 4 ; i++ ) {
		#if defined(CONFIG_CPU_S3C2443)
		writel(val, S3C_CIPRCLRSA(i));
		#elif (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
		writel(val, S3C_CIPRYSA(i));
		#endif
	}
	
	if (cfg->src_fmt & CAMIF_YCBCR420) {
		cbcr_size = area/4;
		cfg->img_buf[0].virt_cb = cfg->pp_virt_buf + area; 
		cfg->img_buf[0].phys_cb = cfg->pp_phys_buf + area;
		cfg->img_buf[0].virt_cr = cfg->pp_virt_buf + area + cbcr_size;
		cfg->img_buf[0].phys_cr = cfg->pp_phys_buf + area + cbcr_size;

	}
	else if(cfg->src_fmt & CAMIF_YCBCR422){
		area = area * 2;
		cfg->img_buf[0].virt_cb = 0; 
		cfg->img_buf[0].phys_cb = 0;
		cfg->img_buf[0].virt_cr = 0;
		cfg->img_buf[0].phys_cr = 0;
		
	} 
	cfg->img_buf[0].virt_y = cfg->pp_virt_buf;
	cfg->img_buf[0].phys_y = cfg->pp_phys_buf;

#if defined(CONFIG_CPU_S3C2443) 
	writel(cfg->img_buf[0].phys_y, S3C_CIMSYSA);
	writel(cfg->img_buf[0].phys_y+area, S3C_CIMSYEND);
	
	writel(cfg->img_buf[0].phys_cb, S3C_CIMSCBSA);
	writel(cfg->img_buf[0].phys_cb+cbcr_size, S3C_CIMSCBEND);
	
	writel(cfg->img_buf[0].phys_cr, S3C_CIMSCRSA);
	writel(cfg->img_buf[0].phys_cr+cbcr_size, S3C_CIMSCREND);

	writel(cfg->cis->source_x, S3C_CIMSWIDTH);
	
#elif  (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
	writel(cfg->img_buf[0].phys_y, S3C_MSPRY0SA);
	writel(cfg->img_buf[0].phys_y+area, S3C_MSPRY0END);
	
	writel(cfg->img_buf[0].phys_cb, S3C_MSPRCB0SA);
	writel(cfg->img_buf[0].phys_cb+cbcr_size, S3C_MSPRCB0END);
	
	writel(cfg->img_buf[0].phys_cr, S3C_MSPRCR0SA);
	writel(cfg->img_buf[0].phys_cr+cbcr_size, S3C_MSPRCR0END);

	/* MSDMA for codec source image width */
	val = readl(S3C_MSCOWIDTH);
#ifdef SW_IPC
	val = (val & ~(0x1<<31));	// AutoLoadDisable
#else
	val = (val & ~(0x1<<31))|(0x1<<31);	// AutoLoadEnable
#endif
	val |= (cfg->cis->source_y<<16);	// MSPRHEIGHT
	val |= (cfg->cis->source_x<<0);		// MSPRWIDTH
	writel(val, S3C_MSPRWIDTH);
#endif

	return 0;
}
#endif

static int camif_setup_memory_output(camif_cfg_t *cfg)
{
	if (cfg->dma_type & CAMIF_CODEC ) {
		camif_pp_codec(cfg);
	}

	if ( cfg->dma_type & CAMIF_PREVIEW) {
		camif_pp_preview(cfg);
	}
	return 0;
}


static int camif_setup_lcd_fifo_output(camif_cfg_t *cfg) 
{
	/* To Be Implemented */
	return 0;
}



/* 
 * After calling camif_g_frame_num,
 * this func must be called 
 */
u8 * camif_g_frame(camif_cfg_t *cfg)
{
	u8 * ret = NULL;
	int cnt = cfg->now_frame_num;

	if(cfg->dma_type & CAMIF_PREVIEW) {
		ret = cfg->img_buf[cnt].virt_rgb;
	}
	if (cfg->dma_type & CAMIF_CODEC) {
		if ((cfg->dst_fmt & CAMIF_RGB16) || (cfg->dst_fmt & CAMIF_RGB24))
			ret = cfg->img_buf[cnt].virt_rgb;
		else
			ret = cfg->img_buf[cnt].virt_y;
	}
	return ret;
}


/* This function must be called in module initial time */
int camif_source_fmt(camif_cis_t *cis) 
{
#ifdef TOMTOM_INTERLACE_MODE
	u32 cmd = readl(S3C_CISRCFMT);

	/* Configure CISRCFMT --Source Format */
	cmd &=~ CAMIF_ITU601;
	if (cis->itu_fmt & CAMIF_ITU601) {
		cmd = CAMIF_ITU601;
	}
	else {
		assert ( cis->itu_fmt & CAMIF_ITU656);
		cmd = CAMIF_ITU656;
	}
	cmd &=~ (0x1fff<<16 | 0x1fff);
	cmd  |= S3C_CISRCFMT_SOURCEHSIZE(cis->source_x)| S3C_CISRCFMT_SOURCEVSIZE(cis->source_y);
	
	/* Order422 */
	cmd &=~ (0x3<<14);
	cmd |=  cis->order422;
	writel(cmd, S3C_CISRCFMT);
#else
	u32 cmd = 0;

	/* Configure CISRCFMT --Source Format */
	if (cis->itu_fmt & CAMIF_ITU601) {
		cmd = CAMIF_ITU601;
	}
	else {
		assert ( cis->itu_fmt & CAMIF_ITU656);
		cmd = CAMIF_ITU656;
	}
	cmd  |= S3C_CISRCFMT_SOURCEHSIZE(cis->source_x)| S3C_CISRCFMT_SOURCEVSIZE(cis->source_y);
	
	/* Order422 */
	cmd |=  cis->order422;
	writel(cmd, S3C_CISRCFMT);
#endif
	return 0 ;
}
   

/* 
 * Codec Input YCBCR422 will be Fixed
 * cfg->flip removed because cfg->flip will be set in camif_change_flip func. 
 */
static int camif_target_fmt(camif_cfg_t *cfg)
{
	u32 cmd = 0;

	if (cfg->dma_type & CAMIF_CODEC) {
		cmd |= S3C_CICOTRGFMT_TARGETHSIZE_CO(cfg->target_x)| S3C_CICOTRGFMT_TARGETVSIZE_CO(cfg->target_y);
		
		/* YCBCR setting */  
		if ( cfg->dst_fmt & CAMIF_YCBCR420 ) {
			#if (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
			cmd |= S3C_CICOTRGFMT_OUTFORMAT_YCBCR420OUT;
			#else
			cmd |= S3C_CICOTRGFMT_OUT422_420|S3C_CICOTRGFMT_IN422_422;
			#endif
			writel(cmd, S3C_CICOTRGFMT);		//Interleadve Off 
		}
		else if(cfg->dst_fmt & CAMIF_YCBCR422) {
			#if (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
			if(cfg->interlace_capture)
				cmd |= S3C_CICOTRGFMT_OUTFORMAT_YCBCR422OUTINTERLEAVE;
			else
				cmd |= S3C_CICOTRGFMT_OUTFORMAT_YCBCR422OUT;
			#else
			if(cfg->interlace_capture)
				cmd |= S3C_CICOTRGFMT_OUT422_422|S3C_CICOTRGFMT_IN422_422|(1<<29);
			else
				cmd |= S3C_CICOTRGFMT_OUT422_422|S3C_CICOTRGFMT_IN422_422;
			#endif
			writel(cmd, S3C_CICOTRGFMT);		 
		}
		else if((cfg->dst_fmt & CAMIF_RGB24) ||(cfg->dst_fmt & CAMIF_RGB16)) {
			#if (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
			cmd |= S3C_CICOTRGFMT_OUTFORMAT_RGBOUT;
			writel(cmd, S3C_CICOTRGFMT);	
			#else
			cmd |= S3C_CICOTRGFMT_OUT422_422|S3C_CICOTRGFMT_IN422_422;
			writel(cmd | (1<<29), S3C_CICOTRGFMT);	//(1<<29) Interleave On for RGB on CODEC path
			#endif
		}
		else {
			printk("camif_target_fmt() - Invalid Format\n");
		}
	} 
	else {
		assert(cfg->dma_type & CAMIF_PREVIEW);
		
#if defined(CONFIG_CPU_S3C2412)
		cmd = readl(S3C_CICOTRGFMT);
		cmd &= S3C_CICOTRGFMT_TARGETHSIZE_CO(0)| S3C_CICOTRGFMT_TARGETVSIZE_CO(0);
		cmd |= S3C_CICOTRGFMT_TARGETHSIZE_CO(cfg->target_x)| S3C_CICOTRGFMT_TARGETVSIZE_CO(cfg->target_y);

		assert(cfg->fmt & CAMIF_YCBCR422);
			cmd |= S3C_CICOTRGFMT_OUT422_422|S3C_CICOTRGFMT_IN422_422;
		writel(cmd | (1<<29), S3C_CICOTRGFMT);	//(1<<29) Interleave On(YCbCr 4:2:2 Only)
		
#elif defined(CONFIG_CPU_S3C2443)
		cmd = readl(S3C_CIPRTRGFMT);
		cmd &= S3C_CIPRTRGFMT_TARGETHSIZE_PR(0) | S3C_CIPRTRGFMT_TARGETVSIZE_PR(0);
		cmd |= S3C_CIPRTRGFMT_TARGETHSIZE_PR(cfg->target_x)| S3C_CIPRTRGFMT_TARGETVSIZE_PR(cfg->target_y);
		writel(cmd | (2<<30), S3C_CIPRTRGFMT);	
		
#elif (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
		cmd = readl(S3C_CIPRTRGFMT);
		cmd &= S3C_CIPRTRGFMT_TARGETHSIZE_PR(0) | S3C_CIPRTRGFMT_TARGETVSIZE_PR(0);
		cmd |= S3C_CIPRTRGFMT_TARGETHSIZE_PR(cfg->target_x)| S3C_CIPRTRGFMT_TARGETVSIZE_PR(cfg->target_y);

		/* YCBCR setting */  
		if ( cfg->dst_fmt & CAMIF_YCBCR420 ) {
			cmd |= S3C_CICOTRGFMT_OUTFORMAT_YCBCR420OUT;			
		}
		else if(cfg->dst_fmt & CAMIF_YCBCR422) {	
			cmd |= S3C_CICOTRGFMT_OUTFORMAT_YCBCR422OUT;			 
		}
		else if((cfg->dst_fmt & CAMIF_RGB24) ||(cfg->dst_fmt & CAMIF_RGB16)) {	
			cmd |= S3C_CICOTRGFMT_OUTFORMAT_RGBOUT;	
		}
		else {
			printk("camif_target_fmt() - Invalid Format\n");
		}
		
		writel(cmd, S3C_CIPRTRGFMT);	
#endif
	}
	return 0;
}

void camif_change_flip(camif_cfg_t *cfg)
{
	u32 cmd = 0;
	if (cfg->dma_type & CAMIF_CODEC ) {
		cmd  = readl(S3C_CICOTRGFMT);
		cmd &= ~((1<<14)|(1<<15)); /* Clear FLIP Mode */
		cmd |= cfg->flip;
		writel(cmd, S3C_CICOTRGFMT);
	}
	else {
#if defined(CONFIG_CPU_S3C2412)
		cmd  = readl(S3C_CICOTRGFMT);
		cmd &= ~((1<<14)|(1<<15)); /* Clear FLIP Mode */
		cmd |= cfg->flip;
		writel(cmd, S3C_CICOTRGFMT);
#else
		cmd  = readl(S3C_CIPRTRGFMT);
		cmd &= ~((1<<13)|(1<<14)|(1<<15));     /* Clear FLIP + ROTATE Mode */
		cmd |= cfg->flip;
		writel(cmd, S3C_CIPRTRGFMT);
#endif
	}
}


void camif_change_image_effect(camif_cfg_t *cfg)
{
	u32 val = readl(S3C_CIIMGEFF);     /* Image effect related */
	val &= ~((1<<26)|(1<<27)|(1<<28));       /* Clear image effect */

#if (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
	val |= ((1<<31)|(1<<30));       /* Enable effect in Preview & Codec */
#endif

	switch(cfg->effect) {
		case CAMIF_SILHOUETTE:
			val |= S3C_CIIMGEFF_FIN_SILHOUETTE;
			break;
			
		case CAMIF_EMBOSSING:
			val |= S3C_CIIMGEFF_FIN_EMBOSSING;
			break;
			
		case CAMIF_ART_FREEZE:
			val |= S3C_CIIMGEFF_FIN_ARTFREEZE;
			break;
			
		case CAMIF_NEGATIVE:
			val |= S3C_CIIMGEFF_FIN_NEGATIVE;
			break;
			
		case CAMIF_ARBITRARY_CB_CR:
			val |= S3C_CIIMGEFF_FIN_ARBITRARY;
			break;
			
		case CAMIF_BYPASS:
		default:
			break;
	}
	writel(val, S3C_CIIMGEFF);

}


/* Must:
 * Before calling this function,
 * you must use "camif_setup_fimc_controller"
 * If you want to enable both CODEC and preview
 *  you must do it at the same time.
 */
int camif_capture_start(camif_cfg_t *cfg)
{
	int i=0;
	u32 val;
	u32 n_cmd = readl(S3C_CIIMGCPT);		/* Next Command */

	switch(cfg->capture_enable) {
		case CAMIF_BOTH_DMA_ON:
			camif_reset(CAMIF_RESET,0); /* Flush Camera Core Buffer */

			// For Codec
			val = readl(S3C_CICOSCCTRL);
			val |= S3C_CICOSCCTRL_COSCALERSTART;
			writel(val, S3C_CICOSCCTRL);
			
#if defined(CONFIG_CPU_S3C2412)
			n_cmd = S3C_CIIMGCPT_IMGCPTEN_COSC | (1<<24);
#else
			// For Preview
			val = readl(S3C_CIPRSCCTRL);
			val |= S3C_CIPRSCCTRL_START;
			writel(val, S3C_CIPRSCCTRL);
			
			n_cmd |= S3C_CIIMGCPT_IMGCPTEN_COSC | S3C_CIIMGCPT_IMGCPTEN_PRSC;			
#endif
			break;
		case CAMIF_DMA_ON:
			camif_reset(CAMIF_RESET,0); /* Flush Camera Core Buffer */	
			
			if (cfg->dma_type & CAMIF_CODEC) {
				val = readl(S3C_CICOSCCTRL);
				val |= S3C_CICOSCCTRL_COSCALERSTART;
				writel(val, S3C_CICOSCCTRL);
				n_cmd |= S3C_CIIMGCPT_IMGCPTEN_COSC | (1<<24);
			} else {
#if defined(CONFIG_CPU_S3C2412)
				val = readl(S3C_CICOSCCTRL);
				val |= S3C_CICOSCCTRL_COSCALERSTART;
				writel(val, S3C_CICOSCCTRL);
				n_cmd |= S3C_CIIMGCPT_IMGCPTEN_COSC | (5<<24);

#else
				val = readl(S3C_CIPRSCCTRL);
				val |= S3C_CIPRSCCTRL_START;
				writel(val, S3C_CIPRSCCTRL);
				n_cmd |= S3C_CIIMGCPT_IMGCPTEN_PRSC;
#endif

			}
			
			/* wait until Sync Time expires */
			/* First settting, to wait VSYNC fall  */
			/* By VESA spec,in 640x480 @60Hz 
			   MAX Delay Time is around 64us which "while" has.*/ 
			while(S3C_CICOSTATUS_VSYNC & readl(S3C_CICOSTATUS)); 

			break;
			
		default:
			break;
	}
	
#if defined(CONFIG_CPU_S3C2412)
	if (cfg->fmt & CAMIF_RGB24) 
		n_cmd |= (3<<25); 	        /* RGB24 Codec DMA->RGB 24bit */ 
	else
		n_cmd |= (2<<25);               /* RGB16 Codec DMA->RGB 16bit*/
#else
	if (cfg->dma_type & CAMIF_CODEC) {
		if (cfg->dst_fmt & CAMIF_RGB24) 
			n_cmd |= (3<<25); 	        /* RGB24 Codec DMA->RGB 24 bit*/ 
		else if(cfg->dst_fmt & CAMIF_RGB16)
			n_cmd |= (1<<25);               /* RGB16 Codec DMA->RGB 16 bit */
		else if(cfg->dst_fmt & CAMIF_YCBCR420)
			n_cmd |= (2<<25);               /* YUV420 Codec DMA->YUV420 */
		else if(cfg->dst_fmt & CAMIF_YCBCR422)
			n_cmd |= (0<<25);               /* YUV422 Codec DMA->YUV422 */
	}
#endif
	writel(n_cmd|S3C_CIIMGCPT_IMGCPTEN, S3C_CIIMGCPT);
	return 0;
}


int camif_capture_stop(camif_cfg_t *cfg)
{
	u32 val;
	u32 n_cmd = readl(S3C_CIIMGCPT);	/* Next Command */

	switch(cfg->capture_enable) {
		case CAMIF_BOTH_DMA_OFF:
			val = readl(S3C_CICOSCCTRL);
			val &= ~S3C_CICOSCCTRL_COSCALERSTART;
			writel(val, S3C_CICOSCCTRL);
			
#if !defined(CONFIG_CPU_S3C2412)
			val = readl(S3C_CIPRSCCTRL);
			val &= ~S3C_CIPRSCCTRL_START;
			writel(val, S3C_CIPRSCCTRL);
#endif
			n_cmd = 0;
			break;
			
		case CAMIF_DMA_OFF_L_IRQ: /* fall thru */
		case CAMIF_DMA_OFF:
			if (cfg->dma_type & CAMIF_CODEC) {
				val = readl(S3C_CICOSCCTRL);
				val &= ~S3C_CICOSCCTRL_COSCALERSTART;
				writel(val, S3C_CICOSCCTRL);
				n_cmd &= ~S3C_CIIMGCPT_IMGCPTEN_COSC;
				if (!(n_cmd & S3C_CIIMGCPT_IMGCPTEN_PRSC)) {
					n_cmd = 0;
				}
			}else {
#if defined(CONFIG_CPU_S3C2412)
				val = readl(S3C_CICOSCCTRL);
				val &= ~S3C_CICOSCCTRL_COSCALERSTART;
				writel(val, S3C_CICOSCCTRL);
#else
				val = readl(S3C_CIPRSCCTRL);
				val &= ~S3C_CIPRSCCTRL_START;
				writel(val, S3C_CIPRSCCTRL);
#endif
				n_cmd &= ~S3C_CIIMGCPT_IMGCPTEN_PRSC;
				if (!(n_cmd & S3C_CIIMGCPT_IMGCPTEN_COSC))
					n_cmd = 0;
			}
			break;
		default:
			printk("s3c_camif.c : Unexpected \n");
	}
	
	writel(n_cmd|(0<<24), S3C_CIIMGCPT);



	if(cfg->capture_enable == CAMIF_DMA_OFF_L_IRQ) { /* Last IRQ  */
		if (cfg->dma_type & CAMIF_CODEC) {
			val = readl(S3C_CICOCTRL);
			val |= S3C_CICOCTRL_LASTIRQEN;
			writel(val, S3C_CICOCTRL);
		}
		else { 
#if defined(CONFIG_CPU_S3C2412)
			val = readl(S3C_CICOCTRL);
			val |= S3C_CICOCTRL_LASTIRQEN;
			writel(val, S3C_CICOCTRL);
			
#else
			val = readl(S3C_CIPRCTRL);
			val |= S3C_CIPRCTRL_LASTIRQEN_ENABLE;
			writel(val, S3C_CIPRCTRL);
#endif
		}
	} 

	return 0;
}


#if defined(CONFIG_CPU_S3C2443) || (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
int camif_start_postprocessing(camif_cfg_t *cfg) 
{
	u32 val= readl(S3C_CIMSCTRL);	/* Next Command */

	if (cfg->dst_fmt & CAMIF_YCBCR420) {
		val |= (1<<1);
	}
	else {
		val &= ~(1<<1);
	}

	// clear ENVID_MS
	val &= ~(1 << 0);
	writel(val, S3C_CIMSCTRL);

	// Set ENVID_MS
	val |= (1 << 0);
	writel(val, S3C_CIMSCTRL);

	printk(KERN_INFO "camif_start_postprocessing() started\n");
	
	// Waiting for EOF_MS
	while((readl(S3C_CIMSCTRL) & (1<<6)) == 0);

	printk(KERN_INFO "camif_start_postprocessing() finished\n");
	return 0;
	
}
#endif


#if (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
/* 2007-05-11 by YREOM PIP for S3C6400 */
int camif_set_codec_msdma(camif_cfg_t * cfg)
{
	int ret = 0;
	u32 val, val_width;
	u32 addr_start_Y=0, addr_start_CB=0, addr_start_CR=0;
	u32 addr_end_Y=0, addr_end_CB=0, addr_end_CR=0;

	/* Codec path input data selection */	
	// Clear codec path input 
	val = readl(S3C_MSCOCTRL);	
	val &= ~(1<<3);
	writel(val, S3C_MSCOCTRL);
	
	// Set MSDMA input path
	val = readl(S3C_MSCOCTRL);	
	val |= (1<<3);
	writel(val, S3C_MSCOCTRL);
	
	/* MSDMAFormat */			
	switch(cfg->src_fmt) {
		default:
		case 420:
			val = readl(S3C_MSCOCTRL);
			val = (val & ~(0x3<<1)) | (0x0<<1);
			writel(val, S3C_MSCOCTRL);

			addr_start_Y = cfg->pp_phys_buf;
			addr_start_CB = addr_start_Y+(cfg->cis->source_x*cfg->cis->source_y);
			addr_start_CR = addr_start_CB+(cfg->cis->source_x*cfg->cis->source_y*1/4);

			addr_end_Y = addr_start_Y+(cfg->cis->source_x*cfg->cis->source_y);
			addr_end_CB = addr_start_CB+(cfg->cis->source_x*cfg->cis->source_y*1/4);
			addr_end_CR = addr_start_CR+(cfg->cis->source_x*cfg->cis->source_y*1/4);
			break;

		case 422:
			val = readl(S3C_MSCOCTRL);
			val = (val & ~(0x3<<1)) |(0x2<<1);
			writel(val, S3C_MSCOCTRL);

			addr_start_Y = cfg->pp_phys_buf;
			addr_start_CB = addr_start_Y+(cfg->cis->source_x*cfg->cis->source_y);
			addr_start_CR = addr_start_CB+(cfg->cis->source_x*cfg->cis->source_y*1/2);

			addr_end_Y = addr_start_Y+(cfg->cis->source_x*cfg->cis->source_y);
			addr_end_CB = addr_start_CB+(cfg->cis->source_x*cfg->cis->source_y*1/2);
			addr_end_CR = addr_start_CR+(cfg->cis->source_x*cfg->cis->source_y*1/2);
			break;

		case 16:
		case 24:
			/* To Do */
			break;
	}

	/* MSDMA memory */
	writel(addr_start_Y, S3C_MSCOY0SA);
	writel(addr_start_CB, S3C_MSCOCB0SA);
	writel(addr_start_CR, S3C_MSCOCR0SA);

	writel(addr_end_Y, S3C_MSCOY0END);
	writel(addr_end_CB, S3C_MSCOCB0END);
	writel(addr_end_CR, S3C_MSCOCR0END);

	/* MSDMA memory offset */
	writel(0, S3C_MSCOYOFF);
	writel(0, S3C_MSCOCBOFF);
	writel(0, S3C_MSCOCROFF);

	/* MSDMA for codec source image width */
	val_width = readl(S3C_MSCOWIDTH);
	val_width = (val_width & ~(0x1<<31))|(0x1<<31);	// AutoLoadEnable
	val_width |= (cfg->cis->source_y<<16);	// MSCOHEIGHT
	val_width |= (cfg->cis->source_x<<0);	// MSCOWIDTH
	writel(val_width, S3C_MSCOWIDTH);

	return ret;
}

int camif_codec_msdma_start(camif_cfg_t * cfg)
{
	int ret = 0;
	u32 val;

	/* MSDMA start */
	// Clear ENVID_MS
	val = readl(S3C_MSCOCTRL);	
	val &= ~(1<<0);
	writel(val, S3C_MSCOCTRL);
	
	// Set ENVID_MS
	val = readl(S3C_MSCOCTRL);	
	val |= (1<<0);
	writel(val, S3C_MSCOCTRL);

	return ret;
}
EXPORT_SYMBOL(camif_codec_msdma_start);
#endif

int camif_set_preview_msdma(camif_cfg_t * cfg)
{
	int ret = 0;
	u32 val, val_width;
	u32 addr_start_Y=0, addr_start_CB=0, addr_start_CR=0;
	u32 addr_end_Y=0, addr_end_CB=0, addr_end_CR=0;

	/* Codec path input data selection */	

	// Clear preview path input 
	val = readl(S3C_CIMSCTRL);	
#if defined(CONFIG_CPU_S3C2443)
	val &= ~(1<<2);
#else
	val &= ~(1<<3);
#endif
	writel(val, S3C_CIMSCTRL);
	
	// Set MSDMA input path
	val = readl(S3C_CIMSCTRL);	
#if defined(CONFIG_CPU_S3C2443)
	val |= (1<<2);
#else
	val |= (1<<3);
#endif
	writel(val, S3C_CIMSCTRL);
	
	/* MSDMAFormat */			
	switch(cfg->src_fmt) {
		default:
		case CAMIF_YCBCR420:
			val = readl(S3C_CIMSCTRL);
			#if defined(CONFIG_CPU_S3C2443)
			val = (val & ~(0x1<<1)) | (0x1<<1);
			#else
			val = (val & ~(0x3<<1)) | (0x0<<1);
			#endif
			writel(val, S3C_CIMSCTRL);

		#if defined(CONFIG_CPU_S3C2443)
			addr_start_Y = readl(S3C_CIMSYSA);
		#elif 	(defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
			addr_start_Y = readl(S3C_MSPRY0SA);
		#endif
			addr_start_CB = addr_start_Y + (cfg->cis->source_x*cfg->cis->source_y);
			addr_start_CR = addr_start_CB + (cfg->cis->source_x*cfg->cis->source_y*1/4);

			addr_end_Y = addr_start_Y + (cfg->cis->source_x*cfg->cis->source_y);
			addr_end_CB = addr_start_CB + (cfg->cis->source_x*cfg->cis->source_y*1/4);
			addr_end_CR = addr_start_CR + (cfg->cis->source_x*cfg->cis->source_y*1/4);
			break;

		case CAMIF_YCBCR422:
			val = readl(S3C_CIMSCTRL);
			#if defined(CONFIG_CPU_S3C2443)
			val = (val & ~(0x1<<5)) |(0x1<<5);	//Interleave_MS
			val = (val & ~(0x1<<1)) |(0x0<<1);
			val = (val & ~(0x3<<3)) |(0x0<<3);	//YCbYCr
			#elif (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
			val = (val & ~(0x3<<1)) |(0x2<<1);	//YCbCr 422 Interleave
			val = (val & ~(0x3<<4)) |(0x3<<4);	//YCbYCr
			#else
			val = (val & ~(0x3<<1)) |(0x2<<1);
			#endif
			writel(val, S3C_CIMSCTRL);

		#if defined(CONFIG_CPU_S3C2443)
			addr_start_Y = readl(S3C_CIMSYSA);
		#elif 	(defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
			addr_start_Y = readl(S3C_MSPRY0SA);
		#endif
			addr_start_CB = addr_start_Y + (cfg->cis->source_x*cfg->cis->source_y);
			addr_start_CR = addr_start_CB + (cfg->cis->source_x*cfg->cis->source_y*1/2);

			addr_end_Y = addr_start_Y + (cfg->cis->source_x*cfg->cis->source_y);
			addr_end_CB = addr_start_CB + (cfg->cis->source_x*cfg->cis->source_y*1/2);
			addr_end_CR = addr_start_CR + (cfg->cis->source_x*cfg->cis->source_y*1/2);
			break;
			
		case CAMIF_RGB16:
		case CAMIF_RGB24:
			/* To Do */
			break;
	}

#if defined(CONFIG_CPU_S3C2443)
	/* MSDMA memory */
	writel(addr_start_Y, S3C_CIMSYSA);
	writel(addr_start_CB, S3C_CIMSCBSA);
	writel(addr_start_CR, S3C_CIMSCRSA);

	writel(addr_end_Y, S3C_CIMSYEND);
	writel(addr_end_CB, S3C_CIMSCBEND);
	writel(addr_end_CR, S3C_CIMSCREND);

	/* MSDMA memory offset - default : 0 */
	writel(0, S3C_CIMSYOFF);
	writel(0, S3C_CIMSCBOFF);
	writel(0, S3C_CIMSCROFF);

#elif 	(defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
	/* MSDMA memory */
	writel(addr_start_Y, S3C_MSPRY0SA);
	writel(addr_start_CB, S3C_MSPRCB0SA);
	writel(addr_start_CR, S3C_MSPRCR0SA);

	writel(addr_end_Y, S3C_MSPRY0END);
	writel(addr_end_CB, S3C_MSPRCB0END);
	writel(addr_end_CR, S3C_MSPRCR0END);

	/* MSDMA memory offset */
	writel(0, S3C_MSPRYOFF);
	writel(0, S3C_MSPRCBOFF);
	writel(0, S3C_MSPRCROFF);
#endif

	/* MSDMA for codec source image width */
#if defined(CONFIG_CPU_S3C2443)
	val_width = readl(S3C_CIMSWIDTH);
#elif 	(defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
	val_width = readl(S3C_MSPRWIDTH);
#endif

#ifdef SW_IPC
	val_width = (val_width & ~(0x1<<31))|(0x1<<31);	// AutoLoadEnable
#else
	val_width = (val_width & ~(0x1<<31));	// AutoLoadDisable
#endif
	val_width |= (cfg->cis->source_y<<16);		// MSCOHEIGHT
	val_width |= (cfg->cis->source_x<<0);		// MSCOWIDTH
#if defined(CONFIG_CPU_S3C2443)
	val_width = cfg->cis->source_x;
	writel(val_width, S3C_CIMSWIDTH);
#elif 	(defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
	writel(val_width, S3C_MSPRWIDTH);
#endif

	return ret;
}

int camif_preview_msdma_start(camif_cfg_t * cfg)
{
	int ret = 0;
	u32 val;

	/* MSDMA start */
	#if	(!defined(CONFIG_CPU_S3C6400) && !defined(CONFIG_CPU_S3C6410))
	// Clear ENVID_MS
	val = readl(S3C_CIMSCTRL);	
	val &= ~(1<<0);
	writel(val, S3C_CIMSCTRL);
	#endif
	
	// Set ENVID_MS
	val = readl(S3C_CIMSCTRL);	
	val |= (1<<0);
	writel(val, S3C_CIMSCTRL);

	while(readl(S3C_CIMSCTRL)&(1<<6) == 0);
	return ret;
}
EXPORT_SYMBOL(camif_preview_msdma_start);

/* LastIRQEn is autoclear */
void camif_last_irq_en(camif_cfg_t *cfg)
{
	u32 val;
	if(cfg->capture_enable == CAMIF_BOTH_DMA_ON) {
		val = readl(S3C_CICOCTRL);
		val |= S3C_CICOCTRL_LASTIRQEN;
		writel(val, S3C_CICOCTRL);
		
#if !defined(CONFIG_CPU_S3C2412)
		val = readl(S3C_CIPRCTRL);
		val |= S3C_CIPRCTRL_LASTIRQEN_ENABLE;
		writel(val, S3C_CIPRCTRL);
#endif
	}
	else {
		if (cfg->dma_type & CAMIF_CODEC) {
			val = readl(S3C_CICOCTRL);
			val |= S3C_CICOCTRL_LASTIRQEN;
			writel(val, S3C_CICOCTRL);
		}
		else {
#if defined(CONFIG_CPU_S3C2412)
			val = readl(S3C_CICOCTRL);
			val |= S3C_CICOCTRL_LASTIRQEN;
			writel(val, S3C_CICOCTRL);
			
#else
			val = readl(S3C_CIPRCTRL);
			val |= S3C_CIPRCTRL_LASTIRQEN_ENABLE;
			writel(val, S3C_CIPRCTRL);
#endif
		}
			
	}
}

static int  
camif_scaler_internal(u32 srcWidth, u32 dstWidth, u32 *ratio, u32 *shift)
{
	if(srcWidth>=64*dstWidth) {
		printk(KERN_ERR "s3c_camif.c: Out of prescaler range: srcWidth /dstWidth = %d(< 64)\n",
			 srcWidth/dstWidth);
		return 1;
	}
	else if(srcWidth>=32*dstWidth) {
		*ratio=32;
		*shift=5;
	}
	else if(srcWidth>=16*dstWidth) {
		*ratio=16;
		*shift=4;
	}
	else if(srcWidth>=8*dstWidth) {
		*ratio=8;
		*shift=3;
	}
	else if(srcWidth>=4*dstWidth) {
		*ratio=4;
		*shift=2;
	}
	else if(srcWidth>=2*dstWidth) {
		*ratio=2;
		*shift=1;
	}
	else {
		*ratio=1;
		*shift=0;
	}  
	return 0;
}


int camif_g_fifo_status(camif_cfg_t *cfg) 
{
	u32 reg, val;
	if (cfg->dma_type & CAMIF_CODEC) {
		u32 flag = S3C_CICOSTATUS_OVFIY_CO|S3C_CICOSTATUS_OVFICB_CO|S3C_CICOSTATUS_OVFICR_CO;
		reg = readl(S3C_CICOSTATUS);
		
		if (reg & flag) {
			/* FIFO Error Count ++  */
			val = readl(S3C_CIWDOFST);
			val |= S3C_CIWDOFST_CLROVCOFIY|S3C_CIWDOFST_CLROVCOFICB|S3C_CIWDOFST_CLROVCOFICR;
			writel(val, S3C_CIWDOFST);
			val = readl(S3C_CIWDOFST);
			val &= ~(S3C_CIWDOFST_CLROVCOFIY|S3C_CIWDOFST_CLROVCOFICB|S3C_CIWDOFST_CLROVCOFICR);
			writel(val, S3C_CIWDOFST);
			return 1; /* Error */
		}
	}
	
	if (cfg->dma_type & CAMIF_PREVIEW) {
#if defined(CONFIG_CPU_S3C2412)
		u32 flag = S3C_CICOSTATUS_OVFIY_CO|S3C_CICOSTATUS_OVFICB_CO|S3C_CICOSTATUS_OVFICR_CO;
		reg = readl(S3C_CICOSTATUS);
		
		if (reg & flag) {
			/* FIFO Error Count ++  */
			val = readl(S3C_CIWDOFST);
			val |= S3C_CIWDOFST_CLROVCOFIY|S3C_CIWDOFST_CLROVCOFICB|S3C_CIWDOFST_CLROVCOFICR;
			writel(val, S3C_CIWDOFST);
			
			val = readl(S3C_CIWDOFST);
			val &= ~(S3C_CIWDOFST_CLROVCOFIY|S3C_CIWDOFST_CLROVCOFICB|S3C_CIWDOFST_CLROVCOFICR);
			writel(val, S3C_CIWDOFST);
			return 1; /* Error */
		}
#else
		u32 flag = S3C_CIPRSTATUS_OVFICB_PR|S3C_CIPRSTATUS_OVFICR_PR;
		reg = readl(S3C_CIPRSTATUS);
		
		if (reg & flag) {
			/* FIFO Error Count ++  */
			val = readl(S3C_CIWDOFST);
			val |= S3C_CIWDOFST_CLROVPRFICB|S3C_CIWDOFST_CLROVPRFICR;
			writel(val, S3C_CIWDOFST);
			
			val = readl(S3C_CIWDOFST);
			val &= ~(S3C_CIWDOFST_CLROVCOFIY|S3C_CIWDOFST_CLROVCOFICB|S3C_CIWDOFST_CLROVCOFICR);
			writel(val, S3C_CIWDOFST);
			return 1; /* Error */
		}

#endif
	}
	return 0;		/* No Error */
}


/* Policy:
 * if codec or preview define the win offset,
 *    other must follow that value.
 */
int camif_win_offset(camif_cis_t *cis)
{
	u32 h = cis->win_hor_ofst;	/* Camera input offset ONLY */
	u32 v = cis->win_ver_ofst;	/* Camera input offset ONLY */
	u32 h2 = cis->win_hor_ofst2;	/* Camera input offset ONLY */
	u32 v2 = cis->win_ver_ofst2;	/* Camera input offset ONLY */

	/*Clear Overflow */
#if defined(CONFIG_CPU_S3C2412)
	writel(S3C_CIWDOFST_CLROVCOFIY|S3C_CIWDOFST_CLROVCOFICB|S3C_CIWDOFST_CLROVCOFICR, S3C_CIWDOFST);
#else
	writel(S3C_CIWDOFST_CLROVCOFIY|S3C_CIWDOFST_CLROVCOFICB|S3C_CIWDOFST_CLROVCOFICR|S3C_CIWDOFST_CLROVPRFICB|S3C_CIWDOFST_CLROVPRFICR, S3C_CIWDOFST);
#endif
	writel(0, S3C_CIWDOFST);		/* ? Dummy - Clear */

	if (!h && !v)	{
		writel(0, S3C_CIWDOFST);
		writel(0, S3C_CIDOWSFT2);
		return 0;
	}
	
	writel(S3C_CIWDOFST_WINOFSEN | S3C_CIWDOFST_WINHOROFST(h) | S3C_CIWDOFST_WINVEROFST(v), S3C_CIWDOFST);
	writel(S3C_CIDOWSFT2_WINHOROFST2(h) | S3C_CIDOWSFT2_WINVEROFST2(v), S3C_CIDOWSFT2);
	writel(S3C_CIDOWSFT2_WINHOROFST2(h2) | S3C_CIDOWSFT2_WINVEROFST2(v2), S3C_CIDOWSFT2);
	return 0;
}

/*  
 * when you change the resolution in a specific camera,
 * sometimes, it is necessary to change the polarity 
 *                                       -- SW.LEE
 */
static void camif_polarity(camif_cis_t *cis)
{
	u32 val;
	u32 cmd;
	
	cmd = readl(S3C_CIGCTRL);
	cmd = cmd & ~((1<<26)|(1<<25)|(1<<24)); /* clear polarity */
	
	if (cis->polarity_pclk)
		cmd |= S3C_CIGCTRL_INVPOLPCLK;
	
	if (cis->polarity_vsync)
		cmd |= S3C_CIGCTRL_INVPOLVSYNC;
	
	if (cis->polarity_href)
		cmd |= S3C_CIGCTRL_INVPOLHREF;
	
	val = readl(S3C_CIGCTRL);
	val |= cmd;
	writel(val, S3C_CIGCTRL);
}


int camif_setup_fimc_controller(camif_cfg_t *cfg)
{
	
	if(camif_malloc(cfg) ) {
		printk(KERN_ERR "s3c_camif.c: Instead of using consistent_alloc()\n"
			        "    Let me use dedicated mem for DMA \n");
		return -1;
	}

	camif_setup_intput_path(cfg);
	
	if(camif_setup_scaler(cfg)) {
		printk(KERN_ERR "s3c_camif.c:Preview Scaler, Change WinHorOfset or Target Size\n");
		return 1;
	}
	camif_target_fmt(cfg);
	if (camif_dma_burst(cfg)) {
		printk(KERN_ERR "s3c_camif.c:DMA Busrt Length Error \n");
		return 1;
	}

	camif_setup_output_path(cfg);
	
	return 0;
}

int inline camif_dynamic_close(camif_cfg_t *cfg)
{
	if(cfg->dma_type & CAMIF_PREVIEW) {
		if(cfg->interlace_capture == 1)
			return 0;
			
	}
	camif_demalloc(cfg);
	return 0;
}


static int camif_setup_intput_path(camif_cfg_t *cfg) 
{
	
	if(cfg->input_channel == CAMERA_INPUT ){  // Camera Input
		camif_setup_camera_input(cfg); 
	}
	else{  // MSDMA Input
		camif_setup_msdma_input(cfg); 
	}

	return 0;
}


static int camif_setup_camera_input(camif_cfg_t *cfg) 
{
	u32 val;
	camif_win_offset(cfg->cis);
	camif_polarity(cfg->cis);

	if (cfg->dma_type & CAMIF_CODEC) {  //  CODEC input path setup : External camera:0, MSDMA:1
#if (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
		val = readl(S3C_MSCOCTRL);	
		val &= ~(1<<3);			// External camera
		writel(val, S3C_MSCOCTRL);
#endif
	}
	else if(cfg->dma_type & CAMIF_PREVIEW){  // Preview input path setup : External camera:0, MSDMA:1
		val = readl(S3C_CIMSCTRL);	
		val &= ~(1<<3);			// External camera
		writel(val, S3C_CIMSCTRL);
	}
	else{
		printk("camif_setup_camera_input : CAMERA:DMA_TYPE Wrong \n");
	}

	return 0;
}


static int camif_setup_msdma_input(camif_cfg_t *cfg) 
{

	if(cfg->input_channel == MSDMA_FROM_PREVIEW) // Preview MSDMA Input
		camif_set_preview_msdma(cfg);
#if 	(defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
	else // Codec MSDMA Input
		camif_set_codec_msdma(cfg); 
#endif
	return 0;
}


static int camif_setup_output_path(camif_cfg_t *cfg) 
{
	if(cfg->output_channel == CAMIF_OUT_FIFO)
		camif_setup_lcd_fifo_output(cfg); 
	else
		camif_setup_memory_output(cfg);
	
	return 0;
	
}


static int camif_target_area(camif_cfg_t *cfg) 
{
	u32 rect = cfg->target_x * cfg->target_y;

	if (cfg->dma_type & CAMIF_CODEC ) {
		writel(rect, S3C_CICOTAREA);
	}
	
	if (cfg->dma_type & CAMIF_PREVIEW) {
#if defined(CONFIG_CPU_S3C2412)
		writel(rect, S3C_CICOTAREA);
#else
		writel(rect, S3C_CIPRTAREA);
#endif
	}
	
	return 0;
}

static int inline camif_hw_reg(camif_cfg_t *cfg)
{
	u32 cmd = 0;
	
	if (cfg->dma_type & CAMIF_CODEC) {
		writel(S3C_CICOSCPRERATIO_SHFACTOR_CO(cfg->sc.shfactor)
			|S3C_CICOSCPRERATIO_PREHORRATIO_CO(cfg->sc.prehratio)|S3C_CICOSCPRERATIO_PREVERRATIO_CO(cfg->sc.prevratio), S3C_CICOSCPRERATIO);
		writel(S3C_CICOSCPREDST_PREDSTWIDTH_CO(cfg->sc.predst_x)|S3C_CICOSCPREDST_PREDSTHEIGHT_CO(cfg->sc.predst_y), S3C_CICOSCPREDST);
		
		/* Differ from Preview */
		if (cfg->sc.scalerbypass)
			cmd |= S3C_CICOSCCTRL_SCALERBYPASS_CO;

		#if (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
		/* Differ from Codec */
		if (cfg->dst_fmt & CAMIF_RGB24) {
			cmd |= S3C_CICOSCCTRL_OUTRGB_FMT_RGB888;
		}
		else { /* RGB16 */
			cmd |= S3C_CICOSCCTRL_OUTRGB_FMT_RGB565;
		}	
		#endif
		
		if (cfg->sc.scaleup_h & cfg->sc.scaleup_v)
			cmd |= S3C_CICOSCCTRL_SCALEUP_H|S3C_CICOSCCTRL_SCALEUP_V;

		writel(cmd | S3C_CICOSCCTRL_MAINHORRATIO_CO(cfg->sc.mainhratio) | S3C_CICOSCCTRL_MAINVERRATIO_CO(cfg->sc.mainvratio), S3C_CICOSCCTRL);
		
	}
	else if (cfg->dma_type & CAMIF_PREVIEW) {
#if defined(CONFIG_CPU_S3C2412)
		writel(S3C_CICOSCPRERATIO_SHFACTOR_CO(cfg->sc.shfactor)
			|S3C_CICOSCPRERATIO_PREHORRATIO_CO(cfg->sc.prehratio)|S3C_CICOSCPRERATIO_PREVERRATIO_CO(cfg->sc.prevratio), S3C_CICOSCPRERATIO);
		writel(S3C_CICOSCPREDST_PREDSTWIDTH_CO(cfg->sc.predst_x)|S3C_CICOSCPREDST_PREDSTHEIGHT_CO(cfg->sc.predst_y), S3C_CICOSCPREDST);

		if (cfg->sc.scaleup_h & cfg->sc.scaleup_v)
			cmd |= BIT30|BIT29;

		/* Differ from Preview */
		if (cfg->sc.scalerbypass)
			cmd |= S3C_CICOSCCTRL_SCALERBYPASS_CO;
		writel(cmd | S3C_CICOSCCTRL_MAINHORRATIO_CO(cfg->sc.mainhratio) | S3C_CICOSCCTRL_MAINVERRATIO_CO(cfg->sc.mainvratio), S3C_CICOSCCTRL);
#else
		writel(S3C_CIPRSCPRERATIO_SHFACTOR_PR(cfg->sc.shfactor)
			|S3C_CIPRSCPRERATIO_PREHORRATIO_PR(cfg->sc.prehratio)|S3C_CIPRSCPRERATIO_PREVERRATIO_PR(cfg->sc.prevratio), S3C_CIPRSCPRERATIO);
		writel(S3C_CIPRSCPREDST_PREDSTWIDTH_PR(cfg->sc.predst_x)|S3C_CIPRSCPREDST_PREDSTHEIGHT_PR(cfg->sc.predst_y), S3C_CIPRSCPREDST);


		/* Differ from Codec */
		if (cfg->dst_fmt & CAMIF_RGB24) {
			#if defined(CONFIG_CPU_S3C2443)
			cmd |= S3C_CIPRSCCTRL_RGBFORMAT_24;
			#elif (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
			cmd |= S3C_CIPRSCCTRL_OUTRGB_FMT_PR_RGB888;
			#endif
		}
		else { /* RGB16 */
			#if (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
			cmd |= S3C_CIPRSCCTRL_OUTRGB_FMT_PR_RGB565;
			#endif;
		}	

		if (cfg->sc.scaleup_h & cfg->sc.scaleup_v)
			cmd |= ((1<<30)|(1<<29));

	#if defined(CONFIG_CPU_S3C2443)
		writel(cmd | S3C_CIPRSCCTRL_MAINHORRATIO_PR(cfg->sc.mainhratio) | S3C_CIPRSCCTRL_MAINVERRATIO_PR(cfg->sc.mainvratio), S3C_CIPRSCCTRL);
	#elif (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
		writel(cmd | S3C_CIPRSCCTRL_MAINHORRATIO_PR(cfg->sc.mainhratio)|S3C_CIPRSCCTRL_MAINVERRATIO_PR(cfg->sc.mainvratio), S3C_CIPRSCCTRL);
	#endif
#endif
	}
	else {
		printk("s3c_camif.c : CAMERA:DMA_TYPE Wrong \n");
	}
	
	return 0;
}


/* Configure Pre-scaler control  & main scaler control register */
static int camif_setup_scaler(camif_cfg_t *cfg)
{
	int tx = cfg->target_x, ty=cfg->target_y;
	int sx, sy;

	if (tx <= 0 || ty<= 0) {
		printk("s3c_camif.c: Invalid target size \n");
		return -1;
	}
#if 0
	sx = cfg->cis->source_x - 2*cfg->cis->win_hor_ofst;
	sy = cfg->cis->source_y - 2*cfg->cis->win_ver_ofst;	
#else
	sx = cfg->cis->source_x - (cfg->cis->win_hor_ofst + cfg->cis->win_hor_ofst2);
	sy = cfg->cis->source_y - (cfg->cis->win_ver_ofst + cfg->cis->win_hor_ofst2);	
#endif
	if (sx <= 0 || sy<= 0) 	{
		printk("s3c_camif.c: Invalid source size \n");
		return -1;
	}
	cfg->sc.modified_src_x = sx;
	cfg->sc.modified_src_y = sy;

	/* Pre-scaler control register 1 */
	camif_scaler_internal(sx, tx, &cfg->sc.prehratio,&cfg->sc.hfactor);
	camif_scaler_internal(sy, ty, &cfg->sc.prevratio,&cfg->sc.vfactor);

	if (cfg->dma_type & CAMIF_PREVIEW) {
		if ( (sx /cfg->sc.prehratio) <= 640 ) {}
		else {
			printk(KERN_INFO "s3c_camif.c: Internal Preview line buffer is 640 pixels\n");
			//return 1; /* Error */
		}
	}

	cfg->sc.shfactor = 10-(cfg->sc.hfactor+cfg->sc.vfactor);
	/* Pre-scaler control register 2 */
	cfg->sc.predst_x = sx / cfg->sc.prehratio;
	cfg->sc.predst_y = sy / cfg->sc.prevratio;

	/* Main-scaler control register */
	cfg->sc.mainhratio = (sx << 8)/(tx << cfg->sc.hfactor);
	cfg->sc.mainvratio = (sy << 8)/(ty << cfg->sc.vfactor);

	DPRINTK(" sx %d, sy %d tx %d ty %d  \n",sx,sy,tx,ty);
	DPRINTK(" hfactor %d  vfactor %d \n",cfg->sc.hfactor,cfg->sc.vfactor);
	DPRINTK(" prehratio %d  prevration %d \n",cfg->sc.prehratio,cfg->sc.prevratio);
	DPRINTK(" mainhratio %d  mainvratio %d \n",cfg->sc.mainhratio,cfg->sc.mainvratio);

	cfg->sc.scaleup_h  = (sx <= tx) ? 1: 0;
	cfg->sc.scaleup_v  = (sy <= ty) ? 1: 0;
	
	if ( cfg->sc.scaleup_h != cfg->sc.scaleup_v) {
		DPRINTK(KERN_ERR "s3c_camif.c : scaleup_h must be same to scaleup_v \n");
		DPRINTK(KERN_ERR " sx %d, sy %d tx %d ty %d  \n",sx,sy,tx,ty);
		DPRINTK(KERN_ERR " hfactor %d  vfactor %d \n",cfg->sc.hfactor,cfg->sc.vfactor);
		DPRINTK(KERN_ERR " prehratio %d  prevration %d \n",cfg->sc.prehratio,cfg->sc.prevratio);
		DPRINTK(KERN_ERR " mainhratio %d  mainvratio %d \n",cfg->sc.mainhratio,cfg->sc.mainvratio);
		DPRINTK(KERN_ERR " scaleup_h %d  scaleup_v %d \n",cfg->sc.scaleup_h,cfg->sc.scaleup_v);
	}
	camif_hw_reg(cfg);
	camif_target_area(cfg);
	
	return 0;
}

/******************************************************
 CalculateBurstSize - Calculate the busrt lengths
 Description:
 - dstHSize: the number of the byte of H Size.
********************************************************/
static void camif_g_bsize(u32 hsize, u32 *mburst, u32 *rburst)
{
	u32 tmp;

	tmp = (hsize>>2) & 0xf;
	switch(tmp) {
		case 0:
			*mburst=16;
			*rburst=16;
			break;
		case 4:
			*mburst=16;
			*rburst=4;
			break;
		case 8:
			*mburst=16;
			*rburst=8;
			break;
		default:
			tmp=(hsize/4)%8;
			switch(tmp) {
				case 0:
					*mburst=8;
					*rburst=8;
					break;
				case 4:
					*mburst=8;
					*rburst=4;
				default:
					*mburst=4;
					tmp=(hsize/4)%4;
					*rburst= (tmp) ? tmp: 4;
					break;
			}
			break;
	}
}

/* UXGA 1600x1200 */
/* SXGA 1028x1024 */
/* XGA 1024x768 */
/* SVGA 800x600 */
/* VGA 640x480 */
/* CIF 352x288 */
/* QVGA 320x240 */
/* QCIF 176x144 */
/* ret val 
        1 : DMA Size Error 
*/

#define BURST_ERR 1 
static int camif_dma_burst(camif_cfg_t *cfg)
{
	u32 val;
	u32 yburst_m, yburst_r;
	u32 cburst_m, cburst_r;
	int width = cfg->target_x;

	if (cfg->dma_type & CAMIF_CODEC ) {
		if((cfg->dst_fmt == CAMIF_RGB16)||(cfg->dst_fmt == CAMIF_RGB24)) {
			if(cfg->dst_fmt == CAMIF_RGB24) {
				if (width %2 != 0 )  return BURST_ERR;   /* DMA Burst Length Error */
				camif_g_bsize(width*4,&yburst_m,&yburst_r);
			
			}
			else {	/* CAMIF_RGB16 */
				if ((width/2) %2 != 0 )  return BURST_ERR; /* DMA Burst Length Error */
				camif_g_bsize(width*2,&yburst_m,&yburst_r);
			}
			
			val = readl(S3C_CICOCTRL);
			if(cfg->dst_fmt ==CAMIF_RGB24) 
				val = S3C_CICOCTRL_YBURST1_CO(yburst_m/2) | S3C_CICOCTRL_YBURST2_CO(yburst_r/4) | (4<<9) | (2<<4);
			else
				val = S3C_CICOCTRL_YBURST1_CO(yburst_m/2) | S3C_CICOCTRL_YBURST2_CO(yburst_r/2) | (4<<9) | (2<<4);
			writel(val, S3C_CICOCTRL);
		}
		else {
			/* CODEC DMA WIDHT is multiple of 16 */
			if (width %16 != 0 )  return BURST_ERR;   /* DMA Burst Length Error */
#ifdef TOMTOM_INTERLACE_MODE
			camif_g_bsize(width*2,&yburst_m,&yburst_r);
			yburst_m = yburst_m >> 1;
			yburst_r = yburst_r >> 1;
			cburst_m = yburst_m >> 1;
			cburst_r = yburst_r >> 1;
			if(cfg->interlace_capture)
			#ifdef CONFIG_CPU_S3C2443	
				writel(width, S3C_CICOSCOS);
			#else
				writel(width, S3C_CICOSCOSY);
			#endif
#else
			camif_g_bsize(width,&yburst_m,&yburst_r);
			camif_g_bsize(width/2,&cburst_m,&cburst_r);
#endif
			val = readl(S3C_CICOCTRL);
			val = S3C_CICOCTRL_YBURST1_CO(yburst_m) | S3C_CICOCTRL_CBURST1_CO(cburst_m) | S3C_CICOCTRL_YBURST2_CO(yburst_r) | S3C_CICOCTRL_CBURST2_CO(cburst_r);
			writel(val, S3C_CICOCTRL);
		}
	}

	if (cfg->dma_type & CAMIF_PREVIEW) {
		if(cfg->dst_fmt == CAMIF_RGB24) {
			if (width %2 != 0 )  return BURST_ERR;   /* DMA Burst Length Error */
			camif_g_bsize(width*4,&yburst_m,&yburst_r);
		}
		else {		/* CAMIF_RGB16 */
			if ((width/2) %2 != 0 )  return BURST_ERR; /* DMA Burst Length Error */
			camif_g_bsize(width*2,&yburst_m,&yburst_r);
		}
#if defined(CONFIG_CPU_S3C2412)	
		val = readl(S3C_CICOCTRL);
		if(cfg->dst_fmt ==CAMIF_RGB24) 
			val = S3C_CICOCTRL_YBURST1_CO(yburst_m/2) | S3C_CICOCTRL_YBURST2_CO(yburst_r/4) | (4<<9) | (2<<4);
		else
			val = S3C_CICOCTRL_YBURST1_CO(yburst_m/2) | S3C_CICOCTRL_YBURST2_CO(yburst_r/2) | (4<<9) | (2<<4);
		writel(val, S3C_CICOCTRL);
#else
		val = readl(S3C_CIPRCTRL);
		val = S3C_CICOCTRL_YBURST1_CO(yburst_m) | S3C_CICOCTRL_YBURST2_CO(yburst_r);
		writel(val, S3C_CIPRCTRL);
#endif
	}
	return 0;
}

static int camif_gpio_init(void)
{
	u32 val;

#if defined(CONFIG_CPU_S3C2443)
	s3c_gpio_cfgpin(S3C_GPJ0, S3C2440_GPJ0_CAMDATA0);
	s3c_gpio_cfgpin(S3C_GPJ1, S3C2440_GPJ1_CAMDATA1);
	s3c_gpio_cfgpin(S3C_GPJ2, S3C2440_GPJ2_CAMDATA2);
	s3c_gpio_cfgpin(S3C_GPJ3, S3C2440_GPJ3_CAMDATA3);
	s3c_gpio_cfgpin(S3C_GPJ4, S3C2440_GPJ4_CAMDATA4);
	s3c_gpio_cfgpin(S3C_GPJ5, S3C2440_GPJ5_CAMDATA5);
	s3c_gpio_cfgpin(S3C_GPJ6, S3C2440_GPJ6_CAMDATA6);
	s3c_gpio_cfgpin(S3C_GPJ7, S3C2440_GPJ7_CAMDATA7);
	s3c_gpio_cfgpin(S3C_GPJ8, S3C2440_GPJ8_CAMPCLK);
	s3c_gpio_cfgpin(S3C_GPJ9, S3C2440_GPJ9_CAMVSYNC);
	s3c_gpio_cfgpin(S3C_GPJ10, S3C2440_GPJ10_CAMHREF);
	s3c_gpio_cfgpin(S3C_GPJ11, S3C2440_GPJ11_CAMCLKOUT);
	s3c_gpio_cfgpin(S3C_GPJ12, S3C2440_GPJ12_CAMRESET);

	writel(0x1fff, S3C_GPJDN);

	#ifdef TOMTOM_INTERLACE_MODE
	s3c_gpio_cfgpin(S3C_GPF0, S3C_GPF0_EINT0);
	#endif
#elif (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
	s3c_gpio_cfgpin(S3C_GPF5, S3C_GPF5_CAMIF_YDATA0);
	s3c_gpio_cfgpin(S3C_GPF6, S3C_GPF6_CAMIF_YDATA1);
	s3c_gpio_cfgpin(S3C_GPF7, S3C_GPF7_CAMIF_YDATA2);
	s3c_gpio_cfgpin(S3C_GPF8, S3C_GPF8_CAMIF_YDATA03);
	s3c_gpio_cfgpin(S3C_GPF9, S3C_GPF9_CAMIF_YDATA4);
	s3c_gpio_cfgpin(S3C_GPF10, S3C_GPF10_CAMIF_YDATA5);
	s3c_gpio_cfgpin(S3C_GPF11, S3C_GPF11_CAMIF_YDATA06);
	s3c_gpio_cfgpin(S3C_GPF12, S3C_GPF12_CAMIF_YDATA7);
	s3c_gpio_cfgpin(S3C_GPF2, S3C_GPF2_CAMIF_CLK);
	s3c_gpio_cfgpin(S3C_GPF4, S3C_GPF4_CAMIF_VSYNC);
	s3c_gpio_cfgpin(S3C_GPF1, S3C_GPF1_CAMIF_HREF);
	s3c_gpio_cfgpin(S3C_GPF0, S3C_GPF0_CAMIF_CLK);
	s3c_gpio_cfgpin(S3C_GPF3, S3C_GPF3_CAMIF_RST);

	writel(0x0, S3C_GPFPU);
#endif
	return 0;
}


void clear_camif_irq(int irq) {
	
#if (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
	u32 val=0;
	
	if(irq == IRQ_CAMIF_C) {
		val = readl(S3C_CIGCTRL);
		val |= (1<<19);
	}
	else if (irq == IRQ_CAMIF_P) {
		val = readl(S3C_CIGCTRL);
		val |= (1<<18);
	}
	writel(val, S3C_CIGCTRL);
#endif
}

/* 
   Reset Camera IP in CPU
   Reset External Sensor 
 */
void camif_reset(int is, int delay)
{
	u32	val;
	u32 tmp;
	
	switch (is) {
		case CAMIF_RESET:
			tmp = readl(S3C_CISRCFMT);
			if(tmp &= (1<<31)){ // ITU-R BT 601
				val = readl(S3C_CIGCTRL);
			#if (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
				val |= S3C_CIGCTRL_SWRST | (1<<20);    /* For level interrupt */
			#else
				val |= S3C_CIGCTRL_SWRST | (1<<29);

			#endif
				writel(val, S3C_CIGCTRL);
				mdelay(1);
				val = readl(S3C_CIGCTRL);
				val &= ~S3C_CIGCTRL_SWRST;
				writel(val, S3C_CIGCTRL);
			}
			else{ // ITU-R BT 656
				tmp = readl(S3C_CISRCFMT);
				tmp |= (1<<31);
				writel(tmp, S3C_CISRCFMT);
				val = readl(S3C_CIGCTRL);
			#if (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
				val |= S3C_CIGCTRL_SWRST | (1<<20);    /* For level interrupt */
			#else
				val |= S3C_CIGCTRL_SWRST | (1<<29);

			#endif
				writel(val, S3C_CIGCTRL);
				mdelay(1);
				val = readl(S3C_CIGCTRL);
				val &= ~S3C_CIGCTRL_SWRST;
				writel(val, S3C_CIGCTRL);
				tmp = readl(S3C_CISRCFMT);
				tmp &= ~(1<<31);
				writel(tmp, S3C_CISRCFMT);
			}
			break;
			
		case CAMIF_EX_RESET_AH: /*Active High */
			val = readl(S3C_CIGCTRL);
			val |=  S3C_CIGCTRL_CAMRST;
			writel(val, S3C_CIGCTRL);
			udelay(200);
			val &= ~S3C_CIGCTRL_CAMRST;
			val = readl(S3C_CIGCTRL);
			writel(val, S3C_CIGCTRL);
			udelay(delay);
			val = readl(S3C_CIGCTRL);
			val |=  S3C_CIGCTRL_CAMRST;
			writel(val, S3C_CIGCTRL);	
			break;
			
		case CAMIF_EX_RESET_AL:	/*Active Low */
			val = readl(S3C_CIGCTRL);
			val &= ~S3C_CIGCTRL_CAMRST;
			writel(val, S3C_CIGCTRL);
			udelay(200);
			val = readl(S3C_CIGCTRL);
			val |=  S3C_CIGCTRL_CAMRST;
			writel(val, S3C_CIGCTRL);
			udelay(delay);
			val = readl(S3C_CIGCTRL);
			val &= ~S3C_CIGCTRL_CAMRST;
			writel(val, S3C_CIGCTRL);
			break;
			
		default:
			break;
	}
}
		
/* For Camera Operation,
 * we can give the high priority to REQ2 of ARBITER1 
 */

/* Please move me into proper place 
 *  camif_cis_t is not because "rmmod imgsenor" will delete the instance of camif_cis_t  
 */
static u32 irq_old_priority; 

static void camif_bus_priority(int flag)
{
	u32 val;
	
	if (flag) {	

		irq_old_priority = readl(S3C_PRIORITY);	
		val = irq_old_priority;	
		val &= ~(3<<7);
		writel(val, S3C_PRIORITY);
		val |=  (1<<7); /* Arbiter 1, REQ2 first */
		writel(val, S3C_PRIORITY);
		val &= ~(1<<1); /* Disable Priority Rotate */
		writel(val, S3C_PRIORITY);

	} 
	else {
		writel(irq_old_priority, S3C_PRIORITY);
	}

}


/* Init external image sensor 
 *  Before make some value into image senor,
 *  you must set up the pixel clock.
 */
void camif_setup_sensor(void)
{
	camif_reset(CAMIF_RESET, 0);
	camif_gpio_init();
	
#if defined(CONFIG_VIDEO_SAMSUNG_S5K3AA)
	s3c_camif_set_clock(48000000);
#elif (defined(CONFIG_VIDEO_OV9650) || defined(CONFIG_VIDEO_SAA7113))
#if defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410)
	s3c_camif_set_clock(24000000); 
#endif
#else
	s3c_camif_set_clock(19000000);
#endif

/* Sometimes ,Before loading I2C module, we need the reset signal */
	camif_reset(CAMIF_EX_RESET_AH,1000);

}

void camif_hw_close(camif_cfg_t *cfg)
{
	camif_bus_priority(0);
	
#if defined (CONFIG_CPU_S3C2443) || (defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410))
	s3c_camif_disable_clock();
#endif
}

void camif_hw_open(camif_cis_t *cis)
{
	camif_source_fmt(cis);
	camif_win_offset(cis);
	camif_bus_priority(1);
}

