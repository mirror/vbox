; $Id$
;; @file
; VMM - tstVMMR0CallHost-2A.asm - Switch-back wrapper.
;

;
; Copyright (C) 2006-2021 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;

%include "VBox/SUPR0StackWrapper.mac"

SUPR0StackWrapperGeneric tstWrappedThin, 1
SUPR0StackWrapperGeneric tstWrapped4,    4
SUPR0StackWrapperGeneric tstWrapped5,    5
SUPR0StackWrapperGeneric tstWrapped6,    6
SUPR0StackWrapperGeneric tstWrapped7,    7
SUPR0StackWrapperGeneric tstWrapped8,    8
SUPR0StackWrapperGeneric tstWrapped9,    9
SUPR0StackWrapperGeneric tstWrapped10,   10
SUPR0StackWrapperGeneric tstWrapped16,   16
SUPR0StackWrapperGeneric tstWrapped20,   20

