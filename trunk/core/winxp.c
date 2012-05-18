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

#include "winxp.h"
#include "debug.h"
#include "mmu.h"
#include "common.h"
#include "vmmstring.h"
#include <ddk/ntddk.h>

/* ################ */
/* #### MACROS #### */
/* ################ */

#define SHAREDUSERDATA 0x7ffe0000
#define KUSER_SHARED_DATA_PHYPAGES_OFFSET 0x2e8
#define SystemModuleInformation 11

/* ############### */
/* #### TYPES #### */
/* ############### */

typedef NTSTATUS (NTAPI *NTQUERYSYSTEMINFORMATION)(ULONG, PVOID, ULONG, PULONG);

static struct {
  NTQUERYSYSTEMINFORMATION NtQuerySystemInformation;
  ULONG PsLoadedModuleList;
  PEPROCESS PsInitialSystemProcess;
} WindowsSymbols;

#ifdef GUEST_WIN_7
static hvm_status  Windows7FindNetworkConnections(hvm_address cr3, SOCKET *buf, Bit32u maxsize, Bit32u *psize);
static hvm_status  Windows7FindNetworkData(hvm_address cr3, SOCKET *buf, Bit32u maxsize, Bit32u *psize, unsigned int type);
#else
static hvm_status  WindowsFindNetworkConnections(hvm_address cr3, SOCKET *buf, Bit32u maxsize, Bit32u *psize);
static hvm_status  WindowsFindNetworkSockets(hvm_address cr3, SOCKET *buf, Bit32u maxsize, Bit32u *psize);
#endif
static hvm_address WindowsFindPsLoadedModuleList(PDRIVER_OBJECT DriverObject);

/* ################ */
/* #### BODIES #### */
/* ################ */

/* NOTE: To be invoked in non-root mode! */
hvm_status WindowsInit(PDRIVER_OBJECT DriverObject)
{
  UNICODE_STRING u;

  hvm_address fs;

  __asm__ __volatile__ (

			"push %%eax\n"
			"mov %%fs:0x20, %%eax\n"
			"mov %%eax, %0\n"
			"pop %%eax\n"
			:"=m"(fs)
			::"memory"
			);

  Log("fs is 0x%x", fs);

  RtlInitUnicodeString(&u, L"NtQuerySystemInformation");
  WindowsSymbols.NtQuerySystemInformation = (NTQUERYSYSTEMINFORMATION) MmGetSystemRoutineAddress(&u);  
  if (!WindowsSymbols.NtQuerySystemInformation) return HVM_STATUS_UNSUCCESSFUL;

  WindowsSymbols.PsLoadedModuleList = WindowsFindPsLoadedModuleList(DriverObject);
  if (WindowsSymbols.PsLoadedModuleList == 0) return HVM_STATUS_UNSUCCESSFUL;
  
  RtlInitUnicodeString(&u, L"PsInitialSystemProcess");
  WindowsSymbols.PsInitialSystemProcess= (PEPROCESS) *((PEPROCESS*) MmGetSystemRoutineAddress(&u));
  if (WindowsSymbols.PsInitialSystemProcess == 0) return HVM_STATUS_UNSUCCESSFUL;
  
  return HVM_STATUS_SUCCESS;
}

static hvm_address WindowsFindPsLoadedModuleList(PDRIVER_OBJECT DriverObject)
{
  hvm_address v;

  v = 0;

  if (!DriverObject) return 0;

  v = *(hvm_address*) ((hvm_address) DriverObject + FIELD_OFFSET(DRIVER_OBJECT, DriverSection));

  return v;
}

hvm_status WindowsGetKeyboardVector(unsigned char* pv)
{
  NTSTATUS       r;
  PDEVICE_OBJECT pDeviceObject;
  PFILE_OBJECT   pFileObject;
  UNICODE_STRING us;
  PKINTERRUPT    pKInterrupt;

  pDeviceObject = NULL;
  pKInterrupt   = NULL;
  
  RtlInitUnicodeString(&us, L"\\Device\\KeyboardClass0");

  // Get the DeviceObject top-of-the-stack of the kbdclass device
  r = IoGetDeviceObjectPointer(&us, FILE_READ_ATTRIBUTES, &pFileObject, &pDeviceObject);

  if(r != STATUS_SUCCESS)
    return HVM_STATUS_UNSUCCESSFUL;

  while(pDeviceObject->DeviceType != FILE_DEVICE_8042_PORT) {
    PR_DEVOBJ_EXTENSION pde;

    // Go down to the lower-level object
    pde = (PR_DEVOBJ_EXTENSION) pDeviceObject->DeviceObjectExtension;
    if (pde->AttachedTo)
      pDeviceObject = pde->AttachedTo;
    else // We reached the lowest level
      return HVM_STATUS_UNSUCCESSFUL;
  }

  // pDeviceObject == i8042prt's DeviceObject
  pKInterrupt = (PKINTERRUPT) ((PPORT_KEYBOARD_EXTENSION) pDeviceObject->DeviceExtension)->InterruptObject;

  *pv = (unsigned char) (pKInterrupt->Vector & 0xff);

  return HVM_STATUS_SUCCESS;
}

/* NOTE: To be invoked in non-root mode! */
hvm_status WindowsGetKernelBase(hvm_address* pbase)
{
  Bit32u i, Byte, ModuleCount;
  hvm_address* pBuffer;
  PSYSTEM_MODULE_INFORMATION pSystemModuleInformation;
  hvm_status r;

  WindowsSymbols.NtQuerySystemInformation(SystemModuleInformation, (PVOID) &Byte, 0, (PULONG) &Byte);
      
  pBuffer = MmAllocateNonCachedMemory(Byte);          
  if(!pBuffer)
    return HVM_STATUS_UNSUCCESSFUL;
                
  if(WindowsSymbols.NtQuerySystemInformation(SystemModuleInformation, pBuffer, Byte, (PULONG) &Byte)) {
    MmFreeNonCachedMemory(pBuffer, Byte);
    return HVM_STATUS_UNSUCCESSFUL;
  }

  ModuleCount = *(hvm_address*) pBuffer;
  pSystemModuleInformation = (PSYSTEM_MODULE_INFORMATION)((unsigned char*) pBuffer + sizeof(Bit32u));

  r = HVM_STATUS_UNSUCCESSFUL;

  for(i = 0; i < ModuleCount; i++) {
    if(strstr(pSystemModuleInformation->ImageName, "ntoskrnl.exe") ||
       strstr(pSystemModuleInformation->ImageName, "ntkrnlpa.exe")) {
      r = HVM_STATUS_SUCCESS;
      *pbase = (hvm_address) pSystemModuleInformation->Base;
      break;
    }
    pSystemModuleInformation++;
  }
                
  MmFreeNonCachedMemory(pBuffer, Byte);

  return r;
}

hvm_status  WindowsFindModuleByName(hvm_address cr3, Bit8u *name, MODULE_DATA *pmodule)
{
  MODULE_DATA prev, next;
  hvm_status r;

  r = ProcessGetNextModule(cr3, NULL, &next);

  while (1) {
    if (r == HVM_STATUS_END_OF_FILE) {
      /* No more processes */
      break;
    } else if (r != HVM_STATUS_SUCCESS) {
      /* Some error occurred */
      Log("[Win] Can't read traverse modules list");
      break;
    }

    if (!vmm_strncmpi(name, next.name, vmm_strlen(name))) {
      vmm_memcpy(pmodule, &next, sizeof(next));
      return HVM_STATUS_SUCCESS; /* FOUND! */
    }

    /* Get the next process */
    prev = next;
    r = ProcessGetNextModule(cr3, &prev, &next);
  }

  /* Not found */
  return HVM_STATUS_UNSUCCESSFUL;
}

hvm_status WindowsFindModule(hvm_address cr3, hvm_address addr, PWSTR name, Bit32u namesz)
{
  hvm_status  r;
  hvm_address proc, v, pldr;
  ULONG       module_current, module_head;
  LIST_ENTRY le;
  PEB_LDR_DATA ldr;
  LDR_MODULE module;
  CHAR namec[64]; 

  /* Find the EPROCESS structure for the process with the specified cr3
     value */
  r = WindowsFindProcess(cr3, &proc);

  if (r != HVM_STATUS_SUCCESS)
    return r;

  /* Read the PEB address */
  r = MmuReadVirtualRegion(cr3, proc + OFFSET_EPROCESS_PEB, &v, sizeof(v));
  if (r != HVM_STATUS_SUCCESS ) {
    ComPrint("[HyperDbg] Can't read PEB at offset %.8x\n", proc + OFFSET_EPROCESS_PEB);
    return r;
  }

  if (v == 0) {
    /* System process -- use PsLoadedModuleList as the head of the list of
       loaded modules  */
    module_current = module_head = WindowsSymbols.PsLoadedModuleList;
  } else {
    /* Read the PPEB_LDR_DATA from the process */
    r = MmuReadVirtualRegion(cr3, v + OFFSET_PEB_LDRDATA, &pldr, sizeof(pldr));
    if (r != HVM_STATUS_SUCCESS ) {
      ComPrint("[HyperDbg] Can't read PPEB_LDR_DATA at offset %.8x\n", v + OFFSET_PEB_LDRDATA);
      return r;
    }

    r = MmuReadVirtualRegion(cr3, pldr, &ldr, sizeof(ldr));
    if (r != HVM_STATUS_SUCCESS ) {
      ComPrint("[HyperDbg] Can't read PEB_LDR_DATA at offset %.8x\n", pldr);
      return r;
    }

    module_current = module_head = (ULONG) ldr.InLoadOrderModuleList.Flink;
  }

  /* Traverse the modules list */
  do {
    r = MmuReadVirtualRegion(cr3, (hvm_address) module_current - FIELD_OFFSET(LDR_MODULE, InLoadOrderModuleList), 
			     &module, sizeof(module));
    if (r != HVM_STATUS_SUCCESS ) break;

    vmm_memset((unsigned char*) name, 0, namesz*2);

    r = MmuReadVirtualRegion(cr3, (hvm_address) module.BaseDllName.Buffer, name, 
			     MIN(module.BaseDllName.MaximumLength, namesz));

    if (r == HVM_STATUS_SUCCESS && \
	addr < (ULONG) module.BaseAddress + (ULONG)module.SizeOfImage && \
	addr >= (ULONG)module.BaseAddress) {
      return HVM_STATUS_SUCCESS; /* FOUND! */
    }

    module_current = (ULONG) module.InLoadOrderModuleList.Flink;
  } while (module_current != module_head);

  /* TODO */
  return HVM_STATUS_UNSUCCESSFUL;
}

Bit32u WindowsGuessFrames(void)
{
  Bit32u dwPhyPages;

  dwPhyPages = *(Bit32u*) (SHAREDUSERDATA + KUSER_SHARED_DATA_PHYPAGES_OFFSET);

  return dwPhyPages;
}

hvm_bool WindowsProcessIsTerminating(hvm_address cr3)
{
  hvm_address pep;
  Bit32u flags;
  hvm_bool b;
  hvm_status r;

  r = WindowsFindProcess(cr3, &pep);
  
  b = TRUE;

  if (r == HVM_STATUS_SUCCESS) {
    /* Read the Flags field of the EPROCESS structure */
    r = MmuReadVirtualRegion(cr3, pep + OFFSET_EPROCESS_FLAGS, &flags, sizeof(flags));
    if (r == HVM_STATUS_SUCCESS && (flags & EPROCESS_FLAGS_DELETE) == 0) {
      /* Still running */
      b = FALSE;
    }
  }

  return b;
}

/* FIXME: Hidden processes (e.g., unlinked from the EPROCESS list) pwn us
   :-) See "Rootkit Detection in Windows Systems" (Rutskowa) */
hvm_status WindowsFindProcess(hvm_address cr3, hvm_address* ppep)
{
  PROCESS_DATA prev, next;
  hvm_status r;
  hvm_bool found;

  /* Iterate over ("visible") system processes */
  found = FALSE;
  r = ProcessGetNextProcess(context.GuestContext.cr3, NULL, &next);

  while (1) {
    if (r == HVM_STATUS_END_OF_FILE) {
      /* No more processes */
      break;
    } else if (r != HVM_STATUS_SUCCESS) {
      Log("[Win] Can't read next process. Current process: %.8x", next.pobj);
      break;
    }

    if (next.cr3 == cr3) {
      /* Found! */
      found = TRUE;
      *ppep = next.pobj;
      break;
    }

    /* Go on with next process */
    prev = next;
    r = ProcessGetNextProcess(context.GuestContext.cr3, &prev, &next);
  }

  return found ? HVM_STATUS_SUCCESS : HVM_STATUS_UNSUCCESSFUL;
}

hvm_status WindowsFindProcessPid(hvm_address cr3, hvm_address* ppid)
{
  hvm_address pep;
  hvm_status r;

  r = WindowsFindProcess(cr3, &pep);
  if (r != HVM_STATUS_SUCCESS) return r;

  r = MmuReadVirtualRegion(cr3, pep + OFFSET_EPROCESS_UNIQUEPID, ppid, sizeof(hvm_address));
  if (r != HVM_STATUS_SUCCESS) return r;

  return HVM_STATUS_SUCCESS;
}

hvm_status WindowsFindProcessName(hvm_address cr3, char* name)
{
  hvm_address pep;
  hvm_status r;

  r = WindowsFindProcess(cr3, &pep);
  if (r != HVM_STATUS_SUCCESS) return r;

  r = MmuReadVirtualRegion(cr3, pep + OFFSET_EPROCESS_IMAGEFILENAME, name, 16);
  if (r != HVM_STATUS_SUCCESS) return r;

  return HVM_STATUS_SUCCESS;
}

hvm_status WindowsFindProcessTid(hvm_address cr3, hvm_address* ptid)
{
  hvm_address pep, kthread_current, kthread_head;
  hvm_status  r;
  hvm_bool    found;
  LIST_ENTRY  le;
  Bit8u       c;

  r = WindowsFindProcess(cr3, &pep);
  if (r != HVM_STATUS_SUCCESS) return r;

  /* Traverse the list of threads for the specified process in order to find
     the one that is currently running */

  /* Get a pointer to the KTHREAD structure of the first thread in the list */
  r = MmuReadVirtualRegion(cr3, pep + OFFSET_KPROCESS_THREADLISTHEAD, &kthread_head, sizeof(kthread_head));
  if (r != HVM_STATUS_SUCCESS) return r;

  kthread_head = kthread_head - OFFSET_KTHREAD_THREADLISTENTRY;

  kthread_current = kthread_head;
  found = FALSE;

  do {
    /* Read the state of the current thread */
    r = MmuReadVirtualRegion(cr3, kthread_current + OFFSET_KTHREAD_STATE, &c, sizeof(c));
    if (r == HVM_STATUS_SUCCESS && c == KTHREAD_STATE_RUNNING) {
      /* Found running thread */
      found = TRUE;
      break;
    }

    /* Go on with the next thread */
    r = MmuReadVirtualRegion(cr3, kthread_current + OFFSET_KTHREAD_THREADLISTENTRY, &le, sizeof(le));
    if (r != HVM_STATUS_SUCCESS) {
      Log("[Win] Can't read LIST_ENTRY at offset %.8x, base %.8x", kthread_current + OFFSET_KTHREAD_THREADLISTENTRY, kthread_current);
      break;
    }

    kthread_current = (hvm_address) (le.Flink) - OFFSET_KTHREAD_THREADLISTENTRY;
  } while (kthread_current != kthread_head);

  if (found) {
    /* Read the TID of the running thread */
    CLIENT_ID cid;

    r = MmuReadVirtualRegion(cr3, kthread_current + OFFSET_ETHREAD_CID, &cid, sizeof(cid));
    if (r != HVM_STATUS_SUCCESS) return r;

    *ptid = (hvm_address) cid.UniqueThread;
  }

  return found ? HVM_STATUS_SUCCESS : HVM_STATUS_UNSUCCESSFUL;
}

hvm_status WindowsGetNextProcess(hvm_address cr3, PPROCESS_DATA pprev, PPROCESS_DATA pnext)
{
  hvm_status r;
  
  if (!pnext) return HVM_STATUS_UNSUCCESSFUL;

  if (!pprev) {
    /* Start with the first process, i.e., System */
    pnext->pobj = (hvm_address) WindowsSymbols.PsInitialSystemProcess;
  } else {
    /* Go on with the next process in the linked-list of active ones */
    LIST_ENTRY   le;
    
    if (!pprev->pobj) return HVM_STATUS_UNSUCCESSFUL;

    r = MmuReadVirtualRegion(cr3, pprev->pobj + OFFSET_EPROCESS_ACTIVELINKS, &le, sizeof(le));
    if (r != HVM_STATUS_SUCCESS) {
      Log("[Win] Can't read LIST_ENTRY at offset %.8x", pprev->pobj + OFFSET_EPROCESS_ACTIVELINKS);
      return HVM_STATUS_UNSUCCESSFUL;
    }

    pnext->pobj = (hvm_address) (le.Flink) - OFFSET_EPROCESS_ACTIVELINKS;

    if (pnext->pobj == (hvm_address) WindowsSymbols.PsInitialSystemProcess)
      return HVM_STATUS_END_OF_FILE;
  }

  /* Read the CR3 of the current process */
  r = MmuReadVirtualRegion(cr3, pnext->pobj + FIELD_OFFSET(KPROCESS, DirectoryTableBase), &(pnext->cr3), sizeof(pnext->cr3));
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  /* Read the PID of the process */
  r = MmuReadVirtualRegion(cr3, pnext->pobj + OFFSET_EPROCESS_UNIQUEPID, &(pnext->pid), sizeof(pnext->pid));
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  /* Read the name of the process */
  r = MmuReadVirtualRegion(cr3, pnext->pobj + OFFSET_EPROCESS_IMAGEFILENAME, pnext->name, 16);
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  return HVM_STATUS_SUCCESS;
}

hvm_status WindowsGetNextModule(hvm_address cr3, PMODULE_DATA pprev, PMODULE_DATA pnext)
{
  hvm_status r;
  LDR_MODULE module;
  Bit16u wname[64];

  if (!pnext) return HVM_STATUS_UNSUCCESSFUL;

  if (!pprev) {
    /* Start with the first process, i.e., System */
    pnext->pobj = (hvm_address) WindowsSymbols.PsLoadedModuleList - FIELD_OFFSET(LDR_MODULE, InLoadOrderModuleList);
  } else {
    /* Go on with the next process in the linked-list of modules */
    if (!pprev->pobj) return HVM_STATUS_UNSUCCESSFUL;

    r = MmuReadVirtualRegion(cr3, (hvm_address) pprev->pobj, &module, sizeof(module));
    if (r != HVM_STATUS_SUCCESS ) return HVM_STATUS_UNSUCCESSFUL;

    pnext->pobj = (hvm_address) module.InLoadOrderModuleList.Flink - FIELD_OFFSET(LDR_MODULE, InLoadOrderModuleList);

    if (pnext->pobj == (hvm_address) WindowsSymbols.PsLoadedModuleList - FIELD_OFFSET(LDR_MODULE, InLoadOrderModuleList))
      return HVM_STATUS_END_OF_FILE;
  }

  pnext->baseaddr   = (hvm_address) module.BaseAddress;
  pnext->entrypoint = (hvm_address) module.EntryPoint;

  r = MmuReadVirtualRegion(cr3, (hvm_address) module.BaseDllName.Buffer, wname,
  			   MIN(module.BaseDllName.MaximumLength, sizeof(wname)/sizeof(Bit16u)));

  if (r != HVM_STATUS_SUCCESS) {
    pnext->name[0] = '\0';
  } else {
    vmm_memset(pnext->name, 0, sizeof(pnext->name));
    wide2ansi(pnext->name, (Bit8u*) wname, module.BaseDllName.Length/2);
  }

  return HVM_STATUS_SUCCESS;
}


#ifdef GUEST_WIN_7
hvm_status Windows7FindTargetInfo(hvm_address cr3, hvm_address *target_pep, hvm_address *target_kthread) {

  hvm_status r;
  hvm_address kthread_head, kthread_current, thread_pep, pid;
  char process_name[32];
  LIST_ENTRY le;
  Bit8u thread_state;

  WindowsFindProcess(cr3, target_pep);
    
  /* Traverse KTHREAD list of the process */
  r = MmuReadVirtualRegion(cr3, (*target_pep) + OFFSET_KPROCESS_THREADLISTHEAD, &kthread_head, sizeof(kthread_head));
  if(r != HVM_STATUS_SUCCESS) {

    Log("[Win] Can't read ThreadListHead of EPROCESS 0x%08hx, fail on va 0x%08hx", *target_pep, (*target_pep) + OFFSET_KPROCESS_THREADLISTHEAD);
    return r;
  }
  kthread_head = kthread_current = kthread_head - OFFSET_KTHREAD_THREADLISTENTRY;

  Log("[Win] KTHREAD head is at va 0x%08hx", kthread_head);
  
  do {

    MmuReadVirtualRegion(cr3, kthread_current + OFFSET_KTHREAD_PROCESS, &thread_pep, sizeof(hvm_address));
    MmuReadVirtualRegion(cr3, kthread_current + OFFSET_KTHREAD_THREADLISTENTRY, &le, sizeof(le));

    if(thread_pep == (*target_pep)) {

      Log("[Win] KTHREAD 0x%08hx", kthread_current);

      *target_kthread = kthread_current;

      vmm_memset(process_name, 0, sizeof(process_name));
      r = MmuReadVirtualRegion(cr3, thread_pep + OFFSET_EPROCESS_IMAGEFILENAME, &process_name, sizeof(process_name));
      if(r != HVM_STATUS_SUCCESS) vmm_strncpy(process_name, "UNKNOWN", 7);
      Log("[Win] ----> PEPROCESS:   0x%08hx/%s", thread_pep, process_name);
      MmuReadVirtualRegion(cr3, thread_pep + OFFSET_EPROCESS_UNIQUEPID, &pid, sizeof(pid));
      MmuReadVirtualRegion(cr3, kthread_current + OFFSET_KTHREAD_STATE, &thread_state, sizeof(thread_state));
      Log("[Win] ----> Process PID: 0x%x", pid);
      Log("[Win] ----> Flink:       0x%08hx", le.Flink);
      Log("[Win] ----> Blink:       0x%08hx", le.Blink);
      Log("[Win] ----> State:       %d", thread_state);
    }
    kthread_current = (hvm_address)le.Flink - OFFSET_KTHREAD_THREADLISTENTRY;
  } while(kthread_current != kthread_head);

  Log("[Win] Finish visit of process's ThreadList.");

  return HVM_STATUS_SUCCESS;
}

hvm_status Windows7UnlinkProc(hvm_address cr3, hvm_address target_pep, hvm_address target_kthread, unsigned int *dispatcher_index, Bit8u *success)
{
  hvm_address thread_pep, kthread_current, kthread_head, kthread_running, kthread_next;
  hvm_address kprcb, ready_list_entry;
  hvm_address pid;
  hvm_status  r;
  LIST_ENTRY  le, dispatcher_ready_array[32];
  SINGLE_LIST_ENTRY sle;
  char process_name[32], bitmap_binary_str[33];
  Bit8u thread_state;
  Bit32u ready_summary;
  unsigned int j;

  /* Unlink EPROCESS */
  /* MmuReadVirtualRegion(cr3, pep + OFFSET_EPROCESS_ACTIVELINKS, &le, sizeof(le)); */
  /* MmuWriteVirtualRegion(cr3, (hvm_address)le.Blink, &le.Flink, sizeof(hvm_address)); */
  /* MmuWriteVirtualRegion(cr3, (hvm_address)le.Flink + sizeof(hvm_address), &le.Blink, sizeof(hvm_address)); */

  /* check KTHREADs of interesting process */

  if(context.GuestContext.rax == target_kthread){
      
    Log("[Win] Denied running of target process!!! Switch to idle.");

    context.GuestContext.rax = 0;

    *dispatcher_index = context.GuestContext.rdi;

    *success = 1;
      
    /* If we want to use custom GDT, this code is wrong */
    __asm__ __volatile__ (

			  "push %%eax\n"
			  "mov %%fs:0x20, %%eax\n"
			  "mov %%eax, %0\n"
			  "pop %%eax\n"
			  :"=m"(kprcb)
			  ::"memory"
			  );

    r = MmuReadVirtualRegion(cr3, kprcb + OFFSET_PRCB_DISPATCHER_READY, &dispatcher_ready_array, sizeof(dispatcher_ready_array));
    if (r != HVM_STATUS_SUCCESS) {
      Log("[Win] Can't read LIST_ENTRY array at offset %.8x, base %.8x", kprcb + OFFSET_PRCB_DISPATCHER_READY, kprcb);
      return r;
    }

    r = MmuReadVirtualRegion(cr3, kprcb + OFFSET_PRCB_READYSUMMARY, &ready_summary, sizeof(ready_summary));
    if (r != HVM_STATUS_SUCCESS) {
      Log("[Win] Can't read ready_summary bitmap at va %08hx", kprcb + OFFSET_PRCB_READYSUMMARY);
      return r;
    }

    r = MmuReadVirtualRegion(cr3, kprcb + OFFSET_PRCB_CURRENT_THREAD, &kthread_running, sizeof(kthread_running));
    if (r != HVM_STATUS_SUCCESS) {
      Log("[Win] Can't read current thread ptr from va 0x%08hx...", kprcb + OFFSET_PRCB_CURRENT_THREAD);
      return r;
    }

    r = MmuReadVirtualRegion(cr3, kprcb + OFFSET_PRCB_NEXT_THREAD, &kthread_next, sizeof(kthread_next));
    if (r != HVM_STATUS_SUCCESS) {
      Log("[Win] Can't read next thread ptr from va 0x%08hx...", kprcb + OFFSET_PRCB_NEXT_THREAD);
      return r;
    }
  }

  return HVM_STATUS_SUCCESS;
}

hvm_status Windows7RelinkProc(hvm_address cr3, hvm_address target_kthread, unsigned int dispatcher_index)
{
  hvm_status r;
  hvm_address kprcb;
  LIST_ENTRY dispatcher_entry;
  hvm_address buffer;

  hvm_address thread_pep, kthread_current, kthread_head;
  hvm_address ready_list_entry;
  hvm_address pid;
  LIST_ENTRY  le, dispatcher_ready_array[32];
  char process_name[32];
  Bit8u thread_state;
  unsigned int j;

  /* If we want to use custom GDT, this code is wrong */
  __asm__ __volatile__ (

			"push %%eax\n"
			"mov %%fs:0x20, %%eax\n"
			"mov %%eax, %0\n"
			"pop %%eax\n"
			:"=m"(kprcb)
			::"memory"
			);

  r = MmuReadVirtualRegion(cr3, kprcb + OFFSET_PRCB_DISPATCHER_READY + dispatcher_index*sizeof(LIST_ENTRY), &dispatcher_entry, sizeof(dispatcher_entry));
  if (r != HVM_STATUS_SUCCESS) {
    Log("[Win] Can't read LIST_ENTRY (ready dispatcher entry) at va 0x%08hx (0x%08hx+0x%08hx*%0d)", kprcb + OFFSET_PRCB_DISPATCHER_READY + dispatcher_index*sizeof(LIST_ENTRY), kprcb + OFFSET_PRCB_DISPATCHER_READY, dispatcher_index, sizeof(LIST_ENTRY));
    return r;
  }

  buffer = target_kthread + OFFSET_KTHREAD_WAITLISTENTRY;
  
  kthread_head = (hvm_address)dispatcher_entry.Flink - OFFSET_KTHREAD_WAITLISTENTRY;
  kthread_current = kthread_head;
  do {
    r = MmuReadVirtualRegion(cr3, kthread_current + OFFSET_KTHREAD_WAITLISTENTRY, &le, sizeof(le));
    if (r != HVM_STATUS_SUCCESS) {
      Log("[Win] Can't read LIST_ENTRY at offset %.8x, base %.8x", kthread_current + OFFSET_KTHREAD_WAITLISTENTRY, kthread_current);
      return r;
    }

    if((hvm_address)(le.Flink) - OFFSET_KTHREAD_WAITLISTENTRY == kthread_head)
      MmuWriteVirtualRegion(cr3, kthread_current + OFFSET_KTHREAD_WAITLISTENTRY, &buffer, sizeof(hvm_address));

    kthread_current = (hvm_address)(le.Flink) - OFFSET_KTHREAD_WAITLISTENTRY;

  }while(kthread_current != kthread_head);

  /* Insert target thread as new head */

  /* Overwrite Blink of list head */
  buffer = target_kthread + OFFSET_KTHREAD_WAITLISTENTRY;
  r = MmuWriteVirtualRegion(cr3, (hvm_address)dispatcher_entry.Flink + sizeof(hvm_address), &buffer, sizeof(hvm_address));
  if (r != HVM_STATUS_SUCCESS) {
    return HVM_STATUS_UNSUCCESSFUL;
  }

  /* Overwrite Flink of dispatcher entry */
  r = MmuWriteVirtualRegion(cr3, kprcb + OFFSET_PRCB_DISPATCHER_READY + dispatcher_index*sizeof(LIST_ENTRY), &buffer, sizeof(hvm_address));
  if (r != HVM_STATUS_SUCCESS) {
    return HVM_STATUS_UNSUCCESSFUL;
  }

  /* Overwrite Flink of target kthread */
  buffer = (hvm_address)dispatcher_entry.Flink;
  r = MmuWriteVirtualRegion(cr3, target_kthread + OFFSET_KTHREAD_WAITLISTENTRY, &buffer, sizeof(hvm_address));
  if (r != HVM_STATUS_SUCCESS) {
    return HVM_STATUS_UNSUCCESSFUL;
  }

  /* Overwrite Blink of target kthread */
  buffer = kprcb + OFFSET_PRCB_DISPATCHER_READY + dispatcher_index*sizeof(LIST_ENTRY);
  r = MmuWriteVirtualRegion(cr3, target_kthread + OFFSET_KTHREAD_WAITLISTENTRY + sizeof(hvm_address), &buffer, sizeof(hvm_address));
  if (r != HVM_STATUS_SUCCESS) {
    return HVM_STATUS_UNSUCCESSFUL;
  }

  return HVM_STATUS_SUCCESS;
}

hvm_status Windows7FindNetworkConnections(hvm_address cr3, SOCKET* buf, Bit32u maxsize, Bit32u *psize) {

  hvm_status r;
  hvm_address partition_table, partition_entry_array, partition_entry, tmp;
  hvm_address phash, pipinfo, peprocess, pipaddresses;
  Bit32u i, j, k, h, npart, nmaxhash, hash_index, tcp_conn_found = 0;
  Bit32u local_ip, remote_ip, pid, ports, ipv6;
  Bit16u local_port, remote_port, local_ipv6[8], remote_ipv6[8];
  MODULE_DATA tcp_module;

  r = WindowsFindModuleByName(cr3, "tcpip.sys", &tcp_module);
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  /* Read number of partition entry */
  r = MmuReadVirtualRegion(cr3, tcp_module.baseaddr + OFFSET_PARTITION_COUNTER, &npart, sizeof(npart));
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  /* Read partition table */
  r = MmuReadVirtualRegion(cr3, tcp_module.baseaddr + OFFSET_PARTITION_TABLE, &partition_entry_array, sizeof(hvm_address));
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;
 
  /* For all partitions */
  for(j = 0; j < npart; j++) {

    /* Read partition_entry va */
    r = MmuReadVirtualRegion(cr3, partition_entry_array + 0x48*j + sizeof(hvm_address), &partition_entry, sizeof(hvm_address));
    if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

    r = MmuReadVirtualRegion(cr3, partition_entry + OFFSET_PART_ENTRY_HASHTABLE, &phash, sizeof(hvm_address));
    if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

    r = MmuReadVirtualRegion(cr3, partition_entry + OFFSET_PART_ENTRY_MAXNUM, &nmaxhash, sizeof(nmaxhash));
    if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

    /* For all entries in hash table */
    for(hash_index = 0; hash_index < nmaxhash; hash_index++) {

      r = MmuReadVirtualRegion(cr3, phash + hash_index * (2*sizeof(hvm_address)), &pipinfo, sizeof(hvm_address));
      if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;
	  
      /* If the pointer is the hash entry itself, skip */
      while(pipinfo && pipinfo != phash+hash_index*(2*sizeof(hvm_address))) {

	/* found a valid IPInfo structure */

	r = MmuReadVirtualRegion(cr3, pipinfo + OFFSET_IPINFO_PEPROCESS, &peprocess, sizeof(hvm_address));
	if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	r = MmuReadVirtualRegion(cr3, peprocess + OFFSET_EPROCESS_UNIQUEPID, &pid, sizeof(pid));
	if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	r = MmuReadVirtualRegion(cr3, pipinfo + OFFSET_IPINFO_PORTS, &ports, sizeof(ports));
	if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	remote_port = (ports & 0xffff0000) >> 16;
	local_port = ports & 0x0000ffff;

	r = MmuReadVirtualRegion(cr3, pipinfo + OFFSET_IPv6_1, &tmp, sizeof(hvm_address));
	if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	r = MmuReadVirtualRegion(cr3, tmp + OFFSET_IPv6_2, &ipv6, sizeof(ipv6));
	if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	if(ipv6 != 0x17) ipv6 = 0;
	else Log("[HyperDbg] Ipv6 connection at pipinfo 0x%08hx!!!", pipinfo); 

	r = MmuReadVirtualRegion(cr3, pipinfo + OFFSET_IPINFO_IPADDRESSES, &pipaddresses, sizeof(hvm_address));
	if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	r = MmuReadVirtualRegion(cr3, pipaddresses + OFFSET_REMOTE_IP, &tmp, sizeof(hvm_address));
	if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	if(ipv6) {

	  for(k = 0, h = 0; k < 0xe; k+=2, h++) {
	    r = MmuReadVirtualRegion(cr3, tmp + k, &remote_ipv6[h], sizeof(remote_ipv6[0]));
	    if(r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;
	  }
	}
	else {

	  r = MmuReadVirtualRegion(cr3, tmp, &remote_ip, sizeof(remote_ip));
	  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;
	}

	r = MmuReadVirtualRegion(cr3, pipaddresses, &pipaddresses, sizeof(hvm_address));
	if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	if(ipv6) {

	  for(k = 0, h = 0; k < 0xe; k+=2, h++) {
	    r = MmuReadVirtualRegion(cr3, pipaddresses + OFFSET_LOCAL_IP + k, &local_ipv6[h], sizeof(remote_ipv6[0]));
	    if(r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;
	  }
	}
	else {
	  r = MmuReadVirtualRegion(cr3, pipaddresses + OFFSET_LOCAL_IP, &local_ip, sizeof(local_ip));
	  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;
	}
	if(tcp_conn_found < maxsize) {
	      
	  vmm_memset(&buf[tcp_conn_found], 0, sizeof(SOCKET));
	  buf[tcp_conn_found].state       = SocketStateEstablished;
	  buf[tcp_conn_found].local_port  = vmm_ntohs(local_port);
	  buf[tcp_conn_found].remote_port = vmm_ntohs(remote_port);
	  buf[tcp_conn_found].pid         = pid;
	  buf[tcp_conn_found].protocol    = 6; /* TCP */

	  if(ipv6) {

	    for(k = 0; k < 8; k++)
	      buf[tcp_conn_found].local_ipv6[k] = local_ipv6[k];

	    for(k = 0; k < 8; k++)
	      buf[tcp_conn_found].remote_ipv6[k] = remote_ipv6[k];

	    buf[tcp_conn_found].local_ip = -1;
	    buf[tcp_conn_found].remote_ip = -1;	    
	  }
	  else {
	    buf[tcp_conn_found].local_ip  = local_ip;
	    buf[tcp_conn_found].remote_ip = remote_ip;
	  }

	  tcp_conn_found++;
	}
	
	r = MmuReadVirtualRegion(cr3, pipinfo, &pipinfo, sizeof(hvm_address));
	if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;
      }
    }
  }

  *psize = tcp_conn_found;

  Log("[HyperDbg] Search of TCP connections finished. Found: %d", tcp_conn_found);

  return HVM_STATUS_SUCCESS;
}

hvm_status Windows7FindNetworkData(hvm_address cr3, SOCKET* buf, Bit32u maxsize, Bit32u *psize, unsigned int type) {

  hvm_status r;
  hvm_address tmp, port_pool, pbitmap, pipinfo, peprocess;
  unsigned int i, j, k, h, skip, bitmap_size, page_number, page_offset, data_found = 0;
  Bit32u local_ip, pid;
  Bit16u local_port, remote_port, local_ipv6[8];
  Bit8u  bitmap_byte, ipv6;
  MODULE_DATA tcp_module;

  r = WindowsFindModuleByName(cr3, "tcpip.sys", &tcp_module);
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  if(type == UDP) r = MmuReadVirtualRegion(cr3, tcp_module.baseaddr + OFFSET_UDP_PORT_POOL_PTR, &port_pool, sizeof(hvm_address));
  else r = MmuReadVirtualRegion(cr3, tcp_module.baseaddr + OFFSET_TCP_PORT_POOL_PTR, &port_pool, sizeof(hvm_address));
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  r = MmuReadVirtualRegion(cr3, port_pool + OFFSET_BITMAP_PTR, &pbitmap, sizeof(hvm_address));
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  r = MmuReadVirtualRegion(cr3, port_pool + OFFSET_BITMAP_SIZE, &bitmap_size, sizeof(bitmap_size));
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  skip = 1; 
  /* Iterate at byte granularity */
  bitmap_size /= 8;
  for(i = 0; i < bitmap_size; i++) {

    r = MmuReadVirtualRegion(cr3, pbitmap + i, &bitmap_byte, sizeof(bitmap_byte));
    if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;
    
    for(j = skip; j < 8; j++) {

      if((bitmap_byte >> j) & 0x01) {
      
	page_number = ((i*8+j) >> 8) & 0x00ff;
	page_offset =     (i*8+j)    & 0x00ff;

	r = MmuReadVirtualRegion(cr3, port_pool + OFFSET_PAGES_ARRAY + page_number * sizeof(hvm_address), &pipinfo, sizeof(hvm_address));
	if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	r = MmuReadVirtualRegion(cr3, pipinfo + 0x14, &pipinfo, sizeof(hvm_address));
	if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	r = MmuReadVirtualRegion(cr3, (pipinfo + (page_offset << 0x03) + 0x04), &pipinfo, sizeof(hvm_address));
	if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	while(pipinfo) {

	  /* Zeroing low 2 bytes */
	  pipinfo &= 0xfffffffc;

	  if(type == UDP) r = MmuReadVirtualRegion(cr3, pipinfo + OFFSET_UDP_LOCAL_PORT, &local_port, sizeof(local_port));
	  else r = MmuReadVirtualRegion(cr3, pipinfo + OFFSET_SOCKET_LOCAL_PORT, &local_port, sizeof(local_port));
	  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	  if(type == SOCKETS) {
	    r = MmuReadVirtualRegion(cr3, pipinfo - 0x08, &tmp, sizeof(hvm_address));
	    if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	    r = MmuReadVirtualRegion(cr3, tmp + OFFSET_SOCKET_REMOTE_PORT, &remote_port, sizeof(remote_port));
	    if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;
	  }

	  if(type == UDP) {

	    r = MmuReadVirtualRegion(cr3, pipinfo + OFFSET_UDP_IPv6_1, &tmp, sizeof(hvm_address));
	    if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	    r = MmuReadVirtualRegion(cr3, tmp + OFFSET_UDP_IPv6_2, &ipv6, sizeof(ipv6));
	    if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;
	  }
	  else {

	    r = MmuReadVirtualRegion(cr3, pipinfo + OFFSET_IPv6_1, &tmp, sizeof(hvm_address));
	    if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	    r = MmuReadVirtualRegion(cr3, tmp + OFFSET_IPv6_2, &ipv6, sizeof(ipv6));
	    if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;
	  }
	  
	  if(ipv6 != 0x17) ipv6 = 0;

	  if(type == UDP) r = MmuReadVirtualRegion(cr3, pipinfo - 0x14, &tmp, sizeof(hvm_address));
	  else r = MmuReadVirtualRegion(cr3, pipinfo - 0x0c, &tmp, sizeof(hvm_address));
	  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	  if(tmp != 0) {
	  
	    if(ipv6) {

	      for(k = 0, h = 0; k < 0xe; k+=2, h++){
		r = MmuReadVirtualRegion(cr3, tmp + OFFSET_LISTEN_LOCAL_IP + k, &local_ipv6[h], sizeof(local_ipv6[0]));
		if(r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;
	      }
	    }
	    else r = MmuReadVirtualRegion(cr3, tmp + OFFSET_LISTEN_LOCAL_IP, &local_ip, sizeof(local_ip));
	    if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;
	  }
	  else {
	    
	    if(ipv6) local_ipv6[0] = local_ipv6[1] = local_ipv6[2] = local_ipv6[3] = local_ipv6[4] = local_ipv6[5] = local_ipv6[6] = local_ipv6[7] = 0;
	    else local_ip = 0;
	  }

	  if(type == UDP) r = MmuReadVirtualRegion(cr3, pipinfo + OFFSET_UDP_PEPROCESS, &peprocess, sizeof(hvm_address));
	  else r = MmuReadVirtualRegion(cr3, pipinfo + OFFSET_SOCKET_PEPROCESS, &peprocess, sizeof(hvm_address));
	  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	  r = MmuReadVirtualRegion(cr3, peprocess + OFFSET_EPROCESS_UNIQUEPID, &pid, sizeof(pid));
	  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

	  if(data_found < maxsize && pid > 0) {
	    
	    /* NOTE: it appears that an IPInfo struct is created in the bitmap
	       when a connection is ESTABLISHED but with PID == 0. We skim them off
	       as we already have it in the other structure. */

	    vmm_memset(&buf[data_found], 0, sizeof(SOCKET));
	    buf[data_found].state       = SocketStateListen;
	    buf[data_found].local_port  = vmm_ntohs(local_port);
	    buf[data_found].pid         = pid;
	    buf[data_found].protocol    = type==UDP ? 17 : 6;

	    if(type == SOCKETS) buf[data_found].remote_port = vmm_ntohs(remote_port);

	    if(ipv6) {

	      for(k = 0; k < 8; k++)
		buf[data_found].local_ipv6[k]   = local_ipv6[k];

	      buf[data_found].local_ip = -1;
	    }
	    else {
	      buf[data_found].local_ip  = local_ip;
	    }

	    data_found++;
	  }

	  r = MmuReadVirtualRegion(cr3, pipinfo, &pipinfo, sizeof(hvm_address));
	  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;
	}
      }
    }
    skip = 0;
  }

  *psize = data_found;

  Log("[HyperDbg] Search of %s finished. Found: %d", type==UDP?"UDP connections":"sockets", data_found);
  
  return HVM_STATUS_SUCCESS;
}

hvm_status Windows7BuildSocketList(hvm_address cr3, SOCKET* buf, Bit32u maxsize, Bit32u *psize)
{
  hvm_status r;
  unsigned int n1, n2, n3;

  r = Windows7FindNetworkConnections(cr3, buf, maxsize, &n1);
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  r = Windows7FindNetworkData(cr3, buf + n1, maxsize - n1, &n2, SOCKETS);
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;
  
  r = Windows7FindNetworkData(cr3, buf + n1 + n2, maxsize - (n1+n2), &n3, UDP);
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;  

  *psize = (n1+n2+n3);

  return r;
}

#else

static hvm_status WindowsFindNetworkConnections(hvm_address cr3, SOCKET *buf, Bit32u maxsize, Bit32u *psize)
{
  hvm_status r;
  hvm_address table_base, table_entry;
  unsigned int i, j;
  Bit32u table_size;  
  TCPT_OBJECT obj;
  MODULE_DATA tcp_module;

  r = WindowsFindModuleByName(cr3, "tcpip.sys", &tcp_module);
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  /* Read table base */
  r = MmuReadVirtualRegion(cr3, tcp_module.baseaddr + OFFSET_TCB_TABLE, &table_base, sizeof(table_base));
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  /* Read table size */
  r = MmuReadVirtualRegion(cr3, tcp_module.baseaddr + OFFSET_TCB_TABLE_SIZE, &table_size, sizeof(table_size));
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  j = 0;
  for (i=0; i<table_size && i<maxsize; i++) {
    r = MmuReadVirtualRegion(cr3, table_base + (i*4), &table_entry, sizeof(table_entry));
    if (r != HVM_STATUS_SUCCESS) 
      continue;

    if (!table_entry) {
      /* Invalid entry */
      continue;
    }
    
    r = MmuReadVirtualRegion(cr3, table_entry, &obj, sizeof(obj));
    if (r != HVM_STATUS_SUCCESS) 
      continue;
    vmm_memset(&buf[j], 0, sizeof(SOCKET));

    buf[j].state       = SocketStateEstablished;
    buf[j].remote_ip   = obj.RemoteIpAddress;
    buf[j].local_ip    = obj.LocalIpAddress;
    buf[j].remote_port = vmm_ntohs(obj.RemotePort);
    buf[j].local_port  = vmm_ntohs(obj.LocalPort);
    buf[j].pid         = obj.Pid;
    buf[j].protocol    = 7;	/* TCP */
    j++;
  }

  *psize = j;
  return HVM_STATUS_SUCCESS;
}

static hvm_status  WindowsFindNetworkSockets(hvm_address cr3, SOCKET *buf, Bit32u maxsize, Bit32u *psize)
{
  hvm_status r;
  MODULE_DATA tcp_module;
  hvm_address next, table_base, table_entry;
  Bit32u  local_ip, pid, table_size;
  Bit16u local_port, protocol;
  LARGE_INTEGER create_time;

  unsigned int i, j;

  r = WindowsFindModuleByName(cr3, "tcpip.sys", &tcp_module);
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  /* Read addrobj base */
  r = MmuReadVirtualRegion(cr3, tcp_module.baseaddr + OFFSET_ADDROBJ_TABLE, &table_base, sizeof(table_base));
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  /* Read table size */
  r = MmuReadVirtualRegion(cr3, tcp_module.baseaddr + OFFSET_ADDROBJ_TABLE_SIZE, &table_size, sizeof(table_size));
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  j = 0;
  for (i=0; i<table_size; i++) {
    r = MmuReadVirtualRegion(cr3, table_base + (i*4), &table_entry, sizeof(table_entry));
    if (r != HVM_STATUS_SUCCESS) 
      continue;

    if (!table_entry) {
      /* Invalid entry */
      continue;
    }

    while (table_entry && MmIsAddressValid((PVOID) table_entry)) {
      r = MmuReadVirtualRegion(cr3, table_entry + OFFSET_ADDRESS_OBJECT_NEXT, &next, sizeof(next));
      if (r != HVM_STATUS_SUCCESS) break;

      r = MmuReadVirtualRegion(cr3, table_entry + OFFSET_ADDRESS_OBJECT_LOCALIP, &local_ip, sizeof(local_ip));
      if (r != HVM_STATUS_SUCCESS) break;

      r = MmuReadVirtualRegion(cr3, table_entry + OFFSET_ADDRESS_OBJECT_LOCALPORT, &local_port, sizeof(local_port));
      if (r != HVM_STATUS_SUCCESS) break;

      r = MmuReadVirtualRegion(cr3, table_entry + OFFSET_ADDRESS_OBJECT_PID, &pid, sizeof(pid));
      if (r != HVM_STATUS_SUCCESS) break;

      r = MmuReadVirtualRegion(cr3, table_entry + OFFSET_ADDRESS_OBJECT_PROTOCOL, &protocol, sizeof(protocol));
      if (r != HVM_STATUS_SUCCESS) break;

      r = MmuReadVirtualRegion(cr3, table_entry + OFFSET_ADDRESS_OBJECT_CREATETIME, &create_time, sizeof(create_time));
      if (r != HVM_STATUS_SUCCESS) break;

      local_port = vmm_ntohs(local_port);

      vmm_memset(&buf[j], 0, sizeof(SOCKET));
      buf[j].state       = SocketStateListen;
      buf[j].local_ip    = local_ip;
      buf[j].local_port  = local_port;
      buf[j].pid         = pid;
      buf[j].protocol    = protocol;

      j++;
      table_entry = next;
    }
  }

  *psize = j;

  return HVM_STATUS_SUCCESS;
}

hvm_status WindowsBuildSocketList(hvm_address cr3, SOCKET* buf, Bit32u maxsize, Bit32u *psize)
{
  hvm_status r;
  unsigned int n1, n2;

  r = WindowsFindNetworkConnections(cr3, buf, maxsize, &n1);
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  r = WindowsFindNetworkSockets(cr3, buf + n1, maxsize - n1, &n2);
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  *psize = (n1+n2);

  return HVM_STATUS_SUCCESS;
}

#endif
