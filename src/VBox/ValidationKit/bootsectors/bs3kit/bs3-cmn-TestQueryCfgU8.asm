; $Id$
;; @file
; BS3Kit - Bs3TestQueryCfgU8, Bs3TestQueryCfgBool.
;

;
; Copyright (C) 2007-2022 Oracle Corporation
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
%include "VBox/VMMDevTesting.mac"

BS3_EXTERN_DATA16 g_fbBs3VMMDevTesting
TMPL_BEGIN_TEXT

;;
; @cproto   BS3_DECL(uint8_t) Bs3TestQueryCfgU8(uint16_t uCfg);
; @cproto   BS3_DECL(bool)    Bs3TestQueryCfgBool(uint16_t uCfg);
;
BS3_GLOBAL_NAME_EX BS3_CMN_NM(Bs3TestQueryCfgBool), function, 3
BS3_PROC_BEGIN_CMN Bs3TestQueryCfgU8, BS3_PBC_HYBRID
        BS3_CALL_CONV_PROLOG 1
        push    xBP
        mov     xBP, xSP
        push    xDX

        xor     al, al
        cmp     byte [BS3_DATA16_WRT(g_fbBs3VMMDevTesting)], 0
        je      .no_vmmdev

        ; Issue the query command.
        mov     dx, VMMDEV_TESTING_IOPORT_CMD
%if TMPL_BITS == 16
        mov     ax, VMMDEV_TESTING_CMD_QUERY_CFG - VMMDEV_TESTING_CMD_MAGIC_HI_WORD
        out     dx, ax
%else
        mov     eax, VMMDEV_TESTING_CMD_QUERY_CFG
        out     dx, eax
%endif

        ; Write what we wish to query.
        mov     ax, [xBP + xCB + cbCurRetAddr]
        mov     dx, VMMDEV_TESTING_IOPORT_DATA
        out     dx, ax

        ; Read back the result.
        in      al, dx

.no_vmmdev:
        pop     xDX
        pop     xBP
        BS3_CALL_CONV_EPILOG 1
        BS3_HYBRID_RET
BS3_PROC_END_CMN   Bs3TestQueryCfgU8

%if TMPL_BITS == 16
BS3_GLOBAL_NAME_EX BS3_CMN_NM_FAR(Bs3TestQueryCfgBool), function, 3
        jmp     BS3_CMN_NM_FAR(Bs3TestQueryCfgU8)
%endif
