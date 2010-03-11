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
#ifndef ___VBoxVideoShgsmi_h___
#define ___VBoxVideoShgsmi_h___

#include <iprt/cdefs.h>
#include <VBox/VBoxVideo.h>

typedef DECLCALLBACK(void) FNVBOXSHGSMICMDCOMPLETION(struct _HGSMIHEAP * pHeap, void *pvCmd, void *pvContext);
typedef FNVBOXSHGSMICMDCOMPLETION *PFNVBOXSHGSMICMDCOMPLETION;

typedef DECLCALLBACK(void) FNVBOXSHGSMICMDCOMPLETION_IRQ(struct _HGSMIHEAP * pHeap, void *pvCmd, void *pvContext,
                                        PFNVBOXSHGSMICMDCOMPLETION *ppfnCompletion, void **ppvCompletion);
typedef FNVBOXSHGSMICMDCOMPLETION_IRQ *PFNVBOXSHGSMICMDCOMPLETION_IRQ;

typedef struct VBOXSHGSMILIST_ENTRY
{
    struct VBOXSHGSMILIST_ENTRY *pNext;
} VBOXSHGSMILIST_ENTRY, *PVBOXSHGSMILIST_ENTRY;

typedef struct VBOXSHGSMILIST
{
    PVBOXSHGSMILIST_ENTRY pFirst;
    PVBOXSHGSMILIST_ENTRY pLast;
} VBOXSHGSMILIST, *PVBOXSHGSMILIST;

DECLINLINE(bool) vboxSHGSMIListIsEmpty(PVBOXSHGSMILIST pList)
{
    return !pList->pFirst;
}

DECLINLINE(void) vboxSHGSMIListInit(PVBOXSHGSMILIST pList)
{
    pList->pFirst = pList->pLast = NULL;
}

DECLINLINE(void) vboxSHGSMIListPut(PVBOXSHGSMILIST pList, PVBOXSHGSMILIST_ENTRY pFirst, PVBOXSHGSMILIST_ENTRY pLast)
{
    Assert(pFirst);
    Assert(pLast);
    pLast->pNext = NULL;
    if (pList->pLast)
    {
        Assert(pList->pFirst);
        pList->pLast->pNext = pFirst;
        pList->pLast = pLast;
    }
    else
    {
        Assert(!pList->pFirst);
        pList->pFirst = pFirst;
        pList->pLast = pLast;
    }
}

DECLINLINE(void) vboxSHGSMIListCat(PVBOXSHGSMILIST pList1, PVBOXSHGSMILIST pList2)
{
    vboxSHGSMIListPut(pList1, pList2->pFirst, pList2->pLast);
    pList2->pFirst = pList2->pLast = NULL;
}

DECLINLINE(void) vboxSHGSMICmdListDetach(PVBOXSHGSMILIST pList, PVBOXSHGSMILIST_ENTRY *ppFirst, PVBOXSHGSMILIST_ENTRY *ppLast)
{
    *ppFirst = pList->pFirst;
    if (ppLast)
        *ppLast = pList->pLast;
    pList->pFirst = NULL;
    pList->pLast = NULL;
}

DECLINLINE(void) vboxSHGSMICmdListDetach2List(PVBOXSHGSMILIST pList, PVBOXSHGSMILIST pDstList)
{
    vboxSHGSMICmdListDetach(pList, &pDstList->pFirst, &pDstList->pLast);
}

void VBoxSHGSMICommandSubmitAsynch (struct _HGSMIHEAP *pHeap, PVOID pvBuff, FNVBOXSHGSMICMDCOMPLETION pfnCompletion, PVOID pvCompletion, uint32_t fFlags);
void VBoxSHGSMICommandSubmitAsynchIrq (struct _HGSMIHEAP * pHeap, PVOID pvBuff, PFNVBOXSHGSMICMDCOMPLETION_IRQ pfnCompletion, PVOID pvCompletion, uint32_t fFlags);
void VBoxSHGSMICommandSubmitAsynchEvent (struct _HGSMIHEAP * pHeap, PVOID pvBuff, RTSEMEVENT hEventSem);
int VBoxSHGSMICommandSubmitSynch (struct _HGSMIHEAP * pHeap, PVOID pCmd);
int VBoxSHGSMICommandSubmitSynchCompletion (struct _HGSMIHEAP * pHeap, PVOID pCmd);
void* VBoxSHGSMICommandAlloc (struct _HGSMIHEAP * pHeap, HGSMISIZE cbData, uint8_t u8Channel, uint16_t u16ChannelInfo);
void VBoxSHGSMICommandFree (struct _HGSMIHEAP * pHeap, void *pvBuffer);
int VBoxSHGSMICommandProcessCompletion (struct _HGSMIHEAP * pHeap, HGSMIOFFSET offCmd, bool bIrq, PVBOXSHGSMILIST pPostProcessList);
int VBoxSHGSMICommandPostprocessCompletion (struct _HGSMIHEAP * pHeap, PVBOXSHGSMILIST pPostProcessList);

void vboxSHGSMICbCommandWrite(struct _HGSMIHEAP * pHeap, HGSMIOFFSET data);

#endif /* #ifndef ___VBoxVideoShgsmi_h___ */
