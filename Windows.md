HyperDbg is almost completely OS-Independent. However, some of its part aren't (e.g., the installation driver). In this page we will keep an updated record of the development status of the Windows version.

# Windows Support #

Windows XP is the first system HyperDbg was developed for. Even if still under development, this version of HyperDbg is more stable than the one for Linux.

So far we have tested it on different hardware and emulated machines running the following versions of Windows:
  * Windows XP SP2
  * Windows XP SP3

## Compilation ##

  1. Download and install `MinGW` and `MSYS` [here](http://sourceforge.net/projects/mingw/files/Automated%20MinGW%20Installer/mingw-get-inst/mingw-get-inst-20101030/)
  1. Download HyperDbg code as explained [here](http://code.google.com/p/hyperdbg/source/checkout)
  1. Edit Makefile.windows with your favorite editor and find the following definitions:
```
-DVIDEO_DEFAULT_RESOLUTION_X=
-DVIDEO_DEFAULT_RESOLUTION_Y=
-DVIDEO_ADDRESS_MANUAL
```
  1. Modify them to your needs. If you eliminate the -DVIDEO\_ADDRESS\_MANUAL row HyperDbg will try to automatically retrieve your video card address.
  1. Compile with the following command:
```
$ make -f Makefile.windows
```

Please refer to the [Video](Video.md) page if you are experiencing issues with the video card.

## Known Bugs && Limitations ##

  * Trying to use HyperDbg on a Windows system where an accelerated video driver is being used will most probably lead to a failure
  * The option xpauto (-DXPVIDEO) is not currently working: see [issue #4](https://code.google.com/p/hyperdbg/issues/detail?id=#4)