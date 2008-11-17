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
;       - eax, ecx, edx, r8
;
; ASSUMPTION:
;       - current CS and DS selectors are wide open
;
; *****************************************************************************
ALIGNCODE(16)
BEGINPROC vmmR0HostToGuestAsm
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

    ; Setup stack; use the lss_esp, ss pair for lss
    DEBUG_CHAR('3')
    mov     eax, [edx + CPUM.Hyper.esp]
    mov     [edx + CPUM.Hyper.lss_esp], eax
    lss     esp, [edx + CPUM.Hyper.lss_esp]

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
    mov     esi, [edx + CPUM.ulOffCPUMCPU]
    mov     esi, [edx + esi + CPUMCPU.fUseFlags]

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
    mov     ebx, [edx + CPUM.Hyper.dr]
    mov     dr0, ebx
    mov     ecx, [edx + CPUM.Hyper.dr + 8*1]
    mov     dr1, ecx
    mov     eax, [edx + CPUM.Hyper.dr + 8*2]
    mov     dr2, eax
    mov     ebx, [edx + CPUM.Hyper.dr + 8*3]
    mov     dr3, ebx
    ;mov     eax, [edx + CPUM.Hyper.dr + 8*6]
    mov     ecx, 0ffff0ff0h
    mov     dr6, ecx
    mov     eax, [edx + CPUM.Hyper.dr + 8*7]
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
    ; Convert to CPUMCPU pointer
    add     edx, [edx + CPUM.ulOffCPUMCPU]
    
    ; Skip return address (assumes called!)
    lea     esp, [esp + 4]

    ;
    ; Guest Context (assumes esp now points to CPUMCTXCORE structure).
    ;
    ; general purpose registers
    push    eax                         ; save return code.
    mov     eax, [esp + 4 + CPUMCTXCORE.edi]
    mov     [edx + CPUMCPU.Guest.edi], eax
    mov     eax, [esp + 4 + CPUMCTXCORE.esi]
    mov     [edx + CPUMCPU.Guest.esi], eax
    mov     eax, [esp + 4 + CPUMCTXCORE.ebp]
    mov     [edx + CPUMCPU.Guest.ebp], eax
    mov     eax, [esp + 4 + CPUMCTXCORE.eax]
    mov     [edx + CPUMCPU.Guest.eax], eax
    mov     eax, [esp + 4 + CPUMCTXCORE.ebx]
    mov     [edx + CPUMCPU.Guest.ebx], eax
    mov     eax, [esp + 4 + CPUMCTXCORE.edx]
    mov     [edx + CPUMCPU.Guest.edx], eax
    mov     eax, [esp + 4 + CPUMCTXCORE.ecx]
    mov     [edx + CPUMCPU.Guest.ecx], eax
    mov     eax, [esp + 4 + CPUMCTXCORE.esp]
    mov     [edx + CPUMCPU.Guest.esp], eax
    ; selectors
    mov     eax, [esp + 4 + CPUMCTXCORE.ss]
    mov     [edx + CPUMCPU.Guest.ss], eax
    mov     eax, [esp + 4 + CPUMCTXCORE.gs]
    mov     [edx + CPUMCPU.Guest.gs], eax
    mov     eax, [esp + 4 + CPUMCTXCORE.fs]
    mov     [edx + CPUMCPU.Guest.fs], eax
    mov     eax, [esp + 4 + CPUMCTXCORE.es]
    mov     [edx + CPUMCPU.Guest.es], eax
    mov     eax, [esp + 4 + CPUMCTXCORE.ds]
    mov     [edx + CPUMCPU.Guest.ds], eax
    mov     eax, [esp + 4 + CPUMCTXCORE.cs]
    mov     [edx + CPUMCPU.Guest.cs], eax
    ; flags
    mov     eax, [esp + 4 + CPUMCTXCORE.eflags]
    mov     [edx + CPUMCPU.Guest.eflags], eax
    ; eip
    mov     eax, [esp + 4 + CPUMCTXCORE.eip]
    mov     [edx + CPUMCPU.Guest.eip], eax
    ; jump to common worker code.
    pop     eax                         ; restore return code.
    ; Load CPUM into edx again
    sub     edx, [edx + CPUMCPU.ulOffCPUM]

    add     esp, CPUMCTXCORE_size      ; skip CPUMCTXCORE structure

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
    jmp short gth_sysenter_no

ALIGNCODE(16)
gth_sysenter_no:

    ;; @todo AMD syscall

    ; Restore FPU if guest has used it.
    ; Using fxrstor should ensure that we're not causing unwanted exception on the host.
    mov     esi, [rdx + r8 + CPUMCPU.fUseFlags] ; esi == use flags.
    test    esi, CPUM_USED_FPU
    jz short gth_fpu_no
    mov     rcx, cr0
    and     rcx, ~(X86_CR0_TS | X86_CR0_EM)
    mov     cr0, rcx

    fxsave  [rdx + r8 + CPUMCPU.Guest.fpu]
    fxrstor [rdx + r8 + CPUMCPU.Host.fpu]
    jmp short gth_fpu_no

ALIGNCODE(16)
gth_fpu_no:

    ; Control registers.
    ; Would've liked to have these highere up in case of crashes, but
    ; the fpu stuff must be done before we restore cr0.
    mov     rcx, [rdx + r8 + CPUMCPU.Host.cr4]
    mov     cr4, rcx
    mov     rcx, [rdx + r8 + CPUMCPU.Host.cr0]
    mov     cr0, rcx
    ;mov     rcx, [rdx + r8 + CPUMCPU.Host.cr2] ; assumes this is waste of time.
    ;mov     cr2, rcx

    ; restore debug registers (if modified) (esi must still be fUseFlags!)
    ; (must be done after cr4 reload because of the debug extension.)
    test    esi, CPUM_USE_DEBUG_REGS | CPUM_USE_DEBUG_REGS_HOST
    jz short gth_debug_regs_no
    jmp     gth_debug_regs_restore
gth_debug_regs_no:
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
    mov     rax, [rdx + r8 + CPUMCPU.Host.dr0]
    mov     dr0, rax
    mov     rbx, [rdx + r8 + CPUMCPU.Host.dr1]
    mov     dr1, rbx
    mov     rcx, [rdx + r8 + CPUMCPU.Host.dr2]
    mov     dr2, rcx
    mov     rax, [rdx + r8 + CPUMCPU.Host.dr3]
    mov     dr3, rax
gth_debug_regs_dr7:
    mov     rbx, [rdx + r8 + CPUMCPU.Host.dr6]
    mov     dr6, rbx
    mov     rcx, [rdx + r8 + CPUMCPU.Host.dr7]
    mov     dr7, rcx
    jmp     gth_debug_regs_no

ENDPROC VMMGCGuestToHostAsm


GLOBALNAME End
;
; The description string (in the text section).
;
NAME(Description):
    db "32-bits  to/from AMD64", 0

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
        at VMMSWITCHERDEF.offHCCode1,                   dd NAME(HCExitTarget)               - NAME(Start)
        at VMMSWITCHERDEF.cbHCCode1,                    dd NAME(End)                        - NAME(HCExitTarget)
        at VMMSWITCHERDEF.offIDCode0,                   dd NAME(IDEnterTarget)              - NAME(Start)
        at VMMSWITCHERDEF.cbIDCode0,                    dd NAME(JmpGCTarget)                - NAME(IDEnterTarget)
        at VMMSWITCHERDEF.offIDCode1,                   dd NAME(IDExitTarget)               - NAME(Start)
        at VMMSWITCHERDEF.cbIDCode1,                    dd NAME(HCExitTarget)               - NAME(IDExitTarget)
        at VMMSWITCHERDEF.offGCCode,                    dd NAME(JmpGCTarget)                - NAME(Start)
        at VMMSWITCHERDEF.cbGCCode,                     dd NAME(IDExitTarget)               - NAME(JmpGCTarget)

    iend

