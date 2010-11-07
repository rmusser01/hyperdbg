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

#ifndef _TYPES_H
#define _TYPES_H

#if defined(WIN32)
/* Windows */
#include <inttypes.h>
#include <stddef.h>

typedef unsigned char     Bit8u;
typedef   signed char     Bit8s;
typedef unsigned short    Bit16u;
typedef   signed short    Bit16s;
typedef unsigned int      Bit32u;
typedef   signed int      Bit32s;
typedef unsigned __int64  Bit64u;
typedef   signed __int64  Bit64s;

#else
/* Linux */
#include <linux/types.h>

typedef unsigned           char   Bit8u;
typedef   signed           char   Bit8s;
typedef unsigned short     int    Bit16u;
typedef signed short       int    Bit16s;
typedef unsigned           int    Bit32u;
typedef   signed           int    Bit32s;
typedef unsigned long long int    Bit64u;
typedef   signed long long int    Bit64s;

#endif

#define GET32L(val64) ((Bit32u)(((Bit64u)(val64)) & 0xFFFFFFFF))
#define GET32H(val64) ((Bit32u)(((Bit64u)(val64)) >> 32))

#if HVM_ARCH_BITS == 64
typedef Bit64u hvm_address;
#else
typedef Bit32u hvm_address;
#endif

typedef Bit64u hvm_phy_address;

#ifdef GUEST_LINUX
typedef struct _PHYSICAL_ADDRESS 
{
  struct {
    Bit32u LowPart;
    Bit64s HighPart;
  } u;
} PHYSICAL_ADDRESS;
#endif

typedef Bit32u hvm_bool;
#define TRUE  1
#define FALSE 0

/* Status values */
typedef Bit32u hvm_status;
#define HVM_STATUS_SUCCESS           0x00000000
#define HVM_STATUS_UNSUCCESSFUL      0xC0000001
#define HVM_STATUS_INVALID_PARAMETER 0xC000000D
#define HVM_STATUS_END_OF_FILE       0xC0000011
#define HVM_SUCCESS(x) ((x) == HVM_STATUS_SUCCESS)

/* Common types */
#ifndef NULL
#define NULL 0
#endif

#endif	/* _TYPES_H */
