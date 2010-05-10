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

#ifndef _PILL_X86_H
#define _PILL_X86_H

#include <ntddk.h>
#include "msr.h"

#pragma pack (push, 1)

//////////////
//  EFLAGS  //
//////////////
typedef struct _EFLAGS
{
  unsigned Reserved1	:10;
  unsigned ID		:1;		// Identification flag
  unsigned VIP		:1;		// Virtual interrupt pending
  unsigned VIF		:1;		// Virtual interrupt flag
  unsigned AC		:1;		// Alignment check
  unsigned VM		:1;		// Virtual 8086 mode
  unsigned RF		:1;		// Resume flag
  unsigned Reserved2	:1;
  unsigned NT		:1;		// Nested task flag
  unsigned IOPL		:2;		// I/O privilege level
  unsigned OF		:1;
  unsigned DF		:1;
  unsigned IF		:1;		// Interrupt flag
  unsigned TF		:1;		// Task flag
  unsigned SF		:1;		// Sign flag
  unsigned ZF		:1;		// Zero flag
  unsigned Reserved3	:1;
  unsigned AF		:1;		// Borrow flag
  unsigned Reserved4	:1;
  unsigned PF		:1;		// Parity flag
  unsigned Reserved5	:1;
  unsigned CF		:1;		// Carry flag [Bit 0]
} EFLAGS;

#define FLAGS_CF_MASK (1 << 0)
#define FLAGS_PF_MASK (1 << 2)
#define FLAGS_AF_MASK (1 << 4)
#define FLAGS_ZF_MASK (1 << 6)
#define FLAGS_SF_MASK (1 << 7)
#define FLAGS_TF_MASK (1 << 8)
#define FLAGS_IF_MASK (1 << 9)
#define FLAGS_RF_MASK (1 << 16)
#define FLAGS_TO_ULONG(f) (*(ULONG32*)(&f))

/////////////////////////
//  CONTROL REGISTERS  //
/////////////////////////
typedef struct _CR0_REG
{
  unsigned PE		:1;			// Protected Mode Enabled [Bit 0]
  unsigned MP		:1;			// Monitor Coprocessor FLAG
  unsigned EM		:1;			// Emulate FLAG
  unsigned TS		:1;			// Task Switched FLAG
  unsigned ET		:1;			// Extension Type FLAG
  unsigned NE		:1;			// Numeric Error
  unsigned Reserved1	:10;	        	// 
  unsigned WP		:1;			// Write Protect
  unsigned Reserved2	:1;			// 
  unsigned AM		:1;			// Alignment Mask
  unsigned Reserved3	:10;     		// 
  unsigned NW		:1;			// Not Write-Through
  unsigned CD		:1;			// Cache Disable
  unsigned PG		:1;			// Paging Enabled
} CR0_REG;

typedef struct _CR4_REG
{
  unsigned VME		:1;			// Virtual Mode Extensions
  unsigned PVI		:1;			// Protected-Mode Virtual Interrupts
  unsigned TSD		:1;			// Time Stamp Disable
  unsigned DE		:1;			// Debugging Extensions
  unsigned PSE		:1;			// Page Size Extensions
  unsigned PAE		:1;			// Physical Address Extension
  unsigned MCE		:1;			// Machine-Check Enable
  unsigned PGE		:1;			// Page Global Enable
  unsigned PCE		:1;			// Performance-Monitoring Counter Enable
  unsigned OSFXSR	:1;			// OS Support for FXSAVE/FXRSTOR
  unsigned OSXMMEXCPT	:1;			// OS Support for Unmasked SIMD Floating-Point Exceptions
  unsigned Reserved1	:2;			// 
  unsigned VMXE		:1;			// Virtual Machine Extensions Enabled
  unsigned Reserved2	:18;		        // 
} CR4_REG;

#define CR0_TO_ULONG(cr0) (*(ULONG32*)(&cr0))
#define CR4_TO_ULONG(cr4) (*(ULONG32*)(&cr4))

//////////////////////////
//  SELECTOR REGISTERS  //
//////////////////////////

#define LA_ACCESSED		0x01
#define LA_READABLE		0x02    // for code segments
#define LA_WRITABLE		0x02    // for data segments
#define LA_CONFORMING	        0x04    // for code segments
#define LA_EXPANDDOWN	        0x04    // for data segments
#define LA_CODE			0x08
#define LA_STANDARD		0x10
#define LA_DPL_0		0x00
#define LA_DPL_1		0x20
#define LA_DPL_2		0x40
#define LA_DPL_3		0x60
#define LA_PRESENT		0x80

#define LA_LDT64		0x02
#define LA_ATSS64		0x09
#define LA_BTSS64		0x0b
#define LA_CALLGATE64	        0x0c
#define LA_INTGATE64	        0x0e
#define LA_TRAPGATE64	        0x0f

#define HA_AVAILABLE	        0x01
#define HA_LONG			0x02
#define HA_DB			0x04
#define HA_GRANULARITY	        0x08

typedef struct _GDTR
{
  unsigned	Limit		:16;
  unsigned	BaseLo		:16;
  unsigned	BaseHi		:16;
} GDTR;

typedef struct
{
  unsigned	LimitLo	:16;
  unsigned	BaseLo	:16;
  unsigned	BaseMid	:8;
  unsigned	Type	:4;
  unsigned	System	:1;
  unsigned	DPL	:2;
  unsigned	Present	:1;
  unsigned	LimitHi	:4;
  unsigned	AVL	:1;
  unsigned	L	:1;
  unsigned	DB	:1;
  unsigned	Gran	:1;	// Granularity
  unsigned	BaseHi	:8;
} SEGMENT_DESCRIPTOR, *PSEGMENT_DESCRIPTOR;

typedef struct
{
  USHORT limit0;
  USHORT base0;
  UCHAR  base1;
  UCHAR  attr0;
  UCHAR  limit1attr1;
  UCHAR  base2;
} SEGMENT_DESCRIPTOR2, *PSEGMENT_DESCRIPTOR2;

/* 
* Attribute for segment selector. This is a copy of bit 40:47 & 52:55 of the
* segment descriptor. 
*/
typedef union
{
  USHORT UCHARs;
  struct
  {
    USHORT type:4;              /* 0;  Bit 40-43 */
    USHORT s:1;                 /* 4;  Bit 44 */
    USHORT dpl:2;               /* 5;  Bit 45-46 */
    USHORT p:1;                 /* 7;  Bit 47 */
    // gap!       
    USHORT avl:1;               /* 8;  Bit 52 */
    USHORT l:1;                 /* 9;  Bit 53 */
    USHORT db:1;                /* 10; Bit 54 */
    USHORT g:1;                 /* 11; Bit 55 */
    USHORT Gap:4;
  } fields;
} SEGMENT_ATTRIBUTES;

/* Page table entry */
typedef struct _PTE
{
  ULONG Present           :1;
  ULONG Writable          :1;
  ULONG Owner             :1;
  ULONG WriteThrough      :1;
  ULONG CacheDisable      :1;
  ULONG Accessed          :1;
  ULONG Dirty             :1;
  ULONG LargePage         :1;
  ULONG Global            :1;
  ULONG ForUse1           :1;
  ULONG ForUse2           :1;
  ULONG ForUse3           :1;
  ULONG PageBaseAddr      :20;
} PTE, *PPTE;

typedef struct
{
  USHORT sel;
  SEGMENT_ATTRIBUTES attributes;
  ULONG32 limit;
  ULONG64 base;
} SEGMENT_SELECTOR, *PSEGMENT_SELECTOR;

#pragma pack (pop)

/* Access to 32-bit registers */
ULONG32 RegGetFlags();
ULONG32 RegGetCr0();
ULONG32 RegGetCr2();
ULONG32 RegGetCr3();
ULONG32 RegGetCr4();
ULONG32 RegGetIdtBase();

/* Access to 16-bit registers */
USHORT  RegGetCs();
USHORT  RegGetDs();
USHORT  RegGetEs();
USHORT  RegGetFs();
USHORT  RegGetGs();
USHORT  RegGetSs();
USHORT  RegGetTr();
USHORT  RegGetLdtr();
USHORT  RegGetIdtLimit();

VOID RegSetFlags(ULONG32 v);
VOID RegSetCr0(ULONG32 v);
VOID RegSetCr4(ULONG32 v);
VOID RegSetIdtr(PVOID base, ULONG limit);

/* Read MSR register to LARGE_INTEGER variable */
ULONG64 ReadMSRToLarge(ULONG32 reg);

/* Segments-related stuff */
ULONG    GetSegmentDescriptorBase(ULONG gdt_base , USHORT seg_selector);
ULONG    GetSegmentDescriptorDPL(ULONG gdt_base, USHORT seg_selector);
ULONG    GetSegmentDescriptorLimit(ULONG gdt_base, USHORT selector);
ULONG    GetSegmentDescriptorAR(ULONG gdt_base, USHORT selector);

#endif /* _PILL_X86_H */
