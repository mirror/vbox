; $Id$
;; @file
; VirtualBox Support Driver - Windows NT specific assembly parts.
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

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "iprt/ntwrap.mac"

BEGINCODE
%ifdef RT_ARCH_AMD64
%define _DbgPrint DbgPrint
%endif
extern _DbgPrint

%if 1 ; see alternative in SUPDrv-win.cpp
;;
; Kind of alias for DbgPrint
BEGINPROC SUPR0Printf
        jmp     _DbgPrint
ENDPROC SUPR0Printf
%endif


%ifdef RT_WITH_W64_UNWIND_HACK
 %ifdef RT_ARCH_AMD64

;
; This has the same order as the list in SUPDrv.c
;
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0ComponentRegisterFactory
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0ComponentDeregisterFactory
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0ComponentQueryFactory
NtWrapDyn2DrvFunctionWith5Params       supdrvNtWrap, SUPR0ObjRegister
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0ObjAddRef
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0ObjAddRefEx
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0ObjRelease
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0ObjVerifyAccess
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0LockMem
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0UnlockMem
NtWrapDyn2DrvFunctionWith5Params       supdrvNtWrap, SUPR0ContAlloc
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0ContFree
NtWrapDyn2DrvFunctionWith5Params       supdrvNtWrap, SUPR0LowAlloc
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0LowFree
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0MemAlloc
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0MemGetPhys
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0MemFree
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0PageAlloc
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0PageFree
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0Printf            - cannot wrap this buster.
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, SUPR0GetPagingMode
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMemAlloc
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMemAllocZ
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMemFree
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMemDup
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMemDupEx
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMemRealloc
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTR0MemObjAllocLow
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTR0MemObjAllocPage
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTR0MemObjAllocPhys
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTR0MemObjAllocPhysNC
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTR0MemObjAllocCont
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTR0MemObjEnterPhys
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTR0MemObjLockUser
NtWrapDyn2DrvFunctionWith5Params       supdrvNtWrap, RTR0MemObjMapKernel
NtWrapDyn2DrvFunctionWith7Params       supdrvNtWrap, RTR0MemObjMapKernelEx
NtWrapDyn2DrvFunctionWith6Params       supdrvNtWrap, RTR0MemObjMapUser
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTR0MemObjAddress      - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTR0MemObjAddressR3    - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTR0MemObjSize         - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTR0MemObjIsMapping    - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTR0MemObjGetPagePhysAddr - not necessary
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTR0MemObjFree
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTProcSelf             - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTR0ProcHandleSelf     - not necessary
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSemFastMutexCreate
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSemFastMutexDestroy
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSemFastMutexRequest
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSemFastMutexRelease
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSemEventCreate
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSemEventSignal
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSemEventWait
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSemEventWaitNoResume
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSemEventDestroy
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSemEventMultiCreate
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSemEventMultiSignal
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSemEventMultiReset
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSemEventMultiWait
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSemEventMultiWaitNoResume
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSemEventMultiDestroy
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSpinlockCreate
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSpinlockDestroy
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSpinlockAcquire
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSpinlockRelease
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSpinlockAcquireNoInts
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTSpinlockReleaseNoInts
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTTimeNanoTS           - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTTimeMilliTS          - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTTimeSystemNanoTS     - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTTimeSystemMilliTS    - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTThreadNativeSelf     - not necessary
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTThreadSleep
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTThreadYield
%if 0 ; Thread APIs, Part 2
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTThreadSelf
NtWrapDyn2DrvFunctionWith7Params       supdrvNtWrap, RTThreadCreate
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTThreadGetNative
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTThreadWait
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTThreadWaitNoResume
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTThreadGetName
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTThreadSelfName
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTThreadGetType
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTThreadUserSignal
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTThreadUserReset
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTThreadUserWait
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTThreadUserWaitNoResume
%endif
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTLogDefaultInstance   - a bit of a gamble, but we do not want the overhead!
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMpCpuId              - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMpCpuIdFromSetIndex  - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMpCpuIdToSetIndex    - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMpIsCpuPossible      - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMpGetCount           - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMpGetMaxCpuId        - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMpGetOnlineCount     - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMpGetOnlineSet       - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMpGetSet             - not necessary
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMpIsCpuOnline        - not necessary
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMpIsCpuWorkPending
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMpOnAll
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMpOnOthers
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMpOnSpecific
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTMpPokeCpu
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTLogRelDefaultInstance - not necessary.
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTLogSetDefaultInstanceThread
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTLogLogger            - can't wrap this buster.
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTLogLoggerEx          - can't wrap this buster.
NtWrapDyn2DrvFunctionWith5Params       supdrvNtWrap, RTLogLoggerExV
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTLogPrintf            - can't wrap this buster. ;; @todo provide va_list log wrappers in RuntimeR0.
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, RTLogPrintfV
NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, AssertMsg1
;NtWrapDyn2DrvFunctionWithAllRegParams  supdrvNtWrap, AssertMsg2             - can't wrap this buster.
NtWrapDrv2DynFunctionWithAllRegParams   supdrvNtWrap, RTPowerSignalEvent


;;
; @cproto DECLASM(int) supdrvNtWrapVMMR0EntryEx(PFNRT pfnVMMR0EntryEx, PVM pVM, unsigned idCpu, unsigned uOperation, PSUPVMMR0REQHDR pReq, uint64_t u64Arg, PSUPDRVSESSION pSession);
;
; @param    pfnVMMR0EntryEx     rcx
; @param    pVM                 rdx
; @param    idCpu               r8
; @param    uOperation          r9
; @param    pReq                [rsp + 28h] / [rbp + 30h]
; @param    u64Arg              [rsp + 30h] / [rbp + 38h]
; @param    pSession            [rsp + 38h] / [rbp + 40h]
;
BEGINPROC supdrvNtWrapVMMR0EntryEx
        NtWrapProlog supdrvNtWrapVMMR0EntryEx
        NtWrapCreateMarker

        mov     rax, rcx
        mov     rcx, rdx
        mov     rdx, r8
        mov     r8, r9
        mov     r9, [rbp + 30h]
        mov     r11, [rbp + 38h]
        mov     [rsp + 20h], r11
        mov     r11, [rbp + 40h]
        mov     [rsp + 28h], r11
        call    rax

        NtWrapDestroyMarker
        NtWrapEpilog supdrvNtWrapVMMR0EntryEx
ENDPROC   supdrvNtWrapVMMR0EntryEx


;;
; @cproto DECLASM(int)    supdrvNtWrapVMMR0EntryFast(PFNRT pfnVMMR0EntryFast, PVM pVM, unsigned idCPU, unsigned uOperation);
;
; @param    pfnVMMR0EntryFast   rcx
; @param    pVM                 rdx
; @param    idCPU               r8
; @param    uOperation          r9
;
BEGINPROC supdrvNtWrapVMMR0EntryFast
        NtWrapProlog supdrvNtWrapVMMR0EntryFast
        NtWrapCreateMarker

        mov     rax, rcx
        mov     rcx, rdx
        mov     rdx, r8
        mov     r8, r9
        call    rax

        NtWrapDestroyMarker
        NtWrapEpilog supdrvNtWrapVMMR0EntryFast
ENDPROC   supdrvNtWrapVMMR0EntryFast


;;
; @cproto DECLASM(void)   supdrvNtWrapObjDestructor(PFNRT pfnDestruction, void *pvObj, void *pvUser1, void *pvUser2);
;
; @param    pfnDestruction      rcx
; @param    pvObj               rdx
; @param    pvUser1             r8
; @param    pvUser2             r9
;
BEGINPROC supdrvNtWrapObjDestructor
        NtWrapProlog supdrvNtWrapObjDestructor
        NtWrapCreateMarker

        mov     rax, rcx
        mov     rcx, rdx
        mov     rdx, r8
        mov     r8, r9
        call    rax

        NtWrapDestroyMarker
        NtWrapEpilog supdrvNtWrapObjDestructor
ENDPROC   supdrvNtWrapObjDestructor


;;
; @cproto DECLASM(void *) supdrvNtWrapQueryFactoryInterface(PFNRT pfnQueryFactoryInterface, struct SUPDRVFACTORY const *pSupDrvFactory,
;                                                           PSUPDRVSESSION pSession, const char *pszInterfaceUuid);
;
; @param    pfnQueryFactoryInterface    rcx
; @param    pSupDrvFactory      rdx
; @param    pSession            r8
; @param    pszInterfaceUuid    r9
;
BEGINPROC supdrvNtWrapQueryFactoryInterface
        NtWrapProlog supdrvNtWrapQueryFactoryInterface
        NtWrapCreateMarker

        mov     rax, rcx
        mov     rcx, rdx
        mov     rdx, r8
        mov     r8, r9
        call    rax

        NtWrapDestroyMarker
        NtWrapEpilog supdrvNtWrapQueryFactoryInterface
ENDPROC   supdrvNtWrapQueryFactoryInterface


;;
; @cproto DECLASM(int)    supdrvNtWrapModuleInit(PFNRT pfnModuleInit);
;
; @param    pfnModuleInit       rcx
;
BEGINPROC supdrvNtWrapModuleInit
        NtWrapProlog supdrvNtWrapModuleInit
        NtWrapCreateMarker

        call    rcx

        NtWrapDestroyMarker
        NtWrapEpilog supdrvNtWrapModuleInit
ENDPROC   supdrvNtWrapModuleInit


;;
; @cproto DECLASM(void)   supdrvNtWrapModuleTerm(PFNRT pfnModuleTerm);
;
; @param    pfnModuleInit       rcx
;
BEGINPROC supdrvNtWrapModuleTerm
        NtWrapProlog supdrvNtWrapModuleTerm
        NtWrapCreateMarker

        call    rcx

        NtWrapDestroyMarker
        NtWrapEpilog supdrvNtWrapModuleTerm
ENDPROC   supdrvNtWrapModuleTerm


;;
; @cproto DECLASM(int) supdrvNtWrapServiceReqHandler(PFNRT pfnServiceReqHandler, PSUPDRVSESSION pSession, uint32_t uOperation, uint64_t u64Arg, PSUPR0SERVICEREQHDR pReqHdr);
;
; @param    pfnSerivceReqHandler rcx
; @param    pSession            rdx
; @param    uOperation          r8
; @param    u64Arg              r9
; @param    pReq                [rsp + 28h] / [rbp + 30h]
;
BEGINPROC supdrvNtWrapServiceReqHandler
        NtWrapProlog supdrvNtWrapServiceReqHandler
        NtWrapCreateMarker

        mov     rax, rcx
        mov     rcx, rdx
        mov     rdx, r8
        mov     r8, r9
        mov     r9, [rbp + 30h]
        call    rax

        NtWrapDestroyMarker
        NtWrapEpilog supdrvNtWrapServiceReqHandler
ENDPROC   supdrvNtWrapServiceReqHandler


 %endif ; RT_ARCH_AMD64
%endif ; RT_WITH_W64_UNWIND_HACK

