/* $Id$ */
/** @file
 * VBoxDrv - OS/2 specifics.
 */

/*
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
#undef RT_MAX

#include "SUPDRV.h"
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
static SUPDRVDEVEXT         g_DevExt;
/** Spinlock protecting g_apSessionHashTab. */
static RTSPINLOCK           g_Spinlock = NIL_RTSPINLOCK;
/** Hash table */
static PSUPDRVSESSION       g_apSessionHashTab[19];
/** Calculates the index into g_apSessionHashTab.*/
#define SESSION_HASH(sfn) ((sfn) % RT_ELEMENTS(g_apSessionHashTab))

__BEGIN_DECLS
/* Defined in SUPDrvA-os2.asm */
extern uint16_t             g_offLogHead;
extern uint16_t volatile    g_offLogTail;
extern uint16_t const       g_cchLogMax;
extern char                 g_szLog[];
/* (init only:) */
extern char                 g_szInitText[];
extern uint16_t             g_cchInitText;
extern uint16_t             g_cchInitTextMax;
__END_DECLS


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/



/**
 * 32-bit Ring-0 initialization.
 *
 * @returns 0 on success, non-zero on failure.
 * @param   pszArgs     Pointer to the device arguments.
 */
DECLASM(int) VBoxDrvInit(const char *pszArgs)
{
    dprintf(("VBoxDrvInit: pszArgs=%s\n", pszArgs));

    /*
     * Initialize the runtime.
     */
    int rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        /*
         * Initialize the device extension.
         */
        rc = supdrvInitDevExt(&g_DevExt);
        if (RT_SUCCESS(rc))
        {
            /*
             * Initialize the session hash table.
             */
            rc = RTSpinlockCreate(&g_Spinlock);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Process the commandline. Later.
                 */
                bool fVerbose = true;

                /*
                 * Success
                 */
                if (fVerbose)
                {
                    strcpy(&g_szInitText[0],
                           "\r\n"
                           "VirtualBox.org Support Driver for OS/2 version " VBOX_VERSION_STRING "\r\n"
                           "Copyright (C) 2007 Knut St. Osmundsen\r\n"
                           "Copyright (C) 2007 InnoTek Systemberatung GmbH\r\n");
                    g_cchInitText = strlen(&g_szInitText[0]);
                }
                return VINF_SUCCESS;
            }
            g_cchInitText = RTStrPrintf(&g_szInitText[0], g_cchInitTextMax, "VBoxDrv.sys: RTSpinlockCreate failed, rc=%Vrc\n", rc);
            supdrvDeleteDevExt(&g_DevExt);
        }
        else
            g_cchInitText = RTStrPrintf(&g_szInitText[0], g_cchInitTextMax, "VBoxDrv.sys: supdrvInitDevExt failed, rc=%Vrc\n", rc);
        RTR0Term();
    }
    else
        g_cchInitText = RTStrPrintf(&g_szInitText[0], g_cchInitTextMax, "VBoxDrv.sys: RTR0Init failed, rc=%Vrc\n", rc);
    return rc;
}


DECLASM(int) VBoxDrvOpen(uint16_t sfn)
{
    int                 rc;
    PSUPDRVSESSION      pSession;

    /*
     * Create a new session.
     */
    rc = supdrvCreateSession(&g_DevExt, &pSession);
    if (RT_SUCCESS(rc))
    {
        pSession->Process = RTProcSelf();
        pSession->R0Process = RTR0ProcHandleSelf();
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

    dprintf(("VBoxDrvOpen: g_DevExt=%p pSession=%p rc=%d pid=%d\n", &g_DevExt, pSession, rc, (int)RTProcSelf()));
    return rc;
}


DECLASM(int) VBoxDrvClose(uint16_t sfn)
{
    dprintf(("VBoxDrvClose: pid=%d sfn=%d\n", (int)RTProcSelf(), sfn));

    /*
     * Remove from the hash table.
     */
    PSUPDRVSESSION  pSession;
    const RTPROCESS Process = RTProcSelf();
    const unsigned  iHash = SESSION_HASH(sfn);
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
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
            PSUPDRVSESSION pPrev = pSession;
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
        OSDBGPRINT(("VBoxDrvIoctl: WHUT?!? pSession == NULL! This must be a mistake... pid=%d sfn=%d\n", (int)Process, sfn));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Close the session.
     */
    supdrvCloseSession(&g_DevExt, pSession);
    return 0;
}


DECLASM(int) VBoxDrvIOCtlFast(uint16_t sfn, uint8_t iFunction, int32_t *prc)
{
    /*
     * Find the session.
     */
    RTSPINLOCKTMP       Tmp = RTSPINLOCKTMP_INITIALIZER;
    const RTPROCESS     Process = RTProcSelf();
    const unsigned      iHash = SESSION_HASH(sfn);
    PSUPDRVSESSION      pSession;

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
        OSDBGPRINT(("VBoxDrvIoctl: WHUT?!? pSession == NULL! This must be a mistake... pid=%d\n", (int)Process));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Dispatch the fast IOCtl.
     */
    *prc = supdrvIOCtlFast(iFunction, &g_DevExt, pSession);
    return 0;
}


DECLASM(int) VBoxDrvIOCtl(uint16_t sfn, uint8_t iCat, uint8_t iFunction, void *pvParm, void *pvData, uint16_t *pcbParm, uint16_t *pcbData)
{
    /*
     * Find the session.
     */
    RTSPINLOCKTMP       Tmp = RTSPINLOCKTMP_INITIALIZER;
    const RTPROCESS     Process = RTProcSelf();
    const unsigned      iHash = SESSION_HASH(sfn);
    PSUPDRVSESSION      pSession;

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
        OSDBGPRINT(("VBoxDrvIoctl: WHUT?!? pSession == NULL! This must be a mistake... pid=%d\n", (int)Process));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Verify the category and dispatch the IOCtl.
     */
    if (RT_LIKELY(iCat == SUP_CTL_CATEGORY))
    {
        dprintf(("VBoxDrvIOCtl: pSession=%p iFunction=%#x pvParm=%p pvData=%p *pcbParm=%d *pcbData=%d\n", pSession, iFunction, pvParm, pvData, *pcbParm, *pcbData));
        Assert(pvParm);
        Assert(!pvData);

        /*
         * Lock the buffers.
         */
        PSUPDRVIOCTLDATA pArgs = (PSUPDRVIOCTLDATA)pvParm;
        AssertReturn(*pcbParm == sizeof(*pArgs), VERR_INVALID_PARAMETER);
        KernVMLock_t ParmLock;
        int rc = KernVMLock(VMDHL_WRITE, pvParm, *pcbParm, &ParmLock, (KernPageList_t *)-1, NULL);
        AssertMsgReturn(!rc, ("KernVMLock(VMDHL_WRITE, %p, %#x, &p, NULL, NULL) -> %d\n", pvParm, *pcbParm, &ParmLock, rc), VERR_LOCK_FAILED);

        /* lock the in and out buffers. */
        KernVMLock_t    InLock;
        bool            fInLocked = pArgs->pvIn && pArgs->cbIn;
        if (fInLocked)
        {
            rc = KernVMLock(VMDHL_WRITE, pArgs->pvIn, pArgs->cbIn, &InLock, (KernPageList_t *)-1, NULL);
            if (rc)
            {
                AssertMsgFailed(("KernVMLock(VMDHL_WRITE, %p, %#x, &p, NULL, NULL) -> %d\n", pArgs->pvIn, pArgs->cbIn, &InLock, rc));
                KernVMUnlock(&ParmLock);
                return VERR_LOCK_FAILED;
            }
        }

        KernVMLock_t    OutLock;
        bool            fOutLocked = pArgs->pvOut && pArgs->cbOut;
        if (fOutLocked)
        {
            rc = KernVMLock(VMDHL_WRITE, pArgs->pvOut, pArgs->cbOut, &OutLock, (KernPageList_t *)-1, NULL);
            if (rc)
            {
                AssertMsgFailed(("KernVMLock(VMDHL_WRITE, %p, %#x, &p, NULL, NULL) -> %d\n", pArgs->pvOut, pArgs->cbOut, &OutLock, rc));
                KernVMUnlock(&ParmLock);
                if (fInLocked)
                    KernVMUnlock(&InLock);
                return VERR_LOCK_FAILED;
            }
        }

        /*
         * Process the IOCtl.
         */
        unsigned cbReturned = 0;
        pArgs->rc = rc = supdrvIOCtl(iFunction, &g_DevExt, pSession, pArgs->pvIn, pArgs->cbIn, pArgs->pvOut, pArgs->cbOut, &cbReturned);

        /*
         * Unlock the buffers.
         */
        if (fOutLocked)
        {
            int rc2 = KernVMUnlock(&OutLock);
            AssertMsg(!rc2, ("rc2=%d\n", rc2));
        }
        if (fInLocked)
        {
            int rc2 = KernVMUnlock(&InLock);
            AssertMsg(!rc2, ("rc2=%d\n", rc2));
        }
        int rc2 = KernVMUnlock(&ParmLock);
        AssertMsg(!rc2, ("rc2=%d\n", rc2));

        dprintf2(("VBoxDrvIOCtl: returns VINF_SUCCESS / %d\n", rc));
        return VINF_SUCCESS;
    }
    return VERR_NOT_SUPPORTED;
}


void VBOXCALL   supdrvOSObjInitCreator(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession)
{
    NOREF(pObj);
    NOREF(pSession);
}


bool VBOXCALL   supdrvOSObjCanAccess(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession, const char *pszObjName, int *prc)
{
    NOREF(pObj);
    NOREF(pSession);
    NOREF(pszObjName);
    NOREF(prc);
    return false;
}


/**
 * Callback for writing to the log buffer.
 *
 * @returns number of bytes written.
 * @param   pvArg       Unused.
 * @param   pachChars   Pointer to an array of utf-8 characters.
 * @param   cbChars     Number of bytes in the character array pointed to by pachChars.
 */
static DECLCALLBACK(size_t) VBoxDrvLogOutput(void *pvArg, const char *pachChars, size_t cbChars)
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


SUPR0DECL(int) SUPR0Printf(const char *pszFormat, ...)
{
    va_list va;

#if 0 //def DEBUG_bird
    va_start(va, pszFormat);
    RTLogComPrintfV(pszFormat, va);
    va_end(va);
#endif

    va_start(va, pszFormat);
    int cch = RTLogFormatV(VBoxDrvLogOutput, NULL, pszFormat, va);
    va_end(va);

    return cch;
}
