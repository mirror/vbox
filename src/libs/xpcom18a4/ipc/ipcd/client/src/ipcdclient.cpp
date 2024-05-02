/* vim:set ts=2 sw=2 et cindent: */
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
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
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
#include "ipcdclient.h"
#include "ipcConnection.h"
#include "ipcConfig.h"
#include "ipcm.h"

#include "nsIFile.h"
#include "nsEventQueueUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "nsAutoLock.h"
#include "nsProxyRelease.h"
#include "nsCOMArray.h"

#include <iprt/asm.h>
#include <iprt/critsect.h>
#include <iprt/env.h>
#include <iprt/message.h>
#include <iprt/pipe.h>
#include <iprt/process.h>
#include <iprt/string.h>

#include <VBox/log.h>

/* ------------------------------------------------------------------------- */

#define IPC_REQUEST_TIMEOUT (30 * RT_MS_1SEC)

/* ------------------------------------------------------------------------- */

class ipcTargetData
{
public:
  static NS_HIDDEN_(ipcTargetData*) Create();

  // threadsafe addref/release
  NS_HIDDEN_(nsrefcnt) AddRef()  { return ASMAtomicIncU32(&refcnt); }
  NS_HIDDEN_(nsrefcnt) Release() { uint32_t r = ASMAtomicDecU32(&refcnt); if (r == 0) delete this; return r; }

  NS_HIDDEN_(void) SetObserver(ipcIMessageObserver *aObserver, PRBool aOnCurrentThread);

  // protects access to the members of this class
  PRMonitor *monitor;

  // this may be null
  nsCOMPtr<ipcIMessageObserver> observer;

  // the message observer is called via this event queue
  nsCOMPtr<nsIEventQueue> eventQ;

  // incoming messages are added to this list
  RTLISTANCHOR            LstPendingMsgs;

  // non-zero if the observer has been disabled (this means that new messages
  // should not be dispatched to the observer until the observer is re-enabled
  // via IPC_EnableMessageObserver).
  PRInt32 observerDisabled;

private:

  ipcTargetData()
    : monitor(nsAutoMonitor::NewMonitor("ipcTargetData"))
    , observerDisabled(0)
    , refcnt(0)
    {
        RTListInit(&LstPendingMsgs);
    }

  ~ipcTargetData()
  {
    if (monitor)
      nsAutoMonitor::DestroyMonitor(monitor);

    /* Free all the pending messages. */
    PIPCMSG pIt, pItNext;
    RTListForEachSafe(&LstPendingMsgs, pIt, pItNext, IPCMSG, NdMsg)
    {
        RTListNodeRemove(&pIt->NdMsg);
        IPCMsgFree(pIt, true /*fFreeStruct*/);
    }
  }

  volatile uint32_t refcnt;
};

ipcTargetData *
ipcTargetData::Create()
{
  ipcTargetData *td = new ipcTargetData;
  if (!td)
    return NULL;

  if (!td->monitor)
  {
    delete td;
    return NULL;
  }
  return td;
}

void
ipcTargetData::SetObserver(ipcIMessageObserver *aObserver, PRBool aOnCurrentThread)
{
  observer = aObserver;

  if (aOnCurrentThread)
    NS_GetCurrentEventQ(getter_AddRefs(eventQ));
  else
    eventQ = nsnull;
}

/* ------------------------------------------------------------------------- */

typedef nsRefPtrHashtable<nsIDHashKey, ipcTargetData> ipcTargetMap;

class ipcClientState
{
public:
  static NS_HIDDEN_(ipcClientState *) Create();

  ~ipcClientState()
  {
    RTCritSectRwDelete(&critSect);
    RTCritSectDelete(&CritSectCache);

    /* Free all the cached messages. */
    PIPCMSG pIt, pItNext;
    RTListForEachSafe(&LstMsgCache, pIt, pItNext, IPCMSG, NdMsg)
    {
        RTListNodeRemove(&pIt->NdMsg);
        IPCMsgFree(pIt, true /*fFreeStruct*/);
    }
  }

  RTCRITSECTRW  critSect;
  ipcTargetMap  targetMap;
  PRBool        connected;
  PRBool        shutdown;

  // our process's client id
  PRUint32      selfID;

  nsCOMArray<ipcIClientObserver> clientObservers;

  RTCRITSECT     CritSectCache;
  RTLISTANCHOR  LstMsgCache;
  uint32_t      cMsgsInCache;

private:

  ipcClientState()
    : connected(PR_FALSE)
    , shutdown(PR_FALSE)
    , selfID(0)
  {
    /* Not employing the lock validator here to keep performance up in debug builds. */
    RTCritSectRwInitEx(&critSect, RTCRITSECT_FLAGS_NO_LOCK_VAL, NIL_RTLOCKVALCLASS, RTLOCKVAL_SUB_CLASS_NONE, NULL);

    RTCritSectInit(&CritSectCache);
    RTListInit(&LstMsgCache);
    cMsgsInCache = 0;
  }
};

ipcClientState *
ipcClientState::Create()
{
  ipcClientState *cs = new ipcClientState;
  if (!cs)
    return NULL;

  if (!RTCritSectRwIsInitialized(&cs->critSect) || !cs->targetMap.Init())
  {
    delete cs;
    return NULL;
  }

  return cs;
}

/* ------------------------------------------------------------------------- */

static ipcClientState *gClientState;

DECLHIDDEN(void) IPC_MsgFree(PIPCMSG pMsg)
{
    if (pMsg->fStack)
        return;

    int vrc = RTCritSectTryEnter(&gClientState->CritSectCache);
    if (RT_SUCCESS(vrc))
    {
        if (gClientState->cMsgsInCache < 10)
        {
            RTListAppend(&gClientState->LstMsgCache, &pMsg->NdMsg);
            gClientState->cMsgsInCache++;
            RTCritSectLeave(&gClientState->CritSectCache);
            return;
        }

        RTCritSectLeave(&gClientState->CritSectCache);
    }

    IPCMsgFree(pMsg, true /*fFreeStruct*/);
}

DECLINLINE(PIPCMSG) ipcdMsgGetFromCache(void)
{
    PIPCMSG pMsg = NULL;

    if (gClientState->cMsgsInCache)
    {
        int vrc = RTCritSectTryEnter(&gClientState->CritSectCache);
        if (RT_SUCCESS(vrc))
        {
            if (gClientState->cMsgsInCache)
            {
                pMsg = RTListRemoveFirst(&gClientState->LstMsgCache, IPCMSG, NdMsg);
                gClientState->cMsgsInCache--;
            }

            RTCritSectLeave(&gClientState->CritSectCache);
        }
    }

    return pMsg;
}

static PIPCMSG IPC_MsgClone(PCIPCMSG pMsg)
{
    PIPCMSG pClone = ipcdMsgGetFromCache();
    if (pClone)
    {
        int vrc = IPCMsgCloneWithMsg(pMsg, pClone);
        if (RT_SUCCESS(vrc))
            return pClone;

        /* Don't bother putting the clone back into the cache. */
        IPCMsgFree(pClone, true /*fFreeStruct*/);
    }

    /* Allocate new */
    return IPCMsgClone(pMsg);
}

static PIPCMSG IPC_MsgNewSg(const nsID &target, size_t cbTotal, PCRTSGSEG paSegs, uint32_t cSegs)
{
    PIPCMSG pMsg = ipcdMsgGetFromCache();
    if (pMsg)
    {
        int vrc = IPCMsgInitSg(pMsg, target, cbTotal, paSegs, cSegs);
        if (RT_SUCCESS(vrc))
            return pMsg;
    }

    return IPCMsgNewSg(target, cbTotal, paSegs, cSegs);
}


static PRBool
GetTarget(const nsID &aTarget, ipcTargetData **td)
{
  RTCritSectRwEnterShared(&gClientState->critSect);
  PRBool fRc = gClientState->targetMap.Get(nsIDHashKey(&aTarget).GetKey(), td);
  RTCritSectRwLeaveShared(&gClientState->critSect);
  return fRc;
}

static PRBool
PutTarget(const nsID &aTarget, ipcTargetData *td)
{
  RTCritSectRwEnterExcl(&gClientState->critSect);
  PRBool fRc = gClientState->targetMap.Put(nsIDHashKey(&aTarget).GetKey(), td);
  RTCritSectRwLeaveExcl(&gClientState->critSect);
  return fRc;
}

static void
DelTarget(const nsID &aTarget)
{
  RTCritSectRwEnterExcl(&gClientState->critSect);
  gClientState->targetMap.Remove(nsIDHashKey(&aTarget).GetKey());
  RTCritSectRwLeaveExcl(&gClientState->critSect);
}

/* ------------------------------------------------------------------------- */

static nsresult
GetDaemonPath(nsCString &dpath)
{
  nsCOMPtr<nsIFile> file;

  nsresult rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR,
                                       getter_AddRefs(file));
  if (NS_SUCCEEDED(rv))
  {
    rv = file->AppendNative(NS_LITERAL_CSTRING(IPC_DAEMON_APP_NAME));
    if (NS_SUCCEEDED(rv))
      rv = file->GetNativePath(dpath);
  }

  return rv;
}

/* ------------------------------------------------------------------------- */

static void
ProcessPendingQ(const nsID &aTarget)
{
    RTLISTANCHOR LstPendingMsgs;
    RTListInit(&LstPendingMsgs);

    nsRefPtr<ipcTargetData> td;
    if (GetTarget(aTarget, getter_AddRefs(td)))
    {
        nsAutoMonitor mon(td->monitor);

        // if the observer for this target has been temporarily disabled, then
        // we must not processing any pending messages at this time.

        if (!td->observerDisabled)
            RTListMove(&LstPendingMsgs, &td->LstPendingMsgs);
    }

    // process pending queue outside monitor
    while (!RTListIsEmpty(&LstPendingMsgs))
    {
        PIPCMSG pMsg = RTListGetFirst(&LstPendingMsgs, IPCMSG, NdMsg);
        RTListNodeRemove(&pMsg->NdMsg);

        // it is possible that messages for other targets are in the queue
        // (currently, this can be only a IPCM_MSG_PSH_CLIENT_STATE message
        // initially addressed to IPCM_TARGET, see IPC_OnMessageAvailable())
        // --ignore them.
        if (td->observer && IPCMsgGetTarget(pMsg)->Equals(aTarget))
            td->observer->OnMessageAvailable(pMsg->upUser,
                                             *IPCMsgGetTarget(pMsg),
                                             (const PRUint8 *)IPCMsgGetPayload(pMsg),
                                             IPCMsgGetPayloadSize(pMsg));
        else
        {
          // the IPCM target does not have an observer, and therefore any IPCM
          // messages that make it here will simply be dropped.
          NS_ASSERTION(   aTarget.Equals(IPCM_TARGET)
                       || IPCMsgGetTarget(pMsg)->Equals(IPCM_TARGET),
                       "unexpected target");
          Log(("dropping IPCM message: type=%x\n", IPCM_GetType(pMsg)));
        }
        IPC_MsgFree(pMsg);
    }
}

/* ------------------------------------------------------------------------- */

// WaitTarget enables support for multiple threads blocking on the same
// message target.  the selector is called while inside the target's monitor.

typedef nsresult (* ipcMessageSelector)(void *arg, ipcTargetData *td, PCIPCMSG pMsg);

// selects any
static nsresult DefaultSelector(void *arg, ipcTargetData *td, PCIPCMSG pMsg)
{
  return NS_OK;
}

static nsresult
WaitTarget(const nsID           &aTarget,
           RTMSINTERVAL         aTimeout,
           PIPCMSG              *ppMsg,
           ipcMessageSelector    aSelector = nsnull,
           void                 *aArg = nsnull)
{
  *ppMsg = NULL;

  if (!aSelector)
    aSelector = DefaultSelector;

  nsRefPtr<ipcTargetData> td;
  if (!GetTarget(aTarget, getter_AddRefs(td)))
    return NS_ERROR_INVALID_ARG; // bad aTarget

  PRBool isIPCMTarget = aTarget.Equals(IPCM_TARGET);

  uint64_t timeStart = RTTimeProgramMilliTS();
  uint64_t timeEnd;
  if (aTimeout == RT_INDEFINITE_WAIT)
    timeEnd = UINT64_MAX;
  else if (aTimeout == 0)
    timeEnd = timeStart;
  else
  {
    timeEnd = timeStart + aTimeout;

    // if overflowed, then set to max value
    if (timeEnd < timeStart)
      timeEnd = UINT64_MAX;
  }

  nsresult rv = NS_ERROR_ABORT;

  nsAutoMonitor mon(td->monitor);

  // only the ICPM target is allowed to wait for a message after shutdown
  // (but before disconnection).  this gives client observers called from
  // IPC_Shutdown a chance to use IPC_SendMessage to send necessary
  // "last minute" messages to other clients.

  while (gClientState->connected && (!gClientState->shutdown || isIPCMTarget))
  {
    //
    // NOTE:
    //
    // we must start at the top of the pending queue, possibly revisiting
    // messages that our selector has already rejected.  this is necessary
    // because the queue may have been modified while we were waiting on
    // the monitor.  the impact of this on performance remains to be seen.
    //
    // one cheap solution is to keep a counter that is incremented each
    // time a message is removed from the pending queue.  that way we can
    // avoid revisiting all messages sometimes.
    //

    PIPCMSG pIt, pItNext;
    // loop over pending queue until we find a message that our selector likes.
    RTListForEachSafe(&td->LstPendingMsgs, pIt, pItNext, IPCMSG, NdMsg)
    {
      //
      // it is possible that this call to WaitTarget() has been initiated by
      // some other selector, that might be currently processing the same
      // message (since the message remains in the queue until the selector
      // returns TRUE).  here we prevent this situation by using a special flag
      // to guarantee that every message is processed only once.
      //
      if (!IPCMsgIsFlagSet(pIt, IPC_MSG_HDR_FLAG_IN_PROCESS))
      {
        IPCMsgSetFlag(pIt, IPC_MSG_HDR_FLAG_IN_PROCESS);
        nsresult acceptedRV = (aSelector)(aArg, td, pIt);
        IPCMsgClearFlag(pIt, IPC_MSG_HDR_FLAG_IN_PROCESS);

        if (acceptedRV != IPC_WAIT_NEXT_MESSAGE)
        {
          if (acceptedRV == NS_OK)
          {
            // remove from pending queue
            RTListNodeRemove(&pIt->NdMsg);
            *ppMsg = pIt;
            break;
          }
          else /* acceptedRV == IPC_DISCARD_MESSAGE */
          {
            RTListNodeRemove(&pIt->NdMsg);
            IPC_MsgFree(pIt);
            continue;
          }
        }
      }
    }

    if (*ppMsg)
    {
      rv = NS_OK;
      break;
    }
#ifdef VBOX
    else
    {
      /* Special client liveness check if there is no message to process.
       * This is necessary as there might be several threads waiting for
       * a message from a single client, and only one gets the DOWN msg. */
      nsresult aliveRV = (aSelector)(aArg, td, NULL);
      if (aliveRV != IPC_WAIT_NEXT_MESSAGE)
      {
        *ppMsg = NULL;
        break;
      }
    }
#endif /* VBOX */

    uint64_t t = RTTimeProgramMilliTS();
    if (   aTimeout != RT_INDEFINITE_WAIT
        && t > timeEnd) // check if timeout has expired
    {
      rv = IPC_ERROR_WOULD_BLOCK;
      break;
    }
    mon.Wait(  aTimeout == RT_INDEFINITE_WAIT
             ? RT_INDEFINITE_WAIT
             : timeEnd - t);

    Log(("woke up from sleep [pendingQempty=%d connected=%d shutdown=%d isIPCMTarget=%d]\n",
          RTListIsEmpty(&td->LstPendingMsgs), gClientState->connected,
          gClientState->shutdown, isIPCMTarget));
  }

  return rv;
}

/* ------------------------------------------------------------------------- */

static void
PostEvent(nsIEventTarget *eventTarget, PLEvent *ev)
{
  if (!ev)
    return;

  nsresult rv = eventTarget->PostEvent(ev);
  if (NS_FAILED(rv))
  {
    NS_WARNING("PostEvent failed");
    PL_DestroyEvent(ev);
  }
}

static void
PostEventToMainThread(PLEvent *ev)
{
  nsCOMPtr<nsIEventQueue> eventQ;
  NS_GetMainEventQ(getter_AddRefs(eventQ));
  if (!eventQ)
  {
    NS_WARNING("unable to get reference to main event queue");
    PL_DestroyEvent(ev);
    return;
  }
  PostEvent(eventQ, ev);
}

/* ------------------------------------------------------------------------- */

class ipcEvent_ClientState : public PLEvent
{
public:
  ipcEvent_ClientState(PRUint32 aClientID, PRUint32 aClientState)
    : mClientID(aClientID)
    , mClientState(aClientState)
  {
    PL_InitEvent(this, nsnull, HandleEvent, DestroyEvent);
  }

  PR_STATIC_CALLBACK(void *) HandleEvent(PLEvent *ev)
  {
    // maybe we've been shutdown!
    if (!gClientState)
      return nsnull;

    ipcEvent_ClientState *self = (ipcEvent_ClientState *) ev;

    for (PRInt32 i=0; i<gClientState->clientObservers.Count(); ++i)
      gClientState->clientObservers[i]->OnClientStateChange(self->mClientID,
                                                            self->mClientState);
    return nsnull;
  }

  PR_STATIC_CALLBACK(void) DestroyEvent(PLEvent *ev)
  {
    delete (ipcEvent_ClientState *) ev;
  }

private:
  PRUint32 mClientID;
  PRUint32 mClientState;
};

/* ------------------------------------------------------------------------- */

class ipcEvent_ProcessPendingQ : public PLEvent
{
public:
  ipcEvent_ProcessPendingQ(const nsID &aTarget)
    : mTarget(aTarget)
  {
    PL_InitEvent(this, nsnull, HandleEvent, DestroyEvent);
  }

  PR_STATIC_CALLBACK(void *) HandleEvent(PLEvent *ev)
  {
    ProcessPendingQ(((ipcEvent_ProcessPendingQ *) ev)->mTarget);
    return nsnull;
  }

  PR_STATIC_CALLBACK(void) DestroyEvent(PLEvent *ev)
  {
    delete (ipcEvent_ProcessPendingQ *) ev;
  }

private:
  const nsID mTarget;
};

static void
CallProcessPendingQ(const nsID &target, ipcTargetData *td)
{
  // we assume that we are inside td's monitor

  PLEvent *ev = new ipcEvent_ProcessPendingQ(target);
  if (!ev)
    return;

  nsresult rv;

  if (td->eventQ)
    rv = td->eventQ->PostEvent(ev);
  else
    rv = IPC_DoCallback((ipcCallbackFunc) PL_HandleEvent, ev);

  if (NS_FAILED(rv))
    PL_DestroyEvent(ev);
}

/* ------------------------------------------------------------------------- */

static void
DisableMessageObserver(const nsID &aTarget)
{
  nsRefPtr<ipcTargetData> td;
  if (GetTarget(aTarget, getter_AddRefs(td)))
  {
    nsAutoMonitor mon(td->monitor);
    ++td->observerDisabled;
  }
}

static void
EnableMessageObserver(const nsID &aTarget)
{
  nsRefPtr<ipcTargetData> td;
  if (GetTarget(aTarget, getter_AddRefs(td)))
  {
    nsAutoMonitor mon(td->monitor);
    if (td->observerDisabled > 0 && --td->observerDisabled == 0)
      if (!RTListIsEmpty(&td->LstPendingMsgs))
        CallProcessPendingQ(aTarget, td);
  }
}

/* ------------------------------------------------------------------------- */

// converts IPCM_ERROR_* status codes to NS_ERROR_* status codes
static nsresult nsresult_from_ipcm_result(int32_t status)
{
  nsresult rv = NS_ERROR_FAILURE;

  switch (status)
  {
    case IPCM_ERROR_GENERIC:        rv = NS_ERROR_FAILURE; break;
    case IPCM_ERROR_INVALID_ARG:    rv = NS_ERROR_INVALID_ARG; break;
    case IPCM_ERROR_NO_CLIENT:      rv = NS_ERROR_CALL_FAILED; break;
    // TODO: select better mapping for the below codes
    case IPCM_ERROR_NO_SUCH_DATA:
    case IPCM_ERROR_ALREADY_EXISTS: rv = NS_ERROR_FAILURE; break;
    default:                        NS_ASSERTION(PR_FALSE, "No conversion");
  }

  return rv;
}

/* ------------------------------------------------------------------------- */

// selects the next IPCM message with matching request index
static nsresult
WaitIPCMResponseSelector(void *arg, ipcTargetData *td, PCIPCMSG pMsg)
{
#ifdef VBOX
  if (!pMsg)
    return IPC_WAIT_NEXT_MESSAGE;
#endif /* VBOX */
  PRUint32 requestIndex = *(PRUint32 *) arg;
  return IPCM_GetRequestIndex(pMsg) == requestIndex ? NS_OK : IPC_WAIT_NEXT_MESSAGE;
}

// wait for an IPCM response message.  if responseMsg is null, then it is
// assumed that the caller does not care to get a reference to the
// response itself.  if the response is an IPCM_MSG_ACK_RESULT, then the
// status code is mapped to a nsresult and returned by this function.
static nsresult
WaitIPCMResponse(PRUint32 requestIndex, PIPCMSG *ppMsgResponse = NULL)
{
  PIPCMSG pMsg;

  nsresult rv = WaitTarget(IPCM_TARGET, IPC_REQUEST_TIMEOUT, &pMsg,
                           WaitIPCMResponseSelector, &requestIndex);
  if (NS_FAILED(rv))
    return rv;

  if (IPCM_GetType(pMsg) == IPCM_MSG_ACK_RESULT)
  {
    PCIPCMMSGRESULT pIpcmRes = (PCIPCMMSGRESULT)IPCMsgGetPayload(pMsg);
    if (pIpcmRes->i32Status < 0)
      rv = nsresult_from_ipcm_result(pIpcmRes->i32Status);
    else
      rv = NS_OK;
  }

  if (ppMsgResponse)
    *ppMsgResponse = pMsg;
  else
    IPC_MsgFree(pMsg);

  return rv;
}

// make an IPCM request and wait for a response.
static nsresult
MakeIPCMRequest(PIPCMSG pMsg, PIPCMSG *ppMsgResponse = NULL)
{
    AssertPtrReturn(pMsg, NS_ERROR_OUT_OF_MEMORY);

    uint32_t requestIndex = IPCM_GetRequestIndex(pMsg);

    // suppress 'ProcessPendingQ' for IPCM messages until we receive the
    // response to this IPCM request.  if we did not do this then there
    // would be a race condition leading to the possible removal of our
    // response from the pendingQ between sending the request and waiting
    // for the response.
    DisableMessageObserver(IPCM_TARGET);

    nsresult rv = IPC_SendMsg(pMsg);
    if (NS_SUCCEEDED(rv))
        rv = WaitIPCMResponse(requestIndex, ppMsgResponse);

    EnableMessageObserver(IPCM_TARGET);
    return rv;
}

/* ------------------------------------------------------------------------- */

static void RemoveTarget(const nsID &aTarget, PRBool aNotifyDaemon)
{
    DelTarget(aTarget);

    if (aNotifyDaemon)
    {
        IPCMMSGSTACK IpcmMsg;
        IPCMSG IpcMsg;

        IPCMMsgDelTargetInit(&IpcmMsg.Ipcm.AddDelTarget, aTarget);
        IPCMsgInitStack(&IpcMsg, IPCM_TARGET, &IpcmMsg, sizeof(IpcmMsg.Ipcm.AddDelTarget));
        nsresult rv = MakeIPCMRequest(&IpcMsg);
        if (NS_FAILED(rv))
            Log(("failed to delete target: rv=%x\n", rv));
    }
}

static nsresult
DefineTarget(const nsID           &aTarget,
             ipcIMessageObserver  *aObserver,
             PRBool                aOnCurrentThread,
             PRBool                aNotifyDaemon,
             ipcTargetData       **aResult)
{
    nsresult rv;

    nsRefPtr<ipcTargetData> td( ipcTargetData::Create() );
    if (!td)
        return NS_ERROR_OUT_OF_MEMORY;
    td->SetObserver(aObserver, aOnCurrentThread);

    if (!PutTarget(aTarget, td))
        return NS_ERROR_OUT_OF_MEMORY;

    if (aNotifyDaemon)
    {
        IPCMMSGSTACK IpcmMsg;
        IPCMSG IpcMsg;

        IPCMMsgAddTargetInit(&IpcmMsg.Ipcm.AddDelTarget, aTarget);
        IPCMsgInitStack(&IpcMsg, IPCM_TARGET, &IpcmMsg, sizeof(IpcmMsg.Ipcm.AddDelTarget));
        rv = MakeIPCMRequest(&IpcMsg);
        if (NS_FAILED(rv))
        {
          Log(("failed to add target: rv=%x\n", rv));
          RemoveTarget(aTarget, PR_FALSE);
          return rv;
        }
    }

    if (aResult)
        NS_ADDREF(*aResult = td);
    return NS_OK;
}

/* ------------------------------------------------------------------------- */

static nsresult
TryConnect()
{
    nsCAutoString dpath;
    nsresult rv = GetDaemonPath(dpath);
    if (NS_FAILED(rv))
        return rv;

    rv = IPC_Connect(dpath.get());
    if (NS_FAILED(rv))
        return rv;

    gClientState->connected = PR_TRUE;

    rv = DefineTarget(IPCM_TARGET, nsnull, PR_FALSE, PR_FALSE, nsnull);
    if (NS_FAILED(rv))
        return rv;

    PIPCMSG pMsg = NULL;

    // send CLIENT_HELLO and wait for CLIENT_ID response...
    IPCMMSGSTACK IpcmMsg;
    IPCMSG IpcMsg;

    IPCMMsgClientHelloInit(&IpcmMsg.Ipcm.ClientHello);
    IPCMsgInitStack(&IpcMsg, IPCM_TARGET, &IpcmMsg, sizeof(IpcmMsg.Ipcm.ClientHello));
    rv = MakeIPCMRequest(&IpcMsg, &pMsg);
    if (NS_FAILED(rv))
    {
#ifdef VBOX  /* MakeIPCMRequest may return a failure (e.g. NS_ERROR_CALL_FAILED) and a response msg. */
        if (pMsg)
            IPC_MsgFree(pMsg);
#endif
        return rv;
    }

    if (IPCM_GetType(pMsg) == IPCM_MSG_ACK_CLIENT_ID)
    {
        PCIPCMMSGCLIENTID pIpcmClientId = (PCIPCMMSGCLIENTID)IPCMsgGetPayload(pMsg);
        gClientState->selfID = pIpcmClientId->u32ClientId;
    }
    else
    {
        Log(("unexpected response from CLIENT_HELLO message: type=%x!\n", IPCM_GetType(pMsg)));
        rv = NS_ERROR_UNEXPECTED;
    }

    IPC_MsgFree(pMsg);
    return rv;
}

nsresult
IPC_Init()
{
  NS_ENSURE_TRUE(!gClientState, NS_ERROR_ALREADY_INITIALIZED);

  gClientState = ipcClientState::Create();
  if (!gClientState)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = TryConnect();
  if (NS_FAILED(rv))
    IPC_Shutdown();

  return rv;
}

PR_STATIC_CALLBACK(PLDHashOperator)
EnumerateTargetMapAndNotify(const nsID    &aKey,
                            ipcTargetData *aData,
                            void          *aClosure);

nsresult
IPC_Shutdown()
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  Log(("IPC_Shutdown: connected=%d\n",gClientState->connected));

  if (gClientState->connected)
  {
    {
      // first, set the shutdown flag and unblock any calls to WaitTarget.
      // all targets but IPCM will not be able to use WaitTarget any more.

      RTCritSectRwEnterExcl(&gClientState->critSect);
      gClientState->shutdown = PR_TRUE;
      gClientState->targetMap.EnumerateRead(EnumerateTargetMapAndNotify, nsnull);
      RTCritSectRwLeaveExcl(&gClientState->critSect);
    }

    // inform all client observers that we're being shutdown to let interested
    // parties gracefully uninitialize themselves.  the IPCM target is still
    // fully operational at this point, so they can use IPC_SendMessage
    // (this is essential for the DConnect extension, for example, to do the
    // proper uninitialization).

    ipcEvent_ClientState *ev = new ipcEvent_ClientState(IPC_SENDER_ANY,
                                                        IPCM_CLIENT_STATE_DOWN);
    ipcEvent_ClientState::HandleEvent (ev);
    ipcEvent_ClientState::DestroyEvent (ev);

    IPC_Disconnect();
  }

  //
  // make gClientState nsnull before deletion to cause all public IPC_*
  // calls (possibly made during ipcClientState destruction) to return
  // NS_ERROR_NOT_INITIALIZED.
  //
  // NOTE: isn't just checking for gClientState->connected in every appropriate
  // IPC_* method a better solution?
  //
  ipcClientState *aClientState = gClientState;
  gClientState = nsnull;
  delete aClientState;

  return NS_OK;
}

/* ------------------------------------------------------------------------- */

nsresult
IPC_DefineTarget(const nsID          &aTarget,
                 ipcIMessageObserver *aObserver,
                 PRBool               aOnCurrentThread)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  // do not permit the re-definition of the IPCM protocol's target.
  if (aTarget.Equals(IPCM_TARGET))
    return NS_ERROR_INVALID_ARG;

  nsresult rv;

  nsRefPtr<ipcTargetData> td;
  if (GetTarget(aTarget, getter_AddRefs(td)))
  {
    // clear out observer before removing target since we want to ensure that
    // the observer is released on the main thread.
    {
      nsAutoMonitor mon(td->monitor);
      td->SetObserver(aObserver, aOnCurrentThread);
    }

    // remove target outside of td's monitor to avoid holding the monitor
    // while entering the client state's monitor.
    if (!aObserver)
      RemoveTarget(aTarget, PR_TRUE);

    rv = NS_OK;
  }
  else
  {
    if (aObserver)
      rv = DefineTarget(aTarget, aObserver, aOnCurrentThread, PR_TRUE, nsnull);
    else
      rv = NS_ERROR_INVALID_ARG; // unknown target
  }

  return rv;
}

nsresult
IPC_DisableMessageObserver(const nsID &aTarget)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  // do not permit modifications to the IPCM protocol's target.
  if (aTarget.Equals(IPCM_TARGET))
    return NS_ERROR_INVALID_ARG;

  DisableMessageObserver(aTarget);
  return NS_OK;
}

nsresult
IPC_EnableMessageObserver(const nsID &aTarget)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  // do not permit modifications to the IPCM protocol's target.
  if (aTarget.Equals(IPCM_TARGET))
    return NS_ERROR_INVALID_ARG;

  EnableMessageObserver(aTarget);
  return NS_OK;
}

nsresult
IPC_SendMessage(PRUint32       aReceiverID,
                const nsID    &aTarget,
                const PRUint8 *aData,
                PRUint32       aDataLen)
{
    NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

    // do not permit sending IPCM messages
    if (aTarget.Equals(IPCM_TARGET))
      return NS_ERROR_INVALID_ARG;

    nsresult rv;
    if (aReceiverID == 0)
    {
        RTSGSEG Seg = { (void *)aData, aDataLen };
        PIPCMSG pMsg = IPC_MsgNewSg(aTarget, aDataLen, &Seg, 1 /*cSegs*/);
        if (!pMsg)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = IPC_SendMsg(pMsg);
    }
    else
    {
        IPCMSGHDR InnerMsgHdr;
        IPCMMSGFORWARD IpcmFwd;
        RTSGSEG aSegs[3];

        /* Construct the forwarded message. */
        IpcmFwd.Hdr.u32Type         = IPCM_MSG_REQ_FORWARD;
        IpcmFwd.Hdr.u32RequestIndex = IPCM_NewRequestIndex();
        IpcmFwd.u32ClientId         = aReceiverID;

        aSegs[0].pvSeg = &IpcmFwd;
        aSegs[0].cbSeg = sizeof(IpcmFwd);
        aSegs[1].pvSeg = IPCMsgHdrInit(&InnerMsgHdr, aTarget, aDataLen);
        aSegs[1].cbSeg = sizeof(InnerMsgHdr);
        aSegs[2].pvSeg = (void *)aData;
        aSegs[2].cbSeg = aDataLen;

        PIPCMSG pMsg = IPC_MsgNewSg(IPCM_TARGET, aDataLen + sizeof(IpcmFwd) + sizeof(InnerMsgHdr),
                                    &aSegs[0], RT_ELEMENTS(aSegs));
        rv = MakeIPCMRequest(pMsg);
    }

    return rv;
}

struct WaitMessageSelectorData
{
  PRUint32             senderID;
  ipcIMessageObserver *observer;
  PRBool               senderDead;
};

static nsresult WaitMessageSelector(void *arg, ipcTargetData *td, PCIPCMSG pMsg)
{
  WaitMessageSelectorData *data = (WaitMessageSelectorData *) arg;
#ifdef VBOX
  if (!pMsg)
  {
    /* Special NULL message which asks to check whether the client is
     * still alive. Called when there is nothing suitable in the queue. */
    ipcIMessageObserver *obs = data->observer;
    if (!obs)
      obs = td->observer;
    NS_ASSERTION(obs, "must at least have a default observer");

    nsresult rv = obs->OnMessageAvailable(IPC_SENDER_ANY, nsID(), 0, 0);
    if (rv != IPC_WAIT_NEXT_MESSAGE)
      data->senderDead = PR_TRUE;

    return rv;
  }
#endif /* VBOX */

  // process the specially forwarded client state message to see if the
  // sender we're waiting a message from has died.

  if (IPCMsgGetTarget(pMsg)->Equals(IPCM_TARGET))
  {
    switch (IPCM_GetType(pMsg))
    {
      case IPCM_MSG_PSH_CLIENT_STATE:
      {
        PCIPCMMSGCLIENTSTATE pClientState = (PCIPCMMSGCLIENTSTATE)IPCMsgGetPayload(pMsg);
        if ((data->senderID == IPC_SENDER_ANY ||
             pClientState->u32ClientId == data->senderID) &&
             pClientState->u32ClientStatus == IPCM_CLIENT_STATE_DOWN)
        {
          Log(("sender (%d) we're waiting a message from (%d) has died\n",
               pClientState->u32ClientId, data->senderID));

          if (data->senderID != IPC_SENDER_ANY)
          {
            // we're waiting on a particular client, so IPC_WaitMessage must
            // definitely fail with the NS_ERROR_xxx result.

            data->senderDead = PR_TRUE;
            return IPC_DISCARD_MESSAGE; // consume the message
          }
          else
          {
            // otherwise inform the observer about the client death using a special
            // null message with an empty target id, and fail IPC_WaitMessage call
            // with NS_ERROR_xxx only if the observer accepts this message.

            ipcIMessageObserver *obs = data->observer;
            if (!obs)
              obs = td->observer;
            NS_ASSERTION(obs, "must at least have a default observer");

            nsresult rv = obs->OnMessageAvailable(pClientState->u32ClientId, nsID(), 0, 0);
            if (rv != IPC_WAIT_NEXT_MESSAGE)
              data->senderDead = PR_TRUE;

            return IPC_DISCARD_MESSAGE; // consume the message
          }
        }
#ifdef VBOX
        else if ((data->senderID == IPC_SENDER_ANY ||
                  pClientState->u32ClientId) &&
                 pClientState->u32ClientStatus == IPCM_CLIENT_STATE_UP)
        {
          Log(("sender (%d) we're waiting a message from (%d) has come up\n",
               pClientState->u32ClientId, data->senderID));
          if (data->senderID == IPC_SENDER_ANY)
          {
            // inform the observer about the client appearance using a special
            // null message with an empty target id, but a length of 1.

            ipcIMessageObserver *obs = data->observer;
            if (!obs)
              obs = td->observer;
            NS_ASSERTION(obs, "must at least have a default observer");

            /*nsresult rv = */obs->OnMessageAvailable(pClientState->u32ClientId, nsID(), 0, 1);
            /* VBoxSVC/VBoxXPCOMIPCD auto-start can cause that a client up
             * message arrives while we're already waiting for a response
             * from this client. Don't declare the connection as dead in
             * this case. A client ID wraparound can't falsely trigger
             * this, since the waiting thread would have hit the liveness
             * check in the mean time. We MUST consume the message, otherwise
             * IPCM messages pile up as long as there is a pending call, which
             * can lead to severe processing overhead. */
            return IPC_DISCARD_MESSAGE; // consume the message
          }
        }
#endif /* VBOX */
        break;
      }
      default:
        NS_NOTREACHED("unexpected message");
    }
    return IPC_WAIT_NEXT_MESSAGE; // continue iterating
  }

  nsresult rv = IPC_WAIT_NEXT_MESSAGE;

  if (data->senderID == IPC_SENDER_ANY ||
      pMsg->upUser == data->senderID)
  {
    ipcIMessageObserver *obs = data->observer;
    if (!obs)
      obs = td->observer;
    NS_ASSERTION(obs, "must at least have a default observer");

    rv = obs->OnMessageAvailable(pMsg->upUser,
                                 *IPCMsgGetTarget(pMsg),
                                 (const PRUint8 *)IPCMsgGetPayload(pMsg),
                                 IPCMsgGetPayloadSize(pMsg));
  }

  // stop iterating if we got a match that the observer accepted.
  return rv != IPC_WAIT_NEXT_MESSAGE ? NS_OK : IPC_WAIT_NEXT_MESSAGE;
}

nsresult
IPC_WaitMessage(PRUint32             aSenderID,
                const nsID          &aTarget,
                ipcIMessageObserver *aObserver,
                ipcIMessageObserver *aConsumer,
                RTMSINTERVAL        aTimeout)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  // do not permit waiting for IPCM messages
  if (aTarget.Equals(IPCM_TARGET))
    return NS_ERROR_INVALID_ARG;

  // use aObserver as the message selector
  WaitMessageSelectorData data = { aSenderID, aObserver, PR_FALSE };

  PIPCMSG pMsg;
  nsresult rv = WaitTarget(aTarget, aTimeout, &pMsg, WaitMessageSelector, &data);
  if (NS_FAILED(rv))
    return rv;

  // if the selector has accepted some message, then we pass it to aConsumer
  // for safe processing.  The IPC susbsystem is quite stable here (i.e. we're
  // not inside any of the monitors, and the message has been already removed
  // from the pending queue).
  if (aObserver && aConsumer)
  {
    aConsumer->OnMessageAvailable(pMsg->upUser,
                                  *IPCMsgGetTarget(pMsg),
                                  (const PRUint8 *)IPCMsgGetPayload(pMsg),
                                  IPCMsgGetPayloadSize(pMsg));
  }

  IPC_MsgFree(pMsg);

  // if the requested sender has died while waiting, return an error
  if (data.senderDead)
    return NS_ERROR_ABORT; // XXX better error code?

  return NS_OK;
}

/* ------------------------------------------------------------------------- */

nsresult
IPC_GetID(PRUint32 *aClientID)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  *aClientID = gClientState->selfID;
  return NS_OK;
}

nsresult
IPC_AddName(const char *aName)
{
    NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

    size_t cbStr = strlen(aName) + 1; /* Includes terminator. */
    const IPCMMSGHDR Hdr = { IPCM_MSG_REQ_CLIENT_ADD_NAME, IPCM_NewRequestIndex() };
    RTSGSEG aSegs[2];

    aSegs[0].pvSeg = (void *)&Hdr;
    aSegs[0].cbSeg = sizeof(Hdr);

    aSegs[1].pvSeg = (void *)aName;
    aSegs[1].cbSeg = cbStr;

    PIPCMSG pMsg = IPC_MsgNewSg(IPCM_TARGET, cbStr + sizeof(Hdr),
                                &aSegs[0], RT_ELEMENTS(aSegs));
    return MakeIPCMRequest(pMsg);
}

nsresult
IPC_RemoveName(const char *aName)
{
    NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

    size_t cbStr = strlen(aName) + 1; /* Includes terminator. */
    const IPCMMSGHDR Hdr = { IPCM_MSG_REQ_CLIENT_DEL_NAME, IPCM_NewRequestIndex() };
    RTSGSEG aSegs[2];

    aSegs[0].pvSeg = (void *)&Hdr;
    aSegs[0].cbSeg = sizeof(Hdr);

    aSegs[1].pvSeg = (void *)aName;
    aSegs[1].cbSeg = cbStr;

    PIPCMSG pMsg = IPC_MsgNewSg(IPCM_TARGET, cbStr + sizeof(Hdr),
                                &aSegs[0], RT_ELEMENTS(aSegs));
    return MakeIPCMRequest(pMsg);
}

/* ------------------------------------------------------------------------- */

nsresult
IPC_AddClientObserver(ipcIClientObserver *aObserver)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  return gClientState->clientObservers.AppendObject(aObserver)
      ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult
IPC_RemoveClientObserver(ipcIClientObserver *aObserver)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  for (PRInt32 i = 0; i < gClientState->clientObservers.Count(); ++i)
  {
    if (gClientState->clientObservers[i] == aObserver)
      gClientState->clientObservers.RemoveObjectAt(i);
  }

  return NS_OK;
}

/* ------------------------------------------------------------------------- */

// this function could be called on any thread
nsresult
IPC_ResolveClientName(const char *aName, PRUint32 *aClientID)
{
    NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

    size_t cbStr = strlen(aName) + 1; /* Includes terminator. */
    const IPCMMSGHDR Hdr = { IPCM_MSG_REQ_QUERY_CLIENT_BY_NAME, IPCM_NewRequestIndex() };
    RTSGSEG aSegs[2];

    aSegs[0].pvSeg = (void *)&Hdr;
    aSegs[0].cbSeg = sizeof(Hdr);

    aSegs[1].pvSeg = (void *)aName;
    aSegs[1].cbSeg = cbStr;

    PIPCMSG pMsg = IPC_MsgNewSg(IPCM_TARGET, cbStr + sizeof(Hdr),
                                &aSegs[0], RT_ELEMENTS(aSegs));

    PIPCMSG pMsgResp = NULL;
    nsresult rv = MakeIPCMRequest(pMsg, &pMsgResp);
    if (NS_FAILED(rv))
    {
        /* MakeIPCMRequest may return a failure (e.g. NS_ERROR_CALL_FAILED) and a response msg. */
        if (pMsgResp)
            IPC_MsgFree(pMsgResp);

        return rv;
    }

    if (IPCM_GetType(pMsgResp) == IPCM_MSG_ACK_CLIENT_ID)
    {
        PCIPCMMSGCLIENTID pClientId = (PCIPCMMSGCLIENTID)IPCMsgGetPayload(pMsgResp);
        *aClientID = pClientId->u32ClientId;
    }
    else
    {
        Log(("unexpected IPCM response: type=%x\n", IPCM_GetType(pMsg)));
        rv = NS_ERROR_UNEXPECTED;
    }

    IPC_MsgFree(pMsgResp);
    return rv;
}


/* ------------------------------------------------------------------------- */

nsresult
IPC_SpawnDaemon(const char *path)
{
    /*
     * Setup an anonymous pipe that we can use to determine when the daemon
     * process has started up.  the daemon will write a char to the pipe, and
     * when we read it, we'll know to proceed with trying to connect to the
     * daemon.
     */
    RTPIPE hPipeWr = NIL_RTPIPE;
    RTPIPE hPipeRd = NIL_RTPIPE;
    int vrc = RTPipeCreate(&hPipeRd, &hPipeWr, RTPIPE_C_INHERIT_WRITE);
    if (RT_SUCCESS(vrc))
    {
      char szPipeInheritFd[32]; RT_ZERO(szPipeInheritFd);
      const char *const s_apszArgs[] = { (char *const) path,
                                         "--auto-shutdown",
                                         "--inherit-startup-pipe",
                                         &szPipeInheritFd[0], NULL };

      ssize_t cch = RTStrFormatU32(&szPipeInheritFd[0], sizeof(szPipeInheritFd),
                                   (uint32_t)RTPipeToNative(hPipeWr), 10 /*uiBase*/,
                                   0 /*cchWidth*/, 0 /*cchPrecision*/, 0 /*fFlags*/);
      Assert(cch > 0); RT_NOREF(cch);

      RTHANDLE hStdNil;
      hStdNil.enmType = RTHANDLETYPE_FILE;
      hStdNil.u.hFile = NIL_RTFILE;

      vrc = RTProcCreateEx(path, s_apszArgs, RTENV_DEFAULT,
                           RTPROC_FLAGS_DETACHED, &hStdNil, &hStdNil, &hStdNil,
                           NULL /* pszAsUser */, NULL /* pszPassword */, NULL /* pExtraData */,
                           NULL /* phProcess */);
      if (RT_SUCCESS(vrc))
      {
          vrc = RTPipeClose(hPipeWr); AssertRC(vrc); RT_NOREF(vrc);
          hPipeWr = NIL_RTPIPE;

          size_t cbRead = 0;
          char msg[10];
          memset(msg, '\0', sizeof(msg));
          vrc = RTPipeReadBlocking(hPipeRd, &msg[0], sizeof(msg) - 1, &cbRead);
          if (   RT_SUCCESS(vrc)
              && cbRead == 5
              && !strcmp(msg, "READY"))
          {
            RTPipeClose(hPipeRd);
            return NS_OK;
          }
      }

      if (hPipeWr != NIL_RTPIPE)
          RTPipeClose(hPipeWr);
      RTPipeClose(hPipeRd);
    }

    return NS_ERROR_FAILURE;
}

/* ------------------------------------------------------------------------- */

PR_STATIC_CALLBACK(PLDHashOperator)
EnumerateTargetMapAndNotify(const nsID    &aKey,
                            ipcTargetData *aData,
                            void          *aClosure)
{
  nsAutoMonitor mon(aData->monitor);

  // wake up anyone waiting on this target.
  mon.NotifyAll();

  return PL_DHASH_NEXT;
}

// called on a background thread
void
IPC_OnConnectionEnd(nsresult error)
{
  // now, go through the target map, and tickle each monitor.  that should
  // unblock any calls to WaitTarget.

  RTCritSectRwEnterExcl(&gClientState->critSect);
  gClientState->connected = PR_FALSE;
  gClientState->targetMap.EnumerateRead(EnumerateTargetMapAndNotify, nsnull);
  RTCritSectRwLeaveExcl(&gClientState->critSect);
}

/* ------------------------------------------------------------------------- */

static void
PlaceOnPendingQ(const nsID &target, ipcTargetData *td, PIPCMSG pMsg)
{
  nsAutoMonitor mon(td->monitor);

  // we only want to dispatch a 'ProcessPendingQ' event if we have not
  // already done so.
  PRBool dispatchEvent = RTListIsEmpty(&td->LstPendingMsgs);

  // put this message on our pending queue
  RTListAppend(&td->LstPendingMsgs, &pMsg->NdMsg);

#ifdef LOG_ENABLED
  {
    char *targetStr = target.ToString();
    Log(("placed message on pending queue for target %s and notifying all...\n", targetStr));
    nsMemory::Free(targetStr);
  }
#endif

  // wake up anyone waiting on this queue
  mon.NotifyAll();

  // proxy call to target's message procedure
  if (dispatchEvent)
    CallProcessPendingQ(target, td);
}

PR_STATIC_CALLBACK(PLDHashOperator)
EnumerateTargetMapAndPlaceMsg(const nsID    &aKey,
                              ipcTargetData *aData,
                              void          *userArg)
{
  if (!aKey.Equals(IPCM_TARGET))
  {
    // place a message clone to a target's event queue
    PCIPCMSG pMsg = (PCIPCMSG)userArg;
    PIPCMSG pClone = IPC_MsgClone(pMsg);
    Assert(pClone);
    if (pClone)
       PlaceOnPendingQ(aKey, aData, pClone);
  }

  return PL_DHASH_NEXT;
}

/* ------------------------------------------------------------------------- */

// called on a background thread
DECLHIDDEN(void) IPC_OnMessageAvailable(PCIPCMSG pMsg)
{
  const nsID target = *IPCMsgGetTarget(pMsg);

#ifdef LOG_ENABLED
  {
    char *targetStr = target.ToString();
    Log(("got message for target: %s\n", targetStr));
    nsMemory::Free(targetStr);

//     IPC_LogBinary((const PRUint8 *) msg->Data(), msg->DataLen());
  }
#endif

  if (target.Equals(IPCM_TARGET))
  {
    switch (IPCM_GetType(pMsg))
    {
      case IPCM_MSG_PSH_FORWARD:
      {
        PCIPCMMSGFORWARD pFwd = (PCIPCMMSGFORWARD)IPCMsgGetPayload(pMsg);

        /** @todo De-uglify this. */
        /* Forward the inner message. */
        IPCMSG InnerMsg; RT_ZERO(InnerMsg);
        InnerMsg.pMsgHdr = (PIPCMSGHDR)(pFwd + 1);
        InnerMsg.cbBuf   = InnerMsg.pMsgHdr->cbMsg;
        InnerMsg.pbBuf   = (uint8_t *)InnerMsg.pMsgHdr;
        InnerMsg.upUser  = pFwd->u32ClientId;

        /* Recurse to forward the inner message (it will get cloned). */
        IPC_OnMessageAvailable(&InnerMsg);
        return;
      }
      case IPCM_MSG_PSH_CLIENT_STATE:
      {
        PCIPCMMSGCLIENTSTATE pClientState = (PCIPCMMSGCLIENTSTATE)IPCMsgGetPayload(pMsg);
        PostEventToMainThread(new ipcEvent_ClientState(pClientState->u32ClientId,
                                                       pClientState->u32ClientStatus));

        // go through the target map, and place this message to every target's
        // pending event queue.  that unblocks all WaitTarget calls (on all
        // targets) giving them an opportuninty to finish wait cycle because of
        // the peer client death, when appropriate.
        RTCritSectRwEnterShared(&gClientState->critSect);
        gClientState->targetMap.EnumerateRead(EnumerateTargetMapAndPlaceMsg, (void *)pMsg);
        RTCritSectRwLeaveShared(&gClientState->critSect);
        return;
      }
    }
  }

  nsRefPtr<ipcTargetData> td;
  if (GetTarget(target, getter_AddRefs(td)))
  {
    PIPCMSG pClone = IPC_MsgClone(pMsg);
    PlaceOnPendingQ(target, td, pClone);
  }
  else
    NS_WARNING("message target is undefined");
}
