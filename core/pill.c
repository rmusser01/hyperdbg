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

#include "config.h"
#include "debug.h"
#include "pill.h"
#include "mmu.h"
#include "vt.h"
#include "events.h"
#include "x86.h"
#include "msr.h"
#include "idt.h"
#include "comio.h"
#include "common.h"
#include "vmhandlers.h"

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

/* #################### */
/* #### PROTOTYPES #### */
/* #################### */

static void       StartVT();
static void       InitVMMIDT(PIDT_ENTRY pidt);
static hvm_status RegisterEvents(void);

/* Plugin & guest initialization/finalization */
static hvm_status InitGuest(PDRIVER_OBJECT DriverObject);
static hvm_status FiniGuest(void);
static hvm_status InitPlugin(void);
static hvm_status FiniPlugin(void);

/* ################# */
/* #### GLOBALS #### */
/* ################# */

/* Static variables, that are not directly accessed by other modules or plugins. */
static hvm_bool	   ScrubTheLaunch = FALSE;
static hvm_address GuestReturn;
static hvm_address GuestStack;
static hvm_address HostCR3;

/* ################ */
/* #### BODIES #### */
/* ################ */

static hvm_status RegisterEvents(void)
{
  EVENT_CONDITION_HYPERCALL hypercall;

  /* Initialize the event handler */
  EventInit();

  /* Register a hypercall to switch off the VM */
  hypercall.hypernum = HYPERCALL_SWITCHOFF;

  if(!EventSubscribe(EventHypercall, &hypercall, sizeof(hypercall), HypercallSwitchOff)) {
    WindowsLog("ERROR: Unable to register switch-off hypercall handler");
    return HVM_STATUS_UNSUCCESSFUL;
  }

  return HVM_STATUS_SUCCESS;
}

/* Initialize the IDT of the VMM */
static void InitVMMIDT(PIDT_ENTRY pidt)
{
  int i;
  IDT_ENTRY idte_null;

  idte_null.Selector   = RegGetCs();

  /* Present, DPL 0, Type 0xe (INT gate) */
  idte_null.Access     = (1 << 15) | (0xe << 8);

  idte_null.LowOffset  = (Bit32u) NullIDTHandler & 0xffff;
  idte_null.HighOffset = (Bit32u) NullIDTHandler >> 16;

  for (i=0; i<256; i++) {
    pidt[i] = idte_null;
  }
}

__declspec(naked) void StartVT()
{	
  //	Get the Guest Return RIP.
  //
  //	Hi	|	    |
  //		+-----------+
  //		|    RIP    |
  //		+-----------+ <--	ESP after the CALL
  //	Lo	|	    |

  __asm	{ POP GuestReturn };

  WindowsLog("Guest Return EIP: %.8x", GuestReturn);
  WindowsLog("Enabling VT mode");

  /* Set thread affinity */
  KeSetSystemAffinityThread((KAFFINITY) 0x00000001);
  WindowsLog("Running on Processor #%d", KeGetCurrentProcessorNumber());

  /* Enable VT support */
  if (!HVM_SUCCESS(hvm_x86_ops.vt_hardware_enable())) {
    goto Abort;
  }

  /* ************************************************* */
  /* **** The processor is now in root operation! **** */
  /* ****    No more APIs after this point!       **** */
  /* ************************************************* */

  /* Initialize the VMCS */
  if (!HVM_SUCCESS(hvm_x86_ops.vt_vmcs_initialize(GuestStack, GuestReturn, HostCR3)))
    goto Abort;

  /* Update the events that must be handled by the HVM */
  if (!HVM_SUCCESS(hvm_x86_ops.hvm_update_events()))
    goto Abort;

  /* LAUNCH! */
  hvm_x86_ops.vt_launch();

  Log("VMLAUNCH Failure");
	
 Abort:

  ScrubTheLaunch = TRUE;
  __asm {
    MOV		ESP, GuestStack;
    JMP		GuestReturn;
  };
}

/* Driver unload procedure */
VOID DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
  WindowsLog("[vmm-unload] Active processor bitmap: %.8x", (ULONG) KeQueryActiveProcessors());
  WindowsLog("[vmm-unload] Disabling VT mode");

  KeSetSystemAffinityThread((KAFFINITY) 0x00000001);

  MmuFini();			/* Finalize the MMU (e.g., deallocate the host's PT) */
  FiniPlugin();			/* Finalize plugins */
  FiniGuest();			/* Finalize guest-specific structures */

  if(hvm_x86_ops.vt_enabled()) {
    hvm_x86_ops.vt_hypercall(HYPERCALL_SWITCHOFF);
  }

  WindowsLog("[vmm-unload] Freeing memory regions");

  hvm_x86_ops.vt_finalize();

  WindowsLog("[vmm-unload] Driver unloaded");
}

/* Driver entry point */
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
  CR4_REG cr4;

  hvm_address EntryRFlags = 0;
  hvm_address EntryRAX    = 0;
  hvm_address EntryRCX    = 0;
  hvm_address EntryRDX    = 0;
  hvm_address EntryRBX    = 0;
  hvm_address EntryRSP    = 0;
  hvm_address EntryRBP    = 0;
  hvm_address EntryRSI    = 0;
  hvm_address EntryRDI    = 0;
	
  DriverObject->DriverUnload = DriverUnload;

  /* Initialize debugging port (COM_PORT_ADDRESS, see comio.h) */
  PortInit();
  ComInit();

  WindowsLog("Driver Routines");
  WindowsLog("---------------");
  WindowsLog("   Driver Entry:  %.8x", DriverEntry);
  WindowsLog("   Driver Unload: %.8x", DriverUnload);
  WindowsLog("   StartVT:       %.8x", StartVT);
  WindowsLog("   VMMEntryPoint: %.8x", hvm_x86_ops.hvm_handle_exit);
	
  /* Check if PAE is enabled or not */
  CR4_TO_ULONG(cr4) = RegGetCr4();

#ifdef ENABLE_PAE
  if(!cr4.PAE) {
    WindowsLog("PAE support enabled, but the guest is NOT using it ");
    WindowsLog("Add the option /pae to boot.ini");
    goto error;
  }
#else
  if(cr4.PAE) {
    WindowsLog("PAE support disabled, but the guest is using it ");
    WindowsLog("Add the options /noexecute=alwaysoff /nopae to boot.ini");
    goto error;
  }
#endif

  /* Register event handlers */
  if (!HVM_SUCCESS(RegisterEvents())) {
    WindowsLog("Failed to register events");
    goto error;
  }

  /* Initialize VT */
  if (!HVM_SUCCESS(hvm_x86_ops.vt_initialize(InitVMMIDT))) {
    WindowsLog("Failed to initialize VT");
    goto error;
  }

  /* Initialize the MMU */
  if (!HVM_SUCCESS(MmuInit(&HostCR3))) {
  	WindowsLog("Failed to initialize MMU");
    goto error;
  }

  /* Initialize guest-specific stuff */
  if (!HVM_SUCCESS(InitGuest(DriverObject))) {
  	WindowsLog("Failed to initialize guest-specific stuff");
    goto error;
  }

  /* Initialize plugins */
  if (!HVM_SUCCESS(InitPlugin())) {
  	WindowsLog("Failed to initialize plugin");
    goto error;
  }

  __asm {
    CLI;
    MOV GuestStack, ESP;
  };
	
  /* Save the state of the architecture */
  __asm {
    PUSHAD;
    POP		EntryRDI;
    POP		EntryRSI;
    POP		EntryRBP;
    POP		EntryRSP;
    POP		EntryRBX;
    POP		EntryRDX;
    POP		EntryRCX;
    POP		EntryRAX;
    PUSHFD;
    POP		EntryRFlags;
  };
	
  StartVT();
	
  /* Restore the state of the architecture */
  __asm {
    PUSH	EntryRFlags;
    POPFD;
    PUSH	EntryRAX;
    PUSH	EntryRCX;
    PUSH	EntryRDX;
    PUSH	EntryRBX;
    PUSH	EntryRSP;
    PUSH	EntryRBP;
    PUSH	EntryRSI;
    PUSH	EntryRDI;
    POPAD;
  };
	
  __asm {
    STI;
    MOV		ESP, GuestStack;
  };
	
  WindowsLog("Running on processor #%d", KeGetCurrentProcessorNumber());
	
  if(ScrubTheLaunch == TRUE){
    WindowsLog("ERROR: Launch aborted");
    goto error;
  }
	
  WindowsLog("VM is now executing");
	
  return STATUS_SUCCESS;

 error:
  /* Cleanup & return an error code */
  FiniPlugin();

  hvm_x86_ops.vt_finalize();

  return STATUS_UNSUCCESSFUL;
}

static hvm_status InitGuest(PDRIVER_OBJECT DriverObject)
{
#ifdef GUEST_WINDOWS
  if (WindowsInit(DriverObject) != HVM_STATUS_SUCCESS) {
    WindowsLog("ERROR: Windows-specific initialization routine has failed");
    return HVM_STATUS_UNSUCCESSFUL;
  }
#elif defined GUEST_LINUX
#error Unimplemented
#else
#error Please specify a valid guest OS
#endif

  return HVM_STATUS_SUCCESS;
}

static hvm_status FiniGuest(void)
{
  return HVM_STATUS_SUCCESS;
}

static hvm_status InitPlugin(void)
{
#ifdef ENABLE_HYPERDBG
  /* Initialize the guest module of HyperDbg */
  if(HyperDbgGuestInit() != HVM_STATUS_SUCCESS) {
    WindowsLog("ERROR: HyperDbg GUEST initialization error");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  /* Initialize the host module of HyperDbg */
  if(HyperDbgHostInit() != HVM_STATUS_SUCCESS) {
    WindowsLog("ERROR: HyperDbg HOST initialization error");
    return HVM_STATUS_UNSUCCESSFUL;
  }
#endif

  return HVM_STATUS_SUCCESS;
}

static hvm_status FiniPlugin(void)
{
#ifdef ENABLE_HYPERDBG
  HyperDbgHostFini();
  HyperDbgGuestFini();
#endif

  return HVM_STATUS_SUCCESS;
}

