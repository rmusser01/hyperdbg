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

#include "vmhandlers.h"
#include "vmm.h"
#include "vmx.h"
#include "events.h"
#include "debug.h"
#include "common.h"
#include "msr.h"
#include "x86.h"

/* When this variable is TRUE, we are single stepping over an I/O
   instruction */
BOOLEAN isIOStepping = FALSE;

static InjectException(ULONG trap, ULONG type);

VOID HandleCR(VOID)
{
  EVENT_CONDITION_CR cr;
  EVENT_PUBLISH_STATUS s;
  ULONG	movcrControlRegister, movcrAccessType, movcrOperandType, movcrGeneralPurposeRegister;

  movcrControlRegister = (vmcs.ExitState.ExitQualification & 0x0000000F);
  movcrAccessType      = ((vmcs.ExitState.ExitQualification & 0x00000030) >> 4);
  movcrOperandType     = ((vmcs.ExitState.ExitQualification & 0x00000040) >> 6);
  movcrGeneralPurposeRegister = ((vmcs.ExitState.ExitQualification & 0x00000F00) >> 8);

  /* Notify to plugins */
  cr.crno    = (UCHAR) movcrControlRegister;
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
      ULONG x;

      if (movcrControlRegister == 0) 
	x = GUEST_CR0;
      else if (movcrControlRegister == 3)
	x = GUEST_CR3;
      else
	x = GUEST_CR4;	  

      switch(movcrGeneralPurposeRegister) {
      case 0:  VmxWrite(x, vmcs.GuestState.EAX); break;
      case 1:  VmxWrite(x, vmcs.GuestState.ECX); break;
      case 2:  VmxWrite(x, vmcs.GuestState.EDX); break;
      case 3:  VmxWrite(x, vmcs.GuestState.EBX); break;
      case 4:  VmxWrite(x, vmcs.GuestState.ESP); break;
      case 5:  VmxWrite(x, vmcs.GuestState.EBP); break;
      case 6:  VmxWrite(x, vmcs.GuestState.ESI); break;
      case 7:  VmxWrite(x, vmcs.GuestState.EDI); break;
      default: break;
      }
    } else if (movcrAccessType == 1) {
      /* reg32 <-- CRx */
      ULONG x;

      if (movcrControlRegister == 0)
	x = vmcs.GuestState.CR0;
      else if (movcrControlRegister == 3)
	x = vmcs.GuestState.CR3;
      else
	x = vmcs.GuestState.CR4;

      switch(movcrGeneralPurposeRegister) {
      case 0:  vmcs.GuestState.EAX = x; break;
      case 1:  vmcs.GuestState.ECX = x; break;
      case 2:  vmcs.GuestState.EDX = x; break;
      case 3:  vmcs.GuestState.EBX = x; break;
      case 4:  vmcs.GuestState.ESP = x; break;
      case 5:  vmcs.GuestState.EBP = x; break;
      case 6:  vmcs.GuestState.ESI = x; break;
      case 7:  vmcs.GuestState.EDI = x; break;
      default: break;
      }
    }
  }
}

VOID HandleHLT(VOID)
{
  EVENT_CONDITION_NONE none;

  EventPublish(EventHlt, &none, sizeof(none));
}

VOID HandleIO(VOID)
{
  EVENT_IO_DIRECTION dir;
  ULONG port;
  EVENT_CONDITION_IO io;
  EVENT_PUBLISH_STATUS s;

  dir  = (vmcs.ExitState.ExitQualification & (1 << 3)) ? EventIODirectionIn : EventIODirectionOut;
  port = (vmcs.ExitState.ExitQualification & 0xffff0000) >> 16;

  io.direction = dir;
  io.portnum = port;
  s = EventPublish(EventIO, &io, sizeof(io));

  if (s != EventPublishHandled) {
    /* We would have to emulate the I/O instruction. However, this is quite
       hard, especially one of the arguments is a memory address. For this
       reason, we fall back on a simpler (and more effective) solution: we
       disable I/O traps, let the guest do a single step, and then enable I/O
       traps again */
    ULONG32 v;
    isIOStepping = TRUE;
    
    /* Disable I/O bitmaps */
    v = VmxRead(CPU_BASED_VM_EXEC_CONTROL);
    CmClearBit32(&v, 25);		
    VmxWrite(CPU_BASED_VM_EXEC_CONTROL, VmxAdjustControls(v, IA32_VMX_PROCBASED_CTLS));

    /* Re-exec faulty instruction */
    VmxWrite(GUEST_RIP, (ULONG) vmcs.GuestState.EIP);

    /* Enable single-step, but don't trap current instruction */
    v = vmcs.GuestState.EFLAGS | FLAGS_TF_MASK | FLAGS_RF_MASK;
    vmcs.GuestState.EFLAGS = v; 
    VmxWrite(GUEST_RFLAGS, FLAGS_TO_ULONG(v));
  }
}

VOID HandleVMCALL(VOID)
{
  EVENT_CONDITION_HYPERCALL event;
  EVENT_CALLBACK f;
  EVENT_PUBLISH_STATUS s;

  Log("VMCALL detected - CPU0", vmcs.GuestState.EAX);

  event.hypernum = vmcs.GuestState.EAX;
  s = EventPublish(EventHypercall, &event, sizeof(event));

  if (s == EventPublishNone) {
    /* Unexisting hypercall */
    Log("- Unimplemented hypercall", vmcs.GuestState.EAX);
  }
}

/* Deny through the injection of an #UD exception */
VOID HandleVMLAUNCH(VOID)
{
  /* The interruption error code should be undefined, so we just copy the one
     passed by the CPU */
  VmxWrite(VM_ENTRY_EXCEPTION_ERROR_CODE, vmcs.ExitState.ExitInterruptionErrorCode);
  VmxWrite(VM_ENTRY_INTR_INFO_FIELD, 
	   INTR_INFO_VALID_MASK | INTR_TYPE_HW_EXCEPTION | TRAP_INVALID_OP);

  /* Re-exec faulty instruction */
  VmxWrite(GUEST_RIP, (ULONG) vmcs.GuestState.EIP);
}

VOID HandleNMI(VOID)
{
  ULONG trap;
  EVENT_PUBLISH_STATUS s;
  EVENT_CONDITION_EXCEPTION e;
  BOOLEAN isSynteticDebug;

  trap = vmcs.ExitState.ExitInterruptionInformation & INTR_INFO_VECTOR_MASK;
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
      ULONG32 v;
      v = VmxRead(CPU_BASED_VM_EXEC_CONTROL);
      CmSetBit(&v, 25);		
      VmxWrite(CPU_BASED_VM_EXEC_CONTROL, VmxAdjustControls(v, IA32_VMX_PROCBASED_CTLS));
      isIOStepping = FALSE;
      isSynteticDebug = TRUE;
    }
    break;
  default:
    /* Unhandled exception/nmi */
    Log("Unexpected exception/NMI", trap);
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
      VmxWrite(GUEST_RFLAGS, FLAGS_TO_ULONG(vmcs.GuestState.EFLAGS) & ~FLAGS_TF_MASK);
    } else {
      /* Pass hw exception to the guest OS */
      InjectException(INTR_TYPE_HW_EXCEPTION, trap);
    }

    /* Re-execute the faulty instruction */
    VmxWrite(GUEST_RIP, (ULONG) vmcs.GuestState.EIP);

    /* Handle PF */
    if (trap == TRAP_PAGE_FAULT) {
      /* Restore into CR2 the faulting address */
      __asm {
	PUSH EAX;
	MOV EAX, vmcs.ExitState.ExitQualification;
	MOV CR2, EAX;
	POP EAX;
      };
    }
  }
}

static InjectException(ULONG trap, ULONG type)
{
  ULONG32 temp32;

  /* Read the page-fault error code and write it into the VM-entry exception error code field */
  VmxWrite(VM_ENTRY_EXCEPTION_ERROR_CODE, vmcs.ExitState.ExitInterruptionErrorCode);

  /* Write the VM-entry interruption-information field */
  temp32 = (INTR_INFO_VALID_MASK | trap | type);

  /* Check if bits 11 (deliver code) and 31 (valid) are set. In this
     case, error code has to be delivered to guest OS */
  if ((vmcs.ExitState.ExitInterruptionInformation & INTR_INFO_DELIVER_CODE_MASK) &&
      (vmcs.ExitState.ExitInterruptionInformation & INTR_INFO_VALID_MASK)) {
    temp32 |= INTR_INFO_DELIVER_CODE_MASK;
  }

  VmxWrite(VM_ENTRY_INTR_INFO_FIELD, temp32);
}
