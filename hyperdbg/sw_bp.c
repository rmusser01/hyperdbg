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

#include "sw_bp.h"
#include "mmu.h"
#include "debug.h"

#define INT3_OPCODE 0xcc

typedef struct _SW_BP
{
  PULONG Addr;
  UCHAR OldOpcode;
} SW_BP, *PSW_BP;

/* ################# */
/* #### GLOBALS #### */
/* ################# */

static SW_BP sw_bps[MAXSWBPS];

/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

ULONG32 GetFirstFree(VOID);

/* ################ */
/* #### BODIES #### */
/* ################ */

/* If it returns MAXSWBPS, we have a full-error */
ULONG32 SwBreakpointSet(ULONG cr3, PULONG address)
{
  ULONG32 index;
  PSW_BP ptr;
  UCHAR  op;
  NTSTATUS r;

  index = GetFirstFree();
  if(index == MAXSWBPS) return MAXSWBPS;
  ptr = &sw_bps[index];
  ptr->Addr = address;

  r = MmuReadVirtualRegion(cr3, (ULONG) address, &(ptr->OldOpcode), sizeof(ptr->OldOpcode));
  if (r != STATUS_SUCCESS)
    return MAXSWBPS;

  op = INT3_OPCODE;
  r = MmuWriteVirtualRegion(cr3, (ULONG) address, &op, sizeof(op));
  if (r != STATUS_SUCCESS)
    return MAXSWBPS;

  return index;
}

BOOLEAN SwBreakpointDelete(ULONG cr3, PULONG address)
{
  ULONG32 i;
  PSW_BP ptr;
  NTSTATUS r;

  i = 0;
  while(i < MAXSWBPS && sw_bps[i].Addr != address) i++;

  if(i == MAXSWBPS) {
    /* No breakpoint at this address */
    return FALSE; 
  }

  /* Restore the old code */
  ptr = &sw_bps[i];
  r = MmuWriteVirtualRegion(cr3, (ULONG) address, &(ptr->OldOpcode), sizeof(ptr->OldOpcode));
  if (r != STATUS_SUCCESS)
    return FALSE;

  ptr->Addr = 0;
  ptr->OldOpcode = 0;

  return TRUE;
}

BOOLEAN SwBreakpointDeleteById(ULONG cr3, ULONG32 id)
{
  PSW_BP ptr;
  NTSTATUS r;

  if(id >= MAXSWBPS) return FALSE;
  ptr = &sw_bps[id];
  if(ptr->Addr == 0) return FALSE;

  r = MmuWriteVirtualRegion(cr3, (ULONG) ptr->Addr, &(ptr->OldOpcode), sizeof(ptr->OldOpcode));
  if (r != STATUS_SUCCESS)
    return FALSE;

  ptr->Addr = 0;
  ptr->OldOpcode = 0;

  return TRUE;
}

ULONG32 GetFirstFree()
{
  ULONG32 i;
  i = 0;
  while(i < MAXSWBPS && sw_bps[i].Addr != 0) i++;
  return i;
}
