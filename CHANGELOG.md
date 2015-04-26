# CHANGELOG #

## 18 May 2012 ##

We welcome _pago_ (Mattia Pagnozzi) as a committer of the HyperDbg project. Thanks for your contributions.

### Bug Fixes: ###
Some minor bug fixes.

### Support for new guests: ###
  * Windows 7 32 bit (tested on Home Basic / Home Premium): http://goo.gl/DgmxI
  * Linux Kernel 3.2 32 bit without PAE (tested on Debian, linux-image-3.2.0-2-486): http://goo.gl/GSIjp

### New Features: ###
  * 16 entries cyclic command history
  * memory dump and disassembler commands are now CR3 specific
  * breakpoints can be set as permanent and CR3 dependent
  * breakpoints listing
  * VM-Introspection for Win 7: IPv4/v6, Processes, Drivers
  * VM-Introspection for Linux: IPv4, Processes (Modules are WIP)
  * experimental features to mangle with Win 7 scheduler

## 26 Dec 2010 ##

We switched from WDK compiler to GCC and we now support Linux (still experimental). See [Linux](Linux.md) and [Windows](Windows.md) renewed compilation instructions!

### Bug Fixes: ###
Some minor bug fixes.

## 3 Aug 2010 ##

### New Features: ###
  * New basic VM-Introspection capabilities:
    * list of _guest_ processes;
    * list of _guest_ drivers;
    * list of _guest_ network connections.

  * HyperDbg GUI now has a _more-like_ pager to handle commands with very long output.

  * HyperDbg now displays also the TID of the currently running thread along with the PID of the process.

  * HyperDbg is now able to intercept and handle hypercall 0xdead0002. We will use this hypercall to implement (user|kernel)space calls to the hypervisor through `VMCALL`.

  * The hypervisor now uses its own CR3.

  * PAE support added. HyperDbg can now be run on systems that use PAE too.

  * HyperDbg now automatically detects video card **and** screen resolution (XP only). Thanks to Jon Larimer.

  * Core and HyperDbg code are now 99.9% VMX-independent.

  * HyperDbg loader now checks that VMX is not disabled by the BIOS. Thanks to Jon Larimer.

### Bug Fixes: ###
  * Fixed [issue #3](https://code.google.com/p/hyperdbg/issues/detail?id=#3): Unloading fails when PAE is enabled.