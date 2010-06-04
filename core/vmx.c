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

#include <ntddk.h>
#include "types.h"
#include "vt.h"
#include "idt.h"
#include "x86.h"
#include "msr.h"
#include "vmx.h"
#include "config.h"
#include "common.h"
#include "vmhandlers.h"
#include "events.h"
#include "debug.h"

#pragma warning ( disable : 4102 ) /* Unreferenced label */
#pragma warning ( disable : 4731 ) /* EBP modified by inline asm */

/* Memory types used to access the VMCS (bits 53:50 of the IA32_VMX_BASIC MSR */
#define VMX_MEMTYPE_UNCACHEABLE 0
#define VMX_MEMTYPE_WRITEBACK   6

static struct {
  Bit32u ExitReason;
  Bit32u ExitQualification;
  Bit32u ExitInterruptionInformation;
  Bit32u ExitInterruptionErrorCode;
  Bit32u ExitInstructionLength;
  Bit32u ExitInstructionInformation;

  Bit32u IDTVectoringInformationField;
  Bit32u IDTVectoringErrorCode;
} vmxcontext;

/* VMX operations */
static hvm_bool   VmxHasCPUSupport(void);
static hvm_bool   VmxIsEnabled(void);
static hvm_status VmxInitialize(void (*idt_initializer)(PIDT_ENTRY pidt));
static hvm_status VmxFinalize(void);
static hvm_status VmxHardwareEnable(void);
static hvm_status VmxHardwareDisable(void);
static void       VmxInvalidateTLB(void);
static void       VmxSetCr0(hvm_address cr0);
static void       VmxSetCr3(hvm_address cr3);
static void       VmxSetCr4(hvm_address cr4);
static void       VmxTrapIO(hvm_bool enabled);
static Bit32u     VmxGetExitInstructionLength(void);

static hvm_status VmxVmcsInitialize(hvm_address guest_stack, hvm_address guest_return);
static Bit32u     VmxVmcsRead(Bit32u encoding);
static void       VmxVmcsWrite(Bit32u encoding, Bit32u value);

static void       VmxHvmHandleExit(void);
static hvm_status VmxHvmSwitchOff(void);
static hvm_status VmxHvmUpdateEvents(void);
static void       VmxHvmInjectHwException(Bit32u trap, Bit32u type);

/* Internal VMX functions (i.e., not used outside this module) */
static void       VmxInternalHandleCR(void);
static void       VmxInternalHandleIO(void);
static void       VmxInternalHandleNMI(void);
static void       VmxInternalHvmInjectException(Bit32u type, Bit32u trap, Bit32u error_code);

/* Assembly functions (defined in i386/vmx-asm.asm) */
void    __stdcall VmxLaunch(void);
Bit32u  __stdcall VmxTurnOn(Bit32u phyvmxonhigh, Bit32u phyvmxonlow);
void    __stdcall VmxTurnOff(void);
Bit32u  __stdcall VmxClear(Bit32u phyvmxonhigh, Bit32u phyvmxonlow);
Bit32u  __stdcall VmxPtrld(Bit32u phyvmxonhigh, Bit32u phyvmxonlow);
void    __stdcall VmxResume(void);
Bit32u  __stdcall VmxRead(Bit32u encoding);
void    __stdcall VmxWrite(Bit32u encoding, Bit32u value);
void    __stdcall VmxVmCall(Bit32u num);

struct HVM_X86_OPS hvm_x86_ops = {
  /* VT-related */
  VmxHasCPUSupport,		/* vt_cpu_has_support */
  NULL,                         /* vt_disabled_by_bios */
  VmxIsEnabled,                 /* vt_enabled */
  VmxInitialize,                /* vt_initialize */
  VmxFinalize,                  /* vt_finalize */
  VmxLaunch,                    /* vt_launch */
  VmxHardwareEnable,		/* vt_hardware_enable */
  VmxHardwareDisable,		/* vt_hardware_disable */
  VmxVmCall,			/* vt_hypercall */
  VmxVmcsInitialize,		/* vt_vmcs_initialize */
  VmxVmcsRead,			/* vt_vmcs_read */
  VmxVmcsWrite,			/* vt_vmcs_write */
  VmxSetCr0,			/* vt_set_cr0 */
  VmxSetCr3,			/* vt_set_cr3 */
  VmxSetCr4,			/* vt_set_cr4 */
  VmxTrapIO,			/* vt_trap_io */
  VmxGetExitInstructionLength,	/* vt_get_exit_instr_len */

  /* Memory management */
  VmxInvalidateTLB,     	/* mmu_tlb_flush */

  /* HVM-related */
  VmxHvmHandleExit,     	/* hvm_handle_exit */
  VmxHvmSwitchOff,		/* hvm_switch_off */
  VmxHvmUpdateEvents,		/* hvm_update_events */
  VmxHvmInjectHwException,	/* hvm_inject_hw_exception */
};

/* Utility functions */
static Bit32u  VmxAdjustControls(Bit32u Ctl, Bit32u Msr);
static void    VmxReadGuestContext(void);

/* This global structure represents the initial VMX state. All these variables
   are wrapped together in order to be exposed to the plugins. */
typedef struct {
  Bit32u*           pVMXONRegion;	    /* VMA of VMXON region */
  PHYSICAL_ADDRESS  PhysicalVMXONRegionPtr; /* PMA of VMXON region */

  Bit32u*           pVMCSRegion;	    /* VMA of VMCS region */
  PHYSICAL_ADDRESS  PhysicalVMCSRegionPtr;  /* PMA of VMCS region */

  void*             VMMStack;               /* VMM stack area */

  Bit32u*           pIOBitmapA;	            /* VMA of I/O bitmap A */
  PHYSICAL_ADDRESS  PhysicalIOBitmapA;      /* PMA of I/O bitmap A */

  Bit32u*           pIOBitmapB;	            /* VMA of I/O bitmap B */
  PHYSICAL_ADDRESS  PhysicalIOBitmapB;      /* PMA of I/O bitmap B */

  PIDT_ENTRY        VMMIDT;                 /* VMM interrupt descriptor table */
} VMX_INIT_STATE, *PVMX_INIT_STATE;

static hvm_bool       vmxIsActive = FALSE;
static VMX_INIT_STATE vmxInitState;
static hvm_bool	      HandlerLogging = FALSE;

static Bit32u VmxVmcsRead(Bit32u encoding)
{
  return VmxRead(encoding);
}

static void VmxVmcsWrite(Bit32u encoding, Bit32u value)
{
  /* Adjust the value, if needed */
  switch (encoding) {
  case CPU_BASED_VM_EXEC_CONTROL:
    value = VmxAdjustControls(value, IA32_VMX_PROCBASED_CTLS);
    break;
  case PIN_BASED_VM_EXEC_CONTROL:
    value = VmxAdjustControls(value, IA32_VMX_PINBASED_CTLS);
    break;
  case VM_ENTRY_CONTROLS:
    value = VmxAdjustControls(value, IA32_VMX_ENTRY_CTLS);
    break;
  case VM_EXIT_CONTROLS:
    value = VmxAdjustControls(value, IA32_VMX_EXIT_CTLS);
    break;
  default:
    break;
  }

  VmxWrite(encoding, value);
}

static hvm_bool VmxIsEnabled(void)
{
  return vmxIsActive;
}

static hvm_status VmxVmcsInitialize(hvm_address guest_stack, hvm_address guest_return)
{
  IA32_VMX_BASIC_MSR vmxBasicMsr;
  RFLAGS rflags;
  MSR msr;
  GDTR gdt_reg;
  IDTR idt_reg;
  Bit16u seg_selector = 0;
  Bit32u temp32, gdt_base, idt_base;

  // GDT Info
  __asm { SGDT gdt_reg };
  gdt_base = (gdt_reg.BaseHi << 16) | gdt_reg.BaseLo;
	
  // IDT Segment Selector
  __asm	{ SIDT idt_reg };
  idt_base = (idt_reg.BaseHi << 16) | idt_reg.BaseLo;	

  //  27.6 PREPARATION AND LAUNCHING A VIRTUAL MACHINE
  // (1) Create a VMCS region in non-pageable memory of size specified by
  //	 the VMX capability MSR IA32_VMX_BASIC and aligned to 4-KBytes.
  //	 Software should read the capability MSRs to determine width of the 
  //	 physical addresses that may be used for a VMCS region and ensure
  //	 the entire VMCS region can be addressed by addresses with that width.
  //	 The term "guest-VMCS address" refers to the physical address of the
  //	 new VMCS region for the following steps.

  ReadMSR(IA32_VMX_BASIC_MSR_CODE, (PMSR) &vmxBasicMsr);

  switch(vmxBasicMsr.MemType) {
  case VMX_MEMTYPE_UNCACHEABLE:
    Log("Unsupported memory type %.8x", vmxBasicMsr.MemType);
    return HVM_STATUS_UNSUCCESSFUL;
    break;
  case VMX_MEMTYPE_WRITEBACK:
    break;
  default:
    Log("ERROR: Unknown VMCS region memory type");
    return HVM_STATUS_UNSUCCESSFUL;
    break;
  }
	
  // (2) Initialize the version identifier in the VMCS (first 32 bits)
  //	 with the VMCS revision identifier reported by the VMX
  //	 capability MSR IA32_VMX_BASIC.
  *(vmxInitState.pVMCSRegion) = vmxBasicMsr.RevId;

  // (3) Execute the VMCLEAR instruction by supplying the guest-VMCS address.
  //	 This will initialize the new VMCS region in memory and set the launch
  //	 state of the VMCS to "clear". This action also invalidates the
  //	 working-VMCS pointer register to FFFFFFFF_FFFFFFFFH. Software should
  //	 verify successful execution of VMCLEAR by checking if RFLAGS.CF = 0
  //	 and RFLAGS.ZF = 0.
  FLAGS_TO_ULONG(rflags) = VmxClear(0, vmxInitState.PhysicalVMCSRegionPtr.LowPart);

  if(rflags.CF != 0 || rflags.ZF != 0) {
    Log("ERROR: VMCLEAR operation failed");
    return HVM_STATUS_UNSUCCESSFUL;
  }
	
  Log("SUCCESS: VMCLEAR operation completed");
	
  // (4) Execute the VMPTRLD instruction by supplying the guest-VMCS address.
  //	 This initializes the working-VMCS pointer with the new VMCS region's
  //	 physical address.
  VmxPtrld(0, vmxInitState.PhysicalVMCSRegionPtr.LowPart);

  //
  //  ***********************************
  //  *	H.1.1 16-Bit Guest-State Fields *
  //  ***********************************

  VmxVmcsWrite(GUEST_CS_SELECTOR,   RegGetCs() & 0xfff8);
  VmxVmcsWrite(GUEST_SS_SELECTOR,   RegGetSs() & 0xfff8);
  VmxVmcsWrite(GUEST_DS_SELECTOR,   RegGetDs() & 0xfff8);
  VmxVmcsWrite(GUEST_ES_SELECTOR,   RegGetEs() & 0xfff8);
  VmxVmcsWrite(GUEST_FS_SELECTOR,   RegGetFs() & 0xfff8);
  VmxVmcsWrite(GUEST_GS_SELECTOR,   RegGetGs() & 0xfff8);
  VmxVmcsWrite(GUEST_LDTR_SELECTOR, RegGetLdtr() & 0xfff8);

  /* Guest TR selector */
  __asm	{ STR seg_selector };
  CmClearBit16(&seg_selector, 2); // TI Flag
  VmxVmcsWrite(GUEST_TR_SELECTOR, seg_selector & 0xfff8);

  //  **********************************
  //  *	H.1.2 16-Bit Host-State Fields *
  //  **********************************

  VmxVmcsWrite(HOST_CS_SELECTOR, RegGetCs() & 0xfff8);
  VmxVmcsWrite(HOST_SS_SELECTOR, RegGetSs() & 0xfff8);
  VmxVmcsWrite(HOST_DS_SELECTOR, RegGetDs() & 0xfff8);
  VmxVmcsWrite(HOST_ES_SELECTOR, RegGetEs() & 0xfff8);
  VmxVmcsWrite(HOST_FS_SELECTOR, RegGetFs() & 0xfff8);
  VmxVmcsWrite(HOST_GS_SELECTOR, RegGetGs() & 0xfff8);
  VmxVmcsWrite(HOST_TR_SELECTOR, RegGetTr() & 0xfff8);

  //  ***********************************
  //  *	H.2.2 64-Bit Guest-State Fields *
  //  ***********************************

  VmxVmcsWrite(VMCS_LINK_POINTER, 0xFFFFFFFF);
  VmxVmcsWrite(VMCS_LINK_POINTER_HIGH, 0xFFFFFFFF);

  /* Reserved Bits of IA32_DEBUGCTL MSR must be 0 */
  ReadMSR(IA32_DEBUGCTL, &msr);
  VmxVmcsWrite(GUEST_IA32_DEBUGCTL, msr.Lo);
  VmxVmcsWrite(GUEST_IA32_DEBUGCTL_HIGH, msr.Hi);

  //	*******************************
  //	* H.3.1 32-Bit Control Fields *
  //	*******************************

  /* Pin-based VM-execution controls */
  VmxVmcsWrite(PIN_BASED_VM_EXEC_CONTROL, 0);

  /* Primary processor-based VM-execution controls */
  temp32 = 0;
  CmSetBit32(&temp32, CPU_BASED_PRIMARY_IO); /* Use I/O bitmaps */
  CmSetBit32(&temp32, 7); /* HLT */
  VmxVmcsWrite(CPU_BASED_VM_EXEC_CONTROL, temp32);

  /* I/O bitmap */
  VmxVmcsWrite(IO_BITMAP_A_HIGH, vmxInitState.PhysicalIOBitmapA.HighPart);  
  VmxVmcsWrite(IO_BITMAP_A,      vmxInitState.PhysicalIOBitmapA.LowPart); 
  VmxVmcsWrite(IO_BITMAP_B_HIGH, vmxInitState.PhysicalIOBitmapB.HighPart);  
  VmxVmcsWrite(IO_BITMAP_B,      vmxInitState.PhysicalIOBitmapB.LowPart); 

  /* Time-stamp counter offset */
  VmxVmcsWrite(TSC_OFFSET, 0);
  VmxVmcsWrite(TSC_OFFSET_HIGH, 0);

  VmxVmcsWrite(PAGE_FAULT_ERROR_CODE_MASK, 0);
  VmxVmcsWrite(PAGE_FAULT_ERROR_CODE_MATCH, 0);
  VmxVmcsWrite(CR3_TARGET_COUNT, 0);
  VmxVmcsWrite(CR3_TARGET_VALUE0, 0);
  VmxVmcsWrite(CR3_TARGET_VALUE1, 0);                        
  VmxVmcsWrite(CR3_TARGET_VALUE2, 0);
  VmxVmcsWrite(CR3_TARGET_VALUE3, 0);

  /* VM-exit controls */
  temp32 = 0;
  CmSetBit32(&temp32, VM_EXIT_ACK_INTERRUPT_ON_EXIT);
  VmxVmcsWrite(VM_EXIT_CONTROLS, temp32);

  /* VM-entry controls */
  VmxVmcsWrite(VM_ENTRY_CONTROLS, 0);

  VmxVmcsWrite(VM_EXIT_MSR_STORE_COUNT, 0);
  VmxVmcsWrite(VM_EXIT_MSR_LOAD_COUNT, 0);

  VmxVmcsWrite(VM_ENTRY_MSR_LOAD_COUNT, 0);
  VmxVmcsWrite(VM_ENTRY_INTR_INFO_FIELD, 0);
	
  //  ***********************************
  //  *	H.3.3 32-Bit Guest-State Fields *
  //  ***********************************

  VmxVmcsWrite(GUEST_CS_LIMIT,   GetSegmentDescriptorLimit(gdt_base, RegGetCs()));
  VmxVmcsWrite(GUEST_SS_LIMIT,   GetSegmentDescriptorLimit(gdt_base, RegGetSs()));
  VmxVmcsWrite(GUEST_DS_LIMIT,   GetSegmentDescriptorLimit(gdt_base, RegGetDs()));
  VmxVmcsWrite(GUEST_ES_LIMIT,   GetSegmentDescriptorLimit(gdt_base, RegGetEs()));
  VmxVmcsWrite(GUEST_FS_LIMIT,   GetSegmentDescriptorLimit(gdt_base, RegGetFs()));
  VmxVmcsWrite(GUEST_GS_LIMIT,   GetSegmentDescriptorLimit(gdt_base, RegGetGs()));
  VmxVmcsWrite(GUEST_LDTR_LIMIT, GetSegmentDescriptorLimit(gdt_base, RegGetLdtr()));
  VmxVmcsWrite(GUEST_TR_LIMIT,   GetSegmentDescriptorLimit(gdt_base, RegGetTr()));

  /* Guest GDTR/IDTR limit */
  VmxVmcsWrite(GUEST_GDTR_LIMIT, gdt_reg.Limit);
  VmxVmcsWrite(GUEST_IDTR_LIMIT, idt_reg.Limit);

  /* DR7 */
  VmxVmcsWrite(GUEST_DR7, 0x400);

  /* Guest interruptibility and activity state */
  VmxVmcsWrite(GUEST_INTERRUPTIBILITY_INFO, 0);
  VmxVmcsWrite(GUEST_ACTIVITY_STATE, 0);

  /* Set segment access rights */
  VmxVmcsWrite(GUEST_CS_AR_BYTES,   GetSegmentDescriptorAR(gdt_base, RegGetCs()));
  VmxVmcsWrite(GUEST_DS_AR_BYTES,   GetSegmentDescriptorAR(gdt_base, RegGetDs()));
  VmxVmcsWrite(GUEST_SS_AR_BYTES,   GetSegmentDescriptorAR(gdt_base, RegGetSs()));
  VmxVmcsWrite(GUEST_ES_AR_BYTES,   GetSegmentDescriptorAR(gdt_base, RegGetEs()));
  VmxVmcsWrite(GUEST_FS_AR_BYTES,   GetSegmentDescriptorAR(gdt_base, RegGetFs()));
  VmxVmcsWrite(GUEST_GS_AR_BYTES,   GetSegmentDescriptorAR(gdt_base, RegGetGs()));
  VmxVmcsWrite(GUEST_LDTR_AR_BYTES, GetSegmentDescriptorAR(gdt_base, RegGetLdtr()));
  VmxVmcsWrite(GUEST_TR_AR_BYTES,   GetSegmentDescriptorAR(gdt_base, RegGetTr()));

  /* Guest IA32_SYSENTER_CS */
  ReadMSR(IA32_SYSENTER_CS, &msr);
  VmxVmcsWrite(GUEST_SYSENTER_CS, msr.Lo);

  //  ******************************************
  //  * H.4.3 Natural-Width Guest-State Fields *
  //  ******************************************

  /* Guest CR0 */
  temp32 = RegGetCr0();
  CmSetBit32(&temp32, 0);	// PE
  CmSetBit32(&temp32, 5);	// NE
  CmSetBit32(&temp32, 31);	// PG
  VmxVmcsWrite(GUEST_CR0, temp32);

  /* Guest CR3 */
  VmxVmcsWrite(GUEST_CR3, RegGetCr3());

  temp32 = RegGetCr4();
  CmSetBit32(&temp32, 13);	// VMXE
  VmxVmcsWrite(GUEST_CR4, temp32);

  /* Guest segment base addresses */
  VmxVmcsWrite(GUEST_CS_BASE,   GetSegmentDescriptorBase(gdt_base, RegGetCs()));
  VmxVmcsWrite(GUEST_SS_BASE,   GetSegmentDescriptorBase(gdt_base, RegGetSs()));
  VmxVmcsWrite(GUEST_DS_BASE,   GetSegmentDescriptorBase(gdt_base, RegGetDs()));
  VmxVmcsWrite(GUEST_ES_BASE,   GetSegmentDescriptorBase(gdt_base, RegGetEs()));
  VmxVmcsWrite(GUEST_FS_BASE,   GetSegmentDescriptorBase(gdt_base, RegGetFs()));
  VmxVmcsWrite(GUEST_GS_BASE,   GetSegmentDescriptorBase(gdt_base, RegGetGs()));
  VmxVmcsWrite(GUEST_LDTR_BASE, GetSegmentDescriptorBase(gdt_base, RegGetLdtr()));
  VmxVmcsWrite(GUEST_TR_BASE,   GetSegmentDescriptorBase(gdt_base, RegGetTr()));

  /* Guest GDTR/IDTR base */
  VmxVmcsWrite(GUEST_GDTR_BASE, gdt_reg.BaseLo | (gdt_reg.BaseHi << 16));
  VmxVmcsWrite(GUEST_IDTR_BASE, idt_reg.BaseLo | (idt_reg.BaseHi << 16));

  /* Guest RFLAGS */
  FLAGS_TO_ULONG(rflags) = RegGetFlags();
  VmxVmcsWrite(GUEST_RFLAGS, FLAGS_TO_ULONG(rflags));

  /* Guest IA32_SYSENTER_ESP */
  ReadMSR(IA32_SYSENTER_ESP, &msr);
  VmxVmcsWrite(GUEST_SYSENTER_ESP, msr.Lo);

  /* Guest IA32_SYSENTER_EIP */
  ReadMSR(IA32_SYSENTER_EIP, &msr);
  VmxVmcsWrite(GUEST_SYSENTER_EIP, msr.Lo);
	
  //	*****************************************
  //	* H.4.4 Natural-Width Host-State Fields *
  //	*****************************************

  /* Host CR0, CR3 and CR4 */
  VmxVmcsWrite(HOST_CR0, RegGetCr0() & ~(1 << 16)); /* Disable WP */
  VmxVmcsWrite(HOST_CR3, RegGetCr3());
  VmxVmcsWrite(HOST_CR4, RegGetCr4());

  /* Host FS, GS and TR base */
  VmxVmcsWrite(HOST_FS_BASE, GetSegmentDescriptorBase(gdt_base, RegGetFs()));
  VmxVmcsWrite(HOST_GS_BASE, GetSegmentDescriptorBase(gdt_base, RegGetGs()));
  VmxVmcsWrite(HOST_TR_BASE, GetSegmentDescriptorBase(gdt_base, RegGetTr()));

  /* Host GDTR/IDTR base (they both hold *linear* addresses) */
  VmxVmcsWrite(HOST_GDTR_BASE, gdt_reg.BaseLo | (gdt_reg.BaseHi << 16));
  VmxVmcsWrite(HOST_IDTR_BASE, GetSegmentDescriptorBase(gdt_base, RegGetDs()) + (Bit32u) vmxInitState.VMMIDT);

  /* Host IA32_SYSENTER_ESP/EIP/CS */
  ReadMSR(IA32_SYSENTER_ESP, &msr);
  VmxVmcsWrite(HOST_IA32_SYSENTER_ESP, msr.Lo);

  ReadMSR(IA32_SYSENTER_EIP, &msr);
  VmxVmcsWrite(HOST_IA32_SYSENTER_EIP, msr.Lo);

  ReadMSR(IA32_SYSENTER_CS, &msr);
  VmxVmcsWrite(HOST_IA32_SYSENTER_CS, msr.Lo);

  // (5) Issue a sequence of VMWRITEs to initialize various host-state area
  //	 fields in the working VMCS. The initialization sets up the context
  //	 and entry-points to the VMM VIRTUAL-MACHINE MONITOR PROGRAMMING
  //	 CONSIDERATIONS upon subsequent VM exits from the guest. Host-state
  //	 fields include control registers (CR0, CR3 and CR4), selector fields
  //	 for the segment registers (CS, SS, DS, ES, FS, GS and TR), and base-
  //	 address fields (for FS, GS, TR, GDTR and IDTR; RSP, RIP and the MSRs
  //	 that control fast system calls).

  // (6) Use VMWRITEs to set up the various VM-exit control fields, VM-entry
  //	 control fields, and VM-execution control fields in the VMCS. Care
  //	 should be taken to make sure the settings of individual fields match
  //	 the allowed 0 and 1 settings for the respective controls as reported
  //	 by the VMX capability MSRs (see Appendix G). Any settings inconsistent
  //	 with the settings reported by the capability MSRs will cause VM
  //	 entries to fail.
	
  // (7) Use VMWRITE to initialize various guest-state area fields in the
  //	 working VMCS. This sets up the context and entry-point for guest
  //	 execution upon VM entry. Chapter 22 describes the guest-state loading
  //	 and checking done by the processor for VM entries to protected and
  //	 virtual-8086 guest execution.

  /* Clear the VMX Abort Error Code prior to VMLAUNCH */
  RtlZeroMemory((vmxInitState.pVMCSRegion + 4), 4);
  Log("Clearing VMX abort error code: %.8x", *(vmxInitState.pVMCSRegion + 4));

  /* Set RIP, RSP for the Guest right before calling VMLAUNCH */
  Log("Setting Guest RSP to %.8x", guest_stack);
  VmxVmcsWrite(GUEST_RSP, (hvm_address) guest_stack);
	
  Log("Setting Guest RIP to %.8x", guest_return);
  VmxVmcsWrite(GUEST_RIP, (hvm_address) guest_return);

  /* Set RIP, RSP for the Host right before calling VMLAUNCH */
  Log("Setting Host RSP to %.8x", ((hvm_address) vmxInitState.VMMStack + VMM_STACK_SIZE - 1));
  VmxVmcsWrite(HOST_RSP, ((hvm_address) vmxInitState.VMMStack + VMM_STACK_SIZE - 1));

  Log("Setting Host RIP to %.8x", hvm_x86_ops.hvm_handle_exit);
  VmxVmcsWrite(HOST_RIP, (hvm_address) hvm_x86_ops.hvm_handle_exit);

  return HVM_STATUS_SUCCESS;
}

static hvm_status VmxInitialize(void (*idt_initializer)(PIDT_ENTRY pidt))
{
  /* Allocate the VMXON region memory */
  vmxInitState.pVMXONRegion = MmAllocateNonCachedMemory(4096);
  if(vmxInitState.pVMXONRegion == NULL) {
    WindowsLog("ERROR: Allocating VMXON region memory");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  WindowsLog("VMXONRegion virtual address:  %.8x", vmxInitState.pVMXONRegion);
  RtlZeroMemory(vmxInitState.pVMXONRegion, 4096);
  vmxInitState.PhysicalVMXONRegionPtr = MmGetPhysicalAddress(vmxInitState.pVMXONRegion);
  WindowsLog("VMXONRegion physical address: %.8x", vmxInitState.PhysicalVMXONRegionPtr.LowPart);

  /* Allocate the VMCS region memory */
  vmxInitState.pVMCSRegion = MmAllocateNonCachedMemory(4096);
  if(vmxInitState.pVMCSRegion == NULL) {
    WindowsLog("ERROR: Allocating VMCS region memory");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  WindowsLog("VMCSRegion virtual address:  %.8x", vmxInitState.pVMCSRegion);
  RtlZeroMemory(vmxInitState.pVMCSRegion, 4096);
  vmxInitState.PhysicalVMCSRegionPtr = MmGetPhysicalAddress(vmxInitState.pVMCSRegion);
  WindowsLog("VMCSRegion physical address: %.8x", vmxInitState.PhysicalVMCSRegionPtr.LowPart);
	
  /* Allocate stack for the VM exit handler */
  vmxInitState.VMMStack = ExAllocatePoolWithTag(NonPagedPool, VMM_STACK_SIZE, 'kSkF');
  if(vmxInitState.VMMStack == NULL) {
    WindowsLog("ERROR: Allocating VM exit handler stack memory");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  RtlZeroMemory(vmxInitState.VMMStack, VMM_STACK_SIZE);
  WindowsLog("VMMStack: %.8x", vmxInitState.VMMStack);

  /* Allocate a memory page for the I/O bitmap A */
  vmxInitState.pIOBitmapA = MmAllocateNonCachedMemory(4096);
  if(vmxInitState.pIOBitmapA == NULL) {
    WindowsLog("ERROR: Allocating I/O bitmap A memory");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  WindowsLog("I/O bitmap A virtual address:  %.8x", vmxInitState.pIOBitmapA);
  RtlZeroMemory(vmxInitState.pIOBitmapA, 4096);
  vmxInitState.PhysicalIOBitmapA = MmGetPhysicalAddress(vmxInitState.pIOBitmapA);
  WindowsLog("I/O bitmap A physical address: %.8x", vmxInitState.PhysicalIOBitmapA.LowPart);

  /* Allocate a memory page for the I/O bitmap A */
  vmxInitState.pIOBitmapB = MmAllocateNonCachedMemory(4096);
  if(vmxInitState.pIOBitmapB == NULL) {
    WindowsLog("ERROR: Allocating I/O bitmap A memory");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  WindowsLog("I/O bitmap A virtual address:  %.8x", vmxInitState.pIOBitmapB);
  RtlZeroMemory(vmxInitState.pIOBitmapB, 4096);
  vmxInitState.PhysicalIOBitmapB = MmGetPhysicalAddress(vmxInitState.pIOBitmapB);
  WindowsLog("I/O bitmap A physical address: %.8x", vmxInitState.PhysicalIOBitmapB.LowPart);

  /* Allocate & initialize the IDT for the VMM */
  vmxInitState.VMMIDT = MmAllocateNonCachedMemory(sizeof(IDT_ENTRY)*256);
  if (vmxInitState.VMMIDT == NULL) {
    WindowsLog("ERROR: Allocating VMM interrupt descriptor table");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  idt_initializer(vmxInitState.VMMIDT);
  WindowsLog("VMMIDT is at %.8x", vmxInitState.VMMIDT);

  return HVM_STATUS_SUCCESS;
}

static hvm_status VmxFinalize(void)
{
  /* Deallocate memory regions allocated during the initialization phase */
  if (vmxInitState.pVMXONRegion)
    MmFreeNonCachedMemory(vmxInitState.pVMXONRegion , 4096);
  if (vmxInitState.pVMCSRegion)
    MmFreeNonCachedMemory(vmxInitState.pVMCSRegion , 4096);
  if (vmxInitState.VMMStack)
    ExFreePoolWithTag(vmxInitState.VMMStack, 'kSkF');
  if (vmxInitState.pIOBitmapA)
    MmFreeNonCachedMemory(vmxInitState.pIOBitmapA , 4096);
  if (vmxInitState.pIOBitmapB)
    MmFreeNonCachedMemory(vmxInitState.pIOBitmapB , 4096);
  if (vmxInitState.VMMIDT)
    MmFreeNonCachedMemory(vmxInitState.VMMIDT , sizeof(IDT_ENTRY)*256);

  return HVM_STATUS_SUCCESS;
}

static hvm_bool VmxHasCPUSupport(void)
{
  VMX_FEATURES vmxFeatures;

  __asm {
    PUSHAD;
    MOV		EAX, 1;
    CPUID;
    // ECX contains the VMX_FEATURES FLAGS (VMX supported if bit 5 equals 1)
    MOV		vmxFeatures, ECX;
    POPAD;
  };

  return (vmxFeatures.VMX != 0);
}

static hvm_status VmxHardwareEnable(void)
{
  IA32_FEATURE_CONTROL_MSR vmxFeatureControl;
  IA32_VMX_BASIC_MSR vmxBasicMsr;
  RFLAGS  rflags;
  CR0_REG cr0_reg;
  CR4_REG cr4_reg;

  // (1) Check VMX support in processor using CPUID.
  if (!VmxHasCPUSupport()) {
    WindowsLog("VMX support not present");
    return HVM_STATUS_UNSUCCESSFUL;
  }
	
  WindowsLog("VMX support present");
	
  // (2) Determine the VMX capabilities supported by the processor through
  //     the VMX capability MSRs.
  ReadMSR(IA32_VMX_BASIC_MSR_CODE, (PMSR) &vmxBasicMsr);
  ReadMSR(IA32_FEATURE_CONTROL_CODE, (PMSR) &vmxFeatureControl);

  // (3) Create a VMXON region in non-pageable memory of a size specified by
  //	 IA32_VMX_BASIC_MSR and aligned to a 4-byte boundary. The VMXON region
  //	 must be hosted in cache-coherent memory.
  WindowsLog("VMXON region size:      %.8x", vmxBasicMsr.szVmxOnRegion);
  WindowsLog("VMXON access width bit: %.8x", vmxBasicMsr.PhyAddrWidth);
  WindowsLog("      [   1] --> 32-bit");
  WindowsLog("      [   0] --> 64-bit");
  WindowsLog("VMXON memory type:      %.8x", vmxBasicMsr.MemType);
  WindowsLog("      [   0]  --> Strong uncacheable");
  WindowsLog("      [ 1-5]  --> Unused");
  WindowsLog("      [   6]  --> Write back");
  WindowsLog("      [7-15]  --> Unused");

  switch(vmxBasicMsr.MemType) {
  case VMX_MEMTYPE_UNCACHEABLE:
    WindowsLog("Unsupported memory type %.8x", vmxBasicMsr.MemType);
    return HVM_STATUS_UNSUCCESSFUL;
    break;
  case VMX_MEMTYPE_WRITEBACK:
    break;
  default:
    WindowsLog("ERROR: Unknown VMXON region memory type");
    return HVM_STATUS_UNSUCCESSFUL;
    break;
  }

  // (4) Initialize the version identifier in the VMXON region (first 32 bits)
  //	 with the VMCS revision identifier reported by capability MSRs.
  *(vmxInitState.pVMXONRegion) = vmxBasicMsr.RevId;
	
  WindowsLog("vmxBasicMsr.RevId: %.8x", vmxBasicMsr.RevId);

  // (5) Ensure the current processor operating mode meets the required CR0
  //	 fixed bits (CR0.PE=1, CR0.PG=1). Other required CR0 fixed bits can
  //	 be detected through the IA32_VMX_CR0_FIXED0 and IA32_VMX_CR0_FIXED1
  //	 MSRs.
  CR0_TO_ULONG(cr0_reg) = RegGetCr0();

  if(cr0_reg.PE != 1) {
    WindowsLog("ERROR: Protected mode not enabled");
    WindowsLog("Value of CR0: %.8x", CR0_TO_ULONG(cr0_reg));
    return HVM_STATUS_UNSUCCESSFUL;
  }

  WindowsLog("Protected mode enabled");

  if(cr0_reg.PG != 1) {
    WindowsLog("ERROR: Paging not enabled");
    WindowsLog("Value of CR0: %.8x", CR0_TO_ULONG(cr0_reg));
    return HVM_STATUS_UNSUCCESSFUL;
  }
	
  WindowsLog("Paging enabled");

  // This was required by first processors that supported VMX	
  cr0_reg.NE = 1;

  RegSetCr0(CR0_TO_ULONG(cr0_reg));

  // (6) Enable VMX operation by setting CR4.VMXE=1 [bit 13]. Ensure the
  //	 resultant CR4 value supports all the CR4 fixed bits reported in
  //	 the IA32_VMX_CR4_FIXED0 and IA32_VMX_CR4_FIXED1 MSRs.
  CR4_TO_ULONG(cr4_reg) = RegGetCr4();

  WindowsLog("Old CR4: %.8x", CR4_TO_ULONG(cr4_reg));
  cr4_reg.VMXE = 1;
  WindowsLog("New CR4: %.8x", CR4_TO_ULONG(cr4_reg));

  RegSetCr4(CR4_TO_ULONG(cr4_reg));
	
  // (7) Ensure that the IA32_FEATURE_CONTROL_MSR (MSR index 0x3A) has been
  //	 properly programmed and that its lock bit is set (bit 0=1) and VMX is
  //	 enabled (bit 2=1). This MSR is generally configured by the BIOS using 
  //	 WRMSR. If it's not set, it's safe for us to set it.
  WindowsLog("IA32_FEATURE_CONTROL Lock Bit: %.8x, EnableVmx bit %.8x", 
	     vmxFeatureControl.Lock, vmxFeatureControl.EnableVmxon);
	
  if(vmxFeatureControl.Lock != 1) {
    // MSR hasn't been locked, we can enable VMX and lock it
    WindowsLog("Setting IA32_FEATURE_CONTROL Lock Bit and Vmxon Enable bit");
    vmxFeatureControl.EnableVmxon = 1;
    vmxFeatureControl.Lock = 1;
    WriteMSR(IA32_FEATURE_CONTROL_CODE, 0, ((PMSR)&vmxFeatureControl)->Lo);
  } else {
    if(vmxFeatureControl.EnableVmxon == 0) {
      // If the lock bit is set and VMX is disabled, it was most likely done
      // by the BIOS and we can't use VMXON
      WindowsLog("ERROR: VMX is disabled by the BIOS");
      return HVM_STATUS_UNSUCCESSFUL;
    }
  }

  // (8) Execute VMXON with the physical address of the VMXON region as the
  //	 operand. Check successful execution of VMXON by checking if
  //	 RFLAGS.CF=0.
  FLAGS_TO_ULONG(rflags) = VmxTurnOn(0, vmxInitState.PhysicalVMXONRegionPtr.LowPart);

  if(rflags.CF == 1) {
    WindowsLog("ERROR: VMXON operation failed");
    return HVM_STATUS_UNSUCCESSFUL;
  }

  /* VMXON was successful, so we cannot use WindowsLog() anymore */
  vmxIsActive = TRUE;

  Log("SUCCESS: VMXON operation completed");
  Log("VMM is now running");
	
  return HVM_STATUS_SUCCESS;
}

static hvm_status VmxHardwareDisable(void)
{
  /* TODO */
  return HVM_STATUS_SUCCESS;
}

static void VmxSetCr0(hvm_address cr0)
{
  hvm_x86_ops.vt_vmcs_write(GUEST_CR0, cr0);
}

static void VmxSetCr3(hvm_address cr3)
{
  hvm_x86_ops.vt_vmcs_write(GUEST_CR3, cr3);
}

static void VmxSetCr4(hvm_address cr4)
{
  hvm_x86_ops.vt_vmcs_write(GUEST_CR4, cr4);
}

static void VmxTrapIO(hvm_bool enabled)
{
  Bit32u v;

  v = hvm_x86_ops.vt_vmcs_read(CPU_BASED_VM_EXEC_CONTROL);

  if (enabled) {
    /* Enable I/O bitmaps */
    CmSetBit32(&v,   CPU_BASED_PRIMARY_IO);
  } else {
    /* Disable I/O bitmaps */
    CmClearBit32(&v, CPU_BASED_PRIMARY_IO);
  }

  hvm_x86_ops.vt_vmcs_write(CPU_BASED_VM_EXEC_CONTROL, v);
}

static Bit32u VmxGetExitInstructionLength(void)
{
  return vmxcontext.ExitInstructionLength;
}

static void VmxInvalidateTLB(void)
{
  __asm { 
    MOV EAX, CR3;
    MOV CR3, EAX;     
  };
}

Bit32u VmxAdjustControls(Bit32u c, Bit32u n)
{
  MSR msr;

  ReadMSR(n, &msr);
  c &= msr.Hi;     /* bit == 0 in high word ==> must be zero */
  c |= msr.Lo;     /* bit == 1 in low word  ==> must be one  */

  return c;
}

/* Updates CPU state with the values from the VMCS cache structure. 

   NOTE: we update only those registers that are not already present in the
   (hardware) VMCS. */
static __declspec(naked) void VmxUpdateGuestContext(void)
{
  VmxVmcsWrite(GUEST_RFLAGS, FLAGS_TO_ULONG(context.GuestContext.RFLAGS));

  /* Set the next instruction to be executed */
  VmxVmcsWrite(GUEST_RIP, context.GuestContext.ResumeRIP);

  __asm { MOV EAX, context.GuestContext.RAX };
  __asm { MOV EBX, context.GuestContext.RBX };
  __asm { MOV ECX, context.GuestContext.RCX };
  __asm { MOV EDX, context.GuestContext.RDX };
  __asm { MOV EDI, context.GuestContext.RDI };
  __asm { MOV ESI, context.GuestContext.RSI };
  __asm { MOV EBP, context.GuestContext.RBP };
  __asm { RET } ;
}

/* Read guest state from the VMCS into the global CPU_CONTEXT variable. This
   procedure reads everything *except for* general purpose registers.

   NOTE: general purpose registers are not stored into the VMCS, so they are
   saved at the very beginning of the VmxEntryPoint() procedure */
static void VmxReadGuestContext(void)
{
  /* Exit state */
  vmxcontext.ExitReason                   = VmxRead(VM_EXIT_REASON);
  vmxcontext.ExitQualification            = VmxRead(EXIT_QUALIFICATION);
  vmxcontext.ExitInterruptionInformation  = VmxRead(VM_EXIT_INTR_INFO);
  vmxcontext.ExitInterruptionErrorCode    = VmxRead(VM_EXIT_INTR_ERROR_CODE);
  vmxcontext.IDTVectoringInformationField = VmxRead(IDT_VECTORING_INFO_FIELD);
  vmxcontext.IDTVectoringErrorCode        = VmxRead(IDT_VECTORING_ERROR_CODE);
  vmxcontext.ExitInstructionLength        = VmxRead(VM_EXIT_INSTRUCTION_LEN);
  vmxcontext.ExitInstructionInformation   = VmxRead(VMX_INSTRUCTION_INFO);

  /* Read guest state */
  context.GuestContext.RIP    = VmxRead(GUEST_RIP);
  context.GuestContext.RSP    = VmxRead(GUEST_RSP);
  context.GuestContext.CS     = VmxRead(GUEST_CS_SELECTOR);
  context.GuestContext.CR0    = VmxRead(GUEST_CR0);
  context.GuestContext.CR3    = VmxRead(GUEST_CR3);
  context.GuestContext.CR4    = VmxRead(GUEST_CR4);
  context.GuestContext.RFLAGS = VmxRead(GUEST_RFLAGS);

  /* Writing the Guest VMCS RIP uses general registers. Must complete this
     before setting general registers for guest return state */
  context.GuestContext.ResumeRIP = context.GuestContext.RIP + vmxcontext.ExitInstructionLength;
}

static hvm_status VmxHvmUpdateEvents(void)
{
  Bit32u temp32;

  /* I/O bitmap */
  EventUpdateIOBitmaps((Bit8u*) vmxInitState.pIOBitmapA, (Bit8u*) vmxInitState.pIOBitmapB);

  /* Exception bitmap */
  temp32 = 0;
  EventUpdateExceptionBitmap(&temp32);
  CmSetBit32(&temp32, TRAP_DEBUG);   // DO NOT DISABLE: needed to step over I/O instructions!!!
  VmxVmcsWrite(EXCEPTION_BITMAP, temp32);

  return HVM_STATUS_SUCCESS;
}

static void VmxInternalHvmInjectException(Bit32u type, Bit32u trap, Bit32u error_code)
{
  Bit32u v;

  /* Write the VM-entry interruption-information field */
  v = (INTR_INFO_VALID_MASK | trap | type);

  /* Check if bits 11 (deliver code) and 31 (valid) are set. In this
     case, error code has to be delivered to guest OS */
  if (error_code != HVM_DELIVER_NO_ERROR_CODE) {
    VmxVmcsWrite(VM_ENTRY_EXCEPTION_ERROR_CODE, error_code);
    v |= INTR_INFO_DELIVER_CODE_MASK;
  }

  VmxVmcsWrite(VM_ENTRY_INTR_INFO_FIELD, v);
}

static void VmxHvmInjectHwException(Bit32u trap, Bit32u error_code)
{
  VmxInternalHvmInjectException(INTR_TYPE_HW_EXCEPTION, trap, error_code);
}

static hvm_status VmxHvmSwitchOff(void)
{
  /* Switch off VMX mode */
  Log("Terminating VMX Mode");
  Log("Flow returning to address %.8x", context.GuestContext.ResumeRIP);

  /* TODO: We should restore the whole original guest state here -- se Joanna's
     source code */
  RegSetIdtr((void*) VmxRead(GUEST_IDTR_BASE), VmxRead(GUEST_IDTR_LIMIT));

  __asm {
    /* Restore guest CR0 */
    MOV EAX, context.GuestContext.CR0;
    MOV CR0, EAX;

    /* Restore guest CR3 */
    MOV EAX, context.GuestContext.CR3;
    MOV CR3, EAX;

    /* Restore guest CR4 */
    MOV EAX, context.GuestContext.CR4;

    /* mov cr4, eax; */
    _emit 0x0f;
    _emit 0x22;
    _emit 0xe0;

  };

  /* Turn off VMX */
  VmxTurnOff();
  vmxIsActive = FALSE;

  __asm {
    /* Restore general-purpose registers */
    MOV EAX, context.GuestContext.RAX;
    MOV EBX, context.GuestContext.RBX;
    MOV ECX, context.GuestContext.RCX;
    MOV EDX, context.GuestContext.RDX;
    MOV EDI, context.GuestContext.RDI;
    MOV ESI, context.GuestContext.RSI;
    MOV EBP, context.GuestContext.RBP;

    /* Restore ESP */
    MOV	ESP, context.GuestContext.RSP;

    /* Restore guest RFLAGS */
    PUSH EAX;
    MOV EAX, context.GuestContext.RFLAGS;
    PUSH EAX;
    POPFD;
    POP EAX;

    /* Resume guest execution */
    JMP	context.GuestContext.ResumeRIP;
  };

  /* Unreachable */
  return STATUS_SUCCESS;
}

static void VmxInternalHandleCR(void)
{
  Bit8u movcrControlRegister;
  Bit32u movcrAccessType, movcrOperandType, movcrGeneralPurposeRegister;
  VtCrAccessType accesstype;
  VtRegister gpr;

  movcrControlRegister = (Bit8u) (vmxcontext.ExitQualification & 0x0000000F);
  movcrAccessType      = ((vmxcontext.ExitQualification & 0x00000030) >> 4);
  movcrOperandType     = ((vmxcontext.ExitQualification & 0x00000040) >> 6);
  movcrGeneralPurposeRegister = ((vmxcontext.ExitQualification & 0x00000F00) >> 8);

  /* Read access type */
  switch (movcrAccessType) {
  case 0:
    accesstype = VT_CR_ACCESS_WRITE;
    break;
  case 1:
  default: 
    accesstype = VT_CR_ACCESS_READ; 
    break;
  case 2: 
    accesstype = VT_CR_ACCESS_CLTS;  
    break;
  case 3: 
    accesstype = VT_CR_ACCESS_LMSW;  
    break;
  }

  /* Read general purpose register */
  if (movcrOperandType == 1 && accesstype != VT_CR_ACCESS_CLTS && accesstype != VT_CR_ACCESS_LMSW) {
    switch (movcrGeneralPurposeRegister) {
    case 0:  gpr = VT_REGISTER_RAX; break;
    case 1:  gpr = VT_REGISTER_RCX; break;
    case 2:  gpr = VT_REGISTER_RDX; break;
    case 3:  gpr = VT_REGISTER_RBX; break;
    case 4:  gpr = VT_REGISTER_RSP; break;
    case 5:  gpr = VT_REGISTER_RBP; break;
    case 6:  gpr = VT_REGISTER_RSI; break;
    case 7:  gpr = VT_REGISTER_RDI; break;
    case 8:  gpr = VT_REGISTER_R8;  break;
    case 9:  gpr = VT_REGISTER_R9;  break;
    case 10: gpr = VT_REGISTER_R10; break;
    case 11: gpr = VT_REGISTER_R11; break;
    case 12: gpr = VT_REGISTER_R12; break;
    case 13: gpr = VT_REGISTER_R13; break;
    case 14: gpr = VT_REGISTER_R14; break;
    case 15: gpr = VT_REGISTER_R15; break;
    default: gpr = 0;               break;
    }
  } else {
    gpr = 0;
  }

  /* Invoke the VMX-independent handler */
  HandleCR(movcrControlRegister,           /* Control register involved in this operation */
	   accesstype,                     /* Access type */
	   movcrOperandType == 1,          /* Does this operation involve a memory operand? */
	   gpr                  	   /* The general purpose register involved in this operation 
					      (only if the previous argument is "false") */
	   );
}

static void VmxInternalHandleIO(void)
{
  Bit8u    size;
  Bit16u   port;
  hvm_bool isoutput, isstring, isrep;

  port     = (Bit16u) ((vmxcontext.ExitQualification & 0xffff0000) >> 16);
  size     = (vmxcontext.ExitQualification & 7) + 1;
  isoutput = !(vmxcontext.ExitQualification & (1 << 3));
  isstring = (vmxcontext.ExitQualification & (1 << 4)) != 0;
  isrep    = (vmxcontext.ExitQualification & (1 << 5)) != 0;

  HandleIO(port,		/* I/O port */
	   isoutput,		/* Direction */
	   size,		/* I/O operation size */
	   isstring,		/* Is this a string operation? */
	   isrep		/* Does this instruction have a REP prefix? */
	   );
}

static void VmxInternalHandleNMI(void)
{
  Bit32u trap, error_code;

  trap = vmxcontext.ExitInterruptionInformation & INTR_INFO_VECTOR_MASK;

  /* Check if bits 11 (deliver code) and 31 (valid) are set. In this
     case, error code has to be delivered to guest OS */
  if ((vmxcontext.ExitInterruptionInformation & INTR_INFO_DELIVER_CODE_MASK) &&
      (vmxcontext.ExitInterruptionInformation & INTR_INFO_VALID_MASK)) {
    error_code = vmxcontext.ExitInterruptionErrorCode;
  } else {
    error_code = HVM_DELIVER_NO_ERROR_CODE;
  }

  HandleNMI(trap, 		         /* Trap number */
	    error_code,			 /* Exception error code */
	    vmxcontext.ExitQualification /* Exit qualification 
					    (should be meaningful only for #PF and #DB) */
	    );
}

///////////////////////
//  VMM Entry Point  //
///////////////////////
__declspec(naked) void VmxHvmHandleExit()
{
  __asm	{ PUSHFD };

  /* Record the general-purpose registers. We save them here in order to be
     sure that they don't get tampered by the execution of the VMM */
  __asm { MOV context.GuestContext.RAX, EAX };
  __asm { MOV context.GuestContext.RBX, EBX };
  __asm { MOV context.GuestContext.RCX, ECX };
  __asm { MOV context.GuestContext.RDX, EDX };
  __asm { MOV context.GuestContext.RDI, EDI };
  __asm { MOV context.GuestContext.RSI, ESI };
  __asm { MOV context.GuestContext.RBP, EBP };

  /* Restore host EBP */
  __asm	{ MOV EBP, ESP };

  VmxReadGuestContext();

  /* Restore host IDT -- Not sure if this is really needed. I'm pretty sure we
     have to fix the LIMIT fields of host's IDTR. */
  RegSetIdtr((void*) VmxRead(HOST_IDTR_BASE), 0x7ff);

  /* Enable logging only for particular VM-exit events */
  if( vmxcontext.ExitReason == EXIT_REASON_VMCALL ) {
    HandlerLogging = TRUE;
  } else {
    HandlerLogging = FALSE;
  }

  if(HandlerLogging) {
    Log("----- VMM Handler CPU0 -----");
    Log("Guest RAX: %.8x", context.GuestContext.RAX);
    Log("Guest RBX: %.8x", context.GuestContext.RBX);
    Log("Guest RCX: %.8x", context.GuestContext.RCX);
    Log("Guest RDX: %.8x", context.GuestContext.RDX);
    Log("Guest RDI: %.8x", context.GuestContext.RDI);
    Log("Guest RSI: %.8x", context.GuestContext.RSI);
    Log("Guest RBP: %.8x", context.GuestContext.RBP);
    Log("Exit Reason:        %.8x", vmxcontext.ExitReason);
    Log("Exit Qualification: %.8x", vmxcontext.ExitQualification);
    Log("Exit Interruption Information:   %.8x", vmxcontext.ExitInterruptionInformation);
    Log("Exit Interruption Error Code:    %.8x", vmxcontext.ExitInterruptionErrorCode);
    Log("IDT-Vectoring Information Field: %.8x", vmxcontext.IDTVectoringInformationField);
    Log("IDT-Vectoring Error Code:        %.8x", vmxcontext.IDTVectoringErrorCode);
    Log("VM-Exit Instruction Length:      %.8x", vmxcontext.ExitInstructionLength);
    Log("VM-Exit Instruction Information: %.8x", vmxcontext.ExitInstructionInformation);
    Log("VM Exit RIP: %.8x", context.GuestContext.RIP);
    Log("VM Exit RSP: %.8x", context.GuestContext.RSP);
    Log("VM Exit CS:  %.4x", context.GuestContext.CS);
    Log("VM Exit CR0: %.8x", context.GuestContext.CR0);
    Log("VM Exit CR3: %.8x", context.GuestContext.CR3);
    Log("VM Exit CR4: %.8x", context.GuestContext.CR4);
    Log("VM Exit RFLAGS: %.8x", context.GuestContext.RFLAGS);
  }

  /////////////////////////////////////////////
  //  *** EXIT REASON CHECKS START HERE ***  //
  /////////////////////////////////////////////

  switch(vmxcontext.ExitReason) {
    /////////////////////////////////////////////////////////////////////////////////////
    //  VMCLEAR, VMLAUNCH, VMPTRLD, VMPTRST, VMREAD, VMWRITE, VMRESUME, VMXOFF, VMXON  //
    /////////////////////////////////////////////////////////////////////////////////////

  case EXIT_REASON_VMCLEAR:
  case EXIT_REASON_VMPTRLD: 
  case EXIT_REASON_VMPTRST: 
  case EXIT_REASON_VMREAD:  
  case EXIT_REASON_VMRESUME:
  case EXIT_REASON_VMWRITE:
  case EXIT_REASON_VMXOFF:
  case EXIT_REASON_VMXON:
    Log("Request has been denied (reason: %.8x)", vmxcontext.ExitReason);

    VmxUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

  case EXIT_REASON_VMLAUNCH:
    HandleVMLAUNCH();

    VmxUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

    //////////////
    //  VMCALL  //
    //////////////
  case EXIT_REASON_VMCALL:
    HandleVMCALL();

    VmxUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

    ////////////
    //  INVD  //
    ////////////
  case EXIT_REASON_INVD:
    Log("INVD detected");

    __asm { INVD };

    VmxUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

    /////////////
    //  RDMSR  //
    /////////////
  case EXIT_REASON_MSR_READ:
    Log("Read MSR #%.8x", context.GuestContext.RCX);

    VmxUpdateGuestContext();

    __asm {
			
      MOV		ECX, context.GuestContext.RCX;
      RDMSR;

      JMP		Resume;
    };

    /* Unreachable */
    break;

    /////////////
    //  WRMSR  //
    /////////////
  case EXIT_REASON_MSR_WRITE:
    Log("Write MSR #%.8x", context.GuestContext.RCX);

    WriteMSR(context.GuestContext.RCX, context.GuestContext.RDX, context.GuestContext.RAX);
    VmxUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

    /////////////
    //  CPUID  //
    /////////////
  case EXIT_REASON_CPUID:
    if(HandlerLogging) {
      Log("CPUID detected (RAX: %.8x)", context.GuestContext.RAX);
    }

    /* XXX Do we really need this check? */
    if(context.GuestContext.RAX == 0x00000000) {
      VmxUpdateGuestContext();

      __asm {
	MOV		EAX, 0x00000000;
	CPUID;
	MOV		EBX, 0x61656C43;
	MOV		ECX, 0x2E636E6C;
	MOV		EDX, 0x74614872;
	JMP		Resume;
      };
    }

    VmxUpdateGuestContext();

    __asm {
      MOV		EAX, context.GuestContext.RAX;
      CPUID;
      JMP		Resume;
    };

    /* Unreachable */
    break;

    ///////////////////////////////
    //  Control Register Access  //
    ///////////////////////////////
  case EXIT_REASON_CR_ACCESS:
    VmxInternalHandleCR();

    VmxUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

    ///////////////////////
    //  I/O instruction  //
    ///////////////////////
  case EXIT_REASON_IO_INSTRUCTION:
    VmxInternalHandleIO();

    VmxUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

  case EXIT_REASON_EXCEPTION_NMI:
    VmxInternalHandleNMI();

    VmxUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

  case EXIT_REASON_HLT:
    HandleHLT();

    // Cannot execute HLT with interrupt disabled
    // __asm { HLT };

    VmxUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

  default:
    /* Unknown exit condition */
    break;
  }

	
 Exit:
  // This label is reached when we are not able to handle the reason that
  // triggered this VM exit. In this case, we simply switch off VMX mode.
  HypercallSwitchOff(NULL);
	
 Resume:
  // Exit reason handled. Need to execute the VMRESUME without having
  // changed the state of the GPR and ESP et cetera.
  __asm { POPFD } ;

  VmxResume();
}

