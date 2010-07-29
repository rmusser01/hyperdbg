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

#include "types.h"
#include <stdarg.h>

unsigned char* vmm_strncat(unsigned char *dst, unsigned char *src, Bit32u n);
unsigned char* vmm_strncpy(unsigned char *dst, unsigned char *src, Bit32u n);
Bit32s         vmm_strncmp(unsigned char *str1, unsigned char *str2, Bit32u n);
Bit32s         vmm_strncmpi(unsigned char *str1, unsigned char *str2, Bit32u n);
Bit32u         vmm_strlen(unsigned char *str);
hvm_bool       vmm_strtoul(char *str, Bit32u *out);
void           vmm_memset(void *s, int c, Bit32u n);

int            vmm_snprintf(char*, size_t, const char*, ...);
int            vmm_vsnprintf(char*, size_t, const char*, va_list);

unsigned char  vmm_tolower(unsigned char c);
unsigned char  vmm_toupper(unsigned char c);

hvm_bool       vmm_islower(char c);
hvm_bool       vmm_isupper(char c);
hvm_bool       vmm_isalpha(char c);
hvm_bool       vmm_isdigit(char c);
hvm_bool       vmm_isxdigit(char c);
hvm_bool       vmm_isascii(char c);

#endif /* _STRING_H */
