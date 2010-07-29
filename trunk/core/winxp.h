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

#ifndef _PILL_WINXP_H
#define _PILL_WINXP_H

#include <ntddk.h>
#include "types.h"

/* Default segment selectors */
#define WINDOWS_DEFAULT_DS 0x23
#define WINDOWS_DEFAULT_FS 0x30

/* In-memory location of IDT base address */
#define WINDOWS_PIDT_BASE  0xffdff038

/* Structure offsets */
#define OFFSET_EPROCESS_UNIQUEPID      0x084
#define OFFSET_EPROCESS_ACTIVELINKS    0x088
#define OFFSET_EPROCESS_IMAGEFILENAME  0x174
#define OFFSET_EPROCESS_FLAGS          0x248
#define OFFSET_EPROCESS_EXITSTATUS     0x24c 

#define OFFSET_KPROCESS_STATE          0x065

#define OFFSET_ETHREAD_CID             0x1ec

#define OFFSET_KTHREAD_STATE           0x02d
#define OFFSET_KTHREAD_WAITLISTENTRY   0x060
#define OFFSET_KTHREAD_QUEUELISTENTRY  0x118
#define OFFSET_KTHREAD_THREADLISTENTRY 0x1b0

#define KTHREAD_STATE_RUNNING          0x02

#define OFFSET_EPROCESS_ACTIVELINKS    0x088
#define OFFSET_EPROCESS_PEB            0x1b0

#define OFFSET_PEB_LDRDATA             0x00c

/* Bits */
#define EPROCESS_FLAGS_EXITING (1 << 2)
#define EPROCESS_FLAGS_DELETE  (1 << 3)

#define EPROCESS_EXITSTATUS_PENDING 0x103

/* Opaque structures (i.e., we don't need them, so we don't include their
   declarations) */
typedef struct _KTRAP_FRAME KTRAP_FRAME, *PKTRAP_FRAME;
typedef struct _KPRCB KPRCB, *PKPRCB;
typedef struct _KQUEUE KQUEUE, *PKQUEUE;

typedef unsigned short WORD;

typedef struct _LDR_MODULE {
  LIST_ENTRY              InLoadOrderModuleList;
  LIST_ENTRY              InMemoryOrderModuleList;
  LIST_ENTRY              InInitializationOrderModuleList;
  PVOID                   BaseAddress;
  PVOID                   EntryPoint;
  ULONG                   SizeOfImage;
  UNICODE_STRING          FullDllName;
  UNICODE_STRING          BaseDllName;
  ULONG                   Flags;
  SHORT                   LoadCount;
  SHORT                   TlsIndex;
  LIST_ENTRY              HashTableEntry;
  ULONG                   TimeDateStamp;
} LDR_MODULE, *PLDR_MODULE;

typedef struct _SYSTEM_MODULE_INFORMATION {
  ULONG Reserved[2];
  PVOID Base;
  ULONG Size;
  ULONG Flags;
  USHORT Index;
  USHORT Unknow;
  USHORT LoadCount;
  USHORT ModuleNameOffset;
  char ImageName[256];    
} SYSTEM_MODULE_INFORMATION, *PSYSTEM_MODULE_INFORMATION;

typedef struct {
  CSHORT         Type;
  USHORT         Size;
  PDEVICE_OBJECT DeviceObject;
  ULONG          PowerFlags;
  PVOID          Dope;
  ULONG          ExtensionFlags;
  PVOID          DeviceNode;
  PDEVICE_OBJECT AttachedTo;
  ULONG          StartIoCount;
  ULONG          StartIoKey;
  PVOID          Vpb;
} R_DEVOBJ_EXTENSION, *PR_DEVOBJ_EXTENSION;

typedef struct _PORT_KEYBOARD_EXTENSION {
  PDEVICE_OBJECT Self;
  PKINTERRUPT    InterruptObject;
} PORT_KEYBOARD_EXTENSION, *PPORT_KEYBOARD_EXTENSION;

typedef struct _KINTERRUPT {
  CSHORT            Type;
  CSHORT            Size;
  LIST_ENTRY        InterruptListEntry;
  ULONG             ServiceRoutine;
  ULONG             ServiceContext;
  KSPIN_LOCK        SpinLock;
  ULONG             TickCount;
  PKSPIN_LOCK       ActualLock;
  PVOID             DispatchAddress;
  ULONG             Vector;
  KIRQL             Irql;
  KIRQL             SynchronizeIrql;
  BOOLEAN           FloatingSave;
  BOOLEAN           Connected;
  CHAR              Number;
  UCHAR             ShareVector;
  KINTERRUPT_MODE   Mode;
  ULONG             ServiceCount;
  ULONG             DispatchCount;
  ULONG             DispatchCode[106];
} KINTERRUPT, *PKINTERRUPT;

typedef struct _KGDTENTRY {
  WORD LimitLow;
  WORD BaseLow;
  ULONG HighWord;
} KGDTENTRY, *PKGDTENTRY;

typedef struct _KIDTENTRY {
  WORD Offset;
  WORD Selector;
  WORD Access;
  WORD ExtendedOffset;
} KIDTENTRY, *PKIDTENTRY;

typedef struct _KEXECUTE_OPTIONS {
  ULONG ExecuteDisable:1;
  ULONG ExecuteEnable:1;
  ULONG DisableThunkEmulation:1;
  ULONG Permanent:1;
  ULONG ExecuteDispatchEnable:1;
  ULONG ImageDispatchEnable:1;
  ULONG Spare:2;
} KEXECUTE_OPTIONS, *PKEXECUTE_OPTIONS;

typedef struct _KPROCESS {
  DISPATCHER_HEADER Header;
  LIST_ENTRY ProfileListHead;
  ULONG DirectoryTableBase;
  ULONG Unused0;
  KGDTENTRY LdtDescriptor;
  KIDTENTRY Int21Descriptor;
  WORD IopmOffset;
  UCHAR Iopl;
  UCHAR Unused;
  ULONG ActiveProcessors;
  ULONG KernelTime;
  ULONG UserTime;
  LIST_ENTRY ReadyListHead;
  SINGLE_LIST_ENTRY SwapListEntry;
  PVOID VdmTrapcHandler;
  LIST_ENTRY ThreadListHead;
  ULONG ProcessLock;
  ULONG Affinity;
  union {
    ULONG AutoAlignment:1;
    ULONG DisableBoost:1;
    ULONG DisableQuantum:1;
    ULONG ReservedFlags:29;
    LONG ProcessFlags;
  };
  CHAR BasePriority;
  CHAR QuantumReset;
  UCHAR State;
  UCHAR ThreadSeed;
  UCHAR PowerState;
  UCHAR IdealNode;
  UCHAR Visited;
  union {
    KEXECUTE_OPTIONS Flags;
    UCHAR ExecuteOptions;
  };
  ULONG StackCount;
  LIST_ENTRY ProcessListEntry;
  UINT64 CycleTime;
} KPROCESS, *PKPROCESS;

typedef struct _PEB_LDR_DATA {
  ULONG Length;
  UCHAR Initialized;
  PVOID SsHandle;
  LIST_ENTRY InLoadOrderModuleList;
  LIST_ENTRY InMemoryOrderModuleList;
  LIST_ENTRY InInitializationOrderModuleList;
  PVOID EntryInProgress;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

hvm_status WindowsInit(PDRIVER_OBJECT DriverObject);
hvm_status WindowsGetKeyboardVector(unsigned char*);
hvm_status WindowsGetKernelBase(hvm_address*);
hvm_status WindowsFindModule(hvm_address cr3, hvm_address rip, PWCHAR name, Bit32u namesz);
Bit32u     WindowsGuessFrames(void);
hvm_bool   WindowsProcessIsTerminating(hvm_address cr3);

/* Process-related functions */
hvm_status WindowsFindProcess(hvm_address cr3, hvm_address* ppep);
hvm_status WindowsFindProcessPid(hvm_address cr3, hvm_address* ppid);
hvm_status WindowsFindProcessTid(hvm_address cr3, hvm_address* ptid);
hvm_status WindowsFindProcessName(hvm_address cr3, char* name);

#endif	/* _PILL_WINXP_H */
