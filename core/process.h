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

#ifndef _PROCESS_H
#define _PROCESS_H

#include "types.h"

typedef struct _PROCESS_DATA {
  hvm_address pid;
  hvm_address cr3;
  hvm_address pobj;		/* Pointer to the process object (e.g., EPROCESS, on Windows)  */
  char        name[32];
} PROCESS_DATA, *PPROCESS_DATA;

typedef struct _MODULE_DATA {
  hvm_address baseaddr;
  hvm_address entrypoint;
  hvm_address pobj;		/* Pointer to the module object (e.g., LDR_MODULE on Windows) */
  char        name[32];
} MODULE_DATA, *PMODULE_DATA;

typedef void (*hvm_process_callback)(PPROCESS_DATA);

/* 'cr3' here is just a valid CR3 of the guest */
hvm_status ProcessGetNextProcess(hvm_address cr3, PPROCESS_DATA pprev, PPROCESS_DATA pnext);
hvm_status ProcessGetNextModule (hvm_address cr3, PMODULE_DATA pprev, PMODULE_DATA pnext);
hvm_status ProcessGetNameByPid(hvm_address cr3, hvm_address pid, char *name);
hvm_status ProcessFindProcessPid(hvm_address cr3, hvm_address *pid);
hvm_status ProcessFindProcessTid(hvm_address cr3, hvm_address *tid);

#endif	/* _PROCESS_H */
