;; @file
;
; testcase helpers.
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

%include "VBox/nasm.mac"
%include "VBox/cpum.mac"

BEGINCODE

;;
; Saves the current CPU context.
;
; @uses     none
; @param    pCtx on stack. (Caller cleans up, as always.)
BEGINPROC TSTSaveCtx
    push    edx
    mov     edx, [esp + 8]              ; argument.
    mov     [edx + CPUMCTX.eax], eax
    mov     [edx + CPUMCTX.ebx], ebx
    mov     [edx + CPUMCTX.ecx], ecx
    pop     dword [edx + CPUMCTX.edx]
    mov     [edx + CPUMCTX.edi], edi
    mov     [edx + CPUMCTX.esi], esi
    mov     [edx + CPUMCTX.ebp], ebp
    mov     [edx + CPUMCTX.esp], esp
    ;mov     eax, [esp] ....not this one...
    ;mov     [edx + CPUMCTX.eip], eax
    xor     eax, eax
    mov     eax, ss
    mov     [edx + CPUMCTX.ss], eax
    mov     eax, cs
    mov     [edx + CPUMCTX.cs], eax
    mov     eax, ds
    mov     [edx + CPUMCTX.ds], eax
    mov     eax, es
    mov     [edx + CPUMCTX.es], eax
    mov     eax, fs
    mov     [edx + CPUMCTX.fs], eax
    mov     eax, gs
    mov     [edx + CPUMCTX.gs], eax
    pushfd
    pop     eax
    mov     [edx + CPUMCTX.eflags], eax
    fxsave  [edx + CPUMCTX.fpu]
    mov     eax, [edx + CPUMCTX.eax]
    mov     edx, [edx + CPUMCTX.edx]
    ret
ENDPROC   TSTSaveCtx


;;
; Compares two context structures.
; @returns  eax == 0 if equal.
; @returns  eax != 0 if different.
; @param    pCtx1 on stack.
; @param    pCtx2 on stack.
; @uses     nothing but eax and flags.
;
BEGINPROC TSTCompareCtx
    push    esi
    push    edi
    push    ecx

    mov     esi, [esp + 10h]            ; pCtx1
    mov     edi, [esp + 14h]            ; pCtx2
    mov     ecx, CPUMCTX_size >> 2
    repz cmpsd
    jz      return

    shl     ecx, 2
    mov     eax, CPUMCTX_size + 1
    sub     eax, ecx                    ; field offset + 1 byte.

    pop     ecx
    pop     edi
    pop     esi
    ret

return:
    xor     eax, eax
    pop     ecx
    pop     edi
    pop     esi
    ret
ENDPROC   TSTCompareCtx

