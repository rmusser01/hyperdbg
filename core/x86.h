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

#ifndef _PILL_X86_H
#define _PILL_X86_H

#include "types.h"
#include "msr.h"
#include "common.h"

/* Trap/fault mnemonics */
#define TRAP_DIVIDE_ERROR      0
#define TRAP_DEBUG             1
#define TRAP_NMI               2
#define TRAP_INT3              3
#define TRAP_OVERFLOW          4
#define TRAP_BOUNDS            5
#define TRAP_INVALID_OP        6
#define TRAP_NO_DEVICE         7
#define TRAP_DOUBLE_FAULT      8
#define TRAP_COPRO_SEG         9
#define TRAP_INVALID_TSS      10
#define TRAP_NO_SEGMENT       11
#define TRAP_STACK_ERROR      12
#define TRAP_GP_FAULT         13
#define TRAP_PAGE_FAULT       14
#define TRAP_SPURIOUS_INT     15
#define TRAP_COPRO_ERROR      16
#define TRAP_ALIGNMENT_CHECK  17
#define TRAP_MACHINE_CHECK    18
#define TRAP_SIMD_ERROR       19
#define TRAP_DEFERRED_NMI     31

#pragma pack (push, 1)

//////////////
//  RFLAGS  //
//////////////
typedef struct _RFLAGS
{
#if HVM_ARCH_BITS == 64
  unsigned ReservedHi	:32;
#endif
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
  unsigned TF		:1;		// Trap flag
  unsigned SF		:1;		// Sign flag
  unsigned ZF		:1;		// Zero flag
  unsigned Reserved3	:1;
  unsigned AF		:1;		// Borrow flag
  unsigned Reserved4	:1;
  unsigned PF		:1;		// Parity flag
  unsigned Reserved5	:1;
  unsigned CF		:1;		// Carry flag [Bit 0]
} RFLAGS;

#define FLAGS_CF_MASK (1 << 0)
#define FLAGS_PF_MASK (1 << 2)
#define FLAGS_AF_MASK (1 << 4)
#define FLAGS_ZF_MASK (1 << 6)
#define FLAGS_SF_MASK (1 << 7)
#define FLAGS_TF_MASK (1 << 8)
#define FLAGS_IF_MASK (1 << 9)
#define FLAGS_OF_MASK (1 << 11)
#define FLAGS_RF_MASK (1 << 16)
#define FLAGS_TO_ULONG(f) (*(hvm_address*)(&f))

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

#define CR0_TO_ULONG(cr0) (*(hvm_address*)(&cr0))
#define CR4_TO_ULONG(cr4) (*(hvm_address*)(&cr4))

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
  unsigned Limit  :16;
  unsigned BaseLo :16;
  unsigned BaseHi :16;
} GDTR;

typedef struct
{
  unsigned LimitLo :16;
  unsigned BaseLo  :16;
  unsigned BaseMid :8;
  unsigned Type	   :4;
  unsigned System  :1;
  unsigned DPL	   :2;
  unsigned Present :1;
  unsigned LimitHi :4;
  unsigned AVL	   :1;
  unsigned L	   :1;
  unsigned DB	   :1;
  unsigned Gran	   :1;	// Granularity
  unsigned BaseHi  :8;
} SEGMENT_DESCRIPTOR, *PSEGMENT_DESCRIPTOR;

typedef struct
{
  Bit16u limit0;
  Bit16u base0;
  Bit8u  base1;
  Bit8u  attr0;
  Bit8u  limit1attr1;
  Bit8u  base2;
} SEGMENT_DESCRIPTOR2, *PSEGMENT_DESCRIPTOR2;

/* 
* Attribute for segment selector. This is a copy of bit 40:47 & 52:55 of the
* segment descriptor. 
*/
typedef union
{
  Bit16u UCHARs;
  struct
  {
    Bit16u type :4;              /* 0;  Bit 40-43 */
    Bit16u s    :1;              /* 4;  Bit 44 */
    Bit16u dpl  :2;              /* 5;  Bit 45-46 */
    Bit16u p    :1;              /* 7;  Bit 47 */
    // gap!       
    Bit16u avl  :1;              /* 8;  Bit 52 */
    Bit16u l    :1;              /* 9;  Bit 53 */
    Bit16u db   :1;              /* 10; Bit 54 */
    Bit16u g    :1;              /* 11; Bit 55 */
    Bit16u Gap  :4;
  } fields;
} SEGMENT_ATTRIBUTES;

/* Page table entry */
#ifdef ENABLE_PAE
#define PTE_DATA_FIELD Bit64u
#else
#define PTE_DATA_FIELD Bit32u
#endif
typedef struct _PTE
{
  PTE_DATA_FIELD Present      :1;
  PTE_DATA_FIELD Writable     :1;
  PTE_DATA_FIELD Owner        :1;
  PTE_DATA_FIELD WriteThrough :1;
  PTE_DATA_FIELD CacheDisable :1;
  PTE_DATA_FIELD Accessed     :1;
  PTE_DATA_FIELD Dirty        :1;
  PTE_DATA_FIELD LargePage    :1;
  PTE_DATA_FIELD Global       :1;
  PTE_DATA_FIELD ForUse1      :1;
  PTE_DATA_FIELD ForUse2      :1;
  PTE_DATA_FIELD ForUse3      :1;
#ifdef ENABLE_PAE
  PTE_DATA_FIELD PageBaseAddr :36;
  PTE_DATA_FIELD Reserved     :16;
#else
  PTE_DATA_FIELD PageBaseAddr :20;
#endif
} PTE, *PPTE;
#undef PTE_DATA_FIELD

typedef struct
{
  Bit16u sel;
  SEGMENT_ATTRIBUTES attributes;
  Bit32u limit;
  Bit64u base;
} SEGMENT_SELECTOR, *PSEGMENT_SELECTOR;

#pragma pack (pop)

/* Access to 32-bit registers */
Bit32u RegGetFlags(void);
Bit32u RegGetCr0(void);
Bit32u RegGetCr2(void);
Bit32u RegGetCr3(void);
Bit32u RegGetCr4(void);
Bit32u RegGetIdtBase(void);

void USESTACK RegSetFlags(hvm_address v);
void USESTACK RegSetCr0(hvm_address v);
void USESTACK RegSetCr2(hvm_address v);
void USESTACK RegSetCr4(hvm_address v);

/* Access to 16-bit registers */
Bit16u RegGetCs(void);
Bit16u RegGetDs(void);
Bit16u RegGetEs(void);
Bit16u RegGetFs(void);
Bit16u RegGetGs(void);
Bit16u RegGetSs(void);
Bit16u RegGetTr(void);
Bit16u RegGetLdtr(void);
Bit16u RegGetIdtLimit(void);

void USESTACK RegSetIdtr(void *base, Bit32u limit);
void USESTACK RegRdtsc(Bit64u *pv);

/* Segments-related stuff */
Bit32u GetSegmentDescriptorBase(Bit32u gdt_base , Bit16u seg_selector);
Bit32u GetSegmentDescriptorDPL(Bit32u gdt_base, Bit16u seg_selector);
Bit32u GetSegmentDescriptorLimit(Bit32u gdt_base, Bit16u selector);
Bit32u GetSegmentDescriptorAR(Bit32u gdt_base, Bit16u selector);

/* IO-access stuff */
Bit8u  USESTACK IoReadPortByte(Bit16u portno);
Bit16u USESTACK IoReadPortWord(Bit16u portno);
Bit32u USESTACK IoReadPortDword(Bit16u portno);
void   USESTACK IoWritePortByte(Bit16u portno, Bit8u value);
void   USESTACK IoWritePortWord(Bit16u portno, Bit16u value);
void   USESTACK IoWritePortDword(Bit16u portno, Bit32u value);

#endif /* _PILL_X86_H */
