;; @file
;
; TRPM - Host Context Ring-0

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

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "VBox/nasm.mac"


BEGINCODE

    align 16
;;
; This is the alternative return from VMMR0Entry() used when
; we need to dispatch an interrupt to the Host (we received it in GC).
;
; As seen in TRPMR0SetupInterruptDispatcherFrame() the stack is different
; than for the normal VMMR0Entry() return.
;
; 32-bit:
;           18  iret frame
;           14  retf selector (interrupt handler)
;           10  retf offset   (interrupt handler)
;            c  es
;            8  fs
;            4  ds
;            0  pVM (esp here)
;
; 64-bit:
;           24  iret frame
;           18  retf selector (interrupt handler)
;           10  retf offset   (interrupt handler)
;            8  uOperation
;            0  pVM
;
BEGINPROC trpmR0InterruptDispatcher
%ifdef __AMD64__
    lea     rsp, [rsp + 10h]            ; skip pVM and uOperation
    db 48h
    retf
%else  ; !__AMD64__
    add     esp, byte 4                 ; skip pVM
    pop     ds
    pop     fs
    pop     es
    retf
%endif ; !__AMD64__
ENDPROC   trpmR0InterruptDispatcher

