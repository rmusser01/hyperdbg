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
.globl 	_CmInitSpinLock, _CmAcquireSpinLock, _CmReleaseSpinLock
.globl 	CmInitSpinLock, CmAcquireSpinLock, CmReleaseSpinLock

CmInitSpinLock:
_CmInitSpinLock:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%eax
	movl	0x8(%ebp), %eax	/* lck */
	andl	$0, (%eax)
	popl	%eax
	leave
	ret

CmAcquireSpinLock:
_CmAcquireSpinLock:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%eax
	movl	0x8(%ebp),%eax	/* lck */
do_lock:
	lock    btsl $0,(%eax)
	jc      do_lock
	popl	%eax
	leave
	ret

CmReleaseSpinLock:	
_CmReleaseSpinLock:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%eax
	movl	0x8(%ebp),%eax	/* lck */
	lock	btrl $0,(%eax)
	popl	%eax
	leave
	ret
