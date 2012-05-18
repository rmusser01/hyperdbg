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
  
  nYou should have received a copy of the GNU General Public License along with
  this program. If not, see <http://www.gnu.org/licenses/>.
  
*/

#include "hyperdbg_print.h"
#include "hyperdbg_cmd.h"
#include "vmmstring.h"
#include "video.h"
#include "gui.h"
#include "pager.h"

void PrintHelp()
{
  int i;

  VideoResetOutMatrix();

  i = 0;

  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "Available commands:");
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - show this help screen", HYPERDBG_CMD_CHAR_HELP);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - dump guest registers",  HYPERDBG_CMD_CHAR_SHOWREGISTERS);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - dump guest processes",  HYPERDBG_CMD_CHAR_SHOWPROCESSES);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - dump guest kernel modules",  HYPERDBG_CMD_CHAR_SHOWMODULES);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - dump guest network connections",  HYPERDBG_CMD_CHAR_SHOWSOCKETS);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c addr|$reg [y [char]] [cr3] - dump y dwords starting from addr or register reg", HYPERDBG_CMD_CHAR_DUMPMEMORY);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c addr|$symbol [cr3] - set sw breakpoint @ address addr or at address of $symbol", HYPERDBG_CMD_CHAR_SW_BP);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c addr|$symbol [cr3] - set permanent sw breakpoint @ address addr or at address of $symbol", HYPERDBG_CMD_CHAR_SW_PERM_BP);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c id - delete sw breakpoint #id (always check id with BP listing)", HYPERDBG_CMD_CHAR_DELETE_SW_BP);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - list sw breakpoints", HYPERDBG_CMD_CHAR_LIST_BP);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - single step", HYPERDBG_CMD_CHAR_SINGLESTEP);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c [addr] [cr3] - disassemble starting from addr (default rip)", HYPERDBG_CMD_CHAR_DISAS);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - continue execution", HYPERDBG_CMD_CHAR_CONTINUE);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c n - print backtrace of n stack frames", HYPERDBG_CMD_CHAR_BACKTRACE);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c addr - lookup symbol associated with address addr", HYPERDBG_CMD_CHAR_SYMBOL);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c addr - lookup nearest symbol to address addr", HYPERDBG_CMD_CHAR_SYMBOL_NEAREST);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - show info on HyperDbg", HYPERDBG_CMD_CHAR_INFO);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "");
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "ONLY FOR WINDOWS 7");
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c cr3 - freeze process with specified cr3", HYPERDBG_CMD_CHAR_UNLINK_PROC);
  vmm_snprintf(out_matrix[i++], OUT_SIZE_X, "%c - unfreeze previous freezed process", HYPERDBG_CMD_CHAR_RELINK_PROC);

  VideoRefreshOutArea(LIGHT_GREEN);
}

void PrintRegisters(PCMD_RESULT buffer)
{
  VideoResetOutMatrix();

  vmm_snprintf(out_matrix[0], OUT_SIZE_X,   "RAX        0x%08hx", buffer->registers.rax);
  vmm_snprintf(out_matrix[1], OUT_SIZE_X,   "RBX        0x%08hx", buffer->registers.rbx);
  vmm_snprintf(out_matrix[2], OUT_SIZE_X,   "RCX        0x%08hx", buffer->registers.rcx);
  vmm_snprintf(out_matrix[3], OUT_SIZE_X,   "RDX        0x%08hx", buffer->registers.rdx);
  vmm_snprintf(out_matrix[4], OUT_SIZE_X,   "RSP        0x%08hx", buffer->registers.rsp);
  vmm_snprintf(out_matrix[5], OUT_SIZE_X,   "RBP        0x%08hx", buffer->registers.rbp);
  vmm_snprintf(out_matrix[6], OUT_SIZE_X,   "RSI        0x%08hx", buffer->registers.rsi);
  vmm_snprintf(out_matrix[7], OUT_SIZE_X,   "RDI        0x%08hx", buffer->registers.rdi);
  vmm_snprintf(out_matrix[8], OUT_SIZE_X,   "RIP        0x%08hx", buffer->registers.rip);
  vmm_snprintf(out_matrix[9], OUT_SIZE_X,   "Resume RIP 0x%08hx", buffer->registers.resumerip);
  vmm_snprintf(out_matrix[10], OUT_SIZE_X,  "RFLAGS     0x%08hx", buffer->registers.rflags);
  vmm_snprintf(out_matrix[11], OUT_SIZE_X,  "CR0        0x%08hx", buffer->registers.cr0);
  vmm_snprintf(out_matrix[12], OUT_SIZE_X,  "CR3        0x%08hx", buffer->registers.cr3);
  vmm_snprintf(out_matrix[13], OUT_SIZE_X,  "CR4        0x%08hx", buffer->registers.cr4);
  vmm_snprintf(out_matrix[14], OUT_SIZE_X,  "CS         0x%04hx", buffer->registers.cs);

  VideoRefreshOutArea(LIGHT_GREEN);
}

void PrintProcesses(PCMD_RESULT buffer, Bit32s size)
{
  int i = 0;
  char tmp[OUT_SIZE_X];

  VideoResetOutMatrix();

  if(size == ERROR_COMMAND_SPECIFIC) {
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "error while retrieving active processes!");
    VideoRefreshOutArea(RED);
    return;
  }

  vmm_snprintf(tmp, sizeof(tmp),  "         CR3              PID             Name");
  PagerAddLine(tmp);

  for (i=0; i<size; i++) {

    /* Print data */
    vmm_snprintf(tmp, sizeof(tmp),  "%.3d.     0x%08hx       0x%08hx      %s", i, buffer->processes[i].cr3, buffer->processes[i].pid, buffer->processes[i].name);
    PagerAddLine(tmp);
  }

  PagerLoop(LIGHT_GREEN);
}

void PrintModules(PCMD_RESULT buffer, Bit32s size)
{
  int i = 0;
  char tmp[OUT_SIZE_X];

  VideoResetOutMatrix();

  if(size == ERROR_COMMAND_SPECIFIC) {
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "error while retrieving kernel modules!");
    VideoRefreshOutArea(RED);
    return;
  }

  vmm_snprintf(tmp, sizeof(tmp),  "         Base             Entry           Name");
  PagerAddLine(tmp);

  for (i=0; i<size; i++) {

    /* Print data */
    vmm_snprintf(tmp, sizeof(tmp),  "%.3d.     0x%08hx       0x%08hx      %s", i, buffer->modules[i].baseaddr, buffer->modules[i].entrypoint, buffer->modules[i].name);
    PagerAddLine(tmp);
  }

  PagerLoop(LIGHT_GREEN);
}

void PrintSockets(PCMD_RESULT buffer, Bit32s size)
{
  hvm_status r;
  Bit32u i = 0;
  char s1[16], s2[16], s1_ipv6[40], s2_ipv6[40], tmp[OUT_SIZE_X], name[32];
  unsigned int ipv6_entries[128] = {0}, entries = -1;

  VideoResetOutMatrix();

  if(size == ERROR_COMMAND_SPECIFIC) {
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "error while retrieving network connections!");
    VideoRefreshOutArea(RED);
    return;
  }

  vmm_snprintf(tmp, sizeof(tmp), "      Proto     Local Addr.  LPort         Remote Addr.  RPort   State    PID/Name");
  PagerAddLine(tmp);
  vmm_snprintf(tmp, sizeof(tmp), "");
  PagerAddLine(tmp);

  for (i=0; i<size; i++) {
    
    if(buffer->sockets[i].local_ip != -1) {

      entries++;

      vmm_memset(s1, 0, sizeof(s1));
      inet_ntoa(buffer->sockets[i].local_ip, s1);
        
      vmm_memset(name, 0, 32);
      r = ProcessGetNameByPid(context.GuestContext.cr3, buffer->sockets[i].pid, name);
      if(r != HVM_STATUS_SUCCESS)
	vmm_strncpy(name, "UNKNOWN", 7);

      switch (buffer->sockets[i].state) {
      case SocketStateEstablished:
      
      	vmm_memset(s2, 0, sizeof(s2));
	inet_ntoa(buffer->sockets[i].remote_ip, s2);

	vmm_snprintf(tmp, sizeof(tmp), "[%03d]  TCP  % 15s  %5d  --> % 15s  %5d     E     %5d/%s", entries, s1, buffer->sockets[i].local_port, s2, buffer->sockets[i].remote_port, buffer->sockets[i].pid, name);
	PagerAddLine(tmp);
	break;
      case SocketStateListen:

	if(buffer->sockets[i].local_ip != -1)
	  vmm_snprintf(tmp, sizeof(tmp), "[%03d]  %s  % 15s  %5d              0.0.0.0      *     L     %5d/%s", entries, buffer->sockets[i].protocol == 6 ? "TCP":"UDP", s1, buffer->sockets[i].local_port, buffer->sockets[i].pid, name);
	PagerAddLine(tmp);
	break;
      default:
	/* vmm_snprintf(tmp, sizeof(tmp), "[%.3d] Unknown socket state\n", entries); */
	entries--;
	break;
      }
    }
    else {
      ipv6_entries[i] = 1;
    }
  }
  
  vmm_snprintf(tmp, sizeof(tmp), "");
  PagerAddLine(tmp);

  for (i=0; i<size; i++) {
    
    if(ipv6_entries[i] == 1) {

      entries++;

      vmm_memset(s1_ipv6, 0, sizeof(s1_ipv6));
      inet_ntoa_v6(buffer->sockets[i].local_ipv6, s1_ipv6);
    
      vmm_memset(name, 0, 32);
      r = ProcessGetNameByPid(context.GuestContext.cr3, buffer->sockets[i].pid, name);
      if(r == HVM_STATUS_UNSUCCESSFUL)
	vmm_strncpy(name, "UNKNOWN", 7);

      switch (buffer->sockets[i].state) {
      case SocketStateEstablished:
      
	vmm_memset(s2_ipv6, 0, sizeof(s2_ipv6));
	inet_ntoa_v6(buffer->sockets[i].remote_ipv6, s2_ipv6);

	vmm_snprintf(tmp, sizeof(tmp), "[%03d]  TCP", entries);
	PagerAddLine(tmp);
	vmm_snprintf(tmp, sizeof(tmp), "             Local ip:    %s", s1_ipv6);
	PagerAddLine(tmp);
	vmm_snprintf(tmp, sizeof(tmp), "	            Local port:  %d", buffer->sockets[i].local_port);
	PagerAddLine(tmp);
	vmm_snprintf(tmp, sizeof(tmp), "             Remote ip:   %s", s2_ipv6);
	PagerAddLine(tmp);
	vmm_snprintf(tmp, sizeof(tmp), "	            Remote port: %d", buffer->sockets[i].remote_port);
	PagerAddLine(tmp);
	vmm_snprintf(tmp, sizeof(tmp), "             State:       Established");
	PagerAddLine(tmp);
	vmm_snprintf(tmp, sizeof(tmp), "             Pid/Name:    %d/%s", buffer->sockets[i].pid, name);
	PagerAddLine(tmp);
	break;
      case SocketStateListen:

	vmm_snprintf(tmp, sizeof(tmp), "[%03d]  %s", entries, buffer->sockets[i].protocol == 6 ? "TCP":"UDP");
	PagerAddLine(tmp);
	vmm_snprintf(tmp, sizeof(tmp), "             Local ip:    %s", s1_ipv6);
	PagerAddLine(tmp);
	vmm_snprintf(tmp, sizeof(tmp), "	            Local port:  %d", buffer->sockets[i].local_port);
	PagerAddLine(tmp);
	vmm_snprintf(tmp, sizeof(tmp), "             Remote ip:   0:0:0:0:0:0:0:0");
	PagerAddLine(tmp);
	vmm_snprintf(tmp, sizeof(tmp), "	            Remote port: *");
	PagerAddLine(tmp);
	vmm_snprintf(tmp, sizeof(tmp), "             State:       Listening");
	PagerAddLine(tmp);
	vmm_snprintf(tmp, sizeof(tmp), "             Pid/Name:    %d/%s", buffer->sockets[i].pid, name);
	PagerAddLine(tmp);
	break;
      default:
	/* vmm_snprintf(tmp, sizeof(tmp), "[%.3d] Unknown socket state\n", entries); */
	entries--;
	break;
      }
    }
  }

  PagerLoop(LIGHT_GREEN);
}

void PrintBPList(PCMD_RESULT buffer)
{
  Bit32u i;
  char tmp[OUT_SIZE_X];

  VideoResetOutMatrix();

  vmm_snprintf(tmp, sizeof(tmp),  "ID        Cr3             Address             Perm?             Cr3Dependent?");

  for (i=0; i<MAXSWBPS; i++) {

    if(buffer->bplist[i].Addr != 0) {
      PagerAddLine(tmp);
      /* Print data */
      vmm_snprintf(tmp, sizeof(tmp),  "%.3d.      0x%08hx      0x%08hx          %5s             %5s", i, buffer->bplist[i].cr3, buffer->bplist[i].Addr, buffer->bplist[i].isPerm?"TRUE":"FALSE", buffer->bplist[i].isCr3Dipendent?"TRUE":"FALSE");
    }
  }

  if(i == 0) {
    vmm_snprintf(tmp, sizeof(tmp),  "No breakpoints.");
  }
  
  PagerAddLine(tmp);
  PagerLoop(LIGHT_GREEN);
}

void PrintUnlinkProc(Bit32s error_code)
{
  VideoResetOutMatrix();  

  switch(error_code) {
  case ERROR_MISSING_PARAM:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Parameter cr3 missing!");
    VideoRefreshOutArea(RED);
    return;
  case ERROR_INVALID_ADDR:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid cr3 parameter!");
    VideoRefreshOutArea(RED);
    return;
  case ERROR_COMMAND_SPECIFIC:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Error while freezing process!");
    VideoRefreshOutArea(RED);
    return;
  }

  vmm_snprintf(out_matrix[0], OUT_SIZE_X,   "Process freezed!");

  VideoRefreshOutArea(LIGHT_GREEN);
}

void PrintRelinkProc(Bit32s error_code)
{
  VideoResetOutMatrix();

  if(error_code == ERROR_COMMAND_SPECIFIC) {
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Error while unfreezing process!");
    VideoRefreshOutArea(RED);
    return;
  }

  vmm_snprintf(out_matrix[0], OUT_SIZE_X,   "Process unfreezed!");

  VideoRefreshOutArea(LIGHT_GREEN);
}

void PrintMemoryDump(PCMD_RESULT buffer, Bit32s size)
{
  Bit32u i, x, y, div, c;
  char a, s, d, f;
  Bit32u buf[4];

  x = 0;
  y = 0;
  VideoResetOutMatrix();
 
  switch(size) {
  case ERROR_MISSING_PARAM:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Parameter addr missing!");
    VideoRefreshOutArea(RED);
    return;
  case ERROR_INVALID_SIZE:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid size parameter!");
    VideoRefreshOutArea(RED);
    return;
  case ERROR_INVALID_REGISTER:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid register!");
    VideoRefreshOutArea(RED);
    return;
  case ERROR_INVALID_ADDR:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid addr parameter!");
    VideoRefreshOutArea(RED);
    return;
  case ERROR_INVALID_MEMORY:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid memory address!");
    VideoRefreshOutArea(RED);
    return;
  default:
    break;
  }

  if(buffer->memory_dump.with_char_rep == TRUE) div = 4;
  else div = 8;

  /* Print the left column of addresses */
  for(i = 0; i <= size/div; i++) {
    if(y >= OUT_SIZE_Y) break;
    /* In the case it's a multiple of 8 bytes, skip last one */
    if((i == size/div) && size%div == 0) break;
    vmm_snprintf(out_matrix[y], 11, "%08hx->", buffer->memory_dump.baseaddr+(i*div*4));
    y++;
  }
  VideoRefreshOutArea(LIGHT_BLUE);
  
  /* We can print a maximum of 7*row addresses */
  y = 0;
  x = 10;
  c = 0;
  for(i = 0; i < size; i++) {
    if(i % div == 0) {
      if(buffer->memory_dump.with_char_rep == TRUE && i != 0) {
	vmm_snprintf(&out_matrix[y][x], 9, "         ");
	x += 9;
	for(c = 0; c < MIN(size, 4); c++) {
	  a = (char) ((buf[c] >> 24) & 0xff);
	  s = (char) ((buf[c] >> 16) & 0xff);
	  d = (char) ((buf[c] >>  8) & 0xff);
	  f = (char) ((buf[c] >>  0) & 0xff);
	  a = vmm_isalpha(a)?a:'.';
	  s = vmm_isalpha(s)?s:'.';
	  d = vmm_isalpha(d)?d:'.';
	  f = vmm_isalpha(f)?f:'.';
	  vmm_snprintf(&out_matrix[y][x], 5, "%c%c%c%c", a, s, d, f);
	  x += 5;
	}
      }
      x = 10;
      if(i != 0) y++;
    }
	
    /* Check if we reached the maximum number of lines */
    if(y >= OUT_SIZE_Y) break;

    if(buffer->memory_dump.with_char_rep == TRUE) {
      if(c % 4 == 0) c = 0;
      buf[c++] = buffer->memory_dump.data[i];
    }
    vmm_snprintf(&out_matrix[y][x], 9, "%08hx", buffer->memory_dump.data[i]);
    x += 9;
    if(buffer->memory_dump.with_char_rep == TRUE && i == size-1) {
      vmm_snprintf(&out_matrix[y][x], 9, "         ");
      x += 9;
      for(c = 0; c < MIN(size, 4); c++) {
	a = (char) ((buf[c] >> 24) & 0xff);
	s = (char) ((buf[c] >> 16) & 0xff);
	d = (char) ((buf[c] >>  8) & 0xff);
	f = (char) ((buf[c] >>  0) & 0xff);
	a = vmm_isalpha(a)?a:'.';
	s = vmm_isalpha(s)?s:'.';
	d = vmm_isalpha(d)?d:'.';
	f = vmm_isalpha(f)?f:'.';
	vmm_snprintf(&out_matrix[y][x], 5, "%c%c%c%c", a, s, d, f);
	x += 5;
      }
    }
  }
  VideoRefreshOutArea(LIGHT_GREEN);
}

void PrintSwBreakpoint(PCMD_RESULT buffer)
{
  /* Prepare shell for output */
  VideoResetOutMatrix();

  switch(buffer->bpinfo.error_code) {

  case ERROR_MISSING_PARAM:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Parameter missing!");
    VideoRefreshOutArea(RED);
    return;
  case ERROR_INVALID_SYMBOL:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Undefined symbol!");
    VideoRefreshOutArea(RED);
    return;
  case ERROR_INVALID_ADDR:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid addr parameter!");
    VideoRefreshOutArea(RED);
    return;
  case ERROR_INVALID_MEMORY:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid memory or invalid cr3!");
    VideoRefreshOutArea(RED);
    return;
  case ERROR_COMMAND_SPECIFIC:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Unable to set bp #%d", buffer->bpinfo.bp_index);
    VideoRefreshOutArea(RED);
    return;
  case ERROR_DOUBLED_BP:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "There is another bp with same address, same cr3!", buffer->bpinfo.bp_index);
    VideoRefreshOutArea(RED);
    return;
  }

  vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Set bp #%d @ 0x%08hx", buffer->bpinfo.bp_index, buffer->bpinfo.addr);

  VideoRefreshOutArea(LIGHT_GREEN);
}

void PrintDeleteSwBreakpoint(PCMD_RESULT buffer)
{
  /* Prepare shell for output */
  VideoResetOutMatrix();

  switch(buffer->bpinfo.error_code) {
  case ERROR_MISSING_PARAM:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Parameter missing!");
    VideoRefreshOutArea(RED);
    return;
  case ERROR_NOSUCH_BP:
      vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Bp not found!");
      VideoRefreshOutArea(RED);
      return;
  }

  vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Deleted bp #%d!", buffer->bpinfo.bp_index);   

  VideoRefreshOutArea(LIGHT_GREEN);
}

void PrintDisassembled(PCMD_RESULT buffer, Bit32s size)
{
  unsigned int i;

  VideoResetOutMatrix();

  switch(size) {

  case ERROR_COMMAND_SPECIFIC:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid RIP!");
    VideoRefreshOutArea(RED);
    return;
  case ERROR_INVALID_ADDR:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid addr parameter!");
    VideoRefreshOutArea(RED);
    return;
  case ERROR_INVALID_MEMORY:
    vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Invalid memory address!");
    VideoRefreshOutArea(RED);
    return;
  }

  for(i = 0; i < size; i++) {

    if(buffer->instructions[i].sym_exists == TRUE)
      vmm_snprintf(out_matrix[i], OUT_SIZE_X, "%08hx: %-24s %s <%s>", buffer->instructions[i].addr, buffer->instructions[i].hexcode, buffer->instructions[i].asmcode, buffer->instructions[i].sym_name);
    else
      vmm_snprintf(out_matrix[i], OUT_SIZE_X, "%08hx: %-24s %s", buffer->instructions[i].addr, buffer->instructions[i].hexcode, buffer->instructions[i].asmcode);
  }

  VideoRefreshOutArea(LIGHT_GREEN);
}

void PrintInfo()
{
  Bit32u start;

  VideoResetOutMatrix();

  start = 8;

  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    ***********************************************************                   ");
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    *                                                         *                   ");
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    *                     HyperDbg %d                   *                   ", HYPERDBG_VERSION);
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    *            " HYPERDBG_URL "           *                   ");
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    *          Coded by: martignlo, roby and joystick         *                   ");
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    *      {lorenzo.martignoni,roberto.paleari}@gmail.com     *                   ");
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    *             joystick@security.dico.unimi.it             *                   ");
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    *                                                         *                   ");
  vmm_snprintf(out_matrix[start++], OUT_SIZE_X, "                    ***********************************************************                   ");
  VideoRefreshOutArea(LIGHT_GREEN);
}

void PrintUnknown()
{
  VideoResetOutMatrix();
  vmm_snprintf(out_matrix[0], OUT_SIZE_X, "Unknown command");
  VideoRefreshOutArea(RED);
}
