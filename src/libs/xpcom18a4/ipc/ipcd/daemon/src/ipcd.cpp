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
#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/getopt.h>
#include <iprt/message.h>
#include <iprt/poll.h>
#include <iprt/socket.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <VBox/log.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "ipcConfig.h"
#include "ipcMessageNew.h"
#include "ipcClient.h"
#include "ipcCommandModule.h"
#include "ipcd.h"
#include "ipcm.h"


//
// upper limit on the number of active connections
// XXX may want to make this more dynamic
//
#define IPC_MAX_CLIENTS 100

/** How many messages should stay in the message cache. */
#define IPC_MAX_MSGS_IN_CACHE 5

/**
 * The IPC daemon state
 */
typedef struct IPCDSTATE
{
    RTSOCKET    hSockListen;
    int         fdListen;
    int         ipcLockFD;
    int         ipcClientCount;

    IPCDCLIENT  ipcClientArray[IPC_MAX_CLIENTS];
    RTLISTANCHOR LstIpcClients;

    RTLISTANCHOR LstIpcMsgsFree;
    uint32_t     cIpcMsgsFree;

    RTPOLLSET   hPollSet;
    RTTHREAD    hThread;
} IPCDSTATE;
typedef IPCDSTATE *PIPCDSTATE;
typedef const IPCDSTATE *PCIPCDSTATE;


//-----------------------------------------------------------------------------
// ipc directory and locking...
//-----------------------------------------------------------------------------

enum Status
{
    EOk = 0,
    ELockFileOpen = -1,
    ELockFileLock = -2,
    ELockFileOwner = -3,
};

static Status AcquireDaemonLock(PIPCDSTATE pThis, const char *baseDir)
{
    const char lockName[] = "lock";

    int dirLen = strlen(baseDir);
    int len = dirLen            // baseDir
            + 1                 // "/"
            + sizeof(lockName); // "lock"

    //
    // Security checks for the directory
    //
    struct stat st;
    if (stat(baseDir, &st) == -1)
    {
        LogFlowFunc(("Cannot stat '%s'.\n", baseDir));
        return ELockFileOwner;
    }

    if (st.st_uid != getuid() && st.st_uid != geteuid())
    {
        LogFlowFunc(("Wrong owner (%d) of '%s'", st.st_uid, baseDir));
        if (   !stat("/tmp", &st)
            && (st.st_mode & 07777) != 01777)
            LogFlowFunc((" -- check /tmp permissions (%o should be 1777)\n",
                         st.st_mode & 07777));
        LogFlowFunc((".\n"));
        return ELockFileOwner;
    }

    if (st.st_mode != (S_IRUSR | S_IWUSR | S_IXUSR | S_IFDIR))
    {
        LogFlowFunc(("Wrong mode (%o) of '%s'", st.st_mode, baseDir));
        if (   !stat("/tmp", &st)
            && (st.st_mode & 07777) != 01777)
            LogFlowFunc((" -- check /tmp permissions (%o should be 1777)\n",
                    st.st_mode & 07777));
        LogFlowFunc((".\n"));
        return ELockFileOwner;
    }

    char *lockFile = (char *) malloc(len);
    memcpy(lockFile, baseDir, dirLen);
    lockFile[dirLen] = '/';
    memcpy(lockFile + dirLen + 1, lockName, sizeof(lockName));

    //
    // open lock file.  it remains open until we shutdown.
    //
    pThis->ipcLockFD = open(lockFile, O_WRONLY|O_CREAT, S_IWUSR|S_IRUSR);
    if (pThis->ipcLockFD == -1)
    {
        free(lockFile);
        return ELockFileOpen;
    }

    //
    // Security checks for the lock file
    //
    if (fstat(pThis->ipcLockFD, &st) == -1)
    {
        LogFlowFunc(("Cannot stat '%s'.\n", lockFile));
        free(lockFile);
        return ELockFileOwner;
    }

    if (st.st_uid != getuid() && st.st_uid != geteuid())
    {
        LogFlowFunc(("Wrong owner (%d) of '%s'.\n", st.st_uid, lockFile));
        free(lockFile);
        return ELockFileOwner;
    }

    if (st.st_mode != (S_IRUSR | S_IWUSR | S_IFREG))
    {
        LogFlowFunc(("Wrong mode (%o) of '%s'.\n", st.st_mode, lockFile));
        free(lockFile);
        return ELockFileOwner;
    }

    free(lockFile);

    //
    // we use fcntl for locking.  assumption: filesystem should be local.
    // this API is nice because the lock will be automatically released
    // when the process dies.  it will also be released when the file
    // descriptor is closed.
    //
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_whence = SEEK_SET;
    if (fcntl(pThis->ipcLockFD, F_SETLK, &lock) == -1)
    {
        LogFlowFunc(("Setting lock failed -> %Rrc.\n", RTErrConvertFromErrno(errno)));
        return ELockFileLock;
    }

    //
    // truncate lock file once we have exclusive access to it.
    //
    ftruncate(pThis->ipcLockFD, 0);

    //
    // write our PID into the lock file (this just seems like a good idea...
    // no real purpose otherwise).
    //
    char buf[32];
    ssize_t nb = RTStrPrintf2(buf, sizeof(buf), "%u\n", (unsigned long) getpid());
    if (nb <= 0)
        return ELockFileOpen;
    write(pThis->ipcLockFD, buf, (size_t)nb);

    return EOk;
}

static Status InitDaemonDir(PIPCDSTATE pThis, const char *socketPath)
{
    LogFlowFunc(("InitDaemonDir [sock=%s]\n", socketPath));

    char *baseDir = RTStrDup(socketPath);

    //
    // make sure IPC directory exists (XXX this should be recursive)
    //
    char *p = strrchr(baseDir, '/');
    if (p)
        p[0] = '\0';
    mkdir(baseDir, 0700);

    //
    // if we can't acquire the daemon lock, then another daemon
    // must be active, so bail.
    //
    Status status = AcquireDaemonLock(pThis, baseDir);

    RTStrFree(baseDir);

    if (status == EOk) {
        // delete an existing socket to prevent bind from failing.
        unlink(socketPath);
    }
    return status;
}


//-----------------------------------------------------------------------------

static int AddClient(PIPCDSTATE pThis, RTSOCKET hSock)
{
    if (pThis->ipcClientCount == IPC_MAX_CLIENTS) {
        LogFlowFunc(("reached maximum client limit\n"));
        return -1;
    }

    /* Find an unused client entry. */
    for (uint32_t i = 0; i < RT_ELEMENTS(pThis->ipcClientArray); i++)
    {
        if (!pThis->ipcClientArray[i].fUsed)
        {
            int vrc = ipcdClientInit(&pThis->ipcClientArray[i], pThis, i, hSock);
            if (RT_SUCCESS(vrc))
                vrc = RTPollSetAddSocket(pThis->hPollSet, hSock, RTPOLL_EVT_READ, i);
            if (RT_SUCCESS(vrc))
            {
                RTListAppend(&pThis->LstIpcClients, &pThis->ipcClientArray[i].NdClients);
                pThis->ipcClientCount++;
                return 0;
            }

            ipcdClientDestroy(&pThis->ipcClientArray[i]);
            break;
        }
    }

    /* Failed to find or set up the IPC client state. */
    return -1;
}

static int RemoveClient(PIPCDSTATE pThis, uint32_t idClient)
{
    int vrc = RTPollSetRemove(pThis->hPollSet, idClient);
    AssertRC(vrc); RT_NOREF(vrc);

    RTListNodeRemove(&pThis->ipcClientArray[idClient].NdClients);
    ipcdClientDestroy(&pThis->ipcClientArray[idClient]);
    pThis->ipcClientCount--;
    return 0;
}

//-----------------------------------------------------------------------------

static void PollLoop(PIPCDSTATE pThis)
{
    for (;;)
    {
        LogFlowFunc(("Polling [ipcClientCount=%d]\n", pThis->ipcClientCount));
        uint32_t idPoll = 0;
        uint32_t fEvents = 0;
        int vrc = RTPoll(pThis->hPollSet, 5 * RT_MS_1MIN, &fEvents, &idPoll);
        if (RT_SUCCESS(vrc))
        { /* likely */ }
        else if (vrc == VERR_TIMEOUT)
            continue;
        else
        {
            LogFlowFunc(("Polling failed with %Rrc\n", vrc));
            break;
        }

        if (idPoll == UINT32_MAX - 1)
        {
            Assert(fEvents & RTPOLL_EVT_READ);
            LogFlowFunc(("Got new connection\n"));

            int fdClient = accept(pThis->fdListen, NULL, NULL);
            if (fdClient == -1)
            {
                /* ignore this error... perhaps the client disconnected. */
                LogFlowFunc(("accept() failed [%d]\n", errno));
            }
            else
            {
                RTSOCKET hSock;
                vrc = RTSocketFromNative(&hSock, fdClient);
                if (RT_SUCCESS(vrc))
                {
                    if (AddClient(pThis, hSock) != 0)
                        RTSocketClose(hSock);
                }
                else
                {
                    LogFlowFunc(("RTSocketFromNative(, %d) -> %Rrc\n", fdClient, vrc));
                    close(fdClient);
                }
            }
        }
        else
        {
            uint32_t fNewFlags = ipcdClientProcess(&pThis->ipcClientArray[idPoll], fEvents);
            if (!fNewFlags)
            {
                /* Cleanup dead client. */
                RemoveClient(pThis, idPoll);

                /* Shutdown if no clients. */
                if (pThis->ipcClientCount == 0)
                {
                    LogFlowFunc(("shutting down\n"));
                    break;
                }
            }
            else if (pThis->ipcClientArray[idPoll].fPollEvts != fNewFlags)
            {
                /* Change flags. */
                vrc = RTPollSetEventsChange(pThis->hPollSet, idPoll, fNewFlags);
                AssertRC(vrc);
                pThis->ipcClientArray[idPoll].fPollEvts = fNewFlags;
            }
        }
    }
}

//-----------------------------------------------------------------------------

static int ipcdPlatformSendMsg(PIPCDCLIENT pIpcClient, PIPCMSG pMsg)
{
    LogFlowFunc(("IPC_PlatformSendMsg\n"));

    PIPCDSTATE pThis = ipcdClientGetDaemonState(pIpcClient);

    ipcdClientEnqueueOutboundMsg(pIpcClient, pMsg);

    /*
     * Since our Process method may have already been called, we must ensure
     * that the RTPOLL_EVT_WRITE flag is set.
     */
    if (!(pIpcClient->fPollEvts & RTPOLL_EVT_WRITE))
    {
        pIpcClient->fPollEvts |= RTPOLL_EVT_WRITE;
        int vrc = RTPollSetEventsChange(pThis->hPollSet, pIpcClient->idPoll, pIpcClient->fPollEvts);
        AssertRC(vrc);
    }

    return VINF_SUCCESS;
}


//-----------------------------------------------------------------------------

DECLHIDDEN(int) IPC_DispatchMsg(PIPCDCLIENT pIpcClient, PCIPCMSG pMsg)
{
    AssertPtr(pIpcClient);
    AssertPtr(pMsg);

    // remember if client is expecting SYNC_REPLY.  we'll add that flag to the
    // next message sent to the client.
    if (IPCMsgIsFlagSet(pMsg, IPC_MSG_FLAG_SYNC_QUERY))
    {
        Assert(!ipcdClientGetExpectsSyncReply(pIpcClient));
        // XXX shouldn't we remember the TargetID as well, and only set the
        //     SYNC_REPLY flag on the next message sent to the same TargetID?
        ipcdClientSetExpectsSyncReply(pIpcClient, true);
    }

    if (IPCMsgGetTarget(pMsg)->Equals(IPCM_TARGET))
        IPCM_HandleMsg(pIpcClient, pMsg);

    return VINF_SUCCESS;
}


DECLHIDDEN(int) IPC_SendMsg(PIPCDCLIENT pIpcClient, PIPCMSG pMsg)
{
    Assert(pMsg);
    Assert(pIpcClient);

    // add SYNC_REPLY flag to message if client is expecting...
    if (ipcdClientGetExpectsSyncReply(pIpcClient)) {
        pMsg->pMsgHdr->u16Flags |= IPC_MSG_FLAG_SYNC_REPLY;
        ipcdClientSetExpectsSyncReply(pIpcClient, false);
    }

    if (ipcdClientHasTarget(pIpcClient, IPCMsgGetTarget(pMsg)))
        return ipcdPlatformSendMsg(pIpcClient, pMsg);

    Log(("  no registered message handler\n"));
    return VERR_NOT_FOUND;
}


DECLHIDDEN(void) IPC_NotifyClientUp(PIPCDCLIENT pIpcClient)
{
    Log(("IPC_NotifyClientUp: clientID=%d\n", ipcdClientGetId(pIpcClient)));

    PIPCDSTATE pThis = ipcdClientGetDaemonState(pIpcClient);
    PIPCDCLIENT pIt;
    RTListForEach(&pThis->LstIpcClients, pIt, IPCDCLIENT, NdClients)
    {
        if (pIt != pIpcClient)
        {
            uint32_t aResp[4] = { IPCM_MSG_PSH_CLIENT_STATE, 0, ipcdClientGetId(pIpcClient), IPCM_CLIENT_STATE_UP };
            IPC_SendMsg(pIt, IPCM_TARGET, &aResp[0], sizeof(aResp));
        }
    }
}


DECLHIDDEN(void) IPC_NotifyClientDown(PIPCDCLIENT pIpcClient)
{
    Log(("IPC_NotifyClientDown: clientID=%d\n", ipcdClientGetId(pIpcClient)));

    PIPCDSTATE pThis = ipcdClientGetDaemonState(pIpcClient);
    PIPCDCLIENT pIt;
    RTListForEach(&pThis->LstIpcClients, pIt, IPCDCLIENT, NdClients)
    {
        if (pIt != pIpcClient)
        {
            uint32_t aResp[4] = { IPCM_MSG_PSH_CLIENT_STATE, 0, ipcdClientGetId(pIpcClient), IPCM_CLIENT_STATE_DOWN };
            IPC_SendMsg(pIt, IPCM_TARGET, &aResp[0], sizeof(aResp));
        }
    }
}

//-----------------------------------------------------------------------------
// IPC daemon methods
//-----------------------------------------------------------------------------

DECLHIDDEN(int) IPC_SendMsg(PIPCDCLIENT pIpcClient, const nsID &target, const void *pvData, size_t cbData)
{
    RTSGSEG Seg = { (void *)pvData, cbData };
    return IPC_SendMsgSg(pIpcClient, target, cbData, &Seg, 1);
}


DECLHIDDEN(int) IPC_SendMsgSg(PIPCDCLIENT pIpcClient, const nsID &target, size_t cbTotal, PCRTSGSEG paSegs, uint32_t cSegs)
{
    PIPCDSTATE pThis = ipcdClientGetDaemonState(pIpcClient);
    PIPCMSG pMsg = NULL;

    if (!pThis->cIpcMsgsFree)
    {
        pMsg = IPCMsgNewSg(target, cbTotal, paSegs, cSegs);
        if (!pMsg)
            return VERR_NO_MEMORY;
    }
    else
    {
        pMsg = RTListRemoveFirst(&pThis->LstIpcMsgsFree, IPCMSG, NdMsg);
        AssertPtr(pMsg);
        pThis->cIpcMsgsFree--;

        int vrc = IPCMsgInitSg(pMsg, target, cbTotal, paSegs, cSegs);
        if (RT_FAILURE(vrc))
        {
            RTListAppend(&pThis->LstIpcMsgsFree, &pMsg->NdMsg);
            pThis->cIpcMsgsFree++;
            return vrc;
        }
    }

    return IPC_SendMsg(pIpcClient, pMsg);
}


DECLHIDDEN(void) IPC_PutMsgIntoCache(PIPCDSTATE pIpcd, PIPCMSG pMsg)
{
    if (pIpcd->cIpcMsgsFree < IPC_MAX_MSGS_IN_CACHE)
    { /*likely*/ }
    else
    {
        IPCMsgFree(pMsg, true /*fFreeStruct*/);
        return;
    }

    pIpcd->cIpcMsgsFree++;
    IPCMsgReset(pMsg);
    RTListAppend(&pIpcd->LstIpcMsgsFree, &pMsg->NdMsg);
}


DECLHIDDEN(PIPCDCLIENT) IPC_GetClientByID(PIPCDSTATE pIpcd, uint32_t idClient)
{
    // linear search OK since number of clients should be small
    PIPCDCLIENT pIt;
    RTListForEach(&pIpcd->LstIpcClients, pIt, IPCDCLIENT, NdClients)
    {
        if (ipcdClientGetId(pIt) == idClient)
            return pIt;
    }
    return NULL;
}


DECLHIDDEN(PIPCDCLIENT) IPC_GetClientByName(PIPCDSTATE pIpcd, const char *pszName)
{
    // linear search OK since number of clients should be small
    PIPCDCLIENT pIt;
    RTListForEach(&pIpcd->LstIpcClients, pIt, IPCDCLIENT, NdClients)
    {
        if (ipcdClientHasName(pIt, pszName))
            return pIt;
    }
    return NULL;
}


static int ipcdInit(PIPCDSTATE pThis, const char *pszSocketPath)
{
    RTListInit(&pThis->LstIpcClients);
    RTListInit(&pThis->LstIpcMsgsFree);
    pThis->cIpcMsgsFree   = 0;
    pThis->fdListen       = 0;
    pThis->hPollSet       = NIL_RTPOLLSET;
    pThis->hSockListen    = NIL_RTSOCKET;
    pThis->ipcLockFD      = 0;
    pThis->ipcClientCount = 0;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    // set socket address
    if (!pszSocketPath)
        IPC_GetDefaultSocketPath(addr.sun_path, sizeof(addr.sun_path));
    else
        RTStrCopy(addr.sun_path, sizeof(addr.sun_path), pszSocketPath);

    Status status = InitDaemonDir(pThis, addr.sun_path);
    if (status != EOk) {
        if (status == ELockFileLock)
            return VERR_ALREADY_EXISTS;
        else
        {
            LogFlowFunc(("InitDaemonDir failed (status=%d)\n", status));
            // don't notify the parent to cause it to fail in PR_Read() after
            // we terminate
            if (status != ELockFileOwner)
                printf("Cannot create a lock file for '%s'.\n"
                        "Check permissions.\n", addr.sun_path);
            return VERR_INVALID_PARAMETER;
        }
    }

    pThis->fdListen = socket(PF_UNIX, SOCK_STREAM, 0);
    if (pThis->fdListen == -1)
    {
        LogFlowFunc(("socket failed [%d]\n", errno));
        return RTErrConvertFromErrno(errno);
    }

    if (bind(pThis->fdListen, (struct sockaddr *)&addr, sizeof(addr)))
    {
        LogFlowFunc(("bind failed [%d]\n", errno));
        return RTErrConvertFromErrno(errno);
    }

    // Use large backlog, as otherwise local sockets can reject connection
    // attempts. Usually harmless, but causes an unnecessary start attempt
    // of IPCD (which will terminate straight away), and the next attempt
    // usually succeeds. But better avoid unnecessary activities.
    if (listen(pThis->fdListen, 128))
    {
        LogFlowFunc(("listen failed [%d]\n", errno));
        return RTErrConvertFromErrno(errno);
    }

    // Increase the file table size to 10240 or as high as possible.
    struct rlimit lim;
    if (getrlimit(RLIMIT_NOFILE, &lim) == 0)
    {
        if (    lim.rlim_cur < 10240
            &&  lim.rlim_cur < lim.rlim_max)
        {
            lim.rlim_cur = lim.rlim_max <= 10240 ? lim.rlim_max : 10240;
            if (setrlimit(RLIMIT_NOFILE, &lim) == -1)
                LogFlowFunc(("WARNING: failed to increase file descriptor limit. (%d)\n", errno));
        }
    }
    else
        LogFlowFunc(("WARNING: failed to obtain per-process file-descriptor limit (%d).\n", errno));

    int vrc = RTSocketFromNative(&pThis->hSockListen, pThis->fdListen);
    if (RT_FAILURE(vrc))
    {
        LogFlowFunc(("RTSocketFromNative() -> %Rrc\n", vrc));
        return vrc;
    }

    pThis->fdListen = (int)RTSocketToNative(pThis->hSockListen);

    vrc = RTPollSetCreate(&pThis->hPollSet);
    if (RT_FAILURE(vrc))
    {
        LogFlowFunc(("RTPollSetCreate() -> %Rrc\n", vrc));
        return vrc;
    }

    vrc = RTPollSetAddSocket(pThis->hPollSet, pThis->hSockListen, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, UINT32_MAX - 1);
    if (RT_FAILURE(vrc))
    {
        LogFlowFunc(("RTPollSetAddSocket() -> %Rrc\n", vrc));
        return vrc;
    }

    return VINF_SUCCESS;
}


static void ipcdTerm(PIPCDSTATE pThis)
{
    Assert(pThis->ipcClientCount == 0);

    // it is critical that we release the lock before closing the socket,
    // otherwise, a client might launch another daemon that would be unable
    // to acquire the lock and would then leave the client without a daemon.

    // deleting directory and files underneath it allows another process
    // to think it has exclusive access.  better to just leave the hidden
    // directory in /tmp and let the OS clean it up via the usual tmpdir
    // cleanup cron job.

    // this removes the advisory lock, allowing other processes to acquire it.
    if (pThis->ipcLockFD) {
        close(pThis->ipcLockFD);
        pThis->ipcLockFD = 0;
    }

    int vrc = VINF_SUCCESS;
    if (pThis->hPollSet != NIL_RTPOLLSET)
    {
        vrc = RTPollSetDestroy(pThis->hPollSet);
        AssertRC(vrc);
        pThis->hPollSet = NIL_RTPOLLSET;
    }

    if (pThis->hSockListen != NIL_RTSOCKET)
    {
        vrc = RTSocketClose(pThis->hSockListen);
        AssertRC(vrc);
        pThis->hSockListen = NIL_RTSOCKET;
    }

    /* Free all the cached messages. */
    PIPCMSG pIt, pItNext;
    RTListForEachSafe(&pThis->LstIpcMsgsFree, pIt, pItNext, IPCMSG, NdMsg)
    {
        RTListNodeRemove(&pIt->NdMsg);
        IPCMsgFree(pIt, true /*fFreeStruct*/);
    }
}


static DECLCALLBACK(int) ipcdThread(RTTHREAD hThreadSelf, void *pvUser)
{
    IPCDSTATE IpcdState;

    int vrc = ipcdInit(&IpcdState, NULL /*pszSocketPath*/);
    *(int *)pvUser = vrc; /* Set the startup status code. */
    RTThreadUserSignal(hThreadSelf);

    if (RT_SUCCESS(vrc))
        PollLoop(&IpcdState);

    ipcdTerm(&IpcdState);
    return VINF_SUCCESS;
}


DECL_EXPORT_NOTHROW(int) RTCALL VBoxXpcomIpcdCreate(PRTTHREAD phThrdIpcd)
{
    int vrcThrdStartup = VINF_SUCCESS;
    int vrc = RTThreadCreate(phThrdIpcd, ipcdThread, &vrcThrdStartup, 0 /*cbStack*/, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "IPCD-Msg");
    if (RT_FAILURE(vrc))
        return vrc;

    vrc = RTThreadUserWait(*phThrdIpcd, RT_MS_30SEC);
    AssertRCReturn(vrc, vrc);

    if (RT_FAILURE(vrcThrdStartup))
    {
        /* Wait for the thread to terminate. */
        vrc = RTThreadWait(*phThrdIpcd, RT_MS_30SEC, NULL /*prc*/);
        AssertRC(vrc);
        *phThrdIpcd = NIL_RTTHREAD;
        LogFlowFunc(("Creating daemon failed -> %Rrc\n", vrcThrdStartup));
        return vrcThrdStartup;
    }

    return VINF_SUCCESS;
}


DECL_EXPORT_NOTHROW(int) RTCALL VBoxXpcomIpcdDestroy(RTTHREAD hThrdIpcd)
{
    /* Normally just need to wait as this will get called when the last client has exited and VBoxSVC is shutting down. */
    int vrcThrd = VINF_SUCCESS;
    int vrc = RTThreadWait(hThrdIpcd, RT_MS_30SEC, &vrcThrd);
    if (RT_FAILURE(vrc))
        return vrc;

    return vrcThrd;
}
