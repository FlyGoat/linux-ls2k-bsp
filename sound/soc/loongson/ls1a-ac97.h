/*
 * ls1a-ac97.c  --  ALSA Soc Audio Layer
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef LS1XAC97_H_
#define LS1XAC97_H_

#define _REG(x) 	((volatile u32 *)(x))
#define _REG2(b,o) 	((volatile u32 *)((b)+(o)))

#define LSON_AC_FMT  		AFM_S16_LE
#define LSON_AC_MAXCHNL 	2


//AC97
#define LS1X_AC97_REGS_BASE 0x1fe74000

//AC97_controler
#define AC97_REG(off)       _REG2(CKSEG1ADDR(LS1X_AC97_REGS_BASE),(off))
#define CSR			AC97_REG(0x00)
#define OCCR0		AC97_REG(0x04)
#define OCCR1		AC97_REG(0x08)
#define OCCR2		AC97_REG(0x0C)
#define ICCR		AC97_REG(0x10)
#define CDC_ID      AC97_REG(0x14)
#define CRAC		AC97_REG(0x18)
                                       /*Reserved 0x1C Reserved*/
#define OC0_DATA    AC97_REG(0x20)     /*20 bits WO Output Channel0 Tx buffer*/
#define OC1_DATA    AC97_REG(0x24)     /*20 bits WO Output Channel1 Tx buffer*/
#define OC2_DATA    AC97_REG(0x28)     /*20 bits WO Output Channel2 Tx buffer*/
#define OC3_DATA    AC97_REG(0x2C)     /*20 bits WO Output Channel3 Tx buffer*/
#define OC4_DATA    AC97_REG(0x30)     /*20 bits WO Output Channel4 Tx buffer*/
#define OC5_DATA    AC97_REG(0x34)     /*20 bits WO Output Channel5 Tx buffer*/
#define OC6_DATA    AC97_REG(0x38)     /*20 bits WO Output Channel6 Tx buffer*/
#define OC7_DATA    AC97_REG(0x3C)     /*20 bits WO Output Channel7 Tx buffer*/
#define OC8_DATA    AC97_REG(0x40)     /*20 bits WO Output Channel8 Tx buffer*/
#define IC0_DATA    AC97_REG(0x44)     /*20 bits RO Input Channel0 Rx buffer*/
#define IC1_DATA    AC97_REG(0x48)     /*20 bits RO Input Channel1 Rx buffer*/
#define IC2_DATA    AC97_REG(0x4C)     /*20 bits RO Input Channel2 Rx buffer*/
                                       /*Reserved AC97_REG(0x50 Reserved*/
#define INTRAW      AC97_REG(0x54)     /*32 bits RO Interrupt RAW status*/
#define INTM        AC97_REG(0x58)     /*32 bits R/W Interrupt Mask*/
#define INTS        AC97_REG(0x5C)     /*32 bits RO Interrupt Masked Status*/
#define CLR_INT     AC97_REG(0x60)     /*1 bit RO Clear Combined and Individual Interrupt*/
#define CLR_OC_INT  AC97_REG(0x64)     /*1 bit RO Clear Output Channel Reference Interrupt*/
#define CLR_IC_INT  AC97_REG(0x68)      /*1 bit RO Clear Input Channel Reference Interrupt*/
#define CLR_CDC_WR  AC97_REG(0x68)      /*1 bit RO Clear Codec Write Done Interrupt*/
#define CLR_CDC_RD  AC97_REG(0x6c)      /*1 bit RO Clear Codec Read Done Interrupt*/

#define OC_DATA     OC0_DATA            /*DEFAULT */
#define IN_DATA     IC1_DATA            /*SETTING */


#define 	INPUT_CHANNELS	8
#define 	OUTPUT_CHANNELS	9

/*reg bits define*/ 
#define CRAR_READ  0x80000000
#define CRAR_WRITE 0x00000000
#define CRAR_CODEC_REG(x)  (((x)&(0x7F))<<16)

//default setting 
#define OCCR	    OCCR0
#define OUT_CHANNEL OC[0]
#define IN_CHANNEL  IC[0]
#define INTS_CRAC_MASK	0x3

extern struct snd_soc_dai_driver ls1a_ac97_dai[];
#define READ_CRAC_MASK 0x1
#define WRITE_CRAC_MASK 0x2
#endif /*LS1XAC97_H_*/



/*Some Private funcs for Init ac97 and codecs
 *
 *
 *
 *
 * Zhuo Qixiang******************Zhuo Qixiang*/



void ls1x_audio_init_hw(void);
	
void ls1x_def_set(void);

int ls1x_init_codec(struct snd_soc_codec *codec);
