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
#include "mmu.h"
#include "sw_bp.h"
#include "x86.h"       /* Needed for the FLAGS_TF_MASK macro */
#include "vt.h"
#include "extern.h"    /* From libudis */
#include "symsearch.h"
#include "syms.h"
#include "common.h"
#include "process.h"
#include "network.h"
#include "pager.h"
#include "hyperdbg_print.h"

#ifdef GUEST_WINDOWS
#include "winxp.h"
#endif

/* ############### */
/* #### TYPES #### */
/* ############### */

typedef enum {
  HYPERDBG_CMD_UNKNOWN = 0,
  HYPERDBG_CMD_HELP,
  HYPERDBG_CMD_SHOWMODULES,
  HYPERDBG_CMD_SHOWREGISTERS,
  HYPERDBG_CMD_SHOWPROCESSES,
  HYPERDBG_CMD_SHOWSOCKETS,
  HYPERDBG_CMD_DUMPMEMORY,
  HYPERDBG_CMD_SW_BP,
  HYPERDBG_CMD_SW_PERM_BP,
  HYPERDBG_CMD_DELETE_SW_BP,
  HYPERDBG_CMD_LIST_BP,
  HYPERDBG_CMD_SINGLESTEP,
  HYPERDBG_CMD_BACKTRACE,
  HYPERDBG_CMD_DISAS,
  HYPERDBG_CMD_CONTINUE,
  HYPERDBG_CMD_SYMBOL,
  HYPERDBG_CMD_SYMBOL_NEAREST,
  HYPERDBG_CMD_INFO,
  HYPERDBG_CMD_UNLINK_PROC,
  HYPERDBG_CMD_RELINK_PROC,
} HYPERDBG_OPCODE;

typedef struct {
  HYPERDBG_OPCODE opcode;
  int             nargs;
  Bit8u           args[4][128];
} HYPERDBG_CMD, *PHYPERDBG_CMD;

/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

static void CmdRetrieveRegisters(PHYPERDBG_CMD pcmd, PCMD_RESULT result);
static void CmdRetrieveProcesses(PHYPERDBG_CMD pcmd, PCMD_RESULT result, Bit32s *size);
static void CmdRetrieveModules(PHYPERDBG_CMD pcmd, PCMD_RESULT result, Bit32s *size);
static void CmdRetrieveSockets(PHYPERDBG_CMD pcmd, PCMD_RESULT result, Bit32s *size);
static void CmdDumpMemory(PHYPERDBG_CMD pcmd, PCMD_RESULT result, Bit32s *size);
static void CmdSwBreakpoint(PHYPERDBG_CMD pcmd, PCMD_RESULT result, hvm_bool isPerm);
static void CmdDeleteSwBreakpoint(PHYPERDBG_CMD pcmd, PCMD_RESULT result);
static void CmdSetSingleStep(PHYPERDBG_CMD pcmd);
static void CmdDisassemble(PHYPERDBG_CMD pcmd, PCMD_RESULT result, Bit32s *size);
static void CmdBacktrace(PHYPERDBG_CMD pcmd);
static void CmdLookupSymbol(PHYPERDBG_CMD pcmd, hvm_bool bExactMatch);
static void CmdUnlinkProc(PHYPERDBG_CMD pcmd, Bit32s *result);
static void CmdRelinkProc(PHYPERDBG_CMD pcmd, Bit32s *result);

static void ParseCommand(Bit8u *buffer, PHYPERDBG_CMD pcmd);
static hvm_bool GetRegFromStr(Bit8u *name, hvm_address *value);

/* ############### */
/* ### GLOBALS ### */
/* ############### */

/* We use this as a global var as it is too big for a stack frame */
CMD_RESULT result;

/* ################ */
/* #### BODIES #### */
/* ################ */

hvm_bool HyperDbgProcessCommand(Bit8u *bufcmd)
{
  HYPERDBG_CMD cmd;
  hvm_bool ExitLoop;
  Bit32s size = 0;

  vmm_memset(&result, 0, sizeof(result));

  /* By default, a command doesn't interrupt the command loop in hyperdbg_host */
  ExitLoop = FALSE; 
  if (vmm_strlen(bufcmd) == 0)
    return ExitLoop;

  ParseCommand(bufcmd, &cmd);

  switch (cmd.opcode) {
  case HYPERDBG_CMD_HELP:
    PrintHelp();
    break;
  case HYPERDBG_CMD_SHOWREGISTERS:
    CmdRetrieveRegisters(&cmd, &result);
    PrintRegisters(&result);
    break;
  case HYPERDBG_CMD_SHOWPROCESSES:
    CmdRetrieveProcesses(&cmd, &result, &size);
    PrintProcesses(&result, size);
    break;
  case HYPERDBG_CMD_SHOWMODULES:
    CmdRetrieveModules(&cmd, &result, &size);
    PrintModules(&result, size);
    break;
  case HYPERDBG_CMD_SHOWSOCKETS:
    CmdRetrieveSockets(&cmd, &result, &size);
    PrintSockets(&result, size);
    break;
  case HYPERDBG_CMD_DUMPMEMORY:
    CmdDumpMemory(&cmd, &result, &size);
    PrintMemoryDump(&result, size);
    break;
  case HYPERDBG_CMD_SW_BP:
    CmdSwBreakpoint(&cmd, &result, FALSE);
    PrintSwBreakpoint(&result);
    break;
  case HYPERDBG_CMD_SW_PERM_BP:
    CmdSwBreakpoint(&cmd, &result, TRUE);
    PrintSwBreakpoint(&result);
    break;
  case HYPERDBG_CMD_LIST_BP:
    SwBreakpointGetBPList(&result);
    PrintBPList(&result);
    break;
  case HYPERDBG_CMD_DELETE_SW_BP:
    CmdDeleteSwBreakpoint(&cmd, &result);
    PrintDeleteSwBreakpoint(&result);
    break;
  case HYPERDBG_CMD_SINGLESTEP:
    CmdSetSingleStep(&cmd);
    /* After this command we have to return control to the guest */
    ExitLoop = TRUE; 
    break;
  case HYPERDBG_CMD_DISAS:
    CmdDisassemble(&cmd, &result, &size);
    PrintDisassembled(&result, size);
    break;
  case HYPERDBG_CMD_UNLINK_PROC:
    CmdUnlinkProc(&cmd, &size);
    PrintUnlinkProc(size);
    break;
  case HYPERDBG_CMD_RELINK_PROC:
    CmdRelinkProc(&cmd, &size);
    PrintRelinkProc(size);
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
    PrintInfo();
    break;
  default:
    PrintUnknown();
    break;
  }
  return ExitLoop;
}

static void CmdRetrieveRegisters(PHYPERDBG_CMD pcmd, PCMD_RESULT result)
{
  result->registers.rax = context.GuestContext.rax;
  result->registers.rbx = context.GuestContext.rbx;
  result->registers.rcx = context.GuestContext.rcx;
  result->registers.rdx = context.GuestContext.rdx;
  result->registers.rsp = context.GuestContext.rsp;
  result->registers.rbp = context.GuestContext.rbp;
  result->registers.rsi = context.GuestContext.rsi;
  result->registers.rdi = context.GuestContext.rdi;
  result->registers.rip = context.GuestContext.rip;
  result->registers.resumerip = context.GuestContext.resumerip;
  result->registers.rflags = context.GuestContext.rflags;
  result->registers.cr0 = context.GuestContext.cr0;
  result->registers.cr3 = context.GuestContext.cr3;
  result->registers.cr4 = context.GuestContext.cr4;
  result->registers.cs = context.GuestContext.cs;
}

static void CmdRetrieveProcesses(PHYPERDBG_CMD pcmd, PCMD_RESULT result, Bit32s *size)
{
  int i;
  hvm_status r;

  r = ProcessGetNextProcess(context.GuestContext.cr3, NULL, &(result->processes[0]));

  for (i=0; ; i++) {
    if (r == HVM_STATUS_END_OF_FILE) {
      /* No more processes */
      break;
    } else if (r != HVM_STATUS_SUCCESS) {
      /* Print an error message */
      *size = ERROR_COMMAND_SPECIFIC;
      return;
    }

    /* Get the next process */
    r = ProcessGetNextProcess(context.GuestContext.cr3, &(result->processes[i]), &(result->processes[i+1]));
  }
  *size = i;
}

static void CmdRetrieveModules(PHYPERDBG_CMD pcmd, PCMD_RESULT result, Bit32s *size)
{
  int i;
  hvm_status r;

  r = ProcessGetNextModule(context.GuestContext.cr3, NULL, &(result->modules[0]));

  for (i=0; ; i++) {
    if (r == HVM_STATUS_END_OF_FILE) {
      /* No more processes */
      break;
    } else if (r != HVM_STATUS_SUCCESS) {
      /* Print an error size */
      *size = ERROR_COMMAND_SPECIFIC;
      return;
    }

    /* Get the next process */
    r = ProcessGetNextModule(context.GuestContext.cr3, &(result->modules[i]), &(result->modules[i+1]));
  }
  *size = i;
}

static void CmdRetrieveSockets(PHYPERDBG_CMD pcmd, PCMD_RESULT result, Bit32s *size)
{
  hvm_status r;
  Bit32u n;

  r = NetworkBuildSocketList(context.GuestContext.cr3,(SOCKET *)(&(result->sockets)), 128, &n);

  if (r != HVM_STATUS_SUCCESS) {
    /* Print an error message */
    *size = ERROR_COMMAND_SPECIFIC;
    return;
  }

  *size = n;
}

static void CmdUnlinkProc(PHYPERDBG_CMD pcmd, Bit32s *result)
{
#ifdef GUEST_WIN_7
  char tmp[OUT_SIZE_X];
  MODULE_DATA ntoskrnl;
  hvm_address pep, cr3;
  hvm_status r;
  int i;
  Bit8u bw = 0xcc;

  if(pcmd->nargs == 0) {
    *result = ERROR_MISSING_PARAM;
    return;
  }

  if(!vmm_strtoul(pcmd->args[0], &cr3)) {
    *result = ERROR_INVALID_ADDR;
  }

  /* Find module data of ntoskrnl.exe */ 
  r = WindowsFindModuleByName(context.GuestContext.cr3, "ntoskrnl.exe", &ntoskrnl);
  if(r == HVM_STATUS_SUCCESS) {

    hyperdbg_state.unlink_bp_addr = ntoskrnl.baseaddr + 0x2fbd2;
    hyperdbg_state.relink_bp_addr = ntoskrnl.baseaddr + 0x2fba5;
  }
  else {
    
    Log("[HyperDbg] ntoskrnl.exe not found....");
    *result = ERROR_COMMAND_SPECIFIC;
    return;
  }

  /* Insert breakpoint */
  r = MmuReadVirtualRegion(context.GuestContext.cr3, hyperdbg_state.unlink_bp_addr, &hyperdbg_state.opcode_backup_unlink, sizeof(Bit8u));
  r = MmuWriteVirtualRegion(context.GuestContext.cr3, hyperdbg_state.unlink_bp_addr, &bw, sizeof(Bit8u));
  
  /* Retrieve some useful info of target process */
  hyperdbg_state.target_cr3 = cr3;
  r = Windows7FindTargetInfo(hyperdbg_state.target_cr3, &hyperdbg_state.target_pep, &hyperdbg_state.target_kthread);

  if (r != HVM_STATUS_SUCCESS) {
    *result = ERROR_COMMAND_SPECIFIC;
    return;
  }

  *result = 0;
#else /* We must do this both for Win XP and Linux guests */
  *result = ERROR_COMMAND_SPECIFIC;
#endif
}

static void CmdRelinkProc(PHYPERDBG_CMD pcmd, Bit32s *result)
{
#ifdef GUEST_WIN_7
  hvm_status r;
  Bit8u bw = 0xcc;

  /* Set relinking breakpoint */
  r = MmuReadVirtualRegion(context.GuestContext.cr3, hyperdbg_state.relink_bp_addr, &hyperdbg_state.opcode_backup_relink, sizeof(Bit8u));
  r = MmuWriteVirtualRegion(context.GuestContext.cr3, hyperdbg_state.relink_bp_addr, &bw, sizeof(Bit8u));

  if(r != HVM_STATUS_SUCCESS) {
    *result = ERROR_COMMAND_SPECIFIC;
    return;
  }

  *result = 0;
#else /* We must do this both for Win XP and Linux guests */
  *result = ERROR_COMMAND_SPECIFIC;
#endif
}

static void CmdDumpMemory(PHYPERDBG_CMD pcmd, PCMD_RESULT result, Bit32s *size)
{
  hvm_status r;
  hvm_address cr3;

  if(pcmd->nargs == 0) {
    *size = ERROR_MISSING_PARAM;;
    return;
  }
  if(pcmd->nargs >= 2) {
    *size = vmm_atoi(pcmd->args[1]);
    if(*size <= 0) {
      *size = ERROR_INVALID_SIZE;
      return;
    }
    if(*size > 512) *size = 512;
    
    if(pcmd->nargs >= 3) {
     
      if(vmm_strncmpi(pcmd->args[2], "char", 4) == 0) {
	result->memory_dump.with_char_rep = TRUE;
	cr3 = context.GuestContext.cr3;
      }
      else {
      
	/* Translate the target cr3 */
	if(!vmm_strtoul(pcmd->args[2], &cr3)) {
	  *size = ERROR_INVALID_MEMORY;
	  return;
	}
 
	if(pcmd->nargs == 4 && vmm_strncmpi(pcmd->args[3], "char", 4) == 0)
	  result->memory_dump.with_char_rep = TRUE;
	else
	  result->memory_dump.with_char_rep = FALSE;
      }
    }
    else cr3 = context.GuestContext.cr3;
  }
  else {
    *size = 1;
    cr3 = context.GuestContext.cr3;
  }

  /* This means we want to dump memory starting from a CPU register */
  if(pcmd->args[0][0] == '$') {
    if(!GetRegFromStr(&pcmd->args[0][1], &(result->memory_dump.baseaddr))) {
      *size = ERROR_INVALID_REGISTER; 
      return;
    }
  }
  else {
    if(!vmm_strtoul(pcmd->args[0], &(result->memory_dump.baseaddr))) {
      *size = ERROR_INVALID_ADDR;
      return;
    }
  }

  /* Check if the address is ok! */ 
  if(!MmuIsAddressValid(cr3, result->memory_dump.baseaddr)) {
    *size = ERROR_INVALID_MEMORY;
    return;
  }

  r = MmuReadVirtualRegion(cr3, result->memory_dump.baseaddr, result->memory_dump.data, sizeof(Bit32s)*(*size));
  if(r != HVM_STATUS_SUCCESS)
    *size = ERROR_COMMAND_SPECIFIC;
}

static void CmdSwBreakpoint(PHYPERDBG_CMD pcmd, PCMD_RESULT result, hvm_bool isPerm)
{
  hvm_address cr3;
  hvm_bool isCr3Dipendent;

  result->bpinfo.addr = 0;
  isCr3Dipendent = FALSE;

  if(pcmd->nargs == 0) {
    result->bpinfo.error_code = ERROR_MISSING_PARAM;
    return;
  }

  /* Check if the param is a symbol name */
  if(pcmd->args[0][0] == '$') {
    result->bpinfo.symbol = SymbolGetFromName((Bit8u*) &pcmd->args[0][1]);
    if(result->bpinfo.symbol) {
      result->bpinfo.addr = result->bpinfo.symbol->addr + hyperdbg_state.win_state.kernel_base;
    }
    else {
      Log("[HyperDbg] Invalid symbol %s!", &pcmd->args[0][1]);
      result->bpinfo.error_code = ERROR_INVALID_SYMBOL;
      return;
    }
  }
  else {
    /* Translate the addr param */
    if(!vmm_strtoul(pcmd->args[0], &(result->bpinfo.addr))) {
      result->bpinfo.error_code = ERROR_INVALID_ADDR;
      return;
    }
  }

  if(pcmd->nargs >= 2) {

    /* Translate the target cr3 */
    if(!vmm_strtoul(pcmd->args[1], &cr3)) {
      result->bpinfo.error_code = ERROR_INVALID_MEMORY;
      return;
    }
    isCr3Dipendent = TRUE;
  }
  else cr3 = context.GuestContext.cr3;

  /* Check if the address is ok */
  if(!MmuIsAddressValid(cr3, result->bpinfo.addr)) {
    Log("[HyperDbg] Invalid memory address!");
    result->bpinfo.error_code = ERROR_INVALID_MEMORY;
    return;
  }

  result->bpinfo.bp_index = SwBreakpointSet(cr3, result->bpinfo.addr, isPerm, isCr3Dipendent);

  if(result->bpinfo.bp_index == MAXSWBPS) {
    result->bpinfo.error_code = ERROR_COMMAND_SPECIFIC;
    return;
  }
  if(result->bpinfo.bp_index == -1) {
    result->bpinfo.error_code = ERROR_DOUBLED_BP;
    return;
  }

  if(cr3 != context.GuestContext.cr3) {
    hyperdbg_state.bp_addr_target = result->bpinfo.addr;
    hyperdbg_state.target_cr3 = cr3;
  }

  result->bpinfo.error_code = 0;
}

static void CmdDeleteSwBreakpoint(PHYPERDBG_CMD pcmd, PCMD_RESULT result)
{
  Bit32s index = 0;

  if(pcmd->nargs == 0) {
    result->bpinfo.error_code = ERROR_MISSING_PARAM;
    return;
  }

  index = vmm_atoi(pcmd->args[0]);
  if(index < 0 || index > MAXSWBPS) {
    (result->bpinfo).error_code = ERROR_NOSUCH_BP;
    return;
  }
  (result->bpinfo).bp_index = index; /* This can't be < 0 so no problem with signed/unsigned conversion */
  if(!SwBreakpointDeleteById((result->bpinfo).bp_index)) {
    result->bpinfo.error_code = ERROR_NOSUCH_BP;
  }
}

static void CmdSetSingleStep(PHYPERDBG_CMD pcmd)
{
  hvm_address flags;

  /* Fetch guest EFLAGS */
  flags = context.GuestContext.rflags;

  /* Enable single-step, but don't trap current instruction */
  flags |= FLAGS_TF_MASK;
  flags |= FLAGS_RF_MASK;
  /* Temporarly disable interrupts in the guest so that the single-stepping instruction isn't interrupted */
  flags &= ~FLAGS_IF_MASK;
  /* Store info on old flags */
  hyperdbg_state.TF_on = (context.GuestContext.rflags & FLAGS_TF_MASK) != 0 ? TRUE : FALSE;
  hyperdbg_state.IF_on = (context.GuestContext.rflags & FLAGS_IF_MASK) != 0 ? TRUE : FALSE;

  /* These will be written @ vmentry */
  context.GuestContext.rflags = flags; 

  /* Update HyperDbg state structure */
  hyperdbg_state.singlestepping = TRUE;
}

static void CmdDisassemble(PHYPERDBG_CMD pcmd, PCMD_RESULT result, Bit32s *size)
{
  hvm_address addr, tmpaddr, i, operand, cr3;
  hvm_address y;
  ud_t ud_obj;
  Bit8u* disasinst;
  Bit8u disasbuf[128];
  PSYMBOL sym;
  
  y = 0;
  addr = 0;

  /* Check if we have to use RIP and if it's valid */
  if(pcmd->nargs == 0) {
    if(MmuIsAddressValid(context.GuestContext.cr3, context.GuestContext.rip)) {
      addr = context.GuestContext.rip;
    } else {
      Log("[HyperDbg] RIP is not valid: %.8x", context.GuestContext.rip);
      *size = ERROR_COMMAND_SPECIFIC;
      return;
    }
    cr3 = context.GuestContext.cr3;
  }
  else {
    if(!vmm_strtoul(pcmd->args[0], &addr)) {
      *size = ERROR_INVALID_ADDR;
      return;
    }

    if(pcmd->nargs == 2) {

      /* Translate the target cr3 */
      if(!vmm_strtoul(pcmd->args[1], &cr3)) {
	result->bpinfo.error_code = ERROR_INVALID_MEMORY;
	return;
      }
    }
    else cr3 = context.GuestContext.cr3;
      
    /* Check if the address is ok */
    if(!MmuIsAddressValid(cr3, addr)) {
      Log("[HyperDbg] Invalid memory address: %.8x", addr);
      *size = ERROR_INVALID_MEMORY;
      return;
    }
  }

  tmpaddr = addr;

  /* Setup disassembler structure */
  ud_init(&ud_obj);
  ud_set_mode(&ud_obj, 32);
  ud_set_syntax(&ud_obj, UD_SYN_ATT);
  
  MmuReadVirtualRegion(cr3, addr, disasbuf, sizeof(disasbuf) ); ///sizeof(Bit8u));
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

    result->instructions[y].addr = tmpaddr;
    vmm_strncpy(result->instructions[y].hexcode, ud_insn_hex(&ud_obj), 32);
    vmm_strncpy(result->instructions[y].asmcode, ud_insn_asm(&ud_obj), 64);
    if(sym) {
      vmm_strncpy(result->instructions[y].sym_name, (sym->name), 64);
      result->instructions[y].sym_exists = TRUE;
    } else result->instructions[y].sym_exists = FALSE;

    tmpaddr += ud_insn_len(&ud_obj);
    y++;
    /* Check if maximum size have been reached */
    if(y >= OUT_SIZE_Y) break;
  }

  *size = y-1;
}

static void CmdBacktrace(PHYPERDBG_CMD pcmd)
{
  Bit32u current_frame, nframes, n;
  hvm_address current_rip;
  hvm_status r;
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
  current_frame = context.GuestContext.rbp;
  nframes = 0;

  while(1) {
    r = MmuReadVirtualRegion(context.GuestContext.cr3, current_frame+4, &current_rip, sizeof(current_rip));

    if(r != HVM_STATUS_SUCCESS) break;

    vmm_memset(namec, 0, sizeof(namec));
    vmm_memset(sym, 0, sizeof(sym));
    vmm_memset(module, 0, sizeof(module));

#ifdef GUEST_WINDOWS
    PSYMBOL nearsym;
    Bit16u name[64];
    if(current_rip >= hyperdbg_state.win_state.kernel_base) {
      
      r = WindowsFindModule(context.GuestContext.cr3, current_rip, name, sizeof(name)/sizeof(Bit16u));
      if(r == HVM_STATUS_SUCCESS) {
	wide2ansi(namec, (Bit8u*)name, sizeof(namec)/sizeof(Bit8s));
	if(vmm_strlen(namec) > 0)
	  vmm_snprintf(module, 96, "[%s]", namec);
      }
      nearsym = SymbolGetNearest((hvm_address) current_rip);
      if(nearsym) {
	if(current_rip-(nearsym->addr + hyperdbg_state.win_state.kernel_base) < 100000) /* FIXME: tune this param */
	  vmm_snprintf(sym, 96, "(%s+%d)", nearsym->name, (current_rip-(nearsym->addr + hyperdbg_state.win_state.kernel_base)));
      }
    }
#elif defined GUEST_LINUX
     
#endif

    vmm_snprintf(out_matrix[nframes], OUT_SIZE_X, "[%02d] @%.8hx %.8hx %-40s %s\n", nframes, current_frame, current_rip, sym, module);

    nframes += 1;
    if ((n != 0 && nframes > n) || nframes >= OUT_SIZE_Y) {
      break;
    }

    /* Go on with the next frame */
    r = MmuReadVirtualRegion(context.GuestContext.cr3, current_frame, &current_frame, sizeof(current_frame));
    if (r != HVM_STATUS_SUCCESS) break;
  }
  VideoRefreshOutArea(LIGHT_GREEN);
}

static void CmdLookupSymbol(PHYPERDBG_CMD pcmd, hvm_bool bExactMatch)
{
  hvm_address base;
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
    if(psym)
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Nearest symbol smaller than %08hx: %s (@%08hx)", 
		   base, psym->name, psym->addr + hyperdbg_state.win_state.kernel_base);
    else
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "No symbol found!");
  }

  VideoRefreshOutArea(LIGHT_GREEN);
}

static hvm_bool GetRegFromStr(Bit8u *name, hvm_address *value)
{
  if(vmm_strncmpi(name, "RAX", 3) == 0) { *value = context.GuestContext.rax; return TRUE; }
  if(vmm_strncmpi(name, "RBX", 3) == 0) { *value = context.GuestContext.rbx; return TRUE; }
  if(vmm_strncmpi(name, "RCX", 3) == 0) { *value = context.GuestContext.rcx; return TRUE; }
  if(vmm_strncmpi(name, "RDX", 3) == 0) { *value = context.GuestContext.rdx; return TRUE; }
  if(vmm_strncmpi(name, "RSP", 3) == 0) { *value = context.GuestContext.rsp; return TRUE; }
  if(vmm_strncmpi(name, "RBP", 3) == 0) { *value = context.GuestContext.rbp; return TRUE; }
  if(vmm_strncmpi(name, "RSI", 3) == 0) { *value = context.GuestContext.rsi; return TRUE; }
  if(vmm_strncmpi(name, "RDI", 3) == 0) { *value = context.GuestContext.rsi; return TRUE; }
  if(vmm_strncmpi(name, "RIP", 3) == 0) { *value = context.GuestContext.rip; return TRUE; }
  if(vmm_strncmpi(name, "CR0", 3) == 0) { *value = context.GuestContext.cr0; return TRUE; }
  if(vmm_strncmpi(name, "CR3", 3) == 0) { *value = context.GuestContext.cr3; return TRUE; }
  if(vmm_strncmpi(name, "CR4", 3) == 0) { *value = context.GuestContext.cr4; return TRUE; }
  return FALSE;
}

static void ParseCommand(Bit8u *buffer, PHYPERDBG_CMD pcmd)
{
  char *p;
  int i, n;

  /* Initialize */
  vmm_memset(pcmd, 0, sizeof(HYPERDBG_CMD));

  p = buffer;

  /* Skip leading spaces */
  while (*p != '\0' && CHAR_IS_SPACE(*p)) p++;

#define PARSE_COMMAND(c)			\
  case HYPERDBG_CMD_CHAR_##c:			\
    pcmd->opcode = HYPERDBG_CMD_##c;		\
    break;

  switch(p[0]) {
    PARSE_COMMAND(HELP);
    PARSE_COMMAND(SHOWREGISTERS);
    PARSE_COMMAND(SHOWPROCESSES);
    PARSE_COMMAND(SHOWMODULES);
    PARSE_COMMAND(SHOWSOCKETS);
    PARSE_COMMAND(DUMPMEMORY);
    PARSE_COMMAND(SW_BP);
    PARSE_COMMAND(SW_PERM_BP);
    PARSE_COMMAND(DELETE_SW_BP);
    PARSE_COMMAND(LIST_BP);
    PARSE_COMMAND(SINGLESTEP);
    PARSE_COMMAND(DISAS);
    PARSE_COMMAND(BACKTRACE);
    PARSE_COMMAND(CONTINUE);
    PARSE_COMMAND(SYMBOL);
    PARSE_COMMAND(SYMBOL_NEAREST);
    PARSE_COMMAND(INFO);
    PARSE_COMMAND(UNLINK_PROC);
    PARSE_COMMAND(RELINK_PROC);
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
