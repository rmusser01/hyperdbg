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

#ifndef _HYPERDBG_COMMON_H
#define _HYPERDBG_COMMON_H

#include "hyperdbg.h"

#define HYPERDBG_MAGIC_SCANCODE     88	/* F12 */

#define MAXSWBPS 100

/* Information related to the state of the internal Linux guest */
typedef struct {
  /* TODO! */
  hvm_address dummy;
} LINUX_STATE;

/* Information related to the state of the internal Windows guest */
typedef struct {
  hvm_address kernel_base;
} WIN_STATE;

typedef struct {
  /* This variable is set to 1 when HyperDbg has been initialized */
  hvm_bool initialized;

  /* This variable is set to TRUE when user has requested to enter
     HyperDbg-mode, otherwise, it is set to FALSE */
  hvm_bool enabled;

  /* This variable is TRUE when guest single-stepping is enabled */
  hvm_bool singlestepping;

  /* These variable is TRUE(the first) and used when we have set a permanent breakpoint */
  hvm_bool hasPermBP;
  hvm_address previous_codeaddr;
  hvm_bool isPermBPCr3Dipendent;
  hvm_address bp_cr3;
  
  /* These variables are needed for right alteration of EFLAGS in singlestepping */
  hvm_bool TF_on;
  hvm_bool IF_on;

  /* This variable is TRUE when HyperDbg running in console mode */
  hvm_bool console_mode;

  /* The interrupt vector that the PIC maps to the keyboard IRQ */
  Bit8u keyb_vector;

  /* Accounting variables */
  Bit32u ntraps;

  union {
    WIN_STATE   win_state;
    LINUX_STATE linux_state;
  };

  /* Variables for process freeze */
  hvm_address target_cr3;
  hvm_address target_kthread;
  hvm_address target_pep;
  hvm_address unlink_bp_addr;
  hvm_address relink_bp_addr;
  unsigned int dispatcher_ready_index;
  Bit8u opcode_backup_unlink;
  Bit8u opcode_backup_relink;

  /* Variables for process BPs and singlestepping */
  hvm_address bp_addr_target;

} HYPERDBG_STATE;

extern HYPERDBG_STATE hyperdbg_state;

#endif	/* _HYPERDBG_COMMON_H */
