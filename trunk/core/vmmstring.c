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

#include "types.h"
#include "vmmstring.h"

/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

static unsigned char vmm_chartohex(char c);
static Bit32u vmm_power(Bit32u base, Bit32u exp);

/* ################ */
/* #### BODIES #### */
/* ################ */

hvm_bool vmm_islower(char c)
{
  return (c >= 'a' && c <= 'z');
}

hvm_bool vmm_isupper(char c)
{
  return (c >= 'A' && c <= 'Z');
}

hvm_bool vmm_isdigit(char c)
{
  return (c >= '0' && c <= '9');
}

hvm_bool vmm_isxdigit(char c)
{
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

hvm_bool vmm_isalpha(char c)
{
  return (vmm_islower(c) || vmm_isupper(c));
}

unsigned char vmm_tolower(unsigned char c)
{
  if (!vmm_isupper(c))
    return c;

  return (c - 'A' + 'a');
}

unsigned char vmm_toupper(unsigned char c)
{
  if (!vmm_islower(c))
    return c;

  return (c - 'a' + 'A');
}

void vmm_memset(void *s, int c, Bit32u n)
{
  unsigned char *p;
  Bit32u i;

  p = (unsigned char*) s;
  for (i=0; i<n; i++) {
    p[i] = (unsigned char) c;
  }
}

Bit32s vmm_memcmp(void *s1, void *s2, Bit32u n)
{
  Bit32u i;
  unsigned char *p1 = (unsigned char*) s1;
  unsigned char *p2 = (unsigned char*) s2;
  
  i = 0;
  while(i < n) {
    if(p1[i] != p2[i]) break;
    i++;
  }

  if(i == n)
    return 0;
  else {
    if(p1[i] < p2[i])
      return -1;
    else
      return 1;
  }
}

void *vmm_memcpy(void *dst, void *src, Bit32u n)
{
  Bit32u i = 0;
  unsigned char *p1 = (unsigned char*) dst;
  unsigned char *p2 = (unsigned char*) src;

  while (i < n) {
    p1[i] = p2[i];
    i++;
  }
  return p1;

}

Bit32s vmm_strncmpi(unsigned char *str1, unsigned char *str2, Bit32u n)
{
  Bit32u i;

  i = 0;
  while(i < n && str1[i] != 0 && str2[i] != 0) {
    if (vmm_tolower(str1[i]) != vmm_tolower(str2[i]))
      break;
    i++;
  }
  /* Strings match */
  if(i == n)
    return 0;
  else {
    /* str1 is "less than" str2 */
    if(vmm_tolower(str1[i]) < vmm_tolower(str2[i]))
      return -1;
    else   /* str1 is "more than" str2 */
      return 1;
  }
}

Bit32s vmm_strncmp(unsigned char *str1, unsigned char *str2, Bit32u n)
{
  Bit32u i;

  i = 0;
  while (i < n && str1[i] != 0 && str2[i] != 0) {
    if (str1[i] != str2[i]) break;
    i++;
  }
  /* Strings match */
  if(i == n)
    return 0;
  else {
    /* str1 is "less than" str2 */
    if(vmm_tolower(str1[i]) < vmm_tolower(str2[i]))
      return -1;
    else   /* str1 is "more than" str2 */
      return 1;
  }

}

unsigned char *vmm_strncpy(unsigned char *dst, unsigned char *src, Bit32u n)
{
  Bit32u i;

  i = 0;
  while (i < n && src[i] != 0) {
    dst[i] = src[i];
    i++;
  }
  return dst;
}

unsigned char *vmm_strncat(unsigned char *dst, unsigned char *src, Bit32u n)
{
  Bit32u i = 0, len = vmm_strlen(dst);
  while(i < n && src[i] != 0) {
    dst[len + i] = src[i];
    i++;
  }
  dst[len + i] = 0;
  return dst;
}


Bit32u vmm_strlen(unsigned char *str)
{
  Bit32u i;
  i = 0;
  while(str[i] != 0x00) i++;
  return i;
}

hvm_bool vmm_strtoul(char *str, Bit32u* result)
{
  Bit32u i, len;
  unsigned char tmp;
  len = 0;
  *result = 0;
  if (str[0] == '0' && str[1] == 'x')
    str = &str[2];

  /* Get the length of the string */
  while(str[len] != 0) len++;
  
  for(i = 0; str[i] != 0; i++) {
    tmp = vmm_chartohex(str[len-1-i]);
    if (tmp > 15) 
      return FALSE;

    *result = *result + tmp * vmm_power(16, i);
  }

  return TRUE;
}

static Bit32u vmm_power(Bit32u base, Bit32u exp)
{
  if (exp == 0) return (Bit32u) 1;
  else return (Bit32u) (base * vmm_power(base, exp-1));
}

static unsigned char vmm_chartohex(char c)
{
  switch(c) {
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    return c - '0';
  case 'a':
  case 'b':
  case 'c':
  case 'd':
  case 'e':
  case 'f':
    return c - 'a' + 10;
  case 'A':
  case 'B':
  case 'C':
  case 'D':
  case 'E':
  case 'F':
    return c - 'A' + 10;
  default:
    return -1;
  }
}
