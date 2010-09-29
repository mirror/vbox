/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___VBoxVideoVdma_h___
#define ___VBoxVideoVdma_h___

#include <iprt/cdefs.h>
#include <iprt/asm.h>
#include <VBox/VBoxVideo.h>
#include "../VBoxVideo.h"

/* ddi dma command queue handling */
typedef enum
{
    VBOXVDMADDI_STATE_UNCKNOWN = 0,
    VBOXVDMADDI_STATE_NOT_QUEUED,
    VBOXVDMADDI_STATE_PENDING,
    VBOXVDMADDI_STATE_SUBMITTED,
    VBOXVDMADDI_STATE_COMPLETED
} VBOXVDMADDI_STATE;

typedef struct VBOXVDMADDI_CMD *PVBOXVDMADDI_CMD;
typedef DECLCALLBACK(VOID) FNVBOXVDMADDICMDCOMPLETE_DPC(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext);
typedef FNVBOXVDMADDICMDCOMPLETE_DPC *PFNVBOXVDMADDICMDCOMPLETE_DPC;

typedef struct VBOXVDMADDI_CMD
{
    LIST_ENTRY QueueEntry;
    VBOXVDMADDI_STATE enmState;
    uint32_t u32FenceId;
    PVBOXWDDM_CONTEXT pContext;
    PFNVBOXVDMADDICMDCOMPLETE_DPC pfnComplete;
    PVOID pvComplete;
} VBOXVDMADDI_CMD, *PVBOXVDMADDI_CMD;

typedef struct VBOXVDMADDI_CMD_QUEUE
{
    volatile uint32_t cQueuedCmds;
    LIST_ENTRY CmdQueue;
    LIST_ENTRY DpcCmdQueue;
} VBOXVDMADDI_CMD_QUEUE, *PVBOXVDMADDI_CMD_QUEUE;

VOID vboxVdmaDdiQueueInit(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD_QUEUE pQueue);
BOOLEAN vboxVdmaDdiCmdCompletedIrq(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD_QUEUE pQueue, PVBOXVDMADDI_CMD pCmd);
VOID vboxVdmaDdiCmdSubmittedIrq(PVBOXVDMADDI_CMD_QUEUE pQueue, PVBOXVDMADDI_CMD pCmd);

NTSTATUS vboxVdmaDdiCmdCompleted(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD_QUEUE pQueue, PVBOXVDMADDI_CMD pCmd);
NTSTATUS vboxVdmaDdiCmdSubmitted(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD_QUEUE pQueue, PVBOXVDMADDI_CMD pCmd);

DECLINLINE(VOID) vboxVdmaDdiCmdInit(PVBOXVDMADDI_CMD pCmd,
        uint32_t u32FenceId, PVBOXWDDM_CONTEXT pContext,
        PFNVBOXVDMADDICMDCOMPLETE_DPC pfnComplete, PVOID pvComplete)
{
    pCmd->QueueEntry.Blink = NULL;
    pCmd->QueueEntry.Flink = NULL;
    pCmd->enmState = VBOXVDMADDI_STATE_NOT_QUEUED;
    pCmd->u32FenceId = u32FenceId;
    pCmd->pContext = pContext;
    pCmd->pfnComplete = pfnComplete;
    pCmd->pvComplete = pvComplete;
}

NTSTATUS vboxVdmaDdiCmdFenceComplete(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_CONTEXT pContext, uint32_t u32FenceId);

DECLCALLBACK(VOID) vboxVdmaDdiCmdCompletionCbFree(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext);

DECLINLINE(VOID) vboxVdmaDdiCmdGetCompletedListIsr(PVBOXVDMADDI_CMD_QUEUE pQueue, LIST_ENTRY *pList)
{
    vboxVideoLeDetach(&pQueue->DpcCmdQueue, pList);
}

#define VBOXVDMADDI_CMD_FROM_ENTRY(_pEntry) ((PVBOXVDMADDI_CMD)(((uint8_t*)(_pEntry)) - RT_OFFSETOF(VBOXVDMADDI_CMD, QueueEntry)))

DECLINLINE(VOID) vboxVdmaDdiCmdHandleCompletedList(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD_QUEUE pQueue, LIST_ENTRY *pList)
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

#ifdef VBOXWDDM_RENDER_FROM_SHADOW
NTSTATUS vboxVdmaHlpUpdatePrimary(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, RECT* pRect);
#endif

#if 0
typedef DECLCALLBACK(int) FNVBOXVDMASUBMIT(struct _DEVICE_EXTENSION* pDevExt, struct VBOXVDMAINFO * pInfo, HGSMIOFFSET offDr, PVOID pvContext);
typedef FNVBOXVDMASUBMIT *PFNVBOXVDMASUBMIT;

typedef struct VBOXVDMASUBMIT
{
    PFNVBOXVDMASUBMIT pfnSubmit;
    PVOID pvContext;
} VBOXVDMASUBMIT, *PVBOXVDMASUBMIT;
#endif

/* start */
typedef enum
{
    VBOXVDMAPIPE_STATE_CLOSED    = 0,
    VBOXVDMAPIPE_STATE_CREATED   = 1,
    VBOXVDMAPIPE_STATE_OPENNED   = 2,
    VBOXVDMAPIPE_STATE_CLOSING   = 3
} VBOXVDMAPIPE_STATE;

typedef struct VBOXVDMAPIPE
{
    KSPIN_LOCK SinchLock;
    KEVENT Event;
    LIST_ENTRY CmdListHead;
    VBOXVDMAPIPE_STATE enmState;
    /* true iff the other end needs Event notification */
    bool bNeedNotify;
} VBOXVDMAPIPE, *PVBOXVDMAPIPE;

typedef struct VBOXVDMAPIPE_CMD_HDR
{
    LIST_ENTRY ListEntry;
} VBOXVDMAPIPE_CMD_HDR, *PVBOXVDMAPIPE_CMD_HDR;

#define VBOXVDMAPIPE_CMD_HDR_FROM_ENTRY(_pE)  ( (PVBOXVDMAPIPE_CMD_HDR)((uint8_t *)(_pE) - RT_OFFSETOF(VBOXVDMAPIPE_CMD_HDR, ListEntry)) )

typedef enum
{
    VBOXVDMAPIPE_CMD_TYPE_UNDEFINED = 0,
    VBOXVDMAPIPE_CMD_TYPE_RECTSINFO,
    VBOXVDMAPIPE_CMD_TYPE_DMACMD
} VBOXVDMAPIPE_CMD_TYPE;

typedef struct VBOXVDMAPIPE_CMD_DR
{
    VBOXVDMAPIPE_CMD_HDR PipeHdr;
    VBOXVDMAPIPE_CMD_TYPE enmType;
} VBOXVDMAPIPE_CMD_DR, *PVBOXVDMAPIPE_CMD_DR;

#define VBOXVDMAPIPE_CMD_DR_FROM_ENTRY(_pE)  ( (PVBOXVDMAPIPE_CMD_DR)VBOXVDMAPIPE_CMD_HDR_FROM_ENTRY(_pE) )

typedef struct VBOXWDDM_DMA_ALLOCINFO
{
    PVBOXWDDM_ALLOCATION pAlloc;
    VBOXVIDEOOFFSET offAlloc;
    UINT segmentIdAlloc;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId;
} VBOXWDDM_DMA_ALLOCINFO, *PVBOXWDDM_DMA_ALLOCINFO;

typedef struct VBOXVDMAPIPE_RECTS
{
    RECT ContextRect;
    VBOXWDDM_RECTS_INFO UpdateRects;
} VBOXVDMAPIPE_RECTS, *PVBOXVDMAPIPE_RECTS;

typedef struct VBOXVDMAPIPE_CMD_RECTSINFO
{
    VBOXVDMAPIPE_CMD_DR Hdr;
    VBOXVDMADDI_CMD DdiCmd;
    struct VBOXWDDM_SWAPCHAIN *pSwapchain;
    VBOXVDMAPIPE_RECTS ContextsRects;
} VBOXVDMAPIPE_CMD_RECTSINFO, *PVBOXVDMAPIPE_CMD_RECTSINFO;

typedef struct VBOXVDMAPIPE_FLAGS_DMACMD
{
    union
    {
        struct
        {
            UINT b2DRelated     : 1;
            UINT b3DRelated     : 1;
            UINT bDecVBVAUnlock : 1;
            UINT Reserved       : 29;
        };
        UINT Value;
    };
} VBOXVDMAPIPE_FLAGS_DMACMD, *PVBOXVDMAPIPE_FLAGS_DMACMD;
typedef struct VBOXVDMAPIPE_CMD_DMACMD
{
    VBOXVDMAPIPE_CMD_DR Hdr;
    VBOXVDMADDI_CMD DdiCmd;
    PDEVICE_EXTENSION pDevExt;
    VBOXVDMACMD_TYPE enmCmd;
    VBOXVDMAPIPE_FLAGS_DMACMD fFlags;
} VBOXVDMAPIPE_CMD_DMACMD, *PVBOXVDMAPIPE_CMD_DMACMD;

typedef struct VBOXVDMA_CLRFILL
{
    VBOXWDDM_DMA_ALLOCINFO Alloc;
    UINT Color;
    VBOXWDDM_RECTS_INFO Rects;
} VBOXVDMA_CLRFILL, *PVBOXVDMA_CLRFILL;

typedef struct VBOXVDMAPIPE_CMD_DMACMD_CLRFILL
{
    VBOXVDMAPIPE_CMD_DMACMD Hdr;
    VBOXVDMA_CLRFILL ClrFill;
} VBOXVDMAPIPE_CMD_DMACMD_CLRFILL, *PVBOXVDMAPIPE_CMD_DMACMD_CLRFILL;

typedef struct VBOXVDMA_BLT
{
    VBOXWDDM_DMA_ALLOCINFO SrcAlloc;
    VBOXWDDM_DMA_ALLOCINFO DstAlloc;
    RECT SrcRect;
    VBOXVDMAPIPE_RECTS DstRects;
} VBOXVDMA_BLT, *PVBOXVDMA_BLT;

typedef struct VBOXVDMAPIPE_CMD_DMACMD_BLT
{
    VBOXVDMAPIPE_CMD_DMACMD Hdr;
    VBOXVDMA_BLT Blt;
} VBOXVDMAPIPE_CMD_DMACMD_BLT, *PVBOXVDMAPIPE_CMD_DMACMD_BLT;

typedef struct VBOXVDMA_FLIP
{
    VBOXWDDM_DMA_ALLOCINFO Alloc;
} VBOXVDMA_FLIP, *PVBOXVDMA_FLIP;

typedef struct VBOXVDMAPIPE_CMD_DMACMD_FLIP
{
    VBOXVDMAPIPE_CMD_DMACMD Hdr;
    VBOXVDMA_FLIP Flip;
} VBOXVDMAPIPE_CMD_DMACMD_FLIP, *PVBOXVDMAPIPE_CMD_DMACMD_FLIP;

typedef struct VBOXVDMA_SHADOW2PRIMARY
{
    VBOXWDDM_DMA_ALLOCINFO ShadowAlloc;
    RECT SrcRect;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
} VBOXVDMA_SHADOW2PRIMARY, *PVBOXVDMA_SHADOW2PRIMARY;

typedef struct VBOXVDMAGG
{
    VBOXVDMAPIPE CmdPipe;
    PKTHREAD pThread;
} VBOXVDMAGG, *PVBOXVDMAGG;

/* DMA commands are currently submitted over HGSMI */
typedef struct VBOXVDMAINFO
{
#ifdef VBOXVDMA
    HGSMIHEAP CmdHeap;
#endif
    UINT      uLastCompletedPagingBufferCmdFenceId;
    BOOL      fEnabled;
#if 0
    VBOXVDMASUBMIT Submitter;
#endif
    /* dma-related commands list processed on the guest w/o host part involvement (guest-guest commands) */
    VBOXVDMAGG DmaGg;
} VBOXVDMAINFO, *PVBOXVDMAINFO;

int vboxVdmaCreate (struct _DEVICE_EXTENSION* pDevExt, VBOXVDMAINFO *pInfo
#ifdef VBOXVDMA
        , ULONG offBuffer, ULONG cbBuffer
#endif
#if 0
        , PFNVBOXVDMASUBMIT pfnSubmit, PVOID pvContext
#endif
        );
int vboxVdmaDisable (struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo);
int vboxVdmaEnable (struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo);
int vboxVdmaDestroy (struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo);

#ifdef VBOXVDMA
int vboxVdmaFlush (struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo);
int vboxVdmaCBufDrSubmit (struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo, PVBOXVDMACBUF_DR pDr);
struct VBOXVDMACBUF_DR* vboxVdmaCBufDrCreate (PVBOXVDMAINFO pInfo, uint32_t cbTrailingData);
void vboxVdmaCBufDrFree (PVBOXVDMAINFO pInfo, struct VBOXVDMACBUF_DR* pDr);

#define VBOXVDMACBUF_DR_DATA_OFFSET() (sizeof (VBOXVDMACBUF_DR))
#define VBOXVDMACBUF_DR_SIZE(_cbData) (VBOXVDMACBUF_DR_DATA_OFFSET() + (_cbData))
#define VBOXVDMACBUF_DR_DATA(_pDr) ( ((uint8_t*)(_pDr)) + VBOXVDMACBUF_DR_DATA_OFFSET() )
#endif
NTSTATUS vboxVdmaGgCmdSubmit(PVBOXVDMAGG pVdma, PVBOXVDMAPIPE_CMD_DR pCmd);
PVBOXVDMAPIPE_CMD_DR vboxVdmaGgCmdCreate(PVBOXVDMAGG pVdma, VBOXVDMAPIPE_CMD_TYPE enmType, uint32_t cbCmd);
void vboxVdmaGgCmdDestroy(PVBOXVDMAPIPE_CMD_DR pDr);

#define VBOXVDMAPIPE_CMD_DR_FROM_DDI_CMD(_pCmd) ((PVBOXVDMAPIPE_CMD_DR)(((uint8_t*)(_pCmd)) - RT_OFFSETOF(VBOXVDMAPIPE_CMD_DR, DdiCmd)))
DECLCALLBACK(VOID) vboxVdmaGgDdiCmdDestroy(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext);

#endif /* #ifndef ___VBoxVideoVdma_h___ */
