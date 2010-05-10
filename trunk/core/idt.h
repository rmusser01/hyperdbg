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

#ifndef _IDT_H
#define _IDT_H

#include <ntddk.h>

#define IDT_TYPE_TASK_GATE    0b00101
#define IDT_TYPE_32_INT_GATE  0b01110
#define IDT_TYPE_16_INT_GATE  0b00110
#define IDT_TYPE_32_TRAP_GATE 0b01111
#define IDT_TYPE_16_TRAP_GATE 0b00111

typedef struct {
  unsigned      LowOffset       :16;
  unsigned      Selector        :16;
  unsigned      Access          :16;
  unsigned      HighOffset      :16;
} IDT_ENTRY, *PIDT_ENTRY;

typedef struct _IDTR {
  unsigned	Limit		:16;
  unsigned	BaseLo		:16;
  unsigned	BaseHi		:16;
} IDTR;

VOID       NullIDTHandler(VOID);
VOID       RegisterIDTHandler(USHORT index, VOID (*handler) (VOID));
PIDT_ENTRY GetIDTEntry(UCHAR num);
VOID       HookIDT(UCHAR entryno, USHORT selector, VOID (*handler)(VOID));

#endif	/* _IDT_H */
