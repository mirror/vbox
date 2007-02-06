; $Id$
;; @file
; VMXM - R0 vmx helpers
;

;
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
;

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "VBox/asmdefs.mac"
%include "VBox/err.mac"
%include "VBox/hwacc_vmx.mac"
%include "VBox/cpum.mac"
%include "VBox/x86.mac"

%ifdef __OS2__ ;; @todo build cvs nasm like on OS X.
 %macro vmwrite 2,
    int3
 %endmacro
 %define vmlaunch int3
 %define vmresume int3
%endif


;; @def MYPUSHAD
; Macro generating an equivalent to pushad

;; @def MYPOPAD
; Macro generating an equivalent to popad

;; @def MYPUSHSEGS
; Macro saving all segment registers on the stack.
; @param 1  full width register name
; @param 2  16-bit regsiter name for \a 1.

;; @def MYPOPSEGS
; Macro restoring all segment registers on the stack
; @param 1  full width register name
; @param 2  16-bit regsiter name for \a 1.

%ifdef __AMD64__
 %ifdef ASM_CALL64_GCC
  %macro MYPUSHAD 0
    push    r15
    push    r14
    push    r13
    push    r12
    push    rbx
  %endmacro
  %macro MYPOPAD 0
    pop     rbx
    pop     r12
    pop     r13
    pop     r14
    pop     r15
  %endmacro

 %else ; ASM_CALL64_MSC
  %macro MYPUSHAD 0
    push    r15
    push    r14
    push    r13
    push    r12
    push    rbx
    push    rsi
    push    rdi
  %endmacro
  %macro MYPOPAD 0
    pop     rsi
    pop     rdi
    pop     rbx
    pop     r12
    pop     r13
    pop     r14
    pop     r15
  %endmacro
 %endif
    ;; @todo check ds,es saving/restoring on AMD64
 %macro MYPUSHSEGS 2
    push    gs
    push    fs 
    mov     %2, es
    push    %1
    mov     %2, ds
    push    %1
 %endmacro 
 %macro MYPOPSEGS 2
    pop     %1
    mov     ds, %2
    pop     %1
    mov     es, %2
    pop     fs
    pop     gs
 %endmacro

%else ; __X86__
  %macro MYPUSHAD 0
    pushad
  %endmacro 
  %macro MYPOPAD 0
    popad
  %endmacro

  %macro MYPUSHSEGS 2
    push    ds 
    push    es 
    push    fs 
    push    gs
  %endmacro 
  %macro MYPOPSEGS 2
    pop     gs
    pop     fs
    pop     es
    pop     ds
  %endmacro
%endif


BEGINCODE

;/**
; * Prepares for and executes VMLAUNCH
; *
; * @note identical to VMXResumeVM, except for the vmlaunch/vmresume opcode
; *
; * @returns VBox status code
; * @param   pCtx        Guest context
; */
BEGINPROC VMXStartVM
    push    xBP
    mov     xBP, xSP

    ;/* First we have to save some final CPU context registers. */
%ifdef __AMD64__
    mov     rax, qword .vmlaunch_done
    push    rax
%else
    push    .vmlaunch_done
%endif
    mov     eax, VMX_VMCS_HOST_RIP  ;/* return address (too difficult to continue after VMLAUNCH?) */
    vmwrite xAX, [xSP]
    ;/* @todo assumes success... */
    add     xSP, xS

    ;/* Manual save and restore:
    ; * - General purpose registers except RIP, RSP
    ; *
    ; * Trashed:
    ; * - CR2 (we don't care)
    ; * - LDTR (reset to 0)
    ; * - DRx (presumably not changed at all)
    ; * - DR7 (reset to 0x400)
    ; * - EFLAGS (reset to BIT(1); not relevant)
    ; *
    ; */

    ;/* Save all general purpose host registers. */
    MYPUSHAD

    ;/* Save segment registers */
    MYPUSHSEGS xAX, ax

    ;/* Save the Guest CPU context pointer. */
%ifdef __AMD64__
 %ifdef ASM_CALL64_GCC
    mov     rsi, rdi ; pCtx
 %else
    mov     rsi, rcx ; pCtx
 %endif
%else
    mov     esi, [ebp + 8] ; pCtx
%endif
    push    xSI

    ; Save LDTR
    xor     eax, eax
    sldt    ax
    push    xAX

    ; Restore CR2
    mov     ebx, [xSI + CPUMCTX.cr2]
    mov     cr2, xBX

    mov     eax, VMX_VMCS_HOST_RSP
    vmwrite xAX, xSP
    ;/* @todo assumes success... */
    ;/* Don't mess with ESP anymore!! */

    ;/* Restore Guest's general purpose registers. */
    mov     eax, [xSI + CPUMCTX.eax]
    mov     ebx, [xSI + CPUMCTX.ebx]
    mov     ecx, [xSI + CPUMCTX.ecx]
    mov     edx, [xSI + CPUMCTX.edx]
    mov     edi, [xSI + CPUMCTX.edi]
    mov     ebp, [xSI + CPUMCTX.ebp]
    mov     esi, [xSI + CPUMCTX.esi]

    vmlaunch
    jmp     .vmlaunch_done;      ;/* here if vmlaunch detected a failure. */

ALIGNCODE(16)
.vmlaunch_done:
    jnc     .vmxstart_good

    pop     xAX         ; saved LDTR
    lldt    ax

    add     xSP, xS     ; pCtx

    ; Restore segment registers
    MYPOPSEGS xAX, ax

    ;/* Restore all general purpose host registers. */
    MYPOPAD
    mov     eax, VERR_VMX_INVALID_VMXON_PTR
    jmp     .vmstart_end

.vmxstart_good:
    jnz     .vmxstart_success

    pop     xAX         ; saved LDTR
    lldt    ax

    add     xSP, xS     ; pCtx

    ; Restore segment registers
    MYPOPSEGS xAX, ax

    ; Restore all general purpose host registers.
    MYPOPAD
    mov     eax, VERR_VMX_UNABLE_TO_START_VM
    jmp     .vmstart_end

.vmxstart_success:
    push    xDI
    mov     xDI, [xSP + xS * 2]          ;/* pCtx */

    mov     [ss:xDI + CPUMCTX.eax], eax
    mov     [ss:xDI + CPUMCTX.ebx], ebx
    mov     [ss:xDI + CPUMCTX.ecx], ecx
    mov     [ss:xDI + CPUMCTX.edx], edx
    mov     [ss:xDI + CPUMCTX.esi], esi
    mov     [ss:xDI + CPUMCTX.ebp], ebp
%ifdef __AMD64__
    pop     xAX                                 ; the guest edi we pushed above
    mov     dword [ss:xDI + CPUMCTX.edi], eax
%else
    pop     dword [ss:xDI + CPUMCTX.edi]        ; the guest edi we pushed above
%endif

    pop     xAX         ; saved LDTR
    lldt    ax

    add     xSP, 4      ; pCtx

    ; Restore segment registers
    MYPOPSEGS xAX, ax

    ; Restore general purpose registers
    MYPOPAD

    mov     eax, VINF_SUCCESS

.vmstart_end:
    pop     xBP
    ret
ENDPROC VMXStartVM


;/**
; * Prepares for and executes VMRESUME
; *
; * @note identical to VMXStartVM, except for the vmlaunch/vmresume opcode
; *
; * @returns VBox status code
; * @param   pCtx        Guest context
; */
BEGINPROC VMXResumeVM
    push    xBP
    mov     xBP, xSP

    ;/* First we have to save some final CPU context registers. */
%ifdef __AMD64__
    mov     rax, qword vmresume_done
    push    rax
%else
    push    vmresume_done
%endif
    mov     eax, VMX_VMCS_HOST_RIP  ;/* return address (too difficult to continue after VMLAUNCH?) */
    vmwrite xAX, [xSP]
    ;/* @todo assumes success... */
    add     xSP, xS

    ;/* Manual save and restore:
    ; * - General purpose registers except RIP, RSP
    ; *
    ; * Trashed:
    ; * - CR2 (we don't care)
    ; * - LDTR (reset to 0)
    ; * - DRx (presumably not changed at all)
    ; * - DR7 (reset to 0x400)
    ; * - EFLAGS (reset to BIT(1); not relevant)
    ; *
    ; */

    ;/* Save all general purpose host registers. */
    MYPUSHAD

    ;/* Save segment registers */
    MYPUSHSEGS xAX, ax

    ;/* Save the Guest CPU context pointer. */
%ifdef __AMD64__
 %ifdef ASM_CALL64_GCC
    mov     rsi, rdi        ; pCtx
 %else
    mov     rsi, rcx        ; pCtx
 %endif
%else
    mov     esi, [ebp + 8]  ; pCtx
%endif
    push    xSI

    ; Save LDTR
    xor     eax, eax
    sldt    ax
    push    xAX

    ; Restore CR2
    mov     xBX, [xSI + CPUMCTX.cr2]
    mov     cr2, xBX

    mov     eax, VMX_VMCS_HOST_RSP
    vmwrite xAX, xSP
    ;/* @todo assumes success... */
    ;/* Don't mess with ESP anymore!! */

    ;/* Restore Guest's general purpose registers. */
    mov     eax, [xSI + CPUMCTX.eax]
    mov     ebx, [xSI + CPUMCTX.ebx]
    mov     ecx, [xSI + CPUMCTX.ecx]
    mov     edx, [xSI + CPUMCTX.edx]
    mov     edi, [xSI + CPUMCTX.edi]
    mov     ebp, [xSI + CPUMCTX.ebp]
    mov     esi, [xSI + CPUMCTX.esi]

    vmresume
    jmp     vmresume_done;      ;/* here if vmresume detected a failure. */

ALIGNCODE(16)
vmresume_done:
    jnc     vmresume_good

    pop     xAX                         ; saved LDTR
    lldt    ax

    add     xSP, xS                     ; pCtx

    ; Restore segment registers
    MYPOPSEGS xAX, ax

    ; Restore all general purpose host registers.
    MYPOPAD
    mov     eax, VERR_VMX_INVALID_VMXON_PTR
    jmp     vmresume_end

vmresume_good:
    jnz     vmresume_success

    pop     xAX                         ; saved LDTR
    lldt    ax

    add     xSP, xS                     ; pCtx

    ; Restore segment registers
    MYPOPSEGS xAX, ax

    ; Restore all general purpose host registers.
    MYPOPAD
    mov     eax, VERR_VMX_UNABLE_TO_RESUME_VM
    jmp     vmresume_end

vmresume_success:
    push    xDI
    mov     xDI, [xSP + xS * 2]         ; pCtx 

    mov     [ss:xDI + CPUMCTX.eax], eax
    mov     [ss:xDI + CPUMCTX.ebx], ebx
    mov     [ss:xDI + CPUMCTX.ecx], ecx
    mov     [ss:xDI + CPUMCTX.edx], edx
    mov     [ss:xDI + CPUMCTX.esi], esi
    mov     [ss:xDI + CPUMCTX.ebp], ebp
%ifdef __AMD64__
    pop     xAX                                 ; the guest edi we pushed above
    mov     dword [ss:xDI + CPUMCTX.edi], eax
%else
    pop     dword [ss:xDI + CPUMCTX.edi]        ; the guest edi we pushed above
%endif

    pop     xAX          ; saved LDTR
    lldt    ax

    add     xSP, xS      ; pCtx

    ; Restore segment registers
    MYPOPSEGS xAX, ax

    ; Restore general purpose registers
    MYPOPAD

    mov     eax, VINF_SUCCESS

vmresume_end:
    pop     xBP
    ret
ENDPROC VMXResumeVM


%ifdef __AMD64__
;/**
; * Executes VMWRITE
; *
; * @returns VBox status code
; * @param   idxField   x86: [ebp + 08h]  msc: rcx  gcc: edi   VMCS index
; * @param   pData      x86: [ebp + 0ch]  msc: rdx  gcc: rsi   Ptr to store VM field value
; */
BEGINPROC VMXWriteVMCS64
%ifdef ASM_CALL64_GCC
    and         edi, 0ffffffffh; serious paranoia
    vmwrite     rdi, [rsi]
%else
    and         ecx, 0ffffffffh; serious paranoia
    vmwrite     rcx, [rdx]
%endif
    jnc         .valid_vmcs
    mov         eax, VERR_VMX_INVALID_VMCS_PTR
    ret
.valid_vmcs:
    jnz         .the_end
    mov         eax, VERR_VMX_INVALID_VMCS_FIELD
.the_end:
    ret
ENDPROC VMXWriteVMCS64

;/**
; * Executes VMREAD
; *
; * @returns VBox status code
; * @param   idxField        VMCS index
; * @param   pData           Ptr to store VM field value
; */
;DECLASM(int) VMXReadVMCS64(uint32_t idxField, uint64_t *pData);
BEGINPROC VMXReadVMCS64
%ifdef ASM_CALL64_GCC
    and         edi, 0ffffffffh; serious paranoia
    vmread      [rsi], rdi
%else
    and         ecx, 0ffffffffh; serious paranoia
    vmread      [rdx], rcx
%endif
    jnc         .valid_vmcs
    mov         eax, VERR_VMX_INVALID_VMCS_PTR
    ret
.valid_vmcs:
    jnz         .the_end
    mov         eax, VERR_VMX_INVALID_VMCS_FIELD
.the_end:
    ret
ENDPROC VMXReadVMCS64


;/**
; * Executes VMXON
; *
; * @returns VBox status code
; * @param   HCPhysVMXOn      Physical address of VMXON structure
; */
;DECLASM(int) VMXEnable(RTHCPHYS HCPhysVMXOn);
BEGINPROC VMXEnable
%ifdef __AMD64__
 %ifdef ASM_CALL64_GCC
    push    rdi
 %else
    push    rcx
 %endif
    vmxon   [rsp]
%else
    vmxon   [esp + 4]
%endif
    jnc     .good
    mov     eax, VERR_VMX_INVALID_VMXON_PTR
    jmp     .the_end

.good:
    jnz     .the_end
    mov     eax, VERR_VMX_GENERIC

.the_end:
%ifdef __AMD64__
    add     rsp, 8
%endif
    ret
ENDPROC VMXEnable


;/**
; * Executes VMXOFF
; */
;DECLASM(void) VMXDisable(void);
BEGINPROC VMXDisable
    vmxoff
    ret
ENDPROC VMXDisable


;/**
; * Executes VMCLEAR
; *
; * @returns VBox status code
; * @param   HCPhysVMCS     Physical address of VM control structure
; */
;DECLASM(int) VMXClearVMCS(RTHCPHYS HCPhysVMCS);
BEGINPROC VMXClearVMCS
%ifdef __AMD64__
 %ifdef ASM_CALL64_GCC
    push    rdi
 %else
    push    rcx
 %endif
    vmclear [rsp]
%else
    vmclear [esp + 4]
%endif
    jnc     .the_end
    mov     eax, VERR_VMX_INVALID_VMCS_PTR
.the_end:
%ifdef __AMD64__
    add     rsp, 8
%endif
    ret
ENDPROC VMXClearVMCS


;/**
; * Executes VMPTRLD
; *
; * @returns VBox status code
; * @param   HCPhysVMCS     Physical address of VMCS structure
; */
;DECLASM(int) VMXActivateVMCS(RTHCPHYS HCPhysVMCS);
BEGINPROC VMXActivateVMCS
%ifdef __AMD64__
 %ifdef ASM_CALL64_GCC
    push    rdi
 %else
    push    rcx
 %endif
    vmclear [rsp]
%else
    vmclear [esp + 4]
%endif
    jnc     .the_end
    mov     eax, VERR_VMX_INVALID_VMCS_PTR
.the_end:
%ifdef __AMD64__
    add     rsp, 8
%endif
    ret
ENDPROC VMXActivateVMCS

%endif ; __AMD64__


;/**
; * Prepares for and executes VMRUN
; *
; * @returns VBox status code
; * @param   HCPhysVMCB     Physical address of host VMCB
; * @param   HCPhysVMCB     Physical address of guest VMCB
; * @param   pCtx           Guest context
; */
BEGINPROC SVMVMRun
%ifdef __AMD64__ ; fake a cdecl stack frame - I'm lazy, sosume.
 %ifdef ASM_CALL64_GCC
    push    rdx
    push    rsi
    push    rdi
 %else
    push    r8
    push    rdx
    push    rcx
 %endif
    push    0
%endif
    push    xBP
    mov     xBP, xSP

    ;/* Manual save and restore:
    ; * - General purpose registers except RIP, RSP, RAX
    ; *
    ; * Trashed:
    ; * - CR2 (we don't care)
    ; * - LDTR (reset to 0)
    ; * - DRx (presumably not changed at all)
    ; * - DR7 (reset to 0x400)
    ; */

    ;/* Save all general purpose host registers. */
    MYPUSHAD

    ; /* Clear fs and gs as a safety precaution. Maybe not necessary. */
    push    fs
    push    gs
    xor     eax, eax
    mov     fs, eax
    mov     gs, eax

    ;/* Save the Guest CPU context pointer. */
    mov     xSI, [xBP + xS*2 + RTHCPHYS_CB*2]   ; pCtx
    push    xSI                     ; push for saving the state at the end 

    ; Restore CR2
    mov     ebx, [xSI + CPUMCTX.cr2]
    mov     cr2, xBX

    ; save host fs, gs, sysenter msr etc
    mov     xAX, [xBP + xS*2]       ; pVMCBHostPhys (64 bits physical address; x86: take low dword only)
    push    xAX                     ; save for the vmload after vmrun
    DB      0x0F, 0x01, 0xDB        ; VMSAVE

    ; setup eax for VMLOAD
    mov     xAX, [xBP + xS*2 + RTHCPHYS_CB]     ; pVMCBPhys (64 bits physical address; take low dword only)

    ;/* Restore Guest's general purpose registers. */
    ;/* EAX is loaded from the VMCB by VMRUN */
    mov     ebx, [xSI + CPUMCTX.ebx]
    mov     ecx, [xSI + CPUMCTX.ecx]
    mov     edx, [xSI + CPUMCTX.edx]
    mov     edi, [xSI + CPUMCTX.edi]
    mov     ebp, [xSI + CPUMCTX.ebp]
    mov     esi, [xSI + CPUMCTX.esi]

    ; Clear the global interrupt flag & execute sti to make sure external interrupts cause a world switch
    DB      0x0f, 0x01, 0xDD        ; CLGI
    sti

    ; load guest fs, gs, sysenter msr etc
    DB      0x0f, 0x01, 0xDA        ; VMLOAD
    ; run the VM
    DB      0x0F, 0x01, 0xD8        ; VMRUN

    ;/* EAX is in the VMCB already; we can use it here. */

    ; save guest fs, gs, sysenter msr etc
    DB      0x0F, 0x01, 0xDB        ; VMSAVE

    ; load host fs, gs, sysenter msr etc
    pop     xAX                     ; pushed above
    DB      0x0F, 0x01, 0xDA        ; VMLOAD

    ; Set the global interrupt flag again, but execute cli to make sure IF=0.
    cli
    DB      0x0f, 0x01, 0xDC        ; STGI

    pop     xAX                     ; pCtx

    mov     [ss:xAX + CPUMCTX.ebx], ebx
    mov     [ss:xAX + CPUMCTX.ecx], ecx
    mov     [ss:xAX + CPUMCTX.edx], edx
    mov     [ss:xAX + CPUMCTX.esi], esi
    mov     [ss:xAX + CPUMCTX.edi], edi
    mov     [ss:xAX + CPUMCTX.ebp], ebp

    ; Restore fs & gs
    pop     gs
    pop     fs

    ; Restore general purpose registers
    MYPOPAD

    mov     eax, VINF_SUCCESS

    pop     xBP
%ifdef __AMD64__
    add     xSP, 4*xS
%endif
    ret
ENDPROC SVMVMRun

%ifdef __AMD64__
%ifdef __WIN__

;;
; Executes INVLPGA
; 
; @param   pPageGC  msc:ecx  gcc:edi  x86:[esp+04]  Virtual page to invalidate
; @param   uASID    msc:edx  gcc:esi  x86:[esp+08]  Tagged TLB id
; 
;DECLASM(void) SVMInvlpgA(RTGCPTR pPageGC, uint32_t uASID);
BEGINPROC SVMInvlpgA
%ifdef __AMD64__
 %ifdef ASM_CALL64_GCC
    mov     eax, edi                    ;; @todo 64-bit guest.
    mov     ecx, esi
 %else
    mov     eax, ecx                    ;; @todo 64-bit guest.
    mov     ecx, edx
 %endif
    invlpga rax, ecx
%else
    mov     eax, [esp + 4]
    mov     ecx, [esp + 8]
    invlpga eax, ecx
%endif
    ret
ENDPROC SVMInvlpgA
%endif
%endif
