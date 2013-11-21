/* Copyright notice
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

#include "pill.h"
#include <asm/io.h>
#include "linux.h"
#include <linux/module.h>
#include <linux/kobject.h>
module_init(DriverEntry);
module_exit(DriverUnload);

static hvm_bool    ScrubTheLaunch = FALSE;
static hvm_address GuestReturn;
static hvm_address HostCR3;

/* #################### */
/* #### PROTOTYPES #### */
/* #################### */
/* Plugin & guest initialization/finalization */
static hvm_status InitGuest(void);
static hvm_status FiniGuest(void);
static hvm_status InitPlugin(void);

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

  /* FIXME: set thread affinity */
  /* const struct cpumask m; */
  /* pid_t p= sys_gettid(); */
  /* if( !sched_setaffinity(p,&m) ) */
  /*   goto Abort; */

  /* GuestLog("Running on Processor #%d", KeGetCurrentProcessorNumber()); */
  
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
			"movl %0,%%esp\n"			
			"jmp  *%1\n"
			::"m"(GuestStack),"m"(GuestReturn)
			);
}

/* Driver unload procedure */
void __exit DriverUnload(void)
{
  ScrubTheLaunch = FALSE;
 /* GuestLog("[vmm-unload] Active processor bitmap: %.8x", (ULONG) KeQueryActiveProcessors()); */
  GuestLog("[vmm-unload] Disabling VT mode");

  /* FIXME: set thread affinity */
  /* const struct cpumask m; */
  /* pid_t p= sys_gettid(); */
  /* sched_setaffinity(p,&m); */
  
  FiniPlugin();			/* Finalize plugins */
  FiniGuest();			/* Finalize guest-specific structures */
    
  if(hvm_x86_ops.vt_enabled()) {
    hvm_x86_ops.vt_hypercall(HYPERCALL_SWITCHOFF);
  }
  
  GuestLog("[vmm-unload] Freeing memory regions");
  
  hvm_x86_ops.vt_finalize();
  MmuFini();			/* Finalize the MMU (e.g., deallocate the host's PT) still not used under linux */       
  GuestLog("[vmm-unload] Driver unloaded");
}

/* Driver entry point */
int __init DriverEntry(void)
{ 
  CR4_REG cr4;

  /* Initialize debugging port (COM_PORT_ADDRESS, see comio.h) */
#ifdef DEBUG
    PortInit();
    ComInit();
#endif

  GuestLog("Driver Routines");
  GuestLog("---------------");
  GuestLog("   Driver Entry:  %.8x", (hvm_address) DriverEntry);
  GuestLog("   StartVT:       %.8x", (hvm_address) StartVT);
  GuestLog("   VMMEntryPoint: %.8x", (hvm_address) hvm_x86_ops.hvm_handle_exit);

  /* Check if PAE is enabled or not */
  CR4_TO_ULONG(cr4) = RegGetCr4();

  
  if(cr4.PAE) {
    GuestLog("ERROR: No support for Linux PAE ATM...");
    goto error;
  }

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

#ifdef ENABLE_HYPERDBG
  /* Initialize the guest module of HyperDbg */
  if(HyperDbgGuestInit() != HVM_STATUS_SUCCESS) {
    GuestLog("ERROR: HyperDbg GUEST initialization error");
    return HVM_STATUS_UNSUCCESSFUL;
  }
#endif
  
  /* Initialize the MMU */
  if (!HVM_SUCCESS(MmuInit(&HostCR3))) {
    GuestLog("Failed to initialize MMU");
    goto error;
  }
  GuestLog("Using private CR3: %08x", HostCR3);
  
  /* Initialize guest-specific stuff */
  if (!HVM_SUCCESS(InitGuest())) {
    GuestLog("Failed to initialize guest-specific stuff");
    goto error;
  }
   
  /* Initialize plugins */
  if (!HVM_SUCCESS(InitPlugin())) {
    GuestLog("Failed to initialize plugin");
    goto error;
  }

  if (!HVM_SUCCESS(LinuxInitStructures())) {
	GuestLog("Failed to initialize data structures");
	goto error;
  }

  DoStartVT();
  
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
  /* Initialize the host module of HyperDbg */
  if(HyperDbgHostInit() != HVM_STATUS_SUCCESS) {
    GuestLog("ERROR: HyperDbg HOST initialization error");
    return HVM_STATUS_UNSUCCESSFUL;
  }
#endif
  
  return HVM_STATUS_SUCCESS;
}

static hvm_status InitGuest(void)
{
  return HVM_STATUS_SUCCESS;
}

static hvm_status FiniGuest(void)
{
  return HVM_STATUS_SUCCESS;
}
