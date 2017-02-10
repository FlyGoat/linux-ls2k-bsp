/*
 *  linux/drivers/video/ls2h_fb.c -- Virtual frame buffer device
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/i2c.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>

#include <asm/addrspace.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <ls2h/ls2h.h>
#include "edid.h"

#ifdef LS2H_FB_DEBUG
#define LS2H_DEBUG(frm, arg...)	\
	printk("ls2h_fb: %s %d: "frm, __func__, __LINE__, ##arg);
#else
#define LS2H_DEBUG(frm, arg...)
#endif /* LS2H_FB_DEBUG */

#define ON	1
#define OFF	0

#define CUR_WIDTH_SIZE		32
#define CUR_HEIGHT_SIZE		32
#define DEFAULT_BITS_PER_PIXEL	16

#ifdef CONFIG_LS2H_SB
#define DEFAULT_CURSOR_MEM	0x90000e0003000000
#define DEFAULT_CURSOR_DMA	0x03000000
#define DEFAULT_FB_MEM		0x90000e0002000000
#define DEFAULT_PHY_ADDR	0x00000e0002000000
#define DEFAULT_FB_DMA		0x02000000
#endif

static struct eep_info {
	struct i2c_adapter *adapter;
	unsigned short addr;
} eeprom_info;

struct ls2h_fb_par {
	struct platform_device *pdev;
	struct fb_info *fb_info;
	unsigned int reg_base;
	unsigned int irq;
	unsigned int htotal;
	unsigned int vtotal;
	u8 *edid;
};
static char *mode_option = NULL;
static void *cursor_mem;
static unsigned int cursor_size = 0x1000;
static void *videomemory;
static	dma_addr_t dma_A;
static dma_addr_t cursor_dma;

static u_long videomemorysize = 0;
module_param(videomemorysize, ulong, 0);
DEFINE_SPINLOCK(fb_lock);

static struct fb_var_screeninfo ls2h_fb_default __initdata = {
	.xres		= 1280,
	.yres		= 1024,
	.xres_virtual	= 1280,
	.yres_virtual	= 1024,
	.xoffset	= 0,
	.yoffset	= 0,
	.bits_per_pixel = DEFAULT_BITS_PER_PIXEL,
	.red		= { 11, 5 ,0},
	.green		= { 5, 6, 0 },
	.blue		= { 0, 5, 0 },
	.activate	= FB_ACTIVATE_NOW,
	.height		= -1,
	.width		= -1,
	.pixclock	= 9259,
	.left_margin	= 248,
	.right_margin	= 48,
	.upper_margin	= 38,
	.lower_margin	= 1,
	.hsync_len	= 112,
	.vsync_len	= 3,
	.sync		= 3,
	.vmode =	FB_VMODE_NONINTERLACED,
};
static struct fb_fix_screeninfo ls2h_fb_fix __initdata = {
	.id =		"Virtual FB",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.xpanstep =	1,
	.ypanstep =	1,
	.ywrapstep =	1,
	.accel =	FB_ACCEL_NONE,
};

static int ls2h_fb_enable __initdata = 0;	/* disabled by default */
module_param(ls2h_fb_enable, bool, 0);

/*
 *  Internal routines
 */

static u_long get_line_length(int xres_virtual, int bpp)
{
	u_long length;

	length = xres_virtual * bpp;
	length = (length + 31) & ~31;
	length >>= 3;
	return (length);
}

/*
 *  Setting the video mode has been split into two parts.
 *  First part, xxxfb_check_var, must not write anything
 *  to hardware, it should only verify and adjust var.
 *  This means it doesn't alter par but it does use hardware
 *  data from it to check this var.
 */

static int ls2h_fb_check_var(struct fb_var_screeninfo *var,
			 struct fb_info *info)
{
	u_long line_length;

	/*
	 *  FB_VMODE_CONUPDATE and FB_VMODE_SMOOTH_XPAN are equal!
	 *  as FB_VMODE_SMOOTH_XPAN is only used internally
	 */

	if (var->vmode & FB_VMODE_CONUPDATE) {
		var->vmode |= FB_VMODE_YWRAP;
		var->xoffset = info->var.xoffset;
		var->yoffset = info->var.yoffset;
	}

	/*
	 *  Some very basic checks
	 */
	if (!var->xres)
		var->xres = 1;
	if (!var->yres)
		var->yres = 1;
	if (var->xres > var->xres_virtual)
		var->xres_virtual = var->xres;
	if (var->yres > var->yres_virtual)
		var->yres_virtual = var->yres;
	if (var->bits_per_pixel <= 1)
		var->bits_per_pixel = 1;
	else if (var->bits_per_pixel <= 8)
		var->bits_per_pixel = 8;
	else if (var->bits_per_pixel <= 16)
		var->bits_per_pixel = 16;
	else if (var->bits_per_pixel <= 24)
		var->bits_per_pixel = 24;
	else if (var->bits_per_pixel <= 32)
		var->bits_per_pixel = 32;
	else
		return -EINVAL;

	if (var->xres_virtual < var->xoffset + var->xres)
		var->xres_virtual = var->xoffset + var->xres;
	if (var->yres_virtual < var->yoffset + var->yres)
		var->yres_virtual = var->yoffset + var->yres;

	/*
	 *  Memory limit
	 */
	line_length =
		get_line_length(var->xres_virtual, var->bits_per_pixel);
	if (videomemorysize &&  line_length * var->yres_virtual > videomemorysize)
		return -ENOMEM;

	/*
	 * Now that we checked it we alter var. The reason being is that the video
	 * mode passed in might not work but slight changes to it might make it
	 * work. This way we let the user know what is acceptable.
	 */
	switch (var->bits_per_pixel) {
	case 1:
	case 8:
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 0;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 15:		/* RGBA 555 */
		var->red.offset = 10;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 5;
		var->blue.offset = 0;
		var->blue.length = 5;
		break;
	case 16:		/* BGR 565 */
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 24:		/* RGB 888 */
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 32:		/* ARGB 8888 */
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		break;
	}
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;

	return 0;
}

static unsigned int cal_freq(unsigned int pixclock)
{
	unsigned int pstdiv, pll_ldf, pll_odf, pll_idf;
	unsigned int div = 0, ldf = 0, odf = 0, idf = 0, fref = 0;
	unsigned int min = 100000, a, fvco, fvcof, b;

	for (pstdiv = 1; pstdiv < 32; pstdiv++) {
		a = pixclock * pstdiv;
		for (pll_odf = 1; pll_odf <= 8; pll_odf *= 2) {
			fvco = a * pll_odf;
			if((fvco < 600000) || (fvco > 1800000))
				continue;
			for ( pll_idf = 1; pll_idf < 8; pll_idf++) {
				fref = 100000 / pll_idf;
				for ( pll_ldf = 8; pll_ldf < 256;
						pll_ldf++) {
					fvcof = fref * 2 * pll_ldf;
					if ((fvcof < 600000) ||
						(fvcof > 1800000))
						continue;
					b = (fvcof > fvco)?
						(fvcof - fvco):
						(fvco -fvcof);
					if (b < min) {
						min = b;
						div = pstdiv;
						ldf = pll_ldf;
						odf = pll_odf;
						idf = pll_idf;
					}
				}
			}
		}
	}

	pll_odf = (odf == 8)? 3 : (odf == 4)? 2 : (odf == 2)? 1 : 0;

	LS2H_DEBUG("pixclock = %d; min = %d; div = %d;"
			"ldf = %d; odf = %d; idf = %d\n",
			pixclock, min, div, ldf, odf, idf);

	a = (div << 24) | (ldf << 16) | (pll_odf << 5) | (idf << 2);
	return a;
}

static void ls2h_reset_cursor_image(void)
{
	u8 __iomem *addr = (u8 *)DEFAULT_CURSOR_MEM;
	memset(addr, 0, 32*32*4);
}

static int ls2h_init_regs(struct fb_info *info)
{
	unsigned int pix_freq;
	unsigned int out, depth, j;
	unsigned int hr, hss, hse, hfl;
	unsigned int vr, vss, vse, vfl;
	struct fb_var_screeninfo *var = &info->var;

	hr	= var->xres;
	hss	= hr + var->right_margin;
	hse	= hss + var->hsync_len;
	hfl	= hse + var->left_margin;

	vr	= var->yres;
	vss	= vr + var->lower_margin;
	vse	= vss + var->vsync_len;
	vfl	= vse + var->upper_margin;

	depth = var->bits_per_pixel;
	pix_freq = PICOS2KHZ(var->pixclock);

	out = cal_freq(pix_freq);
	/* change to refclk */
	ls2h_writel(0, LS2H_PIXCLK0_CTRL1_REG);
	ls2h_writel(0, LS2H_PIXCLK1_CTRL1_REG);
	/* reset pstiev */
	ls2h_writel((out | 0xc0000000), LS2H_PIXCLK0_CTRL0_REG);
	ls2h_writel((out | 0xc0000000), LS2H_PIXCLK1_CTRL0_REG);
	/* wait 10000ns */
	for (j = 1; j <= 1000; j++)
		ls2h_readl(LS2H_CHIP_SAMP0_REG);
	/* set pstdiv */
	ls2h_writel((out | 0x80000000), LS2H_PIXCLK0_CTRL0_REG);
	ls2h_writel((out | 0x80000000), LS2H_PIXCLK1_CTRL0_REG);
	/* wait 10000ns */
	for (j = 1; j <= 1000; j++)
		ls2h_readl(LS2H_CHIP_SAMP0_REG);

	/* pll_powerdown */
	ls2h_writel((out | 0x00000080), LS2H_PIXCLK0_CTRL0_REG);
	ls2h_writel((out | 0x00000080), LS2H_PIXCLK1_CTRL0_REG);
	/* wait 10000ns */
	for (j = 1; j <= 1000; j++)
		ls2h_readl(LS2H_CHIP_SAMP0_REG);
	/* pll_powerup set pll*/
	ls2h_writel(out, LS2H_PIXCLK0_CTRL0_REG);
	ls2h_writel(out, LS2H_PIXCLK1_CTRL0_REG);
	/* wait pll_lock */
	while ((ls2h_readl(LS2H_CHIP_SAMP0_REG) & 0x00001800) != 0x00001800) {
	}
	/* change to pllclk */
	ls2h_writel(0x1, LS2H_PIXCLK0_CTRL1_REG);
	ls2h_writel(0x1, LS2H_PIXCLK1_CTRL1_REG);

	/*Make DVO port putout the panel1 the same with VGA */
	ls2h_writel(0x0, LS2H_FB_CFG_DVO_REG);
	ls2h_writel(0x100200, LS2H_FB_CFG_DVO_REG);

/* VGA */
	ls2h_writel(0, LS2H_FB_CFG_VGA_REG);
	ls2h_writel(0x3, LS2H_FB_CFG_VGA_REG);
	ls2h_writel(dma_A, LS2H_FB_ADDR0_VGA_REG);
	ls2h_writel(dma_A, LS2H_FB_ADDR1_VGA_REG);
	ls2h_writel(0, LS2H_FB_DITCFG_VGA_REG);
	ls2h_writel(0, LS2H_FB_DITTAB_LO_VGA_REG);
	ls2h_writel(0, LS2H_FB_DITTAB_HI_VGA_REG);
	ls2h_writel(0x80001311, LS2H_FB_PANCFG_VGA_REG);
	ls2h_writel(0x80001311, LS2H_FB_PANTIM_VGA_REG);

	ls2h_writel((hfl << 16) | hr, LS2H_FB_HDISPLAY_VGA_REG);
	ls2h_writel(0x40000000 | (hse << 16) | hss, LS2H_FB_HSYNC_VGA_REG);
	ls2h_writel((vfl << 16) | vr, LS2H_FB_VDISPLAY_VGA_REG);
	ls2h_writel(0x40000000 | (vse << 16) | vss, LS2H_FB_VSYNC_VGA_REG);

	switch (depth) {
	case 32:
	case 24:
		ls2h_writel(0x00100104, LS2H_FB_CFG_VGA_REG);
		ls2h_writel((hr * 4 + 255) & ~255, LS2H_FB_STRI_VGA_REG);
		break;
	case 16:
		ls2h_writel(0x00100103, LS2H_FB_CFG_VGA_REG);
		ls2h_writel((hr * 2 + 255) & ~255, LS2H_FB_STRI_VGA_REG);
		break;
	case 15:
		ls2h_writel(0x00100102, LS2H_FB_CFG_VGA_REG);
		ls2h_writel((hr * 2 + 255) & ~255, LS2H_FB_STRI_VGA_REG);
		break;
	case 12:
		ls2h_writel(0x00100101, LS2H_FB_CFG_VGA_REG);
		ls2h_writel((hr * 2 + 255) & ~255, LS2H_FB_STRI_VGA_REG);
		break;
	default:
		ls2h_writel(0x00100104, LS2H_FB_CFG_VGA_REG);
		ls2h_writel((hr * 4 + 255) & ~255, LS2H_FB_STRI_VGA_REG);
		break;
	}

/* cursor */
	/* Select full color ARGB mode */
	ls2h_writel(0x00050212, LS2H_FB_CUR_CFG_REG);
	ls2h_writel(cursor_dma, LS2H_FB_CUR_ADDR_REG);
	ls2h_writel(0x00060122, LS2H_FB_CUR_LOC_ADDR_REG);
	ls2h_writel(0x00eeeeee, LS2H_FB_CUR_BACK_REG);
	ls2h_writel(0x00aaaaaa, LS2H_FB_CUR_FORE_REG);
	ls2h_reset_cursor_image();
/* enable interupt */
	ls2h_writel(0x280 << 16, LS2H_FB_INT_REG);
	return 0;
}

#ifdef LS2H_FB_DEBUG
void show_var(struct fb_var_screeninfo *var)
{
	printk(" xres: %d\n"
		" yres: %d\n",
		var->xres,
		var->yres);
}
#endif

/* This routine actually sets the video mode. It's in here where we
 * the hardware state info->par and fix which can be affected by the
 * change in par. For this driver it doesn't do much.
 */
static int ls2h_fb_set_par(struct fb_info *info)
{
	unsigned long flags;
	info->fix.line_length = get_line_length(info->var.xres_virtual,
						info->var.bits_per_pixel);
#ifdef LS2H_FB_DEBUG
	show_var(&info->var);
#endif
	spin_lock_irqsave(&fb_lock, flags);

	ls2h_init_regs(info);

	spin_unlock_irqrestore(&fb_lock, flags);
	return 0;
}

/*
 *  Set a single color register. The values supplied are already
 *  rounded down to the hardware's capabilities (according to the
 *  entries in the var structure). Return != 0 for invalid regno.
 */

static int ls2h_fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			 u_int transp, struct fb_info *info)
{
	if (regno >= 256)	/* no. of hw registers */
		return 1;
	/*
	 * Program hardware... do anything you want with transp
	 */

	/* grayscale works only partially under directcolor */
	if (info->var.grayscale) {
		/* grayscale = 0.30*R + 0.59*G + 0.11*B */
		red = green = blue =
			(red * 77 + green * 151 + blue * 28) >> 8;
	}

	/* Directcolor:
	 *   var->{color}.offset contains start of bitfield
	 *   var->{color}.length contains length of bitfield
	 *   {hardwarespecific} contains width of RAMDAC
	 *   cmap[X] is programmed to (X << red.offset) | (X << green.offset) | (X << blue.offset)
	 *   RAMDAC[X] is programmed to (red, green, blue)
	 *
	 * Pseudocolor:
	 *    uses offset = 0 && length = RAMDAC register width.
	 *    var->{color}.offset is 0
	 *    var->{color}.length contains widht of DAC
	 *    cmap is not used
	 *    RAMDAC[X] is programmed to (red, green, blue)
	 * Truecolor:
	 *    does not use DAC. Usually 3 are present.
	 *    var->{color}.offset contains start of bitfield
	 *    var->{color}.length contains length of bitfield
	 *    cmap is programmed to (red << red.offset) | (green << green.offset) |
	 *                      (blue << blue.offset) | (transp << transp.offset)
	 *    RAMDAC does not exist
	 */
#define CNVT_TOHW(val,width) ((((val)<<(width))+0x7FFF-(val))>>16)
	switch (info->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
	case FB_VISUAL_PSEUDOCOLOR:
		red = CNVT_TOHW(red, info->var.red.length);
		green = CNVT_TOHW(green, info->var.green.length);
		blue = CNVT_TOHW(blue, info->var.blue.length);
		transp = CNVT_TOHW(transp, info->var.transp.length);
		break;
	case FB_VISUAL_DIRECTCOLOR:
		red = CNVT_TOHW(red, 8);	/* expect 8 bit DAC */
		green = CNVT_TOHW(green, 8);
		blue = CNVT_TOHW(blue, 8);
		/* hey, there is bug in transp handling... */
		transp = CNVT_TOHW(transp, 8);
		break;
	}
#undef CNVT_TOHW
	/* Truecolor has hardware independent palette */
	if (info->fix.visual == FB_VISUAL_TRUECOLOR) {
		u32 v;

		if (regno >= 16)
			return 1;

		v = (red << info->var.red.offset) |
			(green << info->var.green.offset) |
			(blue << info->var.blue.offset) |
			(transp << info->var.transp.offset);
		switch (info->var.bits_per_pixel) {
		case 8:
			break;
		case 16:
			((u32 *) (info->pseudo_palette))[regno] = v;
			break;
		case 24:
		case 32:
			((u32 *) (info->pseudo_palette))[regno] = v;
			break;
		}
		return 0;
	}
	return 0;
}

static int ls2h_fb_blank (int blank_mode, struct fb_info *info)
{
	return 0;
}

/*************************************************************
 *                Hardware Cursor Routines                   *
 *************************************************************/

/**
 * ls2h_enable_cursor - show or hide the hardware cursor
 * @mode: show (1) or hide (0)
 *
 * Description:
 * Shows or hides the hardware cursor
 */
static void ls2h_enable_cursor(int mode)
{
	unsigned int tmp = ls2h_readl(LS2H_FB_CUR_CFG_REG);
	tmp &= ~0xff;
	ls2h_writel(mode ? (tmp | 0x12) : (tmp | 0x10),
			LS2H_FB_CUR_CFG_REG);
}

static void ls2h_load_cursor_image(int width, int height, u8 *data)
{
	u32 __iomem *addr = (u32 *)DEFAULT_CURSOR_MEM;
	int row, col, i, j, bit = 0;
	col = (width > CUR_HEIGHT_SIZE)? CUR_HEIGHT_SIZE : width;
	row = (height > CUR_WIDTH_SIZE)? CUR_WIDTH_SIZE : height;

	for (i = 0; i < CUR_HEIGHT_SIZE; i++) {
		for (j = 0; j < CUR_WIDTH_SIZE; j++) {
			if (i < height && j < width) {
				bit = data[(i * width + width - j) >> 3] &
					(1 << ((i * width + width - j) & 0x7));
				addr[i * CUR_WIDTH_SIZE + j] =
					bit ? 0xffffffff : 0;
			 } else {
				addr[i * CUR_WIDTH_SIZE + j] = 0x0;
			 }
		}
	}
}

static void ls2h_cur_set_hot(u16 x, u16 y)
{
	u32 hot = (x << 16) | (y << 8);
	u32 con = ls2h_readl(LS2H_FB_CUR_CFG_REG) & 0xff;
	ls2h_writel(hot | con, LS2H_FB_CUR_CFG_REG);
}

static int ls2h_fb_cursor(struct fb_info *info, struct fb_cursor *cursor)
{
	if (cursor->image.width > CUR_WIDTH_SIZE ||
			cursor->image.height > CUR_HEIGHT_SIZE)
		return -ENXIO;

	ls2h_enable_cursor(OFF);

	if (cursor->set & FB_CUR_SETPOS) {
		u32 tmp;

		tmp = (cursor->image.dx - info->var.xoffset) & 0xffff;
		tmp |= (cursor->image.dy - info->var.yoffset) << 16;
		ls2h_writel(tmp, LS2H_FB_CUR_LOC_ADDR_REG);
	}

	if (cursor->set & FB_CUR_SETSIZE)
		ls2h_reset_cursor_image();

	if (cursor->set & FB_CUR_SETHOT)
		ls2h_cur_set_hot(cursor->hot.x, cursor->hot.y);

	if (cursor->set & FB_CUR_SETCMAP)
		;

	if (cursor->set & (FB_CUR_SETSHAPE | FB_CUR_SETIMAGE)) {
		int size = ((cursor->image.width + 7) >> 3) *
			cursor->image.height;
		int i;
		u8 *data = kmalloc(32 * 32 * 4, GFP_ATOMIC);

		if (data == NULL)
			return -ENOMEM;

		switch (cursor->rop) {
		case ROP_XOR:
			for (i = 0; i < size; i++)
				data[i] = cursor->image.data[i] ^ cursor->mask[i];
			break;
		case ROP_COPY:
		default:
			for (i = 0; i < size; i++)
				data[i] = cursor->image.data[i] & cursor->mask[i];
			break;
		}

		ls2h_load_cursor_image(cursor->image.width,
				       cursor->image.height, data);
		kfree(data);
	}

	if (cursor->enable)
		ls2h_enable_cursor(ON);

	return 0;
}

struct cursor_req {
	u32 x;
	u32 y;
};

#define CURIOSET_CORLOR		0x4607
#define CURIOSET_POSITION	0x4608
#define CURIOLOAD_ARGB		0x4609
#define CURIOLOAD_IMAGE		0x460A
#define CURIOHIDE_SHOW		0x460B

static int ls2h_fb_ioctl(struct fb_info *info, unsigned int cmd,
		                        unsigned long arg)
{
	u32 tmp;
	struct cursor_req req;
	void __user *argp = (void __user *)arg;
	u8 *cursor_base = (u8 *)DEFAULT_CURSOR_MEM;

	switch (cmd) {
	case CURIOSET_CORLOR:
		break;
	case CURIOSET_POSITION:
		LS2H_DEBUG("CURIOSET_POSITION\n");
		if (copy_from_user(&req, argp, sizeof(struct cursor_req)))
			return -EFAULT;
		tmp = (req.x - info->var.xoffset) & 0xffff;
		tmp |= (req.y - info->var.yoffset) << 16;
		ls2h_writel(tmp, LS2H_FB_CUR_LOC_ADDR_REG);
		break;
	case CURIOLOAD_ARGB:
		LS2H_DEBUG("CURIOLOAD_ARGB\n");
		if (copy_from_user(cursor_base, argp, 32 * 32 * 4))
			return -EFAULT;
		break;
	case CURIOHIDE_SHOW:
		LS2H_DEBUG("CURIOHIDE_SHOW:%s\n", arg ? "show" : "hide");
		ls2h_enable_cursor(arg);
		break;
	default:
		return -ENOTTY;

	}

	return 0;
}


/*
 *  Pan or Wrap the Display
 *
 *  This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
 */

static int ls2h_fb_pan_display(struct fb_var_screeninfo *var,
			struct fb_info *info)
{
	if (var->vmode & FB_VMODE_YWRAP) {
		if (var->yoffset < 0
			|| var->yoffset >= info->var.yres_virtual
			|| var->xoffset)
			return -EINVAL;
	} else {
		if (var->xoffset + var->xres > info->var.xres_virtual ||
			var->yoffset + var->yres > info->var.yres_virtual)
			return -EINVAL;
	}
	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;
	if (var->vmode & FB_VMODE_YWRAP)
		info->var.vmode |= FB_VMODE_YWRAP;
	else
		info->var.vmode &= ~FB_VMODE_YWRAP;
	return 0;
}

#ifndef MODULE
static int __init ls2h_fb_setup(char *options)
{
	char *this_opt;
	ls2h_fb_enable = 1;

	if (!options || !*options)
		return 1;

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt)
			continue;
		if (!strncmp(this_opt, "disable", 7))
			ls2h_fb_enable = 0;
		else
			mode_option = this_opt;
	}
	return 1;
}
#endif	/* MODULE */

static unsigned char *fb_do_probe_ddc_edid(struct i2c_adapter *adapter)
{
	unsigned char start = 0x0;
	unsigned char *buf = kmalloc(EDID_LENGTH, GFP_KERNEL);
	struct i2c_msg msgs[] = {
		{
			.addr	= eeprom_info.addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &start,
		}, {
			.addr	= eeprom_info.addr,
			.flags	= I2C_M_RD,
			.len	= EDID_LENGTH,
			.buf	= buf,
		}
	};

	if (!buf) {
		dev_warn(&adapter->dev, "unable to allocate memory for EDID "
			 "block.\n");
		return NULL;
	}

	if (i2c_transfer(adapter, msgs, 2) == 2)
		return buf;

	dev_warn(&adapter->dev, "unable to read EDID block.\n");
	kfree(buf);
	return NULL;
}

static const struct i2c_device_id dvi_eep_ids[] = {
	{ "dvi-eeprom-edid", 0 },
	{ /* END OF LIST */ }
};

static const struct i2c_device_id vga_eep_ids[] = {
	{ "eeprom-edid", 2 },
	{ /* END OF LIST */ }
};
MODULE_DEVICE_TABLE(i2c, eep_ids);

static int eep_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	eeprom_info.adapter = client->adapter;
	eeprom_info.addr = client->addr;
	return 0;
}

static int eep_remove(struct i2c_client *client)
{

	i2c_unregister_device(client);
	return 0;
}
static struct i2c_driver vga_eep_driver = {
	.driver = {
		.name = "vga_eep-edid",
		.owner = THIS_MODULE,
	},
	.probe = eep_probe,
	.remove = eep_remove,
	.id_table = vga_eep_ids,
};

static struct i2c_driver dvi_eep_driver = {
	.driver = {
		.name = "dvi_eep-edid",
		.owner = THIS_MODULE,
	},
	.probe = eep_probe,
	.remove = eep_remove,
	.id_table = dvi_eep_ids,
};

static bool edid_flag = 0;
static unsigned char *ls2h_fb_i2c_connector(struct ls2h_fb_par *fb_par)
{
	unsigned char *edid = NULL;

	LS2H_DEBUG("edid entry\n");
	if ((read_c0_prid() & 0xf) != PRID_REV_LOONGSON3A)
		if (i2c_add_driver(&dvi_eep_driver)) {
			pr_err("i2c-%d No eeprom device register!",dvi_eep_driver.id_table->driver_data);
			return -ENODEV;
		}

	if (eeprom_info.adapter)
		edid = fb_do_probe_ddc_edid(eeprom_info.adapter);
	if ((read_c0_prid() & 0xf) != PRID_REV_LOONGSON3A)
		if (!edid) {
			if (i2c_add_driver(&vga_eep_driver)) {
				pr_err("i2c-%d No eeprom device register!",vga_eep_driver.id_table->driver_data);
				return -ENODEV;
			}
			edid_flag = 1;
			if (eeprom_info.adapter)
				edid = fb_do_probe_ddc_edid(eeprom_info.adapter);
		}

	if (edid)
		fb_par->edid = edid;

	return edid;
}

static void ls2h_find_init_mode(struct fb_info *info)
{
        struct fb_videomode mode;
        struct fb_var_screeninfo var;
        struct fb_monspecs *specs = &info->monspecs;
	struct ls2h_fb_par *par = info->par;
        int found = 0;
	unsigned char *edid;

        INIT_LIST_HEAD(&info->modelist);
        memset(&mode, 0, sizeof(struct fb_videomode));
        memset(&var, 0, sizeof(struct fb_var_screeninfo));
	var.bits_per_pixel = DEFAULT_BITS_PER_PIXEL;

	edid = ls2h_fb_i2c_connector(par);
	if (!edid)
		goto def;

        fb_edid_to_monspecs(par->edid, specs);

        if (specs->modedb == NULL) {
		printk("ls2h-fb: Unable to get Mode Database\n");
		goto def;
	}

        fb_videomode_to_modelist(specs->modedb, specs->modedb_len,
                                 &info->modelist);
        if (specs->modedb != NULL) {
                const struct fb_videomode *m;
                if (!found) {
                        m = fb_find_best_display(&info->monspecs, &info->modelist);
                        mode = *m;
                        found = 1;
                }

                fb_videomode_to_var(&var, &mode);
        }

        if (mode_option) {
		printk("mode_option: %s\n", mode_option);
                fb_find_mode(&var, info, mode_option, specs->modedb,
                             specs->modedb_len, (found) ? &mode : NULL,
                             info->var.bits_per_pixel);
	}

	info->var = var;
	fb_destroy_modedb(specs->modedb);
	specs->modedb = NULL;
	return;
def:
	info->var = ls2h_fb_default;
	return;
}

/* irq */
static irqreturn_t ls2hfb_irq(int irq, void *dev_id)
{
	unsigned int val, cfg;
	unsigned long flags;

	spin_lock_irqsave(&fb_lock, flags);

	val = ls2h_readl(LS2H_FB_INT_REG);
	ls2h_writel(val & (0xffff << 16), LS2H_FB_INT_REG);

	cfg = ls2h_readl(LS2H_FB_CFG_VGA_REG);
	/* if underflow, reset VGA */
	if (val & 0x280) {
		ls2h_writel(0, LS2H_FB_CFG_VGA_REG);
		ls2h_writel(cfg, LS2H_FB_CFG_VGA_REG);
	}

	spin_unlock_irqrestore(&fb_lock, flags);

	return IRQ_HANDLED;
}

static struct fb_ops ls2h_fb_ops = {
	.owner			= THIS_MODULE,
	.fb_check_var		= ls2h_fb_check_var,
	.fb_set_par		= ls2h_fb_set_par,
	.fb_setcolreg		= ls2h_fb_setcolreg,
	.fb_blank		= ls2h_fb_blank,
	.fb_pan_display		= ls2h_fb_pan_display,
	.fb_fillrect		= cfb_fillrect,
	.fb_copyarea		= cfb_copyarea,
	.fb_imageblit		= cfb_imageblit,
	.fb_cursor		= ls2h_fb_cursor,
	.fb_ioctl		= ls2h_fb_ioctl,
	.fb_compat_ioctl	= ls2h_fb_ioctl,
};

/*
 *  Initialisation
 */
static int ls2h_fb_probe(struct platform_device *dev)
{
	int irq;
	struct fb_info *info;
	int retval = -ENOMEM;
	struct ls2h_fb_par *par;

	irq = platform_get_irq(dev, 0);
	if (irq < 0) {
		dev_err(&dev->dev, "no irq for device\n");
		return -ENOENT;
	}

	info = framebuffer_alloc(sizeof(u32) * 256, &dev->dev);
	if (!info)
		return -ENOMEM;

	info->fix = ls2h_fb_fix;
	info->node = -1;
	info->fbops = &ls2h_fb_ops;
	info->pseudo_palette = info->par;
	info->flags = FBINFO_FLAG_DEFAULT;

	par = kzalloc(sizeof(struct ls2h_fb_par), GFP_KERNEL);
	if (!par) {
		retval = -ENOMEM;
		goto release_info;
	}

	info->par = par;
	par->fb_info = info;
	par->pdev = dev;
	par->irq = irq;
	ls2h_find_init_mode(info);

	if (!videomemorysize) {
		videomemorysize = info->var.xres_virtual *
					info->var.yres_virtual *
					info->var.bits_per_pixel / 8;
	}

	/*
	 * For real video cards we use ioremap.
	 */
	videomemory = (void *)DEFAULT_FB_MEM;
	dma_A = (dma_addr_t)DEFAULT_FB_DMA;

	pr_info("videomemory=%lx\n",(unsigned long)videomemory);
	pr_info("videomemorysize=%lx\n",videomemorysize);
	pr_info("dma_A=%x\n",(int)dma_A);
	memset(videomemory, 0, videomemorysize);

	cursor_mem = (void *)DEFAULT_CURSOR_MEM;
	cursor_dma = (dma_addr_t)DEFAULT_CURSOR_DMA;
	memset (cursor_mem,0x88FFFF00,cursor_size);

	info->screen_base = (char __iomem *)videomemory;
	info->fix.smem_start = DEFAULT_PHY_ADDR;
	info->fix.smem_len = videomemorysize;

	retval = fb_alloc_cmap(&info->cmap, 32, 0);
	if (retval < 0) goto release_par;

	info->fbops->fb_check_var(&info->var, info);
	par->htotal = info->var.xres;
	par->vtotal = info->var.yres;
	retval = register_framebuffer(info);
	if (retval < 0)
		goto release_map;

	retval = request_irq(irq, ls2hfb_irq, IRQF_DISABLED, dev->name, info);
	if (retval) {
		dev_err(&dev->dev, "cannot get irq %d - err %d\n", irq, retval);
		goto unreg_info;
	}

	platform_set_drvdata(dev, info);

	pr_info("fb%d: Virtual frame buffer device, using %ldK of"
			"video memory\n", info->node, videomemorysize >> 10);

	return 0;
unreg_info:
	unregister_framebuffer(info);
release_map:
	fb_dealloc_cmap(&info->cmap);
release_par:
	kfree(par);
release_info:
	platform_set_drvdata(dev, NULL);
	framebuffer_release(info);
	return retval;
}

static int ls2h_fb_remove(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);
	struct ls2h_fb_par *par = info->par;
	int irq = par->irq;

	free_irq(irq, info);
	fb_dealloc_cmap(&info->cmap);
	unregister_framebuffer(info);
	platform_set_drvdata(dev, info);
	framebuffer_release(info);
	if (par->edid)
		kfree(par->edid);
	kfree(par);

	return 0;
}

#if     defined(CONFIG_CPU_LOONGSON3)&&defined(CONFIG_SUSPEND)
/**
 **      ls2h_fb_suspend - Suspend the device.
 **      @dev: platform device
 **      @msg: the suspend event code.
 **
 **      See Documentation/power/devices.txt for more information
 **/
static u32 output_mode;
void console_lock(void);
void console_unlock(void);

static int ls2h_fb_suspend(struct platform_device *dev, pm_message_t msg)
{
	struct fb_info *info = platform_get_drvdata(dev);

	console_lock();
	fb_set_suspend(info, 1);
	console_unlock();

	output_mode = ls2h_readl(LS2H_FB_DVO_OUTPUT_REG);

	return 0;
}

/**
 **      ls2h_fb_resume - Resume the device.
 **      @dev: platform device
 **
 **      See Documentation/power/devices.txt for more information
 **/
static int ls2h_fb_resume(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);

	ls2h_fb_set_par(info);
	ls2h_writel(output_mode, LS2H_FB_DVO_OUTPUT_REG);

	console_lock();
	fb_set_suspend(info, 0);
	console_unlock();

	return 0;
}
#endif

static struct platform_driver ls2h_fb_driver = {
	.probe	= ls2h_fb_probe,
	.remove = ls2h_fb_remove,
#ifdef	CONFIG_SUSPEND
	.suspend = ls2h_fb_suspend,
	.resume	 = ls2h_fb_resume,
#endif
	.driver = {
		.name	= "ls2h-fb",
	},
};

static const struct i2c_device_id eep_ids[] = {
	{ "eeprom-edid", 0 },
	{ /* END OF LIST */ }
};

MODULE_DEVICE_TABLE(i2c, eep_ids);

static struct i2c_driver eep_driver = {
	.driver = {
		.name = "eep-edid",
		.owner = THIS_MODULE,
	},
	.probe = eep_probe,
	.remove = eep_remove,
	.id_table = eep_ids,
};

static int __init ls2h_fb_init(void)
{
	int ret = 0;
        struct pci_dev *pdev = NULL;

        pdev = pci_get_device(PCI_VENDOR_ID_ATI, PCI_ANY_ID, NULL);
        if(pdev)
                return -ENODEV;

#ifndef MODULE
	char *option = NULL;

	if (fb_get_options("ls2h-fb", &option))
		return -ENODEV;
	ls2h_fb_setup(option);
#endif

	if (!ls2h_fb_enable)
		return -ENXIO;

if ((read_c0_prid() & 0xf) == PRID_REV_LOONGSON3A)
	if (i2c_add_driver(&eep_driver)) {
		pr_err("No eeprom device register!");
		return -ENODEV;
	}

	ret = platform_driver_register(&ls2h_fb_driver);

	return ret;
}

module_init(ls2h_fb_init);

#ifdef MODULE
static void __exit ls2h_fb_exit(void)
{
	platform_driver_unregister(&ls2h_fb_driver);
if ((read_c0_prid() & 0xf) != PRID_REV_LOONGSON3A){
	i2c_del_driver(&dvi_eep_driver);
	if (edid_flag)
		i2c_del_driver(&vga_eep_driver);
}else
	i2c_del_driver(&eep_driver);
}

module_exit(ls2h_fb_exit);

MODULE_LICENSE("GPL");
#endif				/* MODULE */
