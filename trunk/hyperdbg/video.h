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

#ifndef _VIDEO_H
#define _VIDEO_H

/* Frame buffer resolutions must be defined UNLESS we are using xp automatic resolution detection */
#ifndef XPVIDEO
#ifndef VIDEO_DEFAULT_RESOLUTION_X
#error VIDEO_DEFAULT_RESOLUTION_X must be defined! 
#endif
#endif

#ifndef XPVIDEO
#ifndef VIDEO_DEFAULT_RESOLUTION_Y
#error VIDEO_DEFAULT_RESOLUTION_Y must be defined! 
#endif
#endif

#define SHELL_SIZE_X 100
#define SHELL_SIZE_Y 50
#define MAX_INPUT_SIZE 94
#define OUT_START_X 2
#define OUT_END_X 98
#define OUT_START_Y 12
#define OUT_END_Y 47
#define OUT_SIZE_X OUT_END_X - OUT_START_X
#define OUT_SIZE_Y OUT_END_Y - OUT_START_Y

#define GUI_COLORIZED

#ifdef GUI_COLORIZED

/* Colorized GUI */
#define BLACK       0x00000000
#define WHITE       0xffffffff
#define VOMIT_GREEN 0x0000c618
#define LIGHT_GREEN 0x0000f800
#define LIGHT_BLUE  0x00009cd3
#define DARK_BLUE   0x00000780
#define BLUE        0x000044b0
#define CYAN        0x0000ffe0
#define RED         0xffff0000
#define BGCOLOR     BLACK

#else 

/* Black & white GUI */
#define BLACK       0x00000000
#define WHITE       BLACK
#define VOMIT_GREEN BLACK
#define LIGHT_GREEN BLACK
#define LIGHT_BLUE  BLACK
#define DARK_BLUE   BLACK
#define BLUE        BLACK
#define CYAN        BLACK
#define RED         BLACK
#define BGCOLOR     0xffffffff // White background

#endif

#include "hyperdbg.h"

hvm_status VideoInit(void);
hvm_status VideoFini(void);
hvm_status VideoAlloc(void);
hvm_status VideoDealloc(void);

hvm_bool VideoEnabled(void);

void VideoSetResolution(Bit32u x, Bit32u y);
void VideoSave(void);
void VideoRestore(void);
void VideoWriteChar(Bit8u, unsigned int, unsigned int, unsigned int);
void VideoWriteString(char*, unsigned int, unsigned int, unsigned int, unsigned int);
void VideoClear(Bit32u color);
hvm_address VideoGetAddress(void);
Bit32u VideoGetFrameBufferSize(void);
#endif	/* _VIDEO_H */

