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

/* xpvideo.c
 *
 * Routines for detecting resolution and video adapter framebuffer address in Window XP.
 * some of this code was inspired by the work of Mysterie:
 *   "http://mysterie.fr/blog/index.php?post/2009/07/22/..."
 *
 * Jon Larimer <jlarimer@gmail.com>
 *
 */

/* Only build if /DXPVIDEO is specified */
#ifdef XPVIDEO

#include <ntddk.h>
#include <ntdef.h>
#include <Ntddvdeo.h>

#include "types.h"
#include "debug.h"

/* Undocumented stuff not included in headers */
extern POBJECT_TYPE IoDriverObjectType; 
extern NTKERNELAPI NTSTATUS ObReferenceObjectByName(  
  IN PUNICODE_STRING ObjectPath,  
  IN ULONG Attributes,
  IN PACCESS_STATE PassedAccessState OPTIONAL,  
  IN ACCESS_MASK DesiredAccess OPTIONAL,
  IN POBJECT_TYPE ObjectType OPTIONAL, 
  IN KPROCESSOR_MODE AccessMode,  
  IN OUT PVOID ParseContext OPTIONAL,  
  OUT PVOID *ObjectPtr  
  );

NTSTATUS XpVideoFindDisplayMiniportDriverName(PUNICODE_STRING name);
NTSTATUS XpVideoGetDeviceObject(PUNICODE_STRING driverName, PDEVICE_OBJECT *deviceObject);
NTSTATUS XpVideoGetVideoMemoryAddress(PDEVICE_OBJECT device, PHYSICAL_ADDRESS *vidMemPhys, ULONG *size);
NTSTATUS XpVideoGetVideoModeInformation(PDEVICE_OBJECT device, VIDEO_MODE_INFORMATION *vidModeinfo); /* FIXME: Unused */
NTSTATUS XpVideoDoDeviceIoControl(PDEVICE_OBJECT device, ULONG ioctl, PVOID input, ULONG inputlen, PVOID output, ULONG outputlen);
ULONG    XpVideoGetRealStride(ULONG width, ULONG stride);
int      XpVideoIsDriverVgaSave(PUNICODE_STRING driverName);

hvm_status XpVideoGetWindowsXPDisplayData(hvm_address *addr, Bit32u *framebuffer_size, Bit32u *width, Bit32u *height, Bit32u *stride) {
  UNICODE_STRING driverName;
  PVOID stringBuf;
  NTSTATUS status;
  VIDEO_MODE_INFORMATION vidModeInfo;
  PDEVICE_OBJECT deviceObject;
  PHYSICAL_ADDRESS physAddr;
  ULONG length;
  int is_vgasave;

  /* Allocate some space for the name of our driver. this is a bit overkill... */
  stringBuf = ExAllocatePoolWithTag(NonPagedPool, 1024, 'lnoj');
  if(!stringBuf) {
    WindowsLog("[xpvideo] can't allocate memory!");
    return HVM_STATUS_UNSUCCESSFUL;
  }

  RtlInitEmptyUnicodeString(&driverName, (PWCHAR)stringBuf, 1024);

  /* Find the name of the display miniport driver */
  status = XpVideoFindDisplayMiniportDriverName(&driverName);
  if(!NT_SUCCESS(status)) {
    ExFreePool(stringBuf);
    return HVM_STATUS_UNSUCCESSFUL;
  }

  /* Is this the default vga driver? */
  is_vgasave = XpVideoIsDriverVgaSave(&driverName);
  if(is_vgasave) {
    WindowsLog("[xpvideo] Driver is standard VGA");
  } else {
    WindowsLog("[xpvideo] Driver is NOT standard VGA");
  }

  /* Now find its device object */
  status = XpVideoGetDeviceObject(&driverName, &deviceObject);
  if(!NT_SUCCESS(status)) {
    ExFreePool(stringBuf);
    return HVM_STATUS_UNSUCCESSFUL;
  }
  
  /* Don't need this anymore! */
  ExFreePool(stringBuf);

  /* Get the physical address of the framebuffer */
  status = XpVideoGetVideoMemoryAddress(deviceObject, &physAddr, framebuffer_size);
  if(!NT_SUCCESS(status)) {
    return HVM_STATUS_UNSUCCESSFUL;
  }

  /* FIXME: 64 bits! */
  *addr = physAddr.LowPart;

  /* Get information on the current mode */
  status = XpVideoDoDeviceIoControl(
    deviceObject,
    IOCTL_VIDEO_QUERY_CURRENT_MODE,
    NULL,
    0,
    &vidModeInfo,
    sizeof(VIDEO_MODE_INFORMATION));

  if(!NT_SUCCESS(status)) {
    return HVM_STATUS_UNSUCCESSFUL;
  }

  /* Set these values */
  *height = vidModeInfo.VisScreenHeight;
  *width = vidModeInfo.VisScreenWidth;

  /* The stride is wrong in many cases. this is a ridiculous hack that tries to find 
     the real stride, at least on my computers. It won't work for everyone. */
  if(is_vgasave) {
    *stride = *width;
  } else {
    *stride = XpVideoGetRealStride(*width, vidModeInfo.ScreenStride / (vidModeInfo.BitsPerPlane / 8));
  }

  WindowsLog("[xpvideo] using resolution %d x %d, stride %d", *width, *height, *stride);

  return HVM_STATUS_SUCCESS;
}

/* Locate the name of the display miniport driver so we can look up a pointer to
 * it's deviceobject. We locate it by looking in \Registry\Machine\Hardware\DeviceMap\Video
 * for a value named \Device\Video0. The value is a string pointing to another key. 
 * We chop the \0000 off the end of the key, add \Video, then look for the Service value
 * on that key. That has the name of the video driver service. from there, we know that
 * we can locate the driver with the name \driver\xxx.
 */
NTSTATUS XpVideoFindDisplayMiniportDriverName(PUNICODE_STRING name) {
  OBJECT_ATTRIBUTES keyObject;
  UNICODE_STRING keyName;
  UNICODE_STRING keyValueName;
  HANDLE keyHandle;
  UCHAR *valueBuffer;

  NTSTATUS status;
  ULONG resultLen;
  KEY_VALUE_FULL_INFORMATION *valueInfo;
  UNICODE_STRING valueString;
  USHORT len;

  /* Open registry key for Video driver device map */
  RtlInitUnicodeString(&keyName, L"\\Registry\\Machine\\Hardware\\DeviceMap\\Video");
  InitializeObjectAttributes(&keyObject, &keyName, OBJ_CASE_INSENSITIVE, NULL, NULL);
  status = ZwOpenKey(&keyHandle, KEY_QUERY_VALUE, &keyObject);
  if(!NT_SUCCESS(status)) {
    WindowsLog("[xpvideo] can't open key %ws", keyName.Buffer);
    return status;
  }

  valueBuffer = (UCHAR *)ExAllocatePoolWithTag(NonPagedPool, 4096, 'lnoj');
  if(!valueBuffer) {
    WindowsLog("[xpvideo] can't allocate registry value buffer");
    ZwClose(keyHandle);
    return STATUS_UNSUCCESSFUL;
  }

  RtlInitUnicodeString(&keyValueName, L"\\Device\\Video0");
  status = ZwQueryValueKey(keyHandle, &keyValueName, KeyValueFullInformation, valueBuffer, 4096, &resultLen);
  if(!NT_SUCCESS(status)) {
    WindowsLog("[xpvideo] cant query value %ws", keyValueName.Buffer);
    ExFreePool(valueBuffer);
    ZwClose(keyHandle);
    return status;
  }

  /* Don't need this anymore */
  ZwClose(keyHandle);

  /* Get offset to the string data from the registry value */
  valueInfo = (KEY_VALUE_FULL_INFORMATION *)valueBuffer;
  RtlInitUnicodeString(&valueString, (PCWSTR)(valueBuffer + valueInfo->DataOffset));

  /* We can use the rest of our pool for the string */
  valueString.MaximumLength = (USHORT)(4096 - (valueInfo->DataOffset + valueInfo->DataLength));

  /* Erase the end of the string, back to the last '\\' */
  len = valueString.Length - 1;
  while(len > 0 && valueString.Buffer[len/2] != '\\')
    len-=2;

  /* Append '\\video' to the string */
  valueString.Length = len;
  RtlAppendUnicodeToString(&valueString, L"\\Video");

  WindowsLog("[xpvideo] video key value is %ws", valueString.Buffer);

  /* Now open that key */
  RtlZeroMemory(&keyObject, sizeof(keyObject));
  RtlInitUnicodeString(&keyName, valueString.Buffer);
  InitializeObjectAttributes(&keyObject, &keyName, OBJ_CASE_INSENSITIVE, NULL, NULL);
  status = ZwOpenKey(&keyHandle, KEY_QUERY_VALUE, &keyObject);
  if(!NT_SUCCESS(status)) {
    WindowsLog("[xpvideo] can't open key %ws", keyName.Buffer);
    ExFreePool(valueBuffer);
    return status;
  }

  RtlInitUnicodeString(&keyValueName, L"Service");
  status = ZwQueryValueKey(keyHandle, &keyValueName, KeyValueFullInformation, valueBuffer, 4096, &resultLen);
  if(!NT_SUCCESS(status)) {
    WindowsLog("[xpvideo] cant query value %ws", keyValueName.Buffer);
    ExFreePool(valueBuffer);
    ZwClose(keyHandle);
    return status;
  }

  valueInfo = (KEY_VALUE_FULL_INFORMATION *)valueBuffer;
  RtlInitUnicodeString(&valueString, (PCWSTR)(valueBuffer + valueInfo->DataOffset));

  WindowsLog("[xpvideo] \\Device\\Video0 service name is '%ws'", valueString.Buffer);

  RtlAppendUnicodeToString(name, L"\\Driver\\");
  RtlAppendUnicodeToString(name, valueString.Buffer);

  WindowsLog("[xpvideo] full driver path is %ws", name->Buffer);

  ExFreePool(valueBuffer);
  ZwClose(keyHandle);
  return STATUS_SUCCESS;
}

/* Do an DeviceIoControl call against a device object. This builds an IRP then calls IoCallDriver. */
NTSTATUS XpVideoDoDeviceIoControl(PDEVICE_OBJECT device, ULONG ioctl, PVOID input, ULONG inputlen, PVOID output, ULONG outputlen) {
  PIRP irp;
  KEVENT event;
  IO_STATUS_BLOCK iostatus;
  NTSTATUS status;

  RtlZeroMemory(output, outputlen);
  KeInitializeEvent(&event, NotificationEvent, FALSE);

  /* Build our IRP */
  irp = IoBuildDeviceIoControlRequest(
    ioctl,
    device,
    input,
    inputlen,
    output,
    outputlen,
    FALSE,
    &event,
    &iostatus);

  if (irp == NULL) {
    WindowsLog("[xpvideo] can't create IRP!");
    return STATUS_INSUFFICIENT_RESOURCES;
  }

  status = IoCallDriver(device, irp);

  if (status == STATUS_PENDING) {
    KeWaitForSingleObject(
      &event,
      Executive,
      KernelMode,
      FALSE,
      NULL);
    status = iostatus.Status;
  }

  if(!NT_SUCCESS(status)) {
    WindowsLog("[xpvideo] DeviceIoControl call failed");
  }

  return status;
}

NTSTATUS XpVideoGetVideoMemoryAddress(PDEVICE_OBJECT device, PHYSICAL_ADDRESS *vidMemPhys, ULONG *size) {
  VIDEO_MEMORY vidMem;
  VIDEO_MEMORY_INFORMATION vidMemInfo;
  PIRP irp;
  KEVENT event;
  IO_STATUS_BLOCK iostatus;
  NTSTATUS status;
  NTSTATUS orig_status;

  /* Send a IOCTL_VIDEO_MAP_MEMORY request to the video miniport driver */
  status = XpVideoDoDeviceIoControl(
    device,
    IOCTL_VIDEO_MAP_VIDEO_MEMORY,
    &vidMem,
    sizeof(VIDEO_MEMORY),
    &vidMemInfo,
    sizeof(VIDEO_MEMORY_INFORMATION));

  /* Get the phyisical address. The reason we can't keep the mapped address the driver gives us is
   * because it's an invalid length sometimes - I've seen it shorter than a single scan line, and any
   * writes past that will PF. What does work is getting the physical address and then calling MmMapIoSpace
   * ourselves to map the memory. 
   */
  if(NT_SUCCESS(status)) {
    *vidMemPhys = MmGetPhysicalAddress(vidMemInfo.FrameBufferBase);
    *size = vidMemInfo.FrameBufferLength;
    WindowsLog("[xpvideo] Framebuffer physical address 0x%.8x, size %d", vidMemPhys, size);
  }

  /* Now ask driver to unmap - we don't need it anymore */
  vidMem.RequestedVirtualAddress = vidMemInfo.FrameBufferBase;
  XpVideoDoDeviceIoControl(
    device,
    IOCTL_VIDEO_UNMAP_VIDEO_MEMORY,
    &vidMem,
    sizeof(VIDEO_MEMORY),
    NULL,
    0);

  return status;
}

NTSTATUS XpVideoGetDeviceObject(PUNICODE_STRING driverName, PDEVICE_OBJECT *deviceObject) {
  NTSTATUS status;
  PDRIVER_OBJECT driverObject;
  PDEVICE_OBJECT tmpDeviceObject;

  /* Open driver by reference to it's name. This is undocumented! */
  status = ObReferenceObjectByName(driverName, OBJ_CASE_INSENSITIVE, NULL, 0, IoDriverObjectType, KernelMode, NULL, (PVOID *)&driverObject);
  if(status != STATUS_SUCCESS) {
    WindowsLog("[xpvideo] Can't find object %ws", driverName->Buffer);
    return status;
  }

  /* Locate driver object */
  tmpDeviceObject = driverObject->DeviceObject;
  while(tmpDeviceObject->NextDevice != NULL) tmpDeviceObject = tmpDeviceObject->NextDevice;

  *deviceObject = tmpDeviceObject;
  return status;
}

ULONG XpVideoGetRealStride(ULONG width, ULONG stride) {
  /* If they're different, assume stride is correct */
  if(width != stride) return stride;

  if(width == 1440) return 2048; /* 1440x900 laptop with intel chipset */
  if(width == 1680) return 1792; /* 1680x1050 desktop lcd with old nvidia chipset */

  return width;
}

/* Return 1 if this driver is the VgaSave driver */
int XpVideoIsDriverVgaSave(PUNICODE_STRING driverName) {
  UNICODE_STRING vgaSave;
  LONG ret;

  RtlInitUnicodeString(&vgaSave, L"\\Driver\\VgaSave");
  ret = RtlCompareUnicodeString(driverName, &vgaSave, TRUE);

  if(ret == 0) {
    return 1;
  } else {
    return 0;
  }
}

#endif
