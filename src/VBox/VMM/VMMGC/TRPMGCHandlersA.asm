; $Id$
;; @file
; TRPM - Guest Context Trap Handlers
;

; Copyright (C) 2006-2007 Sun Microsystems, Inc.
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;
; Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
; Clara, CA 95054 USA or visit http://www.sun.com if you need
; additional information or have any questions.
;

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "VMMGC.mac"
%include "VBox/x86.mac"
%include "VBox/cpum.mac"
%include "VBox/stam.mac"
%include "VBox/vm.mac"
%include "TRPMInternal.mac"
%include "VBox/err.mac"
%include "VBox/trpm.mac"


;*******************************************************************************
;* External Symbols                                                            *
;*******************************************************************************
extern IMPNAME(g_CPUM)                  ; These IMPNAME(g_*) symbols resolve to the import table
extern IMPNAME(g_TRPM)                  ; where there is a pointer to the real symbol. PE imports
extern IMPNAME(g_VM)                    ; are a bit confusing at first... :-)
extern NAME(CPUMGCRestoreInt)
extern NAME(cpumHandleLazyFPUAsm)
extern NAME(CPUMHyperSetCtxCore)
extern NAME(trpmGCTrapInGeneric)
extern NAME(TRPMGCHyperTrap0bHandler)
extern NAME(TRPMGCHyperTrap0dHandler)
extern NAME(TRPMGCHyperTrap0eHandler)
extern NAME(TRPMGCTrap01Handler)
%ifdef VBOX_WITH_NMI
extern NAME(TRPMGCTrap02Handler)
%endif
extern NAME(TRPMGCTrap03Handler)
extern NAME(TRPMGCTrap06Handler)
extern NAME(TRPMGCTrap0bHandler)
extern NAME(TRPMGCTrap0dHandler)
extern NAME(TRPMGCTrap0eHandler)
extern NAME(TRPMGCTrap07Handler)

;; IMPORTANT all COM_ functions trashes esi, some edi and the LOOP_SHORT_WHILE kills ecx.
;%define DEBUG_STUFF 1
;%define DEBUG_STUFF_TRPG 1
;%define DEBUG_STUFF_INT 1

BEGINCODE

;;
; Jump table for trap handlers for hypervisor traps.
;
g_apfnStaticTrapHandlersHyper:
                                        ; N  - M M -  T  - C - D i
                                        ; o  - n o -  y  - o - e p
                                        ;    - e n -  p  - d - s t
                                        ;    -   i -  e  - e - c .
                                        ;    -   c -     -   - r
                                        ; =============================================================
    dd 0                                ;  0 - #DE - F   - N - Divide error
    dd NAME(TRPMGCTrap01Handler)        ;  1 - #DB - F/T - N - Single step, INT 1 instruction
%ifdef VBOX_WITH_NMI
    dd NAME(TRPMGCTrap02Handler)        ;  2 -     - I   - N - Non-Maskable Interrupt (NMI)
%else
    dd 0                                ;  2 -     - I   - N - Non-Maskable Interrupt (NMI)
%endif
    dd NAME(TRPMGCTrap03Handler)        ;  3 - #BP - T   - N - Breakpoint, INT 3 instruction.
    dd 0                                ;  4 - #OF - T   - N - Overflow, INTO instruction.
    dd 0                                ;  5 - #BR - F   - N - BOUND Range Exceeded, BOUND instruction.
    dd 0                                ;  6 - #UD - F   - N - Undefined(/Invalid) Opcode.
    dd 0                                ;  7 - #NM - F   - N - Device not available, FP or (F)WAIT instruction.
    dd 0                                ;  8 - #DF - A   - 0 - Double fault.
    dd 0                                ;  9 -     - F   - N - Coprocessor Segment Overrun (obsolete).
    dd 0                                ;  a - #TS - F   - Y - Invalid TSS, Taskswitch or TSS access.
    dd NAME(TRPMGCHyperTrap0bHandler)   ;  b - #NP - F   - Y - Segment not present.
    dd 0                                ;  c - #SS - F   - Y - Stack-Segment fault.
    dd NAME(TRPMGCHyperTrap0dHandler)   ;  d - #GP - F   - Y - General protection fault.
    dd NAME(TRPMGCHyperTrap0eHandler)   ;  e - #PF - F   - Y - Page fault.
    dd 0                                ;  f -     -     -   - Intel Reserved. Do not use.
    dd 0                                ; 10 - #MF - F   - N - x86 FPU Floating-Point Error (Math fault), FP or (F)WAIT instruction.
    dd 0                                ; 11 - #AC - F   - 0 - Alignment Check.
    dd 0                                ; 12 - #MC - A   - N - Machine Check.
    dd 0                                ; 13 - #XF - F   - N - SIMD Floating-Point Exception.
    dd 0                                ; 14 -     -     -   - Intel Reserved. Do not use.
    dd 0                                ; 15 -     -     -   - Intel Reserved. Do not use.
    dd 0                                ; 16 -     -     -   - Intel Reserved. Do not use.
    dd 0                                ; 17 -     -     -   - Intel Reserved. Do not use.
    dd 0                                ; 18 -     -     -   - Intel Reserved. Do not use.


;;
; Jump table for trap handlers for guest traps
;
g_apfnStaticTrapHandlersGuest:
                                        ; N  - M M -  T  - C - D i
                                        ; o  - n o -  y  - o - e p
                                        ;    - e n -  p  - d - s t
                                        ;    -   i -  e  - e - c .
                                        ;    -   c -     -   - r
                                        ; =============================================================
    dd 0                                ;  0 - #DE - F   - N - Divide error
    dd NAME(TRPMGCTrap01Handler)        ;  1 - #DB - F/T - N - Single step, INT 1 instruction
%ifdef VBOX_WITH_NMI
    dd NAME(TRPMGCTrap02Handler)        ;  2 -     - I   - N - Non-Maskable Interrupt (NMI)
%else
    dd 0                                ;  2 -     - I   - N - Non-Maskable Interrupt (NMI)
%endif
    dd NAME(TRPMGCTrap03Handler)        ;  3 - #BP - T   - N - Breakpoint, INT 3 instruction.
    dd 0                                ;  4 - #OF - T   - N - Overflow, INTO instruction.
    dd 0                                ;  5 - #BR - F   - N - BOUND Range Exceeded, BOUND instruction.
    dd NAME(TRPMGCTrap06Handler)        ;  6 - #UD - F   - N - Undefined(/Invalid) Opcode.
    dd NAME(TRPMGCTrap07Handler)        ;  7 - #NM - F   - N - Device not available, FP or (F)WAIT instruction.
    dd 0                                ;  8 - #DF - A   - 0 - Double fault.
    dd 0                                ;  9 -     - F   - N - Coprocessor Segment Overrun (obsolete).
    dd 0                                ;  a - #TS - F   - Y - Invalid TSS, Taskswitch or TSS access.
    dd NAME(TRPMGCTrap0bHandler)        ;  b - #NP - F   - Y - Segment not present.
    dd 0                                ;  c - #SS - F   - Y - Stack-Segment fault.
    dd NAME(TRPMGCTrap0dHandler)        ;  d - #GP - F   - Y - General protection fault.
    dd NAME(TRPMGCTrap0eHandler)        ;  e - #PF - F   - Y - Page fault.
    dd 0                                ;  f -     -     -   - Intel Reserved. Do not use.
    dd 0                                ; 10 - #MF - F   - N - x86 FPU Floating-Point Error (Math fault), FP or (F)WAIT instruction.
    dd 0                                ; 11 - #AC - F   - 0 - Alignment Check.
    dd 0                                ; 12 - #MC - A   - N - Machine Check.
    dd 0                                ; 13 - #XF - F   - N - SIMD Floating-Point Exception.
    dd 0                                ; 14 -     -     -   - Intel Reserved. Do not use.
    dd 0                                ; 15 -     -     -   - Intel Reserved. Do not use.
    dd 0                                ; 16 -     -     -   - Intel Reserved. Do not use.
    dd 0                                ; 17 -     -     -   - Intel Reserved. Do not use.
    dd 0                                ; 18 -     -     -   - Intel Reserved. Do not use.



;;
; We start by 24 push <vector no.> + jmp <generic entry point>
;
ALIGNCODE(16)
BEGINPROC_EXPORTED TRPMGCHandlerGeneric
%macro TRPMGenericEntry 1
    db 06ah, i                          ; push imm8 - note that this is a signextended value.
    jmp   %1
    ALIGNCODE(8)
%assign i i+1
%endmacro

%assign i 0                             ; start counter.
    TRPMGenericEntry GenericTrap        ; 0
    TRPMGenericEntry GenericTrap        ; 1
    TRPMGenericEntry GenericTrap        ; 2
    TRPMGenericEntry GenericTrap        ; 3
    TRPMGenericEntry GenericTrap        ; 4
    TRPMGenericEntry GenericTrap        ; 5
    TRPMGenericEntry GenericTrap        ; 6
    TRPMGenericEntry GenericTrap        ; 7
    TRPMGenericEntry GenericTrapErrCode ; 8
    TRPMGenericEntry GenericTrap        ; 9
    TRPMGenericEntry GenericTrapErrCode ; a
    TRPMGenericEntry GenericTrapErrCode ; b
    TRPMGenericEntry GenericTrapErrCode ; c
    TRPMGenericEntry GenericTrapErrCode ; d
    TRPMGenericEntry GenericTrapErrCode ; e
    TRPMGenericEntry GenericTrap        ; f  (reserved)
    TRPMGenericEntry GenericTrap        ; 10
    TRPMGenericEntry GenericTrapErrCode ; 11
    TRPMGenericEntry GenericTrap        ; 12
    TRPMGenericEntry GenericTrap        ; 13
    TRPMGenericEntry GenericTrap        ; 14 (reserved)
    TRPMGenericEntry GenericTrap        ; 15 (reserved)
    TRPMGenericEntry GenericTrap        ; 16 (reserved)
    TRPMGenericEntry GenericTrap        ; 17 (reserved)
%undef i
%undef TRPMGenericEntry

;;
; Main exception handler for the guest context
;
; Stack:
;           14  SS
;           10  ESP
;            c  EFLAGS
;            8  CS
;            4  EIP
;            0  vector number
;
; @uses     none
;
ALIGNCODE(8)
GenericTrap:
    ;
    ; for the present we fake an error code ~0
    ;
    push    eax
    mov     eax, 0ffffffffh
    xchg    [esp + 4], eax              ; get vector number, set error code
    xchg    [esp], eax                  ; get saved eax, set vector number
    jmp short GenericTrapErrCode


;;
; Main exception handler for the guest context with error code
;
; Stack:
;           28  GS          (V86 only)
;           24  FS          (V86 only)
;           20  DS          (V86 only)
;           1C  ES          (V86 only)
;           18  SS          (only if ring transition.)
;           14  ESP         (only if ring transition.)
;           10  EFLAGS
;            c  CS
;            8  EIP
;            4  Error code. (~0 for vectors which don't take an error code.)
;            0  vector number
;
; Error code:
;
; 31          16    15      3       2           1           0
;
; reserved          segment         TI          IDT         EXT
;                   selector        GDT/LDT     (1) IDT     External interrupt
;                   index           (IDT=0)     index
;
; NOTE: Page faults (trap 14) have a different error code
;
; @uses     none
;
ALIGNCODE(8)
GenericTrapErrCode:
    cld

    ;
    ; Setup CPUMCTXCORE frame
    ;
    ;   ASSUMPTION: If trap in hypervisor, we assume that we can read two dword
    ;               under the bottom of the stack. This is atm safe.
    ;   ASSUMPTION: There is sufficient stack space.
    ;   ASSUMPTION: The stack is not write protected.
    ;
%define ESPOFF CPUMCTXCORE_size

    sub     esp, CPUMCTXCORE_size
    mov     [esp + CPUMCTXCORE.eax], eax
    mov     [esp + CPUMCTXCORE.ecx], ecx
    mov     [esp + CPUMCTXCORE.edx], edx
    mov     [esp + CPUMCTXCORE.ebx], ebx
    mov     [esp + CPUMCTXCORE.esi], esi
    mov     [esp + CPUMCTXCORE.edi], edi
    mov     [esp + CPUMCTXCORE.ebp], ebp

    mov     eax, [esp + 14h + ESPOFF]           ; esp
    mov     [esp + CPUMCTXCORE.esp], eax
%if GC_ARCH_BITS == 64
    ; zero out the high dword
    mov     dword [esp + CPUMCTXCORE.esp + 4], 0
%endif
    mov     eax, [esp + 18h + ESPOFF]           ; ss
    mov     dword [esp + CPUMCTXCORE.ss], eax

    mov     eax, [esp + 0ch + ESPOFF]           ; cs
    mov     dword [esp + CPUMCTXCORE.cs], eax
    mov     eax, [esp + 08h + ESPOFF]           ; eip
    mov     [esp + CPUMCTXCORE.eip], eax
%if GC_ARCH_BITS == 64
    ; zero out the high dword
    mov     dword [esp + CPUMCTXCORE.eip + 4], 0
%endif
    mov     eax, [esp + 10h + ESPOFF]           ; eflags
    mov     [esp + CPUMCTXCORE.eflags], eax

    mov     eax, es
    mov     dword [esp + CPUMCTXCORE.es], eax
    mov     eax, ds
    mov     dword [esp + CPUMCTXCORE.ds], eax
    mov     eax, fs
    mov     dword [esp + CPUMCTXCORE.fs], eax
    mov     eax, gs
    mov     dword [esp + CPUMCTXCORE.gs], eax

    test    dword [esp + CPUMCTXCORE.eflags], X86_EFL_VM
    jz short gt_SkipV86Entry

    ;
    ; The DS, ES, FS and GS registers are zeroed in V86 mode and their real values are on the stack
    ;
    mov     eax, dword [esp + ESPOFF + 1Ch]
    mov     dword [esp + CPUMCTXCORE.es], eax

    mov     eax, dword [esp + ESPOFF + 20h]
    mov     dword [esp + CPUMCTXCORE.ds], eax

    mov     eax, dword [esp + ESPOFF + 24h]
    mov     dword [esp + CPUMCTXCORE.fs], eax

    mov     eax, dword [esp + ESPOFF + 28h]
    mov     dword [esp + CPUMCTXCORE.gs], eax

gt_SkipV86Entry:
    ;
    ; Disable Ring-0 WP
    ;
    mov     eax, cr0                    ;; @todo elimitate this read?
    and     eax, ~X86_CR0_WRITE_PROTECT
    mov     cr0, eax

    ;
    ; Load Hypervisor DS and ES (get it from the SS)
    ;
    mov     eax, ss
    mov     ds, eax
    mov     es, eax

%ifdef VBOX_WITH_STATISTICS
    ;
    ; Start profiling.
    ;
    mov     edx, [esp +  0h + ESPOFF]   ; vector number
    imul    edx, edx, byte STAMPROFILEADV_size ; assumes < 128.
    add     edx, TRPM.aStatGCTraps
    add     edx, IMP(g_TRPM)
    STAM_PROFILE_ADV_START edx
%endif

    ;
    ; Store the information about the active trap/interrupt.
    ;
    mov     eax, IMP(g_TRPM)
    movzx   edx, byte [esp + 0h + ESPOFF]  ; vector number
    mov     [eax + TRPM.uActiveVector], edx
    mov     edx, [esp + 4h + ESPOFF]       ; error code
    mov     [eax + TRPM.uActiveErrorCode], edx
    mov     dword [eax + TRPM.enmActiveType], TRPM_TRAP
    mov     edx, cr2                       ;; @todo Check how expensive cr2 reads are!
    mov     dword [eax + TRPM.uActiveCR2], edx

%if GC_ARCH_BITS == 64
    ; zero out the high dword
    mov     dword [eax + TRPM.uActiveErrorCode + 4], 0
    mov     dword [eax + TRPM.uActiveCR2 + 4], 0
%endif

    ;
    ; Check if we're in Hypervisor when this happend.
    ;
    test    dword [esp + CPUMCTXCORE.eflags], X86_EFL_VM
    jnz short gt_NotHyperVisor

    test    byte [esp + 0ch + ESPOFF], 3h ; check CPL of the cs selector
    jz      near gt_InHypervisor

    ;
    ; Trap in guest code.
    ;
gt_NotHyperVisor:
%ifdef DEBUG_STUFF_TRPG
    mov     ebx, [esp +  4h + ESPOFF]   ; error code
    mov     ecx, 'trpG'                 ; indicate trap.
    mov     edx, [esp +  0h + ESPOFF]   ; vector number
    lea     eax, [esp]
    call    trpmDbgDumpRegisterFrame
%endif

    ;
    ; Do we have a GC handler for these traps?
    ;
    mov     edx, [esp +  0h + ESPOFF]   ; vector number
    mov     eax, [g_apfnStaticTrapHandlersGuest + edx * 4]
    or      eax, eax
    jnz short gt_HaveHandler
    mov     eax, VINF_EM_RAW_GUEST_TRAP
    jmp short gt_GuestTrap

    ;
    ; Call static handler.
    ;
gt_HaveHandler:
    push    esp                         ; Param 2 - CPUMCTXCORE pointer.
    push    dword IMP(g_TRPM)           ; Param 1 - Pointer to TRPM
    call    eax
    add     esp, byte 8                 ; cleanup stack (cdecl)
    or      eax, eax
    je near gt_continue_guest

    ;
    ; Switch back to the host and process it there.
    ;
gt_GuestTrap:
%ifdef VBOX_WITH_STATISTICS
    mov     edx, [esp +  0h + ESPOFF]   ; vector number
    imul    edx, edx, byte STAMPROFILEADV_size ; assume < 128
    add     edx, IMP(g_TRPM)
    add     edx, TRPM.aStatGCTraps
    STAM_PROFILE_ADV_STOP edx
%endif
    mov     edx, IMP(g_VM)
    call    [edx + VM.pfnVMMGCGuestToHostAsmGuestCtx]

    ;; @todo r=bird: is this path actually every taken? if not we should replace this code with a panic.
    ;
    ; We've returned!
    ; N.B. The stack has been changed now! No CPUMCTXCORE any longer. esp = vector number.
    ; N.B. Current scheduling design causes this code path to be unused.
    ; N.B. Better not use it when in V86 mode!
    ;

    ; Enable WP
    mov     eax, cr0                    ;; @todo try elimiate this read.
    or      eax, X86_CR0_WRITE_PROTECT
    mov     cr0, eax
    ; Restore guest context and continue execution.
    mov     edx, IMP(g_CPUM)
    lea     eax, [esp + 8]
    push    eax
    call    NAME(CPUMGCRestoreInt)
    lea     esp, [esp + 0ch]            ; cleanup call and skip vector & error code.

    iret


    ;
    ; Continue(/Resume/Restart/Whatever) guest execution.
    ;
ALIGNCODE(16)
gt_continue_guest:
%ifdef VBOX_WITH_STATISTICS
    mov     edx, [esp +  0h + ESPOFF]   ; vector number
    imul    edx, edx, byte STAMPROFILEADV_size ; assumes < 128
    add     edx, TRPM.aStatGCTraps
    add     edx, IMP(g_TRPM)
    STAM_PROFILE_ADV_STOP edx
%endif

    ; enable WP
    mov     eax, cr0                    ;; @todo try elimiate this read.
    or      eax, X86_CR0_WRITE_PROTECT
    mov     cr0, eax

    ; restore guest state and start executing again.
    test    dword [esp + CPUMCTXCORE.eflags], X86_EFL_VM
    jnz     gt_V86Return

    mov     ecx, [esp + CPUMCTXCORE.ecx]
    mov     edx, [esp + CPUMCTXCORE.edx]
    mov     ebx, [esp + CPUMCTXCORE.ebx]
    mov     ebp, [esp + CPUMCTXCORE.ebp]
    mov     esi, [esp + CPUMCTXCORE.esi]
    mov     edi, [esp + CPUMCTXCORE.edi]

    mov     eax, [esp + CPUMCTXCORE.esp]
    mov     [esp + 14h + ESPOFF], eax           ; esp
    mov     eax, dword [esp + CPUMCTXCORE.ss]
    mov     [esp + 18h + ESPOFF], eax           ; ss

    mov     eax, dword [esp + CPUMCTXCORE.gs]
    TRPM_NP_GP_HANDLER NAME(trpmGCTrapInGeneric), TRPM_TRAP_IN_MOV_GS
    mov     gs, eax

    mov     eax, dword [esp + CPUMCTXCORE.fs]
    TRPM_NP_GP_HANDLER NAME(trpmGCTrapInGeneric), TRPM_TRAP_IN_MOV_FS
    mov     fs, eax

    mov     eax, dword [esp + CPUMCTXCORE.es]
    TRPM_NP_GP_HANDLER NAME(trpmGCTrapInGeneric), TRPM_TRAP_IN_MOV_ES
    mov     es, eax

    mov     eax, dword [esp + CPUMCTXCORE.ds]
    TRPM_NP_GP_HANDLER NAME(trpmGCTrapInGeneric), TRPM_TRAP_IN_MOV_DS
    mov     ds, eax

    mov     eax, dword [esp + CPUMCTXCORE.cs]
    mov     [esp + 0ch + ESPOFF], eax           ; cs
    mov     eax, [esp + CPUMCTXCORE.eflags]
    mov     [esp + 10h + ESPOFF], eax           ; eflags
    mov     eax, [esp + CPUMCTXCORE.eip]
    mov     [esp + 08h + ESPOFF], eax           ; eip

    ; finally restore our scratch register eax
    mov     eax, [esp + CPUMCTXCORE.eax]

    add     esp, ESPOFF + 8                     ; skip CPUMCTXCORE structure, error code and vector number

    TRPM_NP_GP_HANDLER NAME(trpmGCTrapInGeneric), TRPM_TRAP_IN_IRET
    iret

ALIGNCODE(16)
gt_V86Return:
    mov     ecx, [esp + CPUMCTXCORE.ecx]
    mov     edx, [esp + CPUMCTXCORE.edx]
    mov     ebx, [esp + CPUMCTXCORE.ebx]
    mov     ebp, [esp + CPUMCTXCORE.ebp]
    mov     esi, [esp + CPUMCTXCORE.esi]
    mov     edi, [esp + CPUMCTXCORE.edi]

    mov     eax, [esp + CPUMCTXCORE.esp]
    mov     [esp + 14h + ESPOFF], eax           ; esp
    mov     eax, dword [esp + CPUMCTXCORE.ss]
    mov     [esp + 18h + ESPOFF], eax           ; ss

    mov     eax, dword [esp + CPUMCTXCORE.es]
    mov     [esp + 1ch + ESPOFF], eax           ; es
    mov     eax, dword [esp + CPUMCTXCORE.ds]
    mov     [esp + 20h + ESPOFF], eax           ; ds
    mov     eax, dword [esp + CPUMCTXCORE.fs]
    mov     [esp + 24h + ESPOFF], eax           ; fs
    mov     eax, dword [esp + CPUMCTXCORE.gs]
    mov     [esp + 28h + ESPOFF], eax           ; gs

    mov     eax, [esp + CPUMCTXCORE.eip]
    mov     [esp + 08h + ESPOFF], eax           ; eip
    mov     eax, dword [esp + CPUMCTXCORE.cs]
    mov     [esp + 0ch + ESPOFF], eax           ; cs
    mov     eax, [esp + CPUMCTXCORE.eflags]
    mov     [esp + 10h + ESPOFF], eax           ; eflags

    ; finally restore our scratch register eax
    mov     eax, [esp + CPUMCTXCORE.eax]

    add     esp, ESPOFF + 8                     ; skip CPUMCTXCORE structure, error code and vector number

    TRPM_NP_GP_HANDLER NAME(trpmGCTrapInGeneric), TRPM_TRAP_IN_IRET | TRPM_TRAP_IN_V86
    iret

    ;
    ; Trap in Hypervisor, try to handle it.
    ;
    ;   (eax = pTRPM)
    ;
ALIGNCODE(16)
gt_InHypervisor:
    ; fix ss:esp.
    lea     ebx, [esp + 14h + ESPOFF]   ; calc esp at trap
    mov     [esp + CPUMCTXCORE.esp], ebx; update esp in register frame
    mov     [esp + CPUMCTXCORE.ss], ss  ; update ss in register frame

    ; tell cpum about the context core.
    xchg    esi, eax                    ; save pTRPM - @todo reallocate this variable to esi, edi, or ebx
    push    esp                         ; Param 2 - The new CPUMCTXCORE pointer.
    mov     eax, IMP(g_VM)              ; Param 1 - Pointer to the VMCPU.
    add     eax, [eax + VM.offVMCPU]
    push    eax
    call    NAME(CPUMHyperSetCtxCore)
    add     esp, byte 8                 ; stack cleanup (cdecl)
    xchg    eax, esi                    ; restore pTRPM

    ; check for temporary handler.
    movzx   ebx, byte [eax + TRPM.uActiveVector]
    xor     ecx, ecx
    xchg    ecx, [eax + TRPM.aTmpTrapHandlers + ebx * 4]    ; ecx = Temp handler pointer or 0
    or      ecx, ecx
    jnz short gt_Hyper_HaveTemporaryHandler

    ; check for static trap handler.
    mov     ecx, [g_apfnStaticTrapHandlersHyper + ebx * 4]  ; ecx = Static handler pointer or 0
    or      ecx, ecx
    jnz short gt_Hyper_HaveStaticHandler
    jmp     gt_Hyper_AbandonShip


    ;
    ; Temporary trap handler present, call it (CDECL).
    ;
gt_Hyper_HaveTemporaryHandler:
    push    esp                         ; Param 2 - Pointer to CPUMCTXCORE.
    push    IMP(g_VM)                   ; Param 1 - Pointer to VM.
    call    ecx
    add     esp, byte 8                 ; cleanup stack (cdecl)

    cmp     eax, byte VINF_SUCCESS      ; If completely handled Then resume execution.
    je      near gt_Hyper_Continue
    ;; @todo Handle ALL returns types from temporary handlers!
    jmp     gt_Hyper_AbandonShip


    ;
    ; Static trap handler present, call it (CDECL).
    ;
gt_Hyper_HaveStaticHandler:
    push    esp                         ; Param 2 - Pointer to CPUMCTXCORE.
    push    eax                         ; Param 1 - Pointer to TRPM
    call    ecx
    add     esp, byte 8                 ; cleanup stack (cdecl)

    cmp     eax, byte VINF_SUCCESS      ; If completely handled Then resume execution.
    je short gt_Hyper_Continue
    cmp     eax, VINF_EM_DBG_HYPER_STEPPED
    je short gt_Hyper_ToHost
    cmp     eax, VINF_EM_DBG_HYPER_BREAKPOINT
    je short gt_Hyper_ToHost
    cmp     eax, VINF_EM_DBG_HYPER_ASSERTION
    je short gt_Hyper_ToHost
    jmp     gt_Hyper_AbandonShip

    ;
    ; Pop back to the host to service the error.
    ;
gt_Hyper_ToHost:
    mov     ecx, esp
    mov     edx, IMP(g_VM)
    call    [edx + VM.pfnVMMGCGuestToHostAsm]
    jmp short gt_Hyper_Continue

    ;
    ; Continue(/Resume/Restart/Whatever) hypervisor execution.
    ; Don't reset the TRPM state. Caller takes care of that.
    ;
ALIGNCODE(16)
gt_Hyper_Continue:
%ifdef DEBUG_STUFF
    mov     ebx, [esp +  4h + ESPOFF]   ; error code
    mov     ecx, 'resH'                 ; indicate trap.
    mov     edx, [esp +  0h + ESPOFF]   ; vector number
    lea     eax, [esp]
    call    trpmDbgDumpRegisterFrame
%endif
    ; tell CPUM to use the default CPUMCTXCORE.
    push    byte 0                      ; Param 2 - NULL indicating use default context core.
    mov     eax, IMP(g_VM)              ; Param 1 - Pointer to the VMCPU.
    add     eax, [eax + VM.offVMCPU]
    push    eax
    call    NAME(CPUMHyperSetCtxCore)
    add     esp, byte 8                 ; stack cleanup (cdecl)

%ifdef VBOX_WITH_STATISTICS
    mov     edx, [esp +  0h + ESPOFF]   ; vector number
    imul    edx, edx, byte STAMPROFILEADV_size ; assumes < 128
    add     edx, TRPM.aStatGCTraps
    add     edx, IMP(g_TRPM)
    STAM_PROFILE_ADV_STOP edx
%endif

    ; restore
    mov     ecx, [esp + CPUMCTXCORE.ecx]
    mov     edx, [esp + CPUMCTXCORE.edx]
    mov     ebx, [esp + CPUMCTXCORE.ebx]
    mov     ebp, [esp + CPUMCTXCORE.ebp]
    mov     esi, [esp + CPUMCTXCORE.esi]
    mov     edi, [esp + CPUMCTXCORE.edi]

    mov     eax, dword [esp + CPUMCTXCORE.gs]
    TRPM_NP_GP_HANDLER NAME(trpmGCTrapInGeneric), TRPM_TRAP_IN_MOV_GS | TRPM_TRAP_IN_HYPER
    mov     gs, eax

    mov     eax, dword [esp + CPUMCTXCORE.fs]
    TRPM_NP_GP_HANDLER NAME(trpmGCTrapInGeneric), TRPM_TRAP_IN_MOV_FS | TRPM_TRAP_IN_HYPER
    mov     fs, eax

    mov     eax, dword [esp + CPUMCTXCORE.es]
    mov     es, eax
    mov     eax, dword [esp + CPUMCTXCORE.ds]
    mov     ds, eax

    ; skip esp & ss

    mov     eax, [esp + CPUMCTXCORE.eip]
    mov     [esp + 08h + ESPOFF], eax           ; eip
    mov     eax, dword [esp + CPUMCTXCORE.cs]
    mov     [esp + 0ch + ESPOFF], eax           ; cs
    mov     eax, [esp + CPUMCTXCORE.eflags]
    mov     [esp + 10h + ESPOFF], eax           ; eflags

    ; finally restore our scratch register eax
    mov     eax, [esp + CPUMCTXCORE.eax]

    add     esp, ESPOFF + 8                     ; skip CPUMCTXCORE structure, error code and vector number

    iret


    ;
    ; ABANDON SHIP! DON'T PANIC!
    ;
gt_Hyper_AbandonShip:
%ifdef DEBUG_STUFF
    mov     ebx, [esp +  4h + ESPOFF]   ; error code
    mov     ecx, 'trpH'                 ; indicate trap.
    mov     edx, [esp +  0h + ESPOFF]   ; vector number
    lea     eax, [esp]
    call    trpmDbgDumpRegisterFrame
%endif

gt_Hyper_DontPanic:
    mov     ecx, esp
    mov     edx, IMP(g_VM)
    mov     eax, VERR_TRPM_DONT_PANIC
    call    [edx + VM.pfnVMMGCGuestToHostAsmHyperCtx]
%ifdef DEBUG_STUFF
    COM_S_PRINT 'bad!!!'
%endif
    jmp     gt_Hyper_DontPanic          ; this shall never ever happen!
%undef ESPOFF
ENDPROC TRPMGCHandlerGeneric





;;
; We start by 256 push <vector no.> + jmp interruptworker
;
ALIGNCODE(16)
BEGINPROC_EXPORTED TRPMGCHandlerInterupt
    ; NASM has some nice features, here an example of a loop.
%assign i 0
%rep 256
    db 06ah, i   ; push imm8 - note that this is a signextended value.
    jmp   ti_GenericInterrupt
    ALIGNCODE(8)
%assign i i+1
%endrep

;;
; Main interrupt handler for the guest context
;
; Stack:
;        24 GS          (V86 only)
;        20 FS          (V86 only)
;        1C DS          (V86 only)
;        18 ES          (V86 only)
;        14 SS
;        10 ESP
;         c EFLAGS
;         8 CS
;         4 EIP
; ESP ->  0 Vector number (only use low byte!).
;
; @uses     none
ti_GenericInterrupt:
    cld

    ;
    ; Setup CPUMCTXCORE frame
    ;
    ;   ASSUMPTION: If trap in hypervisor, we assume that we can read two dword
    ;               under the bottom of the stack. This is atm safe.
    ;   ASSUMPTION: There is sufficient stack space.
    ;   ASSUMPTION: The stack is not write protected.
    ;
%define ESPOFF CPUMCTXCORE_size

    sub     esp, CPUMCTXCORE_size
    mov     [esp + CPUMCTXCORE.eax], eax
    mov     [esp + CPUMCTXCORE.ecx], ecx
    mov     [esp + CPUMCTXCORE.edx], edx
    mov     [esp + CPUMCTXCORE.ebx], ebx
    mov     [esp + CPUMCTXCORE.esi], esi
    mov     [esp + CPUMCTXCORE.edi], edi
    mov     [esp + CPUMCTXCORE.ebp], ebp

    mov     eax, [esp + 04h + ESPOFF]           ; eip
    mov     [esp + CPUMCTXCORE.eip], eax
%if GC_ARCH_BITS == 64
    ; zero out the high dword
    mov     dword [esp + CPUMCTXCORE.eip + 4], 0
%endif
    mov     eax, dword [esp + 08h + ESPOFF]     ; cs
    mov     [esp + CPUMCTXCORE.cs], eax
    mov     eax, [esp + 0ch + ESPOFF]           ; eflags
    mov     [esp + CPUMCTXCORE.eflags], eax

    mov     eax, [esp + 10h + ESPOFF]           ; esp
    mov     [esp + CPUMCTXCORE.esp], eax
%if GC_ARCH_BITS == 64
    ; zero out the high dword
    mov     dword [esp + CPUMCTXCORE.esp + 4], 0
%endif
    mov     eax, dword [esp + 14h + ESPOFF]     ; ss
    mov     [esp + CPUMCTXCORE.ss], eax

    mov     eax, es
    mov     dword [esp + CPUMCTXCORE.es], eax
    mov     eax, ds
    mov     dword [esp + CPUMCTXCORE.ds], eax
    mov     eax, fs
    mov     dword [esp + CPUMCTXCORE.fs], eax
    mov     eax, gs
    mov     dword [esp + CPUMCTXCORE.gs], eax

    test    dword [esp + CPUMCTXCORE.eflags], X86_EFL_VM
    jz short ti_SkipV86Entry

    ;
    ; The DS, ES, FS and GS registers are zeroed in V86 mode and their real values are on the stack
    ;
    mov     eax, dword [esp + ESPOFF + 18h]
    mov     dword [esp + CPUMCTXCORE.es], eax

    mov     eax, dword [esp + ESPOFF + 1Ch]
    mov     dword [esp + CPUMCTXCORE.ds], eax

    mov     eax, dword [esp + ESPOFF + 20h]
    mov     dword [esp + CPUMCTXCORE.fs], eax

    mov     eax, dword [esp + ESPOFF + 24h]
    mov     dword [esp + CPUMCTXCORE.gs], eax

ti_SkipV86Entry:

    ;
    ; Disable Ring-0 WP
    ;
    mov     eax, cr0                    ;; @todo try elimiate this read.
    and     eax, ~X86_CR0_WRITE_PROTECT
    mov     cr0, eax

    ;
    ; Load Hypervisor DS and ES (get it from the SS)
    ;
    mov     eax, ss
    mov     ds, eax
    mov     es, eax

    ;
    ; Store the information about the active trap/interrupt.
    ;
    mov     eax, IMP(g_TRPM)
    movzx   edx, byte [esp + 0h + ESPOFF]  ; vector number
    mov     [eax + TRPM.uActiveVector], edx
    xor     edx, edx
    mov     dword [eax + TRPM.enmActiveType], TRPM_HARDWARE_INT
    dec     edx
    mov     [eax + TRPM.uActiveErrorCode], edx
    mov     [eax + TRPM.uActiveCR2], edx
%if GC_ARCH_BITS == 64
    ; zero out the high dword
    mov     dword [eax + TRPM.uActiveErrorCode + 4], 0
    mov     dword [eax + TRPM.uActiveCR2 + 4], 0
%endif

    ;
    ; Check if we're in Hypervisor when this happend.
    ;
    test    byte [esp + 08h + ESPOFF], 3h ; check CPL of the cs selector
    jnz short gi_NotHyperVisor
    jmp     gi_HyperVisor

    ;
    ; Trap in guest code.
    ;
gi_NotHyperVisor:
    and     dword [esp + CPUMCTXCORE.eflags], ~010000h ; Clear RF (Resume Flag). @todo make %defines for eflags.
                                           ; The guest shall not see this in it's state.
%ifdef DEBUG_STUFF_INT
    mov     ecx, 'intG'                    ; indicate trap.
    movzx   edx, byte [esp +  0h + ESPOFF] ; vector number
    lea     eax, [esp]
    call    trpmDbgDumpRegisterFrame
%endif

    ;
    ; Switch back to the host and process it there.
    ;
    mov     edx, IMP(g_VM)
    mov     eax, VINF_EM_RAW_INTERRUPT
    call    [edx + VM.pfnVMMGCGuestToHostAsmGuestCtx]

    ;
    ; We've returned!
    ; NOTE that the stack has been changed now!
    ;      there is no longer any CPUMCTXCORE around and esp points to vector number!
    ;
    ; Reset TRPM state
    mov     eax, IMP(g_TRPM)
    xor     edx, edx
    dec     edx                         ; edx = 0ffffffffh
    xchg    [eax + TRPM.uActiveVector], edx
    mov     [eax + TRPM.uPrevVector], edx

    ; Enable WP
    mov     eax, cr0                    ;; @todo try elimiate this read.
    or      eax, X86_CR0_WRITE_PROTECT
    mov     cr0, eax
    ; restore guest context and continue execution.
    lea     eax, [esp + 8]
    push    eax
    call    NAME(CPUMGCRestoreInt)
    lea     esp, [esp + 0ch]            ; cleanup call and skip vector & error code.

    iret

    ; -+- Entry point -+-
    ;
    ; We're in hypervisor mode which means no guest context
    ; and special care to be taken to restore the hypervisor
    ; context correctely.
    ;
    ; ATM the only place this can happen is when entering a trap handler.
    ; We make ASSUMPTIONS about this in respects to the WP CR0 bit
    ;
gi_HyperVisor:
    lea     eax, [esp + 14h + ESPOFF]      ; calc esp at trap
    mov     [esp + CPUMCTXCORE.esp], eax   ; update esp in register frame
    mov     [esp + CPUMCTXCORE.ss], ss     ; update ss in register frame

%ifdef DEBUG_STUFF_INT
    mov     ebx, [esp +  4h + ESPOFF]      ; error code
    mov     ecx, 'intH'                    ; indicate hypervisor interrupt.
    movzx   edx, byte [esp +  0h + ESPOFF] ; vector number
    lea     eax, [esp]
    call    trpmDbgDumpRegisterFrame
%endif

    mov     ecx, esp
    mov     edx, IMP(g_VM)
    mov     eax, VINF_EM_RAW_INTERRUPT_HYPER
    call    [edx + VM.pfnVMMGCGuestToHostAsm]
%ifdef DEBUG_STUFF_INT
    COM_CHAR '!'
%endif

    ;
    ; We've returned!
    ;
    ; Reset TRPM state - don't record this.
    mov     eax, IMP(g_TRPM)
    mov     dword [eax + TRPM.uActiveVector], 0ffffffffh

    ;
    ; Restore the hypervisor context and return.
    ;
    mov     ecx, [esp + CPUMCTXCORE.ecx]
    mov     edx, [esp + CPUMCTXCORE.edx]
    mov     ebx, [esp + CPUMCTXCORE.ebx]
    mov     ebp, [esp + CPUMCTXCORE.ebp]
    mov     esi, [esp + CPUMCTXCORE.esi]
    mov     edi, [esp + CPUMCTXCORE.edi]

    ; In V86 mode DS, ES, FS & GS are restored by the iret
    test    dword [esp + CPUMCTXCORE.eflags], X86_EFL_VM
    jnz     short ti_SkipSelRegs

    mov     eax, [esp + CPUMCTXCORE.gs]
    TRPM_NP_GP_HANDLER NAME(trpmGCTrapInGeneric), TRPM_TRAP_IN_MOV_GS | TRPM_TRAP_IN_HYPER
    mov     gs, eax

    mov     eax, [esp + CPUMCTXCORE.fs]
    TRPM_NP_GP_HANDLER NAME(trpmGCTrapInGeneric), TRPM_TRAP_IN_MOV_FS | TRPM_TRAP_IN_HYPER
    mov     fs, eax

    mov     eax, [esp + CPUMCTXCORE.es]
    mov     es, eax
    mov     eax, [esp + CPUMCTXCORE.ds]
    mov     ds, eax

ti_SkipSelRegs:
    ; finally restore our scratch register eax
    mov     eax, [esp + CPUMCTXCORE.eax]

    ; skip esp, ss, cs, eip & eflags. Done by iret

    add     esp, ESPOFF + 4h                    ; skip CPUMCTXCORE structure & vector number.

    iret
%undef ESPOFF
ENDPROC TRPMGCHandlerInterupt



;;
; Trap handler for #MC
;
; This handler will forward the #MC to the host OS. Since this
; is generalized in the generic interrupt handler, we just disable
; interrupts and push vector number and jump to the generic code.
;
; Stack:
;           10  SS          (only if ring transition.)
;            c  ESP         (only if ring transition.)
;            8  EFLAGS
;            4  CS
;            0  EIP
;
; @uses     none
;
ALIGNCODE(16)
BEGINPROC_EXPORTED TRPMGCHandlerTrap12
    push    byte 12h
    jmp     ti_GenericInterrupt
ENDPROC TRPMGCHandlerTrap12




;;
; Trap handler for double fault (#DF).
;
; This is a special trap handler executes in separate task with own TSS, with
; one of the intermediate memory contexts instead of the shadow context.
; The handler will unconditionally print an report to the comport configured
; for the COM_S_* macros before attempting to return to the host. If it it ends
; up double faulting more than 10 times, it will simply cause an tripple fault
; to get us out of the mess.
;
; @param    esp         Half way down the hypvervisor stack + the trap frame.
; @param    ebp         Half way down the hypvervisor stack.
; @param    eflags      Interrupts disabled, nested flag is probably set (we don't care).
; @param    ecx         The address of the hypervisor TSS.
; @param    edi         Same as ecx.
; @param    eax         Same as ecx.
; @param    edx         Address of the VM structure.
; @param    esi         Same as edx.
; @param    ebx         Same as edx.
; @param    ss          Hypervisor DS.
; @param    ds          Hypervisor DS.
; @param    es          Hypervisor DS.
; @param    fs          0
; @param    gs          0
;
;
; @remark   To be able to catch errors with WP turned off, it is required that the
;           TSS GDT descriptor and the TSSes are writable (X86_PTE_RW). See SELM.cpp
;           for how to enable this.
;
; @remark   It is *not* safe to resume the VMM after a double fault. (At least not
;           without clearing the busy flag of the TssTrap8 and fixing whatever cause it.)
;
ALIGNCODE(16)
BEGINPROC_EXPORTED TRPMGCHandlerTrap08
    ; be careful.
    cli
    cld

    ;
    ; Load Hypervisor DS and ES (get it from the SS) - paranoia, but the TSS could be overwritten.. :)
    ;
    mov     eax, ss
    mov     ds, eax
    mov     es, eax

    COM_S_PRINT 10,13,'*** Guru Meditation 00000008 - Double Fault! ***',10,13

    ;
    ; Disable write protection.
    ;
    mov     eax, cr0
    and     eax, ~X86_CR0_WRITE_PROTECT
    mov     cr0, eax


    COM_S_PRINT 'VM='
    COM_S_DWORD_REG edx
    COM_S_PRINT '    prevTSS='
    COM_S_DWORD_REG ecx
    COM_S_PRINT '    prevCR3='
    mov    eax, [ecx + VBOXTSS.cr3]
    COM_S_DWORD_REG eax
    COM_S_PRINT '    prevLdtr='
    movzx  eax, word [ecx + VBOXTSS.selLdt]
    COM_S_DWORD_REG eax
    COM_S_NEWLINE

    ;
    ; Create CPUMCTXCORE structure.
    ;
    sub     esp, CPUMCTXCORE_size

    mov     eax, [ecx + VBOXTSS.eip]
    mov     [esp + CPUMCTXCORE.eip], eax
%if GC_ARCH_BITS == 64
    ; zero out the high dword
    mov     dword [esp + CPUMCTXCORE.eip + 4], 0
%endif
    mov     eax, [ecx + VBOXTSS.eflags]
    mov     [esp + CPUMCTXCORE.eflags], eax

    movzx   eax, word [ecx + VBOXTSS.cs]
    mov     dword [esp + CPUMCTXCORE.cs], eax
    movzx   eax, word [ecx + VBOXTSS.ds]
    mov     dword [esp + CPUMCTXCORE.ds], eax
    movzx   eax, word [ecx + VBOXTSS.es]
    mov     dword [esp + CPUMCTXCORE.es], eax
    movzx   eax, word [ecx + VBOXTSS.fs]
    mov     dword [esp + CPUMCTXCORE.fs], eax
    movzx   eax, word [ecx + VBOXTSS.gs]
    mov     dword [esp + CPUMCTXCORE.gs], eax
    movzx   eax, word [ecx + VBOXTSS.ss]
    mov     [esp + CPUMCTXCORE.ss], eax
    mov     eax, [ecx + VBOXTSS.esp]
    mov     [esp + CPUMCTXCORE.esp], eax
%if GC_ARCH_BITS == 64
    ; zero out the high dword
    mov     dword [esp + CPUMCTXCORE.esp + 4], 0
%endif
    mov     eax, [ecx + VBOXTSS.ecx]
    mov     [esp + CPUMCTXCORE.ecx], eax
    mov     eax, [ecx + VBOXTSS.edx]
    mov     [esp + CPUMCTXCORE.edx], eax
    mov     eax, [ecx + VBOXTSS.ebx]
    mov     [esp + CPUMCTXCORE.ebx], eax
    mov     eax, [ecx + VBOXTSS.eax]
    mov     [esp + CPUMCTXCORE.eax], eax
    mov     eax, [ecx + VBOXTSS.ebp]
    mov     [esp + CPUMCTXCORE.ebp], eax
    mov     eax, [ecx + VBOXTSS.esi]
    mov     [esp + CPUMCTXCORE.esi], eax
    mov     eax, [ecx + VBOXTSS.edi]
    mov     [esp + CPUMCTXCORE.edi], eax

    ;
    ; Show regs
    ;
    mov     ebx, 0ffffffffh
    mov     ecx, 'trpH'                 ; indicate trap.
    mov     edx, 08h                    ; vector number
    lea     eax, [esp]
    call    trpmDbgDumpRegisterFrame

    ;
    ; Should we try go back?
    ;
    inc     dword [df_Count]
    cmp     dword [df_Count], byte 10
    jb      df_to_host
    jmp     df_tripple_fault
df_Count: dd 0

    ;
    ; Try return to the host.
    ;
df_to_host:
    COM_S_PRINT 'Trying to return to host...',10,13
    mov     ecx, esp
    mov     edx, IMP(g_VM)
    mov     eax, VERR_TRPM_PANIC
    call    [edx + VM.pfnVMMGCGuestToHostAsmHyperCtx]
    jmp short df_to_host

    ;
    ; Perform a tripple fault.
    ;
df_tripple_fault:
    COM_S_PRINT 'Giving up - tripple faulting the machine...',10,13
    push    byte 0
    push    byte 0
    sidt    [esp]
    mov     word [esp], 0
    lidt    [esp]
    xor     eax, eax
    mov     dword [eax], 0
    jmp     df_tripple_fault

ENDPROC TRPMGCHandlerTrap08




;;
; Internal procedure used to dump registers.
;
; @param    eax     Pointer to CPUMCTXCORE.
; @param    edx     Vector number
; @param    ecx     'trap' if trap, 'int' if interrupt.
; @param    ebx     Error code if trap.
;
trpmDbgDumpRegisterFrame:
    sub     esp, byte 8                 ; working space for sidt/sgdt/etc

;   Init _must_ be done on host before crashing!
;    push    edx
;    push    eax
;    COM_INIT
;    pop     eax
;    pop     edx

    cmp     ecx, 'trpH'
    je near tddrf_trpH
    cmp     ecx, 'trpG'
    je near tddrf_trpG
    cmp     ecx, 'intH'
    je near tddrf_intH
    cmp     ecx, 'intG'
    je near tddrf_intG
    cmp     ecx, 'resH'
    je near tddrf_resH
    COM_S_PRINT 10,13,'*** Bogus Dump Code '
    jmp     tddrf_regs

%if 1 ; the verbose version

tddrf_intG:
    COM_S_PRINT 10,13,'*** Interrupt (Guest) '
    COM_S_DWORD_REG edx
    jmp     tddrf_regs

tddrf_intH:
    COM_S_PRINT 10,13,'*** Interrupt (Hypervisor) '
    COM_S_DWORD_REG edx
    jmp     tddrf_regs

tddrf_trpG:
    COM_S_PRINT 10,13,'*** Trap '
    jmp     tddrf_trap_rest

%else ; the short version

tddrf_intG:
    COM_S_CHAR 'I'
    jmp     tddrf_ret

tddrf_intH:
    COM_S_CHAR 'i'
    jmp     tddrf_ret

tddrf_trpG:
    COM_S_CHAR 'T'
    jmp     tddrf_ret

%endif ; the short version

tddrf_trpH:
    COM_S_PRINT 10,13,'*** Guru Meditation '
    jmp     tddrf_trap_rest

tddrf_resH:
    COM_S_PRINT 10,13,'*** Resuming Hypervisor Trap '
    jmp     tddrf_trap_rest

tddrf_trap_rest:
    COM_S_DWORD_REG edx
    COM_S_PRINT ' ErrorCode='
    COM_S_DWORD_REG ebx
    COM_S_PRINT ' cr2='
    mov ecx, cr2
    COM_S_DWORD_REG ecx

tddrf_regs:
    COM_S_PRINT ' ***',10,13,'cs:eip='
    movzx   ecx, word [eax + CPUMCTXCORE.cs]
    COM_S_DWORD_REG ecx
    COM_S_CHAR ':'
    mov     ecx, [eax + CPUMCTXCORE.eip]
    COM_S_DWORD_REG ecx

    COM_S_PRINT '    ss:esp='
    movzx   ecx, word [eax + CPUMCTXCORE.ss]
    COM_S_DWORD_REG ecx
    COM_S_CHAR ':'
    mov     ecx, [eax + CPUMCTXCORE.esp]
    COM_S_DWORD_REG ecx


    sgdt    [esp]
    COM_S_PRINT 10,13,'  gdtr='
    movzx   ecx, word [esp]
    COM_S_DWORD_REG ecx
    COM_S_CHAR  ':'
    mov     ecx, [esp + 2]
    COM_S_DWORD_REG ecx

    sidt    [esp]
    COM_S_PRINT '      idtr='
    movzx   ecx, word [esp]
    COM_S_DWORD_REG ecx
    COM_S_CHAR  ':'
    mov     ecx, [esp + 2]
    COM_S_DWORD_REG ecx


    str     [esp]                       ; yasm BUG! it generates sldt [esp] here! YASMCHECK!
    COM_S_PRINT 10,13,' tr='
    movzx   ecx, word [esp]
    COM_S_DWORD_REG ecx

    sldt    [esp]
    COM_S_PRINT ' ldtr='
    movzx   ecx, word [esp]
    COM_S_DWORD_REG ecx

    COM_S_PRINT '  eflags='
    mov     ecx, [eax + CPUMCTXCORE.eflags]
    COM_S_DWORD_REG ecx


    COM_S_PRINT 10,13,'cr0='
    mov     ecx, cr0
    COM_S_DWORD_REG ecx

    COM_S_PRINT '  cr2='
    mov     ecx, cr2
    COM_S_DWORD_REG ecx

    COM_S_PRINT '  cr3='
    mov     ecx, cr3
    COM_S_DWORD_REG ecx
    COM_S_PRINT '  cr4='
    mov     ecx, cr4
    COM_S_DWORD_REG ecx


    COM_S_PRINT 10,13,' ds='
    movzx   ecx, word [eax + CPUMCTXCORE.ds]
    COM_S_DWORD_REG ecx

    COM_S_PRINT '   es='
    movzx   ecx, word [eax + CPUMCTXCORE.es]
    COM_S_DWORD_REG ecx

    COM_S_PRINT '   fs='
    movzx   ecx, word [eax + CPUMCTXCORE.fs]
    COM_S_DWORD_REG ecx

    COM_S_PRINT '   gs='
    movzx   ecx, word [eax + CPUMCTXCORE.gs]
    COM_S_DWORD_REG ecx


    COM_S_PRINT 10,13,'eax='
    mov     ecx, [eax + CPUMCTXCORE.eax]
    COM_S_DWORD_REG ecx

    COM_S_PRINT '  ebx='
    mov     ecx, [eax + CPUMCTXCORE.ebx]
    COM_S_DWORD_REG ecx

    COM_S_PRINT '  ecx='
    mov     ecx, [eax + CPUMCTXCORE.ecx]
    COM_S_DWORD_REG ecx

    COM_S_PRINT '  edx='
    mov     ecx, [eax + CPUMCTXCORE.edx]
    COM_S_DWORD_REG ecx


    COM_S_PRINT 10,13,'esi='
    mov     ecx, [eax + CPUMCTXCORE.esi]
    COM_S_DWORD_REG ecx

    COM_S_PRINT '  edi='
    mov     ecx, [eax + CPUMCTXCORE.edi]
    COM_S_DWORD_REG ecx

    COM_S_PRINT '  ebp='
    mov     ecx, [eax + CPUMCTXCORE.ebp]
    COM_S_DWORD_REG ecx


    COM_S_NEWLINE

tddrf_ret:
    add     esp, byte 8
    ret

