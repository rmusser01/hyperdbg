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

#include "symsearch.h"
#include "syms.h"
#include "debug.h"
#include "vmmstring.h"

#define MAX(p,q) (((p) >= (q)) ? (p) : (q))

/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

static BOOLEAN DicotomicSymbolSearch(ULONG addr, LONG32 start, LONG32 end, PULONG32 index);

/* ################ */
/* #### BODIES #### */
/* ################ */

PSYMBOL SymbolGetFromAddress(PULONG addr)
{
  PSYMBOL SearchedSym;
  ULONG32 index;

  if(!DicotomicSymbolSearch((ULONG) addr, 0, NOS-1, (PULONG) &index)) {
    SearchedSym = NULL;
  } else {
    SearchedSym = &syms[index];
  }

  return SearchedSym;
}

PSYMBOL SymbolGetNearest(PULONG addr)
{
  PSYMBOL SearchedSym;
  ULONG32 index;

  DicotomicSymbolSearch((ULONG) addr, 0, NOS-1, (PULONG) &index);
  SearchedSym = &syms[index];

  return SearchedSym;
}

/* Unfortunately, the list is sorted on the address, so we have to use a linear
   search algotithm */
PSYMBOL SymbolGetFromName(PUCHAR name)
{
  PSYMBOL SearchedSym;
  ULONG32 index;

  for(index = 0; index < NOS; index++) {
    SearchedSym = &syms[index];
    /* we use MAX because we have to check with the longer length, otherwise we
       could match, for example, KiFastCallEntry2 when looking for
       KiFastCallEntry */
    if(vmm_strncmpi(SearchedSym->name, name, MAX(strlen(name), strlen(SearchedSym->name))) == 0)
      return SearchedSym;
  }
  return NULL;
}

/* Returns TRUE and sets Index to the index of the found entries if
   found. Otherwise, returns FALSE and Index is undefined */
static BOOLEAN DicotomicSymbolSearch(ULONG addr, LONG32 start, LONG32 end, PULONG32 index)
{
  ULONG32 mid;
  PSYMBOL CurrentSym;

  mid = (start+end)/2;
  CurrentSym = &syms[mid];

  while(end >= start) {
    if(addr == (CurrentSym->addr + hyperdbg_state.win_state.kernel_base)) {
      *index = mid;
      return TRUE;
    }

    if(addr < (CurrentSym->addr + hyperdbg_state.win_state.kernel_base)) {
      end = mid - 1;
    } else {
      start = mid + 1;
    }

    return DicotomicSymbolSearch(addr, start, end, index);
  }

  /* Not found! */
  *index = mid;
  return FALSE; 
}
