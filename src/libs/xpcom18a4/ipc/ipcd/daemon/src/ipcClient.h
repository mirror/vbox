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

#ifndef ipcClientUnix_h__
#define ipcClientUnix_h__

#include <iprt/list.h>
#include <iprt/socket.h>

#include "ipcMessageNew.h"

#include "ipcStringList.h"
#include "ipcIDList.h"


/** Pointer to the IPCD manager state. */
typedef struct IPCDSTATE *PIPCDSTATE;

/**
 * IPC daemon client state.
 *
 * @note Treat these as opaque and use the accessor functions below.
 */
typedef struct IPCDCLIENT
{
    /** Node for the list of clients. */
    RTLISTNODE      NdClients;
    /** The owning IPCD state. */
    PIPCDSTATE      pIpcd;
    /** Flag whether the client state is currently in use. */
    bool            fUsed;
    /** The poll ID for this client. */
    uint32_t        idPoll;
    /** Poll event flags */
    uint32_t        fPollEvts;

    /** Client ID .*/
    uint32_t        idClient;
    /** Client socket. */
    RTSOCKET        hSock;
    /** The current incoming message. */
    IPCMSG          MsgIn;
    /** List of outgoing messages. */
    RTLISTANCHOR    LstMsgsOut;
    /** How much of the current message in the output buffer was already sent. */
    uint32_t        offMsgOutBuf;

    bool            fExpectsSyncReply;
    ipcStringList   mNames;
    ipcIDList       mTargets;
} IPCDCLIENT;
/** Pointer to an IPCD client state. */
typedef IPCDCLIENT *PIPCDCLIENT;
/** Pointer to a const IPCD client state. */
typedef const IPCDCLIENT *PCIPCDCLIENT;


DECLINLINE(PIPCDSTATE) ipcdClientGetDaemonState(PCIPCDCLIENT pThis)
{
    return pThis->pIpcd;
}


DECLINLINE(uint32_t) ipcdClientGetId(PCIPCDCLIENT pThis)
{
    return pThis->idClient;
}


DECLINLINE(void) ipcdClientEnqueueOutboundMsg(PIPCDCLIENT pThis, PIPCMSG pMsg)
{
    RTListAppend(&pThis->LstMsgsOut, &pMsg->NdMsg);
}

DECLINLINE(bool) ipcdClientHasName(PCIPCDCLIENT pThis, const char *pszName)
{
    return pThis->mNames.Find(pszName) != NULL;
}

DECLHIDDEN(void) ipcdClientAddName(PIPCDCLIENT pThis, const char *pszName);

DECLHIDDEN(bool) ipcdClientDelName(PIPCDCLIENT pThis, const char *pszName);

DECLINLINE(bool) ipcdClientHasTarget(PCIPCDCLIENT pThis, const nsID *target)
{
    return pThis->mTargets.Find(*target) != NULL;
}

DECLHIDDEN(void) ipcdClientAddTarget(PIPCDCLIENT pThis, const nsID *target);

DECLHIDDEN(bool) ipcdClientDelTarget(PIPCDCLIENT pThis, const nsID *target);

DECLHIDDEN(int) ipcdClientInit(PIPCDCLIENT pThis, PIPCDSTATE pIpcd, uint32_t idPoll, RTSOCKET hSock);

DECLHIDDEN(void) ipcdClientDestroy(PIPCDCLIENT pThis);

//
// called to process a client file descriptor.  the value of pollFlags
// indicates the state of the socket.
//
// returns:
//   0                - to cancel client connection
//   RTPOLL_EVT_READ  - to poll for a readable socket
//   RTPOLL_EVT_WRITE - to poll for a writable socket
//   (both flags)     - to poll for either a readable or writable socket
//
// the socket is non-blocking.
//
DECLHIDDEN(uint32_t) ipcdClientProcess(PIPCDCLIENT pThis, uint32_t fPollFlags);

DECLINLINE(void) ipcdClientSetExpectsSyncReply(PIPCDCLIENT pThis, bool f)
{
    pThis->fExpectsSyncReply = f;
}

DECLINLINE(bool) ipcdClientGetExpectsSyncReply(PCIPCDCLIENT pThis)
{
    return pThis->fExpectsSyncReply;
}

#endif // !ipcClientUnix_h__
