# What is HyperDbg? #
HyperDbg is a kernel debugger that leverages hardware-assisted
virtualization. More precisely, HyperDbg is based on a minimalistic hypervisor
that is installed while the system runs. Compared to traditional kernel
debuggers (e.g., `WinDbg`, `SoftIce`, `Rasta R0 Debugger`) HyperDbg is completely
transparent to the kernel and can be used to debug kernel code without the need
of serial (or USB) cables. For example, HyperDbg allows to single step the
execution of the kernel, even when the kernel is executing exception and
interrupt handlers. Compared to traditional virtual machine based debuggers
(e.g., the VMware builtin debugger), HyperDbg does not require the kernel to be
run as a guest of a virtual machine, although it is as powerful.

Once loaded, the debugger will sits in background and will pop up the GUI when
the F12 hot-key is pressed or when a debug event occurs.

# Current development status #

The current version of HyperDbg is a prototype and will evolve drastically in
the future. Currently the debugger allows to set breakpoints, to single step
the execution of the kernel, to inspect the memory of the operating system and
of a particular process. Furthermore, HyperDbg include some VM-Introspection techniques that allows to gather useful information about the state of its guest OS such as the list of the running processes, loaded modules and drivers and network connections.
However, the debugger currently does not distinguish
between threads, has limited support for kernel's symbols, and has does not
clone shared pages before setting a breakpoint. Future version of the debugger
will be based on an enhanced version of the hypervisor that guarantees complete
isolation and transparency.

HyperDbg currently only supports:

  * systems with PS/2 keyboards
  * systems with Intel VT-x
  * systems running 32-bit OSes and applications
  * Windows XP **and** Linux (still experimental, click [here](http://code.google.com/p/hyperdbg/wiki/Linux) for more info).

HyperDbg renders the GUI by writing directly to the memory of the video
card. Some video cards are known to give problems. The debugger does not work
correctly when the accelerated drivers for the following cards are loaded:

  * Intel 82915g
  * nvidia GeForce 9800GT
  * nvidia GeForce GT 130

If you have any of the aforementioned cards (and you are using the accelerated
driver) or if the interface is not correctly rendered on the screen, you have
to disable the driver in order to be able to use HyperDbg. The driver used by
default by Windows XP does not give any problem.

See the [Install](Install.md) page for compilation instructions.