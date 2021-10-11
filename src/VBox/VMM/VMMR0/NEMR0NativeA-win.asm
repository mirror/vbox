 ; $Id$
;; @file
; NEM/win - Ring-0 Assembly Routines.
;

;
; Copyright (C) 2021 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;


;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%define RT_ASM_WITH_SEH64
%include "iprt/asmdefs.mac"


;*********************************************************************************************************************************
;*  External Symbols                                                                                                             *
;*********************************************************************************************************************************
BEGINDATA
extern  NAME(g_pfnWinHvGetPartitionProperty)
extern  NAME(g_idVidSysFoundPartition)
extern  NAME(g_hVidSysMatchThread)
extern  NAME(g_enmVidSysMatchProperty)
;extern  NAME(g_pfnHvrWinHvGetPartitionPropertyLeadIn)

BEGINCODE
extern  NAME(RTThreadNativeSelf)


;;
; This is a replacement for WinHvGetPartitionProperty that we slot into VID.SYS's
; import table so we can fish out the partition ID (first parameter).
;
BEGINPROC nemR0VidSysWinHvGetPartitionProperty
;
; Code is shared with nemR0WinHvrWinHvGetPartitionProperty.
;
%macro WinHvGetPartitionPropertyHookBody 0
        ;
        ; Create a frame and save all volatile registers.
        ;
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
        sub     xSP, 0x100+0x20
        SEH64_ALLOCATE_STACK 0x100+0x20
        movdqa  [rbp - 10h], xmm0
        movdqa  [rbp - 20h], xmm1
        movdqa  [rbp - 30h], xmm2
        movdqa  [rbp - 40h], xmm3
        movdqa  [rbp - 50h], xmm4
        movdqa  [rbp - 60h], xmm5
        mov     [rbp - 70h], rcx
        mov     [rbp - 78h], rdx
        mov     [rbp - 80h], r8
        mov     [rbp - 88h], r9
        mov     [rbp - 90h], r10
        mov     [rbp - 98h], r11
SEH64_END_PROLOGUE

        ;
        ; Check if the property matches the one we're querying
        ;
        cmp     edx, [NAME(g_enmVidSysMatchProperty) wrt rip] ; It is really 32-bit.
        jne     .return

        ;
        ; Get the current thread and compare that to g_hVidSysTargetThread,
        ; making sure it's not NULL or NIL.
        ;
        call    NAME(RTThreadNativeSelf)
        or      rax, rax                ; check that it isn't zero.
        jz      .return

        cmp     rax, [NAME(g_hVidSysMatchThread) wrt rip]
        jne     .return

        inc     rax                     ; check for ~0.
        jz      .return

        ;
        ; Got a match, save the partition ID.
        ;
        mov     rcx, [rbp - 70h]
        mov     [NAME(g_idVidSysFoundPartition) wrt rip], rcx

        ;
        ; Restore the volatile registers.
        ;
.return:
        movdqa  xmm0, [rbp - 10h]
        movdqa  xmm1, [rbp - 20h]
        movdqa  xmm2, [rbp - 30h]
        movdqa  xmm3, [rbp - 40h]
        movdqa  xmm4, [rbp - 50h]
        movdqa  xmm5, [rbp - 60h]
        mov     rcx,  [rbp - 70h]
        mov     rdx,  [rbp - 78h]
        mov     r8,   [rbp - 80h]
        mov     r9,   [rbp - 88h]
        mov     r10,  [rbp - 90h]
        mov     r11,  [rbp - 98h]
        leave
%endmacro

        ;
        ; Identical to nemR0WinHvrWinHvGetPartitionProperty except for the
        ; resuming of the real code.
        ;
        WinHvGetPartitionPropertyHookBody

        ;
        ; Instead of returning, jump to the real WinHvGetPartitionProperty code.
        ;
        mov     rax, [NAME(g_pfnWinHvGetPartitionProperty) wrt rip]
        jmp     rax
ENDPROC   nemR0VidSysWinHvGetPartitionProperty


section .textrwx code page execute read write

;;
; This is where we jump to from a WinHvr.sys jmp patch.
;
; This is used if the import table patching doesn't work because the indirect
; call was converted into a direct call by the the retpoline patching code.
;
ALIGNCODE(16)
BEGINPROC nemR0WinHvrWinHvGetPartitionProperty

        ;
        ; Identical to nemR0VidSysWinHvGetPartitionProperty except for the
        ; resuming of the real code.
        ;
        WinHvGetPartitionPropertyHookBody

        ;
        ; Now back to the orignal code.
        ;
        ; We reserve 64 bytes of space for the lead-in code that we replaced
        ; from WinHvGetPartitionProperty and additional 12 for the jump back.
        ;
GLOBALNAME g_abNemR0WinHvrWinHvGetPartitionProperty_OriginalProlog
        times 64 int3                   ; IMPORTANT! Must be at least as big as the 'Org' variable in the C code.
        mov     rax, 01234567890abcdefh
        jmp     rax
ENDPROC   nemR0WinHvrWinHvGetPartitionProperty

