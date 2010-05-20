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

/* kudos to Invisible Things Lab */

#include "comio.h"
#include "debug.h"
#include "common.h"
#include "vmmstring.h"
#include "x86.h"
#include <stdarg.h>

#define TRANSMIT_HOLDING_REGISTER	   0x00
#define RECEIVER_BUFFER_REGISTER           0x00
#define INTERRUPT_ENABLE_REGISTER          0x01
#define INTERRUPT_IDENTIFICATION_REGISTER  0x02
#define LINE_STATUS_REGISTER		   0x05

/* Meaning of the various bits in the LINE_STATUS_REGISTER */
#define LSR_DATA_AVAILABLE              (1 << 0)
#define LSR_OVERRUN_ERROR               (1 << 1)
#define LSR_PARITY_ERROR                (1 << 2)
#define LSR_FRAMING_ERROR               (1 << 3)
#define LSR_BREAK_SIGNAL                (1 << 4)
#define LSR_THR_EMPTY                   (1 << 5)
#define LSR_THR_EMPTY_AND_IDLE          (1 << 6)
#define LSR_ERR_DATA                    (1 << 7)

/* INTERRUPT_ENABLE_REGISTER bits */
#define IER_RECEIVED_DATA               (1 << 0)
#define IER_TRANSMITTER_EMPTY           (1 << 1) 
#define IER_RECEIVER_CHANGED            (1 << 2)
#define IER_MODEM_CHANGED               (1 << 3)
#define IER_SLEEP_MODE                  (1 << 4)
#define IER_LOW_POWER_MODE              (1 << 5)
#define IER_RESERVED1                   (1 << 6)
#define IER_RESERVED2                   (1 << 7)

static Bit16u  DebugComPort = 0;
static Bit32u  ComSpinLock;	/* Spin lock that guards accesses to the COM port */

void __stdcall ComInit()
{
  CmInitSpinLock(&ComSpinLock);
}

void __stdcall ComPrint(Bit8u* fmt, ...)
{
  va_list args;
  Bit8u str[1024] = {0};
  unsigned int i;

  va_start (args, fmt);

  CmAcquireSpinLock(&ComSpinLock);

  vmm_vsnprintf(str, sizeof(str), fmt, args);
  for (i = 0; i < strlen(str); i++)
    PortSendByte(str[i]);

  CmReleaseSpinLock(&ComSpinLock);
}

Bit8u __stdcall ComIsInitialized()
{
  /* Ok, we should also check if ComInit() has been invoked.. but we assume it
     has */
  return (DebugComPort != 0);
}

void __stdcall PortInit()
{
  DebugComPort = COM_PORT_ADDRESS;
}

void __stdcall PortSendByte(Bit8u b)
{
  /* Empty input buffer */
  while (!(IoReadPortByte(DebugComPort + LINE_STATUS_REGISTER) & LSR_THR_EMPTY));

  IoWritePortByte(DebugComPort + TRANSMIT_HOLDING_REGISTER, b);
}

Bit8u __stdcall PortRecvByte(void)
{
  /* Wait until we receive something -- busy waiting!! */
  while ((IoReadPortByte(DebugComPort + LINE_STATUS_REGISTER) & LSR_DATA_AVAILABLE) == 0);

  return IoReadPortByte(DebugComPort + RECEIVER_BUFFER_REGISTER);
}
