; $Id$
;; @file
; innotek Portable Runtime - No-CRT ldexpl - AMD64 & X86.
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
; Computes lrd * 2^exp
; @returns st(0)
; @param    lrd     [rbp + _S*2]
; @param    exp     [ebp + 14h]  GCC:edi  MSC:ecx
BEGINPROC RT_NOCRT(ldexpl)
    push    _BP
    mov     _BP, _SP
    sub     _SP, 10h

    ; load exp
%ifdef RT_ARCH_AMD64 ; ASSUMES ONLY GCC HERE!
    mov     [rsp], edi
    fild    dword [rsp]
%else
    fild    dword [ebp + _S*2 + RTLRD_CB]
%endif
    fld     tword [_BP + _S*2]
    fscale
    fstp    st1

    leave
    ret
ENDPROC   RT_NOCRT(ldexpl)

