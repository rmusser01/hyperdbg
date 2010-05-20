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

#ifndef _PILL_COMMON_H
#define _PILL_COMMON_H

#define CORE_VERSION  "20100325"
#define MIN(a,b) (((a)<(b))?(a):(b))

#include "types.h"
#include "vt.h"

/* Assembly functions (defined in i386/common.asm) */
void __stdcall CmInitSpinLock(Bit32u *plock);
void __stdcall CmAcquireSpinLock(Bit32u *plock);
void __stdcall CmReleaseSpinLock(Bit32u *plock);

void CmSetBit32(Bit32u* dword, Bit32u bit);
void CmClearBit32(Bit32u* dword, Bit32u bit);
void CmClearBit16(Bit16u* word, Bit32u bit);
void CmSleep(Bit32u microseconds);

int wide2ansi(Bit8u* dst, Bit8u* src, Bit32u n);

#endif	/* _PILL_COMMON_H */
