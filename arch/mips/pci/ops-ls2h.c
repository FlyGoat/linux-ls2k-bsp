/*
 * ops-ls2hsoc.c
 *
 * Copyright (C) 2004 ICT CAS
 * Author: Li xiaoyu, ICT CAS
 *   lixy@ict.ac.cn
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>

#include <ls2h/ls2h.h>
#include <ls2h/ls2h_int.h>

#define PCI_ACCESS_READ  0
#define PCI_ACCESS_WRITE 1

static u32 ls2h_pcie_bar_translate(unsigned char access_type, u32 bar_in,
				   unsigned char portnum)
{
	int tmp;
	/* This register is zero in 2h3, !zero in 2h2 */
        tmp = ls2h_readl(LS2H_CHIP_SAMP3_REG);
	/*DN`t use this code in the 2H3, because it does not need to address transformation*/
	if (tmp){
		static unsigned char tag_mem = 0, tag_io = 0;

		if (portnum > LS2H_PCIE_MAX_PORTNUM)
			return bar_in;

		if ((access_type == PCI_ACCESS_WRITE) && LIE_IN_WINDOW(bar_in,
					LS2H_PCIE_MEM0_DOWN_BASE,
					LS2H_PCIE_MEM0_DOWN_MASK))
			return MAP_2_WINDOW(bar_in, LS2H_PCIE_MEM0_UP_BASE,
					    LS2H_PCIE_MEM0_UP_MASK);

		if ((access_type == PCI_ACCESS_READ) && LIE_IN_WINDOW(bar_in,
					LS2H_PCIE_MEM0_UP_BASE,
					LS2H_PCIE_MEM0_UP_MASK)) {
			if (tag_mem)
				return MAP_2_WINDOW(bar_in,
						    LS2H_PCIE_MEM0_BASE_PORT(portnum),
						    LS2H_PCIE_MEM0_UP_MASK);
			else {
				tag_mem = 1;
				return bar_in;
			}
		}

		if ((access_type == PCI_ACCESS_WRITE) && LIE_IN_WINDOW(bar_in,
					LS2H_PCIE_IO_DOWN_BASE,
					LS2H_PCIE_IO_DOWN_MASK))
			return MAP_2_WINDOW(bar_in, LS2H_PCIE_IO_UP_BASE,
					    LS2H_PCIE_IO_UP_MASK);

		if ((access_type == PCI_ACCESS_READ)
			&& LIE_IN_WINDOW(bar_in, LS2H_PCIE_IO_UP_BASE,
				LS2H_PCIE_IO_UP_MASK)
			&& (bar_in & PCI_BASE_ADDRESS_SPACE_IO)) {
			if (tag_io)
				return MAP_2_WINDOW(bar_in,
						    LS2H_PCIE_IO_BASE_PORT(portnum),
						    LS2H_PCIE_IO_UP_MASK);
			else {
				tag_io = 1;
				return bar_in;
			}
		}
	}
	return bar_in;
}

static int is_link_up(u8 port)
{
	u32 reg, data;

	reg = LS2H_PCIE_REG_BASE_PORT(port) | LS2H_PCIE_PORT_REG_STAT1;
	data = ls2h_readl(reg);

	return data & LS2H_PCIE_REG_STAT1_BIT_LINKUP;
}

static int ls2h_pci_config_access(unsigned char access_type,
				  struct pci_bus *bus, unsigned int devfn,
				  int where, u32 * data, unsigned char portnum)
{
	unsigned char busnum = bus->number;
	u_int32_t addr, type;
	u_int32_t addr_i, cfg_addr;
	u32 datarp;
	int device = devfn >> 3;
	int function = devfn & 0x7;
	int reg = where & ~3;
	unsigned char need_bar_translate = 0;

	if (portnum > LS2H_PCIE_MAX_PORTNUM)
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (!bus->parent) {
		/* in-chip virtual-bus has no parent,
		 * so access is routed to PORT_HEAD
		 */
		if (device > 0 || function > 0) {
			*data = -1;
			return PCIBIOS_DEVICE_NOT_FOUND;
		} else {
			addr = LS2H_PCIE_PORT_HEAD_BASE_PORT(portnum) | reg;
			if (reg == PCI_BASE_ADDRESS_0)
				/* the default value of PCI_BASE_ADDRESS_0 of
				 * PORT_HEAD is wrong, use PCI_BASE_ADDESS_1 instead
				 */
				addr += 4;
		}
	} else {
		if (busnum > 255 || device > 31 || function > 1
				|| !is_link_up(portnum)) {
			*data = -1;
			return PCIBIOS_DEVICE_NOT_FOUND;
		}

		if (!bus->parent->parent) {
			/* the bus is child of virtual-bus(pcie slot),
			 * so use Type 0 access for device on it
			 */
			if (device > 0) {
				*data = -1;
				return PCIBIOS_DEVICE_NOT_FOUND;
			}
			type = 0;
		} else {
			/* the bus is emitted from offboard-bridge,
			 * so use Type 1 access for device on it
			 */
			type = 1;
		}

		/* write busnum/devnum/funcnum/type into PCIE_REG_BASE +0x24 */
		addr_i = LS2H_PCIE_REG_BASE_PORT(portnum)
				| LS2H_PCIE_PORT_REG_CFGADDR;
		cfg_addr = (busnum << 16) | (device << 11)
				| (function << 8) | type;
		ls2h_writel(cfg_addr, addr_i);

		/* access mapping memory instead of direct configuration access */
		addr = LS2H_PCIE_DEV_HEAD_BASE_PORT(portnum) | reg;

		if (reg >= PCI_BASE_ADDRESS_0 && reg <= PCI_BASE_ADDRESS_5)
			/* Base Address Register need to be translated */
			need_bar_translate = 1;
	}

	if (access_type == PCI_ACCESS_WRITE) {
		if (need_bar_translate) {
			datarp = ls2h_pcie_bar_translate(PCI_ACCESS_WRITE,
					*data, portnum);
		} else
			datarp = *data;

		ls2h_writel(datarp, addr);
	} else {
		datarp = ls2h_readl(addr);
		if (need_bar_translate) {
			*data = ls2h_pcie_bar_translate(PCI_ACCESS_READ,
					datarp, portnum);
		} else
			*data = datarp;
	}

	return PCIBIOS_SUCCESSFUL;
}

static int ls2h_pcibios_read_port(struct pci_bus *bus, unsigned int devfn,
				       int where, int size, u32 * val, u8 port)
{
	u32 data = 0;

	if (ls2h_pci_config_access(PCI_ACCESS_READ, bus, devfn, where,
				&data, port))
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (size == 1)
		*val = (data >> ((where & 3) << 3)) & 0xff;
	else if (size == 2)
		*val = (data >> ((where & 3) << 3)) & 0xffff;
	else
		*val = data;

	return PCIBIOS_SUCCESSFUL;
}

static int ls2h_pcibios_write_port(struct pci_bus *bus, unsigned int devfn,
					int where, int size, u32 val, u8 port)
{
	u32 data = 0;

	if (size == 4)
		data = val;
	else {
		if (ls2h_pci_config_access(PCI_ACCESS_READ, bus, devfn, where,
					&data, port))
			return PCIBIOS_DEVICE_NOT_FOUND;

		if (size == 1)
			data = (data & ~(0xff << ((where & 3) << 3))) |
			    (val << ((where & 3) << 3));
		else if (size == 2)
			data = (data & ~(0xffff << ((where & 3) << 3))) |
			    (val << ((where & 3) << 3));
	}

	if (ls2h_pci_config_access(PCI_ACCESS_WRITE, bus, devfn, where,
				&data, port))
		return PCIBIOS_DEVICE_NOT_FOUND;

	return PCIBIOS_SUCCESSFUL;
}

static int ls2h_pci_pcibios_read_port0(struct pci_bus *bus, unsigned int devfn,
				       int where, int size, u32 *val)
{
	return ls2h_pcibios_read_port(bus, devfn, where, size,
			val, LS2H_PCIE_PORT0);
}

static int ls2h_pci_pcibios_write_port0(struct pci_bus *bus, unsigned int devfn,
					int where, int size, u32 val)
{
	return ls2h_pcibios_write_port(bus, devfn, where, size,
			val, LS2H_PCIE_PORT0);
}

static int ls2h_pci_pcibios_read_port1(struct pci_bus *bus, unsigned int devfn,
				       int where, int size, u32 *val)
{
	return ls2h_pcibios_read_port(bus, devfn, where, size,
			val, LS2H_PCIE_PORT1);
}

static int ls2h_pci_pcibios_write_port1(struct pci_bus *bus, unsigned int devfn,
					int where, int size, u32 val)
{
	return ls2h_pcibios_write_port(bus, devfn, where, size,
			val, LS2H_PCIE_PORT1);
}
static int ls2h_pci_pcibios_read_port2(struct pci_bus *bus, unsigned int devfn,
				       int where, int size, u32 *val)
{
	return ls2h_pcibios_read_port(bus, devfn, where, size,
			val, LS2H_PCIE_PORT2);
}

static int ls2h_pci_pcibios_write_port2(struct pci_bus *bus, unsigned int devfn,
					int where, int size, u32 val)
{
	return ls2h_pcibios_write_port(bus, devfn, where, size,
			val, LS2H_PCIE_PORT2);
}
static int ls2h_pci_pcibios_read_port3(struct pci_bus *bus, unsigned int devfn,
				       int where, int size, u32 *val)
{
	return ls2h_pcibios_read_port(bus, devfn, where, size,
			val, LS2H_PCIE_PORT3);
}

static int ls2h_pci_pcibios_write_port3(struct pci_bus *bus, unsigned int devfn,
					int where, int size, u32 val)
{
	return ls2h_pcibios_write_port(bus, devfn, where, size,
			val, LS2H_PCIE_PORT3);
}

struct pci_ops ls2h_pcie_ops_port0 = {
	.read	= ls2h_pci_pcibios_read_port0,
	.write	= ls2h_pci_pcibios_write_port0
};

struct pci_ops ls2h_pcie_ops_port1 = {
	.read	= ls2h_pci_pcibios_read_port1,
	.write	= ls2h_pci_pcibios_write_port1
};

struct pci_ops ls2h_pcie_ops_port2 = {
	.read	= ls2h_pci_pcibios_read_port2,
	.write	= ls2h_pci_pcibios_write_port2
};

struct pci_ops ls2h_pcie_ops_port3 = {
	.read	= ls2h_pci_pcibios_read_port3,
	.write	= ls2h_pci_pcibios_write_port3
};
