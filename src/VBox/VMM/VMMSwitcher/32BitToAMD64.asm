; VMM - World Switchers, 32Bit to AMD64.
;

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

;%define DEBUG_STUFF 1
;%define STRICT_IF 1

;*******************************************************************************
;*  Defined Constants And Macros                                               *
;*******************************************************************************
;; Prefix all names.
%define NAME_OVERLOAD(name) vmmR3Switcher32BitToAMD64_ %+ name


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

    ;
    ; Call worker.
    ;
    FIXUP FIX_HC_CPUM_OFF, 1, 0
    mov     edx, 0ffffffffh
    push    cs                          ; allow for far return and restore cs correctly.
    call    NAME(vmmR0HostToGuestAsm)

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

; *****************************************************************************
; vmmR0HostToGuestAsm
;
; Phase one of the switch from host to guest context (host MMU context)
;
; INPUT:
;       - edx       virtual address of CPUM structure (valid in host context)
;
; USES/DESTROYS:
;       - eax, ecx, edx, esi
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
    CPUMCPU_FROM_CPUM(edx)
    ; general registers.
    mov     [edx + CPUMCPU.Host.ebx], ebx
    mov     [edx + CPUMCPU.Host.edi], edi
    mov     [edx + CPUMCPU.Host.esi], esi
    mov     [edx + CPUMCPU.Host.esp], esp
    mov     [edx + CPUMCPU.Host.ebp], ebp
    ; selectors.
    mov     [edx + CPUMCPU.Host.ds], ds
    mov     [edx + CPUMCPU.Host.es], es
    mov     [edx + CPUMCPU.Host.fs], fs
    mov     [edx + CPUMCPU.Host.gs], gs
    mov     [edx + CPUMCPU.Host.ss], ss
    ; special registers.
    sldt    [edx + CPUMCPU.Host.ldtr]
    sidt    [edx + CPUMCPU.Host.idtr]
    sgdt    [edx + CPUMCPU.Host.gdtr]
    str     [edx + CPUMCPU.Host.tr]
    ; flags
    pushfd
    pop     dword [edx + CPUMCPU.Host.eflags]

    FIXUP FIX_NO_SYSENTER_JMP, 0, htg_no_sysenter - NAME(Start) ; this will insert a jmp htg_no_sysenter if host doesn't use sysenter.
    ; save MSR_IA32_SYSENTER_CS register.
    mov     ecx, MSR_IA32_SYSENTER_CS
    mov     ebx, edx                    ; save edx
    rdmsr                               ; edx:eax <- MSR[ecx]
    mov     [ebx + CPUMCPU.Host.SysEnter.cs], eax
    mov     [ebx + CPUMCPU.Host.SysEnter.cs + 4], edx
    xor     eax, eax                    ; load 0:0 to cause #GP upon sysenter
    xor     edx, edx
    wrmsr
    xchg    ebx, edx                    ; restore edx
    jmp short htg_no_sysenter

ALIGNCODE(16)
htg_no_sysenter:

    ;; handle use flags.
    mov     esi, [edx + CPUMCPU.fUseFlags] ; esi == use flags.
    and     esi, ~CPUM_USED_FPU         ; Clear CPUM_USED_* flags. ;;@todo FPU check can be optimized to use cr0 flags!
    mov     [edx + CPUMCPU.fUseFlags], esi

    ; debug registers.
    test    esi, CPUM_USE_DEBUG_REGS | CPUM_USE_DEBUG_REGS_HOST
    jz      htg_debug_regs_no
    jmp     htg_debug_regs_save_dr7and6
htg_debug_regs_no:

    ; control registers.
    mov     eax, cr0
    mov     [edx + CPUMCPU.Host.cr0], eax
    ;Skip cr2; assume host os don't stuff things in cr2. (safe)
    mov     eax, cr3
    mov     [edx + CPUMCPU.Host.cr3], eax
    mov     eax, cr4
    mov     [edx + CPUMCPU.Host.cr4], eax

    ;;
    ;; Load Intermediate memory context.
    ;;
    FIXUP FIX_INTER_32BIT_CR3, 1
    mov     eax, 0ffffffffh
    mov     cr3, eax
    DEBUG_CHAR('?')

    ;;
    ;; Jump to identity mapped location
    ;;
    FIXUP FIX_GC_2_ID_NEAR_REL, 1, NAME(IDEnterTarget) - NAME(Start)
    jmp near NAME(IDEnterTarget)

        
    ; We're now on identity mapped pages!
ALIGNCODE(16)
GLOBALNAME IDEnterTarget
    DEBUG_CHAR('2')
        
    ; 1. Disable paging.
    mov     ebx, cr0
    and     ebx, ~X86_CR0_PG
    mov     cr0, ebx
    DEBUG_CHAR('2')

    ; 2. Enable PAE.
    mov     ecx, cr4
    or      ecx, X86_CR4_PAE
    mov     cr4, ecx

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
    FIXUP FIX_ID_FAR32_TO_64BIT_MODE, 1, NAME(IDEnter64Mode) - NAME(Start)
    jmp     0ffffh:0fffffffeh

    ;
    ; We're in 64-bit mode (ds, ss, es, fs, gs are all bogus).
BITS 64
ALIGNCODE(16)
NAME(IDEnter64Mode):
    DEBUG_CHAR('6')
    jmp     [NAME(pICEnterTarget) wrt rip]

; 64-bit jump target
NAME(pICEnterTarget):
FIXUP FIX_HC_64BIT, 0, NAME(ICEnterTarget) - NAME(Start)
dq 0ffffffffffffffffh

; 64-bit pCpum address.
NAME(pCpumIC):
FIXUP FIX_HC_64BIT_CPUM, 0
dq 0ffffffffffffffffh

    ;
    ; When we arrive here we're at the 64 bit mode of intermediate context
    ;
ALIGNCODE(16)
GLOBALNAME ICEnterTarget     
    ; at this moment we're in 64-bit mode. let's write something to CPUM
    ; Load CPUM pointer into rdx
    mov     rdx, [NAME(pCpumIC) wrt rip]
    ; Load the CPUMCPU offset.
    mov     r8, [rdx + CPUM.ulOffCPUMCPU]
        
    mov rsi, 012345678h
    mov [rdx + r8 + CPUMCPU.uPadding], rsi

    ; now let's switch back
    mov     rax,  0666h
    jmp     NAME(VMMGCGuestToHostAsm)   ; rax = returncode.

BITS 32
;;
; Detour for saving the host DR7 and DR6.
; esi and edx must be preserved.
htg_debug_regs_save_dr7and6:
DEBUG_S_CHAR('s');
    mov     eax, dr7                    ; not sure, but if I read the docs right this will trap if GD is set. FIXME!!!
    mov     [edx + CPUMCPU.Host.dr7], eax
    xor     eax, eax                    ; clear everything. (bit 12? is read as 1...)
    mov     dr7, eax
    mov     eax, dr6                    ; just in case we save the state register too.
    mov     [edx + CPUMCPU.Host.dr6], eax
    jmp     htg_debug_regs_no


BITS 64
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
    int3
ENDPROC vmmGCCallTrampoline


BITS 64
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
    int3
ENDPROC vmmGCGuestToHost

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
    CPUMCPU_FROM_CPUM(rdx)
    FIXUP FIX_INTER_AMD64_CR3, 1
    mov     rax, 0ffffffffh
    mov     cr3, rax
    ;; We're now in intermediate memory context!
        
    ;;
    ;; Jump to identity mapped location
    ;;
    FIXUP FIX_GC_2_ID_NEAR_REL, 1, NAME(IDExitTarget) - NAME(Start)
    jmp near NAME(IDExitTarget)

    ; We're now on identity mapped pages!
ALIGNCODE(16)
GLOBALNAME IDExitTarget
BITS 32     
    DEBUG_CHAR('1')

    ; 1. Deactivate long mode by turning off paging.
    mov     ebx, cr0
    and     ebx, ~X86_CR0_PG
    mov     cr0, ebx
    DEBUG_CHAR('2')

    ; 2. Load 32-bit intermediate page table.
    FIXUP FIX_INTER_32BIT_CR3, 1
    mov     edx, 0ffffffffh
    mov     cr3, edx
    DEBUG_CHAR('3')

    ; 3. Disable long mode.
    mov     ecx, MSR_K6_EFER
    rdmsr
    DEBUG_CHAR('5')
    and     eax, ~(MSR_K6_EFER_LME)
    wrmsr
    DEBUG_CHAR('6')

    ; 3b. Disable PAE.
    mov     eax, cr4
    and     eax, ~X86_CR4_PAE
    mov     cr4, eax
    DEBUG_CHAR('7')

    ; 4. Enable paging.
    or      ebx, X86_CR0_PG
    mov     cr0, ebx
    jmp short just_a_jump
just_a_jump:
    DEBUG_CHAR('8')

    ;;
    ;; 5. Jump to guest code mapping of the code and load the Hypervisor CS.
    ;;
    FIXUP FIX_ID_2_GC_NEAR_REL, 1, NAME(ICExitTarget) - NAME(Start)
    jmp near NAME(ICExitTarget)
   
    ;;
    ;; When we arrive at this label we're at the
    ;; intermediate mapping of the switching code.
    ;;
BITS 32
ALIGNCODE(16)
GLOBALNAME ICExitTarget
    DEBUG_CHAR('8')
    FIXUP FIX_HC_CPUM_OFF, 1, 0
    mov     edx, 0ffffffffh
    CPUMCPU_FROM_CPUM(edx)
    mov     esi, [edx + CPUMCPU.Host.cr3]
    mov     cr3, esi

    ;; now we're in host memory context, let's restore regs
        
    ; activate host gdt and idt
    lgdt    [edx + CPUMCPU.Host.gdtr]
    DEBUG_CHAR('0')
    lidt    [edx + CPUMCPU.Host.idtr]
    DEBUG_CHAR('1')
        
    ; Restore TSS selector; must mark it as not busy before using ltr (!)
    ; ASSUME that this is supposed to be 'BUSY'. (saves 20-30 ticks on the T42p)
    movzx   eax, word [edx + CPUMCPU.Host.tr]          ; eax <- TR
    and     al, 0F8h                                ; mask away TI and RPL bits, get descriptor offset.
    add     eax, [edx + CPUMCPU.Host.gdtr + 2]         ; eax <- GDTR.address + descriptor offset.
    and     dword [eax + 4], ~0200h                 ; clear busy flag (2nd type2 bit)
    ltr     word [edx + CPUMCPU.Host.tr]

    ; activate ldt
    DEBUG_CHAR('2')
    lldt    [edx + CPUMCPU.Host.ldtr]
    ; Restore segment registers
    mov     eax, [edx + CPUMCPU.Host.ds]
    mov     ds, eax
    mov     eax, [edx + CPUMCPU.Host.es]
    mov     es, eax
    mov     eax, [edx + CPUMCPU.Host.fs]
    mov     fs, eax
    mov     eax, [edx + CPUMCPU.Host.gs]
    mov     gs, eax
    ; restore stack
    lss     esp, [edx + CPUMCPU.Host.esp]

    FIXUP FIX_NO_SYSENTER_JMP, 0, gth_sysenter_no - NAME(Start) ; this will insert a jmp gth_sysenter_no if host doesn't use sysenter.
    
    ; restore MSR_IA32_SYSENTER_CS register.
    mov     ecx, MSR_IA32_SYSENTER_CS
    mov     eax, [edx + CPUMCPU.Host.SysEnter.cs]
    mov     ebx, [edx + CPUMCPU.Host.SysEnter.cs + 4]
    xchg    edx, ebx                    ; save/load edx
    wrmsr                               ; MSR[ecx] <- edx:eax
    xchg    edx, ebx                    ; restore edx
    jmp short gth_sysenter_no

ALIGNCODE(16)
gth_sysenter_no:

    ;; @todo AMD syscall

    ; Restore FPU if guest has used it.
    ; Using fxrstor should ensure that we're not causing unwanted exception on the host.
    mov     esi, [edx + CPUMCPU.fUseFlags] ; esi == use flags.
    test    esi, CPUM_USED_FPU
    jz near gth_fpu_no
    mov     ecx, cr0
    and     ecx, ~(X86_CR0_TS | X86_CR0_EM)
    mov     cr0, ecx

    FIXUP FIX_NO_FXSAVE_JMP, 0, gth_no_fxsave - NAME(Start) ; this will insert a jmp gth_no_fxsave if fxsave isn't supported.
    fxsave  [edx + CPUMCPU.Guest.fpu]
    fxrstor [edx + CPUMCPU.Host.fpu]
    jmp near gth_fpu_no

gth_no_fxsave:
    fnsave  [edx + CPUMCPU.Guest.fpu]
    mov     eax, [edx + CPUMCPU.Host.fpu]     ; control word
    not     eax                            ; 1 means exception ignored (6 LS bits)
    and     eax, byte 03Fh                 ; 6 LS bits only
    test    eax, [edx + CPUMCPU.Host.fpu + 4] ; status word
    jz      gth_no_exceptions_pending

    ; technically incorrect, but we certainly don't want any exceptions now!!
    and     dword [edx + CPUMCPU.Host.fpu + 4], ~03Fh

gth_no_exceptions_pending:
    frstor  [edx + CPUMCPU.Host.fpu]
    jmp short gth_fpu_no

ALIGNCODE(16)
gth_fpu_no:

    ; Control registers.
    ; Would've liked to have these highere up in case of crashes, but
    ; the fpu stuff must be done before we restore cr0.
    mov     ecx, [edx + CPUMCPU.Host.cr4]
    mov     cr4, ecx
    mov     ecx, [edx + CPUMCPU.Host.cr0]
    mov     cr0, ecx
    ;mov     ecx, [edx + CPUMCPU.Host.cr2] ; assumes this is waste of time.
    ;mov     cr2, ecx

    ; restore debug registers (if modified) (esi must still be fUseFlags!)
    ; (must be done after cr4 reload because of the debug extension.)
    test    esi, CPUM_USE_DEBUG_REGS | CPUM_USE_DEBUG_REGS_HOST
    jz short gth_debug_regs_no
    jmp     gth_debug_regs_restore
gth_debug_regs_no:

    ; restore general registers.
    mov     eax, edi                    ; restore return code. eax = return code !!
    mov     edi, [edx + CPUMCPU.Host.edi]
    mov     esi, [edx + CPUMCPU.Host.esi]
    mov     ebx, [edx + CPUMCPU.Host.ebx]
    mov     ebp, [edx + CPUMCPU.Host.ebp]
    push    dword [edx + CPUMCPU.Host.eflags]
    popfd

%ifdef DEBUG_STUFF
;    COM_S_CHAR '4'
%endif
    retf

;;
; Detour for restoring the host debug registers.
; edx and edi must be preserved.
gth_debug_regs_restore:
    DEBUG_S_CHAR('d')
    xor     eax, eax
    mov     dr7, eax                    ; paranoia or not?
    test    esi, CPUM_USE_DEBUG_REGS
    jz short gth_debug_regs_dr7
    DEBUG_S_CHAR('r')
    mov     eax, [edx + CPUMCPU.Host.dr0]
    mov     dr0, eax
    mov     ebx, [edx + CPUMCPU.Host.dr1]
    mov     dr1, ebx
    mov     ecx, [edx + CPUMCPU.Host.dr2]
    mov     dr2, ecx
    mov     eax, [edx + CPUMCPU.Host.dr3]
    mov     dr3, eax
gth_debug_regs_dr7:
    mov     ebx, [edx + CPUMCPU.Host.dr6]
    mov     dr6, ebx
    mov     ecx, [edx + CPUMCPU.Host.dr7]
    mov     dr7, ecx
    jmp     gth_debug_regs_no
        
ENDPROC VMMGCGuestToHostAsm

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
     int3

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
      int3
     
GLOBALNAME End
;
; The description string (in the text section).
;
NAME(Description):
    db "32-bits to/from AMD64", 0

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
        at VMMSWITCHERDEF.enmType,                      dd VMMSWITCHER_32_TO_AMD64
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
        at VMMSWITCHERDEF.offHCCode1,                   dd NAME(ICExitTarget)               - NAME(Start)
        at VMMSWITCHERDEF.cbHCCode1,                    dd NAME(End)                        - NAME(ICExitTarget)
        at VMMSWITCHERDEF.offIDCode0,                   dd NAME(IDEnterTarget)              - NAME(Start)
        at VMMSWITCHERDEF.cbIDCode0,                    dd NAME(ICEnterTarget)              - NAME(IDEnterTarget)
        at VMMSWITCHERDEF.offIDCode1,                   dd NAME(IDExitTarget)               - NAME(Start)
        at VMMSWITCHERDEF.cbIDCode1,                    dd NAME(ICExitTarget)               - NAME(IDExitTarget)
        at VMMSWITCHERDEF.offGCCode,                    dd NAME(ICEnterTarget)              - NAME(Start)
        at VMMSWITCHERDEF.cbGCCode,                     dd NAME(IDExitTarget)               - NAME(ICEnterTarget)

    iend

