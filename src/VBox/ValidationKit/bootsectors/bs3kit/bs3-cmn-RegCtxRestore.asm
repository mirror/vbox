; $Id$
;; @file
; BS3Kit - Bs3RegCtxRestore.
;

;
; Copyright (C) 2007-2016 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;
; The contents of this file may alternatively be used under the terms
; of the Common Development and Distribution License Version 1.0
; (CDDL) only, as it comes in the "COPYING.CDDL" file of the
; VirtualBox OSE distribution, in which case the provisions of the
; CDDL are applicable instead of those of the GPL.
;
; You may elect to license modified versions of this file under the
; terms and conditions of either the GPL or the CDDL or both.
;

%include "bs3kit-template-header.mac"


BS3_EXTERN_SYSTEM16 Bs3Gdt
BS3_EXTERN_DATA16 g_bBs3CurrentMode
%if TMPL_BITS != 64
BS3_EXTERN_DATA16 g_uBs3CpuDetected
%endif
TMPL_BEGIN_TEXT
BS3_EXTERN_CMN Bs3Syscall
TMPL_BEGIN_TEXT


;;
; Restores the given register context.
;
; @param        pRegCtx
; @param        fFlags
; @uses         All registers and may trash stack immediately before the resume point.
;
; @note         Only respects the BS3_MODE_CODE_MASK part of pRegCtx->bMode.
;
%if TMPL_BITS == 16 || TMPL_BITS == 32
BS3_PROC_BEGIN_CMN Bs3RegCtxRestore_aborts ; special entry point for when watcom applies __aborts
 %if TMPL_BITS == 16
        CPU 8086
        xor     xAX, xAX
        push    xAX                     ; fake return address.
 %else
        push    0feedfaceh              ; fake return address.
 %endif
%endif
BS3_PROC_BEGIN_CMN Bs3RegCtxRestore
        BS3_CALL_CONV_PROLOG 2
        push    xBP
        mov     xBP, xSP

        ;
        ; If we're not in ring-0, ask the kernel to restore it for us (quicker
        ; and less problematic if we're in a funny context right now with weird
        ; CS or SS values).
        ;
        mov     ax, ss
        test    al, 3
        jz      .in_ring0
%if TMPL_BITS == 16
        mov     si, [bp + 4]
        mov     cx, [bp + 4+2]
        mov     dx, [bp + 8]
        mov     ax, BS3_SYSCALL_RESTORE_CTX
%else
        mov     cx, ds
        mov     xSI, [xBP + xCB*2]
        movzx   edx, word [xBP + xCB*3]
        mov     eax, BS3_SYSCALL_RESTORE_CTX
%endif
        call    Bs3Syscall
.in_ring0:

        ;
        ; Prologue.  Loads ES with BS3KIT_GRPNM_DATA16/FLAT (for g_bBs3CurrentMode
        ; and g_uBs3CpuDetected), DS:xBX with pRegCtx and fFlags into xCX.
        ;
%if TMPL_BITS == 16
        mov     ax, BS3_SEL_DATA16
        mov     es, ax
        lds     bx, [bp + 4]
        mov     cx, [bp + 8]
%elif TMPL_BITS == 32
        mov     ax, BS3_SEL_R0_DS32
        mov     ds, ax
        mov     xBX, [xBP + xCB*2]
        movzx   xCX, word [xBP + xCB*3]
%else
        mov     ax, BS3_SEL_R0_DS64
        mov     ds, ax
        mov     xBX, [xBP + xCB*2]
        movzx   xCX, word [xBP + xCB*3]
%endif

        ; The remainder must be done with interrupts disabled.
        cli

        ;
        ; Update g_bs3CurrentMode.
        ;
        mov     al, [xBX + BS3REGCTX.bMode]
        and     al, BS3_MODE_CODE_MASK
        mov     ah, [BS3_ONLY_16BIT(es:) BS3_DATA16_WRT(g_bBs3CurrentMode)]
        and     ah, ~BS3_MODE_CODE_MASK
        or      al, ah
        mov     [BS3_ONLY_16BIT(es:) BS3_DATA16_WRT(g_bBs3CurrentMode)], al

%if TMPL_BITS == 16
        ;
        ; Check what the CPU can do.
        ;
        cmp     byte [es:BS3_DATA16_WRT(g_uBs3CpuDetected)], BS3CPU_80386
        jae     .restore_full

        ; Do the 80286 specifics first.
        cmp     byte [es:BS3_DATA16_WRT(g_uBs3CpuDetected)], BS3CPU_80286
        jb      .restore_16_bit_ancient
        CPU 286

        lmsw    [bx + BS3REGCTX.cr0]
        cmp     byte [es:BS3_DATA16_WRT(g_bBs3CurrentMode)], BS3_MODE_RM
        je      .restore_16_bit_ancient
        lldt    [bx + BS3REGCTX.ldtr]

        ; TR - complicated because we need to clear the busy bit. ASSUMES GDT.
        str     ax
        cmp     ax, [bx + BS3REGCTX.tr]
        je      .skip_tr_286

        mov     di, word [xBX + BS3REGCTX.tr]
        or      di, di                  ; check for null.
        jz      .load_tr_286

        push    ds
        push    BS3_SEL_SYSTEM16
        pop     ds
        add     di, Bs3Gdt wrt BS3SYSTEM16
        add     di, X86DESCGENERIC_BIT_OFF_TYPE / 8
        and     byte [di], ~(X86_SEL_TYPE_SYS_TSS_BUSY_MASK << (X86DESCGENERIC_BIT_OFF_TYPE % 8))
        pop     ds

.load_tr_286:
        ltr     [bx + BS3REGCTX.tr]
.skip_tr_286:

.restore_16_bit_ancient:
        CPU 8086
        ; Some general registers.
        mov     cx, [bx + BS3REGCTX.rcx]
        mov     dx, [bx + BS3REGCTX.rdx]

        ; Do the return frame and final registers (keep short as we're not quite
        ; NMI safe here if pRegCtx is on the stack).
        cmp     byte [es:BS3_DATA16_WRT(g_bBs3CurrentMode)], BS3_MODE_RM
        mov     di, [bx + BS3REGCTX.rsp]
        je      .restore_16_bit_same_privilege
        cmp     byte [bx + BS3REGCTX.bCpl], 0
        je      .restore_16_bit_same_privilege

        mov     ax, [bx + BS3REGCTX.ss]
        push    ax
        mov     ax, [bx + BS3REGCTX.rsp]
        push    ax
        mov     ax, [bx + BS3REGCTX.rflags]
        push    ax
        mov     ax, [bx + BS3REGCTX.cs]
        push    ax
        mov     ax, [bx + BS3REGCTX.rip]
        push    ax
        mov     ax, [bx + BS3REGCTX.ds]
        push    ax

        mov     si, [bx + BS3REGCTX.rsi]
        mov     di, [bx + BS3REGCTX.rdi]
        mov     es, [bx + BS3REGCTX.es]
        mov     ax, [bx + BS3REGCTX.rax]
        mov     bp, [bx + BS3REGCTX.rbp]    ; restore late for better stacks.
        mov     bx, [bx + BS3REGCTX.rbx]

        pop     ds
        iret

.restore_16_bit_same_privilege:
        sub     di, 2*5                     ; iret frame + pop ds
        mov     si, di
        mov     es, [bx + BS3REGCTX.ss]     ; ES is target stack segment.
        cld

        mov     ax, [bx + BS3REGCTX.ds]
        stosw
        mov     ax, [bx + BS3REGCTX.rbp]    ; Restore esp as late as possible for better stacks.
        stosw
        mov     ax, [bx + BS3REGCTX.rip]
        stosw
        mov     ax, [bx + BS3REGCTX.cs]
        stosw
        mov     ax, [bx + BS3REGCTX.rflags]
        stosw

        mov     di, [bx + BS3REGCTX.rdi]
        mov     es, [bx + BS3REGCTX.es]
        mov     ax, [bx + BS3REGCTX.rax]
        mov     ss, [bx + BS3REGCTX.ss]
        mov     sp, si
        mov     si, [bx + BS3REGCTX.rsi]
        mov     bx, [bx + BS3REGCTX.rbx]

        pop     ds
        pop     bp
        iret

        CPU 386
%endif

.restore_full:
        ;
        ; 80386 or later.
        ; For 32-bit and 16-bit versions, we always use 32-bit iret.
        ;

        ; Restore control registers if they've changed.
        test    cl, BS3TRAPRESUME_F_SKIP_CRX
        jnz     .skip_control_regs
        test    byte [xBX + BS3REGCTX.fbFlags], BS3REG_CTX_F_NO_CR
        jnz     .skip_control_regs

        test    byte [xBX + BS3REGCTX.fbFlags], BS3REG_CTX_F_NO_CR4 ; (old 486s and 386s didn't have CR4)
        jnz     .skip_cr4
%if TMPL_BITS != 64
        test    word [BS3_ONLY_16BIT(es:) BS3_DATA16_WRT(g_uBs3CpuDetected)], BS3CPU_F_CPUID
        jz      .skip_cr4
%endif
        mov     sAX, [xBX + BS3REGCTX.cr4]
        mov     sDX, cr4
        cmp     sAX, sDX
        je      .skip_cr4
        mov     cr4, sAX
.skip_cr4:

        mov     sAX, [xBX + BS3REGCTX.cr0]
        mov     sDX, cr0
        cmp     sAX, sDX
        je      .skip_cr0
        mov     cr0, sAX
.skip_cr0:

        mov     sAX, [xBX + BS3REGCTX.cr3]
        mov     sDX, cr3
        cmp     sAX, sDX
        je      .skip_cr3
        mov     cr3, sAX
.skip_cr3:

        mov     sAX, [xBX + BS3REGCTX.cr2]
        mov     sDX, cr2
        cmp     sAX, sDX
        je      .skip_cr2
        mov     cr2, sAX
.skip_cr2:

        ;
        ; Restore
        ;
%if TMPL_BITS != 64
        ; We cannot restore ldtr and tr if we're in real-mode.
        cmp     byte [BS3_ONLY_16BIT(es:) BS3_DATA16_WRT(g_bBs3CurrentMode)], BS3_MODE_RM
        je      .skip_control_regs
%endif

        ; LDTR
        sldt    ax
        cmp     ax, [xBX + BS3REGCTX.ldtr]
        je      .skip_ldtr
        lldt    [xBX + BS3REGCTX.ldtr]
.skip_ldtr:

        ; TR - complicated because we need to clear the busy bit. ASSUMES GDT.
        str     ax
        cmp     ax, [xBX + BS3REGCTX.tr]
        je      .skip_tr

        movzx   edi, word [xBX + BS3REGCTX.tr]
        or      edi, edi                ; check for null.
        jz      .load_tr

%if TMPL_BITS == 16
        push    ds
        push    BS3_SEL_SYSTEM16
        pop     ds
        add     xDI, Bs3Gdt wrt BS3SYSTEM16
%else
        add     xDI, Bs3Gdt wrt FLAT
%endif
        add     xDI, X86DESCGENERIC_BIT_OFF_TYPE / 8
        and     byte [xDI], ~(X86_SEL_TYPE_SYS_TSS_BUSY_MASK << (X86DESCGENERIC_BIT_OFF_TYPE % 8))
%if TMPL_BITS == 16
        pop     ds
%endif
.load_tr:
        ltr     [xBX + BS3REGCTX.tr]
.skip_tr:

.skip_control_regs:


%if TMPL_BITS == 64
        ;
        ; 64-bit returns are simple because ss:rsp are always restored.
        ;
        movzx   eax, word [xBX + BS3REGCTX.ss]
        push    rax
        push    qword [xBX + BS3REGCTX.rsp]
        push    qword [xBX + BS3REGCTX.rflags]
        movzx   eax, word [xBX + BS3REGCTX.cs]
        push    rax
        push    qword [xBX + BS3REGCTX.rip]

        mov     es,  [xBX + BS3REGCTX.es]
        mov     fs,  [xBX + BS3REGCTX.fs]
        mov     gs,  [xBX + BS3REGCTX.gs]
        mov     rax, [xBX + BS3REGCTX.rax]
        mov     rdx, [xBX + BS3REGCTX.rdx]
        mov     rcx, [xBX + BS3REGCTX.rcx]
        mov     rsi, [xBX + BS3REGCTX.rsi]
        mov     rdi, [xBX + BS3REGCTX.rdi]
        mov     r8,  [xBX + BS3REGCTX.r8]
        mov     r9,  [xBX + BS3REGCTX.r9]
        mov     r10, [xBX + BS3REGCTX.r10]
        mov     r11, [xBX + BS3REGCTX.r11]
        mov     r12, [xBX + BS3REGCTX.r12]
        mov     r13, [xBX + BS3REGCTX.r13]
        mov     r14, [xBX + BS3REGCTX.r14]
        mov     r15, [xBX + BS3REGCTX.r15]
        mov     rbp, [xBX + BS3REGCTX.rbp] ; restore late for better stacks.
        mov     ds,  [xBX + BS3REGCTX.ds]
        mov     rbx, [xBX + BS3REGCTX.rbx]
        iretq

%else
        ;
        ; 32-bit/16-bit is more complicated as we have three different iret frames.
        ;
        cmp     byte [BS3_ONLY_16BIT(es:) BS3_DATA16_WRT(g_bBs3CurrentMode)], BS3_MODE_RM
        je      .iretd_same_cpl_rm

        test    dword [xBX + BS3REGCTX.rflags], X86_EFL_VM
        jnz     .restore_v8086

        cmp     byte [xBX + BS3REGCTX.bCpl], 0
        je      .iretd_same_cpl

        ;
        ; IRETD to different CPL.  Frame includes ss:esp.
        ;
.iretd_different_cpl:
        or      eax, 0ffffffffh         ; poison unused parts of segment pushes
        mov     ax,   [xBX + BS3REGCTX.ss]
        push    eax
        push    dword [xBX + BS3REGCTX.rsp]
        push    dword [xBX + BS3REGCTX.rflags]
        mov     ax,   [xBX + BS3REGCTX.cs]
        push    eax
        push    dword [xBX + BS3REGCTX.rip]
        push    dword [xBX + BS3REGCTX.rbp] ; Restore esp as late as possible for better stacks.
        mov     ax,   [xBX + BS3REGCTX.ds]
        push    xAX

        mov     es,  [xBX + BS3REGCTX.es]
        mov     fs,  [xBX + BS3REGCTX.fs]
        mov     gs,  [xBX + BS3REGCTX.gs]
        mov     eax, [xBX + BS3REGCTX.rax]
        mov     edx, [xBX + BS3REGCTX.rdx]
        mov     ecx, [xBX + BS3REGCTX.rcx]
        mov     esi, [xBX + BS3REGCTX.rsi]
%if TMPL_BITS == 16 ; if SS is 16-bit, we will not be able to restore the high word.
        mov     edi, [xBX + BS3REGCTX.rsp]
        mov     di, sp
        mov     esp, edi
%endif
        mov     edi, [xBX + BS3REGCTX.rdi]
        mov     ebx, [xBX + BS3REGCTX.rbx]

        pop     ds
        pop     ebp
        iretd

        ;
        ; IRETD to same CPL (includes real mode).
        ;
.iretd_same_cpl_rm:
        ; Use STOSD/ES:EDI to create the frame.
        mov     es,  [xBX + BS3REGCTX.ss]
        movzx   esp, word [xBX + BS3REGCTX.rsp]
        jmp     .using_16_bit_stack_pointer

.iretd_same_cpl:
        ; Use STOSD/ES:EDI to create the frame.
        mov     es,  [xBX + BS3REGCTX.ss]
        mov     edi, [xBX + BS3REGCTX.rsp]
        sub     edi, 5*4

        ; Which part of the stack pointer is actually used depends on the SS.D/B bit.
        lar     eax, [xBX + BS3REGCTX.ss]
        jnz     .using_32_bit_stack_pointer
        test    eax, X86LAR_F_D
        jnz     .using_32_bit_stack_pointer
.using_16_bit_stack_pointer:
        movzx   edi, di
        mov     esi, [xBX + BS3REGCTX.rsp]
        mov     si, di                  ; save rsp for later.
        jmp     .es_edi_is_pointing_to_return_frame_location
.using_32_bit_stack_pointer:
        mov     esi, edi
.es_edi_is_pointing_to_return_frame_location:
        cld
        mov     ax,  [xBX + BS3REGCTX.ds]
        o32 stosd
        mov     eax, [xBX + BS3REGCTX.rbp] ; Restore esp as late as possible for better stacks.
        o32 stosd
        mov     eax, [xBX + BS3REGCTX.rip]
        o32 stosd
        mov     ax,  [xBX + BS3REGCTX.cs]
        o32 stosd
        mov     eax, [xBX + BS3REGCTX.rflags]
        o32 stosd

        mov     es,  [xBX + BS3REGCTX.es]
        mov     fs,  [xBX + BS3REGCTX.fs]
        mov     gs,  [xBX + BS3REGCTX.gs]
        mov     eax, [xBX + BS3REGCTX.rax]
        mov     edx, [xBX + BS3REGCTX.rdx]
        mov     ecx, [xBX + BS3REGCTX.rcx]
        mov     edi, [xBX + BS3REGCTX.rdi]
        mov     ebp, [xBX + BS3REGCTX.rbp] ; restore late for better stacks.

        mov     ss,  [xBX + BS3REGCTX.ss]
        mov     esp, esi
        mov     esi, [xBX + BS3REGCTX.rsi]
        mov     ebx, [xBX + BS3REGCTX.rbx]

        o32 pop ds
        pop     ebp
        iretd

        ;
        ; IRETD to v8086 mode.  Frame includes ss:esp and the 4 data segment registers.
        ;
.restore_v8086:
        or      eax, 0ffffffffh         ; poison unused parts of segment pushes
        mov     eax, [xBX + BS3REGCTX.gs]
        push    eax
        mov     eax, [xBX + BS3REGCTX.fs]
        push    eax
        mov     eax, [xBX + BS3REGCTX.ds]
        push    eax
        mov     eax, [xBX + BS3REGCTX.es]
        push    eax
        mov     eax, [xBX + BS3REGCTX.ss]
        push    eax
        push    dword [xBX + BS3REGCTX.rsp]
        push    dword [xBX + BS3REGCTX.rflags]
        mov     ax,   [xBX + BS3REGCTX.cs]
        push    eax
        push    dword [xBX + BS3REGCTX.rip]

        mov     eax, [xBX + BS3REGCTX.rax]
        mov     edx, [xBX + BS3REGCTX.rdx]
        mov     ecx, [xBX + BS3REGCTX.rcx]
        mov     esi, [xBX + BS3REGCTX.rsi]
        mov     edi, [xBX + BS3REGCTX.rdi]
        mov     ebp, [xBX + BS3REGCTX.rbp] ; restore late for better stacks.
        mov     ebx, [xBX + BS3REGCTX.rbx]

        iretd
%endif
BS3_PROC_END_CMN   Bs3RegCtxRestore

