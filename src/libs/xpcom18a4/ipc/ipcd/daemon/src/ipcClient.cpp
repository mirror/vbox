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
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/poll.h>
#include <VBox/log.h>

#include "ipcClient.h"
#include "ipcMessage.h"
#include "ipcd.h"
#include "ipcm.h"

PRUint32 ipcClient::gLastID = 0;

//
// called to initialize this client context
//
// assumptions:
//  - object's memory has already been zero'd out.
//
void
ipcClient::Init(uint32_t idPoll, RTSOCKET hSock)
{
    mID = ++gLastID;

    // every client must be able to handle IPCM messages.
    mTargets.Append(IPCM_TARGET);

    m_hSock  = hSock;
    m_idPoll = idPoll;
    m_fUsed  = true;

    // although it is tempting to fire off the NotifyClientUp event at this
    // time, we must wait until the client sends us a CLIENT_HELLO event.
    // see ipcCommandModule::OnClientHello.
}

//
// called when this client context is going away
//
void
ipcClient::Finalize()
{
    RTSocketClose(m_hSock);
    m_hSock = NIL_RTSOCKET;

    IPC_NotifyClientDown(this);

    mNames.DeleteAll();
    mTargets.DeleteAll();

    mInMsg.Reset();
    mOutMsgQ.DeleteAll();
    m_fUsed = false;
}

void
ipcClient::AddName(const char *name)
{
    LogFlowFunc(("adding client name: %s\n", name));

    if (HasName(name))
        return;

    mNames.Append(name);
}

PRBool
ipcClient::DelName(const char *name)
{
    LogFlowFunc(("deleting client name: %s\n", name));

    return mNames.FindAndDelete(name);
}

void
ipcClient::AddTarget(const nsID &target)
{
    LogFlowFunc(("adding client target\n"));

    if (HasTarget(target))
        return;

    mTargets.Append(target);
}

PRBool
ipcClient::DelTarget(const nsID &target)
{
    LogFlowFunc(("deleting client target\n"));

    //
    // cannot remove the IPCM target
    //
    if (!target.Equals(IPCM_TARGET))
        return mTargets.FindAndDelete(target);

    return PR_FALSE;
}

//
// called to process a client socket
//
// params:
//   poll_flags - the state of the client socket
//
// return:
//   0             - to end session with this client
//   PR_POLL_READ  - to wait for the client socket to become readable
//   PR_POLL_WRITE - to wait for the client socket to become writable
//
int
ipcClient::Process(uint32_t inFlags)
{
    if (inFlags & RTPOLL_EVT_ERROR)
    {
        LogFlowFunc(("client socket appears to have closed\n"));
        return 0;
    }

    // expect to wait for more data
    int outFlags = RTPOLL_EVT_READ;

    if (inFlags & RTPOLL_EVT_READ) {
        LogFlowFunc(("client socket is now readable\n"));

        char buf[_1K];
        size_t cbRead = 0;
        int vrc = RTSocketReadNB(m_hSock, &buf[0], sizeof(buf), &cbRead);
        Assert(vrc != VINF_TRY_AGAIN);

        if (RT_FAILURE(vrc) || cbRead == 0)
            return 0; // cancel connection

        const char *ptr = buf;
        while (cbRead)
        {
            PRUint32 nread;
            PRBool complete;

            if (mInMsg.ReadFrom(ptr, PRUint32(cbRead), &nread, &complete) == PR_FAILURE) {
                LogFlowFunc(("message appears to be malformed; dropping client connection\n"));
                return 0;
            }

            if (complete)
            {
                IPC_DispatchMsg(this, &mInMsg);
                mInMsg.Reset();
            }

            cbRead -= nread;
            ptr    += nread;
        }
    }

    if (inFlags & RTPOLL_EVT_WRITE) {
        LogFlowFunc(("client socket is now writable\n"));

        if (mOutMsgQ.First())
            WriteMsgs();
    }

    if (mOutMsgQ.First())
        outFlags |= RTPOLL_EVT_WRITE;

    return outFlags;
}

//
// called to write out any messages from the outgoing queue.
//
int
ipcClient::WriteMsgs()
{
    while (mOutMsgQ.First())
    {
        const char *buf = (const char *) mOutMsgQ.First()->MsgBuf();
        PRInt32 bufLen = (PRInt32) mOutMsgQ.First()->MsgLen();

        if (mSendOffset)
        {
            buf += mSendOffset;
            bufLen -= mSendOffset;
        }

        size_t cbWritten = 0;
        int vrc = RTSocketWriteNB(m_hSock, buf, bufLen, &cbWritten);
        if (vrc == VINF_SUCCESS)
        { /* likely */ Assert(cbWritten > 0); }
        else
        {
            Assert(   RT_FAILURE(vrc)
                   || (vrc == VINF_TRY_AGAIN && cbWritten == 0));
            break;
        }

        LogFlowFunc(("wrote %d bytes\n", cbWritten));

        if (cbWritten == bufLen)
        {
            mOutMsgQ.DeleteFirst();
            mSendOffset = 0;
        }
        else
            mSendOffset += cbWritten;
    }

    return 0;
}

