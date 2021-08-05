/* $Id$ */
/** @file
 * TPM emulator using a TCP/socket interface to talk to swtpm (https://github.com/stefanberger/swtpm).
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_TCP /** @todo */
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmtpmifs.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/stream.h>
#include <iprt/alloc.h>
#include <iprt/pipe.h>
#include <iprt/poll.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/socket.h>
#include <iprt/tcp.h>
#include <iprt/uuid.h>
#include <iprt/json.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

#define DRVTPMEMU_POLLSET_ID_SOCKET_CTRL 0
#define DRVTPMEMU_POLLSET_ID_SOCKET_DATA 1
#define DRVTPMEMU_POLLSET_ID_WAKEUP      2


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/** @name Protocol definitions to communicate with swtpm, taken from https://github.com/stefanberger/swtpm/blob/master/include/swtpm/tpm_ioctl.h
 * @{ */
/**
 * Commands going over the control channel (big endian).
 */
typedef enum SWTPMCMD
{
    /** Not used. */
    SWTPMCMD_INVALID = 0,
    SWTPMCMD_GET_CAPABILITY,
    SWTPMCMD_INIT,
    SWTPMCMD_SHUTDOWN,
    SWTPMCMD_GET_TPMESTABLISHED,
    SWTPMCMD_SET_LOCALITY,
    SWTPMCMD_HASH_START,
    SWTPMCMD_HASH_DATA,
    SWTPMCMD_HASH_END,
    SWTPMCMD_CANCEL_TPM_CMD,
    SWTPMCMD_STORE_VOLATILE,
    SWTPMCMD_RESET_TPMESTABLISHED,
    SWTPMCMD_GET_STATEBLOB,
    SWTPMCMD_SET_STATEBLOB,
    SWTPMCMD_STOP,
    SWTPMCMD_GET_CONFIG,
    SWTPMCMD_SET_DATAFD,
    SWTPMCMD_SET_BUFFERSIZE,
    SWTPMCMD_GET_INFO,
    /** Blow the enum up to a 32bit size. */
    SWTPMCMD_32BIT_HACK = 0x7fffffff
} SWTPMCMD;


/**
 * Command/Response header.
 */
typedef union SWTPMHDR
{
    /** The command opcode. */
    SWTPMCMD                    enmCmd;
    /** The response result. */
    uint32_t                    u32Resp;
} SWTPMHDR;
AssertCompileSize(SWTPMHDR, sizeof(uint32_t));
/** Pointer to a command/response header. */
typedef SWTPMHDR *PSWTPMHDR;
/** Pointer to a const command/response header. */
typedef const SWTPMHDR *PCSWTPMHDR;


/**
 * Additional command data for SWTPMCMD_INIT.
 */
typedef struct SWTPMCMDTPMINIT
{
    /** Additional flags */
    uint32_t                    u32Flags;
} SWTPMCMDTPMINIT;
/** Pointer to a command data struct for SWTPMCMD_INIT. */
typedef SWTPMCMDTPMINIT *PSWTPMCMDTPMINIT;
/** Pointer to a const command data struct for SWTPMCMD_INIT. */
typedef const SWTPMCMDTPMINIT *PCSWTPMCMDTPMINIT;


/**
 * Response data for a SWTPMCMD_GET_CAPABILITY command.
 */
typedef struct SWTPMRESPGETCAPABILITY
{
    /** The capabilities supported. */
    uint32_t                    u32Caps;
} SWTPMRESPGETCAPABILITY;
/** Pointer to a response data struct for SWTPMCMD_GET_CAPABILITY. */
typedef SWTPMRESPGETCAPABILITY *PSWTPMRESPGETCAPABILITY;
/** Pointer to a const response data struct for SWTPMCMD_GET_CAPABILITY. */
typedef const SWTPMRESPGETCAPABILITY *PCSWTPMRESPGETCAPABILITY;


/** @name Capabilities as returned by SWTPMCMD_GET_CAPABILITY.
 * @{ */
#define SWTPM_CAP_INIT                  RT_BIT_32(0)
#define SWTPM_CAP_SHUTDOWN              RT_BIT_32(1)
#define SWTPM_CAP_GET_TPMESTABLISHED    RT_BIT_32(2)
#define SWTPM_CAP_SET_LOCALITY          RT_BIT_32(3)
#define SWTPM_CAP_HASHING               RT_BIT_32(4)
#define SWTPM_CAP_CANCEL_TPM_CMD        RT_BIT_32(5)
#define SWTPM_CAP_STORE_VOLATILE        RT_BIT_32(6)
#define SWTPM_CAP_RESET_TPMESTABLISHED  RT_BIT_32(7)
#define SWTPM_CAP_GET_STATEBLOB         RT_BIT_32(8)
#define SWTPM_CAP_SET_STATEBLOB         RT_BIT_32(9)
#define SWTPM_CAP_STOP                  RT_BIT_32(10)
#define SWTPM_CAP_GET_CONFIG            RT_BIT_32(11)
#define SWTPM_CAP_SET_DATAFD            RT_BIT_32(12)
#define SWTPM_CAP_SET_BUFFERSIZE        RT_BIT_32(13)
#define SWTPM_CAP_GET_INFO              RT_BIT_32(14)
#define SWTPM_CAP_SEND_COMMAND_HEADER   RT_BIT_32(15)
/** @} */


/**
 * Additional command data for SWTPMCMD_GET_CONFIG.
 */
typedef struct SWTPMCMDGETCONFIG
{
    /** Combination of SWTPM_GET_CONFIG_F_XXX. */
    uint64_t                    u64Flags;
    /** The offset where to start reading from. */
    uint32_t                    u32Offset;
    /** Some padding to a 8 byte alignment. */
    uint32_t                    u32Padding;
} SWTPMCMDGETCONFIG;
/** Pointer to a response data struct for SWTPMCMD_GET_CONFIG. */
typedef SWTPMCMDGETCONFIG *PSWTPMCMDGETCONFIG;
/** Pointer to a const response data struct for SWTPMCMD_GET_CONFIG. */
typedef const SWTPMCMDGETCONFIG *PCSWTPMCMDGETCONFIG;


/** @name Flags for SWTPMCMDGETCONFIG::u64Flags.
 * @{ */
/** Return the TPM specification JSON object. */
#define SWTPM_GET_CONFIG_F_TPM_SPECIFICATION    RT_BIT_64(0)
/** Return the TPM attributes JSON object. */
#define SWTPM_GET_CONFIG_F_TPM_ATTRIBUTES       RT_BIT_64(1)
/** @} */


/**
 * Response data for a SWTPMCMD_GET_CONFIG command.
 */
typedef struct SWTPMRESPGETCONFIG
{
    /** Total size of the object in bytes. */
    uint32_t                    cbTotal;
    /** Size of the chunk returned in this response. */
    uint32_t                    cbThis;
    /** The data returned. */
    uint8_t                     abData[3 * _1K];
} SWTPMRESPGETCONFIG;
/** Pointer to a response data struct for SWTPMCMD_GET_CONFIG. */
typedef SWTPMRESPGETCONFIG *PSWTPMRESPGETCONFIG;
/** Pointer to a const response data struct for SWTPMCMD_GET_CONFIG. */
typedef const SWTPMRESPGETCONFIG *PCSWTPMRESPGETCONFIG;
/** @} */


/**
 * TPM emulator driver instance data.
 *
 * @implements PDMITPMCONNECTOR
 */
typedef struct DRVTPMEMU
{
    /** The stream interface. */
    PDMITPMCONNECTOR    ITpmConnector;
    /** Pointer to the driver instance. */
    PPDMDRVINS          pDrvIns;

    /** Socket handle for the control connection. */
    RTSOCKET            hSockCtrl;
    /** Socket handle for the data connection. */
    RTSOCKET            hSockData;

    /** Poll set used to wait for I/O events. */
    RTPOLLSET           hPollSet;
    /** Reading end of the wakeup pipe. */
    RTPIPE              hPipeWakeR;
    /** Writing end of the wakeup pipe. */
    RTPIPE              hPipeWakeW;

    /** Flag to signal listening thread to shut down. */
    bool volatile       fShutdown;
    /** Flag to signal whether the thread was woken up from external. */
    bool volatile       fWokenUp;

    /** TPM version offered by the emulator. */
    TPMVERSION          enmTpmVers;
} DRVTPMEMU;
/** Pointer to the TPM emulator instance data. */
typedef DRVTPMEMU *PDRVTPMEMU;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/**
 * Executes the given command over the control connection to the TPM emulator.
 *
 * @returns VBox status code.
 * @param   pThis               Pointer to the TPM emulator driver instance data.
 * @param   enmCmd              The command to execute.
 * @param   pvCmd               Additional command data to send.
 * @param   cbCmd               Size of the additional command data in bytes.
 * @param   pu32Resp            Where to store the response code from the reply.
 * @param   pvResp              Where to store additional resposne data.
 * @param   cbResp              Size of the Response data in bytes (excluding the response status code which is implicit).
 * @param   cMillies            Number of milliseconds to wait before aborting the command with a timeout error.
 *
 * @note This method can return success even though the request at such failed, check the content of pu32Resp!
 */
static int drvTpmEmuExecCtrlCmdEx(PDRVTPMEMU pThis, SWTPMCMD enmCmd, const void *pvCmd, size_t cbCmd, uint32_t *pu32Resp,
                                  void *pvResp, size_t cbResp, RTMSINTERVAL cMillies)
{
    SWTPMHDR Hdr;
    RTSGBUF SgBuf;
    RTSGSEG aSegs[2];
    uint32_t cSegs = 1;

    Hdr.enmCmd = (SWTPMCMD)RT_H2BE_U32(enmCmd);
    aSegs[0].pvSeg = &Hdr;
    aSegs[0].cbSeg = sizeof(Hdr);
    if (cbCmd)
    {
        cSegs++;
        aSegs[1].pvSeg = (void *)pvCmd;
        aSegs[1].cbSeg = cbCmd;
    }

    RTSgBufInit(&SgBuf, &aSegs[0], cSegs);
    int rc = RTSocketSgWrite(pThis->hSockCtrl, &SgBuf);
    if (RT_SUCCESS(rc))
    {
        rc = RTSocketSelectOne(pThis->hSockCtrl, cMillies);
        if (RT_SUCCESS(rc))
        {
            uint32_t u32Resp = 0;
            rc = RTSocketRead(pThis->hSockCtrl, &u32Resp, sizeof(u32Resp), NULL /*pcbRead*/);
            if (RT_SUCCESS(rc))
            {
                *pu32Resp = RT_BE2H_U32(u32Resp);
                if (*pu32Resp == 0)
                    rc = RTSocketRead(pThis->hSockCtrl, pvResp, cbResp, NULL /*pcbRead*/);
            }
        }
    }

    return rc;
}


/**
 * Executes the given command over the control connection to the TPM emulator - variant with no command payload.
 *
 * @returns VBox status code.
 * @retval  VERR_NET_IO_ERROR if the executed command returned an error in the response status field.
 * @param   pThis               Pointer to the TPM emulator driver instance data.
 * @param   enmCmd              The command to execute.
 * @param   pvResp              Where to store additional resposne data.
 * @param   cbResp              Size of the Response data in bytes (excluding the response status code which is implicit).
 * @param   cMillies            Number of milliseconds to wait before aborting the command with a timeout error.
 */
static int drvTpmEmuExecCtrlCmdNoPayload(PDRVTPMEMU pThis, SWTPMCMD enmCmd, void *pvResp, size_t cbResp, RTMSINTERVAL cMillies)
{
    uint32_t u32Resp = 0;
    int rc = drvTpmEmuExecCtrlCmdEx(pThis, enmCmd, NULL /*pvCmd*/, 0 /*cbCmd*/, &u32Resp,
                                    pvResp, cbResp, cMillies);
    if (RT_SUCCESS(rc))
    {
        if (u32Resp != 0)
            rc = VERR_NET_IO_ERROR;
    }

    return rc;
}


/**
 * Queries the version of the TPM offered by the remote emulator.
 *
 * @returns VBox status code.
 * @param   pThis               Pointer to the TPM emulator driver instance data.
 */
static int drvTpmEmuQueryTpmVersion(PDRVTPMEMU pThis)
{
    SWTPMCMDGETCONFIG Cmd;
    SWTPMRESPGETCONFIG Resp;
    uint32_t u32Resp = 0;

    RT_ZERO(Cmd); RT_ZERO(Resp);
    Cmd.u64Flags = RT_H2BE_U64(SWTPM_GET_CONFIG_F_TPM_SPECIFICATION);
    Cmd.u32Offset = 0;
    int rc = drvTpmEmuExecCtrlCmdEx(pThis, SWTPMCMD_GET_CONFIG, &Cmd, sizeof(Cmd), &u32Resp,
                                    &Resp, sizeof(Resp), RT_MS_10SEC);
    if (RT_SUCCESS(rc))
    {
        if (RT_BE2H_U32(Resp.cbTotal) == RT_BE2H_U32(Resp.cbThis))
        {
            RTJSONVAL hJsonVal = NIL_RTJSONVAL;
            rc = RTJsonParseFromBuf(&hJsonVal, &Resp.abData[0], sizeof(Resp.abData), NULL /*pErrInfo*/);
            if (RT_SUCCESS(rc))
            {
                /** @todo */
                RTJsonValueRelease(hJsonVal);
            }
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}


/**
 * Queries the capabilities of the remote TPM emulator and verifies that
 * it offers everything we require for operation.
 *
 * @returns VBox status code.
 * @param   pThis               Pointer to the TPM emulator driver instance data.
 */
static int drvTpmEmuQueryAndValidateCaps(PDRVTPMEMU pThis)
{
    SWTPMRESPGETCAPABILITY Resp;
    int rc = drvTpmEmuExecCtrlCmdNoPayload(pThis, SWTPMCMD_GET_CAPABILITY, &Resp, sizeof(Resp), RT_MS_10SEC);
    if (RT_SUCCESS(rc))
    {
        /** @todo */
    }

    return rc;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnStartup} */
static DECLCALLBACK(int) drvTpmEmuStartup(PPDMITPMCONNECTOR pInterface, size_t cbCmdResp)
{
    RT_NOREF(pInterface, cbCmdResp);
    return VERR_NOT_IMPLEMENTED;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnShutdown} */
static DECLCALLBACK(int) drvTpmEmuShutdown(PPDMITPMCONNECTOR pInterface)
{
    RT_NOREF(pInterface);
    return VERR_NOT_IMPLEMENTED;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnReset} */
static DECLCALLBACK(int) drvTpmEmuReset(PPDMITPMCONNECTOR pInterface)
{
    RT_NOREF(pInterface);
    return VERR_NOT_IMPLEMENTED;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnGetVersion} */
static DECLCALLBACK(TPMVERSION) drvTpmEmuGetVersion(PPDMITPMCONNECTOR pInterface)
{
    PDRVTPMEMU pThis = RT_FROM_MEMBER(pInterface, DRVTPMEMU, ITpmConnector);
    return pThis->enmTpmVers;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnGetEstablishedFlag} */
static DECLCALLBACK(bool) drvTpmEmuGetEstablishedFlag(PPDMITPMCONNECTOR pInterface)
{
    RT_NOREF(pInterface);
    return false;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnGetEstablishedFlag} */
static DECLCALLBACK(int) drvTpmEmuResetEstablishedFlag(PPDMITPMCONNECTOR pInterface, uint8_t bLoc)
{
    RT_NOREF(pInterface, bLoc);
    return VERR_NOT_IMPLEMENTED;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnCmdExec} */
static DECLCALLBACK(int) drvTpmEmuCmdExec(PPDMITPMCONNECTOR pInterface, uint8_t bLoc, const void *pvCmd, size_t cbCmd, void *pvResp, size_t cbResp)
{
    RT_NOREF(pInterface, bLoc, pvCmd, cbCmd, pvResp, cbResp);
    return VERR_NOT_IMPLEMENTED;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnCmdCancel} */
static DECLCALLBACK(int) drvTpmEmuCmdCancel(PPDMITPMCONNECTOR pInterface)
{
    RT_NOREF(pInterface);
    return VERR_NOT_IMPLEMENTED;
}


/** @interface_method_impl{PDMIBASE,pfnQueryInterface} */
static DECLCALLBACK(void *) drvTpmEmuQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS      pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVTPMEMU      pThis   = PDMINS_2_DATA(pDrvIns, PDRVTPMEMU);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMITPMCONNECTOR, &pThis->ITpmConnector);
    return NULL;
}


/* -=-=-=-=- PDMDRVREG -=-=-=-=- */

/** @copydoc FNPDMDRVDESTRUCT */
static DECLCALLBACK(void) drvTpmEmuDestruct(PPDMDRVINS pDrvIns)
{
    PDRVTPMEMU pThis = PDMINS_2_DATA(pDrvIns, PDRVTPMEMU);
    LogFlow(("%s\n", __FUNCTION__));
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    if (pThis->hSockCtrl != NIL_RTSOCKET)
    {
        int rc = RTPollSetRemove(pThis->hPollSet, DRVTPMEMU_POLLSET_ID_SOCKET_CTRL);
        AssertRC(rc);

        rc = RTSocketShutdown(pThis->hSockCtrl, true /* fRead */, true /* fWrite */);
        AssertRC(rc);

        rc = RTSocketClose(pThis->hSockCtrl);
        AssertRC(rc); RT_NOREF(rc);

        pThis->hSockCtrl = NIL_RTSOCKET;
    }

    if (pThis->hSockData != NIL_RTSOCKET)
    {
        int rc = RTPollSetRemove(pThis->hPollSet, DRVTPMEMU_POLLSET_ID_SOCKET_DATA);
        AssertRC(rc);

        rc = RTSocketShutdown(pThis->hSockData, true /* fRead */, true /* fWrite */);
        AssertRC(rc);

        rc = RTSocketClose(pThis->hSockData);
        AssertRC(rc); RT_NOREF(rc);

        pThis->hSockCtrl = NIL_RTSOCKET;
    }

    if (pThis->hPipeWakeR != NIL_RTPIPE)
    {
        int rc = RTPipeClose(pThis->hPipeWakeR);
        AssertRC(rc);

        pThis->hPipeWakeR = NIL_RTPIPE;
    }

    if (pThis->hPipeWakeW != NIL_RTPIPE)
    {
        int rc = RTPipeClose(pThis->hPipeWakeW);
        AssertRC(rc);

        pThis->hPipeWakeW = NIL_RTPIPE;
    }

    if (pThis->hPollSet != NIL_RTPOLLSET)
    {
        int rc = RTPollSetDestroy(pThis->hPollSet);
        AssertRC(rc);

        pThis->hPollSet = NIL_RTPOLLSET;
    }
}


/** @copydoc FNPDMDRVCONSTRUCT */
static DECLCALLBACK(int) drvTpmEmuConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVTPMEMU pThis = PDMINS_2_DATA(pDrvIns, PDRVTPMEMU);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                                  = pDrvIns;
    pThis->hSockCtrl                                = NIL_RTSOCKET;
    pThis->hSockData                                = NIL_RTSOCKET;
    pThis->enmTpmVers                               = TPMVERSION_UNKNOWN;

    pThis->hPollSet                                 = NIL_RTPOLLSET;
    pThis->hPipeWakeR                               = NIL_RTPIPE;
    pThis->hPipeWakeW                               = NIL_RTPIPE;

    /* IBase */
    pDrvIns->IBase.pfnQueryInterface                = drvTpmEmuQueryInterface;
    /* ITpmConnector */
    pThis->ITpmConnector.pfnStartup                 = drvTpmEmuStartup;
    pThis->ITpmConnector.pfnShutdown                = drvTpmEmuShutdown;
    pThis->ITpmConnector.pfnReset                   = drvTpmEmuReset;
    pThis->ITpmConnector.pfnGetVersion              = drvTpmEmuGetVersion;
    pThis->ITpmConnector.pfnGetEstablishedFlag      = drvTpmEmuGetEstablishedFlag;
    pThis->ITpmConnector.pfnResetEstablishedFlag    = drvTpmEmuResetEstablishedFlag;
    pThis->ITpmConnector.pfnCmdExec                 = drvTpmEmuCmdExec;
    pThis->ITpmConnector.pfnCmdCancel               = drvTpmEmuCmdCancel;

    /*
     * Validate and read the configuration.
     */
    PDMDRV_VALIDATE_CONFIG_RETURN(pDrvIns, "Location", "");

    char szLocation[_1K];
    int rc = CFGMR3QueryString(pCfg, "Location", &szLocation[0], sizeof(szLocation));
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Configuration error: querying \"Location\" resulted in %Rrc"), rc);

    rc = RTPipeCreate(&pThis->hPipeWakeR, &pThis->hPipeWakeW, 0 /* fFlags */);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("DrvTpmEmu#%d: Failed to create wake pipe"), pDrvIns->iInstance);

    rc = RTPollSetCreate(&pThis->hPollSet);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("DrvTpmEmu#%d: Failed to create poll set"), pDrvIns->iInstance);

    rc = RTPollSetAddPipe(pThis->hPollSet, pThis->hPipeWakeR,
                            RTPOLL_EVT_READ | RTPOLL_EVT_ERROR,
                            DRVTPMEMU_POLLSET_ID_WAKEUP);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("DrvTpmEmu#%d failed to add wakeup pipe for %s to poll set"),
                                   pDrvIns->iInstance, szLocation);

    /*
     * Create/Open the socket.
     */
    char *pszPort = strchr(szLocation, ':');
    if (!pszPort)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_NOT_FOUND, RT_SRC_POS,
                                   N_("DrvTpmEmu#%d: The location misses the port to connect to"),
                                   pDrvIns->iInstance);

    *pszPort = '\0'; /* Overwrite temporarily to avoid copying the hostname into a temporary buffer. */
    uint32_t uPort = 0;
    rc = RTStrToUInt32Ex(pszPort + 1, NULL, 10, &uPort);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("DrvTpmEmu#%d: The port part of the location is not a numerical value"),
                                   pDrvIns->iInstance);

    rc = RTTcpClientConnect(szLocation, uPort, &pThis->hSockCtrl);
    *pszPort = ':'; /* Restore delimiter before checking the status. */
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("DrvTpmEmu#%d failed to connect to socket %s"),
                                   pDrvIns->iInstance, szLocation);

    rc = RTPollSetAddSocket(pThis->hPollSet, pThis->hSockCtrl,
                            RTPOLL_EVT_READ | RTPOLL_EVT_WRITE | RTPOLL_EVT_ERROR,
                            DRVTPMEMU_POLLSET_ID_SOCKET_CTRL);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("DrvTpmEmu#%d failed to add socket for %s to poll set"),
                                   pDrvIns->iInstance, szLocation);

    rc = drvTpmEmuQueryTpmVersion(pThis);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("DrvTpmEmu#%d failed to query TPM version from %s"),
                                   pDrvIns->iInstance, szLocation);

    rc = drvTpmEmuQueryAndValidateCaps(pThis);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("DrvTpmEmu#%d failed to query and validate all capabilities offeres by %s"),
                                   pDrvIns->iInstance, szLocation);

    LogRel(("DrvTpmEmu#%d: Connected to %s\n", pDrvIns->iInstance, szLocation));
    return VINF_SUCCESS;
}


/**
 * TCP stream driver registration record.
 */
const PDMDRVREG g_DrvTpmEmu =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "TpmEmu",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "TPM emulator driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_STREAM,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVTPMEMU),
    /* pfnConstruct */
    drvTpmEmuConstruct,
    /* pfnDestruct */
    drvTpmEmuDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

