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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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
#include <iprt/critsect.h>
#include <iprt/err.h>
#include <iprt/env.h>
#include <iprt/mem.h>
#include <iprt/pipe.h>
#include <iprt/poll.h>
#include <iprt/socket.h>
#include <iprt/string.h>
#include <iprt/thread.h>

#include <VBox/log.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <stdio.h> /* fprintf below */

#include "ipcConnection.h"
#include "ipcMessageQ.h"
#include "ipcConfig.h"


static PRStatus
DoSecurityCheck(int fd, const char *path)
{
    //
    // now that we have a connected socket; do some security checks on the
    // file descriptor.
    //
    //   (1) make sure owner matches
    //   (2) make sure permissions match expected permissions
    //
    // if these conditions aren't met then bail.
    //
    struct stat st;
    if (fstat(fd, &st) == -1)
    {
        LogFlowFunc(("stat failed"));
        return PR_FAILURE;
    }

    if (st.st_uid != getuid() && st.st_uid != geteuid())
    {
        //
        // on OSX 10.1.5, |fstat| has a bug when passed a file descriptor to
        // a socket.  it incorrectly returns a UID of 0.  however, |stat|
        // succeeds, but using |stat| introduces a race condition.
        //
        // XXX come up with a better security check.
        //
        if (st.st_uid != 0) {
            LogFlowFunc(("userid check failed"));
            return PR_FAILURE;
        }
        if (stat(path, &st) == -1) {
            LogFlowFunc(("stat failed"));
            return PR_FAILURE;
        }
        if (st.st_uid != getuid() && st.st_uid != geteuid()) {
            LogFlowFunc(("userid check failed"));
            return PR_FAILURE;
        }
    }

    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------

/* Character written for wakeup. */
static const char magicChar = '\x38';

struct ipcCallback : public ipcListNode<ipcCallback>
{
    ipcCallbackFunc  func;
    void            *arg;
};

typedef ipcList<ipcCallback> ipcCallbackQ;

//-----------------------------------------------------------------------------

struct ipcConnectionState
{
    RTCRITSECT   CritSect;
    RTPIPE       hWakeupPipeW;
    RTPIPE       hWakeupPipeR;
    RTSOCKET     hSockConn;
    RTPOLLSET    hPollSet;
    ipcCallbackQ callback_queue;
    ipcMessageQ  send_queue;
    PRUint32     send_offset; // amount of send_queue.First() already written.
    ipcMessage  *in_msg;
    bool         fShutdown;
};

#define SOCK 0
#define POLL 1

static void ConnDestroy(ipcConnectionState *s)
{
    RTCritSectDelete(&s->CritSect);
    RTPollSetDestroy(s->hPollSet);
    RTPipeClose(s->hWakeupPipeR);
    RTPipeClose(s->hWakeupPipeW);
    RTSocketClose(s->hSockConn);

    if (s->in_msg)
        delete s->in_msg;

    s->send_queue.DeleteAll();
    RTMemFree(s);
}

static ipcConnectionState *ConnCreate(RTSOCKET hSockConn)
{
    ipcConnectionState *s = (ipcConnectionState *)RTMemAllocZ(sizeof(*s));
    if (!s)
      return NULL;

    s->send_offset = 0;
    s->in_msg = NULL;
    s->fShutdown = false;

    int vrc = RTCritSectInit(&s->CritSect);
    if (RT_SUCCESS(vrc))
    {
        vrc = RTPollSetCreate(&s->hPollSet);
        if (RT_SUCCESS(vrc))
        {
            RTPIPE hPipeR;

            vrc = RTPipeCreate(&s->hWakeupPipeR, &s->hWakeupPipeW, 0 /*fFlags*/);
            if (RT_SUCCESS(vrc))
            {
                vrc = RTPollSetAddSocket(s->hPollSet, hSockConn, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, SOCK);
                if (RT_SUCCESS(vrc))
                {
                    vrc = RTPollSetAddPipe(s->hPollSet, s->hWakeupPipeR, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, POLL);
                    if (RT_SUCCESS(vrc))
                    {
                        vrc = RTSocketSetInheritance(hSockConn, false /*fInheritable*/);
                        if (RT_SUCCESS(vrc))
                        {
                            s->hSockConn = hSockConn;
                            return s;
                        }

                        LogFlowFunc(("coudn't make IPC socket non-inheritable [err=%Rrc]\n", vrc));
                        vrc = RTPollSetRemove(s->hPollSet, POLL); AssertRC(vrc);
                    }

                    vrc = RTPollSetRemove(s->hPollSet, SOCK); AssertRC(vrc);
                }

                vrc = RTPipeClose(s->hWakeupPipeR); AssertRC(vrc);
                vrc = RTPipeClose(s->hWakeupPipeW); AssertRC(vrc);
            }

            vrc = RTPollSetDestroy(s->hPollSet); AssertRC(vrc);
        }

        RTCritSectDelete(&s->CritSect);
    }

    return NULL;
}

static nsresult ConnRead(ipcConnectionState *s)
{
    nsresult rv = NS_OK;

    /* Read as much as possible. */
    for (;;)
    {
        char buf[_1K];
        size_t cbRead = 0;
        int vrc = RTSocketReadNB(s->hSockConn, &buf[0], sizeof(buf), &cbRead);
        if (RT_SUCCESS(vrc) && cbRead > 0)
        { /* likely */ }
        else if (vrc == VINF_TRY_AGAIN)
            break; /* Socket is empty, go back to polling. */
        else if (RT_FAILURE(vrc))
        {
            rv = NS_ERROR_UNEXPECTED;
            break;
        }
        else if (cbRead == 0)
        {
            LogFlowFunc(("RTSocketReadNB() returned EOF\n"));
            rv = NS_ERROR_UNEXPECTED;
            break;
        }

        const char *pdata = buf;
        while (cbRead)
        {
            PRUint32 bytesRead;
            PRBool fComplete = PR_FALSE;

            /* No message frame available? Allocate a new one. */
            if (!s->in_msg)
            {
                s->in_msg = new ipcMessage;
                if (RT_UNLIKELY(!s->in_msg))
                {
                    rv = NS_ERROR_OUT_OF_MEMORY;
                    break;
                }
            }

            if (s->in_msg->ReadFrom(pdata, cbRead, &bytesRead, &fComplete) != PR_SUCCESS)
            {
                LogFlowFunc(("error reading IPC message\n"));
                rv = NS_ERROR_UNEXPECTED;
                break;
            }

            Assert(cbRead >= bytesRead);
            cbRead -= bytesRead;
            pdata  += bytesRead;

            if (fComplete)
            {
                // protect against weird re-entrancy cases...
                ipcMessage *m = s->in_msg;
                s->in_msg = NULL;

                IPC_OnMessageAvailable(m);
            }
        }
    }

    return rv;
}

static nsresult ConnWrite(ipcConnectionState *s)
{
    nsresult rv = NS_OK;

    RTCritSectEnter(&s->CritSect);

    // write one message and then return.
    if (s->send_queue.First())
    {
        size_t cbWritten = 0;
        int vrc = RTSocketWriteNB(s->hSockConn,
                                  s->send_queue.First()->MsgBuf() + s->send_offset,
                                  s->send_queue.First()->MsgLen() - s->send_offset,
                                  &cbWritten);
        if (vrc == VINF_SUCCESS && cbWritten)
        {
            s->send_offset += cbWritten;
            if (s->send_offset == s->send_queue.First()->MsgLen())
            {
                s->send_queue.DeleteFirst();
                s->send_offset = 0;

                /* if the send queue is empty, then we need to stop trying to write. */
                if (s->send_queue.IsEmpty())
                {
                    vrc = RTPollSetEventsChange(s->hPollSet, SOCK, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR);
                    AssertRC(vrc);
                }
            }
        }
        else if (vrc != VINF_TRY_AGAIN)
        {
            Assert(RT_FAILURE(vrc));
            LogFlowFunc(("error writing to socket [err=%Rrc]\n", vrc));
            rv = NS_ERROR_UNEXPECTED;
        }
    }

    RTCritSectLeave(&s->CritSect);
    return rv;
}

static DECLCALLBACK(int) ipcConnThread(RTTHREAD hSelf, void *pvArg)
{
    RT_NOREF(hSelf);

    nsresult rv = NS_OK;
    ipcConnectionState *s = (ipcConnectionState *)pvArg;

    // we monitor two file descriptors in this thread.  the first (at index 0) is
    // the socket connection with the IPC daemon.  the second (at index 1) is the
    // pollable event we monitor in order to know when to send messages to the
    // IPC daemon.

    while (NS_SUCCEEDED(rv))
    {
        //
        // poll on the IPC socket and NSPR pollable event
        //
        uint32_t fEvents;
        uint32_t idPoll;
        int vrc = RTPoll(s->hPollSet, RT_INDEFINITE_WAIT, &fEvents, &idPoll);
        if (RT_SUCCESS(vrc))
        {
            if (idPoll == SOCK)
            {
                /* check if we can read... */
                if (fEvents & RTPOLL_EVT_READ)
                    rv = ConnRead(s);

                /* check if we can write... */
                if (fEvents & RTPOLL_EVT_WRITE)
                    rv = ConnWrite(s);
            }
            else
            {
                Assert(   idPoll == POLL
                       && (fEvents & RTPOLL_EVT_READ)
                       && !(fEvents & (RTPOLL_EVT_WRITE | RTPOLL_EVT_ERROR)));

                uint8_t buf[32];
                ipcCallbackQ cbs_to_run;

                // check if something has been added to the send queue.  if so, then
                // acknowledge pollable event (wait should not block), and configure
                // poll flags to find out when we can write.

                /* Drain wakeup pipe. */
                size_t cbRead = 0;
                vrc = RTPipeRead(s->hWakeupPipeR, &buf[0], sizeof(buf), &cbRead);
                Assert(RT_SUCCESS(vrc) && cbRead > 0);

#ifdef DEBUG
                /* Make sure this is not written with something else. */
                for (uint32_t i = 0; i < cbRead; i++)
                    Assert(buf[i] == magicChar);
#endif

                RTCritSectEnter(&s->CritSect);

                if (!s->send_queue.IsEmpty())
                {
                    vrc = RTPollSetEventsChange(s->hPollSet, SOCK, RTPOLL_EVT_READ | RTPOLL_EVT_WRITE | RTPOLL_EVT_ERROR);
                    AssertRC(vrc);
                }

                if (!s->callback_queue.IsEmpty())
                    s->callback_queue.MoveTo(cbs_to_run);

                // check if we should exit this thread.  delay processing a shutdown
                // request until after all queued up messages have been sent and until
                // after all queued up callbacks have been run.
                if (s->fShutdown && s->send_queue.IsEmpty() && s->callback_queue.IsEmpty())
                    rv = NS_ERROR_ABORT;

                RTCritSectLeave(&s->CritSect);

                // check if we have callbacks to run
                while (!cbs_to_run.IsEmpty())
                {
                    ipcCallback *cb = cbs_to_run.First();
                    (cb->func)(cb->arg);
                    cbs_to_run.DeleteFirst();
                }
            }
        }
        else
        {
            LogFlowFunc(("RTPoll returned error %Rrc\n", vrc));
            rv = NS_ERROR_UNEXPECTED;
        }
    }

    // notify termination of the IPC connection
    if (rv == NS_ERROR_ABORT)
        rv = NS_OK;
    IPC_OnConnectionEnd(rv);

    LogFlowFunc(("IPC thread exiting\n"));
    return VINF_SUCCESS;
}

//-----------------------------------------------------------------------------
// IPC connection API
//-----------------------------------------------------------------------------

static ipcConnectionState *gConnState = NULL;
static RTTHREAD gConnThread = NULL;

#ifdef DEBUG
static RTTHREAD gMainThread = NULL;
#endif

static nsresult TryConnect(RTSOCKET *phSocket)
{
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    IPC_GetDefaultSocketPath(addr.sun_path, sizeof(addr.sun_path) - 1);

    // don't use NS_ERROR_FAILURE as we want to detect these kind of errors
    // in the frontend
    nsresult rv = NS_ERROR_SOCKET_FAIL;

    int fdSock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fdSock != -1)
    {
        /* Connect to the local socket. */
        if (connect(fdSock, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            if (RTEnvExist("TESTBOX_UUID"))
                fprintf(stderr, "IPC socket path: %s\n", addr.sun_path);
            LogRel(("IPC socket path: %s\n", addr.sun_path));

            // do some security checks on connection socket...
            if (DoSecurityCheck(fdSock, addr.sun_path) == PR_SUCCESS)
            {
                int vrc = RTSocketFromNative(phSocket, fdSock);
                if (RT_SUCCESS(vrc))
                    return NS_OK;
            }
        }

        close(fdSock);
    }

    return rv;
}

nsresult IPC_Connect(const char *daemonPath)
{
  // synchronous connect, spawn daemon if necessary.
    nsresult rv = NS_ERROR_FAILURE;

    if (gConnState)
        return NS_ERROR_ALREADY_INITIALIZED;

    //
    // here's the connection algorithm:  try to connect to an existing daemon.
    // if the connection fails, then spawn the daemon (wait for it to be ready),
    // and then retry the connection.  it is critical that the socket used to
    // connect to the daemon not be inherited (this causes problems on RH9 at
    // least).
    //

    RTSOCKET hSockConn = NIL_RTSOCKET;
    rv = TryConnect(&hSockConn);
    if (NS_FAILED(rv))
    {
        nsresult rv1 = IPC_SpawnDaemon(daemonPath);
        if (NS_SUCCEEDED(rv1) || rv != NS_ERROR_SOCKET_FAIL)
          rv = rv1; 
        if (NS_SUCCEEDED(rv))
          rv = TryConnect(&hSockConn);
    }

    if (NS_SUCCEEDED(rv))
    {
        //
        // ok, we have a connection to the daemon!
        //

      // build connection state object
        gConnState = ConnCreate(hSockConn);
        if (RT_LIKELY(gConnState))
        {
            hSockConn = NIL_RTSOCKET; // connection state now owns the socket

            int vrc = RTThreadCreate(&gConnThread, ipcConnThread, gConnState, 0 /*cbStack*/,
                                     RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "Ipc-Conn");
            if (RT_SUCCESS(vrc))
            {
#ifdef DEBUG
                gMainThread = RTThreadSelf();
#endif
                return NS_OK;
            }
            else
                rv = NS_ERROR_OUT_OF_MEMORY;
        }
        else
          rv = NS_ERROR_OUT_OF_MEMORY;
    }

    if (gConnState)
    {
        ConnDestroy(gConnState);
        gConnState = NULL;
    }

    if (hSockConn != NIL_RTSOCKET)
        RTSocketClose(hSockConn);

    return rv;
}

nsresult IPC_Disconnect()
{
    // Must disconnect on same thread used to connect!
#ifdef DEBUG
    Assert(gMainThread == RTThreadSelf());
#endif

    if (!gConnState || !gConnThread)
      return NS_ERROR_NOT_INITIALIZED;

    RTCritSectEnter(&gConnState->CritSect);
    gConnState->fShutdown = true;
    size_t cbWrittenIgn = 0;
    int vrc = RTPipeWrite(gConnState->hWakeupPipeW, &magicChar, sizeof(magicChar), &cbWrittenIgn);
    AssertRC(vrc);
    RTCritSectLeave(&gConnState->CritSect);

    int rcThread;
    RTThreadWait(gConnThread, RT_INDEFINITE_WAIT, &rcThread);
    AssertRC(rcThread);

    ConnDestroy(gConnState);

    gConnState = NULL;
    gConnThread = NULL;
    return NS_OK;
}

nsresult IPC_SendMsg(ipcMessage *msg)
{
    if (!gConnState || !gConnThread)
      return NS_ERROR_NOT_INITIALIZED;

    RTCritSectEnter(&gConnState->CritSect);
    gConnState->send_queue.Append(msg);
    size_t cbWrittenIgn = 0;
    int vrc = RTPipeWrite(gConnState->hWakeupPipeW, &magicChar, sizeof(magicChar), &cbWrittenIgn);
    AssertRC(vrc);
    RTCritSectLeave(&gConnState->CritSect);

    return NS_OK;
}

nsresult IPC_DoCallback(ipcCallbackFunc func, void *arg)
{
    if (!gConnState || !gConnThread)
      return NS_ERROR_NOT_INITIALIZED;
    
    ipcCallback *callback = new ipcCallback;
    if (!callback)
      return NS_ERROR_OUT_OF_MEMORY;
    callback->func = func;
    callback->arg = arg;

    RTCritSectEnter(&gConnState->CritSect);
    gConnState->callback_queue.Append(callback);
    size_t cbWrittenIgn = 0;
    int vrc = RTPipeWrite(gConnState->hWakeupPipeW, &magicChar, sizeof(magicChar), &cbWrittenIgn);
    AssertRC(vrc);
    RTCritSectLeave(&gConnState->CritSect);
    return NS_OK;
}

