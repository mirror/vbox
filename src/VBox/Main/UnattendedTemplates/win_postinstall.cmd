@echo off
rem $Id$
rem rem @file
rem Post installation script template for Windows.
rem
rem This runs after the target system has been booted, typically as
rem part of the first logon.
rem

rem
rem Copyright (C) 2017 Oracle Corporation
rem
rem This file is part of VirtualBox Open Source Edition (OSE), as
rem available from http://www.virtualbox.org. This file is free software;
rem you can redistribute it and/or modify it under the terms of the GNU
rem General Public License (GPL) as published by the Free Software
rem Foundation, in version 2 as it comes in the "COPYING" file of the
rem VirtualBox OSE distribution. VirtualBox OSE is distributed in the
rem hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
rem


echo TODO

@@VBOX_COND_IS_INSTALLING_ADDITIONS@@
e:\cert\VBoxCertUtil.exe add-trusted-publisher e:\cert\vbox*.cer --root e:\cert\vbox*.cer
e:\VBoxWindowsAdditions.exe /S
@@VBOX_COND_END@@

