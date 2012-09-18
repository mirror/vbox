/** @file
 * Host channel.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/asm.h>
#include <iprt/assert.h>

#include "HostChannel.h"

static DECLCALLBACK(void) HostChannelCallbackEvent(void *pvCallbacks, void *pvInstance,
                                                   uint32_t u32Id, const void *pvEvent, uint32_t cbEvent);


/* A registered provider of channels. */
typedef struct VBOXHOSTCHPROVIDER
{
    int32_t volatile cRefs;

    RTLISTNODE nodeContext; /* Member of the list of providers in the service context. */

    VBOXHOSTCHCTX *pCtx;

    VBOXHOSTCHANNELINTERFACE iface;

    char *pszName;

    RTLISTANCHOR listChannels;
} VBOXHOSTCHPROVIDER;

/* An established channel. */
typedef struct VBOXHOSTCHINSTANCE
{
    int32_t volatile cRefs;

    RTLISTNODE nodeClient;    /* In the client, for cleanup when a client disconnects. */
    RTLISTNODE nodeProvider;  /* In the provider, needed for cleanup when the provider is unregistered. */

    VBOXHOSTCHCLIENT *pClient; /* The client which uses the channel. */
    VBOXHOSTCHPROVIDER *pProvider; /* NULL if the provider was unregistered. */
    void *pvChannel;               /* Provider's context of the channel. */
    uint32_t u32Handle;        /* handle assigned to the channel by the service. */
} VBOXHOSTCHINSTANCE;

struct VBOXHOSTCHCTX
{
    bool fInitialized;

    RTLISTANCHOR listProviders;
};

/* Only one service instance is supported. */
static VBOXHOSTCHCTX g_ctx = { false };

static VBOXHOSTCHANNELCALLBACKS g_callbacks = 
{
    HostChannelCallbackEvent
};


/*
 * Provider management.
 */

static void vhcProviderDestroy(VBOXHOSTCHPROVIDER *pProvider)
{
    RTStrFree(pProvider->pszName);
}

static int32_t vhcProviderAddRef(VBOXHOSTCHPROVIDER *pProvider)
{
    return ASMAtomicIncS32(&pProvider->cRefs);
}

static void vhcProviderRelease(VBOXHOSTCHPROVIDER *pProvider)
{
    int32_t c = ASMAtomicDecS32(&pProvider->cRefs);
    Assert(c >= 0);
    if (c == 0)
    {
        vhcProviderDestroy(pProvider);
        RTMemFree(pProvider);
    }
}

static VBOXHOSTCHPROVIDER *vhcProviderFind(VBOXHOSTCHCTX *pCtx, const char *pszName)
{
    VBOXHOSTCHPROVIDER *pProvider = NULL;

    int rc = vboxHostChannelLock();

    if (RT_SUCCESS(rc))
    {
        VBOXHOSTCHPROVIDER *pIter;
        RTListForEach(&pCtx->listProviders, pIter, VBOXHOSTCHPROVIDER, nodeContext)
        {
            if (RTStrCmp(pIter->pszName, pszName) == 0)
            {
                pProvider = pIter;

                vhcProviderAddRef(pProvider);

                break;
            }
        }

        vboxHostChannelUnlock();
    }

    return pProvider;
}

static int vhcProviderRegister(VBOXHOSTCHCTX *pCtx, VBOXHOSTCHPROVIDER *pProvider)
{
    int rc = vboxHostChannelLock();

    if (RT_SUCCESS(rc))
    {
        /* @todo check a duplicate. */

        RTListAppend(&pCtx->listProviders, &pProvider->nodeContext);

        vboxHostChannelUnlock();
    }

    if (RT_FAILURE(rc))
    {
        vhcProviderRelease(pProvider);
    }

    return rc;
}

static int vhcProviderUnregister(VBOXHOSTCHPROVIDER *pProvider)
{
    int rc = vboxHostChannelLock();

    if (RT_SUCCESS(rc))
    {
        /* @todo check that the provider is in the list. */
        /* @todo mark the provider as invalid in each instance. also detach channels? */

        RTListNodeRemove(&pProvider->nodeContext);

        vboxHostChannelUnlock();

        vhcProviderRelease(pProvider);
    }

    return rc;
}


/*
 * Select an unique handle for the new channel.
 * Works under the lock.
 */
static int vhcHandleCreate(VBOXHOSTCHCLIENT *pClient, uint32_t *pu32Handle)
{
    bool fOver = false;

    for(;;)
    {
        uint32_t u32Handle = ASMAtomicIncU32(&pClient->u32HandleSrc);

        if (u32Handle == 0)
        {
            if (fOver)
            {
                return VERR_NOT_SUPPORTED;
            }

            fOver = true;
            continue;
        }

        VBOXHOSTCHINSTANCE *pDuplicate = NULL;
        VBOXHOSTCHINSTANCE *pIter;
        RTListForEach(&pClient->listChannels, pIter, VBOXHOSTCHINSTANCE, nodeClient)
        {
            if (pIter->u32Handle == u32Handle)
            {
                pDuplicate = pIter;
                break;
            }
        }

        if (pDuplicate == NULL)
        {
            *pu32Handle = u32Handle;
            break;
        }
    }

    return VINF_SUCCESS;
}


/*
 * Channel instance management.
 */

static void vhcInstanceDestroy(VBOXHOSTCHINSTANCE *pInstance)
{
    /* @todo free u32Handle? */
}

static int32_t vhcInstanceAddRef(VBOXHOSTCHINSTANCE *pInstance)
{
    return ASMAtomicIncS32(&pInstance->cRefs);
}

static void vhcInstanceRelease(VBOXHOSTCHINSTANCE *pInstance)
{
    int32_t c = ASMAtomicDecS32(&pInstance->cRefs);
    Assert(c >= 0);
    if (c == 0)
    {
        vhcInstanceDestroy(pInstance);
        RTMemFree(pInstance);
    }
}

static int vhcInstanceCreate(VBOXHOSTCHCLIENT *pClient, VBOXHOSTCHINSTANCE **ppInstance)
{
    int rc = VINF_SUCCESS;

    VBOXHOSTCHINSTANCE *pInstance = (VBOXHOSTCHINSTANCE *)RTMemAllocZ(sizeof(VBOXHOSTCHINSTANCE));

    if (pInstance)
    {
        rc = vboxHostChannelLock();

        if (RT_SUCCESS(rc))
        {
            rc = vhcHandleCreate(pClient, &pInstance->u32Handle);

            if (RT_SUCCESS(rc))
            {
                vhcInstanceAddRef(pInstance);

                *ppInstance = pInstance;
            }

            vboxHostChannelUnlock();
        }

        if (RT_FAILURE(rc))
        {
            RTMemFree(pInstance);
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    return rc;
}

static VBOXHOSTCHINSTANCE *vhcInstanceFind(VBOXHOSTCHCLIENT *pClient, uint32_t u32Handle)
{
    VBOXHOSTCHINSTANCE *pInstance = NULL;

    int rc = vboxHostChannelLock();

    if (RT_SUCCESS(rc))
    {
        VBOXHOSTCHINSTANCE *pIter;
        RTListForEach(&pClient->listChannels, pIter, VBOXHOSTCHINSTANCE, nodeClient)
        {
            if (pIter->u32Handle == u32Handle)
            {
                pInstance = pIter;

                vhcInstanceAddRef(pInstance);

                break;
            }
        }

        vboxHostChannelUnlock();
    }

    return pInstance;
}

static VBOXHOSTCHINSTANCE *vhcInstanceFindByChannelPtr(VBOXHOSTCHCLIENT *pClient, void *pvChannel)
{
    VBOXHOSTCHINSTANCE *pInstance = NULL;

    int rc = vboxHostChannelLock();

    if (RT_SUCCESS(rc))
    {
        VBOXHOSTCHINSTANCE *pIter;
        RTListForEach(&pClient->listChannels, pIter, VBOXHOSTCHINSTANCE, nodeClient)
        {
            if (pIter->pvChannel == pvChannel)
            {
                pInstance = pIter;

                vhcInstanceAddRef(pInstance);

                break;
            }
        }

        vboxHostChannelUnlock();
    }

    return pInstance;
}

static void vhcInstanceDetach(VBOXHOSTCHINSTANCE *pInstance)
{
    if (pInstance->pProvider)
    {
        pInstance->pProvider->iface.HostChannelDetach(pInstance->pvChannel);
        RTListNodeRemove(&pInstance->nodeProvider);
        vhcProviderRelease(pInstance->pProvider);
        vhcInstanceRelease(pInstance); /* Not in the list anymore. */
    }

    RTListNodeRemove(&pInstance->nodeClient);
    vhcInstanceRelease(pInstance); /* Not in the list anymore. */
}


/*
 * Host channel service functions.
 */

int vboxHostChannelInit(void)
{
    VBOXHOSTCHCTX *pCtx = &g_ctx;

    if (pCtx->fInitialized)
    {
        return VERR_NOT_SUPPORTED;
    }

    pCtx->fInitialized = true;
    RTListInit(&pCtx->listProviders);

    return VINF_SUCCESS;
}

void vboxHostChannelDestroy(void)
{
    VBOXHOSTCHCTX *pCtx = &g_ctx;

    VBOXHOSTCHPROVIDER *pIter;
    VBOXHOSTCHPROVIDER *pIterNext;
    RTListForEachSafe(&pCtx->listProviders, pIter, pIterNext, VBOXHOSTCHPROVIDER, nodeContext)
    {
        vhcProviderUnregister(pIter);
    }
    pCtx->fInitialized = false;
}

int vboxHostChannelClientConnect(VBOXHOSTCHCLIENT *pClient)
{
    /* A guest client is connecting to the service.
     * Later the client will use Attach calls to connect to channel providers.
     * pClient is already zeroed.
     */
    pClient->pCtx = &g_ctx;

    RTListInit(&pClient->listChannels);
    RTListInit(&pClient->listEvents);

    return VINF_SUCCESS;
}

void vboxHostChannelClientDisconnect(VBOXHOSTCHCLIENT *pClient)
{
    /* If there are attached channels, detach them. */
    VBOXHOSTCHINSTANCE *pIter;
    VBOXHOSTCHINSTANCE *pIterNext;
    RTListForEachSafe(&pClient->listChannels, pIter, pIterNext, VBOXHOSTCHINSTANCE, nodeClient)
    {
        vhcInstanceDetach(pIter);
    }
}

int vboxHostChannelAttach(VBOXHOSTCHCLIENT *pClient,
                          uint32_t *pu32Handle,
                          const char *pszName,
                          uint32_t u32Flags)
{
    int rc = VINF_SUCCESS;

    HOSTCHLOG(("HostChannel: Attach: (%d) [%s] 0x%08X\n", pClient->u32ClientID, pszName, u32Flags));

    /* Look if there is a provider. */
    VBOXHOSTCHPROVIDER *pProvider = vhcProviderFind(pClient->pCtx, pszName);

    if (pProvider)
    {
        VBOXHOSTCHINSTANCE *pInstance = NULL;

        rc = vhcInstanceCreate(pClient, &pInstance);

        if (RT_SUCCESS(rc))
        {
            void *pvChannel = NULL;
            rc = pProvider->iface.HostChannelAttach(pProvider->iface.pvProvider,
                                                    &pvChannel,
                                                    u32Flags,
                                                    &g_callbacks, pClient);
            if (RT_SUCCESS(rc))
            {
                vhcProviderAddRef(pProvider);
                pInstance->pProvider = pProvider;

                pInstance->pClient = pClient;
                pInstance->pvChannel = pvChannel;

                vhcInstanceAddRef(pInstance); /* Referenced by the list client's channels. */
                RTListAppend(&pClient->listChannels, &pInstance->nodeClient);

                vhcInstanceAddRef(pInstance); /* Referenced by the list of provider's channels. */
                RTListAppend(&pProvider->listChannels, &pInstance->nodeProvider);

                *pu32Handle = pInstance->u32Handle;

                HOSTCHLOG(("HostChannel: Attach: (%d) handle %d\n", pClient->u32ClientID, pInstance->u32Handle));
            }

            vhcInstanceRelease(pInstance);
        }

        vhcProviderRelease(pProvider);
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}

int vboxHostChannelDetach(VBOXHOSTCHCLIENT *pClient,
                          uint32_t u32Handle)
{
    HOSTCHLOG(("HostChannel: Detach: (%d) handle %d\n", pClient->u32ClientID, u32Handle));

    int rc = VINF_SUCCESS;

    VBOXHOSTCHINSTANCE *pInstance = vhcInstanceFind(pClient, u32Handle);

    if (pInstance)
    {
        vhcInstanceDetach(pInstance);

        vhcInstanceRelease(pInstance);
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}

int vboxHostChannelSend(VBOXHOSTCHCLIENT *pClient,
                        uint32_t u32Handle,
                        const void *pvData,
                        uint32_t cbData)
{
    HOSTCHLOG(("HostChannel: Send: (%d) handle %d, %d bytes\n", pClient->u32ClientID, u32Handle, cbData));

    int rc = VINF_SUCCESS;

    VBOXHOSTCHINSTANCE *pInstance = vhcInstanceFind(pClient, u32Handle);

    if (   pInstance
        && pInstance->pProvider)
    {
        pInstance->pProvider->iface.HostChannelSend(pInstance->pvChannel, pvData, cbData);

        vhcInstanceRelease(pInstance);
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}

int vboxHostChannelRecv(VBOXHOSTCHCLIENT *pClient,
                        uint32_t u32Handle,
                        void *pvData,
                        uint32_t cbData,
                        uint32_t *pu32SizeReceived,
                        uint32_t *pu32SizeRemaining)
{
    HOSTCHLOG(("HostChannel: Recv: (%d) handle %d, cbData %d\n", pClient->u32ClientID, u32Handle, cbData));

    int rc = VINF_SUCCESS;

    VBOXHOSTCHINSTANCE *pInstance = vhcInstanceFind(pClient, u32Handle);

    if (   pInstance
        && pInstance->pProvider)
    {
        rc = pInstance->pProvider->iface.HostChannelRecv(pInstance->pvChannel, pvData, cbData,
                                                         pu32SizeReceived, pu32SizeRemaining);

        HOSTCHLOG(("HostChannel: Recv: (%d) handle %d, rc %Rrc, recv %d, rem %d\n",
                        pClient->u32ClientID, u32Handle, rc, cbData, *pu32SizeReceived, *pu32SizeRemaining));

        vhcInstanceRelease(pInstance);
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}

int vboxHostChannelControl(VBOXHOSTCHCLIENT *pClient,
                           uint32_t u32Handle,
                           uint32_t u32Code,
                           void *pvParm,
                           uint32_t cbParm,
                           void *pvData,
                           uint32_t cbData,
                           uint32_t *pu32SizeDataReturned)
{
    HOSTCHLOG(("HostChannel: Control: (%d) handle %d, cbData %d\n", pClient->u32ClientID, u32Handle, cbData));

    int rc = VINF_SUCCESS;

    VBOXHOSTCHINSTANCE *pInstance = vhcInstanceFind(pClient, u32Handle);

    if (   pInstance
        && pInstance->pProvider)
    {
        pInstance->pProvider->iface.HostChannelControl(pInstance->pvChannel, u32Code,
                                                       pvParm, cbParm,
                                                       pvData, cbData, pu32SizeDataReturned);

        vhcInstanceRelease(pInstance);
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}

typedef struct VBOXHOSTCHANNELEVENT
{
    RTLISTNODE NodeEvent;

    uint32_t u32ChannelHandle;

    uint32_t u32Id;
    void *pvEvent;
    uint32_t cbEvent;
} VBOXHOSTCHANNELEVENT;

/* This is called under the lock. */
int vboxHostChannelQueryEvent(VBOXHOSTCHCLIENT *pClient,
                              bool *pfEvent,
                              uint32_t *pu32Handle,
                              uint32_t *pu32Id,
                              void *pvParm,
                              uint32_t cbParm,
                              uint32_t *pcbParmOut)
{
    /* Check if there is something in the client's event queue. */

    VBOXHOSTCHANNELEVENT *pEvent = RTListGetFirst(&pClient->listEvents, VBOXHOSTCHANNELEVENT, NodeEvent);

    HOSTCHLOG(("HostChannel: QueryEvent: (%d), event %p\n", pClient->u32ClientID, pEvent));

    if (pEvent)
    {
        /* Report the event. */
        RTListNodeRemove(&pEvent->NodeEvent);

        *pfEvent = true;
        *pu32Handle = pEvent->u32ChannelHandle;
        *pu32Id = pEvent->u32Id;

        uint32_t cbToCopy = RT_MIN(cbParm, pEvent->cbEvent);

        HOSTCHLOG(("HostChannel: QueryEvent: (%d), cbParm %d, cbEvent %d\n",
                        pClient->u32ClientID, cbParm, pEvent->cbEvent));

        if (cbToCopy > 0)
        {
            memcpy(pvParm, pEvent->pvEvent, cbToCopy);
        }

        *pcbParmOut = cbToCopy;

        RTMemFree(pEvent);
    }
    else
    {
        /* Tell the caller that there is no event. */
        *pfEvent = false;
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) HostChannelCallbackEvent(void *pvCallbacks, void *pvChannel,
                                                   uint32_t u32Id, const void *pvEvent, uint32_t cbEvent)
{
    VBOXHOSTCHCLIENT *pClient = (VBOXHOSTCHCLIENT *)pvCallbacks;

    VBOXHOSTCHINSTANCE *pInstance = vhcInstanceFindByChannelPtr(pClient, pvChannel);

    HOSTCHLOG(("HostChannel: CallbackEvent: (%d) instance %p\n",
               pClient->u32ClientID, pInstance));

    if (!pInstance)
    {
#ifdef DEBUG_sunlover
        AssertFailed();
#endif
        return;
    }

    int rc = vboxHostChannelLock();
    if (RT_FAILURE(rc))
    {
        return;
    }

    uint32_t u32ChannelHandle = pInstance->u32Handle;

    HOSTCHLOG(("HostChannel: CallbackEvent: (%d) handle %d, async %d, cbEvent %d\n",
                    pClient->u32ClientID, u32ChannelHandle, pClient->fAsync, cbEvent));

    /* Check whether the event is waited. */
    if (pClient->fAsync)
    {
        /* Report the event. */
        vboxHostChannelReportAsync(pClient, u32ChannelHandle, u32Id, pvEvent, cbEvent);

        pClient->fAsync = false;
    }
    else
    {
        /* Put it to the queue. */
        VBOXHOSTCHANNELEVENT *pEvent = (VBOXHOSTCHANNELEVENT *)RTMemAlloc(sizeof(VBOXHOSTCHANNELEVENT) + cbEvent);

        if (pEvent)
        {
            pEvent->u32ChannelHandle = u32ChannelHandle;
            pEvent->u32Id = u32Id;

            if (cbEvent)
            {
                pEvent->pvEvent = &pEvent[0];
                memcpy(pEvent->pvEvent, pvEvent, cbEvent);
            }
            else
            {
                pEvent->pvEvent = NULL;
            }

            pEvent->cbEvent = cbEvent;

            if (RT_SUCCESS(rc))
            {
                RTListAppend(&pClient->listEvents, &pEvent->NodeEvent);
            }
            else
            {
                RTMemFree(pEvent);
            }
        }
    }

    vboxHostChannelUnlock();
}

int vboxHostChannelRegister(const char *pszName,
                            const VBOXHOSTCHANNELINTERFACE *pInterface,
                            uint32_t cbInterface)
{
    int rc = VINF_SUCCESS;

    VBOXHOSTCHCTX *pCtx = &g_ctx;

    VBOXHOSTCHPROVIDER *pProvider = (VBOXHOSTCHPROVIDER *)RTMemAllocZ(sizeof(VBOXHOSTCHPROVIDER));

    if (pProvider)
    {
        pProvider->pCtx = pCtx;
        pProvider->iface = *pInterface;

        RTListInit(&pProvider->listChannels);

        pProvider->pszName = RTStrDup(pszName);
        if (pProvider->pszName)
        {
            vhcProviderAddRef(pProvider);
            rc = vhcProviderRegister(pCtx, pProvider);
        }
        else
        {
            RTMemFree(pProvider);
            rc = VERR_NO_MEMORY;
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    return rc;
}

int vboxHostChannelUnregister(const char *pszName)
{
    int rc = VINF_SUCCESS;

    VBOXHOSTCHCTX *pCtx = &g_ctx;

    VBOXHOSTCHPROVIDER *pProvider = vhcProviderFind(pCtx, pszName);

    if (pProvider)
    {
        rc = vhcProviderUnregister(pProvider);
        vhcProviderRelease(pProvider);
    }

    return rc;
}
