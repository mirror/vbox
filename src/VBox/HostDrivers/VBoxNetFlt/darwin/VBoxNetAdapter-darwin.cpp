/* $Id$ */
/** @file
 * VBoxNetAdapter - Virtual Network Adapter Driver (Host), Darwin Specific Code.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/

#define LOG_GROUP LOG_GROUP_NET_TAP_DRV
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/uuid.h>

#include <sys/systm.h>
__BEGIN_DECLS /* Buggy 10.4 headers, fixed in 10.5. */
#include <sys/kpi_mbuf.h>
__END_DECLS

#include <net/ethernet.h>
#include <net/if_ether.h>
#include <net/if_types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <sys/errno.h>
#include <sys/param.h>

#define VBOXNETADA_MAX_INSTANCES  8
#define VBOXNETADA_MAX_FAMILIES   4
#define VBOXNETADA_NAME           "vboxnet"
#define VBOXNETADA_MTU            1500
#define VBOXNETADA_DETACH_TIMEOUT 500

#define VBOXNETADA_FROM_IFACE(iface) ((PVBOXNETADA) ifnet_softc(iface))

/**
 * Void NETADAs mark vacant slots in NETADA array. Valid NETADAs are busy slots.
 * As soon as slot is being modified its state changes to transitional.
 * NETADAs in transitional state must only be accessed by the thread that
 * put it to this state.
 */
enum VBoxNetAdaState
{
    VBOXNETADA_ST_VOID,
    VBOXNETADA_ST_TRANSITIONAL,
    VBOXNETADA_ST_VALID
};
typedef enum VBoxNetAdaState VBOXNETADASTATE;

struct VBoxNetAda
{
    /** Mutex protecting access to enmState. */
    RTSEMFASTMUTEX    hStateMtx;
    /** Denotes availability of this NETADA. */
    VBOXNETADASTATE      enmState;
    /** Corresponds to the digit at the end of NETADA name. */
    uint8_t           uUnit;
    /** Event to signal detachment of interface. */
    RTSEMEVENT        hEvtDetached;
    /** Pointer to Darwin interface structure. */
    ifnet_t           pIface;
    /* todo: MAC address? */
    /** Protocol families attached to this NETADA. */
    protocol_family_t aAttachedFamilies[VBOXNETADA_MAX_FAMILIES];
};
typedef struct VBoxNetAda VBOXNETADA;
typedef VBOXNETADA *PVBOXNETADA;

VBOXNETADA g_aAdapters[VBOXNETADA_MAX_INSTANCES];

DECLINLINE(bool) vboxNetAdaIsValid(PVBOXNETADA pAda)
{
    return pAda->enmState == VBOXNETADA_ST_VALID;
}

DECLINLINE(bool) vboxNetAdaIsVoid(PVBOXNETADA pAda)
{
    return pAda->enmState == VBOXNETADA_ST_VOID;
}

static void vboxNetAdaComposeMACAddress(PVBOXNETADA pAda, PCRTMAC pCustomMac, struct sockaddr_dl *pMac)
{
    pMac->sdl_len = sizeof(*pMac);
    pMac->sdl_family = AF_LINK;
    pMac->sdl_alen = ETHER_ADDR_LEN;
    pMac->sdl_nlen = 0;
    pMac->sdl_slen = 0;
    if (pMac)
        memcpy(LLADDR(pMac), pCustomMac->au8, ETHER_ADDR_LEN);
    else
    {
        memcpy(LLADDR(pMac), "\0vbox0", pMac->sdl_alen);
        LLADDR(pMac)[ETHER_ADDR_LEN - 1] += pAda->uUnit;
    }
}

static void vboxNetAdaComposeUUID(PVBOXNETADA pAda, PRTUUID pUuid)
{
    /* Generate UUID from name and MAC address. */
    RTUuidClear(pUuid);
    memcpy(pUuid->au8, "vboxnet", 7);
    pUuid->Gen.u8ClockSeqHiAndReserved = (pUuid->Gen.u8ClockSeqHiAndReserved & 0x3f) | 0x80;
    pUuid->Gen.u16TimeHiAndVersion = (pUuid->Gen.u16TimeHiAndVersion & 0x0fff) | 0x4000;
    pUuid->Gen.u8ClockSeqLow = pAda->uUnit;
    memcpy(pUuid->Gen.au8Node, "\0vbox0", sizeof(pUuid->Gen.au8Node));
    pUuid->Gen.au8Node[sizeof(pUuid->Gen.au8Node) - 1] += pAda->uUnit;
}


static errno_t vboxNetAdaOutput(ifnet_t pIface, mbuf_t pMBuf)
{
    mbuf_freem_list(pMBuf);
    return 0;
}

static void vboxNetAdaAttachFamily(PVBOXNETADA pAda, protocol_family_t Family)
{
    u_int32_t i;
    for (i = 0; i < VBOXNETADA_MAX_FAMILIES; i++)
        if (pAda->aAttachedFamilies[i] == 0)
        {
            pAda->aAttachedFamilies[i] = Family;
            break;
        }
}

static void vboxNetAdaDetachFamily(PVBOXNETADA pAda, protocol_family_t Family)
{
    u_int32_t i;
    for (i = 0; i < VBOXNETADA_MAX_FAMILIES; i++)
        if (pAda->aAttachedFamilies[i] == Family)
            pAda->aAttachedFamilies[i] = 0;
}

static errno_t vboxNetAdaAddProto(ifnet_t pIface, protocol_family_t Family, const struct ifnet_demux_desc *pDemuxDesc, u_int32_t nDesc)
{
    PVBOXNETADA pAda = VBOXNETADA_FROM_IFACE(pIface);
    Assert(pAda);
    vboxNetAdaAttachFamily(pAda, Family);
    LogFlow(("vboxNetAdaAddProto: Family=%d.\n", Family));
    return ether_add_proto(pIface, Family, pDemuxDesc, nDesc);
}

static errno_t vboxNetAdaDelProto(ifnet_t pIface, protocol_family_t Family)
{
    PVBOXNETADA pAda = VBOXNETADA_FROM_IFACE(pIface);
    Assert(pAda);
    LogFlow(("vboxNetAdaDelProto: Family=%d.\n", Family));
    vboxNetAdaDetachFamily(pAda, Family);
    return ether_del_proto(pIface, Family);
}

static void vboxNetAdaDetach(ifnet_t pIface)
{
    PVBOXNETADA pAda = VBOXNETADA_FROM_IFACE(pIface);
    Assert(pAda);
    Log2(("vboxNetAdaDetach: Signaling detach to vboxNetAdaUnregisterDevice.\n"));
    /* Let vboxNetAdaUnregisterDevice know that the interface has been detached. */
    RTSemEventSignal(pAda->hEvtDetached);
}


static int vboxNetAdaRegisterDevice(PVBOXNETADA pAda, const struct sockaddr_dl *pMACAddress)
{
    struct ifnet_init_params Params;
    RTUUID uuid;
    
    vboxNetAdaComposeUUID(pAda, &uuid);
    Params.uniqueid = uuid.au8;
    Params.uniqueid_len = sizeof(uuid);
    Params.name = VBOXNETADA_NAME;
    Params.unit = pAda->uUnit;
    Params.family = IFNET_FAMILY_ETHERNET;
    Params.type = IFT_ETHER;
    Params.output = vboxNetAdaOutput;
    Params.demux = ether_demux;
    Params.add_proto = vboxNetAdaAddProto;
    Params.del_proto = vboxNetAdaDelProto;
    Params.check_multi = ether_check_multi;
    Params.framer = ether_frameout;
    Params.softc = pAda;
    Params.ioctl = ether_ioctl;
    Params.set_bpf_tap = NULL;
    Params.detach = vboxNetAdaDetach;
    Params.event = NULL;
    Params.broadcast_addr = "\xFF\xFF\xFF\xFF\xFF\xFF";
    Params.broadcast_len = ETHER_ADDR_LEN;

    errno_t err = ifnet_allocate(&Params, &pAda->pIface);
    if (!err)
    {
        err = ifnet_attach(pAda->pIface, pMACAddress);
        if (!err)
        {
            err = ifnet_set_flags(pAda->pIface, IFF_RUNNING | IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST, 0xFFFF);
            if (!err)
            {
                ifnet_set_mtu(pAda->pIface, VBOXNETADA_MTU);
                return VINF_SUCCESS;
            }
            else
                Log(("vboxNetAdaRegisterDevice: Failed to set flags (err=%d).\n", err));
            ifnet_detach(pAda->pIface);
        }
        else
            Log(("vboxNetAdaRegisterDevice: Failed to attach to interface (err=%d).\n", err));
        ifnet_release(pAda->pIface);
    }
    else
        Log(("vboxNetAdaRegisterDevice: Failed to allocate interface (err=%d).\n", err));

    return RTErrConvertFromErrno(err);
}

static int vboxNetAdaUnregisterDevice(PVBOXNETADA pAda)
{
    u_int32_t i;
    /* Bring down the interface */
    int rc = VINF_SUCCESS;
    errno_t err = ifnet_set_flags(pAda->pIface, 0, IFF_UP | IFF_RUNNING);
    if (err)
        Log(("vboxNetAdaUnregisterDevice: Failed to bring down interface "
             "(err=%d).\n", err));
    /* Detach all protocols. */
    for (i = 0; i < VBOXNETADA_MAX_FAMILIES; i++)
        if (pAda->aAttachedFamilies[i])
            ifnet_detach_protocol(pAda->pIface, pAda->aAttachedFamilies[i]);
    err = ifnet_detach(pAda->pIface);
    if (err)
        Log(("vboxNetAdaUnregisterDevice: Failed to detach interface "
             "(err=%d).\n", err));
    Log2(("vboxNetAdaUnregisterDevice: Waiting for 'detached' event...\n"));
    /* Wait until we get a signal from detach callback. */
    rc = RTSemEventWait(pAda->hEvtDetached, VBOXNETADA_DETACH_TIMEOUT);
    if (rc == VERR_TIMEOUT)
        LogRel(("VBoxNETADA: Failed to detach interface %s%d\n.",
                VBOXNETADA_NAME, pAda->uUnit));
    err = ifnet_release(pAda->pIface);
    if (err)
        Log(("vboxNetAdaUnregisterDevice: Failed to release interface (err=%d).\n", err));
    
    return rc;
}

int vboxNetAdaCreate (PVBOXNETADA *ppAda, PCRTMAC pMac)
{
    int rc;
    unsigned i;

    for (i = 0; i < RT_ELEMENTS(g_aAdapters); i++)
    {
        PVBOXNETADA pAda = &g_aAdapters[i];
        RTSemFastMutexRequest(pAda->hStateMtx);
        if (vboxNetAdaIsVoid(pAda))
        {
            pAda->enmState = VBOXNETADA_ST_TRANSITIONAL;
            RTSemFastMutexRelease(pAda->hStateMtx);
            /* Found an empty slot -- use it. */
            struct sockaddr_dl mac;
            Assert(pAda->hEvtDetached == NIL_RTSEMEVENT);
            rc = RTSemEventCreate(&pAda->hEvtDetached);
            if (RT_FAILURE(rc))
                return rc;
            pAda->uUnit = i;
                vboxNetAdaComposeMACAddress(pAda, pMac, &mac);
            rc = vboxNetAdaRegisterDevice(pAda, &mac);
            *ppAda = pAda;
            RTSemFastMutexRequest(pAda->hStateMtx);
            pAda->enmState = VBOXNETADA_ST_VALID;
            RTSemFastMutexRelease(pAda->hStateMtx);
            return rc;
        }
        RTSemFastMutexRelease(pAda->hStateMtx);
    }

    /* All slots in adapter array are busy. */
    return VERR_OUT_OF_RESOURCES;
}

int vboxNetAdaDestroy (PVBOXNETADA pAda)
{
    int rc;

    RTSemFastMutexRequest(pAda->hStateMtx);
    if (!vboxNetAdaIsValid(pAda))
    {
        RTSemFastMutexRelease(pAda->hStateMtx);
        return VERR_INVALID_PARAMETER;
    }
    pAda->enmState = VBOXNETADA_ST_TRANSITIONAL;
    RTSemFastMutexRelease(pAda->hStateMtx);

    rc = vboxNetAdaUnregisterDevice(pAda);
    if (RT_FAILURE(rc))
        Log(("vboxNetAdaDestroy: Failed to unregister device (rc=%Rrc).\n", rc));

    RTSemEventDestroy(pAda->hEvtDetached);
    pAda->hEvtDetached = NIL_RTSEMEVENT;

    RTSemFastMutexRequest(pAda->hStateMtx);
    pAda->enmState = VBOXNETADA_ST_VOID;
    RTSemFastMutexRelease(pAda->hStateMtx);

    return rc;
}

int vboxNetAdaModuleStart(void)
{
    memset(&g_aAdapters, 0, sizeof(g_aAdapters));
    for (unsigned i = 0; i < RT_ELEMENTS(g_aAdapters); i++)
    {
        int rc = RTSemFastMutexCreate(&g_aAdapters[i].hStateMtx);
        if (RT_FAILURE(rc))
        {
            Log(("vboxNetAdaModuleStart: Failed to create fast mutex (rc=%Rrc).\n", rc));
            return rc;
        }
    }

    return VINF_SUCCESS;
}

int vboxNetAdaModuleStop(void)
{
    int rc;
    unsigned i;

    for (i = 0; i < RT_ELEMENTS(g_aAdapters); i++)
    {
        vboxNetAdaDestroy(&g_aAdapters[i]);
        rc = RTSemFastMutexDestroy(g_aAdapters[i].hStateMtx);
        if (RT_FAILURE(rc))
            Log(("vboxNetAdaModuleStop: Failed to destroy fast mutex (rc=%Rrc).\n", rc));
    }

    return VINF_SUCCESS;
}

