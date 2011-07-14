; $Id$
;; @file
; CPUM - Guest Context Assembly Routines.
;

;
; Copyright (C) 2006-2007 Oracle Corporation
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
%include "VBox/asmdefs.mac"
%include "VBox/vmm/vm.mac"
%include "VBox/err.mac"
%include "VBox/vmm/stam.mac"
%include "CPUMInternal.mac"
%include "iprt/x86.mac"
%include "VBox/vmm/cpum.mac"

%ifdef IN_RING3
 %error "The jump table doesn't link on leopard."
%endif

;
; Enables write protection of Hypervisor memory pages.
; !note! Must be commented out for Trap8 debug handler.
;
%define ENABLE_WRITE_PROTECTION 1

BEGINCODE


;;
; Handles lazy FPU saving and restoring.
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
; @param    pCPUMCPU  x86:[esp+4] GCC:rdi MSC:rcx     CPUMCPU pointer
;
align 16
BEGINPROC   cpumHandleLazyFPUAsm
    ;
    ; Figure out what to do.
    ;
    ; There are two basic actions:
    ;   1. Save host fpu and restore guest fpu.
    ;   2. Generate guest trap.
    ;
    ; When entering the hypervisor we'll always enable MP (for proper wait
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
%ifdef RT_ARCH_AMD64
 %ifdef RT_OS_WINDOWS
    mov     xDX, rcx
 %else
    mov     xDX, rdi
 %endif
%else
    mov     xDX, dword [esp + 4]
%endif
    test    dword [xDX + CPUMCPU.fUseFlags], CPUM_USED_FPU
    jz      hlfpua_not_loaded
    jmp     hlfpua_to_host

    ;
    ; Take action.
    ;
align 16
hlfpua_not_loaded:
    mov     eax, [xDX + CPUMCPU.Guest.cr0]
    and     eax, X86_CR0_MP | X86_CR0_EM | X86_CR0_TS
%ifdef RT_ARCH_AMD64
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
    RTCCPTR_DEF ~(X86_CR0_TS | X86_CR0_MP)
    RTCCPTR_DEF ~(X86_CR0_TS)
    RTCCPTR_DEF ~(X86_CR0_TS | X86_CR0_MP)
    RTCCPTR_DEF ~(X86_CR0_TS)
    RTCCPTR_DEF ~(X86_CR0_MP)
    RTCCPTR_DEF 0
    RTCCPTR_DEF ~(X86_CR0_MP)
    RTCCPTR_DEF 0

    ;
    ; Action - switch FPU context and change cr0 flags.
    ;
align 16
hlfpua_switch_fpu_ctx:
%ifndef IN_RING3 ; IN_RC or IN_RING0
    mov     xCX, cr0
 %ifdef RT_ARCH_AMD64
    lea     r8, [hlfpu_afFlags wrt rip]
    and     rcx, [rax*4 + r8]                   ; calc the new cr0 flags.
 %else
    and     ecx, [eax*2 + hlfpu_afFlags]        ; calc the new cr0 flags.
 %endif
    mov     xAX, cr0
    and     xAX, ~(X86_CR0_TS | X86_CR0_EM)
    mov     cr0, xAX                            ; clear flags so we don't trap here.
%endif
%ifndef RT_ARCH_AMD64
    mov     eax, edx                            ; Calculate the PCPUM pointer
    sub     eax, [edx + CPUMCPU.offCPUM]
    test    dword [eax + CPUM.CPUFeatures.edx], X86_CPUID_FEATURE_EDX_FXSR
    jz short hlfpua_no_fxsave
%endif

    fxsave  [xDX + CPUMCPU.Host.fpu]
    or      dword [xDX + CPUMCPU.fUseFlags], (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM)
    fxrstor [xDX + CPUMCPU.Guest.fpu]
hlfpua_finished_switch:
%ifdef IN_RC
    mov     cr0, xCX                            ; load the new cr0 flags.
%endif
    ; return continue execution.
    xor     eax, eax
    ret

%ifndef RT_ARCH_AMD64
; legacy support.
hlfpua_no_fxsave:
    fnsave  [xDX + CPUMCPU.Host.fpu]
    or      dword [xDX + CPUMCPU.fUseFlags], dword (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM) ; yasm / nasm
    mov     eax, [xDX + CPUMCPU.Guest.fpu]      ; control word
    not     eax                                 ; 1 means exception ignored (6 LS bits)
    and     eax, byte 03Fh                      ; 6 LS bits only
    test    eax, [xDX + CPUMCPU.Guest.fpu + 4]  ; status word
    jz short hlfpua_no_exceptions_pending
    ; technically incorrect, but we certainly don't want any exceptions now!!
    and     dword [xDX + CPUMCPU.Guest.fpu + 4], ~03Fh
hlfpua_no_exceptions_pending:
    frstor  [xDX + CPUMCPU.Guest.fpu]
    jmp near hlfpua_finished_switch
%endif ; !RT_ARCH_AMD64


    ;
    ; Action - Generate Guest trap.
    ;
hlfpua_action_4:
hlfpua_to_host:
    mov     eax, VINF_EM_RAW_GUEST_TRAP
    ret
ENDPROC     cpumHandleLazyFPUAsm


