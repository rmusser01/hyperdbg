/*
  Copyright notice
  ================
  
  Copyright (C) 2010 - 2013 
      Lorenzo  Martignoni <martignlo@gmail.com>
      Roberto  Paleari    <roberto.paleari@gmail.com>
      Aristide Fattori    <joystick@security.di.unimi.it>
      Mattia   Pagnozzi   <pago@security.di.unimi.it>
  
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

#ifndef _EPT_H
#define _EPT_H
#include "types.h"

#define HOST_GB 4								/* Amount of GB in system (use 4 on 32 bit to get a full mapping) */

#define READ  0x1
#define WRITE 0x2
#define EXEC  0x4

#define MEM_TYPE_UNCACHEABLE  0
#define MEM_TYPE_WRITECOMBINE 1
#define MEM_TYPE_WRITETHROUGH 4
#define MEM_TYPE_WRITEPROTECT 5
#define MEM_TYPE_WRITEBACK    6

extern hvm_address VIRT_PT_BASES[HOST_GB*512];

#pragma pack (push, 1)

typedef struct
{
  hvm_phy_address Eptp;
  hvm_phy_address Rsvd;

} INVEPT_DESCRIPTOR, *PINVEPT_DESCRIPTOR;

#pragma pack (pop)

extern INVEPT_DESCRIPTOR EPTInveptDesc;

extern hvm_address     Pml4;
extern hvm_phy_address Phys_Pml4; 

void EPTInit(void);
Bit8u EPTGetMemoryType(hvm_address address);
void EPTAlterPT(hvm_address guest_phy, Bit8u perms, hvm_bool isRemove);
hvm_address EPTGetEntry(hvm_address guest_phy);
void EPTProtectPhysicalRange(hvm_address base, Bit32u size, Bit8u permsToRemove);

#define EPTRemovePTperms(guest_phy, permsToRemove) EPTAlterPT(guest_phy, permsToRemove, TRUE);
#define EPTMapPhysicalAddress(guest_phy, perms) EPTAlterPT(guest_phy, perms, FALSE);

#endif /* _EPT_H */
