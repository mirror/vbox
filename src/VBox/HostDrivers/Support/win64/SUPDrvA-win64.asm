;
; VBox host drivers - Ring-0 support drivers - Win64 host:
; Assembly part of Win64 ring-0 host driver

; Copyright (C) 2006 InnoTek Systemberatung GmbH
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
extern DbgPrint

;;
; Kind of alias for DbgPrint
BEGINPROC AssertMsg2
        jmp     DbgPrint
ENDPROC AssertMsg2

;;
; Kind of alias for DbgPrint
BEGINPROC SUPR0Printf
        jmp     DbgPrint
ENDPROC SUPR0Printf


