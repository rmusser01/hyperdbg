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

#include "types.h"
#include "process.h"
#include "network.h"

#include <ddk/ntddk.h>

/* Default segment selectors */
#define WINDOWS_DEFAULT_DS 0x23
#define WINDOWS_DEFAULT_FS 0x30

/* In-memory location of IDT base address */
#define WINDOWS_PIDT_BASE  0xffdff038

/* Structure offsets */

#ifdef GUEST_WIN_7

#define OFFSET_EPROCESS_QUANTUMRESET     0x061
#define OFFSET_EPROCESS_UNIQUEPID        0x0b4
#define OFFSET_EPROCESS_ACTIVELINKS      0x0b8
#define OFFSET_EPROCESS_IMAGEFILENAME    0x16c
#define OFFSET_EPROCESS_FLAGS            0x270
#define OFFSET_EPROCESS_EXITSTATUS       0x274 

#define OFFSET_KPROCESS_STATE            0x000

#define OFFSET_KPROCESS_THREADLISTHEAD   0x02c
#define OFFSET_KPROCESS_READYLISTHEAD    0x044

#define OFFSET_ETHREAD_CID               0x22c

#define OFFSET_KTHREAD_STATE             0x068
#define OFFSET_KTHREAD_WAITLISTENTRY     0x074
#define OFFSET_KTHREAD_QUEUELISTENTRY    0x120
#define OFFSET_KTHREAD_PROCESS           0x150
#define OFFSET_KTHREAD_QUANTUMRESET      0x197
#define OFFSET_KTHREAD_THREADLISTENTRY   0x1e0

#define KTHREAD_STATE_RUNNING            0x002
#define KTHREAD_STATE_READY              0x001

#define OFFSET_EPROCESS_PEB              0x1a8

#define OFFSET_PEB_LDRDATA               0x00c

/* scheduling offsets */

#define OFFSET_PRCB_CURRENT_THREAD      0x0004
#define OFFSET_PRCB_NEXT_THREAD         0x0008
#define OFFSET_PRCB_IDLE_THREAD         0x000c

/* LIST_ENTRY struct */
#define OFFSET_PRCB_WAITLISTHEAD        0x31e0
/* 32 bits bitmap */
#define OFFSET_PRCB_READYSUMMARY        0x31ec
/* array of 32 _LIST_ENTRY struct */
#define OFFSET_PRCB_DISPATCHER_READY    0x3220
/* _SINGLE_LIST_ENTRY struct */
#define OFFSET_PRCB_DEFERRED_READY      0x31f4

/* tcpip.sys internal offsets for Win 7 32 bit */

#define SOCKETS                        0x00000
#define UDP                            0x00001

#define OFFSET_PARTITION_TABLE         0xf7020
#define OFFSET_PARTITION_COUNTER       0xf7026

#define OFFSET_PART_ENTRY_MAXNUM       0x00008
#define OFFSET_PART_ENTRY_HASHTABLE    0x00020

#define OFFSET_IPINFO_PORTS            0x00024
#define OFFSET_IPINFO_PEPROCESS        0x00160
#define OFFSET_IPINFO_IPADDRESSES     -0x00004
#define OFFSET_REMOTE_IP               0x00008
#define OFFSET_LOCAL_IP                0x00080
#define OFFSET_IPv6_1                 -0x00008
#define OFFSET_IPv6_2                  0x0000c

#define OFFSET_TCP_PORT_POOL_PTR       0xf77a0
#define OFFSET_UDP_PORT_POOL_PTR       0xf7a5c

#define OFFSET_BITMAP_SIZE             0x00050
#define OFFSET_BITMAP_PTR              0x00054
#define OFFSET_PAGES_ARRAY             0x00058
#define OFFSET_SOCKET_LOCAL_PORT      -0x00002
#define OFFSET_SOCKET_REMOTE_PORT      0x0000c
#define OFFSET_SOCKET_PEPROCESS       -0x00028

#define OFFSET_SOCKET_IPv6_1          -0x00038
#define OFFSET_SOCKET_IPv6_2           0x0000c

#define OFFSET_UDP_LOCAL_PORT         -0x00004
#define OFFSET_UDP_PEPROCESS          -0x00034

#define OFFSET_LISTEN_LOCAL_IP         0x00080

#define OFFSET_UDP_IPv6_1             -0x00038
#define OFFSET_UDP_IPv6_2              0x0000c

#else /* GUEST_WINDOWS XP */

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

#define KTHREAD_STATE_RUNNING          0x002
#define OFFSET_KPROCESS_THREADLISTHEAD 0x050

#define OFFSET_EPROCESS_ACTIVELINKS    0x088
#define OFFSET_EPROCESS_PEB            0x1b0

#define OFFSET_PEB_LDRDATA             0x00c

/* tcpip.sys internal offsets */
#define OFFSET_TCB_TABLE               0x493e8
#define OFFSET_TCB_TABLE_SIZE          0x3f3b0
#define OFFSET_ADDROBJ_TABLE           0x48360
#define OFFSET_ADDROBJ_TABLE_SIZE      0x48364

#define OFFSET_ADDRESS_OBJECT_NEXT       0x000
#define OFFSET_ADDRESS_OBJECT_LOCALIP    0x02c
#define OFFSET_ADDRESS_OBJECT_LOCALPORT  0x030
#define OFFSET_ADDRESS_OBJECT_PROTOCOL   0x032
#define OFFSET_ADDRESS_OBJECT_PID        0x148
#define OFFSET_ADDRESS_OBJECT_CREATETIME 0x158

#endif

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

#pragma pack (push, 1)
/* This is the structure that maps established connections */
typedef struct _TCPT_OBJECT {
  /* 0x0 */
  struct _TCPT_OBJECT *Next;
  /* 0x4 */
  ULONG32 reserved1;
  /* 0x8 */
  ULONG32 reserved2;
  /* 0xc */
  ULONG32 RemoteIpAddress;
  /* 0x10 */
  ULONG32 LocalIpAddress;
  /* 0x14 */
  USHORT RemotePort;
  /* 0x16 */
  USHORT LocalPort;
  /* 0x18 */
  ULONG32 Pid;
} TCPT_OBJECT, *PTCPT_OBJECT;

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
#pragma pack (pop)

hvm_status WindowsInit(PDRIVER_OBJECT DriverObject);
hvm_status WindowsGetKeyboardVector(unsigned char*);
hvm_status WindowsGetKernelBase(hvm_address*);
hvm_status  WindowsFindModuleByName(hvm_address cr3, Bit8u *name, MODULE_DATA *pmodule);
/* Find the module that contains the specified address ('rip') */
hvm_status WindowsFindModule(hvm_address cr3, hvm_address addr, PWCHAR name, Bit32u namesz);
Bit32u     WindowsGuessFrames(void);
hvm_bool   WindowsProcessIsTerminating(hvm_address cr3);

/* Process-related functions */
hvm_status WindowsFindProcess(hvm_address cr3, hvm_address* ppep);
hvm_status WindowsFindProcessPid(hvm_address cr3, hvm_address* ppid);
hvm_status WindowsFindProcessTid(hvm_address cr3, hvm_address* ptid);
hvm_status WindowsFindProcessName(hvm_address cr3, char* name);
hvm_status WindowsGetNextProcess(hvm_address cr3, PROCESS_DATA* pprev, PROCESS_DATA* pnext);
hvm_status WindowsGetNextModule(hvm_address cr3, MODULE_DATA* pprev, MODULE_DATA* pnext);
hvm_status Windows7UnlinkProc(hvm_address cr3, hvm_address target_pep, hvm_address target_kthread, unsigned int *dispatcher_index, Bit8u *success);
hvm_status Windows7RelinkProc(hvm_address cr3, hvm_address target_kthread, hvm_address dispatcher_index);
hvm_status Windows7FindTargetInfo(hvm_address cr3, hvm_address *target_pep, hvm_address *target_kthread);
/* Network-related functions */
hvm_status WindowsBuildSocketList(hvm_address cr3, SOCKET* buf, Bit32u maxsize, Bit32u *psize);

#endif	/* _PILL_WINXP_H */
