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

#ifndef _NETWORK_H
#define _NETWORK_H

#include "types.h"

Bit16u vmm_ntohs(Bit16u v);
Bit32u vmm_ntohl(Bit32u v);

void inet_ntoa(Bit32u a, char *destination_buffer);
void inet_ntoa_v6(Bit16u *a, char *internal_buffer);

typedef enum {
  SocketStateUnknown = 0,
  SocketStateListen,
  SocketStateEstablished,
} SOCKET_STATE;

typedef struct _SOCKET {
  SOCKET_STATE state;
  Bit32u remote_ip;
  Bit32u local_ip;
  Bit16u remote_port;
  Bit16u local_port;
  Bit32u pid;
  Bit16u protocol;
  Bit16u local_ipv6[8];
  Bit16u remote_ipv6[8];
} SOCKET;

hvm_status NetworkBuildSocketList(hvm_address cr3, SOCKET *buf, Bit32u maxsize, Bit32u *psize);

#endif	/* _NETWORK_H */
