;; @file
;
; CPUM - Guest Context Assembly Routines.

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


;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "VBox/nasm.mac"
%include "VBox/vm.mac"
%include "VBox/err.mac"
%include "VBox/stam.mac"
%include "CPUMInternal.mac"
%include "VBox/x86.mac"
%include "VBox/cpum.mac"


;
; Enables write protection of Hypervisor memory pages.
; !note! Must be commented out for Trap8 debug handler.
;
%define ENABLE_WRITE_PROTECTION 1

;; @def CPUM_REG
; The register which we load the CPUM pointer into.
%ifdef __AMD64__
 %define CPUM_REG   rdx
%else
 %define CPUM_REG   edx
%endif

BEGINCODE


;;
; Handles lazy FPU saveing and restoring.
;
; This handler will implement lazy fpu (sse/mmx/stuff) saving.
; Two actions may be taken in this handler since the Guest OS may
; be doing lazy fpu switching. So, we'll have to generate those
; traps which the Guest CPU CTX shall have according to the
; its CR0 flags. If no traps for the Guest OS, we'll save the host
; context and restore the guest context.
;
; @returns  0 if caller should continue execution.
; @returns  VINF_EM_RAW_GUEST_TRAP if a guest trap should be generated.
; @param    pCPUM  x86:[esp+4] GCC:rdi MSC:rcx     CPUM pointer
;
align 16
BEGINPROC   CPUMHandleLazyFPUAsm
    ;
    ; Figure out what to do.
    ;
    ; There are two basic actions:
    ;   1. Save host fpu and restore guest fpu.
    ;   2. Generate guest trap.
    ;
    ; When entering the hypvervisor we'll always enable MP (for proper wait
    ; trapping) and TS (for intercepting all fpu/mmx/sse stuff). The EM flag
    ; is taken from the guest OS in order to get proper SSE handling.
    ;
    ;
    ; Actions taken depending on the guest CR0 flags:
    ;
    ;   3    2    1
    ;  TS | EM | MP | FPUInstr | WAIT :: VMM Action
    ; ------------------------------------------------------------------------
    ;   0 |  0 |  0 | Exec     | Exec :: Clear TS & MP, Save HC, Load GC.
    ;   0 |  0 |  1 | Exec     | Exec :: Clear TS, Save HC, Load GC.
    ;   0 |  1 |  0 | #NM      | Exec :: Clear TS & MP, Save HC, Load GC;
    ;   0 |  1 |  1 | #NM      | Exec :: Clear TS, Save HC, Load GC.
    ;   1 |  0 |  0 | #NM      | Exec :: Clear MP, Save HC, Load GC. (EM is already cleared.)
    ;   1 |  0 |  1 | #NM      | #NM  :: Go to host taking trap there.
    ;   1 |  1 |  0 | #NM      | Exec :: Clear MP, Save HC, Load GC. (EM is already set.)
    ;   1 |  1 |  1 | #NM      | #NM  :: Go to host taking trap there.

    ;
    ; Before taking any of these actions we're checking if we have already
    ; loaded the GC FPU. Because if we have, this is an trap for the guest - raw ring-3.
    ;
%ifdef __AMD64__
 %ifdef __WIN__
    mov     CPUM_REG, rcx
 %else
    mov     CPUM_REG, rdi
 %endif
%else
    mov     CPUM_REG, dword [esp + 4]
%endif
    test    dword [CPUM_REG + CPUM.fUseFlags], CPUM_USED_FPU
    jz      hlfpua_not_loaded
    jmp     hlfpua_to_host

    ;
    ; Take action.
    ;
align 16
hlfpua_not_loaded:
    mov     eax, [CPUM_REG + CPUM.Guest.cr0]
    and     eax, X86_CR0_MP | X86_CR0_EM | X86_CR0_TS
%ifdef __AMD64__
    lea     r8, [hlfpuajmp1 wrt rip]
    jmp     qword [rax*4 + r8]
%else
    jmp     dword [eax*2 + hlfpuajmp1]
%endif
align 16
;; jump table using fpu related cr0 flags as index.
hlfpuajmp1:
    RTCCPTR_DEF hlfpua_switch_fpu_ctx
    RTCCPTR_DEF hlfpua_switch_fpu_ctx
    RTCCPTR_DEF hlfpua_switch_fpu_ctx
    RTCCPTR_DEF hlfpua_switch_fpu_ctx
    RTCCPTR_DEF hlfpua_switch_fpu_ctx
    RTCCPTR_DEF hlfpua_to_host
    RTCCPTR_DEF hlfpua_switch_fpu_ctx
    RTCCPTR_DEF hlfpua_to_host
;; and mask for cr0.
hlfpu_afFlags:
    dd      ~(X86_CR0_TS | X86_CR0_MP)
    dd      ~(X86_CR0_TS)
    dd      ~(X86_CR0_TS | X86_CR0_MP)
    dd      ~(X86_CR0_TS)
    dd      ~(X86_CR0_MP)
    dd      0
    dd      ~(X86_CR0_MP)
    dd      0

    ;
    ; Action - switch FPU context and change cr0 flags.
    ;
align 16
hlfpua_switch_fpu_ctx:
%ifndef IN_RING3 ; IN_GC or IN_RING0
    mov     ecx, cr0
 %ifdef __AMD64__
    lea     r8, [hlfpu_afFlags wrt rip]
    and     ecx, [eax*2 + r8]                   ; calc the new cr0 flags.
 %else
    and     ecx, [eax*2 + hlfpu_afFlags]        ; calc the new cr0 flags.
 %endif
    mov     eax, cr0
    and     eax, dword ~(X86_CR0_TS | X86_CR0_EM)
    mov     cr0, eax                            ; clear flags so we don't trap here.
%endif
%ifndef __AMD64__
    test    dword [CPUM_REG + CPUM.CPUFeatures.edx], X86_CPUID_FEATURE_EDX_FXSR
    jz short hlfpua_no_fxsave
%endif

    fxsave  [CPUM_REG + CPUM.Host.fpu]
    or      dword [CPUM_REG + CPUM.fUseFlags], (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM)
    fxrstor [CPUM_REG + CPUM.Guest.fpu]
hlfpua_finished_switch:
%ifdef IN_GC
    mov     cr0, ecx                            ; load the new cr0 flags.
%endif
    ; return continue execution.
    xor     eax, eax
    ret

%ifndef __AMD64__
; legacy support.
hlfpua_no_fxsave:
    fnsave  [CPUM_REG + CPUM.Host.fpu]
    or      dword [CPUM_REG + CPUM.fUseFlags], dword (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM) ; yasm / nasm
    mov     eax, [CPUM_REG + CPUM.Guest.fpu]    ; control word
    not     eax                                 ; 1 means exception ignored (6 LS bits)
    and     eax, byte 03Fh                      ; 6 LS bits only
    test    eax, [CPUM_REG + CPUM.Guest.fpu + 4]; status word
    jz short hlfpua_no_exceptions_pending
    ; technically incorrect, but we certainly don't want any exceptions now!!
    and     dword [CPUM_REG + CPUM.Guest.fpu + 4], ~03Fh
hlfpua_no_exceptions_pending:
    frstor  [CPUM_REG + CPUM.Guest.fpu]
    jmp near hlfpua_finished_switch
%endif ; !__AMD64__


    ;
    ; Action - Generate Guest trap.
    ;
hlfpua_action_4:
hlfpua_to_host:
    mov     eax, VINF_EM_RAW_GUEST_TRAP
    ret
ENDPROC     CPUMHandleLazyFPUAsm


;;
; Restores the host's FPU/XMM state
;
; @returns  0
; @param    pCPUM  x86:[esp+4] GCC:rdi MSC:rcx     CPUM pointer
;
align 16
BEGINPROC CPUMRestoreHostFPUStateAsm
%ifdef __AMD64__
 %ifdef __WIN__
    mov     CPUM_REG, rcx
 %else
    mov     CPUM_REG, rdi
 %endif
%else
    mov     CPUM_REG, dword [esp + 4]
%endif

    ; Restore FPU if guest has used it.
    ; Using fxrstor should ensure that we're not causing unwanted exception on the host.
    test    dword [CPUM_REG + CPUM.fUseFlags], CPUM_USED_FPU
    jz short gth_fpu_no

    mov     eax, cr0
    mov     ecx, eax                    ; save old CR0
    and     eax, dword ~(X86_CR0_TS | X86_CR0_EM)
    mov     cr0, eax

    fxsave  [CPUM_REG + CPUM.Guest.fpu]
    fxrstor [CPUM_REG + CPUM.Host.fpu]

    mov     cr0, ecx                    ; and restore old CR0 again
    and     dword [CPUM_REG + CPUM.fUseFlags], ~CPUM_USED_FPU
gth_fpu_no:
    xor     eax, eax
    ret
ENDPROC   CPUMRestoreHostFPUStateAsm

