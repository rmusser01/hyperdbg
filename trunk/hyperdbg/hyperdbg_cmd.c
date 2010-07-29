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

#include "hyperdbg.h"
#include "hyperdbg_cmd.h"
#include "hyperdbg_common.h"
#include "vmmstring.h"
#include "debug.h"
#include "video.h"
#include "gui.h"
#include "stdlib.h"		/* FIXME: re-implement atoi() */
#include "mmu.h"
#include "sw_bp.h"
#include "x86.h"       /* Needed for the FLAGS_TF_MASK macro */
#include "vt.h"
#include "extern.h"    /* From libudis */
#include "symsearch.h"
#include "syms.h"
#include "common.h"
#include "process.h"

#ifdef GUEST_WINDOWS
#include "winxp.h"
#endif

/* ################ */
/* #### MACROS #### */
/* ################ */

#define HYPERDBG_CMD_CHAR_SW_BP          'b'
#define HYPERDBG_CMD_CHAR_CONTINUE       'c'
#define HYPERDBG_CMD_CHAR_DISAS          'd'
#define HYPERDBG_CMD_CHAR_DELETE_SW_BP   'D'
#define HYPERDBG_CMD_CHAR_HELP           'h'
#define HYPERDBG_CMD_CHAR_INFO           'i'
#define HYPERDBG_CMD_CHAR_SYMBOL_NEAREST 'n'
#define HYPERDBG_CMD_CHAR_SHOWPROCESSES  'p'
#define HYPERDBG_CMD_CHAR_SHOWREGISTERS  'r'
#define HYPERDBG_CMD_CHAR_SINGLESTEP     's'
#define HYPERDBG_CMD_CHAR_BACKTRACE      't'
#define HYPERDBG_CMD_CHAR_SYMBOL         'S'
#define HYPERDBG_CMD_CHAR_DUMPMEMORY     'x'

#define CHAR_IS_SPACE(c) (c==' ' || c=='\t' || c=='\f')

/* ############### */
/* #### TYPES #### */
/* ############### */

typedef enum {
  HYPERDBG_CMD_UNKNOWN = 0,
  HYPERDBG_CMD_HELP,
  HYPERDBG_CMD_SHOWREGISTERS,
  HYPERDBG_CMD_SHOWPROCESSES,
  HYPERDBG_CMD_DUMPMEMORY,
  HYPERDBG_CMD_SW_BP,
  HYPERDBG_CMD_DELETE_SW_BP,
  HYPERDBG_CMD_SINGLESTEP,
  HYPERDBG_CMD_BACKTRACE,
  HYPERDBG_CMD_DISAS,
  HYPERDBG_CMD_CONTINUE,
  HYPERDBG_CMD_SYMBOL,
  HYPERDBG_CMD_SYMBOL_NEAREST,
  HYPERDBG_CMD_INFO,
} HYPERDBG_OPCODE;

typedef struct {
  HYPERDBG_OPCODE opcode;
  int             nargs;
  Bit8u           args[4][128];
} HYPERDBG_CMD, *PHYPERDBG_CMD;

/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

static void CmdHelp(PHYPERDBG_CMD pcmd);
static void CmdShowRegisters(PHYPERDBG_CMD pcmd);
static void CmdShowProcesses(PHYPERDBG_CMD pcmd);
static void CmdDumpMemory(PHYPERDBG_CMD pcmd);
static void CmdSwBreakpoint(PHYPERDBG_CMD pcmd);
static void CmdDeleteSwBreakpoint(PHYPERDBG_CMD pcmd);
static void CmdSetSingleStep(PHYPERDBG_CMD pcmd);
static void CmdDisassemble(PHYPERDBG_CMD pcmd);
static void CmdBacktrace(PHYPERDBG_CMD pcmd);
static void CmdLookupSymbol(PHYPERDBG_CMD pcmd, hvm_bool bExactMatch);
static void CmdShowInfo(PHYPERDBG_CMD pcmd);
static void CmdUnknown(PHYPERDBG_CMD pcmd);

static void ParseCommand(Bit8u *buffer, PHYPERDBG_CMD pcmd);
static hvm_bool GetRegFromStr(Bit8u *name, hvm_address *value);

/* ################ */
/* #### BODIES #### */
/* ################ */

hvm_bool HyperDbgProcessCommand(Bit8u *bufcmd)
{
  HYPERDBG_CMD cmd;
  hvm_bool ExitLoop;

  /* By default, a command doesn't interrupt the command loop in hyperdbg_host */
  ExitLoop = FALSE; 
  if (vmm_strlen(bufcmd) == 0)
    return ExitLoop;

  ParseCommand(bufcmd, &cmd);

  switch (cmd.opcode) {
  case HYPERDBG_CMD_HELP:
    CmdHelp(&cmd);
    break;
  case HYPERDBG_CMD_SHOWREGISTERS:
    CmdShowRegisters(&cmd);
    break;
  case HYPERDBG_CMD_SHOWPROCESSES:
    CmdShowProcesses(&cmd);
    break;
  case HYPERDBG_CMD_DUMPMEMORY:
    CmdDumpMemory(&cmd);
    break;
  case HYPERDBG_CMD_SW_BP:
    CmdSwBreakpoint(&cmd);
    break;
  case HYPERDBG_CMD_DELETE_SW_BP:
    CmdDeleteSwBreakpoint(&cmd);
    break;
  case HYPERDBG_CMD_SINGLESTEP:
    CmdSetSingleStep(&cmd);
    /* After this command we have to return control to the guest */
    ExitLoop = TRUE; 
    break;
  case HYPERDBG_CMD_DISAS:
    CmdDisassemble(&cmd);
    break;
  case HYPERDBG_CMD_BACKTRACE:
    CmdBacktrace(&cmd);
    break;
  case HYPERDBG_CMD_SYMBOL:
    CmdLookupSymbol(&cmd, TRUE);
    break;
  case HYPERDBG_CMD_SYMBOL_NEAREST:
    CmdLookupSymbol(&cmd, FALSE);
    break;
  case HYPERDBG_CMD_CONTINUE:
    ExitLoop = TRUE;
    break;
  case HYPERDBG_CMD_INFO:
    CmdShowInfo(&cmd);
    break;
  default:
    CmdUnknown(&cmd);
    break;
  }
  return ExitLoop;
}

static void CmdHelp(PHYPERDBG_CMD pcmd)
{
  int i;

  VideoResetOutMatrix();

  i = 0;

  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "Available commands:");
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - show this help screen", HYPERDBG_CMD_CHAR_HELP);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - dump guest registers",  HYPERDBG_CMD_CHAR_SHOWREGISTERS);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - dump guest processes",  HYPERDBG_CMD_CHAR_SHOWPROCESSES);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c addr|$reg [y] - dump y dwords starting from addr or register reg", HYPERDBG_CMD_CHAR_DUMPMEMORY);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c addr|$symbol - set sw breakpoint @ address addr or at address of $symbol", HYPERDBG_CMD_CHAR_SW_BP);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c addr|$id - delete sw breakpoint @ address addr or #id", HYPERDBG_CMD_CHAR_DELETE_SW_BP);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - single step", HYPERDBG_CMD_CHAR_SINGLESTEP);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c [addr] - disassemble starting from addr (default rip)", HYPERDBG_CMD_CHAR_DISAS);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - continue execution", HYPERDBG_CMD_CHAR_CONTINUE);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c n - print backtrace of n stack frames", HYPERDBG_CMD_CHAR_BACKTRACE);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c addr - lookup symbol associated with address addr", HYPERDBG_CMD_CHAR_SYMBOL);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c addr - lookup nearest symbol to address addr", HYPERDBG_CMD_CHAR_SYMBOL_NEAREST);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - show info on HyperDbg", HYPERDBG_CMD_CHAR_INFO);

  VideoRefreshOutArea(LIGHT_GREEN);
}

static void CmdShowRegisters(PHYPERDBG_CMD pcmd)
{
  VideoResetOutMatrix();

  vmm_snprintf(out_matrix[0], OUT_SIZE_X,  "RAX        0x%08hx", context.GuestContext.RAX);
  vmm_snprintf(out_matrix[1], OUT_SIZE_X,  "RBX        0x%08hx", context.GuestContext.RBX);
  vmm_snprintf(out_matrix[2], OUT_SIZE_X,  "RCX        0x%08hx", context.GuestContext.RCX);
  vmm_snprintf(out_matrix[3], OUT_SIZE_X,  "RDX        0x%08hx", context.GuestContext.RDX);
  vmm_snprintf(out_matrix[4], OUT_SIZE_X,  "RSP        0x%08hx", context.GuestContext.RSP);
  vmm_snprintf(out_matrix[5], OUT_SIZE_X,  "RBP        0x%08hx", context.GuestContext.RBP); 
  vmm_snprintf(out_matrix[6], OUT_SIZE_X,  "RSI        0x%08hx", context.GuestContext.RSI);
  vmm_snprintf(out_matrix[7], OUT_SIZE_X,  "RDI        0x%08hx", context.GuestContext.RDI);
  vmm_snprintf(out_matrix[8], OUT_SIZE_X,  "RIP        0x%08hx", context.GuestContext.RIP);
  vmm_snprintf(out_matrix[9], OUT_SIZE_X,  "Resume RIP 0x%08hx", context.GuestContext.ResumeRIP);
  vmm_snprintf(out_matrix[10], OUT_SIZE_X, "RFLAGS     0x%08hx", context.GuestContext.RFLAGS);
  vmm_snprintf(out_matrix[11], OUT_SIZE_X, "CR0        0x%08hx", context.GuestContext.CR0);
  vmm_snprintf(out_matrix[12], OUT_SIZE_X, "CR3        0x%08hx", context.GuestContext.CR3);
  vmm_snprintf(out_matrix[13], OUT_SIZE_X, "CR4        0x%08hx", context.GuestContext.CR4);
  vmm_snprintf(out_matrix[14], OUT_SIZE_X, "CS         0x%04hx", context.GuestContext.CS);

  VideoRefreshOutArea(LIGHT_GREEN);
}

static void CmdShowProcesses(PHYPERDBG_CMD pcmd)
{
  PROCESS_DATA pp[24];
  int i, n;
  hvm_status r;

  VideoResetOutMatrix();

  r = ProcessGetActiveProcesses(context.GuestContext.CR3, pp, sizeof(pp)/sizeof(PROCESS_DATA), &n);
  if (r != HVM_STATUS_SUCCESS) {
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "error while retriving the list of active processes!");
    VideoRefreshOutArea(RED);
    return;
  }

  for (i=0; i<n; i++) {
    vmm_snprintf(out_matrix[i], OUT_SIZE_X,  "%.2d. CR3: %.8x; PID: %.8x; name: %s",
		 i, pp[i].cr3, pp[i].pid, pp[i].name);    
  }

  VideoRefreshOutArea(LIGHT_GREEN);
}

static void CmdDumpMemory(PHYPERDBG_CMD pcmd)
{
  int i, n, x, y;
  hvm_address base;
  hvm_bool test;
  hvm_status r;
  Bit32u v;

  x = 0;
  y = 0;
  base = 0;
  VideoResetOutMatrix();
  if(pcmd->nargs == 0) {
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "addr parameter missing!");
    VideoRefreshOutArea(RED);
    return;
  }
  if(pcmd->nargs == 2) {
    n = atoi(pcmd->args[1]);
    if(n <= 0) {
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid size parameter!");
      VideoRefreshOutArea(RED);
      return;
    }
  }
  else
    n = 1;

  /* This means we want to dump memory starting from a CPU register */
  if(pcmd->args[0][0] == '$') {
    if(!GetRegFromStr(&pcmd->args[0][1], &base)) {
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid register!");
      VideoRefreshOutArea(RED);
      return;

    }
  }
  else {
    if(!vmm_strtoul(pcmd->args[0], &base)) {
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid addr parameter!");
      VideoRefreshOutArea(RED);
      return;
    }
  }

  /* Check if the address is ok! */ 
  if(!MmuIsAddressValid(context.GuestContext.CR3, base)) {
    Log("[HyperDbg] Invalid memory address!\n");
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid memory address!");
    VideoRefreshOutArea(RED);
    return;
  }

  /* Print the left column of addresses */
  for(i = 0; i <= n/8; i++) {
    if(y >= OUT_SIZE_Y) break;
    /* In the case it's a multiple of 8 bytes, skip last one */
    if((i == n/8) && n%8 == 0) break; 
    vmm_snprintf(out_matrix[y], 11, "%08hx->", base+(i*8*4));
    y++;
  }
  VideoRefreshOutArea(LIGHT_BLUE);
  
  /* We can print a maximum of 7*row addresses */
  y = 0;
  x = 10;
  for(i = 0; i < n; i++) {
    if(i % 8 == 0) {
      x = 10;
      if(i != 0) y++;
    }
	
    /* Check if we reached the maximum number of lines */
    if(y >= OUT_SIZE_Y) break; 

    r = MmuReadVirtualRegion(context.GuestContext.CR3, base+(i*4), &v, sizeof(v));

    if (r == HVM_STATUS_SUCCESS) {
      vmm_snprintf(&out_matrix[y][x], 9, "%08hx", v);
    } else {
      vmm_snprintf(&out_matrix[y][x], 9, "????????");
    }

    x += 9;
  }
  VideoRefreshOutArea(LIGHT_GREEN);
}

static void CmdSwBreakpoint(PHYPERDBG_CMD pcmd)
{
  hvm_address addr;
  hvm_bool test;
  Bit32u bp_index;
  PSYMBOL symbol;

  addr = 0;

  /* Prepare shell for output */
  VideoResetOutMatrix();
  if(pcmd->nargs == 0) {
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "parameter missing!");
    VideoRefreshOutArea(RED);
    return;
  }

  /* Check if the param is a symbol name */
  if(pcmd->args[0][0] == '$') {
    symbol = SymbolGetFromName((Bit8u*) &pcmd->args[0][1]);
    if(symbol)
      addr = symbol->addr + hyperdbg_state.win_state.kernel_base;
    else {
      Log("[HyperDbg] Invalid symbol!");
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Undefined symbol %s!",  &pcmd->args[0][1]);
      VideoRefreshOutArea(RED);
      return;
    }
  }
  else {
  /* Translate the addr param */
    if(!vmm_strtoul(pcmd->args[0], &addr)) {
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid addr parameter!");
      VideoRefreshOutArea(RED);
      return;
    }

    /* Check if the address is ok */
    if(!MmuIsAddressValid(context.GuestContext.CR3, addr)) {
      Log("[HyperDbg] Invalid memory address!");
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid memory address!");
      VideoRefreshOutArea(RED);
      return;
    }
  }

  bp_index = SwBreakpointSet(context.GuestContext.CR3, addr);
  if(bp_index == MAXSWBPS) {
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Unable to set bp #%d", bp_index);
    VideoRefreshOutArea(RED);
    return;
  }

  vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Set bp #%d @ 0x%08hx", bp_index, addr);
  VideoRefreshOutArea(LIGHT_GREEN);
}

static void CmdDeleteSwBreakpoint(PHYPERDBG_CMD pcmd)
{
  hvm_address addr;
  Bit32u index;
  addr = 0;

  /* Prepare shell for output */
  VideoResetOutMatrix();
  if(pcmd->nargs == 0) {
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Parameter missing!");
    VideoRefreshOutArea(RED);
    return;
  }

  /* Check if we want to delete a bp by its id */
  if(pcmd->args[0][0] == '$') {
    index = atoi(&pcmd->args[0][1]);
    if(!SwBreakpointDeleteById(context.GuestContext.CR3, index)) {
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Bp #%d not found!", index);
      VideoRefreshOutArea(RED);
      return;
    }
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Deleted bp #%d!", index);
    VideoRefreshOutArea(LIGHT_GREEN);
  } else {
    /* Delete ID by addr */
    if(!vmm_strtoul(pcmd->args[0], &addr)) {
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid addr parameter!");
      VideoRefreshOutArea(RED);
      return;
    }
    /* Check if the address is ok */
    if(!MmuIsAddressValid(context.GuestContext.CR3, addr)) {
      Log("[HyperDbg] Invalid memory address: %.8x", addr);
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid memory address!");
      VideoRefreshOutArea(RED);
      return;
    }
    if(!SwBreakpointDelete(context.GuestContext.CR3, addr)) {
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Bp @%08hx not found!", addr);
      VideoRefreshOutArea(RED);
      return;
    }
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Bp @%08hx deleted!", addr);
    VideoRefreshOutArea(LIGHT_GREEN);
  }
}

static void CmdSetSingleStep(PHYPERDBG_CMD pcmd)
{
  hvm_address flags;

  /* Fetch guest EFLAGS */
  flags = context.GuestContext.RFLAGS;

  /* Set guest flag TF; leave others unchanged */
  flags = flags | FLAGS_TF_MASK;

  /* These will be written @ vmentry */
  context.GuestContext.RFLAGS = flags; 

  /* Update HyperDbg state structure */
  hyperdbg_state.singlestepping = TRUE;
}

static void CmdDisassemble(PHYPERDBG_CMD pcmd)
{
  hvm_address addr, tmpaddr, i, operand;
  hvm_address y, rip;
  ud_t ud_obj;
  Bit8u* disasinst;
  Bit8u disasbuf[128];
  PSYMBOL sym;
  
  y = 0;
  addr = 0;
  VideoResetOutMatrix();

  /* Check if we have to use RIP and if it's valid */
  if(pcmd->nargs == 0) {
    if(MmuIsAddressValid(context.GuestContext.CR3, context.GuestContext.RIP)) {
      addr = context.GuestContext.RIP;
    } else {
      Log("[HyperDbg] RIP is not valid: %.8x", context.GuestContext.RIP);
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid RIP!");
      VideoRefreshOutArea(RED);
      return;
    }
  }
  else {
    if(!vmm_strtoul(pcmd->args[0], &addr)) {
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid addr parameter!");
      VideoRefreshOutArea(RED);
      return;
    }

    /* Check if the address is ok */
    if(!MmuIsAddressValid(context.GuestContext.CR3, addr)) {
      Log("[HyperDbg] Invalid memory address: %.8x", addr);
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid memory address!");
      VideoRefreshOutArea(RED);
      return;
    }
  }

  tmpaddr = addr;

  /* Setup disassembler structure */
  ud_init(&ud_obj);
  ud_set_mode(&ud_obj, 32);
  ud_set_syntax(&ud_obj, UD_SYN_ATT);
  
  MmuReadVirtualRegion(context.GuestContext.CR3, addr, disasbuf, sizeof(disasbuf) ); ///sizeof(Bit8u));
  ud_set_input_buffer(&ud_obj, disasbuf, sizeof(disasbuf)/sizeof(Bit8u));

  while(ud_disassemble(&ud_obj)) {
    /* check if we can resolv a symbol */
    /* FIXME: reengineer better */
    sym = 0;
    i = 0;
    disasinst = (Bit8u*) ud_insn_asm(&ud_obj);
    while(disasinst[i] != (Bit8u)'\0' && disasinst[i] != (Bit8u)' ') i++; /* reach the end or the first operand */
    if(disasinst[i] != 0 && (vmm_strlen(disasinst) - (&disasinst[i] - disasinst)) == 11) { /* 11 is the size of 0x12345678 + 1 space */
      /* check if it's likely to be an address */
      if(disasinst[i+1] == (Bit8u)'0' && disasinst[i+2] == 'x') {
	if(vmm_strtoul(&disasinst[i+1], &operand)) { /* if it can be converted to a hvm_address */
	  sym = SymbolGetFromAddress(operand);
	}
      }
    }
    if(sym)
      vmm_snprintf(out_matrix[y], OUT_SIZE_X, "%08hx: %-24s %s <%s>", tmpaddr, ud_insn_hex(&ud_obj), ud_insn_asm(&ud_obj), sym->name);
    else
      vmm_snprintf(out_matrix[y], OUT_SIZE_X, "%08hx: %-24s %s", tmpaddr, ud_insn_hex(&ud_obj), ud_insn_asm(&ud_obj));
    tmpaddr += ud_insn_len(&ud_obj);
    y++;
    /* Check if maximum size have been reached */
    if(y >= OUT_SIZE_Y) break;
  }

  VideoRefreshOutArea(LIGHT_GREEN);
}

static void CmdBacktrace(PHYPERDBG_CMD pcmd)
{
  Bit32u current_frame, nframes, n;
  hvm_address current_rip;
  hvm_status r;
  PSYMBOL nearsym;
  Bit16u name[64];
  Bit8u  namec[64];
  Bit8u  sym[96];
  Bit8u  module[96];
  
  VideoResetOutMatrix();
  /* Parse (optional) command argument */
  if(pcmd->nargs == 0) {
    n = 0;
  } else {
    if(!vmm_strtoul(pcmd->args[0], &n)) {
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid stack frames number parameter!");
      VideoRefreshOutArea(RED);
      return;
    }
  }

  /* Print backtrace */
  current_frame = context.GuestContext.RBP;
  nframes = 0;

  while (1) {
    r = MmuReadVirtualRegion(context.GuestContext.CR3, current_frame+4, &current_rip, sizeof(current_rip));
    if (r != HVM_STATUS_SUCCESS) break;

    vmm_memset(namec, 0, sizeof(namec));
    vmm_memset(sym, 0, sizeof(sym));
    vmm_memset(module, 0, sizeof(module));
    if(current_rip >= hyperdbg_state.win_state.kernel_base) {
      /* FIXME -> check if it's win */

      r = WindowsFindModule(context.GuestContext.CR3, current_rip, name, 64);
      if(r == HVM_STATUS_SUCCESS) {
	wide2ansi(namec, (Bit8u*)name, sizeof(namec)/sizeof(Bit8s));
	if(vmm_strlen(namec) > 0)
	  vmm_snprintf(module, 96, "[%s]", namec);
      }
      nearsym = SymbolGetNearest((hvm_address) current_rip);
      if(current_rip-(nearsym->addr + hyperdbg_state.win_state.kernel_base) < 100000) /* FIXME: tune this param */
	vmm_snprintf(sym, 96, "(%s+%d)", nearsym->name, (current_rip-(nearsym->addr + hyperdbg_state.win_state.kernel_base)));
    }

    vmm_snprintf(out_matrix[nframes], OUT_SIZE_X, "[%02d] @%.8hx %.8hx %-40s %s\n", nframes, current_frame, current_rip, sym, module);

    nframes += 1;
    if ((n != 0 && nframes > n) || nframes >= OUT_SIZE_Y) {
      break;
    }

    /* Go on with the next frame */
    r = MmuReadVirtualRegion(context.GuestContext.CR3, current_frame, &current_frame, sizeof(current_frame));
    if (r != HVM_STATUS_SUCCESS) break;
  }
  VideoRefreshOutArea(LIGHT_GREEN);
}

static void CmdLookupSymbol(PHYPERDBG_CMD pcmd, hvm_bool bExactMatch)
{
  hvm_address base;
  Bit8u *name;
  PSYMBOL psym;

  base = 0;
  VideoResetOutMatrix();

  if(pcmd->nargs == 0) {
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "addr parameter missing!");
    VideoRefreshOutArea(RED);
    return;
  }

  if(!vmm_strtoul(pcmd->args[0], &base)) {
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid addr parameter!");
    VideoRefreshOutArea(RED);
    return;
  }

  if (bExactMatch) {
    psym = SymbolGetFromAddress(base);
    if(psym) 
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "%08hx: %s", base, psym->name);
    else 
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "No symbol defined for %08hx", base);
  } else {
    psym = SymbolGetNearest(base);
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Nearest symbol smaller than %08hx: %s (@%08hx)", 
		 base, psym->name, psym->addr + hyperdbg_state.win_state.kernel_base);
  }

  VideoRefreshOutArea(LIGHT_GREEN);
}

static void CmdShowInfo(PHYPERDBG_CMD pcmd)
{
  Bit32u start;

  VideoResetOutMatrix();

  start = 8;

  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    ***********************************************************                   ");
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    *                                                         *                   ");
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    *                     HyperDbg " HYPERDBG_VERSION "                   *                   ");
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    *         " HYPERDBG_URL "         *                   ");
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    *          Coded by: martignlo, roby and joystick         *                   ");
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    *      {lorenzo.martignoni,roberto.paleari}@gmail.com     *                   ");
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    *             joystick@security.dico.unimi.it             *                   ");
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    *                                                         *                   ");
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    ***********************************************************                   ");
  VideoRefreshOutArea(LIGHT_GREEN);
}

static void CmdUnknown(PHYPERDBG_CMD pcmd)
{
  VideoResetOutMatrix();
  vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Unknown command");
  VideoRefreshOutArea(RED);
}

static hvm_bool GetRegFromStr(Bit8u *name, hvm_address *value)
{
  if(vmm_strncmpi(name, "RAX", 3) == 0) { *value = context.GuestContext.RAX; return TRUE; }
  if(vmm_strncmpi(name, "RBX", 3) == 0) { *value = context.GuestContext.RBX; return TRUE; }
  if(vmm_strncmpi(name, "RCX", 3) == 0) { *value = context.GuestContext.RCX; return TRUE; }
  if(vmm_strncmpi(name, "RDX", 3) == 0) { *value = context.GuestContext.RDX; return TRUE; }
  if(vmm_strncmpi(name, "RSP", 3) == 0) { *value = context.GuestContext.RSP; return TRUE; }
  if(vmm_strncmpi(name, "RBP", 3) == 0) { *value = context.GuestContext.RBP; return TRUE; }
  if(vmm_strncmpi(name, "RSI", 3) == 0) { *value = context.GuestContext.RSI; return TRUE; }
  if(vmm_strncmpi(name, "RDI", 3) == 0) { *value = context.GuestContext.RSI; return TRUE; }
  if(vmm_strncmpi(name, "RIP", 3) == 0) { *value = context.GuestContext.RIP; return TRUE; }
  if(vmm_strncmpi(name, "CR0", 3) == 0) { *value = context.GuestContext.CR0; return TRUE; }
  if(vmm_strncmpi(name, "CR3", 3) == 0) { *value = context.GuestContext.CR3; return TRUE; }
  if(vmm_strncmpi(name, "CR4", 3) == 0) { *value = context.GuestContext.CR4; return TRUE; }
  
  return FALSE;
}

static void ParseCommand(Bit8u *buffer, PHYPERDBG_CMD pcmd)
{
  char *p;
  int i, n;

  /* Initialize */
  memset(pcmd, 0, sizeof(HYPERDBG_CMD));

  p = buffer;

  /* Skip leading spaces */
  while (*p != '\0' && CHAR_IS_SPACE(*p)) p++;

#define PARSE_COMMAND(c)			\
  case HYPERDBG_CMD_CHAR_##c:			\
    pcmd->opcode = HYPERDBG_CMD_##c ;		\
    break;

  switch(p[0]) {
    PARSE_COMMAND(HELP);
    PARSE_COMMAND(SHOWREGISTERS);
    PARSE_COMMAND(SHOWPROCESSES);
    PARSE_COMMAND(DUMPMEMORY);
    PARSE_COMMAND(SW_BP);
    PARSE_COMMAND(DELETE_SW_BP);
    PARSE_COMMAND(SINGLESTEP);
    PARSE_COMMAND(DISAS);
    PARSE_COMMAND(BACKTRACE);
    PARSE_COMMAND(CONTINUE);
    PARSE_COMMAND(SYMBOL);
    PARSE_COMMAND(SYMBOL_NEAREST);
    PARSE_COMMAND(INFO);
  default:
    pcmd->opcode = HYPERDBG_CMD_UNKNOWN;
    break;
  }

#undef PARSE_COMMAND
  
  p++;

  for (i=0; i<sizeof(pcmd->args)/sizeof(pcmd->args[0]); i++) {
    /* Read i-th argument */
    while (*p != '\0' && CHAR_IS_SPACE(*p)) p++;

    /* Check if we reached EOS */
    if (*p == '\0') break;

    /* Calculate argument length */
    for (n=0; *p != '\0' && !CHAR_IS_SPACE(*p); n++) p++;
    vmm_strncpy(pcmd->args[i], p-n, MIN(n, sizeof(pcmd->args[0])));
  
  }
  for (i=0; i<sizeof(pcmd->args)/sizeof(pcmd->args[0]); i++) {
    if (vmm_strlen(pcmd->args[i]) == 0)
      break;
    pcmd->nargs++;
  }
}

