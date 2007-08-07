; $Id$
;; @file
; innotek Portable Runtime - No-CRT cosl - AMD64 & X86.
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

%include "iprt/asmdefs.mac"

BEGINCODE

%ifdef RT_ARCH_AMD64
 %define _SP rsp
 %define _BP rbp
 %define _S  8
%else
 %define _SP esp
 %define _BP ebp
 %define _S  4
%endif

;;
; compute the cosine of ldr, measured in radians.
; @returns st(0)
; @param    lrd     [rbp + _S*2]
BEGINPROC RT_NOCRT(cosl)
    push    _BP
    mov     _BP, _SP
    sub     _SP, 10h

    fld     tword [_BP + _S*2]
    fcos
    fnstsw  ax
    test    ah, 4
    jz      .done

    fldpi
    fadd    st0, st0
    fxch    st1
.again:
    fprem1
    fnstsw  ax
    test    ah, 4
    jnz     .again

    fstp    st0
    fcos

.done:
    leave
    ret
ENDPROC   RT_NOCRT(cosl)

