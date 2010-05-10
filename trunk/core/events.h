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

#ifndef _EVENTS_H
#define _EVENTS_H

#include <ntddk.h>

typedef enum {
  EventPublishNone,              /* Event not accepted */
  EventPublishHandled,		 /* The event should not be passed at the next
				    handler or at the guest OS */
  EventPublishPass,		 /* The event should be passed on */
} EVENT_PUBLISH_STATUS;

/* If you add an event type, you must also modify EventCheckCondition() in events.c */
typedef enum {
  EventNone = 0,
  EventHypercall,
  EventException,
  EventIO,
  EventControlRegister,
  EventHlt,
} EVENT_TYPE;

typedef EVENT_PUBLISH_STATUS (*EVENT_CALLBACK)(VOID);

typedef struct _EVENT_CONDITION_HYPERCALL {
  ULONG hypernum;
} EVENT_CONDITION_HYPERCALL, *PEVENT_CONDITION_HYPERCALL;

typedef struct _EVENT_CONDITION_EXCEPTION {
  ULONG exceptionnum;
} EVENT_CONDITION_EXCEPTION, *PEVENT_CONDITION_EXCEPTION;

typedef enum {
  EventIODirectionBoth = 0,
  EventIODirectionIn,
  EventIODirectionOut,
} EVENT_IO_DIRECTION;

typedef struct _EVENT_CONDITION_IO {
  EVENT_IO_DIRECTION direction;
  ULONG portnum;
} EVENT_CONDITION_IO, *PEVENT_CONDITION_IO;

typedef struct _EVENT_CONDITION_CR {
  UCHAR   crno;
  BOOLEAN iswrite;
} EVENT_CONDITION_CR, *PEVENT_CONDITION_CR;

typedef int EVENT_CONDITION_NONE;

NTSTATUS EventInit(VOID);
BOOLEAN EventSubscribe(EVENT_TYPE type, PVOID pcondition, int condition_size, EVENT_CALLBACK callback);
BOOLEAN EventUnsubscribe(EVENT_TYPE type, PVOID pcondition, int condition_size);
BOOLEAN EventHasType(EVENT_TYPE type);
EVENT_PUBLISH_STATUS EventPublish(EVENT_TYPE type, PVOID pcondition, int condition_size);

VOID EventUpdateExceptionBitmap(PULONG pbitmap);
VOID EventUpdateIOBitmaps(PUCHAR pIOBitmapA, PUCHAR pIOBitmapB);

#endif	/* _EVENTS_H */
