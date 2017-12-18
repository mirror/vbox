; $Id$
;; @file
; IPRT - Companion to nt3fakes-r0drv-nt.cpp that provides import stuff to satisfy the linker.
;

;
; Copyright (C) 2006-2017 Oracle Corporation
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

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "iprt/asmdefs.mac"

BEGINCODE

extern      _PsGetVersion@16
GLOBALNAME _imp__PsGetVersion@16
    dd      _PsGetVersion@16

extern      _ZwQuerySystemInformation@16
GLOBALNAME _imp__ZwQuerySystemInformation@16
    dd      _ZwQuerySystemInformation@16

extern      _KeInitializeTimerEx@8
GLOBALNAME _imp__KeInitializeTimerEx@8
    dd      _KeInitializeTimerEx@8

extern      _KeSetTimerEx@20
GLOBALNAME _imp__KeSetTimerEx@20
    dd      _KeSetTimerEx@20

extern      _IoAttachDeviceToDeviceStack@8
GLOBALNAME _imp__IoAttachDeviceToDeviceStack@8
    dd      _IoAttachDeviceToDeviceStack@8

extern      _PsGetCurrentProcessId@0
GLOBALNAME _imp__PsGetCurrentProcessId@0
    dd      _PsGetCurrentProcessId@0

extern      _ZwYieldExecution@0
GLOBALNAME _imp__ZwYieldExecution@0
    dd      _ZwYieldExecution@0

