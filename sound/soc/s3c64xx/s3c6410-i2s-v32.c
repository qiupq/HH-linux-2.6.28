/*
 * s3c-i2s.c  --  ALSA Soc Audio Layer
 *
 * (c) 2006 Wolfson Microelectronics PLC.
 * Graeme Gregory graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 * (c) 2004-2005 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *	Ryu Euiyoul <ryu.real@gmail.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *
 *  Revision history
 *    11th Dec 2006   Merged with Simtec driver
 *    10th Nov 2006   Initial version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <asm-arm/plat-s3c64xx/regs-iis.h>
//#include <asm/arch/regs-iis.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/audio.h>
#include <asm/arch/dma.h>
#include <asm/arch/regs-s3c-clock.h>

#include "../s3c24xx/s3c-pcm.h"
#include "../s3c24xx/s3c-i2s.h"

#ifdef CONFIG_SND_DEBUG
#define s3cdbg(x...) printk(x)
#else
#define s3cdbg(x...)
#endif

/* used to disable sysclk if external crystal is used */
static int extclk = 0;
module_param(extclk, int, 0);
MODULE_PARM_DESC(extclk, "set to 1 to disable s3c24XX i2s sysclk");

static struct s3c2410_dma_client s3c24xx_dma_client_out = {
	.name = "I2S PCM Stereo out"
};

static struct s3c2410_dma_client s3c24xx_dma_client_in = {
	.name = "I2S PCM Stereo in"
};

static struct s3c24xx_pcm_dma_params s3c24xx_i2s_pcm_stereo_out = {
	.client		= &s3c24xx_dma_client_out,
	.channel	= DMACH_I2S_OUT,
	.dma_addr	= S3C6400_PA_IIS + S3C64XX_IISFIFO,
	.dma_size	= 4,
};

static struct s3c24xx_pcm_dma_params s3c24xx_i2s_pcm_stereo_in = {
	.client		= &s3c24xx_dma_client_in,
	.channel	= DMACH_I2S_IN,
	.dma_addr	= S3C6400_PA_IIS + S3C64XX_IISFIFORX,
	.dma_size	= 4,
};

struct s3c24xx_i2s_info {
	void __iomem	*regs;
	struct clk	*iis_clk;
	int master;
};
static struct s3c24xx_i2s_info s3c24xx_i2s;

static void s3c24xx_snd_txctrl(int on)
{
	u32 iisfcon;
	u32 iiscon;
	u32 iismod;

	s3cdbg("Entered %s : on = %d \n", __FUNCTION__, on);

	iiscon  = readl(s3c24xx_i2s.regs + S3C64XX_IIS0CON);
	iismod  = readl(s3c24xx_i2s.regs + S3C64XX_IIS0MOD);
	iisfcon = readl(s3c24xx_i2s.regs + S3C64XX_IISFCON);

	s3cdbg("r: IISCON: %x IISMOD: %x IISFCON: %x\n", iiscon, iismod, iisfcon);

	if (on) {
		iiscon |= S3C64XX_IIS0CON_I2SACTIVE;
		writel(iismod,  s3c24xx_i2s.regs + S3C64XX_IIS0MOD);
		writel(iisfcon, s3c24xx_i2s.regs + S3C64XX_IISFCON);
		writel(iiscon,  s3c24xx_i2s.regs + S3C64XX_IIS0CON);
	} else {
		/* note, we have to disable the FIFOs otherwise bad things
		 * seem to happen when the DMA stops. According to the
		 * Samsung supplied kernel, this should allow the DMA
		 * engine and FIFOs to reset. If this isn't allowed, the
		 * DMA engine will simply freeze randomly.
		 */

		iiscon &=~(S3C64XX_IIS0CON_I2SACTIVE);
		iismod &= ~S3C64XX_IIS0MOD_TXMODE;

		writel(iiscon,  s3c24xx_i2s.regs + S3C64XX_IIS0CON);
		writel(iisfcon, s3c24xx_i2s.regs + S3C64XX_IISFCON);
		writel(iismod,  s3c24xx_i2s.regs + S3C64XX_IIS0MOD);
	}

	s3cdbg("w: IISCON: %x IISMOD: %x IISFCON: %x\n", iiscon, iismod, iisfcon);
}

static void s3c24xx_snd_rxctrl(int on)
{
	u32 iisfcon;
	u32 iiscon;
	u32 iismod;

	s3cdbg("Entered %s: on = %d\n", __FUNCTION__, on);

	iisfcon = readl(s3c24xx_i2s.regs + S3C64XX_IISFCON);
	iiscon  = readl(s3c24xx_i2s.regs + S3C64XX_IIS0CON);
	iismod  = readl(s3c24xx_i2s.regs + S3C64XX_IIS0MOD);

	s3cdbg("r: IISCON: %x IISMOD: %x IISFCON: %x\n", iiscon, iismod, iisfcon);

	if (on) {
		
		iiscon |= S3C64XX_IIS0CON_I2SACTIVE;

		writel(iismod,  s3c24xx_i2s.regs + S3C64XX_IIS0MOD);
		writel(iisfcon, s3c24xx_i2s.regs + S3C64XX_IISFCON);
		writel(iiscon,  s3c24xx_i2s.regs + S3C64XX_IIS0CON);
	} else {
		/* note, we have to disable the FIFOs otherwise bad things
		 * seem to happen when the DMA stops. According to the
		 * Samsung supplied kernel, this should allow the DMA
		 * engine and FIFOs to reset. If this isn't allowed, the
		 * DMA engine will simply freeze randomly.
		 */

#if !defined (CONFIG_CPU_S3C6400) && !defined (CONFIG_CPU_S3C6410)
		iisfcon &= ~S3C2410_IISFCON_RXENABLE;
		iisfcon &= ~S3C2410_IISFCON_RXDMA;
		iiscon  |= S3C2410_IISCON_RXIDLE;
		iiscon  &= ~S3C2410_IISCON_RXDMAEN;
		iismod  &= ~S3C2410_IISMOD_RXMODE;
#else
		iiscon &=~ S3C64XX_IIS0CON_I2SACTIVE;
		iismod &= ~S3C64XX_IIS0MOD_RXMODE;
#endif
		writel(iisfcon, s3c24xx_i2s.regs + S3C64XX_IISFCON);
		writel(iiscon,  s3c24xx_i2s.regs + S3C64XX_IIS0CON);
		writel(iismod,  s3c24xx_i2s.regs + S3C64XX_IIS0MOD);

	}
	s3cdbg("w: IISCON: %x IISMOD: %x IISFCON: %x\n", iiscon, iismod, iisfcon);
}

/*
 * Wait for the LR signal to allow synchronisation to the L/R clock
 * from the codec. May only be needed for slave mode.
 */
static int s3c24xx_snd_lrsync(void)
{
	u32 iiscon;
	unsigned long timeout = jiffies + msecs_to_jiffies(5);

	s3cdbg("Entered %s\n", __FUNCTION__);

	while (1) {
		iiscon = readl(s3c24xx_i2s.regs + S3C64XX_IIS0CON);
		if (iiscon & S3C64XX_IISCON_LRINDEX)
			break;

		if (timeout < jiffies)
			return -ETIMEDOUT;
	}

	return 0;
}

/*
 * Check whether CPU is the master or slave
 */
static inline int s3c24xx_snd_is_clkmaster(void)
{
	s3cdbg("Entered %s\n", __FUNCTION__);

	return (readl(s3c24xx_i2s.regs + S3C64XX_IIS0MOD) & S3C64XX_IISMOD_SLAVE) ? 0:1;
}

/*
 * Set S3C24xx I2S DAI format
 */
static int s3c_i2s_set_fmt(struct snd_soc_cpu_dai *cpu_dai,
		unsigned int fmt)
{
#if 0
	u32 iismod;

	s3cdbg("Entered %s: fmt = %d\n", __FUNCTION__, fmt);

	iismod = readl(s3c24xx_i2s.regs + S3C64XX_IIS0MOD);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
#if !defined (CONFIG_CPU_S3C6400) && !defined (CONFIG_CPU_S3C6410)
		iismod |= S3C64XX_IISMOD_SLAVE;
#else
		iismod |= S3C2410_IISMOD_MASTER;
#endif
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_LEFT_J:
#if !defined (CONFIG_CPU_S3C6400) && !defined (CONFIG_CPU_S3C6410)
		iismod |= S3C2410_IISMOD_MSB;
#else
		iismod |= S3C_IIS0MOD_MSB;
#endif
		break;
	case SND_SOC_DAIFMT_I2S:
		break;
	default:
		return -EINVAL;
	}

	writel(iismod, s3c24xx_i2s.regs + S3C64XX_IIS0MOD);
#endif
	return 0;

}

static int s3c_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	unsigned long iiscon;
	unsigned long iismod;
	unsigned long iisfcon;
	
	s3cdbg("Entered %s\n", __FUNCTION__);

	/*Set I2C port to controll WM8753 codec*/
	s3c_gpio_pullup(S3C_GPB5, 0);
	s3c_gpio_pullup(S3C_GPB6, 0);
	s3c_gpio_cfgpin(S3C_GPB5, S3C_GPB5_I2C_SCL0);
	s3c_gpio_cfgpin(S3C_GPB6, S3C_GPB6_I2C_SDA0);

	s3c24xx_i2s.master = 1;
	
	/* Configure the I2S pins in correct mode */
	s3c_gpio_cfgpin(S3C_GPD2,S3C_GPD2_I2S_LRCLK0);

	if (s3c24xx_i2s.master && !extclk){
		s3cdbg("Setting Clock Output as we are Master\n");
		s3c_gpio_cfgpin(S3C_GPD0,S3C_GPD0_I2S_CLK0);
		
	}
	s3c_gpio_cfgpin(S3C_GPD1,S3C_GPD1_I2S_CDCLK0);
	s3c_gpio_cfgpin(S3C_GPD3,S3C_GPD3_I2S_DI0);
	s3c_gpio_cfgpin(S3C_GPD4,S3C_GPD4_I2S_DO0);

	/* pull-up-enable, pull-down-disable*/
	s3c_gpio_pullup(S3C_GPD0, 0x2);
	s3c_gpio_pullup(S3C_GPD1, 0x2);
	s3c_gpio_pullup(S3C_GPD2, 0x2);
	s3c_gpio_pullup(S3C_GPD3, 0x2);
	s3c_gpio_pullup(S3C_GPD4, 0x2);

	s3cdbg("substream->stream : %d\n", substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		rtd->dai->cpu_dai->dma_data = &s3c24xx_i2s_pcm_stereo_out;
	} else {
		rtd->dai->cpu_dai->dma_data = &s3c24xx_i2s_pcm_stereo_in;
	}

	/* Working copies of registers */
	iiscon = readl(s3c24xx_i2s.regs + S3C64XX_IIS0CON);
	iismod = readl(s3c24xx_i2s.regs + S3C64XX_IIS0MOD);
	iisfcon = readl(s3c24xx_i2s.regs + S3C64XX_IIS0FIC);
	/* is port used by another stream */
	if (!(iiscon & S3C64XX_IIS0CON_I2SACTIVE)) {

		// Clear BFS field [2:1]
		iismod &= ~(0x3<<1);
		iismod |= S3C64XX_IIS0MOD_32FS | S3C64XX_IIS0MOD_INTERNAL_CLK;

		if (!s3c24xx_i2s.master)
			iismod |= S3C64XX_IIS0MOD_IMS_SLAVE;
		else
			iismod |= S3C64XX_IIS0MOD_IMS_EXTERNAL_MASTER;
	}

	/* enable TX & RX all to support Full-duplex */
	iismod |= S3C64XX_IIS0MOD_TXRXMODE;
	iiscon |= S3C64XX_IIS0CON_TXDMACTIVE;
	iisfcon |= S3C64XX_IIS_TX_FLUSH;
	iiscon |= S3C64XX_IIS0CON_RXDMACTIVE;
	iisfcon |= S3C64XX_IIS_RX_FLUSH;

	writel(iiscon, s3c24xx_i2s.regs + S3C64XX_IIS0CON);
	writel(iismod, s3c24xx_i2s.regs + S3C64XX_IIS0MOD);
	writel(iisfcon, s3c24xx_i2s.regs + S3C64XX_IIS0FIC);

	// Tx, Rx fifo flush bit clear
	iisfcon  &= ~(S3C64XX_IIS_TX_FLUSH | S3C64XX_IIS_RX_FLUSH);
	writel(iisfcon, s3c24xx_i2s.regs + S3C64XX_IIS0FIC);

	s3cdbg("IISCON: %lx IISMOD: %lx", iiscon, iismod);

	return 0;

}

static int s3c_i2s_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;

	s3cdbg("Entered %s: cmd = %d\n", __FUNCTION__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (!s3c24xx_snd_is_clkmaster()) {
			ret = s3c24xx_snd_lrsync();
			if (ret)
				goto exit_err;
		}

		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			s3c24xx_snd_rxctrl(1);
		else
			s3c24xx_snd_txctrl(1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			s3c24xx_snd_rxctrl(0);
		else
			s3c24xx_snd_txctrl(0);
		break;
	default:
		ret = -EINVAL;
		break;
	}

exit_err:
	return ret;
}

static void s3c64xx_i2s_shutdown(struct snd_pcm_substream *substream)
{
	unsigned long iismod, iiscon;

	s3cdbg("Entered %s\n", __FUNCTION__);
	
	iismod=readl(s3c24xx_i2s.regs + S3C64XX_IIS0MOD);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		iismod &= ~S3C64XX_IIS0MOD_TXMODE;
	} else {
		iismod &= ~S3C64XX_IIS0MOD_RXMODE;
	}

	writel(iismod,s3c24xx_i2s.regs + S3C64XX_IIS0MOD);

	iiscon=readl(s3c24xx_i2s.regs + S3C64XX_IIS0CON);
	iiscon &= !S3C64XX_IIS0CON_I2SACTIVE;
	writel(iiscon,s3c24xx_i2s.regs + S3C64XX_IIS0CON);

	/* Clock disable 
	 * PCLK & SCLK gating disable 
	 */
	__raw_writel(__raw_readl(S3C_PCLK_GATE)&~(S3C_CLKCON_PCLK_IIS0), S3C_PCLK_GATE);
	__raw_writel(__raw_readl(S3C_SCLK_GATE)&~(S3C_CLKCON_SCLK_AUDIO0), S3C_SCLK_GATE);

	/* EPLL disable */
	__raw_writel(__raw_readl(S3C_EPLL_CON0)&~(1<<31) ,S3C_EPLL_CON0);	
	
}


/*
 * Set S3C24xx Clock source
 */
static int s3c_i2s_set_sysclk(struct snd_soc_cpu_dai *cpu_dai,
	int clk_id, unsigned int freq, int dir)
{
	u32 iismod = readl(s3c24xx_i2s.regs + S3C64XX_IIS0MOD);

	s3cdbg("Entered %s : clk_id = %d\n", __FUNCTION__, clk_id);

	iismod &= ~S3C64XX_IISMOD_MPLL;

	switch (clk_id) {
	case S3C24XX_CLKSRC_PCLK:
		break;
	case S3C24XX_CLKSRC_MPLL:
		iismod |= S3C64XX_IISMOD_MPLL;
		break;
	default:
		return -EINVAL;
	}

	writel(iismod, s3c24xx_i2s.regs + S3C64XX_IIS0MOD);
	return 0;
}

/*
 * Set S3C24xx Clock dividers
 */
static int s3c_i2s_set_clkdiv(struct snd_soc_cpu_dai *cpu_dai,
	int div_id, int div)
{
	u32 reg;

	s3cdbg("Entered %s : div_id = %d, div = %d\n", __FUNCTION__, div_id, div);

	switch (div_id) {
	case S3C24XX_DIV_MCLK:
		break;
	case S3C24XX_DIV_BCLK:
		reg = readl(s3c24xx_i2s.regs + S3C64XX_IIS0MOD) & ~(S3C64XX_IISMOD_384FS);
		writel(reg | div, s3c24xx_i2s.regs + S3C64XX_IIS0MOD);
		break;
	case S3C24XX_DIV_PRESCALER:
		writel(div|(1<<15),s3c24xx_i2s.regs + S3C64XX_IIS0PSR);
		break;
	default:
		return -EINVAL;
	}
	
	return 0;
}

static int s3c_i2s_probe(struct platform_device *pdev)
{
	s3cdbg("Entered %s\n", __FUNCTION__);

	s3c24xx_i2s.regs = ioremap(S3C24XX_PA_IIS, 0x100);
	if (s3c24xx_i2s.regs == NULL)
		return -ENXIO;
	return 0;
}

#ifdef CONFIG_PM
static int s3c_i2s_suspend(struct platform_device *dev,
	struct snd_soc_cpu_dai *dai)
{
	s3cdbg("Entered %s\n", __FUNCTION__);
	return 0;
}

static int s3c_i2s_resume(struct platform_device *pdev,
	struct snd_soc_cpu_dai *dai)
{
	s3cdbg("Entered %s\n", __FUNCTION__);
	return 0;
}

#else
#define s3c_i2s_suspend	NULL
#define s3c_i2s_resume	NULL
#endif


#define S3C24XX_I2S_RATES \
	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 | SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)

struct snd_soc_cpu_dai s3c_i2s_dai = {
	.name = "s3c-i2s",
	.id = 0,
	.type = SND_SOC_DAI_I2S,
	.probe = s3c_i2s_probe,
	.suspend = s3c_i2s_suspend,
	.resume = s3c_i2s_resume,
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = S3C24XX_I2S_RATES,
		.formats = SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE,},
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = S3C24XX_I2S_RATES,
		.formats = SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE,},
	.ops = {
		.shutdown = s3c64xx_i2s_shutdown,
		.trigger = s3c_i2s_trigger,
		.hw_params = s3c_i2s_hw_params,},
	.dai_ops = {
		.set_fmt = s3c_i2s_set_fmt,
		.set_clkdiv = s3c_i2s_set_clkdiv,
		.set_sysclk = s3c_i2s_set_sysclk,
	},
};
EXPORT_SYMBOL_GPL(s3c_i2s_dai);

/* Module information */
MODULE_AUTHOR("Ben Dooks, <ben@simtec.co.uk>");
MODULE_DESCRIPTION("s3c24xx I2S SoC Interface");
MODULE_LICENSE("GPL");
