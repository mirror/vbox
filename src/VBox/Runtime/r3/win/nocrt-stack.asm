; $Id$
;; @file
; IPRT - Stack related Visual C++ support routines.
;

;
; Copyright (C) 2022 Oracle Corporation
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


%include "iprt/asmdefs.mac"


;*********************************************************************************************************************************
;*  Global Variables                                                                                                             *
;*********************************************************************************************************************************
BEGINDATA
GLOBALNAME __security_cookie
        dd  0xdeadbeef
        dd  0x0c00ffe


BEGINCODE

BEGINPROC __GSHandlerCheck
        int3
ENDPROC   __GSHandlerCheck


BEGINPROC __chkstk
        int3
ENDPROC   __chkstk


BEGINPROC _RTC_AllocaHelper
        int3
ENDPROC   _RTC_AllocaHelper


BEGINPROC _RTC_InitBase
        int3
ENDPROC   _RTC_InitBase


BEGINPROC _RTC_CheckStackVars
        int3
ENDPROC   _RTC_CheckStackVars


BEGINPROC _RTC_CheckStackVars2
        int3
ENDPROC   _RTC_CheckStackVars2


BEGINPROC _RTC_Shutdown
        int3
ENDPROC   _RTC_Shutdown


BEGINPROC __security_check_cookie
        int3
ENDPROC   __security_check_cookie



; Not stack related stubs.
BEGINPROC __C_specific_handler
        int3
ENDPROC   __C_specific_handler


BEGINPROC __report_rangecheckfailure
        int3
ENDPROC   __report_rangecheckfailure


BEGINPROC __guard_dispatch_icall_fptr
        int3
ENDPROC   __guard_dispatch_icall_fptr

