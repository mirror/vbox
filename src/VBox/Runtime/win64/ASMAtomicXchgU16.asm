;; @file
; innotek Portable Runtime - ASMAtomicXchgU16().
;

;
;  Copyright (C) 2006-2007 innotek GmbH
; 
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License as published by the Free Software Foundation,
;  in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
;  distribution. VirtualBox OSE is distributed in the hope that it will
;  be useful, but WITHOUT ANY WARRANTY of any kind.


;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "iprt/asmdefs.mac"

BEGINCODE

;;
; @returns rax  Current *pu16 value
; @param   rcx  pu16    Pointer to the 16-bit variable to update.
; @param   rdx  u16     The 16-bit value to assign to *pu16.
BEGINPROC_EXPORTED ASMAtomicXchgU16
        xchg    [rcx], dx
        movzx   eax, dx
        ret
ENDPROC ASMAtomicXchgU16

