#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <asm/bootinfo.h>
#include <ec_wpce775l.h>
#include <workarounds.h>
#include <asm/sections.h>
#include <asm/uasm.h>
#include <asm/regdef.h>

#include <loongson.h>
#define GPIO_LCD_CNTL		5
#define GPIO_BACKLIGHIT_CNTL	7
#define SYNCI   0x041f0000
#define SYNC    0xf
#define RA      31

unsigned int last_timer_irq_count[NR_CPUS];
extern void (* r4k_blast_scache_node)(long node);

void loongson3_cache_stall_unlock(int cpu, int irq)
{
	unsigned int core_id;
	unsigned int node_id;
	int curr_timer_irq_count;
	int i;
	unsigned int cpu_next = cpu;
	struct irq_desc *desc = irq_to_desc(irq);

	if (!desc)
		return;

	if (*per_cpu_ptr(desc->kstat_irqs, cpu) & 0xFF)
		return;

	core_id = cpu_data[cpu].core;
	node_id = cpu_data[cpu].package;

	for (i = 1; i <= cores_per_package; i++)
	{
		cpu_next = (node_id << 2) + ((core_id + i) % cores_per_package);
		if (cpu_online(cpu_next))
			break;
	}

	/* Current package has only on core online */
	if (cpu_next == cpu)
		return;

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

void loongson3_local_irq_optimize(unsigned int cpu_type)
{
	unsigned int *p;

	switch(cpu_type) {
	case PRID_REV_LOONGSON3A_R1:
	case PRID_REV_LOONGSON3B_R1:
	case PRID_REV_LOONGSON3B_R2:
	/* do noting for legacy processors. */
		break;
	default:
		/* optimize arch_local_irq_disable */
		p = (unsigned int *)&arch_local_irq_disable;
		uasm_i_di(&p, 0);
		uasm_i_jr(&p, RA);
		uasm_i_nop(&p);
		break;
	}
}

/*
 * for 3A1000 synci will lead to crash, so use the sync intead of synci
 * in the vmlinux range
 */
void  loongson3_inst_fixup(void)
{
	unsigned int cpu_type = read_c0_prid() & 0xF;

	if (cpu_type == PRID_REV_LOONGSON3A_R1) {
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

	loongson3_local_irq_optimize(cpu_type);
}
EXPORT_SYMBOL(loongson3_inst_fixup);
