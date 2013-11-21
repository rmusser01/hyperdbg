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

#include "vt.h"

#define _OFFSETOF(s, m)				\
  (&(((s*)0)->m))
#define DEFINE(_sym, _val)						\
  __asm__ __volatile__ ( "\n->" #_sym " %0 " #_val : : "i" (_val) )
#define BLANK()					\
  __asm__ __volatile__ ( "\n->" : : )
#define OFFSET(_sym, _str, _mem)		\
  DEFINE(_sym, _OFFSETOF(_str, _mem));

#define CONTEXT_SYMBOL(s)					\
  OFFSET(CONTEXT_##s, struct CPU_CONTEXT, GuestContext.s);

void __foo__ (void)
{
  CONTEXT_SYMBOL(rip);
  CONTEXT_SYMBOL(resumerip);
  CONTEXT_SYMBOL(rsp);
  CONTEXT_SYMBOL(cs);
  CONTEXT_SYMBOL(cr0);
  CONTEXT_SYMBOL(cr3);
  CONTEXT_SYMBOL(cr4);
  CONTEXT_SYMBOL(rflags);
  BLANK();

  CONTEXT_SYMBOL(rax);
  CONTEXT_SYMBOL(rbx);
  CONTEXT_SYMBOL(rcx);
  CONTEXT_SYMBOL(rdx);
  CONTEXT_SYMBOL(rdi);
  CONTEXT_SYMBOL(rsi);
  CONTEXT_SYMBOL(rbp);     
}
