;; @file
;
; VMXM - R0 vmx helpers

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
    push    ebp
    mov     ebp, esp

    ;/* First we have to save some final CPU context registers. */
    push    vmlaunch_done
    mov     eax, VMX_VMCS_HOST_RIP  ;/* return address (too difficult to continue after VMLAUNCH?) */
    vmwrite eax, [esp]
    ;/* @todo assumes success... */
    add     esp, 4

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
    pushad

    ;/* Save segment registers */
    push    ds
    push    es
    push    fs
    push    gs

    ;/* Save the Guest CPU context pointer. */
    mov     esi, [ebp + 8] ; pCtx
    push    esi

    ; Save LDTR
    xor     eax, eax
    sldt    ax
    push    eax

    ; Restore CR2
    mov     ebx, [esi + CPUMCTX.cr2]
    mov     cr2, ebx

    mov     eax, VMX_VMCS_HOST_RSP
    vmwrite eax, esp
    ;/* @todo assumes success... */
    ;/* Don't mess with ESP anymore!! */

    ;/* Restore Guest's general purpose registers. */
    mov     eax, [esi + CPUMCTX.eax]
    mov     ebx, [esi + CPUMCTX.ebx]
    mov     ecx, [esi + CPUMCTX.ecx]
    mov     edx, [esi + CPUMCTX.edx]
    mov     edi, [esi + CPUMCTX.edi]
    mov     ebp, [esi + CPUMCTX.ebp]
    mov     esi, [esi + CPUMCTX.esi]

    vmlaunch
    jmp     vmlaunch_done;      ;/* here if vmlaunch detected a failure. */

ALIGNCODE(16)
vmlaunch_done:
    jnc     vmxstart_good

    pop     eax         ; saved LDTR
    lldt    ax

    add     esp, 4      ; pCtx

    ; Restore segment registers
    pop     gs
    pop     fs
    pop     es
    pop     ds

    ;/* Restore all general purpose host registers. */
    popad
    mov     eax, VERR_VMX_INVALID_VMXON_PTR
    jmp     vmstart_end

vmxstart_good:
    jnz     vmxstart_success

    pop     eax         ; saved LDTR
    lldt    ax

    add     esp, 4      ; pCtx

    ; Restore segment registers
    pop     gs
    pop     fs
    pop     es
    pop     ds
    ;/* Restore all general purpose host registers. */
    popad
    mov     eax, VERR_VMX_UNABLE_TO_START_VM
    jmp     vmstart_end

vmxstart_success:
    push    edi
    mov     edi, dword [esp+8]      ;/* pCtx */

    mov     [ss:edi + CPUMCTX.eax], eax
    mov     [ss:edi + CPUMCTX.ebx], ebx
    mov     [ss:edi + CPUMCTX.ecx], ecx
    mov     [ss:edi + CPUMCTX.edx], edx
    mov     [ss:edi + CPUMCTX.esi], esi
    mov     [ss:edi + CPUMCTX.ebp], ebp
    pop     dword [ss:edi + CPUMCTX.edi]     ; guest edi we pushed above

    pop     eax         ; saved LDTR
    lldt    ax

    add     esp, 4      ; pCtx

    ; Restore segment registers
    pop     gs
    pop     fs
    pop     es
    pop     ds

    ; Restore general purpose registers
    popad

    mov     eax, VINF_SUCCESS

vmstart_end:
    pop     ebp
    ret
ENDPROC VMStartVM


;/**
; * Prepares for and executes VMRESUME
; *
; * @note identical to VMXStartVM, except for the vmlaunch/vmresume opcode
; *
; * @returns VBox status code
; * @param   pCtx        Guest context
; */
BEGINPROC VMXResumeVM
    push    ebp
    mov     ebp, esp

    ;/* First we have to save some final CPU context registers. */
    push    vmresume_done
    mov     eax, VMX_VMCS_HOST_RIP  ;/* return address (too difficult to continue after VMLAUNCH?) */
    vmwrite eax, [esp]
    ;/* @todo assumes success... */
    add     esp, 4

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
    pushad

    ;/* Save segment registers */
    push    ds
    push    es
    push    fs
    push    gs

    ;/* Save the Guest CPU context pointer. */
    mov     esi, [ebp + 8] ; pCtx
    push    esi

    ; Save LDTR
    xor     eax, eax
    sldt    ax
    push    eax

    ; Restore CR2
    mov     ebx, [esi + CPUMCTX.cr2]
    mov     cr2, ebx

    mov     eax, VMX_VMCS_HOST_RSP
    vmwrite eax, esp
    ;/* @todo assumes success... */
    ;/* Don't mess with ESP anymore!! */

    ;/* Restore Guest's general purpose registers. */
    mov     eax, [esi + CPUMCTX.eax]
    mov     ebx, [esi + CPUMCTX.ebx]
    mov     ecx, [esi + CPUMCTX.ecx]
    mov     edx, [esi + CPUMCTX.edx]
    mov     edi, [esi + CPUMCTX.edi]
    mov     ebp, [esi + CPUMCTX.ebp]
    mov     esi, [esi + CPUMCTX.esi]

    vmresume
    jmp     vmresume_done;      ;/* here if vmresume detected a failure. */

ALIGNCODE(16)
vmresume_done:
    jnc     vmresume_good

    pop     eax         ; saved LDTR
    lldt    ax

    add     esp, 4      ; pCtx

    ; Restore segment registers
    pop     gs
    pop     fs
    pop     es
    pop     ds

    ;/* Restore all general purpose host registers. */
    popad
    mov     eax, VERR_VMX_INVALID_VMXON_PTR
    jmp     vmresume_end

vmresume_good:
    jnz     vmresume_success

    pop     eax         ; saved LDTR
    lldt    ax

    add     esp, 4      ; pCtx

    ; Restore segment registers
    pop     gs
    pop     fs
    pop     es
    pop     ds
    ;/* Restore all general purpose host registers. */
    popad
    mov     eax, VERR_VMX_UNABLE_TO_RESUME_VM
    jmp     vmresume_end

vmresume_success:
    push    edi
    mov     edi, dword [esp+8]      ;/* pCtx */

    mov     [ss:edi + CPUMCTX.eax], eax
    mov     [ss:edi + CPUMCTX.ebx], ebx
    mov     [ss:edi + CPUMCTX.ecx], ecx
    mov     [ss:edi + CPUMCTX.edx], edx
    mov     [ss:edi + CPUMCTX.esi], esi
    mov     [ss:edi + CPUMCTX.ebp], ebp
    pop     dword [ss:edi + CPUMCTX.edi]     ; guest edi we pushed above

    pop     eax         ; saved LDTR
    lldt    ax

    add     esp, 4      ; pCtx

    ; Restore segment registers
    pop     gs
    pop     fs
    pop     es
    pop     ds

    ; Restore general purpose registers
    popad

    mov     eax, VINF_SUCCESS

vmresume_end:
    pop     ebp
    ret
ENDPROC VMXResumeVM




;/**
; * Prepares for and executes VMRUN
; *
; * @returns VBox status code
; * @param   pVMCBHostPhys  Physical address of host VMCB
; * @param   pVMCBPhys      Physical address of guest VMCB
; * @param   pCtx           Guest context
; */
BEGINPROC SVMVMRun
    push    ebp
    mov     ebp, esp

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
    pushad

    ; /* Clear fs and gs as a safety precaution. Maybe not necessary. */
    push    fs
    push    gs
    xor     eax, eax
    mov     fs, eax
    mov     gs, eax

    ;/* Save the Guest CPU context pointer. */
    mov     esi, [ebp + 24] ; pCtx
    push    esi

    ; Restore CR2
    mov     ebx, [esi + CPUMCTX.cr2]
    mov     cr2, ebx

    ; save host fs, gs, sysenter msr etc
    mov     eax, [ebp + 8]          ; pVMCBHostPhys (64 bits physical address; take low dword only)
    push    eax                     ; save for the vmload after vmrun
    DB      0x0F, 0x01, 0xDB        ; VMSAVE

    ; setup eax for VMLOAD
    mov     eax, [ebp + 16]         ; pVMCBPhys (64 bits physical address; take low dword only)

    ;/* Restore Guest's general purpose registers. */
    ;/* EAX is loaded from the VMCB by VMRUN */
    mov     ebx, [esi + CPUMCTX.ebx]
    mov     ecx, [esi + CPUMCTX.ecx]
    mov     edx, [esi + CPUMCTX.edx]
    mov     edi, [esi + CPUMCTX.edi]
    mov     ebp, [esi + CPUMCTX.ebp]
    mov     esi, [esi + CPUMCTX.esi]

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
    pop     eax                     ; pushed above
    DB      0x0F, 0x01, 0xDA        ; VMLOAD

    ; Set the global interrupt flag again, but execute cli to make sure IF=0.
    cli
    DB      0x0f, 0x01, 0xDC        ; STGI

    pop     eax         ; pCtx

    mov     [ss:eax + CPUMCTX.ebx], ebx
    mov     [ss:eax + CPUMCTX.ecx], ecx
    mov     [ss:eax + CPUMCTX.edx], edx
    mov     [ss:eax + CPUMCTX.esi], esi
    mov     [ss:eax + CPUMCTX.edi], edi
    mov     [ss:eax + CPUMCTX.ebp], ebp

    ; Restore fs & gs
    pop     gs
    pop     fs

    ; Restore general purpose registers
    popad

    mov     eax, VINF_SUCCESS

    pop     ebp
    ret
ENDPROC SVMVMRun
