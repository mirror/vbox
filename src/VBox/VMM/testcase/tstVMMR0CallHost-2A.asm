; $Id$
;; @file
; VMM - tstVMMR0CallHost-2A.asm - Switch-back wrapper.
;

;
; Copyright (C) 2006-2020 Oracle Corporation
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

SUPR0StackWrapperGeneric tstWrapped1
SUPR0StackWrapperGeneric tstWrappedThin

