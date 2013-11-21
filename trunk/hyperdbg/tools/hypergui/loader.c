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

#include "loader.h"
#include "common.h"

static char driver_filename[MAX_PATH];

DWORD LoaderLoadDriver()
{
  DWORD x;
  SC_HANDLE hSCM, hService;

  debug("[*] Loading driver: %s", driver_filename);

  hSCM = hService = NULL;
  x = ERROR_SUCCESS;

  hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
  if (!hSCM) {
    x = GetLastError();
    ShowError("Unable to open SCManager (error #%d).", (unsigned int) x);
    goto end;
  }

  hService = CreateService(hSCM, HYPERDBG_SERVICE_NAME, "HyperDbg",
			   SERVICE_START | DELETE | SERVICE_STOP, SERVICE_KERNEL_DRIVER,
			   SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, driver_filename,
			   NULL, NULL, NULL, NULL, NULL);

  if (!hService) {
    x = GetLastError();
    ShowError("Unable to create driver service (error #%d).", (unsigned int) x);
    goto end;
  }

  if (!StartService(hService, 0, NULL)) {
    SERVICE_STATUS ss;

    x = GetLastError();

    ShowError("Unable to start driver service (error #%d).", (unsigned int) x);

    // Cleanup
    hService = OpenService(hSCM, HYPERDBG_SERVICE_NAME, SERVICE_START | DELETE | SERVICE_STOP);
    ControlService(hService, SERVICE_CONTROL_STOP, &ss);
    DeleteService(hService);
    goto end;
  }

 end:
  CloseServiceHandle(hService);
  CloseServiceHandle(hSCM);
  return x;
}

DWORD LoaderRemoveDriver()
{
  DWORD x;
  SC_HANDLE hSCM, hService;
  SERVICE_STATUS ss;

  hSCM = hService = NULL;
  x = ERROR_SUCCESS;

  hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
  if (!hSCM) {
    x = GetLastError();
    ShowError("Unable to open SCManager (error #%d).", (unsigned int) x);
    goto end;
  }

  hService = OpenService(hSCM, HYPERDBG_SERVICE_NAME, SERVICE_START | DELETE | SERVICE_STOP);
  ControlService(hService, SERVICE_CONTROL_STOP, &ss);
  DeleteService(hService);

 end:
  CloseServiceHandle(hService);
  CloseServiceHandle(hSCM);
  return x;  
}

BOOL LoaderInit()
{
  BOOL b;

  /* Just in the case it was already installed.. */
  LoaderRemoveDriver();

  /* The 'driver_filename' global variable will contain the name of the driver
     (.sys file) in Windows system directory */
  GetSystemDirectory(driver_filename, sizeof(driver_filename) - sizeof(HYPERDBG_DRIVER_FILENAME));
  strncat(driver_filename, "\\", sizeof(driver_filename));
  strncat(driver_filename, HYPERDBG_DRIVER_FILENAME, sizeof(driver_filename));

  
  /* Copy the driver to Windows system directory (overwrite if exists) */
  b = CopyFile(HYPERDBG_DRIVER_FILENAME, driver_filename, FALSE);
  if (!b) {
    GetSystemDirectory(driver_filename, sizeof(driver_filename));
    ShowError("Unable to copy driver file. "
	      "Check that '%s' exists in the current directory and that you have write access to Windows system directory "
	      "('%s')\n", HYPERDBG_DRIVER_FILENAME, driver_filename);
    return FALSE;
  }

  return TRUE;
}

BOOL LoaderFini()
{
  if(!driver_filename[0]) {
    /* 'driver_filename' was not initialized. This happens if we invoked
       HyperGUI with the '/r' command-line switch */
    GetSystemDirectory(driver_filename, sizeof(driver_filename) - sizeof(HYPERDBG_DRIVER_FILENAME));
    strncat(driver_filename, "\\", sizeof(driver_filename));
    strncat(driver_filename, HYPERDBG_DRIVER_FILENAME, sizeof(driver_filename));
  }

  DeleteFile(driver_filename);
  return TRUE;
}
