; $Id$
;; @file
; X86 instruction set exploration/testcase #1.
;

;
; Copyright (C) 2011-2012 Oracle Corporation
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


%ifdef RT_ARCH_AMD64
 %define arch_fxsave    o64 fxsave
 %define arch_fxrstor   o64 fxrstor
%else
 %define arch_fxsave    fxsave
 %define arch_fxrstor   fxrstor
%endif

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

;; @name Floating point constants.
; @{
g_r32_0dot1:    dd 0.1
g_r32_3dot2:    dd 3.2
g_r32_Zero:     dd 0.0
g_r32_One:      dd 1.0
g_r32_Two:      dd 2.0
g_r32_Three:    dd 3.0
g_r32_Ten:      dd 10.0
g_r32_Eleven:   dd 11.0
g_r32_ThirtyTwo:dd 32.0
g_r32_Min:      dd 000800000h
g_r32_Max:      dd 07f7fffffh
g_r32_Inf:      dd 07f800000h
g_r32_SNaN:     dd 07f800001h
g_r32_SNaNMax:  dd 07fbfffffh
g_r32_QNaN:     dd 07fc00000h
g_r32_QNaNMax:  dd 07fffffffh

g_r64_0dot1:    dq 0.1
g_r64_6dot9:    dq 6.9
g_r64_Zero:     dq 0.0
g_r64_One:      dq 1.0
g_r64_Two:      dq 2.0
g_r64_Three:    dq 3.0
g_r64_Ten:      dq 10.0
g_r64_Eleven:   dq 11.0
g_r64_ThirtyTwo:dq 32.0
g_r64_Min:      dq 00010000000000000h
g_r64_Max:      dq 07fefffffffffffffh
g_r64_Inf:      dq 07ff0000000000000h
g_r64_SNaN:     dq 07ff0000000000001h
g_r64_SNaNMax:  dq 07ff7ffffffffffffh
g_r64_QNaN:     dq 07ff8000000000000h
g_r64_QNaNMax:  dq 07fffffffffffffffh
g_r64_DnMin:    dq 00000000000000001h
g_r64_DnMax:    dq 0000fffffffffffffh


g_r80_0dot1:    dt 0.1
g_r80_3dot2:    dt 3.2
g_r80_Zero:     dt 0.0
g_r80_One:      dt 1.0
g_r80_Two:      dt 2.0
g_r80_Three:    dt 3.0
g_r80_Ten:      dt 10.0
g_r80_Eleven:   dt 11.0
g_r80_ThirtyTwo:dt 32.0
g_r80_Min:      dt 000018000000000000000h
g_r80_Max:      dt 07ffeffffffffffffffffh
g_r80_Inf:      dt 07fff8000000000000000h

g_r32V1:        dd 3.2
g_r32V2:        dd -1.9
g_r64V1:        dq 6.4
g_r80V1:        dt 8.0

; Denormal numbers.
g_r32D0:        dd 000200000h
;; @}

;; @name Upconverted Floating point constants
; @{
;g_r80_r32_0dot1:        dt 0.1
g_r80_r32_3dot2:        dt 04000cccccd0000000000h
;g_r80_r32_Zero:         dt 0.0
;g_r80_r32_One:          dt 1.0
;g_r80_r32_Two:          dt 2.0
;g_r80_r32_Three:        dt 3.0
;g_r80_r32_Ten:          dt 10.0
;g_r80_r32_Eleven:       dt 11.0
;g_r80_r32_ThirtyTwo:    dt 32.0
;; @}

;; @name Decimal constants.
; @{
g_u64Zero:      dd 0
g_u32Zero:      dw 0
g_u64Two:       dd 2
g_u32Two:       dw 2
;; @}


;;
; The last global data item. We build this as we write the code.
GLOBALNAME g_aTrapInfo


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;   Defined Constants And Macros                                              ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Reference a variable
%ifdef RT_ARCH_AMD64
 %define REF(a_Name)     a_Name wrt rip
%else
 %define REF(a_Name)     a_Name
%endif

;; Reference a global variable
%ifdef RT_ARCH_AMD64
 %define REF_EXTERN(a_Name)     NAME(a_Name) wrt rip
%else
 %define REF_EXTERN(a_Name)     NAME(a_Name)
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
        jmp     .return
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


;;
; Macro for recording a FPU instruction trapping on a following fwait.
;
; Uses stack.
;
; @param        1       The status flags that are expected to be set afterwards.
; @param        2       C0..C3 to mask out in case undefined.
; @param        3+      The instruction which should trap.
; @uses         eax
;
%macro FpuShouldTrap 3+
        fnclex
        %3
%%trap:
        fwait
%%trap_end:
        mov     eax, __LINE__
        jmp     .return
BEGINDATA
%%trapinfo: istruc TRAPINFO
        at TRAPINFO.uTrapPC,    RTCCPTR_DEF     %%trap
        at TRAPINFO.uResumePC,  RTCCPTR_DEF     %%resume
        at TRAPINFO.u8TrapNo,   db              X86_XCPT_MF
        at TRAPINFO.cbInstr,    db              (%%trap_end - %%trap)
iend
BEGINCODE
%%resume:
        FpuCheckFSW ((%1) | X86_FSW_ES | X86_FSW_B), %2
        fnclex
%endmacro

;;
; Macro for recording checking the FSW value.
;
; Uses stack.
;
; @param        1       The status flags that are expected to be set afterwards.
; @param        2       C0..C3 to mask out in case undefined.
; @uses         eax
;
%macro FpuCheckFSW 2
%%resume:
        fnstsw  ax
        and     eax, ~X86_FSW_TOP_MASK & ~(%2)
        cmp     eax, (%1)
        je      %%ok
        ;int3
        lea     eax, [eax + __LINE__ * 100000]
        jmp     .return
%%ok:
%endmacro


;;
; Checks that ST0 has a certain value
;
; @uses tword at [xSP]
;
%macro CheckSt0Value 3
        fstp    tword [xSP]
        fld     tword [xSP]
        cmp     dword [xSP], %1
        je      %%ok1
%%bad:
        mov     eax, __LINE__
        jmp     .return
%%ok1:
        cmp     dword [xSP + 4], %2
        jne     %%bad
        cmp     word  [xSP + 8], %3
        jne     %%bad
%endmacro

;; Checks that ST0 contains QNaN.
%define CheckSt0Value_QNaN              CheckSt0Value 0x00000000, 0xc0000000, 0xffff
;; Checks that ST0 contains +Inf.
%define CheckSt0Value_PlusInf           CheckSt0Value 0x00000000, 0x80000000, 0x7fff
;; Checks that ST0 contains 3 & 1/3.
%define CheckSt0Value_3_and_a_3rd       CheckSt0Value 0x55555555, 0xd5555555, 0x4000
;; Checks that ST0 contains 3 & 1/3.
%define CheckSt0Value_3_and_two_3rds    CheckSt0Value 0xaaaaaaab, 0xeaaaaaaa, 0x4000


;;
; Macro for recording checking the FSW value of a FXSAVE image.
;
; Uses stack.
;
; @param        1       Address expression for the FXSAVE image.
; @param        2       The status flags that are expected to be set afterwards.
; @param        3       C0..C3 to mask out in case undefined.
; @uses         eax
; @sa   FpuCheckFSW
;
%macro FxSaveCheckFSW 3
%%resume:
        movzx   eax, word [%1 + X86FXSTATE.FSW]
        and     eax, ~X86_FSW_TOP_MASK & ~(%3)
        cmp     eax, (%2)
        je      %%ok
        mov     eax, 10000000 + __LINE__
        jmp     .return
%%ok:
%endmacro


;;
; Checks that ST0 is empty in an FXSAVE image.
;
; @uses         eax
; @param        1       Address expression for the FXSAVE image.
;
%macro FxSaveCheckSt0Empty 1
        movzx   eax, word [%1 + X86FXSTATE.FSW]
        and     eax, X86_FSW_TOP_MASK
        shr     eax, X86_FSW_TOP_SHIFT
        bt      [%1 + X86FXSTATE.FTW], eax
        jnc     %%ok
        mov     eax, 20000000 + __LINE__
        jmp     .return
%%ok:
%endmacro


;;
; Checks that ST0 is not-empty in an FXSAVE image.
;
; @uses         eax
; @param        1       Address expression for the FXSAVE image.
;
%macro FxSaveCheckSt0NonEmpty 1
        movzx   eax, word [%1 + X86FXSTATE.FSW]
        and     eax, X86_FSW_TOP_MASK
        shr     eax, X86_FSW_TOP_SHIFT
        bt      [%1 + X86FXSTATE.FTW], eax
        jc      %%ok
        mov     eax, 30000000 + __LINE__
        jmp     .return
%%ok:
%endmacro

;;
; Checks that ST0 in a FXSAVE image has a certain value (empty or not
; is ignored).
;
; @uses         eax
; @param        1       Address expression for the FXSAVE image.
; @param        2       First dword of value.
; @param        3       Second dword of value.
; @param        4       Final word of value.
;
%macro FxSaveCheckSt0ValueEx 4
        cmp     dword [%1 + X86FXSTATE.st0], %2
        je      %%ok1
%%bad:
        mov     eax, 40000000 + __LINE__
        jmp     .return
%%ok1:
        cmp     dword [%1 + X86FXSTATE.st0 + 4], %3
        jne     %%bad
        cmp     word  [%1 + X86FXSTATE.st0 + 8], %4
        jne     %%bad
%endmacro


;;
; Checks if ST0 in a FXSAVE image has the same value as the specified
; floating point (80-bit) constant.
;
; @uses         eax, xDX
; @param        1       Address expression for the FXSAVE image.
; @param        2       The address expression of the constant.
;
%macro FxSaveCheckSt0ValueConstEx 2
        mov     eax, [%2]
        cmp     dword [%1 + X86FXSTATE.st0], eax
        je      %%ok1
%%bad:
        mov     eax, 40000000 + __LINE__
        jmp     .return
%%ok1:
        mov     eax, [4 + %2]
        cmp     dword [%1 + X86FXSTATE.st0 + 4], eax
        jne     %%bad
        mov     ax, [8 + %2]
        cmp     word  [%1 + X86FXSTATE.st0 + 8], ax
        jne     %%bad
%endmacro


;;
; Checks that ST0 in a FXSAVE image has a certain value.
;
; @uses         eax
; @param        1       Address expression for the FXSAVE image.
; @param        2       First dword of value.
; @param        3       Second dword of value.
; @param        4       Final word of value.
;
%macro FxSaveCheckSt0Value 4
        FxSaveCheckSt0NonEmpty %1
        FxSaveCheckSt0ValueEx %1, %2, %3, %4
%endmacro


;;
; Checks that ST0 in a FXSAVE image is empty and that the value stored is the
; init value set by FpuInitWithCW.
;
; @uses         eax
; @param        1       Address expression for the FXSAVE image.
;
%macro FxSaveCheckSt0EmptyInitValue 1
        FxSaveCheckSt0Empty %1
        FxSaveCheckSt0ValueEx %1, 0x40404040, 0x40404040, 0xffff
%endmacro

;;
; Checks that ST0 in a FXSAVE image is non-empty and has the same value as the
; specified constant (80-bit).
;
; @uses         eax, xDX
; @param        1       Address expression for the FXSAVE image.
; @param        2       The address expression of the constant.
%macro FxSaveCheckSt0ValueConst 2
        FxSaveCheckSt0NonEmpty %1
        FxSaveCheckSt0ValueConstEx %1, %2
%endmacro

;; Checks that ST0 contains QNaN.
%define FxSaveCheckSt0Value_QNaN(p)              FxSaveCheckSt0Value p, 0x00000000, 0xc0000000, 0xffff
;; Checks that ST0 contains +Inf.
%define FxSaveCheckSt0Value_PlusInf(p)           FxSaveCheckSt0Value p, 0x00000000, 0x80000000, 0x7fff
;; Checks that ST0 contains 3 & 1/3.
%define FxSaveCheckSt0Value_3_and_a_3rd(p)       FxSaveCheckSt0Value p, 0x55555555, 0xd5555555, 0x4000
;; Checks that ST0 contains 3 & 1/3.
%define FxSaveCheckSt0Value_3_and_two_3rds(p)    FxSaveCheckSt0Value p, 0xaaaaaaab, 0xeaaaaaaa, 0x4000


;;
; Function prologue saving all registers except EAX and aligns the stack
; on a 16-byte boundrary.
;
%macro SAVE_ALL_PROLOGUE 0
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
        and     xSP, ~0fh;
%endmacro


;;
; Function epilogue restoring all regisers except EAX.
;
%macro SAVE_ALL_EPILOGUE 0
%ifdef RT_ARCH_AMD64
        lea     rsp, [rbp - 14 * 8]
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     r11
        pop     r10
        pop     r9
        pop     r8
%else
        lea     esp, [ebp - 6 * 4]
%endif
        pop     xDI
        pop     xSI
        pop     xDX
        pop     xCX
        pop     xBX
        popf
        leave
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


;;
; Loads all general, MMX and SSE registers except xBP and xSP with unique values.
;
x861_LoadUniqueRegValuesSSE:
        fninit
        movq    mm0, [REF(._mm0)]
        movq    mm1, [REF(._mm1)]
        movq    mm2, [REF(._mm2)]
        movq    mm3, [REF(._mm3)]
        movq    mm4, [REF(._mm4)]
        movq    mm5, [REF(._mm5)]
        movq    mm6, [REF(._mm6)]
        movq    mm7, [REF(._mm7)]
        movdqu  xmm0, [REF(._xmm0)]
        movdqu  xmm1, [REF(._xmm1)]
        movdqu  xmm2, [REF(._xmm2)]
        movdqu  xmm3, [REF(._xmm3)]
        movdqu  xmm4, [REF(._xmm4)]
        movdqu  xmm5, [REF(._xmm5)]
        movdqu  xmm6, [REF(._xmm6)]
        movdqu  xmm7, [REF(._xmm7)]
%ifdef RT_ARCH_AMD64
        movdqu  xmm8,  [REF(._xmm8)]
        movdqu  xmm9,  [REF(._xmm9)]
        movdqu  xmm10, [REF(._xmm10)]
        movdqu  xmm11, [REF(._xmm11)]
        movdqu  xmm12, [REF(._xmm12)]
        movdqu  xmm13, [REF(._xmm13)]
        movdqu  xmm14, [REF(._xmm14)]
        movdqu  xmm15, [REF(._xmm15)]
%endif
        call    x861_LoadUniqueRegValues
        ret
._mm0:   times 8  db 040h
._mm1:   times 8  db 041h
._mm2:   times 8  db 042h
._mm3:   times 8  db 043h
._mm4:   times 8  db 044h
._mm5:   times 8  db 045h
._mm6:   times 8  db 046h
._mm7:   times 8  db 047h
._xmm0:  times 16 db 080h
._xmm1:  times 16 db 081h
._xmm2:  times 16 db 082h
._xmm3:  times 16 db 083h
._xmm4:  times 16 db 084h
._xmm5:  times 16 db 085h
._xmm6:  times 16 db 086h
._xmm7:  times 16 db 087h
%ifdef RT_ARCH_AMD64
._xmm8:  times 16 db 088h
._xmm9:  times 16 db 089h
._xmm10: times 16 db 08ah
._xmm11: times 16 db 08bh
._xmm12: times 16 db 08ch
._xmm13: times 16 db 08dh
._xmm14: times 16 db 08eh
._xmm15: times 16 db 08fh
%endif
; end x861_LoadUniqueRegValuesSSE


;;
; Clears all MMX and SSE registers.
;
x861_ClearRegistersSSE:
        fninit
        movq    mm0,   [REF(.zero)]
        movq    mm1,   [REF(.zero)]
        movq    mm2,   [REF(.zero)]
        movq    mm3,   [REF(.zero)]
        movq    mm4,   [REF(.zero)]
        movq    mm5,   [REF(.zero)]
        movq    mm6,   [REF(.zero)]
        movq    mm7,   [REF(.zero)]
        movdqu  xmm0,  [REF(.zero)]
        movdqu  xmm1,  [REF(.zero)]
        movdqu  xmm2,  [REF(.zero)]
        movdqu  xmm3,  [REF(.zero)]
        movdqu  xmm4,  [REF(.zero)]
        movdqu  xmm5,  [REF(.zero)]
        movdqu  xmm6,  [REF(.zero)]
        movdqu  xmm7,  [REF(.zero)]
%ifdef RT_ARCH_AMD64
        movdqu  xmm8,  [REF(.zero)]
        movdqu  xmm9,  [REF(.zero)]
        movdqu  xmm10, [REF(.zero)]
        movdqu  xmm11, [REF(.zero)]
        movdqu  xmm12, [REF(.zero)]
        movdqu  xmm13, [REF(.zero)]
        movdqu  xmm14, [REF(.zero)]
        movdqu  xmm15, [REF(.zero)]
%endif
        ret

        ret
.zero   times 16 db 000h
; x861_ClearRegistersSSE


;;
; Loads all general, MMX and SSE registers except xBP and xSP with unique values.
;
x861_LoadUniqueRegValuesSSEAndGRegs:
        call    x861_LoadUniqueRegValuesSSE
        call    x861_LoadUniqueRegValues
        ret

;;
; Clears all general, MMX and SSE registers except xBP and xSP.
;
x861_ClearRegistersSSEAndGRegs:
        call    x861_ClearRegistersSSE
        call    x861_ClearRegisters
        ret

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
        mov     xDI, [REF_EXTERN(g_pbEfPage)]
        lea     xDI, [xDI + 0x1000 - 2]
        mov     xDX, es
        mov     [xDI], dx
        mov     es, [xDI]               ; should not crash

        ; Saving is always a word access.
        mov     eax, __LINE__
        mov     xDI, [REF_EXTERN(g_pbEfPage)]
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
        mov     xDI, [REF_EXTERN(g_pbEfPage)]
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
        lea     xDI, [REF(g_szAlpha)]
        xor     eax, eax                ; find the end
        mov     ecx, g_cchAlpha + 1
        repne scasb
        cmp     ecx, 1
        mov     eax, __LINE__
        jne     .failed

        ; check that repe scasb (al=0) behaves like expected.
        lea     xDI, [REF(g_szAlpha)]
        xor     eax, eax                ; find the end
        mov     ecx, g_cchAlpha + 1
        repe scasb
        cmp     ecx, g_cchAlpha
        mov     eax, __LINE__
        jne     .failed

        ; repne is last, it wins.
        lea     xDI, [REF(g_szAlpha)]
        xor     eax, eax                ; find the end
        mov     ecx, g_cchAlpha + 1
        db 0f3h                         ; repe  - ignored
        db 0f2h                         ; repne
        scasb
        cmp     ecx, 1
        mov     eax, __LINE__
        jne     .failed

        ; repe is last, it wins.
        lea     xDI, [REF(g_szAlpha)]
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
        mov     xDI, [REF_EXTERN(g_pbEfPage)]
        xor     eax, eax
        mov     ecx, 01000h
        rep stosb

        mov     xDI, [REF_EXTERN(g_pbEfPage)]
        mov     ecx, 4
        mov     eax, 0ffh
        db 0f2h                         ; repne
        stosb
        mov     eax, __LINE__
        cmp     ecx, 0
        jne     .failed
        mov     eax, __LINE__
        mov     xDI, [REF_EXTERN(g_pbEfPage)]
        cmp     dword [xDI], 0ffffffffh
        jne     .failed
        cmp     dword [xDI+4], 0
        jne     .failed

        mov     xDI, [REF_EXTERN(g_pbEfPage)]
        mov     ecx, 4
        mov     eax, 0feh
        db 0f3h                         ; repe
        stosb
        mov     eax, __LINE__
        cmp     ecx, 0
        jne     .failed
        mov     eax, __LINE__
        mov     xDI, [REF_EXTERN(g_pbEfPage)]
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
        mov     xDI, [REF_EXTERN(g_pbEfPage)]
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
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        add     xDI, 1000h - 8h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        mov     byte [xDI+2], 08fh
        mov     dword [xDI+3], 000000000h
        mov     byte [xDI+7], 0cch
        ShouldTrap X86_XCPT_UD, call xDI

        mov     eax, __LINE__
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        add     xDI, 1000h - 7h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        mov     byte [xDI+2], 08Fh
        mov     dword [xDI+3], 000000000h
        ShouldTrap X86_XCPT_UD, call xDI

        mov     eax, __LINE__
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        add     xDI, 1000h - 4h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        mov     byte [xDI+2], 08Fh
        mov     byte [xDI+3], 000h
        ShouldTrap X86_XCPT_PF, call xDI

        mov     eax, __LINE__
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        add     xDI, 1000h - 6h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        mov     byte [xDI+2], 08Fh
        mov     byte [xDI+3], 00h
        mov     byte [xDI+4], 00h
        mov     byte [xDI+5], 00h
        ShouldTrap X86_XCPT_PF, call xDI

        mov     eax, __LINE__
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        add     xDI, 1000h - 5h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        mov     byte [xDI+2], 08Fh
        mov     byte [xDI+3], 00h
        mov     byte [xDI+4], 00h
        ShouldTrap X86_XCPT_PF, call xDI

        mov     eax, __LINE__
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        add     xDI, 1000h - 4h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        mov     byte [xDI+2], 08Fh
        mov     byte [xDI+3], 00h
        ShouldTrap X86_XCPT_PF, call xDI

        mov     eax, __LINE__
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        add     xDI, 1000h - 3h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        mov     byte [xDI+2], 08Fh
        ShouldTrap X86_XCPT_PF, call xDI

        mov     eax, __LINE__
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        add     xDI, 1000h - 2h
        mov     byte [xDI+0], 0f0h
        mov     byte [xDI+1], 002h
        ShouldTrap X86_XCPT_PF, call xDI

        mov     eax, __LINE__
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
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
; Tests the effect of prefix order in group 14.
;
BEGINPROC   x861_Test2
        SAVE_ALL_PROLOGUE

        ; Check testcase preconditions.
        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db       00Fh, 073h, 0D0h, 080h  ;    psrlq   mm0, 128
        call    .check_mm0_zero_and_xmm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 00Fh, 073h, 0D0h, 080h  ;    psrlq   xmm0, 128
        call    .check_xmm0_zero_and_mm0_nz


        ;
        ; Real test - Inject other prefixes before the 066h and see what
        ;             happens.
        ;

        ; General checks that order does not matter, etc.
        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 026h, 066h, 00Fh, 073h, 0D0h, 080h
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 026h, 00Fh, 073h, 0D0h, 080h
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 067h, 00Fh, 073h, 0D0h, 080h
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 067h, 066h, 00Fh, 073h, 0D0h, 080h
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 067h, 066h, 065h, 00Fh, 073h, 0D0h, 080h
        call    .check_xmm0_zero_and_mm0_nz

%ifdef RT_ARCH_AMD64
        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 048h, 066h, 00Fh, 073h, 0D0h, 080h ; REX.W
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 044h, 066h, 00Fh, 073h, 0D0h, 080h ; REX.R
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 042h, 066h, 00Fh, 073h, 0D0h, 080h ; REX.X
        call    .check_xmm0_zero_and_mm0_nz

        ; Actually for REX, order does matter if the prefix is used.
        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 041h, 066h, 00Fh, 073h, 0D0h, 080h ; REX.B
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 041h, 00Fh, 073h, 0D0h, 080h ; REX.B
        call    .check_xmm8_zero_and_xmm0_nz
%endif

        ; Check all ignored prefixes (repeates some of the above).
        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 026h, 00Fh, 073h, 0D0h, 080h ; es
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 065h, 00Fh, 073h, 0D0h, 080h ; gs
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 064h, 00Fh, 073h, 0D0h, 080h ; fs
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 02eh, 00Fh, 073h, 0D0h, 080h ; cs
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 036h, 00Fh, 073h, 0D0h, 080h ; ss
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 03eh, 00Fh, 073h, 0D0h, 080h ; ds
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 067h, 00Fh, 073h, 0D0h, 080h ; addr size
        call    .check_xmm0_zero_and_mm0_nz

%ifdef RT_ARCH_AMD64
        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 048h, 00Fh, 073h, 0D0h, 080h ; REX.W
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 044h, 00Fh, 073h, 0D0h, 080h ; REX.R
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 042h, 00Fh, 073h, 0D0h, 080h ; REX.X
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 041h, 00Fh, 073h, 0D0h, 080h ; REX.B - has actual effect on the instruction.
        call    .check_xmm8_zero_and_xmm0_nz
%endif

        ; Repeated prefix until we hit the max opcode limit.
        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 066h, 00Fh, 073h, 0D0h, 080h
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 066h, 066h, 00Fh, 073h, 0D0h, 080h
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 066h, 066h, 066h, 066h, 066h, 066h, 066h, 00Fh, 073h, 0D0h, 080h
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 066h, 066h, 066h, 066h, 066h, 066h, 066h, 066h, 066h, 066h, 00Fh, 073h, 0D0h, 080h
        call    .check_xmm0_zero_and_mm0_nz

        ShouldTrap X86_XCPT_GP, db 066h, 066h, 066h, 066h, 066h, 066h, 066h, 066h, 066h, 066h, 066h, 066h, 00Fh, 073h, 0D0h, 080h

%ifdef RT_ARCH_AMD64
        ; Repeated REX is parsed, but only the last byte matters.
        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 041h, 048h, 00Fh, 073h, 0D0h, 080h ; REX.B, REX.W
        call    .check_xmm0_zero_and_mm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 048h, 041h, 00Fh, 073h, 0D0h, 080h ; REX.B, REX.W
        call    .check_xmm8_zero_and_xmm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 048h, 044h, 042h, 048h, 044h, 042h, 048h, 044h, 042h, 041h, 00Fh, 073h, 0D0h, 080h
        call    .check_xmm8_zero_and_xmm0_nz

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     eax, __LINE__
        db 066h, 041h, 041h, 041h, 041h, 041h, 041h, 041h, 041h, 041h, 04eh, 00Fh, 073h, 0D0h, 080h
        call    .check_xmm0_zero_and_mm0_nz
%endif

        ; Undefined sequences with prefixes that counts.
        ShouldTrap X86_XCPT_UD, db 0f0h, 066h, 00Fh, 073h, 0D0h, 080h ; LOCK
        ShouldTrap X86_XCPT_UD, db 0f2h, 066h, 00Fh, 073h, 0D0h, 080h ; REPNZ
        ShouldTrap X86_XCPT_UD, db 0f3h, 066h, 00Fh, 073h, 0D0h, 080h ; REPZ
        ShouldTrap X86_XCPT_UD, db 066h, 0f2h, 00Fh, 073h, 0D0h, 080h
        ShouldTrap X86_XCPT_UD, db 066h, 0f3h, 00Fh, 073h, 0D0h, 080h
        ShouldTrap X86_XCPT_UD, db 066h, 0f3h, 0f2h, 00Fh, 073h, 0D0h, 080h
        ShouldTrap X86_XCPT_UD, db 066h, 0f2h, 0f3h, 00Fh, 073h, 0D0h, 080h
        ShouldTrap X86_XCPT_UD, db 0f2h, 066h, 0f3h, 00Fh, 073h, 0D0h, 080h
        ShouldTrap X86_XCPT_UD, db 0f3h, 066h, 0f2h, 00Fh, 073h, 0D0h, 080h
        ShouldTrap X86_XCPT_UD, db 0f3h, 0f2h, 066h, 00Fh, 073h, 0D0h, 080h
        ShouldTrap X86_XCPT_UD, db 0f2h, 0f3h, 066h, 00Fh, 073h, 0D0h, 080h
        ShouldTrap X86_XCPT_UD, db 0f0h, 0f2h, 066h, 0f3h, 00Fh, 073h, 0D0h, 080h
        ShouldTrap X86_XCPT_UD, db 0f0h, 0f3h, 066h, 0f2h, 00Fh, 073h, 0D0h, 080h
        ShouldTrap X86_XCPT_UD, db 0f0h, 0f3h, 0f2h, 066h, 00Fh, 073h, 0D0h, 080h
        ShouldTrap X86_XCPT_UD, db 0f0h, 0f2h, 0f3h, 066h, 00Fh, 073h, 0D0h, 080h

.success:
        xor     eax, eax
.return:
        SAVE_ALL_EPILOGUE
        ret

.check_xmm0_zero_and_mm0_nz:
        sub     xSP, 20h
        movdqu  [xSP], xmm0
        cmp     dword [xSP], 0
        jne     .failed3
        cmp     dword [xSP + 4], 0
        jne     .failed3
        cmp     dword [xSP + 8], 0
        jne     .failed3
        cmp     dword [xSP + 12], 0
        jne     .failed3
        movq    [xSP], mm0
        cmp     dword [xSP], 0
        je      .failed3
        cmp     dword [xSP + 4], 0
        je      .failed3
        add     xSP, 20h
        ret

.check_mm0_zero_and_xmm0_nz:
        sub     xSP, 20h
        movq    [xSP], mm0
        cmp     dword [xSP], 0
        jne     .failed3
        cmp     dword [xSP + 4], 0
        jne     .failed3
        movdqu  [xSP], xmm0
        cmp     dword [xSP], 0
        je      .failed3
        cmp     dword [xSP + 4], 0
        je      .failed3
        cmp     dword [xSP + 8], 0
        je      .failed3
        cmp     dword [xSP + 12], 0
        je      .failed3
        add     xSP, 20h
        ret

%ifdef RT_ARCH_AMD64
.check_xmm8_zero_and_xmm0_nz:
        sub     xSP, 20h
        movdqu  [xSP], xmm8
        cmp     dword [xSP], 0
        jne     .failed3
        cmp     dword [xSP + 4], 0
        jne     .failed3
        cmp     dword [xSP + 8], 0
        jne     .failed3
        cmp     dword [xSP + 12], 0
        jne     .failed3
        movdqu  [xSP], xmm0
        cmp     dword [xSP], 0
        je      .failed3
        cmp     dword [xSP + 4], 0
        je      .failed3
        cmp     dword [xSP + 8], 0
        je      .failed3
        cmp     dword [xSP + 12], 0
        je      .failed3
        add     xSP, 20h
        ret
%endif

.failed3:
        add     xSP, 20h + xS
        jmp     .return


ENDPROC     x861_Test2


;;
; Tests how much fxsave and fxrstor actually accesses of their 512 memory
; operand.
;
BEGINPROC   x861_Test3
        SAVE_ALL_PROLOGUE
        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]

        ; Check testcase preconditions.
        fxsave   [xDI]
        fxrstor  [xDI]

        add     xDI, PAGE_SIZE - 512
        mov     xSI, xDI
        fxsave  [xDI]
        fxrstor [xDI]

        ; 464:511 are available to software use.  Check that they are left
        ; untouched by fxsave.
        mov     eax, 0aabbccddh
        mov     ecx, 512 / 4
        cld
        rep stosd
        mov     xDI, xSI
        fxsave  [xDI]

        mov     ebx, 512
.chech_software_area_loop:
        cmp     [xDI + xBX - 4], eax
        jne     .chech_software_area_done
        sub     ebx, 4
        jmp     .chech_software_area_loop
.chech_software_area_done:
        cmp     ebx, 464
        mov     eax, __LINE__
        ja      .return

        ; Check that a save + restore + save cycle yield the same results.
        mov     xBX, [REF_EXTERN(g_pbEfExecPage)]
        mov     xDI, xBX
        mov     eax, 066778899h
        mov     ecx, 512 * 2 / 4
        cld
        rep stosd
        fxsave  [xBX]

        call    x861_ClearRegistersSSEAndGRegs
        mov     xBX, [REF_EXTERN(g_pbEfExecPage)]
        fxrstor [xBX]

        fxsave  [xBX + 512]
        mov     xSI, xBX
        lea     xDI, [xBX + 512]
        mov     ecx, 512
        cld
        repe cmpsb
        mov     eax, __LINE__
        jnz     .return


        ; 464:511 are available to software use.  Let see how carefully access
        ; to the full 512 bytes are checked...
        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        add     xDI, PAGE_SIZE - 512
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 16]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 32]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 48]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 64]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 80]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 96]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 128]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 144]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 160]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 176]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 192]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 208]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 224]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 240]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 256]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 384]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 432]
        ShouldTrap X86_XCPT_PF, fxsave  [xDI + 496]

        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 16]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 32]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 48]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 64]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 80]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 96]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 128]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 144]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 160]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 176]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 192]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 208]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 224]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 240]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 256]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 384]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 432]
        ShouldTrap X86_XCPT_PF, fxrstor [xDI + 496]

        ; Unaligned accesses will cause #GP(0). This takes precedence over #PF.
        ShouldTrap X86_XCPT_GP, fxsave  [xDI + 1]
        ShouldTrap X86_XCPT_GP, fxsave  [xDI + 2]
        ShouldTrap X86_XCPT_GP, fxsave  [xDI + 3]
        ShouldTrap X86_XCPT_GP, fxsave  [xDI + 4]
        ShouldTrap X86_XCPT_GP, fxsave  [xDI + 5]
        ShouldTrap X86_XCPT_GP, fxsave  [xDI + 6]
        ShouldTrap X86_XCPT_GP, fxsave  [xDI + 7]
        ShouldTrap X86_XCPT_GP, fxsave  [xDI + 8]
        ShouldTrap X86_XCPT_GP, fxsave  [xDI + 9]
        ShouldTrap X86_XCPT_GP, fxsave  [xDI + 10]
        ShouldTrap X86_XCPT_GP, fxsave  [xDI + 11]
        ShouldTrap X86_XCPT_GP, fxsave  [xDI + 12]
        ShouldTrap X86_XCPT_GP, fxsave  [xDI + 13]
        ShouldTrap X86_XCPT_GP, fxsave  [xDI + 14]
        ShouldTrap X86_XCPT_GP, fxsave  [xDI + 15]

        ShouldTrap X86_XCPT_GP, fxrstor [xDI + 1]
        ShouldTrap X86_XCPT_GP, fxrstor [xDI + 2]
        ShouldTrap X86_XCPT_GP, fxrstor [xDI + 3]
        ShouldTrap X86_XCPT_GP, fxrstor [xDI + 4]
        ShouldTrap X86_XCPT_GP, fxrstor [xDI + 5]
        ShouldTrap X86_XCPT_GP, fxrstor [xDI + 6]
        ShouldTrap X86_XCPT_GP, fxrstor [xDI + 7]
        ShouldTrap X86_XCPT_GP, fxrstor [xDI + 8]
        ShouldTrap X86_XCPT_GP, fxrstor [xDI + 9]
        ShouldTrap X86_XCPT_GP, fxrstor [xDI + 10]
        ShouldTrap X86_XCPT_GP, fxrstor [xDI + 11]
        ShouldTrap X86_XCPT_GP, fxrstor [xDI + 12]
        ShouldTrap X86_XCPT_GP, fxrstor [xDI + 13]
        ShouldTrap X86_XCPT_GP, fxrstor [xDI + 14]
        ShouldTrap X86_XCPT_GP, fxrstor [xDI + 15]

        ; Lets check what a FP in fxsave changes ... nothing on intel.
        mov     ebx, 16
.fxsave_pf_effect_loop:
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        add     xDI, PAGE_SIZE - 512 * 2
        mov     xSI, xDI
        mov     eax, 066778899h
        mov     ecx, 512 * 2 / 4
        cld
        rep stosd

        ShouldTrap X86_XCPT_PF, fxsave  [xSI + PAGE_SIZE - 512 + xBX]

        mov     ecx, 512 / 4
        lea     xDI, [xSI + 512]
        cld
        repz cmpsd
        lea     xAX, [xBX + 20000]
        jnz     .return

        add     ebx, 16
        cmp     ebx, 512
        jbe     .fxsave_pf_effect_loop

        ; Lets check that a FP in fxrstor does not have any effect on the FPU or SSE state.
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        mov     ecx, PAGE_SIZE / 4
        mov     eax, 0ffaa33cch
        cld
        rep stosd

        call    x861_LoadUniqueRegValuesSSEAndGRegs
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        fxsave  [xDI]

        call    x861_ClearRegistersSSEAndGRegs
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        fxsave  [xDI + 512]

        mov     ebx, 16
.fxrstor_pf_effect_loop:
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        mov     xSI, xDI
        lea     xDI, [xDI + PAGE_SIZE - 512 + xBX]
        mov     ecx, 512
        sub     ecx, ebx
        cld
        rep movsb                       ; copy unique state to end of page.

        push    xBX
        call    x861_ClearRegistersSSEAndGRegs
        pop     xBX
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        ShouldTrap X86_XCPT_PF, fxrstor  [xDI + PAGE_SIZE - 512 + xBX] ; try load unique state

        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        lea     xSI, [xDI + 512]        ; point it to the clean state, which is what we expect.
        lea     xDI, [xDI + 1024]
        fxsave  [xDI]                   ; save whatever the fpu state currently is.
        mov     ecx, 512 / 4
        cld
        repe cmpsd
        lea     xAX, [xBX + 40000]
        jnz     .return                 ; it shouldn't be modified by faulting fxrstor, i.e. a clean state.

        add     ebx, 16
        cmp     ebx, 512
        jbe     .fxrstor_pf_effect_loop

.success:
        xor     eax, eax
.return:
        SAVE_ALL_EPILOGUE
        ret
ENDPROC     x861_Test3


;;
; Tests various multibyte NOP sequences.
;
BEGINPROC   x861_Test4
        SAVE_ALL_PROLOGUE
        call    x861_ClearRegisters

        ; Intel recommended sequences.
        nop
        db 066h, 090h
        db 00fh, 01fh, 000h
        db 00fh, 01fh, 040h, 000h
        db 00fh, 01fh, 044h, 000h, 000h
        db 066h, 00fh, 01fh, 044h, 000h, 000h
        db 00fh, 01fh, 080h, 000h, 000h, 000h, 000h
        db 00fh, 01fh, 084h, 000h, 000h, 000h, 000h, 000h
        db 066h, 00fh, 01fh, 084h, 000h, 000h, 000h, 000h, 000h

        ; Check that the NOPs are allergic to lock prefixing.
        ShouldTrap X86_XCPT_UD, db 0f0h, 090h               ; lock prefixed NOP.
        ShouldTrap X86_XCPT_UD, db 0f0h, 066h, 090h         ; lock prefixed two byte NOP.
        ShouldTrap X86_XCPT_UD, db 0f0h, 00fh, 01fh, 000h   ; lock prefixed three byte NOP.

        ; Check the range of instructions that AMD marks as NOPs.
%macro TST_NOP 1
        db 00fh, %1, 000h
        db 00fh, %1, 040h, 000h
        db 00fh, %1, 044h, 000h, 000h
        db 066h, 00fh, %1, 044h, 000h, 000h
        db 00fh, %1, 080h, 000h, 000h, 000h, 000h
        db 00fh, %1, 084h, 000h, 000h, 000h, 000h, 000h
        db 066h, 00fh, %1, 084h, 000h, 000h, 000h, 000h, 000h
        ShouldTrap X86_XCPT_UD, db 0f0h, 00fh, %1, 000h
%endmacro
        TST_NOP 019h
        TST_NOP 01ah
        TST_NOP 01bh
        TST_NOP 01ch
        TST_NOP 01dh
        TST_NOP 01eh
        TST_NOP 01fh

        ; The AMD P group, intel marks this as a NOP.
        TST_NOP 00dh

.success:
        xor     eax, eax
.return:
        SAVE_ALL_EPILOGUE
        ret
ENDPROC     x861_Test4


;;
; Tests an reserved FPU encoding, checking that it does not affect the FPU or
; CPU state in any way.
;
; @uses stack
%macro FpuNopEncoding 1+
        fnclex
        call    SetFSW_C0_thru_C3

        push    xBP
        mov     xBP, xSP
        sub     xSP, 1024
        and     xSP, ~0fh
        call    SaveFPUAndGRegsToStack
        %1
        call    CompareFPUAndGRegsOnStackIgnoreOpAndIp
        leave

        jz      %%ok
        add     eax, __LINE__
        jmp     .return
%%ok:
%endmacro

;;
; Used for marking encodings which has a meaning other than FNOP and
; needs investigating.
%macro FpuReservedEncoding 2
        fnclex
        call    SetFSW_C0_thru_C3

        push    xBP
        mov     xBP, xSP
        sub     xSP, 2048
        and     xSP, ~0fh
        arch_fxsave [xSP + 1024]
        %1
        call    SaveFPUAndGRegsToStack

        arch_fxrstor [xSP + 1024]
        %2
        call    CompareFPUAndGRegsOnStackIgnoreOpAndIp
        ;arch_fxrstor [xSP + 1024]
        leave

        jz      %%ok
        add     eax, __LINE__
        jmp     .return
%%ok:
%endmacro


;;
; Saves the FPU and general registers to the stack area right next to the
; return address.
;
; The required area size is 512 + 80h = 640.
;
; @uses Nothing, except stack.
;
SaveFPUAndGRegsToStack:
        ; Must clear the FXSAVE area.
        pushf
        push    xCX
        push    xAX
        push    xDI

        lea     xDI, [xSP + xS * 5]
        mov     xCX, 512 / 4
        mov     eax, 0cccccccch
        cld
        rep stosd

        pop     xDI
        pop     xAX
        pop     xCX
        popf

        ; Save the FPU state.
        arch_fxsave [xSP + xS]

        ; Save GRegs (80h bytes).
%ifdef RT_ARCH_AMD64
        mov     [xSP + 512 + xS + 000h], xAX
        mov     [xSP + 512 + xS + 008h], xBX
        mov     [xSP + 512 + xS + 010h], xCX
        mov     [xSP + 512 + xS + 018h], xDX
        mov     [xSP + 512 + xS + 020h], xDI
        mov     [xSP + 512 + xS + 028h], xSI
        mov     [xSP + 512 + xS + 030h], xBP
        mov     [xSP + 512 + xS + 038h], r8
        mov     [xSP + 512 + xS + 040h], r9
        mov     [xSP + 512 + xS + 048h], r10
        mov     [xSP + 512 + xS + 050h], r11
        mov     [xSP + 512 + xS + 058h], r12
        mov     [xSP + 512 + xS + 060h], r13
        mov     [xSP + 512 + xS + 068h], r14
        mov     [xSP + 512 + xS + 070h], r15
        pushf
        pop     rax
        mov     [xSP + 512 + xS + 078h], rax
        mov     rax, [xSP + 512 + xS + 000h]
%else
        mov     [xSP + 512 + xS + 000h], eax
        mov     [xSP + 512 + xS + 004h], eax
        mov     [xSP + 512 + xS + 008h], ebx
        mov     [xSP + 512 + xS + 00ch], ebx
        mov     [xSP + 512 + xS + 010h], ecx
        mov     [xSP + 512 + xS + 014h], ecx
        mov     [xSP + 512 + xS + 018h], edx
        mov     [xSP + 512 + xS + 01ch], edx
        mov     [xSP + 512 + xS + 020h], edi
        mov     [xSP + 512 + xS + 024h], edi
        mov     [xSP + 512 + xS + 028h], esi
        mov     [xSP + 512 + xS + 02ch], esi
        mov     [xSP + 512 + xS + 030h], ebp
        mov     [xSP + 512 + xS + 034h], ebp
        mov     [xSP + 512 + xS + 038h], eax
        mov     [xSP + 512 + xS + 03ch], eax
        mov     [xSP + 512 + xS + 040h], eax
        mov     [xSP + 512 + xS + 044h], eax
        mov     [xSP + 512 + xS + 048h], eax
        mov     [xSP + 512 + xS + 04ch], eax
        mov     [xSP + 512 + xS + 050h], eax
        mov     [xSP + 512 + xS + 054h], eax
        mov     [xSP + 512 + xS + 058h], eax
        mov     [xSP + 512 + xS + 05ch], eax
        mov     [xSP + 512 + xS + 060h], eax
        mov     [xSP + 512 + xS + 064h], eax
        mov     [xSP + 512 + xS + 068h], eax
        mov     [xSP + 512 + xS + 06ch], eax
        mov     [xSP + 512 + xS + 070h], eax
        mov     [xSP + 512 + xS + 074h], eax
        pushf
        pop     eax
        mov     [xSP + 512 + xS + 078h], eax
        mov     [xSP + 512 + xS + 07ch], eax
        mov     eax, [xSP + 512 + xS + 000h]
%endif
        ret

;;
; Compares the current FPU and general registers to that found in the stack
; area prior to the return address.
;
; @uses     Stack, flags and eax/rax.
; @returns  eax is zero on success, eax is 1000000 * offset on failure.
;           ZF reflects the eax value to save a couple of instructions...
;
CompareFPUAndGRegsOnStack:
        lea     xSP, [xSP - (1024 - xS)]
        call    SaveFPUAndGRegsToStack

        push    xSI
        push    xDI
        push    xCX

        mov     xCX, 640
        lea     xSI, [xSP + xS*3]
        lea     xDI, [xSI + 1024]

        cld
        repe cmpsb
        je      .ok

        ;int3
        lea     xAX, [xSP + xS*3]
        xchg    xAX, xSI
        sub     xAX, xSI

        push    xDX
        mov     xDX, 1000000
        mul     xDX
        pop     xDX
        jmp     .return
.ok:
        xor     eax, eax
.return:
        pop     xCX
        pop     xDI
        pop     xSI
        lea     xSP, [xSP + (1024 - xS)]
        or      eax, eax
        ret

;;
; Same as CompareFPUAndGRegsOnStack, except that it ignores the FOP and FPUIP
; registers.
;
; @uses     Stack, flags and eax/rax.
; @returns  eax is zero on success, eax is 1000000 * offset on failure.
;           ZF reflects the eax value to save a couple of instructions...
;
CompareFPUAndGRegsOnStackIgnoreOpAndIp:
        lea     xSP, [xSP - (1024 - xS)]
        call    SaveFPUAndGRegsToStack

        push    xSI
        push    xDI
        push    xCX

        mov     xCX, 640
        lea     xSI, [xSP + xS*3]
        lea     xDI, [xSI + 1024]

        mov     word [xSI + X86FXSTATE.FOP], 0          ; ignore
        mov     word [xDI + X86FXSTATE.FOP], 0          ; ignore
        mov     dword [xSI + X86FXSTATE.FPUIP], 0       ; ignore
        mov     dword [xDI + X86FXSTATE.FPUIP], 0       ; ignore

        cld
        repe cmpsb
        je      .ok

        ;int3
        lea     xAX, [xSP + xS*3]
        xchg    xAX, xSI
        sub     xAX, xSI

        push    xDX
        mov     xDX, 1000000
        mul     xDX
        pop     xDX
        jmp     .return
.ok:
        xor     eax, eax
.return:
        pop     xCX
        pop     xDI
        pop     xSI
        lea     xSP, [xSP + (1024 - xS)]
        or      eax, eax
        ret


SetFSW_C0_thru_C3:
        sub     xSP, 20h
        fstenv  [xSP]
        or      word [xSP + 4], X86_FSW_C0 | X86_FSW_C1 | X86_FSW_C2 | X86_FSW_C3
        fldenv  [xSP]
        add     xSP, 20h
        ret


;;
; Tests some odd floating point instruction encodings.
;
BEGINPROC   x861_Test5
        SAVE_ALL_PROLOGUE

        ; standard stuff...
        fld dword [REF(g_r32V1)]
        fld qword [REF(g_r64V1)]
        fld tword [REF(g_r80V1)]
        fld qword [REF(g_r64V1)]
        fld dword [REF(g_r32V2)]
        fld dword [REF(g_r32V1)]

        ; Test the nop check.
        FpuNopEncoding fnop


        ; the 0xd9 block
        ShouldTrap X86_XCPT_UD, db 0d9h, 008h
        ShouldTrap X86_XCPT_UD, db 0d9h, 009h
        ShouldTrap X86_XCPT_UD, db 0d9h, 00ah
        ShouldTrap X86_XCPT_UD, db 0d9h, 00bh
        ShouldTrap X86_XCPT_UD, db 0d9h, 00ch
        ShouldTrap X86_XCPT_UD, db 0d9h, 00dh
        ShouldTrap X86_XCPT_UD, db 0d9h, 00eh
        ShouldTrap X86_XCPT_UD, db 0d9h, 00fh

        ShouldTrap X86_XCPT_UD, db 0d9h, 0d1h
        ShouldTrap X86_XCPT_UD, db 0d9h, 0d2h
        ShouldTrap X86_XCPT_UD, db 0d9h, 0d3h
        ShouldTrap X86_XCPT_UD, db 0d9h, 0d4h
        ShouldTrap X86_XCPT_UD, db 0d9h, 0d5h
        ShouldTrap X86_XCPT_UD, db 0d9h, 0d6h
        ShouldTrap X86_XCPT_UD, db 0d9h, 0d7h
        FpuReservedEncoding {db 0d9h, 0d8h}, { fstp st0 }
        FpuReservedEncoding {db 0d9h, 0d9h}, { fstp st1 }
        FpuReservedEncoding {db 0d9h, 0dah}, { fstp st2 }
        FpuReservedEncoding {db 0d9h, 0dbh}, { fstp st3 }
        FpuReservedEncoding {db 0d9h, 0dch}, { fstp st4 }
        FpuReservedEncoding {db 0d9h, 0ddh}, { fstp st5 }
        FpuReservedEncoding {db 0d9h, 0deh}, { fstp st6 }
        ;FpuReservedEncoding {db 0d9h, 0dfh}, { fstp st7 } ; This variant seems to ignore empty ST(0) values!
        ShouldTrap X86_XCPT_UD, db 0d9h, 0e2h
        ShouldTrap X86_XCPT_UD, db 0d9h, 0e3h
        ShouldTrap X86_XCPT_UD, db 0d9h, 0e6h
        ShouldTrap X86_XCPT_UD, db 0d9h, 0e7h
        ShouldTrap X86_XCPT_UD, db 0d9h, 0efh
        ShouldTrap X86_XCPT_UD, db 0d9h, 008h
        ShouldTrap X86_XCPT_UD, db 0d9h, 00fh

        ; the 0xda block
        ShouldTrap X86_XCPT_UD, db 0dah, 0e0h
        ShouldTrap X86_XCPT_UD, db 0dah, 0e1h
        ShouldTrap X86_XCPT_UD, db 0dah, 0e2h
        ShouldTrap X86_XCPT_UD, db 0dah, 0e3h
        ShouldTrap X86_XCPT_UD, db 0dah, 0e4h
        ShouldTrap X86_XCPT_UD, db 0dah, 0e5h
        ShouldTrap X86_XCPT_UD, db 0dah, 0e6h
        ShouldTrap X86_XCPT_UD, db 0dah, 0e7h
        ShouldTrap X86_XCPT_UD, db 0dah, 0e8h
        ShouldTrap X86_XCPT_UD, db 0dah, 0eah
        ShouldTrap X86_XCPT_UD, db 0dah, 0ebh
        ShouldTrap X86_XCPT_UD, db 0dah, 0ech
        ShouldTrap X86_XCPT_UD, db 0dah, 0edh
        ShouldTrap X86_XCPT_UD, db 0dah, 0eeh
        ShouldTrap X86_XCPT_UD, db 0dah, 0efh
        ShouldTrap X86_XCPT_UD, db 0dah, 0f0h
        ShouldTrap X86_XCPT_UD, db 0dah, 0f1h
        ShouldTrap X86_XCPT_UD, db 0dah, 0f2h
        ShouldTrap X86_XCPT_UD, db 0dah, 0f3h
        ShouldTrap X86_XCPT_UD, db 0dah, 0f4h
        ShouldTrap X86_XCPT_UD, db 0dah, 0f5h
        ShouldTrap X86_XCPT_UD, db 0dah, 0f6h
        ShouldTrap X86_XCPT_UD, db 0dah, 0f7h
        ShouldTrap X86_XCPT_UD, db 0dah, 0f8h
        ShouldTrap X86_XCPT_UD, db 0dah, 0f9h
        ShouldTrap X86_XCPT_UD, db 0dah, 0fah
        ShouldTrap X86_XCPT_UD, db 0dah, 0fbh
        ShouldTrap X86_XCPT_UD, db 0dah, 0fch
        ShouldTrap X86_XCPT_UD, db 0dah, 0fdh
        ShouldTrap X86_XCPT_UD, db 0dah, 0feh
        ShouldTrap X86_XCPT_UD, db 0dah, 0ffh

        ; the 0xdb block
        FpuNopEncoding db 0dbh, 0e0h ; fneni
        FpuNopEncoding db 0dbh, 0e1h ; fndisi
        FpuNopEncoding db 0dbh, 0e4h ; fnsetpm
        ShouldTrap X86_XCPT_UD, db 0dbh, 0e5h
        ShouldTrap X86_XCPT_UD, db 0dbh, 0e6h
        ShouldTrap X86_XCPT_UD, db 0dbh, 0e7h
        ShouldTrap X86_XCPT_UD, db 0dbh, 0f8h
        ShouldTrap X86_XCPT_UD, db 0dbh, 0f9h
        ShouldTrap X86_XCPT_UD, db 0dbh, 0fah
        ShouldTrap X86_XCPT_UD, db 0dbh, 0fbh
        ShouldTrap X86_XCPT_UD, db 0dbh, 0fch
        ShouldTrap X86_XCPT_UD, db 0dbh, 0fdh
        ShouldTrap X86_XCPT_UD, db 0dbh, 0feh
        ShouldTrap X86_XCPT_UD, db 0dbh, 0ffh
        ShouldTrap X86_XCPT_UD, db 0dbh, 020h
        ShouldTrap X86_XCPT_UD, db 0dbh, 023h
        ShouldTrap X86_XCPT_UD, db 0dbh, 030h
        ShouldTrap X86_XCPT_UD, db 0dbh, 032h

        ; the 0xdc block
        FpuReservedEncoding {db 0dch, 0d0h}, { fcom st0 }
        FpuReservedEncoding {db 0dch, 0d1h}, { fcom st1 }
        FpuReservedEncoding {db 0dch, 0d2h}, { fcom st2 }
        FpuReservedEncoding {db 0dch, 0d3h}, { fcom st3 }
        FpuReservedEncoding {db 0dch, 0d4h}, { fcom st4 }
        FpuReservedEncoding {db 0dch, 0d5h}, { fcom st5 }
        FpuReservedEncoding {db 0dch, 0d6h}, { fcom st6 }
        FpuReservedEncoding {db 0dch, 0d7h}, { fcom st7 }
        FpuReservedEncoding {db 0dch, 0d8h}, { fcomp st0 }
        FpuReservedEncoding {db 0dch, 0d9h}, { fcomp st1 }
        FpuReservedEncoding {db 0dch, 0dah}, { fcomp st2 }
        FpuReservedEncoding {db 0dch, 0dbh}, { fcomp st3 }
        FpuReservedEncoding {db 0dch, 0dch}, { fcomp st4 }
        FpuReservedEncoding {db 0dch, 0ddh}, { fcomp st5 }
        FpuReservedEncoding {db 0dch, 0deh}, { fcomp st6 }
        FpuReservedEncoding {db 0dch, 0dfh}, { fcomp st7 }

        ; the 0xdd block
        FpuReservedEncoding {db 0ddh, 0c8h}, { fxch st0 }
        FpuReservedEncoding {db 0ddh, 0c9h}, { fxch st1 }
        FpuReservedEncoding {db 0ddh, 0cah}, { fxch st2 }
        FpuReservedEncoding {db 0ddh, 0cbh}, { fxch st3 }
        FpuReservedEncoding {db 0ddh, 0cch}, { fxch st4 }
        FpuReservedEncoding {db 0ddh, 0cdh}, { fxch st5 }
        FpuReservedEncoding {db 0ddh, 0ceh}, { fxch st6 }
        FpuReservedEncoding {db 0ddh, 0cfh}, { fxch st7 }
        ShouldTrap X86_XCPT_UD, db 0ddh, 0f0h
        ShouldTrap X86_XCPT_UD, db 0ddh, 0f1h
        ShouldTrap X86_XCPT_UD, db 0ddh, 0f2h
        ShouldTrap X86_XCPT_UD, db 0ddh, 0f3h
        ShouldTrap X86_XCPT_UD, db 0ddh, 0f4h
        ShouldTrap X86_XCPT_UD, db 0ddh, 0f5h
        ShouldTrap X86_XCPT_UD, db 0ddh, 0f6h
        ShouldTrap X86_XCPT_UD, db 0ddh, 0f7h
        ShouldTrap X86_XCPT_UD, db 0ddh, 0f8h
        ShouldTrap X86_XCPT_UD, db 0ddh, 0f9h
        ShouldTrap X86_XCPT_UD, db 0ddh, 0fah
        ShouldTrap X86_XCPT_UD, db 0ddh, 0fbh
        ShouldTrap X86_XCPT_UD, db 0ddh, 0fch
        ShouldTrap X86_XCPT_UD, db 0ddh, 0fdh
        ShouldTrap X86_XCPT_UD, db 0ddh, 0feh
        ShouldTrap X86_XCPT_UD, db 0ddh, 0ffh
        ShouldTrap X86_XCPT_UD, db 0ddh, 028h
        ShouldTrap X86_XCPT_UD, db 0ddh, 02fh

        ; the 0xde block
        FpuReservedEncoding {db 0deh, 0d0h}, { fcomp st0 }
        FpuReservedEncoding {db 0deh, 0d1h}, { fcomp st1 }
        FpuReservedEncoding {db 0deh, 0d2h}, { fcomp st2 }
        FpuReservedEncoding {db 0deh, 0d3h}, { fcomp st3 }
        FpuReservedEncoding {db 0deh, 0d4h}, { fcomp st4 }
        FpuReservedEncoding {db 0deh, 0d5h}, { fcomp st5 }
        FpuReservedEncoding {db 0deh, 0d6h}, { fcomp st6 }
        FpuReservedEncoding {db 0deh, 0d7h}, { fcomp st7 }
        ShouldTrap X86_XCPT_UD, db 0deh, 0d8h
        ShouldTrap X86_XCPT_UD, db 0deh, 0dah
        ShouldTrap X86_XCPT_UD, db 0deh, 0dbh
        ShouldTrap X86_XCPT_UD, db 0deh, 0dch
        ShouldTrap X86_XCPT_UD, db 0deh, 0ddh
        ShouldTrap X86_XCPT_UD, db 0deh, 0deh
        ShouldTrap X86_XCPT_UD, db 0deh, 0dfh

        ; the 0xdf block
        FpuReservedEncoding {db 0dfh, 0c8h}, { fxch st0 }
        FpuReservedEncoding {db 0dfh, 0c9h}, { fxch st1 }
        FpuReservedEncoding {db 0dfh, 0cah}, { fxch st2 }
        FpuReservedEncoding {db 0dfh, 0cbh}, { fxch st3 }
        FpuReservedEncoding {db 0dfh, 0cch}, { fxch st4 }
        FpuReservedEncoding {db 0dfh, 0cdh}, { fxch st5 }
        FpuReservedEncoding {db 0dfh, 0ceh}, { fxch st6 }
        FpuReservedEncoding {db 0dfh, 0cfh}, { fxch st7 }
        FpuReservedEncoding {db 0dfh, 0d0h}, { fstp st0 }
        FpuReservedEncoding {db 0dfh, 0d1h}, { fstp st1 }
        FpuReservedEncoding {db 0dfh, 0d2h}, { fstp st2 }
        FpuReservedEncoding {db 0dfh, 0d3h}, { fstp st3 }
        FpuReservedEncoding {db 0dfh, 0d4h}, { fstp st4 }
        FpuReservedEncoding {db 0dfh, 0d5h}, { fstp st5 }
        FpuReservedEncoding {db 0dfh, 0d6h}, { fstp st6 }
        FpuReservedEncoding {db 0dfh, 0d7h}, { fstp st7 }
        FpuReservedEncoding {db 0dfh, 0d8h}, { fstp st0 }
        FpuReservedEncoding {db 0dfh, 0d9h}, { fstp st1 }
        FpuReservedEncoding {db 0dfh, 0dah}, { fstp st2 }
        FpuReservedEncoding {db 0dfh, 0dbh}, { fstp st3 }
        FpuReservedEncoding {db 0dfh, 0dch}, { fstp st4 }
        FpuReservedEncoding {db 0dfh, 0ddh}, { fstp st5 }
        FpuReservedEncoding {db 0dfh, 0deh}, { fstp st6 }
        FpuReservedEncoding {db 0dfh, 0dfh}, { fstp st7 }
        ShouldTrap X86_XCPT_UD, db 0dfh, 0e1h
        ShouldTrap X86_XCPT_UD, db 0dfh, 0e2h
        ShouldTrap X86_XCPT_UD, db 0dfh, 0e3h
        ShouldTrap X86_XCPT_UD, db 0dfh, 0e4h
        ShouldTrap X86_XCPT_UD, db 0dfh, 0e5h
        ShouldTrap X86_XCPT_UD, db 0dfh, 0e6h
        ShouldTrap X86_XCPT_UD, db 0dfh, 0e7h
        ShouldTrap X86_XCPT_UD, db 0dfh, 0f8h
        ShouldTrap X86_XCPT_UD, db 0dfh, 0f9h
        ShouldTrap X86_XCPT_UD, db 0dfh, 0fah
        ShouldTrap X86_XCPT_UD, db 0dfh, 0fbh
        ShouldTrap X86_XCPT_UD, db 0dfh, 0fch
        ShouldTrap X86_XCPT_UD, db 0dfh, 0fdh
        ShouldTrap X86_XCPT_UD, db 0dfh, 0feh
        ShouldTrap X86_XCPT_UD, db 0dfh, 0ffh


.success:
        xor     eax, eax
.return:
        SAVE_ALL_EPILOGUE
        ret

ENDPROC     x861_Test5


;;
; Tests some floating point exceptions and such.
;
;
;
BEGINPROC   x861_Test6
        SAVE_ALL_PROLOGUE
        sub     xSP, 2048

        ; Load some pointers.
        lea     xSI, [REF(g_r32V1)]
        mov     xDI, [REF_EXTERN(g_pbEfExecPage)]
        add     xDI, PAGE_SIZE          ; invalid page.

        ;
        ; Check denormal numbers.
        ; Turns out the number is loaded onto the stack even if an exception is triggered.
        ;
        fninit
        mov     dword [xSP], X86_FCW_PC_64 | X86_FCW_RC_NEAREST
        fldcw   [xSP]
        FpuShouldTrap  X86_FSW_DE, 0, fld dword [REF(g_r32D0)]
        CheckSt0Value 0x00000000, 0x80000000, 0x3f7f

        mov     dword [xSP], X86_FCW_PC_64 | X86_FCW_RC_NEAREST | X86_FCW_DM
        fldcw   [xSP]
        fld     dword [REF(g_r32D0)]
        fwait
        FpuCheckFSW X86_FSW_DE, 0
        CheckSt0Value 0x00000000, 0x80000000, 0x3f7f

        ;
        ; stack overflow
        ;
        fninit
        mov     dword [xSP], X86_FCW_PC_64 | X86_FCW_RC_NEAREST
        fldcw   [xSP]
        fld     qword [REF(g_r64V1)]
        fld     dword [xSI]
        fld     dword [xSI]
        fld     dword [xSI]
        fld     dword [xSI]
        fld     dword [xSI]
        fld     dword [xSI]
        fld     tword [REF(g_r80V1)]
        fwait

        FpuShouldTrap  X86_FSW_IE | X86_FSW_SF | X86_FSW_C1, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3, \
                fld     dword [xSI]
        CheckSt0Value 0x00000000, 0x80000000, 0x4002

        FpuShouldTrap  X86_FSW_IE | X86_FSW_SF | X86_FSW_C1, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3, \
                fld     dword [xSI]
        CheckSt0Value 0x00000000, 0x80000000, 0x4002

        ; stack overflow vs #PF.
        ShouldTrap X86_XCPT_PF, fld     dword [xDI]
        fwait

        ; stack overflow vs denormal number
        FpuShouldTrap  X86_FSW_IE | X86_FSW_SF | X86_FSW_C1, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3, \
                fld     dword [xSI]
        CheckSt0Value 0x00000000, 0x80000000, 0x4002

        ;
        ; Mask the overflow exception. We should get QNaN now regardless of
        ; what we try to push (provided the memory is valid).
        ;
        mov     dword [xSP], X86_FCW_PC_64 | X86_FCW_RC_NEAREST | X86_FCW_IM
        fldcw   [xSP]

        fld     dword [xSI]
        FpuCheckFSW X86_FSW_IE | X86_FSW_SF | X86_FSW_C1, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        fnclex
        CheckSt0Value 0x00000000, 0xc0000000, 0xffff

        fld     qword [REF(g_r64V1)]
        FpuCheckFSW X86_FSW_IE | X86_FSW_SF | X86_FSW_C1, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        fnclex
        CheckSt0Value 0x00000000, 0xc0000000, 0xffff

        ; This is includes denormal values.
        fld     dword [REF(g_r32D0)]
        fwait
        FpuCheckFSW X86_FSW_IE | X86_FSW_SF | X86_FSW_C1, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        CheckSt0Value 0x00000000, 0xc0000000, 0xffff
        fnclex

        ;
        ; #PF vs previous stack overflow. I.e. whether pending FPU exception
        ; is checked before fetching memory operands.
        ;
        mov     dword [xSP], X86_FCW_PC_64 | X86_FCW_RC_NEAREST
        fldcw   [xSP]
        fld     qword [REF(g_r64V1)]
        ShouldTrap X86_XCPT_MF, fld     dword [xDI]
        fnclex

        ;
        ; What happens when we unmask an exception and fwait?
        ;
        mov     dword [xSP], X86_FCW_PC_64 | X86_FCW_RC_NEAREST | X86_FCW_IM
        fldcw   [xSP]
        fld     dword [xSI]
        fwait
        FpuCheckFSW X86_FSW_IE | X86_FSW_SF | X86_FSW_C1, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        mov     dword [xSP], X86_FCW_PC_64 | X86_FCW_RC_NEAREST
        fldcw   [xSP]
        FpuCheckFSW X86_FSW_ES | X86_FSW_B | X86_FSW_IE | X86_FSW_SF | X86_FSW_C1, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3

        ShouldTrap X86_XCPT_MF, fwait
        ShouldTrap X86_XCPT_MF, fwait
        ShouldTrap X86_XCPT_MF, fwait
        fnclex


.success:
        xor     eax, eax
.return:
        add     xSP, 2048
        SAVE_ALL_EPILOGUE
        ret
ENDPROC     x861_Test6


extern NAME(RTTestISub)

;;
; Sets the current subtest.
%macro SetSubTest 1
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
        lea     rdi, [%%s_szName wrt rip]
 %else
        lea     rcx, [%%s_szName wrt rip]
 %endif
        call    NAME(RTTestISub)
%else
        push    %%s_szName
        call    NAME(RTTestISub)
        add     esp, 4
%endif
        jmp     %%done
%%s_szName:
        db %1, 0
%%done:
%endmacro


;;
; Checks the opcode and CS:IP FPU.
;
; @returns ZF=1 on success, ZF=0 on failure.
; @param    xSP + xS    fxsave image followed by fnstenv.
; @param    xCX         Opcode address (no prefixes).
;
CheckOpcodeCsIp:
        push    xBP
        mov     xBP, xSP
        push    xAX

        ; Check the IP.
%ifdef RT_ARCH_AMD64
        cmp     rcx, [xBP + xS*2 + X86FXSTATE.FPUIP]
%else
        cmp     ecx, [xBP + xS*2 + X86FXSTATE.FPUIP]
%endif
        jne     .failure1

        mov     ax, cs
        cmp     ax, [xBP + xS*2 + 512 + X86FSTENV32P.FPUCS]
        jne     .failure2

        ; Check the opcode.  This may be disabled.
        cmp     word [xBP + xS*2 + X86FXSTATE.FOP], 0
        je      .success

        mov     ah, [xCX]
        mov     al, [xCX + 1]
        and     ax, 07ffh
        cmp     ax, [xBP + xS*2 + X86FXSTATE.FOP]
        jne     .failure3

.success:
        xor     eax, eax                ; clear Z
.return:
        pop     xAX
        leave
        ret

.failure1:
        mov     eax, 10000000
        jmp     .failure
.failure2:
        mov     eax, 20000000
        jmp     .failure
.failure3:
        mov     eax, 30000000
        jmp     .failure
.failure:
        or      eax, eax
        leave
        ret





;;
; Checks the opcode, CS:IP and DS:DP of the FPU.
;
; @returns ZF=1 on success, ZF=0+EAX on failure.
; @param    xSP + xS    fxsave image followed by fnstenv.
; @param    xCX         Opcode address (no prefixes).
; @param    xDX         Memory address (DS relative).
;
CheckOpcodeCsIpDsDp:
        push    xBP
        mov     xBP, xSP
        push    xAX

        ; Check the memory operand.
%ifdef RT_ARCH_AMD64
        cmp     rdx, [xBP + xS*2 + X86FXSTATE.FPUDP]
%else
        cmp     edx, [xBP + xS*2 + X86FXSTATE.FPUDP]
%endif
        jne     .failure1

        mov     ax, ds
        cmp     ax, [xBP + xS*2 + 512 + X86FSTENV32P.FPUDS]
        jne     .failure2

.success:
        pop     xAX
        leave
        ; Let CheckOpcodeCsIp to the rest.
        jmp     CheckOpcodeCsIp

.failure1:
        mov     eax, 60000000
        jmp     .failure
.failure2:
        mov     eax, 80000000
.failure:
        or      eax, eax
        leave
        ret


;;
; Checks a FPU instruction taking a memory operand.
;
; @uses xCX, xDX, xAX, Stack.
;
%macro FpuCheckOpcodeCsIpDsDp 2
%%instruction:
        %1
        arch_fxsave  [xSP]
        fnstenv [xSP + 512]             ; for the selectors (64-bit)
        arch_fxrstor [xSP]              ; fnstenv screws up the ES bit.
        lea     xDX, %2
        lea     xCX, [REF(%%instruction)]
        call    CheckOpcodeCsIpDsDp
        jz      %%ok
        or      eax, __LINE__
        jmp     .return
%%ok:
%endmacro


;;
; Checks a trapping FPU instruction taking a memory operand.
;
; Upon return, there is are two FXSAVE image on the stack at xSP.
;
; @uses xCX, xDX, xAX, Stack.
;
; @param    %1  The instruction.
; @param    %2  Operand memory address (DS relative).
;
%macro FpuTrapOpcodeCsIpDsDp 2
%%instruction:
        %1
        fxsave [xSP + 1024 +512]        ; FPUDS and FPUCS for 64-bit hosts.
                                        ; WEIRD: When saved after FWAIT they are ZEROed! (64-bit Intel)
%%trap:
        fwait
%%trap_end:
        mov     eax, __LINE__
        jmp     .return
BEGINDATA
%%trapinfo: istruc TRAPINFO
        at TRAPINFO.uTrapPC,    RTCCPTR_DEF     %%trap
        at TRAPINFO.uResumePC,  RTCCPTR_DEF     %%resume
        at TRAPINFO.u8TrapNo,   db              X86_XCPT_MF
        at TRAPINFO.cbInstr,    db              (%%trap_end - %%trap)
iend
BEGINCODE
%%resume:
        arch_fxsave  [xSP]
        lea     xDX, %2
        lea     xCX, [REF(%%instruction)]
        call    CheckOpcodeCsIpDsDp
        jz      %%ok
        or      eax, __LINE__
        jmp     .return
%%ok:
%endmacro


;;
; Checks that the FPU and GReg state is completely unchanged after an instruction
; resulting in a CPU trap.
;
; @param        1       The trap number.
; @param        2+      The instruction which should trap.
;
%macro FpuCheckCpuTrapUnchangedState 2+
        call    SaveFPUAndGRegsToStack
        ShouldTrap %1, %2
        call    CompareFPUAndGRegsOnStack
        jz      %%ok
        or      eax, __LINE__
        jmp     .return
%%ok:
%endmacro


;;
; Initialize the FPU and set CW to %1.
;
; @uses dword at [xSP].
;
%macro FpuInitWithCW 1
        call    x861_LoadUniqueRegValuesSSE
        fninit
        mov     dword [xSP], %1
        fldcw   [xSP]
%endmacro


;;
; First bunch of FPU instruction tests.
;
;
BEGINPROC   x861_TestFPUInstr1
        SAVE_ALL_PROLOGUE
        sub     xSP, 2048

        ;
        ; FDIV with 64-bit floating point memory operand.
        ;
        SetSubTest "FDIV m64r"

        ; ## Normal operation. ##
        fninit
        FpuCheckOpcodeCsIpDsDp { fld  dword [REF(g_r32_3dot2)]     }, [REF(g_r32_3dot2)]
        CheckSt0Value 0x00000000, 0xcccccd00, 0x4000
        FpuCheckOpcodeCsIpDsDp { fdiv qword [REF(g_r64_One)]       }, [REF(g_r64_One)]
        FpuCheckFSW 0, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        CheckSt0Value 0x00000000, 0xcccccd00, 0x4000


        ; ## Masked exceptions. ##
        ; Masked stack underflow.
        fninit
        FpuCheckOpcodeCsIpDsDp { fdiv    qword [REF(g_r64_One)]    }, [REF(g_r64_One)]
        FpuCheckFSW X86_FSW_IE | X86_FSW_SF, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        CheckSt0Value_QNaN

        ; Masked zero divide.
        fninit
        FpuCheckOpcodeCsIpDsDp { fld     dword [REF(g_r32_3dot2)]  }, [REF(g_r32_3dot2)]
        FpuCheckOpcodeCsIpDsDp { fdiv    qword [REF(g_r64_Zero)]   }, [REF(g_r64_Zero)]
        FpuCheckFSW X86_FSW_ZE, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        CheckSt0Value_PlusInf

        ; Masked Inf/Inf.
        fninit
        FpuCheckOpcodeCsIpDsDp { fld     dword [REF(g_r32_Inf)]    }, [REF(g_r32_Inf)]
        FpuCheckOpcodeCsIpDsDp { fdiv    qword [REF(g_r64_Inf)]    }, [REF(g_r64_Inf)]
        FpuCheckFSW X86_FSW_IE, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        CheckSt0Value_QNaN

        ; Masked 0/0.
        fninit
        FpuCheckOpcodeCsIpDsDp { fld     dword [REF(g_r32_Zero)]   }, [REF(g_r32_Zero)]
        FpuCheckOpcodeCsIpDsDp { fdiv    qword [REF(g_r64_Zero)]   }, [REF(g_r64_Zero)]
        FpuCheckFSW X86_FSW_IE, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        CheckSt0Value_QNaN

        ; Masked precision exception, rounded down.
        fninit
        FpuCheckOpcodeCsIpDsDp { fld     dword [REF(g_r32_Ten)]    }, [REF(g_r32_Ten)]
        FpuCheckOpcodeCsIpDsDp { fdiv    qword [REF(g_r64_Three)]  }, [REF(g_r64_Three)]
        FpuCheckFSW X86_FSW_PE, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        CheckSt0Value_3_and_a_3rd

        ; Masked precision exception, rounded up.
        fninit
        FpuCheckOpcodeCsIpDsDp { fld     dword [REF(g_r32_Eleven)] }, [REF(g_r32_Eleven)]
        FpuCheckOpcodeCsIpDsDp { fdiv    qword [REF(g_r64_Three)]  }, [REF(g_r64_Three)]
        FpuCheckFSW X86_FSW_PE | X86_FSW_C1, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        CheckSt0Value_3_and_two_3rds

        ; Masked overflow exception.
        fninit
        FpuCheckOpcodeCsIpDsDp { fld     tword [REF(g_r80_Max)]    }, [REF(g_r80_Max)]
        FpuCheckOpcodeCsIpDsDp { fdiv    qword [REF(g_r64_0dot1)]  }, [REF(g_r64_0dot1)]
        FpuCheckFSW X86_FSW_PE | X86_FSW_OE | X86_FSW_C1, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        CheckSt0Value_PlusInf

        ; Masked underflow exception.
        fninit
        FpuCheckOpcodeCsIpDsDp { fld     tword [REF(g_r80_Min)]    }, [REF(g_r80_Min)]
        FpuCheckOpcodeCsIpDsDp { fdiv    qword [REF(g_r64_Ten)]    }, [REF(g_r64_Ten)]
        FpuCheckFSW X86_FSW_PE | X86_FSW_UE | X86_FSW_C1, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        CheckSt0Value 0xcccccccd, 0x0ccccccc, 0x0000

        ; Denormal operand.
        fninit
        FpuCheckOpcodeCsIpDsDp { fld     tword [REF(g_r80_One)]    }, [REF(g_r80_One)]
        FpuCheckOpcodeCsIpDsDp { fdiv    qword [REF(g_r64_DnMax)]  }, [REF(g_r64_DnMax)]
        FxSaveCheckFSW xSP, X86_FSW_DE | X86_FSW_PE, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        FxSaveCheckSt0Value xSP, 0x00000800, 0x80000000, 0x43fd

        ; ## Unmasked exceptions. ##
        ; Stack underflow - TOP and ST0 unmodified.
        FpuInitWithCW X86_FCW_PC_64 | X86_FCW_RC_NEAREST
        FpuTrapOpcodeCsIpDsDp  { fdiv    qword [REF(g_r64_One)]    }, [REF(g_r64_One)]
        FxSaveCheckFSW xSP, X86_FSW_IE | X86_FSW_SF | X86_FSW_B | X86_FSW_ES, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        FxSaveCheckSt0EmptyInitValue xSP

        ; Zero divide - Unmodified ST0.
        FpuInitWithCW X86_FCW_PC_64 | X86_FCW_RC_NEAREST
        FpuCheckOpcodeCsIpDsDp { fld     dword [REF(g_r32_3dot2)]  }, [REF(g_r32_3dot2)]
        FpuTrapOpcodeCsIpDsDp  { fdiv    qword [REF(g_r64_Zero)]   }, [REF(g_r64_Zero)]
        FxSaveCheckFSW xSP, X86_FSW_ZE | X86_FSW_ES | X86_FSW_B, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        FxSaveCheckSt0ValueConst xSP, REF(g_r80_r32_3dot2)

        ; Invalid Operand (Inf/Inf) - Unmodified ST0.
        FpuInitWithCW X86_FCW_PC_64 | X86_FCW_RC_NEAREST
        FpuCheckOpcodeCsIpDsDp { fld     dword [REF(g_r32_Inf)]    }, [REF(g_r32_Inf)]
        FpuTrapOpcodeCsIpDsDp  { fdiv    qword [REF(g_r64_Inf)]    }, [REF(g_r64_Inf)]
        FpuCheckFSW X86_FSW_IE | X86_FSW_ES | X86_FSW_B, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        FxSaveCheckSt0ValueConst xSP, REF(g_r80_Inf)

        ; Invalid Operand (0/0) - Unmodified ST0.
        FpuInitWithCW X86_FCW_PC_64 | X86_FCW_RC_NEAREST
        FpuCheckOpcodeCsIpDsDp { fld     dword [REF(g_r32_Zero)]   }, [REF(g_r32_Zero)]
        FpuTrapOpcodeCsIpDsDp  { fdiv    qword [REF(g_r64_Zero)]   }, [REF(g_r64_Zero)]
        FxSaveCheckFSW xSP, X86_FSW_IE | X86_FSW_ES | X86_FSW_B, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        FxSaveCheckSt0ValueConst xSP, REF(g_r80_Zero)

        ; Precision exception, rounded down.
        FpuInitWithCW X86_FCW_PC_64 | X86_FCW_RC_NEAREST
        FpuCheckOpcodeCsIpDsDp { fld     dword [REF(g_r32_Ten)]    }, [REF(g_r32_Ten)]
        FpuTrapOpcodeCsIpDsDp  { fdiv    qword [REF(g_r64_Three)]  }, [REF(g_r64_Three)]
        FxSaveCheckFSW xSP, X86_FSW_PE | X86_FSW_ES | X86_FSW_B, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        FxSaveCheckSt0Value_3_and_a_3rd(xSP)

        ; Precision exception, rounded up.
        FpuInitWithCW X86_FCW_PC_64 | X86_FCW_RC_NEAREST
        FpuCheckOpcodeCsIpDsDp { fld     dword [REF(g_r32_Eleven)] }, [REF(g_r32_Eleven)]
        FpuTrapOpcodeCsIpDsDp  { fdiv    qword [REF(g_r64_Three)]  }, [REF(g_r64_Three)]
        FxSaveCheckFSW xSP, X86_FSW_PE | X86_FSW_C1 | X86_FSW_ES | X86_FSW_B, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        FxSaveCheckSt0Value_3_and_two_3rds(xSP)

        ; Overflow exception.
        FpuInitWithCW X86_FCW_PC_64 | X86_FCW_RC_NEAREST
        FpuCheckOpcodeCsIpDsDp { fld     tword [REF(g_r80_Max)]    }, [REF(g_r80_Max)]
        FpuTrapOpcodeCsIpDsDp  { fdiv    qword [REF(g_r64_0dot1)]  }, [REF(g_r64_0dot1)]
        FxSaveCheckFSW xSP, X86_FSW_PE | X86_FSW_OE | X86_FSW_ES | X86_FSW_B, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        FxSaveCheckSt0Value xSP, 0xfffffd7f, 0x9fffffff, 0x2002

        ; Underflow exception.
        FpuInitWithCW X86_FCW_PC_64 | X86_FCW_RC_NEAREST
        FpuCheckOpcodeCsIpDsDp { fld     tword [REF(g_r80_Min)]    }, [REF(g_r80_Min)]
        FpuTrapOpcodeCsIpDsDp  { fdiv    qword [REF(g_r64_Ten)]    }, [REF(g_r64_Ten)]
        FxSaveCheckFSW xSP, X86_FSW_PE | X86_FSW_UE | X86_FSW_C1 | X86_FSW_ES | X86_FSW_B, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        FxSaveCheckSt0Value xSP, 0xcccccccd, 0xcccccccc, 0x5ffd

        ; Denormal operand - Unmodified ST0.
        FpuInitWithCW X86_FCW_PC_64 | X86_FCW_RC_NEAREST
        FpuCheckOpcodeCsIpDsDp { fld     tword [REF(g_r80_One)]    }, [REF(g_r80_One)]
        FpuTrapOpcodeCsIpDsDp  { fdiv    qword [REF(g_r64_DnMax)]  }, [REF(g_r64_DnMax)]
        FxSaveCheckFSW xSP, X86_FSW_DE | X86_FSW_ES | X86_FSW_B, X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3
        FxSaveCheckSt0ValueConst xSP, REF(g_r80_One)

        ;;; @todo exception priority checks.



        ; ## A couple of variations on the #PF theme. ##

        FpuInitWithCW X86_FCW_PC_64 | X86_FCW_RC_NEAREST
        mov     xBX, [REF_EXTERN(g_pbEfExecPage)]
        FpuCheckCpuTrapUnchangedState X86_XCPT_PF, fdiv qword [xBX + PAGE_SIZE]

        ; Check that a pending FPU exception takes precedence over a #PF.
        fninit
        fdiv    qword [REF(g_r64_One)]
        fstcw   [xSP]
        and      word [xSP], ~(X86_FCW_IM)
        fldcw   [xSP]
        mov     xBX, [REF_EXTERN(g_pbEfExecPage)]
        ShouldTrap X86_XCPT_MF, fdiv qword [xBX + PAGE_SIZE]

.success:
        xor     eax, eax
.return:
        add     xSP, 2048
        SAVE_ALL_EPILOGUE
        ret

ENDPROC     x861_TestFPUInstr1




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

