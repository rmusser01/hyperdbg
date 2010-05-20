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
#include "vmmstring.h"
#include "video.h"
#include "debug.h"
#include "gui.h"
#include "mmu.h"
#include "common.h"
#include "extern.h"    /* From libudis */
#include "symsearch.h"
#ifdef GUEST_WINDOWS
#include "winxp.h"
#endif

/* ################# */
/* #### GLOBALS #### */
/* ################# */

Bit8u out_matrix[OUT_SIZE_Y][OUT_SIZE_X];
Bit8u out_matrix_cache[OUT_SIZE_Y][OUT_SIZE_X];

/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

static void VideoShowDisassembled(void);

/* ################ */
/* #### BODIES #### */
/* ################ */

void VideoUpdateShell(Bit8u* buffer)
{
  int i;

  /* Erase previous input line */
  for(i = 4; i < SHELL_SIZE_X-1; i++) {
    VideoWriteChar(' ', BLACK, i, SHELL_SIZE_Y-2);
  }
  /* Draw current input buffer */
  VideoWriteString(buffer, MAX_INPUT_SIZE, WHITE, 4, SHELL_SIZE_Y-2);
}

/* FIXME: after we decide new size of the shell we have to reset all sizes */
void VideoInitShell(void)
{
  int i;
  char tmp[SHELL_SIZE_X];

  VideoClear(BGCOLOR);

  VideoDrawFrame();

  /* 1st row */
  VideoWriteString("RAX=", 4, LIGHT_BLUE, 2, 1);
  vmm_snprintf(tmp, 9, "%08hx", context.GuestContext.RAX);
  VideoWriteString(tmp, 8, LIGHT_GREEN, 6, 1);

  VideoWriteString("RBX=", 4, LIGHT_BLUE, 15, 1);
  vmm_snprintf(tmp, 9, "%08hx",   context.GuestContext.RBX);
  VideoWriteString(tmp, 8, LIGHT_GREEN, 19, 1);

  VideoWriteString("RCX=", 4, LIGHT_BLUE, 28, 1);
  vmm_snprintf(tmp, 9, "%08hx",   context.GuestContext.RCX);
  VideoWriteString(tmp, 8, LIGHT_GREEN, 32, 1);
	
  VideoWriteString("RDX=", 4, LIGHT_BLUE, 41, 1);
  vmm_snprintf(tmp, 9, "%08hx", context.GuestContext.RDX);
  VideoWriteString(tmp, 8, LIGHT_GREEN, 45, 1);

  VideoWriteString("RSP=", 4, LIGHT_BLUE, 54, 1);
  vmm_snprintf(tmp, 9, "%08hx",   context.GuestContext.RSP);
  VideoWriteString(tmp, 8, LIGHT_GREEN, 58, 1);

  VideoWriteString("RBP=", 4, LIGHT_BLUE, 67, 1);
  vmm_snprintf(tmp, 9, "%08hx",   context.GuestContext.RBP);
  VideoWriteString(tmp, 8, LIGHT_GREEN, 71, 1);

  VideoWriteString("RIP=", 4, LIGHT_BLUE, 80, 1);
  vmm_snprintf(tmp, 9, "%08hx",   context.GuestContext.RIP);
  VideoWriteString(tmp, 8, LIGHT_GREEN, 84, 1);

  /* 2nd row */
  VideoWriteString("RSI=", 4, LIGHT_BLUE, 2, 2);
  vmm_snprintf(tmp, 9, "%08hx",  context.GuestContext.RSI);
  VideoWriteString(tmp, 8, LIGHT_GREEN, 6, 2);

  VideoWriteString("RDI=", 4, LIGHT_BLUE, 15, 2);
  vmm_snprintf(tmp, 9, "%08hx",   context.GuestContext.RDI);
  VideoWriteString(tmp, 8, LIGHT_GREEN, 19, 2);

  VideoWriteString("CR0=", 4, LIGHT_BLUE, 28, 2);
  vmm_snprintf(tmp, 9, "%08hx",   context.GuestContext.CR0);
  VideoWriteString(tmp, 8, LIGHT_GREEN, 32, 2);

  VideoWriteString("CR3=", 4, LIGHT_BLUE, 41, 2);
  vmm_snprintf(tmp, 9, "%08hx",  context.GuestContext.CR3);
  VideoWriteString(tmp, 8, LIGHT_GREEN, 45, 2);

  VideoWriteString("CR4=", 4, LIGHT_BLUE, 54, 2);
  vmm_snprintf(tmp, 9, "%08hx",   context.GuestContext.CR4);
  VideoWriteString(tmp, 8, LIGHT_GREEN, 58, 2);

  VideoWriteString("CS=", 3, LIGHT_BLUE, 67, 2);
  vmm_snprintf(tmp, 5, "%04hx",   context.GuestContext.CS);
  VideoWriteString(tmp, 4, LIGHT_GREEN, 70, 2);
	
  VideoWriteString("RFLAGS=", 7, LIGHT_BLUE, 80, 2);
  vmm_snprintf(tmp, 9, "%08hx",   context.GuestContext.RFLAGS);
  VideoWriteString(tmp, 8, LIGHT_GREEN, 87, 2);

  /* Draw delimiter lines */
  for(i = 1; i < SHELL_SIZE_X-1; i++) {
    VideoWriteChar('-', WHITE, i, 3);
    VideoWriteChar('-', WHITE, i, 11);
    VideoWriteChar('-', WHITE, i, SHELL_SIZE_Y-3);
  }
  /* Draw cursor */
  VideoWriteString("> ", 2, RED, 2, SHELL_SIZE_Y-2);
  
  VideoShowDisassembled();
  VideoResetOutMatrix();
  VideoResetOutMatrixCache();
}

void VideoDrawFrame(void)
{
  Bit32u i;     /*0123456789012345678*/
  Bit8u *footer = "-[ Made in Italy ]-";
  /* Bit8u *footer =   "-------------------"; */
  Bit32u color;
  VideoPrintHeader();

  for(i = 1; i < SHELL_SIZE_Y-1; i++) {
    VideoWriteChar('|', LIGHT_GREEN, 0, i);
    VideoWriteChar('|', RED, SHELL_SIZE_X-1, i);
  }

  VideoWriteChar('+', LIGHT_GREEN, 0, SHELL_SIZE_Y-1);

  for(i = 1; i < SHELL_SIZE_X/2; i++)
    VideoWriteChar('-', LIGHT_GREEN, i, SHELL_SIZE_Y-1);

  for(i = SHELL_SIZE_X/2; i < (SHELL_SIZE_X-1-vmm_strlen(footer)); i++)
    VideoWriteChar('-', RED, i, SHELL_SIZE_Y-1);

  /* write footer */
  for(i = 0; i < vmm_strlen(footer); i++) {
    if(i < 2 || i > 10) color = RED;
    else if(i > 2 && i < 8) color = LIGHT_GREEN;
    else color = WHITE;
    VideoWriteChar(footer[i], color, SHELL_SIZE_X-1-vmm_strlen(footer)+i, SHELL_SIZE_Y-1); 
  }
  
/*   VideoWriteString(footer, 2, RED, SHELL_SIZE_X-1-vmm_strlen(footer), SHELL_SIZE_Y-1); */
/*   VideoWriteString(&footer[2], 5, LIGHT_GREEN, SHELL_SIZE_X-1-vmm_strlen(footer)+2, SHELL_SIZE_Y-1); */
/*   VideoWriteString(&footer[7], 3, WHITE, SHELL_SIZE_X-1-vmm_strlen(footer)+7, SHELL_SIZE_Y-1); */
/*   VideoWriteString(&footer[10], 6, RED, SHELL_SIZE_X-1-vmm_strlen(footer)+10, SHELL_SIZE_Y-1); */
/*   VideoWriteString(&footer[16], 2, RED, SHELL_SIZE_X-1-vmm_strlen(footer)+16, SHELL_SIZE_Y-1); */

  VideoWriteChar('-', RED, SHELL_SIZE_X-2, SHELL_SIZE_Y-1);
  VideoWriteChar('+', RED, SHELL_SIZE_X-1, SHELL_SIZE_Y-1);
}


void VideoResetOutMatrixCache(void)
{
  int i, j; 
  for(i = 0; i < OUT_SIZE_Y; i++)
    for(j = 0; j < OUT_SIZE_X; j++)
      out_matrix_cache[i][j] = 0x20;
  /* memset(&out_matrix_cache[i], 0x20, OUT_SIZE_X*OUT_SIZE_Y); */
}

void VideoResetOutMatrix(void)
{
  int i, j;
  for(i = 0; i < OUT_SIZE_Y; i++)
    for(j = 0; j < OUT_SIZE_X; j++)
      out_matrix[i][j] = 0x20;
  /* memset(&out_matrix[i], 0x20, OUT_SIZE_X*OUT_SIZE_Y); */
}

/* Redraw the whole out area */
void VideoRefreshOutArea(unsigned int color)
{
  int i, j;
  for(i = 0; i < OUT_SIZE_Y; i++) {
    for(j = 0; j < OUT_SIZE_X; j++) {
      if(out_matrix[i][j] != out_matrix_cache[i][j]) {
	/* Update cache */
	out_matrix_cache[i][j] = out_matrix[i][j]; 
	VideoWriteString(&out_matrix[i][j], 1, color, OUT_START_X+j, OUT_START_Y+i);
      }
    }
  }
}

void VideoPrintHeader(void)
{
  int i, numberofdash;
  char *name;

  name = "=[ HyperDbg ]=";
  i = 0;
  numberofdash = (((SHELL_SIZE_X - vmm_strlen(name))/2)-1);
  VideoWriteChar('+', LIGHT_GREEN, 0, 0);

  for(i = 1; i <= numberofdash; i++) {
    VideoWriteChar('-', LIGHT_GREEN, i, 0);
    VideoWriteChar('-', RED, i+numberofdash+vmm_strlen(name), 0);
  }

  VideoWriteString(name, vmm_strlen(name), WHITE, numberofdash+1, 0);
  VideoWriteChar('+', RED, SHELL_SIZE_X-1, 0);

#if GUEST_WINDOWS
  {
    hvm_address pep, pid;
    hvm_status r;
    char str_tmp[64];
    char proc_name[16];
    r = WindowsFindProcess(context.GuestContext.CR3, &pep);
    if (r != HVM_STATUS_SUCCESS) goto error;

    r = MmuReadVirtualRegion(context.GuestContext.CR3, pep + OFFSET_EPROCESS_UNIQUEPID, &pid, sizeof(pid));
    if (r != HVM_STATUS_SUCCESS) goto error;

    vmm_memset(str_tmp, 0, sizeof(str_tmp));

    r = MmuReadVirtualRegion(context.GuestContext.CR3, pep + OFFSET_EPROCESS_IMAGEFILENAME, proc_name, 16);
    if (r != HVM_STATUS_SUCCESS) goto error;
    vmm_snprintf(str_tmp, sizeof(str_tmp), "=[pid: %.8x; proc: %s]=", pid, proc_name);
    VideoWriteString(str_tmp, vmm_strlen(str_tmp), LIGHT_GREEN, 2, 0);
  error:
    ;
  }
#endif
}

/* FIXME: after we decide the new size of the, shell we have to reset all the
   sizes */
void VideoShowDisassembled()
{
  hvm_address addr, tmpaddr;
  Bit32u y, x, operand, i;
  ud_t ud_obj;
  Bit8u str_addr[10], instr[SHELL_SIZE_X-15], disasbuf[96];
  Bit8u* disasinst;
  PSYMBOL sym;
  memset(str_addr, 0x20, 10);
  memset(instr, 0x20, SHELL_SIZE_X-15);
  y = 4;
  x = 2;
  i = 0;
  operand = 0;
  VideoResetOutMatrix();
  
  if(MmuIsAddressValid(context.GuestContext.CR3, context.GuestContext.RIP)) {
    addr = context.GuestContext.RIP;
  } else { /* FIXME: rewrite better */
    ComPrint("[HyperDbg] RIP not valid!\n");
    VideoWriteString("RIP not valid!", 14, RED, x, y);
    return; /* end here */
  }
  tmpaddr = addr;

  /* Setup disassembler structure */
  ud_init(&ud_obj);
  ud_set_mode(&ud_obj, 32);
  ud_set_syntax(&ud_obj, UD_SYN_ATT);

  MmuReadVirtualRegion(context.GuestContext.CR3, addr, disasbuf, sizeof(disasbuf)/sizeof(Bit8u));
  ud_set_input_buffer(&ud_obj, disasbuf, sizeof(disasbuf)/sizeof(Bit8u));

  while(ud_disassemble(&ud_obj)) {
    sym = 0; /* reset symbol */
    i = 0;
    vmm_snprintf(str_addr, 10, "%08hx:", tmpaddr);
    //str_addr[8] = 0;
    VideoWriteString(str_addr, 9, LIGHT_BLUE, x, y);
    /* check if the operand is an address */
    /* FIXME: write better ;-) */
    disasinst = (Bit8u*) ud_insn_asm(&ud_obj);
    while(disasinst[i] != (Bit8u)'\0' && disasinst[i] != (Bit8u)' ') i++; /* reach the end or the first operand */
    if(disasinst[i] != 0 && (vmm_strlen(disasinst) - (&disasinst[i] - disasinst)) == 11) { /* 11 is the size of 0x12345678 + 1 space (USE >= 11 to parse also inst 0x12345678, XXX */
      /* check if it's likely to be an address */
      if(disasinst[i+1] == (Bit8u)'0' && disasinst[i+2] == 'x') {
	if(vmm_strtoul(&disasinst[i+1], &operand)) { /* if it's convertible to a hvm_address */
	  sym = SymbolGetFromAddress((hvm_address)operand);
	}
      }
    }
    if(sym)
      vmm_snprintf(instr, SHELL_SIZE_X-15, " %-24s %s <%s>", ud_insn_hex(&ud_obj), ud_insn_asm(&ud_obj), sym->name);
    else
      vmm_snprintf(instr, SHELL_SIZE_X-15, " %-24s %s", ud_insn_hex(&ud_obj), ud_insn_asm(&ud_obj));
      
    


    /* Is this the current instruction? */
    if(tmpaddr == addr) {
      VideoWriteString(instr, MIN(vmm_strlen(instr), SHELL_SIZE_X-15), RED, x+9, y);
    } else {
      VideoWriteString(instr, MIN(vmm_strlen(instr), SHELL_SIZE_X-15), LIGHT_GREEN, x+9, y);
    }

    tmpaddr += ud_insn_len(&ud_obj);
    y++;

    /* Check if maximum size has been reached reached */
    if(y >= 11) break;
  }
}
