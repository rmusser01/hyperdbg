HyperDbg is almost completely OS-Independent. However, some of its part aren't (e.g., the installation driver). In this page we will keep an updated record of the development status of the Linux version.

# Linux Support #

So far, Linux support in HyperDbg is still experimental and far from being complete.
However, we have tested it on these distro/kernels:

  * Debian _squeeze_ 32bit/2.6.26.2
  * Debian _squeeze_ 32bit/2.6.30.4
  * Debian _sid_ 32bit/2.6.32.5

At the moment, there is no support for PAE, so no _bigmem_ kernels.

## Compilation ##

  1. If you don't have them already, download the headers for your kernel:
```
# apt-get install linux-headers-$(uname -r)
```
  1. Download HyperDbg code, as explained [here](http://code.google.com/p/hyperdbg/source/checkout)
  1. Edit Makefile.linux with your favorite editor and find the following definitions:
```
-DVIDEO_DEFAULT_RESOLUTION_X=
-DVIDEO_DEFAULT_RESOLUTION_Y=
-DVIDEO_ADDRESS_MANUAL
```
  1. Modify them to your needs. If you eliminate the -DVIDEO\_ADDRESS\_MANUAL line, HyperDbg will try to automatically retrieve your video card address.
  1. Copy Makefile.linux over Kbuild
  1. Compile with the following command:
```
$ make -f Makefile.linux
```

Please refer to the [Video](Video.md) page if you are experiencing issues with your video card.


## Known Bugs && Limitations ##

  * As for Windows, trying to use HyperDbg on a Linux system where an accelerated video driver is being used will most probably lead to a failure
  * We do not detect video resolution automatically
  * No support for symbols
  * No introspection techniques implemented