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

#include "common.h"
#include "x86.h"
#include "debug.h"

/* Set a single bit of a DWORD argument */
void CmSetBit32(Bit32u* dword, Bit32u bit)
{
  Bit32u mask = ( 1 << bit );
  *dword = *dword | mask;
}

/* Clear a single bit of a DWORD argument */
void CmClearBit32(Bit32u* dword, Bit32u bit)
{
  Bit32u mask = 0xFFFFFFFF;
  Bit32u sub = (1 << bit);
  mask = mask - sub;
  *dword = *dword & mask;
}

/* Clear a single bit of a WORD argument */
void CmClearBit16(Bit16u* word, Bit32u bit)
{
  Bit16u mask = 0xFFFF;
  Bit16u sub = (Bit16u) (1 << bit);
  mask = mask - sub;
  *word = *word & mask;
}

int wide2ansi(Bit8u* dst, Bit8u* src, Bit32u n)
{
  Bit32u cnt;

  if (!dst || !src)
    return -1;

  for (cnt = 0; cnt < n; ++cnt) {
    dst[cnt] = src[2*cnt];
    if (src[2*cnt + 1])
      break;
  }

  return cnt;
}

void CmSleep(Bit32u microseconds)
{
  //  Bit32u v;
  
  Bit64u t0, t1, cycles;
  Bit32u freq;
  /* FIXME: get freq dinamically */
  freq = 1000; //*1000*1000; /* 1GHz */
  cycles = microseconds * (freq);
  RegRdtsc(&t0);
  do {
    RegRdtsc(&t1);
  } while (t1 < t0 + cycles);
  
  /*
    Int 15h AH=86h
    BIOS - WAIT (AT,PS)

    AH = 86h
    CX:DX = interval in microseconds
    Return:
    CF clear if successful (wait interval elapsed)
    CF set on error or AH=83h wait already in progress
    AH = status (see #00496)

    Note: The resolution of the wait period is 977 microseconds on
    many systems because many BIOSes use the 1/1024 second fast
    interrupt from the AT real-time clock chip which is available on INT 70;
    because newer BIOSes may have much more precise timers available, it is
    not possible to use this function accurately for very short delays unless
    the precise behavior of the BIOS is known (or found through testing)
  */

/*   while (microseconds) { */
/*     v = microseconds; */

/*     if (v > 4000000) { */
/*       v = 4000000; */
/*     } */

/*     __asm { */
/*       push eax; */
/*       push ecx; */
/*       push edx; */

/*       mov  ah, 0x86; */
/*       mov  edx, microseconds; */

/*       mov  ecx, edx; */
/*       shr  ecx, 16; */

/*       int 0x15; */
	
/*       pop edx; */
/*       pop ecx; */
/*       pop eax; */
/*     }; */

/*     microseconds -= v; */
/*   } */

}
