/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "../VBoxVideo.h"
#include "../Helper.h"

#include <VBox/VBoxGuestLib.h>
#include <VBox/VBoxVideo.h>

#include <iprt/asm.h>
#include <iprt/semaphore.h>

#include "VBoxVideoShgsmi.h"


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


DECLINLINE(void) vboxSHGSMICommandDoSubmit(struct _HGSMIHEAP * pHeap, PVBOXSHGSMIHEADER pHeader)
{
    /* Initialize the buffer and get the offset for port IO. */
    HGSMIOFFSET offBuffer = HGSMIHeapBufferOffset (pHeap, pHeader);

    /* Submit the buffer to the host. */
    vboxSHGSMICbCommandWrite(pHeap, offBuffer);
}

/* do not wait for completion */
DECLINLINE(void) vboxSHGSMICommandSubmitAsynch (struct _HGSMIHEAP * pHeap, PVBOXSHGSMIHEADER pHeader)
{
    /* ensure the command is not removed until we're processing it */
    vboxSHGSMICommandRetain(pHeader);

    vboxSHGSMICommandDoSubmit(pHeap, pHeader);

    if(!(ASMAtomicReadU32((volatile uint32_t *)&pHeader->fFlags) & VBOXSHGSMI_FLAG_HG_ASYNCH))
    {
        PFNVBOXSHGSMICMDCOMPLETION pfnCompletion = (PFNVBOXSHGSMICMDCOMPLETION)pHeader->u64Info1;
        pfnCompletion(pHeap, VBoxSHGSMIBufferData (pHeader), (PVOID)pHeader->u64Info2);
    }

    vboxSHGSMICommandRelease(pHeap, pHeader);

}

void VBoxSHGSMICommandSubmitAsynchEvent (struct _HGSMIHEAP * pHeap, PVOID pvBuff, RTSEMEVENT hEventSem)
{
    PVBOXSHGSMIHEADER pHeader = VBoxSHGSMIBufferHeader (pvBuff);
    pHeader->u64Info1 = (uint64_t)vboxSHGSMICompletionSetEvent;
    pHeader->u64Info2 = (uint64_t)hEventSem;
    pHeader->fFlags   = VBOXSHGSMI_FLAG_GH_ASYNCH_IRQ;

    vboxSHGSMICommandSubmitAsynch (pHeap, pHeader);
}

int VBoxSHGSMICommandSubmitSynch (struct _HGSMIHEAP * pHeap, PVOID pCmd)
{
    RTSEMEVENT hEventSem;
    int rc = RTSemEventCreate(&hEventSem);
    Assert(RT_SUCCESS(rc));
    if (RT_SUCCESS(rc))
    {
        VBoxSHGSMICommandSubmitAsynchEvent (pHeap, pCmd, hEventSem);

        rc = RTSemEventWait(hEventSem, RT_INDEFINITE_WAIT);
        Assert(RT_SUCCESS(rc));
        if (RT_SUCCESS(rc))
            RTSemEventDestroy(hEventSem);
    }
    return rc;
}

void VBoxSHGSMICommandSubmitAsynch (struct _HGSMIHEAP * pHeap, PVOID pvBuff, PFNVBOXSHGSMICMDCOMPLETION pfnCompletion, PVOID pvCompletion, uint32_t fFlags)
{
    fFlags &= ~VBOXSHGSMI_FLAG_GH_ASYNCH_CALLBACK_IRQ;
    PVBOXSHGSMIHEADER pHeader = VBoxSHGSMIBufferHeader (pvBuff);
    pHeader->u64Info1 = (uint64_t)pfnCompletion;
    pHeader->u64Info2 = (uint64_t)pvCompletion;
    pHeader->fFlags = fFlags;

    vboxSHGSMICommandSubmitAsynch (pHeap, pHeader);
}

void VBoxSHGSMICommandSubmitAsynchIrq (struct _HGSMIHEAP * pHeap, PVOID pvBuff, PFNVBOXSHGSMICMDCOMPLETION_IRQ pfnCompletion, PVOID pvCompletion, uint32_t fFlags)
{
    fFlags |= VBOXSHGSMI_FLAG_GH_ASYNCH_CALLBACK_IRQ | VBOXSHGSMI_FLAG_GH_ASYNCH_IRQ;
    PVBOXSHGSMIHEADER pHeader = VBoxSHGSMIBufferHeader (pvBuff);
    pHeader->u64Info1 = (uint64_t)pfnCompletion;
    pHeader->u64Info2 = (uint64_t)pvCompletion;
    /* we must assign rather than or because flags field does not get zeroed on command creation */
    pHeader->fFlags = fFlags;

    vboxSHGSMICommandSubmitAsynch (pHeap, pHeader);
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

#define VBOXSHGSMI_CMD2LISTENTRY(_pCmd) ((PVBOXSHGSMILIST_ENTRY)&(_pCmd)->pvNext)
#define VBOXSHGSMI_LISTENTRY2CMD(_pEntry) ( (PVBOXSHGSMIHEADER)((uint8_t *)(_pEntry) - RT_OFFSETOF(VBOXSHGSMIHEADER, pvNext)) )

int VBoxSHGSMICommandProcessCompletion (struct _HGSMIHEAP * pHeap, HGSMIOFFSET offCmd, bool bIrq, PVBOXSHGSMILIST pPostProcessList)
{
    int rc = VINF_SUCCESS;

    PVBOXSHGSMIHEADER pCur = (PVBOXSHGSMIHEADER)HGSMIOffsetToPointer (&pHeap->area, offCmd);
    Assert(pCur);
    if (pCur)
    {
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
                vboxSHGSMIListPut(pPostProcessList, VBOXSHGSMI_CMD2LISTENTRY(pCur), VBOXSHGSMI_CMD2LISTENTRY(pCur));
        } while (0);
    }
    else
    {
        rc = VERR_INVALID_PARAMETER;
    }

    return rc;
}

int VBoxSHGSMICommandPostprocessCompletion (struct _HGSMIHEAP * pHeap, PVBOXSHGSMILIST pPostProcessList)
{
    PVBOXSHGSMILIST_ENTRY pNext, pCur;
    for (pCur = pPostProcessList->pFirst; pCur; pCur = pNext)
    {
        /* need to save next since the command may be released in a pfnCallback and thus its data might be invalid */
        pNext = pCur->pNext;
        PVBOXSHGSMIHEADER pCmd = VBOXSHGSMI_LISTENTRY2CMD(pCur);
        PFNVBOXSHGSMICMDCOMPLETION pfnCallback = (PFNVBOXSHGSMICMDCOMPLETION)pCmd->u64Info1;
        void *pvCallback = (void*)pCmd->u64Info2;
        pfnCallback(pHeap, pCmd, pvCallback);
    }

    return VINF_SUCCESS;
}
