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

static ULONG WindowsFindPsLoadedModuleList(PDRIVER_OBJECT DriverObject);

/* ################ */
/* #### BODIES #### */
/* ################ */

/* NOTE: To be invoked in non-root mode! */
NTSTATUS WindowsInit(PDRIVER_OBJECT DriverObject)
{
  UNICODE_STRING u;

  RtlInitUnicodeString(&u, L"NtQuerySystemInformation");
  WindowsSymbols.NtQuerySystemInformation = (NTQUERYSYSTEMINFORMATION) MmGetSystemRoutineAddress(&u);  
  if (!WindowsSymbols.NtQuerySystemInformation) return STATUS_UNSUCCESSFUL;

  WindowsSymbols.PsLoadedModuleList = WindowsFindPsLoadedModuleList(DriverObject);
  if (WindowsSymbols.PsLoadedModuleList == 0) return STATUS_UNSUCCESSFUL;

  return STATUS_SUCCESS;
}

static ULONG WindowsFindPsLoadedModuleList(PDRIVER_OBJECT DriverObject)
{
  ULONG v;

  v = 0;

  if (!DriverObject) return 0;

  v = *(PULONG) ((ULONG) DriverObject + FIELD_OFFSET(DRIVER_OBJECT, DriverSection));

  return v;
}

/* FIXME: Hidden processes (e.g., unlinked from the EPROCESS list) pwn us
   :-) See "Rootkit Detection in Windows Systems" (Rutskowa) */
NTSTATUS WindowsFindProcess(ULONG cr3, PULONG ppep)
{
  ULONG      head, cur;
  ULONG      cur_cr3;
  LIST_ENTRY le;
  NTSTATUS   r;
  BOOLEAN    found;

  /* Iterate over ("visible") system processes */
  head = (ULONG) PsInitialSystemProcess;
  cur = head;

  found = FALSE;
  do {
    /* Read the CR3 of the current process */
    r = MmuReadVirtualRegion(cr3, (ULONG) cur + FIELD_OFFSET(KPROCESS, DirectoryTableBase), &cur_cr3, sizeof(cur_cr3));
    if (r == STATUS_SUCCESS && cur_cr3 == cr3) {
      /* Found! */
      found = TRUE;
      *ppep = cur;
      break;
    }

    /* Go on with the next process */
    r = MmuReadVirtualRegion(cr3, (ULONG) cur + OFFSET_EPROCESS_ACTIVELINKS, &le, sizeof(le));
    if (r != STATUS_SUCCESS) {
      Log("[WinXP] Can't read LIST_ENTRY at offset %.8x", (ULONG) cur + OFFSET_EPROCESS_ACTIVELINKS);
      break;
    }

    cur = (ULONG) (le.Flink) - OFFSET_EPROCESS_ACTIVELINKS;
  } while (cur != head);

  return found ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

NTSTATUS WindowsGetKeyboardVector(PUCHAR pv)
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
    return r;

  while(pDeviceObject->DeviceType != FILE_DEVICE_8042_PORT) {
    PR_DEVOBJ_EXTENSION pde;

    // Go down to the lower-level object
    pde = (PR_DEVOBJ_EXTENSION) pDeviceObject->DeviceObjectExtension;
    if (pde->AttachedTo)
      pDeviceObject = pde->AttachedTo;
    else // We reached the lowest level
      return STATUS_UNSUCCESSFUL;
  }

  // pDeviceObject == i8042prt's DeviceObject
  pKInterrupt = (PKINTERRUPT) ((PPORT_KEYBOARD_EXTENSION) pDeviceObject->DeviceExtension)->InterruptObject;

  *pv = (UCHAR) (pKInterrupt->Vector & 0xff);

  return STATUS_SUCCESS;
}

/* NOTE: To be invoked in non-root mode! */
NTSTATUS WindowsGetKernelBase(PULONG pbase)
{
  ULONG i, Byte, ModuleCount;
  PVOID pBuffer;
  PSYSTEM_MODULE_INFORMATION pSystemModuleInformation;
  NTSTATUS r;

  WindowsSymbols.NtQuerySystemInformation(SystemModuleInformation, (PVOID) &Byte, 0, &Byte);
      
  pBuffer = MmAllocateNonCachedMemory(Byte);          
  if(!pBuffer)
    return STATUS_UNSUCCESSFUL;
                
  if(WindowsSymbols.NtQuerySystemInformation(SystemModuleInformation, pBuffer, Byte, &Byte)) {
    MmFreeNonCachedMemory(pBuffer, Byte);
    return STATUS_UNSUCCESSFUL;
  }

  ModuleCount = *(PULONG) pBuffer;
  pSystemModuleInformation = (PSYSTEM_MODULE_INFORMATION)((PUCHAR) pBuffer + sizeof(ULONG));

  r = STATUS_UNSUCCESSFUL;

  for(i = 0; i < ModuleCount; i++) {
    if(strstr(pSystemModuleInformation->ImageName, "ntoskrnl.exe") ||
       strstr(pSystemModuleInformation->ImageName, "ntkrnlpa.exe")) {
      r = STATUS_SUCCESS;
      *pbase = (ULONG) pSystemModuleInformation->Base;
      break;
    }
    pSystemModuleInformation++;
  }
                
  MmFreeNonCachedMemory(pBuffer, Byte);
  return r;
}

NTSTATUS WindowsFindModule(ULONG cr3, ULONG eip, PWSTR name, ULONG namesz)
{
  ULONG      v, proc, pldr, module_current, module_head;
  LIST_ENTRY le;
  NTSTATUS   r;
  PEB_LDR_DATA ldr;
  LDR_MODULE module;
  CHAR namec[64]; 
  /* Find the EPROCESS structure for the process with the specified cr3
     value */
  r = WindowsFindProcess(cr3, &proc);
/*   ComPrint("[WinXP] proc %08hx name %s\n", proc, proc+OFFSET_EPROCESS_IMAGEFILENAME); */
  if (r != STATUS_SUCCESS) return r;

  /* Read the PEB address */
  r = MmuReadVirtualRegion(cr3, (ULONG) proc + OFFSET_EPROCESS_PEB, &v, sizeof(v));
  if (r != STATUS_SUCCESS ) {
    ComPrint("[HyperDbg] Can't read PEB at offset %.8x\n", (ULONG) proc + OFFSET_EPROCESS_PEB);
    return STATUS_UNSUCCESSFUL;
  }

/*   ComPrint("PEB @%.8x\n", v); */
  if (v == 0) {
    /* System process -- use PsLoadedModuleList as the head of the list of
       loaded modules  */
    module_current = module_head = WindowsSymbols.PsLoadedModuleList;
  } else {
    /* Read the PPEB_LDR_DATA from the process */
    r = MmuReadVirtualRegion(cr3, (ULONG) v + OFFSET_PEB_LDRDATA, &pldr, sizeof(pldr));
    if (r != STATUS_SUCCESS ) {
      ComPrint("[HyperDbg] Can't read PPEB_LDR_DATA at offset %.8x\n", (ULONG) v + OFFSET_PEB_LDRDATA);
      return STATUS_UNSUCCESSFUL;
    }

    r = MmuReadVirtualRegion(cr3, pldr, &ldr, sizeof(ldr));
    if (r != STATUS_SUCCESS ) {
      ComPrint("[HyperDbg] Can't read PEB_LDR_DATA at offset %.8x\n", (ULONG) pldr);
      return STATUS_UNSUCCESSFUL;
    }

    module_current = module_head = (ULONG) ldr.InLoadOrderModuleList.Flink;
  }

  /* Traverse the modules list */
  do {
    r = MmuReadVirtualRegion(cr3, (ULONG) module_current - FIELD_OFFSET(LDR_MODULE, InLoadOrderModuleList), 
			     &module, sizeof(module));
    if (r != STATUS_SUCCESS ) break;
    vmm_memset((PUCHAR)name, 0, namesz*2);
    r = MmuReadVirtualRegion(cr3, (ULONG) module.BaseDllName.Buffer, name, 
			     MIN(module.BaseDllName.MaximumLength, namesz));
    if (r == STATUS_SUCCESS ) {
      if(eip < (ULONG)module.BaseAddress+(ULONG)module.SizeOfImage && eip >= (ULONG)module.BaseAddress) {
	return STATUS_SUCCESS; /* FOUND! */
      }
    }
    module_current = (ULONG) module.InLoadOrderModuleList.Flink;
  } while (module_current != module_head);

  /* TODO */
  return STATUS_UNSUCCESSFUL;
}

ULONG WindowsGuessFrames(VOID)
{
  ULONG dwPhyPages;

  dwPhyPages = *(PULONG) (SHAREDUSERDATA + KUSER_SHARED_DATA_PHYPAGES_OFFSET);

  return dwPhyPages;
}

BOOLEAN WindowsProcessIsTerminating(ULONG cr3)
{
  ULONG pep, flags;
  BOOLEAN b;
  NTSTATUS r;

  r = WindowsFindProcess(cr3, &pep);
  
  b = TRUE;

  if (r == STATUS_SUCCESS) {
    /* Read the Flags field of the EPROCESS structure */
    r = MmuReadVirtualRegion(cr3, pep + OFFSET_EPROCESS_FLAGS, &flags, sizeof(flags));
    if (r == STATUS_SUCCESS && (flags & EPROCESS_FLAGS_DELETE) == 0) {
      /* Still running */
      b = FALSE;
    }
  }

  return b;
}

