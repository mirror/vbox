; $Id$
;; @file
; VMM - World Switchers, AMD64 to PAE.
;

;
;  Copyright (C) 2006-2007 innotek GmbH
;
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License as published by the Free Software Foundation,
;  in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
;  distribution. VirtualBox OSE is distributed in the hope that it will
;  be useful, but WITHOUT ANY WARRANTY of any kind.

;%define DEBUG_STUFF 1
;%define STRICT_IF 1

;*******************************************************************************
;*  Defined Constants And Macros                                               *
;*******************************************************************************
;; Prefix all names.
%define NAME_OVERLOAD(name) vmmR3SwitcherAMD64ToPAE_ %+ name


;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "VBox/asmdefs.mac"
%include "VBox/x86.mac"
%include "VBox/cpum.mac"
%include "VBox/stam.mac"
%include "VBox/vm.mac"
%include "CPUMInternal.mac"
%include "VMMSwitcher/VMMSwitcher.mac"


;
; Start the fixup records
;   We collect the fixups in the .data section as we go along
;   It is therefore VITAL that no-one is using the .data section
;   for anything else between 'Start' and 'End'.
;
BEGINDATA
GLOBALNAME Fixups



BEGINCODE
GLOBALNAME Start

%ifndef VBOX_WITH_HYBIRD_32BIT_KERNEL
BITS 64

;;
; The C interface.
;
; @param    pVM  GCC: rdi  MSC:rcx  The VM handle.
;
BEGINPROC vmmR0HostToGuest
%ifdef DEBUG_STUFF
    COM64_S_NEWLINE
    COM64_S_CHAR '^'
%endif
    ;
    ; The ordinary version of the code.
    ;

 %ifdef STRICT_IF
    pushf
    pop     rax
    test    eax, X86_EFL_IF
    jz      .if_clear_in
    mov     eax, 0c0ffee00h
    ret
.if_clear_in:
 %endif

    ;
    ; make r9 = pVM and rdx = pCpum.
    ; rax, rcx and r8 are scratch here after.
 %ifdef RT_OS_WINDOWS
    mov     r9, rcx
 %else
    mov     r9, rdi
 %endif
    lea     rdx, [r9 + VM.cpum]

 %ifdef VBOX_WITH_STATISTICS
    ;
    ; Switcher stats.
    ;
    lea     r8, [r9 + VM.StatSwitcherToGC]
    STAM64_PROFILE_ADV_START r8
 %endif

    ;
    ; Call worker (far return).
    ;
    mov     eax, cs
    push    rax
    call    NAME(vmmR0HostToGuestAsm)

 %ifdef VBOX_WITH_STATISTICS
    ;
    ; Switcher stats.
    ;
    lea     r8, [r9 + VM.StatSwitcherToGC]
    STAM64_PROFILE_ADV_STOP r8
 %endif

    ret
ENDPROC vmmR0HostToGuest


%else ; VBOX_WITH_HYBIRD_32BIT_KERNEL


BITS 32

;;
; The C interface.
;
BEGINPROC vmmR0HostToGuest
%ifdef DEBUG_STUFF
    COM32_S_NEWLINE
    COM32_S_CHAR '^'
%endif

 %ifdef VBOX_WITH_STATISTICS
    ;
    ; Switcher stats.
    ;
    FIXUP FIX_HC_VM_OFF, 1, VM.StatSwitcherToGC
    mov     edx, 0ffffffffh
    STAM_PROFILE_ADV_START edx
 %endif

    ; Thunk to/from 64 bit when invoking the worker routine.
    ;
    FIXUP FIX_HC_VM_OFF, 1, VM.cpum
    mov     edx, 0ffffffffh

    push    0
    push    cs
    push    0
    FIXUP FIX_HC_32BIT, 1, .vmmR0HostToGuestReturn - NAME(Start)
    push    0ffffffffh

    FIXUP FIX_HC_64BIT_CS, 1
    push    0ffffh
    FIXUP FIX_HC_32BIT, 1, NAME(vmmR0HostToGuestAsm) - NAME(Start)
    push    0ffffffffh
    retf
.vmmR0HostToGuestReturn:

    ;
    ; This selector reloading is probably not necessary, but we do it anyway to be quite sure
    ; the CPU has the right idea about the selectors.
    ;
    mov     edx, ds
    mov     ds, edx
    mov     ecx, es
    mov     es, ecx
    mov     edx, ss
    mov     ss, edx

 %ifdef VBOX_WITH_STATISTICS
    ;
    ; Switcher stats.
    ;
    FIXUP FIX_HC_VM_OFF, 1, VM.StatSwitcherToHC
    mov     edx, 0ffffffffh
    STAM_PROFILE_ADV_STOP edx
 %endif

    ret
ENDPROC vmmR0HostToGuest

BITS 64
%endif ;!VBOX_WITH_HYBIRD_32BIT_KERNEL



; *****************************************************************************
; vmmR0HostToGuestAsm
;
; Phase one of the switch from host to guest context (host MMU context)
;
; INPUT:
;       - edx       virtual address of CPUM structure (valid in host context)
;
; USES/DESTROYS:
;       - eax, ecx, edx
;
; ASSUMPTION:
;       - current CS and DS selectors are wide open
;
; *****************************************************************************
ALIGNCODE(16)
BEGINPROC vmmR0HostToGuestAsm
    ;;
    ;; Save CPU host context
    ;;      Skip eax, edx and ecx as these are not preserved over calls.
    ;;
    ; general registers.
    ; mov     [rdx + CPUM.Host.rax], rax - scratch
    mov     [rdx + CPUM.Host.rbx], rbx
    ; mov     [rdx + CPUM.Host.rcx], rcx - scratch
    ; mov     [rdx + CPUM.Host.rdx], rdx - scratch
    mov     [rdx + CPUM.Host.rdi], rdi
    mov     [rdx + CPUM.Host.rsi], rsi
    mov     [rdx + CPUM.Host.rsp], rsp
    mov     [rdx + CPUM.Host.rbp], rbp
    ; mov     [rdx + CPUM.Host.r8 ], r8 - scratch
    ; mov     [rdx + CPUM.Host.r9 ], r9 - scratch
    mov     [rdx + CPUM.Host.r10], r10
    mov     [rdx + CPUM.Host.r11], r11
    mov     [rdx + CPUM.Host.r12], r12
    mov     [rdx + CPUM.Host.r13], r13
    mov     [rdx + CPUM.Host.r14], r14
    mov     [rdx + CPUM.Host.r15], r15
    ; selectors.
    mov     [rdx + CPUM.Host.ds], ds
    mov     [rdx + CPUM.Host.es], es
    mov     [rdx + CPUM.Host.fs], fs
    mov     [rdx + CPUM.Host.gs], gs
    mov     [rdx + CPUM.Host.ss], ss
    ; MSRs
    mov     rbx, rdx
    mov     ecx, MSR_K8_FS_BASE
    rdmsr
    mov     [rbx + CPUM.Host.FSbase], eax
    mov     [rbx + CPUM.Host.FSbase + 4], edx
    mov     ecx, MSR_K8_GS_BASE
    rdmsr
    mov     [rbx + CPUM.Host.GSbase], eax
    mov     [rbx + CPUM.Host.GSbase + 4], edx
    mov     ecx, MSR_K6_EFER
    rdmsr
    mov     [rbx + CPUM.Host.efer], eax
    mov     [rbx + CPUM.Host.efer + 4], edx
    mov     ecx, MSR_K6_EFER
    mov     rdx, rbx
    ; special registers.
    sldt    [rdx + CPUM.Host.ldtr]
    sidt    [rdx + CPUM.Host.idtr]
    sgdt    [rdx + CPUM.Host.gdtr]
    str     [rdx + CPUM.Host.tr]        ; yasm BUG, generates sldt. YASMCHECK!
    ; flags
    pushf
    pop     qword [rdx + CPUM.Host.rflags]

    FIXUP FIX_NO_SYSENTER_JMP, 0, htg_no_sysenter - NAME(Start) ; this will insert a jmp htg_no_sysenter if host doesn't use sysenter.
    ; save MSR_IA32_SYSENTER_CS register.
    mov     ecx, MSR_IA32_SYSENTER_CS
    mov     rbx, rdx                    ; save edx
    rdmsr                               ; edx:eax <- MSR[ecx]
    mov     [rbx + CPUM.Host.SysEnter.cs], rax
    mov     [rbx + CPUM.Host.SysEnter.cs + 4], rdx
    xor     rax, rax                    ; load 0:0 to cause #GP upon sysenter
    xor     rdx, rdx
    wrmsr
    mov     rdx, rbx                    ; restore edx
    jmp short htg_no_sysenter

ALIGNCODE(16)
htg_no_sysenter:

    ;; handle use flags.
    mov     esi, [rdx + CPUM.fUseFlags] ; esi == use flags.
    and     esi, ~CPUM_USED_FPU   ; Clear CPUM_USED_* flags. ;;@todo FPU check can be optimized to use cr0 flags!
    mov     [rdx + CPUM.fUseFlags], esi

    ; debug registers.
    test    esi, CPUM_USE_DEBUG_REGS | CPUM_USE_DEBUG_REGS_HOST
    jz      htg_debug_regs_no
    jmp     htg_debug_regs_save
htg_debug_regs_no:
    DEBUG_CHAR('a')                     ; trashes esi

    ; control registers.
    mov     rax, cr0
    mov     [rdx + CPUM.Host.cr0], rax
    ;mov     rax, cr2                   ; assume host os don't suff things in cr2. (safe)
    ;mov     [rdx + CPUM.Host.cr2], rax
    mov     rax, cr3
    mov     [rdx + CPUM.Host.cr3], rax
    mov     rax, cr4
    mov     [rdx + CPUM.Host.cr4], rax

    ;;
    ;; Start switching to VMM context.
    ;;

    ;
    ; Change CR0 and CR4 so we can correctly emulate FPU/MMX/SSE[23] exceptions
    ; Also disable WP. (eax==cr4 now)
    ; Note! X86_CR4_PSE and X86_CR4_PAE are important if the host thinks so :-)
    ;
    and     rax, X86_CR4_MCE | X86_CR4_PSE | X86_CR4_PAE
    mov     ecx, [rdx + CPUM.Guest.cr4]
    DEBUG_CHAR('b')                     ; trashes esi
    ;; @todo Switcher cleanup: Determin base CR4 during CPUMR0Init / VMMR3SelectSwitcher putting it
    ;                          in CPUM.Hyper.cr4 (which isn't currently being used). That should
    ;                          simplify this operation a bit (and improve locality of the data).

    ;
    ; CR4.AndMask and CR4.OrMask are set in CPUMR3Init based on the presence of
    ; FXSAVE support on the host CPU
    ;
    and     ecx, [rdx + CPUM.CR4.AndMask]
    or      eax, ecx
    or      eax, [rdx + CPUM.CR4.OrMask]
    mov     cr4, rax
    DEBUG_CHAR('c')                     ; trashes esi

    mov     eax, [rdx + CPUM.Guest.cr0]
    and     eax, X86_CR0_EM
    or      eax, X86_CR0_PE | X86_CR0_PG | X86_CR0_TS | X86_CR0_ET | X86_CR0_NE | X86_CR0_MP
    mov     cr0, rax
    DEBUG_CHAR('0')                     ; trashes esi


    ; Load new gdt so we can do far jump to guest code after cr3 reload.
    lgdt    [rdx + CPUM.Hyper.gdtr]
    DEBUG_CHAR('1')                     ; trashes esi

    ;;
    ;; Load Intermediate memory context.
    ;;
    FIXUP FIX_INTER_AMD64_CR3, 1
    mov     eax, 0ffffffffh
    mov     cr3, rax
    DEBUG_CHAR('2')                     ; trashes esi

    ;;
    ;; 1. Switch to compatibility mode, placing ourselves in identity mapped code.
    ;;
    jmp far [NAME(fpIDEnterTarget) wrt rip]

; 16:32 Pointer to IDEnterTarget.
NAME(fpIDEnterTarget):
    FIXUP FIX_ID_32BIT, 0, NAME(IDEnterTarget) - NAME(Start)
dd  0
    FIXUP FIX_HYPER_CS, 0
dd  0


;;
; Detour for saving the host DR7 and DR6.
; esi and rdx must be preserved.
htg_debug_regs_save:
DEBUG_S_CHAR('s');
    mov     rax, dr7                    ; not sure, but if I read the docs right this will trap if GD is set. FIXME!!!
    mov     [rdx + CPUM.Host.dr7], rax
    xor     eax, eax                    ; clear everything. (bit 12? is read as 1...)
    mov     dr7, rax
    mov     rax, dr6                    ; just in case we save the state register too.
    mov     [rdx + CPUM.Host.dr6], rax
    ; save host DR0-3?
    test    esi, CPUM_USE_DEBUG_REGS
    jz near htg_debug_regs_no
DEBUG_S_CHAR('S');
    mov     rax, dr0
    mov     [rdx + CPUM.Host.dr0], rax
    mov     rbx, dr1
    mov     [rdx + CPUM.Host.dr1], rbx
    mov     rcx, dr2
    mov     [rdx + CPUM.Host.dr2], rcx
    mov     rax, dr3
    mov     [rdx + CPUM.Host.dr3], rax
    jmp     htg_debug_regs_no


    ; We're now on an identity mapped pages! in 32-bit compatability mode.
BITS 32
ALIGNCODE(16)
GLOBALNAME IDEnterTarget
    DEBUG_CHAR('3')

    ; 2. Deactivate long mode by turning off paging.
    mov     ebx, cr0
    and     ebx, ~X86_CR0_PG
    mov     cr0, ebx
    DEBUG_CHAR('4')

    ; 3. Load 32-bit intermediate page table.
    FIXUP FIX_INTER_PAE_CR3, 1
    mov     edx, 0ffffffffh
    mov     cr3, edx

    ; 4. Disable long mode.
    ;    We also use the chance to disable syscall/sysret and fast fxsave/fxrstor.
    mov     ecx, MSR_K6_EFER
    rdmsr
    DEBUG_CHAR('5')
    and     eax, ~(MSR_K6_EFER_LME | MSR_K6_EFER_SCE | MSR_K6_EFER_FFXSR)
    wrmsr
    DEBUG_CHAR('6')

    ; 5. Enable paging.
    or      ebx, X86_CR0_PG
    mov     cr0, ebx
    jmp short just_a_jump
just_a_jump:
    DEBUG_CHAR('7')

    ;;
    ;; 6. Jump to guest code mapping of the code and load the Hypervisor CS.
    ;;
    FIXUP FIX_ID_2_GC_NEAR_REL, 1, NAME(JmpGCTarget) - NAME(Start)
    jmp near NAME(JmpGCTarget)


    ;;
    ;; When we arrive at this label we're at the
    ;; guest code mapping of the switching code.
    ;;
ALIGNCODE(16)
GLOBALNAME JmpGCTarget
    DEBUG_CHAR('-')
    ; load final cr3 and do far jump to load cs.
    FIXUP FIX_HYPER_PAE_CR3, 1
    mov     eax, 0ffffffffh
    mov     cr3, eax
    DEBUG_CHAR('0')

    ;;
    ;; We're in VMM MMU context and VMM CS is loaded.
    ;; Setup the rest of the VMM state.
    ;;
    ; Load selectors
    DEBUG_CHAR('1')
    FIXUP FIX_HYPER_DS, 1
    mov     eax, 0ffffh
    mov     ds, eax
    mov     es, eax
    xor     eax, eax
    mov     gs, eax
    mov     fs, eax
    ; Load pCpum into EDX
    FIXUP FIX_GC_CPUM_OFF, 1, 0
    mov     edx, 0ffffffffh
    ; Activate guest IDT
    DEBUG_CHAR('2')
    lidt    [edx + CPUM.Hyper.idtr]

    ; Setup stack
    DEBUG_CHAR('3')
    lss     esp, [edx + CPUM.Hyper.esp]

    ; Restore TSS selector; must mark it as not busy before using ltr (!)
    DEBUG_CHAR('4')
    FIXUP FIX_GC_TSS_GDTE_DW2, 2
    and     dword [0ffffffffh], ~0200h      ; clear busy flag (2nd type2 bit)
    DEBUG_CHAR('5')
    ltr     word [edx + CPUM.Hyper.tr]
    DEBUG_CHAR('6')

    ; Activate the ldt (now we can safely crash).
    lldt    [edx + CPUM.Hyper.ldtr]
    DEBUG_CHAR('7')

    ;; use flags.
    mov     esi, [edx + CPUM.fUseFlags]

    ; debug registers
    test    esi, CPUM_USE_DEBUG_REGS
    jz      htg_debug_regs_guest_no
    jmp     htg_debug_regs_guest
htg_debug_regs_guest_no:
    DEBUG_CHAR('9')

    ; General registers.
    mov     ebx, [edx + CPUM.Hyper.ebx]
    mov     ebp, [edx + CPUM.Hyper.ebp]
    mov     esi, [edx + CPUM.Hyper.esi]
    mov     edi, [edx + CPUM.Hyper.edi]
    push    dword [edx + CPUM.Hyper.eflags]
    popfd
    DEBUG_CHAR('!')

    ;;
    ;; Return to the VMM code which either called the switcher or
    ;; the code set up to run by HC.
    ;;
%ifdef DEBUG_STUFF
    COM32_S_PRINT ';eip='
    mov     eax, [edx + CPUM.Hyper.eip]
    COM32_S_DWORD_REG eax
    COM32_S_CHAR ';'
%endif
    mov     eax, [edx + CPUM.Hyper.eip]
%ifdef VBOX_WITH_STATISTICS
    FIXUP FIX_GC_VM_OFF, 1, VM.StatSwitcherToGC
    mov     edx, 0ffffffffh
    STAM32_PROFILE_ADV_STOP edx
    FIXUP FIX_GC_CPUM_OFF, 1, 0
    mov     edx, 0ffffffffh
%endif
    jmp     eax

;;
; Detour for saving host DR0-3 and loading hypervisor debug registers.
; esi and edx must be preserved.
htg_debug_regs_guest:
    DEBUG_S_CHAR('D')
    DEBUG_S_CHAR('R')
    DEBUG_S_CHAR('x')
    ; load hyper DR0-7
    mov     ebx, [edx + CPUM.Hyper.dr0]
    mov     dr0, ebx
    mov     ecx, [edx + CPUM.Hyper.dr1]
    mov     dr1, ecx
    mov     eax, [edx + CPUM.Hyper.dr2]
    mov     dr2, eax
    mov     ebx, [edx + CPUM.Hyper.dr3]
    mov     dr3, ebx
    ;mov     eax, [edx + CPUM.Hyper.dr6]
    mov     ecx, 0ffff0ff0h
    mov     dr6, ecx
    mov     eax, [edx + CPUM.Hyper.dr7]
    mov     dr7, eax
    jmp     htg_debug_regs_guest_no

ENDPROC vmmR0HostToGuestAsm


;;
; Trampoline for doing a call when starting the hyper visor execution.
;
; Push any arguments to the routine.
; Push the argument frame size (cArg * 4).
; Push the call target (_cdecl convention).
; Push the address of this routine.
;
;
ALIGNCODE(16)
BEGINPROC vmmGCCallTrampoline
%ifdef DEBUG_STUFF
    COM32_S_CHAR 'c'
    COM32_S_CHAR 't'
    COM32_S_CHAR '!'
%endif

    ; call routine
    pop     eax                         ; call address
    mov     esi, edx                    ; save edx
    pop     edi                         ; argument count.
%ifdef DEBUG_STUFF
    COM32_S_PRINT ';eax='
    COM32_S_DWORD_REG eax
    COM32_S_CHAR ';'
%endif
    call    eax                         ; do call
    add     esp, edi                    ; cleanup stack

    ; return to the host context.
    push    byte 0                      ; eip
    mov     edx, esi                    ; CPUM pointer

%ifdef DEBUG_STUFF
    COM32_S_CHAR '`'
%endif
    jmp     NAME(VMMGCGuestToHostAsm)   ; eax = returncode.
ENDPROC vmmGCCallTrampoline



;;
; The C interface.
;
ALIGNCODE(16)
BEGINPROC vmmGCGuestToHost
%ifdef DEBUG_STUFF
    push    esi
    COM_NEWLINE
    DEBUG_CHAR('b')
    DEBUG_CHAR('a')
    DEBUG_CHAR('c')
    DEBUG_CHAR('k')
    DEBUG_CHAR('!')
    COM_NEWLINE
    pop     esi
%endif
    mov     eax, [esp + 4]
    jmp     NAME(VMMGCGuestToHostAsm)
ENDPROC vmmGCGuestToHost


;;
; VMMGCGuestToHostAsmGuestCtx
;
; Switches from Guest Context to Host Context.
; Of course it's only called from within the GC.
;
; @param    eax     Return code.
; @param    esp + 4 Pointer to CPUMCTXCORE.
;
; @remark   ASSUMES interrupts disabled.
;
ALIGNCODE(16)
BEGINPROC VMMGCGuestToHostAsmGuestCtx
    DEBUG_CHAR('~')

%ifdef VBOX_WITH_STATISTICS
    FIXUP FIX_GC_VM_OFF, 1, VM.StatTotalInGC
    mov     edx, 0ffffffffh
    STAM32_PROFILE_ADV_STOP edx

    FIXUP FIX_GC_VM_OFF, 1, VM.StatTotalGCToQemu
    mov     edx, 0ffffffffh
    STAM32_PROFILE_ADV_START edx

    FIXUP FIX_GC_VM_OFF, 1, VM.StatSwitcherToHC
    mov     edx, 0ffffffffh
    STAM32_PROFILE_ADV_START edx
%endif

    ;
    ; Load the CPUM pointer.
    ;
    FIXUP FIX_GC_CPUM_OFF, 1, 0
    mov     edx, 0ffffffffh

    ; Skip return address (assumes called!)
    lea     esp, [esp + 4]

    ;
    ; Guest Context (assumes CPUMCTXCORE layout).
    ;
    ; general purpose registers (layout is pushad)
    pop     dword [edx + CPUM.Guest.edi]
    pop     dword [edx + CPUM.Guest.esi]
    pop     dword [edx + CPUM.Guest.ebp]
    pop     dword [edx + CPUM.Guest.eax]
    pop     dword [edx + CPUM.Guest.ebx]
    pop     dword [edx + CPUM.Guest.edx]
    pop     dword [edx + CPUM.Guest.ecx]
    pop     dword [edx + CPUM.Guest.esp]
    pop     dword [edx + CPUM.Guest.ss]
    pop     dword [edx + CPUM.Guest.gs]
    pop     dword [edx + CPUM.Guest.fs]
    pop     dword [edx + CPUM.Guest.es]
    pop     dword [edx + CPUM.Guest.ds]
    pop     dword [edx + CPUM.Guest.cs]
    ; flags
    pop     dword [edx + CPUM.Guest.eflags]
    ; eip
    pop     dword [edx + CPUM.Guest.eip]
    jmp     vmmGCGuestToHostAsm_EIPDone
ENDPROC VMMGCGuestToHostAsmGuestCtx


;;
; VMMGCGuestToHostAsmHyperCtx
;
; This is an alternative entry point which we'll be using
; when the we have the hypervisor context and need to save
; that before going to the host.
;
; This is typically useful when abandoning the hypervisor
; because of a trap and want the trap state to be saved.
;
; @param    eax     Return code.
; @param    ecx     Points to CPUMCTXCORE.
; @uses     eax,edx,ecx
ALIGNCODE(16)
BEGINPROC VMMGCGuestToHostAsmHyperCtx
    DEBUG_CHAR('#')

%ifdef VBOX_WITH_STATISTICS
    FIXUP FIX_GC_VM_OFF, 1, VM.StatTotalInGC
    mov     edx, 0ffffffffh
    STAM32_PROFILE_ADV_STOP edx

    FIXUP FIX_GC_VM_OFF, 1, VM.StatTotalGCToQemu
    mov     edx, 0ffffffffh
    STAM32_PROFILE_ADV_START edx

    FIXUP FIX_GC_VM_OFF, 1, VM.StatSwitcherToHC
    mov     edx, 0ffffffffh
    STAM32_PROFILE_ADV_START edx
%endif

    ;
    ; Load the CPUM pointer.
    ;
    FIXUP FIX_GC_CPUM_OFF, 1, 0
    mov     edx, 0ffffffffh

    push    eax                         ; save return code.
    ; general purpose registers
    mov     eax, [ecx + CPUMCTXCORE.edi]
    mov     [edx + CPUM.Hyper.edi], eax
    mov     eax, [ecx + CPUMCTXCORE.esi]
    mov     [edx + CPUM.Hyper.esi], eax
    mov     eax, [ecx + CPUMCTXCORE.ebp]
    mov     [edx + CPUM.Hyper.ebp], eax
    mov     eax, [ecx + CPUMCTXCORE.eax]
    mov     [edx + CPUM.Hyper.eax], eax
    mov     eax, [ecx + CPUMCTXCORE.ebx]
    mov     [edx + CPUM.Hyper.ebx], eax
    mov     eax, [ecx + CPUMCTXCORE.edx]
    mov     [edx + CPUM.Hyper.edx], eax
    mov     eax, [ecx + CPUMCTXCORE.ecx]
    mov     [edx + CPUM.Hyper.ecx], eax
    mov     eax, [ecx + CPUMCTXCORE.esp]
    mov     [edx + CPUM.Hyper.esp], eax
    ; selectors
    mov     eax, [ecx + CPUMCTXCORE.ss]
    mov     [edx + CPUM.Hyper.ss], eax
    mov     eax, [ecx + CPUMCTXCORE.gs]
    mov     [edx + CPUM.Hyper.gs], eax
    mov     eax, [ecx + CPUMCTXCORE.fs]
    mov     [edx + CPUM.Hyper.fs], eax
    mov     eax, [ecx + CPUMCTXCORE.es]
    mov     [edx + CPUM.Hyper.es], eax
    mov     eax, [ecx + CPUMCTXCORE.ds]
    mov     [edx + CPUM.Hyper.ds], eax
    mov     eax, [ecx + CPUMCTXCORE.cs]
    mov     [edx + CPUM.Hyper.cs], eax
    ; flags
    mov     eax, [ecx + CPUMCTXCORE.eflags]
    mov     [edx + CPUM.Hyper.eflags], eax
    ; eip
    mov     eax, [ecx + CPUMCTXCORE.eip]
    mov     [edx + CPUM.Hyper.eip], eax
    ; jump to common worker code.
    pop     eax                         ; restore return code.
    jmp     vmmGCGuestToHostAsm_SkipHyperRegs

ENDPROC VMMGCGuestToHostAsmHyperCtx


;;
; VMMGCGuestToHostAsm
;
; This is an alternative entry point which we'll be using
; when the we have saved the guest state already or we haven't
; been messing with the guest at all.
;
; @param    eax     Return code.
; @uses     eax, edx, ecx (or it may use them in the future)
;
ALIGNCODE(16)
BEGINPROC VMMGCGuestToHostAsm
    DEBUG_CHAR('%')

%ifdef VBOX_WITH_STATISTICS
    FIXUP FIX_GC_VM_OFF, 1, VM.StatTotalInGC
    mov     edx, 0ffffffffh
    STAM32_PROFILE_ADV_STOP edx

    FIXUP FIX_GC_VM_OFF, 1, VM.StatTotalGCToQemu
    mov     edx, 0ffffffffh
    STAM32_PROFILE_ADV_START edx

    FIXUP FIX_GC_VM_OFF, 1, VM.StatSwitcherToHC
    mov     edx, 0ffffffffh
    STAM32_PROFILE_ADV_START edx
%endif

    ;
    ; Load the CPUM pointer.
    ;
    FIXUP FIX_GC_CPUM_OFF, 1, 0
    mov     edx, 0ffffffffh

    pop     dword [edx + CPUM.Hyper.eip] ; call return from stack
    jmp short vmmGCGuestToHostAsm_EIPDone

ALIGNCODE(16)
vmmGCGuestToHostAsm_EIPDone:
    ; general registers which we care about.
    mov     dword [edx + CPUM.Hyper.ebx], ebx
    mov     dword [edx + CPUM.Hyper.esi], esi
    mov     dword [edx + CPUM.Hyper.edi], edi
    mov     dword [edx + CPUM.Hyper.ebp], ebp
    mov     dword [edx + CPUM.Hyper.esp], esp

    ; special registers which may change.
vmmGCGuestToHostAsm_SkipHyperRegs:
%ifdef STRICT_IF
    pushf
    pop     ecx
    test    ecx, X86_EFL_IF
    jz      .if_clear_out
    mov     eax, 0c0ffee01h
    cli
.if_clear_out:
%endif
    ; str     [edx + CPUM.Hyper.tr] - double fault only, and it won't be right then either.
    sldt    [edx + CPUM.Hyper.ldtr]

    ; No need to save CRx here. They are set dynamically according to Guest/Host requirements.
    ; FPU context is saved before restore of host saving (another) branch.


    ;;
    ;; Load Intermediate memory context.
    ;;
    mov     edi, eax                    ; save return code in EDI (careful with COM_DWORD_REG from here on!)
    FIXUP FIX_INTER_PAE_CR3, 1
    mov     eax, 0ffffffffh
    mov     cr3, eax
    DEBUG_CHAR('?')

    ;; We're now in intermediate memory context!

    ;;
    ;; 0. Jump to identity mapped location
    ;;
    FIXUP FIX_GC_2_ID_NEAR_REL, 1, NAME(IDExitTarget) - NAME(Start)
    jmp near NAME(IDExitTarget)

    ; We're now on identity mapped pages!
ALIGNCODE(16)
GLOBALNAME IDExitTarget
    DEBUG_CHAR('1')

    ; 1. Disable paging.
    mov     ebx, cr0
    and     ebx, ~X86_CR0_PG
    mov     cr0, ebx
    DEBUG_CHAR('2')

    ; 2. Enable PAE - already enabled.

    ; 3. Load long mode intermediate CR3.
    FIXUP FIX_INTER_AMD64_CR3, 1
    mov     ecx, 0ffffffffh
    mov     cr3, ecx
    DEBUG_CHAR('3')

    ; 4. Enable long mode.
    mov     ebp, edx
    mov     ecx, MSR_K6_EFER
    rdmsr
    or      eax, MSR_K6_EFER_LME
    wrmsr
    mov     edx, ebp
    DEBUG_CHAR('4')

    ; 5. Enable paging.
    or      ebx, X86_CR0_PG
    mov     cr0, ebx
    DEBUG_CHAR('5')

    ; Jump from compatability mode to 64-bit mode.
    FIXUP FIX_ID_FAR32_TO_64BIT_MODE, 1, NAME(IDExit64Mode) - NAME(Start)
    jmp     0ffffh:0fffffffeh

    ;
    ; We're in 64-bit mode (ds, ss, es, fs, gs are all bogus).
    ; Move on to the HC mapping.
    ;
BITS 64
ALIGNCODE(16)
NAME(IDExit64Mode):
    DEBUG_CHAR('6')
    jmp     [NAME(pHCExitTarget) wrt rip]

; 64-bit jump target
NAME(pHCExitTarget):
FIXUP FIX_HC_64BIT, 0, NAME(HCExitTarget) - NAME(Start)
dq 0ffffffffffffffffh

; 64-bit pCpum address.
NAME(pCpumHC):
FIXUP FIX_HC_64BIT_CPUM, 0
dq 0ffffffffffffffffh

    ;
    ; When we arrive here we're at the host context
    ; mapping of the switcher code.
    ;
ALIGNCODE(16)
GLOBALNAME HCExitTarget
    DEBUG_CHAR('9')

    ; load final cr3
    mov     rsi, [rdx + CPUM.Host.cr3]
    mov     cr3, rsi
    DEBUG_CHAR('@')

    ;;
    ;; Restore Host context.
    ;;
    ; Load CPUM pointer into edx
    mov     rdx, [NAME(pCpumHC) wrt rip]
    ; activate host gdt and idt
    lgdt    [rdx + CPUM.Host.gdtr]
    DEBUG_CHAR('0')
    lidt    [rdx + CPUM.Host.idtr]
    DEBUG_CHAR('1')
    ; Restore TSS selector; must mark it as not busy before using ltr (!)
%if 1 ; ASSUME that this is supposed to be 'BUSY'. (saves 20-30 ticks on the T42p)
    movzx   eax, word [rdx + CPUM.Host.tr]          ; eax <- TR
    and     al, 0F8h                                ; mask away TI and RPL bits, get descriptor offset.
    add     rax, [rdx + CPUM.Host.gdtr + 2]         ; eax <- GDTR.address + descriptor offset.
    and     dword [rax + 4], ~0200h                 ; clear busy flag (2nd type2 bit)
    ltr     word [rdx + CPUM.Host.tr]
%else
    movzx   eax, word [rdx + CPUM.Host.tr]          ; eax <- TR
    and     al, 0F8h                                ; mask away TI and RPL bits, get descriptor offset.
    add     rax, [rdx + CPUM.Host.gdtr + 2]         ; eax <- GDTR.address + descriptor offset.
    mov     ecx, [rax + 4]                          ; ecx <- 2nd descriptor dword
    mov     ebx, ecx                                ; save orginal value
    and     ecx, ~0200h                             ; clear busy flag (2nd type2 bit)
    mov     [rax + 4], ccx                          ; not using xchg here is paranoia..
    ltr     word [rdx + CPUM.Host.tr]
    xchg    [rax + 4], ebx                          ; using xchg is paranoia too...
%endif
    ; activate ldt
    DEBUG_CHAR('2')
    lldt    [rdx + CPUM.Host.ldtr]
    ; Restore segment registers
    mov     eax, [rdx + CPUM.Host.ds]
    mov     ds, eax
    mov     eax, [rdx + CPUM.Host.es]
    mov     es, eax
    mov     eax, [rdx + CPUM.Host.fs]
    mov     fs, eax
    mov     eax, [rdx + CPUM.Host.gs]
    mov     gs, eax
    ; restore stack
    mov     eax, [rdx + CPUM.Host.ss]
    mov     ss, eax
    mov     rsp, [rdx + CPUM.Host.rsp]

    FIXUP FIX_NO_SYSENTER_JMP, 0, gth_sysenter_no - NAME(Start) ; this will insert a jmp gth_sysenter_no if host doesn't use sysenter.
    ; restore MSR_IA32_SYSENTER_CS register.
    mov     ecx, MSR_IA32_SYSENTER_CS
    mov     eax, [rdx + CPUM.Host.SysEnter.cs]
    mov     ebx, [rdx + CPUM.Host.SysEnter.cs + 4]
    mov     rbx, rdx                    ; save/load edx
    wrmsr                               ; MSR[ecx] <- edx:eax
    mov     rdx, rbx                    ; restore edx
    jmp short gth_sysenter_no

ALIGNCODE(16)
gth_sysenter_no:

    ;; @todo AMD syscall

    ; Restore FPU if guest has used it.
    ; Using fxrstor should ensure that we're not causing unwanted exception on the host.
    mov     esi, [rdx + CPUM.fUseFlags] ; esi == use flags.
    test    esi, CPUM_USED_FPU
    jz short gth_fpu_no
    mov     rcx, cr0
    and     rcx, ~(X86_CR0_TS | X86_CR0_EM)
    mov     cr0, rcx

    fxsave  [rdx + CPUM.Guest.fpu]
    fxrstor [rdx + CPUM.Host.fpu]
    jmp short gth_fpu_no

ALIGNCODE(16)
gth_fpu_no:

    ; Control registers.
    ; Would've liked to have these highere up in case of crashes, but
    ; the fpu stuff must be done before we restore cr0.
    mov     rcx, [rdx + CPUM.Host.cr4]
    mov     cr4, rcx
    mov     rcx, [rdx + CPUM.Host.cr0]
    mov     cr0, rcx
    ;mov     rcx, [rdx + CPUM.Host.cr2] ; assumes this is waste of time.
    ;mov     cr2, rcx

    ; restore debug registers (if modified) (esi must still be fUseFlags!)
    ; (must be done after cr4 reload because of the debug extension.)
    test    esi, CPUM_USE_DEBUG_REGS | CPUM_USE_DEBUG_REGS_HOST
    jz short gth_debug_regs_no
    jmp     gth_debug_regs_restore
gth_debug_regs_no:

    ; Restore MSRs
    mov     rbx, rdx
    mov     ecx, MSR_K8_FS_BASE
    mov     eax, [rbx + CPUM.Host.FSbase]
    mov     edx, [rbx + CPUM.Host.FSbase + 4]
    wrmsr
    mov     ecx, MSR_K8_GS_BASE
    mov     eax, [rbx + CPUM.Host.GSbase]
    mov     edx, [rbx + CPUM.Host.GSbase + 4]
    wrmsr
    mov     ecx, MSR_K6_EFER
    mov     eax, [rbx + CPUM.Host.efer]
    mov     edx, [rbx + CPUM.Host.efer + 4]
    wrmsr
    mov     rdx, rbx


    ; restore general registers.
    mov     eax, edi                    ; restore return code. eax = return code !!
    ; mov     rax, [rdx + CPUM.Host.rax] - scratch + return code
    mov     rbx, [rdx + CPUM.Host.rbx]
    ; mov     rcx, [rdx + CPUM.Host.rcx] - scratch
    ; mov     rdx, [rdx + CPUM.Host.rdx] - scratch
    mov     rdi, [rdx + CPUM.Host.rdi]
    mov     rsi, [rdx + CPUM.Host.rsi]
    mov     rsp, [rdx + CPUM.Host.rsp]
    mov     rbp, [rdx + CPUM.Host.rbp]
    ; mov     r8,  [rdx + CPUM.Host.r8 ] - scratch
    ; mov     r9,  [rdx + CPUM.Host.r9 ] - scratch
    mov     r10, [rdx + CPUM.Host.r10]
    mov     r11, [rdx + CPUM.Host.r11]
    mov     r12, [rdx + CPUM.Host.r12]
    mov     r13, [rdx + CPUM.Host.r13]
    mov     r14, [rdx + CPUM.Host.r14]
    mov     r15, [rdx + CPUM.Host.r15]

    ; finally restore flags. (probably not required)
    push    qword [rdx + CPUM.Host.rflags]
    popf


%ifdef DEBUG_STUFF
    COM64_S_CHAR '4'
%endif
    db 048h
    retf

;;
; Detour for restoring the host debug registers.
; edx and edi must be preserved.
gth_debug_regs_restore:
    DEBUG_S_CHAR('d')
    xor     eax, eax
    mov     dr7, rax                    ; paranoia or not?
    test    esi, CPUM_USE_DEBUG_REGS
    jz short gth_debug_regs_dr7
    DEBUG_S_CHAR('r')
    mov     rax, [rdx + CPUM.Host.dr0]
    mov     dr0, rax
    mov     rbx, [rdx + CPUM.Host.dr1]
    mov     dr1, rbx
    mov     rcx, [rdx + CPUM.Host.dr2]
    mov     dr2, rcx
    mov     rax, [rdx + CPUM.Host.dr3]
    mov     dr3, rax
gth_debug_regs_dr7:
    mov     rbx, [rdx + CPUM.Host.dr6]
    mov     dr6, rbx
    mov     rcx, [rdx + CPUM.Host.dr7]
    mov     dr7, rcx
    jmp     gth_debug_regs_no

ENDPROC VMMGCGuestToHostAsm


GLOBALNAME End
;
; The description string (in the text section).
;
NAME(Description):
    db "AMD64 to/from PAE", 0

extern NAME(Relocate)

;
; End the fixup records.
;
BEGINDATA
    db FIX_THE_END                      ; final entry.
GLOBALNAME FixupsEnd

;;
; The switcher definition structure.
ALIGNDATA(16)
GLOBALNAME Def
    istruc VMMSWITCHERDEF
        at VMMSWITCHERDEF.pvCode,                       RTCCPTR_DEF NAME(Start)
        at VMMSWITCHERDEF.pvFixups,                     RTCCPTR_DEF NAME(Fixups)
        at VMMSWITCHERDEF.pszDesc,                      RTCCPTR_DEF NAME(Description)
        at VMMSWITCHERDEF.pfnRelocate,                  RTCCPTR_DEF NAME(Relocate)
        at VMMSWITCHERDEF.enmType,                      dd VMMSWITCHER_AMD64_TO_PAE
        at VMMSWITCHERDEF.cbCode,                       dd NAME(End)                        - NAME(Start)
        at VMMSWITCHERDEF.offR0HostToGuest,             dd NAME(vmmR0HostToGuest)           - NAME(Start)
        at VMMSWITCHERDEF.offGCGuestToHost,             dd NAME(vmmGCGuestToHost)           - NAME(Start)
        at VMMSWITCHERDEF.offGCCallTrampoline,          dd NAME(vmmGCCallTrampoline)        - NAME(Start)
        at VMMSWITCHERDEF.offGCGuestToHostAsm,          dd NAME(VMMGCGuestToHostAsm)        - NAME(Start)
        at VMMSWITCHERDEF.offGCGuestToHostAsmHyperCtx,  dd NAME(VMMGCGuestToHostAsmHyperCtx)- NAME(Start)
        at VMMSWITCHERDEF.offGCGuestToHostAsmGuestCtx,  dd NAME(VMMGCGuestToHostAsmGuestCtx)- NAME(Start)
        ; disasm help
        at VMMSWITCHERDEF.offHCCode0,                   dd 0
        at VMMSWITCHERDEF.cbHCCode0,                    dd NAME(IDEnterTarget)              - NAME(Start)
        at VMMSWITCHERDEF.offHCCode1,                   dd NAME(HCExitTarget)               - NAME(Start)
        at VMMSWITCHERDEF.cbHCCode1,                    dd NAME(End)                        - NAME(HCExitTarget)
        at VMMSWITCHERDEF.offIDCode0,                   dd NAME(IDEnterTarget)              - NAME(Start)
        at VMMSWITCHERDEF.cbIDCode0,                    dd NAME(JmpGCTarget)                - NAME(IDEnterTarget)
        at VMMSWITCHERDEF.offIDCode1,                   dd NAME(IDExitTarget)               - NAME(Start)
        at VMMSWITCHERDEF.cbIDCode1,                    dd NAME(HCExitTarget)               - NAME(IDExitTarget)
        at VMMSWITCHERDEF.offGCCode,                    dd NAME(JmpGCTarget)                - NAME(Start)
        at VMMSWITCHERDEF.cbGCCode,                     dd NAME(IDExitTarget)               - NAME(JmpGCTarget)

    iend

