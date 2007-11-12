/** $Id$ */
/** @file
 * Universial TAP network transport driver.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define ASYNC_NET

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_TUN
#include <VBox/log.h>
#include <VBox/pdmdrv.h>

#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/path.h>
#ifdef ASYNC_NET
# include <iprt/thread.h>
# include <iprt/asm.h>
# include <iprt/semaphore.h>
#endif
#ifdef RT_OS_SOLARIS
# include <iprt/process.h>
# include <iprt/env.h>
# ifdef VBOX_WITH_CROSSBOW
#  include <iprt/mem.h>
# endif
#endif

#include <sys/ioctl.h>
#include <sys/poll.h>
#ifdef RT_OS_SOLARIS
# include <sys/stat.h>
# include <sys/ethernet.h>
# include <sys/sockio.h>
# include <netinet/in.h>
# include <netinet/in_systm.h>
# include <netinet/ip.h>
# include <netinet/ip_icmp.h>
# include <netinet/udp.h>
# include <netinet/tcp.h>
# include <net/if.h>
# include <stropts.h>
# include <fcntl.h>
# include <ctype.h>
# include <stdlib.h>
# ifdef VBOX_WITH_CROSSBOW
#  include <limits.h>
#  include <libdlpi.h>
#  include <libdlvnic.h>
# endif
#else
# include <sys/fcntl.h>
#endif
#include <errno.h>
#ifdef ASYNC_NET
# include <unistd.h>
#endif

#ifdef RT_OS_L4
# include <l4/vboxserver/file.h>
#endif

#include "Builtins.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef enum ASYNCSTATE
{
    //ASYNCSTATE_SUSPENDED = 1,
    ASYNCSTATE_RUNNING,
    ASYNCSTATE_TERMINATE
} ASYNCSTATE;

/**
 * Block driver instance data.
 */
typedef struct DRVTAP
{
    /** The network interface. */
    PDMINETWORKCONNECTOR    INetworkConnector;
    /** The network interface. */
    PPDMINETWORKPORT        pPort;
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
    /** TAP device file handle. */
    RTFILE                  FileDevice;
    /** The configured TAP device name. */
    char                   *pszDeviceName;
#ifdef RT_OS_SOLARIS
    /** The actual TAP/VNIC device name. */
    char                   *pszDeviceNameActual;
# ifdef VBOX_WITH_CROSSBOW
    /** Crossbow: MAC address of the device. */
    PDMMAC                  MacAddress;
    /** Crossbow: Handle of the NIC. */
    dlpi_handle_t           pDeviceHandle;
    /** Crossbow: ID of the virtual NIC. */
    uint_t                  uDeviceID;
# else
    /** IP device file handle (/dev/udp). */
    RTFILE                  IPFileDevice;
# endif
#endif
    /** TAP setup application. */
    char                   *pszSetupApplication;
    /** TAP terminate application. */
    char                   *pszTerminateApplication;
#ifdef ASYNC_NET
    /** The write end of the control pipe. */
    RTFILE                  PipeWrite;
    /** The read end of the control pipe. */
    RTFILE                  PipeRead;
    /** The thread state. */
    ASYNCSTATE volatile     enmState;
    /** Reader thread. */
    RTTHREAD                Thread;
    /** We are waiting for more receive buffers. */
    uint32_t volatile       fOutOfSpace;
    /** Event semaphore for blocking on receive. */
    RTSEMEVENT              EventOutOfSpace;
#endif

#ifdef VBOX_WITH_STATISTICS
    /** Number of sent packets. */
    STAMCOUNTER             StatPktSent;
    /** Number of sent bytes. */
    STAMCOUNTER             StatPktSentBytes;
    /** Number of received packets. */
    STAMCOUNTER             StatPktRecv;
    /** Number of received bytes. */
    STAMCOUNTER             StatPktRecvBytes;
    /** Profiling packet transmit runs. */
    STAMPROFILE             StatTransmit;
    /** Profiling packet receive runs. */
    STAMPROFILEADV          StatReceive;
#ifdef ASYNC_NET
    STAMPROFILE             StatRecvOverflows;
#endif
#endif /* VBOX_WITH_STATISTICS */

#ifdef LOG_ENABLED
    /** The nano ts of the last transfer. */
    uint64_t                u64LastTransferTS;
    /** The nano ts of the last receive. */
    uint64_t                u64LastReceiveTS;
#endif
} DRVTAP, *PDRVTAP;


/** Converts a pointer to TAP::INetworkConnector to a PRDVTAP. */
#define PDMINETWORKCONNECTOR_2_DRVTAP(pInterface) ( (PDRVTAP)((uintptr_t)pInterface - RT_OFFSETOF(DRVTAP, INetworkConnector)) )


/*******************************************************************************
*   Internal Functions & Globals                                               *
*******************************************************************************/
#ifdef RT_OS_SOLARIS
# ifdef VBOX_WITH_CROSSBOW
static int              SolarisCreateVNIC(PDRVTAP pData);
static int              SolarisGetNIC(char *pszNICName, size_t cbSize);
static int              SolarisOpenNIC(PDRVTAP pData, const char *pszNICName);
static int              SolarisDLPIErr2VBoxErr(int rc);
# else
static int              SolarisTAPAttach(PPDMDRVINS pDrvIns);
# endif
#endif


/**
 * Send data to the network.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pvBuf           Data to send.
 * @param   cb              Number of bytes to send.
 * @thread  EMT
 */
static DECLCALLBACK(int) drvTAPSend(PPDMINETWORKCONNECTOR pInterface, const void *pvBuf, size_t cb)
{
    PDRVTAP pData = PDMINETWORKCONNECTOR_2_DRVTAP(pInterface);
    STAM_COUNTER_INC(&pData->StatPktSent);
    STAM_COUNTER_ADD(&pData->StatPktSentBytes, cb);
    STAM_PROFILE_START(&pData->StatTransmit, a);

#ifdef LOG_ENABLED
    uint64_t u64Now = RTTimeProgramNanoTS();
    LogFlow(("drvTAPSend: %-4d bytes at %llu ns  deltas: r=%llu t=%llu\n",
             cb, u64Now, u64Now - pData->u64LastReceiveTS, u64Now - pData->u64LastTransferTS));
    pData->u64LastTransferTS = u64Now;
#endif
    Log2(("drvTAPSend: pvBuf=%p cb=%#x\n"
          "%.*Vhxd\n",
          pvBuf, cb, cb, pvBuf));

    int rc = RTFileWrite(pData->FileDevice, pvBuf, cb, NULL);

    STAM_PROFILE_STOP(&pData->StatTransmit, a);
    AssertRC(rc);
    return rc;
}


/**
 * Set promiscuous mode.
 *
 * This is called when the promiscuous mode is set. This means that there doesn't have
 * to be a mode change when it's called.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   fPromiscuous    Set if the adaptor is now in promiscuous mode. Clear if it is not.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvTAPSetPromiscuousMode(PPDMINETWORKCONNECTOR pInterface, bool fPromiscuous)
{
    LogFlow(("drvTAPSetPromiscuousMode: fPromiscuous=%d\n", fPromiscuous));
    /* nothing to do */
}


/**
 * Notification on link status changes.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   enmLinkState    The new link state.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvTAPNotifyLinkChanged(PPDMINETWORKCONNECTOR pInterface, PDMNETWORKLINKSTATE enmLinkState)
{
    LogFlow(("drvNATNotifyLinkChanged: enmLinkState=%d\n", enmLinkState));
    /** @todo take action on link down and up. Stop the polling and such like. */
}


/**
 * More receive buffer has become available.
 *
 * This is called when the NIC frees up receive buffers.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvTAPNotifyCanReceive(PPDMINETWORKCONNECTOR pInterface)
{
    PDRVTAP pData = PDMINETWORKCONNECTOR_2_DRVTAP(pInterface);

    LogFlow(("drvTAPNotifyCanReceive:\n"));
    /** @todo r=bird: With a bit unfavorable scheduling it's possible to get here
     * before fOutOfSpace is set by the overflow code. This will mean that, unless
     * more receive descriptors become available, the receive thread will be stuck
     * until it times out and cause a hickup in the network traffic.
     * There is a simple, but not perfect, workaround for this problem in DrvTAPOs2.cpp.
     *
     * A better solution would be to ditch the NotifyCanReceive callback and instead
     * change the CanReceive to do all the work. This will reduce the amount of code
     * duplication, and would permit pcnet to avoid queuing unnecessary ring-3 tasks.
     */

    /* ensure we wake up only once */
    if (ASMAtomicXchgU32(&pData->fOutOfSpace, false))
        RTSemEventSignal(pData->EventOutOfSpace);
}


#ifdef ASYNC_NET
/**
 * Asynchronous I/O thread for handling receive.
 *
 * @returns VINF_SUCCESS (ignored).
 * @param   Thread          Thread handle.
 * @param   pvUser          Pointer to a DRVTAP structure.
 */
static DECLCALLBACK(int) drvTAPAsyncIoThread(RTTHREAD ThreadSelf, void *pvUser)
{
    PDRVTAP pData = (PDRVTAP)pvUser;
    LogFlow(("drvTAPAsyncIoThread: pData=%p\n", pData));
    STAM_PROFILE_ADV_START(&pData->StatReceive, a);

    int rc = RTSemEventCreate(&pData->EventOutOfSpace);
    AssertRC(rc);

    /*
     * Polling loop.
     */
    for (;;)
    {
        /*
         * Wait for something to become available.
         */
        struct pollfd aFDs[2];
        aFDs[0].fd      = pData->FileDevice;
        aFDs[0].events  = POLLIN | POLLPRI;
        aFDs[0].revents = 0;
        aFDs[1].fd      = pData->PipeRead;
        aFDs[1].events  = POLLIN | POLLPRI | POLLERR | POLLHUP;
        aFDs[1].revents = 0;
        STAM_PROFILE_ADV_STOP(&pData->StatReceive, a);
        errno=0;
        rc = poll(&aFDs[0], ELEMENTS(aFDs), -1 /* infinite */);
        STAM_PROFILE_ADV_START(&pData->StatReceive, a);
        if (    rc > 0
            &&  (aFDs[0].revents & (POLLIN | POLLPRI))
            &&  !aFDs[1].revents)
        {
            /*
             * Read the frame.
             */
            char achBuf[4096];
            size_t cbRead = 0;
#ifdef VBOX_WITH_CROSSBOW
            rc = VINF_SUCCESS;
            cbRead = sizeof(achBuf);
            int rc2 = dlpi_recv(pData->pDeviceHandle, NULL, NULL, achBuf, &cbRead, -1, NULL);
            if (rc2 != DLPI_SUCCESS)
                rc = SolarisDLPIErr2VBoxErr(rc2);
#else
            rc = RTFileRead(pData->FileDevice, achBuf, sizeof(achBuf), &cbRead);
#endif
            if (VBOX_SUCCESS(rc))
            {
                AssertMsg(cbRead <= 1536, ("cbRead=%d\n", cbRead));

                /*
                 * Wait for the device to have space for this frame.
                 * Most guests use frame-sized receive buffers, hence non-zero cbMax
                 * automatically means there is enough room for entire frame. Some
                 * guests (eg. Solaris) use large chains of small receive buffers
                 * (each 128 or so bytes large). We will still start receiving as soon
                 * as cbMax is non-zero because:
                 *  - it would be quite expensive for pfnCanReceive to accurately
                 *    determine free receive buffer space
                 *  - if we were waiting for enough free buffers, there is a risk
                 *    of deadlocking because the guest could be waiting for a receive
                 *    overflow error to allocate more receive buffers
                 */
                size_t cbMax = pData->pPort->pfnCanReceive(pData->pPort);
                if (cbMax == 0)
                {
                    /** @todo receive overflow handling needs serious improving! */
                    STAM_PROFILE_ADV_STOP(&pData->StatReceive, a);
                    STAM_PROFILE_START(&pData->StatRecvOverflows, b);
                    while (   cbMax == 0
                           && pData->enmState != ASYNCSTATE_TERMINATE)
                    {
                        LogFlow(("drvTAPAsyncIoThread: cbMax=%d cbRead=%d waiting...\n", cbMax, cbRead));
#if 1
                        /* We get signalled by the network driver. 50ms is just for sanity */
                        ASMAtomicXchgU32(&pData->fOutOfSpace, true);
                        RTSemEventWait(pData->EventOutOfSpace, 50);
#else
                        RTThreadSleep(1);
#endif
                        cbMax = pData->pPort->pfnCanReceive(pData->pPort);
                    }
                    ASMAtomicXchgU32(&pData->fOutOfSpace, false);
                    STAM_PROFILE_STOP(&pData->StatRecvOverflows, b);
                    STAM_PROFILE_ADV_START(&pData->StatReceive, a);
                    if (pData->enmState == ASYNCSTATE_TERMINATE)
                        break;
                }

                /*
                 * Pass the data up.
                 */
#ifdef LOG_ENABLED
                uint64_t u64Now = RTTimeProgramNanoTS();
                LogFlow(("drvTAPAsyncIoThread: %-4d bytes at %llu ns  deltas: r=%llu t=%llu\n",
                         cbRead, u64Now, u64Now - pData->u64LastReceiveTS, u64Now - pData->u64LastTransferTS));
                pData->u64LastReceiveTS = u64Now;
#endif
                Log2(("drvTAPAsyncIoThread: cbRead=%#x\n"
                      "%.*Vhxd\n",
                      cbRead, cbRead, achBuf));
                STAM_COUNTER_INC(&pData->StatPktRecv);
                STAM_COUNTER_ADD(&pData->StatPktRecvBytes, cbRead);
                rc = pData->pPort->pfnReceive(pData->pPort, achBuf, cbRead);
                AssertRC(rc);
            }
            else
            {
                LogFlow(("drvTAPAsyncIoThread: RTFileRead -> %Vrc\n", rc));
                if (rc == VERR_INVALID_HANDLE)
                    break;
                RTThreadYield();
            }
        }
        else if (   rc > 0
                 && aFDs[1].revents)
        {
            LogFlow(("drvTAPAsyncIoThread: Control message: enmState=%d revents=%#x\n", pData->enmState, aFDs[1].revents));
            if (pData->enmState == ASYNCSTATE_TERMINATE)
                break;
            if (aFDs[1].revents & (POLLHUP | POLLERR | POLLNVAL))
                break;

            /* drain the pipe */
            char ch;
            size_t cbRead;
            RTFileRead(pData->PipeRead, &ch, 1, &cbRead);
        }
        else
        {
            /*
             * poll() failed for some reason. Yield to avoid eating too much CPU.
             *
             * EINTR errors have been seen frequently. They should be harmless, even
             * if they are not supposed to occur in our setup.
             */
            if (errno == EINTR)
                Log(("rc=%d revents=%#x,%#x errno=%p %s\n", rc, aFDs[0].revents, aFDs[1].revents, errno, strerror(errno)));
            else
                AssertMsgFailed(("rc=%d revents=%#x,%#x errno=%p %s\n", rc, aFDs[0].revents, aFDs[1].revents, errno, strerror(errno)));
            RTThreadYield();
        }
    }

    rc = RTSemEventDestroy(pData->EventOutOfSpace);
    AssertRC(rc);

    LogFlow(("drvTAPAsyncIoThread: returns %Vrc\n", VINF_SUCCESS));
    STAM_PROFILE_ADV_STOP(&pData->StatReceive, a);
    return VINF_SUCCESS;
}

#else
/**
 * Poller callback.
 */
static DECLCALLBACK(void) drvTAPPoller(PPDMDRVINS pDrvIns)
{
    /* check how much the device/driver can receive now. */
    PDRVTAP pData = PDMINS2DATA(pDrvIns, PDRVTAP);
    STAM_PROFILE_ADV_START(&pData->StatReceive, a);

    size_t  cbMax = pData->pPort->pfnCanReceive(pData->pPort);
    while (cbMax > 0)
    {
        /* check for data to read */
        struct pollfd aFDs[1];
        aFDs[0].fd      = pData->FileDevice;
        aFDs[0].events  = POLLIN | POLLPRI;
        aFDs[0].revents = 0;
        if (poll(&aFDs[0], 1, 0) > 0)
        {
            if (aFDs[0].revents & (POLLIN | POLLPRI))
            {
                /* data waiting, read it. */
                char        achBuf[4096];
                size_t      cbRead = 0;
#ifdef VBOX_WITH_CROSSBOW
                int rc = VINF_SUCCESS;
                cbRead = sizeof(achBuf);
                int rc2 = dlpi_recv(pData->pDeviceHandle, NULL, NULL, achBuf, &cbRead, -1, NULL);
                if (rc2 != DLPI_SUCCESS)
                    rc = SolarisDLPIErr2VBoxErr(rc2);
#else
                int rc = RTFileRead(pData->FileDevice, achBuf, RT_MIN(sizeof(achBuf), cbMax), &cbRead);
#endif
                if (VBOX_SUCCESS(rc))
                {
                    STAM_COUNTER_INC(&pData->StatPktRecv);
                    STAM_COUNTER_ADD(&pData->StatPktRecvBytes, cbRead);

                    /* push it up to guy over us. */
                    Log2(("drvTAPPoller: cbRead=%#x\n"
                          "%.*Vhxd\n",
                          cbRead, cbRead, achBuf));
                    rc = pData->pPort->pfnReceive(pData->pPort, achBuf, cbRead);
                    AssertRC(rc);
                }
                else
                    AssertRC(rc);
                if (VBOX_FAILURE(rc) || !cbRead)
                    break;
            }
            else
                break;
        }
        else
            break;

        cbMax = pData->pPort->pfnCanReceive(pData->pPort);
    }

    STAM_PROFILE_ADV_STOP(&pData->StatReceive, a);
}
#endif


#if defined(RT_OS_SOLARIS)
/**
 * Calls OS-specific TAP setup application/script.
 *
 * @returns VBox error code.
 * @param   pData           The instance data.
 */
static int drvTAPSetupApplication(PDRVTAP pData)
{
    char *pszArgs[3];
    pszArgs[0] = pData->pszSetupApplication;
    pszArgs[1] = pData->pszDeviceNameActual;
    pszArgs[2] = NULL;

    Log2(("Starting TAP setup application: %s %s\n", pData->pszSetupApplication, pData->pszDeviceNameActual));
    RTPROCESS pid = NIL_RTPROCESS;
    int rc = RTProcCreate(pszArgs[0], pszArgs, RTENV_DEFAULT, 0, &pid);
    if (RT_SUCCESS(rc))
    {
        RTPROCSTATUS Status;
        rc = RTProcWait(pid, 0, &Status);
        if (RT_SUCCESS(rc))
        {
            if (Status.iStatus == 0 && Status.enmReason == RTPROCEXITREASON_NORMAL)
                return VINF_SUCCESS;
            
            LogRel(("TAP#%d: Error running TAP setup application: %s\n", pData->pDrvIns->iInstance, pData->pszSetupApplication));
            return VERR_HOSTIF_INIT_FAILED;
        }
        else
        {
            LogRel(("TAP#%d: RTProcWait failed for: %s\n", pData->pDrvIns->iInstance, pData->pszSetupApplication));
            return VERR_HOSTIF_INIT_FAILED;
        }
    }
    else
    {
        /* Bad. RTProcCreate() failed! */
        LogRel(("TAP#%d: Failed to fork() process for running TAP setup application: %s\n", pData->pDrvIns->iInstance,
              pData->pszSetupApplication, strerror(errno)));
    }
    
    return VERR_HOSTIF_INIT_FAILED;    
}


/**
 * Calls OS-specific TAP terminate application/script.
 *
 * @returns VBox error code.
 * @param   pData           The instance data.
 */
static int drvTAPTerminateApplication(PDRVTAP pData)
{
    char *pszArgs[3];
    pszArgs[0] = pData->pszTerminateApplication;
    pszArgs[1] = pData->pszDeviceNameActual;
    pszArgs[2] = NULL;

    Log2(("Starting TAP terminate application: %s %s\n", pData->pszTerminateApplication, pData->pszDeviceNameActual));
    RTPROCESS pid = NIL_RTPROCESS;
    int rc = RTProcCreate(pszArgs[0], pszArgs, RTENV_DEFAULT, 0, &pid);
    if (RT_SUCCESS(rc))
    {
        RTPROCSTATUS Status;
        rc = RTProcWait(pid, 0, &Status);
        if (RT_SUCCESS(rc))
        {
            if (Status.iStatus == 0 && Status.enmReason == RTPROCEXITREASON_NORMAL)
                return VINF_SUCCESS;
            
            LogRel(("TAP#%d: Error running TAP terminate application: %s\n", pData->pDrvIns->iInstance, pData->pszTerminateApplication));
            return VERR_HOSTIF_TERM_FAILED;
        }
        else
        {
            LogRel(("TAP#%d: RTProcWait failed for: %s\n", pData->pDrvIns->iInstance, pData->pszTerminateApplication));
            return VERR_HOSTIF_INIT_FAILED;
        }
    }
    else
    {
        /* Bad. RTProcCreate() failed! */
        LogRel(("TAP#%d: Failed to fork() process for running TAP terminate application: %s\n", pData->pDrvIns->iInstance,
              pData->pszTerminateApplication, strerror(errno)));
    }
    return VERR_HOSTIF_TERM_FAILED;    
}

#endif /* RT_OS_SOLARIS */


#ifdef RT_OS_SOLARIS
# ifdef VBOX_WITH_CROSSBOW
/**
 * Crossbow: create a virtual NIC.
 *
 * @returns VBox error code.
 * @param   pData           The instance data.
 */
static int SolarisCreateVNIC(PDRVTAP pData)
{
    /*
     * Get a physical NIC.
     */
    /** @todo r=bird: I'm I right in thinking that this just gets the name of the
     * last ethernet NIC and binds us to that? If so, this really needs to be
     * a user option. On OS/2 this is passed in as 'ConnectTo', using the same name
     * is possibly a good idea even if the type is different (we need string not integer). */
    char szNICName[_LIFNAMSIZ];
    int ret = SolarisGetNIC(szNICName, sizeof(szNICName));
    if (VBOX_FAILURE(ret))
         return VERR_HOSTIF_INIT_FAILED;

    /*
     * Setup VNIC parameters.
     */
    dladm_vnic_attr_sys_t VNICAttr;
    memset(&VNICAttr, 0, sizeof(VNICAttr));
    size_t cbDestSize = sizeof(VNICAttr.va_dev_name);
    if (strlcpy(VNICAttr.va_dev_name, szNICName, cbDestSize) >= cbDestSize)
        return VERR_BUFFER_OVERFLOW;
    Assert(sizeof(struct ether_addr) == sizeof(pData->MacAddress));
    memcpy(VNICAttr.va_mac_addr, &pData->MacAddress, ETHERADDRL);
    VNICAttr.va_mac_len = ETHERADDRL;

    uint_t VnicID;
    bool fAutoID = true;
#if 0
    /* Disabled for now, since Crossbow does not entirely respect our own VNIC ID.*/
    if (pData->pszDeviceName)
    {
        size_t cch = strlen(pData->pszDeviceName);
        if (cch > 1 && isdigit(pData->pszDeviceName[cch - 1]) != 0)
        {
            VnicID = pData->pszDeviceName[cch - 1] - '0';
            fAutoID = false;
        }
    }
#endif

    /*
     * Create the VNIC.
     */
/** r=bird: The users should be able to create the vnic himself and pass it down. This would be the
 * same as the tapN interface name.  */
    uint32_t flags = DLADM_VNIC_OPT_TEMP;
    if (fAutoID)
        flags |= DLADM_VNIC_OPT_AUTOID;

    dladm_status_t rc = dladm_vnic_create(fAutoID ? 0 : VnicID, szNICName, VNIC_MAC_ADDR_TYPE_FIXED,
                           (uchar_t *)&pData->MacAddress, ETHERADDRL, &VnicID, flags);
    if (rc != DLADM_STATUS_OK)
        return PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                           N_("dladm_vnic_create() failed. NIC %s probably incorrect."), szNICName);
    
    pData->pszDeviceNameActual = NULL;
    RTStrAPrintf(&pData->pszDeviceNameActual, "vnic%u", VnicID);
    pData->uDeviceID = VnicID;

    ret = SolarisOpenNIC(pData, szNICName);
    if (VBOX_FAILURE(ret))
        return ret;
    return VINF_SUCCESS;
}


/**
 * Crossbow: Obtain a physical NIC for binding the virtual NIC.
 *
 * @returns VBox error code.
 * @param   pszNICName      Where to store the NIC name.
 * @param   cchNICName      The size of the buffer buffer pszNICName points to.
 */
static int SolarisGetNIC(char *pszNICName, size_t cchNICName)
{
    /*
     * Try and obtain the a physical NIC to bind the VNIC to.
     */
    int InetSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (RT_UNLIKELY(InetSocket == -1))
    {
        LogRel(("SolarisGetNIC: Socket creation for AF_INET family failed.\n"));
        return VERR_HOSTIF_INIT_FAILED;
    }

    int rc;
    struct lifnum IfNum;
    IfNum.lifn_family = AF_UNSPEC;
    if (ioctl(InetSocket, SIOCGLIFNUM, &IfNum) >= 0)
    {
        caddr_t pBuf = (caddr_t)RTMemAlloc(IfNum.lifn_count * sizeof(struct lifreq));
        if (pBuf)
        {
            struct lifconf IfCfg;
            memset(&IfCfg, 0, sizeof(IfCfg));
            IfCfg.lifc_family = AF_UNSPEC;
            IfCfg.lifc_buf = pBuf;
            IfCfg.lifc_len = IfNum.lifn_count * sizeof(struct lifreq);
            if (ioctl(InetSocket, SIOCGLIFCONF, &IfCfg) >= 0)
            {
                /*
                 * Loop through all NICs on the machine. We'll use the first ethernet NIC
                 * that is not a loopback interface for binding the VNIC.
                 */
                rc = VERR_GENERAL_FAILURE;    /** @todo find a better return code. */
                struct lifreq *paIf = IfCfg.lifc_req;
                int iIf = IfCfg.lifc_len / sizeof(struct lifreq);
                while (iIf-- > 0)
                    if (strncmp(paIf[iIf].lifr_name, "lo", 2) != 0)
                    {
                        dlpi_handle_t hNIC = NULL;
                        if (dlpi_open(paIf[iIf].lifr_name, &hNIC, DLPI_RAW) == DLPI_SUCCESS)
                        {
                            dlpi_info_t NICInfo;
                            int rc2 = dlpi_info(hNIC, &NICInfo, 0);
                            dlpi_close(hNIC);
                            if (    rc2 == DLPI_SUCCESS
                                &&  NICInfo.di_mactype == DL_ETHER)
                            {
                                size_t cch = strlen(paIf[iIf].lifr_name);
                                if (cch < cchNICName)
                                {
                                    memcpy(pszNICName, paIf[iIf].lifr_name, cch + 1);
                                    rc = VINF_SUCCESS;
                                }
                                else
                                    rc = VERR_BUFFER_OVERFLOW;
                                break;
                            }
                        }
                    }
            }
            else
            {
                LogRel(("SolarisGetNIC: SIOCGLIFCONF failed\n"));
                rc = VERR_HOSTIF_INIT_FAILED;
            }
            free(pBuf);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
    {
        LogRel(("SolarisGetNIC: SIOCGLIFNUM failed\n"));
        rc = VERR_HOSTIF_INIT_FAILED;
    }
    close(InetSocket);
    return rc;
}


/**
 * Crossbow: Open & configure the physical NIC.
 *
 * @returns VBox error code.
 * @param   pData           The instance data.
 * @param   pszNICName      Name of the physical NIC.
 * @param   pEtherAddr      Ethernet address to use for the VNIC.
 */
static int SolarisOpenNIC(PDRVTAP pData, const char *pszNICName)
{
    /*
     * Open & bind the NIC using the datalink provider routine.
     */
    int rc = dlpi_open(pszNICName, &pData->pDeviceHandle, DLPI_RAW);
    if (rc != DLPI_SUCCESS)
        return PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                           N_("Failed to open VNIC in raw mode."));

    /*
     * If we decide to get NIC name directly from user/env var., we will
     * need to checks here to make sure the NIC has a ethernet address.
     */
    rc = dlpi_bind(pData->pDeviceHandle, DLPI_ANY_SAP, NULL);
    if (rc == DLPI_SUCCESS)
    {
        rc = dlpi_set_physaddr(pData->pDeviceHandle, DL_CURR_PHYS_ADDR, &pData->MacAddress, ETHERADDRL);
        if (rc == DLPI_SUCCESS)
        {
            rc = dlpi_promiscon(pData->pDeviceHandle, DL_PROMISC_SAP);
            if (rc == DLPI_SUCCESS)
            {
                /* Need to use DL_PROMIS_PHYS (not multicast) as we cannot be sure what the guest needs. */
                rc = dlpi_promiscon(pData->pDeviceHandle, DL_PROMISC_PHYS);
                if (rc == DLPI_SUCCESS)
                {
                    pData->FileDevice = dlpi_fd(pData->pDeviceHandle);
                    if (pData->FileDevice >= 0)
                    {
                        Log(("SolarisOpenNIC: %s -> %d\n", pszNICName, pData->FileDevice));
                        return VINF_SUCCESS;
                    }

                    rc = PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                             N_("Failed to obtain file descriptor for VNIC."));
                }
                else
                    rc = PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                             N_("Failed to set appropriate promiscous mode."));
            }
            else
                rc = PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                         N_("Failed to activate promiscous mode for VNIC."));
        }
        else
            rc = PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                     N_("Failed to set physical address for VNIC."));
    }
    else
        rc = PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                 N_("Failed to bind VNIC."));
    dlpi_close(pData->pDeviceHandle);
    return rc;
}


/**
 * Crossbow: Delete the virtual NIC.
 *
 * @returns VBox error code.
 * @param   pData           The instance data.
 */
static int SolarisDeleteVNIC(PDRVTAP pData)
{
    if (pData->pszDeviceNameActual)
    {
        dladm_status_t rc = dladm_vnic_delete(pData->uDeviceID, DLADM_VNIC_OPT_TEMP);
        if (rc == DLADM_STATUS_OK)
            return VINF_SUCCESS;
    }
    return VERR_HOSTIF_TERM_FAILED;
}


/**
 * Crossbow: Converts a Solaris DLPI error code to a VBox error code.
 *
 * @returns corresponding VBox error code.
 * @param   rc  DLPI error code (DLPI_* defines).
 */
static int SolarisDLPIErr2VBoxErr(int rc)
{
    switch (rc)
    {
        case DLPI_SUCCESS:          return VINF_SUCCESS;
        case DLPI_EINVAL:           return VERR_INVALID_PARAMETER;
        case DLPI_ELINKNAMEINVAL:   return VERR_INVALID_NAME;
        case DLPI_EINHANDLE:        return VERR_INVALID_HANDLE;
        case DLPI_ETIMEDOUT:        return VERR_TIMEOUT;
        case DLPI_FAILURE:          return VERR_GENERAL_FAILURE;        

        case DLPI_EVERNOTSUP:
        case DLPI_EMODENOTSUP:
        case DLPI_ERAWNOTSUP:
        case DLPI_ENOTENOTSUP:
        case DLPI_EUNAVAILSAP:      return VERR_NOT_SUPPORTED;

        /*  Define VBox error codes for these, if really needed. */
        case DLPI_ENOLINK:
        case DLPI_EBADLINK:
        case DLPI_ENOTEIDINVAL:
        case DLPI_EBADMSG:
        case DLPI_ENOTSTYLE2:       return VERR_GENERAL_FAILURE;
    }

    AssertMsgFailed(("SolarisDLPIErr2VBoxErr: Unhandled error %d\n", rc));    
    return VERR_UNRESOLVED_ERROR;
}

# else  /* VBOX_WITH_CROSSBOW */

/** From net/if_tun.h, installed by Universal TUN/TAP driver */
# define TUNNEWPPA                   (('T'<<16) | 0x0001)
/** Whether to enable ARP for TAP. */
# define VBOX_SOLARIS_TAP_ARP        1

/**
 * Creates/Attaches TAP device to IP.
 *
 * @returns VBox error code.
 * @param   pDrvIns          The driver instance data.
 * @param   pszDevName       Pointer to device name.
 */
static DECLCALLBACK(int) SolarisTAPAttach(PPDMDRVINS pDrvIns)
{
    PDRVTAP pData = PDMINS2DATA(pDrvIns, PDRVTAP);
    LogFlow(("SolarisTapAttach: pData=%p\n", pData));


    int IPFileDes = open("/dev/udp", O_RDWR, 0);
    if (IPFileDes < 0)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_HIF_OPEN_FAILED, RT_SRC_POS,
                                   N_("Failed to open /dev/udp. errno=%d"), errno);

    int TapFileDes = open("/dev/tap", O_RDWR, 0);
    if (TapFileDes < 0)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_HIF_OPEN_FAILED, RT_SRC_POS,
                                   N_("Failed to open /dev/tap for TAP. errno=%d"), errno);

    /* Use the PPA from the ifname if possible (e.g "tap2", then use 2 as PPA) */
    int iPPA = -1;
    if (pData->pszDeviceName)
    {
        size_t cch = strlen(pData->pszDeviceName);
        if (cch > 1 && isdigit(pData->pszDeviceName[cch - 1]) != 0)
            iPPA = pData->pszDeviceName[cch - 1] - '0';
    }

    struct strioctl ioIF;
    ioIF.ic_cmd = TUNNEWPPA;
    ioIF.ic_len = sizeof(iPPA);
    ioIF.ic_dp = (char *)(&iPPA);
    ioIF.ic_timout = 0;
    iPPA = ioctl(TapFileDes, I_STR, &ioIF);
    if (iPPA < 0)
    {
        close(TapFileDes);
        return PDMDrvHlpVMSetError(pDrvIns, VERR_HOSTIF_IOCTL, RT_SRC_POS,
                                   N_("Failed to get new interface. errno=%d"), errno);
    }

    int InterfaceFD = open("/dev/tap", O_RDWR, 0);
    if (!InterfaceFD)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_HIF_OPEN_FAILED, RT_SRC_POS,
                                   N_("Failed to open interface /dev/tap. errno=%d"), errno);

    if (ioctl(InterfaceFD, I_PUSH, "ip") == -1)
    {
        close(InterfaceFD);
        return PDMDrvHlpVMSetError(pDrvIns, VERR_HOSTIF_IOCTL, RT_SRC_POS,
                                   N_("Failed to push IP. errno=%d"), errno);
    }

    struct lifreq ifReq;
    memset(&ifReq, 0, sizeof(ifReq));
    if (ioctl(InterfaceFD, SIOCGLIFFLAGS, &ifReq) == -1)
        LogRel(("TAP#%d: Failed to get interface flags.\n", pDrvIns->iInstance));

    char szTmp[16];
    char *pszDevName = pData->pszDeviceName;
    if (!pData->pszDeviceName || !*pData->pszDeviceName)
    {
        RTStrPrintf(szTmp, sizeof(szTmp), "tap%d", iPPA);
        pszDevName = szTmp;
    }

    ifReq.lifr_ppa = iPPA;
    RTStrPrintf (ifReq.lifr_name, sizeof(ifReq.lifr_name), pszDevName);

    if (ioctl(InterfaceFD, SIOCSLIFNAME, &ifReq) == -1)
        LogRel(("TAP#%d: Failed to set PPA. errno=%d\n", pDrvIns->iInstance, errno));

    if (ioctl(InterfaceFD, SIOCGLIFFLAGS, &ifReq) == -1)
        LogRel(("TAP#%d: Failed to get interface flags after setting PPA. errno=%d\n", pDrvIns->iInstance, errno));

#ifdef VBOX_SOLARIS_TAP_ARP
    /* Interface */
    if (ioctl(InterfaceFD, I_PUSH, "arp") == -1)
        LogRel(("TAP#%d: Failed to push ARP to Interface FD. errno=%d\n", pDrvIns->iInstance, errno));

    /* IP */
    if (ioctl(IPFileDes, I_POP, NULL) == -1)
        LogRel(("TAP#%d: Failed I_POP from IP FD. errno=%d\n", pDrvIns->iInstance, errno));

    if (ioctl(IPFileDes, I_PUSH, "arp") == -1)
        LogRel(("TAP#%d: Failed to push ARP to IP FD. errno=%d\n", pDrvIns->iInstance, errno));

    /* ARP */
    int ARPFileDes = open("/dev/tap", O_RDWR, 0);
    if (ARPFileDes < 0)
        LogRel(("TAP#%d: Failed to open for /dev/tap for ARP. errno=%d", pDrvIns->iInstance, errno));

    if (ioctl(ARPFileDes, I_PUSH, "arp") == -1)
        LogRel(("TAP#%d: Failed to push ARP to ARP FD. errno=%d\n", pDrvIns->iInstance, errno));

    ioIF.ic_cmd = SIOCSLIFNAME;
    ioIF.ic_timout = 0;
    ioIF.ic_len = sizeof(ifReq);
    ioIF.ic_dp = (char *)&ifReq;
    if (ioctl(ARPFileDes, I_STR, &ioIF) == -1)
        LogRel(("TAP#%d: Failed to set interface name to ARP.\n", pDrvIns->iInstance));
#endif

    /* We must use I_LINK and not I_PLINK as I_PLINK makes the link persistent.
     * Then we would not be able unlink the interface if we reuse it.
     * Even 'unplumb' won't work after that.
     */
    int IPMuxID = ioctl(IPFileDes, I_LINK, InterfaceFD);
    if (IPMuxID == -1)
    {
        close(InterfaceFD);
#ifdef VBOX_SOLARIS_TAP_ARP
        close(ARPFileDes);
#endif
        LogRel(("TAP#%d: Cannot link TAP device to IP.\n", pDrvIns->iInstance));
        return PDMDrvHlpVMSetError(pDrvIns, VERR_HOSTIF_IOCTL, RT_SRC_POS,
                    N_("Failed to link TAP device to IP. Check TAP interface name. errno=%d"), errno);
    }

#ifdef VBOX_SOLARIS_TAP_ARP
    int ARPMuxID = ioctl(IPFileDes, I_LINK, ARPFileDes);
    if (ARPMuxID == -1)
        LogRel(("TAP#%d: Failed to link TAP device to ARP\n", pDrvIns->iInstance));

    close(ARPFileDes);
#endif
    close(InterfaceFD);

    /* Reuse ifReq */
    memset(&ifReq, 0, sizeof(ifReq));
    RTStrPrintf (ifReq.lifr_name, sizeof(ifReq.lifr_name), pszDevName);
    ifReq.lifr_ip_muxid  = IPMuxID;
#ifdef VBOX_SOLARIS_TAP_ARP
    ifReq.lifr_arp_muxid = ARPMuxID;
#endif

    if (ioctl(IPFileDes, SIOCSLIFMUXID, &ifReq) == -1)
    {
#ifdef VBOX_SOLARIS_TAP_ARP
        ioctl(IPFileDes, I_PUNLINK, ARPMuxID);
#endif
        ioctl(IPFileDes, I_PUNLINK, IPMuxID);
        close(IPFileDes);
        LogRel(("TAP#%d: Failed to set Mux ID.\n", pDrvIns->iInstance));
        return PDMDrvHlpVMSetError(pDrvIns, VERR_HOSTIF_IOCTL, RT_SRC_POS,
                                   N_("Failed to set Mux ID. Check TAP interface name. errno=%d"), errno);
    }

    pData->FileDevice = (RTFILE)TapFileDes;
    pData->IPFileDevice = (RTFILE)IPFileDes;
    pData->pszDeviceNameActual = RTStrDup(pszDevName);

    return VINF_SUCCESS;
}

# endif /* VBOX_WITH_CROSSBOW */
#endif  /* RT_OS_SOLARIS */


/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  Any thread.
 */
static DECLCALLBACK(void *) drvTAPQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVTAP pData = PDMINS2DATA(pDrvIns, PDRVTAP);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_NETWORK_CONNECTOR:
            return &pData->INetworkConnector;
        default:
            return NULL;
    }
}


/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvTAPDestruct(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvTAPDestruct\n"));
    PDRVTAP pData = PDMINS2DATA(pDrvIns, PDRVTAP);

#ifdef ASYNC_NET
    /*
     * Terminate the Async I/O Thread.
     */
    ASMAtomicXchgSize(&pData->enmState, ASYNCSTATE_TERMINATE);
    if (pData->Thread != NIL_RTTHREAD)
    {
        /* Ensure that it does not spin in the CanReceive loop */
        if (ASMAtomicXchgU32(&pData->fOutOfSpace, false))
            RTSemEventSignal(pData->EventOutOfSpace);

        int rc = RTFileWrite(pData->PipeWrite, "", 1, NULL);
        AssertRC(rc);
        rc = RTThreadWait(pData->Thread, 5000, NULL);
        AssertRC(rc);
        pData->Thread = NIL_RTTHREAD;
    }

    /*
     * Terminate the control pipe.
     */
    if (pData->PipeWrite != NIL_RTFILE)
    {
        int rc = RTFileClose(pData->PipeWrite);
        AssertRC(rc);
        pData->PipeWrite = NIL_RTFILE;
    }
    if (pData->PipeRead != NIL_RTFILE)
    {
        int rc = RTFileClose(pData->PipeRead);
        AssertRC(rc);
        pData->PipeRead = NIL_RTFILE;
    }
#endif

#ifdef RT_OS_SOLARIS
    if (pData->FileDevice != NIL_RTFILE)
    {
        int rc = RTFileClose(pData->FileDevice);
        AssertRC(rc);
        pData->FileDevice = NIL_RTFILE;
    }

# ifndef VBOX_WITH_CROSSBOW
    if (pData->IPFileDevice != NIL_RTFILE)
    {
        int rc = RTFileClose(pData->IPFileDevice);
        AssertRC(rc);
        pData->IPFileDevice = NIL_RTFILE;
    }
# endif

    /*
     * Call TerminateApplication after closing the device otherwise
     * TerminateApplication would not be able to unplumb it.
     */
    if (pData->pszTerminateApplication)
        drvTAPTerminateApplication(pData);

# ifdef VBOX_WITH_CROSSBOW
    /* Finally unregister the VNIC */
    dlpi_close(pData->pDeviceHandle);
    SolarisDeleteVNIC(pData);
# endif

    RTStrFree(pData->pszDeviceNameActual);
#endif  /* RT_OS_SOLARIS */

    MMR3HeapFree(pData->pszDeviceName);
    MMR3HeapFree(pData->pszSetupApplication);
    MMR3HeapFree(pData->pszTerminateApplication);
}


/**
 * Construct a TAP network transport driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvTAPConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVTAP pData = PDMINS2DATA(pDrvIns, PDRVTAP);

    /*
     * Init the static parts.
     */
    pData->pDrvIns                      = pDrvIns;
    pData->FileDevice                   = NIL_RTFILE;
    pData->pszDeviceName                = NULL;
#ifdef RT_OS_SOLARIS
    pData->pszDeviceNameActual          = NULL;
# ifdef VBOX_WITH_CROSSBOW
    pData->pDeviceHandle                = NULL;
    pData->uDeviceID                    = 0;
# else
    pData->IPFileDevice                 = NIL_RTFILE;
# endif
#endif
    pData->pszSetupApplication          = NULL;
    pData->pszTerminateApplication      = NULL;
#ifdef ASYNC_NET
    pData->Thread                       = NIL_RTTHREAD;
    pData->enmState                     = ASYNCSTATE_RUNNING;
#endif
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvTAPQueryInterface;
    /* INetwork */
    pData->INetworkConnector.pfnSend                = drvTAPSend;
    pData->INetworkConnector.pfnSetPromiscuousMode  = drvTAPSetPromiscuousMode;
    pData->INetworkConnector.pfnNotifyLinkChanged   = drvTAPNotifyLinkChanged;
    pData->INetworkConnector.pfnNotifyCanReceive    = drvTAPNotifyCanReceive;

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Device\0InitProg\0TermProg\0FileHandle\0TAPSetupApplication\0TAPTerminateApplication\0MAC"))
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES, "");

    /*
     * Check that no-one is attached to us.
     */
    int rc = pDrvIns->pDrvHlp->pfnAttach(pDrvIns, NULL);
    if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DRVINS_NO_ATTACH,
                                N_("Configuration error: Cannot attach drivers to the TAP driver!"));

    /*
     * Query the network port interface.
     */
    pData->pPort = (PPDMINETWORKPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_NETWORK_PORT);
    if (!pData->pPort)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
                                N_("Configuration error: The above device/driver didn't export the network port interface!"));

    /*
     * Read the configuration.
     */
#if defined(RT_OS_SOLARIS)   /** @todo Other platforms' TAP code should be moved here from ConsoleImpl & VBoxBFE. */
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "TAPSetupApplication", &pData->pszSetupApplication);
    if (VBOX_SUCCESS(rc))
    {
        if (!RTPathExists(pData->pszSetupApplication))
            return PDMDrvHlpVMSetError(pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                       N_("Invalid TAP setup program path: %s"), pData->pszSetupApplication);
    }
    else if (rc != VERR_CFGM_VALUE_NOT_FOUND)
        return PDMDRV_SET_ERROR(pDrvIns, rc, N_("Configuration error: failed to query \"TAPTerminateApplication\""));

    rc = CFGMR3QueryStringAlloc(pCfgHandle, "TAPTerminateApplication", &pData->pszTerminateApplication);
    if (VBOX_SUCCESS(rc))
    {
        if (!RTPathExists(pData->pszTerminateApplication))
            return PDMDrvHlpVMSetError(pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                       N_("Invalid TAP terminate program path: %s"), pData->pszTerminateApplication);
    }
    else if (rc != VERR_CFGM_VALUE_NOT_FOUND)
        return PDMDRV_SET_ERROR(pDrvIns, rc, N_("Configuration error: failed to query \"TAPTerminateApplication\""));

# ifdef VBOX_WITH_CROSSBOW
    rc = CFGMR3QueryBytes(pCfgHandle, "MAC", &pData->MacAddress, sizeof(pData->MacAddress));
    if (VBOX_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc, N_("Configuration error: Failed to query \"MAC\""));
# endif

    rc = CFGMR3QueryStringAlloc(pCfgHandle, "Device", &pData->pszDeviceName);
    if (VBOX_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Query for \"Device\" string failed!"));

    /*
     * Do the setup.
     */
# ifdef VBOX_WITH_CROSSBOW
    rc = SolarisCreateVNIC(pData);
# else
    rc = SolarisTAPAttach(pDrvIns);
# endif
    if (VBOX_FAILURE(rc))
        return rc;

    if (pData->pszSetupApplication)
    {
        rc = drvTAPSetupApplication(pData);
        if (VBOX_FAILURE(rc))
            return PDMDrvHlpVMSetError(pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                       N_("Error running TAP setup application. rc=%d"), rc);
    }

#else /* !RT_OS_SOLARIS */

    int32_t iFile;
    rc = CFGMR3QueryS32(pCfgHandle, "FileHandle", &iFile);
    if (VBOX_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Query for \"FileHandle\" 32-bit signed integer failed!"));
    pData->FileDevice = (RTFILE)iFile;
    if (!RTFileIsValid(pData->FileDevice))
        return PDMDrvHlpVMSetError(pDrvIns, VERR_INVALID_HANDLE, RT_SRC_POS,
                                   N_("The TAP file handle %RTfile is not valid!"), pData->FileDevice);
#endif /* !RT_OS_SOLARIS */

    /*
     * Make sure the descriptor is non-blocking and valid.
     *
     * We should actually query if it's a TAP device, but I haven't
     * found any way to do that.
     */
    if (fcntl(pData->FileDevice, F_SETFL, O_NONBLOCK) == -1)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_HOSTIF_IOCTL, RT_SRC_POS,
                                   N_("Configuration error: Failed to configure /dev/net/tun. errno=%d"), errno);
    /** @todo determine device name. This can be done by reading the link /proc/<pid>/fd/<fd> */
    Log(("drvTAPContruct: %d (from fd)\n", pData->FileDevice));
    rc = VINF_SUCCESS;

#ifdef ASYNC_NET
    /*
     * Create the control pipe.
     */
    int fds[2];
#ifdef RT_OS_L4
    /* XXX We need to tell the library which interface we are using */
    fds[0] = vboxrtLinuxFd2VBoxFd(VBOXRT_FT_TAP, 0);
#endif
    if (pipe(&fds[0]) != 0) /** @todo RTPipeCreate() or something... */
    {
        int rc = RTErrConvertFromErrno(errno);
        AssertRC(rc);
        return rc;
    }
    pData->PipeRead = fds[0];
    pData->PipeWrite = fds[1];

    /*
     * Create the async I/O thread.
     */
    rc = RTThreadCreate(&pData->Thread, drvTAPAsyncIoThread, pData, 128*_1K, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "TAP");
    AssertRCReturn(rc, rc);
#else
    /*
     * Register poller
     */
    rc = pDrvIns->pDrvHlp->pfnPDMPollerRegister(pDrvIns, drvTAPPoller);
    AssertRCReturn(rc, rc);
#endif

#ifdef VBOX_WITH_STATISTICS
    /*
     * Statistics.
     */
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatPktSent,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,        "Number of sent packets.",          "/Drivers/TAP%d/Packets/Sent", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatPktSentBytes,  STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,             "Number of sent bytes.",            "/Drivers/TAP%d/Bytes/Sent", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatPktRecv,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,        "Number of received packets.",      "/Drivers/TAP%d/Packets/Received", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatPktRecvBytes,  STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,             "Number of received bytes.",        "/Drivers/TAP%d/Bytes/Received", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatTransmit,      STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL,    "Profiling packet transmit runs.",  "/Drivers/TAP%d/Transmit", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatReceive,       STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL,    "Profiling packet receive runs.",   "/Drivers/TAP%d/Receive", pDrvIns->iInstance);
# ifdef ASYNC_NET
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatRecvOverflows, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_OCCURENCE, "Profiling packet receive overflows.", "/Drivers/TAP%d/RecvOverflows", pDrvIns->iInstance);
# endif
#endif /* VBOX_WITH_STATISTICS */

    return rc;
}


/**
 * TAP network transport driver registration record.
 */
const PDMDRVREG g_DrvHostInterface =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "HostInterface",
    /* pszDescription */
    "TAP Network Transport Driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_NETWORK,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVTAP),
    /* pfnConstruct */
    drvTAPConstruct,
    /* pfnDestruct */
    drvTAPDestruct,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL, /** @todo Do power on, suspend and resume handlers! */
    /* pfnResume */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL
};

