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

#ifndef _EVENTS_H
#define _EVENTS_H

#include "types.h"
#include <stddef.h>  /* Includes size_t type */
#include "vt.h"			 /* Includes VtRegister */

typedef enum {
  EventPublishNone,              /* Event not accepted */
  EventPublishHandled,		 /* The event should not be passed at the next
				    handler or at the guest OS */
  EventPublishPass,		 /* The event should be passed on */
} EVENT_PUBLISH_STATUS;

/* This structure is the argument that is passed to event handlers */
typedef struct {
  union {
    struct {
      Bit8u    size;
      hvm_bool isstring;
      hvm_bool isrep;
    } EventIO;
#ifdef ENABLE_EPT
    struct {
      hvm_address guestLinearAddress;
      hvm_address guestPhysicalAddress;
      hvm_bool    is_linear_valid;
      hvm_bool    in_page_walk;
      Bit8u       attemptType;                     /* rwx */      
    } EventEPTViolation;
#endif		
    struct {
      Bit8u      crno;
      hvm_bool   iswrite;
      VtRegister gpr;
    } EventCR;
  };
} EVENT_ARGUMENTS, *PEVENT_ARGUMENTS;

/* If you add an event type, you must also modify EventCheckCondition() in events.c */
typedef enum {
  EventNone = 0,
  EventHypercall,
  EventException,
  EventIO,
  EventControlRegister,
  EventHlt,
#ifdef ENABLE_EPT
  EventEPTViolation,
#endif
} HVM_EVENT_TYPE;

typedef EVENT_PUBLISH_STATUS (*EVENT_CALLBACK)(PEVENT_ARGUMENTS);

#ifdef ENABLE_EPT
typedef struct _EVENT_CONDITION_EPT_VIOLATION {
  hvm_bool read;
  hvm_bool write;     
  hvm_bool exec;       
  hvm_bool is_linear_valid;
  hvm_bool in_page_walk;
  hvm_bool fill_an_entry;

} EVENT_CONDITION_EPT_VIOLATION, *PEVENT_CONDITION_EPT_VIOLATION;
#endif

typedef struct _EVENT_CONDITION_HYPERCALL {
  Bit32u hypernum;
} EVENT_CONDITION_HYPERCALL, *PEVENT_CONDITION_HYPERCALL;

typedef struct _EVENT_CONDITION_EXCEPTION {
  Bit32u exceptionnum;
} EVENT_CONDITION_EXCEPTION, *PEVENT_CONDITION_EXCEPTION;

typedef enum {
  EventIODirectionBoth = 0,
  EventIODirectionIn,
  EventIODirectionOut,
} EVENT_IO_DIRECTION;

typedef struct _EVENT_CONDITION_IO {
  EVENT_IO_DIRECTION direction;
  Bit32u portnum;
} EVENT_CONDITION_IO, *PEVENT_CONDITION_IO;

typedef struct _EVENT_CONDITION_CR {
  Bit8u    crno;
  hvm_bool iswrite;
} EVENT_CONDITION_CR, *PEVENT_CONDITION_CR;

typedef int EVENT_CONDITION_NONE;

hvm_status EventInit(void);
hvm_bool EventSubscribe(HVM_EVENT_TYPE type, void* pcondition, int condition_size, EVENT_CALLBACK callback);
hvm_bool EventUnsubscribe(HVM_EVENT_TYPE type, void* pcondition, int condition_size);
hvm_bool EventHasType(HVM_EVENT_TYPE type);
EVENT_PUBLISH_STATUS EventPublish(HVM_EVENT_TYPE type, PEVENT_ARGUMENTS args, void* pcondition, int condition_size);

void EventUpdateExceptionBitmap(Bit32u* pbitmap);
void EventUpdateIOBitmaps(Bit8u* pIOBitmapA, Bit8u* pIOBitmapB);

#endif	/* _EVENTS_H */
