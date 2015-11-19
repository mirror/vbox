; $Id$
;; @file
; BS3Kit - First Object, calling real-mode main().
;

;
; Copyright (C) 2007-2015 Oracle Corporation
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



%include "bs3kit.mac"


;
; Define all the segments and their grouping, just to get that right once at
; the start of everything.
;

; 16-bit text
BS3_BEGIN_TEXT16
BS3_GLOBAL_DATA Bs3Text16_StartOfSegment, 0

%ifdef ASM_FORMAT_ELF
section BS3TEXT16_END   align=2 progbits alloc exec nowrite
%else
section BS3TEXT16_END   align=2 CLASS=BS3CODE16 PUBLIC USE16
%endif

BS3_GLOBAL_DATA Bs3Text16_Size, 2
    dw  BS3_DATA_NM(Bs3Text16_EndOfSegment) wrt BS3TEXT16
BS3_GLOBAL_DATA Bs3Text16_EndOfSegment, 0

%ifndef ASM_FORMAT_ELF
GROUP CGROUP16 BS3TEXT16 BS3TEXT16_END
%endif

; 16-bit data
BS3_BEGIN_DATA16
BS3_GLOBAL_DATA Bs3Data16_StartOfSegment, 0
    db      10,13,'eye-catcher: BS3DATA16',10,13
%ifdef ASM_FORMAT_ELF
section BS3DATA16CONST  align=2   progbits alloc noexec write
section BS3DATA16CONST2 align=2   progbits alloc noexec write
section BS3DATA16_DATA  align=2   progbits alloc noexec write
section BS3DATA16_END   align=2   progbits alloc noexec write
%else
section BS3DATA16CONST  align=2   CLASS=FAR_DATA PUBLIC USE16
section BS3DATA16CONST2 align=2   CLASS=FAR_DATA PUBLIC USE16
section BS3DATA16_DATA  align=2   CLASS=FAR_DATA PUBLIC USE16
section BS3DATA16_END   align=2   CLASS=FAR_DATA PUBLIC USE16
%endif

BS3_GLOBAL_DATA Bs3Data16_EndOfSegment, 0

%ifndef ASM_FORMAT_ELF
GROUP BS3DATA16_GROUP BS3DATA16 BS3DATA16CONST BS3DATA16CONST2 BS3DATA16_END
%endif

; 32-bit text
BS3_BEGIN_TEXT32
BS3_GLOBAL_DATA Bs3Text32_StartOfSegment, 0
    db      10,13,'eye-catcher: BS3TEXT32',10,13

%ifdef ASM_FORMAT_ELF
section BS3TEXT32_END   align=1 progbits alloc exec nowrite
%else
section BS3TEXT32_END   align=1 CLASS=BS3CODE32 PUBLIC USE32 FLAT
%endif
BS3_GLOBAL_DATA Bs3Data16_Size, 4
    dd  BS3_DATA_NM(Bs3Data16_EndOfSegment) wrt BS3DATA16
BS3_GLOBAL_DATA Bs3Text32_EndOfSegment, 0

; 64-bit text
BS3_BEGIN_TEXT64
BS3_GLOBAL_DATA Bs3Text64_StartOfSegment, 0
    db      10,13,'eye-catcher: BS3TEXT64',10,13

%ifdef ASM_FORMAT_ELF
section BS3TEXT64_END   align=1 progbits alloc exec nowrite
%else
section BS3TEXT64_END   align=1 CLASS=CODE PUBLIC USE32 FLAT
%endif
BS3_GLOBAL_DATA Bs3Text64_EndOfSegment, 0

; 32-bit data
BS3_BEGIN_DATA32
BS3_GLOBAL_DATA Bs3Data32_StartOfSegment, 0
    db      10,13,'eye-catcher: BS3DATA32',10,13
%ifdef ASM_FORMAT_ELF
section BS3DATA32_END   align=16   progbits alloc noexec write
%else
section BS3DATA32_END   align=16   CLASS=FAR_DATA PUBLIC USE32 FLAT
%endif
BS3_GLOBAL_DATA Bs3Data32_EndOfSegment, 0

; 64-bit data
BS3_BEGIN_DATA64
BS3_GLOBAL_DATA Bs3Data64_StartOfSegment, 0
    db      10,13,'eye-catcher: BS3DATA64',10,13
%ifdef ASM_FORMAT_ELF
section BS3DATA64_END   align=16   progbits alloc noexec write
%else
section BS3DATA64_END   align=16   CLASS=DATA PUBLIC USE32 FLAT
%endif

ALIGNDATA(16)
    db      10,13,'eye-catcher: sizes  ',10,13
BS3_GLOBAL_DATA Bs3Data16Thru64Text32And64_TotalSize, 4
    dd  BS3_DATA_NM(Bs3Data64_EndOfSegment) wrt BS3DATA16
BS3_GLOBAL_DATA Bs3TotalImageSize, 4
    dd  BS3_DATA_NM(Bs3Data64_EndOfSegment) wrt BS3TEXT16
BS3_GLOBAL_DATA Bs3Data64_EndOfSegment, 0

BS3_BEGIN_SYSTEM16



;*********************************************************************************************************************************
;*  External Symbols                                                                                                             *
;*********************************************************************************************************************************
BS3_BEGIN_TEXT16
extern BS3_CMN_NM(Bs3Shutdown)
extern NAME(Main_rm)

BS3_BEGIN_SYSTEM16
extern BS3_DATA_NM(Bs3Gdt)
extern BS3_DATA_NM(Bs3Lgdt_Gdt)

;
; Nothing to init here, just call main and shutdown if it returns.
;
BS3_BEGIN_TEXT16
GLOBALNAME start
    jmp     .after_eye_catcher
    db      10,13,'eye-catcher: BS3TEXT16',10,13
.after_eye_catcher:
    mov     ax, BS3DATA16
    mov     es, ax
    mov     ds, ax
    call    NAME(Main_rm)
    call    BS3_CMN_NM(Bs3Shutdown)

