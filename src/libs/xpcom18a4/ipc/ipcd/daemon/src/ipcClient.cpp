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
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/poll.h>
#include <VBox/log.h>

#include "ipcClient.h"
#include "ipcd.h"
#include "ipcm.h"

static volatile uint32_t g_idClientLast = 0;


//
// called to initialize this client context
//
// assumptions:
//  - object's memory has already been zero'd out.
//
DECLHIDDEN(int) ipcdClientInit(PIPCDCLIENT pThis, PIPCDSTATE pIpcd, uint32_t idPoll, RTSOCKET hSock)
{
    pThis->idClient = ASMAtomicIncU32(&g_idClientLast);

    // every client must be able to handle IPCM messages.
    pThis->mTargets.Append(IPCM_TARGET);

    pThis->pIpcd     = pIpcd;
    pThis->hSock     = hSock;
    pThis->idPoll    = idPoll;
    pThis->fPollEvts = RTPOLL_EVT_READ;
    pThis->fUsed     = true;

    RTListInit(&pThis->LstMsgsOut);

    // although it is tempting to fire off the NotifyClientUp event at this
    // time, we must wait until the client sends us a CLIENT_HELLO event.
    // see ipcCommandModule::OnClientHello.

    return IPCMsgInit(&pThis->MsgIn, 0 /*cbBuf*/);
}

//
// called when this client context is going away
//
DECLHIDDEN(void) ipcdClientDestroy(PIPCDCLIENT pThis)
{
    RTSocketClose(pThis->hSock);
    pThis->hSock = NIL_RTSOCKET;

    IPC_NotifyClientDown(pThis);

    pThis->mNames.DeleteAll();
    pThis->mTargets.DeleteAll();

    IPCMsgFree(&pThis->MsgIn, false /*fFreeStruct*/);
    pThis->fUsed = false;

    /** @todo Free all outgoing messages. */
}


DECLHIDDEN(void) ipcdClientAddName(PIPCDCLIENT pThis, const char *pszName)
{
    LogFlowFunc(("adding client name: %s\n", pszName));

    if (ipcdClientHasName(pThis, pszName))
        return;

    pThis->mNames.Append(pszName);
}


DECLHIDDEN(bool) ipcdClientDelName(PIPCDCLIENT pThis, const char *pszName)
{
    LogFlowFunc(("deleting client name: %s\n", pszName));

    return pThis->mNames.FindAndDelete(pszName);
}


DECLHIDDEN(void) ipcdClientAddTarget(PIPCDCLIENT pThis, const nsID *target)
{
    LogFlowFunc(("adding client target\n"));

    if (ipcdClientHasTarget(pThis, target))
        return;

    pThis->mTargets.Append(*target);
}


DECLHIDDEN(bool) ipcdClientDelTarget(PIPCDCLIENT pThis, const nsID *target)
{
    LogFlowFunc(("deleting client target\n"));

    //
    // cannot remove the IPCM target
    //
    if (!target->Equals(IPCM_TARGET))
        return pThis->mTargets.FindAndDelete(*target);

    return false;
}

//
// called to write out any messages from the outgoing queue.
//
static int ipcdClientWriteMsgs(PIPCDCLIENT pThis)
{
    while (!RTListIsEmpty(&pThis->LstMsgsOut))
    {
        PIPCMSG pMsg         = RTListGetFirst(&pThis->LstMsgsOut, IPCMSG, NdMsg);
        const uint8_t *pbBuf = (const uint8_t *)IPCMsgGetBuf(pMsg);
        size_t cbBuf         = IPCMsgGetSize(pMsg);

        if (pThis->offMsgOutBuf)
        {
            Assert(cbBuf > pThis->offMsgOutBuf);
            pbBuf += pThis->offMsgOutBuf;
            cbBuf -= pThis->offMsgOutBuf;
        }

        size_t cbWritten = 0;
        int vrc = RTSocketWriteNB(pThis->hSock, pbBuf, cbBuf, &cbWritten);
        if (vrc == VINF_SUCCESS)
        { /* likely */ Assert(cbWritten > 0); }
        else
        {
            Assert(   RT_FAILURE(vrc)
                   || (vrc == VINF_TRY_AGAIN && cbWritten == 0));
            break;
        }

        LogFlowFunc(("wrote %d bytes\n", cbWritten));

        if (cbWritten == cbBuf)
        {
            RTListNodeRemove(&pMsg->NdMsg);
            IPC_PutMsgIntoCache(pThis->pIpcd, pMsg);
            pThis->offMsgOutBuf = 0;
        }
        else
            pThis->offMsgOutBuf += cbWritten;
    }

    return 0;
}

DECLHIDDEN(uint32_t) ipcdClientProcess(PIPCDCLIENT pThis, uint32_t fPollFlags)
{
    if (fPollFlags & RTPOLL_EVT_ERROR)
    {
        LogFlowFunc(("client socket appears to have closed\n"));
        return 0;
    }

    uint32_t fOutFlags = RTPOLL_EVT_READ;

    if (fPollFlags & RTPOLL_EVT_READ) {
        LogFlowFunc(("client socket is now readable\n"));

        uint8_t abBuf[_1K];
        size_t cbRead = 0;
        int vrc = RTSocketReadNB(pThis->hSock, &abBuf[0], sizeof(abBuf), &cbRead);
        Assert(vrc != VINF_TRY_AGAIN);

        if (RT_FAILURE(vrc) || cbRead == 0)
            return 0; // cancel connection

        const uint8_t *pb = abBuf;
        while (cbRead)
        {
            size_t cbProcessed;
            bool fDone;

            int vrc = IPCMsgReadFrom(&pThis->MsgIn, pb, cbRead, &cbProcessed, &fDone);
            if (RT_FAILURE(vrc))
            {
                LogFlowFunc(("message appears to be malformed; dropping client connection\n"));
                return 0;
            }

            if (fDone)
            {
                IPC_DispatchMsg(pThis, &pThis->MsgIn);
                IPCMsgReset(&pThis->MsgIn);
            }

            cbRead -= cbProcessed;
            pb     += cbProcessed;
        }
    }

    if (fPollFlags & RTPOLL_EVT_WRITE)
    {
        LogFlowFunc(("client socket is now writable\n"));

        if (!RTListIsEmpty(&pThis->LstMsgsOut))
            ipcdClientWriteMsgs(pThis);
    }

    if (!RTListIsEmpty(&pThis->LstMsgsOut))
        fOutFlags |= RTPOLL_EVT_WRITE;

    return fOutFlags;
}
