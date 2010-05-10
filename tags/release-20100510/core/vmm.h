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

#include <ntddk.h>

#include "vmx.h"
#include "events.h"

typedef struct {
  /* Exit state */
  struct {
    ULONG ExitReason;
    ULONG ExitQualification;
    ULONG ExitInterruptionInformation;
    ULONG ExitInterruptionErrorCode;
    ULONG ExitInstructionLength;
    ULONG ExitInstructionInformation;

    ULONG IDTVectoringInformationField;
    ULONG IDTVectoringErrorCode;
  } ExitState;

  /* Guest state */
  struct {
    ULONG EIP;
    ULONG ResumeEIP;
    ULONG ESP;
    ULONG CS;
    ULONG CR0;
    ULONG CR3;
    ULONG CR4;
    ULONG EFLAGS;

    ULONG EAX;
    ULONG EBX;
    ULONG ECX;
    ULONG EDX;
    ULONG EDI;
    ULONG ESI;
    ULONG EBP;
  } GuestState;
} VMCS_CACHE;

extern VMCS_CACHE vmcs;

/* VMM entry point */
VOID VMMEntryPoint(VOID);

/* The hypercall invoked when we want to turn off VMX. This is registered by
   pill.c. */
EVENT_PUBLISH_STATUS HypercallSwitchOff(VOID);

/* VM introspection functions */
void VMMReadGuestMemory(ULONG32 addr, int size, UCHAR *buf);

#endif /* _PILL_VMM_H */
