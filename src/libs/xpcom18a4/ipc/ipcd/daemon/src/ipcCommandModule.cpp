/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#define LOG_GROUP LOG_GROUP_IPC
#include <stdlib.h>
#include <string.h>
#include "ipcCommandModule.h"
#include "ipcClient.h"
#include "ipcMessageNew.h"
#include "ipcd.h"
#include "ipcm.h"

#include <VBox/log.h>

typedef void FNIPCMMSGHANDLER(PIPCDCLIENT pIpcClient, PCIPCMSG pMsg);
/** Pointer to a IPCM message handler. */
typedef FNIPCMMSGHANDLER *PFNIPCMMSGHANDLER;


DECLINLINE(uint32_t) IPCM_GetType(PCIPCMSG pMsg)
{
    return ((const ipcmMessageHeader *)IPCMsgGetPayload(pMsg))->mType;
}


DECLINLINE(uint32_t) IPCM_GetRequestIndex(PCIPCMSG pMsg)
{
    return ((const ipcmMessageHeader *)IPCMsgGetPayload(pMsg))->mRequestIndex;
}

//
// message handlers
//

static DECLCALLBACK(void) ipcmOnPing(PIPCDCLIENT pIpcClient, PCIPCMSG pMsg) RT_NOTHROW_DEF
{
    Log(("got PING\n"));

    const uint32_t aResp[3] = { IPCM_MSG_ACK_RESULT, IPCM_GetRequestIndex(pMsg), IPCM_OK };
    IPC_SendMsg(pIpcClient, IPCM_TARGET, &aResp[0], sizeof(aResp));
}

static DECLCALLBACK(void) ipcmOnClientHello(PIPCDCLIENT pIpcClient, PCIPCMSG pMsg) RT_NOTHROW_DEF
{
    Log(("got CLIENT_HELLO\n"));

    const uint32_t aResp[3] = { IPCM_MSG_ACK_CLIENT_ID, IPCM_GetRequestIndex(pMsg), ipcdClientGetId(pIpcClient) };
    IPC_SendMsg(pIpcClient, IPCM_TARGET, &aResp[0], sizeof(aResp));

    //
    // NOTE: it would almost make sense for this notification to live
    // in the transport layer code.  however, clients expect to receive
    // a CLIENT_ID as the first message following a CLIENT_HELLO, so we
    // must not allow modules to see a client until after we have sent
    // the CLIENT_ID message.
    //
    IPC_NotifyClientUp(pIpcClient);
}

static DECLCALLBACK(void) ipcmOnClientAddName(PIPCDCLIENT pIpcClient, PCIPCMSG pMsg) RT_NOTHROW_DEF
{
    Log(("got CLIENT_ADD_NAME\n"));

    int32_t status = IPCM_OK;
    uint32_t requestIndex = IPCM_GetRequestIndex(pMsg);

    const char *pszName = (const char *)((uint8_t *)IPCMsgGetPayload(pMsg) + 2 * sizeof(uint32_t));
    if (pszName)
    {
        PIPCDCLIENT pIpcClientResult = IPC_GetClientByName(ipcdClientGetDaemonState(pIpcClient), pszName);
        if (pIpcClientResult)
        {
            Log(("  client with such name already exists (ID = %d)\n", ipcdClientGetId(pIpcClientResult)));
            status = IPCM_ERROR_ALREADY_EXISTS;
        }
        else
            ipcdClientAddName(pIpcClient, pszName);
    }
    else
        status = IPCM_ERROR_INVALID_ARG;

    const uint32_t aResp[3] = { IPCM_MSG_ACK_RESULT, requestIndex, (uint32_t)status };
    IPC_SendMsg(pIpcClient, IPCM_TARGET, &aResp[0], sizeof(aResp));
}

static DECLCALLBACK(void) ipcmOnClientDelName(PIPCDCLIENT pIpcClient, PCIPCMSG pMsg) RT_NOTHROW_DEF
{
    Log(("got CLIENT_DEL_NAME\n"));

    PRInt32 status = IPCM_OK;
    PRUint32 requestIndex = IPCM_GetRequestIndex(pMsg);

    const char *pszName = (const char *)((uint8_t *)IPCMsgGetPayload(pMsg) + 2 * sizeof(uint32_t));
    if (pszName)
    {
        if (!ipcdClientDelName(pIpcClient, pszName))
        {
            Log(("  client doesn't have name '%s'\n", pszName));
            status = IPCM_ERROR_NO_SUCH_DATA;
        }
    }
    else
        status = IPCM_ERROR_INVALID_ARG;

    const uint32_t aResp[3] = { IPCM_MSG_ACK_RESULT, IPCM_GetRequestIndex(pMsg), (uint32_t)status  };
    IPC_SendMsg(pIpcClient, IPCM_TARGET, &aResp[0], sizeof(aResp));
}

static DECLCALLBACK(void) ipcmOnClientAddTarget(PIPCDCLIENT pIpcClient, PCIPCMSG pMsg) RT_NOTHROW_DEF
{
    Log(("got CLIENT_ADD_TARGET\n"));

    PRInt32 status = IPCM_OK;
    PRUint32 requestIndex = IPCM_GetRequestIndex(pMsg);

    const nsID *pidTarget = (const nsID *)((uint8_t *)IPCMsgGetPayload(pMsg) + 2 * sizeof(uint32_t));
    if (ipcdClientHasTarget(pIpcClient, pidTarget))
    {
        Log(("  target already defined for client\n"));
        status = IPCM_ERROR_ALREADY_EXISTS;
    }
    else
        ipcdClientAddTarget(pIpcClient, pidTarget);

    const uint32_t aResp[3] = { IPCM_MSG_ACK_RESULT, IPCM_GetRequestIndex(pMsg), (uint32_t)status };
    IPC_SendMsg(pIpcClient, IPCM_TARGET, &aResp[0], sizeof(aResp));
}

static DECLCALLBACK(void) ipcmOnClientDelTarget(PIPCDCLIENT pIpcClient, PCIPCMSG pMsg) RT_NOTHROW_DEF
{
    Log(("got CLIENT_DEL_TARGET\n"));

    PRInt32 status = IPCM_OK;
    PRUint32 requestIndex = IPCM_GetRequestIndex(pMsg);

    const nsID *pidTarget = (const nsID *)((uint8_t *)IPCMsgGetPayload(pMsg) + 2 * sizeof(uint32_t));
    if (!ipcdClientDelTarget(pIpcClient, pidTarget))
    {
        Log(("  client doesn't have the given target\n"));
        status = IPCM_ERROR_NO_SUCH_DATA;
    }

    const uint32_t aResp[3] = { IPCM_MSG_ACK_RESULT, IPCM_GetRequestIndex(pMsg), (uint32_t)status };
    IPC_SendMsg(pIpcClient, IPCM_TARGET, &aResp[0], sizeof(aResp));
}

static DECLCALLBACK(void) ipcmOnQueryClientByName(PIPCDCLIENT pIpcClient, PCIPCMSG pMsg) RT_NOTHROW_DEF
{
    Log(("got QUERY_CLIENT_BY_NAME\n"));

    PRUint32 requestIndex = IPCM_GetRequestIndex(pMsg);

    const char *pszName = (const char *)((uint8_t *)IPCMsgGetPayload(pMsg) + 2 * sizeof(uint32_t));
    PIPCDCLIENT pIpcClientResult = IPC_GetClientByName(ipcdClientGetDaemonState(pIpcClient), pszName);
    if (pIpcClientResult)
    {
        Log(("  client exists w/ ID = %u\n", ipcdClientGetId(pIpcClientResult)));
        const uint32_t aResp[3] = { IPCM_MSG_ACK_CLIENT_ID, requestIndex, ipcdClientGetId(pIpcClientResult) };
        IPC_SendMsg(pIpcClient, IPCM_TARGET, &aResp[0], sizeof(aResp));
    }
    else
    {
        Log(("  client does not exist\n"));
        const uint32_t aResp[3] = { IPCM_MSG_ACK_RESULT, requestIndex, (uint32_t)IPCM_ERROR_NO_CLIENT };
        IPC_SendMsg(pIpcClient, IPCM_TARGET, &aResp[0], sizeof(aResp));
    }
}

static DECLCALLBACK(void) ipcmOnForward(PIPCDCLIENT pIpcClient, PCIPCMSG pMsg) RT_NOTHROW_DEF
{
    Log(("got FORWARD\n"));

    uint32_t idClient = *(uint32_t *)((uint8_t *)IPCMsgGetPayload(pMsg) + 2 * sizeof(uint32_t));
    PIPCDCLIENT pIpcClientDst = IPC_GetClientByID(ipcdClientGetDaemonState(pIpcClient), idClient);
    if (!pIpcClientDst)
    {
        Log(("  destination client not found!\n"));
        const uint32_t aResp[3] = { IPCM_MSG_ACK_RESULT, IPCM_GetRequestIndex(pMsg), (uint32_t)IPCM_ERROR_NO_CLIENT };
        IPC_SendMsg(pIpcClient, IPCM_TARGET, &aResp[0], sizeof(aResp));
        return;
    }
    // inform client that its message will be forwarded
    const uint32_t aResp[3] = { IPCM_MSG_ACK_RESULT, IPCM_GetRequestIndex(pMsg), IPCM_OK };
    IPC_SendMsg(pIpcClient, IPCM_TARGET, &aResp[0], sizeof(aResp));

    uint32_t aIpcmFwdHdr[3] = { IPCM_MSG_PSH_FORWARD, IPCM_NewRequestIndex(), ipcdClientGetId(pIpcClient) };
    RTSGSEG aSegs[2];

    aSegs[0].pvSeg = &aIpcmFwdHdr[0];
    aSegs[0].cbSeg = sizeof(aIpcmFwdHdr);

    aSegs[1].pvSeg = (uint8_t *)IPCMsgGetPayload(pMsg) + sizeof(aIpcmFwdHdr);
    aSegs[1].cbSeg = IPCMsgGetPayloadSize(pMsg) - sizeof(aIpcmFwdHdr);

    IPC_SendMsgSg(pIpcClientDst, IPCM_TARGET, IPCMsgGetPayloadSize(pMsg), &aSegs[0], RT_ELEMENTS(aSegs));
}


DECLHIDDEN(void) IPCM_HandleMsg(PIPCDCLIENT pIpcClient, PCIPCMSG pMsg)
{
    static const PFNIPCMMSGHANDLER aHandlers[] =
    {
        ipcmOnPing,
        ipcmOnForward,
        ipcmOnClientHello,
        ipcmOnClientAddName,
        ipcmOnClientDelName,
        ipcmOnClientAddTarget,
        ipcmOnClientDelTarget,
        ipcmOnQueryClientByName
    };

    uint32_t u32Type = IPCM_GetType(pMsg);
    Log(("IPCM_HandleMsg [u32Type=%x]\n", u32Type));

    if (!(u32Type & IPCM_MSG_CLASS_REQ))
    {
        Log(("not a request -- ignoring message\n"));
        return;
    }

    u32Type &= ~IPCM_MSG_CLASS_REQ;
    u32Type--;
    if (u32Type >= RT_ELEMENTS(aHandlers))
    {
        Log(("unknown request -- ignoring message\n"));
        return;
    }

    aHandlers[u32Type](pIpcClient, pMsg);
}
