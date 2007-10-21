;; @file
; innotek Portable Runtime - ASMAtomicBitClear().
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
; Atomically clears a bit in a bitmap.
;
; @param   rcx  pvBitmap    Pointer to the bitmap.
; @param   edx  iBit        The bit to toggle set.
; @remark  No memory barrier, take care on smp.
;
BEGINPROC_EXPORTED ASMAtomicBitClear
        lock btr [rcx], edx
        ret
ENDPROC ASMAtomicBitClear

