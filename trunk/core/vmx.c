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

#include "x86.h"
#include "msr.h"
#include "vmx.h"

ULONG32 VmxAdjustControls(ULONG32 c, ULONG32 n)
{
  MSR msr;

  ReadMSR(n, &msr);
  c &= msr.Hi;     /* bit == 0 in high word ==> must be zero */
  c |= msr.Lo;      /* bit == 1 in low word  ==> must be one  */
  return c;
}

