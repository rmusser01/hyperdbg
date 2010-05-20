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

#ifndef _PILL_H
#define _PILL_H

#include <ntddk.h>
#include "types.h"
#include "idt.h"

/* This global structure represents the initial state of the VMM. All these
   variables are wrapped together in order to be exposed to the plugins. */
typedef struct {
  Bit32u*           pVMXONRegion;	    /* VMA of VMXON region */
  PHYSICAL_ADDRESS  PhysicalVMXONRegionPtr; /* PMA of VMXON region */

  Bit32u*           pVMCSRegion;	    /* VMA of VMCS region */
  PHYSICAL_ADDRESS  PhysicalVMCSRegionPtr;  /* PMA of VMCS region */

  void*             VMMStack;               /* VMM stack area */

  Bit32u*           pIOBitmapA;	            /* VMA of I/O bitmap A */
  PHYSICAL_ADDRESS  PhysicalIOBitmapA;      /* PMA of I/O bitmap A */

  Bit32u*           pIOBitmapB;	            /* VMA of I/O bitmap B */
  PHYSICAL_ADDRESS  PhysicalIOBitmapB;      /* PMA of I/O bitmap B */

  PIDT_ENTRY        VMMIDT;                 /* VMM interrupt descriptor table */
} VMM_INIT_STATE, *PVMM_INIT_STATE;

extern hvm_bool HandlerLogging;
extern hvm_bool VMXIsActive;

#endif	/* _PILL_H */
