; $Id$
;; @file
; VMM - Temporary hack for darwin stack switching.
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

;*********************************************************************************************************************************
;*  Header Files                                                                                                                 *
;*********************************************************************************************************************************
;%define RT_ASM_WITH_SEH64_ALT
%include "VBox/asmdefs.mac"
%include "VBox/SUPR0StackWrapper.mac"


SUPR0StackWrapperGeneric vmmR0LoggerFlushInner, 5
SUPR0StackWrapperGeneric pdmR0CritSectEnterContendedOnKrnlStk, 6
SUPR0StackWrapperGeneric pdmR0CritSectLeaveSignallingOnKrnlStk, 5
SUPR0StackWrapperGeneric pdmCritSectRwEnterShared, 6
SUPR0StackWrapperGeneric pdmCritSectRwLeaveSharedWorker, 3
SUPR0StackWrapperGeneric pdmCritSectRwEnterExcl, 6
SUPR0StackWrapperGeneric pdmCritSectRwLeaveExclWorker, 6
SUPR0StackWrapperGeneric pgmR0PoolGrowOnKrnlStk, 3

