;; @file
; innotek Portable Runtime - ASMGetIDTR().
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
; Gets the content of the IDTR CPU register.
; @param   rcx  pIdtr   Where to store the IDTR contents.
;
BEGINPROC_EXPORTED ASMGetIDTR
        sidt    [rcx]
        ret
ENDPROC ASMGetIDTR


