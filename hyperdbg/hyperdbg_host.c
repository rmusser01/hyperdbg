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

#include "hyperdbg_common.h"
#include "hyperdbg_host.h"
#include "hyperdbg_cmd.h"
#include "common.h"
#include "keyboard.h"
#include "video.h"
#include "x86.h"
#include "debug.h"
#include "vmm.h"
#include "vmmstring.h"
#include "gui.h"
#include "events.h"
#include "sw_bp.h"

/* ################ */
/* #### MACROS #### */
/* ################ */

#define HYPERDBG_HYPERCALL_SETRES    0xdead0001

/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

static EVENT_PUBLISH_STATUS HyperDbgSwBpHandler(void);
static EVENT_PUBLISH_STATUS HyperDbgDebugHandler(void);
static EVENT_PUBLISH_STATUS HyperDbgIOHandler(void);

/* This hypercall is invoked when we want to change the resolution of the GUI
   dynamically */
static EVENT_PUBLISH_STATUS HyperDbgHypercallSetResolution(void);

static void HyperDbgEnter(void);
static void HyperDbgCommandLoop(void);

/* ################ */
/* #### BODIES #### */
/* ################ */

static EVENT_PUBLISH_STATUS HyperDbgHypercallSetResolution(void)
{
  VideoSetResolution(context.GuestContext.RBX, context.GuestContext.RCX);

  return EventPublishHandled;
}

static void HyperDbgEnter(void)
{
  hvm_status r;

  /* Update HyperDbg state structure */
  hyperdbg_state.enabled = TRUE;

  if(!VideoEnabled()) {
    r = VideoAlloc();
    if(r != HVM_STATUS_SUCCESS)
      Log("[HyperDbg] Cannot allocate video memory!");
  }

  Log("[HyperDbg] Entering HyperDbg command loop");

  HyperDbgCommandLoop();

  Log("[HyperDbg] Leaving HyperDbg command loop");

  hyperdbg_state.enabled = FALSE;
}

static void HyperDbgCommandLoop(void)
{
  hvm_address flags;
  Bit8u c, keyboard_buffer[256];
  int i;
  hvm_bool isMouse, exitLoop;

  exitLoop = FALSE;
  
  /* Backup RFLAGS */
  flags = RegGetFlags();

  /* Disable interrupts (only if they were enabled) */
  RegSetFlags(flags & ~FLAGS_IF_MASK);

  if (VideoEnabled()) {
    /* Backup video memory */
    if (!hyperdbg_state.singlestepping) {
      VideoSave();
    }
    VideoInitShell();
  }

  /* We are not currently single-stepping */
  hyperdbg_state.singlestepping = FALSE;
  
  i = 0;
  memset(keyboard_buffer, 0, sizeof(keyboard_buffer));
  while (1) {
    if (KeyboardReadKeystroke(&c, FALSE, &isMouse) != HVM_STATUS_SUCCESS) {
      /* Sleep for some time, just to avoid full busy waiting */
      CmSleep(100);
      continue;
    }

    if (isMouse) {
      /* Skip mouse events */
      continue;
    }

    if (c == HYPERDBG_MAGIC_SCANCODE) {
      Log("[HyperDbg] Magic key detected! Disabling HyperDbg...");
      break;
    }

    c = KeyboardScancodeToKeycode(c);

    /* Process this key */
    switch (c) {
    case 0:
      /* Unrecognized key -- ignore it */
      break;

    case '\b':
      /* Backslash */
      if (i > 0)
	keyboard_buffer[--i] = 0;
      break;

    case '\n':
      /* End of command */
      exitLoop = HyperDbgProcessCommand(keyboard_buffer);
      i = 0;
      memset(keyboard_buffer, 0, sizeof(keyboard_buffer));
      break;

    case '\t':
      /* Replace tabs with spaces */
      c = ' ';
      /* "break" intentionally omitted! */

    default:
      /* Store this keycode. The '-1' is to ensure keyboard_buffer is always
	 null-terminated */
      if (i < (sizeof(keyboard_buffer)/sizeof(Bit8u)-1))
	keyboard_buffer[i++] = c;
      break;
    } /* end switch */

    if (VideoEnabled()) {
      VideoUpdateShell(keyboard_buffer);
    }

    if(exitLoop) {
      break; /* Command says that we must return control to the guest */
    }
  }

  if(VideoEnabled() && !hyperdbg_state.singlestepping) {
    /* Restore video memory */
    VideoRestore();
  }

  /* Restore original flags */
/*   RegSetFlags(flags); */
}

/* Invoked for write operations to the keyboard I/O port */
static EVENT_PUBLISH_STATUS HyperDbgIOHandler(void)
{
  Bit8u c;
  Bit32u size;
  hvm_bool isRep, isString, isMouse;

  size     = (context.ExitContext.ExitQualification & 7) + 1;
  isString = (context.ExitContext.ExitQualification & (1 << 4)) != 0;
  isRep    = (context.ExitContext.ExitQualification & (1 << 5)) != 0;

  if (size != 1 || isString || isRep) {
    return EventPublishPass;
  }

  /* 'inb', from keyboard port, not rep, not string */
  if (KeyboardReadKeystroke(&c, FALSE, &isMouse) != HVM_STATUS_SUCCESS) {
    /* Something bad happened.. */
    return EventPublishPass;
  }

  /* Emulate the 'inb' instruction. The destination is always %al */
  context.GuestContext.RAX = ((hvm_address) context.GuestContext.RAX & 0xffffff00) | (c & 0xff);

  if (!isMouse && c == HYPERDBG_MAGIC_SCANCODE) {
    /* This is our "magic" scancode! */

    /* FIXME: quite dirty... The problem is that RIP points to the 'inb'
       instruction, that will never be executed. For this reason, the debugger
       should not highlight 'inb' as the current instruction. Note: the "core"
       does not update the guest RIP with the value cached in the VMCS */
    context.GuestContext.RIP += context.ExitContext.ExitInstructionLength;

    HyperDbgEnter();
  }

  return EventPublishHandled;
}

static EVENT_PUBLISH_STATUS HyperDbgSwBpHandler(void)
{
  hvm_bool ours;

  /* DeleteSwBp will check if this bp has been setup by us and eventually reset it */
  Log("[HyperDbg] checking bp @%.8x", context.GuestContext.RIP);
  ours = SwBreakpointDelete(context.GuestContext.CR3, context.GuestContext.RIP);
  if(!ours) { /* Pass it to the guest */
    return EventPublishPass;
  }

  Log("[HyperDbg] bp is ours, resetting GUEST_RIP to %.8x", context.GuestContext.RIP);
  /* Update guest RIP to re-execute the faulty instruction */
  VmxWrite(GUEST_RIP, context.GuestContext.RIP);

  Log("[HyperDbg] Done!");

  HyperDbgEnter();

  return EventPublishHandled;
}

static EVENT_PUBLISH_STATUS HyperDbgDebugHandler(void)
{
  hvm_address flags;

  /* Check if we are single-stepping or not. This is needed because #DB
     exceptions are also generated by the core mechanisms that handles I/O
     instructions */
  if (!hyperdbg_state.singlestepping)
    return EventPublishPass;

  /* FIXME: unset iff TF has not been set by the guest */
  /* Unset TF and hide it to the guest */
  flags = context.GuestContext.RFLAGS;
  flags &= ~FLAGS_TF_MASK; 
  context.GuestContext.RFLAGS = flags;

  /* Update guest RIP to re-execute the faulty instruction */
  VmxWrite(GUEST_RIP, context.GuestContext.RIP);

  HyperDbgEnter();

  /* Let's set RF to 1 so that we won't trap when really executing the instruction */
  flags = context.GuestContext.RFLAGS;
  flags |= FLAGS_RF_MASK;
  context.GuestContext.RFLAGS = flags;
  VmxWrite(GUEST_RFLAGS, FLAGS_TO_ULONG(flags));

  return EventPublishHandled;
}

EVENT_PUBLISH_STATUS HyperDbgIO(void)
{
  return EventPublishHandled;
}

hvm_status HyperDbgHostInit(PVMM_INIT_STATE state)
{
  EVENT_CONDITION_EXCEPTION exception;
  EVENT_CONDITION_IO io;
  EVENT_CONDITION_HYPERCALL hypercall;
  hvm_status r;

  /* Init keyboard module */
  r = KeyboardInit();
  if (r != HVM_STATUS_SUCCESS)
    return r;

  /* Register the INT3 handler */
  exception.exceptionnum = TRAP_INT3;
  if(!EventSubscribe(EventException, &exception, sizeof(exception), HyperDbgSwBpHandler)) {
    return HVM_STATUS_UNSUCCESSFUL;
  }

  /* Register the DEBUG handler */
  exception.exceptionnum = TRAP_DEBUG;
  if(!EventSubscribe(EventException, &exception, sizeof(exception), HyperDbgDebugHandler)) {
    return HVM_STATUS_UNSUCCESSFUL;
  }

  /* Register a hypercall to update screen resolution */
  hypercall.hypernum = HYPERDBG_HYPERCALL_SETRES;

  if(!EventSubscribe(EventHypercall, &hypercall, sizeof(hypercall), HyperDbgHypercallSetResolution)) {
    WindowsLog("ERROR: Unable to register screen resolution hypercall handler");
    return HVM_STATUS_UNSUCCESSFUL;
  }

  /* Trap keyboard-related I/O instructions */
  io.direction = EventIODirectionIn;
  io.portnum = (Bit32u) KEYB_REGISTER_OUTPUT;
  if(!EventSubscribe(EventIO, &io, sizeof(io), HyperDbgIOHandler)) {
    return HVM_STATUS_UNSUCCESSFUL;
  }

  return HVM_STATUS_SUCCESS;
}

hvm_status HyperDbgHostFini(void)
{
  EVENT_CONDITION_EXCEPTION exception;

  /* Remove DEBUG handler */
  exception.exceptionnum = TRAP_DEBUG;
  EventUnsubscribe(EventException, &exception, sizeof(exception));

  /* Remove INT3 handler */  
  exception.exceptionnum = TRAP_INT3;
  EventUnsubscribe(EventException, &exception, sizeof(exception));

  return HVM_STATUS_SUCCESS;
}
