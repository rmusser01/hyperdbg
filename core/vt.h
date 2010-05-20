#ifndef _VT_H
#define _VT_H

/* FIXME: remove this dependency */
#include "vmx.h"

#include "types.h"
#include "msr.h"
#include "idt.h"

typedef struct CPU_CONTEXT {
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
};

typedef struct HVM_X86_OPS {
  /* VT-related */
  hvm_bool    (*vt_cpu_has_support)(void);
  hvm_bool    (*vt_disabled_by_bios)(void);
  hvm_bool    (*vt_enabled)(void);
  hvm_status  (*vt_initialize)(void (*idt_initializer)(PIDT_ENTRY));
  hvm_status  (*vt_finalize)(void);
  void        (*vt_launch)(void);
  hvm_status  (*vt_hardware_enable)(void);
  hvm_status  (*vt_hardware_disable)(void);
  void        (*vt_hypercall)(Bit32u num);
  hvm_status  (*vt_vmcs_initialize)(hvm_address guest_stack, hvm_address guest_return);
  Bit32u      (*vt_vmcs_read)(Bit32u encoding);
  void        (*vt_vmcs_write)(Bit32u encoding, Bit32u value);

  /* Memory management */
  void (*mmu_tlb_flush)(void);

  /* HVM-related */
  void       (*hvm_handle_exit)(void);
  hvm_status (*hvm_switch_off)(void);
  hvm_status (*hvm_update_events)(void);
};

extern struct CPU_CONTEXT context;
extern struct HVM_X86_OPS hvm_x86_ops;

#endif	/* _VT_H */
