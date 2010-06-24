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
#include <VBox/VBoxVideo.h>
#include "../VBoxVideo.h"

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
    VBOXVDMAPIPE_CMD_TYPE_RECTSINFO = 1,
    VBOXVDMAPIPE_CMD_TYPE_DMACMD    = 2
} VBOXVDMAPIPE_CMD_TYPE;

typedef struct VBOXVDMAPIPE_CMD_DR
{
    VBOXVDMAPIPE_CMD_HDR PipeHdr;
    VBOXVDMAPIPE_CMD_TYPE enmType;
} VBOXVDMAPIPE_CMD_DR, *PVBOXVDMAPIPE_CMD_DR;

#define VBOXVDMAPIPE_CMD_DR_FROM_ENTRY(_pE)  ( (PVBOXVDMAPIPE_CMD_DR)VBOXVDMAPIPE_CMD_HDR_FROM_ENTRY(_pE) )

typedef struct VBOXVDMAPIPE_CMD_RECTSINFO
{
    VBOXVDMAPIPE_CMD_DR Hdr;
    PVBOXWDDM_CONTEXT pContext;
    RECT ContextRect;
    VBOXWDDM_RECTS_INFO UpdateRects;
} VBOXVDMAPIPE_CMD_RECTSINFO, *PVBOXVDMAPIPE_CMD_RECTSINFO;

typedef struct VBOXVDMAGG
{
    VBOXVDMAPIPE CmdPipe;
    PKTHREAD pThread;
} VBOXVDMAGG, *PVBOXVDMAGG;

/* DMA commands are currently submitted over HGSMI */
typedef struct VBOXVDMAINFO
{
    HGSMIHEAP CmdHeap;
    UINT      uLastCompletedPagingBufferCmdFenceId;
    BOOL      fEnabled;
#if 0
    VBOXVDMASUBMIT Submitter;
#endif
    /* dma-related commands list processed on the guest w/o host part involvement (guest-guest commands) */
    VBOXVDMAGG DmaGg;
} VBOXVDMAINFO, *PVBOXVDMAINFO;

int vboxVdmaCreate (struct _DEVICE_EXTENSION* pDevExt, VBOXVDMAINFO *pInfo, ULONG offBuffer, ULONG cbBuffer
#if 0
        , PFNVBOXVDMASUBMIT pfnSubmit, PVOID pvContext
#endif
        );
int vboxVdmaDisable (struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo);
int vboxVdmaEnable (struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo);
int vboxVdmaFlush (struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo);
int vboxVdmaDestroy (struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo);
int vboxVdmaCBufDrSubmit (struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo, PVBOXVDMACBUF_DR pDr);
struct VBOXVDMACBUF_DR* vboxVdmaCBufDrCreate (PVBOXVDMAINFO pInfo, uint32_t cbTrailingData);
void vboxVdmaCBufDrFree (PVBOXVDMAINFO pInfo, struct VBOXVDMACBUF_DR* pDr);

#define VBOXVDMACBUF_DR_DATA_OFFSET() (sizeof (VBOXVDMACBUF_DR))
#define VBOXVDMACBUF_DR_SIZE(_cbData) (VBOXVDMACBUF_DR_DATA_OFFSET() + (_cbData))
#define VBOXVDMACBUF_DR_DATA(_pDr) ( ((uint8_t*)(_pDr)) + VBOXVDMACBUF_DR_DATA_OFFSET() )
#endif /* #ifndef ___VBoxVideoVdma_h___ */
