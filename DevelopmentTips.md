We currently use the following setup for the development and testing of HyperDbg

# Compiling #

**NOTE: the following instructions refer to older versions of HyperDbg, please refer to renewed instructions if you want to compile the SVN version: [Linux](Linux.md) [Windows](Windows.md)**

To compile HyperDbg code, we usually upload the code into a Virtual Box virtual machine running Windows XP. To compile the code, you need to install the [Windows Driver Kit](http://www.microsoft.com/whdc/devtools/wdk/wdkpkg.mspx).

We believe that the quickest way to share the code between the development machine and the one used to compile is to use Virtual Box's shared folders. To compile, open a **Windows XP x86 Free Build Environment** shell, move to the shared directory where HyperDbg code resides ($HDBG), and type:
```
build_code.cmd hyperdbg xres yres
```
If everything compiles correctly, you should find the following file:
```
$HDBG\bin\i386\pill.sys
```
This is the minimal driver containing HyperDbg itself and the loader.

# Testing #

The testing environment is a little bit more complex. Although a physical machine can be used for the testing we suggest to use [Bochs](http://bochs.sourceforge.net/)(2.4.5). You need to create a Windows XP image for BOCHS. The fastest way to share a file (e.g., pill.sys) between your host and the guest running in BOCHS is to create a ISO image and then to bind the image to cdrom simulated by BOCHS. Alternatively, a floppy image can be used. You can use `mkisofs` to create the ISO image:
```
$ mkisofs -o cd.iso pill.sys
```
Then, you can make it available into the Windows guest through the BOCHS GUI.

To start HyperDbg you have two choices:

  * HyperGui (in $HDBG\hyperdbg\hypergui\);

  * [ORSLOADER](http://www.osronline.com/article.cfm?article=157).

Another useful tool to debug loading problems is [DbgView](http://technet.microsoft.com/en-us/sysinternals/bb896647.aspx). Using this tool you can analyze the initial output generated during the loading of HyperDbg.


## Bochs Serial Port Setup ##

Once the hypervisor is loaded, debug messages are sent to the serial port. These messages can be enabled by uncommenting the line:
```
#define DEBUG 1
```
in the file $HDBG\core\debug.h. To capture these messages, you also need to configure BOCHS to simulate a serial port and to record into a file all the output on this port. To do so you have add the following line to BOCHS's configuration file:
```
com1: enabled=1, mode=file,dev=serial.txt
```
`serial.txt` is the name of the file in which debug messages will be saved.


## Speed up the testing ##

Since Windows within BOCHS is very slow and since BSODs can occur very frequently, we suggest to take a snapshot of the system once booted. Unfortunately the last time we tried, BOCHS failed to properly restore a snapshot. To workaround the issue we adopt the following tricks:
  * we run a Virtual Box machine (Windows or Linux);
  * we launch BOCHS within the Virtual Box machine;
  * once the OS within BOCHS is booted, we take a snapshot of the Virtual Box machine.

Thus, it is sufficient to restore the Virtual Box machine, to restore BOCHS.