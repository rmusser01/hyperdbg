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
hvm_bool isIOStepping = FALSE;

static InjectException(Bit32u trap, Bit32u type);

EVENT_PUBLISH_STATUS HypercallSwitchOff(void)
{
  return HVM_SUCCESS(hvm_x86_ops.hvm_switch_off()) ? \
    EventPublishHandled : EventPublishPass;
}

void HandleCR(void)
{
  EVENT_CONDITION_CR cr;
  EVENT_PUBLISH_STATUS s;
  Bit32u movcrControlRegister, movcrAccessType, movcrOperandType, movcrGeneralPurposeRegister;

  movcrControlRegister = (context.ExitContext.ExitQualification & 0x0000000F);
  movcrAccessType      = ((context.ExitContext.ExitQualification & 0x00000030) >> 4);
  movcrOperandType     = ((context.ExitContext.ExitQualification & 0x00000040) >> 6);
  movcrGeneralPurposeRegister = ((context.ExitContext.ExitQualification & 0x00000F00) >> 8);

  /* Notify to plugins */
  cr.crno    = (Bit8u) movcrControlRegister;
  cr.iswrite = movcrAccessType == 0;
  s = EventPublish(EventControlRegister, &cr, sizeof(cr));  

  if (s == EventPublishHandled) {
    /* Event has been handled by the plugin */
    return;
  }

  /* Process the event (only for MOV CRx, REG instructions) */
  if (movcrOperandType == 0 && 
      (movcrControlRegister == 0 || movcrControlRegister == 3 || movcrControlRegister == 4)) {
    if (movcrAccessType == 0) {
      /* CRx <-- reg32 */
      Bit32u x;

      if (movcrControlRegister == 0) 
	x = GUEST_CR0;
      else if (movcrControlRegister == 3)
	x = GUEST_CR3;
      else
	x = GUEST_CR4;	  

      switch(movcrGeneralPurposeRegister) {
      case 0:  hvm_x86_ops.vt_vmcs_write(x, context.GuestContext.RAX); break;
      case 1:  hvm_x86_ops.vt_vmcs_write(x, context.GuestContext.RCX); break;
      case 2:  hvm_x86_ops.vt_vmcs_write(x, context.GuestContext.RDX); break;
      case 3:  hvm_x86_ops.vt_vmcs_write(x, context.GuestContext.RBX); break;
      case 4:  hvm_x86_ops.vt_vmcs_write(x, context.GuestContext.RSP); break;
      case 5:  hvm_x86_ops.vt_vmcs_write(x, context.GuestContext.RBP); break;
      case 6:  hvm_x86_ops.vt_vmcs_write(x, context.GuestContext.RSI); break;
      case 7:  hvm_x86_ops.vt_vmcs_write(x, context.GuestContext.RDI); break;
      default: break;
      }
    } else if (movcrAccessType == 1) {
      /* reg32 <-- CRx */
      hvm_address x;

      if (movcrControlRegister == 0)
	x = context.GuestContext.CR0;
      else if (movcrControlRegister == 3)
	x = context.GuestContext.CR3;
      else
	x = context.GuestContext.CR4;

      switch(movcrGeneralPurposeRegister) {
      case 0:  context.GuestContext.RAX = x; break;
      case 1:  context.GuestContext.RCX = x; break;
      case 2:  context.GuestContext.RDX = x; break;
      case 3:  context.GuestContext.RBX = x; break;
      case 4:  context.GuestContext.RSP = x; break;
      case 5:  context.GuestContext.RBP = x; break;
      case 6:  context.GuestContext.RSI = x; break;
      case 7:  context.GuestContext.RDI = x; break;
      default: break;
      }
    }
  }
}

void HandleHLT(void)
{
  EVENT_CONDITION_NONE none;

  EventPublish(EventHlt, &none, sizeof(none));
}

void HandleIO(void)
{
  EVENT_IO_DIRECTION dir;
  Bit32u port;
  EVENT_CONDITION_IO io;
  EVENT_PUBLISH_STATUS s;

  dir  = (context.ExitContext.ExitQualification & (1 << 3)) ? EventIODirectionIn : EventIODirectionOut;
  port = (context.ExitContext.ExitQualification & 0xffff0000) >> 16;

  io.direction = dir;
  io.portnum = port;
  s = EventPublish(EventIO, &io, sizeof(io));

  if (s != EventPublishHandled) {
    /* We would have to emulate the I/O instruction. However, this is quite
       hard, especially one of the arguments is a memory address. For this
       reason, we fall back on a simpler (and more effective) solution: we
       disable I/O traps, let the guest do a single step, and then enable I/O
       traps again */
    Bit32u v;
    isIOStepping = TRUE;
    
    /* Disable I/O bitmaps */
    v = hvm_x86_ops.vt_vmcs_read(CPU_BASED_VM_EXEC_CONTROL);
    CmClearBit32(&v, 25);		
    hvm_x86_ops.vt_vmcs_write(CPU_BASED_VM_EXEC_CONTROL, v);

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
  s = EventPublish(EventHypercall, &event, sizeof(event));

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
  hvm_x86_ops.vt_vmcs_write(VM_ENTRY_EXCEPTION_ERROR_CODE, 
			    context.ExitContext.ExitInterruptionErrorCode);

  hvm_x86_ops.vt_vmcs_write(VM_ENTRY_INTR_INFO_FIELD, 
	   INTR_INFO_VALID_MASK | INTR_TYPE_HW_EXCEPTION | TRAP_INVALID_OP);

  /* Re-exec faulty instruction */
  context.GuestContext.ResumeRIP = context.GuestContext.RIP;
}

void HandleNMI(void)
{
  Bit32u trap;
  EVENT_PUBLISH_STATUS s;
  EVENT_CONDITION_EXCEPTION e;
  hvm_bool isSynteticDebug;

  trap = context.ExitContext.ExitInterruptionInformation & INTR_INFO_VECTOR_MASK;
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
      Bit32u v;

      v = hvm_x86_ops.vt_vmcs_read(CPU_BASED_VM_EXEC_CONTROL);
      CmSetBit32(&v, 25);		
      hvm_x86_ops.vt_vmcs_write(CPU_BASED_VM_EXEC_CONTROL, v);

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

  s = EventPublish(EventException, &e, sizeof(e));
  
  if (s == EventPublishNone || s == EventPublishPass) {
    if (isSynteticDebug) {
      /* This is a synthetic #DB exception (used to execute I/O instruction
	 that trap), and no plugin handled it. We can safely disable
	 single-stepping */

      /* TODO: Check if TF was not enabled by the guest before!!! */
      context.GuestContext.RFLAGS = FLAGS_TO_ULONG(context.GuestContext.RFLAGS) & ~FLAGS_TF_MASK;
    } else {
      /* Pass hw exception to the guest OS */
      InjectException(INTR_TYPE_HW_EXCEPTION, trap);
    }

    /* Re-execute the faulty instruction */
    context.GuestContext.ResumeRIP = context.GuestContext.RIP;

    /* Handle PF */
    if (trap == TRAP_PAGE_FAULT) {
      /* Restore into CR2 the faulting address */
      RegSetCr2(context.ExitContext.ExitQualification);
    }
  }
}

static InjectException(Bit32u trap, Bit32u type)
{
  Bit32u v;

  /* Read the page-fault error code and write it into the VM-entry exception error code field */
  hvm_x86_ops.vt_vmcs_write(VM_ENTRY_EXCEPTION_ERROR_CODE, 
			    context.ExitContext.ExitInterruptionErrorCode);

  /* Write the VM-entry interruption-information field */
  v = (INTR_INFO_VALID_MASK | trap | type);

  /* Check if bits 11 (deliver code) and 31 (valid) are set. In this
     case, error code has to be delivered to guest OS */
  if ((context.ExitContext.ExitInterruptionInformation & INTR_INFO_DELIVER_CODE_MASK) &&
      (context.ExitContext.ExitInterruptionInformation & INTR_INFO_VALID_MASK)) {
    v |= INTR_INFO_DELIVER_CODE_MASK;
  }

  hvm_x86_ops.vt_vmcs_write(VM_ENTRY_INTR_INFO_FIELD, v);
}
