;
; VBox host drivers - Ring-0 support drivers - Shared code
;
; Import trunks for 64-bit ELF
;
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
;
;

%include "iprt/asmdefs.mac"

BEGINCODE

%macro GEN_THUNK 1
extern SUPR0$ %+ %1
BEGINPROC %1
    mov     r11, qword SUPR0$ %+ %1
    jmp     r11
ENDPROC %1
%endmacro

GEN_THUNK SUPR0ObjRegister
GEN_THUNK SUPR0ObjAddRef
GEN_THUNK SUPR0ObjRelease
GEN_THUNK SUPR0ObjVerifyAccess
GEN_THUNK SUPR0LockMem
GEN_THUNK SUPR0UnlockMem
GEN_THUNK SUPR0ContAlloc
GEN_THUNK SUPR0ContFree
GEN_THUNK SUPR0MemAlloc
GEN_THUNK SUPR0MemGetPhys
GEN_THUNK SUPR0MemFree
GEN_THUNK SUPR0Printf
GEN_THUNK RTMemAlloc
GEN_THUNK RTMemAllocZ
GEN_THUNK RTMemFree
;GEN_THUNK RTSemMutexCreate
;GEN_THUNK RTSemMutexRequest
;GEN_THUNK RTSemMutexRelease
;GEN_THUNK RTSemMutexDestroy
GEN_THUNK RTSemFastMutexCreate
GEN_THUNK RTSemFastMutexDestroy
GEN_THUNK RTSemFastMutexRequest
GEN_THUNK RTSemFastMutexRelease
GEN_THUNK RTSemEventCreate
GEN_THUNK RTSemEventSignal
GEN_THUNK RTSemEventWait
GEN_THUNK RTSemEventDestroy
GEN_THUNK RTSpinlockCreate
GEN_THUNK RTSpinlockDestroy
GEN_THUNK RTSpinlockAcquire
GEN_THUNK RTSpinlockRelease
GEN_THUNK RTSpinlockAcquireNoInts
GEN_THUNK RTSpinlockReleaseNoInts
GEN_THUNK RTThreadSelf
GEN_THUNK RTThreadSleep
GEN_THUNK RTThreadYield
GEN_THUNK RTLogDefaultInstance
GEN_THUNK RTLogRelDefaultInstance
GEN_THUNK RTLogLoggerEx
GEN_THUNK RTLogLoggerExV
GEN_THUNK AssertMsg1
GEN_THUNK AssertMsg2

