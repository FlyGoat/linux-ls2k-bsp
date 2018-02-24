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
#define V0      2
#define A0      4
#define RA      31
#define C0_STATUS               12, 0

enum label_id {
        label_irq_restore_out = 1,
        label___irq_restore_out,
};

UASM_L_LA(_irq_restore_out)
UASM_L_LA(___irq_restore_out)

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

#if defined(CONFIG_LOONGSON_EA_PM_HOTKEY)
void ea_turn_off_lvds(void);
void ea_turn_on_lvds(void);
#endif

void turn_off_lvds(void)
{
	if (loongson_workarounds & WORKAROUND_LVDS_EC)
		ec_write(INDEX_BACKLIGHT_STSCTRL, BACKLIGHT_OFF);
	if (loongson_workarounds & WORKAROUND_LVDS_GPIO)
		gpio_lvds_off();
#if defined(CONFIG_LOONGSON_EA_PM_HOTKEY)
	if (loongson_workarounds & WORKAROUND_LVDS_EA_EC)
		ea_turn_off_lvds();
#endif
}
EXPORT_SYMBOL(turn_off_lvds);

void turn_on_lvds(void)
{
	if (loongson_workarounds & WORKAROUND_LVDS_EC)
		ec_write(INDEX_BACKLIGHT_STSCTRL, BACKLIGHT_ON);
	if (loongson_workarounds & WORKAROUND_LVDS_GPIO)
		gpio_lvds_on();
#if defined(CONFIG_LOONGSON_EA_PM_HOTKEY)
	if (loongson_workarounds & WORKAROUND_LVDS_EA_EC)
		ea_turn_on_lvds();
#endif
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

static inline int loongson3_ccnuma_platform(void)
{
	struct loongson_params *loongson_p;
	struct efi_cpuinfo_loongson *ecpu;

	loongson_p = &(((struct boot_params *)fw_arg2)->efi.smbios.lp);
	ecpu = (struct efi_cpuinfo_loongson *)((u64)loongson_p + loongson_p->cpu_offset);

	if (ecpu->nr_cpus <= 4)
		return 0;

	return 1;
}

extern u32 nr_nodes_loongson;
void loongson3_arch_func_optimize(unsigned int cpu_type)
{
	unsigned int *p;
	struct uasm_label labels[3];
	struct uasm_reloc relocs[3];
	struct uasm_label *l = labels;
	struct uasm_reloc *r = relocs;

	if (!loongson3_ccnuma_platform()) {
#ifdef CONFIG_PHASE_LOCK
		/* optimize loongson3_phase_lock_acquire for SMP */
		p = (unsigned int *)&loongson3_phase_lock_acquire;
		uasm_i_jr(&p, RA);
		uasm_i_nop(&p);

		/* optimize loongson3_phase_lock_release for SMP */
		p = (unsigned int *)&loongson3_phase_lock_release;
		uasm_i_jr(&p, RA);
		uasm_i_nop(&p);
#endif
	}

	switch(cpu_type) {
	case PRID_REV_LOONGSON3A_R1:
	case PRID_REV_LOONGSON3B_R1:
	case PRID_REV_LOONGSON3B_R2:
	/* do noting for legacy processors. */
		break;
	default:
		memset(labels, 0, sizeof(labels));
		memset(relocs, 0, sizeof(relocs));

		/* optimize arch_local_irq_disable */
		p = (unsigned int *)&arch_local_irq_disable;
		uasm_i_di(&p, 0);
		uasm_i_jr(&p, RA);
		uasm_i_nop(&p);

		/* optimize arch_local_irq_save */
		p = (unsigned int *)&arch_local_irq_save;
		if (cpu_type == PRID_REV_LOONGSON3A_R2) {
			uasm_i_mfc0(&p, V0, C0_STATUS);
			uasm_i_di(&p, 0);
		}
		else
			uasm_i_di(&p, V0);
		uasm_i_andi(&p, V0, V0, 0x1);
		uasm_i_jr(&p, RA);
		uasm_i_nop(&p);

		/* optimize arch_local_irq_restore */
		p = (unsigned int *)&arch_local_irq_restore;
		uasm_il_beqz(&p, &r, A0, label_irq_restore_out);
		uasm_i_di(&p, 0);
		uasm_i_ei(&p, 0);
		uasm_l_irq_restore_out(&l, p);
		uasm_i_jr(&p, RA);
		uasm_i_nop(&p);

		/* optimize __arch_local_irq_restore */
		p = (unsigned int *)&__arch_local_irq_restore;
		uasm_il_beqz(&p, &r, A0, label___irq_restore_out);
		uasm_i_di(&p, 0);
		uasm_i_ei(&p, 0);
		uasm_l___irq_restore_out(&l, p);
		uasm_i_jr(&p, RA);
		uasm_i_nop(&p);

		uasm_resolve_relocs(relocs, labels);
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

	/* GS464 do not support synci, and sync is just enough for GS464. */
	if (cpu_type == PRID_REV_LOONGSON3A_R1) {
		unsigned int *inst_ptr = (unsigned int *)&_text;
		unsigned int *inst_ptr_init = (unsigned int *)&__init_begin;
		while(inst_ptr <= (unsigned int *)&_etext) {
			if (*inst_ptr == SYNCI)
				*inst_ptr = SYNC;
			inst_ptr++;
		}

		while(inst_ptr_init <= (unsigned int *)&__init_end) {
			if (*inst_ptr_init == SYNCI)
				*inst_ptr_init = SYNC;
			inst_ptr_init++;
		}
	}

	loongson3_arch_func_optimize(cpu_type);
}
EXPORT_SYMBOL(loongson3_inst_fixup);
