; $Id$
;; @file
; IPRT - Stack related Visual C++ support routines.
;

;
; Copyright (C) 2022 Oracle and/or its affiliates.
;
; This file is part of VirtualBox base platform packages, as
; available from https://www.virtualbox.org.
;
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation, in version 3 of the
; License.
;
; This program is distributed in the hope that it will be useful, but
; WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, see <https://www.gnu.org/licenses>.
;
; The contents of this file may alternatively be used under the terms
; of the Common Development and Distribution License Version 1.0
; (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
; in the VirtualBox distribution, in which case the provisions of the
; CDDL are applicable instead of those of the GPL.
;
; You may elect to license modified versions of this file under the
; terms and conditions of either the GPL or the CDDL or both.
;
; SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
;



;*********************************************************************************************************************************
;*      Header Files                                                                                                             *
;*********************************************************************************************************************************
%if 0 ; YASM's builtin SEH64 support doesn't cope well with code alignment, so use our own.
 %define RT_ASM_WITH_SEH64
%else
 %define RT_ASM_WITH_SEH64_ALT
%endif
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
%ifdef RT_ARCH_X86
extern NAME(_RTC_CheckEspFailed)
%endif



;;
; Probe stack to trigger guard faults, and for x86 to allocate stack space.
;
; @param    xAX     Frame size.
; @uses     AMD64: Nothing (because we don't quite now the convention).
;           x86:   ESP = ESP - EAX; nothing else
;
ALIGNCODE(64)
GLOBALNAME_RAW  __alloca_probe, __alloca_probe, function
BEGINPROC_RAW   __chkstk
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
        push    xAX
        SEH64_PUSH_GREG xAX
        push    xBX
        SEH64_PUSH_GREG xBX
        SEH64_END_PROLOGUE

        ;
        ; Adjust eax so we can use xBP for stack addressing.
        ;
        sub     xAX, xCB*2
        jle     .touch_loop_done

        ;
        ; Subtract what's left of the current page from eax and only engage
        ; the touch loop if (int)xAX > 0.
        ;
        mov     ebx, PAGE_SIZE - 1
        and     ebx, ebp
        sub     xAX, xBX
        jnl     .touch_loop

.touch_loop_done:
        pop     xBX
        pop     xAX
        leave
%ifndef RT_ARCH_X86
        ret
%else
        ;
        ; Do the stack space allocation and jump to the return location.
        ;
        sub     esp, eax
        add     esp, 4
        jmp    dword [esp + eax - 4]
%endif

        ;
        ; The touch loop.
        ;
.touch_loop:
        sub     xBX, PAGE_SIZE
        mov     [xBP + xBX], bl
        sub     xAX, PAGE_SIZE
        jnl     .touch_loop
        jmp     .touch_loop_done
ENDPROC_RAW     __chkstk


%ifdef RT_ARCH_X86
;;
; 8 and 16 byte aligned alloca w/ probing.
;
; This routine adjusts the allocation size so __chkstk will return a
; correctly aligned allocation.
;
; @param    xAX     Unaligned allocation size.
;
%macro __alloc_probe_xxx 1
ALIGNCODE(16)
BEGINPROC_RAW   __alloca_probe_ %+ %1
        push    ecx

        ;
        ; Calc the ESP address after the allocation and adjust EAX so that it
        ; will be aligned as desired.
        ;
        lea     ecx, [esp + 8]
        sub     ecx, eax
        and     ecx, %1 - 1
        add     eax, ecx
        jc      .bad_alloc_size
.continue:

        pop     ecx
        jmp     __alloca_probe

.bad_alloc_size:
  %ifdef RT_STRICT
        int3
  %endif
        or      eax, 0xfffffff0
        jmp     .continue
ENDPROC_RAW     __alloca_probe_ %+ %1
%endmacro

__alloc_probe_xxx 16
__alloc_probe_xxx 8
%endif ; RT_ARCH_X86


;;
; This just initializes a global and calls _RTC_SetErrorFuncW to NULL, and
; since we don't have either of those we have nothing to do here.
BEGINPROC _RTC_InitBase
        SEH64_END_PROLOGUE
        ret
ENDPROC   _RTC_InitBase


;;
; Nothing to do here.
BEGINPROC _RTC_Shutdown
        SEH64_END_PROLOGUE
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
        SEH64_PUSH_xBP
        SEH64_END_PROLOGUE

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


%ifdef RT_ARCH_X86
;;
; Called to follow up on a 'CMP ESP, EBP' kind of instruction,
; expected to report failure if the compare failed.
;
ALIGNCODE(16)
BEGINPROC _RTC_CheckEsp
        jne     .unexpected_esp
        ret

.unexpected_esp:
        push    ebp
        mov     ebp, esp
        push    eax
        push    ecx
        push    edx

        ; DECLASM(void) _RTC_CheckEspFailed(uintptr_t uEip, uintptr_t uEsp, uintptr_t uEbp)
        push    dword [ebp]
        lea     edx, [ebp + 8]
        push    edx
        mov     ecx, [ebp + 8]
        push    ecx
        call    NAME(_RTC_CheckEspFailed)

        pop     edx
        pop     ecx
        pop     eax
        leave
        ret
ENDPROC   _RTC_CheckEsp
%endif ; RT_ARCH_X86



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
        SEH64_END_PROLOGUE

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
        SEH64_END_PROLOGUE
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
BEGINPROC __report_rangecheckfailure
        SEH64_END_PROLOGUE
        int3
ENDPROC   __report_rangecheckfailure

