; $Id$
;; @file
; VirtualBox Support Library - Tracer Interface, Assembly bits.
;

;
; Copyright (C) 2012 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;
; The contents of this file may alternatively be used under the terms
; of the Common Development and Distribution License Version 1.0
; (CDDL) only, as it comes in the "COPYING.CDDL" file of the
; VirtualBox OSE distribution, in which case the provisions of the
; CDDL are applicable instead of those of the GPL.
;
; You may elect to license modified versions of this file under the
; terms and conditions of either the GPL or the CDDL or both.
;

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "iprt/asmdefs.mac"


;*******************************************************************************
;*  Structures and Typedefs                                                    *
;*******************************************************************************
struc SUPREQHDR

        .u32Cookie              resd 1
        .u32SessionCookie       resd 1
        .cbIn                   resd 1
        .cbOut                  resd 1
        .fFlags                 resd 1
        .rc                     resd 1
endstruc

struc SUPDRVTRACERUSRCTX32
        .idProbe                resd 1
        .cBits                  resb 1
        .abReserved             resb 3
        .u.X86.uVtgProbeLoc     resd 1
        .u.X86.aArgs            resd 20
        .u.X86.eip              resd 1
        .u.X86.eflags           resd 1
        .u.X86.eax              resd 1
        .u.X86.ecx              resd 1
        .u.X86.edx              resd 1
        .u.X86.ebx              resd 1
        .u.X86.esp              resd 1
        .u.X86.ebp              resd 1
        .u.X86.esi              resd 1
        .u.X86.edi              resd 1
        .u.X86.cs               resw 1
        .u.X86.ss               resw 1
        .u.X86.ds               resw 1
        .u.X86.es               resw 1
        .u.X86.fs               resw 1
        .u.X86.gs               resw 1
endstruc

struc SUPDRVTRACERUSRCTX64
        .idProbe                resd 1
        .cBits                  resb 1
        .abReserved             resb 3
        .u.Amd64.uVtgProbeLoc   resq 1
        .u.Amd64.aArgs          resq 10
        .u.Amd64.rip            resq 1
        .u.Amd64.rflags         resq 1
        .u.Amd64.rax            resq 1
        .u.Amd64.rcx            resq 1
        .u.Amd64.rdx            resq 1
        .u.Amd64.rbx            resq 1
        .u.Amd64.rsp            resq 1
        .u.Amd64.rbp            resq 1
        .u.Amd64.rsi            resq 1
        .u.Amd64.rdi            resq 1
        .u.Amd64.r8             resq 1
        .u.Amd64.r9             resq 1
        .u.Amd64.r10            resq 1
        .u.Amd64.r11            resq 1
        .u.Amd64.r12            resq 1
        .u.Amd64.r13            resq 1
        .u.Amd64.r14            resq 1
        .u.Amd64.r15            resq 1
endstruc

struc SUPTRACERUMODFIREPROBE
        .Hdr                    resb SUPREQHDR_size
        .In                     resb SUPDRVTRACERUSRCTX64_size
endstruc


extern NAME(suplibTracerFireProbe)


BEGINCODE


;;
; Set up a SUPTRACERUMODFIREPROBE request package on the stack and a C helper
; function in SUPLib.cpp to do the rest.
;
EXPORTEDNAME SUPTracerFireProbe
        push    xBP
        mov     xBP, xSP

        ;
        ; Allocate package and set the sizes (the helper does the rest of
        ; the header).  Setting the sizes here allows the helper to verify our
        ; idea of the request sizes.
        ;
        lea     xSP, [xBP - SUPTRACERUMODFIREPROBE_size - 8]

        mov     dword [xSP + SUPTRACERUMODFIREPROBE.Hdr + SUPREQHDR.cbIn],  SUPTRACERUMODFIREPROBE_size
        mov     dword [xSP + SUPTRACERUMODFIREPROBE.Hdr + SUPREQHDR.cbOut], SUPREQHDR_size

%ifdef RT_ARCH_AMD64
        ;
        ; Save the AMD64 context.
        ;
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.rax], rax
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.rcx], rcx
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.rdx], rdx
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.rbx], rbx
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.rsi], rsi
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.rdi], rdi
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.r8 ], r8
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.r9 ], r9
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.r10], r10
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.r11], r11
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.r12], r12
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.r13], r13
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.r14], r14
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.r15], r15
        pushf
        pop     xAX
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.rflags], xAX
        mov     xAX, [xBP + xS]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.rip], xAX
        mov     xAX, [xBP]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.rbp], xAX
        lea     xAX, [xBP + xS*2]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.rsp], xAX
 %ifdef ASM_CALL64_MSC
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.uVtgProbeLoc], rcx
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*0], rdx
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*1], r8
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*2], r9
        mov     xAX, [xBP + xS*2 + 0x20 + xS*0]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*3], xAX
        mov     xAX, [xBP + xS*2 + 0x20 + xS*1]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*4], xAX
        mov     xAX, [xBP + xS*2 + 0x20 + xS*2]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*5], xAX
        mov     xAX, [xBP + xS*2 + 0x20 + xS*3]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*6], xAX
        mov     xAX, [xBP + xS*2 + 0x20 + xS*4]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*7], xAX
        mov     xAX, [xBP + xS*2 + 0x20 + xS*5]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*8], xAX
        mov     xAX, [xBP + xS*2 + 0x20 + xS*6]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*9], xAX
        mov     eax, [xCX + 4]          ; VTGPROBELOC::idProbe.
 %else
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.uVtgProbeLoc], rdi
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*0], rsi
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*1], rdx
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*2], rcx
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*3], r8
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*4], r9
        mov     xAX, [xBP + xS*2 + xS*0]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*5], xAX
        mov     xAX, [xBP + xS*2 + xS*1]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*6], xAX
        mov     xAX, [xBP + xS*2 + xS*2]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*7], xAX
        mov     xAX, [xBP + xS*2 + xS*3]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*8], xAX
        mov     xAX, [xBP + xS*2 + xS*4]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.u.Amd64.aArgs + xS*9], xAX
        mov     eax, [xDI + 4]          ; VTGPROBELOC::idProbe.
 %endif
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.idProbe], eax
        mov     dword [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX64.cBits], 64

        ;
        ; Call the helper.
        ;
 %ifdef ASM_CALL64_MSC
        mov     xDX, xSP
        sub     xSP, 0x20
        call    NAME(suplibTracerFireProbe)
 %else
        mov     xSI, xSP
  %ifdef PIC
        call    [rel NAME(suplibTracerFireProbe) wrt rip]
  %else
        call    NAME(suplibTracerFireProbe)
  %endif
 %endif

%elifdef RT_ARCH_X86
        ;
        ; Save the X86 context.
        ;
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX32.u.X86.eax], eax
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX32.u.X86.ecx], ecx
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX32.u.X86.edx], edx
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX32.u.X86.ebx], ebx
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX32.u.X86.esi], esi
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX32.u.X86.edi], edi
        pushf
        pop     xAX
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX32.u.X86.eflags], xAX
        mov     xAX, [xBP + xS]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX32.u.X86.eip], xAX
        mov     xAX, [xBP]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX32.u.X86.ebp], xAX
        lea     xAX, [xBP + xS*2]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX32.u.X86.esp], xAX

        mov     xCX, [xBP + xS*2 + xS*0]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX32.u.X86.uVtgProbeLoc], xCX ; keep, used below.

        mov     edx, 20
.more:
        dec     edx
        mov     xAX, [xBP + xS*2 + xS*xDX]
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX32.u.X86.aArgs + xS*xDX], xAX
        jnz     .more

        mov     eax, [xCX + 4]          ; VTGPROBELOC::idProbe.
        mov     [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX32.idProbe], eax
        mov     dword [xSP + SUPTRACERUMODFIREPROBE.In + SUPDRVTRACERUSRCTX32.cBits], 32

        ;
        ; Call the helper.
        ;
        mov     xDX, xSP
        push    xDX
        push    xCX
        call    NAME(suplibTracerFireProbe)
%else
 %error "Arch not supported (or correctly defined)."
%endif


        leave
        ret
ENDPROC SUPTracerFireProbe


