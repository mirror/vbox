;; @file
;
; tstProg-1a assembly.

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
extern NAME(TSTCompareCtx)
extern NAME(TSTSaveCtx)


BEGINPROC tstProg1ForeverAsm
    mov     edx, [esp + 4]              ; pCtx1
    mov     ecx, [esp + 8]              ; pCtx2
    push    0                           ; counter

    ; a little marker for debugging.
    mov     eax, ds
    mov     gs, eax
    mov     fs, eax

    ;
    ; Get initial context.
    ; The state must be 100% correct here, which means
    ; we assume both ctx structs are initially equal and do compare them
    ; like we would in the loop.
    ;
    push    ecx
    push    edx
    call    NAME(TSTCompareCtx)
    lea     esp, [esp + 8]
    or      eax, eax
    jz      ok
    int03
ok:
    ; now we actually get the first state.
    push    edx
    call    NAME(TSTSaveCtx)
    lea     esp, [esp + 4]

    ;
    ; Forever.
    ;
daloop:
    xchg    ebx, [esp]
    lea     ebx, [ebx + 1]
    xchg    ebx, [esp]
    ; Save context.
    push    ecx
    call    NAME(TSTSaveCtx)
    lea     esp, [esp + 4]

    ; Compare with original context.
    push    ecx
    push    edx
    call    NAME(TSTCompareCtx)
    lea     esp, [esp + 8]
    or      eax, eax
    jz      daloop

    ; not equal!
    pop     edx
    shl     edx, 16
    or      eax, edx
    pop     ebx
    ret
ENDPROC   tstProg1ForeverAsm



