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
%include "iprt/asmdefs.mac"

BEGINCODE
%ifdef RT_ARCH_AMD64
%define _DbgPrint DbgPrint
%endif
extern _DbgPrint

;;
; Kind of alias for DbgPrint
BEGINPROC AssertMsg2
        jmp     _DbgPrint
ENDPROC AssertMsg2

;;
; Kind of alias for DbgPrint
BEGINPROC SUPR0Printf
        jmp     _DbgPrint
ENDPROC SUPR0Printf


%ifdef SUPDRV_WITH_UNWIND_HACK
 %ifdef RT_ARCH_AMD64

;;
; Common prolog, take the proc name as argument.
; This creates a 0x80 byte stack frame.
;
%macro NtWrapProlog 1
[proc_frame %1]
        push    rbp
        [pushreg rbp]
        mov     rbp, rsp
        [setframe rbp, 0]
        sub     rsp, 0x80
        [allocstack 0x80]

        ; save rdi and load rbp into it
        mov     [rbp - 8h], rdi
        [savereg rdi, 0x78]
        mov     rdi, rbp
[endprolog]
%endmacro

;;
; Common epilog, take the proc name as argument.
%macro NtWrapEpilog 1
        ; restore rbp and rdi then return.
        mov     rbp, rdi
        mov     rdi, [rdi - 8h]
        leave
        ret
[endproc_frame %1]
%endmacro

;;
; Create a stack marker with the rbp. The marker is 32 byte big.
; This is 32-byte aligned and 32 byte in size.
;
; Trashes r10
%macro NtWrapCreateMarker 0
        lea     r10, [rbp - 30h]
        and     r10, ~1fh               ; 32-byte align it.
        mov     dword [r10      ], 0x20080901
        mov     dword [r10 + 04h], 0x20080902
        mov     qword [r10 + 08h], rbp
        mov     dword [r10 + 10h], 0x20080903
        mov     dword [r10 + 14h], 0x20080904
        mov     qword [r10 + 18h], rbp
%endmacro

;;
; Destroys the stack marker.
;
; Trashes r10
%macro NtWrapDestroyMarker 0
        lea     r10, [rbp - 30h]
        and     r10, ~1fh               ; 32-byte align it.
        mov     [r10      ], rbp
        mov     [r10 + 08h], rbp
        mov     [r10 + 10h], rbp
        mov     [r10 + 18h], rbp
%endmacro

;;
; Find the stack marker with the rbp of the entry frame.
;
; Search the current stack page inline, call a helper function
; which does a safe search of any further stack pages.
;
; Trashes       rax, r10 and r11.
; Modifies      rbp
;
%macro NtWrapLocateMarker 0
        mov     rax, rbp
        and     rax, ~1fh               ; 32-byte align it.

        ;
        ; Calc remainig space in the current page. If we're on a
        ; page boundrary, we'll search the entire previous page.
        ;
        mov     r10, rax
        neg     r10
        and     r10, 0fffh
        inc     r10
        shr     r10, 5                  ; /= 32 bytes
        jz      %%not_found             ; If zero, take the slow path

        ;
        ; The search loop.
        ;
%%again:
        dec     r10
        lea     rax, [rax + 20h]
        jz      %%not_found
        cmp     dword [rax      ], 0x20080901
        je      %%candidate
        jmp     %%again

%%not_found:
        call    NAME(NtWrapLocateMarkerHelper)
        jmp     %%done

%%candidate:
        cmp     dword [rax + 04h], 0x20080902
        jne     %%again
        cmp     dword [rax + 10h], 0x20080903
        jne     %%again
        cmp     dword [rax + 14h], 0x20080904
        jne     %%again
        mov     r11,  [rax + 08h]
        cmp     r11,  [rax + 18h]
        jne     %%again

        ; found it, change rbp.
        mov     rbp, r11
%%done:
%endmacro

;;
; Wraps a function with 4 or less argument that will go into registers.
%macro NtWrapFunctionWithAllRegParams 1
extern NAME(%1)
BEGINPROC supdrvNtWrap%1
        NtWrapProlog supdrvNtWrap%1
        NtWrapLocateMarker

        call    NAME(%1)

        NtWrapEpilog supdrvNtWrap%1
ENDPROC   supdrvNtWrap%1
%endmacro

;;
; Wraps a function with 5 argument, where the first 4 goes into registers.
%macro NtWrapFunctionWith5Params 1
extern NAME(%1)
BEGINPROC supdrvNtWrap%1
        NtWrapProlog supdrvNtWrap%1
        NtWrapLocateMarker

        mov     r11, [rdi + 30h]
        mov     [rsp + 20h], r11
        call    NAME(%1)

        NtWrapEpilog supdrvNtWrap%1
ENDPROC   supdrvNtWrap%1
%endmacro

;;
; Wraps a function with 6 argument, where the first 4 goes into registers.
%macro NtWrapFunctionWith6Params 1
extern NAME(%1)
BEGINPROC supdrvNtWrap%1
        NtWrapProlog supdrvNtWrap%1
        NtWrapLocateMarker

        mov     r11, [rdi + 30h]
        mov     [rsp + 20h], r11
        mov     r10, [rdi + 38h]
        mov     [rsp + 28h], r10
        call    NAME(%1)

        NtWrapEpilog supdrvNtWrap%1
ENDPROC   supdrvNtWrap%1
%endmacro

;;
; Wraps a function with 7 argument, where the first 4 goes into registers.
%macro NtWrapFunctionWith7Params 1
extern NAME(%1)
BEGINPROC supdrvNtWrap%1
        NtWrapProlog supdrvNtWrap%1
        NtWrapLocateMarker

        mov     r11, [rdi + 30h]
        mov     [rsp + 20h], r11
        mov     r10, [rdi + 38h]
        mov     [rsp + 28h], r10
        mov     rax, [rdi + 40h]
        mov     [rsp + 30h], rax
        call    NAME(%1)

        NtWrapEpilog supdrvNtWrap%1
ENDPROC   supdrvNtWrap%1
%endmacro

extern IoGetStackLimits

;;
; Helper that cautiously continues the stack marker search
; NtWrapLocateMarker started.
;
; The stack layout at the time is something like this.
;       rbp+08h         callers return address.
;       rbp-00h         saved rbp
;       rbp-08h         saved rdi
;       rbp-09h
;         thru          unused.
;       rbp-80h
;       rbp-88h         our return address.
;       rbp-89h
;         thru          callee register dump zone.
;       rbp-a0h
;
; @param    rax         Current stack location.
; @param    rdi         Parent stack frame pointer. (This should equal rbp on entry.)
;
; Trashes:  rax, r10, r11.
;           Will use the callers stack frame for register saving ASSUMING that
;           rbp-80h thru rbp-09h is unused.
;
; Modifies: rbp
;
BEGINPROC NtWrapLocateMarkerHelper
        ;
        ; Prolog. Save volatile regs and reserve callee space.
        ;
        sub     rsp, 20h                ; For IoGetStackLimits().
        mov     [rdi - 80h], rax
        mov     [rdi - 78h], rcx
        mov     [rdi - 70h], rdx
        mov     [rdi - 68h], r8
        mov     [rdi - 60h], r9

        ;
        ; Call VOID IoGetStackLimits(OUT PULONG_PTR LowLimit, OUT PULONG_PTR HighLimit);
        ;
        ; Use rdi-40h for the high limit and rdi-50h for the low one, we're only
        ; interested in the high one.
        ;
        lea     rcx, [rdi - 40h]        ; arg #1 LowLimit
        lea     rdx, [rdi - 50h]        ; arg #2 HighLimit
        mov     [rdx], eax              ; paranoia - init to end of current search.
        call    IoGetStackLimits

        ;
        ; Move the top address into r10, restore rax and continue
        ; the search. Check that r10 is less than 3 pages from rax.
        ;
        mov     rax, [rdi - 80h]        ; Restore eax (see prolog)
        mov     r10, [rdi - 50h]        ; HighLimit
        and     r10, ~1fh               ; 32-byte align it (downwards)
        sub     r10, rax
        jz      .not_found              ; If already at the top of the stack.
        cmp     r10, 3000h
        jae     .out_of_bounds          ; If too far away, something is busted.
        shr     r10, 5                  ; /= 32.

        ; The loop body.
.search_loop:
        cmp     dword [rax      ], 0x20080901
        je      .candidate
.continue_searching:
        dec     r10
        jz      .not_found
        lea     rax, [rax + 20h]
        jmp     .search_loop

        ; Found the first marker, check for the rest.
.candidate:
        cmp     dword [rax + 04h], 0x20080902
        jne     .continue_searching
        cmp     dword [rax + 10h], 0x20080903
        jne     .continue_searching
        cmp     dword [rax + 14h], 0x20080904
        jne     .continue_searching
        mov     r11,  [rax + 08h]
        cmp     r11,  [rax + 18h]
        jne     .continue_searching

        ; found it, change rbp.
        mov     rbp, r11

        ;
        ; Restore registers and pop the stack frame.
        ;
.epilog:
        mov     r9,  [rdi - 60h]
        mov     r8,  [rdi - 68h]
        mov     rdx, [rdi - 70h]
        mov     rcx, [rdi - 78h]
        ; mov     rax, [rdi - 80h]
        add     rsp, 20h
        ret

        ;
        ; Needless to say, this isn't supposed to happen. Thus the int3.
        ; Note down r10 and rax.
        ;
.out_of_bounds:
%ifdef DEBUG
        int3
%endif
.not_found:
%ifdef DEBUG
        int3
%endif
        jmp     .epilog
ENDPROC   NtWrapLocateMarkerHelper



;
; This has the same order as the list in SUPDrv.c
;
NtWrapFunctionWithAllRegParams  SUPR0ComponentRegisterFactory
NtWrapFunctionWithAllRegParams  SUPR0ComponentDeregisterFactory
NtWrapFunctionWithAllRegParams  SUPR0ComponentQueryFactory
NtWrapFunctionWith5Params       SUPR0ObjRegister
NtWrapFunctionWithAllRegParams  SUPR0ObjAddRef
NtWrapFunctionWithAllRegParams  SUPR0ObjRelease
NtWrapFunctionWithAllRegParams  SUPR0ObjVerifyAccess
NtWrapFunctionWithAllRegParams  SUPR0LockMem
NtWrapFunctionWithAllRegParams  SUPR0UnlockMem
NtWrapFunctionWith5Params       SUPR0ContAlloc
NtWrapFunctionWithAllRegParams  SUPR0ContFree
NtWrapFunctionWith5Params       SUPR0LowAlloc
NtWrapFunctionWithAllRegParams  SUPR0LowFree
NtWrapFunctionWithAllRegParams  SUPR0MemAlloc
NtWrapFunctionWithAllRegParams  SUPR0MemGetPhys
NtWrapFunctionWithAllRegParams  SUPR0MemFree
NtWrapFunctionWithAllRegParams  SUPR0PageAlloc
NtWrapFunctionWithAllRegParams  SUPR0PageFree
;NtWrapFunctionWithAllRegParams  SUPR0Printf            - cannot wrap this buster.
NtWrapFunctionWithAllRegParams  RTMemAlloc
NtWrapFunctionWithAllRegParams  RTMemAllocZ
NtWrapFunctionWithAllRegParams  RTMemFree
NtWrapFunctionWithAllRegParams  RTMemDup
NtWrapFunctionWithAllRegParams  RTMemDupEx
NtWrapFunctionWithAllRegParams  RTMemRealloc
NtWrapFunctionWithAllRegParams  RTR0MemObjAllocLow
NtWrapFunctionWithAllRegParams  RTR0MemObjAllocPage
NtWrapFunctionWithAllRegParams  RTR0MemObjAllocPhys
NtWrapFunctionWithAllRegParams  RTR0MemObjAllocPhysNC
NtWrapFunctionWithAllRegParams  RTR0MemObjAllocCont
NtWrapFunctionWithAllRegParams  RTR0MemObjLockUser
NtWrapFunctionWith5Params       RTR0MemObjMapKernel
NtWrapFunctionWith6Params       RTR0MemObjMapUser
;NtWrapFunctionWithAllRegParams  RTR0MemObjAddress      - not necessary
;NtWrapFunctionWithAllRegParams  RTR0MemObjAddressR3    - not necessary
;NtWrapFunctionWithAllRegParams  RTR0MemObjSize         - not necessary
;NtWrapFunctionWithAllRegParams  RTR0MemObjIsMapping    - not necessary
;NtWrapFunctionWithAllRegParams  RTR0MemObjGetPagePhysAddr - not necessary
NtWrapFunctionWithAllRegParams  RTR0MemObjFree
;NtWrapFunctionWithAllRegParams  RTProcSelf             - not necessary
;NtWrapFunctionWithAllRegParams  RTR0ProcHandleSelf     - not necessary
NtWrapFunctionWithAllRegParams  RTSemFastMutexCreate
NtWrapFunctionWithAllRegParams  RTSemFastMutexDestroy
NtWrapFunctionWithAllRegParams  RTSemFastMutexRequest
NtWrapFunctionWithAllRegParams  RTSemFastMutexRelease
NtWrapFunctionWithAllRegParams  RTSemEventCreate
NtWrapFunctionWithAllRegParams  RTSemEventSignal
NtWrapFunctionWithAllRegParams  RTSemEventWait
NtWrapFunctionWithAllRegParams  RTSemEventWaitNoResume
NtWrapFunctionWithAllRegParams  RTSemEventDestroy
NtWrapFunctionWithAllRegParams  RTSemEventMultiCreate
NtWrapFunctionWithAllRegParams  RTSemEventMultiSignal
NtWrapFunctionWithAllRegParams  RTSemEventMultiReset
NtWrapFunctionWithAllRegParams  RTSemEventMultiWait
NtWrapFunctionWithAllRegParams  RTSemEventMultiWaitNoResume
NtWrapFunctionWithAllRegParams  RTSemEventMultiDestroy
NtWrapFunctionWithAllRegParams  RTSpinlockCreate
NtWrapFunctionWithAllRegParams  RTSpinlockDestroy
NtWrapFunctionWithAllRegParams  RTSpinlockAcquire
NtWrapFunctionWithAllRegParams  RTSpinlockRelease
NtWrapFunctionWithAllRegParams  RTSpinlockAcquireNoInts
NtWrapFunctionWithAllRegParams  RTSpinlockReleaseNoInts
;NtWrapFunctionWithAllRegParams  RTTimeNanoTS           - not necessary
;NtWrapFunctionWithAllRegParams  RTTimeMilliTS          - not necessary
;NtWrapFunctionWithAllRegParams  RTTimeSystemNanoTS     - not necessary
;NtWrapFunctionWithAllRegParams  RTTimeSystemMilliTS    - not necessary
;NtWrapFunctionWithAllRegParams  RTThreadNativeSelf     - not necessary
NtWrapFunctionWithAllRegParams  RTThreadSleep
NtWrapFunctionWithAllRegParams  RTThreadYield
%if 0 ; Thread APIs, Part 2
;NtWrapFunctionWithAllRegParams  RTThreadSelf
NtWrapFunctionWith7Params       RTThreadCreate
NtWrapFunctionWithAllRegParams  RTThreadGetNative
NtWrapFunctionWithAllRegParams  RTThreadWait
NtWrapFunctionWithAllRegParams  RTThreadWaitNoResume
NtWrapFunctionWithAllRegParams  RTThreadGetName
NtWrapFunctionWithAllRegParams  RTThreadSelfName
NtWrapFunctionWithAllRegParams  RTThreadGetType
NtWrapFunctionWithAllRegParams  RTThreadUserSignal
NtWrapFunctionWithAllRegParams  RTThreadUserReset
NtWrapFunctionWithAllRegParams  RTThreadUserWait
NtWrapFunctionWithAllRegParams  RTThreadUserWaitNoResume
%endif
;NtWrapFunctionWithAllRegParams  RTLogDefaultInstance   - a bit of a gamble, but we do not want the overhead!
;NtWrapFunctionWithAllRegParams  RTMpCpuId              - not necessary
;NtWrapFunctionWithAllRegParams  RTMpCpuIdFromSetIndex  - not necessary
;NtWrapFunctionWithAllRegParams  RTMpCpuIdToSetIndex    - not necessary
;NtWrapFunctionWithAllRegParams  RTMpIsCpuPossible      - not necessary
;NtWrapFunctionWithAllRegParams  RTMpGetCount           - not necessary
;NtWrapFunctionWithAllRegParams  RTMpGetMaxCpuId        - not necessary
;NtWrapFunctionWithAllRegParams  RTMpGetOnlineCount     - not necessary
;NtWrapFunctionWithAllRegParams  RTMpGetOnlineSet       - not necessary
;NtWrapFunctionWithAllRegParams  RTMpGetSet             - not necessary
;NtWrapFunctionWithAllRegParams  RTMpIsCpuOnline        - not necessary
NtWrapFunctionWithAllRegParams  RTMpOnAll
NtWrapFunctionWithAllRegParams  RTMpOnOthers
NtWrapFunctionWithAllRegParams  RTMpOnSpecific
;NtWrapFunctionWithAllRegParams  RTLogRelDefaultInstance - not necessary.
NtWrapFunctionWithAllRegParams  RTLogSetDefaultInstanceThread
;NtWrapFunctionWithAllRegParams  RTLogLogger            - can't wrap this buster.
;NtWrapFunctionWithAllRegParams  RTLogLoggerEx          - can't wrap this buster.
NtWrapFunctionWith5Params       RTLogLoggerExV
;NtWrapFunctionWithAllRegParams  RTLogPrintf            - can't wrap this buster. ;; @todo provide va_list log wrappers in RuntimeR0.
NtWrapFunctionWithAllRegParams  RTLogPrintfV
NtWrapFunctionWithAllRegParams  AssertMsg1
;NtWrapFunctionWithAllRegParams  AssertMsg2             - can't wrap this buster.


;;
; @cproto DECLASM(int) supdrvNtWrapVMMR0EntryEx(PFNRT pfnVMMR0EntryEx, PVM pVM, unsigned uOperation, PSUPVMMR0REQHDR pReq, uint64_t u64Arg, PSUPDRVSESSION pSession);
;
; @param    pfnVMMR0EntryEx     rcx
; @param    pVM                 rdx
; @param    uOperation          r8
; @param    pReq                r9
; @param    u64Arg              [rsp + 28h] / [rbp + 30h]
; @param    pSession            [rsp + 30h] / [rbp + 38h]
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
        call    rax

        NtWrapDestroyMarker
        NtWrapEpilog supdrvNtWrapVMMR0EntryEx
ENDPROC   supdrvNtWrapVMMR0EntryEx


;;
; @cproto DECLASM(int)    supdrvNtWrapVMMR0EntryFast(PFNRT pfnVMMR0EntryFast, PVM pVM, unsigned uOperation);
;
; @param    pfnVMMR0EntryFast   rcx
; @param    pVM                 rdx
; @param    uOperation          r8
;
BEGINPROC supdrvNtWrapVMMR0EntryFast
        NtWrapProlog supdrvNtWrapVMMR0EntryFast
        NtWrapCreateMarker

        mov     rax, rcx
        mov     rcx, rdx
        mov     rdx, r8
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



 %endif ; RT_ARCH_AMD64
%endif ; SUPDRV_WITH_UNWIND_HACK

