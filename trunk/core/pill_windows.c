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

#include "pill.h"
#include "winxp.h"

static hvm_bool    ScrubTheLaunch = FALSE;
static hvm_address GuestReturn;
static hvm_address HostCR3;

/* #################### */
/* #### PROTOTYPES #### */
/* #################### */
/* Plugin & guest initialization/finalization */
static hvm_status InitGuest(PDRIVER_OBJECT DriverObject);
static hvm_status FiniGuest(void);
static hvm_status InitPlugin(void);

/* ################# */
/* #### GLOBALS #### */
/* ################# */

static UNICODE_STRING kqap,kssat;
static UNICODE_STRING kgcpn; /* TO FIX */
static t_KeQueryActiveProcessors KeQueryActiveProcessors;
static t_KeSetSystemAffinityThread KeSetSystemAffinityThread;
static t_KeGetCurrentProcessorNumber KeGetCurrentProcessorNumber; /* TO FIX */

/* ################ */
/* #### BODIES #### */
/* ################ */

void StartVT()
{
  //	Get the Guest Return RIP.
  //
  //	Hi	|	    |
  //		+-----------+
  //		|    RIP    |
  //		+-----------+ <--	ESP after the CALL
  //	Lo	|	    |
  
  GuestReturn = (hvm_address) __builtin_return_address(0);
  
  GuestLog("Guest Return EIP: %.8x", GuestReturn);
  GuestLog("Enabling VT mode");
  
  /* Set thread affinity */
  KeSetSystemAffinityThread((KAFFINITY) 0x00000001);
  
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

  __asm__ __volatile__ (
			"movl %%ebp, %%esp\n"
			"popl %%ebp\n"
			"ret"
			::"m"(GuestStack),"m"(GuestReturn)
			);
}

/* Driver unload procedure */
VOID DDKAPI DriverUnload(PDRIVER_OBJECT DriverObject)
{
  GuestLog("[vmm-unload] Active processor bitmap: %.8x", (ULONG) KeQueryActiveProcessors());
  GuestLog("[vmm-unload] Disabling VT mode");

  KeSetSystemAffinityThread((KAFFINITY) 0x00000001);

  FiniPlugin();			/* Finalize plugins */
  FiniGuest();			/* Finalize guest-specific structures */

  if(hvm_x86_ops.vt_enabled()) {
    hvm_x86_ops.vt_hypercall(HYPERCALL_SWITCHOFF);
  }

  GuestLog("[vmm-unload] Freeing memory regions");

  hvm_x86_ops.vt_finalize();
  MmuFini();			/* Finalize the MMU (e.g., deallocate the host's PT) */
  GuestLog("[vmm-unload] Driver unloaded");
}

/* Driver entry point */
NTSTATUS DDKAPI DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
  CR4_REG cr4;

  ScrubTheLaunch= FALSE;

  /* Resolve some symbols */
  RtlInitUnicodeString(&kqap, L"KeQueryActiveProcessors");
  RtlInitUnicodeString(&kssat, L"KeSetSystemAffinityThread");

  /* FIXME */
  RtlInitUnicodeString(&kgcpn, L"KeGetCurrentProcessorNumber");
  KeQueryActiveProcessors = (t_KeQueryActiveProcessors) MmGetSystemRoutineAddress(&kqap);
  KeSetSystemAffinityThread = (t_KeSetSystemAffinityThread) MmGetSystemRoutineAddress(&kssat);

  /* FIXME */  
  KeGetCurrentProcessorNumber = (t_KeGetCurrentProcessorNumber) MmGetSystemRoutineAddress(&kgcpn);
  
  DriverObject->DriverUnload = (PDRIVER_UNLOAD) &DriverUnload;
  
  /* Initialize debugging port (COM_PORT_ADDRESS, see comio.h) */
  PortInit();
  ComInit();

  GuestLog("Driver Routines");
  GuestLog("---------------");
  GuestLog("   Driver Entry:  %.8x", DriverEntry);
  GuestLog("   Driver Unload: %.8x", DriverUnload);
  GuestLog("   StartVT:       %.8x", StartVT);
  GuestLog("   VMMEntryPoint: %.8x", hvm_x86_ops.hvm_handle_exit);
  
  /* Check if PAE is enabled or not */
  CR4_TO_ULONG(cr4) = RegGetCr4();
  
#ifdef ENABLE_PAE
  if(!cr4.PAE) {
    GuestLog("PAE support enabled, but the guest is NOT using it ");
    GuestLog("Add the option /pae to boot.ini");
    goto error;
  }
#else
  if(cr4.PAE) {
    GuestLog("PAE support disabled, but the guest is using it ");
    GuestLog("Add the options /noexecute=alwaysoff /nopae to boot.ini");
    goto error;
  }
#endif

  /* Register event handlers */
  if (!HVM_SUCCESS(RegisterEvents())) {
    GuestLog("Failed to register events");
    goto error;
  }
 
  /* Initialize VT */
  if (!HVM_SUCCESS(hvm_x86_ops.vt_initialize(InitVMMIDT))) {
    GuestLog("Failed to initialize VT");
    goto error;
  }
 
  /* Initialize the MMU */
  if (!HVM_SUCCESS(MmuInit(&HostCR3))) {
    GuestLog("Failed to initialize MMU");
    goto error;
  }
   
  /* Initialize guest-specific stuff */
  if (!HVM_SUCCESS(InitGuest(DriverObject))) {
    GuestLog("Failed to initialize guest-specific stuff");
    goto error;
  }
   
  /* Initialize plugins */
  if (!HVM_SUCCESS(InitPlugin())) {
    GuestLog("Failed to initialize plugin");
    goto error;
  }
   
  DoStartVT();
   
  /* TOFIX GuestLog("Running on Processor #%d", KeGetCurrentProcessorNumber()); */
   
  if(ScrubTheLaunch == TRUE){
    GuestLog("ERROR: Launch aborted");
    goto error;
  }
	
  GuestLog("VM is now executing");
  	
  return STATUS_SUCCESS;

 error:
  /* Cleanup & return an error code */
  FiniPlugin();

  hvm_x86_ops.vt_finalize();

  return STATUS_UNSUCCESSFUL;
}

static hvm_status InitPlugin(void)
{
#ifdef ENABLE_HYPERDBG
  /* Initialize the guest module of HyperDbg */
  if(HyperDbgGuestInit() != HVM_STATUS_SUCCESS) {
    GuestLog("ERROR: HyperDbg GUEST initialization error");
    return HVM_STATUS_UNSUCCESSFUL;
  }

  /* Initialize the host module of HyperDbg */
  if(HyperDbgHostInit() != HVM_STATUS_SUCCESS) {
    GuestLog("ERROR: HyperDbg HOST initialization error");
    return HVM_STATUS_UNSUCCESSFUL;
  }
#endif
  
  return HVM_STATUS_SUCCESS;
}

static hvm_status InitGuest(PDRIVER_OBJECT DriverObject)
{
  if (WindowsInit(DriverObject) != HVM_STATUS_SUCCESS) {
    GuestLog("ERROR: Windows-specific initialization routine has failed");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  return HVM_STATUS_SUCCESS;
}

static hvm_status FiniGuest(void)
{
  return HVM_STATUS_SUCCESS;
}

