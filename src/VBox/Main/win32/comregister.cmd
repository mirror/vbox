@echo off

REM /*
REM  * Script to register the VirtualBox COM classes
REM  * (both inproc and out-of-process)
REM  */

REM /*
REM  Copyright (C) 2006-2007 innotek GmbH
REM 
REM  This file is part of VirtualBox Open Source Edition (OSE), as
REM  available from http://www.virtualbox.org. This file is free software;
REM  you can redistribute it and/or modify it under the terms of the GNU
REM  General Public License (GPL) as published by the Free Software
REM  Foundation, in version 2 as it comes in the "COPYING" file of the
REM  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
REM  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
REM 
REM  */


VBoxSVC.exe /ReregServer

regsvr32 /s /u VBoxC.dll
regsvr32 /s VBoxC.dll
