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

#define MAXSWBPS 100

hvm_address SwBreakpointSet(hvm_address cr3, hvm_address address);
hvm_bool    SwBreakpointDelete(hvm_address cr3, hvm_address address);
hvm_bool    SwBreakpointDeleteById(hvm_address cr3, Bit32u id); 

#endif
