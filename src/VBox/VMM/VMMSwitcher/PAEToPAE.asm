;; @file
;
; VMM - World Switchers, PAE to PAE

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
;*   Defined Constants And Macros                                              *
;*******************************************************************************
%define SWITCHER_TYPE               VMMSWITCHER_PAE_TO_PAE
%define SWITCHER_DESCRIPTION        "PAE to/from PAE"
%define NAME_OVERLOAD(name)         vmmR3SwitcherPAEToPAE_ %+ name
%define SWITCHER_FIX_INTER_CR3_HC   FIX_INTER_PAE_CR3
%define SWITCHER_FIX_INTER_CR3_GC   FIX_INTER_PAE_CR3
%define SWITCHER_FIX_HYPER_CR3      FIX_HYPER_PAE_CR3

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "VBox/nasm.mac"
%include "VMMSwitcher/PAEand32Bit.mac"

