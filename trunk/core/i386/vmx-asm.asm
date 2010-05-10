; Copyright notice
; ================
; 
; Copyright (C) 2010
;     Lorenzo  Martignoni <martignlo@gmail.com>
;     Roberto  Paleari    <roberto.paleari@gmail.com>
;     Aristide Fattori    <joystick@security.dico.unimi.it>
; 
; This program is free software: you can redistribute it and/or modify it under
; the terms of the GNU General Public License as published by the Free Software
; Foundation, either version 3 of the License, or (at your option) any later
; version.
; 
; HyperDbg is distributed in the hope that it will be useful, but WITHOUT ANY
; WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
; A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
; 
; You should have received a copy of the GNU General Public License along with
; this program. If not, see <http://www.gnu.org/licenses/>.
; 

.686p
.model flat,StdCall
option casemap:none

; MACROS

vmx_call MACRO
	BYTE	0Fh, 01h, 0C1h
ENDM
	
vmx_launch MACRO
	BYTE	0Fh, 01h, 0C2h
ENDM

vmx_resume MACRO
	BYTE	0Fh, 01h, 0C3h
ENDM

vmx_off MACRO
	BYTE	0Fh, 01h, 0C4h
ENDM

vmx_ptrld MACRO
	BYTE	0Fh, 0C7h
ENDM
	
vmx_read MACRO
	BYTE	0Fh, 078h
ENDM

vmx_write MACRO
	BYTE	0Fh, 079h
ENDM

vmx_on MACRO
	BYTE	0F3h, 0Fh, 0C7h
ENDM

vmx_clear MACRO
	BYTE	066h, 0Fh, 0C7h
ENDM
	
MODRM_EBX_EAX MACRO  ;/* EBX, EAX */
	BYTE	0C3h
ENDM

MODRM_EAX_MESP MACRO  ;/* EAX, [ESP] */
	BYTE	04h, 024h
ENDM

MODRM_MESP MACRO  ;/* [ESP] */
	BYTE	034h, 024h
ENDM
	
; ##################################################

.CODE
	
; PROCEDURES
	
VmxLaunch PROC	
	vmx_launch
	ret
VmxLaunch ENDP

VmxTurnOn PROC StdCall _phyvmxonhigh, _phyvmxonlow
	push _phyvmxonhigh
	push _phyvmxonlow
	
	vmx_on
	MODRM_MESP

	;; save eflags in eax, to return to the caller the effect of vmxon on
	;; control flags
	pushfd
	pop eax
	
	add esp, 8
	ret
VmxTurnOn ENDP

VmxClear PROC StdCall _phyvmxonhigh, _phyvmxonlow
	push _phyvmxonhigh
	push _phyvmxonlow
	
	vmx_clear
	MODRM_MESP

	;; save eflags in eax, to return to the caller the effect of vmclear on
	;; control flags
	pushfd
	pop eax
	
	add esp, 8	
	ret
VmxClear ENDP

VmxPtrld PROC StdCall _phyvmxonhigh, _phyvmxonlow
	push _phyvmxonhigh
	push _phyvmxonlow
	
	vmx_ptrld
	MODRM_MESP

	;; save eflags in eax, to return to the caller the effect of vmptrld on
	;; control flags
	pushfd
	pop eax
	
	add esp, 8	
	ret
VmxPtrld ENDP
	
VmxResume PROC
	vmx_resume
	ret
VmxResume ENDP

VmxTurnOff PROC
	vmx_off	
	ret
VmxTurnOff ENDP
	
; vmxRead(field)
VmxRead PROC StdCall _field
	push ebx
	
	mov eax, _field
	vmx_read
	MODRM_EBX_EAX  ; read value stored in eax

	mov eax, ebx
	pop ebx
	ret
VmxRead ENDP

VmxWrite PROC StdCall _field, _value
	push _value
	mov eax, _field
	
	vmx_write
	MODRM_EAX_MESP  ;read value stored in ecx

	pop eax
	
	ret
VmxWrite ENDP	

VmxVmCall PROC StdCall _HypercallNumber
	push eax
	
	mov eax, _HypercallNumber
	vmx_call

	pop eax
	ret
VmxVmCall ENDP
	
END