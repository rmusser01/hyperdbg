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

#ifndef _PILL_H
#define _PILL_H

#include "config.h"
#include "debug.h"
#include "mmu.h"
#include "vt.h"
#include "events.h"
#include "x86.h"
#include "msr.h"
#include "idt.h"
#include "comio.h"
#include "common.h"
#include "vmhandlers.h"

#ifdef ENABLE_HYPERDBG
#include "hyperdbg_guest.h"
#include "hyperdbg_host.h"
#endif

hvm_status FiniPlugin(void);

#ifdef GUEST_WINDOWS
#include <ddk/ntddk.h>

  typedef  KAFFINITY (*t_KeQueryActiveProcessors)(void);
  typedef  VOID (*t_KeSetSystemAffinityThread)( KAFFINITY Affinity);
  typedef ULONG (*t_KeGetCurrentProcessorNumber)(void); /* FIXME */

  NTSTATUS DDKAPI DriverEntry( PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
  VOID     DDKAPI DriverUnload( PDRIVER_OBJECT DriverObject);

#elif defined GUEST_LINUX
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

  MODULE_LICENSE("GPL");

  int  __init DriverEntry(void);
  void __exit DriverUnload(void);
#endif

/* ################ */
/* #### MACROS #### */
/* ################ */

#define HYPERCALL_SWITCHOFF 0xcafebabe

/* #################### */
/* #### PROTOTYPES #### */
/* #################### */
void       StartVT(void)  asm("_StartVT");
hvm_status RegisterEvents(void);
void       InitVMMIDT(PIDT_ENTRY pidt);

/* Assembly functions (defined in i386/vmx-asm.S) */
void DoStartVT(void);

/* ################# */
/* #### GLOBALS #### */
/* ################# */
extern hvm_address EntryRFlags asm("_EntryRFlags");
extern hvm_address EntryRAX    asm("_EntryRAX");
extern hvm_address EntryRCX    asm("_EntryRCX");
extern hvm_address EntryRDX    asm("_EntryRDX");
extern hvm_address EntryRBX    asm("_EntryRBX");
extern hvm_address EntryRSP    asm("_EntryRSP");
extern hvm_address EntryRBP    asm("_EntryRBP");
extern hvm_address EntryRSI    asm("_EntryRSI");
extern hvm_address EntryRDI    asm("_EntryRDI");   

extern hvm_address GuestStack  asm("_GuestStack");

#endif /* _PILL_H */
