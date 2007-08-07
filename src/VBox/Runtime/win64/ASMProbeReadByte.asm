;; @file
; innotek Portable Runtime - ASMProbeReadByte().
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
; Probes a byte pointer for read access.
;
; While the function will not fault if the byte is not read accessible,
; the idea is to do this in a safe place like before acquiring locks
; and such like.
;
; Also, this functions guarantees that an eager compiler is not going
; to optimize the probing away.
;
; @param   rcx  pvByte      Pointer to the byte.
BEGINPROC_EXPORTED ASMProbeReadByte
        mov     al, [rcx]
        ret
ENDPROC ASMProbeReadByte

