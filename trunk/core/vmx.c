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

#ifdef GUEST_WINDOWS
#include <ddk/ntddk.h>
#elif defined GUEST_LINUX
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mempool.h>
#endif

#include "types.h"
#include "vt.h"
#include "idt.h"
#include "x86.h"
#include "msr.h"
#include "mmu.h"
#include "vmx.h"
#include "config.h"
#include "common.h"
#include "vmhandlers.h"
#include "events.h"
#include "debug.h"
#include "vmmstring.h"

#ifdef ENABLE_EPT
#include "ept.h"
#endif

/* Memory types used to access the VMCS (bits 53:50 of the IA32_VMX_BASIC MSR) */
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

#ifdef ENABLE_EPT
  hvm_address     GuestLinearAddress;
  hvm_phy_address GuestPhysicalAddress;
#endif
	
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

static hvm_status          VmxVmcsInitialize(hvm_address guest_stack, hvm_address guest_return, hvm_address host_cr3);
static Bit32u     USESTACK VmxVmcsRead(Bit32u encoding);
       void       USESTACK VmxVmcsWrite(Bit32u encoding, Bit32u value) asm("_VmxVmcsWrite");

static hvm_status VmxHvmSwitchOff(void);
static hvm_status VmxHvmUpdateEvents(void);
static void       VmxHvmInjectHwException(Bit32u trap, Bit32u type);
       void       VmxHvmInternalHandleExit(void) asm("_VmxHvmInternalHandleExit");
/* Internal VMX functions (i.e., not used outside this module) */
static void       VmxInternalHandleCR(void);
static void       VmxInternalHandleIO(void);
static void       VmxInternalHandleNMI(void);
static void       VmxInternalHvmInjectException(Bit32u type, Bit32u trap, Bit32u error_code);

/* Assembly functions (defined in i386/vmx-asm.s) */
void            VmxLaunch(void);
Bit32u USESTACK VmxTurnOn(Bit32u phyvmxonhigh, Bit32u phyvmxonlow);
void            VmxTurnOff(void);
Bit32u USESTACK VmxClear(Bit32u phyvmxonhigh, Bit32u phyvmxonlow);
Bit32u USESTACK VmxPtrld(Bit32u phyvmxonhigh, Bit32u phyvmxonlow);
void            VmxResume(void);
Bit32u USESTACK VmxRead(Bit32u encoding);
void   USESTACK VmxWrite(Bit32u encoding, Bit32u value);
void   USESTACK VmxVmCall(Bit32u num);
void            VmxHvmHandleExit(void);
#ifdef ENABLE_EPT
void   USESTACK EptInvept(Bit32u eptp_high, Bit32u eptp_low, Bit32u rsvd_high, Bit32u rsvd_low);
#endif

struct HVM_X86_OPS hvm_x86_ops = {
  /* VT-related */
  &VmxHasCPUSupport,		/* vt_cpu_has_support */
  NULL,                         /* vt_disabled_by_bios */
  &VmxIsEnabled,                /* vt_enabled */
  &VmxInitialize,               /* vt_initialize */
  &VmxFinalize,                 /* vt_finalize */
  (void*)&VmxLaunch,            /* vt_launch */
  &VmxHardwareEnable,		/* vt_hardware_enable */
  &VmxHardwareDisable,		/* vt_hardware_disable */
  (void*)&VmxVmCall,            /* vt_hypercall */
  &VmxVmcsInitialize,		/* vt_vmcs_initialize */
  &VmxVmcsRead,			/* vt_vmcs_read */
  &VmxVmcsWrite,		/* vt_vmcs_write */
  &VmxSetCr0,			/* vt_set_cr0 */
  &VmxSetCr3,			/* vt_set_cr3 */
  &VmxSetCr4,			/* vt_set_cr4 */
  &VmxTrapIO,			/* vt_trap_io */
  &VmxGetExitInstructionLength,	/* vt_get_exit_instr_len */

  /* Memory management */
  &VmxInvalidateTLB,     	/* mmu_tlb_flush */

  /* HVM-related */
  &VmxHvmHandleExit,     	/* hvm_handle_exit */
  &VmxHvmSwitchOff,		/* hvm_switch_off */
  &VmxHvmUpdateEvents,		/* hvm_update_events */
  &VmxHvmInjectHwException,	/* hvm_inject_hw_exception */
};

/* Utility functions */
static Bit32u VmxAdjustControls(Bit32u Ctl, Bit32u Msr);
static void   VmxReadGuestContext(void);

/* This global structure represents the initial VMX state. All these variables
   are wrapped together in order to be exposed to the plugins. */
typedef struct {
  Bit32u*           pVMXONRegion;	    /* VMA of VMXON region */
  hvm_phy_address   PhysicalVMXONRegionPtr; /* PMA of VMXON region */

  Bit32u*           pVMCSRegion;	    /* VMA of VMCS region */
  hvm_phy_address   PhysicalVMCSRegionPtr;  /* PMA of VMCS region */

  void*             VMMStack;               /* VMM stack area */

  Bit32u*           pIOBitmapA;	            /* VMA of I/O bitmap A */
  hvm_phy_address   PhysicalIOBitmapA;      /* PMA of I/O bitmap A */

  Bit32u*           pIOBitmapB;	            /* VMA of I/O bitmap B */
  hvm_phy_address   PhysicalIOBitmapB;      /* PMA of I/O bitmap B */

  PIDT_ENTRY        VMMIDT;                 /* VMM interrupt descriptor table */
} VMX_INIT_STATE, *PVMX_INIT_STATE;

static hvm_bool       vmxIsActive = FALSE;
static VMX_INIT_STATE vmxInitState;
static hvm_bool	      HandlerLogging = FALSE;

static Bit32u USESTACK VmxVmcsRead(Bit32u encoding)
{
  return VmxRead(encoding);
}

void USESTACK VmxVmcsWrite(Bit32u encoding, Bit32u value)
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

static hvm_status VmxVmcsInitialize(hvm_address guest_stack, hvm_address guest_return, hvm_address host_cr3)
{
  IA32_VMX_BASIC_MSR vmxBasicMsr;
  RFLAGS rflags;
  MSR msr;
  GDTR gdt_reg;
  IDTR idt_reg;
  Bit16u seg_selector = 0;
  Bit32u temp32, gdt_base, idt_base;

#ifdef ENABLE_EPT
  hvm_phy_address phys_pdpt, phys_pd, phys_pt;
  hvm_address pdpt, pd, pt;
  Bit32u i, j, h, map;
  Bit64u temp64;
#endif

	// GDT Info
  __asm__ __volatile__ (
			"sgdt %0\n"
			:"=m"(gdt_reg)
			::"memory"
			);
  gdt_base = (gdt_reg.BaseHi << 16) | gdt_reg.BaseLo;
	
  // IDT Segment Selector
  __asm__ __volatile__ (
			"sidt %0\n"
			:"=m"(idt_reg)
			::"memory"
			);
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
  FLAGS_TO_ULONG(rflags) = VmxClear(GET32H(vmxInitState.PhysicalVMCSRegionPtr), GET32L(vmxInitState.PhysicalVMCSRegionPtr));

  if(rflags.CF != 0 || rflags.ZF != 0) {
    Log("ERROR: VMCLEAR operation failed");
    return HVM_STATUS_UNSUCCESSFUL;
  }
	
  Log("SUCCESS: VMCLEAR operation completed");
	
  // (4) Execute the VMPTRLD instruction by supplying the guest-VMCS address.
  //	 This initializes the working-VMCS pointer with the new VMCS region's
  //	 physical address.
  VmxPtrld(GET32H(vmxInitState.PhysicalVMCSRegionPtr), GET32L(vmxInitState.PhysicalVMCSRegionPtr));

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
  __asm__ __volatile__	(
			 "str %0\n"
			 :"=m"(seg_selector)
			 ::"memory"
			 );
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
	CmSetBit32(&temp32, CPU_BASED_PRIMARY_HLT); /* HLT */
  CmSetBit32(&temp32, CPU_BASED_USE_MSR_BITMAPS); /* Enable MSR bitmaps */
  VmxVmcsWrite(CPU_BASED_VM_EXEC_CONTROL, temp32);

  /* I/O bitmap */
  VmxVmcsWrite(IO_BITMAP_A_HIGH, GET32H(vmxInitState.PhysicalIOBitmapA));  
  VmxVmcsWrite(IO_BITMAP_A,      GET32L(vmxInitState.PhysicalIOBitmapA)); 
  VmxVmcsWrite(IO_BITMAP_B_HIGH, GET32H(vmxInitState.PhysicalIOBitmapB));  
  VmxVmcsWrite(IO_BITMAP_B,      GET32L(vmxInitState.PhysicalIOBitmapB)); 

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
  Log("Setting Host CR3 to %.8x%.8x", GET32H(host_cr3), GET32L(host_cr3));
  VmxVmcsWrite(HOST_CR3, host_cr3);
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
  vmm_memset((vmxInitState.pVMCSRegion + 4), 0, 4);
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

#ifdef ENABLE_EPT

  EPTInit();

  /* Init Virtual PT Bases */
  vmm_memset(VIRT_PT_BASES, 0, HOST_GB*512*sizeof(hvm_address));
  /* Allocate memory for EPT paging structures */

  /* We need only one entry of EPT PML4 table */
  Pml4 = (hvm_address)GUEST_MALLOC(4096);	/* Alloc 4K to be sure of 4K alignment :-( */
  MmuGetPhysicalAddress(RegGetCr3(), Pml4, &Phys_Pml4);
  vmm_memset((void *)Pml4, 0, 4096);

  /* We need only HOST_GB entries of EPT PDPT */
  pdpt = (hvm_address)GUEST_MALLOC(4096);	/* Alloc 4K to be sure of 4K alignment :-( */
  MmuGetPhysicalAddress(RegGetCr3(), pdpt, &phys_pdpt);
  vmm_memset((void *)pdpt, 0, 4096);

  /* Fill PML4E with PDPT base address and RWX permissions */
  *(hvm_address *)Pml4 = (GET32L(phys_pdpt) & 0xfffff000) | 0x7;

  map = 0;

  for(i = 0; i < HOST_GB; i++) {

    /* Allocate memory for PD */
    pd = (hvm_address)GUEST_MALLOC(4096);
    if(!pd) {
      return HVM_STATUS_UNSUCCESSFUL;
    }
    MmuGetPhysicalAddress(RegGetCr3(), pd, &phys_pd);
    vmm_memset((void *)pd, 0, 4096);

    /* Fill i-th PDPTE with i-th PD baseaddr and RWX permissions */
    *(hvm_address *)(pdpt+i*8) = (GET32L(phys_pd) & 0xfffff000) | 0x7;

    for(j = 0; j < 4096; j=j+8) {

      /* Allocate memory for PT */
      pt = (hvm_address)GUEST_MALLOC(4096);
      if(!pt) {
				return HVM_STATUS_UNSUCCESSFUL;
      }
      /* Store Virtual PT base in ad-hoc array */
      VIRT_PT_BASES[(i*512) + (j/8)] = pt;
      /* Get Phys Addr */
      MmuGetPhysicalAddress(RegGetCr3(), pt, &phys_pt);
      vmm_memset((void *)pt, 0, 4096);

      /* Fill j-th PDE with PT baseaddr and RWX permissions */
      *(hvm_address *)(pd+j) = (GET32L(phys_pt) & 0xfffff000) | 0x7;

      /* 1:1 physical memory mapping */
      for(h = 0; h < 4096; h=h+8) {
      	/* Log("Fill PDPT[%d] PD[%d] PT[%d] with 0x%08hx", i, j/8, h/8, ((map << 12) | 0x37)); */
				*(hvm_address *)(pt+h) = (map << 12) | ((Bit8u) EPTGetMemoryType((map << 12)) << 3) | READ | WRITE | EXEC;
      	map++;
      }
    }
  }

  temp32 = VmxVmcsRead(CPU_BASED_VM_EXEC_CONTROL);
  CmSetBit32(&temp32, CPU_BASED_PRIMARY_ACTIVATE_SEC); /* Activate secondary controls */
  VmxVmcsWrite(CPU_BASED_VM_EXEC_CONTROL, temp32);

  /* Write EPTP (Memory Type WB, Page Walk 4 ---> 3 = 0x1e) */
  temp64 = 0;
  temp64 = (Phys_Pml4 & 0xfffff000) | 0x1e;
  VmxVmcsWrite(EPTP_ADDR, temp64);

  temp32 = 0;
  CmSetBit32(&temp32, 1); /* Enable EPT */
  VmxVmcsWrite(SECONDARY_VM_EXEC_CONTROL, temp32);

  vmm_memset(&EPTInveptDesc, 0, sizeof(EPTInveptDesc));
  EPTInveptDesc.Eptp = VmxVmcsRead(EPTP_ADDR);

  Log("SUCCESS: EPT enabled.");

#endif
  
  return HVM_STATUS_SUCCESS;
}

static hvm_status VmxInitialize(void (*idt_initializer)(PIDT_ENTRY pidt))
{
  hvm_status r;
  hvm_address cr3;
#ifdef GUEST_LINUX
  mempool_t* pool;
#endif

  cr3 = RegGetCr3();
  
  /* Allocate the VMXON region memory */
  vmxInitState.pVMXONRegion = (Bit32u*) GUEST_MALLOC(4096);
  
  if(vmxInitState.pVMXONRegion == NULL) {
    GuestLog("ERROR: Allocating VMXON region memory");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  vmm_memset(vmxInitState.pVMXONRegion, 0, 4096);
  
  r = MmuGetPhysicalAddress(cr3, (hvm_address) vmxInitState.pVMXONRegion, &vmxInitState.PhysicalVMXONRegionPtr);
  if (r != HVM_STATUS_SUCCESS) {
    GuestLog("ERROR: Can't determine physical address for VMXON region");
    return HVM_STATUS_UNSUCCESSFUL;
  }

  /* Allocate the VMCS region memory */
  vmxInitState.pVMCSRegion = (Bit32u*) GUEST_MALLOC(4096);    
    
  if(vmxInitState.pVMCSRegion == NULL) {
    GuestLog("ERROR: Allocating VMCS region memory");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  vmm_memset(vmxInitState.pVMCSRegion, 0, 4096);
  
  r = MmuGetPhysicalAddress(cr3, (hvm_address) vmxInitState.pVMCSRegion, &vmxInitState.PhysicalVMCSRegionPtr);
  if (r != HVM_STATUS_SUCCESS) {
    GuestLog("ERROR: Can't determine physical address for VMCS region");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  
  /* Allocate stack for the VM exit handler */
#ifdef GUEST_WINDOWS
  vmxInitState.VMMStack = ExAllocatePoolWithTag(NonPagedPool, VMM_STACK_SIZE, 'gbdh');
#elif defined GUEST_LINUX
  pool = mempool_create_kmalloc_pool(1,VMM_STACK_SIZE);
  vmxInitState.VMMStack = mempool_alloc(pool,GFP_KERNEL);
#endif
  
  if(vmxInitState.VMMStack == NULL) {
    GuestLog("ERROR: Allocating VM exit handler stack memory");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  vmm_memset(vmxInitState.VMMStack, 0, VMM_STACK_SIZE);

 
  /* Allocate a memory page for the I/O bitmap A */
  vmxInitState.pIOBitmapA = GUEST_MALLOC(4096);
  
  if(vmxInitState.pIOBitmapA == NULL) {
    GuestLog("ERROR: Allocating I/O bitmap A memory");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  vmm_memset(vmxInitState.pIOBitmapA, 0, 4096);
  
  r = MmuGetPhysicalAddress(cr3, (hvm_address) vmxInitState.pIOBitmapA, &vmxInitState.PhysicalIOBitmapA);
  
  if (r != HVM_STATUS_SUCCESS)
    {
      GuestLog("ERROR: Can't determine physical address for I/O bitmap A");
      return HVM_STATUS_UNSUCCESSFUL;
    }

  /* Allocate a memory page for the I/O bitmap B */
  vmxInitState.pIOBitmapB = GUEST_MALLOC(4096);

  if(vmxInitState.pIOBitmapB == NULL) {
    GuestLog("ERROR: Allocating I/O bitmap A memory");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  vmm_memset(vmxInitState.pIOBitmapB, 0, 4096);

  r = MmuGetPhysicalAddress(cr3, (hvm_address) vmxInitState.pIOBitmapB , &vmxInitState.PhysicalIOBitmapB);
  if (r != HVM_STATUS_SUCCESS)
    {
      GuestLog("ERROR: Can't determine physical address for I/O bitmap B");
      return HVM_STATUS_UNSUCCESSFUL;
    }

  /* Allocate & initialize the IDT for the VMM */
  vmxInitState.VMMIDT = GUEST_MALLOC(sizeof(IDT_ENTRY)*256);
 
  if (vmxInitState.VMMIDT == NULL) {
    GuestLog("ERROR: Allocating VMM interrupt descriptor table");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  idt_initializer(vmxInitState.VMMIDT);

  return HVM_STATUS_SUCCESS;
}

static hvm_status VmxFinalize(void)
{
  /* Deallocate memory regions allocated during the initialization phase */
  if (vmxInitState.pVMXONRegion)
    GUEST_FREE(vmxInitState.pVMXONRegion, 4096);
  if (vmxInitState.pVMCSRegion)
    GUEST_FREE(vmxInitState.pVMCSRegion, 4096);

  if (vmxInitState.VMMStack) {
#ifdef GUEST_LINUX
    mempool_destroy(vmxInitState.VMMStack);
#elif defined GUEST_WINDOWS
    ExFreePoolWithTag(vmxInitState.VMMStack, 'gbdh');
#endif  
  }

  if (vmxInitState.pIOBitmapA)
    GUEST_FREE(vmxInitState.pIOBitmapA , 4096);
  if (vmxInitState.pIOBitmapB)
    GUEST_FREE(vmxInitState.pIOBitmapB , 4096);
  if (vmxInitState.VMMIDT)
    GUEST_FREE(vmxInitState.VMMIDT , sizeof(IDT_ENTRY)*256);

  return HVM_STATUS_SUCCESS;
}

static hvm_bool VmxHasCPUSupport(void)
{
  VMX_FEATURES vmxFeatures;

  __asm__ __volatile__ (
			"pushal\n"
			"movl $0x1,%%eax\n"
			"cpuid\n"
			// ecx contains the vmx_features flags (vmx supported if bit 5 equals 1)
			"movl %%ecx,%0\n"
			"popal\n"
			:"=m"(vmxFeatures)
			::"memory"
			);

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
    GuestLog("VMX support not present");
    return HVM_STATUS_UNSUCCESSFUL;
  }
	
  GuestLog("VMX support present");

  // (2) Determine the VMX capabilities supported by the processor through
  //     the VMX capability MSRs.
  ReadMSR(IA32_VMX_BASIC_MSR_CODE, (PMSR) &vmxBasicMsr);
  ReadMSR(IA32_FEATURE_CONTROL_CODE, (PMSR) &vmxFeatureControl);

  // (3) Create a VMXON region in non-pageable memory of a size specified by
  //	 IA32_VMX_BASIC_MSR and aligned to a 4-byte boundary. The VMXON region
  //	 must be hosted in cache-coherent memory.
  GuestLog("VMXON region size:      %.8x", vmxBasicMsr.szVmxOnRegion);
  GuestLog("VMXON access width bit: %.8x", vmxBasicMsr.PhyAddrWidth);
  GuestLog("      [   1] --> 32-bit");
  GuestLog("      [   0] --> 64-bit");
  GuestLog("VMXON memory type:      %.8x", vmxBasicMsr.MemType);
  GuestLog("      [   0]  --> Strong uncacheable");
  GuestLog("      [ 1-5]  --> Unused");
  GuestLog("      [   6]  --> Write back");
  GuestLog("      [7-15]  --> Unused");

  switch(vmxBasicMsr.MemType) {
  case VMX_MEMTYPE_UNCACHEABLE:
    GuestLog("Unsupported memory type %.8x", vmxBasicMsr.MemType);
    return HVM_STATUS_UNSUCCESSFUL;
    break;
  case VMX_MEMTYPE_WRITEBACK:
    break;
  default:
    GuestLog("ERROR: Unknown VMXON region memory type");
    return HVM_STATUS_UNSUCCESSFUL;
    break;
  }

  // (4) Initialize the version identifier in the VMXON region (first 32 bits)
  //	 with the VMCS revision identifier reported by capability MSRs.
  *(vmxInitState.pVMXONRegion) = vmxBasicMsr.RevId;
	
  GuestLog("vmxBasicMsr.RevId: %.8x", vmxBasicMsr.RevId);
  
  // (5) Ensure the current processor operating mode meets the required CR0
  //	 fixed bits (CR0.PE=1, CR0.PG=1). Other required CR0 fixed bits can
  //	 be detected through the IA32_VMX_CR0_FIXED0 and IA32_VMX_CR0_FIXED1
  //	 MSRs.
  CR0_TO_ULONG(cr0_reg) = RegGetCr0();

  if(cr0_reg.PE != 1) {
    GuestLog("ERROR: Protected mode not enabled");
    GuestLog("Value of CR0: %.8x", CR0_TO_ULONG(cr0_reg));
    return HVM_STATUS_UNSUCCESSFUL;
  }

  GuestLog("Protected mode enabled");

  if(cr0_reg.PG != 1) {
    GuestLog("ERROR: Paging not enabled");
    GuestLog("Value of CR0: %.8x", CR0_TO_ULONG(cr0_reg));
    return HVM_STATUS_UNSUCCESSFUL;
  }
	
  GuestLog("Paging enabled");

  // This was required by first processors that supported VMX	
  cr0_reg.NE = 1;
  
  RegSetCr0(CR0_TO_ULONG(cr0_reg));
  
  // (6) Enable VMX operation by setting CR4.VMXE=1 [bit 13]. Ensure the
  //	 resultant CR4 value supports all the CR4 fixed bits reported in
  //	 the IA32_VMX_CR4_FIXED0 and IA32_VMX_CR4_FIXED1 MSRs.
  CR4_TO_ULONG(cr4_reg) = RegGetCr4();

  GuestLog("Old CR4: %.8x", CR4_TO_ULONG(cr4_reg));
  cr4_reg.VMXE = 1;
  GuestLog("New CR4: %.8x", CR4_TO_ULONG(cr4_reg));

  RegSetCr4(CR4_TO_ULONG(cr4_reg));

  // (7) Ensure that the IA32_FEATURE_CONTROL_MSR (MSR index 0x3A) has been
  //	 properly programmed and that its lock bit is set (bit 0=1) and VMX is
  //	 enabled (bit 2=1). This MSR is generally configured by the BIOS using 
  //	 WRMSR. If it's not set, it's safe for us to set it.
  GuestLog("IA32_FEATURE_CONTROL Lock Bit: %.8x, EnableVmx bit %.8x", vmxFeatureControl.Lock, vmxFeatureControl.EnableVmxon);
	
  if(vmxFeatureControl.Lock != 1) {
    // MSR hasn't been locked, we can enable VMX and lock it
    GuestLog("Setting IA32_FEATURE_CONTROL Lock Bit and Vmxon Enable bit");
    vmxFeatureControl.EnableVmxon = 1;
    vmxFeatureControl.Lock = 1;
    WriteMSR(IA32_FEATURE_CONTROL_CODE, 0, ((PMSR)&vmxFeatureControl)->Lo);
  } else {
    if(vmxFeatureControl.EnableVmxon == 0) {
      // If the lock bit is set and VMX is disabled, it was most likely done
      // by the BIOS and we can't use VMXON
      GuestLog("ERROR: VMX is disabled by the BIOS");
      return HVM_STATUS_UNSUCCESSFUL;
    }
  }
  
  
  // (8) Execute VMXON with the physical address of the VMXON region as the
  //	 operand. Check successful execution of VMXON by checking if
  //	 RFLAGS.CF=0.

  FLAGS_TO_ULONG(rflags) = VmxTurnOn(GET32H(vmxInitState.PhysicalVMXONRegionPtr), GET32L(vmxInitState.PhysicalVMXONRegionPtr));

  if(rflags.CF == 1) {
    GuestLog("ERROR: VMXON operation failed");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  
  /* VMXON was successful, so we cannot use GuestLog() anymore */
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
	context.GuestContext.cr0 = cr0;
  VmxVmcsWrite(GUEST_CR0, cr0);	/* This is redundant but whatever */
}

static void VmxSetCr3(hvm_address cr3)
{
	context.GuestContext.cr3 = cr3;
  VmxVmcsWrite(GUEST_CR3, cr3); /* This is redundant but whatever */
}

static void VmxSetCr4(hvm_address cr4)
{
	context.GuestContext.cr4 = cr4;
  VmxVmcsWrite(GUEST_CR4, cr4); /* This is redundant but whatever */
}

static void VmxTrapIO(hvm_bool enabled)
{
  Bit32u v;

  v = VmxVmcsRead(CPU_BASED_VM_EXEC_CONTROL);

  if (enabled) {
    /* Enable I/O bitmaps */
    CmSetBit32(&v,   CPU_BASED_PRIMARY_IO);
  } else {
    /* Disable I/O bitmaps */
    CmClearBit32(&v, CPU_BASED_PRIMARY_IO);
  }

  VmxVmcsWrite(CPU_BASED_VM_EXEC_CONTROL, v);
}

static Bit32u VmxGetExitInstructionLength(void)
{
  return vmxcontext.ExitInstructionLength;
}

static void VmxInvalidateTLB(void)
{
  __asm__ __volatile__ ( 
			"movl %cr3,%eax\n"
			"movl %eax,%cr3\n"
			 );
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

#ifdef ENABLE_EPT
  vmxcontext.GuestLinearAddress           = VmxRead(GUEST_LINEAR_ADDRESS);
  vmxcontext.GuestPhysicalAddress         = VmxRead(GUEST_PHYSICAL_ADDRESS);
#endif

  /* Read guest state */
  context.GuestContext.rip    = VmxRead(GUEST_RIP);
  context.GuestContext.rsp    = VmxRead(GUEST_RSP);
  context.GuestContext.cs     = VmxRead(GUEST_CS_SELECTOR);
  context.GuestContext.cr0    = VmxRead(GUEST_CR0);
  context.GuestContext.cr3    = VmxRead(GUEST_CR3);
  context.GuestContext.cr4    = VmxRead(GUEST_CR4);
  context.GuestContext.rflags = VmxRead(GUEST_RFLAGS);

  /* Writing the Guest VMCS RIP uses general registers. Must complete this
     before setting general registers for guest return state */
  context.GuestContext.resumerip = context.GuestContext.rip + vmxcontext.ExitInstructionLength;
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
  Log("Flow returning to address %.8x", context.GuestContext.resumerip);

  /* TODO: We should restore the whole original guest state here -- se Joanna's
     source code */
  RegSetIdtr((void*) VmxRead(GUEST_IDTR_BASE), VmxRead(GUEST_IDTR_LIMIT));
  
  __asm__ __volatile__ (
			"pushl  %%eax\n"
  			/* Restore guest CR0 */
  			"movl	%0,%%eax\n"
  			"movl	%%eax,%%cr0\n"

  			/* Restore guest CR3 */
  			"movl	%1,%%eax\n"
  			"movl	%%eax,%%cr3\n"

  			/* Restore guest CR4 */
  			"movl	%2,%%eax\n"
			
  			/* movl %eax, %cr4; */
  			".byte 0x0f\n"
  			".byte 0x22\n"
  			".byte 0xe0\n"

		        "popl  %%eax\n"
  			::"m"(context.GuestContext.cr0),"m"(context.GuestContext.cr3),"m"(context.GuestContext.cr4)
  			);
  /* Turn off VMX */
  VmxTurnOff();
  vmxIsActive = FALSE;

  __asm__ __volatile__(
		       /* Restore general-purpose registers */
		       "movl	%0,%%eax\n"
		       "movl	%1,%%ebx\n"
		       "movl	%2,%%ecx\n"
		       "movl	%3,%%edx\n"
		       "movl	%4,%%edi\n"
		       "movl	%5,%%esi\n"
		       "movl	%6,%%ebp\n"

		       /* Restore ESP */
		       "movl	%7,%%esp\n"

		       /* Restore guest RFLAGS */
		       "pushl	%%eax\n"
		       "movl	%8,%%eax\n"
		       "pushl	%%eax\n"
		       "popfl\n"
		       "popl	%%eax\n"

		       /* Resume guest execution */
		       "jmp	*%9\n"
		       ::"m"(context.GuestContext.rax), "m"(context.GuestContext.rbx), "m"(context.GuestContext.rcx),    \
			 "m"(context.GuestContext.rdx), "m"(context.GuestContext.rdi), "m"(context.GuestContext.rsi),    \
			 "m"(context.GuestContext.rbp), "m"(context.GuestContext.rsp), "m"(context.GuestContext.rflags), \
			 "m"(context.GuestContext.resumerip)
		       );

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
  if (movcrOperandType == 0 && accesstype != VT_CR_ACCESS_CLTS && accesstype != VT_CR_ACCESS_LMSW) {
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

void VmxHvmInternalHandleExit(void)
{
  Bit32u interruptibility, activitystate, pending_debug, vectoring_error_code, vectoring_information;
  VmxReadGuestContext();

  /* Restore host IDT -- Not sure if this is really needed. I'm pretty sure we */
  /* have to fix the LIMIT fields of host's IDTR. */

  RegSetIdtr((void*) VmxRead(HOST_IDTR_BASE), 0x7ff);
  /* Enable logging only for particular VM-exit events */
  if(vmxcontext.ExitReason == EXIT_REASON_VMCALL || vmxcontext.ExitReason == EXIT_REASON_EPT_MISCONFIGURATION) {
    HandlerLogging = TRUE;
  } else {
    HandlerLogging = FALSE;
  }

  if (HandlerLogging) {
    Log("----- VMM Handler CPU0 -----");
    Log("Guest RAX: %.8x", context.GuestContext.rax);
    Log("Guest RBX: %.8x", context.GuestContext.rbx);
    Log("Guest RCX: %.8x", context.GuestContext.rcx);
    Log("Guest RDX: %.8x", context.GuestContext.rdx);
    Log("Guest RDI: %.8x", context.GuestContext.rdi);
    Log("Guest RSI: %.8x", context.GuestContext.rsi);
    Log("Guest RBP: %.8x", context.GuestContext.rbp);
    Log("Exit Reason:        %d", vmxcontext.ExitReason);
    Log("Exit Qualification: %.8x", vmxcontext.ExitQualification);
    Log("Exit Interruption Information:   %.8x", vmxcontext.ExitInterruptionInformation);
    Log("Exit Interruption Error Code:    %.8x", vmxcontext.ExitInterruptionErrorCode);
    Log("IDT-Vectoring Information Field: %.8x", vmxcontext.IDTVectoringInformationField);
    Log("IDT-Vectoring Error Code:        %.8x", vmxcontext.IDTVectoringErrorCode);
    Log("VM-Exit Instruction Length:      %.8x", vmxcontext.ExitInstructionLength);
    Log("VM-Exit Instruction Information: %.8x", vmxcontext.ExitInstructionInformation);
    Log("VM Exit RIP: %.8x", context.GuestContext.rip);
    Log("VM Exit RSP: %.8x", context.GuestContext.rsp);
    Log("VM Exit CS:  %.4x", context.GuestContext.cs);
    Log("VM Exit CR0: %.8x", context.GuestContext.cr0);
    Log("VM Exit CR3: %.8x", context.GuestContext.cr3);
    Log("VM Exit CR4: %.8x", context.GuestContext.cr4);
    Log("VM Exit RFLAGS: %.8x", context.GuestContext.rflags);
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

    goto Resume;

    /* Unreachable */
    break;

  case EXIT_REASON_VMLAUNCH:
    HandleVMLAUNCH();
    
    goto Resume;

    /* Unreachable */
    break;

    //////////////
    //  VMCALL  //
    //////////////
  case EXIT_REASON_VMCALL:
    HandleVMCALL();

    goto Resume;

    /* Unreachable */
    break;

    ////////////
    //  INVD  //
    ////////////
  case EXIT_REASON_INVD:
    Log("INVD detected");

    __asm__ __volatile__ (
			  "invd\n"
			  );

    goto Resume;

    /* Unreachable */
    break;

    /////////////
    //  RDMSR  //
    /////////////
  case EXIT_REASON_MSR_READ:
		/* This gets triggered only for MSRs configured in the MSR bitmap. By default, it just re-execute the faulty instruction. */
    Log("Read MSR #%.8x", context.GuestContext.rcx);
    __asm__ __volatile__ (
			  "movl	%0,%%ecx\n"
			  "rdmsr\n"
			  ::"m"(context.GuestContext.rcx)
			  );
    goto Resume;
    /* Unreachable */
    break;

    /////////////
    //  WRMSR  //
    /////////////
  case EXIT_REASON_MSR_WRITE:
    Log("Write MSR #%.8x", context.GuestContext.rcx);

    WriteMSR(context.GuestContext.rcx, context.GuestContext.rdx, context.GuestContext.rax);
    goto Resume;

    /* Unreachable */
    break;

    /////////////
    //  CPUID  //
    /////////////
  case EXIT_REASON_CPUID:
    if(HandlerLogging) {
      Log("CPUID detected (RAX: %.8x)", context.GuestContext.rax);
    }
    
		/* Change here if you want to mask some CPU flag */
		__asm__ __volatile__(
												 "movl	%4,%%eax\n"
												 "movl  %5,%%ecx\n"
												 "cpuid\n"
												 "movl  %%eax, %0\n"
												 "movl  %%ebx, %1\n"
												 "movl  %%ecx, %2\n"
												 "movl  %%edx, %3\n"
												 :"=m"(context.GuestContext.rax),"=m"(context.GuestContext.rbx), "=m"(context.GuestContext.rcx), "=m"(context.GuestContext.rdx)
												 :"m"(context.GuestContext.rax),"m"(context.GuestContext.rcx)
												 :"eax","ebx","ecx","edx"
												 );

    goto Resume;

    /* Unreachable */
    break;

    ///////////////////////////////
    //  Control Register Access  //
    ///////////////////////////////
  case EXIT_REASON_CR_ACCESS:

    VmxInternalHandleCR();

    goto Resume;

    /* Unreachable */
    break;

    ///////////////////////
    //  I/O instruction  //
    ///////////////////////
  case EXIT_REASON_IO_INSTRUCTION:
    VmxInternalHandleIO();
    
    goto Resume;

    /* Unreachable */
    break;

  case EXIT_REASON_EXCEPTION_NMI:
    VmxInternalHandleNMI();

    goto Resume;
    
    /* Unreachable */
    break;

#ifdef ENABLE_EPT
  case EXIT_REASON_EPT_VIOLATION:
    vectoring_information = VmxRead(IDT_VECTORING_INFO_FIELD);
    vectoring_error_code  = VmxRead(IDT_VECTORING_ERROR_CODE);

    if((vectoring_information & (1 << 31)) == 0) {
      /* If the VM exit stored 0 for bit 31 for IDT-vectoring information field
				 (because the VM exit did not occur during delivery of an event through
				 the IDT; see Section 27.2.3), the value saved is 1. */

      context.GuestContext.rflags &= ~FLAGS_RF_MASK;
    }
    
    HandleEPTViolation( vmxcontext.GuestLinearAddress,
			GET32L(vmxcontext.GuestPhysicalAddress),
			(vmxcontext.ExitQualification & 0x80) != 0,     /* GuestLinear is valid? */
			 vmxcontext.ExitQualification & 0x7,            /* Attempt type */
			(vmxcontext.ExitQualification & 0x100) == 0,    /* Violation in guest page walk? */
			(vmxcontext.ExitQualification & 0x38) == 0      /* Fill an entry? */
			);

    goto Resume;

    /* Unreachable */
    break;
#endif
  case EXIT_REASON_HLT:
    HandleHLT();

    goto Resume;
    
    /* Unreachable */
    break;
    
  default:
    /* Unknown exit condition */
    break;
  }

  // This instruction is reached when we are not able to handle the reason that
  // triggered this VM exit. In this case, we simply switch off VMX mode.
  HypercallSwitchOff(NULL);
  
 Resume:
  /* We need to check if TF is set*/
  if((context.GuestContext.rflags & FLAGS_TF_MASK) != 0) {
    /* Here we must check if interruptibility-state field indicates a blocking cause of STI, MOV SS, IRET or HLT */
    interruptibility = VmxVmcsRead(GUEST_INTERRUPTIBILITY_INFO);
    activitystate = VmxVmcsRead(GUEST_ACTIVITY_STATE);

    /*       STI (bit 0)                       MOV SS (bit1)                    IRET(bit3)                      HLT (x)      */
    if((interruptibility & 0x1) != 0 || (interruptibility & 0x2) != 0 || (interruptibility & 0x4) != 0 || (activitystate == 1)) {
      /* According to: http://www.mail-archive.com/kvm@vger.kernel.org/msg08324.html we must clear both bits */
      interruptibility &= ~0x1;
      interruptibility &= ~0x2;
      VmxVmcsWrite(GUEST_INTERRUPTIBILITY_INFO, interruptibility);
    }
  }
  else {
    /* We must set GUEST_PENDING_DBG_EXCEPTIONS.BS = 0 because RFLAGS.TF == 0*/
    pending_debug = VmxVmcsRead(GUEST_PENDING_DBG_EXCEPTIONS);
    pending_debug &= ~0x4000; /* bit 14 */
    VmxVmcsWrite(GUEST_PENDING_DBG_EXCEPTIONS, pending_debug);
  }

  /* Mirror context.GuestContext fields into VMCS Guest Fields as update guest context doesn't do it. */
  /* NB: we can do it here because there will be no more updates to context.GuestContext */
  VmxVmcsWrite(GUEST_RIP, context.GuestContext.resumerip);
  VmxVmcsWrite(GUEST_RSP, context.GuestContext.rsp);
  VmxVmcsWrite(GUEST_CS_SELECTOR, context.GuestContext.cs);
  VmxVmcsWrite(GUEST_CR0, context.GuestContext.cr0);
  VmxVmcsWrite(GUEST_CR3, context.GuestContext.cr3);
  VmxVmcsWrite(GUEST_CR4, context.GuestContext.cr4);
  VmxVmcsWrite(GUEST_RFLAGS, context.GuestContext.rflags);

  return;
  // Exit reason handled. Need to execute the VMRESUME without having
  // changed the state of the GPR and ESP et cetera.
}
