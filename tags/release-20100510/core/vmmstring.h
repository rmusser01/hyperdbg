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

#ifndef _STRING_H
#define _STRING_H

#include <ntddk.h>

UCHAR*  vmm_strncpy(UCHAR *dst, UCHAR *src, ULONG n);
LONG32  vmm_strncmp(UCHAR *str1, UCHAR *str2, ULONG n);
LONG32  vmm_strncmpi(UCHAR *str1, UCHAR *str2, ULONG n);
ULONG32 vmm_strlen(UCHAR *str);
BOOLEAN vmm_strtoul(char *str, PULONG out);
VOID    vmm_memset(VOID *s, int c, ULONG n);
int     vmm_snprintf(char*, size_t, const char*, ...);
int     vmm_vsnprintf(char*, size_t, const char*, va_list);
BOOLEAN vmm_islower(char c);
BOOLEAN vmm_isupper(char c);
BOOLEAN vmm_isalpha(char c);
BOOLEAN vmm_isdigit(char c);
BOOLEAN vmm_isxdigit(char c);
BOOLEAN vmm_isascii(char c);
UCHAR   vmm_tolower(UCHAR c);
UCHAR   vmm_toupper(UCHAR c);

#endif /* _STRING_H */
