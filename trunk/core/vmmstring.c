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

/* #include "string.h" */
/* #include "debug.h" */

#include <ntddk.h>
/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

static UCHAR vmm_chartohex(char c);
static ULONG vmm_power(ULONG base, ULONG exp);

/* ################ */
/* #### BODIES #### */
/* ################ */

BOOLEAN vmm_islower(char c)
{
  return (c >= 'a' && c <= 'z');
}

BOOLEAN vmm_isupper(char c)
{
  return (c >= 'A' && c <= 'Z');
}

BOOLEAN vmm_isdigit(char c)
{
  return (c >= '0' && c <= '9');
}

BOOLEAN vmm_isxdigit(char c)
{
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

BOOLEAN vmm_isascii(char c)
{
  return (c >= 0 && c <= 127);
}

BOOLEAN vmm_isalpha(char c)
{
  return (vmm_islower(c) || vmm_isupper(c));
}

UCHAR vmm_tolower(UCHAR c)
{
  if (!vmm_isupper(c))
    return c;

  return (c - 'A' + 'a');
}

UCHAR vmm_toupper(UCHAR c)
{
  if (!vmm_islower(c))
    return c;

  return (c - 'a' + 'A');
}

VOID vmm_memset(VOID *s, int c, ULONG n)
{
  UCHAR *p;
  ULONG i;

  p = (UCHAR*) s;
  for (i=0; i<n; i++) {
    p[i] = (UCHAR) c;
  }
}

LONG32 vmm_strncmpi(UCHAR *str1, UCHAR *str2, ULONG n)
{
  ULONG i;
  UCHAR c1, c2;

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

LONG32 vmm_strncmp(UCHAR *str1, UCHAR *str2, ULONG n)
{
  ULONG i;

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

ULONG32 vmm_strlen(UCHAR *str)
{
  ULONG32 i;
  i = 0;
  while(str[i] != 0x00) i++;
  return i;
}

BOOLEAN vmm_strtoul(char *str, PULONG result)
{
  ULONG32 i, len;
  UCHAR tmp;
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

static ULONG vmm_power(ULONG base, ULONG exp)
{
  if (exp == 0) return (ULONG) 1;
  else return (ULONG) (base * vmm_power(base, exp-1));
}

static UCHAR vmm_chartohex(char c)
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
