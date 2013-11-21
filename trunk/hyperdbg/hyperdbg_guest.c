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

#include "hyperdbg.h"
#include "hyperdbg_common.h"
#include "hyperdbg_guest.h"
#include "hyperdbg_host.h"

#ifdef GUEST_WINDOWS
#include "winxp.h"
#elif defined GUEST_LINUX

#endif

#include "keyboard.h"
#include "debug.h"
#include "vt.h"
#include "video.h"
#include "mmu.h"

/* ################# */
/* #### GLOBALS #### */
/* ################# */

/* This one is also accessed by module hyperdbg_host.c */
HYPERDBG_STATE hyperdbg_state;

/* ################ */
/* #### BODIES #### */
/* ################ */

/* This function gets called when VMX is still off. */
hvm_status HyperDbgGuestInit(void)
{
  hvm_status r;

  /* Initialize the video subsystem */
  if(VideoInit() != HVM_STATUS_SUCCESS) {
    GuestLog("[HyperDbg] Video initialization error");
    return HVM_STATUS_UNSUCCESSFUL;
  }

  /* Allocate video buffer */
  r = VideoAlloc();
  if(r != HVM_STATUS_SUCCESS) {
    GuestLog("[HyperDbg] Cannot initialize video!");
    return HVM_STATUS_UNSUCCESSFUL;
  }

  /* Initialize global hyperdbg_state structure fields */
  hyperdbg_state.initialized = TRUE;
  hyperdbg_state.enabled = FALSE;
  hyperdbg_state.singlestepping = FALSE;
  hyperdbg_state.TF_on = FALSE;
  hyperdbg_state.IF_on = FALSE;
  hyperdbg_state.console_mode = TRUE;
  hyperdbg_state.hasPermBP = FALSE;
  hyperdbg_state.ntraps = 0;
  
  /* Init guest-specific fields */
#ifdef GUEST_WINDOWS
  r = WindowsGetKernelBase(&hyperdbg_state.win_state.kernel_base);
  if (r != HVM_STATUS_SUCCESS) {
    GuestLog("[HyperDbg] Cannot initialize guest-specific variables!");
    return HVM_STATUS_UNSUCCESSFUL;
  }
#elif defined GUEST_LINUX

#else
#error Invalid HyperDBG guest!
#endif

  GuestLog("[HyperDbg] Guest initialization ok!");

  return HVM_STATUS_SUCCESS;
}

/* This function finalizes HyperDbg. Can be called both in VMX operation (e.g.,
   when unloading the driver) and with VMX turned off (e.g., when pill
   installation is aborting). It is always invoked in non-root mode. */
hvm_status HyperDbgGuestFini(void)
{
  if(!hyperdbg_state.initialized) return HVM_STATUS_SUCCESS;

  GuestLog("[HyperDbg] Unloading...");

  /* Deallocate video buffer */
  VideoDealloc();

  hyperdbg_state.initialized = FALSE;

  return HVM_STATUS_SUCCESS;
}
