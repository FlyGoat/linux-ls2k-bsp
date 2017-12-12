/*
 * ls-i2s.c  --  ALSA Soc Audio Layer
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <ls2k.h>
#include "ls-lib.h"

#define IISVERSION	(i2sbase + 0x0)
#define IISCONFIG	(i2sbase + 0x4)
#define IISCTRL	(i2sbase + 0x8)
#define IISRxData	(0x1fe0d000 + 0xc)
#define	IISTxData	(0x1fe0d000 + 0x10)

struct pxa_i2s_port {
	u32 sadiv;
	u32 sacr0;
	u32 sacr1;
	u32 saimr;
	int master;
	u32 fmt;
};

static int clk_ena = 0;

static struct ls_pcm_dma_params ls_i2s_pcm_stereo_out = {
	.name			= "I2S PCM Stereo out",
	.dev_addr		= IISTxData,
};

static struct ls_pcm_dma_params ls_i2s_pcm_stereo_in = {
	.name			= "I2S PCM Stereo in",
	.dev_addr		= IISRxData,
};

static int ls_i2s_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	return 0;
}

static int ls_i2s_set_dai_fmt(struct snd_soc_dai *cpu_dai,
		unsigned int fmt)
{
	return 0;
}

static int ls_i2s_set_dai_sysclk(struct snd_soc_dai *cpu_dai,
		int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int ls_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct ls_pcm_dma_params *dma_data;
	void *i2sbase = dai->dev->platform_data;
	unsigned char rat_cddiv;
	unsigned char rat_bitdiv;
	int wlen = 16;
	int depth = 16;

	clk_ena = 1;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data = &ls_i2s_pcm_stereo_out;
	else
		dma_data = &ls_i2s_pcm_stereo_in;

	snd_soc_dai_set_dma_data(dai, substream, dma_data);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		writel(readl(IISCTRL)|0x0d280, IISCTRL);
	else
		writel(readl(IISCTRL)|0x0e800, IISCTRL);

	rat_bitdiv = 0x2f;
	rat_cddiv = 0x5;

	writel((wlen<<24) | (depth<<16) | (rat_bitdiv<<8) | (rat_cddiv<<0), IISCONFIG) ;

	return 0;
}

static int ls_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
			      struct snd_soc_dai *dai)
{
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static void ls_i2s_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
}

#ifdef CONFIG_PM
static int ls_i2s_suspend(struct snd_soc_dai *dai)
{
	return 0;
}

static int ls_i2s_resume(struct snd_soc_dai *dai)
{
	return 0;
}

#else
#define ls_i2s_suspend	NULL
#define ls_i2s_resume	NULL
#endif

static int ls_i2s_probe(struct snd_soc_dai *dai)
{
	return 0;
}

static int  ls_i2s_remove(struct snd_soc_dai *dai)
{
	return 0;
}

#define PXA2XX_I2S_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | \
		SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000)

static const struct snd_soc_dai_ops pxa_i2s_dai_ops = {
	.startup	= ls_i2s_startup,
	.shutdown	= ls_i2s_shutdown,
	.trigger	= ls_i2s_trigger,
	.hw_params	= ls_i2s_hw_params,
	.set_fmt	= ls_i2s_set_dai_fmt,
	.set_sysclk	= ls_i2s_set_dai_sysclk,
};

static struct snd_soc_dai_driver pxa_i2s_dai = {
	.name = "ls-i2s",
	.probe = ls_i2s_probe,
	.remove = ls_i2s_remove,
	.suspend = ls_i2s_suspend,
	.resume = ls_i2s_resume,
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = PXA2XX_I2S_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = PXA2XX_I2S_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.ops = &pxa_i2s_dai_ops,
	.symmetric_rates = 1,
};

static const struct snd_soc_component_driver pxa_i2s_component = {
	.name		= "ls-i2s",
};

static int ls_i2s_drv_probe(struct platform_device *pdev)
{
	struct resource *r;
	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL) {
		dev_err(&pdev->dev, "no IO memory resource defined\n");
		return -ENODEV;
	}
	pdev->dev.kobj.name = "ls-i2s";
	pdev->dev.platform_data = ioremap(r->start, r->end - r->start + 1);;
	return snd_soc_register_component(&pdev->dev, &pxa_i2s_component,
					  &pxa_i2s_dai, 1);
}

static int ls_i2s_drv_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id snd_ls_i2s_dt_match[] = {
    { .compatible = "loongson,ls-i2s", },
    {},
};
MODULE_DEVICE_TABLE(of, snd_ls_i2s_dt_match);
#endif

static struct platform_driver ls_i2s_driver = {
	.probe = ls_i2s_drv_probe,
	.remove = ls_i2s_drv_remove,
	.driver = {
		.name = "ls-i2s",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(snd_ls_i2s_dt_match),
#endif
	},
};

static int __init ls_i2s_init(void)
{
	return platform_driver_register(&ls_i2s_driver);
}

static void __exit ls_i2s_exit(void)
{
	platform_driver_unregister(&ls_i2s_driver);
}

module_init(ls_i2s_init);
module_exit(ls_i2s_exit);

MODULE_AUTHOR("mashuai");
MODULE_DESCRIPTION("loongson I2S SoC Interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ls-i2s");
