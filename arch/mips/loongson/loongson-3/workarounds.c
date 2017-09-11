#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <asm/bootinfo.h>
#include <ec_wpce775l.h>
#include <workarounds.h>
#include <asm/sections.h>

#include <loongson.h>
#define GPIO_LCD_CNTL		5
#define GPIO_BACKLIGHIT_CNTL	7
#define SYNCI   0x041f0000
#define SYNC    0xf

unsigned int last_timer_irq_count[NR_CPUS];
extern void (* r4k_blast_scache_node)(long node);

void loongson3_cache_stall_unlock(int cpu, int irq)
{
	unsigned int core_id;
	unsigned int node_id;
	unsigned int cpu_next;
	int curr_timer_irq_count;
	struct irq_desc *desc = irq_to_desc(irq);

	if (!desc)
		return;

	if (*per_cpu_ptr(desc->kstat_irqs, cpu) & 0x3F)
		return;

	core_id = cpu_data[cpu].core;
	node_id = cpu_data[cpu].package;

	cpu_next = cpu + 1;
	if (core_id == (cores_per_package - 1))
		cpu_next -= cores_per_package;

	curr_timer_irq_count = *per_cpu_ptr(desc->kstat_irqs, cpu_next);

	if (curr_timer_irq_count == last_timer_irq_count[cpu_next])
		r4k_blast_scache_node(node_id);
	else
		last_timer_irq_count[cpu_next] = curr_timer_irq_count;
}
EXPORT_SYMBOL(loongson3_cache_stall_unlock);

void gpio_lvds_off(void)
{
	gpio_direction_output(GPIO_BACKLIGHIT_CNTL, 0);
	gpio_direction_output(GPIO_LCD_CNTL, 0);
}

static void gpio_lvds_on(void)
{
	gpio_direction_output(GPIO_LCD_CNTL, 1);
	msleep(250);
	gpio_direction_output(GPIO_BACKLIGHIT_CNTL, 1);
}

void turn_off_lvds(void)
{
	if (loongson_workarounds & WORKAROUND_LVDS_EC)
		ec_write(INDEX_BACKLIGHT_STSCTRL, BACKLIGHT_OFF);
	if (loongson_workarounds & WORKAROUND_LVDS_GPIO)
		gpio_lvds_off();
}
EXPORT_SYMBOL(turn_off_lvds);

void turn_on_lvds(void)
{
	if (loongson_workarounds & WORKAROUND_LVDS_EC)
		ec_write(INDEX_BACKLIGHT_STSCTRL, BACKLIGHT_ON);
	if (loongson_workarounds & WORKAROUND_LVDS_GPIO)
		gpio_lvds_on();
}
EXPORT_SYMBOL(turn_on_lvds);

static int __init usb_fix_for_tmcs(void)
{
	if (loongson_workarounds & WORKAROUND_USB_TMCS) {
		gpio_request(13, "gpio13");
		gpio_request(12, "gpio12");
		gpio_request(11, "gpio11");
		gpio_request(9, "gpio9");
		gpio_request(8, "gpio8");

		gpio_direction_output(11, 1);
		msleep(1000);
		gpio_direction_output(13, 0);
		gpio_direction_output(12, 0);
		gpio_direction_output(9, 0);
		gpio_direction_output(8, 0);

		gpio_set_value(8, 1);
		gpio_set_value(13, 1);
		msleep(1000);
		gpio_set_value(9, 1);
		gpio_set_value(12, 1);
	}

	return 0;
}

late_initcall(usb_fix_for_tmcs);

/*
 * for 3A1000 synci will lead to crash, so use the sync intead of synci
 * in the vmlinux range
 */
void  loongson3_inst_fixup(void)
{
	if ((read_c0_prid() & 0xf) == PRID_REV_LOONGSON3A_R1) {
			unsigned int *inst_ptr = (unsigned int *)&_text;
			unsigned int *inst_ptr_init = (unsigned int *)&__init_begin;
			while(inst_ptr <= (unsigned int *)&_etext)
			{
					if (*inst_ptr == SYNCI)
							*inst_ptr = SYNC;
					inst_ptr++;
			}

			while(inst_ptr_init <= (unsigned int *)&__init_end)
			{
					if (*inst_ptr_init == SYNCI)
							*inst_ptr_init = SYNC;
					inst_ptr_init++;
			}
	}
}
EXPORT_SYMBOL(loongson3_inst_fixup);
