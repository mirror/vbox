/* $Id$ */
/** @file
 * VBoxNetAdp - Virtual Network Adapter Driver (Host), Darwin Specific Code.
 */

/*
 * Copyright (C) 2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
/*
 * Deal with conflicts first.
 * PVM - BSD mess, that FreeBSD has correct a long time ago.
 * iprt/types.h before sys/param.h - prevents UINT32_C and friends.
 */
#include <iprt/types.h>
#include <sys/param.h>
#undef PVM

#define LOG_GROUP LOG_GROUP_NET_ADP_DRV
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/version.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/alloca.h>

#include <sys/systm.h>
RT_C_DECLS_BEGIN /* Buggy 10.4 headers, fixed in 10.5. */
#include <sys/kpi_mbuf.h>
RT_C_DECLS_END

#include <net/ethernet.h>
#include <net/if_ether.h>
#include <net/if_types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <miscfs/devfs/devfs.h>
extern "C" {
#include <net/bpf.h>
}

#define VBOXNETADP_OS_SPECFIC 1
#include "../VBoxNetAdpInternal.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The maximum number of SG segments.
 * Used to prevent stack overflow and similar bad stuff. */
#define VBOXNETADP_DARWIN_MAX_SEGS       32
#define VBOXNETADP_DARWIN_MAX_FAMILIES   4
#define VBOXNETADP_DARWIN_NAME           "vboxnet"
#define VBOXNETADP_DARWIN_MTU            1500
#define VBOXNETADP_DARWIN_DETACH_TIMEOUT 500

#define VBOXNETADP_FROM_IFACE(iface) ((PVBOXNETADP) ifnet_softc(iface))

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
RT_C_DECLS_BEGIN
static kern_return_t    VBoxNetAdpDarwinStart(struct kmod_info *pKModInfo, void *pvData);
static kern_return_t    VBoxNetAdpDarwinStop(struct kmod_info *pKModInfo, void *pvData);
RT_C_DECLS_END

static int VBoxNetAdpDarwinOpen(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess);
static int VBoxNetAdpDarwinClose(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess);
static int VBoxNetAdpDarwinIOCtl(dev_t Dev, u_long iCmd, caddr_t pData, int fFlags, struct proc *pProcess);

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Declare the module stuff.
 */
RT_C_DECLS_BEGIN
extern kern_return_t _start(struct kmod_info *pKModInfo, void *pvData);
extern kern_return_t _stop(struct kmod_info *pKModInfo, void *pvData);

KMOD_EXPLICIT_DECL(VBoxNetAdp, VBOX_VERSION_STRING, _start, _stop)
DECLHIDDEN(kmod_start_func_t *) _realmain = VBoxNetAdpDarwinStart;
DECLHIDDEN(kmod_stop_func_t  *) _antimain = VBoxNetAdpDarwinStop;
DECLHIDDEN(int)                 _kext_apple_cc = __APPLE_CC__;
RT_C_DECLS_END

/**
 * The (common) global data.
 */
static int   g_nCtlDev = -1; /* Major dev number */
static void *g_hCtlDev = 0;  /* FS dev handle */

/**
 * The character device switch table for the driver.
 */
static struct cdevsw    g_ChDev =
{
    /*.d_open     = */VBoxNetAdpDarwinOpen,
    /*.d_close    = */VBoxNetAdpDarwinClose,
    /*.d_read     = */eno_rdwrt,
    /*.d_write    = */eno_rdwrt,
    /*.d_ioctl    = */VBoxNetAdpDarwinIOCtl,
    /*.d_stop     = */eno_stop,
    /*.d_reset    = */eno_reset,
    /*.d_ttys     = */NULL,
    /*.d_select   = */eno_select,
    /*.d_mmap     = */eno_mmap,
    /*.d_strategy = */eno_strat,
    /*.d_getc     = */eno_getc,
    /*.d_putc     = */eno_putc,
    /*.d_type     = */0
};



static void vboxNetAdpDarwinComposeUUID(PVBOXNETADP pThis, PRTUUID pUuid)
{
    /* Generate UUID from name and MAC address. */
    RTUuidClear(pUuid);
    memcpy(pUuid->au8, "vboxnet", 7);
    pUuid->Gen.u8ClockSeqHiAndReserved = (pUuid->Gen.u8ClockSeqHiAndReserved & 0x3f) | 0x80;
    pUuid->Gen.u16TimeHiAndVersion = (pUuid->Gen.u16TimeHiAndVersion & 0x0fff) | 0x4000;
    pUuid->Gen.u8ClockSeqLow = pThis->iUnit;
    vboxNetAdpComposeMACAddress(pThis, (PRTMAC)pUuid->Gen.au8Node);
}

static errno_t vboxNetAdpDarwinOutput(ifnet_t pIface, mbuf_t pMBuf)
{
    PVBOXNETADP pThis = VBOXNETADP_FROM_IFACE(pIface);
    Assert(pThis);
    if (pThis->u.s.nTapMode & BPF_MODE_OUTPUT)
    {
        Log2(("vboxnetadp: out len=%d\n%.*Rhxd\n", mbuf_len(pMBuf), 14, mbuf_data(pMBuf)));
        bpf_tap_out(pIface, DLT_EN10MB, pMBuf, NULL, 0);
    }
    mbuf_freem_list(pMBuf);
    return 0;
}

static void vboxNetAdpDarwinAttachFamily(PVBOXNETADP pThis, protocol_family_t Family)
{
    u_int32_t i;
    for (i = 0; i < VBOXNETADP_MAX_FAMILIES; i++)
        if (pThis->u.s.aAttachedFamilies[i] == 0)
        {
            pThis->u.s.aAttachedFamilies[i] = Family;
            break;
        }
}

static void vboxNetAdpDarwinDetachFamily(PVBOXNETADP pThis, protocol_family_t Family)
{
    u_int32_t i;
    for (i = 0; i < VBOXNETADP_MAX_FAMILIES; i++)
        if (pThis->u.s.aAttachedFamilies[i] == Family)
            pThis->u.s.aAttachedFamilies[i] = 0;
}

static errno_t vboxNetAdpDarwinAddProto(ifnet_t pIface, protocol_family_t Family, const struct ifnet_demux_desc *pDemuxDesc, u_int32_t nDesc)
{
    PVBOXNETADP pThis = VBOXNETADP_FROM_IFACE(pIface);
    Assert(pThis);
    vboxNetAdpDarwinAttachFamily(pThis, Family);
    LogFlow(("vboxNetAdpAddProto: Family=%d.\n", Family));
    return ether_add_proto(pIface, Family, pDemuxDesc, nDesc);
}

static errno_t vboxNetAdpDarwinDelProto(ifnet_t pIface, protocol_family_t Family)
{
    PVBOXNETADP pThis = VBOXNETADP_FROM_IFACE(pIface);
    Assert(pThis);
    LogFlow(("vboxNetAdpDelProto: Family=%d.\n", Family));
    vboxNetAdpDarwinDetachFamily(pThis, Family);
    return ether_del_proto(pIface, Family);
}

static void vboxNetAdpDarwinDetach(ifnet_t pIface)
{
    PVBOXNETADP pThis = VBOXNETADP_FROM_IFACE(pIface);
    Assert(pThis);
    Log2(("vboxNetAdpDarwinDetach: Signaling detach to vboxNetAdpUnregisterDevice.\n"));
    /* Let vboxNetAdpDarwinUnregisterDevice know that the interface has been detached. */
    RTSemEventSignal(pThis->u.s.hEvtDetached);
}

static errno_t vboxNetAdpDarwinDemux(ifnet_t pIface, mbuf_t pMBuf,
                                     char *pFrameHeader,
                                     protocol_family_t *pProtocolFamily)
{
    PVBOXNETADP pThis = VBOXNETADP_FROM_IFACE(pIface);
    Assert(pThis);
    Log2(("vboxNetAdpDarwinDemux: mode=%d\n", pThis->u.s.nTapMode));
    if (pThis->u.s.nTapMode & BPF_MODE_INPUT)
    {
        Log2(("vboxnetadp: in len=%d\n%.*Rhxd\n", mbuf_len(pMBuf), 14, pFrameHeader));
        bpf_tap_in(pIface, DLT_EN10MB, pMBuf, pFrameHeader, ETHER_HDR_LEN);
    }
    return ether_demux(pIface, pMBuf, pFrameHeader, pProtocolFamily);
}

static errno_t vboxNetAdpDarwinBpfTap(ifnet_t pIface, u_int32_t uLinkType, bpf_tap_mode nMode)
{
    PVBOXNETADP pThis = VBOXNETADP_FROM_IFACE(pIface);
    Assert(pThis);
    Log2(("vboxNetAdpDarwinBpfTap: mode=%d\n", nMode));
    pThis->u.s.nTapMode = nMode;
    return 0;
}

static errno_t vboxNetAdpDarwinBpfSend(ifnet_t pIface, u_int32_t uLinkType, mbuf_t pMBuf)
{
    LogRel(("vboxnetadp: BPF send function is not implemented (dlt=%d)\n", uLinkType));
    mbuf_freem_list(pMBuf);
    return 0;
}

int vboxNetAdpOsCreate(PVBOXNETADP pThis, PCRTMAC pMACAddress)
{
    int rc;
    struct ifnet_init_params Params;
    RTUUID uuid;
    struct sockaddr_dl mac;

    pThis->u.s.hEvtDetached = NIL_RTSEMEVENT;
    rc = RTSemEventCreate(&pThis->u.s.hEvtDetached);
    if (RT_FAILURE(rc))
    {
        printf("vboxNetAdpOsCreate: failed to create semaphore (rc=%d).\n", rc);
        return rc;
    }

    pThis->u.s.nTapMode = BPF_MODE_DISABLED;

    mac.sdl_len = sizeof(mac);
    mac.sdl_family = AF_LINK;
    mac.sdl_alen = ETHER_ADDR_LEN;
    mac.sdl_nlen = 0;
    mac.sdl_slen = 0;
    memcpy(LLADDR(&mac), pMACAddress->au8, mac.sdl_alen);

    RTStrPrintf(pThis->szName, VBOXNETADP_MAX_NAME_LEN, "%s%d", VBOXNETADP_NAME, pThis->iUnit);
    vboxNetAdpDarwinComposeUUID(pThis, &uuid);
    Params.uniqueid = uuid.au8;
    Params.uniqueid_len = sizeof(uuid);
    Params.name = VBOXNETADP_NAME;
    Params.unit = pThis->iUnit;
    Params.family = IFNET_FAMILY_ETHERNET;
    Params.type = IFT_ETHER;
    Params.output = vboxNetAdpDarwinOutput;
    Params.demux = vboxNetAdpDarwinDemux;
    Params.add_proto = vboxNetAdpDarwinAddProto;
    Params.del_proto = vboxNetAdpDarwinDelProto;
    Params.check_multi = ether_check_multi;
    Params.framer = ether_frameout;
    Params.softc = pThis;
    Params.ioctl = (ifnet_ioctl_func)ether_ioctl;
    Params.set_bpf_tap = NULL;
    Params.detach = vboxNetAdpDarwinDetach;
    Params.event = NULL;
    Params.broadcast_addr = "\xFF\xFF\xFF\xFF\xFF\xFF";
    Params.broadcast_len = ETHER_ADDR_LEN;

    errno_t err = ifnet_allocate(&Params, &pThis->u.s.pIface);
    if (!err)
    {
        err = ifnet_attach(pThis->u.s.pIface, &mac);
        if (!err)
        {
            err = bpf_attach(pThis->u.s.pIface, DLT_EN10MB, ETHER_HDR_LEN,
                      vboxNetAdpDarwinBpfSend, vboxNetAdpDarwinBpfTap);
            if (err)
            {
                LogRel(("vboxnetadp: bpf_attach failed with %d\n", err));
            }
            err = ifnet_set_flags(pThis->u.s.pIface, IFF_RUNNING | IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST, 0xFFFF);
            if (!err)
            {
                ifnet_set_mtu(pThis->u.s.pIface, VBOXNETADP_MTU);
                Log2(("vboxnetadp: created interface %s (%p), pThis=%p, pMagic=%p, *pMagic=%x(%x)\n",
                        pThis->szName, pThis->u.s.pIface, pThis, ifnet_softc(pThis->u.s.pIface), *(uint32_t*)ifnet_softc(pThis->u.s.pIface), pThis->uMagic));
                return VINF_SUCCESS;
            }
            else
                Log(("vboxNetAdpDarwinRegisterDevice: Failed to set flags (err=%d).\n", err));
            ifnet_detach(pThis->u.s.pIface);
        }
        else
            Log(("vboxNetAdpDarwinRegisterDevice: Failed to attach to interface (err=%d).\n", err));
        ifnet_release(pThis->u.s.pIface);
    }
    else
        Log(("vboxNetAdpDarwinRegisterDevice: Failed to allocate interface (err=%d).\n", err));

    RTSemEventDestroy(pThis->u.s.hEvtDetached);
    pThis->u.s.hEvtDetached = NIL_RTSEMEVENT;

    return RTErrConvertFromErrno(err);
}

void vboxNetAdpOsDestroy(PVBOXNETADP pThis)
{
    u_int32_t i;
    /* Bring down the interface */
    int rc = VINF_SUCCESS;
    errno_t err;

    AssertPtr(pThis->u.s.pIface);
    Assert(pThis->u.s.hEvtDetached != NIL_RTSEMEVENT);

    err = ifnet_set_flags(pThis->u.s.pIface, 0, IFF_UP | IFF_RUNNING);
    if (err)
        Log(("vboxNetAdpDarwinUnregisterDevice: Failed to bring down interface "
             "(err=%d).\n", err));
    /* Detach all protocols. */
    for (i = 0; i < VBOXNETADP_MAX_FAMILIES; i++)
        if (pThis->u.s.aAttachedFamilies[i])
            ifnet_detach_protocol(pThis->u.s.pIface, pThis->u.s.aAttachedFamilies[i]);
    err = ifnet_detach(pThis->u.s.pIface);
    if (err)
        Log(("vboxNetAdpDarwinUnregisterDevice: Failed to detach interface "
             "(err=%d).\n", err));
    Log2(("vboxNetAdpDarwinUnregisterDevice: Waiting for 'detached' event...\n"));
    /* Wait until we get a signal from detach callback. */
    rc = RTSemEventWait(pThis->u.s.hEvtDetached, VBOXNETADP_DETACH_TIMEOUT);
    if (rc == VERR_TIMEOUT)
        LogRel(("VBoxAdpDrv: Failed to detach interface %s%d\n.",
                VBOXNETADP_NAME, pThis->iUnit));
    err = ifnet_release(pThis->u.s.pIface);
    if (err)
        Log(("vboxNetAdpUnregisterDevice: Failed to release interface (err=%d).\n", err));

    RTSemEventDestroy(pThis->u.s.hEvtDetached);
    pThis->u.s.hEvtDetached = NIL_RTSEMEVENT;
}

/**
 * Device open. Called on open /dev/vboxnetctl
 *
 * @param   pInode      Pointer to inode info structure.
 * @param   pFilp       Associated file pointer.
 */
static int VBoxNetAdpDarwinOpen(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess)
{
    char szName[128];
    szName[0] = '\0';
    proc_name(proc_pid(pProcess), szName, sizeof(szName));
    Log(("VBoxNetAdpDarwinOpen: pid=%d '%s'\n", proc_pid(pProcess), szName));
    return 0;
}

/**
 * Close device.
 */
static int VBoxNetAdpDarwinClose(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess)
{
    Log(("VBoxNetAdpDarwinClose: pid=%d\n", proc_pid(pProcess)));
    return 0;
}

/**
 * Device I/O Control entry point.
 *
 * @returns Darwin for slow IOCtls and VBox status code for the fast ones.
 * @param   Dev         The device number (major+minor).
 * @param   iCmd        The IOCtl command.
 * @param   pData       Pointer to the data (if any it's a SUPDRVIOCTLDATA (kernel copy)).
 * @param   fFlags      Flag saying we're a character device (like we didn't know already).
 * @param   pProcess    The process issuing this request.
 */
static int VBoxNetAdpDarwinIOCtl(dev_t Dev, u_long iCmd, caddr_t pData, int fFlags, struct proc *pProcess)
{
    uint32_t cbReq = IOCPARM_LEN(iCmd);
    PVBOXNETADPREQ pReq = (PVBOXNETADPREQ)pData;
    int rc;

    Log(("VBoxNetAdpDarwinIOCtl: param len %#x; iCmd=%#lx\n", cbReq, iCmd));
    switch (IOCBASECMD(iCmd))
    {
        case IOCBASECMD(VBOXNETADP_CTL_ADD):
        {
            if (   (IOC_DIRMASK & iCmd) != IOC_INOUT
                || cbReq < sizeof(VBOXNETADPREQ))
                return EINVAL;

            PVBOXNETADP pNew;
            Log(("VBoxNetAdpDarwinIOCtl: szName=%s\n", pReq->szName));
            rc = vboxNetAdpCreate(&pNew,
                                  pReq->szName[0] && RTStrEnd(pReq->szName, RT_MIN(cbReq, sizeof(pReq->szName))) ?
                                  pReq->szName : NULL);
            if (RT_FAILURE(rc))
                return EINVAL;

            Assert(strlen(pReq->szName) < sizeof(pReq->szName));
            strncpy(pReq->szName, pNew->szName, sizeof(pReq->szName) - 1);
            pReq->szName[sizeof(pReq->szName) - 1] = '\0';
            Log(("VBoxNetAdpDarwinIOCtl: Added '%s'\n", pReq->szName));
            break;
        }

        case IOCBASECMD(VBOXNETADP_CTL_REMOVE):
        {
            if (!RTStrEnd(pReq->szName, RT_MIN(cbReq, sizeof(pReq->szName))))
                return EINVAL;

            PVBOXNETADP pAdp = vboxNetAdpFindByName(pReq->szName);
            if (!pAdp)
                return EINVAL;

            rc = vboxNetAdpDestroy(pAdp);
            if (RT_FAILURE(rc))
                return EINVAL;
            Log(("VBoxNetAdpDarwinIOCtl: Removed %s\n", pReq->szName));
            break;
        }

        default:
            printf("VBoxNetAdpDarwinIOCtl: unknown command %lx.\n", IOCBASECMD(iCmd));
            return EINVAL;
    }

    return 0;
}

int  vboxNetAdpOsInit(PVBOXNETADP pThis)
{
    /*
     * Init the darwin specific members.
     */
    pThis->u.s.pIface = NULL;
    pThis->u.s.hEvtDetached = NIL_RTSEMEVENT;
    memset(pThis->u.s.aAttachedFamilies, 0, sizeof(pThis->u.s.aAttachedFamilies));

    return VINF_SUCCESS;
}

/**
 * Start the kernel module.
 */
static kern_return_t    VBoxNetAdpDarwinStart(struct kmod_info *pKModInfo, void *pvData)
{
    int rc;

    /*
     * Initialize IPRT and find our module tag id.
     * (IPRT is shared with VBoxDrv, it creates the loggers.)
     */
    rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        Log(("VBoxNetAdpDarwinStart\n"));
        rc = vboxNetAdpInit();
        if (RT_SUCCESS(rc))
        {
            g_nCtlDev = cdevsw_add(-1, &g_ChDev);
            if (g_nCtlDev < 0)
            {
                LogRel(("VBoxAdp: failed to register control device."));
                rc = VERR_CANT_CREATE;
            }
            else
            {
                g_hCtlDev = devfs_make_node(makedev(g_nCtlDev, 0), DEVFS_CHAR,
                                            UID_ROOT, GID_WHEEL, 0600, VBOXNETADP_CTL_DEV_NAME);
                if (!g_hCtlDev)
                {
                    LogRel(("VBoxAdp: failed to create FS node for control device."));
                    rc = VERR_CANT_CREATE;
                }
            }
        }

        if (RT_SUCCESS(rc))
        {
            LogRel(("VBoxAdpDrv: version " VBOX_VERSION_STRING " r%d\n", VBOX_SVN_REV));
            return KMOD_RETURN_SUCCESS;
        }

        LogRel(("VBoxAdpDrv: failed to initialize device extension (rc=%d)\n", rc));
        RTR0Term();
    }
    else
        printf("VBoxAdpDrv: failed to initialize IPRT (rc=%d)\n", rc);

    return KMOD_RETURN_FAILURE;
}


/**
 * Stop the kernel module.
 */
static kern_return_t VBoxNetAdpDarwinStop(struct kmod_info *pKModInfo, void *pvData)
{
    Log(("VBoxNetAdpDarwinStop\n"));

    vboxNetAdpShutdown();
    /* Remove control device */
    devfs_remove(g_hCtlDev);
    cdevsw_remove(g_nCtlDev, &g_ChDev);

    RTR0Term();

    return KMOD_RETURN_SUCCESS;
}
