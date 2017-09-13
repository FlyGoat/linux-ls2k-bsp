/*
 * linux/sound/mips/ls1a-pcm.h -- ALSA PCM interface for the Intel PXA2xx chip
 *
 * Author:	Nicolas Pitre
 * Created:	Nov 30, 2004
 * Copyright:	MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _LS1A_PCM_H
#define _LS1A_PCM_H
#include <asm/delay.h>
#include "ls1a-pcm-private.h"

struct ls1a_runtime_data {
	int dma_ch;
	struct ls1a_pcm_dma_params *params;
	ls1a_dma_desc *dma_desc_array;
	dma_addr_t dma_desc_array_phys;

	//将要执行的dma描述符物理地址
	ls1a_dma_desc *dma_desc_ready;
	dma_addr_t dma_desc_ready_phys;

	ls1a_dma_desc *dma_position_desc;
	dma_addr_t dma_position_desc_phys;
};

struct ls1a_pcm_client {
	struct ls1a_pcm_dma_params *playback_params;
	struct ls1a_pcm_dma_params *capture_params;
	int (*startup)(struct snd_pcm_substream *);
	void (*shutdown)(struct snd_pcm_substream *);
	int (*prepare)(struct snd_pcm_substream *);
};

extern int ls1a_pcm_new(struct snd_card *, struct ls1a_pcm_client *, struct snd_pcm **);


/* platform data */
extern struct snd_soc_platform_driver ls1a_soc_platform;

/*Zhuo Qixiang REGS OPRERATION*/


static inline u32 read_reg(volatile u32 * reg)
{
	return (*reg);
}
static inline void write_reg(volatile u32 * reg, u32 val)
{
	*(reg) = (val);
}

void ls1a_ac97_write (struct snd_ac97 *ac97, unsigned short reg, unsigned short val);
unsigned short ls1a_ac97_read (struct snd_ac97 *ac97, unsigned short reg);
#endif
