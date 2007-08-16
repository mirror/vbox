;; @file
; VBoxGuest - VirtualBox Guest Additions Interface, MASM/ALP header.
;

;
;  Copyright (C) 2006-2007 innotek GmbH
; 
;  innotek GmbH confidential
;  All rights reserved


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

