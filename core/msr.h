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

#ifndef _PILL_MSR_H
#define _PILL_MSR_H

#include "common.h"

#pragma pack (push, 1)

/* MSRs */
#define IA32_FEATURE_CONTROL_CODE		0x03A
#define IA32_SYSENTER_CS                        0x174
#define IA32_SYSENTER_ESP                       0x175
#define IA32_SYSENTER_EIP                       0x176
#define IA32_DEBUGCTL                           0x1D9
#define IA32_VMX_BASIC_MSR_CODE			0x480
#define IA32_VMX_PINBASED_CTLS                  0x481
#define IA32_VMX_PROCBASED_CTLS                 0x482
#define IA32_VMX_EXIT_CTLS                      0x483
#define IA32_VMX_ENTRY_CTLS                     0x484
#define IA32_VMX_MISC                           0x485
#define IA32_VMX_CR0_FIXED0                     0x486
#define IA32_VMX_CR0_FIXED1                     0x487
#define IA32_VMX_CR4_FIXED0                     0x488
#define IA32_VMX_CR4_FIXED1                     0x489
#define	IA32_FS_BASE    		   0xc0000100
#define	IA32_GS_BASE	                   0xc0000101

#define MSR_IA32_MTRRCAP			0xfe
#define MSR_IA32_MTRR_DEF_TYPE			0x2ff
#define MSR_IA32_MTRR_PHYSBASE(n)		(0x200 + 2*(n))
#define MSR_IA32_MTRR_PHYSMASK(n)		(0x200 + 2*(n) + 1)
#define MSR_IA32_MTRR_FIX64K_00000		0x250
#define MSR_IA32_MTRR_FIX16K_80000		0x258
#define MSR_IA32_MTRR_FIX16K_A0000		0x259
#define MSR_IA32_MTRR_FIX4K_C0000		0x268
#define MSR_IA32_MTRR_FIX4K_C8000		0x269
#define MSR_IA32_MTRR_FIX4K_D0000		0x26a
#define MSR_IA32_MTRR_FIX4K_D8000		0x26b
#define MSR_IA32_MTRR_FIX4K_E0000		0x26c
#define MSR_IA32_MTRR_FIX4K_E8000		0x26d
#define MSR_IA32_MTRR_FIX4K_F0000		0x26e
#define MSR_IA32_MTRR_FIX4K_F8000		0x26f

///////////
//  MSR  //
///////////
typedef struct _MSR
{
  Bit32u	Lo;
  Bit32u	Hi;
} MSR, *PMSR;

/////////////////////////////
//  SPECIAL MSR REGISTERS  //
/////////////////////////////
typedef struct _IA32_VMX_BASIC_MSR
{

  unsigned RevId		:32;	// Bits 31...0 contain the VMCS revision identifier
  unsigned szVmxOnRegion        :12;	// Bits 43...32 report # of bytes for VMXON region 
  unsigned RegionClear	        :1;	// Bit 44 set only if bits 32-43 are clear
  unsigned Reserved1		:3;	// Undefined
  unsigned PhyAddrWidth	        :1;	// Physical address width for referencing VMXON, VMCS, etc.
  unsigned DualMon		:1;	// Reports whether the processor supports dual-monitor
                                        // treatment of SMI and SMM
  unsigned MemType		:4;	// Memory type that the processor uses to access the VMCS
  unsigned VmExitReport 	:1;	// Reports weather the procesor reports info in the VM-exit
                                        // instruction information field on VM exits due to execution
                                        // of the INS and OUTS instructions
  unsigned Reserved2		:9;	// Undefined

} IA32_VMX_BASIC_MSR;


typedef struct _IA32_FEATURE_CONTROL_MSR
{
  unsigned Lock			:1;	// Bit 0 is the lock bit - cannot be modified once lock is set
  unsigned Reserved1		:1;	// Undefined
  unsigned EnableVmxon	        :1;	// Bit 2. If this bit is clear, VMXON causes a general protection exception
  unsigned Reserved2		:29;	// Undefined
  unsigned Reserved3		:32;	// Undefined

} IA32_FEATURE_CONTROL_MSR;

#pragma pack (pop)

/* Read MSR register to LARGE_INTEGER variable */
void USESTACK ReadMSR (Bit32u reg, PMSR msr);
void USESTACK WriteMSR(Bit32u reg, Bit32u highpart, Bit32u lowpart);

#endif /* _PILL_MSR_H */
