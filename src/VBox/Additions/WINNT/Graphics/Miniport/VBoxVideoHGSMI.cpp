/* $Id$ */
/** @file
 * VirtualBox Video miniport driver for NT/2k/XP - HGSMI related functions.
 */

/*
 * Copyright (C) 2006-2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <string.h>

#include "VBoxVideo.h"
#include "Helper.h"

#include <iprt/asm.h>
#include <iprt/log.h>
#include <iprt/thread.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxGuest.h>
#include <VBox/VBoxVideo.h>

// #include <VBoxDisplay.h>

// #include "vboxioctl.h"

/** Send completion notification to the host for the command located at offset
 * @a offt into the host command buffer. */
void HGSMINotifyHostCmdComplete(PHGSMIHOSTCOMMANDCONTEXT pCtx, HGSMIOFFSET offt)
{
    VBoxVideoCmnPortWriteUlong(pCtx->port, offt);
}

/** Acknowlege an IRQ. */
void HGSMIClearIrq(PHGSMIHOSTCOMMANDCONTEXT pCtx)
{
    VBoxVideoCmnPortWriteUlong(pCtx->port, HGSMIOFFSET_VOID);
}

/** Inform the host that a command has been handled. */
static void HGSMIHostCmdComplete(PHGSMIHOSTCOMMANDCONTEXT pCtx, void * pvMem)
{
    HGSMIBUFFERHEADER *pHdr = HGSMIBufferHeaderFromData(pvMem);
    HGSMIOFFSET offMem = HGSMIPointerToOffset(&pCtx->areaCtx, pHdr);
    Assert(offMem != HGSMIOFFSET_VOID);
    if(offMem != HGSMIOFFSET_VOID)
    {
        HGSMINotifyHostCmdComplete(pCtx, offMem);
    }
}

/** Submit an incoming host command to the appropriate handler. */
static void hgsmiHostCmdProcess(PHGSMIHOSTCOMMANDCONTEXT pCtx,
                                HGSMIOFFSET offBuffer)
{
    int rc = HGSMIBufferProcess(&pCtx->areaCtx, &pCtx->channels, offBuffer);
    Assert(!RT_FAILURE(rc));
    if(RT_FAILURE(rc))
    {
        /* failure means the command was not submitted to the handler for some reason
         * it's our responsibility to notify its completion in this case */
        HGSMINotifyHostCmdComplete(pCtx, offBuffer);
    }
    /* if the cmd succeeded it's responsibility of the callback to complete it */
}

/** Get the next command from the host. */
static HGSMIOFFSET hgsmiGetHostBuffer(PHGSMIHOSTCOMMANDCONTEXT pCtx)
{
    return VBoxVideoCmnPortReadUlong(pCtx->port);
}

/** Get and handle the next command from the host. */
static void hgsmiHostCommandQueryProcess(PHGSMIHOSTCOMMANDCONTEXT pCtx)
{
    HGSMIOFFSET offset = hgsmiGetHostBuffer(pCtx);
    AssertReturnVoid(offset != HGSMIOFFSET_VOID);
    hgsmiHostCmdProcess(pCtx, offset);
}

/** Drain the host command queue. */
void hgsmiProcessHostCommandQueue(PHGSMIHOSTCOMMANDCONTEXT pCtx)
{
    while (pCtx->pfHostFlags->u32HostFlags & HGSMIHOSTFLAGS_COMMANDS_PENDING)
    {
        if (!ASMAtomicCmpXchgBool(&pCtx->fHostCmdProcessing, true, false))
            return;
        hgsmiHostCommandQueryProcess(pCtx);
        ASMAtomicWriteBool(&pCtx->fHostCmdProcessing, false);
    }
}

/** Detect whether HGSMI is supported by the host. */
bool VBoxHGSMIIsSupported (void)
{
    uint16_t DispiId;

    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ID);
    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_DATA, VBE_DISPI_ID_HGSMI);

    DispiId = VBoxVideoCmnPortReadUshort(VBE_DISPI_IOPORT_DATA);

    return (DispiId == VBE_DISPI_ID_HGSMI);
}


void* vboxHGSMIBufferAlloc(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                         HGSMISIZE cbData,
                         uint8_t u8Ch,
                         uint16_t u16Op)
{
#ifdef VBOX_WITH_WDDM
    /* @todo: add synchronization */
#endif
    return HGSMIHeapAlloc (&pCtx->heapCtx, cbData, u8Ch, u16Op);
}

void vboxHGSMIBufferFree(PHGSMIGUESTCOMMANDCONTEXT pCtx, void *pvBuffer)
{
#ifdef VBOX_WITH_WDDM
    /* @todo: add synchronization */
#endif
    HGSMIHeapFree (&pCtx->heapCtx, pvBuffer);
}

int vboxHGSMIBufferSubmit(PHGSMIGUESTCOMMANDCONTEXT pCtx, void *pvBuffer)
{
    /* Initialize the buffer and get the offset for port IO. */
    HGSMIOFFSET offBuffer = HGSMIHeapBufferOffset (&pCtx->heapCtx, pvBuffer);

    Assert(offBuffer != HGSMIOFFSET_VOID);
    if (offBuffer != HGSMIOFFSET_VOID)
    {
        /* Submit the buffer to the host. */
        VBoxVideoCmnPortWriteUlong(pCtx->port, offBuffer);
        return VINF_SUCCESS;
    }

    return VERR_INVALID_PARAMETER;
}


static int vboxQueryConfHGSMI(PHGSMIGUESTCOMMANDCONTEXT pCtx, uint32_t u32Index,
                              uint32_t *pulValue)
{
    int rc = VINF_SUCCESS;
    VBVACONF32 *p;
    LogFunc(("u32Index = %d\n", u32Index));

    /* Allocate the IO buffer. */
    p = (VBVACONF32 *)HGSMIHeapAlloc(&pCtx->heapCtx,
                                     sizeof(VBVACONF32), HGSMI_CH_VBVA,
                                     VBVA_QUERY_CONF32);
    if (p)
    {
        /* Prepare data to be sent to the host. */
        p->u32Index = u32Index;
        p->u32Value = 0;
        rc = vboxHGSMIBufferSubmit(pCtx, p);
        if (RT_SUCCESS(rc))
        {
            *pulValue = p->u32Value;
            LogFunc(("u32Value = %d\n", p->u32Value));
        }
        /* Free the IO buffer. */
        HGSMIHeapFree(&pCtx->heapCtx, p);
    }
    else
        rc = VERR_NO_MEMORY;
    LogFunc(("rc = %d\n", rc));
    return rc;
}


static int vboxHGSMIBufferLocation(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                                   HGSMIOFFSET offLocation)
{
    HGSMIBUFFERLOCATION *p;
    int rc = VINF_SUCCESS;

    /* Allocate the IO buffer. */
    p = (HGSMIBUFFERLOCATION *)HGSMIHeapAlloc(&pCtx->heapCtx,
                                              sizeof(HGSMIBUFFERLOCATION),
                                              HGSMI_CH_HGSMI,
                                              HGSMI_CC_HOST_FLAGS_LOCATION);
    if (p)
    {
        /* Prepare data to be sent to the host. */
        p->offLocation = offLocation;
        p->cbLocation  = sizeof(HGSMIHOSTFLAGS);
        rc = vboxHGSMIBufferSubmit(pCtx, p);
        /* Free the IO buffer. */
        HGSMIHeapFree (&pCtx->heapCtx, p);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


static int vboxHGSMISendCapsInfo(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                                 uint32_t fCaps)
{
    VBVACAPS *pCaps;
    int rc = VINF_SUCCESS;

    /* Allocate the IO buffer. */
    pCaps = (VBVACAPS *)HGSMIHeapAlloc(&pCtx->heapCtx,
                                       sizeof(VBVACAPS), HGSMI_CH_VBVA,
                                       VBVA_INFO_CAPS);

    if (pCaps)
    {
        /* Prepare data to be sent to the host. */
        pCaps->rc    = VERR_NOT_IMPLEMENTED;
        pCaps->fCaps = fCaps;
        rc = vboxHGSMIBufferSubmit(pCtx, pCaps);
        if (RT_SUCCESS(rc))
        {
            AssertRC(pCaps->rc);
            rc = pCaps->rc;
        }
        /* Free the IO buffer. */
        HGSMIHeapFree(&pCtx->heapCtx, pCaps);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


static int vboxVBVAInitInfoHeap(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                                uint32_t u32HeapOffset, uint32_t u32HeapSize)
{
    VBVAINFOHEAP *p;
    int rc = VINF_SUCCESS;

    /* Allocate the IO buffer. */
    p = (VBVAINFOHEAP *)HGSMIHeapAlloc(&pCtx->heapCtx,
                                       sizeof (VBVAINFOHEAP), HGSMI_CH_VBVA,
                                       VBVA_INFO_HEAP);
    if (p)
    {
        /* Prepare data to be sent to the host. */
        p->u32HeapOffset = u32HeapOffset;
        p->u32HeapSize   = u32HeapSize;
        rc = vboxHGSMIBufferSubmit(pCtx, p);
        /* Free the IO buffer. */
        HGSMIHeapFree(&pCtx->heapCtx, p);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


int VBoxHGSMISendViewInfo(PHGSMIGUESTCOMMANDCONTEXT pCtx, uint32_t u32Count,
                          PFNHGSMIFILLVIEWINFO pfnFill, void *pvData)
{
    int rc;
    /* Issue the screen info command. */
    void *p = vboxHGSMIBufferAlloc(pCtx, sizeof(VBVAINFOVIEW) * u32Count,
                                   HGSMI_CH_VBVA, VBVA_INFO_VIEW);
    if (p)
    {
        VBVAINFOVIEW *pInfo = (VBVAINFOVIEW *)p;
        rc = pfnFill(pvData, pInfo);
        if (RT_SUCCESS(rc))
            vboxHGSMIBufferSubmit (pCtx, p);
        vboxHGSMIBufferFree(pCtx, p);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


/**
 * Get the information needed to map the basic communication structures in
 * device memory into our address space.
 *
 * @param  cbVRAM               how much video RAM is allocated to the device
 * @param  poffVRAMBaseMapping  where to save the offset from the start of the
 *                              device VRAM of the whole area to map 
 * @param  pcbMapping           where to save the mapping size
 * @param  poffGuestHeapMemory  where to save the offset into the mapped area
 *                              of the guest heap backing memory
 * @param  pcbGuestHeapMemory   where to save the size of the guest heap
 *                              backing memory
 * @param  poffHostFlags        where to save the offset into the mapped area
 *                              of the host flags
 */
void vboxHGSMIGetBaseMappingInfo(uint32_t cbVRAM,
                                 uint32_t *poffVRAMBaseMapping,
                                 uint32_t *pcbMapping,
                                 uint32_t *poffGuestHeapMemory,
                                 uint32_t *pcbGuestHeapMemory,
                                 uint32_t *poffHostFlags)
{
    AssertPtrReturnVoid(poffVRAMBaseMapping);
    AssertPtrReturnVoid(pcbMapping);
    AssertPtrReturnVoid(poffGuestHeapMemory);
    AssertPtrReturnVoid(pcbGuestHeapMemory);
    AssertPtrReturnVoid(poffHostFlags);
    *poffVRAMBaseMapping = cbVRAM - VBVA_ADAPTER_INFORMATION_SIZE;
    *pcbMapping = VBVA_ADAPTER_INFORMATION_SIZE;
    *poffGuestHeapMemory = 0;
    *pcbGuestHeapMemory = VBVA_ADAPTER_INFORMATION_SIZE - sizeof(HGSMIHOSTFLAGS);
    *poffHostFlags = VBVA_ADAPTER_INFORMATION_SIZE - sizeof(HGSMIHOSTFLAGS);
}


/**
 * Set up the HGSMI guest-to-host command context.
 * @returns iprt status value
 * @param  pCtx                    the context to set up
 * @param  pvGuestHeapMemory       a pointer to the mapped backing memory for
 *                                 the guest heap
 * @param  cbGuestHeapMemory       the size of the backing memory area
 * @param  offVRAMGuestHeapMemory  the offset of the memory pointed to by
 *                                 @a pvGuestHeapMemory within the video RAM
 */
int vboxHGSMISetupGuestContext(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                               void *pvGuestHeapMemory,
                               uint32_t cbGuestHeapMemory,
                               uint32_t offVRAMGuestHeapMemory)
{
    /** @todo should we be using a fixed ISA port value here? */
    pCtx->port = (RTIOPORT)VGA_PORT_HGSMI_GUEST;
    return HGSMIHeapSetup(&pCtx->heapCtx, pvGuestHeapMemory,
                          cbGuestHeapMemory, offVRAMGuestHeapMemory,
                          false /*fOffsetBased*/);
}


/**
 * Get the information needed to map the area used by the host to send back
 * requests.
 *
 * @param  pCtx                the guest context used to query the host
 * @param  cbVRAM              how much video RAM is allocated to the device
 * @param  offVRAMBaseMapping  the offset of the basic communication structures
 *                             into the guest's VRAM
 * @param  poffVRAMHostArea    where to store the offset of the host area into
 *                             the guest's VRAM
 * @param  pcbHostArea         where to store the size of the host area
 */
void vboxHGSMIGetHostAreaMapping(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                                 uint32_t cbVRAM, uint32_t offVRAMBaseMapping,
                                 uint32_t *poffVRAMHostArea,
                                 uint32_t *pcbHostArea)
{
    uint32_t offVRAMHostArea = offVRAMBaseMapping, cbHostArea = 0;

    AssertPtrReturnVoid(poffVRAMHostArea);
    AssertPtrReturnVoid(pcbHostArea);
    vboxQueryConfHGSMI(pCtx, VBOX_VBVA_CONF32_HOST_HEAP_SIZE, &cbHostArea);
    if (cbHostArea != 0)
    {
        uint32_t cbHostAreaMaxSize = cbVRAM / 4;
        /** @todo what is the idea of this? */
        if (cbHostAreaMaxSize >= VBVA_ADAPTER_INFORMATION_SIZE)
        {
            cbHostAreaMaxSize -= VBVA_ADAPTER_INFORMATION_SIZE;
        }
        if (cbHostArea > cbHostAreaMaxSize)
        {
            cbHostArea = cbHostAreaMaxSize;
        }
        /* Round up to 4096 bytes. */
        cbHostArea = (cbHostArea + 0xFFF) & ~0xFFF;
        offVRAMHostArea = offVRAMBaseMapping - cbHostArea;
    }

    *pcbHostArea = cbHostArea;
    *poffVRAMHostArea = offVRAMHostArea;
    LogFunc(("offVRAMHostArea = 0x%08X, cbHostArea = 0x%08X\n",
             offVRAMHostArea, cbHostArea));
}


/** Initialise the host context structure. */
void vboxHGSMISetupHostContext(PHGSMIHOSTCOMMANDCONTEXT pCtx,
                               void *pvBaseMapping, uint32_t offHostFlags,
                               void *pvHostAreaMapping,
                               uint32_t offVRAMHostArea, uint32_t cbHostArea)
{
    uint8_t *pu8HostFlags = ((uint8_t *)pvBaseMapping) + offHostFlags;
    pCtx->pfHostFlags = (HGSMIHOSTFLAGS *)pu8HostFlags;
    /** @todo should we really be using a fixed ISA port value here? */
    pCtx->port        = (RTIOPORT)VGA_PORT_HGSMI_HOST;
    HGSMIAreaInitialize(&pCtx->areaCtx, pvHostAreaMapping, cbHostArea,
                         offVRAMHostArea);
}


/**
 * Mirror the information in the host context structure to the host.
 */
static int vboxHGSMISendHostCtxInfo(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                                    HGSMIOFFSET offBufferLocation,
                                    uint32_t fCaps, uint32_t u32HeapOffset,
                                    uint32_t u32HeapSize)
{
    Log(("VBoxVideo::vboxSetupAdapterInfo\n"));

    /* setup the flags first to ensure they are initialized by the time the
     * host heap is ready */
    int rc = vboxHGSMIBufferLocation(pCtx, offBufferLocation);
    AssertRC(rc);
    if (RT_SUCCESS(rc) && fCaps)
    {
        /* Inform about caps */
        rc = vboxHGSMISendCapsInfo(pCtx, fCaps);
        AssertRC(rc);
    }
    if (RT_SUCCESS (rc))
    {
        /* Report the host heap location. */
        rc = vboxVBVAInitInfoHeap(pCtx, u32HeapOffset, u32HeapSize);
        AssertRC(rc);
    }
    Log(("VBoxVideo::vboxSetupAdapterInfo finished rc = %d\n", rc));
    return rc;
}


/**
 * Returns the count of virtual monitors attached to the guest.  Returns
 * 1 on failure.
 */
unsigned vboxHGSMIGetMonitorCount(PHGSMIGUESTCOMMANDCONTEXT pCtx)
{
    /* Query the configured number of displays. */
    uint32_t cDisplays = 0;
    vboxQueryConfHGSMI(pCtx, VBOX_VBVA_CONF32_MONITOR_COUNT, &cDisplays);
    LogFunc(("cDisplays = %d\n", cDisplays));
    if (cDisplays == 0 || cDisplays > VBOX_VIDEO_MAX_SCREENS)
        /* Host reported some bad value. Continue in the 1 screen mode. */
        cDisplays = 1;
    return cDisplays;
}


/**
 * Helper function to register secondary displays (DualView). Note that this will not
 * be available on pre-XP versions, and some editions on XP will fail because they are
 * intentionally crippled.
 *
 * HGSMI variant is a bit different because it uses only HGSMI interface (VBVA channel)
 * to talk to the host.
 */
void VBoxSetupDisplaysHGSMI(PVBOXVIDEO_COMMON pCommon,
                            uint32_t AdapterMemorySize, uint32_t fCaps)
{
    /** @todo I simply converted this from Windows error codes.  That is wrong,
     * but we currently freely mix and match those (failure == rc > 0) and iprt
     * ones (failure == rc < 0) anyway.  This needs to be fully reviewed and
     * fixed. */
    int rc = VINF_SUCCESS;
    uint32_t offVRAMBaseMapping, cbMapping, offGuestHeapMemory, cbGuestHeapMemory,
             offHostFlags, offVRAMHostArea, cbHostArea;
    Log(("VBoxVideo::VBoxSetupDisplays: pCommon = %p\n", pCommon));

    memset(pCommon, 0, sizeof(*pCommon));
    pCommon->cbVRAM    = AdapterMemorySize;
    pCommon->cDisplays = 1;
    pCommon->bHGSMI    = VBoxHGSMIIsSupported ();
    /* Why does this use VBoxVideoCmnMemZero?  The MSDN docs say that it should
     * only be used on mapped display adapter memory.  Done with memset above. */
    // VBoxVideoCmnMemZero(&pCommon->areaHostHeap, sizeof(HGSMIAREA));
    if (pCommon->bHGSMI)
    {
        vboxHGSMIGetBaseMappingInfo(pCommon->cbVRAM, &offVRAMBaseMapping,
                                    &cbMapping, &offGuestHeapMemory,
                                    &cbGuestHeapMemory, &offHostFlags);

        /* Map the adapter information. It will be needed for HGSMI IO. */
        /** @todo all callers of VBoxMapAdapterMemory expect it to use iprt
         * error codes, but it doesn't. */
        rc = VBoxMapAdapterMemory (pCommon, &pCommon->pvAdapterInformation,
                                   offVRAMBaseMapping, cbMapping);
        if (RT_FAILURE(rc))
        {
            Log(("VBoxVideo::VBoxSetupDisplays: VBoxMapAdapterMemory pvAdapterInfoirrmation failed rc = %d\n",
                     rc));

            pCommon->bHGSMI = false;
        }
        else
        {
            /* Setup an HGSMI heap within the adapter information area. */
            rc = vboxHGSMISetupGuestContext(&pCommon->guestCtx,
                                            pCommon->pvAdapterInformation,
                                            cbGuestHeapMemory,
                                              offVRAMBaseMapping
                                            + offGuestHeapMemory);

            if (RT_FAILURE(rc))
            {
                Log(("VBoxVideo::VBoxSetupDisplays: HGSMIHeapSetup failed rc = %d\n",
                         rc));

                pCommon->bHGSMI = false;
            }
        }
    }

    /* Setup the host heap and the adapter memory. */
    if (pCommon->bHGSMI)
    {
        vboxHGSMIGetHostAreaMapping(&pCommon->guestCtx, pCommon->cbVRAM,
                                    offVRAMBaseMapping, &offVRAMHostArea,
                                    &cbHostArea);
        if (cbHostArea)
        {

            /* Map the heap region.
             *
             * Note: the heap will be used for the host buffers submitted to the guest.
             *       The miniport driver is responsible for reading FIFO and notifying
             *       display drivers.
             */
            pCommon->cbMiniportHeap = cbHostArea;
            rc = VBoxMapAdapterMemory (pCommon, &pCommon->pvMiniportHeap,
                                       offVRAMHostArea, cbHostArea);
            if (RT_FAILURE(rc))
            {
                pCommon->pvMiniportHeap = NULL;
                pCommon->cbMiniportHeap = 0;
                pCommon->bHGSMI = false;
            }
            else
                vboxHGSMISetupHostContext(&pCommon->hostCtx,
                                          pCommon->pvAdapterInformation,
                                          offHostFlags,
                                          pCommon->pvMiniportHeap,
                                          offVRAMHostArea, cbHostArea);
        }
        else
        {
            /* Host has not requested a heap. */
            pCommon->pvMiniportHeap = NULL;
            pCommon->cbMiniportHeap = 0;
        }
    }

    if (pCommon->bHGSMI)
    {
        /* Setup the information for the host. */
        rc = vboxHGSMISendHostCtxInfo(&pCommon->guestCtx,
                                      offVRAMBaseMapping + offHostFlags,
                                      fCaps, offVRAMHostArea,
                                      pCommon->cbMiniportHeap);

        if (RT_FAILURE(rc))
        {
            pCommon->bHGSMI = false;
        }
    }

    /* Check whether the guest supports multimonitors. */
    if (pCommon->bHGSMI)
        /* Query the configured number of displays. */
        pCommon->cDisplays = vboxHGSMIGetMonitorCount(&pCommon->guestCtx);

    if (!pCommon->bHGSMI)
        VBoxFreeDisplaysHGSMI(pCommon);

    Log(("VBoxVideo::VBoxSetupDisplays: finished\n"));
}

static bool VBoxUnmapAdpInfoCallback(void *pvCommon)
{
    PVBOXVIDEO_COMMON pCommon = (PVBOXVIDEO_COMMON)pvCommon;

    pCommon->hostCtx.pfHostFlags = NULL;
    return true;
}

void VBoxFreeDisplaysHGSMI(PVBOXVIDEO_COMMON pCommon)
{
    VBoxUnmapAdapterMemory(pCommon, &pCommon->pvMiniportHeap);
    HGSMIHeapDestroy(&pCommon->guestCtx.heapCtx);

    /* Unmap the adapter information needed for HGSMI IO. */
    VBoxSyncToVideoIRQ(pCommon, VBoxUnmapAdpInfoCallback, pCommon);
    VBoxUnmapAdapterMemory(pCommon, &pCommon->pvAdapterInformation);
}


bool vboxUpdatePointerShape(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                            uint32_t fFlags,
                            uint32_t cHotX,
                            uint32_t cHotY,
                            uint32_t cWidth,
                            uint32_t cHeight,
                            uint8_t *pPixels,
                            uint32_t cbLength)
{
    VBVAMOUSEPOINTERSHAPE *p;
    uint32_t cbData = 0;
    int rc = VINF_SUCCESS;

    if (fFlags & VBOX_MOUSE_POINTER_SHAPE)
    {
        /* Size of the pointer data: sizeof (AND mask) + sizeof (XOR_MASK) */
        cbData = ((((cWidth + 7) / 8) * cHeight + 3) & ~3)
                 + cWidth * 4 * cHeight;
        /* If shape is supplied, then always create the pointer visible.
         * See comments in 'vboxUpdatePointerShape'
         */
        fFlags |= VBOX_MOUSE_POINTER_VISIBLE;
    }
#ifndef DEBUG_misha
    LogFunc(("cbData %d, %dx%d\n", cbData, cWidth, cHeight));
#endif
    if (cbData > cbLength)
    {
        LogFunc(("calculated pointer data size is too big (%d bytes, limit %d)\n",
                 cbData, cbLength));
        return false;
    }
    /* Allocate the IO buffer. */
    p = (VBVAMOUSEPOINTERSHAPE *)HGSMIHeapAlloc(&pCtx->heapCtx,
                                                  sizeof(VBVAMOUSEPOINTERSHAPE)
                                                + cbData,
                                                HGSMI_CH_VBVA,
                                                VBVA_MOUSE_POINTER_SHAPE);
    if (p)
    {
        /* Prepare data to be sent to the host. */
        /* Will be updated by the host. */
        p->i32Result = VINF_SUCCESS;
        /* We have our custom flags in the field */
        p->fu32Flags = fFlags;
        p->u32HotX   = cHotX;
        p->u32HotY   = cHotY;
        p->u32Width  = cWidth;
        p->u32Height = cHeight;
        if (p->fu32Flags & VBOX_MOUSE_POINTER_SHAPE)
            /* Copy the actual pointer data. */
            memcpy (p->au8Data, pPixels, cbData);
        rc = vboxHGSMIBufferSubmit(pCtx, p);
        if (RT_SUCCESS(rc))
            rc = p->i32Result;
        /* Free the IO buffer. */
        HGSMIHeapFree(&pCtx->heapCtx, p);
    }
    else
        rc = VERR_NO_MEMORY;
#ifndef DEBUG_misha
    LogFunc(("rc %d\n", rc));
#endif
    return RT_SUCCESS(rc);
}

#ifndef VBOX_WITH_WDDM
typedef struct _VBVAMINIPORT_CHANNELCONTEXT
{
    PFNHGSMICHANNELHANDLER pfnChannelHandler;
    void *pvChannelHandler;
}VBVAMINIPORT_CHANNELCONTEXT;

typedef struct _VBVADISP_CHANNELCONTEXT
{
    /** The generic command handler builds up a list of commands - in reverse
     * order! - here */
    VBVAHOSTCMD *pCmd;
    bool bValid;
}VBVADISP_CHANNELCONTEXT;

typedef struct _VBVA_CHANNELCONTEXTS
{
    PVBOXVIDEO_COMMON pCommon;
    uint32_t cUsed;
    uint32_t cContexts;
    VBVAMINIPORT_CHANNELCONTEXT mpContext;
    VBVADISP_CHANNELCONTEXT aContexts[1];
}VBVA_CHANNELCONTEXTS;

static int vboxVBVADeleteChannelContexts(PVBOXVIDEO_COMMON pCommon,
                                         VBVA_CHANNELCONTEXTS * pContext)
{
    VBoxVideoCmnMemFreeDriver(pCommon, pContext);
    return VINF_SUCCESS;
}

static int vboxVBVACreateChannelContexts(PVBOXVIDEO_COMMON pCommon, VBVA_CHANNELCONTEXTS ** ppContext)
{
    uint32_t cDisplays = (uint32_t)pCommon->cDisplays;
    const size_t size = RT_OFFSETOF(VBVA_CHANNELCONTEXTS, aContexts[cDisplays]);
    VBVA_CHANNELCONTEXTS * pContext = (VBVA_CHANNELCONTEXTS*)VBoxVideoCmnMemAllocDriver(pCommon, size);
    if(pContext)
    {
        memset(pContext, 0, size);
        pContext->cContexts = cDisplays;
        pContext->pCommon = pCommon;
        *ppContext = pContext;
        return VINF_SUCCESS;
    }
    return VERR_GENERAL_FAILURE;
}

static VBVADISP_CHANNELCONTEXT* vboxVBVAFindHandlerInfo(VBVA_CHANNELCONTEXTS *pCallbacks, int iId)
{
    if(iId < 0)
    {
        return NULL;
    }
    else if(pCallbacks->cContexts > (uint32_t)iId)
    {
        return &pCallbacks->aContexts[iId];
    }
    return NULL;
}

DECLCALLBACK(void) hgsmiHostCmdComplete (HVBOXVIDEOHGSMI hHGSMI, struct _VBVAHOSTCMD * pCmd)
{
    PHGSMIHOSTCOMMANDCONTEXT pCtx = &((PVBOXVIDEO_COMMON)hHGSMI)->hostCtx;
    HGSMIHostCmdComplete(pCtx, pCmd);
}

/** Reverses a NULL-terminated linked list of VBVAHOSTCMD structures. */
static VBVAHOSTCMD *vboxVBVAReverseList(VBVAHOSTCMD *pList)
{
    VBVAHOSTCMD *pFirst = NULL;
    while (pList)
    {
        VBVAHOSTCMD *pNext = pList;
        pList = pList->u.pNext;
        pNext->u.pNext = pFirst;
        pFirst = pNext;
    }
    return pFirst;
}

DECLCALLBACK(int) hgsmiHostCmdRequest (HVBOXVIDEOHGSMI hHGSMI, uint8_t u8Channel, uint32_t iDevice, struct _VBVAHOSTCMD ** ppCmd)
{
//    if(display < 0)
//        return VERR_INVALID_PARAMETER;
    if(!ppCmd)
        return VERR_INVALID_PARAMETER;

    PHGSMIHOSTCOMMANDCONTEXT pCtx = &((PVBOXVIDEO_COMMON)hHGSMI)->hostCtx;

    /* pick up the host commands */
    hgsmiProcessHostCommandQueue(pCtx);

    HGSMICHANNEL *pChannel = HGSMIChannelFindById (&pCtx->channels, u8Channel);
    if(pChannel)
    {
        VBVA_CHANNELCONTEXTS * pContexts = (VBVA_CHANNELCONTEXTS *)pChannel->handler.pvHandler;
        VBVADISP_CHANNELCONTEXT *pDispContext = vboxVBVAFindHandlerInfo(pContexts, iDevice);
        Assert(pDispContext);
        if(pDispContext)
        {
            VBVAHOSTCMD *pCmd;
            do
                pCmd = ASMAtomicReadPtrT(&pDispContext->pCmd, VBVAHOSTCMD *);
            while (!ASMAtomicCmpXchgPtr(&pDispContext->pCmd, NULL, pCmd));
            *ppCmd = vboxVBVAReverseList(pCmd);

            return VINF_SUCCESS;
        }
    }

    return VERR_INVALID_PARAMETER;
}


static DECLCALLBACK(int) vboxVBVAChannelGenericHandler(void *pvHandler, uint16_t u16ChannelInfo, void *pvBuffer, HGSMISIZE cbBuffer)
{
    VBVA_CHANNELCONTEXTS *pCallbacks = (VBVA_CHANNELCONTEXTS*)pvHandler;
//    Assert(0);
    Assert(cbBuffer > VBVAHOSTCMD_HDRSIZE);
    if(cbBuffer > VBVAHOSTCMD_HDRSIZE)
    {
        VBVAHOSTCMD *pHdr = (VBVAHOSTCMD*)pvBuffer;
        Assert(pHdr->iDstID >= 0);
        if(pHdr->iDstID >= 0)
        {
            VBVADISP_CHANNELCONTEXT* pHandler = vboxVBVAFindHandlerInfo(pCallbacks, pHdr->iDstID);
            Assert(pHandler && pHandler->bValid);
            if(pHandler && pHandler->bValid)
            {
                VBVAHOSTCMD *pFirst = NULL, *pLast = NULL;
                for(VBVAHOSTCMD *pCur = pHdr; pCur; )
                {
                    Assert(!pCur->u.Data);
                    Assert(!pFirst);
                    Assert(!pLast);

                    switch(u16ChannelInfo)
                    {
                        case VBVAHG_DISPLAY_CUSTOM:
                        {
#if 0  /* Never taken */
                            if(pLast)
                            {
                                pLast->u.pNext = pCur;
                                pLast = pCur;
                            }
                            else
#endif
                            {
                                pFirst = pCur;
                                pLast = pCur;
                            }
                            Assert(!pCur->u.Data);
#if 0  /* Who is supposed to set pNext? */
                            //TODO: use offset here
                            pCur = pCur->u.pNext;
                            Assert(!pCur);
#else
                            Assert(!pCur->u.pNext);
                            pCur = NULL;
#endif
                            Assert(pFirst);
                            Assert(pFirst == pLast);
                            break;
                        }
                        case VBVAHG_EVENT:
                        {
                            VBVAHOSTCMDEVENT *pEventCmd = VBVAHOSTCMD_BODY(pCur, VBVAHOSTCMDEVENT);
                            VBoxVideoCmnSignalEvent(pCallbacks->pCommon, pEventCmd->pEvent);
                        }
                        default:
                        {
                            Assert(u16ChannelInfo==VBVAHG_EVENT);
                            Assert(!pCur->u.Data);
#if 0  /* pLast has been asserted to be NULL, and who should set pNext? */
                            //TODO: use offset here
                            if(pLast)
                                pLast->u.pNext = pCur->u.pNext;
                            VBVAHOSTCMD * pNext = pCur->u.pNext;
                            pCur->u.pNext = NULL;
#else
                            Assert(!pCur->u.pNext);
#endif
                            HGSMIHostCmdComplete(&pCallbacks->pCommon->hostCtx, pCur);
#if 0  /* pNext is NULL, and the other things have already been asserted */
                            pCur = pNext;
                            Assert(!pCur);
                            Assert(!pFirst);
                            Assert(pFirst == pLast);
#else
                            pCur = NULL;
#endif
                            break;
                        }
                    }
                }

                /* we do not support lists currently */
                Assert(pFirst == pLast);
                if(pLast)
                {
                    Assert(pLast->u.pNext == NULL);
                }

                if(pFirst)
                {
                    Assert(pLast);
                    VBVAHOSTCMD *pCmd;
                    do
                    {
                        pCmd = ASMAtomicReadPtrT(&pHandler->pCmd, VBVAHOSTCMD *);
                        pFirst->u.pNext = pCmd;
                    }
                    while (!ASMAtomicCmpXchgPtr(&pHandler->pCmd, pFirst, pCmd));
                }
                else
                {
                    Assert(!pLast);
                }
                return VINF_SUCCESS;
            }
        }
        else
        {
            //TODO: impl
//          HGSMIMINIPORT_CHANNELCONTEXT *pHandler = vboxVideoHGSMIFindHandler;
//           if(pHandler && pHandler->pfnChannelHandler)
//           {
//               pHandler->pfnChannelHandler(pHandler->pvChannelHandler, u16ChannelInfo, pHdr, cbBuffer);
//
//               return VINF_SUCCESS;
//           }
        }
    }
    /* no handlers were found, need to complete the command here */
    HGSMIHostCmdComplete(&pCallbacks->pCommon->hostCtx, pvBuffer);
    return VINF_SUCCESS;
}

static HGSMICHANNELHANDLER g_OldHandler;

int vboxVBVAChannelDisplayEnable(PVBOXVIDEO_COMMON pCommon,
        int iDisplay, /* negative would mean this is a miniport handler */
        uint8_t u8Channel)
{
    VBVA_CHANNELCONTEXTS * pContexts;
    HGSMICHANNEL * pChannel = HGSMIChannelFindById (&pCommon->hostCtx.channels, u8Channel);
    if(!pChannel)
    {
        int rc = vboxVBVACreateChannelContexts(pCommon, &pContexts);
        if(RT_FAILURE(rc))
        {
            return rc;
        }
    }
    else
    {
        pContexts = (VBVA_CHANNELCONTEXTS *)pChannel->handler.pvHandler;
    }

    VBVADISP_CHANNELCONTEXT *pDispContext = vboxVBVAFindHandlerInfo(pContexts, iDisplay);
    Assert(pDispContext);
    if(pDispContext)
    {
#ifdef DEBUGVHWASTRICT
        Assert(!pDispContext->bValid);
#endif
        Assert(!pDispContext->pCmd);
        if(!pDispContext->bValid)
        {
            pDispContext->bValid = true;
            pDispContext->pCmd = NULL;

            int rc = VINF_SUCCESS;
            if(!pChannel)
            {
                rc = HGSMIChannelRegister (&pCommon->hostCtx.channels,
                                           u8Channel,
                                           "VGA Miniport HGSMI channel",
                                           vboxVBVAChannelGenericHandler,
                                           pContexts,
                                           &g_OldHandler);
            }

            if(RT_SUCCESS(rc))
            {
                pContexts->cUsed++;
                return VINF_SUCCESS;
            }
        }
    }

    if(!pChannel)
    {
        vboxVBVADeleteChannelContexts(pCommon, pContexts);
    }

    return VERR_GENERAL_FAILURE;
}
#endif /* !VBOX_WITH_WDDM */

/** @todo Mouse pointer position to be read from VMMDev memory, address of the memory region
 * can be queried from VMMDev via an IOCTL. This VMMDev memory region will contain
 * host information which is needed by the guest.
 *
 * Reading will not cause a switch to the host.
 *
 * Have to take into account:
 *  * synchronization: host must write to the memory only from EMT,
 *    large structures must be read under flag, which tells the host
 *    that the guest is currently reading the memory (OWNER flag?).
 *  * guest writes: may be allocate a page for the host info and make
 *    the page readonly for the guest.
 *  * the information should be available only for additions drivers.
 *  * VMMDev additions driver will inform the host which version of the info it expects,
 *    host must support all versions.
 *
 */

