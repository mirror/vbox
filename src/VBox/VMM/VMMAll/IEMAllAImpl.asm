; $Id$
;; @file
; IEM - Instruction Implementation in Assembly.
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


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;   Header Files                                                               ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%include "VBox/asmdefs.mac"
%include "VBox/err.mac"
%include "iprt/x86.mac"


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;   Defined Constants And Macros                                               ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;
; We employ some macro assembly here to hid the calling convention differences.
;
%ifdef RT_ARCH_AMD64
 %macro PROLOGUE_1_ARGS 0
 %endmacro
 %macro EPILOGUE_1_ARGS 0
 %endmacro
 %macro PROLOGUE_2_ARGS 0
 %endmacro
 %macro EPILOGUE_2_ARGS 0
 %endmacro
 %macro PROLOGUE_3_ARGS 0
 %endmacro
 %macro EPILOGUE_3_ARGS 0
 %endmacro
 %macro PROLOGUE_4_ARGS 0
 %endmacro
 %macro EPILOGUE_4_ARGS 0
 %endmacro

 %ifdef ASM_CALL64_GCC
  %define A0        rdi
  %define A0_32     edi
  %define A0_16     di
  %define A0_8      dil

  %define A1        rsi
  %define A1_32     esi
  %define A1_16     si
  %define A1_8      sil

  %define A2        rdx
  %define A2_32     edx
  %define A2_16     dx
  %define A2_8      dl

  %define A3        rcx
  %define A3_32     ecx
  %define A3_16     cx
 %endif

 %ifdef ASM_CALL64_MSC
  %define A0        rcx
  %define A0_32     ecx
  %define A0_16     cx
  %define A0_8      cl

  %define A1        rdx
  %define A1_32     edx
  %define A1_16     dx
  %define A1_8      dl

  %define A2        r8
  %define A2_32     r8d
  %define A2_16     r8w
  %define A2_8      r8b

  %define A3        r9
  %define A3_32     r9d
  %define A3_16     r9w
 %endif

 %define T0         rax
 %define T0_32      eax
 %define T0_16      ax
 %define T0_8       al

 %define T1         r11
 %define T1_32      r11d
 %define T1_16      r11w
 %define T1_8       r11b

%else
 ; x86
 %macro PROLOGUE_1_ARGS 0
        push    edi
 %endmacro
 %macro EPILOGUE_1_ARGS 0
        pop     edi
 %endmacro

 %macro PROLOGUE_2_ARGS 0
        push    edi
 %endmacro
 %macro EPILOGUE_2_ARGS 0
        pop     edi
 %endmacro

 %macro PROLOGUE_3_ARGS 0
        push    ebx
        mov     ebx, [esp + 4 + 4]
        push    edi
 %endmacro
 %macro EPILOGUE_3_ARGS 0
        pop     edi
        pop     ebx
 %endmacro

 %macro PROLOGUE_4_ARGS 0
        push    ebx
        push    edi
        push    esi
        mov     ebx, [esp + 12 + 4 + 0]
        mov     esi, [esp + 12 + 4 + 4]
 %endmacro
 %macro EPILOGUE_4_ARGS 0
        pop     esi
        pop     edi
        pop     ebx
 %endmacro

 %define A0         ecx
 %define A0_32      ecx
 %define A0_16       cx
 %define A0_8        cl

 %define A1         edx
 %define A1_32      edx
 %define A1_16      dx
 %define A1_8       dl

 %define A2         ebx
 %define A2_32      ebx
 %define A2_16      bx
 %define A2_8       bl

 %define A3         esi
 %define A3_32      esi
 %define A3_16      si

 %define T0         eax
 %define T0_32      eax
 %define T0_16      ax
 %define T0_8       al

 %define T1         edi
 %define T1_32      edi
 %define T1_16      di
%endif

;;
; NAME for fastcall functions.
;
;; @todo 'global @fastcall@12' is still broken in yasm and requires dollar 
;         escaping (or whatever the dollar is good for here).  Thus the ugly
;         prefix argument.
;
%define NAME_FASTCALL(a_Name, a_cbArgs, a_Dollar)   NAME(a_Name)
%ifdef RT_ARCH_X86
 %ifdef RT_OS_WINDOWS                                 
  %undef NAME_FASTCALL
  %define NAME_FASTCALL(a_Name, a_cbArgs, a_Prefix) a_Prefix %+ a_Name %+ @ %+ a_cbArgs
 %endif        
%endif

;;
; BEGINPROC for fastcall functions.
;
; @param        1       The function name (C).
; @param        2       The argument size on x86.
;
%macro BEGINPROC_FASTCALL 2
 %ifdef ASM_FORMAT_PE
  export %1=NAME_FASTCALL(%1,%2,$@)
 %endif
 %ifdef __NASM__
  %ifdef ASM_FORMAT_OMF
   export NAME(%1) NAME_FASTCALL(%1,%2,$@)
  %endif
 %endif
 %ifndef ASM_FORMAT_BIN
  global NAME_FASTCALL(%1,%2,$@)
 %endif
NAME_FASTCALL(%1,%2,@):
%endmacro


;;
; Load the relevant flags from [%1] if there are undefined flags (%3).
;
; @remarks      Clobbers T0, stack. Changes EFLAGS.
; @param        A2      The register pointing to the flags.
; @param        1       The parameter (A0..A3) pointing to the eflags.
; @param        2       The set of modified flags.
; @param        3       The set of undefined flags.
;
%macro IEM_MAYBE_LOAD_FLAGS 3
 ;%if (%3) != 0
        pushf                           ; store current flags
        mov     T0_32, [%1]             ; load the guest flags
        and     dword [xSP], ~(%2 | %3) ; mask out the modified and undefined flags
        and     T0_32, (%2 | %3)        ; select the modified and undefined flags.
        or      [xSP], T0               ; merge guest flags with host flags.
        popf                            ; load the mixed flags.
 ;%endif
%endmacro

;;
; Update the flag.
;
; @remarks  Clobbers T0, T1, stack.
; @param        1       The register pointing to the EFLAGS.
; @param        2       The mask of modified flags to save.
; @param        3       The mask of undefined flags to (maybe) save.
;
%macro IEM_SAVE_FLAGS 3
 %if (%2 | %3) != 0
        pushf
        pop     T1
        mov     T0_32, [%1]             ; flags
        and     T0_32, ~(%2 | %3)       ; clear the modified & undefined flags.
        and     T1_32, (%2 | %3)        ; select the modified and undefined flags.
        or      T0_32, T1_32            ; combine the flags.
        mov     [%1], T0_32             ; save the flags.
 %endif
%endmacro


;;
; Macro for implementing a binary operator.
;
; This will generate code for the 8, 16, 32 and 64 bit accesses with locked
; variants, except on 32-bit system where the 64-bit accesses requires hand
; coding.
;
; All the functions takes a pointer to the destination memory operand in A0,
; the source register operand in A1 and a pointer to eflags in A2.
;
; @param        1       The instruction mnemonic.
; @param        2       Non-zero if there should be a locked version.
; @param        3       The modified flags.
; @param        4       The undefined flags.
;
%macro IEMIMPL_BIN_OP 4
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u8, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        %1      byte [A0], A1_8
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u8

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        %1      word [A0], A1_16
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u16

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        %1      dword [A0], A1_32
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u32

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 16
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        %1      qword [A0], A1
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64
 %else ; stub it for now - later, replace with hand coded stuff.
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 16
        int3
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64
  %endif ; !RT_ARCH_AMD64

 %if %2 != 0 ; locked versions requested?

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u8_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        lock %1 byte [A0], A1_8
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u8_locked

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        lock %1 word [A0], A1_16
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u16_locked

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        lock %1 dword [A0], A1_32
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u32_locked

  %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64_locked, 16
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        lock %1 qword [A0], A1
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64_locked
  %else ; stub it for now - later, replace with hand coded stuff.
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64_locked, 16
        int3
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64_locked
  %endif ; !RT_ARCH_AMD64
 %endif ; locked
%endmacro

;            instr,lock,modified-flags.
IEMIMPL_BIN_OP add,  1, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
IEMIMPL_BIN_OP adc,  1, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
IEMIMPL_BIN_OP sub,  1, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
IEMIMPL_BIN_OP sbb,  1, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
IEMIMPL_BIN_OP or,   1, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF), X86_EFL_AF,
IEMIMPL_BIN_OP xor,  1, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF), X86_EFL_AF,
IEMIMPL_BIN_OP and,  1, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF), X86_EFL_AF,
IEMIMPL_BIN_OP cmp,  0, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
IEMIMPL_BIN_OP test, 0, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF), X86_EFL_AF,


;;
; Macro for implementing a bit operator.
;
; This will generate code for the 16, 32 and 64 bit accesses with locked
; variants, except on 32-bit system where the 64-bit accesses requires hand
; coding.
;
; All the functions takes a pointer to the destination memory operand in A0,
; the source register operand in A1 and a pointer to eflags in A2.
;
; @param        1       The instruction mnemonic.
; @param        2       Non-zero if there should be a locked version.
; @param        3       The modified flags.
; @param        4       The undefined flags.
;
%macro IEMIMPL_BIT_OP 4
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        %1      word [A0], A1_16
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u16

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        %1      dword [A0], A1_32
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u32

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 16
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        %1      qword [A0], A1
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64
 %else ; stub it for now - later, replace with hand coded stuff.
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 16
        int3
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64
  %endif ; !RT_ARCH_AMD64

 %if %2 != 0 ; locked versions requested?

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        lock %1 word [A0], A1_16
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u16_locked

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        lock %1 dword [A0], A1_32
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u32_locked

  %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64_locked, 16
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        lock %1 qword [A0], A1
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64_locked
  %else ; stub it for now - later, replace with hand coded stuff.
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64_locked, 16
        int3
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64_locked
  %endif ; !RT_ARCH_AMD64
 %endif ; locked
%endmacro
IEMIMPL_BIT_OP bt,  0, (X86_EFL_CF), (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)
IEMIMPL_BIT_OP btc, 1, (X86_EFL_CF), (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)
IEMIMPL_BIT_OP bts, 1, (X86_EFL_CF), (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)
IEMIMPL_BIT_OP btr, 1, (X86_EFL_CF), (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)

;;
; Macro for implementing a bit search operator.
;
; This will generate code for the 16, 32 and 64 bit accesses, except on 32-bit
; system where the 64-bit accesses requires hand coding.
;
; All the functions takes a pointer to the destination memory operand in A0,
; the source register operand in A1 and a pointer to eflags in A2.
;
; @param        1       The instruction mnemonic.
; @param        2       The modified flags.
; @param        3       The undefined flags.
;
%macro IEMIMPL_BIT_OP 3
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %2, %3
        %1      T0_16, A1_16
        mov     [A0], T0_16
        IEM_SAVE_FLAGS                 A2, %2, %3
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u16

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %2, %3
        %1      T0_32, A1_32
        mov     [A0], T0_32
        IEM_SAVE_FLAGS                 A2, %2, %3
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u32

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 16
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %2, %3
        %1      T0, A1
        mov     [A0], T0
        IEM_SAVE_FLAGS                 A2, %2, %3
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64
 %else ; stub it for now - later, replace with hand coded stuff.
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 16
        int3
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64
 %endif ; !RT_ARCH_AMD64
%endmacro
IEMIMPL_BIT_OP bsf, (X86_EFL_ZF), (X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF)
IEMIMPL_BIT_OP bsr, (X86_EFL_ZF), (X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF)


;
; IMUL is also a similar but yet different case (no lock, no mem dst).
; The rDX:rAX variant of imul is handled together with mul further down.
;
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_imul_two_u16, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_CF), (X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)
        imul    A1_16, word [A0]
        mov     [A0], A1_16
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_CF), (X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_imul_two_u16

BEGINPROC_FASTCALL iemAImpl_imul_two_u32, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_CF), (X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)
        imul    A1_32, dword [A0]
        mov     [A0], A1_32
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_CF), (X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_imul_two_u32

BEGINPROC_FASTCALL iemAImpl_imul_two_u64, 16
        PROLOGUE_3_ARGS
%ifdef RT_ARCH_AMD64
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_CF), (X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)
        imul    A1, qword [A0]
        mov     [A0], A1
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_CF), (X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)
%else
        int3 ;; @todo implement me
%endif
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_imul_two_u64


;
; XCHG for memory operands.  This implies locking.  No flag changes.
;
; Each function takes two arguments, first the pointer to the memory,
; then the pointer to the register.  They all return void.
;
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_xchg_u8, 8
        PROLOGUE_2_ARGS
        mov     T0_8, [A1]
        xchg    [A0], T0_8
        mov     [A1], T0_8
        EPILOGUE_2_ARGS
        ret
ENDPROC iemAImpl_xchg_u8

BEGINPROC_FASTCALL iemAImpl_xchg_u16, 8
        PROLOGUE_2_ARGS
        mov     T0_16, [A1]
        xchg    [A0], T0_16
        mov     [A1], T0_16
        EPILOGUE_2_ARGS
        ret
ENDPROC iemAImpl_xchg_u16

BEGINPROC_FASTCALL iemAImpl_xchg_u32, 8
        PROLOGUE_2_ARGS
        mov     T0_32, [A1]
        xchg    [A0], T0_32
        mov     [A1], T0_32
        EPILOGUE_2_ARGS
        ret
ENDPROC iemAImpl_xchg_u32

BEGINPROC_FASTCALL iemAImpl_xchg_u64, 8
%ifdef RT_ARCH_AMD64
        PROLOGUE_2_ARGS
        mov     T0, [A1]
        xchg    [A0], T0
        mov     [A1], T0
        EPILOGUE_2_ARGS
        ret
%else
        int3
%endif
ENDPROC iemAImpl_xchg_u64


;
; XADD for memory operands.
;
; Each function takes three arguments, first the pointer to the
; memory/register, then the pointer to the register, and finally a pointer to
; eflags.  They all return void.
;
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_xadd_u8, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0_8, [A1]
        xadd    [A0], T0_8
        mov     [A1], T0_8
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_xadd_u8

BEGINPROC_FASTCALL iemAImpl_xadd_u16, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0_16, [A1]
        xadd    [A0], T0_16
        mov     [A1], T0_16
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_xadd_u16

BEGINPROC_FASTCALL iemAImpl_xadd_u32, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0_32, [A1]
        xadd    [A0], T0_32
        mov     [A1], T0_32
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_xadd_u32

BEGINPROC_FASTCALL iemAImpl_xadd_u64, 12
%ifdef RT_ARCH_AMD64
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0, [A1]
        xadd    [A0], T0
        mov     [A1], T0
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
        ret
%else
        int3
%endif
ENDPROC iemAImpl_xadd_u64

BEGINPROC_FASTCALL iemAImpl_xadd_u8_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0_8, [A1]
        lock xadd [A0], T0_8
        mov     [A1], T0_8
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_xadd_u8_locked

BEGINPROC_FASTCALL iemAImpl_xadd_u16_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0_16, [A1]
        lock xadd [A0], T0_16
        mov     [A1], T0_16
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_xadd_u16_locked

BEGINPROC_FASTCALL iemAImpl_xadd_u32_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0_32, [A1]
        lock xadd [A0], T0_32
        mov     [A1], T0_32
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_xadd_u32_locked

BEGINPROC_FASTCALL iemAImpl_xadd_u64_locked, 12
%ifdef RT_ARCH_AMD64
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0, [A1]
        lock xadd [A0], T0
        mov     [A1], T0
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
        ret
%else
        int3
%endif
ENDPROC iemAImpl_xadd_u64_locked


;;
; Macro for implementing a unary operator.
;
; This will generate code for the 8, 16, 32 and 64 bit accesses with locked
; variants, except on 32-bit system where the 64-bit accesses requires hand
; coding.
;
; All the functions takes a pointer to the destination memory operand in A0,
; the source register operand in A1 and a pointer to eflags in A2.
;
; @param        1       The instruction mnemonic.
; @param        2       The modified flags.
; @param        3       The undefined flags.
;
%macro IEMIMPL_UNARY_OP 3
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u8, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        %1      byte [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u8

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u8_locked, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        lock %1 byte [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u8_locked

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        %1      word [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u16

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16_locked, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        lock %1 word [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u16_locked

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        %1      dword [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u32

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32_locked, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        lock %1 dword [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u32_locked

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        %1      qword [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64_locked, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        lock %1 qword [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64_locked
 %else
        ; stub them for now.
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 8
        int3
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64_locked, 8
        int3
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64_locked
 %endif

%endmacro

IEMIMPL_UNARY_OP inc, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF), 0
IEMIMPL_UNARY_OP dec, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF), 0
IEMIMPL_UNARY_OP neg, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
IEMIMPL_UNARY_OP not, 0, 0



;;
; Macro for implementing a shift operation.
;
; This will generate code for the 8, 16, 32 and 64 bit accesses, except on
; 32-bit system where the 64-bit accesses requires hand coding.
;
; All the functions takes a pointer to the destination memory operand in A0,
; the shift count in A1 and a pointer to eflags in A2.
;
; @param        1       The instruction mnemonic.
; @param        2       The modified flags.
; @param        3       The undefined flags.
;
; Makes ASSUMPTIONS about A0, A1 and A2 assignments.
;
%macro IEMIMPL_SHIFT_OP 3
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u8, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, %2, %3
 %ifdef ASM_CALL64_GCC
        mov     cl, A1_8
        %1      byte [A0], cl
 %else
        xchg    A1, A0
        %1      byte [A1], cl
 %endif
        IEM_SAVE_FLAGS       A2, %2, %3
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u8

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, %2, %3
 %ifdef ASM_CALL64_GCC
        mov     cl, A1_8
        %1      word [A0], cl
 %else
        xchg    A1, A0
        %1      word [A1], cl
 %endif
        IEM_SAVE_FLAGS       A2, %2, %3
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u16

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, %2, %3
 %ifdef ASM_CALL64_GCC
        mov     cl, A1_8
        %1      dword [A0], cl
 %else
        xchg    A1, A0
        %1      dword [A1], cl
 %endif
        IEM_SAVE_FLAGS       A2, %2, %3
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u32

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, %2, %3
 %ifdef ASM_CALL64_GCC
        mov     cl, A1_8
        %1      qword [A0], cl
 %else
        xchg    A1, A0
        %1      qword [A1], cl
 %endif
        IEM_SAVE_FLAGS       A2, %2, %3
        EPILOGUE_3_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64
 %else ; stub it for now - later, replace with hand coded stuff.
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 12
        int3
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64
  %endif ; !RT_ARCH_AMD64

%endmacro

IEMIMPL_SHIFT_OP rol, (X86_EFL_OF | X86_EFL_CF), 0
IEMIMPL_SHIFT_OP ror, (X86_EFL_OF | X86_EFL_CF), 0
IEMIMPL_SHIFT_OP rcl, (X86_EFL_OF | X86_EFL_CF), 0
IEMIMPL_SHIFT_OP rcr, (X86_EFL_OF | X86_EFL_CF), 0
IEMIMPL_SHIFT_OP shl, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF), (X86_EFL_AF)
IEMIMPL_SHIFT_OP shr, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF), (X86_EFL_AF)
IEMIMPL_SHIFT_OP sar, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF), (X86_EFL_AF)


;;
; Macro for implementing a double precision shift operation.
;
; This will generate code for the 16, 32 and 64 bit accesses, except on
; 32-bit system where the 64-bit accesses requires hand coding.
;
; The functions takes the destination operand (r/m) in A0, the source (reg) in
; A1, the shift count in A2 and a pointer to the eflags variable/register in A3.
;
; @param        1       The instruction mnemonic.
; @param        2       The modified flags.
; @param        3       The undefined flags.
;
; Makes ASSUMPTIONS about A0, A1, A2 and A3 assignments.
;
%macro IEMIMPL_SHIFT_DBL_OP 3
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16, 16
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS A3, %2, %3
 %ifdef ASM_CALL64_GCC
        xchg    A3, A2
        %1      [A0], A1_16, cl
        xchg    A3, A2
 %else
        xchg    A0, A2
        %1      [A2], A1_16, cl
 %endif
        IEM_SAVE_FLAGS       A3, %2, %3
        EPILOGUE_4_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u16

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 16
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS A3, %2, %3
 %ifdef ASM_CALL64_GCC
        xchg    A3, A2
        %1      [A0], A1_32, cl
        xchg    A3, A2
 %else
        xchg    A0, A2
        %1      [A2], A1_32, cl
 %endif
        IEM_SAVE_FLAGS       A3, %2, %3
        EPILOGUE_4_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u32

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 20
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS A3, %2, %3
 %ifdef ASM_CALL64_GCC
        xchg    A3, A2
        %1      [A0], A1, cl
        xchg    A3, A2
 %else
        xchg    A0, A2
        %1      [A2], A1, cl
 %endif
        IEM_SAVE_FLAGS       A3, %2, %3
        EPILOGUE_4_ARGS
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64
 %else ; stub it for now - later, replace with hand coded stuff.
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 20
        int3
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64
  %endif ; !RT_ARCH_AMD64

%endmacro

IEMIMPL_SHIFT_DBL_OP shld, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF), (X86_EFL_AF)
IEMIMPL_SHIFT_DBL_OP shrd, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF), (X86_EFL_AF)


;;
; Macro for implementing a multiplication operations.
;
; This will generate code for the 8, 16, 32 and 64 bit accesses, except on
; 32-bit system where the 64-bit accesses requires hand coding.
;
; The 8-bit function only operates on AX, so it takes no DX pointer.  The other
; functions takes a pointer to rAX in A0, rDX in A1, the operand in A2 and a
; pointer to eflags in A3.
;
; The functions all return 0 so the caller can be used for div/idiv as well as
; for the mul/imul implementation.
;
; @param        1       The instruction mnemonic.
; @param        2       The modified flags.
; @param        3       The undefined flags.
;
; Makes ASSUMPTIONS about A0, A1, A2, A3, T0 and T1 assignments.
;
%macro IEMIMPL_MUL_OP 3
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u8, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, %2, %3
        mov     al, [A0]
        %1      A1_8
        mov     [A0], ax
        IEM_SAVE_FLAGS       A2, %2, %3
        EPILOGUE_3_ARGS
        xor     eax, eax
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u8

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16, 16
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS A3, %2, %3
        mov     ax, [A0]
 %ifdef ASM_CALL64_GCC
        %1      A2_16
        mov     [A0], ax
        mov     [A1], dx
 %else
        mov     T1, A1
        %1      A2_16
        mov     [A0], ax
        mov     [T1], dx
 %endif
        IEM_SAVE_FLAGS       A3, %2, %3
        EPILOGUE_4_ARGS
        xor     eax, eax
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u16

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 16
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS A3, %2, %3
        mov     eax, [A0]
 %ifdef ASM_CALL64_GCC
        %1      A2_32
        mov     [A0], eax
        mov     [A1], edx
 %else
        mov     T1, A1
        %1      A2_32
        mov     [A0], eax
        mov     [T1], edx
 %endif
        IEM_SAVE_FLAGS       A3, %2, %3
        EPILOGUE_4_ARGS
        xor     eax, eax
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u32

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 20
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS A3, %2, %3
        mov     rax, [A0]
 %ifdef ASM_CALL64_GCC
        %1      A2
        mov     [A0], rax
        mov     [A1], rdx
 %else
        mov     T1, A1
        %1      A2
        mov     [A0], rax
        mov     [T1], rdx
 %endif
        IEM_SAVE_FLAGS       A3, %2, %3
        EPILOGUE_4_ARGS
        xor     eax, eax
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64
 %else ; stub it for now - later, replace with hand coded stuff.
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 20
        int3
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64
  %endif ; !RT_ARCH_AMD64

%endmacro

IEMIMPL_MUL_OP mul,  (X86_EFL_OF | X86_EFL_CF), (X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)
IEMIMPL_MUL_OP imul, (X86_EFL_OF | X86_EFL_CF), (X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)


;;
; Macro for implementing a division operations.
;
; This will generate code for the 8, 16, 32 and 64 bit accesses, except on
; 32-bit system where the 64-bit accesses requires hand coding.
;
; The 8-bit function only operates on AX, so it takes no DX pointer.  The other
; functions takes a pointer to rAX in A0, rDX in A1, the operand in A2 and a
; pointer to eflags in A3.
;
; The functions all return 0 on success and -1 if a divide error should be
; raised by the caller.
;
; @param        1       The instruction mnemonic.
; @param        2       The modified flags.
; @param        3       The undefined flags.
;
; Makes ASSUMPTIONS about A0, A1, A2, A3, T0 and T1 assignments.
;
%macro IEMIMPL_DIV_OP 3
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u8, 12
        PROLOGUE_3_ARGS

        test    A1_8, A1_8
        jz      .div_zero
        ;; @todo test for overflow

        IEM_MAYBE_LOAD_FLAGS A2, %2, %3
        mov     ax, [A0]
        %1      A1_8
        mov     [A0], ax
        IEM_SAVE_FLAGS       A2, %2, %3
        xor     eax, eax

.return:
        EPILOGUE_3_ARGS
        ret
.div_zero:
        mov     eax, -1
        jmp     .return
ENDPROC iemAImpl_ %+ %1 %+ _u8

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16, 16
        PROLOGUE_4_ARGS

        test    A1_16, A1_16
        jz      .div_zero
        ;; @todo test for overflow

        IEM_MAYBE_LOAD_FLAGS A3, %2, %3
 %ifdef ASM_CALL64_GCC
        mov     T1, A2
        mov     ax, [A0]
        mov     dx, [A1]
        %1      T1_16
        mov     [A0], ax
        mov     [A1], dx
 %else
        mov     T1, A1
        mov     ax, [A0]
        mov     dx, [T1]
        %1      A2_16
        mov     [A0], ax
        mov     [T1], dx
 %endif
        IEM_SAVE_FLAGS       A3, %2, %3
        xor     eax, eax

.return:
        EPILOGUE_4_ARGS
        ret
.div_zero:
        mov     eax, -1
        jmp     .return
ENDPROC iemAImpl_ %+ %1 %+ _u16

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 16
        PROLOGUE_4_ARGS

        test    A1_32, A1_32
        jz      .div_zero
        ;; @todo test for overflow

        IEM_MAYBE_LOAD_FLAGS A3, %2, %3
        mov     eax, [A0]
 %ifdef ASM_CALL64_GCC
        mov     T1, A2
        mov     eax, [A0]
        mov     edx, [A1]
        %1      T1_32
        mov     [A0], eax
        mov     [A1], edx
 %else
        mov     T1, A1
        mov     eax, [A0]
        mov     edx, [T1]
        %1      A2_32
        mov     [A0], eax
        mov     [T1], edx
 %endif
        IEM_SAVE_FLAGS       A3, %2, %3
        xor     eax, eax

.return:
        EPILOGUE_4_ARGS
        ret
.div_zero:
        mov     eax, -1
        jmp     .return
ENDPROC iemAImpl_ %+ %1 %+ _u32

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 20
        PROLOGUE_4_ARGS

        test    A1, A1
        jz      .div_zero
        ;; @todo test for overflow

        IEM_MAYBE_LOAD_FLAGS A3, %2, %3
        mov     rax, [A0]
 %ifdef ASM_CALL64_GCC
        mov     T1, A2
        mov     rax, [A0]
        mov     rdx, [A1]
        %1      T1
        mov     [A0], rax
        mov     [A1], rdx
 %else
        mov     T1, A1
        mov     rax, [A0]
        mov     rdx, [T1]
        %1      A2
        mov     [A0], rax
        mov     [T1], rdx
 %endif
        IEM_SAVE_FLAGS       A3, %2, %3
        xor     eax, eax

.return:
        EPILOGUE_4_ARGS
        ret
.div_zero:
        mov     eax, -1
        jmp     .return
ENDPROC iemAImpl_ %+ %1 %+ _u64
 %else ; stub it for now - later, replace with hand coded stuff.
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 20
        int3
        ret
ENDPROC iemAImpl_ %+ %1 %+ _u64
  %endif ; !RT_ARCH_AMD64

%endmacro

IEMIMPL_DIV_OP div,  0, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF)
IEMIMPL_DIV_OP idiv, 0, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF)

