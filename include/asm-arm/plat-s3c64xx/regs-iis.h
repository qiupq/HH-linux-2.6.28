/* linux/include/asm-arm/arch-s3c64XX/regs-iis.h
 *
 * Copyright (c) 2003 Simtec Electronics <linux@simtec.co.uk>
 *		      http://www.simtec.co.uk/products/SWLINUX/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * S3C64XX IIS register definition
*/

#ifndef __ASM_ARCH_REGS_IIS_H
#define __ASM_ARCH_REGS_IIS_H

#define S3C64XX_IIS0REG(x)     ((x) + S3C6410_PA_IIS_V40)

//#define S3C_IIS0CON             S3C64XX_IIS0REG(0x00)
//#define S3C_IIS0MOD             S3C64XX_IIS0REG(0x04)
//#define S3C_IIS0FIC             S3C64XX_IIS0REG(0x08)
//#define S3C_IIS0PSR             S3C64XX_IIS0REG(0x0C)
//#define S3C_IIS0TXD             S3C64XX_IIS0REG(0x10)
//#define S3C_IIS0RXD             S3C64XX_IIS0REG(0x14)

#define S3C64XX_IIS0CON             (0x00)
#define S3C64XX_IIS0MOD             (0x04)
#define S3C64XX_IIS0FIC             (0x08)
#define S3C64XX_IIS0PSR             (0x0C)
#define S3C64XX_IIS0TXD             (0x10)
#define S3C64XX_IIS0RXD             (0x14)

#define S3C64XX_IISCON_LRINDEX	  (1<<8)
#define S3C64XX_IISCON_TXFIFORDY  (1<<7)
#define S3C64XX_IISCON_RXFIFORDY  (1<<6)
#define S3C64XX_IISCON_TXDMAEN	  (1<<5)
#define S3C64XX_IISCON_RXDMAEN	  (1<<4)
#define S3C64XX_IISCON_TXIDLE	  (1<<3)
#define S3C64XX_IISCON_RXIDLE	  (1<<2)
#define S3C64XX_IISCON_PSCEN	  (1<<1)
#define S3C64XX_IISCON_IISEN	  (1<<0)

//#define S3C64XX_IISMOD_MPLL	  (1<<9)
#define S3C64XX_IISMOD_MPLL	  (0x01<<10)
#define S3C64XX_IISMOD_SLAVE	  (1<<8)
#define S3C64XX_IISMOD_NOXFER	  (0<<6)
#define S3C64XX_IISMOD_RXMODE	  (1<<6)
#define S3C64XX_IISMOD_TXMODE	  (2<<6)
#define S3C64XX_IISMOD_TXRXMODE	  (3<<6)
#define S3C64XX_IISMOD_LR_LLOW	  (0<<5)
#define S3C64XX_IISMOD_LR_RLOW	  (1<<5)
#define S3C64XX_IISMOD_IIS	  (0<<4)
#define S3C64XX_IISMOD_MSB	  (1<<4)
#define S3C64XX_IISMOD_8BIT	  (0<<3)
#define S3C64XX_IISMOD_16BIT	  (1<<3)
#define S3C64XX_IISMOD_BITMASK	  (1<<3)
#define S3C64XX_IISMOD_256FS	  (0<<2)
#define S3C64XX_IISMOD_384FS	  (1<<2)
#define S3C64XX_IISMOD_16FS	  (0<<0)
#define S3C64XX_IISMOD_32FS	  (1<<0)
#define S3C64XX_IISMOD_48FS	  (2<<0)
#define S3C64XX_IISMOD_FS_MASK	  (3<<0)

#define S3C64XX_IIS0MOD_DCE_MASK            (0x3<<16)
#define S3C64XX_IIS0MOD_DCE_SD2             (0x1<<17)
#define S3C64XX_IIS0MOD_DCE_SD1             (0x1<<16)
#define S3C64XX_IIS0MOD_BLC_MASK            (0x3<<13)
#define S3C64XX_IIS0MOD_BLC_16BIT           (0x0<<13)
#define S3C64XX_IIS0MOD_BLC_08BIT           (0x1<<13)
#define S3C64XX_IIS0MOD_BLC_24BIT           (0x2<<13)
#define S3C64XX_IIS0MOD_CLK_MASK            (0x7<<10)
#define S3C64XX_IIS0MOD_INTERNAL_CLK        (0x0<<12)
#define S3C64XX_IIS0MOD_EXTERNAL_CLK        (0x1<<12)
#define S3C64XX_IIS0MOD_IMS_INTERNAL_MASTER (0x0<<10)
#define S3C64XX_IIS0MOD_IMS_EXTERNAL_MASTER (0x1<<10)
#define S3C64XX_IIS0MOD_IMS_SLAVE           (0x2<<10)
#define S3C64XX_IIS0MOD_MODE_MASK           (0x3<<8)
#define S3C64XX_IIS0MOD_TXMODE              (0x0<<8)
#define S3C64XX_IIS0MOD_RXMODE              (0x1<<8)
#define S3C64XX_IIS0MOD_TXRXMODE            (0x2<<8)
#define S3C64XX_IIS0MOD_FM_MASK             (0x3<<5)
#define S3C64XX_IIS0MOD_IIS                 (0x0<<5)
#define S3C64XX_IIS0MOD_MSB                 (0x1<<5)
#define S3C64XX_IIS0MOD_LSB                 (0x2<<5)
#define S3C64XX_IIS0MOD_FS_MASK             (0x3<<3)
#define S3C64XX_IIS0MOD_768FS               (0x3<<3)
#define S3C64XX_IIS0MOD_384FS               (0x2<<3)
#define S3C64XX_IIS0MOD_512FS               (0x1<<3)
#define S3C64XX_IIS0MOD_256FS               (0x0<<3)
#define S3C64XX_IIS0MOD_BFS_MASK            (0x3<<1)
#define S3C64XX_IIS0MOD_48FS                (0x1<<1)
#define S3C64XX_IIS0MOD_32FS                (0x0<<1)

#define S3C64XX_IISPSR		(0x08)
#define S3C64XX_IISPSR_INTMASK	(31<<5)
#define S3C64XX_IISPSR_INTSHIFT	(5)
#define S3C64XX_IISPSR_EXTMASK	(31<<0)
#define S3C64XX_IISPSR_EXTSHFIT	(0)

#define S3C64XX_IISFCON  (0x0c)

#define S3C64XX_IISFCON_TXDMA	  (1<<15)
#define S3C64XX_IISFCON_RXDMA	  (1<<14)
#define S3C64XX_IISFCON_TXENABLE  (1<<13)
#define S3C64XX_IISFCON_RXENABLE  (1<<12)
#define S3C64XX_IISFCON_TXMASK	  (0x3f << 6)
#define S3C64XX_IISFCON_TXSHIFT	  (6)
#define S3C64XX_IISFCON_RXMASK	  (0x3f)
#define S3C64XX_IISFCON_RXSHIFT	  (0)

#define S3C64XX_IISFIFO 	(0x10)
#define S3C64XX_IISFIFORX  	(0x14)

#define S3C64XX_IIS0CON_I2SACTIVE           (0x1<<0)
#define S3C64XX_IIS0CON_RXDMACTIVE          (0x1<<1)
#define S3C64XX_IIS0CON_I2SACTIVE           (0x1<<0)
#define S3C64XX_IIS0CON_TXDMACTIVE          (0x1<<2)

#define S3C64XX_IIS_TX_FLUSH        (0x1<<15)
#define S3C64XX_IIS_RX_FLUSH        (0x1<<7)

#define S3C64XX_IISCON_FTXURINTEN           (0x1<<16)

#define S3C64XX_IIS0MOD_24BIT               (0x2<<13)
#define S3C64XX_IIS0MOD_8BIT                (0x1<<13)
#define S3C64XX_IIS0MOD_16BIT               (0x0<<13)
#endif /* __ASM_ARCH_REGS_IIS_H */
