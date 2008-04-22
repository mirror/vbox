; $Id$
;; @file
; IPRT - Time using SUPLib, the Assembly Implementation.
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
; The contents of this file may alternatively be used under the terms
; of the Common Development and Distribution License Version 1.0
; (CDDL) only, as it comes in the "COPYING.CDDL" file of the
; VirtualBox OSE distribution, in which case the provisions of the
; CDDL are applicable instead of those of the GPL.
;
; You may elect to license modified versions of this file under the
; terms and conditions of either the GPL or the CDDL or both.
;
; Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
; Clara, CA 95054 USA or visit http://www.sun.com if you need
; additional information or have any questions.
;

%ifndef IN_GUEST

%include "iprt/asmdefs.mac"
%include "VBox/sup.mac"


;; Keep this in sync with iprt/time.h.
struc RTTIMENANOTSDATA
    .pu64Prev           RTCCPTR_RES 1
    .pfnBad             RTCCPTR_RES 1
    .pfnRediscover      RTCCPTR_RES 1
    .pvDummy            RTCCPTR_RES 1
    .c1nsSteps          resd 1
    .cExpired           resd 1
    .cBadPrev           resd 1
    .cUpdateRaces       resd 1
endstruc


BEGINDATA
%undef IN_SUPLIB
%undef IMPORTED_SUPLIB
%ifdef IN_SUP_R0
 %define IN_SUPLIB
%endif
%ifdef IN_SUP_R3
 %define IN_SUPLIB
%endif
%ifdef IN_SUP_GC
 %define IN_SUPLIB
%endif
%ifdef IN_SUPLIB
 extern NAME(g_pSUPGlobalInfoPage)
%elifdef IN_RING0
 %ifdef RT_OS_WINDOWS
  %define IMPORTED_SUPLIB
  extern IMPNAME(g_SUPGlobalInfoPage)
 %else
  extern NAME(g_SUPGlobalInfoPage)
 %endif
%else
 %ifdef RT_OS_WINDOWS
  %define IMPORTED_SUPLIB
  extern IMPNAME(g_pSUPGlobalInfoPage)
 %else
  extern NAME(g_pSUPGlobalInfoPage)
 %endif
%endif


BEGINCODE

;
; The default stuff that works everywhere.
; Uses cpuid for serializing.
;
%undef  ASYNC_GIP
%undef  USE_LFENCE
%define NEED_TRANSACTION_ID
%define NEED_TO_SAVE_REGS
%define rtTimeNanoTSInternalAsm    RTTimeNanoTSLegacySync
%include "timesupA.mac"

%define ASYNC_GIP
%ifdef IN_GC
 %undef NEED_TRANSACTION_ID
%endif
%define rtTimeNanoTSInternalAsm    RTTimeNanoTSLegacyAsync
%include "timesupA.mac"

;
; Alternative implementation that employs lfence instead of cpuid.
;
%undef  ASYNC_GIP
%define USE_LFENCE
%define NEED_TRANSACTION_ID
%undef  NEED_TO_SAVE_REGS
%define rtTimeNanoTSInternalAsm    RTTimeNanoTSLFenceSync
%include "timesupA.mac"

%define ASYNC_GIP
%ifdef IN_GC
 %undef NEED_TRANSACTION_ID
%endif
%define rtTimeNanoTSInternalAsm    RTTimeNanoTSLFenceAsync
%include "timesupA.mac"


%endif ; !IN_GUEST
