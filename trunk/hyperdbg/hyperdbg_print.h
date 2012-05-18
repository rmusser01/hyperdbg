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

#ifndef _HYPERDBG_PRINT_H
#define _HYPERDBG_PRINT_H

#include "hyperdbg.h"
#include "network.h"
#include "process.h"
#include "syms.h"
#include "vt.h" /* context */
#include "hyperdbg_common.h"

/* ################ */
/* #### MACROS #### */
/* ################ */

/* Error Codes */
#define ERROR_MISSING_PARAM    -1
#define ERROR_INVALID_SIZE     -2
#define ERROR_INVALID_REGISTER -3
#define ERROR_INVALID_ADDR     -4
#define ERROR_INVALID_MEMORY   -5
#define ERROR_INVALID_SYMBOL   -6

/* Cmd Specific Errors */
#define ERROR_COMMAND_SPECIFIC -100
#define ERROR_DOUBLED_BP       -101
#define ERROR_NOSUCH_BP        -102

//#define MAXSWBPS 100

#ifdef GUEST_WINDOWS
#include "winxp.h"
#endif

typedef struct {
  hvm_address rax;
  hvm_address rbx;
  hvm_address rcx;
  hvm_address rdx;
  hvm_address rsp;
  hvm_address rbp;
  hvm_address rsi;
  hvm_address rdi;
  hvm_address rip;
  hvm_address resumerip;
  hvm_address rflags;
  hvm_address cr0;
  hvm_address cr3;
  hvm_address cr4;
  hvm_address cs;
} REGISTERS, *PREGISTERS;

typedef struct {
  hvm_address baseaddr;
  hvm_bool with_char_rep;
  Bit32u data[512];
} MEMORYDUMP, *PMEMORYDUMP;

typedef struct {
  PSYMBOL symbol;
  Bit32u bp_index;
  hvm_address addr;
  Bit32s error_code;
  hvm_bool isPerm;
  hvm_bool isCr3Dipendent;
} BPINFO, *PBINFO;

typedef struct {
  hvm_address addr;
  char hexcode[32];
  char asmcode[64];
  char sym_name[64];
  hvm_bool sym_exists;
} INSTRUCTION_DATA, *PINSTRUCTION_DATA;

typedef struct {
  hvm_address Addr;
  hvm_address cr3;
  hvm_bool isPerm;
  hvm_bool isCr3Dipendent;
} BPLIST, *PBLIST;

typedef struct {
  union {
    REGISTERS registers;
    PROCESS_DATA processes[128];
    MODULE_DATA modules[128];
    SOCKET sockets[128];
    MEMORYDUMP memory_dump;
    BPINFO bpinfo;
    BPLIST bplist[MAXSWBPS];
    INSTRUCTION_DATA instructions[128];
  };
} CMD_RESULT, *PCMD_RESULT;

/* #################### */
/* #### PROTOTYPES #### */
/* #################### */

void PrintHelp(void);
void PrintRegisters(PCMD_RESULT buffer);
void PrintProcesses(PCMD_RESULT buffer, Bit32s size);
void PrintModules(PCMD_RESULT buffer, Bit32s size);
void PrintSockets(PCMD_RESULT buffer, Bit32s size);
void PrintMemoryDump(PCMD_RESULT buffer, Bit32s size);
void PrintSwBreakpoint(PCMD_RESULT buffer);
void PrintDeleteSwBreakpoint(PCMD_RESULT buffer);
void PrintBPList(PCMD_RESULT buffer);
void PrintDisassembled(PCMD_RESULT buffer, Bit32s size);
void PrintInfo(void);
void PrintUnlinkProc(Bit32s error_code);
void PrintRelinkProc(Bit32s error_code);
void PrintUnknown(void);

#endif	/* _HYPERDBG_PRINT_H */
