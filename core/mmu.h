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

#ifndef _MMU_H
#define _MMU_H

#include "types.h"
#include "x86.h"

/* Memory-management defines */
#define MMU_PAGE_SIZE        0x1000
#define MMU_PAGE_ALIGN(addr) (((Bit32u) (addr)) & ~(MMU_PAGE_SIZE - 1))

#define MMU_PAGE_OFFSET(a) ((hvm_address) (a) - (hvm_address) MMU_PAGE_ALIGN(a))

/* **** PAE **** */
#ifdef ENABLE_PAE
#define LARGEPAGE_SIZE 0x200000
#define CR3_ALIGN(v) ((v) & 0xffffffe0)

#define VA_TO_PDPTE(a) (((a) & 0xc0000000) >> 30)
#define VA_TO_PDE(a)   (((a) & 0x3fe00000) >> 21)
#define VA_TO_PTE(a)   (((a) & 0x001ff000) >> 12)

#define PDE_TO_VA(a)   (((a) << 21) & 0x3fe00000)
#define PTE_TO_VA(a)   (((a) << 12) & 0x001ff000)

#define PHY_TO_FRAME(a)      ((Bit64u) (((a) >> 12) & 0xfffffffffffff))
#define PHY_TO_LARGEFRAME(a) ((a) >> 21)

#define FRAME_TO_PHY(a)      ((a) << 12)
#define LARGEFRAME_TO_PHY(a) (((a)>>9) << 21)

#define PDE_TO_VALID(a)  ((hvm_address) (a) & 0x1)

#define LARGEPAGE_ALIGN(a) ((a) & ~((LARGEPAGE_SIZE)-1))
#define LARGEPAGE_OFFSET(a) ((hvm_address) (a) - (hvm_address) LARGEPAGE_ALIGN(a))

#define READ_PTE(a) *(Bit64u*) (a)

#else
/* **** Non-PAE **** */
#define LARGEPAGE_SIZE     0x400000
#define CR3_ALIGN(v) ((v) & 0xfffff000)

#define VA_TO_PDE(a)   (((a) & 0xffc00000) >> 22)
#define VA_TO_PTE(a)   (((a) & 0x003ff000) >> 12)

#define PDE_TO_VA(a)   (((a) << 22) & 0xffc00000)
#define PTE_TO_VA(a)   (((a) << 12) & 0x003ff000)

#define PHY_TO_FRAME(a)      ((Bit32u) (((a) >> 12) & 0xfffff))
#define PHY_TO_LARGEFRAME(a) ((a) >> 22)

#define FRAME_TO_PHY(a) ((a) << 12)
#define LARGEFRAME_TO_PHY(a) (((a)>>10) << 22)

#define PDE_TO_VALID(a)  ((hvm_address) (a) & 0x1)

#define LARGEPAGE_ALIGN(a) ((a) & ~((LARGEPAGE_SIZE)-1))
#define LARGEPAGE_OFFSET(a) ((hvm_address) (a) - (hvm_address) LARGEPAGE_ALIGN(a))

#define READ_PTE(a) *(Bit32u*) (a)

#endif	/* ENABLE_PAE */

hvm_status MmuInit(hvm_address *pcr3);
hvm_status MmuFini(void);

hvm_status MmuMapPhysicalPage(hvm_phy_address phy, hvm_address* pva, PPTE poriginal);
hvm_status MmuUnmapPhysicalPage(hvm_address va, PTE original);

hvm_status MmuReadWriteVirtualRegion(hvm_address cr3, hvm_address va, void* buffer, Bit32u size, hvm_bool isWrite);
hvm_status MmuReadWritePhysicalRegion(hvm_phy_address phy, void* buffer, Bit32u size, hvm_bool isWrite);

hvm_status MmuGetPhysicalAddress(hvm_address cr3, hvm_address va, hvm_phy_address* pphy);
hvm_bool   MmuIsAddressValid(hvm_address cr3, hvm_address va);
hvm_bool   MmuIsAddressWritable(hvm_address cr3, hvm_address va);

void       MmuVirtToPTE( hvm_address, hvm_address *);

hvm_address MmuGetHostPT(void);

#define    MmuWriteVirtualRegion(cr3, va, buffer, size) MmuReadWriteVirtualRegion(cr3, va, buffer, size, TRUE)
#define    MmuReadVirtualRegion(cr3, va, buffer, size)  MmuReadWriteVirtualRegion(cr3, va, buffer, size, FALSE)

#define    MmuWritePhysicalRegion(phy, buffer, size) MmuReadWritePhysicalRegion(phy, buffer, size, TRUE)
#define    MmuReadPhysicalRegion(phy, buffer, size)  MmuReadWritePhysicalRegion(phy, buffer, size, FALSE)
 
#endif	/* _MMU_H */
