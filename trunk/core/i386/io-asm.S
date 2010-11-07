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
// m version.
// 
// HyperDbg is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <http://www.gnu.org/licenses/>.
// 
	
.text
.globl _IoReadPortByte, _IoReadPortWord, _IoReadPortDword, _IoWritePortByte, _IoWritePortWord, _IoWritePortDword
.globl IoReadPortByte, IoReadPortWord, IoReadPortDword, IoWritePortByte, IoWritePortWord, IoWritePortDword

IoReadPortByte:	
_IoReadPortByte:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%edx
	movl	0x8(%ebp),%edx	/* __portno */
	in	%dx,%al	
	popl	%edx
	leave
	ret

IoReadPortWord:
_IoReadPortWord:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%edx
	movl	0x8(%ebp),%edx	/* __portno */
	in 	%dx,%ax
	popl	%edx
	leave
	ret

IoReadPortDword:
_IoReadPortDword:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%edx
	movl	0x8(%ebp),%edx	/* __portno */
	in	%dx,%eax
	popl	%edx
	leave
	ret

IoWritePortByte:
_IoWritePortByte:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%edx
	pushl	%eax
	movl	0x8(%ebp),%edx	/* __portno */
	movl	0xc(%ebp),%eax	/* v */
	out	%al,%dx
	popl	%eax
	popl	%edx
	leave
	ret

IoWritePortWord:
_IoWritePortWord:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%edx
	pushl	%eax
	movl	0x8(%ebp),%edx	/* __portno */
	movl	0xc(%ebp),%eax	/* v */
	out	%ax,%dx
	popl	%eax
	popl	%edx
	leave
	ret

IoWritePortDword:
_IoWritePortDword:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%edx
	pushl	%eax
	movl	0x8(%ebp),%edx	/* __portno */
	movl	0xc(%ebp),%eax	/* v */
	out	%eax,%dx
	popl	%eax
	popl	%edx
	leave
	ret
