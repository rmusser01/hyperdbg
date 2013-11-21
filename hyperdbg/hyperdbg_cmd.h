/*
  Copyright notice
  ================
  
  Copyright (C) 2010 - 2013
      Lorenzo  Martignoni <martignlo@gmail.com>
      Roberto  Paleari    <roberto.paleari@gmail.com>
      Aristide Fattori    <joystick@security.di.unimi.it>
      Mattia   Pagnozzi   <pago@security.di.unimi.it>
  
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

#ifndef _HYPERDBG_CMD_H
#define _HYPERDBG_CMD_H

#include "hyperdbg.h"

/* ################ */
/* #### MACROS #### */
/* ################ */

#define HYPERDBG_CMD_CHAR_SW_BP          'b'
#define HYPERDBG_CMD_CHAR_SW_PERM_BP     'B'
#define HYPERDBG_CMD_CHAR_CONTINUE       'c'
#define HYPERDBG_CMD_CHAR_DISAS          'd'
#define HYPERDBG_CMD_CHAR_DELETE_SW_BP   'D'
#define HYPERDBG_CMD_CHAR_LIST_BP        'L'
#define HYPERDBG_CMD_CHAR_HELP           'h'
#define HYPERDBG_CMD_CHAR_INFO           'i'
#define HYPERDBG_CMD_CHAR_SYMBOL_NEAREST 'n'
#define HYPERDBG_CMD_CHAR_SHOWMODULES    'm'
#define HYPERDBG_CMD_CHAR_SHOWPROCESSES  'p'
#define HYPERDBG_CMD_CHAR_SHOWREGISTERS  'r'
#define HYPERDBG_CMD_CHAR_SHOWSOCKETS    'w'
#define HYPERDBG_CMD_CHAR_SINGLESTEP     's'
#define HYPERDBG_CMD_CHAR_BACKTRACE      't'
#define HYPERDBG_CMD_CHAR_SYMBOL         'S'
#define HYPERDBG_CMD_CHAR_DUMPMEMORY     'x'
#define HYPERDBG_CMD_CHAR_UNLINK_PROC    'f'
#define HYPERDBG_CMD_CHAR_RELINK_PROC    'u'

#define CHAR_IS_SPACE(c) (c==' ' || c=='\t' || c=='\f')

hvm_bool HyperDbgProcessCommand(Bit8u *cmd);

#endif	/* _HYPERDBG_CMD_H */
