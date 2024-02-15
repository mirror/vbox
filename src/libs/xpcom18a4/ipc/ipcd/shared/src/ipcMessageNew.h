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

#ifndef ipcMessageNew_h__
#define ipcMessageNew_h__

#include <iprt/assert.h>
#include <iprt/assertcompile.h>
#include <iprt/cdefs.h>
#include <iprt/err.h>
#include <iprt/list.h>
#include <iprt/types.h>
#include <iprt/mem.h>
#include <iprt/sg.h>

#include "nsID.h"

//
// ipc message format:
//
//   +------------------------------------+
//   | DWORD : length                     |
//   +------------------+-----------------+
//   | WORD  : version  | WORD : flags    |
//   +------------------+-----------------+
//   | nsID  : target                     |
//   +------------------------------------+
//   | data                               |
//   +------------------------------------+
//
// header is 24 bytes.  flags are defined below.  default value of flags is
// zero.  protocol implementations should ignore unrecognized flags.  target
// is a 16 byte UUID indicating the intended receiver of this message.
//
typedef struct IPCMSGHDR
{
    /** Size of the message in bytes, including this header. */
    uint32_t cbMsg;
    /** The version of the IPC message header. */
    uint16_t u16Version;
    /** Flags for the message. */
    uint16_t u16Flags;
    /** The target ID for this message. */
    nsID     idTarget;
} IPCMSGHDR;
AssertCompileSize(IPCMSGHDR, 4 + 2 + 2 + 16);
/** Pointer to the IPC message header. */
typedef IPCMSGHDR *PIPCMSGHDR;
/** Pointer to a const IPC message header. */
typedef const IPCMSGHDR *PCIPCMSGHDR;

/** IPC message version. */
#define IPC_MSG_HDR_VERSION         UINT16_C(0x1)

//
// the IPC message protocol supports synchronous messages.  these messages can
// only be sent from a client to the daemon.  a daemon module cannot send a
// synchronous message.  the client sets the SYNC_QUERY flag to indicate that
// it is expecting a response with the SYNC_REPLY flag set.
//
#define IPC_MSG_HDR_FLAG_SYNC_QUERY RT_BIT(0)
#define IPC_MSG_HDR_FLAG_SYNC_REPLY RT_BIT(1)

//
// a special flag to prevent repeated processing of the same message by two
// or more selectors when walking through the queue of pending messages
// in WaitTarget().
//
#define IPC_MSG_HDR_FLAG_IN_PROCESS RT_BIT(2)


/**
 * IPC message structure.
 */
typedef struct IPCMSG
{
    /** List node for putting the message onto a list. */
    RTLISTNODE              NdMsg;
    /** Pointer to the IPC message header. */
    PIPCMSGHDR              pMsgHdr;
    /** Size of the message buffer in bytes (maximum size of the buffer). */
    size_t                  cbBuf;
    /** Pointer to the message buffer. */
    uint8_t                 *pbBuf;
    /** Offset into the message buffer to append data to. */
    uint32_t                offAppend;
    /** Flag whether the message is complete and the buffer is therefore readonly right now. */
    bool                    fReadonly;
} IPCMSG;
/** Pointer to an IPC message. */
typedef IPCMSG *PIPCMSG;
/** Pointer to a const IPC message. */
typedef const IPCMSG *PCIPCMSG;


DECLINLINE(int) IPCMsgInit(PIPCMSG pThis, size_t cbBuf)
{
    Assert(!pThis->pbBuf);

    pThis->pbBuf = (uint8_t *)RTMemAlloc(sizeof(*pThis->pMsgHdr) + cbBuf);
    if (RT_UNLIKELY(!pThis->pbBuf))
        return VERR_NO_MEMORY;

    pThis->cbBuf     = sizeof(*pThis->pMsgHdr) + cbBuf;
    pThis->offAppend = 0;
    pThis->fReadonly = false;
    pThis->pMsgHdr   = NULL;
    return VINF_SUCCESS;
}


/**
 * Allocates a new IPC message structure which can hold the given amount of data initially.
 *
 * @returns Pointer to the IPC message structure or NULL if out of memory.
 * @param   cbMsg       Size of the payload message buffer in bytes, can be 0 if not known at allocation time.
 */
DECLINLINE(PIPCMSG) IPCMsgAlloc(size_t cbMsg)
{
    PIPCMSG pThis = (PIPCMSG)RTMemAllocZ(sizeof(*pThis));
    if (RT_UNLIKELY(!pThis))
        return NULL;

    if (cbMsg)
    {
        pThis->pbBuf = (uint8_t *)RTMemAlloc(sizeof(*pThis->pMsgHdr) + cbMsg);
        if (RT_UNLIKELY(!pThis->pbBuf))
        {
            RTMemFree(pThis);
            return NULL;
        }

        pThis->cbBuf = sizeof(*pThis->pMsgHdr) + cbMsg;
    }

    return pThis;
}


/**
 * Frees the given message and all associated resources.
 *
 * @param   pThis       The IPC message to free.
 * @param   fFreeStruct Flag whether to free the structure (true) or just the message buffer (false).
 */
DECLINLINE(void) IPCMsgFree(PIPCMSG pThis, bool fFreeStruct)
{
    if (pThis->pbBuf)
        RTMemFree(pThis->pbBuf);
    pThis->pbBuf     = NULL;
    pThis->pMsgHdr   = NULL;
    pThis->cbBuf     = 0;
    pThis->offAppend = 0;
    pThis->fReadonly = false;

    if (fFreeStruct)
        RTMemFree(pThis);
}


/**
 * Reset the given message structure to accept a new message.
 *
 * @param   pThis       The IPC message to reset.
 */
DECLINLINE(void) IPCMsgReset(PIPCMSG pThis)
{
    pThis->pMsgHdr   = NULL;
    pThis->offAppend = 0;
    pThis->fReadonly = false;
}


/**
 * Returns a pointer to the message data including the header.
 *
 * @param   pThis       The IPC message.
 */
DECLINLINE(void *) IPCMsgGetBuf(PCIPCMSG pThis)
{
    AssertReturn(pThis->pMsgHdr, NULL);

    return pThis->pMsgHdr;
}


/**
 * Returns the size of the message in bytes including the header.
 *
 * @param   pThis       The IPC message.
 */
DECLINLINE(size_t) IPCMsgGetSize(PCIPCMSG pThis)
{
    AssertReturn(pThis->pMsgHdr, 0);

    return pThis->pMsgHdr->cbMsg;
}


/**
 * Returns the message header of the given message.
 *
 * @returns Pointer to the IPC message header.
 * @param   pThis       The IPC message.
 */
DECLINLINE(PCIPCMSGHDR) IPCMsgGetHdr(PCIPCMSG pThis)
{
    Assert(pThis->pMsgHdr);
    return pThis->pMsgHdr;
}


DECLINLINE(const nsID *) IPCMsgGetTarget(PCIPCMSG pThis)
{
    AssertReturn(pThis->pMsgHdr, NULL);
    return &pThis->pMsgHdr->idTarget;
}


/**
 * Returns a pointer to the message payload data.
 *
 * @param   pThis       The IPC message.
 */
DECLINLINE(void *) IPCMsgGetPayload(PCIPCMSG pThis)
{
    AssertReturn(pThis->pMsgHdr, NULL);

    return pThis->pMsgHdr + 1;
}


/**
 * Returns the size of the message payload in bytes.
 *
 * @param   pThis       The IPC message.
 */
DECLINLINE(size_t) IPCMsgGetPayloadSize(PCIPCMSG pThis)
{
    AssertReturn(pThis->pMsgHdr, 0);

    return pThis->pMsgHdr->cbMsg - sizeof(*pThis->pMsgHdr);
}


/**
 * Returns whether the message is readonly (or complete).
 *
 * @returns Flag whether the message is readonly.
 * @param   pThis       The IPC message.
 */
DECLINLINE(bool) IPCMsgIsReadonly(PCIPCMSG pThis)
{
    return pThis->fReadonly;
}


/**
 * Returns whether the given flag is set in the message header.
 *
 * @returns true if the flag is set false it is clear.
 * @param   pThis       The IPC message.
 * @param   fFlag       The flag (or combination) to check.
 */
DECLINLINE(bool) IPCMsgIsFlagSet(PCIPCMSG pThis, uint16_t fFlag)
{
    return RT_BOOL(pThis->pMsgHdr->u16Flags & fFlag);
}


/**
 * Sets the given flag in the message header.
 *
 * @param   pThis       The IPC message.
 * @param   fFlag       The flag to set.
 */
DECLINLINE(void) IPCMsgSetFlag(PIPCMSG pThis, uint16_t fFlag)
{
    pThis->pMsgHdr->u16Flags |= fFlag;
}


/**
 * Init worker for a given message frame.
 *
 * @param   pThis       The message to initialize.
 * @param   target      the target for the message.
 * @param   cbTotal     Size of the payload in bytes.
 * @param   paSegs      Pointer to the segment array making up the message payload.
 * @param   cSegs       Number of segments in the array.
 */
DECLINLINE(void) ipcMsgInitWorker(PIPCMSG pThis, const nsID &target, size_t cbTotal, PCRTSGSEG paSegs, uint32_t cSegs)
{
    pThis->pMsgHdr             = (PIPCMSGHDR)pThis->pbBuf;
    pThis->pMsgHdr->cbMsg      = sizeof(*pThis->pMsgHdr) + cbTotal;
    pThis->pMsgHdr->u16Version = IPC_MSG_HDR_VERSION;
    pThis->pMsgHdr->u16Flags   = 0;
    pThis->pMsgHdr->idTarget   = target;

    uint8_t *pb = (uint8_t *)(pThis->pMsgHdr + 1);
    for (uint32_t i = 0; i < cSegs; i++)
    {
        memcpy(pb, paSegs[i].pvSeg, paSegs[i].cbSeg);
        pb += paSegs[i].cbSeg;
    }

    pThis->fReadonly = true;
}


/**
 * Allocates a new message with the given target and content.
 *
 * @returns Pointer to the allocated message structure or NULL if out of memory.
 * @param   target      The target for the message.
 * @param   cbTotal     Size of the payload in bytes.
 * @param   paSegs      Pointer to the segment array making up the message payload.
 * @param   cSegs       Number of segments in the array.
 */
DECLINLINE(PIPCMSG) IPCMsgNewSg(const nsID &target, size_t cbTotal, PCRTSGSEG paSegs, uint32_t cSegs)
{
    PIPCMSG pThis = IPCMsgAlloc(cbTotal);
    if (RT_LIKELY(pThis))
        ipcMsgInitWorker(pThis, target, cbTotal, paSegs, cSegs);

    return pThis;
}


/**
 * Initializes a given message frame, growing the data buffer if necessary.
 *
 * @param   pThis       The message to initialize.
 * @param   target      the target for the message.
 * @param   cbTotal     Size of the payload in bytes.
 * @param   paSegs      Pointer to the segment array making up the message payload.
 * @param   cSegs       Number of segments in the array.
 */
DECLINLINE(int) IPCMsgInitSg(PIPCMSG pThis, const nsID &target, size_t cbTotal, PCRTSGSEG paSegs, uint32_t cSegs)
{
    uint32_t cbMsg = sizeof(*pThis->pMsgHdr) + cbTotal;
    if (pThis->cbBuf < cbMsg)
    {
        uint8_t *pbBufNew = (uint8_t *)RTMemRealloc(pThis->pbBuf, cbMsg);
        if (!pbBufNew)
            return VERR_NO_MEMORY;

        pThis->pbBuf = pbBufNew;
        pThis->cbBuf = cbMsg;
    }

    ipcMsgInitWorker(pThis, target, cbTotal, paSegs, cSegs);
    return VINF_SUCCESS;
}


/**
 * Reads data for the given message from the given buffer.
 *
 * @returns IPRT status code.
 * @param   pThis       The message to read into.
 * @param   pvData      The data buffer holding message data.
 * @param   cbData      Size of the data buffer in bytes.
 * @param   pcbRead     Where to store the number of bytes read from the data buffer.
 * @param   pfDone      Where to store the flag whether the message is complete and can be parsed after returning from this call.
 */
DECLINLINE(int) IPCMsgReadFrom(PIPCMSG pThis, const void *pvData, size_t cbData, size_t *pcbRead, bool *pfDone)
{
    size_t cbHdrRead = 0;
    if (!pThis->pMsgHdr)
    {
        /* No full header received yet. */
        Assert(pThis->cbBuf >= sizeof(*pThis->pMsgHdr));

        cbHdrRead = RT_MIN(cbData, sizeof(*pThis->pMsgHdr) - pThis->offAppend);
        memcpy(pThis->pbBuf + pThis->offAppend, pvData, cbHdrRead);

        pThis->offAppend += cbHdrRead;
        Assert(pThis->offAppend <= sizeof(*pThis->pMsgHdr));
        if (pThis->offAppend == sizeof(*pThis->pMsgHdr))
        {
            /* Complete header received. */
            pThis->pMsgHdr = (PIPCMSGHDR)pThis->pbBuf;

            if (pThis->pMsgHdr->cbMsg > pThis->cbBuf)
            {
                Assert(pThis->pMsgHdr->cbMsg < 10 * _1M);

                uint8_t *pbBufNew = (uint8_t *)RTMemRealloc(pThis->pbBuf, pThis->pMsgHdr->cbMsg);
                if (!pbBufNew)
                    return VERR_NO_MEMORY;

                pThis->pbBuf   = pbBufNew;
                pThis->pMsgHdr = (PIPCMSGHDR)pbBufNew;
                pThis->cbBuf   = pThis->pMsgHdr->cbMsg;
            }

            /* fall through below after adjusting the buffer pointers. */
            pvData = (const uint8_t *)pvData + cbHdrRead;
            cbData -= cbHdrRead;
        }
        else
        {
            *pfDone  = false;
            *pcbRead = cbHdrRead;
            return VINF_SUCCESS;
        }
    }

    Assert(pThis->pMsgHdr);
    Assert(pThis->pMsgHdr->cbMsg <= pThis->cbBuf);

    /* We know the size of the message because a full message header was received already. */
    size_t cbLeft = pThis->pMsgHdr->cbMsg - pThis->offAppend;
    cbData = RT_MIN(cbData, cbLeft);
    Assert(pThis->offAppend + cbData <= pThis->cbBuf);

    if (cbData == cbLeft)
        *pfDone = true;
    else
        *pfDone = false;

    memcpy(pThis->pbBuf + pThis->offAppend, pvData, cbData);
    pThis->offAppend += cbData;
    *pcbRead          = cbData + cbHdrRead;
    return VINF_SUCCESS;
}

#endif // !ipcMessageNew_h__
