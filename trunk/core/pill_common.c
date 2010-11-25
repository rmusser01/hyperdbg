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

/* Inspired to Shawn Embleton's Virtual Machine Monitor */

#include "pill.h"

/* ################# */
/* #### GLOBALS #### */
/* ################# */

hvm_address EntryRFlags;
hvm_address EntryRAX;
hvm_address EntryRCX;
hvm_address EntryRDX;
hvm_address EntryRBX;
hvm_address EntryRSP;
hvm_address EntryRBP;
hvm_address EntryRSI;
hvm_address EntryRDI;   

hvm_address GuestStack;

/* ################ */
/* #### BODIES #### */
/* ################ */

hvm_status RegisterEvents(void)
{
  EVENT_CONDITION_HYPERCALL hypercall;

  /* Initialize the event handler */
  EventInit();

  /* Register a hypercall to switch off the VM */
  hypercall.hypernum = HYPERCALL_SWITCHOFF;

  if(!EventSubscribe(EventHypercall, &hypercall, sizeof(hypercall), HypercallSwitchOff)) {
    GuestLog("ERROR: Unable to register switch-off hypercall handler");
    return HVM_STATUS_UNSUCCESSFUL;
  }

  return HVM_STATUS_SUCCESS;
}

/* Initialize the IDT of the VMM */
void InitVMMIDT(PIDT_ENTRY pidt)
{
  int i;
  IDT_ENTRY idte_null;
  idte_null.Selector   = RegGetCs();

  /* Present, DPL 0, Type 0xe (INT gate) */
  idte_null.Access     = (1 << 15) | (0xe << 8);

  idte_null.LowOffset  = (Bit32u) NullIDTHandler & 0xffff;
  idte_null.HighOffset = (Bit32u) NullIDTHandler >> 16;

  for (i=0; i<256; i++) {
    pidt[i] = idte_null;
  }
}

hvm_status FiniPlugin(void)
{
#ifdef ENABLE_HYPERDBG
  if (!HVM_SUCCESS(HyperDbgHostFini()))
    return HVM_STATUS_UNSUCCESSFUL;
  
  if (!HVM_SUCCESS(HyperDbgGuestFini()))
    return HVM_STATUS_UNSUCCESSFUL;
#endif

  return HVM_STATUS_SUCCESS;
}

