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

#include <stdlib.h>
#include <string.h>
#include "ipcMessage.h"

#include <iprt/assert.h>
#include <iprt/mem.h>


ipcMessage::~ipcMessage()
{
    if (m_pvBuf)
        RTMemFree(m_pvBuf);

    mMsgHdr    = NULL;
    m_pvBuf    = NULL;
    m_cbBufMax = 0;
}

void
ipcMessage::Reset()
{
    mMsgHdr      = NULL;
    mMsgOffset   = 0;
    mMsgComplete = PR_FALSE;

    /* For large messages we don't want to keep the memory around forever. */
    if (m_cbBufMax > _16K)
    {
        RTMemFree(m_pvBuf);
        m_pvBuf    = NULL;
        m_cbBufMax = 0;
    }
}

ipcMessage *
ipcMessage::Clone() const
{
    ipcMessage *clone = new ipcMessage();
    if (!clone)
        return NULL;

    // copy buf if non-null
    if (mMsgHdr) {
        clone->mMsgHdr    = (ipcMessageHeader *) RTMemDup(mMsgHdr, mMsgHdr->mLen);
        clone->m_pvBuf    = clone->mMsgHdr;
        clone->m_cbBufMax = mMsgHdr->mLen;
    }
    else
        clone->mMsgHdr = NULL;

    clone->mMsgOffset = mMsgOffset;
    clone->mMsgComplete = mMsgComplete;

    return clone;
}

PRStatus
ipcMessage::Init(const nsID &target, const char *data, PRUint32 dataLen)
{
    mMsgComplete = PR_FALSE;

    // allocate message data
    PRUint32 msgLen = IPC_MSG_HEADER_SIZE + dataLen;
    if (m_cbBufMax < msgLen)
    {
        void *pvNew = RTMemRealloc(m_pvBuf, msgLen);
        if (!pvNew)
            return PR_FAILURE;

        m_pvBuf    = pvNew;
        m_cbBufMax = msgLen;
    }

    mMsgHdr = (ipcMessageHeader *)m_pvBuf;

    // fill in message data
    mMsgHdr->mLen = msgLen;
    mMsgHdr->mVersion = IPC_MSG_VERSION;
    mMsgHdr->mFlags = 0;
    mMsgHdr->mTarget = target;

    if (data)
        SetData(0, data, dataLen);

    mMsgComplete = PR_TRUE;
    return PR_SUCCESS;
}

PRStatus
ipcMessage::SetData(PRUint32 offset, const char *data, PRUint32 dataLen)
{
    Assert(mMsgHdr != NULL);

    if (offset + dataLen > DataLen())
        return PR_FAILURE;

    memcpy((char *) Data() + offset, data, dataLen);
    return PR_SUCCESS;
}

PRBool
ipcMessage::Equals(const nsID &target, const char *data, PRUint32 dataLen) const
{
    return mMsgComplete &&
           mMsgHdr->mTarget.Equals(target) &&
           DataLen() == dataLen &&
           memcmp(Data(), data, dataLen) == 0;
}

PRBool
ipcMessage::Equals(const ipcMessage *msg) const
{
    PRUint32 msgLen = MsgLen();
    return mMsgComplete && msg->mMsgComplete &&
           msgLen == msg->MsgLen() &&
           memcmp(MsgBuf(), msg->MsgBuf(), msgLen) == 0;
}

PRStatus
ipcMessage::WriteTo(char     *buf,
                    PRUint32  bufLen,
                    PRUint32 *bytesWritten,
                    PRBool   *complete)
{
    if (!mMsgComplete)
        return PR_FAILURE;

    if (mMsgOffset == MsgLen()) {
        *bytesWritten = 0;
        *complete = PR_TRUE;
        return PR_SUCCESS;
    }

    PRUint32 count = MsgLen() - mMsgOffset;
    if (count > bufLen)
        count = bufLen;

    memcpy(buf, MsgBuf() + mMsgOffset, count);
    mMsgOffset += count;

    *bytesWritten = count;
    *complete = (mMsgOffset == MsgLen());

    return PR_SUCCESS;
}

PRStatus
ipcMessage::ReadFrom(const char *buf,
                     PRUint32    bufLen,
                     PRUint32   *bytesRead,
                     PRBool     *complete)
{
    *bytesRead = 0;

    if (mMsgComplete) {
        *complete = PR_TRUE;
        return PR_SUCCESS;
    }

    if (mMsgHdr) {
        // appending data to buffer
        if (mMsgOffset < sizeof(PRUint32)) {
            // we haven't learned the message length yet
            if (mMsgOffset + bufLen < sizeof(PRUint32)) {
                // we still don't know the length of the message!
                memcpy((char *) mMsgHdr + mMsgOffset, buf, bufLen);
                mMsgOffset += bufLen;
                *bytesRead = bufLen;
                *complete = PR_FALSE;
                return PR_SUCCESS;
            }
            else {
                // we now have enough data to determine the message length
                PRUint32 count = sizeof(PRUint32) - mMsgOffset;
                memcpy((char *) MsgBuf() + mMsgOffset, buf, count);
                mMsgOffset += count;
                buf += count;
                bufLen -= count;
                *bytesRead = count;

                if (MsgLen() > m_cbBufMax) {
                    // realloc message buffer to the correct size
                    void *pvNew = RTMemRealloc(m_pvBuf, MsgLen());
                    if (!pvNew)
                        return PR_FAILURE;
                    m_pvBuf    = pvNew;
                    m_cbBufMax = MsgLen();
                    mMsgHdr = (ipcMessageHeader *)m_pvBuf;
                }
            }
        }
    }
    else {
        if (bufLen < sizeof(PRUint32)) {
            // not enough data available in buffer to determine allocation size
            // allocate a partial buffer
            PRUint32 msgLen = IPC_MSG_GUESSED_SIZE;
            if (msgLen > m_cbBufMax)
            {
                void *pvNew = RTMemRealloc(m_pvBuf, msgLen);
                if (!pvNew)
                    return PR_FAILURE;
                m_pvBuf    = pvNew;
                m_cbBufMax = msgLen;
                mMsgHdr = (ipcMessageHeader *)m_pvBuf;
            }

            memcpy(mMsgHdr, buf, bufLen);
            mMsgOffset = bufLen;
            *bytesRead = bufLen;
            *complete = PR_FALSE;
            return PR_SUCCESS;
        }
        else {
            PRUint32 msgLen = *(PRUint32 *) buf;
            if (msgLen > m_cbBufMax)
            {
                void *pvNew = RTMemRealloc(m_pvBuf, msgLen);
                if (!pvNew)
                    return PR_FAILURE;
                m_pvBuf    = pvNew;
                m_cbBufMax = msgLen;
            }
            mMsgHdr = (ipcMessageHeader *)m_pvBuf;
            mMsgHdr->mLen = msgLen;
            mMsgOffset = 0;
        }
    }

    // have mMsgHdr at this point

    PRUint32 count = MsgLen() - mMsgOffset;
    if (count > bufLen)
        count = bufLen;

    memcpy((char *) mMsgHdr + mMsgOffset, buf, count);
    mMsgOffset += count;
    *bytesRead += count;

    *complete = mMsgComplete = (mMsgOffset == MsgLen());
    return PR_SUCCESS;
}
