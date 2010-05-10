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
VMCS_CACHE vmcs;

/* Temporary variable */
static ULONG    temp32;

/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

static VOID VMMReadGuestState(VOID);

/* ################ */
/* #### BODIES #### */
/* ################ */

EVENT_PUBLISH_STATUS HypercallSwitchOff(VOID)
{
  /* Switch off VMX mode */
  Log("- Terminating VMX Mode.", 0xDEADDEAD);
  Log("- Flow Control Return to Address", vmcs.GuestState.ResumeEIP);

  /* TODO: We should restore the whole original guest state here -- se Joanna's
     source code */
  RegSetIdtr((PVOID) VmxRead(GUEST_IDTR_BASE), VmxRead(GUEST_IDTR_LIMIT));

  __asm {
    /* Restore guest CR0 */
    mov eax, vmcs.GuestState.CR0;
    mov cr0, eax;

    /* Restore guest CR3 */
    mov eax, vmcs.GuestState.CR3;
    mov cr3, eax;

    /* Restore guest CR4 */
    mov eax, vmcs.GuestState.CR4;
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
    MOV EAX, vmcs.GuestState.EAX;
    MOV EBX, vmcs.GuestState.EBX;
    MOV ECX, vmcs.GuestState.ECX;
    MOV EDX, vmcs.GuestState.EDX;
    MOV EDI, vmcs.GuestState.EDI;
    MOV ESI, vmcs.GuestState.ESI;
    MOV EBP, vmcs.GuestState.EBP;

    /* Restore ESP */
    MOV	ESP, vmcs.GuestState.ESP;

    /* Restore guest RFLAGS */
    push eax;
    mov eax, vmcs.GuestState.EFLAGS;
    push eax;
    popfd;
    pop eax;

    /* Resume guest execution */
    JMP	vmcs.GuestState.ResumeEIP;
  };

  /* Unreachable */
  return EventPublishHandled;
}

/* Read guest state from the VMCS into the global VMCS_CACHE variable. This
   procedure reads everything *except for* general purpose registers.

   NOTE: general purpose registers are not stored into the VMCS, so they are
   saved at the very beginning of the VMMEntryPoint() procedure */
static VOID VMMReadGuestState(VOID)
{
  /* Exit state */
  vmcs.ExitState.ExitReason                   = VmxRead(VM_EXIT_REASON);
  vmcs.ExitState.ExitQualification            = VmxRead(EXIT_QUALIFICATION);
  vmcs.ExitState.ExitInterruptionInformation  = VmxRead(VM_EXIT_INTR_INFO);
  vmcs.ExitState.ExitInterruptionErrorCode    = VmxRead(VM_EXIT_INTR_ERROR_CODE);
  vmcs.ExitState.IDTVectoringInformationField = VmxRead(IDT_VECTORING_INFO_FIELD);
  vmcs.ExitState.IDTVectoringErrorCode        = VmxRead(IDT_VECTORING_ERROR_CODE);
  vmcs.ExitState.ExitInstructionLength        = VmxRead(VM_EXIT_INSTRUCTION_LEN);
  vmcs.ExitState.ExitInstructionInformation   = VmxRead(VMX_INSTRUCTION_INFO);

  /* Read guest state */
  vmcs.GuestState.EIP    = VmxRead(GUEST_RIP);
  vmcs.GuestState.ESP    = VmxRead(GUEST_RSP);
  vmcs.GuestState.CS     = VmxRead(GUEST_CS_SELECTOR);
  vmcs.GuestState.CR0    = VmxRead(GUEST_CR0);
  vmcs.GuestState.CR3    = VmxRead(GUEST_CR3);
  vmcs.GuestState.CR4    = VmxRead(GUEST_CR4);
  vmcs.GuestState.EFLAGS = VmxRead(GUEST_RFLAGS);

  /* Writing the Guest VMCS EIP uses general registers. Must complete this
     before setting general registers for guest return state */
  vmcs.GuestState.ResumeEIP = vmcs.GuestState.EIP + vmcs.ExitState.ExitInstructionLength;
}

/* Updates CPU state with the values from the VMCS cache structure. 

   Note: we update only those registers that are not already present in the
   (hardware) VMCS. */
static __declspec( naked ) VOID VMMUpdateGuestState(VOID)
{
  __asm { MOV EAX, vmcs.GuestState.EAX };
  __asm { MOV EBX, vmcs.GuestState.EBX };
  __asm { MOV ECX, vmcs.GuestState.ECX };
  __asm { MOV EDX, vmcs.GuestState.EDX };
  __asm { MOV EDI, vmcs.GuestState.EDI };
  __asm { MOV ESI, vmcs.GuestState.ESI };
  __asm { MOV EBP, vmcs.GuestState.EBP };
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
  __asm { MOV vmcs.GuestState.EAX, EAX };
  __asm { MOV vmcs.GuestState.EBX, EBX };
  __asm { MOV vmcs.GuestState.ECX, ECX };
  __asm { MOV vmcs.GuestState.EDX, EDX };
  __asm { MOV vmcs.GuestState.EDI, EDI };
  __asm { MOV vmcs.GuestState.ESI, ESI };
  __asm { MOV vmcs.GuestState.EBP, EBP };

  /* Restore host EBP */
  __asm	{ MOV EBP, ESP };

  VMMReadGuestState();

  /* Restore host IDT -- Not sure if this is really needed. I'm pretty sure we
     have to fix the LIMIT fields of host's IDTR. */
  RegSetIdtr((PVOID) VmxRead(HOST_IDTR_BASE), 0x7ff);

  /* Enable logging only for particular VM-exit events */
  if( vmcs.ExitState.ExitReason == EXIT_REASON_VMCALL ) {
    HandlerLogging = 1;
  } else {
    HandlerLogging = 0;
  }

  if(HandlerLogging) {
    Log("----- VMM Handler CPU0 -----", 0);
    Log("Guest EAX", vmcs.GuestState.EAX);
    Log("Guest EBX", vmcs.GuestState.EBX);
    Log("Guest ECX", vmcs.GuestState.ECX);
    Log("Guest EDX", vmcs.GuestState.EDX);
    Log("Guest EDI", vmcs.GuestState.EDI);
    Log("Guest ESI", vmcs.GuestState.ESI);
    Log("Guest EBP", vmcs.GuestState.EBP);
    Log("Exit Reason", vmcs.ExitState.ExitReason);
    Log("Exit Qualification", vmcs.ExitState.ExitQualification);
    Log("Exit Interruption Information", vmcs.ExitState.ExitInterruptionInformation);
    Log("Exit Interruption Error Code", vmcs.ExitState.ExitInterruptionErrorCode);
    Log("IDT-Vectoring Information Field", vmcs.ExitState.IDTVectoringInformationField);
    Log("IDT-Vectoring Error Code", vmcs.ExitState.IDTVectoringErrorCode);
    Log("VM-Exit Instruction Length", vmcs.ExitState.ExitInstructionLength);
    Log("VM-Exit Instruction Information", vmcs.ExitState.ExitInstructionInformation);
    Log("VM Exit EIP", vmcs.GuestState.EIP);
    Log("VM Exit ESP", vmcs.GuestState.ESP);
    Log("VM Exit CS", vmcs.GuestState.CS);
    Log("VM Exit CR0", vmcs.GuestState.CR0);
    Log("VM Exit CR3", vmcs.GuestState.CR3);
    Log("VM Exit CR4", vmcs.GuestState.CR4);
    Log("VM Exit EFLAGS", vmcs.GuestState.EFLAGS);
  }

  /* Set the next instructin to be executed */
  VmxWrite(GUEST_RIP, (ULONG) vmcs.GuestState.ResumeEIP);

  /////////////////////////////////////////////
  //  *** EXIT REASON CHECKS START HERE ***  //
  /////////////////////////////////////////////

  switch(vmcs.ExitState.ExitReason) {
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
    Log("Request has been denied - CPU0", vmcs.ExitState.ExitReason);

    VMMUpdateGuestState();
    goto Resume;

    /* Unreachable */
    break;

  case EXIT_REASON_VMLAUNCH:
    HandleVMLAUNCH();

    VMMUpdateGuestState();
    goto Resume;

    /* Unreachable */
    break;

    //////////////
    //  VMCALL  //
    //////////////
  case EXIT_REASON_VMCALL:
    HandleVMCALL();

    VMMUpdateGuestState();
    goto Resume;

    /* Unreachable */
    break;

    ////////////
    //  INVD  //
    ////////////
  case EXIT_REASON_INVD:
    Log("INVD detected - CPU0", 0);

    __asm { INVD };

    VMMUpdateGuestState();
    goto Resume;

    /* Unreachable */
    break;

    /////////////
    //  RDMSR  //
    /////////////
  case EXIT_REASON_MSR_READ:
    Log("Read MSR - CPU0", vmcs.GuestState.ECX);

    VMMUpdateGuestState();

    __asm {
			
      MOV		ECX, vmcs.GuestState.ECX;
      RDMSR;

      JMP		Resume;
    };

    /* Unreachable */
    break;

    /////////////
    //  WRMSR  //
    /////////////
  case EXIT_REASON_MSR_WRITE:
    Log("Write MSR - CPU0", vmcs.GuestState.ECX);

    WriteMSR(vmcs.GuestState.ECX, vmcs.GuestState.EDX, vmcs.GuestState.EAX);
    VMMUpdateGuestState();
    goto Resume;

    /* Unreachable */
    break;

    /////////////
    //  CPUID  //
    /////////////
  case EXIT_REASON_CPUID:
    if(HandlerLogging) {
      Log("CPUID detected - CPU0", 0);
      Log("- EAX", vmcs.GuestState.EAX);
    }

    /* XXX Do we really need this check? */
    if(vmcs.GuestState.EAX == 0x00000000) {
      VMMUpdateGuestState();

      __asm {
	MOV		EAX, 0x00000000;
	CPUID;
	MOV		EBX, 0x61656C43;
	MOV		ECX, 0x2E636E6C;
	MOV		EDX, 0x74614872;
	JMP		Resume;
      };
    }

    VMMUpdateGuestState();

    __asm {
      MOV		EAX, vmcs.GuestState.EAX;
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

    VMMUpdateGuestState();
    goto Resume;

    /* Unreachable */
    break;

    ///////////////////////
    //  I/O instruction  //
    ///////////////////////
  case EXIT_REASON_IO_INSTRUCTION:
    HandleIO();

    VMMUpdateGuestState();
    goto Resume;

    /* Unreachable */
    break;

  case EXIT_REASON_EXCEPTION_NMI:
    HandleNMI();

    VMMUpdateGuestState();
    goto Resume;

    /* Unreachable */
    break;

  case EXIT_REASON_HLT:
    HandleHLT();

    // Cannot execute HLT with interrupt disabled
    // __asm { HLT };

    VMMUpdateGuestState();
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

