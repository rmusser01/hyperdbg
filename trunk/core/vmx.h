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

#ifndef _PILL_VMX_H
#define _PILL_VMX_H

#include "types.h"

////////////////////
//  VMX FEATURES  //
////////////////////
typedef struct _VMX_FEATURES
{
  unsigned SSE3		:1;		// SSE3 Extensions
  unsigned RES1		:2;
  unsigned MONITOR	:1;		// MONITOR/WAIT
  unsigned DS_CPL	:1;		// CPL qualified Debug Store
  unsigned VMX		:1;		// Virtual Machine Technology
  unsigned RES2		:1;
  unsigned EST		:1;		// Enhanced Intel© Speedstep Technology
  unsigned TM2		:1;		// Thermal monitor 2
  unsigned SSSE3	:1;		// SSSE3 extensions
  unsigned CID		:1;		// L1 context ID
  unsigned RES3		:2;
  unsigned CX16		:1;		// CMPXCHG16B
  unsigned xTPR		:1;		// Update control
  unsigned PDCM		:1;		// Performance/Debug capability MSR
  unsigned RES4		:2;
  unsigned DCA		:1;
  unsigned RES5		:13;
	
} VMX_FEATURES;

/////////////////
//  MISC DATA  //
/////////////////
typedef struct _MISC_DATA
{
  unsigned	Reserved1		:6;	// [0-5]
  unsigned	ActivityStates      	:3;	// [6-8]
  unsigned	Reserved2		:7;	// [9-15]
  unsigned	CR3Targets		:9;	// [16-24]

  // 512*(N+1) is the recommended maximum number of MSRs
  unsigned	MaxMSRs			:3;	// [25-27]

  unsigned	Reserved3		:4;	// [28-31]
  unsigned	MSEGRevID		:32;	// [32-63]

} MISC_DATA;

/* Exit reasons */
#define EXIT_REASON_EXCEPTION_NMI        0
#define EXIT_REASON_EXTERNAL_INTERRUPT   1
#define EXIT_REASON_TRIPLE_FAULT         2
#define EXIT_REASON_INIT                 3
#define EXIT_REASON_SIPI                 4
#define EXIT_REASON_IO_SMI               5
#define EXIT_REASON_OTHER_SMI            6
#define EXIT_REASON_PENDING_INTERRUPT    7
#define EXIT_REASON_TASK_SWITCH          9
#define EXIT_REASON_CPUID               10
#define EXIT_REASON_HLT                 12
#define EXIT_REASON_INVD                13
#define EXIT_REASON_INVLPG              14
#define EXIT_REASON_RDPMC               15
#define EXIT_REASON_RDTSC               16
#define EXIT_REASON_RSM                 17
#define EXIT_REASON_VMCALL              18
#define EXIT_REASON_VMCLEAR             19
#define EXIT_REASON_VMLAUNCH            20
#define EXIT_REASON_VMPTRLD             21
#define EXIT_REASON_VMPTRST             22
#define EXIT_REASON_VMREAD              23
#define EXIT_REASON_VMRESUME            24
#define EXIT_REASON_VMWRITE             25
#define EXIT_REASON_VMXOFF              26
#define EXIT_REASON_VMXON               27
#define EXIT_REASON_CR_ACCESS           28
#define EXIT_REASON_DR_ACCESS           29
#define EXIT_REASON_IO_INSTRUCTION      30
#define EXIT_REASON_MSR_READ            31
#define EXIT_REASON_MSR_WRITE           32
#define EXIT_REASON_INVALID_GUEST_STATE 33
#define EXIT_REASON_MSR_LOADING         34
#define EXIT_REASON_MWAIT_INSTRUCTION   36
#define EXIT_REASON_MONITOR_INSTRUCTION 39
#define EXIT_REASON_PAUSE_INSTRUCTION   40
#define EXIT_REASON_MACHINE_CHECK       41
#define EXIT_REASON_TPR_BELOW_THRESHOLD 43

/* VM-execution control bits */
#define CPU_BASED_PRIMARY_IO            25

/* VM-exit control bits */
#define VM_EXIT_ACK_INTERRUPT_ON_EXIT   15

/* Exception/NMI-related information */
#define INTR_INFO_VECTOR_MASK           0xff            /* bits 0:7 */
#define INTR_INFO_INTR_TYPE_MASK        0x700           /* bits 8:10 */
#define INTR_INFO_DELIVER_CODE_MASK     0x800           /* bit 11 must be set to push error code on guest stack*/
#define INTR_INFO_VALID_MASK            0x80000000      /* bit 31 must be set to identify valid events */
#define INTR_TYPE_EXT_INTR              (0 << 8)        /* external interrupt */
#define INTR_TYPE_NMI                   (2 << 8)        /* NMI */
#define INTR_TYPE_HW_EXCEPTION          (3 << 8)        /* hardware exception */
#define INTR_TYPE_SW_EXCEPTION          (6 << 8)        /* software exception */

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

/* VMCS Encodings */
enum {
  GUEST_ES_SELECTOR = 0x00000800,
  GUEST_CS_SELECTOR = 0x00000802,
  GUEST_SS_SELECTOR = 0x00000804,
  GUEST_DS_SELECTOR = 0x00000806,
  GUEST_FS_SELECTOR = 0x00000808,
  GUEST_GS_SELECTOR = 0x0000080a,
  GUEST_LDTR_SELECTOR = 0x0000080c,
  GUEST_TR_SELECTOR = 0x0000080e,
  HOST_ES_SELECTOR = 0x00000c00,
  HOST_CS_SELECTOR = 0x00000c02,
  HOST_SS_SELECTOR = 0x00000c04,
  HOST_DS_SELECTOR = 0x00000c06,
  HOST_FS_SELECTOR = 0x00000c08,
  HOST_GS_SELECTOR = 0x00000c0a,
  HOST_TR_SELECTOR = 0x00000c0c,
  IO_BITMAP_A = 0x00002000,
  IO_BITMAP_A_HIGH = 0x00002001,
  IO_BITMAP_B = 0x00002002,
  IO_BITMAP_B_HIGH = 0x00002003,
  MSR_BITMAP = 0x00002004,
  MSR_BITMAP_HIGH = 0x00002005,
  VM_EXIT_MSR_STORE_ADDR = 0x00002006,
  VM_EXIT_MSR_STORE_ADDR_HIGH = 0x00002007,
  VM_EXIT_MSR_LOAD_ADDR = 0x00002008,
  VM_EXIT_MSR_LOAD_ADDR_HIGH = 0x00002009,
  VM_ENTRY_MSR_LOAD_ADDR = 0x0000200a,
  VM_ENTRY_MSR_LOAD_ADDR_HIGH = 0x0000200b,
  TSC_OFFSET = 0x00002010,
  TSC_OFFSET_HIGH = 0x00002011,
  VIRTUAL_APIC_PAGE_ADDR = 0x00002012,
  VIRTUAL_APIC_PAGE_ADDR_HIGH = 0x00002013,
  VMCS_LINK_POINTER = 0x00002800,
  VMCS_LINK_POINTER_HIGH = 0x00002801,
  GUEST_IA32_DEBUGCTL = 0x00002802,
  GUEST_IA32_DEBUGCTL_HIGH = 0x00002803,
  PIN_BASED_VM_EXEC_CONTROL = 0x00004000,
  CPU_BASED_VM_EXEC_CONTROL = 0x00004002,
  EXCEPTION_BITMAP = 0x00004004,
  PAGE_FAULT_ERROR_CODE_MASK = 0x00004006,
  PAGE_FAULT_ERROR_CODE_MATCH = 0x00004008,
  CR3_TARGET_COUNT = 0x0000400a,
  VM_EXIT_CONTROLS = 0x0000400c,
  VM_EXIT_MSR_STORE_COUNT = 0x0000400e,
  VM_EXIT_MSR_LOAD_COUNT = 0x00004010,
  VM_ENTRY_CONTROLS = 0x00004012,
  VM_ENTRY_MSR_LOAD_COUNT = 0x00004014,
  VM_ENTRY_INTR_INFO_FIELD = 0x00004016,
  VM_ENTRY_EXCEPTION_ERROR_CODE = 0x00004018,
  VM_ENTRY_INSTRUCTION_LEN = 0x0000401a,
  TPR_THRESHOLD = 0x0000401c,
  SECONDARY_VM_EXEC_CONTROL = 0x0000401e,
  VM_INSTRUCTION_ERROR = 0x00004400,
  VM_EXIT_REASON = 0x00004402,
  VM_EXIT_INTR_INFO = 0x00004404,
  VM_EXIT_INTR_ERROR_CODE = 0x00004406,
  IDT_VECTORING_INFO_FIELD = 0x00004408,
  IDT_VECTORING_ERROR_CODE = 0x0000440a,
  VM_EXIT_INSTRUCTION_LEN = 0x0000440c,
  VMX_INSTRUCTION_INFO = 0x0000440e,
  GUEST_ES_LIMIT = 0x00004800,
  GUEST_CS_LIMIT = 0x00004802,
  GUEST_SS_LIMIT = 0x00004804,
  GUEST_DS_LIMIT = 0x00004806,
  GUEST_FS_LIMIT = 0x00004808,
  GUEST_GS_LIMIT = 0x0000480a,
  GUEST_LDTR_LIMIT = 0x0000480c,
  GUEST_TR_LIMIT = 0x0000480e,
  GUEST_GDTR_LIMIT = 0x00004810,
  GUEST_IDTR_LIMIT = 0x00004812,
  GUEST_ES_AR_BYTES = 0x00004814,
  GUEST_CS_AR_BYTES = 0x00004816,
  GUEST_SS_AR_BYTES = 0x00004818,
  GUEST_DS_AR_BYTES = 0x0000481a,
  GUEST_FS_AR_BYTES = 0x0000481c,
  GUEST_GS_AR_BYTES = 0x0000481e,
  GUEST_LDTR_AR_BYTES = 0x00004820,
  GUEST_TR_AR_BYTES = 0x00004822,
  GUEST_INTERRUPTIBILITY_INFO = 0x00004824,
  GUEST_ACTIVITY_STATE = 0x00004826,
  GUEST_SM_BASE = 0x00004828,
  GUEST_SYSENTER_CS = 0x0000482A,
  HOST_IA32_SYSENTER_CS = 0x00004c00,
  CR0_GUEST_HOST_MASK = 0x00006000,
  CR4_GUEST_HOST_MASK = 0x00006002,
  CR0_READ_SHADOW = 0x00006004,
  CR4_READ_SHADOW = 0x00006006,
  CR3_TARGET_VALUE0 = 0x00006008,
  CR3_TARGET_VALUE1 = 0x0000600a,
  CR3_TARGET_VALUE2 = 0x0000600c,
  CR3_TARGET_VALUE3 = 0x0000600e,
  EXIT_QUALIFICATION = 0x00006400,
  GUEST_LINEAR_ADDRESS = 0x0000640a,
  GUEST_CR0 = 0x00006800,
  GUEST_CR3 = 0x00006802,
  GUEST_CR4 = 0x00006804,
  GUEST_ES_BASE = 0x00006806,
  GUEST_CS_BASE = 0x00006808,
  GUEST_SS_BASE = 0x0000680a,
  GUEST_DS_BASE = 0x0000680c,
  GUEST_FS_BASE = 0x0000680e,
  GUEST_GS_BASE = 0x00006810,
  GUEST_LDTR_BASE = 0x00006812,
  GUEST_TR_BASE = 0x00006814,
  GUEST_GDTR_BASE = 0x00006816,
  GUEST_IDTR_BASE = 0x00006818,
  GUEST_DR7 = 0x0000681a,
  GUEST_RSP = 0x0000681c,
  GUEST_RIP = 0x0000681e,
  GUEST_RFLAGS = 0x00006820,
  GUEST_PENDING_DBG_EXCEPTIONS = 0x00006822,
  GUEST_SYSENTER_ESP = 0x00006824,
  GUEST_SYSENTER_EIP = 0x00006826,
  HOST_CR0 = 0x00006c00,
  HOST_CR3 = 0x00006c02,
  HOST_CR4 = 0x00006c04,
  HOST_FS_BASE = 0x00006c06,
  HOST_GS_BASE = 0x00006c08,
  HOST_TR_BASE = 0x00006c0a,
  HOST_GDTR_BASE = 0x00006c0c,
  HOST_IDTR_BASE = 0x00006c0e,
  HOST_IA32_SYSENTER_ESP = 0x00006c10,
  HOST_IA32_SYSENTER_EIP = 0x00006c12,
  HOST_RSP = 0x00006c14,
  HOST_RIP = 0x00006c16,
};

#endif /* _PILL_VMX_H */
