; $Id$
;; @file
; IPRT - Stack related Visual C++ support routines.
;

;
; Copyright (C) 2022 Oracle Corporation
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



;*********************************************************************************************************************************
;*      Header Files                                                                                                             *
;*********************************************************************************************************************************
%include "iprt/asmdefs.mac"
%include "iprt/x86.mac"


;*********************************************************************************************************************************
;*      Structures and Typedefs                                                                                                  *
;*********************************************************************************************************************************

;; Variable descriptor.
struc RTC_VAR_DESC_T
        .offFrame               resd 1
        .cbVar                  resd 1
        alignb                  RTCCPTR_CB
        .pszName                RTCCPTR_RES 1
endstruc

;; Frame descriptor.
struc RTC_FRAME_DESC_T
        .cVars                  resd 1
        alignb                  RTCCPTR_CB
        .paVars                 RTCCPTR_RES 1   ; Array of RTC_VAR_DESC_T.
endstruc

;; An alloca allocation.
struc RTC_ALLOCA_ENTRY_T
        .uGuard1                resd 1
        .pNext                  RTCCPTR_RES 1   ; Misaligned.
%if ARCH_BITS == 32
        .pNextPad               resd 1
%endif
        .cb                     RTCCPTR_RES 1   ; Misaligned.
%if ARCH_BITS == 32
        .cbPad                  resd 1
%endif
        .auGuard2               resd 3
endstruc

%ifdef RT_ARCH_X86
 %define FASTCALL_NAME(a_Name, a_cbArgs)        $@ %+ a_Name %+ @ %+ a_cbArgs
%else
 %define FASTCALL_NAME(a_Name, a_cbArgs)        NAME(a_Name)
%endif


;*********************************************************************************************************************************
;*      Defined Constants And Macros                                                                                             *
;*********************************************************************************************************************************
%define VARIABLE_MARKER_PRE     0xcccccccc
%define VARIABLE_MARKER_POST    0xcccccccc

%define ALLOCA_FILLER_BYTE      0xcc
%define ALLOCA_FILLER_32        0xcccccccc


;*********************************************************************************************************************************
;*  Global Variables                                                                                                             *
;*********************************************************************************************************************************
BEGINDATA
GLOBALNAME __security_cookie
        dd  0xdeadbeef
        dd  0x0c00ffe0


;*********************************************************************************************************************************
;*  External Symbols                                                                                                             *
;*********************************************************************************************************************************
BEGINCODE
extern NAME(_RTC_StackVarCorrupted)
extern NAME(_RTC_SecurityCookieMismatch)


BEGINPROC __GSHandlerCheck
        int3
ENDPROC   __GSHandlerCheck

;;
; Probe stack to trigger guard faults.
;
; @param    eax     Frame size.
; @uses     Nothing (because we don't quite now the convention).
;
ALIGNCODE(32)
BEGINPROC __chkstk
        push    xBP
        mov     xBP, xSP
        pushf
        push    xAX
        push    xBX

        xor     ebx, ebx
.again:
        sub     xBX, PAGE_SIZE
        mov     [xBP + xBX], bl
        sub     eax, PAGE_SIZE
        jnl     .again

        pop     xBX
        pop     xAX
        popf
        leave
        ret
ENDPROC   __chkstk


;;
; This just initializes a global and calls _RTC_SetErrorFuncW to NULL, and
; since we don't have either of those we have nothing to do here.
BEGINPROC _RTC_InitBase
        ret
ENDPROC   _RTC_InitBase


;;
; Nothing to do here.
BEGINPROC _RTC_Shutdown
        ret
ENDPROC   _RTC_Shutdown




;;
; Checks stack variable markers.
;
; This seems to be a regular C function in the CRT, but x86 is conveniently
; using the fastcall convention which makes it very similar to amd64.
;
; We try make this as sleek as possible, leaving all the trouble for when we
; find a corrupted stack variable and need to call a C function to complain.
;
; @param        pStackFrame     The caller RSP/ESP.  [RCX/ECX]
; @param        pFrameDesc      Frame descriptor.    [RDX/EDX]
;
ALIGNCODE(64)
BEGINPROC_RAW   FASTCALL_NAME(_RTC_CheckStackVars, 8)
        push    xBP

        ;
        ; Load the variable count into eax and check that it's not zero.
        ;
        mov     eax, [xDX + RTC_FRAME_DESC_T.cVars]
        test    eax, eax
        jz      .return

        ;
        ; Make edx/rdx point to the current variable and xBP be the frame pointer.
        ; The latter frees up xCX for scratch use and incidentally make stack access
        ; go via SS instead of DS (mostly irrlevant in 64-bit and 32-bit mode).
        ;
        mov     xDX, [xDX + RTC_FRAME_DESC_T.paVars]
        mov     xBP, xCX

        ;
        ; Loop thru the variables and check that their markers/fences haven't be
        ; trampled over.
        ;
.next_var:
        ; Marker before the variable.
%if ARCH_BITS == 64
        movsxd  rcx, dword [xDX + RTC_VAR_DESC_T.offFrame]
%else
        mov     xCX, dword [xDX + RTC_VAR_DESC_T.offFrame]
%endif
        cmp     dword [xBP + xCX - 4], VARIABLE_MARKER_PRE
        jne     .corrupted

        ; Marker after the variable.
        add     ecx, dword [xDX + RTC_VAR_DESC_T.cbVar]
%if ARCH_BITS == 64
        movsxd  rcx, ecx
%endif
        cmp     dword [xBP + xCX], VARIABLE_MARKER_POST
        jne     .corrupted

        ;
        ; Advance to the next variable.
        ;
.advance:
        add     xDX, RTC_VAR_DESC_T_size
        dec     eax
        jnz     .next_var

        ;
        ; Return.
        ;
.return:
        pop     xBP
        ret

        ;
        ; Complain about corrupt variable.
        ;
.corrupted:
        push    xAX
        push    xDX
%ifdef RT_ARCH_AMD64
        sub     xSP, 28h
        mov     xCX, xBP                ; frame pointer + variable descriptor.
%else
        push    xBP                     ; save EBP
        push    xDX                     ; parameter 2 - variable descriptor
        push    xBP                     ; parameter 1 - frame pointer.
        lea     xBP, [xSP + 3*xCB]      ; turn it into a frame pointer during the call for better unwind.
%endif

        call    NAME(_RTC_StackVarCorrupted)

%ifdef RT_ARCH_AMD64
        add     xSP, 28h
%else
        add     xSP, xCB * 4            ; parameters
        pop     xBP
%endif
        pop     xDX
        pop     xAX
        jmp     .advance
ENDPROC_RAW     FASTCALL_NAME(_RTC_CheckStackVars, 8)


;;
; Initialize an alloca allocation list entry and add it to it.
;
; When this is call, presumably _RTC_CheckStackVars2 is used to verify the frame.
;
; @param        pNewEntry       Pointer to the new entry.               [RCX/ECX]
; @param        cbEntry         The entry size, including header.       [RDX/EDX]
; @param        ppHead          Pointer to the list head pointer.       [R8/stack]
;
ALIGNCODE(64)
BEGINPROC_RAW   FASTCALL_NAME(_RTC_AllocaHelper, 12)
        ;
        ; Check that input isn't NULL or the size isn't zero.
        ;
        test    xCX, xCX
        jz      .return
        test    xDX, xDX
        jz      .return
%if ARCH_BITS == 64
        test    r8, r8
%else
        cmp     dword [xSP + xCB], 0
%endif
        jz      .return

        ;
        ; Memset the memory to ALLOCA_FILLER
        ;
%if ARCH_BITS == 64
        mov     r10, rdi                ; save rdi
        mov     r11, rcx                ; save pNewEntry
%else
        push    xDI
        push    xCX
        cld                             ; paranoia
%endif

        mov     al, ALLOCA_FILLER_BYTE
        mov     xDI, xCX                ; entry pointer
        mov     xCX, xDX                ; entry size (in bytes)
        rep stosb

%if ARCH_BITS == 64
        mov     rdi, r10
%else
        pop     xCX
        pop     xDI
%endif

        ;
        ; Fill in the entry and link it as onto the head of the chain.
        ;
%if ARCH_BITS == 64
        mov     [r11 + RTC_ALLOCA_ENTRY_T.cb], xDX
        mov     xAX, [r8]
        mov     [r11 + RTC_ALLOCA_ENTRY_T.pNext], xAX
        mov     [r8], r11
%else
        mov     [xCX + RTC_ALLOCA_ENTRY_T.cb], xDX
        mov     xAX, [xSP + xCB]        ; ppHead
        mov     xDX, [xAX]
        mov     [xCX + RTC_ALLOCA_ENTRY_T.pNext], xDX
        mov     [xAX], xCX
%endif

.return:
%if ARCH_BITS == 64
        ret
%else
        ret     4
%endif
ENDPROC_RAW     FASTCALL_NAME(_RTC_AllocaHelper, 12)


;;
; Checks if the secuity cookie ok, complaining and terminating if it isn't.
;
ALIGNCODE(16)
BEGINPROC_RAW   FASTCALL_NAME(__security_check_cookie, 4)
        cmp     xCX, [NAME(__security_cookie) xWrtRIP]
        jne     .corrupted
        ;; amd64 version checks if the top 16 bits are zero, we skip that for now.
        ret

.corrupted:
%ifdef RT_ARCH_AMD64
        jmp     NAME(_RTC_SecurityCookieMismatch)
%else
        push    ebp
        mov     ebp, esp
        push    ecx
        call    NAME(_RTC_SecurityCookieMismatch)
        pop     ecx
        leave
        ret
%endif
ENDPROC_RAW     FASTCALL_NAME(__security_check_cookie, 4)



; Not stack related stubs.
BEGINPROC __C_specific_handler
        int3
ENDPROC   __C_specific_handler


BEGINPROC __report_rangecheckfailure
        int3
ENDPROC   __report_rangecheckfailure

