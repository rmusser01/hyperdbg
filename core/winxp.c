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
} WindowsSymbols;

static hvm_address WindowsFindPsLoadedModuleList(PDRIVER_OBJECT DriverObject);

/* ################ */
/* #### BODIES #### */
/* ################ */

/* NOTE: To be invoked in non-root mode! */
hvm_status WindowsInit(PDRIVER_OBJECT DriverObject)
{
  UNICODE_STRING u;

  RtlInitUnicodeString(&u, L"NtQuerySystemInformation");
  WindowsSymbols.NtQuerySystemInformation = (NTQUERYSYSTEMINFORMATION) MmGetSystemRoutineAddress(&u);  
  if (!WindowsSymbols.NtQuerySystemInformation) return HVM_STATUS_UNSUCCESSFUL;

  WindowsSymbols.PsLoadedModuleList = WindowsFindPsLoadedModuleList(DriverObject);
  if (WindowsSymbols.PsLoadedModuleList == 0) return HVM_STATUS_UNSUCCESSFUL;

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

  WindowsSymbols.NtQuerySystemInformation(SystemModuleInformation, (PVOID) &Byte, 0, &Byte);
      
  pBuffer = MmAllocateNonCachedMemory(Byte);          
  if(!pBuffer)
    return HVM_STATUS_UNSUCCESSFUL;
                
  if(WindowsSymbols.NtQuerySystemInformation(SystemModuleInformation, pBuffer, Byte, &Byte)) {
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

hvm_status WindowsFindModule(hvm_address cr3, hvm_address rip, PWSTR name, Bit32u namesz)
{
  NTSTATUS    r;
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
	rip < (ULONG) module.BaseAddress + (ULONG)module.SizeOfImage && \
	rip >= (ULONG)module.BaseAddress) {
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
  hvm_address head, cur;
  hvm_address cur_cr3;
  LIST_ENTRY  le;
  hvm_status  r;
  hvm_bool    found;

  /* Iterate over ("visible") system processes */
  head = (hvm_address) PsInitialSystemProcess;
  cur = head;

  found = FALSE;
  do {
    /* Read the CR3 of the current process */
    r = MmuReadVirtualRegion(cr3, cur + FIELD_OFFSET(KPROCESS, DirectoryTableBase), &cur_cr3, sizeof(cur_cr3));
    if (r == HVM_STATUS_SUCCESS && cur_cr3 == cr3) {
      /* Found! */
      found = TRUE;
      *ppep = cur;
      break;
    }

    /* Go on with the next process */
    r = MmuReadVirtualRegion(cr3, cur + OFFSET_EPROCESS_ACTIVELINKS, &le, sizeof(le));
    if (r != HVM_STATUS_SUCCESS) {
      Log("[WinXP] Can't read LIST_ENTRY at offset %.8x", cur + OFFSET_EPROCESS_ACTIVELINKS);
      break;
    }

    cur = (hvm_address) (le.Flink) - OFFSET_EPROCESS_ACTIVELINKS;
  } while (cur != head);

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
  r = MmuReadVirtualRegion(cr3, pep + FIELD_OFFSET(KPROCESS, ThreadListHead), &kthread_head, sizeof(kthread_head));
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
      Log("[WinXP] Can't read LIST_ENTRY at offset %.8x", kthread_current + OFFSET_KTHREAD_THREADLISTENTRY);
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
