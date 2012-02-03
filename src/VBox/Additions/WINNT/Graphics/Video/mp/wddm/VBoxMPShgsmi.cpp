/* $Id$ */

/** @file
 * VBox WDDM Miniport driver
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxMPWddm.h"
#include <iprt/semaphore.h>

/* SHGSMI */
DECLINLINE(void) vboxSHGSMICommandRetain (PVBOXSHGSMIHEADER pCmd)
{
    ASMAtomicIncU32(&pCmd->cRefs);
}

void vboxSHGSMICommandFree (struct _HGSMIHEAP * pHeap, PVBOXSHGSMIHEADER pCmd)
{
    HGSMIHeapFree (pHeap, pCmd);
}

DECLINLINE(void) vboxSHGSMICommandRelease (struct _HGSMIHEAP * pHeap, PVBOXSHGSMIHEADER pCmd)
{
    uint32_t cRefs = ASMAtomicDecU32(&pCmd->cRefs);
    Assert(cRefs < UINT32_MAX / 2);
    if(!cRefs)
        vboxSHGSMICommandFree (pHeap, pCmd);
}

DECLCALLBACK(void) vboxSHGSMICompletionSetEvent(struct _HGSMIHEAP * pHeap, void *pvCmd, void *pvContext)
{
    RTSemEventSignal((RTSEMEVENT)pvContext);
}

DECLCALLBACK(void) vboxSHGSMICompletionCommandRelease(struct _HGSMIHEAP * pHeap, void *pvCmd, void *pvContext)
{
    vboxSHGSMICommandRelease (pHeap, VBoxSHGSMIBufferHeader(pvCmd));
}

/* do not wait for completion */
DECLINLINE(const VBOXSHGSMIHEADER*) vboxSHGSMICommandPrepAsynch (struct _HGSMIHEAP * pHeap, PVBOXSHGSMIHEADER pHeader)
{
    /* ensure the command is not removed until we're processing it */
    vboxSHGSMICommandRetain(pHeader);
    return pHeader;
}

DECLINLINE(void) vboxSHGSMICommandDoneAsynch (struct _HGSMIHEAP * pHeap, const VBOXSHGSMIHEADER* pHeader)
{
    if(!(ASMAtomicReadU32((volatile uint32_t *)&pHeader->fFlags) & VBOXSHGSMI_FLAG_HG_ASYNCH))
    {
        PFNVBOXSHGSMICMDCOMPLETION pfnCompletion = (PFNVBOXSHGSMICMDCOMPLETION)pHeader->u64Info1;
        if (pfnCompletion)
            pfnCompletion(pHeap, VBoxSHGSMIBufferData (pHeader), (PVOID)pHeader->u64Info2);
    }

    vboxSHGSMICommandRelease(pHeap, (PVBOXSHGSMIHEADER)pHeader);
}

const VBOXSHGSMIHEADER* VBoxSHGSMICommandPrepAsynchEvent (struct _HGSMIHEAP * pHeap, PVOID pvBuff, RTSEMEVENT hEventSem)
{
    PVBOXSHGSMIHEADER pHeader = VBoxSHGSMIBufferHeader (pvBuff);
    pHeader->u64Info1 = (uint64_t)vboxSHGSMICompletionSetEvent;
    pHeader->u64Info2 = (uint64_t)hEventSem;
    pHeader->fFlags   = VBOXSHGSMI_FLAG_GH_ASYNCH_IRQ;

    return vboxSHGSMICommandPrepAsynch (pHeap, pHeader);
}

const VBOXSHGSMIHEADER* VBoxSHGSMICommandPrepSynch (struct _HGSMIHEAP * pHeap, PVOID pCmd)
{
    RTSEMEVENT hEventSem;
    int rc = RTSemEventCreate(&hEventSem);
    Assert(RT_SUCCESS(rc));
    if (RT_SUCCESS(rc))
    {
        return VBoxSHGSMICommandPrepAsynchEvent (pHeap, pCmd, hEventSem);
    }
    return NULL;
}

void VBoxSHGSMICommandDoneAsynch (struct _HGSMIHEAP * pHeap, const VBOXSHGSMIHEADER * pHeader)
{
    vboxSHGSMICommandDoneAsynch(pHeap, pHeader);
}

int VBoxSHGSMICommandDoneSynch (struct _HGSMIHEAP * pHeap, const VBOXSHGSMIHEADER* pHeader)
{
    VBoxSHGSMICommandDoneAsynch (pHeap, pHeader);
    RTSEMEVENT hEventSem = (RTSEMEVENT)pHeader->u64Info2;
    int rc = RTSemEventWait(hEventSem, RT_INDEFINITE_WAIT);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
        RTSemEventDestroy(hEventSem);
    return rc;
}

void VBoxSHGSMICommandCancelAsynch (struct _HGSMIHEAP * pHeap, const VBOXSHGSMIHEADER* pHeader)
{
    vboxSHGSMICommandRelease(pHeap, (PVBOXSHGSMIHEADER)pHeader);
}

void VBoxSHGSMICommandCancelSynch (struct _HGSMIHEAP * pHeap, const VBOXSHGSMIHEADER* pHeader)
{
    VBoxSHGSMICommandCancelAsynch (pHeap, pHeader);
    RTSEMEVENT hEventSem = (RTSEMEVENT)pHeader->u64Info2;
    RTSemEventDestroy(hEventSem);
}

const VBOXSHGSMIHEADER* VBoxSHGSMICommandPrepAsynch (struct _HGSMIHEAP * pHeap, PVOID pvBuff, PFNVBOXSHGSMICMDCOMPLETION pfnCompletion, PVOID pvCompletion, uint32_t fFlags)
{
    fFlags &= ~VBOXSHGSMI_FLAG_GH_ASYNCH_CALLBACK_IRQ;
    PVBOXSHGSMIHEADER pHeader = VBoxSHGSMIBufferHeader (pvBuff);
    pHeader->u64Info1 = (uint64_t)pfnCompletion;
    pHeader->u64Info2 = (uint64_t)pvCompletion;
    pHeader->fFlags = fFlags;

    return vboxSHGSMICommandPrepAsynch (pHeap, pHeader);
}

const VBOXSHGSMIHEADER* VBoxSHGSMICommandPrepAsynchIrq (struct _HGSMIHEAP * pHeap, PVOID pvBuff, PFNVBOXSHGSMICMDCOMPLETION_IRQ pfnCompletion, PVOID pvCompletion, uint32_t fFlags)
{
    fFlags |= VBOXSHGSMI_FLAG_GH_ASYNCH_CALLBACK_IRQ | VBOXSHGSMI_FLAG_GH_ASYNCH_IRQ;
    PVBOXSHGSMIHEADER pHeader = VBoxSHGSMIBufferHeader (pvBuff);
    pHeader->u64Info1 = (uint64_t)pfnCompletion;
    pHeader->u64Info2 = (uint64_t)pvCompletion;
    /* we must assign rather than or because flags field does not get zeroed on command creation */
    pHeader->fFlags = fFlags;

    return vboxSHGSMICommandPrepAsynch (pHeap, pHeader);
}

void* VBoxSHGSMICommandAlloc (struct _HGSMIHEAP * pHeap, HGSMISIZE cbData, uint8_t u8Channel, uint16_t u16ChannelInfo)
{
    /* Issue the flush command. */
    PVBOXSHGSMIHEADER pHeader = (PVBOXSHGSMIHEADER)HGSMIHeapAlloc (pHeap, cbData + sizeof (VBOXSHGSMIHEADER), u8Channel, u16ChannelInfo);
    Assert(pHeader);
    if (pHeader)
    {
        pHeader->cRefs = 1;
        return VBoxSHGSMIBufferData(pHeader);
    }
    return NULL;
}

void VBoxSHGSMICommandFree (struct _HGSMIHEAP * pHeap, void *pvBuffer)
{
    PVBOXSHGSMIHEADER pHeader = VBoxSHGSMIBufferHeader(pvBuffer);
    vboxSHGSMICommandRelease (pHeap, pHeader);
}

//int VBoxSHGSMISetup (PVBOXSHGSMIHEAP pHeap,
//                void *pvBase,
//                HGSMISIZE cbArea,
//                HGSMIOFFSET offBase,
//                bool fOffsetBased,
//                PFNVBOXSHGSMINOTIFYHOST pfnNotifyHost,
//                PFNVBOXSHGSMINOTIFYHOST pvNotifyHost)
//{
//    /* Setup a HGSMI heap within the adapter information area. */
//    return HGSMIHeapSetup (&pHeap->Heap,
//                         pvBuffer,
//                         cbBuffer,
//                         offBuffer,
//                         false /*fOffsetBased*/);
//}
//
//int VBoxSHGSMIDestroy (PVBOXSHGSMIHEAP pHeap)
//{
//    HGSMIHeapDestroy (pHeap);
//    return VINF_SUCCESS;
//}

#define VBOXSHGSMI_CMD2LISTENTRY(_pCmd) ((PVBOXVTLIST_ENTRY)&(_pCmd)->pvNext)
#define VBOXSHGSMI_LISTENTRY2CMD(_pEntry) ( (PVBOXSHGSMIHEADER)((uint8_t *)(_pEntry) - RT_OFFSETOF(VBOXSHGSMIHEADER, pvNext)) )

int VBoxSHGSMICommandProcessCompletion (struct _HGSMIHEAP * pHeap, VBOXSHGSMIHEADER* pCur, bool bIrq, PVBOXVTLIST pPostProcessList)
{
    int rc = VINF_SUCCESS;

    do
    {
        if (pCur->fFlags & VBOXSHGSMI_FLAG_GH_ASYNCH_CALLBACK_IRQ)
        {
            Assert(bIrq);

            PFNVBOXSHGSMICMDCOMPLETION pfnCompletion = NULL;
            void *pvCompletion;
            PFNVBOXSHGSMICMDCOMPLETION_IRQ pfnCallback = (PFNVBOXSHGSMICMDCOMPLETION_IRQ)pCur->u64Info1;
            void *pvCallback = (void*)pCur->u64Info2;

            pfnCallback(pHeap, VBoxSHGSMIBufferData(pCur), pvCallback, &pfnCompletion, &pvCompletion);
            if (pfnCompletion)
            {
                pCur->u64Info1 = (uint64_t)pfnCompletion;
                pCur->u64Info2 = (uint64_t)pvCompletion;
                pCur->fFlags &= ~VBOXSHGSMI_FLAG_GH_ASYNCH_CALLBACK_IRQ;
            }
            else
            {
                /* nothing to do with this command */
                break;
            }
        }

        if (!bIrq)
        {
            PFNVBOXSHGSMICMDCOMPLETION pfnCallback = (PFNVBOXSHGSMICMDCOMPLETION)pCur->u64Info1;
            void *pvCallback = (void*)pCur->u64Info2;
            pfnCallback(pHeap, VBoxSHGSMIBufferData(pCur), pvCallback);
        }
        else
            vboxVtListPut(pPostProcessList, VBOXSHGSMI_CMD2LISTENTRY(pCur), VBOXSHGSMI_CMD2LISTENTRY(pCur));
    } while (0);


    return rc;
}

int VBoxSHGSMICommandPostprocessCompletion (struct _HGSMIHEAP * pHeap, PVBOXVTLIST pPostProcessList)
{
    PVBOXVTLIST_ENTRY pNext, pCur;
    for (pCur = pPostProcessList->pFirst; pCur; pCur = pNext)
    {
        /* need to save next since the command may be released in a pfnCallback and thus its data might be invalid */
        pNext = pCur->pNext;
        PVBOXSHGSMIHEADER pCmd = VBOXSHGSMI_LISTENTRY2CMD(pCur);
        PFNVBOXSHGSMICMDCOMPLETION pfnCallback = (PFNVBOXSHGSMICMDCOMPLETION)pCmd->u64Info1;
        void *pvCallback = (void*)pCmd->u64Info2;
        pfnCallback(pHeap, VBoxSHGSMIBufferData(pCmd), pvCallback);
    }

    return VINF_SUCCESS;
}
