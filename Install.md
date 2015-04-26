


# Compilation #
To compile HyperDbg and the analysis infrastructure you need different setups whether you want to compile it under Windows or Linux. Please see the appropriate wiki page:

  * [Linux](Linux.md)
  * [Windows](Windows.md)

# Video issues #

Please check out the [Video](Video.md) wiki page if you are experiencing problems with your video card and HyperDbg.

# HyperGui #

In hyperdbg/tools/hypergui/ you can find an user-friendly HyperDbg loader that
won't be compiled by default when building HyperDbg. To compile it, just move
to the aforementioned directory and run:

```
build /czgw
```

after building HyperGui, you can just ship HyperGui.exe and hyperdbg.sys on the
target machine and double click on HyperGui.exe. HyperGui.exe also offers a
command line interface.

**NOTE**: we are soon going to port HyperGui to gcc too!

# symbol2c.py #

In hyperdbg/tools/ you can find a Python script to automatically parse symbols
for HyperDbg. You have to dowload from microsoft.com the appropriate
symbol file (.pdb). Once you get hold of the file you can launch:

```
$ python symbol2c.py file.pdb syms.c
```

This command will generate the file syms.c. Replace the file hyperdbg/syms.c
with the newly created one and recompile.

Note that, to use symbol2c.py, you need to install the pdbparse library,
available at:

http://code.google.com/p/pdbparse/