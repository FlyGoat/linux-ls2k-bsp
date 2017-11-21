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

extern unsigned long long smp_group[4];
extern void loongson3_send_irq_by_ipi(int cpu, int irqs);

static DEFINE_SPINLOCK(pch_irq_lock);

static void mask_pch_irq(struct irq_data *d)
{
	unsigned long flags;

	unsigned long irq_nr = d->irq;

	spin_lock_irqsave(&pch_irq_lock, flags);

	*irq_mask |= (1ULL << (irq_nr -  LS7A_IOAPIC_IRQ_BASE));

	spin_unlock_irqrestore(&pch_irq_lock, flags);
}

static void unmask_pch_irq(struct irq_data *d)
{
	unsigned long flags;

	unsigned long irq_nr = d->irq;

	spin_lock_irqsave(&pch_irq_lock, flags);

	*irq_mask &= ~(1ULL << (irq_nr - LS7A_IOAPIC_IRQ_BASE));

	spin_unlock_irqrestore(&pch_irq_lock, flags);
}

static struct irq_chip pch_irq_chip = {
	.name        = "LS7A-IOAPIC",
	.irq_mask    = mask_pch_irq,
	.irq_unmask    = unmask_pch_irq,
	.irq_set_affinity = plat_set_irq_affinity,
};

static unsigned int irq_cpu[64] = {[0 ... 63] = -1};
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

//Maximum 26 IPI irqs
unsigned int ls7a_ipi_irq2pos[64] = { [0 ... 63] = -1,
									[LS7A_IOAPIC_GMAC0_OFFSET        ] = 0  ,
									[LS7A_IOAPIC_GMAC1_OFFSET        ] = 1  ,
									[LS7A_IOAPIC_SATA0_OFFSET        ] = 2  ,
									[LS7A_IOAPIC_SATA1_OFFSET        ] = 3  ,
									[LS7A_IOAPIC_SATA2_OFFSET        ] = 4  ,
									[LS7A_IOAPIC_EHCI0_OFFSET        ] = 5  ,
									[LS7A_IOAPIC_EHCI1_OFFSET        ] = 6  ,
									[LS7A_IOAPIC_PCIE_F0_PORT0_OFFSET] = 7  ,
									[LS7A_IOAPIC_PCIE_F0_PORT1_OFFSET] = 8  ,
									[LS7A_IOAPIC_PCIE_F0_PORT2_OFFSET] = 9  ,
									[LS7A_IOAPIC_PCIE_F0_PORT3_OFFSET] = 10 ,
									[LS7A_IOAPIC_PCIE_F1_PORT0_OFFSET] = 11 ,
									[LS7A_IOAPIC_PCIE_F1_PORT1_OFFSET] = 12 ,
									[LS7A_IOAPIC_PCIE_H_LO_OFFSET    ] = 13 ,
									[LS7A_IOAPIC_PCIE_H_HI_OFFSET    ] = 14 ,
									[LS7A_IOAPIC_PCIE_G0_LO_OFFSET   ] = 15 ,
									[LS7A_IOAPIC_PCIE_G0_HI_OFFSET   ] = 16 ,
									[LS7A_IOAPIC_PCIE_G1_LO_OFFSET   ] = 17 ,
									[LS7A_IOAPIC_PCIE_G1_HI_OFFSET   ] = 18 };
unsigned int ls7a_ipi_pos2irq[] = { LS7A_IOAPIC_GMAC0_OFFSET         ,
									LS7A_IOAPIC_GMAC1_OFFSET         ,
									LS7A_IOAPIC_SATA0_OFFSET         ,
									LS7A_IOAPIC_SATA1_OFFSET         ,
									LS7A_IOAPIC_SATA2_OFFSET         ,
									LS7A_IOAPIC_EHCI0_OFFSET         ,
									LS7A_IOAPIC_EHCI1_OFFSET         ,
									LS7A_IOAPIC_PCIE_F0_PORT0_OFFSET ,
									LS7A_IOAPIC_PCIE_F0_PORT1_OFFSET ,
									LS7A_IOAPIC_PCIE_F0_PORT2_OFFSET ,
									LS7A_IOAPIC_PCIE_F0_PORT3_OFFSET ,
									LS7A_IOAPIC_PCIE_F1_PORT0_OFFSET ,
									LS7A_IOAPIC_PCIE_F1_PORT1_OFFSET ,
									LS7A_IOAPIC_PCIE_H_LO_OFFSET     ,
									LS7A_IOAPIC_PCIE_H_HI_OFFSET     ,
									LS7A_IOAPIC_PCIE_G0_LO_OFFSET    ,
									LS7A_IOAPIC_PCIE_G0_HI_OFFSET    ,
									LS7A_IOAPIC_PCIE_G1_LO_OFFSET    ,
									LS7A_IOAPIC_PCIE_G1_HI_OFFSET    };

void handle_7a_irqs(unsigned long long irqs) {
	unsigned int  irq;
	struct irq_data *irqd;
	struct cpumask affinity;
	int cpu = smp_processor_id();

	while(irqs){
		irq = __ffs(irqs);
		irqs &= ~(1ULL<<irq);

		/* handled by local core */
		if ((local_irq & (0x1ULL << irq)) || ls7a_ipi_irq2pos[irq]==-1) {
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
		loongson3_send_irq_by_ipi(irq_cpu[irq], (0x1<<(ls7a_ipi_irq2pos[irq])));
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

static void init_7a_irq(int dev, int irq) {
	*irq_mask  &= ~(1ULL << dev);
	*(volatile unsigned char *)(LS7A_IOAPIC_ROUTE_ENTRY + dev) = USE_7A_INT0;
	irq_set_chip_and_handler(irq, &pch_irq_chip, handle_level_irq);
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
	/* route LPC int to cpu core0 int 0 */
	LOONGSON_INT_ROUTER_LPC = LOONGSON_INT_COREx_INTy(loongson_boot_cpu_id, 0);
	LOONGSON_INT_ROUTER_INTENSET = LOONGSON_INT_ROUTER_INTEN | 0x1 << 10;

	/* route 3A CPU0 INT0 to node0 core0 INT1(IP3) */
	LOONGSON_INT_ROUTER_ENTRY(0) = LOONGSON_INT_COREx_INTy(loongson_boot_cpu_id, 1);
	LOONGSON_INT_ROUTER_INTENSET = LOONGSON_INT_ROUTER_INTEN | 0x1 << 0;
}

void __init ls7a_init_irq(void)
{
	init_7a_irqs();
	ls7a_irq_router_init();
}
