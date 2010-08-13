;
; Export definitions
;
; Copyright (C) 2006-2007 Oracle Corporation
;
; Oracle Corporation confidential
; All rights reserved
;

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "VBox/asmdefs.mac"

extern __imp__RxAcquireExclusiveFcbResourceInMRx
extern __imp__RxCreateNetFobx
extern __imp__RxCreateRxContext
extern __imp__RxDereferenceAndDeleteRxContext_Real
extern __imp__RxDispatchToWorkerThread
extern __imp__RxFinalizeConnection
extern __imp__RxFinishFcbInitialization
extern __imp__RxFsdDispatch
extern __imp__RxGetFileSizeWithLock
extern __imp__RxGetFileSizeWithLock
extern __imp__RxLowIoGetBufferAddress
extern __imp__RxRegisterMinirdr
extern __imp__RxStartMinirdr
extern __imp__RxStopMinirdr
extern __imp__RxpUnregisterMinirdr
extern __imp__RxGetRDBSSProcess


BEGINPROC   RxAcquireExclusiveFcbResourceInMRx@4
    jmp     dword [__imp__RxAcquireExclusiveFcbResourceInMRx]
ENDPROC     RxAcquireExclusiveFcbResourceInMRx@4

BEGINPROC   RxCreateNetFobx@8
    jmp     dword [__imp__RxCreateNetFobx]
ENDPROC     RxCreateNetFobx@8

BEGINPROC   RxCreateRxContext@12
    jmp     dword [__imp__RxCreateRxContext]
ENDPROC     RxCreateRxContext@12

BEGINPROC   RxDereferenceAndDeleteRxContext_Real@4
    jmp     dword [__imp__RxDereferenceAndDeleteRxContext_Real]
ENDPROC     RxDereferenceAndDeleteRxContext_Real@4

BEGINPROC   RxDispatchToWorkerThread@16
    jmp     dword [__imp__RxDispatchToWorkerThread]
ENDPROC     RxDispatchToWorkerThread@16

BEGINPROC   RxFinalizeConnection@12
    jmp     dword [__imp__RxFinalizeConnection]
ENDPROC     RxFinalizeConnection@12

BEGINPROC   RxFinishFcbInitialization@12
    jmp     dword [__imp__RxFinishFcbInitialization]
ENDPROC     RxFinishFcbInitialization@12

BEGINPROC   RxFsdDispatch@8
    jmp     dword [__imp__RxFsdDispatch]
ENDPROC     RxFsdDispatch@8

BEGINPROC   RxGetFileSizeWithLock@8
    jmp     dword [__imp__RxGetFileSizeWithLock]
ENDPROC     RxGetFileSizeWithLock@8

BEGINPROC   RxGetRDBSSProcess@0
    jmp     dword [__imp__RxGetRDBSSProcess]
ENDPROC     RxGetRDBSSProcess@0

BEGINPROC   RxLowIoGetBufferAddress@4
    jmp     dword [__imp__RxLowIoGetBufferAddress]
ENDPROC     RxLowIoGetBufferAddress@4

BEGINPROC   RxRegisterMinirdr@32
    jmp     dword [__imp__RxRegisterMinirdr]
ENDPROC     RxRegisterMinirdr@32

BEGINPROC   RxStartMinirdr@8
    jmp     dword [__imp__RxStartMinirdr]
ENDPROC     RxStartMinirdr@8

BEGINPROC   RxStopMinirdr@8
    jmp     dword [__imp__RxStopMinirdr]
ENDPROC     RxStopMinirdr@8

BEGINPROC   RxpUnregisterMinirdr@4
    jmp     dword [__imp__RxpUnregisterMinirdr]
ENDPROC     RxpUnregisterMinirdr@4

