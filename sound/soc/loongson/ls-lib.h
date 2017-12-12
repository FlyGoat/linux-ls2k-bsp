#ifndef LS_LIB_H
#define LS_LIB_H

#include <linux/platform_device.h>
#include <sound/ac97_codec.h>
#include <linux/irqreturn.h>

/* PCM */

struct ls_pcm_dma_params {
	char *name;			/* stream identifier */
	u32 dcmd;			/* DMA descriptor dcmd field */
	volatile u32 *drcmr;		/* the DMA request channel to use */
	u32 dev_addr;			/* device physical address for DMA */
};

extern int __ls_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params);
extern int __ls_pcm_hw_free(struct snd_pcm_substream *substream);
extern int ls_pcm_trigger(struct snd_pcm_substream *substream, int cmd);
extern snd_pcm_uframes_t ls_pcm_pointer(struct snd_pcm_substream *substream);
extern int __ls_pcm_prepare(struct snd_pcm_substream *substream);
extern irqreturn_t ls_pcm_dma_irq(int irq, void *dev_id);
extern int __ls_pcm_open(struct snd_pcm_substream *substream);
extern int __ls_pcm_close(struct snd_pcm_substream *substream);
extern int ls_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream);
extern void ls_pcm_free_dma_buffers(struct snd_pcm *pcm);

/* AC97 */

extern unsigned short ls_ac97_read(struct snd_ac97 *ac97, unsigned short reg);
extern void ls_ac97_write(struct snd_ac97 *ac97, unsigned short reg, unsigned short val);

extern bool ls_ac97_try_warm_reset(struct snd_ac97 *ac97);
extern bool ls_ac97_try_cold_reset(struct snd_ac97 *ac97);
extern void ls_ac97_finish_reset(struct snd_ac97 *ac97);

extern int ls_ac97_hw_suspend(void);
extern int ls_ac97_hw_resume(void);

extern int ls_ac97_hw_probe(struct platform_device *dev);
extern void ls_ac97_hw_remove(struct platform_device *dev);
#endif
