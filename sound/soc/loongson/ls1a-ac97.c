/*
 * linux/sound/ls1a-ac97.c -- AC97 support for the Loongson 1A chip.
 *
 * Author:	Nicolas Pitre
 * Created:	Dec 02, 2004
 * Copyright:	MontaVista Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/ac97_codec.h>
#include <sound/soc.h>

#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include "ls1a-lib.h"

#include <linux/interrupt.h>
#include "ls1a-pcm.h"
#include "ls1a-ac97.h"

#define DEBUG 1
#if DEBUG
#define ZPRINTK(fmt, args...) printk(KERN_ALERT "<%s>: " fmt, __FUNCTION__ , ## args)
#else
#define ZPRINTK(fmt, args...)
#endif

#include <linux/clk.h>
#include <linux/delay.h>

#include <sound/ac97_codec.h>

//#include <mach/audio.h>

bool ls1a_ac97_try_warm_reset(struct snd_ac97 *ac97)
{
	return true;
}
EXPORT_SYMBOL_GPL(ls1a_ac97_try_warm_reset);

bool ls1a_ac97_try_cold_reset(struct snd_ac97 *ac97)
{
	return true;
}
EXPORT_SYMBOL_GPL(ls1a_ac97_try_cold_reset);


void ls1a_ac97_finish_reset(struct snd_ac97 *ac97)
{
	
}
EXPORT_SYMBOL_GPL(ls1a_ac97_finish_reset);

/*
static irqreturn_t ls1a_ac97_irq(int irq, void *dev_id)
{
	
	return IRQ_HANDLED;
}
*/
#ifdef CONFIG_PM
int ls1a_ac97_hw_suspend(void)
{
	return 0;
}
EXPORT_SYMBOL_GPL(ls1a_ac97_hw_suspend);

int ls1a_ac97_hw_resume(void)
{
	return 0;
}
EXPORT_SYMBOL_GPL(ls1a_ac97_hw_resume);
#endif

int __init ls1a_ac97_hw_probe(struct platform_device *dev)
{
	int ret = 0;

	return ret;
}
EXPORT_SYMBOL_GPL(ls1a_ac97_hw_probe);

void ls1a_ac97_hw_remove(struct platform_device *dev)
{

}
EXPORT_SYMBOL_GPL(ls1a_ac97_hw_remove);

static int capmode = 0x20000;//zhuoqixiang,for identify alc203

int codec_wait_complete(int flag)
{
	int timeout = 10000;

	while ((!(read_reg(INTRAW) & flag)) && (timeout-- > 0))
		udelay(1000);
//	printk(KERN_ALERT "intr_val = 0x%x\n", read_reg(INTRAW));
	if (timeout>0)
	{
		read_reg(CLR_CDC_RD);
		read_reg(CLR_CDC_WR);
	}
	return timeout > 0 ? 0 : 1;
}

void ls1a_ac97_write (struct snd_ac97 *ac97, unsigned short reg, unsigned short val)
{

	//u32 tmp;
	//tmp=(CRAR_WRITE | CRAR_CODEC_REG(reg) | val);
	//printk("REG=0x%02x,val=0x%04x,tmp=0x%08x\n",reg,val,tmp);

	write_reg(CRAC, (CRAR_WRITE | CRAR_CODEC_REG(reg) | val));

	codec_wait_complete(WRITE_CRAC_MASK);
//	ZPRINTK("REG=0x%02x,val=0x%04x\n",reg,val);

}
EXPORT_SYMBOL_GPL(ls1a_ac97_write);



unsigned short ls1a_ac97_read (struct snd_ac97 *ac97, unsigned short reg)
{
	
	/*write crac */
	volatile u16 val;
	
	write_reg(CRAC,(CRAR_READ|CRAR_CODEC_REG(reg)));
	if (codec_wait_complete(READ_CRAC_MASK) != 0)
	{
		ZPRINTK("ERROR\n");
		return -1;
	}
	val = read_reg(CRAC) & 0xffff;
//	ZPRINTK("REG=0x%02x,val=0x%04x\n",reg,val);
	return val;

}
EXPORT_SYMBOL_GPL(ls1a_ac97_read);




static void ls1a_ac97_warm_reset(struct snd_ac97 *ac97)
{
	ls1a_ac97_write(ac97, 0, 0);
}

static void ls1a_ac97_cold_reset(struct snd_ac97 *ac97)
{
	u32 flags, i;

	local_irq_save(flags);
	for(i=0;i<100000;i++) *CSR |=1;
	local_irq_restore(flags);


	ls1a_ac97_write(ac97, 0, 0);
}


int ls1x_init_codec(struct snd_soc_codec *codec)
{
	static unsigned short sample_rate=0xac44;
	int i;

	/*ac97 config*/
	write_reg(OCCR,0x6b6b); //OCCR0   L&& R enable ; 3/4 empty; dma enabled;8 bits;var rate(0x202);
	write_reg(ICCR,0x610000|capmode);//ICCR
	//write_reg(INTM,0xFFFFFFFF); //INTM ;disable all interrupt
	write_reg(INTM, 0x0);

	ls1a_ac97_write(NULL,0,0);//codec reset

	for(i = 0; i < 5; i++)
	{
		printk("======== A/D status = 0x%x\n", ls1a_ac97_read(NULL, 0x26) & 0xffff);
	}

	ls1a_ac97_write(NULL,0x2,0x0808);//|(0x2<<16)|(0<<31));      //Master Vol.
	ls1a_ac97_write(NULL,0x4,0x0808);//|(0x2<<16)|(0<<31));      //headphone Vol.
	ls1a_ac97_write(NULL,0x6,0x0008);//|(0x2<<16)|(0<<31));      //mono Vol.

	ls1a_ac97_write(NULL,0xc,0x0008);//|(0x2<<16)|(0<<31));      //phone Vol.
	ls1a_ac97_write(NULL,0x18,0x0808);//|(0x18<<16)|(0<<31));     //PCM Out Vol.
	ls1a_ac97_write(NULL,0x38,0x0808);                            //surround Out Vol.
	ls1a_ac97_write(NULL,0x2a,1);//0x1|(0x2A<<16)|(0<<31));        //Extended Audio Status  and control
	ls1a_ac97_write(NULL,0x1a,0);//        //select record
	ls1a_ac97_write(NULL,0x2c,sample_rate);//|(0x2c<<16)|(0<<31));     //PCM Out rate
	ls1a_ac97_write(NULL,0x32,sample_rate);//|(0x32<<16)|(0<<31));     //pcm input rate .
	ls1a_ac97_write(NULL,0x34,sample_rate);//|(0x34<<16)|(0<<31));     //MIC rate.
	ls1a_ac97_write(NULL,0x0E,0x35f);//|(0x0E<<16)|(0<<31));     //Mic vol .
	ls1a_ac97_write(NULL,0x1c,0x0f0f);//     //adc record gain
	ls1a_ac97_write(NULL,0x10,0x0101);//     //line in gain
	ls1a_ac97_write(NULL,0x1E,0x0808);//|(0x1E<<16)|(0<<31));     //MIC Gain ADC.
	if(ls1a_ac97_read(NULL,AC97_VENDOR_ID1) == 0x414c && ls1a_ac97_read(NULL,AC97_VENDOR_ID2) == 0x4760)
	ls1a_ac97_write(NULL,0x6a,ls1a_ac97_read(NULL,0x6a)|0x201);


	return 0;
}




/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
//
// codec reg ops 
//
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


struct snd_ac97_bus_ops soc_ac97_ops = {
	.read	= ls1a_ac97_read,
	.write	= ls1a_ac97_write,
	.warm_reset	= ls1a_ac97_warm_reset,
	.reset	= ls1a_ac97_cold_reset,
//	.wait	= ls1a_ac97_wait,//Add by Zhuoqixiang
};

static struct ls1a_pcm_dma_params ls1a_ac97_pcm_stereo_out = {
	.name			= "AC97 PCM Stereo out",
	.dev_addr		= 0x0fe72420 | (1<<31) | (0<<30)//播放,AC97写使能
};

static struct ls1a_pcm_dma_params ls1a_ac97_pcm_stereo_in = {
	.name			= "AC97 PCM Stereo in",
	.dev_addr		= 0x0fe74c4c | (1<<31) | (0<<30)//录音,AC97写使能
};

#ifdef CONFIG_PM
static int ls1a_ac97_suspend(struct snd_soc_dai *dai)
{
	return ls1a_ac97_hw_suspend();
}

static int ls1a_ac97_resume(struct snd_soc_dai *dai)
{
	return ls1a_ac97_hw_resume();
}

#else
#define ls1a_ac97_suspend	NULL
#define ls1a_ac97_resume	NULL
#endif

static int ls1a_ac97_probe(struct platform_device *pdev,
			     struct snd_soc_dai *dai)
{
	ls1a_ac97_cold_reset(NULL);
	
	return 0;

}

static void ls1a_ac97_remove(struct platform_device *pdev,
			       struct snd_soc_dai *dai)
{
	ls1a_ac97_hw_remove(to_platform_device(dai->dev));
}

static int ls1a_ac97_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *cpu_dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct pxa2xx_pcm_dma_params *dma_data;

	printk("params_format=%s\n",params_format(params)==SNDRV_PCM_FORMAT_S16_LE ? "S16_LE":"U8");
	printk("params_channels=%s\n",params_channels(params)==1 ? "MONO":"STEREO");
	
	//采样位数;设置声道
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
		if(params_channels(params)== 2)
			ls1a_ac97_pcm_stereo_out.dev_addr |= (1<<30);//双声道

		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_U8:
			write_reg(OCCR,0x6363); //OCCR0   L&& R enable ; 3/4 empty; dma enabled;8 bits;var rate(0x202);
			while(1);
			break;
		case SNDRV_PCM_FORMAT_S16_LE:
			write_reg(OCCR,0x6b6b); //OCCR0   L&& R enable ; 3/4 empty; dma enabled;16 bits;var rate(0x202);
			ls1a_ac97_pcm_stereo_out.dev_addr |= (1<<28);
			break;
		default:
			printk("NOTICE!___May Not Surpport!\n");
			write_reg(OCCR,0x6363); //OCCR0   L&& R enable ; 3/4 empty; dma enabled;8 bits;var rate(0x202);
			break;
		}
		dma_data = &ls1a_ac97_pcm_stereo_out;
	}
	else{
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_U8:
			write_reg(ICCR,0x610000|capmode);//ICCR
			break;
		case SNDRV_PCM_FORMAT_S16_LE:
			write_reg(ICCR,0x690000|capmode);//ICCR
			ls1a_ac97_pcm_stereo_in.dev_addr |= (1<<28);
			break;
		default:
			printk("NOTICE!___May Not Surpport!\n");
			write_reg(ICCR,0x610000|capmode);//ICCR
			break;
		}
		dma_data = &ls1a_ac97_pcm_stereo_in;
	}

//setup sth.
	snd_soc_dai_set_dma_data(cpu_dai, substream, dma_data);

	return 0;
}

#define LS1A_AC97_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | \
		SNDRV_PCM_RATE_48000)

//#define LS1A_AC97_RATES (SNDRV_PCM_RATE_48000)

static struct snd_soc_dai_ops ls1a_ac97_hifi_dai_ops = {
	.hw_params	= ls1a_ac97_hw_params,
};


/*
 * There is only 1 physical AC97 interface for ls1a, but it
 * has extra fifo's that can be used for aux DACs and ADCs.
 */
struct snd_soc_dai_driver ls1a_ac97_dai_driver[] = {
{
	.name = "ls1a-ac97",
	.id = 0,
	.ac97_control = 1,
	.probe = ls1a_ac97_probe,
	.remove = ls1a_ac97_remove,
	.suspend = ls1a_ac97_suspend,
	.resume = ls1a_ac97_resume,
	.playback = {
		.stream_name = "AC97 Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = LS1A_AC97_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.capture = {
		.stream_name = "AC97 Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = LS1A_AC97_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.ops = &ls1a_ac97_hifi_dai_ops,
},
};

EXPORT_SYMBOL_GPL(ls1a_ac97_dai);
EXPORT_SYMBOL_GPL(soc_ac97_ops);

static const struct snd_soc_component_driver ls1a_ac97_component = {
	.name		= "ls1a-ac97",
};

static int ls1a_ac97_dev_probe(struct platform_device *pdev)
{
	if (pdev->id != -1) {
		dev_err(&pdev->dev, "PXA2xx has only one AC97 port.\n");
		return -ENXIO;
	}

	/* Punt most of the init to the SoC probe; we may need the machine
	 * driver to do interesting things with the clocking to get us up
	 * and running.
	 */
	return snd_soc_register_component(&pdev->dev, &ls1a_ac97_component,
					  ls1a_ac97_dai_driver, ARRAY_SIZE(ls1a_ac97_dai_driver));
}

static int ls1a_ac97_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static struct platform_driver ls1a_ac97_driver = {
	.probe		= ls1a_ac97_dev_probe,
	.remove		= ls1a_ac97_dev_remove,
	.driver		= {
		.name	= "ls1a-ac97",
		.owner	= THIS_MODULE,
	},
};

module_platform_driver(ls1a_ac97_driver);

MODULE_AUTHOR("Nicolas Pitre");
MODULE_DESCRIPTION("AC97 driver for the Intel PXA2xx chip");
MODULE_LICENSE("GPL");
