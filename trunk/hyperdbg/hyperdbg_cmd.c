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
#include "vmm.h"
#include "vmmstring.h"
#include "debug.h"
#include "video.h"
#include "gui.h"
#include "stdlib.h"		/* FIXME: re-implement atoi() */
#include "mmu.h"
#include "sw_bp.h"
#include "x86.h"       /* Needed for the FLAGS_TF_MASK macro */
#include "vmx.h"
#include "extern.h"    /* From libudis */
#include "symsearch.h"
#include "syms.h"
#include "common.h"

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
  UCHAR           args[4][128];
} HYPERDBG_CMD, *PHYPERDBG_CMD;

/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

static VOID CmdHelp(PHYPERDBG_CMD pcmd);
static VOID CmdShowRegisters(PHYPERDBG_CMD pcmd);
static VOID CmdDumpMemory(PHYPERDBG_CMD pcmd);
static VOID CmdSwBreakpoint(PHYPERDBG_CMD pcmd);
static VOID CmdDeleteSwBreakpoint(PHYPERDBG_CMD pcmd);
static VOID CmdSetSingleStep(PHYPERDBG_CMD pcmd);
static VOID CmdDisassemble(PHYPERDBG_CMD pcmd);
static VOID CmdBacktrace(PHYPERDBG_CMD pcmd);
static VOID CmdLookupSymbol(PHYPERDBG_CMD pcmd, BOOLEAN bExactMatch);
static VOID CmdShowInfo(PHYPERDBG_CMD pcmd);
static VOID CmdUnknown(PHYPERDBG_CMD pcmd);

static VOID ParseCommand(UCHAR *buffer, PHYPERDBG_CMD pcmd);
static ULONG GetRegFromStr(UCHAR *name);

/* ################ */
/* #### BODIES #### */
/* ################ */

BOOLEAN HyperDbgProcessCommand(UCHAR *bufcmd)
{
  HYPERDBG_CMD cmd;
  BOOLEAN ExitLoop;

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

static VOID CmdHelp(PHYPERDBG_CMD pcmd)
{
  int i;

  VideoResetOutMatrix();

  i = 0;

  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "Available commands:");
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - show this help screen", HYPERDBG_CMD_CHAR_HELP);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - dump guest registers",  HYPERDBG_CMD_CHAR_SHOWREGISTERS);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c addr|$reg [y] - dump y dwords starting from addr or register reg", HYPERDBG_CMD_CHAR_DUMPMEMORY);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c addr|$symbol - set sw breakpoint @ address addr or at address of $symbol", HYPERDBG_CMD_CHAR_SW_BP);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c addr|$id - delete sw breakpoint @ address addr or #id", HYPERDBG_CMD_CHAR_DELETE_SW_BP);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - single step", HYPERDBG_CMD_CHAR_SINGLESTEP);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c [addr] - disassemble starting from addr (default eip)", HYPERDBG_CMD_CHAR_DISAS);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - continue execution", HYPERDBG_CMD_CHAR_CONTINUE);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c n - print backtrace of n stack frames", HYPERDBG_CMD_CHAR_BACKTRACE);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c addr - lookup symbol associated with address addr", HYPERDBG_CMD_CHAR_SYMBOL);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c addr - lookup nearest symbol to address addr", HYPERDBG_CMD_CHAR_SYMBOL_NEAREST);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - show info on HyperDbg", HYPERDBG_CMD_CHAR_INFO);

  VideoRefreshOutArea(LIGHT_GREEN);
}

static VOID CmdShowRegisters(PHYPERDBG_CMD pcmd)
{
  VideoResetOutMatrix();
  vmm_snprintf(out_matrix[0], OUT_SIZE_X,  "EAX        0x%08hx", vmcs.GuestState.EAX);
  vmm_snprintf(out_matrix[1], OUT_SIZE_X,  "EBX        0x%08hx", vmcs.GuestState.EBX);
  vmm_snprintf(out_matrix[2], OUT_SIZE_X,  "ECX        0x%08hx", vmcs.GuestState.ECX);
  vmm_snprintf(out_matrix[3], OUT_SIZE_X,  "EDX        0x%08hx", vmcs.GuestState.EDX);
  vmm_snprintf(out_matrix[4], OUT_SIZE_X,  "ESP        0x%08hx", vmcs.GuestState.ESP);
  vmm_snprintf(out_matrix[5], OUT_SIZE_X,  "EBP        0x%08hx", vmcs.GuestState.EBP); 
  vmm_snprintf(out_matrix[6], OUT_SIZE_X,  "ESI        0x%08hx", vmcs.GuestState.ESI);
  vmm_snprintf(out_matrix[7], OUT_SIZE_X,  "EDI        0x%08hx", vmcs.GuestState.EDI);
  vmm_snprintf(out_matrix[8], OUT_SIZE_X,  "EIP        0x%08hx", vmcs.GuestState.EIP);
  vmm_snprintf(out_matrix[9], OUT_SIZE_X,  "Resume EIP 0x%08hx", vmcs.GuestState.ResumeEIP);
  vmm_snprintf(out_matrix[10], OUT_SIZE_X, "EFLAGS     0x%08hx", vmcs.GuestState.EFLAGS);
  vmm_snprintf(out_matrix[11], OUT_SIZE_X, "CR0        0x%08hx", vmcs.GuestState.CR0);
  vmm_snprintf(out_matrix[12], OUT_SIZE_X, "CR3        0x%08hx", vmcs.GuestState.CR3);
  vmm_snprintf(out_matrix[13], OUT_SIZE_X, "CR4        0x%08hx", vmcs.GuestState.CR4);
  vmm_snprintf(out_matrix[14], OUT_SIZE_X, "CS         0x%04hx", vmcs.GuestState.CS);

  VideoRefreshOutArea(LIGHT_GREEN);
}

static VOID CmdDumpMemory(PHYPERDBG_CMD pcmd)
{
  int i, n, x, y;
  ULONG base, v;
  BOOLEAN test;
  NTSTATUS r;

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
    base = GetRegFromStr(&pcmd->args[0][1]);
  }
  else {
    if(!vmm_strtoul(pcmd->args[0], &base)) {
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid addr parameter!");
      VideoRefreshOutArea(RED);
      return;
    }
  }

  /* Check if the address is ok! */ 
  if(!MmuIsAddressValid(vmcs.GuestState.CR3, base)) {
    DbgPrint("[HyperDbg] Invalid memory address!\n");
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

    r = MmuReadVirtualRegion(vmcs.GuestState.CR3, base+(i*4), &v, sizeof(v));

    if (r == STATUS_SUCCESS) {
      vmm_snprintf(&out_matrix[y][x], 9, "%08hx", v);
    } else {
      vmm_snprintf(&out_matrix[y][x], 9, "????????");
    }

    x += 9;
  }
  VideoRefreshOutArea(LIGHT_GREEN);
}

static VOID CmdSwBreakpoint(PHYPERDBG_CMD pcmd)
{
  ULONG addr;
  BOOLEAN test;
  ULONG32 bp_index;
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
    symbol = SymbolGetFromName((PUCHAR) &pcmd->args[0][1]);
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
    if(!MmuIsAddressValid(vmcs.GuestState.CR3, addr)) {
      Log("[HyperDbg] Invalid memory address!");
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid memory address!");
      VideoRefreshOutArea(RED);
      return;
    }
  }

  bp_index = SwBreakpointSet(vmcs.GuestState.CR3, (PULONG) addr);
  if(bp_index == MAXSWBPS) {
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Unable to set bp #%d", bp_index);
    VideoRefreshOutArea(RED);
    return;
  }

  vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Set bp #%d @ 0x%08hx", bp_index, addr);
  VideoRefreshOutArea(LIGHT_GREEN);
}

static VOID CmdDeleteSwBreakpoint(PHYPERDBG_CMD pcmd)
{
  ULONG addr;
  ULONG32 index;
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
    if(!SwBreakpointDeleteById(vmcs.GuestState.CR3, index)) {
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
    if(!MmuIsAddressValid(vmcs.GuestState.CR3, addr)) {
      Log("[HyperDbg] Invalid memory address: %.8x", addr);
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid memory address!");
      VideoRefreshOutArea(RED);
      return;
    }
    if(!SwBreakpointDelete(vmcs.GuestState.CR3, (PULONG) addr)) {
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Bp @%08hx not found!", addr);
      VideoRefreshOutArea(RED);
      return;
    }
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Bp @%08hx deleted!", addr);
    VideoRefreshOutArea(LIGHT_GREEN);
  }
}

static VOID CmdSetSingleStep(PHYPERDBG_CMD pcmd)
{
  ULONG32 flags;

  /* Fetch guest EFLAGS */
  flags = vmcs.GuestState.EFLAGS;

  /* Set guest flag TF; leave others unchanged */
  flags = flags | FLAGS_TF_MASK;

  /* These will be written @ vmentry */
  vmcs.GuestState.EFLAGS = flags; 
  VmxWrite(GUEST_RFLAGS, FLAGS_TO_ULONG(flags));

  /* Update HyperDbg state structure */
  hyperdbg_state.singlestepping = TRUE;
}

static VOID CmdDisassemble(PHYPERDBG_CMD pcmd)
{
  ULONG addr, tmpaddr, i, operand;
  ULONG32 y, rip;
  ud_t ud_obj;
  PUCHAR disasinst;
  UCHAR disasbuf[128];
  PSYMBOL sym;
  
  y = 0;
  addr = 0;
  VideoResetOutMatrix();

  /* Check if we have to use EIP and if it's valid */
  if(pcmd->nargs == 0) {
    if(MmuIsAddressValid(vmcs.GuestState.CR3, vmcs.GuestState.EIP)) {
      addr = vmcs.GuestState.EIP;
    } else {
      Log("[HyperDbg] EIP is not valid: %.8x", vmcs.GuestState.EIP);
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid EIP!");
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
    if(!MmuIsAddressValid(vmcs.GuestState.CR3, addr)) {
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
  
  MmuReadVirtualRegion(vmcs.GuestState.CR3, addr, disasbuf, sizeof(disasbuf) ); ///sizeof(UCHAR));
  ud_set_input_buffer(&ud_obj, disasbuf, sizeof(disasbuf)/sizeof(UCHAR));

  while(ud_disassemble(&ud_obj)) {
    /* check if we can resolv a symbol */
    /* FIXME: reengineer better */
    sym = 0;
    i = 0;
    disasinst = (PUCHAR) ud_insn_asm(&ud_obj);
    while(disasinst[i] != (UCHAR)'\0' && disasinst[i] != (UCHAR)' ') i++; /* reach the end or the first operand */
    if(disasinst[i] != 0 && (vmm_strlen(disasinst) - (&disasinst[i] - disasinst)) == 11) { /* 11 is the size of 0x12345678 + 1 space */
      /* check if it's likely to be an address */
      if(disasinst[i+1] == (UCHAR)'0' && disasinst[i+2] == 'x') {
	if(vmm_strtoul(&disasinst[i+1], &operand)) { /* if it's convertible to a ULONG */
	  sym = SymbolGetFromAddress((PULONG)operand);
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

static VOID CmdBacktrace(PHYPERDBG_CMD pcmd)
{
  ULONG current_frame, current_eip, nframes, n;
  NTSTATUS r;
  PSYMBOL nearsym;
  WCHAR name[64];
  UCHAR namec[64];
  UCHAR sym[96];
  UCHAR module[96];
  
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
  current_frame = vmcs.GuestState.EBP;
  nframes = 0;

  while (1) {
    r = MmuReadVirtualRegion(vmcs.GuestState.CR3, current_frame+4, &current_eip, sizeof(current_eip));
    if (r != STATUS_SUCCESS) break;

    vmm_memset(namec, 0, sizeof(namec));
    vmm_memset(sym, 0, sizeof(sym));
    vmm_memset(module, 0, sizeof(module));
    if(current_eip >= hyperdbg_state.win_state.kernel_base) {
      /* FIXME -> check if it's win */

      r = WindowsFindModule(vmcs.GuestState.CR3, current_eip, name, 64);
      if(r == STATUS_SUCCESS) {
	wide2ansi(namec, (PUCHAR)name, sizeof(namec)/sizeof(CHAR));
	if(vmm_strlen(namec) > 0)
	  vmm_snprintf(module, 96, "[%s]", namec);
      }
      nearsym = SymbolGetNearest((PULONG) current_eip);
      if(current_eip-(nearsym->addr + hyperdbg_state.win_state.kernel_base) < 100000) /* FIXME: tune this param */
	vmm_snprintf(sym, 96, "(%s+%d)", nearsym->name, (current_eip-(nearsym->addr + hyperdbg_state.win_state.kernel_base)));
    }

    vmm_snprintf(out_matrix[nframes], OUT_SIZE_X, "[%02d] @%.8hx %.8hx %-40s %s\n", nframes, current_frame, current_eip, sym, module);

    nframes += 1;
    if ((n != 0 && nframes > n) || nframes >= OUT_SIZE_Y) {
      break;
    }

    /* Go on with the next frame */
    r = MmuReadVirtualRegion(vmcs.GuestState.CR3, current_frame, &current_frame, sizeof(current_frame));
    if (r != STATUS_SUCCESS) break;
  }
  VideoRefreshOutArea(LIGHT_GREEN);
}

static VOID CmdLookupSymbol(PHYPERDBG_CMD pcmd, BOOLEAN bExactMatch)
{
  ULONG base;
  UCHAR *name;
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
    psym = SymbolGetFromAddress((PULONG) base);
    if(psym) 
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "%08hx: %s", base, psym->name);
    else 
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "No symbol defined for %08hx", base);
  } else {
    psym = SymbolGetNearest((PULONG) base);
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Nearest symbol smaller than %08hx: %s (@%08hx)", 
		 base, psym->name, psym->addr + hyperdbg_state.win_state.kernel_base);
  }

  VideoRefreshOutArea(LIGHT_GREEN);
}

static VOID CmdShowInfo(PHYPERDBG_CMD pcmd)
{
  ULONG32 start;
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

static VOID CmdUnknown(PHYPERDBG_CMD pcmd)
{
  VideoResetOutMatrix();
  vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Unknown command");
  VideoRefreshOutArea(RED);
}

static ULONG GetRegFromStr(UCHAR *name)
{
  if(vmm_strncmpi(name, "EAX", 3) == 0) return vmcs.GuestState.EAX;
  if(vmm_strncmpi(name, "EBX", 3) == 0) return vmcs.GuestState.EBX;
  if(vmm_strncmpi(name, "ECX", 3) == 0) return vmcs.GuestState.ECX;
  if(vmm_strncmpi(name, "EDX", 3) == 0) return vmcs.GuestState.EDX;
  if(vmm_strncmpi(name, "ESP", 3) == 0) return vmcs.GuestState.ESP;
  if(vmm_strncmpi(name, "EBP", 3) == 0) return vmcs.GuestState.EBP;
  if(vmm_strncmpi(name, "ESI", 3) == 0) return vmcs.GuestState.ESI;
  if(vmm_strncmpi(name, "EIP", 3) == 0) return vmcs.GuestState.EIP;
  if(vmm_strncmpi(name, "CR0", 3) == 0) return vmcs.GuestState.CR0;
  if(vmm_strncmpi(name, "CR3", 3) == 0) return vmcs.GuestState.CR3;
  if(vmm_strncmpi(name, "CR4", 3) == 0) return vmcs.GuestState.CR4;
  
  return (ULONG)0;
}

static VOID ParseCommand(UCHAR *buffer, PHYPERDBG_CMD pcmd)
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
    strncpy(pcmd->args[i], p-n, MIN(n, sizeof(pcmd->args[0])));
  
  }
  for (i=0; i<sizeof(pcmd->args)/sizeof(pcmd->args[0]); i++) {
    if (vmm_strlen(pcmd->args[i]) == 0)
      break;
    pcmd->nargs++;
  }
}

