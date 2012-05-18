/** @file
 * VBox Remote Desktop Extension (VRDE) - SmartCard interface.
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_RemoteDesktop_VRDESCard_h
#define ___VBox_RemoteDesktop_VRDESCard_h

#include <VBox/RemoteDesktop/VRDE.h>

/*
 * Interface for accessing the smart card reader devices on the client.
 *
 * Async callbacks are used for providing feedback, reporting errors, etc.
 *
 * The caller prepares a VRDESCARD*REQ structure and submits it.
 */

#define VRDE_SCARD_INTERFACE_NAME "SCARD"

/** The VRDE server smart card access interface entry points. Interface version 1. */
typedef struct VRDESCARDINTERFACE
{
    /** The header. */
    VRDEINTERFACEHDR header;

    /** Submit an async IO request to the client.
     *
     * @param hServer The VRDE server instance.
     * @param pvUser  The callers context of this request.
     * @param u32Function The function: VRDE_SCARD_FN_*.
     * @param pvData Function specific data: VRDESCARD*REQ.
     * @param cbData Size of data.
     *
     * @return IPRT status code.
     */
    DECLR3CALLBACKMEMBER(int, VRDESCardRequest, (HVRDESERVER hServer,
                                                 void *pvUser,
                                                 uint32_t u32Function,
                                                 const void *pvData,
                                                 uint32_t cbData));

} VRDESCARDINTERFACE;

/* Smartcard interface callbacks. */
typedef struct VRDESCARDCALLBACKS
{
    /** The header. */
    VRDEINTERFACEHDR header;

    /** Notifications.
     *
     * @param pvContext The callbacks context specified in VRDEGetInterface.
     * @param u32Id     The notification identifier: VRDE_SCARD_NOTIFY_*.
     * @param pvData    The notification specific data.
     * @param cbData    The size of buffer pointed by pvData.
     *
     * @return IPRT status code.
     */
    DECLR3CALLBACKMEMBER(int, VRDESCardCbNotify, (void *pvContext,
                                                  uint32_t u32Id,
                                                  void *pvData,
                                                  uint32_t cbData));

    /** IO response.
     *
     * @param pvContext The callbacks context specified in VRDEGetInterface.
     * @param rcRequest The IPRT status code for the request.
     * @param pvUser    The pvUser parameter of VRDESCardRequest.
     * @param u32Function The completed function: VRDE_SCARD_FN_*.
     * @param pvData    Function specific response data: VRDESCARD*RSP.
     * @param cbData    The size of the buffer pointed by pvData.
     *
     * @return IPRT status code.
     */
    DECLR3CALLBACKMEMBER(int, VRDESCardCbResponse, (void *pvContext,
                                                    int rcRequest,
                                                    void *pvUser,
                                                    uint32_t u32Function,
                                                    void *pvData,
                                                    uint32_t cbData));
} VRDESCARDCALLBACKS;


/*
 * Notifications.
 * u32Id parameter of VRDESCARDCALLBACKS::VRDESCardCbNotify.
 */

#define VRDE_SCARD_NOTIFY_ATTACH 1 /* A SCARD RDPDR device has been attached. */
#define VRDE_SCARD_NOTIFY_DETACH 2 /* A SCARD RDPDR device has been detached. */

/*
 * Notifications.
 * Data structures: pvData of VRDESCARDCALLBACKS::VRDESCardCbNotify.
 */
typedef struct VRDESCARDNOTIFYATTACH
{
    uint32_t u32ClientId;
    uint32_t u32DeviceId;
} VRDESCARDNOTIFYATTACH;

typedef struct VRDESCARDNOTIFYDETACH
{
    uint32_t u32ClientId;
    uint32_t u32DeviceId;
} VRDESCARDNOTIFYDETACH;


/*
 * IO request codes.
 * Must be not 0, which is used internally.
 */

#define VRDE_SCARD_FN_ESTABLISHCONTEXT  1
#define VRDE_SCARD_FN_LISTREADERS       2
#define VRDE_SCARD_FN_RELEASECONTEXT    3
#define VRDE_SCARD_FN_GETSTATUSCHANGE   4
#define VRDE_SCARD_FN_CANCEL            5
#define VRDE_SCARD_FN_CONNECT           6
#define VRDE_SCARD_FN_RECONNECT         7
#define VRDE_SCARD_FN_DISCONNECT        8
#define VRDE_SCARD_FN_BEGINTRANSACTION  9
#define VRDE_SCARD_FN_ENDTRANSACTION   10
#define VRDE_SCARD_FN_STATE            11
#define VRDE_SCARD_FN_STATUS           12
#define VRDE_SCARD_FN_TRANSMIT         13
#define VRDE_SCARD_FN_CONTROL          14
#define VRDE_SCARD_FN_GETATTRIB        15
#define VRDE_SCARD_FN_SETATTRIB        16

#define VRDESCARD_MAX_READERS 10
#define VRDESCARD_MAX_ATR_LENGTH 36
#define VRDESCARD_MAX_PCI_DATA 1024

/*
 * IO request data structures.
 */
typedef struct VRDESCARDCONTEXT
{
    uint32_t u32ContextSize;
    uint8_t au8Context[16];
} VRDESCARDCONTEXT;

typedef struct VRDESCARDHANDLE
{
    VRDESCARDCONTEXT Context;
    uint32_t u32HandleSize;
    uint8_t au8Handle[16];
} VRDESCARDHANDLE;

typedef struct VRDESCARDREADERSTATECALL
{
    char *pszReader; /* UTF8 */
    uint32_t u32CurrentState;
} VRDESCARDREADERSTATECALL;

typedef struct VRDESCARDREADERSTATERETURN
{
    uint32_t u32CurrentState;
    uint32_t u32EventState;
    uint32_t u32AtrLength;
    uint8_t au8Atr[VRDESCARD_MAX_ATR_LENGTH];
} VRDESCARDREADERSTATERETURN;

typedef struct VRDESCARDPCI
{
    uint32_t u32Protocol;
    uint32_t u32PciLength; /* Includes u32Protocol and u32PciLength fields. 8 if no data in au8PciData. */
    uint8_t au8PciData[VRDESCARD_MAX_PCI_DATA];
} VRDESCARDPCI;

typedef struct VRDESCARDESTABLISHCONTEXTREQ
{
    uint32_t u32ClientId;
    uint32_t u32DeviceId;
} VRDESCARDESTABLISHCONTEXTREQ;

typedef struct VRDESCARDESTABLISHCONTEXTRSP
{
    uint32_t u32ReturnCode;
    VRDESCARDCONTEXT Context;
} VRDESCARDESTABLISHCONTEXTRSP;

typedef struct VRDESCARDLISTREADERSREQ
{
    VRDESCARDCONTEXT Context;
} VRDESCARDLISTREADERSREQ;

typedef struct VRDESCARDLISTREADERSRSP
{
    uint32_t u32ReturnCode;
    uint32_t cReaders;
    char *apszNames[VRDESCARD_MAX_READERS];  /* UTF8 */
} VRDESCARDLISTREADERSRSP;

typedef struct VRDESCARDRELEASECONTEXTREQ
{
    VRDESCARDCONTEXT Context;
} VRDESCARDRELEASECONTEXTREQ;

typedef struct VRDESCARDRELEASECONTEXTRSP
{
    uint32_t u32ReturnCode;
} VRDESCARDRELEASECONTEXTRSP;

typedef struct VRDESCARDGETSTATUSCHANGEREQ
{
    VRDESCARDCONTEXT Context;
    uint32_t u32Timeout;
    uint32_t cReaders;
    VRDESCARDREADERSTATECALL aReaderStates[VRDESCARD_MAX_READERS];
} VRDESCARDGETSTATUSCHANGEREQ;

typedef struct VRDESCARDGETSTATUSCHANGERSP
{
    uint32_t u32ReturnCode;
    uint32_t cReaders;
    VRDESCARDREADERSTATERETURN aReaderStates[VRDESCARD_MAX_READERS];
} VRDESCARDGETSTATUSCHANGERSP;

typedef struct VRDESCARDCANCELREQ
{
    VRDESCARDCONTEXT Context;
} VRDESCARDCANCELREQ;

typedef struct VRDESCARDCANCELRSP
{
    uint32_t u32ReturnCode;
} VRDESCARDCANCELRSP;

typedef struct VRDESCARDCONNECTREQ
{
    VRDESCARDCONTEXT Context;
    char *pszReader; /* UTF8 */
    uint32_t u32ShareMode;
    uint32_t u32PreferredProtocols;
} VRDESCARDCONNECTREQ;

typedef struct VRDESCARDCONNECTRSP
{
    uint32_t u32ReturnCode;
    VRDESCARDHANDLE hCard;
    uint32_t u32ActiveProtocol;
} VRDESCARDCONNECTRSP;

typedef struct VRDESCARDRECONNECTREQ
{
    VRDESCARDHANDLE hCard;
    uint32_t u32ShareMode;
    uint32_t u32PreferredProtocols;
    uint32_t u32Initialization;
} VRDESCARDRECONNECTREQ;

typedef struct VRDESCARDRECONNECTRSP
{
    uint32_t u32ReturnCode;
    uint32_t u32ActiveProtocol;
} VRDESCARDRECONNECTRSP;

typedef struct VRDESCARDDISCONNECTREQ
{
    VRDESCARDHANDLE hCard;
    uint32_t u32Disposition;
} VRDESCARDDISCONNECTREQ;

typedef struct VRDESCARDDISCONNECTRSP
{
    uint32_t u32ReturnCode;
} VRDESCARDDISCONNECTRSP;

typedef struct VRDESCARDBEGINTRANSACTIONREQ
{
    VRDESCARDHANDLE hCard;
    uint32_t u32Disposition;
} VRDESCARDBEGINTRANSACTIONREQ;

typedef struct VRDESCARDBEGINTRANSACTIONRSP
{
    uint32_t u32ReturnCode;
} VRDESCARDBEGINTRANSACTIONRSP;

typedef struct VRDESCARDENDTRANSACTIONREQ
{
    VRDESCARDHANDLE hCard;
    uint32_t u32Disposition;
} VRDESCARDENDTRANSACTIONREQ;

typedef struct VRDESCARDENDTRANSACTIONRSP
{
    uint32_t u32ReturnCode;
} VRDESCARDENDTRANSACTIONRSP;

typedef struct VRDESCARDSTATEREQ
{
    VRDESCARDHANDLE hCard;
} VRDESCARDSTATEREQ;

typedef struct VRDESCARDSTATERSP
{
    uint32_t u32ReturnCode;
    uint32_t u32State;
    uint32_t u32Protocol;
    uint32_t u32AtrLength;
    uint8_t au8Atr[VRDESCARD_MAX_ATR_LENGTH];
} VRDESCARDSTATERSP;

typedef struct VRDESCARDSTATUSREQ
{
    VRDESCARDHANDLE hCard;
} VRDESCARDSTATUSREQ;

typedef struct VRDESCARDSTATUSRSP
{
    uint32_t u32ReturnCode;
    char *szReader;
    uint32_t u32State;
    uint32_t u32Protocol;
    uint32_t u32AtrLength;
    uint8_t au8Atr[VRDESCARD_MAX_ATR_LENGTH];
} VRDESCARDSTATUSRSP;

typedef struct VRDESCARDTRANSMITREQ
{
    VRDESCARDHANDLE hCard;
    VRDESCARDPCI ioSendPci;
    uint32_t u32SendLength;
    uint8_t *pu8SendBuffer;
    uint32_t u32RecvLength;
} VRDESCARDTRANSMITREQ;

typedef struct VRDESCARDTRANSMITRSP
{
    uint32_t u32ReturnCode;
    VRDESCARDPCI ioRecvPci;
    uint32_t u32RecvLength;
    uint8_t *pu8RecvBuffer;
} VRDESCARDTRANSMITRSP;

typedef struct VRDESCARDCONTROLREQ
{
    VRDESCARDHANDLE hCard;
    uint32_t u32ControlCode;
    uint32_t u32InBufferSize;
    uint8_t *pu8InBuffer;
    uint32_t u32OutBufferSize;
} VRDESCARDCONTROLREQ;

typedef struct VRDESCARDCONTROLRSP
{
    uint32_t u32ReturnCode;
    uint32_t u32OutBufferSize;
    uint8_t *pu8OutBuffer;
} VRDESCARDCONTROLRSP;

typedef struct VRDESCARDGETATTRIBREQ
{
    VRDESCARDHANDLE hCard;
    uint32_t u32AttrId;
    uint32_t u32AttrLen;
} VRDESCARDGETATTRIBREQ;

typedef struct VRDESCARDGETATTRIBRSP
{
    uint32_t u32ReturnCode;
    uint32_t u32AttrLength;
    uint8_t *pu8Attr;
} VRDESCARDGETATTRIBRSP;

typedef struct VRDESCARDSETATTRIBREQ
{
    VRDESCARDHANDLE hCard;
    uint32_t u32AttrId;
    uint32_t u32AttrLen;
    uint8_t *pu8Attr;
} VRDESCARDSETATTRIBREQ;

typedef struct VRDESCARDSETATTRIBRSP
{
    uint32_t u32ReturnCode;
} VRDESCARDSETATTRIBRSP;

#endif
