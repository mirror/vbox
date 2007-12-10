/* $Id$ */
/** @file
 * VBoxGuest - OS/2 specifics.
 */

/*
 * Copyright (C) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 * VBoxDrv - OS/2 specifics.
 *
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <os2ddk/bsekee.h>

#include "VBoxGuestInternal.h"
#include <VBox/VBoxGuest.h>
#include <VBox/version.h>
#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/spinlock.h>
#include <iprt/process.h>
#include <iprt/assert.h>
#include <iprt/log.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Device extention & session data association structure.
 */
static VBOXGUESTDEVEXT      g_DevExt;
/** The memory object for the MMIO memory.  */
static RTR0MEMOBJ           g_MemObjMMIO = NIL_RTR0MEMOBJ;
/** The memory mapping object the MMIO memory. */
static RTR0MEMOBJ           g_MemMapMMIO = NIL_RTR0MEMOBJ;

/** Spinlock protecting g_apSessionHashTab. */
static RTSPINLOCK           g_Spinlock = NIL_RTSPINLOCK;
/** Hash table */
static PVBOXGUESTSESSION    g_apSessionHashTab[19];
/** Calculates the index into g_apSessionHashTab.*/
#define SESSION_HASH(sfn) ((sfn) % RT_ELEMENTS(g_apSessionHashTab))

__BEGIN_DECLS
/* Defined in VBoxGuestA-os2.asm */
extern uint32_t             g_PhysMMIOBase;
extern uint32_t             g_cbMMIO; /* 0 currently not set. */
extern uint16_t             g_IOPortBase;
extern uint8_t              g_bInterruptLine;
extern uint8_t              g_bPciBusNo;
extern uint8_t              g_bPciDevFunNo;
extern RTFAR16              g_fpfnVBoxGuestOs2IDCService16;
extern RTFAR16              g_fpfnVBoxGuestOs2IDCService16Asm;
#ifdef DEBUG_READ
/* (debugging) */
extern uint16_t             g_offLogHead;
extern uint16_t volatile    g_offLogTail;
extern uint16_t const       g_cchLogMax;
extern char                 g_szLog[];
#endif
/* (init only:) */
extern char                 g_szInitText[];
extern uint16_t             g_cchInitText;
extern uint16_t             g_cchInitTextMax;
__END_DECLS


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int vboxGuestOS2MapMemory(void);
static VBOXOSTYPE vboxGuestOS2DetectVersion(void);

/* in VBoxGuestA-os2.asm */
DECLASM(int) VBoxGuestOS2SetIRQ(uint8_t bIRQ);


/**
 * 32-bit Ring-0 initialization.
 *
 * This is called from VBoxGuestA-os2.asm upon the first open call to the vboxgst$ device.
 *
 * @returns 0 on success, non-zero on failure.
 * @param   pszArgs     Pointer to the device arguments.
 */
DECLASM(int) VBoxGuestOS2Init(const char *pszArgs)
{
    Log(("VBoxGuestOS2Init: pszArgs='%s' MMIO=0x%RX32 IOPort=0x%RX16 Int=%#x Bus=%#x Dev=%#x Fun=%d\n",
         pszArgs, g_PhysMMIOBase, g_IOPortBase, g_bInterruptLine, g_bPciBusNo, g_bPciDevFunNo >> 3, g_bPciDevFunNo & 7));

    /*
     * Initialize the runtime.
     */
    int rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        /*
         * Process the commandline. Later.
         */
        bool fVerbose = true;

        /*
         * Map the MMIO memory if found.
         */
        rc = vboxGuestOS2MapMemory();
        if (RT_SUCCESS(rc))
        {
            /*
             * Initialize the device extension.
             */
            if (g_MemMapMMIO != NIL_RTR0MEMOBJ)
                rc = VBoxGuestInitDevExt(&g_DevExt, g_IOPortBase,
                                         RTR0MemObjAddress(g_MemMapMMIO),
                                         RTR0MemObjSize(g_MemMapMMIO),
                                         vboxGuestOS2DetectVersion());
            else
                rc = VBoxGuestInitDevExt(&g_DevExt, g_IOPortBase, NULL, 0,
                                         vboxGuestOS2DetectVersion());
            if (RT_SUCCESS(rc))
            {
                /*
                 * Initialize the session hash table.
                 */
                rc = RTSpinlockCreate(&g_Spinlock);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Configure the interrupt handler.
                     */
                    if (g_bInterruptLine)
                    {
                        rc = VBoxGuestOS2SetIRQ(g_bInterruptLine);
                        if (rc)
                        {
                            Log(("VBoxGuestOS2SetIRQ(%d) -> %d\n", g_bInterruptLine, rc));
                            rc = RTErrConvertFromOS2(rc);
                        }
                    }
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Success
                         */
                        if (fVerbose)
                        {
                            strcpy(&g_szInitText[0],
                                   "\r\n"
                                   "VirtualBox Guest Additions Driver for OS/2 version " VBOX_VERSION_STRING "\r\n"
                                   "Copyright (C) 2007 innotek GmbH\r\n");
                            g_cchInitText = strlen(&g_szInitText[0]);
                        }
                        Log(("VBoxGuestOS2Init: Successfully loaded\n%s", g_szInitText));
                        return VINF_SUCCESS;
                    }

                    g_cchInitText = RTStrPrintf(&g_szInitText[0], g_cchInitTextMax, "VBoxGuest.sys: SetIrq failed for IRQ %#d, rc=%Vrc\n",
                                                g_bInterruptLine, rc);
                }
                else
                    g_cchInitText = RTStrPrintf(&g_szInitText[0], g_cchInitTextMax, "VBoxGuest.sys: RTSpinlockCreate failed, rc=%Vrc\n", rc);
                VBoxGuestDeleteDevExt(&g_DevExt);
            }
            else
                g_cchInitText = RTStrPrintf(&g_szInitText[0], g_cchInitTextMax, "VBoxGuest.sys: VBoxGuestOS2InitDevExt failed, rc=%Vrc\n", rc);

            rc2 = RTR0MemObjFree(g_MemObjMMIO, true /* fFreeMappings */); AssertRC(rc2);
            g_MemObjMMIO = g_MemMapMMIO = NIL_RTR0MEMOBJ;
        }
        else
            g_cchInitText = RTStrPrintf(&g_szInitText[0], g_cchInitTextMax, "VBoxGuest.sys: VBoxGuestOS2MapMMIO failed, rc=%Vrc\n", rc);
        RTR0Term();
    }
    else
        g_cchInitText = RTStrPrintf(&g_szInitText[0], g_cchInitTextMax, "VBoxGuest.sys: RTR0Init failed, rc=%Vrc\n", rc);

    RTLogBackdoorPrintf("VBoxGuestOS2Init: failed rc=%Vrc - %s", rc, &g_szInitText[0]);
    return rc;
}


/**
 * Maps the VMMDev memory.
 *
 * @returns VBox status code.
 * @retval  VERR_VERSION_MISMATCH       The VMMDev memory didn't meet our expectations.
 *
 * @param   pDevExt     The device extension.
 */
static int vboxGuestOS2MapMemory(void)
{
    const RTCCPHYS PhysMMIOBase = g_PhysMMIOBase;

    /*
     * Did we find any MMIO region (0 or NIL)?
     */
    if (    !PhysMMIOBase
        ||  PhysMMIOBase == NIL_RTCCPHYS)
    {
        Assert(g_MemMapMMIO != NIL_RTR0MEMOBJ);
        return VINF_SUCCESS;
    }

    /*
     * Create a physical memory object for it.
     *
     * Since we don't know the actual size (OS/2 doesn't at least), we make
     * a qualified guess using the VMMDEV_RAM_SIZE.
     */
    size_t cb = RT_ALIGN_Z(VMMDEV_RAM_SIZE, PAGE_SIZE);
    int rc = RTR0MemObjEnterPhys(&g_MemObjMMIO, PhysMMIOBase, cb);
    if (RT_FAILURE(rc))
    {
        cb = _4K;
        rc = RTR0MemObjEnterPhys(&g_MemObjMMIO, PhysMMIOBase, cb);
    }
    if (RT_FAILURE(rc))
    {
        Log(("vboxGuestOS2MapMemory: RTR0MemObjEnterPhys(,%RCp,%zx) -> %Rrc\n",
             PhysMMIOBase, cb, rc));
        return rc;
    }

    /*
     * Map the object into kernel space.
     *
     * We want a normal mapping with normal caching, which good in two ways. First
     * since the API doesn't have any flags indicating how the mapping should be cached.
     * And second, because PGM doesn't necessarily respect the cache/writethru bits
     * anyway for normal RAM.
     */
    rc = RTR0MemObjMapKernel(&g_MemMapMMIO, g_MemObjMMIO, (void *)-1, 0,
                             RTMEM_PROT_READ | RTMEM_PROT_WRITE);
    if (RT_SUCCESS(rc))
    {
        /*
         * Validate the VMM memory.
         */
        VMMDevMemory *pVMMDev = (VMMDevMemory *)RTR0MemObjAddress(g_MemMapMMIO);
        Assert(pVMMDev);
        if (    pVMMDev->u32Version == VMMDEV_MEMORY_VERSION
            &&  pVMMDev->u32Size >= 32 /* just for checking sanity */)
        {
            /*
             * Did we hit the the correct size? If not we'll have to
             * redo the mapping using the correct size.
             */
            if (RT_ALIGN_32(pVMMDev->u32Size, PAGE_SIZE) == cb)
                return VINF_SUCCESS;

            Log(("vboxGuestOS2MapMemory: Actual size %#RX32 (tried %#zx)\n", pVMMDev->u32Size, cb));
            cb = RT_ALIGN_32(pVMMDev->u32Size, PAGE_SIZE);

            rc = RTR0MemObjFree(g_MemObjMMIO, true); AssertRC(rc);
            g_MemObjMMIO = g_MemMapMMIO = NIL_RTR0MEMOBJ;

            rc = RTR0MemObjEnterPhys(&g_MemObjMMIO, PhysMMIOBase, cb);
            if (RT_SUCCESS(rc))
            {
                rc = RTR0MemObjMapKernel(&g_MemMapMMIO, g_MemObjMMIO, (void *)-1, 0,
                                         RTMEM_PROT_READ | RTMEM_PROT_WRITE);
                if (RT_SUCCESS(rc))
                    return VINF_SUCCESS;

                Log(("vboxGuestOS2MapMemory: RTR0MemObjMapKernel [%RCp,%zx] -> %Rrc (2nd)\n",
                     PhysMMIOBase, cb, rc));
            }
            else
                Log(("vboxGuestOS2MapMemory: RTR0MemObjEnterPhys(,%RCp,%zx) -> %Rrc (2nd)\n",
                     PhysMMIOBase, cb, rc));
        }
        else
        {
            rc = VERR_VERSION_MISMATCH;
            LogRel(("vboxGuestOS2MapMemory: Bogus VMMDev memory; u32Version=%RX32 (expected %RX32) u32Size=%RX32\n",
                    pVMMDev->u32Version, VMMDEV_MEMORY_VERSION, pVMMDev->u32Size));
        }
    }
    else
        Log(("vboxGuestOS2MapMemory: RTR0MemObjMapKernel [%RCp,%zx] -> %Rrc\n",
             PhysMMIOBase, cb, rc));

    int rc2 = RTR0MemObjFree(g_MemObjMMIO, true /* fFreeMappings */); AssertRC(rc2);
    g_MemObjMMIO = g_MemMapMMIO = NIL_RTR0MEMOBJ;
    return rc;
}


/**
 * Called fromn VBoxGuestOS2Init to determin which OS/2 version this is.
 *
 * @returns VBox OS/2 type.
 */
static VBOXOSTYPE vboxGuestOS2DetectVersion(void)
{
    VBOXOSTYPE enmOSType = OSTypeOS2;

#if 0 /** @todo dig up the version stuff from GIS later and verify that the numbers are actually decimal. */
    unsigned uMajor, uMinor;
    if (uMajor == 2)
    {
        if (uMinor >= 30 && uMinor < 40)
            enmOSType = OSTypeOS2Warp3;
        else if (uMinor >= 40 && uMinor < 45)
            enmOSType = OSTypeOS2Warp4;
        else if (uMinor >= 45 && uMinor < 50)
            enmOSType = OSTypeOS2Warp45;
    }
#endif
    return enmOSType;
}


DECLASM(int) VBoxGuestOS2Open(uint16_t sfn)
{
    int                 rc;
    PVBOXGUESTSESSION   pSession;

    /*
     * Create a new session.
     */
    rc = VBoxGuestCreateUserSession(&g_DevExt, &pSession);
    if (RT_SUCCESS(rc))
    {
        pSession->sfn = sfn;

        /*
         * Insert it into the hash table.
         */
        unsigned iHash = SESSION_HASH(sfn);
        RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
        RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
        pSession->pNextHash = g_apSessionHashTab[iHash];
        g_apSessionHashTab[iHash] = pSession;
        RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
    }

    Log(("VBoxGuestOS2Open: g_DevExt=%p pSession=%p rc=%d pid=%d\n", &g_DevExt, pSession, rc, (int)RTProcSelf()));
    return rc;
}


DECLASM(int) VBoxGuestOS2Close(uint16_t sfn)
{
    Log(("VBoxGuestOS2Close: pid=%d sfn=%d\n", (int)RTProcSelf(), sfn));

    /*
     * Remove from the hash table.
     */
    PVBOXGUESTSESSION   pSession;
    const RTPROCESS     Process = RTProcSelf();
    const unsigned      iHash = SESSION_HASH(sfn);
    RTSPINLOCKTMP       Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);

    pSession = g_apSessionHashTab[iHash];
    if (pSession)
    {
        if (    pSession->sfn == sfn
            &&  pSession->Process == Process)
        {
            g_apSessionHashTab[iHash] = pSession->pNextHash;
            pSession->pNextHash = NULL;
        }
        else
        {
            PVBOXGUESTSESSION pPrev = pSession;
            pSession = pSession->pNextHash;
            while (pSession)
            {
                if (    pSession->sfn == sfn
                    &&  pSession->Process == Process)
                {
                    pPrev->pNextHash = pSession->pNextHash;
                    pSession->pNextHash = NULL;
                    break;
                }

                /* next */
                pPrev = pSession;
                pSession = pSession->pNextHash;
            }
        }
    }
    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
    if (!pSession)
    {
        Log(("VBoxGuestIoctl: WHUT?!? pSession == NULL! This must be a mistake... pid=%d sfn=%d\n", (int)Process, sfn));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Close the session.
     */
    VBoxGuestCloseSession(&g_DevExt, pSession);
    return 0;
}


DECLASM(int) VBoxGuestOS2IOCtlFast(uint16_t sfn, uint8_t iFunction, int32_t *prc)
{
    /*
     * Find the session.
     */
    RTSPINLOCKTMP       Tmp = RTSPINLOCKTMP_INITIALIZER;
    const RTPROCESS     Process = RTProcSelf();
    const unsigned      iHash = SESSION_HASH(sfn);
    PVBOXGUESTSESSION   pSession;

    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
    pSession = g_apSessionHashTab[iHash];
    if (pSession && pSession->Process != Process)
    {
        do pSession = pSession->pNextHash;
        while (     pSession
               &&   (   pSession->sfn != sfn
                     || pSession->Process != Process));
    }
    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
    if (RT_UNLIKELY(!pSession))
    {
        Log(("VBoxGuestIoctl: WHAT?!? pSession == NULL! This must be a mistake... pid=%d\n", (int)Process));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Dispatch the fast IOCtl.
     */
    *prc = VBoxGuestCommonIOCtlFast(iFunction, &g_DevExt, pSession);
    return 0;
}


/**
 * 32-bit IDC service routine.
 *
 * @returns VBox status code.
 * @param   u32Session          The session handle (PVBOXGUESTSESSION).
 * @param   iFunction           The requested function.
 * @param   pvData              The input/output data buffer. The caller ensures that this
 *                              cannot be swapped out, or that it's acceptable to take a
 *                              page in fault in the current context. If the request doesn't
 *                              take input or produces output, apssing NULL is okay.
 * @param   cbData              The size of the data buffer.
 * @param   pcbDataReturned     Where to store the amount of data that's returned.
 *                              This can be NULL if pvData is NULL.
 *
 * @remark  This is called from the 16-bit thunker as well as directly from the 32-bit clients.
 */
DECLASM(int) VBoxGuestOS2IDCService(uint32_t u32Session, unsigned iFunction, void *pvData, size_t cbData, size_t *pcbDataReturned)
{
    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)u32Session;
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertMsgReturn(pSession->sfn == 0xffff, ("%RX16\n", pSession->sfn), VERR_INVALID_HANDLE);
    AssertMsgReturn(pSession->pDevExt == &g_DevExt, ("%p != %p\n", pSession->pDevExt, &g_DevExt), VERR_INVALID_HANDLE);

    int rc;
    switch (iFunction)
    {
        default:
            rc = VBoxGuestCommonIOCtl(iFunction, &g_DevExt, pSession, pvData, cbData, pcbDataReturned);
            break;

        case VBOXGUEST_IOCTL_OS2_IDC_DISCONNECT:
            pSession->sfn = 0;
            VBoxGuestCloseSession(&g_DevExt, pSession);
            rc = VINF_SUCCESS;
            break;
    }
    return rc;
}


/**
 * Worker for VBoxGuestOS2IDC, it creates the kernel session.
 *
 * @returns Pointer to the session.
 */
DECLASM(PVBOXGUESTSESSION) VBoxGuestOS2IDCConnect(void)
{
    PVBOXGUESTSESSION pSession;
    int rc = VBoxGuestCreateKernelSession(&g_DevExt, &pSession);
    if (RT_SUCCESS(rc))
    {
        pSession->sfn = 0xffff;
        return pSession;
    }
    return NULL;
}


DECLASM(int) VBoxGuestOS2IOCtl(uint16_t sfn, uint8_t iCat, uint8_t iFunction, void *pvParm, void *pvData, uint16_t *pcbParm, uint16_t *pcbData)
{
    /*
     * Find the session.
     */
    RTSPINLOCKTMP       Tmp = RTSPINLOCKTMP_INITIALIZER;
    const RTPROCESS     Process = RTProcSelf();
    const unsigned      iHash = SESSION_HASH(sfn);
    PVBOXGUESTSESSION   pSession;

    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
    pSession = g_apSessionHashTab[iHash];
    if (pSession && pSession->Process != Process)
    {
        do pSession = pSession->pNextHash;
        while (     pSession
               &&   (   pSession->sfn != sfn
                     || pSession->Process != Process));
    }
    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
    if (!pSession)
    {
        Log(("VBoxGuestIoctl: WHAT?!? pSession == NULL! This must be a mistake... pid=%d\n", (int)Process));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Verify the category and dispatch the IOCtl.
     *
     * The IOCtl call uses the parameter buffer as generic data input/output
     * buffer similar to the one unix ioctl buffer argument. While the data
     * buffer is used for passing the VBox status code back to the caller
     * since the status codes that OS/2 accepts thru the DosDevIOCtl API is
     * severely restricted.
     */
    if (RT_LIKELY(iCat == VBOXGUEST_IOCTL_CATEGORY))
    {
        Log(("VBoxGuestOS2IOCtl: pSession=%p iFunction=%#x pvParm=%p pvData=%p *pcbParm=%d *pcbData=%d\n", pSession, iFunction, pvParm, pvData, *pcbParm, *pcbData));
        Assert(pvParm || !*pcbData);
        Assert(pvData);
        Assert(*pcbData == sizeof(int32_t)); /* the return code */

        /*
         * Lock the buffers.
         */
        int32_t rc;
        KernVMLock_t ParmLock;
        if (pvParm)
        {
            Assert(*pcbData);
            rc = KernVMLock(VMDHL_WRITE, pvParm, *pcbParm, &ParmLock, (KernPageList_t *)-1, NULL);
            AssertMsgReturn(!rc, ("KernVMLock(VMDHL_WRITE, %p, %#x, &p, NULL, NULL) -> %d\n", pvParm, *pcbParm, &ParmLock, rc), VERR_LOCK_FAILED);
        }

#if 0 /* don't bother locking it since it's only 4 bytes (the return code). */
        KernVMLock_t DataLock;
        if (pvData)
        {
            Assert(*pcbData);
            rc = KernVMLock(VMDHL_WRITE, pvData, *pcbData, &DataLock, (KernPageList_t *)-1, NULL);
            if (rc)
            {
                AssertMsgFailed(("KernVMLock(VMDHL_WRITE, %p, %#x, &p, NULL, NULL) -> %d\n", pvData, *pcbData, &DataLock, rc));
                KernVMUnlock(&ParmLock);
                return VERR_LOCK_FAILED;
            }
        }
#endif

        /*
         * Process the IOCtl.
         */
        size_t cbDataReturned = 0;
        rc = VBoxGuestCommonIOCtl(iFunction, &g_DevExt, pSession,
                                  pvParm, *pcbParm, &cbDataReturned);

        /*
         * Unlock the buffers.
         */
        if (pvParm)
        {
            int rc2 = KernVMUnlock(&ParmLock);
            AssertMsg(!rc2, ("rc2=%d\n", rc2)); NOREF(rc2);
            AssertMsg(cbDataReturned < _64K, ("cbDataReturned=%d\n", cbDataReturned));
            *pcbParm = cbDataReturned;
        }
#if 0
        if (pvData)
        {
            int rc2 = KernVMUnlock(&DataLock);
            AssertMsg(!rc2, ("rc2=%d\n", rc2));
        }
#else
        rc = KernCopyOut(pvData, &rc, sizeof(int32_t));
        AssertMsgReturn(!rc, ("KernCopyOut(%p, %p, sizeof(int32_t)) -> %d\n", pvData, &rc, rc), VERR_LOCK_FAILED);
#endif

        Log2(("VBoxGuestOS2IOCtl: returns VINF_SUCCESS / %d\n", rc));
        return VINF_SUCCESS;
    }
    return VERR_NOT_SUPPORTED;
}


/**
 * 32-bit ISR, called by 16-bit assembly thunker in VBoxGuestA-os2.asm.
 *
 * @returns true if it's our interrupt, false it isn't.
 */
DECLASM(bool) VBoxGuestOS2ISR(void)
{
    Log(("VBoxGuestOS2ISR\n"));

    return VBoxGuestCommonISR(&g_DevExt);
}


#ifdef DEBUG_READ /** @todo figure out this one once and for all... */

/**
 * Callback for writing to the log buffer.
 *
 * @returns number of bytes written.
 * @param   pvArg       Unused.
 * @param   pachChars   Pointer to an array of utf-8 characters.
 * @param   cbChars     Number of bytes in the character array pointed to by pachChars.
 */
static DECLCALLBACK(size_t) vboxGuestNativeLogOutput(void *pvArg, const char *pachChars, size_t cbChars)
{
    size_t cchWritten = 0;
    while (cbChars-- > 0)
    {
        const uint16_t offLogHead = g_offLogHead;
        const uint16_t offLogHeadNext = (offLogHead + 1) & (g_cchLogMax - 1);
        if (offLogHeadNext == g_offLogTail)
            break; /* no */
        g_szLog[offLogHead] = *pachChars++;
        g_offLogHead = offLogHeadNext;
        cchWritten++;
    }
    return cchWritten;
}


int SUPR0Printf(const char *pszFormat, ...)
{
    va_list va;

#if 0 //def DEBUG_bird
    va_start(va, pszFormat);
    RTLogComPrintfV(pszFormat, va);
    va_end(va);
#endif

    va_start(va, pszFormat);
    int cch = RTLogFormatV(vboxGuestNativeLogOutput, NULL, pszFormat, va);
    va_end(va);

    return cch;
}

#endif /* DEBUG_READ */

