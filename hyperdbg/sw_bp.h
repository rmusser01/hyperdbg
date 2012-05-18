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

#ifndef _SW_BP_H
#define _SW_BP_H

#include "hyperdbg.h"
#include "hyperdbg_print.h"
#include "hyperdbg_common.h"

hvm_address SwBreakpointSet(hvm_address cr3, hvm_address address, hvm_bool isPerm, hvm_bool isCr3Dipendent);
hvm_bool    SwBreakpointGetBPInfo(hvm_address cr3, hvm_address address, hvm_bool *isCr3Dipendent, hvm_bool *isPerm, hvm_address *ours_cr3);
hvm_bool    SwBreakpointDelete(hvm_address cr3, hvm_address address);
hvm_bool    SwBreakpointDeletePerm(hvm_address cr3, hvm_address address);
hvm_bool    SwBreakpointDeleteById(Bit32u id); 
void        SwBreakpointGetBPList(PCMD_RESULT result);
#endif
