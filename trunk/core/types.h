#ifndef _TYPES_H
#define _TYPES_H

#if defined(WIN32)
/* Windows */
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
#error "TODO: Linux data types"
#endif

#define GET32L(val64) ((Bit32u)(((Bit64u)(val64)) & 0xFFFFFFFF))
#define GET32H(val64) ((Bit32u)(((Bit64u)(val64)) >> 32))

#if HVM_ARCH_BITS == 64
typedef Bit64u hvm_address;
#else
typedef Bit32u hvm_address;
#endif

typedef Bit32u hvm_bool;
#define TRUE  1
#define FALSE 0

/* Status values */
typedef Bit32u hvm_status;
#define HVM_STATUS_SUCCESS           0x00000000
#define HVM_STATUS_UNSUCCESSFUL      0xC0000001
#define HVM_STATUS_INVALID_PARAMETER 0xC000000D

/* Common types */
#ifndef NULL
#define NULL 0
#endif

#endif	/* _TYPES_H */
