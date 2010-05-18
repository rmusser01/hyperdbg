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

#include "hyperdbg.h"
#include "hyperdbg_common.h"
#include "hyperdbg_guest.h"
#include "hyperdbg_host.h"
#include "winxp.h"
#include "keyboard.h"
#include "debug.h"
#include "vmx.h"
#include "video.h"

/* ################# */
/* #### GLOBALS #### */
/* ################# */

/* This one is also accessed by module hyperdbg_host.c */
HYPERDBG_STATE hyperdbg_state;

/* ################ */
/* #### BODIES #### */
/* ################ */

/* This function gets called when VMX is still off. */
NTSTATUS HyperDbgGuestInit(VOID)
{
  NTSTATUS r;

  /* Initialize the video subsystem */
  if(VideoInit() != STATUS_SUCCESS) {
    WindowsLog("[HyperDbg] Video initialization error");
    return STATUS_UNSUCCESSFUL;
  }

  /* Allocate video buffer */
  r = VideoAlloc();
  if(!NT_SUCCESS(r)) {
    WindowsLog("[HyperDbg] Cannot initialize video!");
    return STATUS_UNSUCCESSFUL;
  }

  /* Initialize global hyperdbg_state structure fields */
  hyperdbg_state.initialized = TRUE;
  hyperdbg_state.enabled = FALSE;
  hyperdbg_state.singlestepping = FALSE;
  hyperdbg_state.ntraps = 0;
  
  /* Init guest-specific fields */
#ifdef GUEST_WINDOWS
  r = WindowsGetKernelBase(&hyperdbg_state.win_state.kernel_base);
  if (!NT_SUCCESS(r)) {
    WindowsLog("[HyperDbg] Cannot initialize guest-specific variables!");
    return STATUS_UNSUCCESSFUL;
  }
#elif defined GUEST_LINUX
#error Linux guest is still unimplemented
#else
#error Invalid HyperDBG guest!
#endif

  WindowsLog("[HyperDbg] Initialized!", 0);

  return STATUS_SUCCESS;
}

/* This function finalizes HyperDbg. Can be called both in VMX operation (e.g.,
   when unloading the driver) and with VMX turned off (e.g., when pill
   installation is aborting). It is always invoked in non-root mode. */
NTSTATUS HyperDbgGuestFini(VOID)
{
  if(!hyperdbg_state.initialized) return STATUS_SUCCESS;

  WindowsLog("[HyperDbg] Unloading...");

  /* Deallocate video buffer */
  VideoDealloc();

  hyperdbg_state.initialized = FALSE;

  return STATUS_SUCCESS;
}
