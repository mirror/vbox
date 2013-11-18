@echo off
REM $Id$
REM
REM Script to register the VirtualBox COM classes
REM (both inproc and out-of-process)
REM

REM
REM Copyright (C) 2006-2013 Oracle Corporation
REM
REM This file is part of VirtualBox Open Source Edition (OSE), as
REM available from http://www.virtualbox.org. This file is free software;
REM you can redistribute it and/or modify it under the terms of the GNU
REM General Public License (GPL) as published by the Free Software
REM Foundation, in version 2 as it comes in the "COPYING" file of the
REM VirtualBox OSE distribution. VirtualBox OSE is distributed in the
REM hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
REM

setlocal

REM
REM Figure out where the script lives first, so that we can invoke the
REM correct VBoxSVC and register the right VBoxC.dll.
REM

REM Determine the current directory.
set _SCRIPT_CURDIR=%CD%
for /f "tokens=*" %%d in ('cd') do set _SCRIPT_CURDIR=%%d

REM Determine a correct self - by %0.
set _SCRIPT_SELF=%0
if exist "%_SCRIPT_SELF%" goto found_self
set _SCRIPT_SELF=%_SCRIPT_SELF%.cmd
if exist "%_SCRIPT_SELF%" goto found_self

REM Determine a correct self - by current working directory.
set _SCRIPT_SELF=%_SCRIPT_CURDIR%\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self

REM Determine a correct self - by the PATH
REM This is very verbose because nested for loops didn't work out.
for /f "tokens=1  delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=2  delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=3  delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=4  delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=5  delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=6  delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=7  delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=8  delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=9  delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=10 delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=11 delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=12 delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=13 delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=14 delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=15 delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=16 delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=17 delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=18 delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=19 delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
for /f "tokens=20 delims=;" %%d in ("%PATH%") do set _SCRIPT_SELF=%%d\comregister.cmd
if exist "%_SCRIPT_SELF%" goto found_self
echo Warning: Not able to determin the comregister.cmd location.
set _VBOX_DIR=
goto register

:found_self
set _VBOX_DIR=
cd "%_SCRIPT_SELF%\.."
for /f "tokens=*" %%d in ('cd') do set _VBOX_DIR=%%d\
cd "%_SCRIPT_CURDIR%"


REM
REM Do the registrations.
REM
if "%ProgramW6432%x" == "x" goto register_x86
goto register_amd64

:register_x86
@echo on
%_VBOX_DIR%VBoxSVC.exe /UnregServer
regsvr32 /s /u %_VBOX_DIR%VBoxC.dll
@if "%1" == "-u" goto end
%_VBOX_DIR%VBoxSVC.exe /RegServer
regsvr32 /s    %_VBOX_DIR%VBoxC.dll
@echo off
goto end

REM Unregister both first, then register them. The order matters here.
:register_amd64
@echo on
%_VBOX_DIR%VBoxSVC.exe /UnregServer
%windir%\syswow64\regsvr32 /s /u %_VBOX_DIR%x86\VBoxClient-x86.dll
%windir%\system32\regsvr32 /s /u %_VBOX_DIR%VBoxC.dll
@if "%1" == "-u" goto end
%_VBOX_DIR%VBoxSVC.exe /RegServer
%windir%\system32\regsvr32 /s    %_VBOX_DIR%VBoxC.dll
%windir%\syswow64\regsvr32 /s    %_VBOX_DIR%x86\VBoxClient-x86.dll
@echo off

:end
@endlocal

