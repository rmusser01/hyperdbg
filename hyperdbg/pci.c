/*
  Copyright notice
  ================
  
  Copyright (C) 2010
      Lorenzo  Martignoni <martignlo@gmail.com>
      Roberto  Paleari    <roberto.paleari@gmail.com>
      Aristide Fattori    <joystick@security.dico.unimi.it>
  
  This program is free software: you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.
  
  HyperDbg is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License along with
  this program. If not, see <http://www.gnu.org/licenses/>.
  
*/

#include <ntddk.h>
#include <ntstrsafe.h>

/* ################ */
/* #### MACROS #### */
/* ################ */

#define PCI_CONF_TYPE_NONE 0
#define PCI_CONF_TYPE_1    1
#define PCI_CONF_TYPE_2    2

/* Offsets in PCI configuration space to the elements of the predefined header
   common to all header types */
#define PCI_VENDOR_ID           0x00        /* (2 byte) vendor id */
#define PCI_DEVICE_ID           0x02        /* (2 byte) device id */
#define PCI_COMMAND             0x04        /* (2 byte) command */
#define PCI_STATUS              0x06        /* (2 byte) status */
#define PCI_REVISION            0x08        /* (1 byte) revision id */
#define PCI_CLASS_API           0x09        /* (1 byte) specific register interface type */
#define PCI_CLASS_SUB           0x0a        /* (1 byte) specific device function */
#define PCI_CLASS_BASE          0x0b        /* (1 byte) device type (display vs network, etc) */
#define PCI_LINE_SIZE           0x0c        /* (1 byte) cache line size in 32 bit words */
#define PCI_LATENCY             0x0d        /* (1 byte) latency timer */
#define PCI_HEADER_TYPE         0x0e        /* (1 byte) header type */
#define PCI_BIST                0x0f        /* (1 byte) built-in self-test */

/* PCI header types */
#define PCI_HEADER_TYPE_NORMAL   0x00
#define PCI_HEADER_TYPE_BRIDGE   0x01
#define PCI_HEADER_TYPE_CARDBUS  0x02

/* Base addresses specify locations in memory or I/O space. Decoded size can be
   determined by writing a value of 0xffffffff to the register, and reading it
   back. Only 1 bits are decoded. */
#define PCI_BASE_ADDRESS_SPACE_MEMORY   0x00
#define PCI_BASE_ADDRESS_MEM_TYPE_32	0x00	/* 32 bit address */
#define PCI_BASE_ADDRESS_SPACE	        0x01	/* 0 = memory, 1 = I/O */
#define PCI_BASE_ADDRESS_SPACE_IO       0x01
#define PCI_BASE_ADDRESS_MEM_TYPE_1M	0x02	/* Below 1M [obsolete] */
#define PCI_BASE_ADDRESS_MEM_TYPE_64	0x04	/* 64 bit address */
#define PCI_BASE_ADDRESS_MEM_TYPE_MASK  0x06
#define PCI_BASE_ADDRESS_MEM_PREFETCH	0x08	/* prefetchable? */
#define PCI_BASE_ADDRESS_0	        0x10	/* 32 bits */
#define PCI_BASE_ADDRESS_1	        0x14	/* 32 bits [htype 0,1 only] */
#define PCI_BASE_ADDRESS_2	        0x18	/* 32 bits [htype 0 only] */
#define PCI_BASE_ADDRESS_3	        0x1c	/* 32 bits */
#define PCI_BASE_ADDRESS_4	        0x20	/* 32 bits */
#define PCI_BASE_ADDRESS_5	        0x24	/* 32 bits */
#define PCI_BASE_ADDRESS_MEM_MASK	(~0x0fUL)
#define PCI_BASE_ADDRESS_IO_MASK	(~0x03UL)

#define PCI_CONF1_ADDRESS(bus, dev, fn, reg)				\
  (0x80000000 | (bus << 16) | (dev << 11) | (fn << 8) | (reg & ~3))

#define PCI_CONF2_ADDRESS(dev, reg)	(unsigned short)(0xC000 | (dev << 8) | reg)

/* ############### */
/* #### TYPES #### */
/* ############### */

struct pci_desc {
  unsigned int id;
  unsigned char name[130];
  struct pci_desc* list;
};
typedef struct pci_desc pci_desc;

typedef struct pci_info {
    USHORT  vendor_id;              /* vendor id */
    USHORT  device_id;              /* device id */
    USHORT  command;                /* bus number */
    USHORT  status;                 /* device number on bus */
    UCHAR   revision;               /* revision id */
    UCHAR   class_api;              /* specific register interface type */
    UCHAR   class_sub;              /* specific device function */
    UCHAR   class_base;             /* device type (display vs network, etc) */
    UCHAR   line_size;              /* cache line size in 32 bit words */
    UCHAR   latency;                /* latency timer */
    UCHAR   header_type;            /* header type */
    UCHAR   bist;                   /* built-in self-test */

    union {
        struct {
	  /* 0x10 */
            ULONG   base_registers[6];      /* base registers, viewed from host */
	    ULONG   cardbus_cis;            /* CardBus CIS pointer */

            USHORT  subsystem_vendor_id;    /* subsystem (add-in card) vendor id */
            USHORT  subsystem_id;           /* subsystem (add-in card) id */

	  /* 0x30 */
	    ULONG   rom_base;               /* rom base address, viewed from host */
            ULONG   rom_base_pci;           /* rom base addr, viewed from pci */
            ULONG   rom_size;               /* rom size */

	  /* 0x3c */
            UCHAR   interrupt_line;         /* interrupt line */
            UCHAR   interrupt_pin;          /* interrupt pin */

	  /* 0x3e */
	    UCHAR   min_grant;              /* burst period @ 33 Mhz */
            UCHAR   max_latency;            /* how often PCI access needed */
        } h0;

        struct {
	  /* 0x10 */
            ULONG   base_registers[2];      /* base registers, viewed from host */

            ULONG   base_registers_pci[2];  /* base registers, viewed from pci */
            ULONG   base_register_sizes[2]; /* size of what base regs point to */
            UCHAR   base_register_flags[2]; /* flags from base address fields */

            UCHAR   primary_bus;
            UCHAR   secondary_bus;
            UCHAR   subordinate_bus;
            UCHAR   secondary_latency;
            UCHAR   io_base;
            UCHAR   io_limit;
            USHORT  secondary_status;
            USHORT  memory_base;
            USHORT  memory_limit;
            USHORT  prefetchable_memory_base;
            USHORT  prefetchable_memory_limit;
            ULONG   prefetchable_memory_base_upper32;
            ULONG   prefetchable_memory_limit_upper32;

	  /* 0x30 */
            USHORT  io_base_upper16;
            USHORT  io_limit_upper16;
            ULONG   rom_base;               /* rom base address, viewed from host */
	    ULONG   rom_base_pci;           /* rom base addr, viewed from pci */
	    
	  /* 0x3c */
	    UCHAR   interrupt_line;         /* interrupt line */
            UCHAR   interrupt_pin;          /* interrupt pin */
            USHORT  bridge_control;     
        } h1; 
    } u;
} pci_info;

/* ################# */
/* #### GLOBALS #### */
/* ################# */

static UCHAR pci_conf_type = PCI_CONF_TYPE_NONE;

/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

static VOID  PCIListController(VOID);
static int   PCIConfRead(unsigned bus, unsigned dev, unsigned fn, unsigned reg, unsigned len, unsigned int *value);

#if 0
static ULONG PCIGetName(unsigned int v, unsigned int d, unsigned char *out_char, int out_size);
#endif

/* See http://www.cs.ucla.edu/~kohler/class/aos-f04/ref/hardware/vgadoc/PCI.TXT */
VOID PCIInit(VOID)
{
  UCHAR tmp1, tmp2;

  WRITE_PORT_UCHAR((PUCHAR) 0xCF8, 0);
  WRITE_PORT_UCHAR((PUCHAR) 0xCFA, 0);

  tmp1 = READ_PORT_UCHAR((PUCHAR) 0xCF8);
  tmp2 = READ_PORT_UCHAR((PUCHAR) 0xCFA);

  if (tmp1 == 0 && tmp2 == 0) {
    pci_conf_type = PCI_CONF_TYPE_2;
  } else {
    ULONG tmplong1, tmplong2;

    tmplong1 = READ_PORT_ULONG((PULONG) 0xCF8);
    WRITE_PORT_ULONG((PULONG) 0xCF8, 0x80000000);

    tmplong2 = READ_PORT_ULONG((PULONG) 0xCF8);
    WRITE_PORT_ULONG((PULONG) 0xCF8, tmplong1);

    if (tmplong2 == 0x80000000) {
      pci_conf_type = PCI_CONF_TYPE_1;
    } else {
      pci_conf_type = PCI_CONF_TYPE_NONE;
    }
  }
}

static VOID PCIListController(VOID)
{
  int i, result;
  unsigned int ctrl_bus, ctrl_dev, ctrl_fn, tmp, vendor, device;
  unsigned char pci_data[0x40];
  unsigned char debug[256];
 
  for (ctrl_bus=0; ctrl_bus<255; ctrl_bus++)
    for (ctrl_dev=0; ctrl_dev<31; ctrl_dev++)
      for (ctrl_fn=0; ctrl_fn<7; ctrl_fn++) {
	result = PCIConfRead(ctrl_bus, ctrl_dev, ctrl_fn, PCI_VENDOR_ID, 2, &vendor);
	result = PCIConfRead(ctrl_bus, ctrl_dev, ctrl_fn, PCI_DEVICE_ID, 2, &device);
	  
	if ((vendor == 0xffff || device == 0xffff ) ||
	    (vendor == 0x0 && device == 0x0))
	  continue;

	for (i=0;i<0x40;i++) {
	  result = PCIConfRead(ctrl_bus, ctrl_dev, ctrl_fn, i,1, &tmp);
	  pci_data[i] = (unsigned char)(tmp & 0xff);
	}
      }
}

static int PCIConfRead(unsigned bus, unsigned dev, unsigned fn, unsigned reg, unsigned len, unsigned int *value)
{
  int result;

  if (!value || (bus > 255) || (dev > 31) || (fn > 7) || (reg > 255))
    return -1;

  result = -1;

  switch (pci_conf_type) {
  case PCI_CONF_TYPE_1:
    WRITE_PORT_ULONG((PULONG) 0xCF8, PCI_CONF1_ADDRESS(bus, dev, fn, reg));

    switch(len) {
    case 1:  *value = READ_PORT_UCHAR ((PUCHAR)  (0xCFC + (reg & 3))); result = 0; break;
    case 2:  *value = READ_PORT_USHORT((PUSHORT) (0xCFC + (reg & 2))); result = 0; break;
    case 4:  *value = READ_PORT_ULONG ((PULONG)  0xCFC); result = 0; break;
    }
    break;

  case PCI_CONF_TYPE_2:
    WRITE_PORT_UCHAR((PUCHAR) 0xCF8, 0xF0 | (fn << 1));
    WRITE_PORT_UCHAR((PUCHAR) 0xCFA, (unsigned char) bus);

    switch(len) {
    case 1:  *value = READ_PORT_UCHAR ((PUCHAR)  PCI_CONF2_ADDRESS(dev, reg)); result = 0; break;
    case 2:  *value = READ_PORT_USHORT((PUSHORT) PCI_CONF2_ADDRESS(dev, reg)); result = 0; break;
    case 4:  *value = READ_PORT_ULONG ((PULONG)  PCI_CONF2_ADDRESS(dev, reg)); result = 0; break;
    }

    WRITE_PORT_UCHAR((PUCHAR) 0xCF8, 0);
    break;
  }

  return result;
}

NTSTATUS PCIDetectDisplay(PULONG pdisplay_address)
{
  int result, i;
  unsigned int ctrl_bus, ctrl_dev, ctrl_fn;
  unsigned int header_type_tmp;

  unsigned int tmp, vendor, device;
  unsigned char pci_data[0x40];
  pci_info* p_pci_info;

  unsigned char debug[256];

  DbgPrint("[*] Starting PCI scan\n");
 
  for (ctrl_bus=0; ctrl_bus<255; ctrl_bus++)
    for (ctrl_dev=0; ctrl_dev<31; ctrl_dev++)
      for (ctrl_fn=0; ctrl_fn<7; ctrl_fn++) {
	result = PCIConfRead(ctrl_bus, ctrl_dev, ctrl_fn, PCI_VENDOR_ID, 2, &vendor);
	result = PCIConfRead(ctrl_bus, ctrl_dev, ctrl_fn, PCI_DEVICE_ID, 2, &device);
	  
	if ((vendor == 0xFFFF || device == 0xFFFF ) ||
	    (vendor == 0x0 && device == 0x0))
	  continue;

	result = PCIConfRead(ctrl_bus, ctrl_dev, ctrl_fn, PCI_HEADER_TYPE, 1, &header_type_tmp);
	header_type_tmp &= 0x7f;

	DbgPrint("[*] Found device! Vendor: %.4x device: %.4x header: %.2x\n", 
		 vendor, device, header_type_tmp);

	for (i=0; i<0x40; i++) {
	  result = PCIConfRead(ctrl_bus, ctrl_dev, ctrl_fn, i, 1, &tmp);
	  pci_data[i] = (unsigned char)(tmp & 0xff);
	}

	p_pci_info = (pci_info*) pci_data;

	/* If its not a display device */
	if (p_pci_info->class_base != 3)
	  continue;

	if (header_type_tmp == PCI_HEADER_TYPE_NORMAL) {
	  int i;
	  for (i=0; i<sizeof(p_pci_info->u.h0.base_registers)/sizeof(ULONG); i++) {
	    ULONG pos, flg, j;
	    
	    pos = p_pci_info->u.h0.base_registers[i];
	    j = PCI_BASE_ADDRESS_0 + 4*i;
	    flg = pci_data[j] | (pci_data[j+1] << 8) | (pci_data[j+2] << 16) | (pci_data[j+3] << 24);
	    if (flg == 0xffffffff)
	      flg = 0;
	    if (!pos && !flg)
	      continue;
	    if (!(flg & PCI_BASE_ADDRESS_SPACE_IO) && (flg & PCI_BASE_ADDRESS_MEM_PREFETCH)) {
	      DbgPrint("[D] Prefetchable PCI memory at %.8x\n", pos & PCI_BASE_ADDRESS_MEM_MASK);
	      *pdisplay_address = pos & PCI_BASE_ADDRESS_MEM_MASK;
	      break;
	    }
	  }
	} else if (header_type_tmp == PCI_HEADER_TYPE_BRIDGE) {
	  *pdisplay_address = p_pci_info->u.h1.base_registers[1];
	  *pdisplay_address &= ~PCI_BASE_ADDRESS_SPACE_IO;
	  *pdisplay_address &= PCI_BASE_ADDRESS_MEM_MASK;
	} else {
	  DbgPrint("[W] Unexpected PCI header\n");
	  continue;
	}

#if 0
	PCIGetName(vendor, device, debug, sizeof(debug));
	DbgPrint("[*] Good device '%s'\n", debug);
#endif

	return STATUS_SUCCESS;
      }

  DbgPrint("[E] PCI scan: failed\n");

  return STATUS_UNSUCCESSFUL;
}

#if 0
static ULONG PCIGetName(unsigned int v, unsigned int d, unsigned char *out_char, int out_size)
{
  unsigned int i, ii;
  pci_desc* vendor;
  pci_desc* device;
  ULONG precision = 0;

  for(i=0; i<sizeof(tab_vendor)/sizeof(pci_desc); i++) {
      vendor=&tab_vendor[i];
      if (vendor->id != v)
	continue;

      RtlStringCbCopyA(out_char, out_size, vendor->name);
      RtlStringCbCatA(out_char, out_size, " ");
      precision=1;
      if (vendor->list == NULL)
	continue;

      for(ii=0;;ii++) {
	device = &(vendor->list[ii]);
	if (device->id == 0)
	  break;

	if (device->id == d)
	  RtlStringCbCatA(out_char, out_size, device->name);
      }

      precision=2;
      return precision;
    }

  return precision;
}
#endif

