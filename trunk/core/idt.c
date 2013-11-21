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

#include "x86.h"
#include "idt.h"

void RegisterIDTHandler(Bit16u index, void (*handler) (void))
{
  IDTR tmp_idt;
  PIDT_ENTRY descriptors, pidt_entry;

  __asm__ __volatile__(
		       "sidt %0\n"
		       :"=m"(tmp_idt)
		       ::"memory"
		       );
  
  descriptors = (PIDT_ENTRY) (tmp_idt.BaseHi << 16 | tmp_idt.BaseLo);
  pidt_entry = &(descriptors[index]);
  
  /* Copy a valid handler (0x2e).
     FIXME: Here we are assuming our handler lies in the same segment of the
     interrupt handler 0x2e.
  */
  descriptors[index] = descriptors[0x2e];
  
  pidt_entry->LowOffset  = ((hvm_address) handler) & 0xffff;
  pidt_entry->HighOffset = ((hvm_address) handler) >> 16;
}

PIDT_ENTRY GetIDTEntry(Bit8u num)
{					
  Bit32u flags;
  PIDT_ENTRY pidt_entry;
  
  /* Be sure the IF is clear */
  flags = RegGetFlags();
  RegSetFlags(flags & ~FLAGS_IF_MASK);

  pidt_entry = &((PIDT_ENTRY) (RegGetIdtBase()))[num];

  /* Restore original flags */
  RegSetFlags(flags);

  return pidt_entry;
}

void HookIDT(Bit8u entryno, Bit16u selector, void (*handler)(void))
{
  PIDT_ENTRY pidt_entry;

  pidt_entry = GetIDTEntry(entryno);
  
  __asm__ __volatile__(
		       "cli\n"
		       "pushal\n"
    
		       /* Save original CR0 */
		       "movl	%%cr0,%%eax\n"
		       "pushl	%%eax\n"
    
		       /* Disable memory protection */
		       "andl	$0xfffeffff,%%eax\n"
		       "movl	%%eax,%%cr0\n"
		       
		       /* Recover original interrupt handler */
		       "movl	%0,%%eax\n" 
		       "movw	%1,%%cx\n" 
		       "movl	%2,%%ebx\n"
		       
		       /* Set the new handler */
		       "movw	%%ax,(%%ebx)\n"   /* LowOffset */
		       "shr	$16,%%eax\n"
		       "movw	%%ax,6(%%ebx)\n" /* HighOffset */
		       "movw	%%cx,2(%%ebx)\n" /* Selector */
		       
		       /* Restore original CR0 */
		       "popl %%eax\n"
		       "movl %%eax,%%cr0\n"

		       "popal\n"
		       "sti\n"
		       ::"m"(handler), "m"(selector), "m"(pidt_entry)
		       );
}
