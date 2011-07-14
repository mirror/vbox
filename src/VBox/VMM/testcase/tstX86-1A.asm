; $Id$
;; @file
; X86 instruction set testcase #1.
;

;
; Copyright (C) 2011 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;   Header Files                                                              ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%include "iprt/asmdefs.mac"
%include "iprt/x86.mac"

;; @todo Move this to a header?
struc TRAPINFO
        .uTrapPC        RTCCPTR_RES 1
        .uResumePC      RTCCPTR_RES 1
        .u8TrapNo       resb 1
        .cbInstr        resb 1
        .au8Padding     resb (RTCCPTR_CB*2 - 2)
endstruc


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;   Global Variables                                                          ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
BEGINDATA
extern NAME(g_pbEfPage)
extern NAME(g_pbEfExecPage)

GLOBALNAME g_szAlpha
        db      "abcdefghijklmnopqrstuvwxyz", 0
g_szAlpha_end:
%define g_cchAlpha (g_szAlpha_end - NAME(g_szAlpha))
        db      0, 0, 0,

;;
; The last global data item. We build this as we write the code.
GLOBALNAME g_aTrapInfo


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;   Defined Constants And Macros                                              ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%define X86_XCPT_UD 6
%define X86_XCPT_GP 13
%define X86_XCPT_PF 14

;; Reference a global variable
%ifdef RT_ARCH_AMD64
 %define REF_GLOBAL(a_Name)     [NAME(a_Name) wrt rip]
%else
 %define REF_GLOBAL(a_Name)     [NAME(a_Name)]
%endif

;;
; Macro for recording a trapping instruction (simple).
;
; @param        1       The trap number.
; @param        2+      The instruction which should trap.
%macro ShouldTrap 2+
%%trap:
        %2
%%trap_end:
        mov     eax, __LINE__
        jmp     .failed
BEGINDATA
%%trapinfo: istruc TRAPINFO
        at TRAPINFO.uTrapPC,    RTCCPTR_DEF     %%trap
        at TRAPINFO.uResumePC,  RTCCPTR_DEF     %%resume
        at TRAPINFO.u8TrapNo,   db              %1
        at TRAPINFO.cbInstr,    db              (%%trap_end - %%trap)
iend
BEGINCODE
%%resume:
%endmacro



BEGINCODE

;;
; Loads all general registers except xBP and xSP with unique values.
;
x861_LoadUniqueRegValues:
%ifdef RT_ARCH_AMD64
        mov     rax, 00000000000000000h
        mov     rcx, 01111111111111111h
        mov     rdx, 02222222222222222h
        mov     rbx, 03333333333333333h
        mov     rsi, 06666666666666666h
        mov     rdi, 07777777777777777h
        mov     r8,  08888888888888888h
        mov     r9,  09999999999999999h
        mov     r10, 0aaaaaaaaaaaaaaaah
        mov     r11, 0bbbbbbbbbbbbbbbbh
        mov     r12, 0cccccccccccccccch
        mov     r13, 0ddddddddddddddddh
        mov     r14, 0eeeeeeeeeeeeeeeeh
        mov     r15, 0ffffffffffffffffh
%else
        mov     eax, 000000000h
        mov     ecx, 011111111h
        mov     edx, 022222222h
        mov     ebx, 033333333h
        mov     esi, 066666666h
        mov     edi, 077777777h
%endif
        ret
; end x861_LoadUniqueRegValues


;;
; Clears all general registers except xBP and xSP.
;
x861_ClearRegisters:
        xor     eax, eax
        xor     ebx, ebx
        xor     ecx, ecx
        xor     edx, edx
        xor     esi, esi
        xor     edi, edi
%ifdef RT_ARCH_AMD64
        xor     r8,  r8
        xor     r9,  r9
        xor     r10, r10
        xor     r11, r11
        xor     r12, r12
        xor     r13, r13
        xor     r14, r14
        xor     r15, r15
%endif
        ret
; x861_ClearRegisters


BEGINPROC x861_Test1
        push    xBP
        mov     xBP, xSP
        pushf
        push    xBX
        push    xCX
        push    xDX
        push    xSI
        push    xDI
%ifdef RT_ARCH_AMD64
        push    r8
        push    r9
        push    r10
        push    r11
        push    r12
        push    r13
        push    r14
        push    r15
%endif

        ;
        ; Odd push behavior
        ;
%if 0 ; Seems to be so on AMD only
%ifdef RT_ARCH_X86
        ; upper word of a 'push cs' is cleared.
        mov     eax, __LINE__
        mov     dword [esp - 4], 0f0f0f0fh
        push    cs
        pop     ecx
        mov     bx, cs
        and     ebx, 0000ffffh
        cmp     ecx, ebx
        jne     .failed

        ; upper word of a 'push ds' is cleared.
        mov     eax, __LINE__
        mov     dword [esp - 4], 0f0f0f0fh
        push    ds
        pop     ecx
        mov     bx, ds
        and     ebx, 0000ffffh
        cmp     ecx, ebx
        jne     .failed

        ; upper word of a 'push es' is cleared.
        mov     eax, __LINE__
        mov     dword [esp - 4], 0f0f0f0fh
        push    es
        pop     ecx
        mov     bx, es
        and     ebx, 0000ffffh
        cmp     ecx, ebx
        jne     .failed
%endif ; RT_ARCH_X86

        ; The upper part of a 'push fs' is cleared.
        mov     eax, __LINE__
        xor     ecx, ecx
        not     xCX
        push    xCX
        pop     xCX
        push    fs
        pop     xCX
        mov     bx, fs
        and     ebx, 0000ffffh
        cmp     xCX, xBX
        jne     .failed

        ; The upper part of a 'push gs' is cleared.
        mov     eax, __LINE__
        xor     ecx, ecx
        not     xCX
        push    xCX
        pop     xCX
        push    gs
        pop     xCX
        mov     bx, gs
        and     ebx, 0000ffffh
        cmp     xCX, xBX
        jne     .failed
%endif

%ifdef RT_ARCH_AMD64
        ; REX.B works with 'push r64'.
        call    x861_LoadUniqueRegValues
        mov     eax, __LINE__
        push    rcx
        pop     rdx
        cmp     rdx, rcx
        jne     .failed

        call    x861_LoadUniqueRegValues
        mov     eax, __LINE__
        db 041h                         ; REX.B
        push    rcx
        pop     rdx
        cmp     rdx, r9
        jne     .failed

        call    x861_LoadUniqueRegValues
        mov     eax, __LINE__
        db 042h                         ; REX.X
        push    rcx
        pop     rdx
        cmp     rdx, rcx
        jne     .failed

        call    x861_LoadUniqueRegValues
        mov     eax, __LINE__
        db 044h                         ; REX.R
        push    rcx
        pop     rdx
        cmp     rdx, rcx
        jne     .failed

        call    x861_LoadUniqueRegValues
        mov     eax, __LINE__
        db 048h                         ; REX.W
        push    rcx
        pop     rdx
        cmp     rdx, rcx
        jne     .failed

        call    x861_LoadUniqueRegValues
        mov     eax, __LINE__
        db 04fh                         ; REX.*
        push    rcx
        pop     rdx
        cmp     rdx, r9
        jne     .failed
%endif

        ;
        ; Zero extening when moving from a segreg as well as memory access sizes.
        ;
        call    x861_LoadUniqueRegValues
        mov     eax, __LINE__
        mov     ecx, ds
        shr     xCX, 16
        cmp     xCX, 0
        jnz     .failed

%ifdef RT_ARCH_AMD64
        call    x861_LoadUniqueRegValues
        mov     eax, __LINE__
        mov     rcx, ds
        shr     rcx, 16
        cmp     rcx, 0
        jnz     .failed
%endif

        call    x861_LoadUniqueRegValues
        mov     eax, __LINE__
        mov     xDX, xCX
        mov     cx, ds
        shr     xCX, 16
        shr     xDX, 16
        cmp     xCX, xDX
        jnz     .failed

        ; Loading is always a word access.
        mov     eax, __LINE__
        mov     xDI, REF_GLOBAL(g_pbEfPage)
        lea     xDI, [xDI + 0x1000 - 2]
        mov     xDX, es
        mov     [xDI], dx
        mov     es, [xDI]               ; should not crash

        ; Saving is always a word access.
        mov     eax, __LINE__
        mov     xDI, REF_GLOBAL(g_pbEfPage)
        mov     dword [xDI + 0x1000 - 4], -1
        mov     [xDI + 0x1000 - 2], ss ; Should not crash.
        mov     bx, ss
        mov     cx, [xDI + 0x1000 - 2]
        cmp     cx, bx
        jne     .failed

%ifdef RT_ARCH_AMD64
        ; Check that the rex.R and rex.W bits don't have any influence over a memory write.
        call    x861_ClearRegisters
        mov     eax, __LINE__
        mov     xDI, REF_GLOBAL(g_pbEfPage)
        mov     dword [xDI + 0x1000 - 4], -1
        db 04ah
        mov     [xDI + 0x1000 - 2], ss ; Should not crash.
        mov     bx, ss
        mov     cx, [xDI + 0x1000 - 2]
        cmp     cx, bx
        jne     .failed
%endif


        ;
        ; Check what happens when both string prefixes are used.
        ;
        cld
        mov     dx, ds
        mov     es, dx

        ; check that repne scasb (al=0) behaves like expected.
        lea     xDI, REF_GLOBAL(g_szAlpha)
        xor     eax, eax                ; find the end
        mov     ecx, g_cchAlpha + 1
        repne scasb
        cmp     ecx, 1
        mov     eax, __LINE__
        jne     .failed

        ; check that repe scasb (al=0) behaves like expected.
        lea     xDI, REF_GLOBAL(g_szAlpha)
        xor     eax, eax                ; find the end
        mov     ecx, g_cchAlpha + 1
        repe scasb
        cmp     ecx, g_cchAlpha
        mov     eax, __LINE__
        jne     .failed

        ; repne is last, it wins.
        lea     xDI, REF_GLOBAL(g_szAlpha)
        xor     eax, eax                ; find the end
        mov     ecx, g_cchAlpha + 1
        db 0f3h                         ; repe  - ignored
        db 0f2h                         ; repne
        scasb
        cmp     ecx, 1
        mov     eax, __LINE__
        jne     .failed

        ; repe is last, it wins.
        lea     xDI, REF_GLOBAL(g_szAlpha)
        xor     eax, eax                ; find the end
        mov     ecx, g_cchAlpha + 1
        db 0f2h                         ; repne - ignored
        db 0f3h                         ; repe
        scasb
        cmp     ecx, g_cchAlpha
        mov     eax, __LINE__
        jne     .failed

        ;
        ; Check if stosb works with both prefixes.
        ;
        cld
        mov     dx, ds
        mov     es, dx
        mov     xDI, REF_GLOBAL(g_pbEfPage)
        xor     eax, eax
        mov     ecx, 01000h
        rep stosb

        mov     xDI, REF_GLOBAL(g_pbEfPage)
        mov     ecx, 4
        mov     eax, 0ffh
        db 0f2h                         ; repne
        stosb
        mov     eax, __LINE__
        cmp     ecx, 0
        jne     .failed
        mov     eax, __LINE__
        mov     xDI, REF_GLOBAL(g_pbEfPage)
        cmp     dword [xDI], 0ffffffffh
        jne     .failed
        cmp     dword [xDI+4], 0
        jne     .failed

        mov     xDI, REF_GLOBAL(g_pbEfPage)
        mov     ecx, 4
        mov     eax, 0feh
        db 0f3h                         ; repe
        stosb
        mov     eax, __LINE__
        cmp     ecx, 0
        jne     .failed
        mov     eax, __LINE__
        mov     xDI, REF_GLOBAL(g_pbEfPage)
        cmp     dword [xDI], 0fefefefeh
        jne     .failed
        cmp     dword [xDI+4], 0
        jne     .failed

        ;
        ; String operations shouldn't crash because of an invalid address if rCX is 0.
        ;
        mov     eax, __LINE__
        cld
        mov     dx, ds
        mov     es, dx
        mov     xDI, REF_GLOBAL(g_pbEfPage)
        xor     xCX, xCX
        rep stosb                       ; no trap

        ;
        ; INS/OUTS will trap in ring-3 even when rCX is 0. (ASSUMES IOPL < 3)
        ;
        mov     eax, __LINE__
        cld
        mov     dx, ss
        mov     ss, dx
        mov     xDI, xSP
        xor     xCX, xCX
        ShouldTrap X86_XCPT_GP, rep insb

        ;
        ; SMSW can get to the whole of CR0.
        ;
        mov     eax, __LINE__
        xor     xBX, xBX
        smsw    xBX
        test    ebx, X86_CR0_PG
        jz      .failed
        test    ebx, X86_CR0_PE
        jz      .failed

        ;
        ; Will the CPU decode the whole r/m+sib stuff before signalling a lock
        ; prefix error?  Use the EF exec page and a LOCK ADD CL,[rDI + disp32]
        ; instruction at the very end of it.
        ;
        mov     eax, __LINE__
        mov     xDI, REF_GLOBAL(g_pbEfExecPage)
        add     xDI, 1000h - 8h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        mov     byte [xDI+2], 08fh
        mov     dword [xDI+3], 000000000h
        mov     byte [xDI+7], 0cch
        ShouldTrap X86_XCPT_UD, call xDI

        mov     eax, __LINE__
        mov     xDI, REF_GLOBAL(g_pbEfExecPage)
        add     xDI, 1000h - 7h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        mov     byte [xDI+2], 08Fh
        mov     dword [xDI+3], 000000000h
        ShouldTrap X86_XCPT_UD, call xDI

        mov     eax, __LINE__
        mov     xDI, REF_GLOBAL(g_pbEfExecPage)
        add     xDI, 1000h - 4h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        mov     byte [xDI+2], 08Fh
        mov     byte [xDI+3], 000h
        ShouldTrap X86_XCPT_PF, call xDI

        mov     eax, __LINE__
        mov     xDI, REF_GLOBAL(g_pbEfExecPage)
        add     xDI, 1000h - 6h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        mov     byte [xDI+2], 08Fh
        mov     byte [xDI+3], 00h
        mov     byte [xDI+4], 00h
        mov     byte [xDI+5], 00h
        ShouldTrap X86_XCPT_PF, call xDI

        mov     eax, __LINE__
        mov     xDI, REF_GLOBAL(g_pbEfExecPage)
        add     xDI, 1000h - 5h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        mov     byte [xDI+2], 08Fh
        mov     byte [xDI+3], 00h
        mov     byte [xDI+4], 00h
        ShouldTrap X86_XCPT_PF, call xDI

        mov     eax, __LINE__
        mov     xDI, REF_GLOBAL(g_pbEfExecPage)
        add     xDI, 1000h - 4h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        mov     byte [xDI+2], 08Fh
        mov     byte [xDI+3], 00h
        ShouldTrap X86_XCPT_PF, call xDI

        mov     eax, __LINE__
        mov     xDI, REF_GLOBAL(g_pbEfExecPage)
        add     xDI, 1000h - 3h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        mov     byte [xDI+2], 08Fh
        ShouldTrap X86_XCPT_PF, call xDI

        mov     eax, __LINE__
        mov     xDI, REF_GLOBAL(g_pbEfExecPage)
        add     xDI, 1000h - 2h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        ShouldTrap X86_XCPT_PF, call xDI

        mov     eax, __LINE__
        mov     xDI, REF_GLOBAL(g_pbEfExecPage)
        add     xDI, 1000h - 1h
        mov     byte [xDI+0], 0f0h
        ShouldTrap X86_XCPT_PF, call xDI



.success:
        xor     eax, eax
.return:
%ifdef RT_ARCH_AMD64
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     r11
        pop     r10
        pop     r9
        pop     r8
%endif
        pop     xDI
        pop     xSI
        pop     xDX
        pop     xCX
        pop     xBX
        popf
        leave
        ret

.failed2:
        mov     eax, -1
.failed:
        jmp     .return
ENDPROC   x861_Test1


;;
; Terminate the trap info array with a NIL entry.
BEGINDATA
GLOBALNAME g_aTrapInfoEnd
istruc TRAPINFO
        at TRAPINFO.uTrapPC,    RTCCPTR_DEF     0
        at TRAPINFO.uResumePC,  RTCCPTR_DEF     0
        at TRAPINFO.u8TrapNo,   db              0
        at TRAPINFO.cbInstr,    db              0
iend

