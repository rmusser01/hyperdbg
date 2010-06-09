/*
Copyright notice
================

Copyright (C) 2010
Jon      Larimer    <jlarimer@gmail.com>
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

/* xpvideo.h
*
* Routines for detecting resolution and video adapter framebuffer address in Window XP
*
* Jon Larimer <jlarimer@gmail.com>
* June 8, 2010
*
*/

#ifdef XPVIDEO

#ifndef _XPVIDEO_H
#define _XPVIDEO_H

#include "hyperdbg.h"

/* Get XP display info using top secret undocumented and very hacky techniques */
hvm_status XpVideoGetWindowsXPDisplayData(hvm_address *addr, Bit32u *framebuffer_size, Bit32u *width, Bit32u *height, Bit32u *stride);

#endif	/* _XPVIDEO_H */

#endif
