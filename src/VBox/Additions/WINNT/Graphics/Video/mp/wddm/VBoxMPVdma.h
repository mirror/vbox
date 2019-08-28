/* $Id$ */
/** @file
 * VBox WDDM Miniport driver
 */

/*
 * Copyright (C) 2011-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VBoxMPVdma_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VBoxMPVdma_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/asm.h>
#include <VBoxVideo.h>
#include <HGSMI.h>

typedef struct _VBOXMP_DEVEXT *PVBOXMP_DEVEXT;

/* ddi dma command queue handling */
typedef enum
{
    VBOXVDMADDI_STATE_UNCKNOWN = 0,
    VBOXVDMADDI_STATE_NOT_DX_CMD,
    VBOXVDMADDI_STATE_NOT_QUEUED,
    VBOXVDMADDI_STATE_PENDING,
    VBOXVDMADDI_STATE_SUBMITTED,
    VBOXVDMADDI_STATE_COMPLETED
} VBOXVDMADDI_STATE;

typedef struct VBOXVDMADDI_CMD *PVBOXVDMADDI_CMD;
typedef DECLCALLBACK(VOID) FNVBOXVDMADDICMDCOMPLETE_DPC(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext);
typedef FNVBOXVDMADDICMDCOMPLETE_DPC *PFNVBOXVDMADDICMDCOMPLETE_DPC;

typedef struct VBOXVDMADDI_CMD
{
    LIST_ENTRY QueueEntry;
    VBOXVDMADDI_STATE enmState;
    uint32_t u32NodeOrdinal;
    uint32_t u32FenceId;
    DXGK_INTERRUPT_TYPE enmComplType;
    PFNVBOXVDMADDICMDCOMPLETE_DPC pfnComplete;
    PVOID pvComplete;
} VBOXVDMADDI_CMD, *PVBOXVDMADDI_CMD;

typedef struct VBOXVDMADDI_CMD_QUEUE
{
    volatile uint32_t cQueuedCmds;
    LIST_ENTRY CmdQueue;
} VBOXVDMADDI_CMD_QUEUE, *PVBOXVDMADDI_CMD_QUEUE;

typedef struct VBOXVDMADDI_NODE
{
    VBOXVDMADDI_CMD_QUEUE CmdQueue;
    UINT uLastCompletedFenceId;
} VBOXVDMADDI_NODE, *PVBOXVDMADDI_NODE;

VOID vboxVdmaDdiNodesInit(PVBOXMP_DEVEXT pDevExt);
BOOLEAN vboxVdmaDdiCmdCompletedIrq(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd, DXGK_INTERRUPT_TYPE enmComplType);
VOID vboxVdmaDdiCmdSubmittedIrq(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd);

NTSTATUS vboxVdmaDdiCmdCompleted(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd, DXGK_INTERRUPT_TYPE enmComplType);
NTSTATUS vboxVdmaDdiCmdSubmitted(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd);

DECLINLINE(VOID) vboxVdmaDdiCmdInit(PVBOXVDMADDI_CMD pCmd, uint32_t u32NodeOrdinal, uint32_t u32FenceId,
                                    PFNVBOXVDMADDICMDCOMPLETE_DPC pfnComplete, PVOID pvComplete)
{
    pCmd->QueueEntry.Blink = NULL;
    pCmd->QueueEntry.Flink = NULL;
    pCmd->enmState = VBOXVDMADDI_STATE_NOT_QUEUED;
    pCmd->u32NodeOrdinal = u32NodeOrdinal;
    pCmd->u32FenceId = u32FenceId;
    pCmd->pfnComplete = pfnComplete;
    pCmd->pvComplete = pvComplete;
}

/* marks the command a submitted in a way that it is invisible for dx runtime,
 * i.e. the dx runtime won't be notified about the command completion
 * this is used to submit commands initiated by the driver, but not by the dx runtime */
DECLINLINE(VOID) vboxVdmaDdiCmdSubmittedNotDx(PVBOXVDMADDI_CMD pCmd)
{
    Assert(pCmd->enmState == VBOXVDMADDI_STATE_NOT_QUEUED);
    pCmd->enmState = VBOXVDMADDI_STATE_NOT_DX_CMD;
}

DECLCALLBACK(VOID) vboxVdmaDdiCmdCompletionCbFree(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext);

VOID vboxVdmaDdiCmdGetCompletedListIsr(PVBOXMP_DEVEXT pDevExt, LIST_ENTRY *pList);

BOOLEAN vboxVdmaDdiCmdIsCompletedListEmptyIsr(PVBOXMP_DEVEXT pDevExt);

#define VBOXVDMADDI_CMD_FROM_ENTRY(_pEntry) ((PVBOXVDMADDI_CMD)(((uint8_t*)(_pEntry)) - RT_OFFSETOF(VBOXVDMADDI_CMD, QueueEntry)))

DECLINLINE(VOID) vboxVdmaDdiCmdHandleCompletedList(PVBOXMP_DEVEXT pDevExt, LIST_ENTRY *pList)
{
    LIST_ENTRY *pEntry = pList->Flink;
    while (pEntry != pList)
    {
        PVBOXVDMADDI_CMD pCmd = VBOXVDMADDI_CMD_FROM_ENTRY(pEntry);
        pEntry = pEntry->Flink;
        if (pCmd->pfnComplete)
            pCmd->pfnComplete(pDevExt, pCmd, pCmd->pvComplete);
    }
}

/* DMA commands are currently submitted over HGSMI */
typedef struct VBOXVDMAINFO
{
#ifdef VBOX_WITH_VDMA
    VBOXSHGSMI CmdHeap;
#endif
    UINT      uLastCompletedPagingBufferCmdFenceId;
    BOOL      fEnabled;
} VBOXVDMAINFO, *PVBOXVDMAINFO;

int vboxVdmaCreate (PVBOXMP_DEVEXT pDevExt, VBOXVDMAINFO *pInfo
#ifdef VBOX_WITH_VDMA
        , ULONG offBuffer, ULONG cbBuffer
#endif
#if 0
        , PFNVBOXVDMASUBMIT pfnSubmit, PVOID pvContext
#endif
        );
int vboxVdmaDisable(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo);
int vboxVdmaEnable(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo);
int vboxVdmaDestroy(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo);

#ifdef VBOX_WITH_VDMA
int vboxVdmaFlush(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo);
DECLINLINE(HGSMIOFFSET) vboxVdmaCBufDrPtrOffset(const PVBOXVDMAINFO pInfo, const void RT_UNTRUSTED_VOLATILE_HOST *pvPtr)
{
    return VBoxSHGSMICommandPtrOffset(&pInfo->CmdHeap, pvPtr);
}
int vboxVdmaCBufDrSubmit(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo, VBOXVDMACBUF_DR RT_UNTRUSTED_VOLATILE_HOST *pDr);
int vboxVdmaCBufDrSubmitSynch(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo, VBOXVDMACBUF_DR RT_UNTRUSTED_VOLATILE_HOST *pDr);
struct VBOXVDMACBUF_DR RT_UNTRUSTED_VOLATILE_HOST *vboxVdmaCBufDrCreate(PVBOXVDMAINFO pInfo, uint32_t cbTrailingData);
void vboxVdmaCBufDrFree(PVBOXVDMAINFO pInfo, struct VBOXVDMACBUF_DR RT_UNTRUSTED_VOLATILE_HOST *pDr);

#define VBOXVDMACBUF_DR_DATA_OFFSET() (sizeof (VBOXVDMACBUF_DR))
#define VBOXVDMACBUF_DR_SIZE(_cbData) (VBOXVDMACBUF_DR_DATA_OFFSET() + (_cbData))
#define VBOXVDMACBUF_DR_DATA(_pDr) ( ((uint8_t*)(_pDr)) + VBOXVDMACBUF_DR_DATA_OFFSET() )

AssertCompile(sizeof (VBOXVDMADDI_CMD) <= RT_SIZEOFMEMB(VBOXVDMACBUF_DR, aGuestData));
#define VBOXVDMADDI_CMD_FROM_BUF_DR(_pDr) ((PVBOXVDMADDI_CMD)(_pDr)->aGuestData)
#define VBOXVDMACBUF_DR_FROM_DDI_CMD(_pCmd) ((PVBOXVDMACBUF_DR)(((uint8_t*)(_pCmd)) - RT_UOFFSETOF(VBOXVDMACBUF_DR, aGuestData)))

#endif

NTSTATUS vboxVdmaGgDmaBltPerform(PVBOXMP_DEVEXT pDevExt, struct VBOXWDDM_ALLOC_DATA * pSrcAlloc, RECT* pSrcRect,
        struct VBOXWDDM_ALLOC_DATA *pDstAlloc, RECT* pDstRect);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VBoxMPVdma_h */
