@echo off

REM /*
REM  * Script to register the VirtualBox COM classes
REM  * (both inproc and out-of-process)
REM  */

REM /*
REM  * Copyright (C) 2006 InnoTek Systemberatung GmbH
REM  *
REM  * This file is part of VirtualBox Open Source Edition (OSE), as
REM  * available from http://www.virtualbox.org. This file is free software;
REM  * you can redistribute it and/or modify it under the terms of the GNU
REM  * General Public License as published by the Free Software Foundation,
REM  * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
REM  * distribution. VirtualBox OSE is distributed in the hope that it will
REM  * be useful, but WITHOUT ANY WARRANTY of any kind.
REM  *
REM  * If you received this file as part of a commercial VirtualBox
REM  * distribution, then only the terms of your commercial VirtualBox
REM  * license agreement apply instead of the previous paragraph.
REM  */


VBoxSVC.exe /ReregServer

regsvr32 /s /u VBoxC.dll
regsvr32 /s VBoxC.dll
