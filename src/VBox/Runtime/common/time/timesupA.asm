; $Id$%ifndef IN_GUEST
;; @file
; innotek Portable Runtime - Time using SUPLib, the Assembly Implementation.
;

;
; Copyright (C) 2006-2007 InnoTek Systemberatung GmbH
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License as published by the Free Software Foundation,
; in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
; distribution. VirtualBox OSE is distributed in the hope that it will
; be useful, but WITHOUT ANY WARRANTY of any kind.
;
;

%ifndef IN_GUEST

%include "iprt/asmdefs.mac"
%include "VBox/sup.mac"


;; Keep this in sync with iprt/time.h.
struc RTTIMENANOTSDATA
    .pu64Prev           RTCCPTR_RES 1
    .pfnBad             RTCCPTR_RES 1
    .pfnRediscover      RTCCPTR_RES 1
    .c1nsSteps          resd 1
    .cExpired           resd 1
    .cBadPrev           resd 1
    .cUpdateRaces       resd 1
endstruc


BEGINDATA
extern NAME(g_pSUPGlobalInfoPage)

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
