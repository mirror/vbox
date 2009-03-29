/** @file
 * iSCSI initiator driver, VD backend.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
#define LOG_GROUP LOG_GROUP_VD_ISCSI
#include "VBoxHDD-Internal.h"
#define VBOX_VDICORE_VD /* Signal that the header is included from here. */
#include "VDICore.h"
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/asm.h>
#include <iprt/thread.h>
#include <iprt/semaphore.h>
#include <iprt/md5.h>
#include <iprt/tcp.h>
#include <iprt/time.h>
#include <VBox/scsi.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/** Default port number to use for iSCSI. */
#define ISCSI_DEFAULT_PORT 3260


/** Converts a number in the range of 0 - 15 into the corresponding hex char. */
#define NUM_2_HEX(b) ('0' + (b) + (((b) > 9) ? 39 : 0))
/** Converts a hex char into the corresponding number in the range 0-15. */
#define HEX_2_NUM(c) (((c) <= '9') ? ((c) - '0') : (((c - 'A' + 10) & 0xf)))
/* Converts a base64 char into the corresponding number in the range 0-63. */
#define B64_2_NUM(c) ((c >= 'A' && c <= 'Z') ? (c - 'A') : (c >= 'a' && c <= 'z') ? (c - 'a' + 26) : (c >= '0' && c <= '9') ? (c - '0' + 52) : (c == '+') ? 62 : (c == '/') ? 63 : -1)


/** Minumum CHAP_MD5 challenge length in bytes. */
#define CHAP_MD5_CHALLENGE_MIN 16
/** Maximum CHAP_MD5 challenge length in bytes. */
#define CHAP_MD5_CHALLENGE_MAX 24


/**
 * SCSI peripheral device type. */
typedef enum SCSIDEVTYPE
{
    /** direct-access device. */
    SCSI_DEVTYPE_DISK = 0,
    /** sequential-access device. */
    SCSI_DEVTYPE_TAPE,
    /** printer device. */
    SCSI_DEVTYPE_PRINTER,
    /** processor device. */
    SCSI_DEVTYPE_PROCESSOR,
    /** write-once device. */
    SCSI_DEVTYPE_WORM,
    /** CD/DVD device. */
    SCSI_DEVTYPE_CDROM,
    /** scanner device. */
    SCSI_DEVTYPE_SCANNER,
    /** optical memory device. */
    SCSI_DEVTYPE_OPTICAL,
    /** medium changer. */
    SCSI_DEVTYPE_CHANGER,
    /** communications device. */
    SCSI_DEVTYPE_COMMUNICATION,
    /** storage array controller device. */
    SCSI_DEVTYPE_RAIDCTL = 0x0c,
    /** enclosure services device. */
    SCSI_DEVTYPE_ENCLOSURE,
    /** simplified direct-access device. */
    SCSI_DEVTYPE_SIMPLEDISK,
    /** optical card reader/writer device. */
    SCSI_DEVTYPE_OCRW,
    /** bridge controller device. */
    SCSI_DEVTYPE_BRIDGE,
    /** object-based storage device. */
    SCSI_DEVTYPE_OSD
} SCSIDEVTYPE;

/** Mask for extracting the SCSI device type out of the first byte of the INQUIRY response. */
#define SCSI_DEVTYPE_MASK 0x1f


/** Maximum PDU size we can handle in one piece. */
#define ISCSI_RECV_PDU_BUFFER_SIZE (65536 + ISCSI_BHS_SIZE)


/** Version of the iSCSI standard which this initiator driver can handle. */
#define ISCSI_MY_VERSION 0


/** Length of ISCSI basic header segment. */
#define ISCSI_BHS_SIZE 48


/** Reserved task tag value. */
#define ISCSI_TASK_TAG_RSVD 0xffffffff


/**
 * iSCSI opcodes. */
typedef enum ISCSIOPCODE
{
    /** NOP-Out. */
    ISCSIOP_NOP_OUT = 0x00000000,
    /** SCSI command. */
    ISCSIOP_SCSI_CMD = 0x01000000,
    /** SCSI task management request. */
    ISCSIOP_SCSI_TASKMGMT_REQ = 0x02000000,
    /** Login request. */
    ISCSIOP_LOGIN_REQ = 0x03000000,
    /** Text request. */
    ISCSIOP_TEXT_REQ = 0x04000000,
    /** SCSI Data-Out. */
    ISCSIOP_SCSI_DATA_OUT = 0x05000000,
    /** Logout request. */
    ISCSIOP_LOGOUT_REQ = 0x06000000,
    /** SNACK request. */
    ISCSIOP_SNACK_REQ = 0x10000000,

    /** NOP-In. */
    ISCSIOP_NOP_IN = 0x20000000,
    /** SCSI response. */
    ISCSIOP_SCSI_RES = 0x21000000,
    /** SCSI Task Management response. */
    ISCSIOP_SCSI_TASKMGMT_RES = 0x22000000,
    /** Login response. */
    ISCSIOP_LOGIN_RES = 0x23000000,
    /** Text response. */
    ISCSIOP_TEXT_RES = 0x24000000,
    /** SCSI Data-In. */
    ISCSIOP_SCSI_DATA_IN = 0x25000000,
    /** Logout response. */
    ISCSIOP_LOGOUT_RES = 0x26000000,
    /** Ready To Transfer (R2T). */
    ISCSIOP_R2T = 0x31000000,
    /** Asynchronous message. */
    ISCSIOP_ASYN_MSG = 0x32000000,
    /** Reject. */
    ISCSIOP_REJECT = 0x3f000000
} ISCSIOPCODE;

/** Mask for extracting the iSCSI opcode out of the first header word. */
#define ISCSIOP_MASK 0x3f000000


/** ISCSI BHS word 0: Request should be processed immediately. */
#define ISCSI_IMMEDIATE_DELIVERY_BIT 0x40000000

/** ISCSI BHS word 0: This is the final PDU for this request/response. */
#define ISCSI_FINAL_BIT 0x00800000
/** ISCSI BHS word 0: Mask for extracting the CSG. */
#define ISCSI_CSG_MASK 0x000c0000
/** ISCSI BHS word 0: Shift offset for extracting the CSG. */
#define ISCSI_CSG_SHIFT 18
/** ISCSI BHS word 0: Mask for extracting the NSG. */
#define ISCSI_NSG_MASK 0x00030000
/** ISCSI BHS word 0: Shift offset for extracting the NSG. */
#define ISCSI_NSG_SHIFT 16

/** ISCSI BHS word 0: task attribute untagged */
#define ISCSI_TASK_ATTR_UNTAGGED 0x00000000
/** ISCSI BHS word 0: task attribute simple */
#define ISCSI_TASK_ATTR_SIMPLE 0x00010000
/** ISCSI BHS word 0: task attribute ordered */
#define ISCSI_TASK_ATTR_ORDERED 0x00020000
/** ISCSI BHS word 0: task attribute head of queue */
#define ISCSI_TASK_ATTR_HOQ 0x00030000
/** ISCSI BHS word 0: task attribute ACA */
#define ISCSI_TASK_ATTR_ACA 0x00040000

/** ISCSI BHS word 0: transit to next login phase. */
#define ISCSI_TRANSIT_BIT 0x00800000
/** ISCSI BHS word 0: continue with login negotiation. */
#define ISCSI_CONTINUE_BIT 0x00400000

/** ISCSI BHS word 0: residual underflow. */
#define ISCSI_RESIDUAL_UNFL_BIT 0x00020000
/** ISCSI BHS word 0: residual overflow. */
#define ISCSI_RESIDUAL_OVFL_BIT 0x00040000
/** ISCSI BHS word 0: Bidirectional read residual underflow. */
#define ISCSI_BI_READ_RESIDUAL_UNFL_BIT 0x00080000
/** ISCSI BHS word 0: Bidirectional read residual overflow. */
#define ISCSI_BI_READ_RESIDUAL_OVFL_BIT 0x00100000

/** ISCSI BHS word 0: SCSI response mask. */
#define ISCSI_SCSI_RESPONSE_MASK 0x0000ff00
/** ISCSI BHS word 0: SCSI status mask. */
#define ISCSI_SCSI_STATUS_MASK 0x000000ff

/** ISCSI BHS word 0: response includes status. */
#define ISCSI_STATUS_BIT 0x00010000


/**
 * iSCSI login status class. */
typedef enum ISCSILOGINSTATUSCLASS
{
    /** Success. */
    ISCSI_LOGIN_STATUS_CLASS_SUCCESS = 0,
    /** Redirection. */
    ISCSI_LOGIN_STATUS_CLASS_REDIRECTION,
    /** Initiator error. */
    ISCSI_LOGIN_STATUS_CLASS_INITIATOR_ERROR,
    /** Target error. */
    ISCSI_LOGIN_STATUS_CLASS_TARGET_ERROR
} ISCSILOGINSTATUSCLASS;


/**
 * iSCSI connection state. */
typedef enum ISCSISTATE
{
    /** Not having a connection/session at all. */
    ISCSISTATE_FREE,
    /** Currently trying to login. */
    ISCSISTATE_IN_LOGIN,
    /** Normal operation, corresponds roughly to the Full Feature Phase. */
    ISCSISTATE_NORMAL,
    /** Currently trying to logout. */
    ISCSISTATE_IN_LOGOUT
} ISCSISTATE;


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Block driver instance data.
 */
typedef struct ISCSIIMAGE
{
    /** Pointer to the filename (location). Not really used. */
    const char          *pszFilename;
    /** Pointer to the initiator name. */
    char                *pszInitiatorName;
    /** Pointer to the target name. */
    char                *pszTargetName;
    /** Pointer to the target address. */
    char                *pszTargetAddress;
    /** Pointer to the user name for authenticating the Initiator. */
    char                *pszInitiatorUsername;
    /** Pointer to the secret for authenticating the Initiator. */
    uint8_t             *pbInitiatorSecret;
    /** Length of the secret for authenticating the Initiator. */
    size_t              cbInitiatorSecret;
    /** Pointer to the user name for authenticating the Target. */
    char                *pszTargetUsername;
    /** Pointer to the secret for authenticating the Initiator. */
    uint8_t             *pbTargetSecret;
    /** Length of the secret for authenticating the Initiator. */
    size_t              cbTargetSecret;
    /** Initiator session identifier. */
    uint64_t            ISID;
    /** SCSI Logical Unit Number. */
    uint64_t            LUN;
    /** Pointer to the per-disk VD interface list. */
    PVDINTERFACE        pVDIfsDisk;
    /** Error interface. */
    PVDINTERFACE        pInterfaceError;
    /** Error interface callback table. */
    PVDINTERFACEERROR   pInterfaceErrorCallbacks;
    /** TCP network stack interface. */
    PVDINTERFACE        pInterfaceNet;
    /** TCP network stack interface callback table. */
    PVDINTERFACETCPNET  pInterfaceNetCallbacks;
    /** Pointer to the per-image VD interface list. */
    PVDINTERFACE        pVDIfsImage;
    /** Config interface. */
    PVDINTERFACE        pInterfaceConfig;
    /** Config interface callback table. */
    PVDINTERFACECONFIG  pInterfaceConfigCallbacks;
    /** Image open flags. */
    unsigned            uOpenFlags;
    /** Number of re-login retries when a connection fails. */
    uint32_t            cISCSIRetries;
    /** Size of volume in sectors. */
    uint32_t            cVolume;
    /** Sector size on volume. */
    uint32_t            cbSector;
    /** Total volume size in bytes. Easiert that multiplying the above values all the time. */
    uint64_t            cbSize;
    /** Current state of the connection/session. */
    ISCSISTATE          state;
    /** Flag whether the first Login Response PDU has been seen. */
    bool                FirstRecvPDU;
    /** Initiator Task Tag of the last iSCSI request PDU. */
    uint32_t            ITT;
    /** Sequence number of the last command. */
    uint32_t            CmdSN;
    /** Sequence number of the next command expected by the target. */
    uint32_t            ExpCmdSN;
    /** Maximum sequence number accepted by the target (determines size of window). */
    uint32_t            MaxCmdSN;
    /** Expected sequence number of next status. */
    uint32_t            ExpStatSN;
    /** Currently active request. */
    PISCSIREQ           paCurrReq;
    /** Segment number of currently active request. */
    uint32_t            cnCurrReq;
    /** Pointer to receive PDU buffer. (Freed by RT) */
    void                *pvRecvPDUBuf;
    /** Length of receive PDU buffer. */
    size_t              cbRecvPDUBuf;
    /** Mutex protecting against concurrent use from several threads. */
    RTSEMMUTEX          Mutex;

    /** Pointer to the target hostname. */
    char                *pszHostname;
    /** Pointer to the target hostname. */
    uint32_t            uPort;
    /** Socket handle of the TCP connection. */
    RTSOCKET            Socket;
    /** Timeout for read operations on the TCP connection (in milliseconds). */
    uint32_t            uReadTimeout;
    /** Flag whether to use the host IP stack or DevINIP. */
    bool                fHostIP;
} ISCSIIMAGE, *PISCSIIMAGE;


/**
 * SCSI transfer directions.
 */
typedef enum SCSIXFER
{
    SCSIXFER_NONE = 0,
    SCSIXFER_TO_TARGET,
    SCSIXFER_FROM_TARGET,
    SCSIXFER_TO_FROM_TARGET
} SCSIXFER, *PSCSIXFER;


/**
 * SCSI request structure.
 */
typedef struct SCSIREQ
{
    /** Transfer direction. */
    SCSIXFER enmXfer;
    /** Length of command block. */
    size_t cbCmd;
    /** Length of Initiator2Target data buffer. */
    size_t cbI2TData;
    /** Length of Target2Initiator data buffer. */
    size_t cbT2IData;
    /** Length of sense buffer. */
    size_t cbSense;
    /** Completion status of the command. */
    uint8_t status;
    /** Pointer to command block. */
    void *pvCmd;
    /** Pointer to Initiator2Target data buffer. */
    const void *pcvI2TData;
    /** Pointer to Target2Initiator data buffer. */
    void *pvT2IData;
    /** Pointer to sense buffer. */
    void *pvSense;
} SCSIREQ, *PSCSIREQ;


/*******************************************************************************
*   Static Variables                                                           *
*******************************************************************************/

/** Counter for getting unique instance IDs. */
static uint32_t s_u32iscsiID = 0;

/** Default LUN. */
static const char *s_iscsiConfigDefaultLUN = "0";

/** Default initiator name. */
static const char *s_iscsiConfigDefaultInitiatorName = "iqn.2008-04.com.sun.virtualbox.initiator";

/** Default timeout, 10 seconds. */
static const char *s_iscsiConfigDefaultTimeout = "10000";

/** Default host IP stack. */
static const char *s_iscsiConfigDefaultHostIPStack = "1";

/** Description of all accepted config parameters. */
static const VDCONFIGINFO s_iscsiConfigInfo[] =
{
    { "TargetName",         NULL,                               VDCFGVALUETYPE_STRING,  VD_CFGKEY_MANDATORY },
    /* LUN is defined of string type to handle the "enc" prefix. */
    { "LUN",                s_iscsiConfigDefaultLUN,            VDCFGVALUETYPE_STRING,  VD_CFGKEY_MANDATORY },
    { "TargetAddress",      NULL,                               VDCFGVALUETYPE_STRING,  VD_CFGKEY_MANDATORY },
    { "InitiatorName",      s_iscsiConfigDefaultInitiatorName,  VDCFGVALUETYPE_STRING,  0 },
    { "InitiatorUsername",  NULL,                               VDCFGVALUETYPE_STRING,  0 },
    { "InitiatorSecret",    NULL,                               VDCFGVALUETYPE_BYTES,   0 },
    { "TargetUsername",     NULL,                               VDCFGVALUETYPE_STRING,  VD_CFGKEY_EXPERT },
    { "TargetSecret",       NULL,                               VDCFGVALUETYPE_BYTES,   VD_CFGKEY_EXPERT },
    { "Timeout",            s_iscsiConfigDefaultTimeout,        VDCFGVALUETYPE_INTEGER, VD_CFGKEY_EXPERT },
    { "HostIPStack",        s_iscsiConfigDefaultHostIPStack,    VDCFGVALUETYPE_INTEGER, VD_CFGKEY_EXPERT },
    { NULL,                 NULL,                               VDCFGVALUETYPE_INTEGER, 0 }
};

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

/* iSCSI low-level functions (only to be used from the iSCSI high-level functions). */
static uint32_t iscsiNewITT(PISCSIIMAGE pImage);
static int iscsiSendPDU(PISCSIIMAGE pImage, PISCSIREQ paReq, uint32_t cnReq);
static int iscsiRecvPDU(PISCSIIMAGE pImage, uint32_t itt, PISCSIRES paRes, uint32_t cnRes);
static int drvISCSIValidatePDU(PISCSIRES paRes, uint32_t cnRes);
static int iscsiTextAddKeyValue(uint8_t *pbBuf, size_t cbBuf, size_t *pcbBufCurr, const char *pcszKey, const char *pcszValue, size_t cbValue);
static int iscsiTextGetKeyValue(const uint8_t *pbBuf, size_t cbBuf, const char *pcszKey, const char **ppcszValue);
static int iscsiStrToBinary(const char *pcszValue, uint8_t *pbValue, size_t *pcbValue);

/* Serial number arithmetic comparison. */
static bool serial_number_less(uint32_t sn1, uint32_t sn2);

/* CHAP-MD5 functions. */
#ifdef IMPLEMENT_TARGET_AUTH
static void chap_md5_generate_challenge(uint8_t *pbChallenge, size_t *pcbChallenge);
#endif
static void chap_md5_compute_response(uint8_t *pbResponse, uint8_t id, const uint8_t *pbChallenge, size_t cbChallenge,
                                      const uint8_t *pbSecret, size_t cbSecret);


/**
 * Internal: signal an error to the frontend.
 */
DECLINLINE(int) iscsiError(PISCSIIMAGE pImage, int rc, RT_SRC_POS_DECL,
                           const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    if (pImage->pInterfaceError)
        pImage->pInterfaceErrorCallbacks->pfnError(pImage->pInterfaceError->pvUser, rc, RT_SRC_POS_ARGS,
                                                   pszFormat, va);
    va_end(va);
    return rc;
}


static int iscsiTransportRead(PISCSIIMAGE pImage, PISCSIRES paResponse, unsigned int cnResponse)
{
    int rc = VINF_SUCCESS;
    unsigned int i = 0;
    size_t cbToRead, cbActuallyRead, residual, cbSegActual = 0, cbAHSLength, cbDataLength;
    char *pDst;

    LogFlowFunc(("cnResponse=%d (%s:%d)\n", cnResponse, pImage->pszHostname, pImage->uPort));
    if (pImage->Socket == NIL_RTSOCKET)
    {
        /* Attempt to reconnect if the connection was previously broken. */
        if (pImage->pszHostname != NULL)
        {
            rc = pImage->pInterfaceNetCallbacks->pfnClientConnect(pImage->pszHostname, pImage->uPort, &pImage->Socket);
            if (RT_UNLIKELY(   RT_FAILURE(rc)
                            && (   rc == VERR_NET_CONNECTION_REFUSED
                                || rc == VERR_NET_CONNECTION_RESET
                                || rc == VERR_NET_UNREACHABLE
                                || rc == VERR_NET_HOST_UNREACHABLE
                                || rc == VERR_NET_CONNECTION_TIMED_OUT)))
            {
                /* Standardize return value for no connection. */
                rc = VERR_NET_CONNECTION_REFUSED;
            }
        }
    }

    if (RT_SUCCESS(rc) && paResponse[0].cbSeg >= 48)
    {
        cbToRead = 0;
        residual = 48;  /* Do not read more than the BHS length before the true PDU length is known. */
        cbSegActual = residual;
        pDst = (char *)paResponse[i].pvSeg;
        uint64_t u64Timeout = RTTimeMilliTS() + pImage->uReadTimeout;
        do
        {
            int64_t cMilliesRemaining = u64Timeout - RTTimeMilliTS();
            if (cMilliesRemaining <= 0)
            {
                rc = VERR_TIMEOUT;
                break;
            }
            Assert(cMilliesRemaining < 1000000);
            rc = pImage->pInterfaceNetCallbacks->pfnSelectOne(pImage->Socket,
                                                              cMilliesRemaining);
            if (RT_FAILURE(rc))
                break;
            rc = pImage->pInterfaceNetCallbacks->pfnRead(pImage->Socket,
                                                         pDst, residual,
                                                         &cbActuallyRead);
            if (RT_FAILURE(rc))
                break;
            if (cbActuallyRead == 0)
            {
                /* The other end has closed the connection. */
                pImage->pInterfaceNetCallbacks->pfnClientClose(pImage->Socket);
                pImage->Socket = NIL_RTSOCKET;
                rc = VERR_NET_CONNECTION_RESET;
                break;
            }
            if (cbToRead == 0)
            {
                /* Currently reading the BHS. */
                residual -= cbActuallyRead;
                pDst += cbActuallyRead;
                if (residual <= 40)
                {
                    /* Enough data read to figure out the actual PDU size. */
                    uint32_t word1 = RT_N2H_U32(((uint32_t *)(paResponse[0].pvSeg))[1]);
                    cbAHSLength = (word1 & 0xff000000) >> 24;
                    cbAHSLength = ((cbAHSLength - 1) | 3) + 1;      /* Add padding. */
                    cbDataLength = word1 & 0x00ffffff;
                    cbDataLength = ((cbDataLength - 1) | 3) + 1;    /* Add padding. */
                    cbToRead = residual + cbAHSLength + cbDataLength;
                    residual += paResponse[0].cbSeg - 48;
                    if (residual > cbToRead)
                        residual = cbToRead;
                    cbSegActual = 48 + cbAHSLength + cbDataLength;
                    /* Check whether we are already done with this PDU (no payload). */
                    if (cbToRead == 0)
                        break;
                }
            }
            else
            {
                cbToRead -= cbActuallyRead;
                if (cbToRead == 0)
                    break;
                pDst += cbActuallyRead;
                residual -= cbActuallyRead;
            }
            if (residual == 0)
            {
                i++;
                if (i >= cnResponse)
                {
                    /* No space left in receive buffers. */
                    rc = VERR_BUFFER_OVERFLOW;
                    break;
                }
                pDst = (char *)paResponse[i].pvSeg;
                residual = paResponse[i].cbSeg;
                if (residual > cbToRead)
                    residual = cbToRead;
                cbSegActual = residual;
            }
        } while (true);
    }
    else
    {
        if (RT_SUCCESS(rc))
            rc = VERR_BUFFER_OVERFLOW;
    }
    if (RT_SUCCESS(rc))
    {
        paResponse[i].cbSeg = cbSegActual;
        for (i++; i < cnResponse; i++)
            paResponse[i].cbSeg = 0;
    }

    if (RT_UNLIKELY(    RT_FAILURE(rc)
                    && (   rc == VERR_NET_CONNECTION_RESET
                        || rc == VERR_NET_CONNECTION_ABORTED
                        || rc == VERR_NET_CONNECTION_RESET_BY_PEER
                        || rc == VERR_NET_CONNECTION_REFUSED
                        || rc == VERR_BROKEN_PIPE)))
    {
        /* Standardize return value for broken connection. */
        rc = VERR_BROKEN_PIPE;
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


static int iscsiTransportWrite(PISCSIIMAGE pImage, PISCSIREQ paRequest, unsigned int cnRequest)
{
    int rc = VINF_SUCCESS;
    uint32_t pad = 0;
    unsigned int i;

    LogFlow(("drvISCSITransportTcpWrite: cnRequest=%d (%s:%d)\n", cnRequest, pImage->pszHostname, pImage->uPort));
    if (pImage->Socket == NIL_RTSOCKET)
    {
        /* Attempt to reconnect if the connection was previously broken. */
        if (pImage->pszHostname != NULL)
        {
            rc = pImage->pInterfaceNetCallbacks->pfnClientConnect(pImage->pszHostname, pImage->uPort, &pImage->Socket);
            if (RT_UNLIKELY(   RT_FAILURE(rc)
                            && (   rc == VERR_NET_CONNECTION_REFUSED
                                || rc == VERR_NET_CONNECTION_RESET
                                || rc == VERR_NET_UNREACHABLE
                                || rc == VERR_NET_HOST_UNREACHABLE
                                || rc == VERR_NET_CONNECTION_TIMED_OUT)))
            {
                /* Standardize return value for no connection. */
                rc = VERR_NET_CONNECTION_REFUSED;
            }
        }
    }

    if (RT_SUCCESS(rc))
    {
        for (i = 0; i < cnRequest; i++)
        {
            /* Write one chunk of data. */
            rc = pImage->pInterfaceNetCallbacks->pfnWrite(pImage->Socket,
                                                          paRequest[i].pcvSeg,
                                                          paRequest[i].cbSeg);
            if (RT_FAILURE(rc))
                break;
            /* Insert proper padding before the next chunk us written. */
            if (paRequest[i].cbSeg & 3)
            {
                rc = pImage->pInterfaceNetCallbacks->pfnWrite(pImage->Socket,
                                                              &pad,
                                                              4 - (paRequest[i].cbSeg & 3));
                if (RT_FAILURE(rc))
                    break;
            }
        }
        /* Send out the request as soon as possible, otherwise the target will
         * answer after an unnecessary delay. */
        pImage->pInterfaceNetCallbacks->pfnFlush(pImage->Socket);
    }

    if (RT_UNLIKELY(    RT_FAILURE(rc)
                    && (   rc == VERR_NET_CONNECTION_RESET
                        || rc == VERR_NET_CONNECTION_ABORTED
                        || rc == VERR_NET_CONNECTION_RESET_BY_PEER
                        || rc == VERR_NET_CONNECTION_REFUSED
                        || rc == VERR_BROKEN_PIPE)))
    {
        /* Standardize return value for broken connection. */
        rc = VERR_BROKEN_PIPE;
    }

    LogFlow(("drvISCSITransportTcpWrite: returns %Rrc\n", rc));
    return rc;
}


static int iscsiTransportOpen(PISCSIIMAGE pImage)
{
    int rc = VINF_SUCCESS;
    size_t cbHostname = 0; /* shut up gcc */
    const char *pcszPort = NULL; /* shut up gcc */
    char *pszPortEnd;
    uint16_t uPort;

    /* Clean up previous connection data. */
    if (pImage->Socket != NIL_RTSOCKET)
    {
        pImage->pInterfaceNetCallbacks->pfnClientClose(pImage->Socket);
        pImage->Socket = NIL_RTSOCKET;
    }
    if (pImage->pszHostname)
    {
        RTMemFree(pImage->pszHostname);
        pImage->pszHostname = NULL;
        pImage->uPort = 0;
    }

    /* Locate the port number via the colon separating the hostname from the port. */
    if (*pImage->pszTargetAddress)
    {
        if (*pImage->pszTargetAddress != '[')
        {
            /* Normal hostname or IPv4 dotted decimal. */
            pcszPort = strchr(pImage->pszTargetAddress, ':');
            if (pcszPort != NULL)
            {
                cbHostname = pcszPort - pImage->pszTargetAddress;
                pcszPort++;
            }
            else
                cbHostname = strlen(pImage->pszTargetAddress);
        }
        else
        {
            /* IPv6 literal address. Contains colons, so skip to closing square bracket. */
            pcszPort = strchr(pImage->pszTargetAddress, ']');
            if (pcszPort != NULL)
            {
                pcszPort++;
                cbHostname = pcszPort - pImage->pszTargetAddress;
                if (*pcszPort == '\0')
                    pcszPort = NULL;
                else if (*pcszPort != ':')
                    rc = VERR_PARSE_ERROR;
                else
                    pcszPort++;
            }
            else
                rc = VERR_PARSE_ERROR;
        }
    }
    else
        rc = VERR_PARSE_ERROR;

    /* Now split address into hostname and port. */
    if (RT_SUCCESS(rc))
    {
        pImage->pszHostname = (char *)RTMemAlloc(cbHostname + 1);
        if (!pImage->pszHostname)
            rc = VERR_NO_MEMORY;
        else
        {
            memcpy(pImage->pszHostname, pImage->pszTargetAddress, cbHostname);
            pImage->pszHostname[cbHostname] = '\0';
            if (pcszPort != NULL)
            {
                rc = RTStrToUInt16Ex(pcszPort, &pszPortEnd, 0, &uPort);
                /* Note that RT_SUCCESS() macro to check the rc value is not strict enough in this case. */
                if (rc == VINF_SUCCESS && *pszPortEnd == '\0' && uPort != 0)
                {
                    pImage->uPort = uPort;
                }
                else
                {
                    rc = VERR_PARSE_ERROR;
                }
            }
            else
                pImage->uPort = ISCSI_DEFAULT_PORT;
        }
    }

    if (RT_FAILURE(rc))
    {
        if (pImage->pszHostname)
        {
            RTMemFree(pImage->pszHostname);
            pImage->pszHostname = NULL;
        }
        pImage->uPort = 0;
    }

    /* Note that in this implementation the actual connection establishment is
     * delayed until a PDU is read or written. */
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


static int iscsiTransportClose(PISCSIIMAGE pImage)
{
    int rc;

    LogFlowFunc(("(%s:%d)\n", pImage->pszHostname, pImage->uPort));
    if (pImage->Socket != NIL_RTSOCKET)
    {
        rc = pImage->pInterfaceNetCallbacks->pfnClientClose(pImage->Socket);
        pImage->Socket = NIL_RTSOCKET;
    }
    else
        rc = VINF_SUCCESS;
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/**
 * Attach to an iSCSI target. Performs all operations necessary to enter
 * Full Feature Phase.
 *
 * @returns VBox status.
 * @param   pImage  The iSCSI connection state to be used.
 */
static int iscsiAttach(PISCSIIMAGE pImage)
{
    int rc;
    uint32_t itt;
    uint32_t csg, nsg, substate;
    uint64_t isid_tsih;
    uint8_t bBuf[4096];         /* Should be large enough even for large authentication values. */
    size_t cbBuf;
    bool transit;
    uint8_t pbChallenge[1024];  /* RFC3720 specifies this as maximum. */
    size_t cbChallenge = 0;     /* shut up gcc */
    uint8_t bChapIdx;
    uint8_t aResponse[RTMD5HASHSIZE];
    uint32_t cnISCSIReq;
    ISCSIREQ aISCSIReq[4];
    uint32_t aReqBHS[12];
    uint32_t cnISCSIRes;
    ISCSIRES aISCSIRes[2];
    uint32_t aResBHS[12];
    char *pszNext;
    LogFlowFunc(("entering\n"));

    Assert(pImage->state == ISCSISTATE_FREE);

    RTSemMutexRequest(pImage->Mutex, RT_INDEFINITE_WAIT);

    /* Make 100% sure the connection isn't reused for a new login. */
    iscsiTransportClose(pImage);

restart:
    pImage->state = ISCSISTATE_IN_LOGIN;
    pImage->ITT = 1;
    pImage->FirstRecvPDU = true;
    pImage->CmdSN = 1;
    pImage->ExpCmdSN = 0;
    pImage->MaxCmdSN = 1;
    pImage->ExpStatSN = 1;

    /*
     * Send login request to target.
     */
    itt = iscsiNewITT(pImage);
    csg = 0;
    nsg = 0;
    substate = 0;
    isid_tsih = pImage->ISID << 16;  /* TSIH field currently always 0 */

    do {
        transit = false;
        cbBuf = 0;
        /* Handle all cases with a single switch statement. */
        switch (csg << 8 | substate)
        {
            case 0x0000:    /* security negotiation, step 0: propose authentication. */
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "SessionType", "Normal", 0);
                if (RT_FAILURE(rc))
                    goto out;
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "InitiatorName", pImage->pszInitiatorName, 0);
                if (RT_FAILURE(rc))
                    goto out;
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "TargetName", pImage->pszTargetName, 0);
                if (RT_FAILURE(rc))
                    goto out;
                if (pImage->pszInitiatorUsername == NULL)
                {
                    /* No authentication. Immediately switch to next phase. */
                    rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "AuthMethod", "None", 0);
                    if (RT_FAILURE(rc))
                        goto out;
                    nsg = 1;
                    transit = true;
                }
                else
                {
                    rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "AuthMethod", "CHAP,None", 0);
                    if (RT_FAILURE(rc))
                        goto out;
                }
                break;
            case 0x0001:    /* security negotiation, step 1: propose CHAP_MD5 variant. */
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "CHAP_A", "5", 0);
                if (RT_FAILURE(rc))
                    goto out;
                break;
            case 0x0002:    /* security negotiation, step 2: send authentication info. */
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "CHAP_N", pImage->pszInitiatorUsername, 0);
                if (RT_FAILURE(rc))
                    goto out;
                chap_md5_compute_response(aResponse, bChapIdx, pbChallenge, cbChallenge,
                                          pImage->pbInitiatorSecret, pImage->cbInitiatorSecret);
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "CHAP_R", (const char *)aResponse, RTMD5HASHSIZE);
                if (RT_FAILURE(rc))
                    goto out;
                nsg = 1;
                transit = true;
                break;
            case 0x0100:    /* login operational negotiation, step 0: set parameters. */
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "HeaderDigest", "None", 0);
                if (RT_FAILURE(rc))
                    goto out;
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "DataDigest", "None", 0);
                if (RT_FAILURE(rc))
                    goto out;
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "MaxConnections", "1", 0);
                if (RT_FAILURE(rc))
                    goto out;
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "InitialR2T", "No", 0);
                if (RT_FAILURE(rc))
                    goto out;
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "ImmediateData", "Yes", 0);
                if (RT_FAILURE(rc))
                    goto out;
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "MaxRecvDataSegmentLength", "65536", 0);
                if (RT_FAILURE(rc))
                    goto out;
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "MaxBurstLength", "262144", 0);
                if (RT_FAILURE(rc))
                    goto out;
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "FirstBurstLength", "65536", 0);
                if (RT_FAILURE(rc))
                    goto out;
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "DefaultTime2Wait", "0", 0);
                if (RT_FAILURE(rc))
                    goto out;
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "DefaultTime2Retain", "60", 0);
                if (RT_FAILURE(rc))
                    goto out;
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "DataPDUInOrder", "Yes", 0);
                if (RT_FAILURE(rc))
                    goto out;
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "DataSequenceInOrder", "Yes", 0);
                if (RT_FAILURE(rc))
                    goto out;
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "ErrorRecoveryLevel", "0", 0);
                if (RT_FAILURE(rc))
                    goto out;
                rc = iscsiTextAddKeyValue(bBuf, sizeof(bBuf), &cbBuf, "MaxOutstandingR2T", "1", 0);
                if (RT_FAILURE(rc))
                    goto out;
                nsg = 3;
                transit = true;
                break;
            case 0x0300:    /* full feature phase. */
            default:
                /* Should never come here. */
                AssertMsgFailed(("send: Undefined login state %d substate %d\n", csg, substate));
                break;
        }

        aReqBHS[0] = RT_H2N_U32(    ISCSI_IMMEDIATE_DELIVERY_BIT
                                |   (csg << ISCSI_CSG_SHIFT)
                                |   (transit ? (nsg << ISCSI_NSG_SHIFT | ISCSI_TRANSIT_BIT) : 0)
                                |   ISCSI_MY_VERSION            /* Minimum version. */
                                |   (ISCSI_MY_VERSION << 8)     /* Maximum version. */
                                |   ISCSIOP_LOGIN_REQ);     /* C=0 */
        aReqBHS[1] = RT_H2N_U32((uint32_t)cbBuf);     /* TotalAHSLength=0 */
        aReqBHS[2] = RT_H2N_U32(isid_tsih >> 32);
        aReqBHS[3] = RT_H2N_U32(isid_tsih & 0xffffffff);
        aReqBHS[4] = itt;
        aReqBHS[5] = RT_H2N_U32(1 << 16);   /* CID=1,reserved */
        aReqBHS[6] = RT_H2N_U32(pImage->CmdSN);
        aReqBHS[7] = RT_H2N_U32(pImage->ExpStatSN);
        aReqBHS[8] = 0;             /* reserved */
        aReqBHS[9] = 0;             /* reserved */
        aReqBHS[10] = 0;            /* reserved */
        aReqBHS[11] = 0;            /* reserved */

        cnISCSIReq = 0;
        aISCSIReq[cnISCSIReq].pcvSeg = aReqBHS;
        aISCSIReq[cnISCSIReq].cbSeg = sizeof(aReqBHS);
        cnISCSIReq++;

        aISCSIReq[cnISCSIReq].pcvSeg = bBuf;
        aISCSIReq[cnISCSIReq].cbSeg = cbBuf;
        cnISCSIReq++;

        rc = iscsiSendPDU(pImage, aISCSIReq, cnISCSIReq);
        if (RT_SUCCESS(rc))
        {
            ISCSIOPCODE cmd;
            ISCSILOGINSTATUSCLASS loginStatusClass;

            /* Place login request in queue. */
            pImage->paCurrReq = aISCSIReq;
            pImage->cnCurrReq = cnISCSIReq;

            cnISCSIRes = 0;
            aISCSIRes[cnISCSIRes].pvSeg = aResBHS;
            aISCSIRes[cnISCSIRes].cbSeg = sizeof(aResBHS);
            cnISCSIRes++;
            aISCSIRes[cnISCSIRes].pvSeg = bBuf;
            aISCSIRes[cnISCSIRes].cbSeg = sizeof(bBuf);
            cnISCSIRes++;

            rc = iscsiRecvPDU(pImage, itt, aISCSIRes, cnISCSIRes);
            if (RT_FAILURE(rc))
                break;
            /** @todo collect partial login responses with Continue bit set. */
            Assert(aISCSIRes[0].pvSeg == aResBHS);
            Assert(aISCSIRes[0].cbSeg >= ISCSI_BHS_SIZE);
            Assert((RT_N2H_U32(aResBHS[0]) & ISCSI_CONTINUE_BIT) == 0);

            cmd = (ISCSIOPCODE)(RT_N2H_U32(aResBHS[0]) & ISCSIOP_MASK);
            if (cmd == ISCSIOP_LOGIN_RES)
            {
                if ((RT_N2H_U32(aResBHS[0]) & 0xff) != ISCSI_MY_VERSION)
                {
                    iscsiTransportClose(pImage);
                    rc = VERR_PARSE_ERROR;
                    break;  /* Give up immediately, as a RFC violation in version fields is very serious. */
                }

                loginStatusClass = (ISCSILOGINSTATUSCLASS)(RT_N2H_U32(aResBHS[9]) >> 24);
                switch (loginStatusClass)
                {
                    case ISCSI_LOGIN_STATUS_CLASS_SUCCESS:
                        uint32_t targetCSG;
                        uint32_t targetNSG;
                        bool targetTransit;

                        if (pImage->FirstRecvPDU)
                        {
                            pImage->FirstRecvPDU = false;
                            pImage->ExpStatSN = RT_N2H_U32(aResBHS[6]) + 1;
                        }

                        targetCSG = (RT_N2H_U32(aResBHS[0]) & ISCSI_CSG_MASK) >> ISCSI_CSG_SHIFT;
                        targetNSG = (RT_N2H_U32(aResBHS[0]) & ISCSI_NSG_MASK) >> ISCSI_NSG_SHIFT;
                        targetTransit = !!(RT_N2H_U32(aResBHS[0]) & ISCSI_TRANSIT_BIT);

                        /* Handle all cases with a single switch statement. */
                        switch (csg << 8 | substate)
                        {
                            case 0x0000:    /* security negotiation, step 0: receive final authentication. */
                                const char *pcszAuthMethod;

                                rc = iscsiTextGetKeyValue(bBuf, aISCSIRes[1].cbSeg, "AuthMethod", &pcszAuthMethod);
                                if (RT_FAILURE(rc))
                                {
                                    rc = VERR_PARSE_ERROR;
                                    break;
                                }
                                if (strcmp(pcszAuthMethod, "None") == 0)
                                {
                                    /* Authentication offered, but none required.  Skip to operational parameters. */
                                    csg = 1;
                                    nsg = 1;
                                    transit = true;
                                    substate = 0;
                                    break;
                                }
                                else if (strcmp(pcszAuthMethod, "CHAP") == 0 && targetNSG == 0 && !targetTransit)
                                {
                                    /* CHAP authentication required, continue with next substate. */
                                    substate++;
                                    break;
                                }

                                /* Unknown auth method or login response PDU headers incorrect. */
                                rc = VERR_PARSE_ERROR;
                                break;
                            case 0x0001:    /* security negotiation, step 1: receive final CHAP variant and challenge. */
                                const char *pcszChapAuthMethod;
                                const char *pcszChapIdxTarget;
                                const char *pcszChapChallengeStr;

                                rc = iscsiTextGetKeyValue(bBuf, aISCSIRes[1].cbSeg, "CHAP_A", &pcszChapAuthMethod);
                                if (RT_FAILURE(rc))
                                {
                                    rc = VERR_PARSE_ERROR;
                                    break;
                                }
                                if (strcmp(pcszChapAuthMethod, "5") != 0)
                                {
                                    rc = VERR_PARSE_ERROR;
                                    break;
                                }
                                rc = iscsiTextGetKeyValue(bBuf, aISCSIRes[1].cbSeg, "CHAP_I", &pcszChapIdxTarget);
                                if (RT_FAILURE(rc))
                                {
                                    rc = VERR_PARSE_ERROR;
                                    break;
                                }
                                rc = RTStrToUInt8Ex(pcszChapIdxTarget, &pszNext, 0, &bChapIdx);
                                if ((rc > VINF_SUCCESS) || *pszNext != '\0')
                                {
                                    rc = VERR_PARSE_ERROR;
                                    break;
                                }
                                rc = iscsiTextGetKeyValue(bBuf, aISCSIRes[1].cbSeg, "CHAP_C", &pcszChapChallengeStr);
                                if (RT_FAILURE(rc))
                                {
                                    rc = VERR_PARSE_ERROR;
                                    break;
                                }
                                cbChallenge = sizeof(pbChallenge);
                                rc = iscsiStrToBinary(pcszChapChallengeStr, pbChallenge, &cbChallenge);
                                if (RT_FAILURE(rc))
                                    break;
                                substate++;
                                transit = true;
                                break;
                            case 0x0002:    /* security negotiation, step 2: check authentication success. */
                                if (targetCSG == 0 && targetNSG == 1 && targetTransit)
                                {
                                    /* Target wants to continue in login operational state, authentication success. */
                                    csg = 1;
                                    nsg = 3;
                                    substate = 0;
                                    break;
                                }
                                rc = VERR_PARSE_ERROR;
                                break;
                            case 0x0100:    /* login operational negotiation, step 0: check results. */
                                if (targetCSG == 1 && targetNSG == 3 && targetTransit)
                                {
                                    /* Target wants to continue in full feature phase, login finished. */
                                    csg = 3;
                                    nsg = 3;
                                    substate = 0;
                                    break;
                                }
                                rc = VERR_PARSE_ERROR;
                                break;
                            case 0x0300:    /* full feature phase. */
                            default:
                                AssertMsgFailed(("recv: Undefined login state %d substate %d\n", csg, substate));
                                rc = VERR_PARSE_ERROR;
                                break;
                        }
                        break;
                    case ISCSI_LOGIN_STATUS_CLASS_REDIRECTION:
                        const char *pcszTargetRedir;

                        /* Target has moved to some other location, as indicated in the TargetAddress key. */
                        rc = iscsiTextGetKeyValue(bBuf, aISCSIRes[1].cbSeg, "TargetAddress", &pcszTargetRedir);
                        if (RT_FAILURE(rc))
                        {
                            rc = VERR_PARSE_ERROR;
                            break;
                        }
                        if (pImage->pszTargetAddress)
                            RTMemFree(pImage->pszTargetAddress);
                        {
                            size_t cb = strlen(pcszTargetRedir) + 1;
                            pImage->pszTargetAddress = (char *)RTMemAlloc(cb);
                            if (!pImage->pszTargetAddress)
                            {
                                rc = VERR_NO_MEMORY;
                                break;
                            }
                            memcpy(pImage->pszTargetAddress, pcszTargetRedir, cb);
                        }
                        rc = iscsiTransportOpen(pImage);
                        goto restart;
                    case ISCSI_LOGIN_STATUS_CLASS_INITIATOR_ERROR:
                        iscsiTransportClose(pImage);
                        pImage->paCurrReq = NULL;
                        pImage->cnCurrReq = 0;
                        rc = VERR_IO_GEN_FAILURE;
                        goto out;
                    case ISCSI_LOGIN_STATUS_CLASS_TARGET_ERROR:
                        iscsiTransportClose(pImage);
                        rc = VINF_EOF;
                        break;
                    default:
                        rc = VERR_PARSE_ERROR;
                }

                /* Remove login request from queue. */
                pImage->paCurrReq = NULL;
                pImage->cnCurrReq = 0;

                if (csg == 3)
                {
                    /*
                     * Finished login, continuing with Full Feature Phase.
                     */
                    rc = VINF_SUCCESS;
                    break;
                }
            }
            else
            {
                AssertMsgFailed(("%s: ignoring unexpected PDU with first word = %#08x\n", __FUNCTION__, RT_N2H_U32(aResBHS[0])));
            }
        }
        else
            break;
    } while (true);

out:
    if (RT_FAILURE(rc))
    {
        /*
         * Close connection to target.
         */
        iscsiTransportClose(pImage);
        pImage->state = ISCSISTATE_FREE;
    }
    else
        pImage->state = ISCSISTATE_NORMAL;

    RTSemMutexRelease(pImage->Mutex);

    LogFlowFunc(("returning %Rrc\n", rc));
    LogRel(("iSCSI: login to target %s %s\n", pImage->pszTargetName, RT_SUCCESS(rc) ? "successful" : "failed"));
    return rc;
}


/**
 * Detach from an iSCSI target.
 *
 * @returns VBox status.
 * @param   pImage  The iSCSI connection state to be used.
 */
static int iscsiDetach(PISCSIIMAGE pImage)
{
    int rc;
    uint32_t itt;
    uint32_t cnISCSIReq = 0;
    ISCSIREQ aISCSIReq[4];
    uint32_t aReqBHS[12];
    LogFlow(("drvISCSIDetach: entering\n"));

    RTSemMutexRequest(pImage->Mutex, RT_INDEFINITE_WAIT);

    if (pImage->state != ISCSISTATE_FREE && pImage->state != ISCSISTATE_IN_LOGOUT)
    {
        pImage->state = ISCSISTATE_IN_LOGOUT;

        /*
         * Send logout request to target.
         */
        itt = iscsiNewITT(pImage);
        aReqBHS[0] = RT_H2N_U32(ISCSI_FINAL_BIT | ISCSIOP_LOGOUT_REQ);  /* I=0,F=1,Reason=close session */
        aReqBHS[1] = RT_H2N_U32(0); /* TotalAHSLength=0,DataSementLength=0 */
        aReqBHS[2] = 0;             /* reserved */
        aReqBHS[3] = 0;             /* reserved */
        aReqBHS[4] = itt;
        aReqBHS[5] = 0;             /* reserved */
        aReqBHS[6] = RT_H2N_U32(pImage->CmdSN);
        aReqBHS[7] = RT_H2N_U32(pImage->ExpStatSN);
        aReqBHS[8] = 0;             /* reserved */
        aReqBHS[9] = 0;             /* reserved */
        aReqBHS[10] = 0;            /* reserved */
        aReqBHS[11] = 0;            /* reserved */
        pImage->CmdSN++;

        aISCSIReq[cnISCSIReq].pcvSeg = aReqBHS;
        aISCSIReq[cnISCSIReq].cbSeg = sizeof(aReqBHS);
        cnISCSIReq++;

        rc = iscsiSendPDU(pImage, aISCSIReq, cnISCSIReq);
        if (RT_SUCCESS(rc))
        {
            /* Place logout request in queue. */
            pImage->paCurrReq = aISCSIReq;
            pImage->cnCurrReq = cnISCSIReq;

            /*
             * Read logout response from target.
             */
            ISCSIRES aISCSIRes;
            uint32_t aResBHS[12];

            aISCSIRes.pvSeg = aResBHS;
            aISCSIRes.cbSeg = sizeof(aResBHS);
            rc = iscsiRecvPDU(pImage, itt, &aISCSIRes, 1);
            if (RT_SUCCESS(rc))
            {
                if (RT_N2H_U32(aResBHS[0]) != (ISCSI_FINAL_BIT | ISCSIOP_LOGOUT_RES))
                    AssertMsgFailed(("iSCSI Logout response invalid\n"));
            }
            else
                AssertMsgFailed(("iSCSI Logout response error, rc=%Rrc\n", rc));

            /* Remove logout request from queue. */
            pImage->paCurrReq = NULL;
            pImage->cnCurrReq = 0;
        }
        else
            AssertMsgFailed(("Could not send iSCSI Logout request, rc=%Rrc\n", rc));
    }

    if (pImage->state != ISCSISTATE_FREE)
    {
        /*
         * Close connection to target.
         */
        rc = iscsiTransportClose(pImage);
        if (RT_FAILURE(rc))
            AssertMsgFailed(("Could not close connection to target, rc=%Rrc\n", rc));
    }

    pImage->state = ISCSISTATE_FREE;

    RTSemMutexRelease(pImage->Mutex);

    LogFlow(("drvISCSIDetach: leaving\n"));
    LogRel(("iSCSI: logout to target %s\n", pImage->pszTargetName));
    return VINF_SUCCESS;
}


/**
 * Perform a command on an iSCSI target. Target must be already in
 * Full Feature Phase.
 *
 * @returns VBOX status.
 * @param   pImage  The iSCSI connection state to be used.
 * @param   pRequest        Command descriptor. Contains all information about
 *                          the command, its transfer directions and pointers
 *                          to the buffer(s) used for transferring data and
 *                          status information.
 */
static int iscsiCommand(PISCSIIMAGE pImage, PSCSIREQ pRequest)
{
    int rc;
    uint32_t itt;
    uint32_t cbData;
    uint32_t cnISCSIReq = 0;
    ISCSIREQ aISCSIReq[4];
    uint32_t aReqBHS[12];

    uint32_t *pDst = NULL;
    size_t cbBufLength;
    uint32_t aStat[64];
    uint32_t ExpDataSN = 0;
    bool final = false;

    LogFlow(("iscsiCommand: entering, CmdSN=%d\n", pImage->CmdSN));

    Assert(pRequest->enmXfer != SCSIXFER_TO_FROM_TARGET);   /**< @todo not yet supported, would require AHS. */
    Assert(pRequest->cbI2TData <= 0xffffff);    /* larger transfers would require R2T support. */
    Assert(pRequest->cbCmd <= 16);      /* would cause buffer overrun below. */

    /* If not in normal state, then the transport connection was dropped. Try
     * to reestablish by logging in, the target might be responsive again. */
    if (pImage->state == ISCSISTATE_FREE)
        rc = iscsiAttach(pImage);

    /* If still not in normal state, then the underlying transport connection
     * cannot be established. Get out before bad things happen (and make
     * sure the caller suspends the VM again). */
    if (pImage->state != ISCSISTATE_NORMAL)
    {
        rc = VERR_NET_CONNECTION_REFUSED;
        goto out;
    }

    /*
     * Send SCSI command to target with all I2T data included.
     */
    cbData = 0;
    if (pRequest->enmXfer == SCSIXFER_FROM_TARGET)
        cbData = (uint32_t)pRequest->cbT2IData;
    else
        cbData = (uint32_t)pRequest->cbI2TData;

    RTSemMutexRequest(pImage->Mutex, RT_INDEFINITE_WAIT);

    itt = iscsiNewITT(pImage);
    memset(aReqBHS, 0, sizeof(aReqBHS));
    aReqBHS[0] = RT_H2N_U32(    ISCSI_FINAL_BIT | ISCSI_TASK_ATTR_ORDERED | ISCSIOP_SCSI_CMD
                            |   (pRequest->enmXfer << 21)); /* I=0,F=1,Attr=Ordered */
    aReqBHS[1] = RT_H2N_U32(0x00000000 | ((uint32_t)pRequest->cbI2TData & 0xffffff)); /* TotalAHSLength=0 */
    aReqBHS[2] = RT_H2N_U32(pImage->LUN >> 32);
    aReqBHS[3] = RT_H2N_U32(pImage->LUN & 0xffffffff);
    aReqBHS[4] = itt;
    aReqBHS[5] = RT_H2N_U32(cbData);
    aReqBHS[6] = RT_H2N_U32(pImage->CmdSN);
    aReqBHS[7] = RT_H2N_U32(pImage->ExpStatSN);
    memcpy(aReqBHS + 8, pRequest->pvCmd, pRequest->cbCmd);
    pImage->CmdSN++;

    aISCSIReq[cnISCSIReq].pcvSeg = aReqBHS;
    aISCSIReq[cnISCSIReq].cbSeg = sizeof(aReqBHS);
    cnISCSIReq++;

    if (    pRequest->enmXfer == SCSIXFER_TO_TARGET
        ||  pRequest->enmXfer == SCSIXFER_TO_FROM_TARGET)
    {
        aISCSIReq[cnISCSIReq].pcvSeg = pRequest->pcvI2TData;
        aISCSIReq[cnISCSIReq].cbSeg = pRequest->cbI2TData;  /* Padding done by transport. */
        cnISCSIReq++;
    }

    rc = iscsiSendPDU(pImage, aISCSIReq, cnISCSIReq);
    if (RT_FAILURE(rc))
        goto out_release;

    /* Place SCSI request in queue. */
    pImage->paCurrReq = aISCSIReq;
    pImage->cnCurrReq = cnISCSIReq;

    /*
     * Read SCSI response/data in PDUs from target.
     */
    if (    pRequest->enmXfer == SCSIXFER_FROM_TARGET
        ||  pRequest->enmXfer == SCSIXFER_TO_FROM_TARGET)
    {
        pDst = (uint32_t *)pRequest->pvT2IData;
        cbBufLength = pRequest->cbT2IData;
    }
    else
        cbBufLength = 0;

    do {
        uint32_t cnISCSIRes = 0;
        ISCSIRES aISCSIRes[4];
        uint32_t aResBHS[12];

        aISCSIRes[cnISCSIRes].pvSeg = aResBHS;
        aISCSIRes[cnISCSIRes].cbSeg = sizeof(aResBHS);
        cnISCSIRes++;
        if (cbBufLength != 0 &&
            (   pRequest->enmXfer == SCSIXFER_FROM_TARGET
             ||  pRequest->enmXfer == SCSIXFER_TO_FROM_TARGET))
        {
            aISCSIRes[cnISCSIRes].pvSeg = pDst;
            aISCSIRes[cnISCSIRes].cbSeg = cbBufLength;
            cnISCSIRes++;
        }
        /* Always reserve space for the status - it's impossible to tell
         * beforehand whether this will be the final PDU or not. */
        aISCSIRes[cnISCSIRes].pvSeg = aStat;
        aISCSIRes[cnISCSIRes].cbSeg = sizeof(aStat);
        cnISCSIRes++;

        rc = iscsiRecvPDU(pImage, itt, aISCSIRes, cnISCSIRes);
        if (RT_FAILURE(rc))
            break;

        final = !!(RT_N2H_U32(aResBHS[0]) & ISCSI_FINAL_BIT);
        ISCSIOPCODE cmd = (ISCSIOPCODE)(RT_N2H_U32(aResBHS[0]) & ISCSIOP_MASK);
        if (cmd == ISCSIOP_SCSI_RES)
        {
            /* This is the final PDU which delivers the status (and may be omitted if
             * the last Data-In PDU included successful completion status). */
            if (!final || ((RT_N2H_U32(aResBHS[0]) & 0x0000ff00) != 0) || (RT_N2H_U32(aResBHS[9]) != ExpDataSN))
            {
                /* SCSI Response in the wrong place or with a (target) failure. */
                rc = VERR_PARSE_ERROR;
                break;
            }
            pRequest->status = RT_N2H_U32(aResBHS[0]) & 0x000000ff;
            uint32_t cbData = RT_N2H_U32(aResBHS[1]) & 0x00ffffff;
            if (cbData >= 2)
            {
                uint32_t cbStat = RT_N2H_U32(aStat[0]) >> 16;
                if (cbStat + 2 > cbData || cbStat > pRequest->cbSense)
                {
                    rc = VERR_BUFFER_OVERFLOW;
                    break;
                }
                pRequest->cbSense = RT_N2H_U32(aStat[0]) >> 16;
                memcpy(pRequest->pvSense, ((const uint8_t *)aStat) + 2, pRequest->cbSense);
            }
            else if (cbData == 1)
            {
                rc = VERR_PARSE_ERROR;
                break;
            }
            break;
        }
        else if (cmd == ISCSIOP_SCSI_DATA_IN)
        {
            /* A Data-In PDU carries some data that needs to be added to the received
             * data in response to the command. There may be both partial and complete
             * Data-In PDUs, so collect data until the status is included or the status
             * is sent in a separate SCSI Result frame (see above). */
            if (final && aISCSIRes[2].cbSeg != 0)
            {
                /* The received PDU is partially stored in the buffer for status.
                 * Must not happen under normal circumstances and is a target error. */
                rc = VERR_BUFFER_OVERFLOW;
                break;
            }
            uint32_t len = RT_N2H_U32(aResBHS[1]) & 0x00ffffff;
            pDst = (uint32_t *)((char *)pDst + len);
            cbBufLength -= len;
            ExpDataSN++;
            if (final && (RT_N2H_U32(aResBHS[0]) & ISCSI_STATUS_BIT) != 0)
            {
                pRequest->status = RT_N2H_U32(aResBHS[0]) & 0x000000ff;
                pRequest->cbSense = 0;
                break;
            }
        }
        else
        {
            rc = VERR_PARSE_ERROR;
            break;
        }
    } while (true);

    /* Remove SCSI request from queue. */
    pImage->paCurrReq = NULL;
    pImage->cnCurrReq = 0;

out_release:
    if (rc == VERR_TIMEOUT)
    {
        /* Drop connection in case the target plays dead. Much better than
         * delaying the next requests until the timed out command actually
         * finishes. Also keep in mind that command shouldn't take longer than
         * about 30-40 seconds, or the guest will lose its patience. */
        iscsiTransportClose(pImage);
        pImage->state = ISCSISTATE_FREE;
    }
    RTSemMutexRelease(pImage->Mutex);

out:
    LogFlow(("iscsiCommand: returns %Rrc\n", rc));
    return rc;
}


/**
 * Generate a new Initiator Task Tag.
 *
 * @returns Initiator Task Tag.
 * @param   pImage  The iSCSI connection state to be used.
 */
static uint32_t iscsiNewITT(PISCSIIMAGE pImage)
{
    uint32_t next_itt;

    next_itt = pImage->ITT++;
    if (pImage->ITT == ISCSI_TASK_TAG_RSVD)
        pImage->ITT = 0;
    return RT_H2N_U32(next_itt);
}


/**
 * Send an iSCSI request. The request can consist of several segments, which
 * are padded to 4 byte boundaries and concatenated.
 *
 * @returns VBOX status
 * @param   pImage  The iSCSI connection state to be used.
 * @param   paReq      Pointer to array of iSCSI request sections.
 * @param   cnReq      Number of valid iSCSI request sections in the array.
 */
static int iscsiSendPDU(PISCSIIMAGE pImage, PISCSIREQ paReq, uint32_t cnReq)
{
    int rc = VINF_SUCCESS;
    uint32_t i;
    /** @todo return VERR_VD_ISCSI_INVALID_STATE in the appropriate situations,
     * needs cleaning up of timeout/disconnect handling a bit, as otherwise
     * too many incorrect errors are signalled. */
    Assert(pImage->paCurrReq == NULL);
    Assert(cnReq >= 1);
    Assert(paReq[0].cbSeg >= ISCSI_BHS_SIZE);

    for (i = 0; i < pImage->cISCSIRetries; i++)
    {
        rc = iscsiTransportWrite(pImage, paReq, cnReq);
        if (RT_SUCCESS(rc))
            break;
        if (rc != VERR_BROKEN_PIPE && rc != VERR_NET_CONNECTION_REFUSED)
            break;
        RTThreadSleep(500);
        if (   pImage->state != ISCSISTATE_IN_LOGIN
            && pImage->state != ISCSISTATE_IN_LOGOUT)
        {
            /* Attempt to re-login when a connection fails, but only when not
             * currently logging in or logging out. */
            rc = iscsiAttach(pImage);
            if (RT_FAILURE(rc))
                break;
        }
    }
    return rc;
}


/**
 * Wait for an iSCSI response with a matching Initiator Target Tag. The response is
 * split into several segments, as requested by the caller-provided buffer specification.
 * Remember that the response can be split into several PDUs by the sender, so make
 * sure that all parts are collected and processed appropriately by the caller.
 *
 * @returns VBOX status
 * @param   pImage  The iSCSI connection state to be used.
 * @param   paRes      Pointer to array of iSCSI response sections.
 * @param   cnRes      Number of valid iSCSI response sections in the array.
 */
static int iscsiRecvPDU(PISCSIIMAGE pImage, uint32_t itt, PISCSIRES paRes, uint32_t cnRes)
{
    int rc = VINF_SUCCESS;
    uint32_t i;
    ISCSIRES aResBuf;

    for (i = 0; i < pImage->cISCSIRetries; i++)
    {
        aResBuf.pvSeg = pImage->pvRecvPDUBuf;
        aResBuf.cbSeg = pImage->cbRecvPDUBuf;
        rc = iscsiTransportRead(pImage, &aResBuf, 1);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_BROKEN_PIPE || rc == VERR_NET_CONNECTION_REFUSED)
            {
                /* Connection broken while waiting for a response - wait a while and
                 * try to restart by re-sending the original request (if any).
                 * This also handles the connection reestablishment (login etc.). */
                RTThreadSleep(500);
                if (pImage->paCurrReq != NULL)
                {
                    rc = iscsiSendPDU(pImage, pImage->paCurrReq, pImage->cnCurrReq);
                    if (RT_FAILURE(rc))
                        break;
                }
            }
            else
            {
                /* Signal other errors (VERR_BUFFER_OVERFLOW etc.) to the caller. */
                break;
            }
        }
        else
        {
            ISCSIOPCODE cmd;
            const uint32_t *pcvResSeg = (const uint32_t *)aResBuf.pvSeg;

            /* Check whether the received PDU is valid, and update the internal state of
             * the iSCSI connection/session. */
            rc = drvISCSIValidatePDU(&aResBuf, 1);
            if (RT_FAILURE(rc))
                continue;
            cmd = (ISCSIOPCODE)(RT_N2H_U32(pcvResSeg[0]) & ISCSIOP_MASK);
            switch (cmd)
            {
                case ISCSIOP_SCSI_RES:
                case ISCSIOP_SCSI_TASKMGMT_RES:
                case ISCSIOP_SCSI_DATA_IN:
                case ISCSIOP_R2T:
                case ISCSIOP_ASYN_MSG:
                case ISCSIOP_TEXT_RES:
                case ISCSIOP_LOGIN_RES:
                case ISCSIOP_LOGOUT_RES:
                case ISCSIOP_REJECT:
                case ISCSIOP_NOP_IN:
                    if (serial_number_less(pImage->MaxCmdSN, RT_N2H_U32(pcvResSeg[8])))
                        pImage->MaxCmdSN = RT_N2H_U32(pcvResSeg[8]);
                    if (serial_number_less(pImage->ExpCmdSN, RT_N2H_U32(pcvResSeg[7])))
                        pImage->ExpCmdSN = RT_N2H_U32(pcvResSeg[7]);
                    break;
                default:
                    rc = VERR_PARSE_ERROR;
            }
            if (RT_FAILURE(rc))
                continue;
            if (    !pImage->FirstRecvPDU
                &&  (cmd != ISCSIOP_SCSI_DATA_IN || (RT_N2H_U32(pcvResSeg[0]) & ISCSI_STATUS_BIT)))
            {
                if (pImage->ExpStatSN == RT_N2H_U32(pcvResSeg[6]))
                {
                    /* StatSN counter is not advanced on R2T and on a target SN update NOP-In. */
                    if (    (cmd != ISCSIOP_R2T)
                        &&  ((cmd != ISCSIOP_NOP_IN) || (RT_N2H_U32(pcvResSeg[4]) != ISCSI_TASK_TAG_RSVD)))
                        pImage->ExpStatSN++;
                }
                else
                {
                    rc = VERR_PARSE_ERROR;
                    continue;
                }
            }
            /* Finally check whether the received PDU matches what the caller wants. */
            if (itt == pcvResSeg[4])
            {
                /* Copy received PDU (one segment) to caller-provided buffers. */
                uint32_t i;
                size_t cbSeg;
                const uint8_t *pSrc;

                pSrc = (const uint8_t *)aResBuf.pvSeg;
                cbSeg = aResBuf.cbSeg;
                for (i = 0; i < cnRes; i++)
                {
                    if (cbSeg > paRes[i].cbSeg)
                    {
                        memcpy(paRes[i].pvSeg, pSrc, paRes[i].cbSeg);
                        pSrc += paRes[i].cbSeg;
                        cbSeg -= paRes[i].cbSeg;
                    }
                    else
                    {
                        memcpy(paRes[i].pvSeg, pSrc, cbSeg);
                        paRes[i].cbSeg = cbSeg;
                        cbSeg = 0;
                        break;
                    }
                }
                if (cbSeg != 0)
                {
                    rc = VERR_BUFFER_OVERFLOW;
                    break;
                }
                for (i++; i < cnRes; i++)
                    paRes[i].cbSeg = 0;
                break;
            }
        }
    }
    return rc;
}


/**
 * Check the static (not dependent on the connection/session state) validity of an iSCSI response PDU.
 *
 * @returns VBOX status
 * @param   paRes      Pointer to array of iSCSI response sections.
 * @param   cnRes      Number of valid iSCSI response sections in the array.
 */
static int drvISCSIValidatePDU(PISCSIRES paRes, uint32_t cnRes)
{
    const uint32_t *pcrgResBHS;
    uint32_t hw0;
    Assert(cnRes >= 1);
    Assert(paRes[0].cbSeg >= ISCSI_BHS_SIZE);

    pcrgResBHS = (const uint32_t *)(paRes[0].pvSeg);
    hw0 = RT_N2H_U32(pcrgResBHS[0]);
    switch (hw0 & ISCSIOP_MASK)
    {
        case ISCSIOP_NOP_IN:
            /* NOP-In responses must not be split into several PDUs nor it may contain
             * ping data for target-initiated pings nor may both task tags be valid task tags. */
            if (    (hw0 & ISCSI_FINAL_BIT) == 0
                ||  (    RT_N2H_U32(pcrgResBHS[4]) == ISCSI_TASK_TAG_RSVD
                     &&  RT_N2H_U32(pcrgResBHS[1]) != 0)
                ||  (    RT_N2H_U32(pcrgResBHS[4]) != ISCSI_TASK_TAG_RSVD
                     &&  RT_N2H_U32(pcrgResBHS[5]) != ISCSI_TASK_TAG_RSVD))
                return VERR_PARSE_ERROR;
            break;
        case ISCSIOP_SCSI_RES:
            /* SCSI responses must not be split into several PDUs nor must the residual
             * bits be contradicting each other nor may the residual bits be set for PDUs
             * containing anything else but a completed command response. Underflow
             * is no reason for declaring a PDU as invalid, as the target may choose
             * to return less data than we assume to get. */
            if (    (hw0 & ISCSI_FINAL_BIT) == 0
                ||  ((hw0 & ISCSI_BI_READ_RESIDUAL_OVFL_BIT) && (hw0 & ISCSI_BI_READ_RESIDUAL_UNFL_BIT))
                ||  ((hw0 & ISCSI_RESIDUAL_OVFL_BIT) && (hw0 & ISCSI_RESIDUAL_UNFL_BIT))
                ||  (    ((hw0 & ISCSI_SCSI_RESPONSE_MASK) == 0)
                     &&  ((hw0 & ISCSI_SCSI_STATUS_MASK) == SCSI_STATUS_OK)
                     &&  (hw0 & (  ISCSI_BI_READ_RESIDUAL_OVFL_BIT | ISCSI_BI_READ_RESIDUAL_UNFL_BIT
                                 | ISCSI_RESIDUAL_OVFL_BIT))))
                return VERR_PARSE_ERROR;
            break;
        case ISCSIOP_LOGIN_RES:
            /* Login responses must not contain contradicting transit and continue bits. */
            if ((hw0 & ISCSI_CONTINUE_BIT) && ((hw0 & ISCSI_TRANSIT_BIT) != 0))
                return VERR_PARSE_ERROR;
            break;
        case ISCSIOP_TEXT_RES:
            /* Text responses must not contain contradicting final and continue bits nor
             * may the final bit be set for PDUs containing a target transfer tag other than
             * the reserved transfer tag (and vice versa). */
            if (    (((hw0 & ISCSI_CONTINUE_BIT) && (hw0 & ISCSI_FINAL_BIT) != 0))
                ||  (((hw0 & ISCSI_FINAL_BIT) && (RT_N2H_U32(pcrgResBHS[5]) != ISCSI_TASK_TAG_RSVD)))
                ||  (((hw0 & ISCSI_FINAL_BIT) == 0) && (RT_N2H_U32(pcrgResBHS[5]) == ISCSI_TASK_TAG_RSVD)))
                return VERR_PARSE_ERROR;
            break;
        case ISCSIOP_SCSI_DATA_IN:
            /* SCSI Data-in responses must not contain contradicting residual bits when
             * status bit is set. */
            if ((hw0 & ISCSI_STATUS_BIT) && (hw0 & ISCSI_RESIDUAL_OVFL_BIT) && (hw0 & ISCSI_RESIDUAL_UNFL_BIT))
                return VERR_PARSE_ERROR;
            break;
        case ISCSIOP_LOGOUT_RES:
            /* Logout responses must not have the final bit unset and may not contain any
             * data or additional header segments. */
            if (    ((hw0 & ISCSI_FINAL_BIT) == 0)
                ||  (RT_N2H_U32(pcrgResBHS[1]) != 0))
                return VERR_PARSE_ERROR;
            break;
        case ISCSIOP_ASYN_MSG:
            /* Asynchronous Messages must not have the final bit unser and may not contain
             * an initiator task tag. */
            if (    ((hw0 & ISCSI_FINAL_BIT) == 0)
                ||  (RT_N2H_U32(pcrgResBHS[4]) != ISCSI_TASK_TAG_RSVD))
                return VERR_PARSE_ERROR;
            break;
        case ISCSIOP_SCSI_TASKMGMT_RES:
        case ISCSIOP_R2T:
        case ISCSIOP_REJECT:
        default:
            /* Do some logging, ignore PDU. */
            LogFlow(("drvISCSIValidatePDU: ignore unhandled PDU, first word %#08x\n", RT_N2H_U32(pcrgResBHS[0])));
            return VERR_PARSE_ERROR;
    }
    /* A target must not send PDUs with MaxCmdSN less than ExpCmdSN-1. */

    if (serial_number_less(RT_N2H_U32(pcrgResBHS[8]), RT_N2H_U32(pcrgResBHS[7])-1))
        return VERR_PARSE_ERROR;

    return VINF_SUCCESS;
}


/**
 * Appends a key-value pair to the buffer. Normal ASCII strings (cbValue == 0) and large binary values
 * of a given length (cbValue > 0) are directly supported. Other value types must be converted to ASCII
 * by the caller. Strings must be in UTF-8 encoding.
 *
 * @returns VBOX status
 * @param   pbBuf       Pointer to the key-value buffer.
 * @param   cbBuf       Length of the key-value buffer.
 * @param   pcbBufCurr  Currently used portion of the key-value buffer.
 * @param   pszKey      Pointer to a string containing the key.
 * @param   pszValue    Pointer to either a string containing the value or to a large binary value.
 * @param   cbValue     Length of the binary value if applicable.
 */
static int iscsiTextAddKeyValue(uint8_t *pbBuf, size_t cbBuf, size_t *pcbBufCurr, const char *pcszKey,
                                   const char *pcszValue, size_t cbValue)
{
    size_t cbBufTmp = *pcbBufCurr;
    size_t cbKey = strlen(pcszKey);
    size_t cbValueEnc;
    uint8_t *pbCurr;

    if (cbValue == 0)
        cbValueEnc = strlen(pcszValue);
    else
        cbValueEnc = cbValue * 2 + 2;   /* 2 hex bytes per byte, 2 bytes prefix */

    if (cbBuf < cbBufTmp + cbKey + 1 + cbValueEnc + 1)
    {
        /* Buffer would overflow, signal error. */
        return VERR_BUFFER_OVERFLOW;
    }

    /*
     * Append a key=value pair (zero terminated string) to the end of the buffer.
     */
    pbCurr = pbBuf + cbBufTmp;
    memcpy(pbCurr, pcszKey, cbKey);
    pbCurr += cbKey;
    *pbCurr++ = '=';
    if (cbValue == 0)
    {
        memcpy(pbCurr, pcszValue, cbValueEnc);
        pbCurr += cbValueEnc;
    }
    else
    {
        *pbCurr++ = '0';
        *pbCurr++ = 'x';
        for (uint32_t i = 0; i < cbValue; i++)
        {
            uint8_t b;
            b = pcszValue[i];
            *pbCurr++ = NUM_2_HEX(b >> 4);
            *pbCurr++ = NUM_2_HEX(b & 0xf);
        }
    }
    *pbCurr = '\0';
    *pcbBufCurr = cbBufTmp + cbKey + 1 + cbValueEnc + 1;

    return VINF_SUCCESS;
}


/**
 * Retrieve the value for a given key from the key=value buffer.
 *
 * @returns VBOX status.
 * @param   pbBuf      Buffer containing key=value pairs.
 * @param   cbBuf      Length of buffer with key=value pairs.
 * @param   pszKey     Pointer to key for which to retrieve the value.
 * @param   ppszValue  Pointer to value string pointer.
 */
static int iscsiTextGetKeyValue(const uint8_t *pbBuf, size_t cbBuf, const char *pcszKey, const char **ppcszValue)
{
    size_t cbKey = strlen(pcszKey);

    while (cbBuf != 0)
    {
        size_t cbKeyValNull = strlen((const char *)pbBuf) + 1;

        if (strncmp(pcszKey, (const char *)pbBuf, cbKey) == 0 && pbBuf[cbKey] == '=')
        {
            *ppcszValue = (const char *)(pbBuf + cbKey + 1);
            return VINF_SUCCESS;
        }
        pbBuf += cbKeyValNull;
        cbBuf -= cbKeyValNull;
    }
    return VERR_INVALID_NAME;
}


/**
 * Convert a long-binary value from a value string to the binary representation.
 *
 * @returns VBOX status
 * @param   pszValue    Pointer to a string containing the textual value representation.
 * @param   pbValue     Pointer to the value buffer for the binary value.
 * @param   pcbValue    In: length of value buffer, out: actual length of binary value.
 */
static int iscsiStrToBinary(const char *pcszValue, uint8_t *pbValue, size_t *pcbValue)
{
    size_t cbValue = *pcbValue;
    char c1, c2, c3, c4;
    Assert(cbValue >= 1);

    if (strlen(pcszValue) < 3)
        return VERR_PARSE_ERROR;
    if (*pcszValue++ != '0')
        return VERR_PARSE_ERROR;
    switch (*pcszValue++)
    {
        case 'x':
        case 'X':
            if (strlen(pcszValue) & 1)
            {
                c1 = *pcszValue++;
                *pbValue++ = HEX_2_NUM(c1);
                cbValue--;
            }
            while (*pcszValue != '\0')
            {
                if (cbValue == 0)
                    return VERR_BUFFER_OVERFLOW;
                c1 = *pcszValue++;
                if ((c1 < '0' || c1 > '9') && (c1 < 'a' || c1 > 'f') && (c1 < 'A' || c1 > 'F'))
                    return VERR_PARSE_ERROR;
                c2 = *pcszValue++;
                if ((c2 < '0' || c2 > '9') && (c2 < 'a' || c2 > 'f') && (c2 < 'A' || c2 > 'F'))
                    return VERR_PARSE_ERROR;
                *pbValue++ = (HEX_2_NUM(c1) << 4) | HEX_2_NUM(c2);
                cbValue--;
            }
            *pcbValue -= cbValue;
            break;
        case 'b':
        case 'B':
            if ((strlen(pcszValue) & 3) != 0)
                return VERR_PARSE_ERROR;
            while (*pcszValue != '\0')
            {
                uint32_t temp;
                if (cbValue == 0)
                    return VERR_BUFFER_OVERFLOW;
                c1 = *pcszValue++;
                if ((c1 < 'A' || c1 > 'Z') && (c1 < 'a' || c1 >'z') && (c1 < '0' || c1 > '9') && (c1 != '+') && (c1 != '/'))
                    return VERR_PARSE_ERROR;
                c2 = *pcszValue++;
                if ((c2 < 'A' || c2 > 'Z') && (c2 < 'a' || c2 >'z') && (c2 < '0' || c2 > '9') && (c2 != '+') && (c2 != '/'))
                    return VERR_PARSE_ERROR;
                c3 = *pcszValue++;
                if ((c3 < 'A' || c3 > 'Z') && (c3 < 'a' || c3 >'z') && (c3 < '0' || c3 > '9') && (c3 != '+') && (c3 != '/') && (c3 != '='))
                    return VERR_PARSE_ERROR;
                c4 = *pcszValue++;
                if (    (c3 == '=' && c4 != '=')
                    ||  ((c4 < 'A' || c4 > 'Z') && (c4 < 'a' || c4 >'z') && (c4 < '0' || c4 > '9') && (c4 != '+') && (c4 != '/') && (c4 != '=')))
                    return VERR_PARSE_ERROR;
                temp = (B64_2_NUM(c1) << 18) | (B64_2_NUM(c2) << 12);
                if (c3 == '=') {
                    if (*pcszValue != '\0')
                        return VERR_PARSE_ERROR;
                    *pbValue++ = temp >> 16;
                    cbValue--;
                } else {
                    temp |= B64_2_NUM(c3) << 6;
                    if (c4 == '=') {
                        if (*pcszValue != '\0')
                            return VERR_PARSE_ERROR;
                        if (cbValue < 2)
                            return VERR_BUFFER_OVERFLOW;
                        *pbValue++ = temp >> 16;
                        *pbValue++ = (temp >> 8) & 0xff;
                        cbValue -= 2;
                    }
                    else
                    {
                        temp |= B64_2_NUM(c4);
                        if (cbValue < 3)
                            return VERR_BUFFER_OVERFLOW;
                        *pbValue++ = temp >> 16;
                        *pbValue++ = (temp >> 8) & 0xff;
                        *pbValue++ = temp & 0xff;
                        cbValue -= 3;
                    }
                }
            }
            *pcbValue -= cbValue;
            break;
        default:
            return VERR_PARSE_ERROR;
    }
    return VINF_SUCCESS;
}


static bool serial_number_less(uint32_t s1, uint32_t s2)
{
    return (s1 < s2 && s2 - s1 < 0x80000000) || (s1 > s2 && s1 - s2 > 0x80000000);
}


#ifdef IMPLEMENT_TARGET_AUTH
static void chap_md5_generate_challenge(uint8_t *pbChallenge, size_t *pcbChallenge)
{
    uint8_t cbChallenge;

    cbChallenge = RTrand_U8(CHAP_MD5_CHALLENGE_MIN, CHAP_MD5_CHALLENGE_MAX);
    RTrand_bytes(pbChallenge, cbChallenge);
    *pcbChallenge = cbChallenge;
}
#endif


static void chap_md5_compute_response(uint8_t *pbResponse, uint8_t id, const uint8_t *pbChallenge, size_t cbChallenge,
                                      const uint8_t *pbSecret, size_t cbSecret)
{
    RTMD5CONTEXT ctx;
    uint8_t bId;

    bId = id;
    RTMd5Init(&ctx);
    RTMd5Update(&ctx, &bId, 1);
    RTMd5Update(&ctx, pbSecret, cbSecret);
    RTMd5Update(&ctx, pbChallenge, cbChallenge);
    RTMd5Final(pbResponse, &ctx);
}

/**
 * Internal. Free all allocated space for representing an image, and optionally
 * delete the image from disk.
 */
static void iscsiFreeImage(PISCSIIMAGE pImage, bool fDelete)
{
    Assert(pImage);
    Assert(!fDelete); /* This MUST be false, the flag isn't supported. */

    if (pImage->Mutex != NIL_RTSEMMUTEX)
    {
        /* Detaching only makes sense when the mutex is there. Otherwise the
         * failure happened long before we could attach to the target. */
        iscsiDetach(pImage);
        RTSemMutexDestroy(pImage->Mutex);
        pImage->Mutex = NIL_RTSEMMUTEX;
    }
    if (pImage->pszTargetName)
    {
        RTMemFree(pImage->pszTargetName);
        pImage->pszTargetName = NULL;
    }
    if (pImage->pszInitiatorName)
    {
        RTMemFree(pImage->pszInitiatorName);
        pImage->pszInitiatorName = NULL;
    }
    if (pImage->pszInitiatorUsername)
    {
        RTMemFree(pImage->pszInitiatorUsername);
        pImage->pszInitiatorUsername = NULL;
    }
    if (pImage->pbInitiatorSecret)
    {
        RTMemFree(pImage->pbInitiatorSecret);
        pImage->pbInitiatorSecret = NULL;
    }
    if (pImage->pszTargetUsername)
    {
        RTMemFree(pImage->pszTargetUsername);
        pImage->pszTargetUsername = NULL;
    }
    if (pImage->pbTargetSecret)
    {
        RTMemFree(pImage->pbTargetSecret);
        pImage->pbTargetSecret = NULL;
    }
    if (pImage->pvRecvPDUBuf)
    {
        RTMemFree(pImage->pvRecvPDUBuf);
        pImage->pvRecvPDUBuf = NULL;
    }
}

/**
 * Internal: Open an image, constructing all necessary data structures.
 */
static int iscsiOpenImage(PISCSIIMAGE pImage, unsigned uOpenFlags)
{
    int rc;
    char *pszLUN = NULL, *pszLUNInitial = NULL;
    bool fLunEncoded = false;
    uint32_t uTimeoutDef = 0;
    uint64_t uHostIPTmp = 0;
    bool fHostIPDef = 0;
    rc = RTStrToUInt32Full(s_iscsiConfigDefaultTimeout, 0, &uTimeoutDef);
    AssertRC(rc);
    rc = RTStrToUInt64Full(s_iscsiConfigDefaultHostIPStack, 0, &uHostIPTmp);
    AssertRC(rc);
    fHostIPDef = !!uHostIPTmp;

    pImage->uOpenFlags      = uOpenFlags;

    /* Get error signalling interface. */
    pImage->pInterfaceError = VDInterfaceGet(pImage->pVDIfsDisk, VDINTERFACETYPE_ERROR);
    if (pImage->pInterfaceError)
        pImage->pInterfaceErrorCallbacks = VDGetInterfaceError(pImage->pInterfaceError);

    /* Get TCP network stack interface. */
    pImage->pInterfaceNet = VDInterfaceGet(pImage->pVDIfsDisk, VDINTERFACETYPE_TCPNET);
    if (pImage->pInterfaceNet)
        pImage->pInterfaceNetCallbacks = VDGetInterfaceTcpNet(pImage->pInterfaceNet);
    else
    {
        rc = iscsiError(pImage, VERR_VD_ISCSI_UNKNOWN_INTERFACE,
                        RT_SRC_POS, N_("iSCSI: TCP network stack interface missing"));
        goto out;
    }

    /* Get configuration interface. */
    pImage->pInterfaceConfig = VDInterfaceGet(pImage->pVDIfsImage, VDINTERFACETYPE_CONFIG);
    if (pImage->pInterfaceConfig)
        pImage->pInterfaceConfigCallbacks = VDGetInterfaceConfig(pImage->pInterfaceConfig);
    else
    {
        rc = iscsiError(pImage, VERR_VD_ISCSI_UNKNOWN_INTERFACE,
                        RT_SRC_POS, N_("iSCSI: configuration interface missing"));
        goto out;
    }

    pImage->ISID            = 0x800000000000ULL | 0x001234560000ULL | (0x00000000cba0ULL + ASMAtomicIncU32(&s_u32iscsiID));
    pImage->cISCSIRetries   = 10;
    pImage->state           = ISCSISTATE_FREE;
    pImage->pvRecvPDUBuf    = RTMemAlloc(ISCSI_RECV_PDU_BUFFER_SIZE);
    pImage->cbRecvPDUBuf    = ISCSI_RECV_PDU_BUFFER_SIZE;
    if (pImage->pvRecvPDUBuf == NULL)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pImage->Mutex           = NIL_RTSEMMUTEX;
    rc = RTSemMutexCreate(&pImage->Mutex);
    if (RT_FAILURE(rc))
        goto out;

    /* Validate configuration, detect unknown keys. */
    if (!VDCFGAreKeysValid(pImage->pInterfaceConfigCallbacks,
                           pImage->pInterfaceConfig->pvUser,
                           "TargetName\0InitiatorName\0LUN\0TargetAddress\0InitiatorUsername\0InitiatorSecret\0TargetUsername\0TargetSecret\0Timeout\0HostIPStack\0"))
    {
        rc = iscsiError(pImage, VERR_VD_ISCSI_UNKNOWN_CFG_VALUES, RT_SRC_POS, N_("iSCSI: configuration error: unknown configuration keys present"));
        goto out;
    }

    /* Query the iSCSI upper level configuration. */
    rc = VDCFGQueryStringAlloc(pImage->pInterfaceConfigCallbacks,
                               pImage->pInterfaceConfig->pvUser,
                               "TargetName", &pImage->pszTargetName);
    if (RT_FAILURE(rc))
    {
        rc = iscsiError(pImage, rc, RT_SRC_POS, N_("iSCSI: configuration error: failed to read TargetName as string"));
        goto out;
    }
    rc = VDCFGQueryStringAllocDef(pImage->pInterfaceConfigCallbacks,
                                  pImage->pInterfaceConfig->pvUser,
                                  "InitiatorName", &pImage->pszInitiatorName,
                                  s_iscsiConfigDefaultInitiatorName);
    if (RT_FAILURE(rc))
    {
        rc = iscsiError(pImage, rc, RT_SRC_POS, N_("iSCSI: configuration error: failed to read InitiatorName as string"));
        goto out;
    }
    rc = VDCFGQueryStringAllocDef(pImage->pInterfaceConfigCallbacks,
                                  pImage->pInterfaceConfig->pvUser,
                                  "LUN", &pszLUN, s_iscsiConfigDefaultLUN);
    if (RT_FAILURE(rc))
    {
        rc = iscsiError(pImage, rc, RT_SRC_POS, N_("iSCSI: configuration error: failed to read LUN as string"));
        goto out;
    }
    pszLUNInitial = pszLUN;
    if (!strncmp(pszLUN, "enc", 3))
    {
        fLunEncoded = true;
        pszLUN += 3;
    }
    rc = RTStrToUInt64Full(pszLUN, 0, &pImage->LUN);
    if (RT_FAILURE(rc))
    {
        rc = iscsiError(pImage, rc, RT_SRC_POS, N_("iSCSI: configuration error: failed to convert LUN to integer"));
        goto out;
    }
    if (!fLunEncoded)
    {
        if (pImage->LUN <= 255)
        {
            pImage->LUN = pImage->LUN << 48; /* uses peripheral device addressing method */
        }
        else if (pImage->LUN <= 16383)
        {
            pImage->LUN = (pImage->LUN << 48) | RT_BIT_64(62); /* uses flat space addressing method */
        }
        else
        {
            rc = VERR_OUT_OF_RANGE;
            rc = iscsiError(pImage, rc, RT_SRC_POS, N_("iSCSI: configuration error: LUN number out of range (0-16383)"));
            goto out;
        }
    }
    rc = VDCFGQueryStringAlloc(pImage->pInterfaceConfigCallbacks,
                               pImage->pInterfaceConfig->pvUser,
                               "TargetAddress", &pImage->pszTargetAddress);
    if (RT_FAILURE(rc))
    {
        rc = iscsiError(pImage, rc, RT_SRC_POS, N_("iSCSI: configuration error: failed to read TargetAddress as string"));
        goto out;
    }
    pImage->pszInitiatorUsername = NULL;
    rc = VDCFGQueryStringAlloc(pImage->pInterfaceConfigCallbacks,
                               pImage->pInterfaceConfig->pvUser,
                               "InitiatorUsername",
                               &pImage->pszInitiatorUsername);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
        rc = VINF_SUCCESS;
    if (RT_FAILURE(rc))
    {
        rc = iscsiError(pImage, rc, RT_SRC_POS, N_("iSCSI: configuration error: failed to read InitiatorUsername as string"));
        goto out;
    }
    pImage->pbInitiatorSecret = NULL;
    pImage->cbInitiatorSecret = 0;
    rc = VDCFGQueryBytesAlloc(pImage->pInterfaceConfigCallbacks,
                              pImage->pInterfaceConfig->pvUser,
                              "InitiatorSecret",
                              (void **)&pImage->pbInitiatorSecret,
                              &pImage->cbInitiatorSecret);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
        rc = VINF_SUCCESS;
    if (RT_FAILURE(rc))
    {
        rc = iscsiError(pImage, rc, RT_SRC_POS, N_("iSCSI: configuration error: failed to read InitiatorSecret as byte string"));
        goto out;
    }
    pImage->pszTargetUsername = NULL;
    rc = VDCFGQueryStringAlloc(pImage->pInterfaceConfigCallbacks,
                               pImage->pInterfaceConfig->pvUser,
                               "TargetUsername",
                               &pImage->pszTargetUsername);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
        rc = VINF_SUCCESS;
    if (RT_FAILURE(rc))
    {
        rc = iscsiError(pImage, rc, RT_SRC_POS, N_("iSCSI: configuration error: failed to read TargetUsername as string"));
        goto out;
    }
    pImage->pbTargetSecret = NULL;
    pImage->cbTargetSecret = 0;
    rc = VDCFGQueryBytesAlloc(pImage->pInterfaceConfigCallbacks,
                              pImage->pInterfaceConfig->pvUser,
                              "TargetSecret", (void **)&pImage->pbTargetSecret,
                              &pImage->cbTargetSecret);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
        rc = VINF_SUCCESS;
    if (RT_FAILURE(rc))
    {
        rc = iscsiError(pImage, rc, RT_SRC_POS, N_("iSCSI: configuration error: failed to read TargetSecret as byte string"));
        goto out;
    }

    pImage->pszHostname    = NULL;
    pImage->uPort          = 0;
    pImage->Socket         = NIL_RTSOCKET;
    /* Query the iSCSI lower level configuration. */
    rc = VDCFGQueryU32Def(pImage->pInterfaceConfigCallbacks,
                          pImage->pInterfaceConfig->pvUser,
                          "Timeout", &pImage->uReadTimeout,
                          uTimeoutDef);
    if (RT_FAILURE(rc))
    {
        rc = iscsiError(pImage, rc, RT_SRC_POS, N_("iSCSI: configuration error: failed to read Timeout as U32"));
        goto out;
    }
    rc = VDCFGQueryBoolDef(pImage->pInterfaceConfigCallbacks,
                           pImage->pInterfaceConfig->pvUser,
                           "HostIPStack", &pImage->fHostIP,
                           fHostIPDef);
    if (RT_FAILURE(rc))
    {
        rc = iscsiError(pImage, rc, RT_SRC_POS, N_("iSCSI: configuration error: failed to read HostIPStack as boolean"));
        goto out;
    }

    /* Don't actually establish iSCSI transport connection if this is just an
     * open to query the image information and the host IP stack isn't used.
     * Even trying is rather useless, as in this context the InTnet IP stack
     * isn't present. Returning dummies is the best possible result anyway. */
    if ((uOpenFlags & VD_OPEN_FLAGS_INFO) && !pImage->fHostIP)
    {
        LogFunc(("Not opening the transport connection as IntNet IP stack is not available. Will return dummies\n"));
        goto out;
    }

    /*
     * Establish the iSCSI transport connection.
     */
    rc = iscsiTransportOpen(pImage);
    if (RT_SUCCESS(rc))
        rc = iscsiAttach(pImage);

    if (RT_FAILURE(rc))
    {
        LogRel(("iSCSI: could not open target %s, rc=%Rrc\n", pImage->pszTargetName, rc));
        goto out;
    }
    LogFlowFunc(("target '%s' opened successfully\n", pImage->pszTargetName));

    SCSIREQ sr;
    uint8_t sense[32];
    uint8_t data8[8];

    /*
     * Inquire available LUNs - purely dummy request.
     */
    uint8_t cdb_rlun[12];
    uint8_t rlundata[16];
    cdb_rlun[0] = SCSI_REPORT_LUNS;
    cdb_rlun[1] = 0;        /* reserved */
    cdb_rlun[2] = 0;        /* reserved */
    cdb_rlun[3] = 0;        /* reserved */
    cdb_rlun[4] = 0;        /* reserved */
    cdb_rlun[5] = 0;        /* reserved */
    cdb_rlun[6] = sizeof(rlundata) >> 24;
    cdb_rlun[7] = (sizeof(rlundata) >> 16) & 0xff;
    cdb_rlun[8] = (sizeof(rlundata) >> 8) & 0xff;
    cdb_rlun[9] = sizeof(rlundata) & 0xff;
    cdb_rlun[10] = 0;       /* reserved */
    cdb_rlun[11] = 0;       /* control */

    sr.enmXfer = SCSIXFER_FROM_TARGET;
    sr.cbCmd = sizeof(cdb_rlun);
    sr.pvCmd = cdb_rlun;
    sr.cbI2TData = 0;
    sr.pcvI2TData = NULL;
    sr.cbT2IData = sizeof(rlundata);
    sr.pvT2IData = rlundata;
    sr.cbSense = sizeof(sense);
    sr.pvSense = sense;

    rc = iscsiCommand(pImage, &sr);
    if (RT_FAILURE(rc))
    {
        LogRel(("iSCSI: Could not get LUN info for target %s, rc=%Rrc\n", pImage->pszTargetName, rc));
        return rc;
    }

    /*
     * Inquire device characteristics - no tapes, scanners etc., please.
     */
    uint8_t cdb_inq[6];
    cdb_inq[0] = SCSI_INQUIRY;
    cdb_inq[1] = 0;         /* reserved */
    cdb_inq[2] = 0;         /* reserved */
    cdb_inq[3] = 0;         /* reserved */
    cdb_inq[4] = sizeof(data8);
    cdb_inq[5] = 0;         /* control */

    sr.enmXfer = SCSIXFER_FROM_TARGET;
    sr.cbCmd = sizeof(cdb_inq);
    sr.pvCmd = cdb_inq;
    sr.cbI2TData = 0;
    sr.pcvI2TData = NULL;
    sr.cbT2IData = sizeof(data8);
    sr.pvT2IData = data8;
    sr.cbSense = sizeof(sense);
    sr.pvSense = sense;

    rc = iscsiCommand(pImage, &sr);
    if (RT_SUCCESS(rc))
    {
        if ((data8[0] & SCSI_DEVTYPE_MASK) != SCSI_DEVTYPE_DISK)
        {
            rc = iscsiError(pImage, VERR_VD_ISCSI_INVALID_TYPE,
                            RT_SRC_POS, N_("iSCSI: target address %s, target name %s, SCSI LUN %lld reports device type=%u"),
                            pImage->pszTargetAddress, pImage->pszTargetName,
                            pImage->LUN, data8[0]);
            LogRel(("iSCSI: Unsupported SCSI peripheral device type %d for target %s\n", data8[0] & SCSI_DEVTYPE_MASK, pImage->pszTargetName));
            goto out;
        }
    }
    else
    {
        LogRel(("iSCSI: Could not get INQUIRY info for target %s, rc=%Rrc\n", pImage->pszTargetName, rc));
        goto out;
    }

    /*
     * Query write disable bit in the device specific parameter entry in the
     * mode parameter header. Refuse read/write opening of read only disks.
     */

    uint8_t cdb_ms[6];
    uint8_t data4[4];
    cdb_ms[0] = SCSI_MODE_SENSE_6;
    cdb_ms[1] = 0;          /* dbd=0/reserved */
    cdb_ms[2] = 0x3f;       /* pc=0/page code=0x3f, ask for all pages */
    cdb_ms[3] = 0;          /* subpage code=0, return everything in page_0 format */
    cdb_ms[4] = sizeof(data4); /* allocation length=4 */
    cdb_ms[5] = 0;          /* control */

    sr.enmXfer = SCSIXFER_FROM_TARGET;
    sr.cbCmd = sizeof(cdb_ms);
    sr.pvCmd = cdb_ms;
    sr.cbI2TData = 0;
    sr.pcvI2TData = NULL;
    sr.cbT2IData = sizeof(data4);
    sr.pvT2IData = data4;
    sr.cbSense = sizeof(sense);
    sr.pvSense = sense;

    rc = iscsiCommand(pImage, &sr);
    if (RT_SUCCESS(rc))
    {
        if (!(uOpenFlags & VD_OPEN_FLAGS_READONLY) && data4[2] & 0x80)
        {
            rc = VERR_VD_IMAGE_READ_ONLY;
            goto out;
        }
   }
    else
    {
        LogRel(("iSCSI: Could not get MODE SENSE info for target %s, rc=%Rrc\n", pImage->pszTargetName, rc));
        goto out;
    }

    /*
     * Determine sector size and capacity of the volume immediately.
     */
    uint8_t cdb_cap[10];

    cdb_cap[0] = SCSI_READ_CAPACITY;
    cdb_cap[1] = 0;         /* reserved */
    cdb_cap[2] = 0;         /* reserved */
    cdb_cap[3] = 0;         /* reserved */
    cdb_cap[4] = 0;         /* reserved */
    cdb_cap[5] = 0;         /* reserved */
    cdb_cap[6] = 0;         /* reserved */
    cdb_cap[7] = 0;         /* reserved */
    cdb_cap[8] = 0;         /* reserved */
    cdb_cap[9] = 0;         /* control */

    sr.enmXfer = SCSIXFER_FROM_TARGET;
    sr.cbCmd = sizeof(cdb_cap);
    sr.pvCmd = cdb_cap;
    sr.cbI2TData = 0;
    sr.pcvI2TData = NULL;
    sr.cbT2IData = sizeof(data8);
    sr.pvT2IData = data8;
    sr.cbSense = sizeof(sense);
    sr.pvSense = sense;

    rc = iscsiCommand(pImage, &sr);
    if (RT_SUCCESS(rc))
    {
        pImage->cVolume = (data8[0] << 24) | (data8[1] << 16) | (data8[2] << 8) | data8[3];
        pImage->cVolume++;
        pImage->cbSector = (data8[4] << 24) | (data8[5] << 16) | (data8[6] << 8) | data8[7];
        pImage->cbSize = (uint64_t)(pImage->cVolume) * pImage->cbSector;
        if (pImage->cVolume == 0 || pImage->cbSector == 0)
        {
            rc = iscsiError(pImage, VERR_VD_ISCSI_INVALID_TYPE,
                            RT_SRC_POS, N_("iSCSI: target address %s, target name %s, SCSI LUN %lld reports media sector count=%lu sector size=%lu"),
                            pImage->pszTargetAddress, pImage->pszTargetName,
                            pImage->LUN, pImage->cVolume, pImage->cbSector);
        }
    }
    else
    {
        LogRel(("iSCSI: Could not determine capacity of target %s, rc=%Rrc\n", pImage->pszTargetName, rc));
        goto out;
    }

    /*
     * Check the read and write cache bits.
     * Try to enable the cache if it is disabled.
     *
     * We already checked that this is a block access device. No need
     * to do it again.
     */
    uint8_t aCachingModePage[32];
    uint8_t aCDBModeSense6[6];

    memset(aCachingModePage, '\0', sizeof(aCachingModePage));
    aCDBModeSense6[0] = SCSI_MODE_SENSE_6;
    aCDBModeSense6[1] = 0;
    aCDBModeSense6[2] = (0x00 << 6) | (0x08 & 0x3f); /* Current values and caching mode page */
    aCDBModeSense6[3] = 0; /* Sub page code. */
    aCDBModeSense6[4] = sizeof(aCachingModePage) & 0xff;
    aCDBModeSense6[5] = 0;
    sr.enmXfer = SCSIXFER_FROM_TARGET;
    sr.cbCmd = sizeof(aCDBModeSense6);
    sr.pvCmd = aCDBModeSense6;
    sr.cbI2TData = 0;
    sr.pcvI2TData = NULL;
    sr.cbT2IData = sizeof(aCachingModePage);
    sr.pvT2IData = aCachingModePage;
    sr.cbSense = sizeof(sense);
    sr.pvSense = sense;
    rc = iscsiCommand(pImage, &sr);
    if (   RT_SUCCESS(rc)
        && (sr.status == SCSI_STATUS_OK)
        && (aCachingModePage[0] >= 15)
        && (aCachingModePage[4 + aCachingModePage[3]] & 0x3f) == 0x08
        && (aCachingModePage[4 + aCachingModePage[3] + 1] > 3))
    {
        uint32_t Offset = 4 + aCachingModePage[3];
        /*
         * Check if the read and/or the write cache is disabled.
         * The write cache is disabled if bit 2 (WCE) is zero and
         * the read cache is disabled if bit 0 (RCD) is set.
         */
        if (!ASMBitTest(&aCachingModePage[Offset + 2], 2) || ASMBitTest(&aCachingModePage[Offset + 2], 0))
        {
            /*
             * Write Cache Enable (WCE) bit is zero or the Read Cache Disable (RCD) is one
             * So one of the caches is disabled. Enable both caches.
             * The rest is unchanged.
             */
            ASMBitSet(&aCachingModePage[Offset + 2], 2);
            ASMBitClear(&aCachingModePage[Offset + 2], 0);

            uint8_t aCDBCaching[6];
            aCDBCaching[0] = SCSI_MODE_SELECT_6;
            aCDBCaching[1] = 0; /* Don't write the page into NV RAM. */
            aCDBCaching[2] = 0;
            aCDBCaching[3] = 0;
            aCDBCaching[4] = sizeof(aCachingModePage) & 0xff;
            aCDBCaching[5] = 0;
            sr.enmXfer = SCSIXFER_TO_TARGET;
            sr.cbCmd = sizeof(aCDBCaching);
            sr.pvCmd = aCDBCaching;
            sr.cbI2TData = sizeof(aCachingModePage);
            sr.pcvI2TData = aCachingModePage;
            sr.cbT2IData = 0;
            sr.pvT2IData = NULL;
            sr.cbSense = sizeof(sense);
            sr.pvSense = sense;
            sr.status  = 0;
            rc = iscsiCommand(pImage, &sr);
            if (   RT_SUCCESS(rc)
                && (sr.status == SCSI_STATUS_OK))
            {
                LogRel(("iSCSI: Enabled read and write cache of target %s\n", pImage->pszTargetName));
            }
            else
            {
                /* Log failures but continue. */
                LogRel(("iSCSI: Could not enable read and write cache of target %s, rc=%Rrc status=%#x\n",
                        pImage->pszTargetName, rc, sr.status));
                LogRel(("iSCSI: Sense:\n%.*Rhxd\n", sr.cbSense, sense));
                rc = VINF_SUCCESS;
            }
        }
    }
    else
    {
        /* Log errors but continue. */
        LogRel(("iSCSI: Could not check write cache of target %s, rc=%Rrc, got mode page %#x\n", pImage->pszTargetName, rc,aCachingModePage[0] & 0x3f));
        LogRel(("iSCSI: Sense:\n%.*Rhxd\n", sr.cbSense, sense));
        rc = VINF_SUCCESS;
    }


out:
    if (RT_FAILURE(rc))
        iscsiFreeImage(pImage, false);
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnCheckIfValid */
static int iscsiCheckIfValid(const char *pszFilename)
{
    LogFlowFunc(("pszFilename=\"%s\"\n", pszFilename));

    /* iSCSI images can't be checked for validity this way, as the filename
     * just can't supply enough configuration information. */
    int rc = VERR_VD_ISCSI_INVALID_HEADER;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnOpen */
static int iscsiOpen(const char *pszFilename, unsigned uOpenFlags,
                     PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                     void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" uOpenFlags=%#x pVDIfsDisk=%#p pVDIfsImage=%#p ppBackendData=%#p\n", pszFilename, uOpenFlags, pVDIfsDisk, pVDIfsImage, ppBackendData));
    int rc;
    PISCSIIMAGE pImage;

    /* Check open flags. All valid flags are supported. */
    if (uOpenFlags & ~VD_OPEN_FLAGS_MASK)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Check remaining arguments. */
    if (   !VALID_PTR(pszFilename)
        || !*pszFilename
        || strchr(pszFilename, '"'))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    pImage = (PISCSIIMAGE)RTMemAllocZ(sizeof(ISCSIIMAGE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }

    pImage->pszFilename = pszFilename;
    pImage->pszInitiatorName = NULL;
    pImage->pszTargetName = NULL;
    pImage->pszTargetAddress = NULL;
    pImage->pszInitiatorUsername = NULL;
    pImage->pbInitiatorSecret = NULL;
    pImage->pszTargetUsername = NULL;
    pImage->pbTargetSecret = NULL;
    pImage->paCurrReq = NULL;
    pImage->pvRecvPDUBuf = NULL;
    pImage->pszHostname = NULL;
    pImage->pVDIfsDisk = pVDIfsDisk;
    pImage->pVDIfsImage = pVDIfsImage;

    rc = iscsiOpenImage(pImage, uOpenFlags);
    if (RT_SUCCESS(rc))
        *ppBackendData = pImage;

out:
    if (RT_SUCCESS(rc))
    {
        LogFlowFunc(("target %s cVolume %d, cbSector %d\n", pImage->pszTargetName, pImage->cVolume, pImage->cbSector));
        LogRel(("iSCSI: target address %s, target name %s, SCSI LUN %lld\n", pImage->pszTargetAddress, pImage->pszTargetName, pImage->LUN));
    }
    LogFlowFunc(("returns %Rrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnCreate */
static int iscsiCreate(const char *pszFilename, uint64_t cbSize,
                       unsigned uImageFlags, const char *pszComment,
                       PCPDMMEDIAGEOMETRY pPCHSGeometry,
                       PCPDMMEDIAGEOMETRY pLCHSGeometry, PCRTUUID pUuid,
                       unsigned uOpenFlags, unsigned uPercentStart,
                       unsigned uPercentSpan, PVDINTERFACE pVDIfsDisk,
                       PVDINTERFACE pVDIfsImage, PVDINTERFACE pVDIfsOperation,
                       void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" cbSize=%llu uImageFlags=%#x pszComment=\"%s\" pPCHSGeometry=%#p pLCHSGeometry=%#p Uuid=%RTuuid uOpenFlags=%#x uPercentStart=%u uPercentSpan=%u pVDIfsDisk=%#p pVDIfsImage=%#p pVDIfsOperation=%#p ppBackendData=%#p", pszFilename, cbSize, uImageFlags, pszComment, pPCHSGeometry, pLCHSGeometry, pUuid, uOpenFlags, uPercentStart, uPercentSpan, pVDIfsDisk, pVDIfsImage, pVDIfsOperation, ppBackendData));
    int rc = VERR_NOT_SUPPORTED;

    LogFlowFunc(("returns %Rrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnRename */
static int iscsiRename(void *pBackendData, const char *pszFilename)
{
    LogFlowFunc(("pBackendData=%#p pszFilename=%#p\n", pBackendData, pszFilename));
    int rc = VERR_NOT_SUPPORTED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnClose */
static int iscsiClose(void *pBackendData, bool fDelete)
{
    LogFlowFunc(("pBackendData=%#p fDelete=%d\n", pBackendData, fDelete));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    Assert(!fDelete);   /* This flag is unsupported. */

    /* Freeing a never allocated image (e.g. because the open failed) is
     * not signalled as an error. After all nothing bad happens. */
    if (pImage)
        iscsiFreeImage(pImage, fDelete);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnRead */
static int iscsiRead(void *pBackendData, uint64_t uOffset, void *pvBuf,
                     size_t cbToRead, size_t *pcbActuallyRead)
{
    /** @todo reinstate logging of the target everywhere - dropped temporarily */
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToRead=%zu pcbActuallyRead=%#p\n", pBackendData, uOffset, pvBuf, cbToRead, pcbActuallyRead));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    uint64_t lba;
    uint16_t tls;
    int rc;

    Assert(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToRead % 512 == 0);

    Assert(pImage->cbSector);
    AssertPtr(pvBuf);

    if (   uOffset + cbToRead > pImage->cbSize
        || cbToRead == 0)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    lba = uOffset / pImage->cbSector;
    tls = (uint16_t)(cbToRead / pImage->cbSector);
    SCSIREQ sr;
    uint8_t cdb[10];
    uint8_t sense[32];

    cdb[0] = SCSI_READ_10;
    cdb[1] = 0;         /* reserved */
    cdb[2] = (lba >> 24) & 0xff;
    cdb[3] = (lba >> 16) & 0xff;
    cdb[4] = (lba >> 8) & 0xff;
    cdb[5] = lba & 0xff;
    cdb[6] = 0;         /* reserved */
    cdb[7] = (tls >> 8) & 0xff;
    cdb[8] = tls & 0xff;
    cdb[9] = 0;         /* control */

    sr.enmXfer = SCSIXFER_FROM_TARGET;
    sr.cbCmd = sizeof(cdb);
    sr.pvCmd = cdb;
    sr.cbI2TData = 0;
    sr.pcvI2TData = NULL;
    sr.cbT2IData = cbToRead;
    sr.pvT2IData = pvBuf;
    sr.cbSense = sizeof(sense);
    sr.pvSense = sense;

    rc = iscsiCommand(pImage, &sr);
    if (RT_FAILURE(rc))
        AssertMsgFailed(("iscsiCommand(%s, %#llx) -> %Rrc\n", pImage->pszTargetName, uOffset, rc));

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnWrite */
static int iscsiWrite(void *pBackendData, uint64_t uOffset, const void *pvBuf,
                     size_t cbToWrite, size_t *pcbWriteProcess,
                     size_t *pcbPreRead, size_t *pcbPostRead, unsigned fWrite)
{
     LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToWrite=%zu pcbWriteProcess=%#p pcbPreRead=%#p pcbPostRead=%#p\n", pBackendData, uOffset, pvBuf, cbToWrite, pcbWriteProcess, pcbPreRead, pcbPostRead));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    uint64_t lba;
    uint16_t tls;
    int rc;

    Assert(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToWrite % 512 == 0);

    Assert(pImage->cbSector);
    Assert(pvBuf);

    if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
    {
        rc = VERR_VD_IMAGE_READ_ONLY;
        goto out;
    }

    *pcbPreRead = 0;
    *pcbPostRead = 0;

    lba = uOffset / pImage->cbSector;
    tls = (uint16_t)(cbToWrite / pImage->cbSector);
    SCSIREQ sr;
    uint8_t cdb[10];
    uint8_t sense[32];

    cdb[0] = SCSI_WRITE_10;
    cdb[1] = 0;         /* reserved */
    cdb[2] = (lba >> 24) & 0xff;
    cdb[3] = (lba >> 16) & 0xff;
    cdb[4] = (lba >> 8) & 0xff;
    cdb[5] = lba & 0xff;
    cdb[6] = 0;         /* reserved */
    cdb[7] = (tls >> 8) & 0xff;
    cdb[8] = tls & 0xff;
    cdb[9] = 0;         /* control */

    sr.enmXfer = SCSIXFER_TO_TARGET;
    sr.cbCmd = sizeof(cdb);
    sr.pvCmd = cdb;
    sr.cbI2TData = cbToWrite;
    sr.pcvI2TData = pvBuf;
    sr.cbT2IData = 0;
    sr.pvT2IData = NULL;
    sr.cbSense = sizeof(sense);
    sr.pvSense = sense;

    rc = iscsiCommand(pImage, &sr);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("iscsiCommand(%s, %#llx) -> %Rrc\n", pImage->pszTargetName, uOffset, rc));
        *pcbWriteProcess = 0;
    }
    else
        *pcbWriteProcess = cbToWrite;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnFlush */
static int iscsiFlush(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    SCSIREQ sr;
    uint8_t cdb[10];
    uint8_t sense[32];

    cdb[0] = SCSI_SYNCHRONIZE_CACHE;
    cdb[1] = 0;         /* reserved */
    cdb[2] = 0;         /* LBA 0 */
    cdb[3] = 0;         /* LBA 0 */
    cdb[4] = 0;         /* LBA 0 */
    cdb[5] = 0;         /* LBA 0 */
    cdb[6] = 0;         /* reserved */
    cdb[7] = 0;         /* transfer everything to disk */
    cdb[8] = 0;         /* transfer everything to disk */
    cdb[9] = 0;         /* control */

    sr.enmXfer = SCSIXFER_TO_TARGET;
    sr.cbCmd = sizeof(cdb);
    sr.pvCmd = cdb;
    sr.cbI2TData = 0;
    sr.pcvI2TData = NULL;
    sr.cbT2IData = 0;
    sr.pvT2IData = NULL;
    sr.cbSense = sizeof(sense);
    sr.pvSense = sense;

    rc = iscsiCommand(pImage, &sr);
    if (RT_FAILURE(rc))
        AssertMsgFailed(("iscsiCommand(%s) -> %Rrc\n", pImage->pszTargetName, rc));
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnGetVersion */
static unsigned iscsiGetVersion(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;

    Assert(pImage);
    NOREF(pImage);

    return 0;
}


/** @copydoc VBOXHDDBACKEND::pfnGetSize */
static uint64_t iscsiGetSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;

    Assert(pImage);

    if (pImage)
        return pImage->cbSize;
    else
        return 0;
}


/** @copydoc VBOXHDDBACKEND::pfnGetFileSize */
static uint64_t iscsiGetFileSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;

    Assert(pImage);
    NOREF(pImage);

    if (pImage)
        return pImage->cbSize;
    else
        return 0;
}


/** @copydoc VBOXHDDBACKEND::pfnGetPCHSGeometry */
static int iscsiGetPCHSGeometry(void *pBackendData,
                                PPDMMEDIAGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p\n", pBackendData, pPCHSGeometry));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_VD_GEOMETRY_NOT_SET;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (PCHS=%u/%u/%u)\n", rc, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnSetPCHSGeometry */
static int iscsiSetPCHSGeometry(void *pBackendData,
                                PCPDMMEDIAGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p PCHS=%u/%u/%u\n", pBackendData, pPCHSGeometry, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            rc = VERR_VD_IMAGE_READ_ONLY;
            goto out;
        }
        rc = VERR_VD_GEOMETRY_NOT_SET;
    }
    else
        rc = VERR_VD_NOT_OPENED;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnGetLCHSGeometry */
static int iscsiGetLCHSGeometry(void *pBackendData,
                                PPDMMEDIAGEOMETRY pLCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p\n", pBackendData, pLCHSGeometry));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_VD_GEOMETRY_NOT_SET;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (LCHS=%u/%u/%u)\n", rc, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnGetImageFlags */
static unsigned iscsiGetImageFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    unsigned uImageFlags;

    Assert(pImage);
    NOREF(pImage);

    uImageFlags = VD_IMAGE_FLAGS_FIXED;

    LogFlowFunc(("returns %#x\n", uImageFlags));
    return uImageFlags;
}


/** @copydoc VBOXHDDBACKEND::pfnGetOpenFlags */
static unsigned iscsiGetOpenFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    unsigned uOpenFlags;

    Assert(pImage);

    if (pImage)
        uOpenFlags = pImage->uOpenFlags;
    else
        uOpenFlags = 0;

    LogFlowFunc(("returns %#x\n", uOpenFlags));
    return uOpenFlags;
}


/** @copydoc VBOXHDDBACKEND::pfnSetOpenFlags */
static int iscsiSetOpenFlags(void *pBackendData, unsigned uOpenFlags)
{
    LogFlowFunc(("pBackendData=%#p\n uOpenFlags=%#x", pBackendData, uOpenFlags));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    /* Image must be opened and the new flags must be valid. Just readonly and
     * info flags are supported. */
    if (!pImage || (uOpenFlags & ~(VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO)))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Implement this operation via reopening the image. */
    iscsiFreeImage(pImage, false);
    rc = iscsiOpenImage(pImage, uOpenFlags);

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnSetLCHSGeometry */
static int iscsiSetLCHSGeometry(void *pBackendData,
                                PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p LCHS=%u/%u/%u\n", pBackendData, pLCHSGeometry, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            rc = VERR_VD_IMAGE_READ_ONLY;
            goto out;
        }
        rc = VERR_VD_GEOMETRY_NOT_SET;
    }
    else
        rc = VERR_VD_NOT_OPENED;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnGetComment */
static int iscsiGetComment(void *pBackendData, char *pszComment,
                          size_t cbComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=%#p cbComment=%zu\n", pBackendData, pszComment, cbComment));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc comment='%s'\n", rc, pszComment));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnSetComment */
static int iscsiSetComment(void *pBackendData, const char *pszComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=\"%s\"\n", pBackendData, pszComment));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VD_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnGetUuid */
static int iscsiGetUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnSetUuid */
static int iscsiSetUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    LogFlowFunc(("%RTuuid\n", pUuid));
    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VD_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnGetModificationUuid */
static int iscsiGetModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnSetModificationUuid */
static int iscsiSetModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    LogFlowFunc(("%RTuuid\n", pUuid));
    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VD_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnGetParentUuid */
static int iscsiGetParentUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnSetParentUuid */
static int iscsiSetParentUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    LogFlowFunc(("%RTuuid\n", pUuid));
    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VD_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnGetParentModificationUuid */
static int iscsiGetParentModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnSetParentModificationUuid */
static int iscsiSetParentModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc;

    LogFlowFunc(("%RTuuid\n", pUuid));
    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VD_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnDump */
static void iscsiDump(void *pBackendData)
{
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;

    Assert(pImage);
    if (pImage)
    {
        /** @todo put something useful here */
        RTLogPrintf("Header: cVolume=%u\n", pImage->cVolume);
    }
}


/** @copydoc VBOXHDDBACKEND::pfnGetTimeStamp */
static int iscsiGetTimeStamp(void *pBackendData, PRTTIMESPEC pTimeStamp)
{
    LogFlowFunc(("pBackendData=%#p pTimeStamp=%#p\n", pBackendData, pTimeStamp));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc = VERR_NOT_SUPPORTED;

    Assert(pImage);
    NOREF(pImage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnGetParentTimeStamp */
static int iscsiGetParentTimeStamp(void *pBackendData, PRTTIMESPEC pTimeStamp)
{
    LogFlowFunc(("pBackendData=%#p pTimeStamp=%#p\n", pBackendData, pTimeStamp));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc = VERR_NOT_SUPPORTED;

    Assert(pImage);
    NOREF(pImage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnSetParentTimeStamp */
static int iscsiSetParentTimeStamp(void *pBackendData, PCRTTIMESPEC pTimeStamp)
{
    LogFlowFunc(("pBackendData=%#p pTimeStamp=%#p\n", pBackendData, pTimeStamp));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc = VERR_NOT_SUPPORTED;

    Assert(pImage);
    NOREF(pImage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnGetParentFilename */
static int iscsiGetParentFilename(void *pBackendData, char **ppszParentFilename)
{
    LogFlowFunc(("pBackendData=%#p ppszParentFilename=%#p\n", pBackendData, ppszParentFilename));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc = VERR_NOT_SUPPORTED;

    Assert(pImage);
    NOREF(pImage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnSetParentFilename */
static int iscsiSetParentFilename(void *pBackendData, const char *pszParentFilename)
{
    LogFlowFunc(("pBackendData=%#p pszParentFilename=%s\n", pBackendData, pszParentFilename));
    PISCSIIMAGE pImage = (PISCSIIMAGE)pBackendData;
    int rc = VERR_NOT_SUPPORTED;

    Assert(pImage);
    NOREF(pImage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnComposeLocation */
static int iscsiComposeLocation(PVDINTERFACE pConfig, char **pszLocation)
{
    char *pszTarget  = NULL;
    char *pszLUN     = NULL;
    char *pszAddress = NULL;
    int rc = VDCFGQueryStringAlloc(VDGetInterfaceConfig(pConfig), pConfig->pvUser, "TargetName", &pszTarget);
    if (RT_SUCCESS(rc))
    {
        rc = VDCFGQueryStringAlloc(VDGetInterfaceConfig(pConfig), pConfig->pvUser, "LUN", &pszLUN);
        if (RT_SUCCESS(rc))
        {
            rc = VDCFGQueryStringAlloc(VDGetInterfaceConfig(pConfig), pConfig->pvUser, "TargetAddress", &pszAddress);
            if (RT_SUCCESS(rc))
            {
                if (RTStrAPrintf(pszLocation, "iscsi://%s/%s/%s",
                                 pszAddress, pszTarget, pszLUN) < 0)
                    rc = VERR_NO_MEMORY;
            }
        }
    }
    RTMemFree(pszTarget);
    RTMemFree(pszLUN);
    RTMemFree(pszAddress);
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnComposeName */
static int iscsiComposeName(PVDINTERFACE pConfig, char **pszName)
{
    char *pszTarget  = NULL;
    char *pszLUN     = NULL;
    char *pszAddress = NULL;
    int rc = VDCFGQueryStringAlloc(VDGetInterfaceConfig(pConfig), pConfig->pvUser, "TargetName", &pszTarget);
    if (RT_SUCCESS(rc))
    {
        rc = VDCFGQueryStringAlloc(VDGetInterfaceConfig(pConfig), pConfig->pvUser, "LUN", &pszLUN);
        if (RT_SUCCESS(rc))
        {
            rc = VDCFGQueryStringAlloc(VDGetInterfaceConfig(pConfig), pConfig->pvUser, "TargetAddress", &pszAddress);
            if (RT_SUCCESS(rc))
            {
                /** @todo think about a nicer looking location scheme for iSCSI */
                if (RTStrAPrintf(pszName, "%s/%s/%s",
                                 pszAddress, pszTarget, pszLUN) < 0)
                    rc = VERR_NO_MEMORY;
            }
        }
    }
    RTMemFree(pszTarget);
    RTMemFree(pszLUN);
    RTMemFree(pszAddress);

    return rc;
}


VBOXHDDBACKEND g_ISCSIBackend =
{
    /* pszBackendName */
    "iSCSI",
    /* cbSize */
    sizeof(VBOXHDDBACKEND),
    /* uBackendCaps */
    VD_CAP_CONFIG | VD_CAP_TCPNET,
    /* papszFileExtensions */
    NULL,
    /* paConfigInfo */
    s_iscsiConfigInfo,
    /* hPlugin */
    NIL_RTLDRMOD,
    /* pfnCheckIfValid */
    iscsiCheckIfValid,
    /* pfnOpen */
    iscsiOpen,
    /* pfnCreate */
    iscsiCreate,
    /* pfnRename */
    iscsiRename,
    /* pfnClose */
    iscsiClose,
    /* pfnRead */
    iscsiRead,
    /* pfnWrite */
    iscsiWrite,
    /* pfnFlush */
    iscsiFlush,
    /* pfnGetVersion */
    iscsiGetVersion,
    /* pfnGetSize */
    iscsiGetSize,
    /* pfnGetFileSize */
    iscsiGetFileSize,
    /* pfnGetPCHSGeometry */
    iscsiGetPCHSGeometry,
    /* pfnSetPCHSGeometry */
    iscsiSetPCHSGeometry,
    /* pfnGetLCHSGeometry */
    iscsiGetLCHSGeometry,
    /* pfnSetLCHSGeometry */
    iscsiSetLCHSGeometry,
    /* pfnGetImageFlags */
    iscsiGetImageFlags,
    /* pfnGetOpenFlags */
    iscsiGetOpenFlags,
    /* pfnSetOpenFlags */
    iscsiSetOpenFlags,
    /* pfnGetComment */
    iscsiGetComment,
    /* pfnSetComment */
    iscsiSetComment,
    /* pfnGetUuid */
    iscsiGetUuid,
    /* pfnSetUuid */
    iscsiSetUuid,
    /* pfnGetModificationUuid */
    iscsiGetModificationUuid,
    /* pfnSetModificationUuid */
    iscsiSetModificationUuid,
    /* pfnGetParentUuid */
    iscsiGetParentUuid,
    /* pfnSetParentUuid */
    iscsiSetParentUuid,
    /* pfnGetParentModificationUuid */
    iscsiGetParentModificationUuid,
    /* pfnSetParentModificationUuid */
    iscsiSetParentModificationUuid,
    /* pfnDump */
    iscsiDump,
    /* pfnGetTimeStamp */
    iscsiGetTimeStamp,
    /* pfnGetParentTimeStamp */
    iscsiGetParentTimeStamp,
    /* pfnSetParentTimeStamp */
    iscsiSetParentTimeStamp,
    /* pfnGetParentFilename */
    iscsiGetParentFilename,
    /* pfnSetParentFilename */
    iscsiSetParentFilename,
    /* pfnIsAsyncIOSupported */
    NULL,
    /* pfnAsyncRead */
    NULL,
    /* pfnAsyncWrite */
    NULL,
    /* pfnComposeLocation */
    iscsiComposeLocation,
    /* pfnComposeName */
    iscsiComposeName
};
