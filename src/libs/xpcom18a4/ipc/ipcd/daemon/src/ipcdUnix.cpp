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
#include <iprt/initterm.h>
#include <iprt/getopt.h>
#include <iprt/message.h>
#include <iprt/poll.h>
#include <iprt/socket.h>
#include <iprt/string.h>
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

#include "plstr.h"
#include "prprf.h"

#include "ipcConfig.h"
#include "ipcMessage.h"
#include "ipcClient.h"
#include "ipcdPrivate.h"
#include "ipcd.h"

//-----------------------------------------------------------------------------
// ipc directory and locking...
//-----------------------------------------------------------------------------

//
// advisory file locking is used to ensure that only one IPC daemon is active
// and bound to the local domain socket at a time.
//
// XXX this code does not work on OS/2.
//
#if !defined(XP_OS2)
#define IPC_USE_FILE_LOCK
#endif

#ifdef IPC_USE_FILE_LOCK

enum Status
{
    EOk = 0,
    ELockFileOpen = -1,
    ELockFileLock = -2,
    ELockFileOwner = -3,
};

static int ipcLockFD = 0;

static Status AcquireDaemonLock(const char *baseDir)
{
    const char lockName[] = "lock";

    int dirLen = strlen(baseDir);
    int len = dirLen            // baseDir
            + 1                 // "/"
            + sizeof(lockName); // "lock"

    char *lockFile = (char *) malloc(len);
    memcpy(lockFile, baseDir, dirLen);
    lockFile[dirLen] = '/';
    memcpy(lockFile + dirLen + 1, lockName, sizeof(lockName));

#ifdef VBOX
    //
    // Security checks for the directory
    //
    struct stat st;
    if (stat(baseDir, &st) == -1)
    {
        printf("Cannot stat '%s'.\n", baseDir);
        return ELockFileOwner;
    }

    if (st.st_uid != getuid() && st.st_uid != geteuid())
    {
        printf("Wrong owner (%d) of '%s'", st.st_uid, baseDir);
        if (   !stat("/tmp", &st)
            && (st.st_mode & 07777) != 01777)
            printf(" -- check /tmp permissions (%o should be 1777)\n",
                    st.st_mode & 07777);
        printf(".\n");
        return ELockFileOwner;
    }

    if (st.st_mode != (S_IRUSR | S_IWUSR | S_IXUSR | S_IFDIR))
    {
        printf("Wrong mode (%o) of '%s'", st.st_mode, baseDir);
        if (   !stat("/tmp", &st)
            && (st.st_mode & 07777) != 01777)
            printf(" -- check /tmp permissions (%o should be 1777)\n",
                    st.st_mode & 07777);
        printf(".\n");
        return ELockFileOwner;
    }
#endif

    //
    // open lock file.  it remains open until we shutdown.
    //
    ipcLockFD = open(lockFile, O_WRONLY|O_CREAT, S_IWUSR|S_IRUSR);

#ifndef VBOX
    free(lockFile);
#endif

    if (ipcLockFD == -1)
        return ELockFileOpen;

#ifdef VBOX
    //
    // Security checks for the lock file
    //
    if (fstat(ipcLockFD, &st) == -1)
    {
        printf("Cannot stat '%s'.\n", lockFile);
        free(lockFile);
        return ELockFileOwner;
    }

    if (st.st_uid != getuid() && st.st_uid != geteuid())
    {
        printf("Wrong owner (%d) of '%s'.\n", st.st_uid, lockFile);
        free(lockFile);
        return ELockFileOwner;
    }

    if (st.st_mode != (S_IRUSR | S_IWUSR | S_IFREG))
    {
        printf("Wrong mode (%o) of '%s'.\n", st.st_mode, lockFile);
        free(lockFile);
        return ELockFileOwner;
    }

    free(lockFile);
#endif

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
    if (fcntl(ipcLockFD, F_SETLK, &lock) == -1)
        return ELockFileLock;

    //
    // truncate lock file once we have exclusive access to it.
    //
    ftruncate(ipcLockFD, 0);

    //
    // write our PID into the lock file (this just seems like a good idea...
    // no real purpose otherwise).
    //
    char buf[32];
    int nb = PR_snprintf(buf, sizeof(buf), "%u\n", (unsigned long) getpid());
    write(ipcLockFD, buf, nb);

    return EOk;
}

static Status InitDaemonDir(const char *socketPath)
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
    Status status = AcquireDaemonLock(baseDir);

    RTStrFree(baseDir);

    if (status == EOk) {
        // delete an existing socket to prevent bind from failing.
        unlink(socketPath);
    }
    return status;
}

static void ShutdownDaemonDir()
{
    LogFlowFunc(("ShutdownDaemonDir\n"));

    // deleting directory and files underneath it allows another process
    // to think it has exclusive access.  better to just leave the hidden
    // directory in /tmp and let the OS clean it up via the usual tmpdir
    // cleanup cron job.

    // this removes the advisory lock, allowing other processes to acquire it.
    if (ipcLockFD) {
        close(ipcLockFD);
        ipcLockFD = 0;
    }
}

#endif // IPC_USE_FILE_LOCK

//-----------------------------------------------------------------------------
// poll list
//-----------------------------------------------------------------------------

//
// declared in ipcdPrivate.h
//
ipcClient *ipcClients = NULL;
int        ipcClientCount = 0;

//
// the first element of this array is always zero; this is done so that the
// k'th element of ipcClientArray corresponds to the k'th element of
// ipcPollList.
//
static ipcClient ipcClientArray[IPC_MAX_CLIENTS];

static RTPOLLSET g_hPollSet = NIL_RTPOLLSET;

//-----------------------------------------------------------------------------

static int AddClient(RTPOLLSET hPollSet, RTSOCKET hSock)
{
    if (ipcClientCount == IPC_MAX_CLIENTS) {
        LogFlowFunc(("reached maximum client limit\n"));
        return -1;
    }

    /* Find an unused client entry. */
    for (uint32_t i = 0; i < RT_ELEMENTS(ipcClientArray); i++)
    {
        if (!ipcClientArray[i].m_fUsed)
        {
            ipcClientArray[i].Init(i, hSock);
            ipcClientArray[i].m_fPollEvts = RTPOLL_EVT_READ;
            int vrc = RTPollSetAddSocket(hPollSet, hSock, RTPOLL_EVT_READ, i);
            if (RT_SUCCESS(vrc))
            {
                ipcClientCount++;
                return 0;
            }

            ipcClientArray[i].Finalize();
            break;
        }
    }

    /* Failed to find or set up the IPC client state. */
    return -1;
}

static int RemoveClient(RTPOLLSET hPollSet, uint32_t idClient)
{
    int vrc = RTPollSetRemove(hPollSet, idClient);
    AssertRC(vrc); RT_NOREF(vrc);

    ipcClientArray[idClient].Finalize();
    --ipcClientCount;
    return 0;
}

//-----------------------------------------------------------------------------

static void PollLoop(RTPOLLSET hPollSet, int fdListen)
{
    ipcClients = ipcClientArray;

    for (;;)
    {
        LogFlowFunc(("Polling [ipcClientCount=%d]\n", ipcClientCount));
        uint32_t idPoll = 0;
        uint32_t fEvents = 0;
        int vrc = RTPoll(hPollSet, 5 * RT_MS_1MIN, &fEvents, &idPoll);
        if (RT_SUCCESS(vrc))
        { /* likely */ }
        else if (vrc == VERR_TIMEOUT)
        {
            /* Shutdown if no clients. */
            if (ipcClientCount == 0)
            {
                LogFlowFunc(("shutting down\n"));
                break;
            }

            continue;
        }
        else
        {
            LogFlowFunc(("Polling failed with %Rrc\n", vrc));
            break;
        }

        if (idPoll == UINT32_MAX - 1)
        {
            Assert(fEvents & RTPOLL_EVT_READ);
            LogFlowFunc(("Got new connection\n"));

            int fdClient = accept(fdListen, NULL, NULL);
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
                    if (AddClient(hPollSet, hSock) != 0)
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
            uint32_t fNewFlags = ipcClientArray[idPoll].Process(fEvents);
            if (!fNewFlags)
            {
                /* Cleanup dead client. */
                RemoveClient(hPollSet, idPoll);
            }
            else if (ipcClientArray[idPoll].m_fPollEvts != fNewFlags)
            {
                /* Change flags. */
                vrc = RTPollSetEventsChange(hPollSet, idPoll, fNewFlags);
                AssertRC(vrc);
                ipcClientArray[idPoll].m_fPollEvts = fNewFlags;
            }
        }
    }
}

//-----------------------------------------------------------------------------

PRStatus
IPC_PlatformSendMsg(ipcClient  *client, ipcMessage *msg)
{
    LogFlowFunc(("IPC_PlatformSendMsg\n"));

    //
    // must copy message onto send queue.
    //
    client->EnqueueOutboundMsg(msg);

    //
    // since our Process method may have already been called, we must ensure
    // that the RTPOLL_EVT_WRITE flag is set.
    //
    if (!(client->m_fPollEvts & RTPOLL_EVT_WRITE))
    {
        client->m_fPollEvts |= RTPOLL_EVT_WRITE;
        int vrc = RTPollSetEventsChange(g_hPollSet, client->m_idPoll, client->m_fPollEvts);
        AssertRC(vrc);
    }

    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------

int main(int argc, char **argv)
{
    /* Set up the runtime without loading the support driver. */
    int vrc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(vrc))
        return RTMsgInitFailure(vrc);

    /*
     * Parse the command line.
     */
    static RTGETOPTDEF const s_aOptions[] =
    {
        { "--inherit-startup-pipe", 'f', RTGETOPT_REQ_UINT32 },
        { "--socket-path",          'p', RTGETOPT_REQ_STRING },
    };

    RTGETOPTSTATE State;
    vrc = RTGetOptInit(&State, argc, argv, &s_aOptions[0], RT_ELEMENTS(s_aOptions), 1,  RTGETOPTINIT_FLAGS_OPTS_FIRST);
    if (RT_FAILURE(vrc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTGetOptInit failed: %Rrc", vrc);

    uint32_t        uStartupPipeFd = UINT32_MAX;
    const char      *pszSocketPath = NULL;
    RTGETOPTUNION   ValueUnion;
    int             chOpt;
    while ((chOpt = RTGetOpt(&State, &ValueUnion)) != 0)
    {
        switch (chOpt)
        {
            case 'f':
                uStartupPipeFd = ValueUnion.u32;
                break;
            case 'p':
                pszSocketPath = ValueUnion.psz;
                break;
            default:
                return RTGetOptPrintError(chOpt, &ValueUnion);
        }
    }

    //
    // ignore SIGINT so <ctrl-c> from terminal only kills the client
    // which spawned this daemon.
    //
    signal(SIGINT, SIG_IGN);
    // XXX block others?  check cartman

    // ensure strict file permissions
    umask(0077);

    LogFlowFunc(("daemon started...\n"));

    //XXX uncomment these lines to test slow starting daemon
    //IPC_Sleep(2);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    // set socket address
    if (!pszSocketPath)
        IPC_GetDefaultSocketPath(addr.sun_path, sizeof(addr.sun_path));
    else
        PL_strncpyz(addr.sun_path, pszSocketPath, sizeof(addr.sun_path));

#ifdef IPC_USE_FILE_LOCK
    Status status = InitDaemonDir(addr.sun_path);
    if (status != EOk) {
        if (status == ELockFileLock) {
            LogFlowFunc(("Another daemon is already running, exiting.\n"));
            // send a signal to the blocked parent to indicate success
            IPC_NotifyParent(uStartupPipeFd);
            return 0;
        }
        else {
            LogFlowFunc(("InitDaemonDir failed (status=%d)\n", status));
            // don't notify the parent to cause it to fail in PR_Read() after
            // we terminate
            if (status != ELockFileOwner)
                printf("Cannot create a lock file for '%s'.\n"
                        "Check permissions.\n", addr.sun_path);
            return 0;
        }
    }
#endif

    int fdListen = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fdListen == -1)
        LogFlowFunc(("socket failed [%d]\n", errno));
    else
    {
        if (bind(fdListen, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            // Use large backlog, as otherwise local sockets can reject connection
            // attempts. Usually harmless, but causes an unnecessary start attempt
            // of IPCD (which will terminate straight away), and the next attempt
            // usually succeeds. But better avoid unnecessary activities.
            if (listen(fdListen, 128) == 0)
            {
                IPC_NotifyParent(uStartupPipeFd);

#if defined(VBOX) && !defined(XP_OS2)
                // Increase the file table size to 10240 or as high as possible.
                struct rlimit lim;
                if (getrlimit(RLIMIT_NOFILE, &lim) == 0)
                {
                    if (    lim.rlim_cur < 10240
                        &&  lim.rlim_cur < lim.rlim_max)
                    {
                        lim.rlim_cur = lim.rlim_max <= 10240 ? lim.rlim_max : 10240;
                        if (setrlimit(RLIMIT_NOFILE, &lim) == -1)
                            printf("WARNING: failed to increase file descriptor limit. (%d)\n", errno);
                    }
                }
                else
                    printf("WARNING: failed to obtain per-process file-descriptor limit (%d).\n", errno);
#endif

                RTSOCKET hSockListen;
                int vrc = RTSocketFromNative(&hSockListen, fdListen);
                if (RT_SUCCESS(vrc))
                {
                    fdListen = (int)RTSocketToNative(hSockListen);

                    RTPOLLSET hPollSet = NIL_RTPOLLSET;
                    vrc = RTPollSetCreate(&hPollSet);
                    if (RT_SUCCESS(vrc))
                    {
                        g_hPollSet = hPollSet;

                        vrc = RTPollSetAddSocket(hPollSet, hSockListen, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, UINT32_MAX - 1);
                        if (RT_SUCCESS(vrc))
                            PollLoop(hPollSet, fdListen);
                        else
                            LogFlowFunc(("RTPollSetCreate() -> %Rrc\n", vrc));
                    }
                    else
                        LogFlowFunc(("RTPollSetCreate() -> %Rrc\n", vrc));

                    vrc = RTPollSetDestroy(hPollSet);
                    AssertRC(vrc);
                }
                else
                    LogFlowFunc(("RTSocketFromNative() -> %Rrc\n", vrc));

                vrc = RTSocketClose(hSockListen);
                AssertRC(vrc);
                fdListen = -1;
            }
            else
                LogFlowFunc(("listen failed [%d]\n", errno));
        }
        else
            LogFlowFunc(("bind failed [%d]\n", errno));

        LogFlowFunc(("closing socket\n"));
        if (fdListen != -1)
            close(fdListen);
    }

    //IPC_Sleep(5);

#ifdef IPC_USE_FILE_LOCK
    // it is critical that we release the lock before closing the socket,
    // otherwise, a client might launch another daemon that would be unable
    // to acquire the lock and would then leave the client without a daemon.

    ShutdownDaemonDir();
#endif

    return 0;
}
