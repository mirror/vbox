;; @file
;
; tstProg-2a assembly.

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

%include "VBox/nasm.mac"

BEGINCODE


BEGINPROC tstProg2ForeverAsm
    mov     edx, [esp + 4]              ; pEflags1
    mov     ecx, [esp + 8]              ; pEflags2
    push    ebx

    ; a little marker for debugging.
    mov     eax, ds
    mov     gs, eax
    mov     fs, eax

    ; save eflags
    xor     eax, eax
    cmp     eax, eax
    pushfd
    pop     dword [edx]

    ;
    ; Forever.
    ;
daloop:
    lea     eax, [eax + 1]
    pushfd
    pop     dword [ecx]
    mov     ebx, [edx]
    cmp     [ecx], ebx
    jz      daloop

    ; not equal!
    int03
    pop     ebx
    ret
ENDPROC   tstProg1ForeverAsm



