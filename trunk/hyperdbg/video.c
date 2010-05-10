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

#include "video.h"
#include "font_256.h"
#include "pci.h"
#include "debug.h"

/* ################ */
/* #### MACROS #### */
/* ################ */

/* FB writing */
#define FONT_X 8
#define FONT_Y 12
#define TOTCHARS 256
#define FONT_NEXT_LINE TOTCHARS * FONT_X
#define FONT_BPP 8
#define BYTE_PER_PIXEL FONT_BPP>>3

#define FRAME_BUFFER_SIZE (video_sizex*video_sizey*FRAME_BUFFER_RESOLUTION_DEPTH)
#define FRAME_BUFFER_RESOLUTION_DEPTH 4

/* Various video memory addresses */
#define VIDEO_ADDRESS_BOCHS   0xe0000000
#define DEFAULT_VIDEO_ADDRESS VIDEO_ADDRESS_BOCHS 

/* Uncomment this line to disable video autodetect */
/* #define VIDEO_ADDRESS_MANUAL */

/* ################# */
/* #### GLOBALS #### */
/* ################# */

static PULONG video_mem = NULL;
static ULONG  ulVideoAddress = 0;
static ULONG **video_backup;
static ULONG video_sizex, video_sizey;

/* ################ */
/* #### BODIES #### */
/* ################ */

NTSTATUS VideoInit(VOID)
{
  NTSTATUS r;

#ifndef VIDEO_ADDRESS_MANUAL
  PCIInit();
  r = PCIDetectDisplay(&ulVideoAddress);
  if (!NT_SUCCESS(r)) {
    DbgPrint("[E] PCI display detection failed!");
    return STATUS_UNSUCCESSFUL;
  }
#else
  ulVideoAddress = (ULONG) DEFAULT_VIDEO_ADDRESS;
#endif
  
  DbgPrint("[*] Found PCI display region at physical address %.8x\n", ulVideoAddress);

  /* Set default screen resolution */
  video_sizex = VIDEO_DEFAULT_RESOLUTION_X;
  video_sizey = VIDEO_DEFAULT_RESOLUTION_Y;

  return STATUS_SUCCESS;
}

NTSTATUS VideoFini(VOID)
{
  return STATUS_SUCCESS;
}

VOID VideoSetResolution(ULONG x, ULONG y)
{
  video_sizex = x;
  video_sizey = y;
}

NTSTATUS VideoAlloc(VOID)
{
  PHYSICAL_ADDRESS pa;
  ULONG32 i;
  pa.u.HighPart = 0;
  pa.u.LowPart  = ulVideoAddress;

  /* Allocate memory to save current pixels */
  video_backup = MmAllocateNonCachedMemory(FONT_Y * SHELL_SIZE_Y * sizeof(ULONG));
  if(video_backup == 0) return STATUS_UNSUCCESSFUL;

  for(i = 0; i < FONT_Y * SHELL_SIZE_Y; i++) {
    video_backup[i] = MmAllocateNonCachedMemory(FONT_X * SHELL_SIZE_X * sizeof(ULONG));
    if(video_backup[i] == 0) return STATUS_UNSUCCESSFUL;
  }

  /* Map video memory */
  video_mem = (PULONG) MmMapIoSpace(pa, FRAME_BUFFER_SIZE, MmWriteCombined);
  if (!video_mem)
    return STATUS_UNSUCCESSFUL;

  return STATUS_SUCCESS;
}

NTSTATUS VideoDealloc(VOID)
{
  ULONG32 i;

/*   ComPrint("[HyperDbg] Deallocating video!\n"); */

  for(i = 0; i < FONT_Y * SHELL_SIZE_Y; i++)
    MmFreeNonCachedMemory(video_backup[i], FONT_X * SHELL_SIZE_X * sizeof(ULONG));

  MmFreeNonCachedMemory(video_backup, FONT_Y * SHELL_SIZE_Y * sizeof(ULONG));
  if (video_mem) {
    MmUnmapIoSpace(video_mem, FRAME_BUFFER_SIZE);
    video_mem = NULL;
  }

  return STATUS_SUCCESS;
}

BOOLEAN VideoEnabled(VOID)
{  
  return (video_mem != 0);
}

/* Writes a string starting from (start_x, start_y) 
 * 
 * WARNING: str is wrapped on following line if longer than the shell and will
 * overwrite everything on its path ;-)
 */
VOID VideoWriteString(char *str, unsigned int len, unsigned int color, unsigned int start_x, unsigned int start_y) 
{
  int cur_x, cur_y;
  unsigned int i;

  cur_x = start_x;
  cur_y = start_y;
  i = 0;

/*   DbgPrint("[V] Writing string %s\n", str); */
  while(i < len) {
    /* '-2' is to avoid overwriting frame */
    if(cur_x == SHELL_SIZE_X - 2) { 
      /* Reset x */
      cur_x = 2;

      /* WARNING: this will lead to overwriting the frame, be careful when
	 using VideoWriteString */
      /* Down one line, or up to 0 */
      cur_y = (cur_y + 1) % SHELL_SIZE_Y;
    }

    VideoWriteChar(str[i], color, cur_x, cur_y);
    cur_x++;
    i++;
  }
}

/* Gets a character from the font map and draws it on the screen */
VOID VideoWriteChar(UCHAR c, unsigned int color, unsigned int x, unsigned int y)
{
  /* Used to loop on font size */
  unsigned int x_pix, y_pix; 

  /* Offset on the screen and on the font map */
  int offset_font_base, offset_screen_base, offset_font_x, offset_screen_y, offset_screen_x;
  int is_pixel_set;

  /* Offset in the font map */
  offset_font_base = (int) c * FONT_X; 

  /* Real offset in the FB */
  offset_screen_base = x * FONT_X + y * FONT_Y * video_sizex; 

  /* For each pixel both in width and in height of a font char */
  for(x_pix = 0; x_pix < FONT_X; x_pix++) {
    /* Get current offset in the font map and in the FB */
    offset_font_x = offset_font_base + x_pix;
    offset_screen_x = offset_screen_base + x_pix;

    for(y_pix = 0; y_pix < FONT_Y; y_pix++) {
      /* Get current y offset in */
      offset_screen_y = (FONT_Y - y_pix - 1) * video_sizex;

      /* Check in the font map if we have to draw the current pixel */
      is_pixel_set = (font_data[(offset_font_x + y_pix*FONT_NEXT_LINE)*BYTE_PER_PIXEL]>>3);

      if(is_pixel_set) { 
	/* Let's draw it! */
	video_mem[offset_screen_x + offset_screen_y] = color;
      } else { 
	/* Let's paint it black! */
	video_mem[offset_screen_x + offset_screen_y] = BGCOLOR;
      }
    }
  }
}

VOID VideoClear(ULONG color)
{
  int i,j;

  for(j = 0; j < FONT_Y * SHELL_SIZE_Y; j++) {
    for(i = 0; i < FONT_X * SHELL_SIZE_X; i++) {
      video_mem[(j*video_sizex)+i] = color;
    }
  }
}

VOID VideoSave(VOID)
{
  int i,j;

  for(j = 0; j < FONT_Y * SHELL_SIZE_Y; j++) {
    for(i = 0; i < FONT_X * SHELL_SIZE_X; i++) {
      video_backup[j][i] = video_mem[(j*video_sizex)+i];
    }
  }
}

VOID VideoRestore(VOID)
{
  int i,j;

  for(j = 0; j < FONT_Y * SHELL_SIZE_Y; j++) {
    for(i = 0; i < FONT_X * SHELL_SIZE_X; i++) {
      video_mem[(j*video_sizex)+i] = video_backup[j][i];
    }
  }
}
