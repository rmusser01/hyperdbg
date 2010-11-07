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

#include "debug.h"
#include "mmu.h"
#include "sw_bp.h"

#define INT3_OPCODE 0xcc

typedef struct _SW_BP {
  hvm_address Addr;
  Bit8u OldOpcode;
} SW_BP, *PSW_BP;

/* ################# */
/* #### GLOBALS #### */
/* ################# */

static SW_BP sw_bps[MAXSWBPS];

/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

Bit32u GetFirstFree(void);

/* ################ */
/* #### BODIES #### */
/* ################ */

/* If it returns MAXSWBPS, we have a full-error */
Bit32u SwBreakpointSet(hvm_address cr3, hvm_address address)
{
  Bit32u index;
  PSW_BP ptr;
  Bit8u  op;
  hvm_status r;

  index = GetFirstFree();
  if(index == MAXSWBPS) return MAXSWBPS;
  ptr = &sw_bps[index];
  ptr->Addr = address;

  r = MmuReadVirtualRegion(cr3, address, &(ptr->OldOpcode), sizeof(ptr->OldOpcode));
  if (r != HVM_STATUS_SUCCESS)
    return MAXSWBPS;

  op = INT3_OPCODE;
  r = MmuWriteVirtualRegion(cr3, address, &op, sizeof(op));
  if (r != HVM_STATUS_SUCCESS)
    return MAXSWBPS;

  return index;
}

hvm_bool SwBreakpointDelete(hvm_address cr3, hvm_address address)
{
  Bit32u i;
  PSW_BP ptr;
  hvm_status r;

  i = 0;
  while(i < MAXSWBPS && sw_bps[i].Addr != address) i++;

  if(i == MAXSWBPS) {
    /* No breakpoint at this address */
    return FALSE; 
  }

  /* Restore the old code */
  ptr = &sw_bps[i];
  r = MmuWriteVirtualRegion(cr3, address, &(ptr->OldOpcode), sizeof(ptr->OldOpcode));
  if (r != HVM_STATUS_SUCCESS)
    return FALSE;

  ptr->Addr = 0;
  ptr->OldOpcode = 0;

  return TRUE;
}

hvm_bool SwBreakpointDeleteById(hvm_address cr3, hvm_address id)
{
  PSW_BP ptr;
  hvm_status r;

  if(id >= MAXSWBPS) return FALSE;
  ptr = &sw_bps[id];
  if(ptr->Addr == 0) return FALSE;

  r = MmuWriteVirtualRegion(cr3, ptr->Addr, &(ptr->OldOpcode), sizeof(ptr->OldOpcode));
  if (r != HVM_STATUS_SUCCESS)
    return FALSE;

  ptr->Addr = 0;
  ptr->OldOpcode = 0;

  return TRUE;
}

Bit32u GetFirstFree(void)
{
  Bit32u i;
  i = 0;
  while(i < MAXSWBPS && sw_bps[i].Addr != 0) i++;
  return i;
}
