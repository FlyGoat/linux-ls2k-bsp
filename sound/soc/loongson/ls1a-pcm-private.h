
/*
 * FileName:lson_ac97.h 
 *
 * data:
 * descriptor:
 * update:
 *
 */

#ifndef FCR_AC97_H__
#define FCR_AC97_H__

#define _REG(x) 	((volatile u32 *)(x))
#define _REG2(b,o) 	((volatile u32 *)((b)+(o)))

#define LSON_AC_FMT  		AFM_S16_LE
#define LSON_AC_MAXCHNL 	2


#define DMA_FROMAC_DESCs 4
#define DMA_TOAC_DESCs 4


//AC97_controler
#define AC97_REG(off)       _REG2(KSEG1ADDR(LS1X_AC97_REGS_BASE),(off))
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
#define READ_CRAC_MASK 0x1
#define WRITE_CRAC_MASK 0x2

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
//
// DMA regs
//
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

//#define FCR_ACDMA_REG_BASE	0x1f004280
#define CONFREG_BASE		0xbfd00000
#define FCR_ACDMA_REG_BASE	0x1fe74080
//#define AC97_REG(off)       _REG2(KSEG1ADDR(FCR_AC97_REGS_BASE),(off))
#define DMA_REG(off)  		_REG2(KSEG1ADDR(FCR_ACDMA_REG_BASE),(off))

/* dma regs */
#define DMA_ORDERED		0x0
#define DMA_SADDR		0x1
#define DMA_DADDR		0x2
#define DMA_LENGTH		0x3
#define DMA_STEP_LENGTH		0x4
#define DMA_STEP_TIMES		0x5
#define	DMA_CMD			0x6

/* bit offset in order_addr_reg */
#define DMA_STOP_OFF		0x4
#define DMA_START_OFF		0x3
#define DMA_ASKVALID_OFF	0x2
#define DMA_DEVNUM_OFF		0x0

/* define dma status */
#define DMA_START		0x0
#define DMA_STOP		0x1

#define DMA_BUF_UPPER		0x3
#define DMA_BUF_LOW		0x1

#define AUD_WRITE_ADDR		0xdfe72420	
#define AUD_READ_ADDR		0x9fe74c4c

#define BLANK_SIZE		0x20

#define ZERO_BUF_SIZE 128
#define DESC_VALID 1

/*
 * audio private data; 
 *
 */


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+ Object:
+ 1.stream/in f_ops->write /out f_ops_read
+ 2.buffers/in/out
+ 3.dma_desc /FIFO /buffers fragram
+ 4.dma_channel/to/ from devices
*++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


#define Z_LEN	0x8
/*dumy*/
typedef struct ls1a_dma_desc {
	u32 ordered;
	u32 saddr;
	u32 daddr;
	u32 length;
	u32 step_length;
	u32 step_times;
	u32 dcmd;
	u32 stats;
} ls1a_dma_desc;


#endif
