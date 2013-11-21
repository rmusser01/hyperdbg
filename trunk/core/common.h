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

#ifndef _PILL_COMMON_H
#define _PILL_COMMON_H

#define MIN(a,b) (((a)<(b))?(a):(b))

#ifdef GUEST_LINUX

#define STATUS_SUCCESS 0
#define STATUS_UNSUCCESSFUL -1;

#define USESTACK __attribute__((regparm(0)))
#define GUEST_MALLOC(size) kmalloc((size), GFP_KERNEL)
#define GUEST_FREE(p,size) kfree(p)

#elif defined GUEST_WINDOWS

#define USESTACK
#define GUEST_MALLOC(size) MmAllocateNonCachedMemory(size)
#define GUEST_FREE(p,size) MmFreeNonCachedMemory((p), (size))

#else
#error Invalid guest
#endif

#include "types.h"
#include "vt.h"

/* Assembly functions (defined in i386/common.asm) */
void USESTACK CmInitSpinLock(Bit32u *plock);
void USESTACK CmAcquireSpinLock(Bit32u *plock);
void USESTACK CmReleaseSpinLock(Bit32u *plock);

void CmSetBit32(Bit32u* dword, Bit32u bit);
void CmClearBit32(Bit32u* dword, Bit32u bit);
void CmClearBit16(Bit16u* word, Bit32u bit); 
void CmSleep(Bit32u microseconds);

int wide2ansi(Bit8u* dst, Bit8u* src, Bit32u n);

#endif	/* _PILL_COMMON_H */
