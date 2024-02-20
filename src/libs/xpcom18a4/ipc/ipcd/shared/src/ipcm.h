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

#ifndef ipcm_h__
#define ipcm_h__

#include <iprt/assertcompile.h>

#include "ipcMessageNew.h"

//-----------------------------------------------------------------------------

//
// IPCM (IPC Manager) protocol support
//

// The IPCM message target identifier:
extern const nsID IPCM_TARGET;

//
// Every IPCM message has the following structure:
//
//  +-----------------------------------------+
//  | (ipc message header)                    |
//  +-----------------------------------------+
//  | DWORD : type                            |
//  +-----------------------------------------+
//  | DWORD : requestIndex                    |
//  +-----------------------------------------+
//  .                                         .
//  . (payload)                               .
//  .                                         .
//  +-----------------------------------------+
//
// where |type| is an integer uniquely identifying the message.  the type is
// composed of a message class identifier and a message number.  there are 3
// message classes:
//
//   ACK - acknowledging a request
//   REQ - making a request
//   PSH - providing unrequested, "pushed" information
//
// The requestIndex field is initialized when a request is made.  An
// acknowledgement's requestIndex is equal to that of its corresponding
// request message.  This enables the requesting side of the message exchange
// to match acknowledgements to requests.  The requestIndex field is ignored
// for PSH messages.
//

// The IPCM message class is stored in the most significant byte.
#define IPCM_MSG_CLASS_REQ (1 << 24)
#define IPCM_MSG_CLASS_ACK (2 << 24)
#define IPCM_MSG_CLASS_PSH (4 << 24)

// Requests
#define IPCM_MSG_REQ_PING                   (IPCM_MSG_CLASS_REQ | 1)
#define IPCM_MSG_REQ_FORWARD                (IPCM_MSG_CLASS_REQ | 2)
#define IPCM_MSG_REQ_CLIENT_HELLO           (IPCM_MSG_CLASS_REQ | 3)
#define IPCM_MSG_REQ_CLIENT_ADD_NAME        (IPCM_MSG_CLASS_REQ | 4)
#define IPCM_MSG_REQ_CLIENT_DEL_NAME        (IPCM_MSG_CLASS_REQ | 5)
#define IPCM_MSG_REQ_CLIENT_ADD_TARGET      (IPCM_MSG_CLASS_REQ | 6)
#define IPCM_MSG_REQ_CLIENT_DEL_TARGET      (IPCM_MSG_CLASS_REQ | 7)
#define IPCM_MSG_REQ_QUERY_CLIENT_BY_NAME   (IPCM_MSG_CLASS_REQ | 8)
#define IPCM_MSG_REQ_QUERY_CLIENT_NAMES     (IPCM_MSG_CLASS_REQ | 9)  // TODO
#define IPCM_MSG_REQ_QUERY_CLIENT_TARGETS   (IPCM_MSG_CLASS_REQ | 10) // TODO

// Acknowledgements
#define IPCM_MSG_ACK_RESULT                 (IPCM_MSG_CLASS_ACK | 1)
#define IPCM_MSG_ACK_CLIENT_ID              (IPCM_MSG_CLASS_ACK | 2)
#define IPCM_MSG_ACK_CLIENT_NAMES           (IPCM_MSG_CLASS_ACK | 3)  // TODO
#define IPCM_MSG_ACK_CLIENT_TARGETS         (IPCM_MSG_CLASS_ACK | 4)  // TODO

// Push messages
#define IPCM_MSG_PSH_CLIENT_STATE           (IPCM_MSG_CLASS_PSH | 1)
#define IPCM_MSG_PSH_FORWARD                (IPCM_MSG_CLASS_PSH | 2)

//-----------------------------------------------------------------------------

//
// IPCM header
//
typedef struct IPCMMSGHDR
{
    uint32_t u32Type;
    uint32_t u32RequestIndex;
} IPCMMSGHDR;
AssertCompileSize(struct IPCMMSGHDR, 8);
/** Pointer to an IPCM header. */
typedef IPCMMSGHDR *PIPCMMSGHDR;
/** Pointer to a const IPCM header. */
typedef const IPCMMSGHDR *PCIPCMMSGHDR;


DECLINLINE(uint32_t) IPCM_GetType(PCIPCMSG pMsg)
{
    return ((PCIPCMMSGHDR)IPCMsgGetPayload(pMsg))->u32Type;
}


DECLINLINE(uint32_t) IPCM_GetRequestIndex(PCIPCMSG pMsg)
{
    return ((PCIPCMMSGHDR)IPCMsgGetPayload(pMsg))->u32RequestIndex;
}


//
// return a request index that is unique to this process.
//
DECLHIDDEN(uint32_t) IPCM_NewRequestIndex();

//-----------------------------------------------------------------------------

//
// The IPCM protocol is detailed below:
//

// REQUESTS

//
// req: IPCM_MSG_REQ_PING
// ack: IPCM_MSG_ACK_RESULT
//
// A PING can be sent from either a client to the daemon, or from the daemon
// to a client.  The expected acknowledgement is a RESULT message with a status
// code of 0.
//
// This request message has no payload.
//

//
// req: IPCM_MSG_REQ_FORWARD
// ack: IPCM_MSG_ACK_RESULT
//
// A FORWARD is sent when a client wishes to send a message to another client.
// The payload of this message is another message that should be forwarded by
// the daemon's IPCM to the specified client.  The expected acknowledgment is
// a RESULT message with a status code indicating success or failure.
//
// When the daemon receives a FORWARD message, it creates a PSH_FORWARD message
// and sends that on to the destination client.
//
// This request message has as its payload:
//
//  +-----------------------------------------+
//  | DWORD : clientID                        |
//  +-----------------------------------------+
//  | (innerMsgHeader)                        |
//  +-----------------------------------------+
//  | (innerMsgData)                          |
//  +-----------------------------------------+
//

//
// req: IPCM_MSG_REQ_CLIENT_HELLO
// ack: IPCM_MSG_REQ_CLIENT_ID <or> IPCM_MSG_REQ_RESULT
//
// A CLIENT_HELLO is sent when a client connects to the IPC daemon.  The
// expected acknowledgement is a CLIENT_ID message informing the new client of
// its ClientID.  If for some reason the IPC daemon cannot accept the new
// client, it returns a RESULT message with a failure status code.
//
// This request message has no payload.
//

//
// req: IPCM_MSG_REQ_CLIENT_ADD_NAME
// ack: IPCM_MSG_ACK_RESULT
//
// A CLIENT_ADD_NAME is sent when a client wishes to register an additional
// name for itself.  The expected acknowledgement is a RESULT message with a
// status code indicating success or failure.
//
// This request message has as its payload a null-terminated ASCII character
// string indicating the name of the client.
//

//
// req: IPCM_MSG_REQ_CLIENT_DEL_NAME
// ack: IPCM_MSG_ACK_RESULT
//
// A CLIENT_DEL_NAME is sent when a client wishes to unregister a name that it
// has registered.  The expected acknowledgement is a RESULT message with a
// status code indicating success or failure.
//
// This request message has as its payload a null-terminated ASCII character
// string indicating the name of the client.
//

//
// req: IPCM_MSG_REQ_CLIENT_ADD_TARGET
// ack: IPCM_MSG_ACK_RESULT
//
// A CLIENT_ADD_TARGET is sent when a client wishes to register an additional
// target that it supports.  The expected acknowledgement is a RESULT message
// with a status code indicating success or failure.
//
// This request message has as its payload a 128-bit UUID indicating the
// target to add.
//

//
// req: IPCM_MSG_REQ_CLIENT_DEL_TARGET
// ack: IPCM_MSG_ACK_RESULT
//
// A CLIENT_DEL_TARGET is sent when a client wishes to unregister a target
// that it has registered.  The expected acknowledgement is a RESULT message
// with a status code indicating success or failure.
//
// This request message has as its payload a 128-bit UUID indicating the
// target to remove.
//

//
// req: IPCM_MSG_REQ_QUERY_CLIENT_BY_NAME
// ack: IPCM_MSG_ACK_CLIENT_ID <or> IPCM_MSG_ACK_RESULT
//
// A QUERY_CLIENT_BY_NAME may be sent by a client to discover the client that
// is known by a common name.  If more than one client matches the name, then
// only the ID of the more recently registered client is returned.  The
// expected acknowledgement is a CLIENT_ID message carrying the ID of the
// corresponding client.  If no client matches the given name or if some error
// occurs, then a RESULT message with a failure status code is returned.
//
// This request message has as its payload a null-terminated ASCII character
// string indicating the client name to query.
//

// ACKNOWLEDGEMENTS

//
// ack: IPCM_MSG_ACK_RESULT
//
// This acknowledgement is returned to indicate a success or failure status.
//
// The payload consists of a single DWORD value.
//
// Possible status codes are listed below (negative values indicate failure
// codes):
//
#define IPCM_OK                      0  // success: generic
#define IPCM_ERROR_GENERIC          -1  // failure: generic
#define IPCM_ERROR_NO_CLIENT        -2  // failure: client does not exist
#define IPCM_ERROR_INVALID_ARG      -3  // failure: invalid request argument
#define IPCM_ERROR_NO_SUCH_DATA     -4  // failure: requested data does not exist
#define IPCM_ERROR_ALREADY_EXISTS   -5  // failure: data to set already exists

//
// ack: IPCM_MSG_ACK_CLIENT_ID
//
// This acknowledgement is returned to specify a client ID.
//
// The payload consists of a single DWORD value.
//

// PUSH MESSAGES

//
// psh: ICPM_MSG_PSH_CLIENT_STATE
//
// This message is sent to clients to indicate the status of other clients.
//
// The payload consists of:
//
//  +-----------------------------------------+
//  | DWORD : clientID                        |
//  +-----------------------------------------+
//  | DWORD : clientState                     |
//  +-----------------------------------------+
//
// where, clientState is one of the following values indicating whether the
// client has recently connected (up) or disconnected (down):
//
#define IPCM_CLIENT_STATE_UP     1
#define IPCM_CLIENT_STATE_DOWN   2

//
// psh: IPCM_MSG_PSH_FORWARD
//
// This message is sent by the daemon to a client on behalf of another client.
// The recipient is expected to unpack the contained message and process it.
//
// The payload of this message matches the payload of IPCM_MSG_REQ_FORWARD,
// with the exception that the clientID field is set to the clientID of the
// sender of the IPCM_MSG_REQ_FORWARD message.
//

// REQUESTS

/** The ping message consists of just the header. */
typedef IPCMMSGHDR IPCMMSGPING;
typedef IPCMMSGPING *PIPCMMSGPING;

DECLINLINE(void) IPCMMsgPingInit(PIPCMMSGPING pThis)
{
    pThis->u32Type         = IPCM_MSG_REQ_PING;
    pThis->u32RequestIndex = IPCM_NewRequestIndex();
}


/** The client hello message consists of just the header. */
typedef IPCMMSGHDR IPCMMSGCLIENTHELLO;
typedef IPCMMSGCLIENTHELLO *PIPCMMSGCLIENTHELLO;

DECLINLINE(void) IPCMMsgClientHelloInit(PIPCMMSGCLIENTHELLO pThis)
{
    pThis->u32Type         = IPCM_MSG_REQ_CLIENT_HELLO;
    pThis->u32RequestIndex = IPCM_NewRequestIndex();
}


typedef struct IPCMMSGFORWARD
{
    IPCMMSGHDR          Hdr;
    uint32_t            u32ClientId;
} IPCMMSGFORWARD;
AssertCompileSize(IPCMMSGFORWARD, 3 * sizeof(uint32_t));
typedef IPCMMSGFORWARD *PIPCMMSGFORWARD;
typedef const IPCMMSGFORWARD *PCIPCMMSGFORWARD;


DECLINLINE(int) ipcmMsgInitHdrStr(PIPCMSG pMsg, uint32_t u32Type, const char *psz)
{
    size_t cbStr = strlen(psz) + 1; /* Includes terminator. */
    const IPCMMSGHDR Hdr = { u32Type, IPCM_NewRequestIndex() };
    RTSGSEG aSegs[2];

    aSegs[0].pvSeg = (void *)&Hdr;
    aSegs[0].cbSeg = sizeof(Hdr);

    aSegs[1].pvSeg = (void *)psz;
    aSegs[1].cbSeg = cbStr;
    return IPCMsgInitSg(pMsg, IPCM_TARGET, cbStr + sizeof(Hdr), &aSegs[0], RT_ELEMENTS(aSegs));
}


DECLINLINE(int) IPCMMsgClientAddNameInit(PIPCMSG pMsg, const char *pszName)
{
    return ipcmMsgInitHdrStr(pMsg, IPCM_MSG_REQ_CLIENT_ADD_NAME, pszName);
}


DECLINLINE(int) IPCMMsgClientDelNameInit(PIPCMSG pMsg, const char *pszName)
{
    return ipcmMsgInitHdrStr(pMsg, IPCM_MSG_REQ_CLIENT_DEL_NAME, pszName);
}


typedef struct IPCMMSGCLIENTADDDELTARGET
{
    IPCMMSGHDR          Hdr;
    nsID                idTarget;
} IPCMMSGCLIENTADDDELTARGET;
AssertCompileSize(IPCMMSGCLIENTADDDELTARGET, 2 * sizeof(uint32_t) + sizeof(nsID));
typedef IPCMMSGCLIENTADDDELTARGET *PIPCMMSGCLIENTADDDELTARGET;
typedef const IPCMMSGCLIENTADDDELTARGET *PCIPCMMSGCLIENTADDDELTARGET;

DECLINLINE(void) IPCMMsgAddTargetInit(PIPCMMSGCLIENTADDDELTARGET pThis, const nsID &target)
{
    pThis->Hdr.u32Type         = IPCM_MSG_REQ_CLIENT_ADD_TARGET;
    pThis->Hdr.u32RequestIndex = IPCM_NewRequestIndex();
    pThis->idTarget            = target;
}


DECLINLINE(void) IPCMMsgDelTargetInit(PIPCMMSGCLIENTADDDELTARGET pThis, const nsID &target)
{
    pThis->Hdr.u32Type         = IPCM_MSG_REQ_CLIENT_DEL_TARGET;
    pThis->Hdr.u32RequestIndex = IPCM_NewRequestIndex();
    pThis->idTarget            = target;
}


DECLINLINE(int) IPCMMsgQueryClientByNameInit(PIPCMSG pMsg, const char *pszName)
{
    return ipcmMsgInitHdrStr(pMsg, IPCM_MSG_REQ_QUERY_CLIENT_BY_NAME, pszName);
}


// ACKNOWLEDGEMENTS

typedef struct IPCMMSGRESULT
{
    IPCMMSGHDR          Hdr;
    int32_t             i32Status;
} IPCMMSGRESULT;
AssertCompileSize(IPCMMSGRESULT, 3 * sizeof(uint32_t));
typedef IPCMMSGRESULT *PIPCMMSGRESULT;
typedef const IPCMMSGRESULT *PCIPCMMSGRESULT;

typedef struct IPCMMSGCLIENTID
{
    IPCMMSGHDR          Hdr;
    uint32_t            u32ClientId;
} IPCMMSGCLIENTID;
AssertCompileSize(IPCMMSGCLIENTID, 3 * sizeof(uint32_t));
typedef IPCMMSGCLIENTID *PIPCMMSGCLIENTID;
typedef const IPCMMSGCLIENTID *PCIPCMMSGCLIENTID;


// PUSH MESSAGES

typedef struct IPCMMSGCLIENTSTATE
{
    IPCMMSGHDR          Hdr;
    uint32_t            u32ClientId;
    uint32_t            u32ClientStatus;
} IPCMMSGCLIENTSTATE;
AssertCompileSize(IPCMMSGCLIENTSTATE, 4 * sizeof(uint32_t));
typedef IPCMMSGCLIENTSTATE *PIPCMMSGCLIENTSTATE;
typedef const IPCMMSGCLIENTSTATE *PCIPCMMSGCLIENTSTATE;

/**
 * Helper structure for stack based messages.
 */
typedef struct IPCMMSGSTACK
{
    IPCMSGHDR                       Hdr;
    union
    {
        IPCMMSGHDR                  Hdr;
        IPCMMSGPING                 Ping;
        IPCMMSGCLIENTHELLO          ClientHello;
        IPCMMSGCLIENTADDDELTARGET   AddDelTarget;
        IPCMMSGRESULT               Result;
        IPCMMSGCLIENTID             ClientId;
        IPCMMSGCLIENTSTATE          ClientState;
    } Ipcm;
} IPCMMSGSTACK;

#endif // !ipcm_h__
