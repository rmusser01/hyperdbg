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

#include "vmm.h"
#include "pill.h"
#include "vmhandlers.h"
#include "vmx.h"
#include "x86.h"
#include "events.h"
#include "debug.h"

/* Suppress warnings */
#pragma warning ( disable : 4102 ) /* Unreferenced label */
#pragma warning ( disable : 4731 ) /* EBP modified by inline asm */

/* Our copy of the virtual machine control structure */
CPU_CONTEXT context;

/* Temporary variable */
static ULONG    temp32;

/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

static VOID VMMReadGuestContext(VOID);

/* ################ */
/* #### BODIES #### */
/* ################ */

EVENT_PUBLISH_STATUS HypercallSwitchOff(VOID)
{
  /* Switch off VMX mode */
  Log("Terminating VMX Mode");
  Log("Flow returning to address %.8x", context.GuestContext.ResumeRIP);

  /* TODO: We should restore the whole original guest state here -- se Joanna's
     source code */
  RegSetIdtr((PVOID) VmxRead(GUEST_IDTR_BASE), VmxRead(GUEST_IDTR_LIMIT));

  __asm {
    /* Restore guest CR0 */
    MOV EAX, context.GuestContext.CR0;
    MOV CR0, EAX;

    /* Restore guest CR3 */
    MOV EAX, context.GuestContext.CR3;
    MOV CR3, EAX;

    /* Restore guest CR4 */
    MOV EAX, context.GuestContext.CR4;
    /* mov cr4, eax; */
    _emit 0x0f;
    _emit 0x22;
    _emit 0xe0;

  };

  /* Turn off VMX */
  VmxTurnOff();
  VMXIsActive = 0;

  __asm {
    /* Restore general-purpose registers */
    MOV EAX, context.GuestContext.RAX;
    MOV EBX, context.GuestContext.RBX;
    MOV ECX, context.GuestContext.RCX;
    MOV EDX, context.GuestContext.RDX;
    MOV EDI, context.GuestContext.RDI;
    MOV ESI, context.GuestContext.RSI;
    MOV EBP, context.GuestContext.RBP;

    /* Restore ESP */
    MOV	ESP, context.GuestContext.RSP;

    /* Restore guest RFLAGS */
    PUSH EAX;
    MOV EAX, context.GuestContext.RFLAGS;
    PUSH EAX;
    POPFD;
    POP EAX;

    /* Resume guest execution */
    JMP	context.GuestContext.ResumeRIP;
  };

  /* Unreachable */
  return EventPublishHandled;
}

/* Read guest state from the VMCS into the global CPU_CONTEXT variable. This
   procedure reads everything *except for* general purpose registers.

   NOTE: general purpose registers are not stored into the VMCS, so they are
   saved at the very beginning of the VMMEntryPoint() procedure */
static VOID VMMReadGuestContext(VOID)
{
  /* Exit state */
  context.ExitContext.ExitReason                   = VmxRead(VM_EXIT_REASON);
  context.ExitContext.ExitQualification            = VmxRead(EXIT_QUALIFICATION);
  context.ExitContext.ExitInterruptionInformation  = VmxRead(VM_EXIT_INTR_INFO);
  context.ExitContext.ExitInterruptionErrorCode    = VmxRead(VM_EXIT_INTR_ERROR_CODE);
  context.ExitContext.IDTVectoringInformationField = VmxRead(IDT_VECTORING_INFO_FIELD);
  context.ExitContext.IDTVectoringErrorCode        = VmxRead(IDT_VECTORING_ERROR_CODE);
  context.ExitContext.ExitInstructionLength        = VmxRead(VM_EXIT_INSTRUCTION_LEN);
  context.ExitContext.ExitInstructionInformation   = VmxRead(VMX_INSTRUCTION_INFO);

  /* Read guest state */
  context.GuestContext.RIP    = VmxRead(GUEST_RIP);
  context.GuestContext.RSP    = VmxRead(GUEST_RSP);
  context.GuestContext.CS     = VmxRead(GUEST_CS_SELECTOR);
  context.GuestContext.CR0    = VmxRead(GUEST_CR0);
  context.GuestContext.CR3    = VmxRead(GUEST_CR3);
  context.GuestContext.CR4    = VmxRead(GUEST_CR4);
  context.GuestContext.RFLAGS = VmxRead(GUEST_RFLAGS);

  /* Writing the Guest VMCS RIP uses general registers. Must complete this
     before setting general registers for guest return state */
  context.GuestContext.ResumeRIP = context.GuestContext.RIP + context.ExitContext.ExitInstructionLength;
}

/* Updates CPU state with the values from the VMCS cache structure. 

   Note: we update only those registers that are not already present in the
   (hardware) VMCS. */
static __declspec( naked ) VOID VMMUpdateGuestContext(VOID)
{
  __asm { MOV EAX, context.GuestContext.RAX };
  __asm { MOV EBX, context.GuestContext.RBX };
  __asm { MOV ECX, context.GuestContext.RCX };
  __asm { MOV EDX, context.GuestContext.RDX };
  __asm { MOV EDI, context.GuestContext.RDI };
  __asm { MOV ESI, context.GuestContext.RSI };
  __asm { MOV EBP, context.GuestContext.RBP };
  __asm { RET } ;
}

///////////////////////
//  VMM Entry Point  //
///////////////////////
__declspec( naked ) VOID VMMEntryPoint()
{
  __asm	{ PUSHFD };

  /* Record the general-purpose registers. We save them here in order to be
     sure that they don't get tampered by the execution of the VMM */
  __asm { MOV context.GuestContext.RAX, EAX };
  __asm { MOV context.GuestContext.RBX, EBX };
  __asm { MOV context.GuestContext.RCX, ECX };
  __asm { MOV context.GuestContext.RDX, EDX };
  __asm { MOV context.GuestContext.RDI, EDI };
  __asm { MOV context.GuestContext.RSI, ESI };
  __asm { MOV context.GuestContext.RBP, EBP };

  /* Restore host EBP */
  __asm	{ MOV EBP, ESP };

  VMMReadGuestContext();

  /* Restore host IDT -- Not sure if this is really needed. I'm pretty sure we
     have to fix the LIMIT fields of host's IDTR. */
  RegSetIdtr((PVOID) VmxRead(HOST_IDTR_BASE), 0x7ff);

  /* Enable logging only for particular VM-exit events */
  if( context.ExitContext.ExitReason == EXIT_REASON_VMCALL ) {
    HandlerLogging = TRUE;
  } else {
    HandlerLogging = FALSE;
  }

  if(HandlerLogging) {
    Log("----- VMM Handler CPU0 -----");
    Log("Guest RAX: %.8x", context.GuestContext.RAX);
    Log("Guest RBX: %.8x", context.GuestContext.RBX);
    Log("Guest RCX: %.8x", context.GuestContext.RCX);
    Log("Guest RDX: %.8x", context.GuestContext.RDX);
    Log("Guest RDI: %.8x", context.GuestContext.RDI);
    Log("Guest RSI: %.8x", context.GuestContext.RSI);
    Log("Guest RBP: %.8x", context.GuestContext.RBP);
    Log("Exit Reason:        %.8x", context.ExitContext.ExitReason);
    Log("Exit Qualification: %.8x", context.ExitContext.ExitQualification);
    Log("Exit Interruption Information:   %.8x", context.ExitContext.ExitInterruptionInformation);
    Log("Exit Interruption Error Code:    %.8x", context.ExitContext.ExitInterruptionErrorCode);
    Log("IDT-Vectoring Information Field: %.8x", context.ExitContext.IDTVectoringInformationField);
    Log("IDT-Vectoring Error Code:        %.8x", context.ExitContext.IDTVectoringErrorCode);
    Log("VM-Exit Instruction Length:      %.8x", context.ExitContext.ExitInstructionLength);
    Log("VM-Exit Instruction Information: %.8x", context.ExitContext.ExitInstructionInformation);
    Log("VM Exit RIP: %.8x", context.GuestContext.RIP);
    Log("VM Exit RSP: %.8x", context.GuestContext.RSP);
    Log("VM Exit CS:  %.4x", context.GuestContext.CS);
    Log("VM Exit CR0: %.8x", context.GuestContext.CR0);
    Log("VM Exit CR3: %.8x", context.GuestContext.CR3);
    Log("VM Exit CR4: %.8x", context.GuestContext.CR4);
    Log("VM Exit RFLAGS: %.8x", context.GuestContext.RFLAGS);
  }

  /* Set the next instructin to be executed */
  VmxWrite(GUEST_RIP, (ULONG) context.GuestContext.ResumeRIP);

  /////////////////////////////////////////////
  //  *** EXIT REASON CHECKS START HERE ***  //
  /////////////////////////////////////////////

  switch(context.ExitContext.ExitReason) {
    /////////////////////////////////////////////////////////////////////////////////////
    //  VMCLEAR, VMLAUNCH, VMPTRLD, VMPTRST, VMREAD, VMWRITE, VMRESUME, VMXOFF, VMXON  //
    /////////////////////////////////////////////////////////////////////////////////////

  case EXIT_REASON_VMCLEAR:
  case EXIT_REASON_VMPTRLD: 
  case EXIT_REASON_VMPTRST: 
  case EXIT_REASON_VMREAD:  
  case EXIT_REASON_VMRESUME:
  case EXIT_REASON_VMWRITE:
  case EXIT_REASON_VMXOFF:
  case EXIT_REASON_VMXON:
    Log("Request has been denied (reason: %.8x)", context.ExitContext.ExitReason);

    VMMUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

  case EXIT_REASON_VMLAUNCH:
    HandleVMLAUNCH();

    VMMUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

    //////////////
    //  VMCALL  //
    //////////////
  case EXIT_REASON_VMCALL:
    HandleVMCALL();

    VMMUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

    ////////////
    //  INVD  //
    ////////////
  case EXIT_REASON_INVD:
    Log("INVD detected");

    __asm { INVD };

    VMMUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

    /////////////
    //  RDMSR  //
    /////////////
  case EXIT_REASON_MSR_READ:
    Log("Read MSR #%.8x", context.GuestContext.RCX);

    VMMUpdateGuestContext();

    __asm {
			
      MOV		ECX, context.GuestContext.RCX;
      RDMSR;

      JMP		Resume;
    };

    /* Unreachable */
    break;

    /////////////
    //  WRMSR  //
    /////////////
  case EXIT_REASON_MSR_WRITE:
    Log("Write MSR #%.8x", context.GuestContext.RCX);

    WriteMSR(context.GuestContext.RCX, context.GuestContext.RDX, context.GuestContext.RAX);
    VMMUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

    /////////////
    //  CPUID  //
    /////////////
  case EXIT_REASON_CPUID:
    if(HandlerLogging) {
      Log("CPUID detected (RAX: %.8x)", context.GuestContext.RAX);
    }

    /* XXX Do we really need this check? */
    if(context.GuestContext.RAX == 0x00000000) {
      VMMUpdateGuestContext();

      __asm {
	MOV		EAX, 0x00000000;
	CPUID;
	MOV		EBX, 0x61656C43;
	MOV		ECX, 0x2E636E6C;
	MOV		EDX, 0x74614872;
	JMP		Resume;
      };
    }

    VMMUpdateGuestContext();

    __asm {
      MOV		EAX, context.GuestContext.RAX;
      CPUID;
      JMP		Resume;
    };

    /* Unreachable */
    break;

    ///////////////////////////////
    //  Control Register Access  //
    ///////////////////////////////
  case EXIT_REASON_CR_ACCESS:
    HandleCR();

    VMMUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

    ///////////////////////
    //  I/O instruction  //
    ///////////////////////
  case EXIT_REASON_IO_INSTRUCTION:
    HandleIO();

    VMMUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

  case EXIT_REASON_EXCEPTION_NMI:
    HandleNMI();

    VMMUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

  case EXIT_REASON_HLT:
    HandleHLT();

    // Cannot execute HLT with interrupt disabled
    // __asm { HLT };

    VMMUpdateGuestContext();
    goto Resume;

    /* Unreachable */
    break;

  default:
    /* Unknown exit condition */
    break;
  }

	
 Exit:
  // This label is reached when we are not able to handle the reason that
  // triggered this VM exit. In this case, we simply switch off VMX mode.
  HypercallSwitchOff();
	
 Resume:
  // Exit reason handled. Need to execute the VMRESUME without having
  // changed the state of the GPR and ESP et cetera.
  __asm { POPFD } ;

  VmxResume();
}

