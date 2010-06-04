@echo off
set CUSTOM_PLUGIN_CFLAGS=
set CUSTOM_CORE_CFLAGS=
set CUSTOM_CORE_INCLUDES=
set CUSTOM_CORE_LIBS=

set CUSTOM_PLUGIN_CFLAGS=/DHVM_ARCH_BITS=32 /DGUEST_WINDOWS=1
set CUSTOM_CORE_CFLAGS=/DHVM_ARCH_BITS=32 /DGUEST_WINDOWS=1

if "%1"=="" goto NoTarget

if "%1"=="hyperdbg"      goto hyperdbg
if "%1"=="hyperdbg-pae"  goto hyperdbg-pae

echo [!] Unknown module: %1
echo     Available modules: hyperdbg hyperdbg-pae naked
goto End

REM The "CUSTOM_CORE_CFLAGS" variable is included into USER_C_FLAGS by core/sources
REM The "CUSTOM_PLUGIN_CFLAGS" variable is included into USER_C_FLAGS by <plugin>/sources

:hyperdbg-pae
set CUSTOM_PLUGIN_CFLAGS=/DENABLE_PAE=1 %CUSTOM_PLUGIN_CFLAGS%
set CUSTOM_CORE_CFLAGS=/DENABLE_PAE=1 %CUSTOM_CORE_CFLAGS%

:hyperdbg

if "%2"=="" goto ErrorParameter
if "%3"=="" goto ErrorParameter

set CUSTOM_PLUGIN_CFLAGS=/DVIDEO_DEFAULT_RESOLUTION_X=%2 %CUSTOM_PLUGIN_CFLAGS%
set CUSTOM_PLUGIN_CFLAGS=/DVIDEO_DEFAULT_RESOLUTION_Y=%3 %CUSTOM_PLUGIN_CFLAGS%

set CUSTOM_CORE_CFLAGS=/DENABLE_HYPERDBG=1 %CUSTOM_CORE_CFLAGS%
set CUSTOM_CORE_INCLUDES=..\hyperdbg
set CUSTOM_CORE_LIBS=..\lib\*\hyperdbg.lib ..\lib\*\libudis86.lib
goto Compile

:naked
REM "naked" module, i.e., without any plugin
build /Dgw
goto End

:Compile
build /Dgw %1
goto End

:NoTarget
echo [!] You must specify a module name
goto End

:ErrorParameter
echo [!] You must specify a screen resolution
goto End

:End

