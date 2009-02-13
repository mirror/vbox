/* $Id$ */
/** @file
 * VBoxTAP - Network TAP Driver (Host), Darwin Specific Code.
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

#define VBOXTAP_MAX_INSTANCES  8
#define VBOXTAP_MAX_FAMILIES   4
#define VBOXTAP_NAME           "vboxnet"
#define VBOXTAP_MTU            1500
#define VBOXTAP_DETACH_TIMEOUT 500

#define VBOXTAP_FROM_IFACE(iface) ((PVBOXTAP) ifnet_softc(iface))

/**
 * Void TAPs mark vacant slots in TAP array. Valid TAPs are busy slots.
 * As soon as slot is being modified its state changes to transitional.
 * TAPs in transitional state must only be accessed by the thread that
 * put it to this state.
 */
enum VBoxTapState
{
    VBOXTAP_ST_VOID,
    VBOXTAP_ST_TRANSITIONAL,
    VBOXTAP_ST_VALID
};
typedef enum VBoxTapState VBOXTAPSTATE;

struct VBoxTap
{
    /** Mutex protecting access to enmState. */
    RTSEMFASTMUTEX    hStateMtx;
    /** Denotes availability of this TAP. */
    VBOXTAPSTATE      enmState;
    /** Corresponds to the digit at the end of TAP name. */
    uint8_t           uUnit;
    /** Event to signal detachment of interface. */
    RTSEMEVENT        hEvtDetached;
    /** Pointer to Darwin interface structure. */
    ifnet_t           pIface;
    /* todo: MAC address? */
    /** Protocol families attached to this TAP. */
    protocol_family_t aAttachedFamilies[VBOXTAP_MAX_FAMILIES];
};
typedef struct VBoxTap VBOXTAP;
typedef VBOXTAP *PVBOXTAP;

VBOXTAP g_aTaps[VBOXTAP_MAX_INSTANCES];

DECLINLINE(bool) vboxTAPIsValid(PVBOXTAP pTap)
{
    return pTap->enmState == VBOXTAP_ST_VALID;
}

DECLINLINE(bool) vboxTAPIsVoid(PVBOXTAP pTap)
{
    return pTap->enmState == VBOXTAP_ST_VOID;
}

static void vboxTAPComposeMACAddress(PVBOXTAP pTap, PCRTMAC pCustomMac, struct sockaddr_dl *pMac)
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
        LLADDR(pMac)[ETHER_ADDR_LEN - 1] += pTap->uUnit;
    }
}

static void vboxTAPComposeUUID(PVBOXTAP pTap, PRTUUID pUuid)
{
    /* Generate UUID from name and MAC address. */
    RTUuidClear(pUuid);
    memcpy(pUuid->au8, "vboxnet", 7);
    pUuid->Gen.u8ClockSeqHiAndReserved = (pUuid->Gen.u8ClockSeqHiAndReserved & 0x3f) | 0x80;
    pUuid->Gen.u16TimeHiAndVersion = (pUuid->Gen.u16TimeHiAndVersion & 0x0fff) | 0x4000;
    pUuid->Gen.u8ClockSeqLow = pTap->uUnit;
    memcpy(pUuid->Gen.au8Node, "\0vbox0", sizeof(pUuid->Gen.au8Node));
    pUuid->Gen.au8Node[sizeof(pUuid->Gen.au8Node) - 1] += pTap->uUnit;
}


static errno_t vboxTAPOutput(ifnet_t pIface, mbuf_t pMBuf)
{
    mbuf_freem_list(pMBuf);
    return 0;
}

static void vboxTAPAttachFamily(PVBOXTAP pTap, protocol_family_t Family)
{
    u_int32_t i;
    for (i = 0; i < VBOXTAP_MAX_FAMILIES; i++)
        if (pTap->aAttachedFamilies[i] == 0)
        {
            pTap->aAttachedFamilies[i] = Family;
            break;
        }
}

static void vboxTAPDetachFamily(PVBOXTAP pTap, protocol_family_t Family)
{
    u_int32_t i;
    for (i = 0; i < VBOXTAP_MAX_FAMILIES; i++)
        if (pTap->aAttachedFamilies[i] == Family)
            pTap->aAttachedFamilies[i] = 0;
}

static errno_t vboxTAPAddProto(ifnet_t pIface, protocol_family_t Family, const struct ifnet_demux_desc *pDemuxDesc, u_int32_t nDesc)
{
    PVBOXTAP pTap = VBOXTAP_FROM_IFACE(pIface);
    Assert(pTap);
    vboxTAPAttachFamily(pTap, Family);
    LogFlow(("vboxTAPAddProto: Family=%d.\n", Family));
    return ether_add_proto(pIface, Family, pDemuxDesc, nDesc);
}

static errno_t vboxTAPDelProto(ifnet_t pIface, protocol_family_t Family)
{
    PVBOXTAP pTap = VBOXTAP_FROM_IFACE(pIface);
    Assert(pTap);
    LogFlow(("vboxTAPDelProto: Family=%d.\n", Family));
    vboxTAPDetachFamily(pTap, Family);
    return ether_del_proto(pIface, Family);
}

static void vboxTAPDetach(ifnet_t pIface)
{
    PVBOXTAP pTap = VBOXTAP_FROM_IFACE(pIface);
    Assert(pTap);
    Log(("vboxTAPDetach: Signaling detach to vboxTAPUnregisterDevice.\n"));
    /* Let vboxTAPUnregisterDevice know that the interface has been detached. */
    RTSemEventSignal(pTap->hEvtDetached);
}


static int vboxTAPRegisterDevice(PVBOXTAP pTap, const struct sockaddr_dl *pMACAddress)
{
    struct ifnet_init_params Params;
    RTUUID uuid;
    
    vboxTAPComposeUUID(pTap, &uuid);
    Params.uniqueid = uuid.au8;
    Params.uniqueid_len = sizeof(uuid);
    Params.name = VBOXTAP_NAME;
    Params.unit = pTap->uUnit;
    Params.family = IFNET_FAMILY_ETHERNET;
    Params.type = IFT_ETHER;
    Params.output = vboxTAPOutput;
    Params.demux = ether_demux;
    Params.add_proto = vboxTAPAddProto;
    Params.del_proto = vboxTAPDelProto;
    Params.check_multi = ether_check_multi;
    Params.framer = ether_frameout;
    Params.softc = pTap;
    Params.ioctl = ether_ioctl;
    Params.set_bpf_tap = NULL;
    Params.detach = vboxTAPDetach;
    Params.event = NULL;
    Params.broadcast_addr = "\xFF\xFF\xFF\xFF\xFF\xFF";
    Params.broadcast_len = ETHER_ADDR_LEN;

    errno_t err = ifnet_allocate(&Params, &pTap->pIface);
    if (!err)
    {
        err = ifnet_attach(pTap->pIface, pMACAddress);
        if (!err)
        {
            err = ifnet_set_flags(pTap->pIface, IFF_RUNNING | IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST, 0xFFFF);
            if (!err)
            {
                ifnet_set_mtu(pTap->pIface, VBOXTAP_MTU);
                return VINF_SUCCESS;
            }
            else
                Log(("vboxTAPRegisterDevice: Failed to set flags (err=%d).\n", err));
            ifnet_detach(pTap->pIface);
        }
        else
            Log(("vboxTAPRegisterDevice: Failed to attach to interface (err=%d).\n", err));
        ifnet_release(pTap->pIface);
    }
    else
        Log(("vboxTAPRegisterDevice: Failed to allocate interface (err=%d).\n", err));

    return RTErrConvertFromErrno(err);
}

static int vboxTAPUnregisterDevice(PVBOXTAP pTap)
{
    u_int32_t i;
    /* Bring down the interface */
    int rc = VINF_SUCCESS;
    errno_t err = ifnet_set_flags(pTap->pIface, 0, IFF_UP | IFF_RUNNING);
    if (err)
        Log(("vboxTAPUnregisterDevice: Failed to bring down interface "
             "(err=%d).\n", err));
    /* Detach all protocols. */
    for (i = 0; i < VBOXTAP_MAX_FAMILIES; i++)
        if (pTap->aAttachedFamilies[i])
            ifnet_detach_protocol(pTap->pIface, pTap->aAttachedFamilies[i]);
    /*
     * We acquire the lock twice: before and after ifnet_detach.
     * The second attempt to get the lock will block until the lock
     * is released in detach callback.
     */
    err = ifnet_detach(pTap->pIface);
    if (err)
        Log(("vboxTAPUnregisterDevice: Failed to detach interface "
             "(err=%d).\n", err));
    Log2(("vboxTAPUnregisterDevice: Trying to acquire lock second time.\n"));
    rc = RTSemEventWait(pTap->hEvtDetached, VBOXTAP_DETACH_TIMEOUT);
    if (rc == VERR_TIMEOUT)
        LogRel(("VBoxTAP: Failed to detach interface %s%d\n.",
                VBOXTAP_NAME, pTap->uUnit));
    err = ifnet_release(pTap->pIface);
    if (err)
        Log(("vboxTAPUnregisterDevice: Failed to release interface (err=%d).\n", err));
    
    return rc;
}

int vboxTAPCreate (PVBOXTAP *ppTap, PCRTMAC pMac)
{
    int rc;
    unsigned i;

    for (i = 0; i < RT_ELEMENTS(g_aTaps); i++)
    {
        PVBOXTAP pTap = &g_aTaps[i];
        RTSemFastMutexRequest(pTap->hStateMtx);
        if (vboxTAPIsVoid(pTap))
        {
            pTap->enmState = VBOXTAP_ST_TRANSITIONAL;
            RTSemFastMutexRelease(pTap->hStateMtx);
            /* Found an empty slot -- use it. */
            struct sockaddr_dl mac;
            Assert(pTap->hEvtDetached == NIL_RTSEMEVENT);
            rc = RTSemEventCreate(&pTap->hEvtDetached);
            if (RT_FAILURE(rc))
                return rc;
            pTap->uUnit = i;
                vboxTAPComposeMACAddress(pTap, pMac, &mac);
            rc = vboxTAPRegisterDevice(pTap, &mac);
            *ppTap = pTap;
            RTSemFastMutexRequest(pTap->hStateMtx);
            pTap->enmState = VBOXTAP_ST_VALID;
            RTSemFastMutexRelease(pTap->hStateMtx);
            return rc;
        }
        RTSemFastMutexRelease(pTap->hStateMtx);
    }

    /* All slots in TAP array are busy. */
    return VERR_OUT_OF_RESOURCES;
}

int vboxTAPDestroy (PVBOXTAP pTap)
{
    int rc;

    RTSemFastMutexRequest(pTap->hStateMtx);
    if (!vboxTAPIsValid(pTap))
    {
        RTSemFastMutexRelease(pTap->hStateMtx);
        return VERR_INVALID_PARAMETER;
    }
    pTap->enmState = VBOXTAP_ST_TRANSITIONAL;
    RTSemFastMutexRelease(pTap->hStateMtx);

    rc = vboxTAPUnregisterDevice(pTap);
    if (RT_FAILURE(rc))
        Log(("vboxTAPDestroy: Failed to unregister device (rc=%Rrc).\n", rc));

    RTSemEventDestroy(pTap->hEvtDetached);
    pTap->hEvtDetached = NIL_RTSEMEVENT;

    RTSemFastMutexRequest(pTap->hStateMtx);
    pTap->enmState = VBOXTAP_ST_VOID;
    RTSemFastMutexRelease(pTap->hStateMtx);

    return rc;
}

int vboxTAPModuleStart(void)
{
    memset(&g_aTaps, 0, sizeof(g_aTaps));
    for (unsigned i = 0; i < RT_ELEMENTS(g_aTaps); i++)
    {
        int rc = RTSemFastMutexCreate(&g_aTaps[i].hStateMtx);
        if (RT_FAILURE(rc))
        {
            Log(("VBoxTAP: Failed to create fast mutex (rc=%Rrc).\n", rc));
            return rc;
        }
    }

    return VINF_SUCCESS;
}

int vboxTAPModuleStop(void)
{
    int rc;
    unsigned i;

    for (i = 0; i < RT_ELEMENTS(g_aTaps); i++)
    {
        vboxTAPDestroy(&g_aTaps[i]);
        rc = RTSemFastMutexDestroy(g_aTaps[i].hStateMtx);
        if (RT_FAILURE(rc))
            Log(("VBoxTAP: Failed to destroy fast mutex (rc=%Rrc).\n", rc));
    }

    return VINF_SUCCESS;
}

