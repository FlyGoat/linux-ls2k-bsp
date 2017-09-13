/*
 * ls1a.c  --  SoC audio for ls1a
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

//#include "../codecs/ac97.h"
#include "ls1a-pcm.h"
#include "ls1a-ac97.h"

static struct snd_soc_card ls1a;

static struct snd_soc_dai_link ls1a_dai[] = {
{
	.name = "AC97",
	.stream_name = "LS1A<-->ALC203",
	.cpu_dai = &ls1a_ac97_dai[0],
	.codec_dai = &ac97_dai,
	.init = ls1x_init_codec,//zhuoqixiang
},
};

static struct snd_soc_card ls1a = {
	.name = "LS1A",
	.platform = &ls1a_soc_platform,
	.dai_link = ls1a_dai,
	.num_links = ARRAY_SIZE(ls1a_dai),
};

/*zqx----snd_soc_device和snd_soc_card代表Machine驱动*/
static struct snd_soc_device ls1a_snd_ac97_devdata = {
	.card = &ls1a,
	.codec_dev = &soc_codec_dev_ac97,
};

/*zqx----platform_device代表整个音频子系统*/
static struct platform_device *ls1a_snd_ac97_device;

static int __init ls1a_init(void)
{
	int ret;

	pr_debug("Entered %s\n", __func__);

	ls1a_snd_ac97_device = platform_device_alloc("soc-audio", -1);
	if (!ls1a_snd_ac97_device)
		return -ENOMEM;

	/*将ls1a_snd_ac97_devdata赋给 'plateform_device->dev->p->drive_data'----dev的核心驱动*/
	platform_set_drvdata(ls1a_snd_ac97_device,
				&ls1a_snd_ac97_devdata);
	ls1a_snd_ac97_devdata.dev = &ls1a_snd_ac97_device->dev;
	ret = platform_device_add(ls1a_snd_ac97_device);

	if (ret)
		platform_device_put(ls1a_snd_ac97_device);

	pr_debug("Exited %s\n", __func__);
	return ret;
}

static void __exit ls1a_exit(void)
{
	pr_debug("Entered %s\n", __func__);
	platform_device_unregister(ls1a_snd_ac97_device);
	pr_debug("Exited %s\n", __func__);
}

module_init(ls1a_init);
module_exit(ls1a_exit);

/* Module information */
MODULE_AUTHOR("Zhuo Qixiang");
MODULE_DESCRIPTION("ALSA SoC LS1A(LS1X)");
MODULE_LICENSE("GPL");





