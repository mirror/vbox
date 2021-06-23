@echo off
REM $Id$
REM REM @file
REM VirtualBox Validation Kit - testbox script, automatic execution wrapper.
REM

REM
REM Copyright (C) 2006-2020 Oracle Corporation
REM
REM This file is part of VirtualBox Open Source Edition (OSE), as
REM available from http://www.virtualbox.org. This file is free software;
REM you can redistribute it and/or modify it under the terms of the GNU
REM General Public License (GPL) as published by the Free Software
REM Foundation, in version 2 as it comes in the "COPYING" file of the
REM VirtualBox OSE distribution. VirtualBox OSE is distributed in the
REM hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
REM
REM The contents of this file may alternatively be used under the terms
REM of the Common Development and Distribution License Version 1.0
REM (CDDL) only, as it comes in the "COPYING.CDDL" file of the
REM VirtualBox OSE distribution, in which case the provisions of the
REM CDDL are applicable instead of those of the GPL.
REM
REM You may elect to license modified versions of this file under the
REM terms and conditions of either the GPL or the CDDL or both.
REM

@echo "$Id$"
@echo on
setlocal EnableExtensions
set exe=python.exe
for /f %%x in ('tasklist /NH /FI "IMAGENAME eq %exe%"') do if %%x == %exe% goto end

if exist %SystemRoot%\System32\aim_ll.exe (
	set RAMEXE=aim
) else if exist %SystemRoot%\System32\imdisk.exe (
    set RAMEXE=imdisk
) else goto defaulttest	

REM Take presence of imdisk.exe or aim_ll.exe as order to test in ramdisk.
set RAMDRIVE=D:
if exist %RAMDRIVE%\TEMP goto skip
if %RAMEXE% == aim (
	aim_ll -a -t vm -s 16G -m %RAMDRIVE% -p "/fs:ntfs /q /y"
) else if %RAMEXE% == aim (
	imdisk -a -s 16GB -m %RAMDRIVE% -p "/fs:ntfs /q /y" -o "awe"
) else goto defaulttest
:skip

set VBOX_INSTALL_PATH=%RAMDRIVE%\VBoxInstall
set TMP=%RAMDRIVE%\TEMP
set TEMP=%TMP%

mkdir %VBOX_INSTALL_PATH%
mkdir %TMP%

set TESTBOXSCRIPT_OPTS=--scratch-root=%RAMDRIVE%\testbox

:defaulttest
%SystemDrive%\Python27\python.exe %SystemDrive%\testboxscript\testboxscript\testboxscript.py --testrsrc-server-type=cifs --builds-server-type=cifs %TESTBOXSCRIPT_OPTS%
pause
:end
