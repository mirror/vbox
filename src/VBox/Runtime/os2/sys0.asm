;; @file
; innotek Portable Runtime - crt0/dll0 replacement stub for OS/2 R0/GC modules.
;

;
; Copyright (C) 2006-2007 innotek GmbH
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License as published by the Free Software Foundation,
; in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
; distribution. VirtualBox OSE is distributed in the hope that it will
; be useful, but WITHOUT ANY WARRANTY of any kind.
;
; If you received this file as part of a commercial VirtualBox
; distribution, then only the terms of your commercial VirtualBox
; license agreement apply instead of the previous paragraph.
;


;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "iprt/asmdefs.mac"


BEGINCODE

global __text
__text:
global ___ehInit
___ehInit:
    db 0cch

..start:
    int3
    xor eax,eax
    ret

BEGINDATA
global __data
__data:
global __data_start
__data_start:

BEGINBSS
global __bss_start
__bss_start:


