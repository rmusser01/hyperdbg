# Video issues #

If automatic video card detection does not work on your system (e.g., detection
will not work if you are going to test HyperDbg on Bochs), please disable
automatic video card detection **before compiling**, by uncommenting the
following line in hyperdbg/video.c:

```
#define VIDEO_ADDRESS_MANUAL
```

and manually set the address of the video card memory by defining the
`DEFAULT_VIDEO_ADDRESS` macro, like below:

```
#define DEFAULT_VIDEO_ADDRESS 0xdeadbeef // where 0xdeadbeef is the address on your machine
```


### How to find video address on Windows ###
To find the appropriate value for this macro, you just need to go to 'Control
Panel' and double-click on 'System'. Then, switch to the 'Hardware' tab and
click on Hardware Devices'. From there, locate the entry of your video card,
right-click on it and select 'Properties'. Go to the 'Resources' tab and find
'Memory Interval', the reported value is the one that you need to insert in the
code. It is possible to find multiple entries labeled as 'Memory Interval'; if
this is the case, select the bigger interval, it will look like something like
this:

```
Memory Interval   0xE0000000 - 0xE0FFFFFF
```
In this case, `0xE0000000` is the address you need to set in the code.

### How to find video address on Linux ###
To find the appropriate value for this macro, you just need to type the following command in a shell:
```
$ lspci -vvv
```

once located the entry relative to your video card, you will find a line
similar to this:

```
Region 1: Memory at d0000000 (32-bit, prefetchable) [size=128M]
```

`d0000000` is the address you need to set in the code.