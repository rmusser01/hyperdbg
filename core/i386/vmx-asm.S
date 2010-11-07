// Copyright notice
// ================
// 
// Copyright (C) 2010
//     Lorenzo  Martignoni <martignlo@gmail.com>
//     Roberto  Paleari    <roberto.paleari@gmail.com>
//     Aristide Fattori    <joystick@security.dico.unimi.it>
// 
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
// 
// HyperDbg is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY// without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <http://www.gnu.org/licenses/>.
// 


// MACROS

.macro vmx_call
	.byte	0x0F, 0x01, 0xC1
.endm
	
.macro vmx_launch
	.byte	0x0F, 0x01, 0xC2
.endm

.macro vmx_resume
	.byte	0x0F, 0x01, 0xC3
.endm

.macro vmx_off
	.byte	0x0F, 0x01, 0xC4
.endm

.macro vmx_ptrld
	.byte	0x0F, 0xC7
.endm
	
.macro vmx_read
	.byte	0x0F, 0x78
.endm

.macro vmx_write
	.byte	0x0F, 0x79
.endm

.macro vmx_on
	.byte	0xF3, 0x0F, 0xC7
.endm

.macro vmx_clear
	.byte	0x66, 0x0F, 0xC7
.endm
	
.macro MODRM_EBX_EAX
	.byte	0xC3
.endm

.macro MODRM_EAX_MESP
	.byte	0x04, 0x24
.endm

.macro MODRM_MESP
	.byte	0x34, 0x24
.endm

// **********************************************************************
	
// GCC on windows requires underscores
.data
str:	.asciz	"REG: %8x\n"

.text
.globl _VmxLaunch, _VmxTurnOn, _VmxClear, _VmxPtrld, _VmxResume, _VmxTurnOff, _VmxRead, _VmxWrite, _VmxVmCall
.globl _VmxHvmHandleExit, _VmxUpdateGuestContext, _DoStartVT, _NullIDTHandler
	
.globl VmxLaunch, VmxTurnOn, VmxClear, VmxPtrld, VmxResume, VmxTurnOff, VmxRead, VmxWrite, VmxVmCall
.globl VmxHvmHandleExit, VmxUpdateGuestContext, DoStartVT, NullIDTHandler

#include "../asm-offset.h"

VmxLaunch:
_VmxLaunch:
	vmx_launch
	ret

VmxTurnOn:	
_VmxTurnOn:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	0x8(%ebp)	/* _phyvmxonhigh */
	pushl	0xc(%ebp)	/* _phyvmxonlow */
	vmx_on
	MODRM_MESP
	/*
	Save %eflags in %eax, to return to the caller the effect of vmxon on
	control flags
	*/
	pushfl
	popl	%eax
	addl	$8, %esp
	leave
	ret

VmxClear:
_VmxClear:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	0x8(%ebp)	/* _phyvmxonhigh */
	pushl	0xc(%ebp)	/* _phyvmxonlow */
	vmx_clear
	MODRM_MESP
	/*
	Save %eflags in %eax, to return to the caller the effect of vmclear on
	control flags
	*/
	pushfl
	popl	%eax
	addl	$8,%esp
	leave
	ret

VmxPtrld:
_VmxPtrld:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	0x8(%ebp)	/* _phyvmxonhigh */
	pushl	0xc(%ebp)       /* _phyvmxonlow */
	vmx_ptrld
	MODRM_MESP
	/*
	Save %eflags in %eax, to return to the caller the effect of vmptrld on
	control flags
	*/
	pushfl
	popl	%eax
	addl	$8, %esp
	leave	
	ret

VmxResume:
_VmxResume:
	vmx_resume
	ret

VmxTurnOff:
_VmxTurnOff:
	vmx_off	
	ret

VmxRead:	
_VmxRead:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	movl	0x8(%ebp),%eax	/* _field */
	vmx_read
	MODRM_EBX_EAX  /* Read value stored in %eax */
	movl	%ebx,%eax
	popl	%ebx
	leave
	ret

VmxWrite:
_VmxWrite:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%eax
	pushl	0xc(%ebp)	/* _value */
	movl	0x8(%ebp),%eax	/* _field */
	vmx_write
	MODRM_EAX_MESP  /* Read value stored in ecx */
	popl	%eax
	popl	%eax
	leave
	ret

VmxVmCall:
_VmxVmCall:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%eax
	movl	0x8(%ebp),%eax	/* _HypercallNumber */
	vmx_call
	popl	%eax
	leave
	ret

VmxHvmHandleExit:	
_VmxHvmHandleExit:
	pushfl

	pushl	%ebx
	lea	(_context),%ebx
	movl	%eax, CONTEXT_rax(%ebx)
	popl	%ebx	

	pushl	%eax
	lea 	(_context),%eax
	movl	%ebx, CONTEXT_rbx(%eax)
	movl	%ecx, CONTEXT_rcx(%eax)
	movl	%edx, CONTEXT_rdx(%eax)
	movl	%edi, CONTEXT_rdi(%eax)
	movl	%esi, CONTEXT_rsi(%eax)
	movl	%ebp, CONTEXT_rbp(%eax)
	popl	%eax

	movl	%esp,%ebp

	call	_VmxHvmInternalHandleExit
	call    _VmxUpdateGuestContext
	popfl
	call	_VmxResume
	ret
	
// Updates CPU state with the values from the VMCS cache structure. 
// NOTE: we update only those registers that are not already present in the
// (hardware) VMCS.
//
VmxUpdateGuestContext:
_VmxUpdateGuestContext:
	pushl	%ebx
	lea 	(_context), %ebx
	pushl	CONTEXT_rflags(%ebx)
	pushl	$0x6820			/* GUEST_RFLAGS */
	call	_VmxVmcsWrite
	addl	$8, %esp
	
	/* Set next instruction to be executed */
	lea 	(_context), %ebx
	pushl	CONTEXT_resumerip(%ebx)
	pushl	$0x681e 	/* _GUEST_RIP */
	call	_VmxVmcsWrite
	addl	$8, %esp

	
	lea 	(_context), %ebx
	movl	CONTEXT_rax(%ebx), %eax
	popl	%ebx
	
	pushl	%eax
	lea	(_context), %eax
	movl	CONTEXT_rbx(%eax), %ebx
	movl	CONTEXT_rcx(%eax), %ecx
	movl	CONTEXT_rdx(%eax), %edx
	movl	CONTEXT_rdi(%eax), %edi
	movl	CONTEXT_rsi(%eax), %esi
	movl	CONTEXT_rbp(%eax), %ebp

	popl	%eax
	ret

DoStartVT:	
_DoStartVT:
	cli
	movl	%esp, _GuestStack

	pushal
	popl	_EntryRDI
	popl	_EntryRSI
	popl	_EntryRBP
	popl	_EntryRSP
	popl	_EntryRBX
	popl	_EntryRDX
	popl	_EntryRCX
	popl	_EntryRAX
	pushfl
	popl	_EntryRFlags

	call	_StartVT

	pushl	_EntryRFlags
	popfl
	pushl	_EntryRAX
	pushl	_EntryRCX
	pushl	_EntryRDX
	pushl	_EntryRBX
	pushl	_EntryRSP
	pushl	_EntryRBP
	pushl	_EntryRSI
	pushl	_EntryRDI
	popal

	sti
	movl	_GuestStack,%esp
	ret

NullIDTHandler:	
_NullIDTHandler:
	iret
