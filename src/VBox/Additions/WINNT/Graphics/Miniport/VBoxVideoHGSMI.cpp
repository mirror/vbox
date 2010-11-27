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
        VBoxHGSMIGetBaseMappingInfo(pCommon->cbVRAM, &offVRAMBaseMapping,
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
            rc = VBoxHGSMISetupGuestContext(&pCommon->guestCtx,
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
        VBoxHGSMIGetHostAreaMapping(&pCommon->guestCtx, pCommon->cbVRAM,
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
                VBoxHGSMISetupHostContext(&pCommon->hostCtx,
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
        rc = VBoxHGSMISendHostCtxInfo(&pCommon->guestCtx,
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
        pCommon->cDisplays = VBoxHGSMIGetMonitorCount(&pCommon->guestCtx);

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
    VBoxHGSMIHostCmdComplete(pCtx, pCmd);
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
    VBoxHGSMIProcessHostQueue(pCtx);

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
                            VBoxHGSMIHostCmdComplete(&pCallbacks->pCommon->hostCtx, pCur);
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
    VBoxHGSMIHostCmdComplete(&pCallbacks->pCommon->hostCtx, pvBuffer);
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

