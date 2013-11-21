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

#ifndef _KEYBOARD_H
#define _KEYBOARD_H

/* In READ mode. Can be read at any time */
#define KEYB_REGISTER_STATUS  0x64

/* In WRITE mode. Writing this port sets Bit 3 of the status register to 1 and
   the byte is treated as a controller command */
#define KEYB_REGISTER_COMMAND 0x64

/* In READ mode. Should be read if bit 0 of status register is 1 */
#define KEYB_REGISTER_OUTPUT  0x60

/* In WRITE mode. Data should only be written if Bit 1 of the status register
   is zero (register is empty) */
#define KEYB_REGISTER_DATA    0x60

/* Read a keystroke from keyboard. If 'unget' is true, the read character is
   sent back to the device. */
hvm_status KeyboardReadKeystroke(Bit8u* pc, hvm_bool unget, hvm_bool* pisMouse);
Bit8u    KeyboardScancodeToKeycode(Bit8u c);
hvm_status KeyboardInit(void);

/* Enable/disable mouse */
hvm_status KeyboardSetMouse(hvm_bool enabled);

typedef struct {
  hvm_bool lshift;
  hvm_bool rshift;
  hvm_bool lctrl;
  hvm_bool lalt;
} KEYBOARD_STATUS;

/* Global that holds current keyboard status */
extern KEYBOARD_STATUS keyboard_status;

#endif /* _KEYBOARD_H */
