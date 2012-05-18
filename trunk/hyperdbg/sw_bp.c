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
  hvm_address cr3;
  Bit8u OldOpcode;
  hvm_bool isPerm;
  hvm_bool isCr3Dipendent;
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
Bit32u SwBreakpointSet(hvm_address cr3, hvm_address address, hvm_bool isPerm, hvm_bool isCr3Dipendent)
{
  Bit32u index;
  PSW_BP ptr;
  Bit8u  op;
  hvm_status r;
  hvm_address found_cr3;
  hvm_bool useless;

  if(SwBreakpointGetBPInfo(cr3, address, &useless, &useless, &found_cr3) && cr3 == found_cr3)
    return -1;

  index = GetFirstFree();
  if(index == MAXSWBPS) return MAXSWBPS;

  ptr = &sw_bps[index];
  ptr->Addr = address;
  ptr->cr3 = cr3;
  ptr->isPerm = isPerm;
  ptr->isCr3Dipendent = isCr3Dipendent;

  r = MmuReadVirtualRegion(cr3, address, &(ptr->OldOpcode), sizeof(ptr->OldOpcode));
  if (r != HVM_STATUS_SUCCESS)
    return MAXSWBPS;

  op = INT3_OPCODE;
  r = MmuWriteVirtualRegion(cr3, address, &op, sizeof(op));
  if (r != HVM_STATUS_SUCCESS)
    return MAXSWBPS;

  return index;
}

hvm_bool SwBreakpointGetBPInfo(hvm_address cr3, hvm_address address, hvm_bool *isCr3Dipendent, hvm_bool *isPerm, hvm_address *ours_cr3)
{
  Bit32u i;

  i = 0;
  while(i < MAXSWBPS && sw_bps[i].Addr != address) i++;

  if(i == MAXSWBPS) {
    /* No breakpoint at this address */
    return FALSE;
  }

  *isCr3Dipendent = sw_bps[i].isCr3Dipendent;
  *isPerm = sw_bps[i].isPerm;
  *ours_cr3 = sw_bps[i].cr3;
  return TRUE;
}

hvm_bool SwBreakpointDeletePerm(hvm_address cr3, hvm_address address)
{
  Bit32u i;
  PSW_BP ptr;
  hvm_status r;

  i = 0;
  while(i < MAXSWBPS && (sw_bps[i].Addr != address || (sw_bps[i].isCr3Dipendent && sw_bps[i].cr3 != cr3))) i++;

  /* Restore the old code */
  ptr = &sw_bps[i];
  r = MmuWriteVirtualRegion(cr3, address, &(ptr->OldOpcode), sizeof(ptr->OldOpcode));
  if (r != HVM_STATUS_SUCCESS)
    return FALSE;

  return TRUE;
}

hvm_bool SwBreakpointDelete(hvm_address cr3, hvm_address address)
{
  Bit32u i;
  PSW_BP ptr;
  hvm_status r;

  Log("[HyperDbg] Delete BP (cr3 0x%08hx, addr 0x%08hx)", cr3, address);

  i = 0;
  while(i < MAXSWBPS && (sw_bps[i].Addr != address)) i++;

  if(i == MAXSWBPS) {
    return FALSE;
  }

  /* Restore the old code */
  ptr = &sw_bps[i];
  r = MmuWriteVirtualRegion(cr3, address, &(ptr->OldOpcode), sizeof(ptr->OldOpcode));
  if (r != HVM_STATUS_SUCCESS)
    return FALSE;

  ptr->Addr = 0;
  ptr->OldOpcode = 0;
  ptr->cr3 = 0;
  ptr->isCr3Dipendent = FALSE;
  ptr->isPerm = FALSE;

  return TRUE;
}

hvm_bool SwBreakpointDeleteById(hvm_address id)
{
  PSW_BP ptr;
  hvm_status r;

  if(id >= MAXSWBPS) return FALSE;
  ptr = &sw_bps[id];
  if(ptr->Addr == 0) return FALSE;

  r = MmuWriteVirtualRegion(ptr->cr3, ptr->Addr, &(ptr->OldOpcode), sizeof(ptr->OldOpcode));
  if (r != HVM_STATUS_SUCCESS)
    return FALSE;

  ptr->Addr = 0;
  ptr->OldOpcode = 0;
  ptr->cr3 = 0;
  ptr->isCr3Dipendent = FALSE;
  ptr->isPerm = FALSE;

  return TRUE;
}

Bit32u GetFirstFree(void)
{
  Bit32u i;
  i = 0;
  while(i < MAXSWBPS && sw_bps[i].Addr != 0) i++;
  return i;
}

void SwBreakpointGetBPList(PCMD_RESULT result)
{
  int i;

  for(i = 0; i < MAXSWBPS; i++) {

    (*result).bplist[i].Addr = sw_bps[i].Addr;
    (*result).bplist[i].cr3 = sw_bps[i].cr3;
    (*result).bplist[i].isPerm = sw_bps[i].isPerm;
    (*result).bplist[i].isCr3Dipendent = sw_bps[i].isCr3Dipendent;
  }
}
