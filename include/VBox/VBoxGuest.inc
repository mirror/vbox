;; @file
; VBoxGuest - VirtualBox Guest Additions Interface, MASM/ALP header.
;

;/*
;  Copyright (C) 2006-2007 innotek GmbH
; 
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License as published by the Free Software Foundation,
;  in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
;  distribution. VirtualBox OSE is distributed in the hope that it will
;  be useful, but WITHOUT ANY WARRANTY of any kind.
; */


VMMDEV_VERSION_MAJOR    EQU     1
VMMDEV_VERSION_MINOR    EQU     4
VMMDEV_VERSION          EQU     000010004h

;; ASSUMES OS/2 for now.
VBOXGUEST_DEVICE_NAME   EQU     "VBoxGst$"


;; aka VBOXGUESTOS2IDCCONNECT 
VBGOS2IDC STRUC
u32Version         DD ?
u32Session         DD ?
pfnServiceEP       DD ?
fpfnServiceEP      DD ?
fpfnServiceAsmEP   DD ?
VBGOS2IDC ENDS    

