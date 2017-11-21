#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/msi.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/pci.h>

#define IRQ_LS7A_MSI_0 0
#define LS7A_NUM_MSI_IRQS 64

/*msi use irq, other device not used*/
static DECLARE_BITMAP(msi_irq_in_use, LS7A_NUM_MSI_IRQS)={0xff00000000000000ULL};

static DEFINE_SPINLOCK(lock);
extern int ls3a_msi_enabled;

#define irq2bit(irq) (irq - IRQ_LS7A_MSI_0)
#define bit2irq(bit) (IRQ_LS7A_MSI_0 + bit)

/*
 * Dynamic irq allocate and deallocation
 */
static int ls7a_create_msi_irq(void)
{
	int irq, pos;
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);
again:
	pos = find_first_zero_bit(msi_irq_in_use, LS7A_NUM_MSI_IRQS);
	if(pos==LS7A_NUM_MSI_IRQS) {
		spin_unlock_irqrestore(&lock, flags);
		return -ENOSPC;
	}

	irq = pos + IRQ_LS7A_MSI_0;
	/* test_and_set_bit operates on 32-bits at a time */
	if (test_and_set_bit(pos, msi_irq_in_use))
		goto again;
	spin_unlock_irqrestore(&lock, flags);

	dynamic_irq_init(irq);

	return irq;
}

static void ls7a_destroy_irq(unsigned int irq)
{
	int pos = irq2bit(irq);

	dynamic_irq_cleanup(irq);

	clear_bit(pos, msi_irq_in_use);
}

void ls7a_teardown_msi_irq(unsigned int irq)
{
	ls7a_destroy_irq(irq);
}


static void ls3a_msi_nop(struct irq_data *data)
{
	return;
}

static struct irq_chip ls7a_msi_chip = {
	.name = "PCI-MSI",
	.irq_ack = ls3a_msi_nop,
	.irq_enable = unmask_msi_irq,
	.irq_disable = mask_msi_irq,
	.irq_mask = mask_msi_irq,
	.irq_unmask = unmask_msi_irq,
};

int ls7a_setup_msi_irq(struct pci_dev *pdev, struct msi_desc *desc)
{
	int irq = ls7a_create_msi_irq();
	struct msi_msg msg;

	if (irq < 0)
		return irq;

	irq_set_msi_desc(irq, desc);

	msg.address_hi = 0xfd;
	msg.address_lo = 0xf8000000;

	msg.data = irq;

	write_msi_msg(irq, &msg);
	irq_set_chip_and_handler(irq, &ls7a_msi_chip, handle_level_irq);

	return 0;
}

static bool nomsix=1;
core_param(nomsix, nomsix, bool, 0664);
static bool nomsi=0;
core_param(nomsi, nomsi, bool, 0664);

int arch_setup_msi_irqs(struct pci_dev *dev, int nvec, int type)
{
	struct msi_desc *entry;
	int ret;

	if (!ls3a_msi_enabled || ((type == PCI_CAP_ID_MSIX && nomsix) || (type == PCI_CAP_ID_MSI && nomsi)))
		return -ENOSPC;

	/*
	 * If an architecture wants to support multiple MSI, it needs to
	 * override arch_setup_msi_irqs()
	 */
	if (type == PCI_CAP_ID_MSI && nvec > 1)
		return 1;

	list_for_each_entry(entry, &dev->msi_list, list) {
		ret = ls7a_setup_msi_irq(dev, entry);
		if (ret < 0)
			return ret;
		if (ret > 0)
			return -ENOSPC;
	}

	return 0;
}

int arch_setup_msi_irq(struct pci_dev *pdev, struct msi_desc *desc)
{
	if(!ls3a_msi_enabled || nomsi)
		return -ENOSPC;
	return ls7a_setup_msi_irq(pdev, desc);
}

void arch_teardown_msi_irq(unsigned int irq)
{
	ls7a_teardown_msi_irq(irq);
}
