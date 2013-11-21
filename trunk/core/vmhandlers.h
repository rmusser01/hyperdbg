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

#ifndef _VMHANDLERS_H
#define _VMHANDLERS_H

#include "vt.h"
#include "events.h"

void HandleVMCALL(void);
void HandleVMLAUNCH(void);
void HandleNMI(Bit32u trap, Bit32u error_code, Bit32u qualification);
void HandleIO(Bit16u port, hvm_bool isoutput, Bit8u size, hvm_bool isstring, hvm_bool isrep);
void HandleCR(Bit8u crno, VtCrAccessType accesstype, hvm_bool ismemory, VtRegister gpr);
void HandleHLT(void);

#ifdef ENABLE_EPT
void HandleEPTViolation(hvm_address guest_linear, hvm_address guest_phy, hvm_bool is_linear_valid, Bit8u attempt_type, hvm_bool in_page_walk, hvm_bool fill_an_entry);
#endif

EVENT_PUBLISH_STATUS HypercallSwitchOff(PEVENT_ARGUMENTS args);

#endif	/*  _VMHANDLERS_H */
