;; @file
; InnoTek Portable Runtime - ASMAtomicXchgU8().
;

;
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

;;
; @returns al Current *pu8 value
; @param   rcx  pu8    Pointer to the 8-bit variable to update.
; @param   dl   u8     The 8-bit value to assign to *pu8.
BEGINPROC_EXPORTED ASMAtomicXchgU8
        xchg    [rcx], dl
        movzx   eax, dl
        ret
ENDPROC ASMAtomicXchgU8

