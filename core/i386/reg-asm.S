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
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <http://www.gnu.org/licenses/>.
// 
	
.text
.globl  _RegGetFlags, _RegSetFlags, _RegGetCr0, _RegSetCr0, _RegGetCr2, _RegSetCr2, _RegGetCr3, _RegGetCr4, _RegSetCr4
.globl	_ReadMSR, _WriteMSR, _RegGetCs, _RegGetDs, _RegGetEs, _RegGetFs, _RegGetGs, _RegGetSs, _RegGetTr, _RegGetLdtr
.globl	_RegSetIdtr, _RegRdtsc

.globl  RegGetFlags, RegSetFlags, RegGetCr0, RegSetCr0, RegGetCr2, RegSetCr2, RegGetCr3, RegGetCr4, RegSetCr4
.globl	ReadMSR, WriteMSR, RegGetCs, RegGetDs, RegGetEs, RegGetFs, RegGetGs, RegGetSs, RegGetTr, RegGetLdtr
.globl	RegSetIdtr, RegRdtsc


RegGetFlags:
_RegGetFlags:
	pushfl
	popl	%eax
	ret

RegSetFlags:
_RegSetFlags:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%eax
	movl	0x8(%ebp),%eax	/* _flags */
	pushl	%eax
	popfl
	popl	%eax
	leave
	ret

RegGetCr0:
_RegGetCr0:
	movl	%cr0,%eax
	ret

RegSetCr0:
_RegSetCr0:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%eax
	movl	0x8(%ebp),%eax	/* _CR0 */
	movl	%eax, %cr0
	popl	%eax
	leave
	ret

RegGetCr2:
_RegGetCr2:
	movl	%cr2,%eax
	ret

RegSetCr2:
_RegSetCr2:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%eax
	movl	0x8(%ebp),%eax	/* _CR2 */
	movl	%eax,%cr2
	popl	%eax
	leave
	ret
	
RegGetCr3:
_RegGetCr3:
	movl	%cr3,%eax
	ret

RegGetCr4:
_RegGetCr4:
	movl	%cr4,%eax
	ret

RegSetCr4:
_RegSetCr4:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%eax
	movl	0x8(%ebp),%eax	/* _CR4 */
	movl	%eax,%cr4
	popl	%eax
	leave
	ret


// Writes the contents of registers EDX:EAX into the 64-bit model specific
// register (MSR) specified in the ECX register. The contents of the EDX
// register are copied to high-order 32 bits of the selected MSR and the
// contents of the EAX register are copied to low-order 32 bits of the MSR.
//	msr.Hi <-- EDX
//	msr.Lo <-- EAX
//
//
WriteMSR:	
_WriteMSR:
	pushl	%ebp
	movl	%esp,%ebp
	pushal
	movl	0x8(%ebp),%ecx	/* encoding */
	movl	0xc(%ebp),%edx	/* _highpart */
	movl	0x10(%ebp),%eax	/* lowpart */
	wrmsr
	popal
	leave
	ret

RegGetCs:
_RegGetCs:
	movw	%cs,%ax
	ret

RegGetDs:
_RegGetDs:
	movw	%ds,%ax
	ret

RegGetEs:
_RegGetEs:
	movw	%es,%ax
	ret

RegGetFs:
_RegGetFs:
	movw	%fs,%ax
	ret

RegGetGs:
_RegGetGs:
	movw	%gs,%ax
	ret

RegGetSs:
_RegGetSs:
	movw	%ss,%ax
	ret

RegGetTr:
_RegGetTr:
	str	%ax
	ret

RegGetLdtr:
_RegGetLdtr:
	sldt	%eax
	ret

	
// Loads the contents of a 64-bit model specific register (MSR) specified
// in the ECX register into registers EDX:EAX. The EDX register is loaded
// with the high-order 32 bits of the MSR and the EAX register is loaded
// with the low-order 32 bits.
//	msr.Hi --> EDX
//	msr.Lo --> EAX
//
ReadMSR:
_ReadMSR:
	pushl	%ebp
	movl	%esp,%ebp
	pushal
	
	movl	0x8(%ebp),%ecx	/* _reg */
	rdmsr	                /* MSR[ecx] --> edx:eax */
	movl	0xc(%ebp),%ecx	/* _msr */
	movl	%eax,(%ecx)
	movl	%edx,4(%ecx)

	popal
	leave
	ret
		
RegSetIdtr:
_RegSetIdtr:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%eax
	pushl	0x8(%ebp)	/* _base */
	shll	$16,0xc(%ebp)	/* _limit */
	pushl	0xc(%ebp)	/* _limit */
	/* FIXME */
	lidt	0x2(%esp)	/* fword ptr [esp+2] */
	popl	%eax
	popl	%eax
	popl	%eax
	leave
	ret

RegRdtsc:
_RegRdtsc:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%eax
	pushl	%edx
	rdtsc
	movl	0x8(%ebp),%ecx	/* _out */
	movl	%eax,(%ecx)
	movl	%edx,4(%ecx)
	popl	%edx
	popl	%eax
	leave
	ret
