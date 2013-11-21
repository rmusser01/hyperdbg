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

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#include "common.h"

extern HWND g_hWndEdit;

void ShowError(const char *msg, ...) {
  va_list ap;
  char tmp[1024];
  
  va_start(ap, msg);
  _vsnprintf(tmp, sizeof(tmp), msg, ap);
  va_end(ap);

  MessageBox(NULL, tmp, "Error", MB_ICONEXCLAMATION | MB_OK);
}

void LogMessage(const char *fmt, ...)
{
  va_list ap;
  char newdata[1024], *data;
  SYSTEMTIME lt;
  LRESULT n;

  GetLocalTime(&lt);
  _snprintf(newdata, sizeof(newdata), "[%02d-%02d-%04d %02d:%02d:%02d.%03d] ",
	   lt.wDay, lt.wMonth, lt.wYear, lt.wHour, lt.wMinute, lt.wSecond, lt.wMilliseconds);

  va_start(ap, fmt);
  _vsnprintf((char*)newdata + strlen(newdata), 
	    sizeof(newdata) - strlen(newdata) - 1, 
	    fmt, ap);
  va_end(ap);

  strncat(newdata, "\r\r\n", sizeof(newdata));

  n = SendMessage(g_hWndEdit, EM_GETLIMITTEXT, 0, 0);
  data = (char*) malloc(n + strlen(newdata));
  if(!data) {
    ShowError("Dynamic allocation error.");
    return;
  }

  /* read existing text */
  SendMessage(g_hWndEdit, WM_GETTEXT, n, (LPARAM) data);

  /* add new text */
  strncat(data, newdata, (n + strlen(newdata)));

  /* update text */
  SendMessage(g_hWndEdit, WM_SETTEXT, 0, (LPARAM) data); 
}
