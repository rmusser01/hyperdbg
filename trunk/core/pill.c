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

/*
  Inspired to Shawn Embleton's Virtual Machine Monitor
*/

#include <ntddk.h>

#include "config.h"
#include "debug.h"
#include "pill.h"
#include "vmx.h"
#include "vmm.h"
#include "events.h"
#include "x86.h"
#include "idt.h"
#include "comio.h"
#include "common.h"

#ifdef GUEST_WINDOWS
#include "winxp.h"
#endif

#ifdef ENABLE_HYPERDBG
#include "hyperdbg_guest.h"
#include "hyperdbg_host.h"
#endif

/* Suppress warnings */
#pragma warning ( disable : 4740 ) /* Control flows out inline asm */

/* ################ */
/* #### MACROS #### */
/* ################ */

#define HYPERCALL_SWITCHOFF 0xcafebabe
#define VMM_STACK_SIZE      0x8000

/* #################### */
/* #### PROTOTYPES #### */
/* #################### */

NTSTATUS	DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
VOID		DriverUnload(IN PDRIVER_OBJECT DriverObject);

static VOID     StartVMX();
static VOID     ReadMSR(ULONG msrEncoding);
static int      EnableVMX(VOID);
static VOID     InitVMMIDT(PIDT_ENTRY pidt);
static NTSTATUS RegisterEvents(VOID);

/* Plugin & guest initialization/finalization */
static NTSTATUS InitGuest(PDRIVER_OBJECT DriverObject);
static NTSTATUS FiniGuest(VOID);
static NTSTATUS InitPlugin(VOID);
static NTSTATUS FiniPlugin(VOID);

/* ################# */
/* #### GLOBALS #### */
/* ################# */

/* These variables are not global, as they are also accessed by vmm.c */
ULONG			        HandlerLogging		= 0;
ULONG			        VMXIsActive		= 0;

/* Static variables, that are not directly accessed by other modules or plugins. */
static VMM_INIT_STATE           VMMInitState;

static ULONG			ErrorCode		= 0;

static EFLAGS			eFlags			= {0};
static MSR			msr			= {0};

static ULONG			ScrubTheLaunch		= 0;

static VMX_FEATURES		vmxFeatures;
static IA32_VMX_BASIC_MSR	vmxBasicMsr;
static IA32_FEATURE_CONTROL_MSR	vmxFeatureControl;

static CR0_REG			cr0_reg = {0};
static CR4_REG			cr4_reg = {0};

static ULONG			temp32 = 0;

static GDTR			gdt_reg = {0};
static IDTR			idt_reg = {0};

static ULONG			gdt_base = 0;
static ULONG			idt_base = 0;

static USHORT			seg_selector = 0;

static MISC_DATA		misc_data = {0};

static PVOID			GuestReturn = NULL;
static ULONG			GuestStack = 0;

/* ################ */
/* #### BODIES #### */
/* ################ */

static NTSTATUS RegisterEvents(VOID)
{
  EVENT_CONDITION_HYPERCALL hypercall;
  ULONG trap;

  trap = 0;
  /* Initialize the event handler */
  EventInit();

  /* Register a hypercall to switch off the VM */
  hypercall.hypernum = HYPERCALL_SWITCHOFF;

  if(!EventSubscribe(EventHypercall, &hypercall, sizeof(hypercall), HypercallSwitchOff)) {
    WindowsLog("ERROR : Unable to register switch-off hypercall handler.", 0);
    return STATUS_UNSUCCESSFUL;
  }

  return STATUS_SUCCESS;
}

/*
  Loads the contents of a 64-bit model specific register (MSR) specified
  in the ECX register into registers EDX:EAX. The EDX register is loaded
  with the high-order 32 bits of the MSR and the EAX register is loaded
  with the low-order 32 bits.
	msr.Hi --> EDX
	msr.Lo --> EAX
*/
static VOID ReadMSR(ULONG msrEncoding)
{
  __asm {
    PUSHAD;
			
    MOV		ECX, msrEncoding;

    RDMSR;

    MOV		msr.Hi, EDX;
    MOV		msr.Lo, EAX;

    POPAD;
  };
}

/* Initialize the IDT of the VMM */
static VOID InitVMMIDT(PIDT_ENTRY pidt)
{
  int i;
  IDT_ENTRY idte_null;

  idte_null.Selector   = RegGetCs();

  /* Present, DPL 0, Type 0xe (INT gate) */
  idte_null.Access     = (1 << 15) | (0xe << 8);

  idte_null.LowOffset  = (ULONG) NullIDTHandler & 0xffff;
  idte_null.HighOffset = (ULONG) NullIDTHandler >> 16;

  for (i=0; i<256; i++) {
    pidt[i] = idte_null;
  }
}

/* Enable VMX */
static int EnableVMX(VOID)
{
  // (1) Check VMX support in processor using CPUID.
  __asm {
    PUSHAD;
    MOV		EAX, 1;
    CPUID;
    // ECX contains the VMX_FEATURES FLAGS (VMX supported if bit 5 equals 1)
    MOV		vmxFeatures, ECX;
    MOV		EAX, 0x80000008;
    CPUID;
    MOV		temp32, EAX;
    POPAD;
  };

  if(vmxFeatures.VMX == 0) {
    WindowsLog("VMX Support Not Present.", vmxFeatures);
    return FALSE;
  }
	
  WindowsLog("VMX Support Present.", vmxFeatures);
	
  // (2)	Determine the VMX capabilities supported by the processor through
  //    	the VMX capability MSRs.
  __asm {
    PUSHAD;

    MOV		ECX, IA32_VMX_BASIC_MSR_CODE;
    RDMSR;
    LEA		EBX, vmxBasicMsr;
    MOV		[EBX+4], EDX;
    MOV		[EBX], EAX;

    MOV		ECX, IA32_FEATURE_CONTROL_CODE;
    RDMSR;
    LEA		EBX, vmxFeatureControl;
    MOV		[EBX+4], EDX;
    MOV		[EBX], EAX;

    POPAD;
  };

  // (3)	Create a VMXON region in non-pageable memory of a size specified by
  //		IA32_VMX_BASIC_MSR and aligned to a 4-byte boundary. The VMXON region
  //		must be hosted in cache-coherent memory.
  WindowsLog("VMXON Region Size", vmxBasicMsr.szVmxOnRegion ) ;
  WindowsLog("VMXON Access Width Bit", vmxBasicMsr.PhyAddrWidth);
  WindowsLog("      [   1] --> 32-bit", 0);
  WindowsLog("      [   0] --> 64-bit", 0);
  WindowsLog("VMXON Memory Type", vmxBasicMsr.MemType);
  WindowsLog("      [   0]  --> Strong Uncacheable", 0);
  WindowsLog("      [ 1-5]  --> Unused", 0);
  WindowsLog("      [   6]  --> Write Back", 0);
  WindowsLog("      [7-15]  --> Unused", 0);

  switch(vmxBasicMsr.MemType) {
  case VMX_MEMTYPE_UNCACHEABLE:
    WindowsLog("Unsupported memory type.", vmxBasicMsr.MemType);
    return FALSE;
    break;
  case VMX_MEMTYPE_WRITEBACK:
    break;
  default:
    WindowsLog("ERROR: Unknown VMXON Region memory type.", 0);
    return FALSE;
    break;
  }

  // (4)	Initialize the version identifier in the VMXON region (first 32 bits)
  //		with the VMCS revision identifier reported by capability MSRs.
  *(VMMInitState.pVMXONRegion) = vmxBasicMsr.RevId;
	
  WindowsLog("vmxBasicMsr.RevId", vmxBasicMsr.RevId);

  // (5)	Ensure the current processor operating mode meets the required CR0
  //		fixed bits (CR0.PE=1, CR0.PG=1). Other required CR0 fixed bits can
  //		be detected through the IA32_VMX_CR0_FIXED0 and IA32_VMX_CR0_FIXED1
  //		MSRs.
  CR0_TO_ULONG(cr0_reg) = RegGetCr0();

  if(cr0_reg.PE != 1) {
    WindowsLog("ERROR: Protected Mode not enabled.", 0);
    WindowsLog("Value of CR0", CR0_TO_ULONG(cr0_reg));
    return FALSE;
  }

  WindowsLog("Protected Mode enabled.", 0);

  if(cr0_reg.PG != 1) {
    WindowsLog("ERROR: Paging not enabled.", 0);
    WindowsLog("Value of CR0", CR0_TO_ULONG(cr0_reg));
    return FALSE;
  }
	
  WindowsLog("Paging enabled.", 0);

  // This was required by first processors that supported VMX	
  cr0_reg.NE = 1;

  RegSetCr0(CR0_TO_ULONG(cr0_reg));

  // (6)	Enable VMX operation by setting CR4.VMXE=1 [bit 13]. Ensure the
  //		resultant CR4 value supports all the CR4 fixed bits reported in
  //		the IA32_VMX_CR4_FIXED0 and IA32_VMX_CR4_FIXED1 MSRs.
  CR4_TO_ULONG(cr4_reg) = RegGetCr4();

  WindowsLog("CR4", CR4_TO_ULONG(cr4_reg));
  cr4_reg.VMXE = 1;
  WindowsLog("CR4", CR4_TO_ULONG(cr4_reg));

  RegSetCr4(CR4_TO_ULONG(cr4_reg));
	
  // (7)	Ensure that the IA32_FEATURE_CONTROL_MSR (MSR index 0x3A) has been
  //		properly programmed and that its lock bit is set (bit 0=1). This MSR
  //		is generally configured by the BIOS using WRMSR.

#if 1
  /* HACK: we do this because BOCHS does not support the IA32_FEATURE_CONTROL_CODE MSR */
  vmxFeatureControl.Lock = 1;
#endif

  WindowsLog("IA32_FEATURE_CONTROL Lock Bit", vmxFeatureControl.Lock);
  if(vmxFeatureControl.Lock != 1) {
    WindowsLog("ERROR: Feature Control Lock Bit != 1.", 0);
    return FALSE;
  }

  // (8)	Execute VMXON with the physical address of the VMXON region as the
  //		operand. Check successful execution of VMXON by checking if
  //		RFLAGS.CF=0.
  FLAGS_TO_ULONG(eFlags) = VmxTurnOn(0, VMMInitState.PhysicalVMXONRegionPtr.LowPart);

  if(eFlags.CF == 1) {
    WindowsLog("ERROR: VMXON operation failed.", 0);
    return FALSE;
  }

  /* VMXON was successful, so we cannot use WindowsLog() anymore */
  VMXIsActive = 1;

  Log("SUCCESS: VMXON operation completed.", 0);
  Log("VMM is now running.", 0);
	
  return TRUE;
}

__declspec(naked) VOID StartVMX()
{	
  //	Get the Guest Return EIP.
  //
  //	Hi	|	    |
  //		+-----------+
  //		|    EIP    |
  //		+-----------+ <--	ESP after the CALL
  //	Lo	|	    |

  __asm	{ POP GuestReturn };

  WindowsLog("Guest Return EIP", GuestReturn);

  ///////////////////////////
  //  Set thread affinity  //
  ///////////////////////////
  WindowsLog("Enabling VMX mode on CPU 0", 0);
  KeSetSystemAffinityThread((KAFFINITY) 0x00000001);
  WindowsLog("Running on Processor", KeGetCurrentProcessorNumber());

  ////////////////
  //  GDT Info  //
  ////////////////
  __asm { SGDT gdt_reg };
  gdt_base = (gdt_reg.BaseHi << 16) | gdt_reg.BaseLo;
  WindowsLog("GDT Base", gdt_base);
  WindowsLog("GDT Limit", gdt_reg.Limit);
	
  ////////////////////////////
  //  IDT Segment Selector  //
  ////////////////////////////
  __asm	{ SIDT idt_reg };
  idt_base = (idt_reg.BaseHi << 16) | idt_reg.BaseLo;	
  WindowsLog("IDT Base", idt_base);
  WindowsLog("IDT Limit", idt_reg.Limit);

  if (!EnableVMX()) {
    goto Abort;
  }

  /* ***************************************************** */
  /* **** The processor is now in VMX root operation! **** */
  /* ****      No more APIs after this point!         **** */
  /* ***************************************************** */

  //  27.6 PREPARATION AND LAUNCHING A VIRTUAL MACHINE
  // (1)	Create a VMCS region in non-pageable memory of size specified by
  //		the VMX capability MSR IA32_VMX_BASIC and aligned to 4-KBytes.
  //		Software should read the capability MSRs to determine width of the 
  //		physical addresses that may be used for a VMCS region and ensure
  //		the entire VMCS region can be addressed by addresses with that width.
  //		The term "guest-VMCS address" refers to the physical address of the
  //		new VMCS region for the following steps.
  switch(vmxBasicMsr.MemType) {
  case VMX_MEMTYPE_UNCACHEABLE:
    Log("Unsupported memory type.", vmxBasicMsr.MemType);
    goto Abort;
    break;
  case VMX_MEMTYPE_WRITEBACK:
    break;
  default:
    Log("ERROR: Unknown VMCS Region memory type.", 0);
    goto Abort;
    break;
  }
	
  // (2)	Initialize the version identifier in the VMCS (first 32 bits)
  //		with the VMCS revision identifier reported by the VMX
  //		capability MSR IA32_VMX_BASIC.
  *(VMMInitState.pVMCSRegion) = vmxBasicMsr.RevId;

  // (3)	Execute the VMCLEAR instruction by supplying the guest-VMCS address.
  //		This will initialize the new VMCS region in memory and set the launch
  //		state of the VMCS to "clear". This action also invalidates the
  //		working-VMCS pointer register to FFFFFFFF_FFFFFFFFH. Software should
  //		verify successful execution of VMCLEAR by checking if RFLAGS.CF = 0
  //		and RFLAGS.ZF = 0.
  FLAGS_TO_ULONG(eFlags) = VmxClear(0, VMMInitState.PhysicalVMCSRegionPtr.LowPart);

  if(eFlags.CF != 0 || eFlags.ZF != 0) {
    Log("ERROR: VMCLEAR operation failed.", 0);
    goto Abort;
  }
	
  Log("SUCCESS : VMCLEAR operation completed.", 0);
	
  // (4)	Execute the VMPTRLD instruction by supplying the guest-VMCS address.
  //		This initializes the working-VMCS pointer with the new VMCS region's
  //		physical address.
  VmxPtrld(0, VMMInitState.PhysicalVMCSRegionPtr.LowPart);

  //
  //  ***********************************
  //  *	H.1.1 16-Bit Guest-State Fields *
  //  ***********************************

  VmxWrite(GUEST_CS_SELECTOR, RegGetCs() & 0xfff8);
  VmxWrite(GUEST_SS_SELECTOR, RegGetSs() & 0xfff8);
  VmxWrite(GUEST_DS_SELECTOR, RegGetDs() & 0xfff8);
  VmxWrite(GUEST_ES_SELECTOR, RegGetEs() & 0xfff8);
  VmxWrite(GUEST_FS_SELECTOR, RegGetFs() & 0xfff8);
  VmxWrite(GUEST_GS_SELECTOR, RegGetGs() & 0xfff8);
  VmxWrite(GUEST_LDTR_SELECTOR, RegGetLdtr() & 0xfff8);

  /* Guest TR selector */
  __asm	{ STR seg_selector };
  CmClearBit16(&seg_selector, 2); // TI Flag
  VmxWrite(GUEST_TR_SELECTOR, seg_selector & 0xfff8);

  //  **********************************
  //  *	H.1.2 16-Bit Host-State Fields *
  //  **********************************

  /* FIXME: Host DS, ES & FS mascherati con 0xFFFC? */
  VmxWrite(HOST_CS_SELECTOR, RegGetCs() & 0xfff8);
  VmxWrite(HOST_SS_SELECTOR, RegGetSs() & 0xfff8);
  VmxWrite(HOST_DS_SELECTOR, RegGetDs() & 0xfff8);
  VmxWrite(HOST_ES_SELECTOR, RegGetEs() & 0xfff8);
  VmxWrite(HOST_FS_SELECTOR, RegGetFs() & 0xfff8);
  VmxWrite(HOST_GS_SELECTOR, RegGetGs() & 0xfff8);
  VmxWrite(HOST_TR_SELECTOR, RegGetTr() & 0xfff8);

  //  ***********************************
  //  *	H.2.2 64-Bit Guest-State Fields *
  //  ***********************************

  VmxWrite(VMCS_LINK_POINTER, 0xFFFFFFFF);
  VmxWrite(VMCS_LINK_POINTER_HIGH, 0xFFFFFFFF);

  /* Reserved Bits of IA32_DEBUGCTL MSR must be 0 */
  ReadMSR(IA32_DEBUGCTL);
  VmxWrite(GUEST_IA32_DEBUGCTL, msr.Lo);
  VmxWrite(GUEST_IA32_DEBUGCTL_HIGH, msr.Hi);

  //	*******************************
  //	* H.3.1 32-Bit Control Fields *
  //	*******************************

  /* Pin-based VM-execution controls */
  temp32 = 0;
  VmxWrite(PIN_BASED_VM_EXEC_CONTROL, VmxAdjustControls(temp32, IA32_VMX_PINBASED_CTLS));

  /* Primary processor-based VM-execution controls */
  temp32 = 0;
  CmSetBit(&temp32, CPU_BASED_PRIMARY_IO); /* Use I/O bitmaps */
  CmSetBit(&temp32, 7); /* HLT */
  VmxWrite(CPU_BASED_VM_EXEC_CONTROL, VmxAdjustControls(temp32, IA32_VMX_PROCBASED_CTLS));

  /* I/O bitmap */
  VmxWrite(IO_BITMAP_A_HIGH, VMMInitState.PhysicalIOBitmapA.HighPart);  
  VmxWrite(IO_BITMAP_A,      VMMInitState.PhysicalIOBitmapA.LowPart); 
  VmxWrite(IO_BITMAP_B_HIGH, VMMInitState.PhysicalIOBitmapB.HighPart);  
  VmxWrite(IO_BITMAP_B,      VMMInitState.PhysicalIOBitmapB.LowPart); 
  EventUpdateIOBitmaps((PUCHAR) VMMInitState.pIOBitmapA, (PUCHAR) VMMInitState.pIOBitmapB);

  /* Exception bitmap */
  temp32 = 0;
  EventUpdateExceptionBitmap(&temp32);
  CmSetBit(&temp32, TRAP_DEBUG);   // DO NOT DISABLE: needed to step over I/O instructions!!!
  VmxWrite(EXCEPTION_BITMAP, temp32);

  /* Time-stamp counter offset */
  VmxWrite(TSC_OFFSET, 0);
  VmxWrite(TSC_OFFSET_HIGH, 0);

  VmxWrite(PAGE_FAULT_ERROR_CODE_MASK, 0);
  VmxWrite(PAGE_FAULT_ERROR_CODE_MATCH, 0);
  VmxWrite(CR3_TARGET_COUNT, 0);
  VmxWrite(CR3_TARGET_VALUE0, 0);
  VmxWrite(CR3_TARGET_VALUE1, 0);                        
  VmxWrite(CR3_TARGET_VALUE2, 0);
  VmxWrite(CR3_TARGET_VALUE3, 0);

  /* VM-exit controls */
  temp32 = 0;
  CmSetBit(&temp32, VM_EXIT_ACK_INTERRUPT_ON_EXIT);
  VmxWrite(VM_EXIT_CONTROLS, VmxAdjustControls(temp32, IA32_VMX_EXIT_CTLS));

  /* VM-entry controls */
  VmxWrite(VM_ENTRY_CONTROLS, VmxAdjustControls(0, IA32_VMX_ENTRY_CTLS));

  VmxWrite(VM_EXIT_MSR_STORE_COUNT, 0);
  VmxWrite(VM_EXIT_MSR_LOAD_COUNT, 0);

  VmxWrite(VM_ENTRY_MSR_LOAD_COUNT, 0);
  VmxWrite(VM_ENTRY_INTR_INFO_FIELD, 0);
	
  //  ***********************************
  //  *	H.3.3 32-Bit Guest-State Fields *
  //  ***********************************

  VmxWrite(GUEST_CS_LIMIT, GetSegmentDescriptorLimit(gdt_base, RegGetCs()));
  VmxWrite(GUEST_SS_LIMIT, GetSegmentDescriptorLimit(gdt_base, RegGetSs()));
  VmxWrite(GUEST_DS_LIMIT, GetSegmentDescriptorLimit(gdt_base, RegGetDs()));
  VmxWrite(GUEST_ES_LIMIT, GetSegmentDescriptorLimit(gdt_base, RegGetEs()));
  VmxWrite(GUEST_FS_LIMIT, GetSegmentDescriptorLimit(gdt_base, RegGetFs()));
  VmxWrite(GUEST_GS_LIMIT, GetSegmentDescriptorLimit(gdt_base, RegGetGs()));
  VmxWrite(GUEST_LDTR_LIMIT, GetSegmentDescriptorLimit(gdt_base, RegGetLdtr()));
  VmxWrite(GUEST_TR_LIMIT, GetSegmentDescriptorLimit(gdt_base, RegGetTr()));

  /* Guest GDTR/IDTR limit */
  VmxWrite(GUEST_GDTR_LIMIT, gdt_reg.Limit);
  VmxWrite(GUEST_IDTR_LIMIT, idt_reg.Limit);

  /* DR7 */
  VmxWrite(GUEST_DR7, 0x400);

  /* Guest interruptibility and activity state */
  VmxWrite(GUEST_INTERRUPTIBILITY_INFO, 0);
  VmxWrite(GUEST_ACTIVITY_STATE, 0);

  /* Set segment access rights */
  VmxWrite(GUEST_CS_AR_BYTES,   GetSegmentDescriptorAR(gdt_base, RegGetCs()));
  VmxWrite(GUEST_DS_AR_BYTES,   GetSegmentDescriptorAR(gdt_base, RegGetDs()));
  VmxWrite(GUEST_SS_AR_BYTES,   GetSegmentDescriptorAR(gdt_base, RegGetSs()));
  VmxWrite(GUEST_ES_AR_BYTES,   GetSegmentDescriptorAR(gdt_base, RegGetEs()));
  VmxWrite(GUEST_FS_AR_BYTES,   GetSegmentDescriptorAR(gdt_base, RegGetFs()));
  VmxWrite(GUEST_GS_AR_BYTES,   GetSegmentDescriptorAR(gdt_base, RegGetGs()));
  VmxWrite(GUEST_LDTR_AR_BYTES, GetSegmentDescriptorAR(gdt_base, RegGetLdtr()));
  VmxWrite(GUEST_TR_AR_BYTES,   GetSegmentDescriptorAR(gdt_base, RegGetTr()));

  /* Guest IA32_SYSENTER_CS */
  ReadMSR(IA32_SYSENTER_CS);
  VmxWrite(GUEST_SYSENTER_CS, msr.Lo);

  //  ******************************************
  //  * H.4.3 Natural-Width Guest-State Fields *
  //  ******************************************

  /* Guest CR0 */
  temp32 = RegGetCr0();
  CmSetBit(&temp32, 0);		// PE
  CmSetBit(&temp32, 5);		// NE
  CmSetBit(&temp32, 31);	// PG
  VmxWrite(GUEST_CR0, temp32);

  /* Guest CR3 */
  VmxWrite(GUEST_CR3, RegGetCr3());

  temp32 = RegGetCr4();
  CmSetBit(&temp32, 13);		// VMXE
  VmxWrite(GUEST_CR4, temp32);

  /* Guest segment base addresses */
  VmxWrite(GUEST_CS_BASE,   GetSegmentDescriptorBase(gdt_base, RegGetCs()));
  VmxWrite(GUEST_SS_BASE,   GetSegmentDescriptorBase(gdt_base, RegGetSs()));
  VmxWrite(GUEST_DS_BASE,   GetSegmentDescriptorBase(gdt_base, RegGetDs()));
  VmxWrite(GUEST_ES_BASE,   GetSegmentDescriptorBase(gdt_base, RegGetEs()));
  VmxWrite(GUEST_FS_BASE,   GetSegmentDescriptorBase(gdt_base, RegGetFs()));
  VmxWrite(GUEST_GS_BASE,   GetSegmentDescriptorBase(gdt_base, RegGetGs()));
  VmxWrite(GUEST_LDTR_BASE, GetSegmentDescriptorBase(gdt_base, RegGetLdtr()));
  VmxWrite(GUEST_TR_BASE,   GetSegmentDescriptorBase(gdt_base, RegGetTr()));

  /* Guest GDTR/IDTR base */
  VmxWrite(GUEST_GDTR_BASE, gdt_reg.BaseLo | (gdt_reg.BaseHi << 16));
  VmxWrite(GUEST_IDTR_BASE, idt_reg.BaseLo | (idt_reg.BaseHi << 16));

  /* Guest RFLAGS */
  FLAGS_TO_ULONG(eFlags) = RegGetFlags();
  VmxWrite(GUEST_RFLAGS, FLAGS_TO_ULONG(eFlags));

  /* Guest IA32_SYSENTER_ESP */
  ReadMSR(IA32_SYSENTER_ESP);
  VmxWrite(GUEST_SYSENTER_ESP, msr.Lo);

  /* Guest IA32_SYSENTER_EIP */
  ReadMSR(IA32_SYSENTER_EIP);
  VmxWrite(GUEST_SYSENTER_EIP, msr.Lo);
	
  //	*****************************************
  //	* H.4.4 Natural-Width Host-State Fields *
  //	*****************************************

  /* Host CR0, CR3 and CR4 */
  VmxWrite(HOST_CR0, RegGetCr0() & ~(1 << 16)); /* Disable WP */
  VmxWrite(HOST_CR3, RegGetCr3());
  VmxWrite(HOST_CR4, RegGetCr4());

  /* Host FS, GS and TR base */
  VmxWrite(HOST_FS_BASE, GetSegmentDescriptorBase(gdt_base, RegGetFs()));
  VmxWrite(HOST_GS_BASE, GetSegmentDescriptorBase(gdt_base, RegGetGs()));
  VmxWrite(HOST_TR_BASE, GetSegmentDescriptorBase(gdt_base, RegGetTr()));

  /* Host GDTR/IDTR base (they both hold *linear* addresses) */
  VmxWrite(HOST_GDTR_BASE, gdt_reg.BaseLo | (gdt_reg.BaseHi << 16));
  VmxWrite(HOST_IDTR_BASE, GetSegmentDescriptorBase(gdt_base, RegGetDs()) + (ULONG) VMMInitState.VMMIDT);

  /* Host IA32_SYSENTER_ESP/EIP/CS */
  ReadMSR(IA32_SYSENTER_ESP);
  VmxWrite(HOST_IA32_SYSENTER_ESP, msr.Lo);

  ReadMSR(IA32_SYSENTER_EIP);
  VmxWrite(HOST_IA32_SYSENTER_EIP, msr.Lo);

  ReadMSR(IA32_SYSENTER_CS);
  VmxWrite(HOST_IA32_SYSENTER_CS, msr.Lo);

  // (5)	Issue a sequence of VMWRITEs to initialize various host-state area
  //		fields in the working VMCS. The initialization sets up the context
  //		and entry-points to the VMM VIRTUAL-MACHINE MONITOR PROGRAMMING
  //		CONSIDERATIONS upon subsequent VM exits from the guest. Host-state
  //		fields include control registers (CR0, CR3 and CR4), selector fields
  //		for the segment registers (CS, SS, DS, ES, FS, GS and TR), and base-
  //		address fields (for FS, GS, TR, GDTR and IDTR; RSP, RIP and the MSRs
  //		that control fast system calls).

  // (6)	Use VMWRITEs to set up the various VM-exit control fields, VM-entry
  //		control fields, and VM-execution control fields in the VMCS. Care
  //		should be taken to make sure the settings of individual fields match
  //		the allowed 0 and 1 settings for the respective controls as reported
  //		by the VMX capability MSRs (see Appendix G). Any settings inconsistent
  //		with the settings reported by the capability MSRs will cause VM
  //		entries to fail.
	
  // (7)	Use VMWRITE to initialize various guest-state area fields in the
  //		working VMCS. This sets up the context and entry-point for guest
  //		execution upon VM entry. Chapter 22 describes the guest-state loading
  //		and checking done by the processor for VM entries to protected and
  //		virtual-8086 guest execution.

  /* Clear the VMX Abort Error Code prior to VMLAUNCH */
  RtlZeroMemory((VMMInitState.pVMCSRegion + 4), 4);
  Log("Clearing VMX Abort Error Code", *(VMMInitState.pVMCSRegion + 4));

  /* Set EIP, ESP for the Guest right before calling VMLAUNCH */
  Log("Setting Guest ESP", GuestStack);
  VmxWrite(GUEST_RSP, (ULONG) GuestStack);
	
  Log("Setting Guest EIP", GuestReturn);
  VmxWrite(GUEST_RIP, (ULONG) GuestReturn);

  /* Set EIP, ESP for the Host right before calling VMLAUNCH */
  Log("Setting Host ESP", ((ULONG) VMMInitState.VMMStack + VMM_STACK_SIZE - 1));
  VmxWrite(HOST_RSP, ((ULONG) VMMInitState.VMMStack + VMM_STACK_SIZE - 1));

  Log("Setting Host EIP", VMMEntryPoint);
  VmxWrite(HOST_RIP, (ULONG) VMMEntryPoint);

  ///////////////
  // VMLAUNCH  //
  ///////////////
  VmxLaunch();

  FLAGS_TO_ULONG(eFlags) = RegGetFlags();

  Log("VMLAUNCH Failure", 0xDEADF00D);
	
  /* Get the error number from VMCS */
  ErrorCode = VmxRead(VM_INSTRUCTION_ERROR);
  Log("VM Instruction Error", ErrorCode);

 Abort:

  ScrubTheLaunch = 1;
  __asm {
    MOV		ESP, GuestStack;
    JMP		GuestReturn;
  };
}

/* Driver unload procedure */
VOID DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
  WindowsLog("[vmm-unload] Active Processor Bitmap  [%08X]", (ULONG) KeQueryActiveProcessors());
  WindowsLog("[vmm-unload] Disabling VMX mode on CPU 0.", 0);

  KeSetSystemAffinityThread((KAFFINITY) 0x00000001);

  FiniPlugin();
  FiniGuest();

  if(VMXIsActive) {
    __asm {
      PUSHAD;
      MOV		EAX, HYPERCALL_SWITCHOFF;
			
      _emit 0x0F;	// VMCALL
      _emit 0x01;
      _emit 0xC1;
			
      POPAD;
    };
  }

  WindowsLog("[vmm-unload] Freeing memory regions.", 0);

  MmFreeNonCachedMemory(VMMInitState.pVMXONRegion, 4096);
  MmFreeNonCachedMemory(VMMInitState.pVMCSRegion, 4096);
  ExFreePoolWithTag(VMMInitState.VMMStack, 'kSkF');
  MmFreeNonCachedMemory(VMMInitState.pIOBitmapA , 4096);
  MmFreeNonCachedMemory(VMMInitState.pIOBitmapB , 4096);
  MmFreeNonCachedMemory(VMMInitState.VMMIDT, sizeof(IDT_ENTRY)*256);

  WindowsLog("[vmm-unload] Driver unloaded.", 0);
}

/* Driver entry point */
NTSTATUS DriverEntry( IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath )
{
  ULONG		EntryEFlags = 0;
  ULONG		cr4 = 0;

  ULONG		EntryEAX = 0;
  ULONG		EntryECX = 0;
  ULONG		EntryEDX = 0;
  ULONG		EntryEBX = 0;
  ULONG		EntryESP = 0;
  ULONG		EntryEBP = 0;
  ULONG		EntryESI = 0;
  ULONG		EntryEDI = 0;
	
  DriverObject->DriverUnload = DriverUnload;

  /* Initialize debugging port (COM_PORT_ADDRESS, see comio.h) */
  PortInit();
  ComInit();

  WindowsLog("Driver Routines", 0);
  WindowsLog("---------------", 0);
  WindowsLog("   Driver Entry", DriverEntry);
  WindowsLog("   Driver Unload", DriverUnload);
  WindowsLog("   StartVMX", StartVMX);
  WindowsLog("   VMMEntryPoint", VMMEntryPoint);
	
  /* Check if PAE is enabled. */
  CR4_TO_ULONG(cr4) = RegGetCr4();

  if(cr4 & 0x00000020) {
    WindowsLog("******************************", 0);
    WindowsLog("Error : PAE must be disabled.",  0);
    WindowsLog("Add the following to boot.ini:", 0);
    WindowsLog("  /noexecute=alwaysoff /nopae",  0);
    WindowsLog("******************************", 0);
    goto error;
  }

  /* Register event handlers */
  if (!NT_SUCCESS(RegisterEvents()))
    goto error;

  /* Allocate the VMXON region memory. */
  VMMInitState.pVMXONRegion = MmAllocateNonCachedMemory(4096);
  if(VMMInitState.pVMXONRegion == NULL) {
    WindowsLog("ERROR : Allocating VMXON Region memory.", 0);
    goto error;
  }
  WindowsLog("VMXONRegion virtual address", VMMInitState.pVMXONRegion);
  RtlZeroMemory(VMMInitState.pVMXONRegion, 4096);
  VMMInitState.PhysicalVMXONRegionPtr = MmGetPhysicalAddress(VMMInitState.pVMXONRegion);
  WindowsLog("VMXONRegion physical address", VMMInitState.PhysicalVMXONRegionPtr.LowPart);

  /* Allocate the VMCS region memory. */
  VMMInitState.pVMCSRegion = MmAllocateNonCachedMemory(4096);
  if(VMMInitState.pVMCSRegion == NULL) {
    WindowsLog("ERROR : Allocating VMCS Region memory.", 0);
    goto error;
  }
  WindowsLog("VMCSRegion virtual address", VMMInitState.pVMCSRegion);
  RtlZeroMemory(VMMInitState.pVMCSRegion, 4096);
  VMMInitState.PhysicalVMCSRegionPtr = MmGetPhysicalAddress(VMMInitState.pVMCSRegion);
  WindowsLog("VMCSRegion physical address", VMMInitState.PhysicalVMCSRegionPtr.LowPart);
	
  /* Allocate stack for the VM Exit Handler. */
  VMMInitState.VMMStack = ExAllocatePoolWithTag(NonPagedPool , VMM_STACK_SIZE, 'kSkF');
  if(VMMInitState.VMMStack == NULL) {
    WindowsLog("ERROR : Allocating VM Exit Handler stack memory.", 0);
    goto error;
  }
  RtlZeroMemory(VMMInitState.VMMStack, VMM_STACK_SIZE);
  WindowsLog("VMMStack", VMMInitState.VMMStack);

  /* Allocate a memory page for the I/O bitmap A */
  VMMInitState.pIOBitmapA = MmAllocateNonCachedMemory(4096);
  if(VMMInitState.pIOBitmapA == NULL) {
    WindowsLog("ERROR : Allocating I/O bitmap A memory.", 0);
    goto error;
  }
  WindowsLog("I/O bitmap A virtual address", VMMInitState.pIOBitmapA);
  RtlZeroMemory(VMMInitState.pIOBitmapA, 4096);
  VMMInitState.PhysicalIOBitmapA = MmGetPhysicalAddress(VMMInitState.pIOBitmapA);
  WindowsLog("I/O bitmap A physical address", VMMInitState.PhysicalIOBitmapA.LowPart);

  /* Allocate a memory page for the I/O bitmap A */
  VMMInitState.pIOBitmapB = MmAllocateNonCachedMemory(4096);
  if(VMMInitState.pIOBitmapB == NULL) {
    WindowsLog("ERROR : Allocating I/O bitmap A memory.", 0);
    goto error;
  }
  WindowsLog("I/O bitmap A virtual address", VMMInitState.pIOBitmapB);
  RtlZeroMemory(VMMInitState.pIOBitmapB, 4096);
  VMMInitState.PhysicalIOBitmapB = MmGetPhysicalAddress(VMMInitState.pIOBitmapB);
  WindowsLog("I/O bitmap A physical address", VMMInitState.PhysicalIOBitmapB.LowPart);

  /* Allocate & initialize the IDT for the VMM */
  VMMInitState.VMMIDT = MmAllocateNonCachedMemory(sizeof(IDT_ENTRY)*256);
  if (VMMInitState.VMMIDT == NULL) {
    WindowsLog("ERROR : Allocating VMM interrupt descriptor table.", 0);
    goto error;
  }
  InitVMMIDT(VMMInitState.VMMIDT);
  WindowsLog("VMMIDT", VMMInitState.VMMIDT);

  if (InitGuest(DriverObject) != STATUS_SUCCESS)
    goto error;
  if (InitPlugin() != STATUS_SUCCESS)
    goto error;

  __asm {
    CLI;
    MOV GuestStack, ESP;
  };
	
  /* Save the state of the architecture */
  __asm {
    PUSHAD;
    POP		EntryEDI;
    POP		EntryESI;
    POP		EntryEBP;
    POP		EntryESP;
    POP		EntryEBX;
    POP		EntryEDX;
    POP		EntryECX;
    POP		EntryEAX;
    PUSHFD;
    POP		EntryEFlags;
  };
	
  StartVMX();
	
  /* Restore the state of the architecture */
  __asm {
    PUSH	EntryEFlags;
    POPFD;
    PUSH	EntryEAX;
    PUSH	EntryECX;
    PUSH	EntryEDX;
    PUSH	EntryEBX;
    PUSH	EntryESP;
    PUSH	EntryEBP;
    PUSH	EntryESI;
    PUSH	EntryEDI;
    POPAD;
  };
	
  __asm {
    STI;
    MOV		ESP, GuestStack;
  };
	
  WindowsLog("Running on Processor", KeGetCurrentProcessorNumber());
	
  if( ScrubTheLaunch == 1 ){
    WindowsLog("ERROR : Launch aborted.", 0);
    goto error;
  }
	
  WindowsLog("VM is now executing.", 0);
	
  return STATUS_SUCCESS;

 error:
  /* Cleanup & return an error code */
  FiniPlugin();

  if (VMMInitState.pVMXONRegion)
    MmFreeNonCachedMemory(VMMInitState.pVMXONRegion , 4096);
  if (VMMInitState.pVMCSRegion)
    MmFreeNonCachedMemory(VMMInitState.pVMCSRegion , 4096);
  if (VMMInitState.VMMStack)
    ExFreePoolWithTag(VMMInitState.VMMStack, 'kSkF');
  if (VMMInitState.pIOBitmapA)
    MmFreeNonCachedMemory(VMMInitState.pIOBitmapA , 4096);
  if (VMMInitState.pIOBitmapB)
    MmFreeNonCachedMemory(VMMInitState.pIOBitmapB , 4096);
  if (VMMInitState.VMMIDT)
    MmFreeNonCachedMemory(VMMInitState.VMMIDT , sizeof(IDT_ENTRY)*256);

  return STATUS_UNSUCCESSFUL;
}

static NTSTATUS InitGuest(PDRIVER_OBJECT DriverObject)
{
#ifdef GUEST_WINDOWS
  if (WindowsInit(DriverObject) != STATUS_SUCCESS) {
    WindowsLog("ERROR: Windows-specific initialization routine has failed.", 0);
    return STATUS_UNSUCCESSFUL;
  }
#elif defined GUEST_LINUX
#error Unimplemented
#else
#error Please specify a valid guest OS
#endif

  return STATUS_SUCCESS;
}

static NTSTATUS FiniGuest(VOID)
{
  return STATUS_SUCCESS;
}

static NTSTATUS InitPlugin(VOID)
{
#ifdef ENABLE_HYPERDBG
  /* Initialize the guest module of HyperDbg */
  if(HyperDbgGuestInit() != STATUS_SUCCESS) {
    WindowsLog("ERROR : HyperDbg GUEST initialization error.", 0);
    return STATUS_UNSUCCESSFUL;
  }
  /* Initialize the host module of HyperDbg */
  if(HyperDbgHostInit(&VMMInitState) != STATUS_SUCCESS) {
    WindowsLog("ERROR : HyperDbg HOST initialization error.", 0);
    return STATUS_UNSUCCESSFUL;
  }
#endif

  return STATUS_SUCCESS;
}

static NTSTATUS FiniPlugin(VOID)
{
#ifdef ENABLE_HYPERDBG
  HyperDbgHostFini();
  HyperDbgGuestFini();
#endif

  return STATUS_SUCCESS;
}

