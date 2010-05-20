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

#ifndef _PILL_VMM_H
#define _PILL_VMM_H

#include "types.h"
#include "vmx.h"
#include "events.h"

typedef struct {
  struct {
    Bit32u ExitReason;
    Bit32u ExitQualification;
    Bit32u ExitInterruptionInformation;
    Bit32u ExitInterruptionErrorCode;
    Bit32u ExitInstructionLength;
    Bit32u ExitInstructionInformation;

    Bit32u IDTVectoringInformationField;
    Bit32u IDTVectoringErrorCode;
  } ExitContext;

  struct {
    hvm_address RIP;
    hvm_address ResumeRIP;
    hvm_address RSP;
    hvm_address CS;
    hvm_address CR0;
    hvm_address CR3;
    hvm_address CR4;
    hvm_address RFLAGS;

    hvm_address RAX;
    hvm_address RBX;
    hvm_address RCX;
    hvm_address RDX;
    hvm_address RDI;
    hvm_address RSI;
    hvm_address RBP;
  } GuestContext;
} CPU_CONTEXT;

extern CPU_CONTEXT context;

/* VMM entry point */
void VMMEntryPoint(void);

/* The hypercall invoked when we want to turn off VMX. This is registered by
   pill.c. */
EVENT_PUBLISH_STATUS HypercallSwitchOff(void);

/* VM introspection functions */
void VMMReadGuestMemory(hvm_address addr, int size, Bit8u *buf);

#endif /* _PILL_VMM_H */
