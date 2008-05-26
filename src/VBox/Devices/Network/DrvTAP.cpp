/** $Id$ */
/** @file
 * Universial TAP network transport driver.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#define LOG_GROUP LOG_GROUP_DRV_TUN
#include <VBox/log.h>
#include <VBox/pdmdrv.h>

#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/path.h>
#include <iprt/thread.h>
#include <iprt/asm.h>
#include <iprt/semaphore.h>
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
# include <stdio.h>
# ifdef VBOX_WITH_CROSSBOW
#  include "solaris/vbox-libdlpi.h"
# endif
#else
# include <sys/fcntl.h>
#endif
#include <errno.h>
#include <unistd.h>

#ifdef RT_OS_L4
# include <l4/vboxserver/file.h>
#endif

#include "Builtins.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
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
# ifdef VBOX_WITH_CROSSBOW
    /** Crossbow: MAC address of the device. */
    PDMMAC                  MacAddress;
    /** Crossbow: Handle of the NIC. */
    dlpi_handle_t           pDeviceHandle;
# else
    /** IP device file handle (/dev/udp). */
    RTFILE                  IPFileDevice;
# endif
    /** Whether device name is obtained from setup application. */
    bool                    fStatic;
#endif
    /** TAP setup application. */
    char                   *pszSetupApplication;
    /** TAP terminate application. */
    char                   *pszTerminateApplication;
    /** The write end of the control pipe. */
    RTFILE                  PipeWrite;
    /** The read end of the control pipe. */
    RTFILE                  PipeRead;
    /** Reader thread. */
    PPDMTHREAD              pThread;

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
*   Internal Functions                                                         *
*******************************************************************************/
#ifdef RT_OS_SOLARIS
# ifdef VBOX_WITH_CROSSBOW
static int              SolarisOpenVNIC(PDRVTAP pData);
static int              SolarisDLPIErr2VBoxErr(int rc);
# else
static int              SolarisTAPAttach(PDRVTAP pData);
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
 * Asynchronous I/O thread for handling receive.
 *
 * @returns VINF_SUCCESS (ignored).
 * @param   Thread          Thread handle.
 * @param   pvUser          Pointer to a DRVTAP structure.
 */
static DECLCALLBACK(int) drvTAPAsyncIoThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVTAP pData = PDMINS2DATA(pDrvIns, PDRVTAP);
    LogFlow(("drvTAPAsyncIoThread: pData=%p\n", pData));

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    STAM_PROFILE_ADV_START(&pData->StatReceive, a);

    /*
     * Polling loop.
     */
    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
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
        int rc = poll(&aFDs[0], ELEMENTS(aFDs), -1 /* infinite */);

        /* this might have changed in the meantime */
        if (pThread->enmState != PDMTHREADSTATE_RUNNING)
            break;

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
            cbRead = sizeof(achBuf);
            rc = g_pfnLibDlpiRecv(pData->pDeviceHandle, NULL, NULL, achBuf, &cbRead, -1, NULL);
            rc = RT_LIKELY(rc == DLPI_SUCCESS) ? VINF_SUCCESS : SolarisDLPIErr2VBoxErr(rc);
#else
            /** @note At least on Linux we will never receive more than one network packet
             *        after poll() returned successfully. I don't know why but a second
             *        RTFileRead() operation will return with VERR_TRY_AGAIN in any case. */
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
                STAM_PROFILE_ADV_STOP(&pData->StatReceive, a);
                int rc = pData->pPort->pfnWaitReceiveAvail(pData->pPort, RT_INDEFINITE_WAIT);
                STAM_PROFILE_ADV_START(&pData->StatReceive, a);

                /*
                 * A return code != VINF_SUCCESS means that we were woken up during a VM
                 * state transistion. Drop the packet and wait for the next one.
                 */
                if (RT_FAILURE(rc))
                    continue;

                /*
                 * Pass the data up.
                 */
#ifdef LOG_ENABLED
                uint64_t u64Now = RTTimeProgramNanoTS();
                LogFlow(("drvTAPAsyncIoThread: %-4d bytes at %llu ns  deltas: r=%llu t=%llu\n",
                         cbRead, u64Now, u64Now - pData->u64LastReceiveTS, u64Now - pData->u64LastTransferTS));
                pData->u64LastReceiveTS = u64Now;
#endif
                Log2(("drvTAPAsyncIoThread: cbRead=%#x\n" "%.*Vhxd\n", cbRead, cbRead, achBuf));
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
            LogFlow(("drvTAPAsyncIoThread: Control message: enmState=%d revents=%#x\n", pThread->enmState, aFDs[1].revents));
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


    LogFlow(("drvTAPAsyncIoThread: returns %Vrc\n", VINF_SUCCESS));
    STAM_PROFILE_ADV_STOP(&pData->StatReceive, a);
    return VINF_SUCCESS;
}


/**
 * Unblock the send thread so it can respond to a state change.
 *
 * @returns VBox status code.
 * @param   pDevIns     The pcnet device instance.
 * @param   pThread     The send thread.
 */
static DECLCALLBACK(int) drvTapAsyncIoWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVTAP pData = PDMINS2DATA(pDrvIns, PDRVTAP);

    int rc = RTFileWrite(pData->PipeWrite, "", 1, NULL);
    AssertRC(rc);

    return VINF_SUCCESS;
}


#if defined(RT_OS_SOLARIS)
/**
 * Calls OS-specific TAP setup application/script.
 *
 * @returns VBox error code.
 * @param   pData           The instance data.
 */
static int drvTAPSetupApplication(PDRVTAP pData)
{
    char szCommand[4096];

#ifdef VBOX_WITH_CROSSBOW
    /* Convert MAC address bytes to string (required by Solaris' dladm). */
    char *pszHex = "0123456789abcdef";
    uint8_t *pMacAddr8 = pData->MacAddress.au8;
    char szMacAddress[3 * sizeof(PDMMAC)];
    for (unsigned int i = 0; i < sizeof(PDMMAC); i++)
    {
        szMacAddress[3 * i] = pszHex[((*pMacAddr8 >> 4) & 0x0f)];
        szMacAddress[3 * i + 1] = pszHex[(*pMacAddr8 & 0x0f)];
        szMacAddress[3 * i + 2] = ':';
        *pMacAddr8++;
    }
    szMacAddress[sizeof(szMacAddress) - 1] =  0;

    RTStrPrintf(szCommand, sizeof(szCommand), "%s %s %s", pData->pszSetupApplication,
            szMacAddress, pData->fStatic ? pData->pszDeviceName : "");
#else
    RTStrPrintf(szCommand, sizeof(szCommand), "%s %s", pData->pszSetupApplication,
            pData->fStatic ? pData->pszDeviceName : "");
#endif

    /* Pipe open the setup application. */
    Log2(("Starting TAP setup application: %s\n", szCommand));
    FILE* pfSetupHandle = popen(szCommand, "r");
    if (pfSetupHandle == 0)
    {
        LogRel(("TAP#%d: Failed to run TAP setup application: %s\n", pData->pDrvIns->iInstance,
              pData->pszSetupApplication, strerror(errno)));
        return VERR_HOSTIF_INIT_FAILED;
    }
    if (!pData->fStatic)
    {
        /* Obtain device name from setup application. */
        char acBuffer[64];
        size_t cBufSize;
        fgets(acBuffer, sizeof(acBuffer), pfSetupHandle);
        cBufSize = strlen(acBuffer);
        /* The script must return the name of the interface followed by a carriage return as the
          first line of its output.  We need a null-terminated string. */
        if ((cBufSize < 2) || (acBuffer[cBufSize - 1] != '\n'))
        {
            pclose(pfSetupHandle);
            LogRel(("The TAP interface setup script did not return the name of a TAP device.\n"));
            return VERR_HOSTIF_INIT_FAILED;
        }
        /* Overwrite the terminating newline character. */
        acBuffer[cBufSize - 1] = 0;
        RTStrAPrintf(&pData->pszDeviceName, "%s", acBuffer);
    }
    int rc = pclose(pfSetupHandle);
    if (!WIFEXITED(rc))
    {
        LogRel(("The TAP interface setup script terminated abnormally.\n"));
        return VERR_HOSTIF_INIT_FAILED;
    }
    if (WEXITSTATUS(rc) != 0)
    {
        LogRel(("The TAP interface setup script returned a non-zero exit code.\n"));
        return VERR_HOSTIF_INIT_FAILED;
    }
    return VINF_SUCCESS;
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
    pszArgs[1] = pData->pszDeviceName;
    pszArgs[2] = NULL;

    Log2(("Starting TAP terminate application: %s %s\n", pData->pszTerminateApplication, pData->pszDeviceName));
    RTPROCESS pid = NIL_RTPROCESS;
    int rc = RTProcCreate(pszArgs[0], pszArgs, RTENV_DEFAULT, 0, &pid);
    if (RT_SUCCESS(rc))
    {
        RTPROCSTATUS Status;
        rc = RTProcWait(pid, 0, &Status);
        if (RT_SUCCESS(rc))
        {
            if (    Status.iStatus == 0
                &&  Status.enmReason == RTPROCEXITREASON_NORMAL)
                return VINF_SUCCESS;

            LogRel(("TAP#%d: Error running TAP terminate application: %s\n", pData->pDrvIns->iInstance, pData->pszTerminateApplication));
        }
        else
            LogRel(("TAP#%d: RTProcWait failed for: %s\n", pData->pDrvIns->iInstance, pData->pszTerminateApplication));
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
 * Crossbow: Open & configure the virtual NIC.
 *
 * @returns VBox error code.
 * @param   pData           The instance data.
 */
static int SolarisOpenVNIC(PDRVTAP pData)
{
    /*
     * Open & bind the NIC using the datalink provider routine.
     */
    int rc = g_pfnLibDlpiOpen(pData->pszDeviceName, &pData->pDeviceHandle, DLPI_RAW);
    if (rc != DLPI_SUCCESS)
        return PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                           N_("Failed to open VNIC \"%s\" in raw mode"), pData->pszDeviceName);

    dlpi_info_t vnicInfo;
    rc = g_pfnLibDlpiInfo(pData->pDeviceHandle, &vnicInfo, 0);
    if (rc == DLPI_SUCCESS)
    {
        if (vnicInfo.di_mactype == DL_ETHER)
        {
            rc = g_pfnLibDlpiBind(pData->pDeviceHandle, DLPI_ANY_SAP, NULL);
            if (rc == DLPI_SUCCESS)
            {
                rc = g_pfnLibDlpiSetPhysAddr(pData->pDeviceHandle, DL_CURR_PHYS_ADDR, &pData->MacAddress, ETHERADDRL);
                if (rc == DLPI_SUCCESS)
                {
                    rc = g_pfnLibDlpiPromiscon(pData->pDeviceHandle, DL_PROMISC_SAP);
                    if (rc == DLPI_SUCCESS)
                    {
                        /* Need to use DL_PROMIS_PHYS (not multicast) as we cannot be sure what the guest needs. */
                        rc = g_pfnLibDlpiPromiscon(pData->pDeviceHandle, DL_PROMISC_PHYS);
                        if (rc == DLPI_SUCCESS)
                        {
                            pData->FileDevice = g_pfnLibDlpiFd(pData->pDeviceHandle);
                            if (pData->FileDevice >= 0)
                            {
                                Log(("SolarisOpenVNIC: %s -> %d\n", pData->pszDeviceName, pData->FileDevice));
                                return VINF_SUCCESS;
                            }

                            rc = PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                                     N_("Failed to obtain file descriptor for VNIC"));
                        }
                        else
                            rc = PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                                     N_("Failed to set appropriate promiscous mode"));
                    }
                    else
                        rc = PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                                 N_("Failed to activate promiscous mode for VNIC"));
                }
                else
                    rc = PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                             N_("Failed to set physical address for VNIC"));
            }
            else
                rc = PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                         N_("Failed to bind VNIC"));
        }
        else
            rc = PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                         N_("VNIC type is not ethernet"));
    }
    else
        rc = PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                         N_("Failed to obtain VNIC info"));
    g_pfnLibDlpiClose(pData->pDeviceHandle);
    return rc;
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
        /* case DLPI_ENOTENOTSUP: */
        case DLPI_EUNAVAILSAP:      return VERR_NOT_SUPPORTED;

        /*  Define VBox error codes for these, if really needed. */
        case DLPI_ENOLINK:
        case DLPI_EBADLINK:
        /* case DLPI_ENOTEIDINVAL: */
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
 * @param   pData            The instance data.
 */
static DECLCALLBACK(int) SolarisTAPAttach(PDRVTAP pData)
{
    LogFlow(("SolarisTapAttach: pData=%p\n", pData));


    int IPFileDes = open("/dev/udp", O_RDWR, 0);
    if (IPFileDes < 0)
        return PDMDrvHlpVMSetError(pData->pDrvIns, VERR_PDM_HIF_OPEN_FAILED, RT_SRC_POS,
                                   N_("Failed to open /dev/udp. errno=%d"), errno);

    int TapFileDes = open("/dev/tap", O_RDWR, 0);
    if (TapFileDes < 0)
        return PDMDrvHlpVMSetError(pData->pDrvIns, VERR_PDM_HIF_OPEN_FAILED, RT_SRC_POS,
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
        return PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_IOCTL, RT_SRC_POS,
                                   N_("Failed to get new interface. errno=%d"), errno);
    }

    int InterfaceFD = open("/dev/tap", O_RDWR, 0);
    if (!InterfaceFD)
        return PDMDrvHlpVMSetError(pData->pDrvIns, VERR_PDM_HIF_OPEN_FAILED, RT_SRC_POS,
                                   N_("Failed to open interface /dev/tap. errno=%d"), errno);

    if (ioctl(InterfaceFD, I_PUSH, "ip") == -1)
    {
        close(InterfaceFD);
        return PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_IOCTL, RT_SRC_POS,
                                   N_("Failed to push IP. errno=%d"), errno);
    }

    struct lifreq ifReq;
    memset(&ifReq, 0, sizeof(ifReq));
    if (ioctl(InterfaceFD, SIOCGLIFFLAGS, &ifReq) == -1)
        LogRel(("TAP#%d: Failed to get interface flags.\n", pData->pDrvIns->iInstance));

    ifReq.lifr_ppa = iPPA;
    RTStrPrintf (ifReq.lifr_name, sizeof(ifReq.lifr_name), pData->pszDeviceName);

    if (ioctl(InterfaceFD, SIOCSLIFNAME, &ifReq) == -1)
        LogRel(("TAP#%d: Failed to set PPA. errno=%d\n", pData->pDrvIns->iInstance, errno));

    if (ioctl(InterfaceFD, SIOCGLIFFLAGS, &ifReq) == -1)
        LogRel(("TAP#%d: Failed to get interface flags after setting PPA. errno=%d\n", pData->pDrvIns->iInstance, errno));

#ifdef VBOX_SOLARIS_TAP_ARP
    /* Interface */
    if (ioctl(InterfaceFD, I_PUSH, "arp") == -1)
        LogRel(("TAP#%d: Failed to push ARP to Interface FD. errno=%d\n", pData->pDrvIns->iInstance, errno));

    /* IP */
    if (ioctl(IPFileDes, I_POP, NULL) == -1)
        LogRel(("TAP#%d: Failed I_POP from IP FD. errno=%d\n", pData->pDrvIns->iInstance, errno));

    if (ioctl(IPFileDes, I_PUSH, "arp") == -1)
        LogRel(("TAP#%d: Failed to push ARP to IP FD. errno=%d\n", pData->pDrvIns->iInstance, errno));

    /* ARP */
    int ARPFileDes = open("/dev/tap", O_RDWR, 0);
    if (ARPFileDes < 0)
        LogRel(("TAP#%d: Failed to open for /dev/tap for ARP. errno=%d", pData->pDrvIns->iInstance, errno));

    if (ioctl(ARPFileDes, I_PUSH, "arp") == -1)
        LogRel(("TAP#%d: Failed to push ARP to ARP FD. errno=%d\n", pData->pDrvIns->iInstance, errno));

    ioIF.ic_cmd = SIOCSLIFNAME;
    ioIF.ic_timout = 0;
    ioIF.ic_len = sizeof(ifReq);
    ioIF.ic_dp = (char *)&ifReq;
    if (ioctl(ARPFileDes, I_STR, &ioIF) == -1)
        LogRel(("TAP#%d: Failed to set interface name to ARP.\n", pData->pDrvIns->iInstance));
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
        LogRel(("TAP#%d: Cannot link TAP device to IP.\n", pData->pDrvIns->iInstance));
        return PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_IOCTL, RT_SRC_POS,
                    N_("Failed to link TAP device to IP. Check TAP interface name. errno=%d"), errno);
    }

#ifdef VBOX_SOLARIS_TAP_ARP
    int ARPMuxID = ioctl(IPFileDes, I_LINK, ARPFileDes);
    if (ARPMuxID == -1)
        LogRel(("TAP#%d: Failed to link TAP device to ARP\n", pData->pDrvIns->iInstance));

    close(ARPFileDes);
#endif
    close(InterfaceFD);

    /* Reuse ifReq */
    memset(&ifReq, 0, sizeof(ifReq));
    RTStrPrintf (ifReq.lifr_name, sizeof(ifReq.lifr_name), pData->pszDeviceName);
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
        LogRel(("TAP#%d: Failed to set Mux ID.\n", pData->pDrvIns->iInstance));
        return PDMDrvHlpVMSetError(pData->pDrvIns, VERR_HOSTIF_IOCTL, RT_SRC_POS,
                                   N_("Failed to set Mux ID. Check TAP interface name. errno=%d"), errno);
    }

    pData->FileDevice = (RTFILE)TapFileDes;
    pData->IPFileDevice = (RTFILE)IPFileDes;

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

#ifdef RT_OS_SOLARIS
    /** @todo r=bird: This *does* need checking against ConsoleImpl2.cpp if used on non-solaris systems. */
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

#endif  /* RT_OS_SOLARIS */

#ifdef RT_OS_SOLARIS
    if (!pData->fStatic)
        RTStrFree(pData->pszDeviceName);    /* allocated by drvTAPSetupApplication */
    else
        MMR3HeapFree(pData->pszDeviceName);
#else
    MMR3HeapFree(pData->pszDeviceName);
#endif
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
# ifdef VBOX_WITH_CROSSBOW
    pData->pDeviceHandle                = NULL;
# else
    pData->IPFileDevice                 = NIL_RTFILE;
# endif
    pData->fStatic                      = true;
#endif
    pData->pszSetupApplication          = NULL;
    pData->pszTerminateApplication      = NULL;

    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvTAPQueryInterface;
    /* INetwork */
    pData->INetworkConnector.pfnSend                = drvTAPSend;
    pData->INetworkConnector.pfnSetPromiscuousMode  = drvTAPSetPromiscuousMode;
    pData->INetworkConnector.pfnNotifyLinkChanged   = drvTAPNotifyLinkChanged;

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
                                N_("Configuration error: Cannot attach drivers to the TAP driver"));

    /*
     * Query the network port interface.
     */
    pData->pPort = (PPDMINETWORKPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_NETWORK_PORT);
    if (!pData->pPort)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
                                N_("Configuration error: The above device/driver didn't export the network port interface"));

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
        pData->fStatic = false;

    /* Obtain the device name from the setup application (if none was specified). */
    if (pData->pszSetupApplication)
    {
        rc = drvTAPSetupApplication(pData);
        if (VBOX_FAILURE(rc))
            return PDMDrvHlpVMSetError(pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                       N_("Error running TAP setup application. rc=%d"), rc);
    }

    /*
     * Do the setup.
     */
# ifdef VBOX_WITH_CROSSBOW
    if (!VBoxLibDlpiFound())
    {
        return PDMDrvHlpVMSetError(pDrvIns, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,
                                       N_("Failed to load library %s required for host interface networking."), LIB_DLPI);
    }
    rc = SolarisOpenVNIC(pData);
# else
    rc = SolarisTAPAttach(pData);
# endif
    if (VBOX_FAILURE(rc))
        return rc;

#else /* !RT_OS_SOLARIS */

    int32_t iFile;
    rc = CFGMR3QueryS32(pCfgHandle, "FileHandle", &iFile);
    if (VBOX_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Query for \"FileHandle\" 32-bit signed integer failed"));
    pData->FileDevice = (RTFILE)iFile;
    if (!RTFileIsValid(pData->FileDevice))
        return PDMDrvHlpVMSetError(pDrvIns, VERR_INVALID_HANDLE, RT_SRC_POS,
                                   N_("The TAP file handle %RTfile is not valid"), pData->FileDevice);
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
    rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pData->pThread, pData, drvTAPAsyncIoThread, drvTapAsyncIoWakeup, 128 * _1K, RTTHREADTYPE_IO, "TAP");
    AssertRCReturn(rc, rc);

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
