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

#include "process.h"
#include "vmmstring.h"
#include "debug.h"

#ifdef GUEST_WINDOWS
#include "winxp.h"
#elif defined GUEST_LINUX
#include <linux/sched.h>
#include "linux.h"
#endif

hvm_status ProcessGetNextProcess(hvm_address cr3, PPROCESS_DATA pprev, PPROCESS_DATA pnext)
{
  hvm_status r;

#ifdef GUEST_WINDOWS
  r = WindowsGetNextProcess(cr3, pprev, pnext);
#elif defined GUEST_LINUX
  r = LinuxGetNextProcess(cr3, pprev, pnext);
#endif

  return r;
}

hvm_status ProcessGetNextModule(hvm_address cr3, PMODULE_DATA pprev, PMODULE_DATA pnext)
{
  hvm_status r;

#ifdef GUEST_WINDOWS
  r = WindowsGetNextModule(cr3, pprev, pnext);
#elif defined GUEST_LINUX
  /* TODO! */
  r = HVM_STATUS_UNSUCCESSFUL;
#endif

  return r;
}

hvm_status ProcessGetNameByPid(hvm_address cr3, hvm_address pid, char *name)
{
  PROCESS_DATA prev, next;
  hvm_status r;

  vmm_memset(&next, 0, sizeof(next));

  r = ProcessGetNextProcess(cr3, NULL, &next);
  while(TRUE) {
    if(r == HVM_STATUS_END_OF_FILE)
      break;
    
    if(r != HVM_STATUS_UNSUCCESSFUL && pid == next.pid) {
#ifdef GUEST_WINDOWS      
      vmm_strncpy(name, next.name, 32);
#elif defined GUEST_LINUX
      vmm_strncpy(name, next.name, TASK_COMM_LEN);
#endif
      return HVM_STATUS_SUCCESS;
    }
    prev = next;
    vmm_memset(next.name, 0, sizeof(next.name));
    r = ProcessGetNextProcess(cr3, &prev, &next);
  }
  return HVM_STATUS_UNSUCCESSFUL;
}

hvm_status ProcessFindProcessPid(hvm_address cr3, hvm_address *pid)
{
  hvm_status r;
  
#ifdef GUEST_WINDOWS
  r = WindowsFindProcessPid(cr3, pid);
#else
  r = LinuxFindProcessPid(cr3, pid);
#endif
  
  return r;
}

hvm_status ProcessFindProcessTid(hvm_address cr3, hvm_address *tid)
{
  hvm_status r;
  
#ifdef GUEST_WINDOWS
  r = WindowsFindProcessTid(cr3, tid);
#else
  r = LinuxFindProcessTid(cr3, tid);
#endif
  
  return r;
}
