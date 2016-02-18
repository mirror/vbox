/* $Id$ */
/** @file
 * Virtual USB - URB pool.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_VUSB
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/mem.h>
#include <iprt/critsect.h>

#include "VUSBInternal.h"

/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/*********************************************************************************************************************************
*   Static Variables                                                                                                             *
*********************************************************************************************************************************/

/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

DECLHIDDEN(int) vusbUrbPoolInit(PVUSBURBPOOL pUrbPool)
{
    int rc = RTCritSectInit(&pUrbPool->CritSectPool);
    if (RT_SUCCESS(rc))
    {
        pUrbPool->cUrbsInPool = 0;
        for (unsigned i = 0; i < RT_ELEMENTS(pUrbPool->apFreeUrbs); i++)
            pUrbPool->apFreeUrbs[i] = NULL;
    }

    return rc;
}


DECLHIDDEN(void) vusbUrbPoolDestroy(PVUSBURBPOOL pUrbPool)
{
    RTCritSectEnter(&pUrbPool->CritSectPool);
    for (unsigned i = 0; i < RT_ELEMENTS(pUrbPool->apFreeUrbs); i++)
    {
        while (pUrbPool->apFreeUrbs[i])
        {
            PVUSBURB pUrb = pUrbPool->apFreeUrbs[i];
            pUrbPool->apFreeUrbs[i] = pUrb->pVUsb->pNext;

            pUrb->u32Magic = 0;
            pUrb->enmState = VUSBURBSTATE_INVALID;
            pUrb->pVUsb->pNext = NULL;
            RTMemFree(pUrb);
        }
    }
    RTCritSectLeave(&pUrbPool->CritSectPool);
    RTCritSectDelete(&pUrbPool->CritSectPool);
}


DECLHIDDEN(PVUSBURB) vusbUrbPoolAlloc(PVUSBURBPOOL pUrbPool, VUSBXFERTYPE enmType,
                                      VUSBDIRECTION enmDir, size_t cbData, size_t cbHci,
                                      size_t cbHciTd, unsigned cTds)
{
    /*
     * Reuse or allocate a new URB.
     */
    /** @todo try find a best fit, MSD which submits rather big URBs intermixed with small control
     * messages ends up with a 2+ of these big URBs when a single one is sufficient.  */
    /** @todo The allocations should be done by the device, at least as an option, since the devices
     * frequently wish to associate their own stuff with the in-flight URB or need special buffering
     * (isochronous on Darwin for instance). */
    /* Get the required amount of additional memory to allocate the whole state. */
    size_t cbMem = cbData + sizeof(VUSBURBVUSBINT) + cbHci + cTds * cbHciTd;
    uint32_t cbDataAllocated = 0;

    AssertReturn(enmType < RT_ELEMENTS(pUrbPool->apFreeUrbs), NULL);

    RTCritSectEnter(&pUrbPool->CritSectPool);
    PVUSBURB pUrbPrev = NULL;
    PVUSBURB pUrb = pUrbPool->apFreeUrbs[enmType];
    while (pUrb)
    {
        if (pUrb->pVUsb->cbDataAllocated >= cbMem)
        {
            /* Unlink and verify part of the state. */
            if (pUrbPrev)
                pUrbPrev->pVUsb->pNext = pUrb->pVUsb->pNext;
            else
                pUrbPool->apFreeUrbs[enmType] = pUrb->pVUsb->pNext;
            Assert(pUrb->u32Magic == VUSBURB_MAGIC);
            Assert(pUrb->enmState == VUSBURBSTATE_FREE);
            Assert(!pUrb->pVUsb->pNext || pUrb->pVUsb->pNext->enmState == VUSBURBSTATE_FREE);
            cbDataAllocated = pUrb->pVUsb->cbDataAllocated;
            break;
        }
        pUrbPrev = pUrb;
        pUrb = pUrb->pVUsb->pNext;
    }

    if (!pUrb)
    {
        /* allocate a new one. */
        cbDataAllocated = cbMem <= _4K  ? RT_ALIGN_32(cbMem, _1K)
                        : cbMem <= _32K ? RT_ALIGN_32(cbMem, _4K)
                                        : RT_ALIGN_32(cbMem, 16*_1K);

        pUrb = (PVUSBURB)RTMemAllocZ(RT_OFFSETOF(VUSBURB, abData[cbDataAllocated]));
        if (RT_UNLIKELY(!pUrb))
        {
            RTCritSectLeave(&pUrbPool->CritSectPool);
            AssertLogRelFailedReturn(NULL);
        }

        pUrbPool->cUrbsInPool++;
        pUrb->u32Magic               = VUSBURB_MAGIC;

    }
    RTCritSectLeave(&pUrbPool->CritSectPool);

    Assert(cbDataAllocated >= cbMem);

    /*
     * (Re)init the URB
     */
    uint32_t offAlloc = cbData;
    pUrb->enmState               = VUSBURBSTATE_ALLOCATED;
    pUrb->fCompleting            = false;
    pUrb->pszDesc                = NULL;
    pUrb->pVUsb                  = (PVUSBURBVUSB)&pUrb->abData[offAlloc];
    offAlloc += sizeof(VUSBURBVUSBINT);
    pUrb->pVUsb->pvFreeCtx       = NULL;
    pUrb->pVUsb->pfnFree         = NULL;
    pUrb->pVUsb->cbDataAllocated = cbDataAllocated;
    pUrb->pVUsb->pNext           = NULL;
    pUrb->pVUsb->ppPrev          = NULL;
    pUrb->pVUsb->pCtrlUrb        = NULL;
    pUrb->pVUsb->u64SubmitTS     = 0;
    pUrb->pVUsb->pvReadAhead     = NULL;
    pUrb->Dev.pvPrivate          = NULL;
    pUrb->Dev.pNext              = NULL;
    pUrb->EndPt                  = ~0;
    pUrb->enmType                = enmType;
    pUrb->enmDir                 = enmDir;
    pUrb->fShortNotOk            = false;
    pUrb->enmStatus              = VUSBSTATUS_INVALID;
    pUrb->cbData                 = cbData;
    pUrb->pHci                   = cbHci ? (PVUSBURBHCI)&pUrb->abData[offAlloc] : NULL;
    offAlloc += cbHci;
    pUrb->paTds                  = (cbHciTd && cTds) ? (PVUSBURBHCITD)&pUrb->abData[offAlloc] : NULL;

    return pUrb;
}


DECLHIDDEN(void) vusbUrbPoolFree(PVUSBURBPOOL pUrbPool, PVUSBURB pUrb)
{
    /*
     * Put it into the LIFO of free URBs.
     * (No ppPrev is needed here.)
     */
    VUSBXFERTYPE enmType = pUrb->enmType;
    AssertReturnVoid(enmType < RT_ELEMENTS(pUrbPool->apFreeUrbs));
    RTCritSectEnter(&pUrbPool->CritSectPool);
    pUrb->enmState = VUSBURBSTATE_FREE;
    pUrb->pVUsb->ppPrev = NULL;
    pUrb->pVUsb->pNext = pUrbPool->apFreeUrbs[enmType];
    pUrbPool->apFreeUrbs[enmType] = pUrb;
    Assert(pUrbPool->apFreeUrbs[enmType]->enmState == VUSBURBSTATE_FREE);
    RTCritSectLeave(&pUrbPool->CritSectPool);
}

