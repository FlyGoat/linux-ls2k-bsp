#include <loongson.h>
#include <irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>

#include <asm/irq_cpu.h>
#include <asm/i8259.h>
#include <asm/mipsregs.h>

#include <loongson-pch.h>

#define USE_7A_INT0 0x1
#define USE_7A_INT1 0x2

static volatile unsigned long long *irq_status = (volatile unsigned long long *)((LS7A_IOAPIC_INT_STATUS));
static volatile unsigned long long *irq_mask   = (volatile unsigned long long *)((LS7A_IOAPIC_INT_MASK  ));
static volatile unsigned long long *irq_edge   = (volatile unsigned long long *)((LS7A_IOAPIC_INT_EDGE  ));
static volatile unsigned long long *irq_clear  = (volatile unsigned long long *)((LS7A_IOAPIC_INT_CLEAR ));
static volatile unsigned long long *irq_msi_en = (volatile unsigned long long *)((LS7A_IOAPIC_HTMSI_EN  ));
static volatile unsigned char *irq_msi_vec = (volatile unsigned char *)((LS7A_IOAPIC_HTMSI_VEC ));

extern unsigned long long smp_group[4];
extern void loongson3_send_irq_by_ipi(int cpu, int irqs);

static DEFINE_SPINLOCK(pch_irq_lock);
extern int ls3a_msi_enabled;

/* Maximum 26 IPI irqs */
#define PCH_DIRQS 26
unsigned char ls7a_ipi_irq2pos[128] = { [0 ... 63] = -1 };
unsigned char ls7a_ipi_pos2irq[64] = { [0 ... 63] = -1 };
static DECLARE_BITMAP(pch_irq_in_use, PCH_DIRQS);

int pch_create_dirq(unsigned int irq)
{
	unsigned long flags;
	int pos;
	spin_lock_irqsave(&pch_irq_lock, flags);
again:
	pos = find_first_zero_bit(pch_irq_in_use, PCH_DIRQS);
	if(pos == PCH_DIRQS)
	{
		spin_unlock_irqrestore(&pch_irq_lock, flags);
		return -ENOSPC;
	}
	if (test_and_set_bit(pos, pch_irq_in_use))
		goto again;
	ls7a_ipi_pos2irq[pos] = irq;
	ls7a_ipi_irq2pos[irq] = pos;
	spin_unlock_irqrestore(&pch_irq_lock, flags);
	return 0;
}

void pch_destroy_dirq(unsigned int irq)
{
	unsigned long flags;
	int pos;
	spin_lock_irqsave(&pch_irq_lock, flags);
	pos = ls7a_ipi_irq2pos[irq];

	if(pos >= 0)
	{
		clear_bit(pos, pch_irq_in_use);
		ls7a_ipi_irq2pos[irq] = -1;
		ls7a_ipi_pos2irq[pos] = -1;
	}
	spin_unlock_irqrestore(&pch_irq_lock, flags);
}

static void mask_pch_irq(struct irq_data *d);
static void unmask_pch_irq(struct irq_data *d);

unsigned int startup_pch_irq(struct irq_data *d)
{
	pch_create_dirq(d->irq);
	unmask_pch_irq(d);
	return 0;
}

void shutdown_pch_irq(struct irq_data *d)
{
	mask_pch_irq(d);
	pch_destroy_dirq(d->irq);
}

static void mask_pch_irq(struct irq_data *d)
{
	unsigned long flags;

	unsigned long irq_nr = d->irq;

	if(ls3a_msi_enabled)
		return;

	spin_lock_irqsave(&pch_irq_lock, flags);

	*irq_mask |= (1ULL << (irq_nr -  LS7A_IOAPIC_IRQ_BASE));

	spin_unlock_irqrestore(&pch_irq_lock, flags);
}

static void unmask_pch_irq(struct irq_data *d)
{
	unsigned long flags;

	unsigned long irq_nr = d->irq;

	spin_lock_irqsave(&pch_irq_lock, flags);

	if(ls3a_msi_enabled)
		*irq_clear |= 1ULL << (irq_nr - LS7A_IOAPIC_IRQ_BASE);
	else
		*irq_mask &= ~(1ULL << (irq_nr - LS7A_IOAPIC_IRQ_BASE));

	spin_unlock_irqrestore(&pch_irq_lock, flags);
}

static struct irq_chip pch_irq_chip = {
	.name        = "LS7A-IOAPIC",
	.irq_mask    = mask_pch_irq,
	.irq_unmask    = unmask_pch_irq,
	.irq_startup	= startup_pch_irq,
	.irq_shutdown	= shutdown_pch_irq,
	.irq_set_affinity = plat_set_irq_affinity,
};

static unsigned int irq_cpu[64] = {[0 ... 63] = -1};
static unsigned int irq_msi[64] = {[0 ... 63] = -1};
static unsigned long long local_irq = (1ULL << LS7A_IOAPIC_UART0_OFFSET         ) |
										(1ULL << LS7A_IOAPIC_I2C0_OFFSET          ) |
										(1ULL << LS7A_IOAPIC_GMAC0_PMT_OFFSET     ) |
										(1ULL << LS7A_IOAPIC_GMAC1_PMT_OFFSET     ) |
										(1ULL << LS7A_IOAPIC_DC_OFFSET            ) |
										(1ULL << LS7A_IOAPIC_GPU_OFFSET           ) |
										(1ULL << LS7A_IOAPIC_OHCI0_OFFSET         ) |
										(1ULL << LS7A_IOAPIC_OHCI1_OFFSET         ) |
										(1ULL << LS7A_IOAPIC_ACPI_INT_OFFSET      ) |
										(1ULL << LS7A_IOAPIC_HPET_INT_OFFSET      ) ;


void handle_7a_irqs(unsigned long long irqs) {
	unsigned int  irq;
	struct irq_data *irqd;
	struct cpumask affinity;
	int cpu = smp_processor_id();

	while(irqs){
		irq = __ffs(irqs);
		irqs &= ~(1ULL<<irq);

		/* handled by local core */
		if ((local_irq & (0x1ULL << irq)) || ls7a_ipi_irq2pos[LS7A_IOAPIC_IRQ_BASE + irq] == (unsigned char)-1) {
			do_IRQ(LS7A_IOAPIC_IRQ_BASE + irq);
			continue;
		}

		irqd = irq_get_irq_data(LS7A_IOAPIC_IRQ_BASE + irq);
		cpumask_and(&affinity, irqd->affinity, cpu_active_mask);
		if (cpumask_empty(&affinity)) {
			do_IRQ(LS7A_IOAPIC_IRQ_BASE + irq);
			continue;
		}

		irq_cpu[irq] = cpumask_next(irq_cpu[irq], &affinity);
		if (irq_cpu[irq] >= nr_cpu_ids)
			irq_cpu[irq] = cpumask_first(&affinity);

		if (irq_cpu[irq] == cpu) {
			do_IRQ(LS7A_IOAPIC_IRQ_BASE + irq);
			continue;
		}

		/* balanced by other cores */
		loongson3_send_irq_by_ipi(irq_cpu[irq], (0x1<<(ls7a_ipi_irq2pos[LS7A_IOAPIC_IRQ_BASE + irq])));
	}
}

void handle_msi_irqs(unsigned long long irqs) {
	unsigned int  irq;
	struct irq_data *irqd;
	struct cpumask affinity;
	int cpu = smp_processor_id();

	while(irqs){
		irq = __ffs(irqs);
		irqs &= ~(1ULL<<irq);


		irqd = irq_get_irq_data(irq);
		cpumask_and(&affinity, irqd->affinity, cpu_active_mask);
		if (cpumask_empty(&affinity)) {
			do_IRQ(irq);
			continue;
		}

		irq_msi[irq] = cpumask_next(irq_msi[irq], &affinity);
		if (irq_msi[irq] >= nr_cpu_ids)
			irq_msi[irq] = cpumask_first(&affinity);

		if (irq_msi[irq] == cpu || ls7a_ipi_irq2pos[irq] == (unsigned char)-1 ) {
			do_IRQ(irq);
			continue;
		}

		/* balanced by other cores */
		loongson3_send_irq_by_ipi(irq_msi[irq], (0x1<<(ls7a_ipi_irq2pos[irq])));
	}
}

void ls7a_irq_dispatch(void)
{
	unsigned long flags;
	volatile unsigned long long intmask;

	/* read irq status register */
	unsigned long long intstatus = *irq_status;

	spin_lock_irqsave(&pch_irq_lock, flags);
	*irq_mask |= intstatus;
	intmask = *irq_mask;
	spin_unlock_irqrestore(&pch_irq_lock, flags);

	handle_7a_irqs(intstatus);
}

void ls7a_msi_irq_dispatch(void)
{
	unsigned int i;
	unsigned long long  irqs;
	int cpu = smp_processor_id();

	unsigned int ls3a_irq_cpu = LOONGSON_INT_ROUTER_ISR(cpu);

	for(i=0; i<2;i++) {
		if(ls3a_irq_cpu & 0x3<<(24+i*2)) {
			irqs = LOONGSON_HT1_INT_VECTOR64(i);
			LOONGSON_HT1_INT_VECTOR64(i) = irqs;

			if(i==1) {
				handle_7a_irqs(irqs);
			} else {
				handle_msi_irqs(irqs);
			}
		}
	}
}

static void init_7a_irq(int dev, int irq) {
	*irq_mask  &= ~(1ULL << dev);
	*(volatile unsigned char *)(LS7A_IOAPIC_ROUTE_ENTRY + dev) = USE_7A_INT0;
	irq_set_chip_and_handler(irq, &pch_irq_chip, handle_level_irq);
	if(ls3a_msi_enabled) {
		*irq_msi_en |= 1ULL << dev;
		*(volatile unsigned char *)(irq_msi_vec+dev) = irq;
	}
}

void init_7a_irqs(void)
{
	*irq_edge   = 0ULL;
	*irq_clear  = 0ULL;
	*irq_status = 0ULL;
	*irq_mask   = 0xffffffffffffffffULL;

	init_7a_irq(LS7A_IOAPIC_UART0_OFFSET        , LS7A_IOAPIC_UART0_IRQ        );
	init_7a_irq(LS7A_IOAPIC_I2C0_OFFSET         , LS7A_IOAPIC_I2C0_IRQ         );
	init_7a_irq(LS7A_IOAPIC_GMAC0_OFFSET        , LS7A_IOAPIC_GMAC0_IRQ        );
	init_7a_irq(LS7A_IOAPIC_GMAC0_PMT_OFFSET    , LS7A_IOAPIC_GMAC0_PMT_IRQ    );
	init_7a_irq(LS7A_IOAPIC_GMAC1_OFFSET        , LS7A_IOAPIC_GMAC1_IRQ        );
	init_7a_irq(LS7A_IOAPIC_GMAC1_PMT_OFFSET    , LS7A_IOAPIC_GMAC1_PMT_IRQ    );
	init_7a_irq(LS7A_IOAPIC_SATA0_OFFSET        , LS7A_IOAPIC_SATA0_IRQ        );
	init_7a_irq(LS7A_IOAPIC_SATA1_OFFSET        , LS7A_IOAPIC_SATA1_IRQ        );
	init_7a_irq(LS7A_IOAPIC_SATA2_OFFSET        , LS7A_IOAPIC_SATA2_IRQ        );
	init_7a_irq(LS7A_IOAPIC_DC_OFFSET           , LS7A_IOAPIC_DC_IRQ           );
	init_7a_irq(LS7A_IOAPIC_GPU_OFFSET          , LS7A_IOAPIC_GPU_IRQ          );
	init_7a_irq(LS7A_IOAPIC_EHCI0_OFFSET        , LS7A_IOAPIC_EHCI0_IRQ        );
	init_7a_irq(LS7A_IOAPIC_OHCI0_OFFSET        , LS7A_IOAPIC_OHCI0_IRQ        );
	init_7a_irq(LS7A_IOAPIC_EHCI1_OFFSET        , LS7A_IOAPIC_EHCI1_IRQ        );
	init_7a_irq(LS7A_IOAPIC_OHCI1_OFFSET        , LS7A_IOAPIC_OHCI1_IRQ        );
	init_7a_irq(LS7A_IOAPIC_PCIE_F0_PORT0_OFFSET, LS7A_IOAPIC_PCIE_F0_PORT0_IRQ);
	init_7a_irq(LS7A_IOAPIC_PCIE_F0_PORT1_OFFSET, LS7A_IOAPIC_PCIE_F0_PORT1_IRQ);
	//init_7a_irq(LS7A_IOAPIC_PCIE_F0_PORT2_OFFSET, LS7A_IOAPIC_PCIE_F0_PORT2_IRQ);
	//init_7a_irq(LS7A_IOAPIC_PCIE_F0_PORT3_OFFSET, LS7A_IOAPIC_PCIE_F0_PORT3_IRQ);
	init_7a_irq(LS7A_IOAPIC_PCIE_F1_PORT0_OFFSET, LS7A_IOAPIC_PCIE_F1_PORT0_IRQ);
	//init_7a_irq(LS7A_IOAPIC_PCIE_F1_PORT1_OFFSET, LS7A_IOAPIC_PCIE_F1_PORT1_IRQ);
	init_7a_irq(LS7A_IOAPIC_PCIE_H_LO_OFFSET    , LS7A_IOAPIC_PCIE_H_LO_IRQ    );
	//init_7a_irq(LS7A_IOAPIC_PCIE_H_HI_OFFSET    , LS7A_IOAPIC_PCIE_H_HI_IRQ    );
	init_7a_irq(LS7A_IOAPIC_PCIE_G0_LO_OFFSET   , LS7A_IOAPIC_PCIE_G0_LO_IRQ   );
	//init_7a_irq(LS7A_IOAPIC_PCIE_G0_HI_OFFSET   , LS7A_IOAPIC_PCIE_G0_HI_IRQ   );
	init_7a_irq(LS7A_IOAPIC_PCIE_G1_LO_OFFSET   , LS7A_IOAPIC_PCIE_G1_LO_IRQ   );
	//init_7a_irq(LS7A_IOAPIC_PCIE_G1_HI_OFFSET   , LS7A_IOAPIC_PCIE_G1_HI_IRQ   );
	init_7a_irq(LS7A_IOAPIC_ACPI_INT_OFFSET     , LS7A_IOAPIC_ACPI_INT_IRQ     );
	init_7a_irq(LS7A_IOAPIC_HPET_INT_OFFSET     , LS7A_IOAPIC_HPET_INT_IRQ     );
	init_7a_irq(LS7A_IOAPIC_AC97_HDA_OFFSET     , LS7A_IOAPIC_AC97_HDA_IRQ     );
}

static void ls7a_irq_router_init(void)
{
	int i;
	/* route LPC int to cpu core0 int 0 */
	LOONGSON_INT_ROUTER_LPC = LOONGSON_INT_COREx_INTy(loongson_boot_cpu_id, 0);
	LOONGSON_INT_ROUTER_INTENSET = LOONGSON_INT_ROUTER_INTEN | 0x1 << 10;

	if(ls3a_msi_enabled) {
		*(volatile int *)(LOONGSON_HT1_CFG_BASE+0x58) = 0x400;//strip1, ht_int 8bit
		/* route HT1 int0 ~ int3 */
		for (i = 0; i < 2; i++) {
			LOONGSON_INT_ROUTER_HT1(i) = 1<<5 | 0xf;
			LOONGSON_HT1_INTN_EN(i) = 0xffffffff;
			LOONGSON_INT_ROUTER_BOUNCE = LOONGSON_INT_ROUTER_BOUNCE | (1<<(i+24));
			LOONGSON_INT_ROUTER_INTENSET = LOONGSON_INT_ROUTER_INTEN | (1<<(i+24));
		}
		LOONGSON_INT_ROUTER_HT1(2) = LOONGSON_INT_COREx_INTy(loongson_boot_cpu_id, 1);
		LOONGSON_HT1_INTN_EN(2) = 0xffffffff;
		LOONGSON_INT_ROUTER_INTENSET = LOONGSON_INT_ROUTER_INTEN | (1<<(2+24));

		LOONGSON_INT_ROUTER_HT1(3) = LOONGSON_INT_COREx_INTy(loongson_boot_cpu_id, 1);
		LOONGSON_HT1_INTN_EN(3) = 0xffffffff;
		LOONGSON_INT_ROUTER_INTENSET = LOONGSON_INT_ROUTER_INTEN | (1<<(3+24));
	} else {
		/* route 3A CPU0 INT0 to node0 core0 INT1(IP3) */
		LOONGSON_INT_ROUTER_ENTRY(0) = LOONGSON_INT_COREx_INTy(loongson_boot_cpu_id, 1);
		LOONGSON_INT_ROUTER_INTENSET = LOONGSON_INT_ROUTER_INTEN | 0x1 << 0;
	}
}

void __init ls7a_init_irq(void)
{
#ifdef CONFIG_LS7A_MSI_SUPPORT
	if((read_c0_prid() & 0xf) == PRID_REV_LOONGSON3A_R3_1) {
		pr_info("Loongson 3A3000F supports HT MSI interrupt, enabling LS7A MSI Interrupt.\n");
		ls3a_msi_enabled = 1;
		pch_irq_chip.name = "LS7A-IOAPIC-MSI";
		loongson_pch->irq_dispatch = ls7a_msi_irq_dispatch;
	}
#endif
	init_7a_irqs();
	ls7a_irq_router_init();
}
