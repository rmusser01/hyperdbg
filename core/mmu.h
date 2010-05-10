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

#include "x86.h"

/* Memory-management defines */
#define PAGE_SIZE 0x1000
#define LARGEPAGE_SIZE 0x400000

#define CR3_TO_PDBASE(v) ((v) & 0xfffff000)

#define VA_TO_PDE(a)   (((a) & 0xffc00000) >> 22)
#define VA_TO_PTE(a)   (((a) & 0x003ff000) >> 12)

#define PDE_TO_VA(a)   (((a) << 22) & 0xffc00000)
#define PTE_TO_VA(a)   (((a) << 12) & 0x003ff000)

#define PHY_TO_VA(a)   ((ULONG32) (a) + 0x80000000)
#define PHY_TO_FRAME(a) ((a) >> 12)
#define FRAME_TO_PHY(a) ((a) << 12)
#define PHY_TO_LARGEFRAME(a) ((a) >> 22)
#define LARGEFRAME_TO_PHY(a) ((a) << 22)

#define PDE_TO_VALID(a)  ((ULONG32) (a) & 0x1)

#define LARGEPAGE_ALIGN(a) ((a) & ~((LARGEPAGE_SIZE)-1))

#define PAGE_OFFSET(a) ((ULONG) (a) - (ULONG) PAGE_ALIGN(a))
#define LARGEPAGE_OFFSET(a) ((ULONG) (a) - (ULONG) LARGEPAGE_ALIGN(a))

NTSTATUS MmuMapPhysicalPage(ULONG phy, PULONG pva, PPTE poriginal);
NTSTATUS MmuUnmapPhysicalPage(ULONG va, PTE original);

#define MmuWriteVirtualRegion(cr3, va, buffer, size) MmuReadWriteVirtualRegion(cr3, va, buffer, size, TRUE)
#define MmuReadVirtualRegion(cr3, va, buffer, size)  MmuReadWriteVirtualRegion(cr3, va, buffer, size, FALSE)

NTSTATUS MmuReadWriteVirtualRegion(ULONG cr3, ULONG va, PVOID buffer, ULONG size, BOOLEAN isWrite);

#define MmuWritePhysicalRegion(phy, buffer, size) MmuReadWritePhysicalRegion(phy, buffer, size, TRUE)
#define MmuReadPhysicalRegion(phy, buffer, size)  MmuReadWritePhysicalRegion(phy, buffer, size, FALSE)

NTSTATUS MmuReadWritePhysicalRegion(ULONG phy, PVOID buffer, ULONG size, BOOLEAN isWrite);

NTSTATUS MmuGetPhysicalAddress(ULONG cr3, ULONG va, PULONG pphy);
BOOLEAN  MmuIsAddressValid(ULONG cr3, ULONG va);
BOOLEAN  MmuIsAddressWritable(ULONG cr3, ULONG va);

#endif	/* _MMU_H */
