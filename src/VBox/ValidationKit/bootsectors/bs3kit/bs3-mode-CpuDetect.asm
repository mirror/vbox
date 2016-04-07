; $Id$
;; @file
; BS3Kit - Bs3CpuDetect
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

BS3_EXTERN_DATA16 g_uBs3CpuDetected
TMPL_BEGIN_TEXT

;;
; Rough CPU detection, mainly for detecting really old CPUs.
;
; A Bs3CpuDetectEx can be added if this is insufficient.
;
; @returns  BS3CPU_xxx in xAX.
; @cproto   BS3_DECL(BS3CPU) Bs3CpuDetect(void);
;
; @uses     xAX.
;
; @remarks  ASSUMES we're in ring-0 when not in some kind of real mode.
;
BS3_PROC_BEGIN_MODE Bs3CpuDetect
CPU 8086
        push    xBP
        mov     xBP, xSP
        pushf
        push    xCX
        push    xDX
        push    xBX

%ifndef TMPL_CMN_PAGING
 %ifdef TMPL_RM
  %if 1                 ; this is simpler
        ;
        ; FLAGS bits 15:12 are always set on 8086, 8088, V20, V30, 80186, and
        ; 80188. FLAGS bit 15 is always zero on 286+, whereas bit 14 is NT and
        ; bits 13:12 are IOPL.
        ;
        test    byte [xBP - xCB + 1], 80h ; Top byte of saved flags.
        jz      .286plus
  %else
        ;
        ; When executing 'PUSH SP' the 8086, 8088, V20, V30, 80186, and 80188
        ; should be pushing the updated SP value instead of the initial one.
        ;
        push    xSP
        pop     xAX
        cmp     xAX, xSP
        je      .286plus
  %endif

        ;
        ; Older than 286.
        ;
        ; Detect 8086/8088/V20/V30 vs. 80186/80188 by checking for pre 80186
        ; shift behavior.  the 80186/188 and later will mask the CL value according
        ; to the width of the destination register, whereas 8086/88 and V20/30 will
        ; perform the exact number of shifts specified.
        ;
        mov     cl, 20h                 ; Shift count; 80186/88 and later will mask this by 0x1f (or 0xf)?
        mov     dx, 7fh
        shl     dx, cl
        cmp     dx, 7fh                 ; If no change, this is a 80186/88.
        mov     xAX, BS3CPU_80186
        je      .return

        ;
        ; Detect 8086/88 vs V20/30 by exploiting undocumented POP CS encoding
        ; that was redefined on V20/30 to SET1.
        ;
        xor     ax, ax                  ; clear
        push    cs
        db      0fh                     ; 8086/88: pop cs       V20/30: set1 bl,cl
        db      14h, 3ch                ; 8086/88: add al, 3ch
                                        ; 8086/88: al = 3ch     V20/30: al = 0, cs on stack, bl modified.
        cmp     al, 3ch
        jne     .is_v20_or_v30
        mov     xAX, BS3CPU_8086
        jmp     .return

.is_v20_or_v30:
        pop     xCX                     ; unclaimed CS
        mov     xAX, BS3CPU_V20
        jmp     .return

 %endif ; TMPL_RM

CPU 286
.286plus:
        ;
        ; The 4th bit of the machine status word / CR0 indicates the precense
        ; of a 80387 or later co-processor (a 80287+80386 => ET=0).  486 and
        ; later should be hardcoding this to 1, according to the documentation
        ; (need to test on 486SX).  The initial idea here then would be to
        ; assume 386+ if ET=1.
        ;
        ; However, it turns out the 286 I've got here has bits 4 thru 15 all
        ; set.  This is very nice though, because only bits 4 and 5 are defined
        ; on later CPUs and the remainder MBZ.  So, check whether any of the MBZ
        ; bits are set, if so, then it's 286.
        ;
        smsw    ax
        test    ax, ~(X86_CR0_PE | X86_CR0_MP | X86_CR0_EM | X86_CR0_TS | X86_CR0_ET | X86_CR0_NE)
        jnz     .is_286

        ;
        ; Detect 80286 by checking whether the IOPL and NT bits of EFLAGS can be
        ; modified or not.  There are different accounts of these bits.  Dr.Dobb's
        ; (http://www.drdobbs.com/embedded-systems/processor-detection-schemes/184409011)
        ; say they are undefined on 286es and will always be zero.  Whereas Intel
        ; iAPX 286 Programmer's Reference Manual (both order #210498-001 and
        ; #210498-003) documents both IOPL and NT, but with comment 4 on page
        ; C-43 stating that they cannot be POPFed in real mode and will both
        ; remain 0.  This is different from the 386+, where the NT flag isn't
        ; privileged according to page 3-37 in #230985-003.  Later Intel docs
        ; (#235383-052US, page 4-192) documents real mode as taking both NT and
        ; IOPL from what POPF reads off the stack - which is the behavior
        ; observed a 386SX here.
        ;
        test    al, X86_CR0_PE          ; This flag test doesn't work in protected mode, ...
        jnz     .386plus                ; ... so ASSUME 386plus if in PE for now.

        pushf                           ; Save a copy of the original flags for restoring IF.
        pushf
        pop     ax
        xor     ax, X86_EFL_IOPL | X86_EFL_NT   ; Try modify IOPL and NT.
        and     ax, ~X86_EFL_IF                 ; Try clear IF.
        push    ax                      ; Load modified flags.
        popf
        pushf                           ; Get actual flags.
        pop     dx
        popf                            ; Restore IF, IOPL and NT.
        cmp     ax, dx
        je      .386plus                ; If any of the flags are set, we're on 386+.

        ; While we could in theory be in v8086 mode at this point and be fooled
        ; by a flaky POPF implementation, we assume this isn't the case in our
        ; execution environment.

.is_286:
        mov     ax, BS3CPU_80286
        jmp     .return
%endif ; !TMPL_CMN_PAGING

CPU 386
.386plus:
        ;
        ; Check for CPUID and AC.  The former flag indicates CPUID support, the
        ; latter was introduced with the 486.
        ;
        mov     ebx, esp                ; Save esp.
        and     esp, 0fffch             ; Clear high word and don't trigger ACs.
        pushfd
        mov     eax, [esp]              ; eax = original EFLAGS.
        xor     dword [esp], X86_EFL_ID | X86_EFL_AC ; Flip the ID and AC flags.
        popfd                           ; Load modified flags.
        pushfd                          ; Save actual flags.
        xchg    eax, [esp]              ; Switch, so the stack has the original flags.
        xor     eax, [esp]              ; Calc changed flags.
        popf                            ; Restore EFLAGS.
        mov     esp, ebx                ; Restore possibly unaligned ESP.
        test    eax, X86_EFL_ID
        jnz     .have_cpuid             ; If ID changed, we've got CPUID.
        test    eax, X86_EFL_AC
        mov     xAX, BS3CPU_80486
        jnz     .return                 ; If AC changed, we've got a 486 without CPUID (or similar).
        mov     xAX, BS3CPU_80386
        jmp     .return

CPU 586
.have_cpuid:
        ;
        ; Do a very simple minded check here using the (standard) family field.
        ; While here, we also check for PAE.
        ;
        mov     eax, 1
        cpuid

        ; Calc the extended family and model values before we mess up EAX.
        mov     cl, ah
        and     cl, 0fh
        cmp     cl, 0fh
        jnz     .not_extended_family
        mov     ecx, eax
        shr     ecx, 20
        and     cl, 7fh
        add     cl, 0fh
.not_extended_family:                   ; cl = family
        mov     ch, al
        shr     ch, 4
        cmp     cl, 0fh
        jae     .extended_model
        cmp     cl, 06h                 ; actually only intel, but we'll let this slip for now.
        jne     .done_model
.extended_model:
        shr     eax, 12
        and     al, 0f0h
        or      ch, al
.done_model:                            ; ch = model

        ; Start assembling return flags, checking for PSE + PAE.
        mov     eax, X86_CPUID_FEATURE_EDX_PSE | X86_CPUID_FEATURE_EDX_PAE
        and     eax, edx
        mov     ah, al
        AssertCompile(X86_CPUID_FEATURE_EDX_PAE_BIT > BS3CPU_F_PAE_BIT - 8)  ; 6 vs 10-8=2
        and     al, X86_CPUID_FEATURE_EDX_PAE
        shr     al, X86_CPUID_FEATURE_EDX_PAE_BIT - (BS3CPU_F_PAE_BIT - 8)
        AssertCompile(X86_CPUID_FEATURE_EDX_PSE_BIT == BS3CPU_F_PSE_BIT - 8) ; 3 vs 11-8=3
        and     ah, X86_CPUID_FEATURE_EDX_PSE
        or      ah, al
        or      ah, (BS3CPU_F_CPUID >> 8)

        ; Add the CPU type based on the family and model values.
        cmp     cl, 6
        jne     .not_family_06h
        mov     al, BS3CPU_PPro
        cmp     ch, 1
        jbe     .return
        mov     al, BS3CPU_PProOrNewer
        jmp     .NewerThanPPro

.not_family_06h:
        mov     al, BS3CPU_PProOrNewer
        ja      .NewerThanPPro
        cmp     cl, 5
        mov     al, BS3CPU_Pentium
        je      .return
        cmp     cl, 4
        mov     al, BS3CPU_80486
        je      .return
        cmp     cl, 3
        mov     al, BS3CPU_80386
        je      .return

.NewerThanPPro:

        ; Check for extended leaves and long mode.
        push    xAX                     ; save PAE+PProOrNewer
        mov     eax, 0x80000000
        cpuid
        sub     eax, 0x80000001         ; Minimum leaf 0x80000001
        cmp     eax, 0x00010000         ; At most 0x10000 leaves.
        ja      .no_ext_leaves

        mov     eax, 0x80000001
        cpuid
        pop     xAX                     ; restore PAE+PProOrNewer
        test    edx, X86_CPUID_EXT_FEATURE_EDX_LONG_MODE
        jz      .no_long_mode
        or      ax, BS3CPU_F_CPUID_EXT_LEAVES | BS3CPU_F_LONG_MODE
        jmp     .return
.no_long_mode:
        or      ax, BS3CPU_F_CPUID_EXT_LEAVES
        jmp     .return
.no_ext_leaves:
        pop     xAX                     ; restore PAE+PProOrNewer

CPU 8086
.return:
        ;
        ; Save the return value.
        ;
        mov     [BS3_DATA16_WRT(g_uBs3CpuDetected)], ax

        ;
        ; Epilogue.
        ;
        pop     xBX
        pop     xDX
        pop     xCX
        popf
        pop     xBP
        ret

BS3_PROC_END_MODE   Bs3CpuDetect

