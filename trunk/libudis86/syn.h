/* -----------------------------------------------------------------------------
 * syn.h
 *
 * Copyright (c) 2006, Vivek Mohan <vivek@sig9.com>
 * All rights reserved. See LICENSE
 * -----------------------------------------------------------------------------
 */
#ifndef UD_SYN_H
#define UD_SYN_H

/* #include <stdio.h> */
#include "vmmstring.h"
#include <stdarg.h>
#include "ltypes.h"

extern const char* ud_reg_tab[];

static void mkasm(struct ud* u, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  u->insn_fill += vmm_vsnprintf((char*) u->insn_buffer + u->insn_fill, 64, fmt, ap);
  va_end(ap);
}

#endif
