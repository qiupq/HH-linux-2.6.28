/* linux/include/asm-arm/arch-s3c2410/map.h
 *
 * Copyright (c) 2003 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C2410 - Memory map definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_MAP_H
#define __ASM_ARCH_MAP_H

#include <asm/plat-s3c/map.h>

/* interrupt controller is the first thing we put in, to make
 * the assembly code for the irq detection easier
 */
#define S3C24XX_VA_IRQ	   S3C_VA_IRQ
#define S3C2410_PA_IRQ	   (0x4A000000)
#define S3C6400_PA_IRQ	   (0x71200000)
#define S5PC100_PA_IRQ		(0xE4000000)

#if defined(CONFIG_PLAT_S5PC1XX)
#define S3C24XX_SZ_IRQ		SZ_4M
#else
#define S3C24XX_SZ_IRQ	   SZ_2M
#endif

/* memory controller registers */
#define S3C24XX_VA_MEMCTRL S3C_VA_MEM
#define S3C2410_PA_MEMCTRL (0x48000000)
#define S3C24XX_SZ_MEMCTRL SZ_8K

/* USB host controller */
#define S3C2410_PA_USBHOST (0x49000000)
#define S3C6400_PA_USBHOST (0x74300000)
#define S5PC100_PA_USBHOST	(0xED400000)
#define S3C24XX_SZ_USBHOST SZ_1M

/* DMA controller */
#define S3C2410_PA_DMA	   (0x4B000000)
#define S3C6400_PA_DMA	   (0x75000000)
#define S5PC100_PA_DMA		(0xE8000000)

#if defined(CONFIG_PLAT_S5PC1XX)
#define S3C24XX_SZ_DMA		SZ_8M
#else
#define S3C24XX_SZ_DMA	   SZ_2M
#endif

/* Clock and Power management */
#define S3C24XX_VA_CLKPWR  S3C_VA_SYS
#define S3C2410_PA_CLKPWR  (0x4C000000)
#define S3C24XX_SZ_CLKPWR  SZ_1M

/* LCD controller */
#define S3C24XX_VA_LCD	   S3C_VA_LCD
#define S3C2410_PA_LCD	   (0x4D000000)
#define S3C2443_PA_LCD	   (0x4C800000)
#define S3C6400_PA_LCD	   (0x77100000)
#define S5PC100_PA_LCD		(0xEE000000)
#define S3C24XX_SZ_LCD	   SZ_1M

/* NAND flash controller */
#define S3C2410_PA_NAND	   (0x4E000000)
#define S3C6400_PA_NAND	   (0x70200000)
#define S5PC100_PA_NAND		(0xE7200000)
#define S3C24XX_SZ_NAND	   SZ_1M

/* UARTs */
#define S3C24XX_VA_UART	   S3C_VA_UART
#define S3C2410_PA_UART	   (0x50000000)
#define S3C6400_PA_UART	   (0x7F005000)
#define S5PC100_PA_UART		(0xEC000000)

#if defined(CONFIG_PLAT_S3C64XX) || defined(CONFIG_PLAT_S5PC1XX)
#define S3C24XX_SZ_UART		SZ_4K
#else
#define S3C24XX_SZ_UART	   SZ_1M
#endif

/* PWM Timers */
#define S3C24XX_VA_TIMER   S3C_VA_TIMER
#define S3C2410_PA_TIMER   (0x51000000)
#define S3C6400_PA_TIMER   (0x7F006000)
#define S5PC100_PA_TIMER	(0xEA000000)
#define S3C24XX_SZ_TIMER   SZ_4K

/* USB Device port */
#define S3C24XX_VA_USBDEV  S3C_VA_USBDEV
#define S3C2410_PA_USBDEV  (0x52000000)
#define S3C2443_PA_USBDEV  (0x49800000)
#define S3C24XX_SZ_USBDEV  SZ_1M

/* Watchdog */
#define S3C24XX_VA_WATCHDOG S3C_VA_WATCHDOG
#define S3C2410_PA_WATCHDOG (0x53000000)
#define S3C6400_PA_WATCHDOG (0x7E004000)
#define S5PC100_PA_WATCHDOG	(0xEA200000)
#define S3C24XX_SZ_WATCHDOG SZ_4K

/* IIC hardware controller */
#define S3C2410_PA_IIC	   (0x54000000)
#define S3C6400_PA_IIC0	   (0x7F004000)
#define S3C6400_PA_IIC1	   (0x7F00F000)
#define S5PC100_PA_IIC		(0xEC100000)
#define S3C24XX_SZ_IIC	   SZ_4K

/* IIS controller */
#define S3C2410_PA_IIS	   (0x55000000)
#define S3C2450_PA_IIS     (0x55000000)
#define S3C2450_PA_IIS1    (0x55000100)
#define S3C6400_PA_IIS	   (0x7F002000)
#define S3C6410_PA_IIS_V40 (0x7F00D000)
#define S5PC100_PA_IIS		(0xF2000000)

#if defined(CONFIG_PLAT_S5PC1XX)
#define S3C24XX_SZ_IIS		SZ_1M
#else
#define S3C24XX_SZ_IIS	   SZ_8K
#endif

/* GPIO ports */

/* the calculation for the VA of this must ensure that
 * it is the same distance apart from the UART in the
 * phsyical address space, as the initial mapping for the IO
 * is done as a 1:1 maping. This puts it (currently) at
 * 0xFA800000, which is not in the way of any current mapping
 * by the base system.
*/
#if defined(CONFIG_PLAT_S3C64XX) 
#define S3C24XX_VA_GPIO		S3C_VA_GPIO
#define S3C6400_PA_GPIO		(0x7F008000)
#define S3C24XX_SZ_GPIO		SZ_4K
#elif defined(CONFIG_PLAT_S5PC1XX)
#define S3C24XX_VA_GPIO		S3C_VA_GPIO
#define S5PC100_PA_GPIO		(0xE0300000)
#define S3C24XX_SZ_GPIO		SZ_1M
#else
#define S3C2410_PA_GPIO		(0x56000000)
#define S3C24XX_VA_GPIO		((S3C2410_PA_GPIO - S3C24XX_PA_UART) + S3C24XX_VA_UART)
#define S3C24XX_SZ_GPIO		SZ_1M
#endif

/* RTC */
#define S3C2410_PA_RTC	   (0x57000000)
#define S3C2450_PA_RTC	   (0x57005000)
#define S3C6400_PA_RTC	   (0x7E005000)
#define S5PC100_PA_RTC		(0xEA300000)

#if defined(CONFIG_PLAT_S5PC1XX)
#define S3C24XX_SZ_RTC		SZ_1M
#else
#define S3C24XX_SZ_RTC	   SZ_4K
#endif

/* ADC */
#define S3C2410_PA_ADC	   (0x58000000)
#define S3C6400_PA_ADC	   (0x7E00B000)
#define S5PC100_PA_ADC		(0xF3000000)

#if defined(CONFIG_PLAT_S5PC1XX)
#define S3C24XX_SZ_ADC		SZ_1M
#else
#define S3C24XX_SZ_ADC	   SZ_4K
#endif

/* SPI */
#define S3C2410_PA_SPI	   (0x59000000)
#define S3C6400_PA_SPI	   (0x7F00B000)
#define S5PC100_PA_SPI		(0xEC300000)

#if defined(CONFIG_PLAT_S5PC1XX)
#define S3C24XX_SZ_SPI		SZ_1M
#else
#define S3C24XX_SZ_SPI	   SZ_8K
#endif

/* SDI */
#define S3C2410_PA_SDI	   (0x5A000000)
#define S3C24XX_SZ_SDI	   SZ_1M

/* CAMIF */
#define S3C24XX_VA_CAMIF   S3C_VA_CAMIF
#define S3C2440_PA_CAMIF   (0x4F000000)
#define S3C2443_PA_CAMIF   (0x4D800000)
#define S3C6400_PA_CAMIF   (0x78000000)
#define S5PC100_PA_CAMIF	(0xEE200000)
#define S3C2440_SZ_CAMIF   SZ_1M
#define S3C2443_SZ_CAMIF   SZ_1M
#define S3C24XX_SZ_CAMIF   SZ_1M

/* AC97 */
#define S3C2440_PA_AC97	   (0x5B000000)
#define S3C6400_PA_AC97	   (0x7F001000)
#define S5PC100_PA_AC97		(0xF2300000)
#define S3C2440_SZ_AC97	   SZ_1M

#if defined(CONFIG_PLAT_S5PC1XX)
#define S3C24XX_SZ_AC97		SZ_1M
#else
#define S3C24XX_SZ_AC97	   SZ_4K
#endif

/* Host I/F Indirect & Direct */
#define S3C24XX_VA_HOSTIFA	S3C_VA_HOSTIFA
#define S3C24XX_PA_HOSTIFA	(0x74000000)
#define S3C24XX_SZ_HOSTIFA	SZ_1M

#define S3C24XX_VA_HOSTIFB	S3C_VA_HOSTIFB
#define S3C24XX_PA_HOSTIFB	(0x74100000)
#define S3C24XX_SZ_HOSTIFB	SZ_1M

/* CHIP_ID */
#define S3C24XX_VA_CHIP_ID	S3C_VA_CHIP_ID
#define S3C64XX_PA_CHIP_ID	(0x7E00E000)
#define S5PC100_PA_CHIP_ID	(0xE0000000)
#define S3C24XX_SZ_CHIP_ID	SZ_4K

/* System Controller  */
#define S3C24XX_VA_SYSCON	S3C_VA_SYSCON
#if defined(CONFIG_PLAT_S5PC1XX)
#define S3C24XX_PA_SYSCON	(0xE0100000)
#define S3C24XX_SZ_SYSCON	SZ_2M
#else
#define S3C24XX_PA_SYSCON	(0x7E00F000)
#define S3C24XX_SZ_SYSCON	SZ_4K
#endif

/* AXI */
#define S3C6410_PA_AXI_SYS	(0x7E003000)
#define S3C6410_PA_AXI_PERI	(0x7E008000)
#define S3C6410_PA_AXI_SFR	(0x7E009000)

/* Ethernet  */
#define S3C24XX_VA_CS8900	S3C_VA_CS8900
#define S3C2443_PA_CS8900	(0x09000000)
#define S3C6400_PA_CS8900	(0x18800000)
#define S3C24XX_SZ_CS8900	SZ_1M

#define S3C6410_PA_SMC9115	(0x18000000)
#define S3C2450_PA_SMC9115	(0x20000000)

/* SROM SFR */
#define S3C24XX_VA_SROMC	S3C_VA_SROMC
#define S3C2443_PA_SROMC	(0x4F000000)
#define S3C6400_PA_SROMC	(0x70000000)
#define S5PC100_PA_SROMC	(0xE7000000)
#define S3C24XX_SZ_SROMC	SZ_1M

/* S3C2443 High-speed SD/MMC */
#define S3C2443_PA_HSMMC   	(0x4A800000)
#define S3C6400_PA_HSMMC   	(0x7C200000)
#define S5PC100_PA_HSMMC	(0xED800000)
#define S3C2443_SZ_HSMMC   	SZ_1M
#define S3C_SZ_HSMMC	   	SZ_1M

/* MMC controller - available on the S3C2400 */
#define S3C2400_PA_MMC 	   	(0x15A00000)
#define S3C2400_SZ_MMC 	   	SZ_1M

/* CF/ATA controller - available on the S3C2443/S3C6400 */
#define S3C2443_PA_CFATA   	(0x4B800000)
#define S3C6400_PA_CFATA   	(0x70300000)
#define S5PC100_PA_CFATA	(0xE7800000)
#define S3C_SZ_CFATA	   	SZ_1M

/* DRAM controller 0  */
#define S3C24XX_VA_DMC0		S3C_VA_DMC0

#if defined(CONFIG_PLAT_S3C64XX)
#define S3C24XX_PA_DMC0		(0x7E000000)
#elif defined(CONFIG_PLAT_S5PC1XX)
#define S3C24XX_PA_DMC0		(0xE6000000)
#endif

#define S3C24XX_SZ_DMC0		SZ_4K

#if !defined(CONFIG_PLAT_S5PC1XX)
/* DRAM controller 1  */
#define S3C24XX_VA_DMC1		S3C_VA_DMC1
#define S3C24XX_PA_DMC1		(0x7E001000)
#define S3C24XX_SZ_DMC1		SZ_4K
#endif

#if defined(CONFIG_CPU_S3C6400)  || defined(CONFIG_CPU_S3C6410)
/* USB OTG */
#define S3C24XX_VA_OTG		S3C_VA_OTG
#define S3C24XX_PA_OTG		(0x7C000000)
#define S3C24XX_SZ_OTG		SZ_1M

/* USB OTG SFR */
#define S3C24XX_VA_OTGSFR	S3C_VA_OTGSFR
#define S3C24XX_PA_OTGSFR	(0x7C100000)
#define S3C24XX_SZ_OTGSFR	SZ_1M
#endif

/* 2D Graphics */
#define S3C6400_PA_G2D		(0x76100000)
#define S3C_SZ_G2D		SZ_1M

#define S3C6400_PA_ROTATOR	(0x77200000)
#define S3C_SZ_ROTATOR		SZ_1M

#if defined(CONFIG_CPU_S3C6410)
/* G3D */
#define S3C6410_PA_G3D 		(0x72000000)
#define S3C6410_SZ_G3D 		SZ_4K
#endif

/* TV-ENCODER */
#define S3C6400_PA_TVENC	(0x76200000)
#define S5PC100_PA_TVENC	(0xF0000000)
#define S3C_SZ_TVENC		SZ_1M

/* TV-SCALER*/
#define S3C6400_PA_TVSCALER	(0x76300000)
#define S3C_SZ_TVSCALER		SZ_1M

/* VPP */
#define S3C6400_PA_VPP		(0x77000000)
#define S5PC100_PA_VPP		(0xF0100000)
#define S3C_SZ_VPP		SZ_1M

/* MFC */
#define S3C6400_PA_MFC		(0x7E002000)
#define S5PC100_PA_MFC		(0xF1000000)
#define S3C_SZ_MFC		SZ_4K

/* JPEG */
#define S3C6400_PA_JPEG		(0x78800000)
#define S5PC100_PA_JPEG		(0xEE500000)

#if defined(CONFIG_PLAT_S5PC1XX)
#define S3C_SZ_JPEG		SZ_1M
#else
#define S3C_SZ_JPEG		SZ_4M
#endif

/* OneNAND */
#define S3C6400_PA_ONENAND	(0x70100000)
#define S5PC100_PA_ONENAND	(0xE7100000)
#define	S3C_SZ_ONENAND		SZ_1M

/* Keypad IF  */
#define S3C6400_PA_KEYPAD	(0x7E00A000)
#define S5PC100_PA_KEYPAD	(0xF3100000)
#define S3C24XX_SZ_KEYPAD	SZ_4K

/* EBI registers for 2443 */
#define S3C24XX_VA_EBI		S3C_VA_EBI
#define S3C24XX_PA_EBI		(0x4E800000)
#define S3C24XX_SZ_EBI		SZ_1M

/* ISA style IO, for each machine to sort out mappings for, if it
 * implements it. We reserve two 16M regions for ISA.
 */

#define S3C24XX_VA_ISA_WORD  S3C_ADDR(0x02000000)
#define S3C24XX_VA_ISA_BYTE  S3C_ADDR(0x03000000)

/* physical addresses of all the chip-select areas */

#define S3C2410_CS0 	(0x00000000)
#define S3C2410_CS1 	(0x08000000)
#define S3C2410_CS2 	(0x10000000)
#define S3C2410_CS3 	(0x18000000)
#define S3C2410_CS4 	(0x20000000)
#define S3C2410_CS5 	(0x28000000)
#define S3C2410_CS6 	(0x30000000)
#define S3C2410_CS7 	(0x38000000)

#define S3C2410_SDRAM_PA    (S3C2410_CS6)
#define S3C_SDRAM_PA        (0x50000000)
#define S5PC1XX_SDRAM_PA	(0x20000000)


/* Use a single interface for common resources between S3C24XX cpus */
#if defined(CONFIG_PLAT_S3C64XX)
#define S3C24XX_PA_ADC		S3C6400_PA_ADC
#define S3C24XX_PA_CAMIF	S3C6400_PA_CAMIF
#define S3C24XX_PA_DMA		S3C6400_PA_DMA
#define S3C24XX_PA_IRQ		S3C6400_PA_IRQ
#define S3C24XX_PA_LCD		S3C6400_PA_LCD
#define S3C24XX_PA_UART		S3C6400_PA_UART
#define S3C24XX_PA_USBHOST	S3C6400_PA_USBHOST
#define S3C24XX_PA_WATCHDOG	S3C6400_PA_WATCHDOG
#define S3C24XX_PA_IIS		S3C6400_PA_IIS
#define S3C24XX_PA_RTC		S3C6400_PA_RTC
#define S3C24XX_PA_SPI		S3C6400_PA_SPI
#define S3C24XX_PA_TIMER	S3C6400_PA_TIMER
#define S3C24XX_PA_NAND		S3C6400_PA_NAND
#define S3C24XX_PA_ONENAND	S3C6400_PA_ONENAND
#define S3C24XX_PA_GPIO		S3C6400_PA_GPIO
#define S3C24XX_PA_CS8900	S3C6400_PA_CS8900
#define S3C24XX_PA_TVENC	S3C6400_PA_TVENC
#define S3C24XX_PA_TVSCALER	S3C6400_PA_TVSCALER
#define S3C24XX_PA_CFATA	S3C6400_PA_CFATA
#define S3C24XX_PA_AC97		S3C6400_PA_AC97
#define S3C24XX_PA_KEYPAD	S3C6400_PA_KEYPAD
#define S3C24XX_PA_SROMC	S3C6400_PA_SROMC
#define S3C_PA_SMC9115		S3C6410_PA_SMC9115
#define S3C_PA_HSMMC	    	S3C6400_PA_HSMMC
#define S3C_PA_CFATA	    	S3C6400_PA_CFATA
#elif defined(CONFIG_PLAT_S5PC1XX)
#define S3C24XX_PA_ADC		S5PC100_PA_ADC
#define S3C24XX_PA_CAMIF	S5PC100_PA_CAMIF
#define S3C24XX_PA_DMA		S5PC100_PA_DMA
#define S3C24XX_PA_IRQ		S5PC100_PA_IRQ
#define S3C24XX_PA_LCD		S5PC100_PA_LCD
#define S3C24XX_PA_UART		S5PC100_PA_UART
#define S3C24XX_PA_USBHOST	S5PC100_PA_USBHOST
#define S3C24XX_PA_WATCHDOG	S5PC100_PA_WATCHDOG
#define S3C24XX_PA_IIS		S5PC100_PA_IIS
#define S3C24XX_PA_IIC		S5PC100_PA_IIC
#define S3C24XX_PA_RTC		S5PC100_PA_RTC
#define S3C24XX_PA_SPI		S5PC100_PA_SPI
#define S3C24XX_PA_TIMER	S5PC100_PA_TIMER
#define S3C24XX_PA_NAND		S5PC100_PA_NAND
#define S3C24XX_PA_ONENAND	S5PC100_PA_ONENAND
#define S3C24XX_PA_GPIO		S5PC100_PA_GPIO
#define S3C24XX_PA_CS8900	S5PC100_PA_CS8900
#define S3C24XX_PA_TVENC	S5PC100_PA_TVENC
#define S3C24XX_PA_TVSCALER	S5PC100_PA_TVSCALER
#define S3C24XX_PA_CFATA	S5PC100_PA_CFATA
#define S3C24XX_PA_AC97		S5PC100_PA_AC97
#define S3C24XX_PA_KEYPAD	S5PC100_PA_KEYPAD
#define S3C24XX_PA_SROMC	S5PC100_PA_SROMC
#define S3C_PA_SMC9115		S3C6410_PA_SMC9115
#define S3C_PA_HSMMC	    	S5PC100_PA_HSMMC
#define S3C_PA_CFATA	    	S5PC100_PA_CFATA
#define S3C24XX_PA_CHIP_ID	S5PC100_PA_CHIP_ID	
#else
#define S3C24XX_PA_IRQ      	S3C2410_PA_IRQ
#define S3C24XX_PA_MEMCTRL 	S3C2410_PA_MEMCTRL
#define S3C24XX_PA_USBHOST  	S3C2410_PA_USBHOST
#define S3C24XX_PA_DMA      	S3C2410_PA_DMA
#define S3C24XX_PA_CLKPWR   	S3C2410_PA_CLKPWR
#if defined(CONFIG_CPU_S3C2443) || defined(CONFIG_CPU_S3C2450) || defined(CONFIG_CPU_S3C2416)
#define S3C24XX_PA_LCD      	S3C2443_PA_LCD
#define S3C24XX_PA_CS8900   	S3C2443_PA_CS8900
#define S3C24XX_PA_USBDEV   	S3C2443_PA_USBDEV
#define S3C24XX_PA_CAMIF    	S3C2443_PA_CAMIF
#define S3C_PA_HSMMC	    	S3C2443_PA_HSMMC
#define S3C_PA_CFATA	    	S3C2443_PA_CFATA
#define S3C24XX_PA_SROMC    	S3C2443_PA_SROMC
#define S3C_PA_SPI_0	        (0x52000000)
#define S3C_SZ_SPI_0	        SZ_1M
#define S3C_PA_SMC9115      	S3C2450_PA_SMC9115
#else
#define S3C24XX_PA_LCD      	S3C2410_PA_LCD
#define S3C24XX_PA_USBDEV   	S3C2410_PA_USBDEV
#endif
#define S3C24XX_PA_UART     	S3C2410_PA_UART
#define S3C24XX_PA_TIMER    	S3C2410_PA_TIMER
#define S3C24XX_PA_WATCHDOG 	S3C2410_PA_WATCHDOG
#define S3C24XX_PA_IIC      	S3C2410_PA_IIC
#define S3C24XX_PA_IIS      	S3C2410_PA_IIS
#define S3C24XX_PA_GPIO     	S3C2410_PA_GPIO
#define S3C24XX_PA_RTC      	S3C2410_PA_RTC
#define S3C24XX_PA_ADC      	S3C2410_PA_ADC
#define S3C24XX_PA_SPI      	S3C2410_PA_SPI
#define S3C24XX_PA_NAND	    	S3C2410_PA_NAND
#define S3C24XX_PA_AC97	    	S3C2440_PA_AC97
#endif

/* deal with the registers that move under the 2412/2413 */

#if defined(CONFIG_CPU_S3C2412) || defined(CONFIG_CPU_S3C2413)
#ifndef __ASSEMBLY__
extern void __iomem *s3c24xx_va_gpio2;
#endif
#ifdef CONFIG_CPU_S3C2412_ONLY
#define S3C24XX_VA_GPIO2 (S3C24XX_VA_GPIO + 0x10)
#else
#define S3C24XX_VA_GPIO2 s3c24xx_va_gpio2
#endif
#else
#define s3c24xx_va_gpio2 S3C24XX_VA_GPIO
#define S3C24XX_VA_GPIO2 S3C24XX_VA_GPIO
#endif

#endif /* __ASM_ARCH_MAP_H */