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

.CODE

IoReadPortByte PROC StdCall __portno
	push edx

	mov edx, __portno
	in  al, dx
	
	pop edx
	ret
IoReadPortByte ENDP

IoReadPortWord PROC StdCall __portno
	push edx
	
	mov edx, __portno
	in  ax, dx
	
	pop edx
	ret
IoReadPortWord ENDP

IoReadPortDword PROC StdCall __portno
	push edx
	
	mov edx, __portno
	in  eax, dx
	
	pop edx
	ret
IoReadPortDword ENDP

IoWritePortByte PROC StdCall __portno, v
	push edx

	mov edx, __portno
	mov eax, v

	out dx, al
	
	pop edx
	ret
IoWritePortByte ENDP

IoWritePortWord PROC StdCall __portno, v
	push edx

	mov edx, __portno
	mov eax, v

	out dx, ax
	
	pop edx
	ret
IoWritePortWord ENDP

IoWritePortDword PROC StdCall __portno, v
	push edx

	mov edx, __portno
	mov eax, v

	out dx, eax
	
	pop edx
	ret
IoWritePortDword ENDP	
	
END