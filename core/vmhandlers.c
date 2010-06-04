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

#include "types.h"
#include "vmhandlers.h"
#include "vt.h"
#include "events.h"
#include "debug.h"
#include "common.h"
#include "msr.h"
#include "x86.h"

/* When this variable is TRUE, we are single stepping over an I/O
   instruction */
static hvm_bool isIOStepping = FALSE;

EVENT_PUBLISH_STATUS HypercallSwitchOff(PEVENT_ARGUMENTS args)
{
  return HVM_SUCCESS(hvm_x86_ops.hvm_switch_off()) ? \
    EventPublishHandled : EventPublishPass;
}

void HandleCR(Bit8u crno, VtCrAccessType accesstype, hvm_bool ismemory, VtRegister gpr)
{
  EVENT_CONDITION_CR cr;
  EVENT_PUBLISH_STATUS s;

  Log("HandleCR(%d, %d, %d, %d)", crno, accesstype, ismemory, gpr);

  /* Notify to plugins */
  cr.crno    = crno;
  cr.iswrite = (accesstype == VT_CR_ACCESS_WRITE);
  s = EventPublish(EventControlRegister, NULL, &cr, sizeof(cr));  

  if (s == EventPublishHandled) {
    /* Event has been handled by the plugin */
    return;
  }

  /* Process the event (only for MOV CRx, REG instructions) */
  if (!ismemory && (crno == 0 || crno == 3 || crno == 4)) {
    if (accesstype == VT_CR_ACCESS_WRITE) {
      /* CRx <-- reg32 */
      void (*f)(hvm_address);      

      if (crno == 0) 
	f = hvm_x86_ops.vt_set_cr0;
      else if (crno == 3)
	f = hvm_x86_ops.vt_set_cr3;
      else
	f = hvm_x86_ops.vt_set_cr4;

      switch(gpr) {
      case VT_REGISTER_RAX:  f(context.GuestContext.RAX); break;
      case VT_REGISTER_RCX:  f(context.GuestContext.RCX); break;
      case VT_REGISTER_RDX:  f(context.GuestContext.RDX); break;
      case VT_REGISTER_RBX:  f(context.GuestContext.RBX); break;
      case VT_REGISTER_RSP:  f(context.GuestContext.RSP); break;
      case VT_REGISTER_RBP:  f(context.GuestContext.RBP); break;
      case VT_REGISTER_RSI:  f(context.GuestContext.RSI); break;
      case VT_REGISTER_RDI:  f(context.GuestContext.RDI); break;
      default: 
	Log("HandleCR(WRITE): unknown register %d", gpr);
	break;
      }
    } else if (accesstype == VT_CR_ACCESS_READ) {
      /* reg32 <-- CRx */
      hvm_address x;

      if (crno == 0)
	x = context.GuestContext.CR0;
      else if (crno == 3)
	x = context.GuestContext.CR3;
      else
	x = context.GuestContext.CR4;

      switch(gpr) {
      case VT_REGISTER_RAX:  context.GuestContext.RAX = x; break;
      case VT_REGISTER_RCX:  context.GuestContext.RCX = x; break;
      case VT_REGISTER_RDX:  context.GuestContext.RDX = x; break;
      case VT_REGISTER_RBX:  context.GuestContext.RBX = x; break;
      case VT_REGISTER_RSP:  context.GuestContext.RSP = x; break;
      case VT_REGISTER_RBP:  context.GuestContext.RBP = x; break;
      case VT_REGISTER_RSI:  context.GuestContext.RSI = x; break;
      case VT_REGISTER_RDI:  context.GuestContext.RDI = x; break;
      default: 
	Log("HandleCR(READ): unknown register %d", gpr);
	break;
      }
    }
  }
}

void HandleHLT(void)
{
  EVENT_CONDITION_NONE none;

  EventPublish(EventHlt, NULL, &none, sizeof(none));
}

void HandleIO(Bit16u port, hvm_bool isoutput, Bit8u size, hvm_bool isstring, hvm_bool isrep)
{
  EVENT_IO_DIRECTION dir;
  EVENT_CONDITION_IO io;
  EVENT_PUBLISH_STATUS s;
  EVENT_ARGUMENTS args;

  /* Initialize event condition */
  dir  = isoutput ? EventIODirectionOut : EventIODirectionIn;
  io.direction = dir;
  io.portnum   = port;

  /* Initialize event arguments */
  args.EventIO.size = size;
  args.EventIO.isstring = isstring;
  args.EventIO.isrep = isrep;

  /* Publish the I/O event (with arguments) */
  s = EventPublish(EventIO, &args, &io, sizeof(io));

  if (s != EventPublishHandled) {
    /* We would have to emulate the I/O instruction. However, this is quite
       hard, especially one of the arguments is a memory address. For this
       reason, we fall back on a simpler (and more effective) solution: we
       disable I/O traps, let the guest do a single step, and then enable I/O
       traps again */
    Bit32u v;
    isIOStepping = TRUE;
    
    /* Disable I/O bitmaps */
    hvm_x86_ops.vt_trap_io(FALSE);

    /* Re-exec faulty instruction */
    context.GuestContext.ResumeRIP = context.GuestContext.RIP;

    /* Enable single-step, but don't trap current instruction */
    context.GuestContext.RFLAGS = context.GuestContext.RFLAGS | FLAGS_TF_MASK | FLAGS_RF_MASK;
  }
}

void HandleVMCALL(void)
{
  EVENT_CONDITION_HYPERCALL event;
  EVENT_CALLBACK f;
  EVENT_PUBLISH_STATUS s;

  Log("VMCALL #%.8x detected", context.GuestContext.RAX);

  event.hypernum = context.GuestContext.RAX;
  s = EventPublish(EventHypercall, NULL, &event, sizeof(event));

  if (s == EventPublishNone) {
    /* Unexisting hypercall */
    Log("Unimplemented hypercall #%.8x", context.GuestContext.RAX);
  }
}

/* Deny through the injection of an #UD exception */
void HandleVMLAUNCH(void)
{
  /* The interruption error code should be undefined, so we just copy the one
     passed by the CPU */
  hvm_x86_ops.hvm_inject_hw_exception(TRAP_INVALID_OP, 0);  

  /* Re-exec faulty instruction */
  context.GuestContext.ResumeRIP = context.GuestContext.RIP;
}

void HandleNMI(Bit32u trap, Bit32u error_code, Bit32u qualification)
{
  EVENT_PUBLISH_STATUS s;
  EVENT_CONDITION_EXCEPTION e;
  hvm_bool isSynteticDebug;

  isSynteticDebug = FALSE;
  
  switch (trap) {
  case TRAP_PAGE_FAULT:
  case TRAP_INT3:
    break;
  case TRAP_DEBUG:
    if (isIOStepping) {
      /* This is a syntetic #DB, generated by our I/O handling mechanism. We
	 must re-enable I/O traps and disable single-stepping if not required
	 by some plugins */
      hvm_x86_ops.vt_trap_io(TRUE);

      isIOStepping = FALSE;
      isSynteticDebug = TRUE;
    }
    break;
  default:
    /* Unhandled exception/nmi */
    Log("Unexpected exception/NMI #%.8x", trap);
    return;
  }

  /* Publish this exception */
  e.exceptionnum = trap;

  s = EventPublish(EventException, NULL, &e, sizeof(e));
  
  if (s == EventPublishNone || s == EventPublishPass) {
    if (isSynteticDebug) {
      /* This is a synthetic #DB exception (used to execute I/O instruction
	 that trap), and no plugin handled it. We can safely disable
	 single-stepping */

      /* TODO: Check if TF was not enabled by the guest before!!! */
      context.GuestContext.RFLAGS = FLAGS_TO_ULONG(context.GuestContext.RFLAGS) & ~FLAGS_TF_MASK;
    } else {
      /* Pass hw exception to the guest OS */
      hvm_x86_ops.hvm_inject_hw_exception(trap, error_code);
    }

    /* Re-execute the faulty instruction */
    context.GuestContext.ResumeRIP = context.GuestContext.RIP;

    /* Handle PF */
    if (trap == TRAP_PAGE_FAULT) {
      /* Restore into CR2 the faulting address */
      RegSetCr2(qualification);
    }
  }
}
