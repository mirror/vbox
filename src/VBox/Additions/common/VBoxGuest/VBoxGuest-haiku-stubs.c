/* $Id$ */
/** @file
 * VBoxGuest kernel module, Haiku Guest Additions, stubs.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * This code is based on:
 *
 * VirtualBox Guest Additions for Haiku.
 * Copyright (c) 2011 Mike Smith <mike@scgtrp.net>
 *                    François Revol <revol@free.fr>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*
 * This file provides stubs for calling VBox runtime functions through the vboxguest module.
 * It should be linked into any driver or module that uses the VBox runtime, except vboxguest
 * itself (which contains the actual library and therefore doesn't need stubs to call it).
 */

#include "VBoxGuest-haiku.h"
#include "VBoxGuestInternal.h"
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/process.h>
#include <iprt/mem.h>
#include <iprt/asm.h>
#include <iprt/mp.h>
#include <iprt/power.h>
#include <iprt/thread.h>

// >>> file('/tmp/stubs.c', 'w').writelines([re.sub(r'^(?P<returntype>[^(]+) \(\*_(?P<functionname>[A-Za-z0-9_]+)\)\((?P<params>[^)]+)\);', lambda m: '%s %s(%s)\n{\n    %sg_VBoxGuest->_%s(%s);\n}\n' % (m.group(1), m.group(2), m.group(3), ('return ' if m.group(1) != 'void' else ''), m.group(2), (', '.join(a.split(' ')[-1].replace('*', '') for a in m.group(3).split(',')) if m.group(3) != 'void' else '')), f) for f in functions])

struct vboxguest_module_info *g_VBoxGuest;

size_t RTLogBackdoorPrintf(const char *pszFormat, ...)
{
    va_list args;
    size_t cb;

    va_start(args, pszFormat);
    cb = g_VBoxGuest->_RTLogBackdoorPrintf(pszFormat, args);
    va_end(args);

    return cb;
}
size_t RTLogBackdoorPrintfV(const char *pszFormat, va_list args)
{
    return g_VBoxGuest->_RTLogBackdoorPrintfV(pszFormat, args);
}
int RTLogSetDefaultInstanceThread(PRTLOGGER pLogger, uintptr_t uKey)
{
    return g_VBoxGuest->_RTLogSetDefaultInstanceThread(pLogger, uKey);
}
int RTMemAllocExTag(size_t cb, size_t cbAlignment, uint32_t fFlags, const char *pszTag, void **ppv)
{
    return g_VBoxGuest->_RTMemAllocExTag(cb, cbAlignment, fFlags, pszTag, ppv);
}
void* RTMemContAlloc(PRTCCPHYS pPhys, size_t cb)
{
    return g_VBoxGuest->_RTMemContAlloc(pPhys, cb);
}
void RTMemContFree(void *pv, size_t cb)
{
    g_VBoxGuest->_RTMemContFree(pv, cb);
}
void RTMemFreeEx(void *pv, size_t cb)
{
    g_VBoxGuest->_RTMemFreeEx(pv, cb);
}
bool RTMpIsCpuPossible(RTCPUID idCpu)
{
    return g_VBoxGuest->_RTMpIsCpuPossible(idCpu);
}
int RTMpNotificationDeregister(PFNRTMPNOTIFICATION pfnCallback, void *pvUser)
{
    return g_VBoxGuest->_RTMpNotificationDeregister(pfnCallback, pvUser);
}
int RTMpNotificationRegister(PFNRTMPNOTIFICATION pfnCallback, void *pvUser)
{
    return g_VBoxGuest->_RTMpNotificationRegister(pfnCallback, pvUser);
}
int RTMpOnAll(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    return g_VBoxGuest->_RTMpOnAll(pfnWorker, pvUser1, pvUser2);
}
int RTMpOnOthers(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    return g_VBoxGuest->_RTMpOnOthers(pfnWorker, pvUser1, pvUser2);
}
int RTMpOnSpecific(RTCPUID idCpu, PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    return g_VBoxGuest->_RTMpOnSpecific(idCpu, pfnWorker, pvUser1, pvUser2);
}
int RTPowerNotificationDeregister(PFNRTPOWERNOTIFICATION pfnCallback, void *pvUser)
{
    return g_VBoxGuest->_RTPowerNotificationDeregister(pfnCallback, pvUser);
}
int RTPowerNotificationRegister(PFNRTPOWERNOTIFICATION pfnCallback, void *pvUser)
{
    return g_VBoxGuest->_RTPowerNotificationRegister(pfnCallback, pvUser);
}
int RTPowerSignalEvent(RTPOWEREVENT enmEvent)
{
    return g_VBoxGuest->_RTPowerSignalEvent(enmEvent);
}
void RTR0AssertPanicSystem(void)
{
    g_VBoxGuest->_RTR0AssertPanicSystem();
}
int RTR0Init(unsigned fReserved)
{
    return g_VBoxGuest->_RTR0Init(fReserved);
}
void* RTR0MemObjAddress(RTR0MEMOBJ MemObj)
{
    return g_VBoxGuest->_RTR0MemObjAddress(MemObj);
}
RTR3PTR RTR0MemObjAddressR3(RTR0MEMOBJ MemObj)
{
    return g_VBoxGuest->_RTR0MemObjAddressR3(MemObj);
}
int RTR0MemObjAllocContTag(PRTR0MEMOBJ pMemObj, size_t cb, bool fExecutable, const char *pszTag)
{
    return g_VBoxGuest->_RTR0MemObjAllocContTag(pMemObj, cb, fExecutable, pszTag);
}
int RTR0MemObjAllocLowTag(PRTR0MEMOBJ pMemObj, size_t cb, bool fExecutable, const char *pszTag)
{
    return g_VBoxGuest->_RTR0MemObjAllocLowTag(pMemObj, cb, fExecutable, pszTag);
}
int RTR0MemObjAllocPageTag(PRTR0MEMOBJ pMemObj, size_t cb, bool fExecutable, const char *pszTag)
{
    return g_VBoxGuest->_RTR0MemObjAllocPageTag(pMemObj, cb, fExecutable, pszTag);
}
int RTR0MemObjAllocPhysExTag(PRTR0MEMOBJ pMemObj, size_t cb, RTHCPHYS PhysHighest, size_t uAlignment, const char *pszTag)
{
    return g_VBoxGuest->_RTR0MemObjAllocPhysExTag(pMemObj, cb, PhysHighest, uAlignment, pszTag);
}
int RTR0MemObjAllocPhysNCTag(PRTR0MEMOBJ pMemObj, size_t cb, RTHCPHYS PhysHighest, const char *pszTag)
{
    return g_VBoxGuest->_RTR0MemObjAllocPhysNCTag(pMemObj, cb, PhysHighest, pszTag);
}
int RTR0MemObjAllocPhysTag(PRTR0MEMOBJ pMemObj, size_t cb, RTHCPHYS PhysHighest, const char *pszTag)
{
    return g_VBoxGuest->_RTR0MemObjAllocPhysTag(pMemObj, cb, PhysHighest, pszTag);
}
int RTR0MemObjEnterPhysTag(PRTR0MEMOBJ pMemObj, RTHCPHYS Phys, size_t cb, uint32_t uCachePolicy, const char *pszTag)
{
    return g_VBoxGuest->_RTR0MemObjEnterPhysTag(pMemObj, Phys, cb, uCachePolicy, pszTag);
}
int RTR0MemObjFree(RTR0MEMOBJ MemObj, bool fFreeMappings)
{
    return g_VBoxGuest->_RTR0MemObjFree(MemObj, fFreeMappings);
}
RTHCPHYS RTR0MemObjGetPagePhysAddr(RTR0MEMOBJ MemObj, size_t iPage)
{
    return g_VBoxGuest->_RTR0MemObjGetPagePhysAddr(MemObj, iPage);
}
bool RTR0MemObjIsMapping(RTR0MEMOBJ MemObj)
{
    return g_VBoxGuest->_RTR0MemObjIsMapping(MemObj);
}
int RTR0MemObjLockKernelTag(PRTR0MEMOBJ pMemObj, void *pv, size_t cb, uint32_t fAccess, const char *pszTag)
{
    return g_VBoxGuest->_RTR0MemObjLockKernelTag(pMemObj, pv, cb, fAccess, pszTag);
}
int RTR0MemObjLockUserTag(PRTR0MEMOBJ pMemObj, RTR3PTR R3Ptr, size_t cb, uint32_t fAccess, RTR0PROCESS R0Process, const char *pszTag)
{
    return g_VBoxGuest->_RTR0MemObjLockUserTag(pMemObj, R3Ptr, cb, fAccess, R0Process, pszTag);
}
int RTR0MemObjMapKernelExTag(PRTR0MEMOBJ pMemObj, RTR0MEMOBJ MemObjToMap, void *pvFixed, size_t uAlignment, unsigned fProt, size_t offSub, size_t cbSub, const char *pszTag)
{
    return g_VBoxGuest->_RTR0MemObjMapKernelExTag(pMemObj, MemObjToMap, pvFixed, uAlignment, fProt, offSub, cbSub, pszTag);
}
int RTR0MemObjMapKernelTag(PRTR0MEMOBJ pMemObj, RTR0MEMOBJ MemObjToMap, void *pvFixed, size_t uAlignment, unsigned fProt, const char *pszTag)
{
    return g_VBoxGuest->_RTR0MemObjMapKernelTag(pMemObj, MemObjToMap, pvFixed, uAlignment, fProt, pszTag);
}
int RTR0MemObjMapUserTag(PRTR0MEMOBJ pMemObj, RTR0MEMOBJ MemObjToMap, RTR3PTR R3PtrFixed, size_t uAlignment, unsigned fProt, RTR0PROCESS R0Process, const char *pszTag)
{
    return g_VBoxGuest->_RTR0MemObjMapUserTag(pMemObj, MemObjToMap, R3PtrFixed, uAlignment, fProt, R0Process, pszTag);
}
int RTR0MemObjProtect(RTR0MEMOBJ hMemObj, size_t offSub, size_t cbSub, uint32_t fProt)
{
    return g_VBoxGuest->_RTR0MemObjProtect(hMemObj, offSub, cbSub, fProt);
}
int RTR0MemObjReserveKernelTag(PRTR0MEMOBJ pMemObj, void *pvFixed, size_t cb, size_t uAlignment, const char *pszTag)
{
    return g_VBoxGuest->_RTR0MemObjReserveKernelTag(pMemObj, pvFixed, cb, uAlignment, pszTag);
}
int RTR0MemObjReserveUserTag(PRTR0MEMOBJ pMemObj, RTR3PTR R3PtrFixed, size_t cb, size_t uAlignment, RTR0PROCESS R0Process, const char *pszTag)
{
    return g_VBoxGuest->_RTR0MemObjReserveUserTag(pMemObj, R3PtrFixed, cb, uAlignment, R0Process, pszTag);
}
size_t RTR0MemObjSize(RTR0MEMOBJ MemObj)
{
    return g_VBoxGuest->_RTR0MemObjSize(MemObj);
}
RTR0PROCESS RTR0ProcHandleSelf(void)
{
    return g_VBoxGuest->_RTR0ProcHandleSelf();
}
void RTR0Term(void)
{
    g_VBoxGuest->_RTR0Term();
}
void RTR0TermForced(void)
{
    g_VBoxGuest->_RTR0TermForced();
}
RTPROCESS RTProcSelf(void)
{
    return g_VBoxGuest->_RTProcSelf();
}
uint32_t RTSemEventGetResolution(void)
{
    return g_VBoxGuest->_RTSemEventGetResolution();
}
uint32_t RTSemEventMultiGetResolution(void)
{
    return g_VBoxGuest->_RTSemEventMultiGetResolution();
}
int RTSemEventMultiWaitEx(RTSEMEVENTMULTI hEventMultiSem, uint32_t fFlags, uint64_t uTimeout)
{
    return g_VBoxGuest->_RTSemEventMultiWaitEx(hEventMultiSem, fFlags, uTimeout);
}
int RTSemEventMultiWaitExDebug(RTSEMEVENTMULTI hEventMultiSem, uint32_t fFlags, uint64_t uTimeout, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    return g_VBoxGuest->_RTSemEventMultiWaitExDebug(hEventMultiSem, fFlags, uTimeout, uId, pszFile, iLine, pszFunction);
}
int RTSemEventWaitEx(RTSEMEVENT hEventSem, uint32_t fFlags, uint64_t uTimeout)
{
    return g_VBoxGuest->_RTSemEventWaitEx(hEventSem, fFlags, uTimeout);
}
int RTSemEventWaitExDebug(RTSEMEVENT hEventSem, uint32_t fFlags, uint64_t uTimeout, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    return g_VBoxGuest->_RTSemEventWaitExDebug(hEventSem, fFlags, uTimeout, uId, pszFile, iLine, pszFunction);
}
bool RTThreadIsInInterrupt(RTTHREAD hThread)
{
    return g_VBoxGuest->_RTThreadIsInInterrupt(hThread);
}
void RTThreadPreemptDisable(PRTTHREADPREEMPTSTATE pState)
{
    g_VBoxGuest->_RTThreadPreemptDisable(pState);
}
bool RTThreadPreemptIsEnabled(RTTHREAD hThread)
{
    return g_VBoxGuest->_RTThreadPreemptIsEnabled(hThread);
}
bool RTThreadPreemptIsPending(RTTHREAD hThread)
{
    return g_VBoxGuest->_RTThreadPreemptIsPending(hThread);
}
bool RTThreadPreemptIsPendingTrusty(void)
{
    return g_VBoxGuest->_RTThreadPreemptIsPendingTrusty();
}
bool RTThreadPreemptIsPossible(void)
{
    return g_VBoxGuest->_RTThreadPreemptIsPossible();
}
void RTThreadPreemptRestore(PRTTHREADPREEMPTSTATE pState)
{
    g_VBoxGuest->_RTThreadPreemptRestore(pState);
}
uint32_t RTTimerGetSystemGranularity(void)
{
    return g_VBoxGuest->_RTTimerGetSystemGranularity();
}
int RTTimerReleaseSystemGranularity(uint32_t u32Granted)
{
    return g_VBoxGuest->_RTTimerReleaseSystemGranularity(u32Granted);
}
int RTTimerRequestSystemGranularity(uint32_t u32Request, uint32_t *pu32Granted)
{
    return g_VBoxGuest->_RTTimerRequestSystemGranularity(u32Request, pu32Granted);
}
void RTSpinlockAcquire(RTSPINLOCK Spinlock)
{
    g_VBoxGuest->_RTSpinlockAcquire(Spinlock);
}
void RTSpinlockRelease(RTSPINLOCK Spinlock)
{
    g_VBoxGuest->_RTSpinlockRelease(Spinlock);
}
void RTSpinlockReleaseNoInts(RTSPINLOCK Spinlock)
{
    g_VBoxGuest->_RTSpinlockReleaseNoInts(Spinlock);
}
void* RTMemTmpAllocTag(size_t cb, const char *pszTag)
{
    return g_VBoxGuest->_RTMemTmpAllocTag(cb, pszTag);
}
void RTMemTmpFree(void *pv)
{
    g_VBoxGuest->_RTMemTmpFree(pv);
}
PRTLOGGER RTLogDefaultInstance(void)
{
    return g_VBoxGuest->_RTLogDefaultInstance();
}
PRTLOGGER RTLogRelDefaultInstance(void)
{
    return g_VBoxGuest->_RTLogRelDefaultInstance();
}
int RTErrConvertToErrno(int iErr)
{
    return g_VBoxGuest->_RTErrConvertToErrno(iErr);
}
int VBoxGuestCommonIOCtl(unsigned iFunction, PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession, void *pvData, size_t cbData, size_t *pcbDataReturned)
{
    return g_VBoxGuest->_VBoxGuestCommonIOCtl(iFunction, pDevExt, pSession, pvData, cbData, pcbDataReturned);
}
int VBoxGuestCreateUserSession(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION *ppSession)
{
    return g_VBoxGuest->_VBoxGuestCreateUserSession(pDevExt, ppSession);
}
void VBoxGuestCloseSession(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession)
{
    g_VBoxGuest->_VBoxGuestCloseSession(pDevExt, pSession);
}
void* VBoxGuestIDCOpen(uint32_t *pu32Version)
{
    return g_VBoxGuest->_VBoxGuestIDCOpen(pu32Version);
}
int VBoxGuestIDCClose(void *pvSession)
{
    return g_VBoxGuest->_VBoxGuestIDCClose(pvSession);
}
int VBoxGuestIDCCall(void *pvSession, unsigned iCmd, void *pvData, size_t cbData, size_t *pcbDataReturned)
{
    return g_VBoxGuest->_VBoxGuestIDCCall(pvSession, iCmd, pvData, cbData, pcbDataReturned);
}
void RTAssertMsg1Weak(const char *pszExpr, unsigned uLine, const char *pszFile, const char *pszFunction)
{
    g_VBoxGuest->_RTAssertMsg1Weak(pszExpr, uLine, pszFile, pszFunction);
}
void RTAssertMsg2Weak(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    RTAssertMsg2WeakV(pszFormat, va);
    va_end(va);
}
void RTAssertMsg2WeakV(const char *pszFormat, va_list va)
{
    g_VBoxGuest->_RTAssertMsg2WeakV(pszFormat, va);
}
bool RTAssertShouldPanic(void)
{
    return g_VBoxGuest->_RTAssertShouldPanic();
}
int RTSemFastMutexCreate(PRTSEMFASTMUTEX phFastMtx)
{
    return g_VBoxGuest->_RTSemFastMutexCreate(phFastMtx);
}
int RTSemFastMutexDestroy(RTSEMFASTMUTEX hFastMtx)
{
    return g_VBoxGuest->_RTSemFastMutexDestroy(hFastMtx);
}
int RTSemFastMutexRelease(RTSEMFASTMUTEX hFastMtx)
{
    return g_VBoxGuest->_RTSemFastMutexRelease(hFastMtx);
}
int RTSemFastMutexRequest(RTSEMFASTMUTEX hFastMtx)
{
    return g_VBoxGuest->_RTSemFastMutexRequest(hFastMtx);
}
int RTSemMutexCreate(PRTSEMMUTEX phFastMtx)
{
    return g_VBoxGuest->_RTSemMutexCreate(phFastMtx);
}
int RTSemMutexDestroy(RTSEMMUTEX hFastMtx)
{
    return g_VBoxGuest->_RTSemMutexDestroy(hFastMtx);
}
int RTSemMutexRelease(RTSEMMUTEX hFastMtx)
{
    return g_VBoxGuest->_RTSemMutexRelease(hFastMtx);
}
int RTSemMutexRequest(RTSEMMUTEX hFastMtx, RTMSINTERVAL cMillies)
{
    return g_VBoxGuest->_RTSemMutexRequest(hFastMtx, cMillies);
}
int RTHeapSimpleRelocate(RTHEAPSIMPLE hHeap, uintptr_t offDelta)
{
    return g_VBoxGuest->_RTHeapSimpleRelocate(hHeap, offDelta);
}
int RTHeapOffsetInit(PRTHEAPOFFSET phHeap, void *pvMemory, size_t cbMemory)
{
    return g_VBoxGuest->_RTHeapOffsetInit(phHeap, pvMemory, cbMemory);
}
int RTHeapSimpleInit(PRTHEAPSIMPLE pHeap, void *pvMemory, size_t cbMemory)
{
    return g_VBoxGuest->_RTHeapSimpleInit(pHeap, pvMemory, cbMemory);
}
void* RTHeapOffsetAlloc(RTHEAPOFFSET hHeap, size_t cb, size_t cbAlignment)
{
    return g_VBoxGuest->_RTHeapOffsetAlloc(hHeap, cb, cbAlignment);
}
void* RTHeapSimpleAlloc(RTHEAPSIMPLE Heap, size_t cb, size_t cbAlignment)
{
    return g_VBoxGuest->_RTHeapSimpleAlloc(Heap, cb, cbAlignment);
}
void RTHeapOffsetFree(RTHEAPOFFSET hHeap, void *pv)
{
    g_VBoxGuest->_RTHeapOffsetFree(hHeap, pv);
}
void RTHeapSimpleFree(RTHEAPSIMPLE Heap, void *pv)
{
    g_VBoxGuest->_RTHeapSimpleFree(Heap, pv);
}
