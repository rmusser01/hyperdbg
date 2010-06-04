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

/* Module for handling generic events. */

#include "types.h"
#include "events.h"
#include "common.h"
#include "debug.h"

/* ################ */
/* #### MACROS #### */
/* ################ */

#define N_EVENTS 512

/* ############### */
/* #### TYPES #### */
/* ############### */

typedef struct _EVENT {
  EVENT_TYPE type;

  union {
    EVENT_CONDITION_HYPERCALL hypercall;
    EVENT_CONDITION_EXCEPTION exception;
    EVENT_CONDITION_IO io;
    EVENT_CONDITION_CR cr;
    EVENT_CONDITION_NONE none;
  } condition;

  EVENT_CALLBACK callback;
} EVENT, *PEVENT;

/* ################# */
/* #### GLOBALS #### */
/* ################# */

static EVENT events[N_EVENTS];

/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

static PEVENT   EventFindInternal(EVENT_TYPE type, void* pcondition, int condition_size);
static hvm_bool EventCheckCondition(EVENT_TYPE type, void* c1, void* c2);

/* ################ */
/* #### BODIES #### */
/* ################ */

hvm_status EventInit(void)
{
  int i;

  for (i=0; i<sizeof(events)/sizeof(EVENT); i++)
    events[i].type = EventNone;

  return HVM_STATUS_SUCCESS;
}

hvm_bool EventSubscribe(EVENT_TYPE type, void* pcondition, int condition_size, EVENT_CALLBACK callback)
{
  hvm_bool b;
  int i;

  if (!pcondition) return FALSE;

  b = FALSE;
  for (i=0; i<sizeof(events)/sizeof(EVENT); i++) {
    if (events[i].type != EventNone)
      continue;

    events[i].type = type;
    memcpy(&(events[i].condition), pcondition, condition_size);
    events[i].callback = callback;

    b = TRUE;
    break;
  }

  return b;
}

hvm_bool EventUnsubscribe(EVENT_TYPE type, void* pcondition, int condition_size)
{
  PEVENT p;

  if (!pcondition) return FALSE;

  p = EventFindInternal(type, pcondition, condition_size);
  if (!p)
    return FALSE;

  p->type = EventNone;
  return TRUE;
}

EVENT_PUBLISH_STATUS EventPublish(EVENT_TYPE type, PEVENT_ARGUMENTS args, void* pcondition, int condition_size)
{
  int i;
  hvm_bool b;
  EVENT_PUBLISH_STATUS s;

  s = EventPublishNone;

  if (!pcondition) return s;

  for (i=0; i<sizeof(events)/sizeof(EVENT); i++) {
    if (events[i].type != type) {
      /* Event type mismatch */
      continue;
    }

    /* Check if event conditions match */
    b = EventCheckCondition(type, pcondition, &(events[i].condition));

    if (!b)
      continue;

    /* Found a matching event */
    s = events[i].callback(args);
      
    if (s == EventPublishHandled) {
      /* No more events to process */
      break;
    }
  }

  return s
;
}

hvm_bool EventHasType(EVENT_TYPE type)
{
  int i;
  hvm_bool b;

  b = FALSE;
  for (i=0; i<sizeof(events)/sizeof(EVENT); i++) {
    if (events[i].type == type) {
      b = TRUE;
      break;
    }
  }

  return b;
}

void EventUpdateExceptionBitmap(Bit32u* pbitmap)
{
  int i;

  for (i=0; i<sizeof(events)/sizeof(EVENT); i++) {  
    if (events[i].type != EventException)
      continue;

    CmSetBit32(pbitmap, events[i].condition.exception.exceptionnum);
  }
}

void EventUpdateIOBitmaps(Bit8u* pIOBitmapA, Bit8u* pIOBitmapB)
{
  int i;
  Bit16u port;

  for (i=0; i<sizeof(events)/sizeof(EVENT); i++) {  
    if (events[i].type != EventIO)
      continue;

    port = (Bit16u) events[i].condition.io.portnum;

    if (port < 0x8000) {
      pIOBitmapA[port / 8] |= 1 << (port & 8);
    } else {
      pIOBitmapB[(port - 0x8000) / 8] |= 1 << ((port - 0x8000) & 8);
    }
  }
}

static hvm_bool EventCheckCondition(EVENT_TYPE type, void* c1, void* c2)
{
  hvm_bool b;

  b = FALSE;
  switch (type) {
  case EventHypercall: {
    PEVENT_CONDITION_HYPERCALL p1, p2;

    p1 = (PEVENT_CONDITION_HYPERCALL) c1;
    p2 = (PEVENT_CONDITION_HYPERCALL) c2;

    if (p1->hypernum == p2->hypernum) {
      b = TRUE;
    }
    break;
  }

  case EventException: {
    PEVENT_CONDITION_EXCEPTION p1, p2;

    p1 = (PEVENT_CONDITION_EXCEPTION) c1;
    p2 = (PEVENT_CONDITION_EXCEPTION) c2;
    if (p1->exceptionnum == p2->exceptionnum) {
      b = TRUE;
    }
    break;
  }

  case EventIO: {
    PEVENT_CONDITION_IO p1, p2;

    p1 = (PEVENT_CONDITION_IO) c1;
    p2 = (PEVENT_CONDITION_IO) c2;
    if ((p1->direction == p2->direction) && (p1->portnum == p2->portnum)) {
      b = TRUE;
    }
    break;
  }

  case EventControlRegister: {
    PEVENT_CONDITION_CR p1, p2;

    p1 = (PEVENT_CONDITION_CR) c1;
    p2 = (PEVENT_CONDITION_CR) c2;
    if ((p1->crno == p2->crno) && (p1->iswrite == p2->iswrite)) {
      b = TRUE;
    }
    break;
  }

  case EventHlt: {
    b = TRUE;
    break;
  }

  default: 
    break;
  }

  return b;
}

static PEVENT EventFindInternal(EVENT_TYPE type, void* pcondition, int condition_size)
{
  int i;

  for (i=0; i<sizeof(events)/sizeof(EVENT); i++) {
    if (events[i].type != type)
      continue;

    if (!memcmp(&(events[i].condition), pcondition, condition_size)) {
      return &(events[i]);
    }
  }

  return NULL;
}
