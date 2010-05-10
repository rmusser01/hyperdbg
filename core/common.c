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

#include <windows.h>

#include "common.h"

/* Set a single bit of a DWORD argument */
VOID CmSetBit(ULONG * dword, ULONG bit)
{
  ULONG mask = ( 1 << bit );
  *dword = *dword | mask;
}

/* Clear a single bit of a DWORD argument */
VOID CmClearBit32(ULONG* dword, ULONG bit)
{
  ULONG mask = 0xFFFFFFFF;
  ULONG sub = ( 1 << bit );
  mask = mask - sub;
  *dword = *dword & mask;
}

/* Clear a single bit of a WORD argument */
VOID CmClearBit16(USHORT* word, ULONG bit)
{
  USHORT mask = 0xFFFF;
  USHORT sub = (USHORT) ( 1 << bit );
  mask = mask - sub;
  *word = *word & mask;
}


int wide2ansi(PUCHAR dst, PUCHAR src, ULONG32 n) {
  ULONG32 cnt;

  if (!dst || !src)
    return -1;

  for (cnt = 0; cnt < n; ++cnt) {
    dst[cnt] = src[2*cnt];
    if (src[2*cnt + 1])
      break;
  }

  return cnt;
}
