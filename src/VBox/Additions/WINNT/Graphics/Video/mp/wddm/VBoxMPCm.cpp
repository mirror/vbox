/* $Id$ */

/** @file
 * VBox WDDM Miniport driver
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxMPWddm.h"

typedef struct VBOXVIDEOCM_CMD_DR
{
    LIST_ENTRY QueueList;
    PVBOXVIDEOCM_CTX pContext;
    uint32_t cbMaxCmdSize;
    volatile uint32_t cRefs;

    VBOXVIDEOCM_CMD_HDR CmdHdr;
} VBOXVIDEOCM_CMD_DR, *PVBOXVIDEOCM_CMD_DR;

AssertCompile(VBOXWDDM_ROUNDBOUND(RT_OFFSETOF(VBOXVIDEOCM_CMD_DR, CmdHdr), 8) == RT_OFFSETOF(VBOXVIDEOCM_CMD_DR, CmdHdr));

#define VBOXVIDEOCM_HEADER_SIZE() (VBOXWDDM_ROUNDBOUND(sizeof (VBOXVIDEOCM_CMD_DR), 8))
#define VBOXVIDEOCM_SIZE_FROMBODYSIZE(_s) (VBOXVIDEOCM_HEADER_SIZE() + (_s))
//#define VBOXVIDEOCM_SIZE(_t) (VBOXVIDEOCM_SIZE_FROMBODYSIZE(sizeof (_t)))
#define VBOXVIDEOCM_BODY(_pCmd, _t) ( (_t*)(((uint8_t*)(_pCmd)) + VBOXVIDEOCM_HEADER_SIZE()) )
#define VBOXVIDEOCM_HEAD(_pCmd) ( (PVBOXVIDEOCM_CMD_DR)(((uint8_t*)(_pCmd)) - VBOXVIDEOCM_HEADER_SIZE()) )

#define VBOXVIDEOCM_SENDSIZE_FROMBODYSIZE(_s) ( VBOXVIDEOCM_SIZE_FROMBODYSIZE(_s) - RT_OFFSETOF(VBOXVIDEOCM_CMD_DR, CmdHdr))

//#define VBOXVIDEOCM_BODY_FIELD_OFFSET(_ot, _t, _f) ( (_ot)( VBOXVIDEOCM_BODY(0, uint8_t) + RT_OFFSETOF(_t, _f) ) )

typedef struct VBOXVIDEOCM_SESSION
{
    /* contexts in this session */
    LIST_ENTRY QueueEntry;
    /* contexts in this session */
    LIST_ENTRY ContextList;
    /* commands list  */
    LIST_ENTRY CommandsList;
    /* event used to notify UMD about pending commands */
    PKEVENT pUmEvent;
    /* sync lock */
    FAST_MUTEX Mutex;
    /* indicates whether event signaling is needed on cmd add */
    bool bEventNeeded;
} VBOXVIDEOCM_SESSION, *PVBOXVIDEOCM_SESSION;

#define VBOXCMENTRY_2_CMD(_pE) ((PVBOXVIDEOCM_CMD_DR)((uint8_t*)(_pE) - RT_OFFSETOF(VBOXVIDEOCM_CMD_DR, QueueList)))

void* vboxVideoCmCmdReinitForContext(void *pvCmd, PVBOXVIDEOCM_CTX pContext)
{
    PVBOXVIDEOCM_CMD_DR pHdr = VBOXVIDEOCM_HEAD(pvCmd);
    pHdr->pContext = pContext;
    pHdr->CmdHdr.u64UmData = pContext->u64UmData;
    return pvCmd;
}

void* vboxVideoCmCmdCreate(PVBOXVIDEOCM_CTX pContext, uint32_t cbSize)
{
    Assert(cbSize);
    if (!cbSize)
        return NULL;

    Assert(VBOXWDDM_ROUNDBOUND(cbSize, 8) == cbSize);
    cbSize = VBOXWDDM_ROUNDBOUND(cbSize, 8);

    Assert(pContext->pSession);
    if (!pContext->pSession)
        return NULL;

    uint32_t cbCmd = VBOXVIDEOCM_SIZE_FROMBODYSIZE(cbSize);
    PVBOXVIDEOCM_CMD_DR pCmd = (PVBOXVIDEOCM_CMD_DR)vboxWddmMemAllocZero(cbCmd);
    Assert(pCmd);
    if (pCmd)
    {
        InitializeListHead(&pCmd->QueueList);
        pCmd->pContext = pContext;
        pCmd->cbMaxCmdSize = VBOXVIDEOCM_SENDSIZE_FROMBODYSIZE(cbSize);
        pCmd->cRefs = 1;
        pCmd->CmdHdr.u64UmData = pContext->u64UmData;
        pCmd->CmdHdr.cbCmd = pCmd->cbMaxCmdSize;
    }
    return VBOXVIDEOCM_BODY(pCmd, void);
}

DECLINLINE(void) vboxVideoCmCmdRetainByHdr(PVBOXVIDEOCM_CMD_DR pHdr)
{
    ASMAtomicIncU32(&pHdr->cRefs);
}

DECLINLINE(void) vboxVideoCmCmdReleaseByHdr(PVBOXVIDEOCM_CMD_DR pHdr)
{
    uint32_t cRefs = ASMAtomicDecU32(&pHdr->cRefs);
    Assert(cRefs < UINT32_MAX/2);
    if (!cRefs)
        vboxWddmMemFree(pHdr);
}

static void vboxVideoCmCmdCancel(PVBOXVIDEOCM_CMD_DR pHdr)
{
    InitializeListHead(&pHdr->QueueList);
    vboxVideoCmCmdReleaseByHdr(pHdr);
}

static void vboxVideoCmCmdPostByHdr(PVBOXVIDEOCM_SESSION pSession, PVBOXVIDEOCM_CMD_DR pHdr, uint32_t cbSize)
{
    bool bSignalEvent = false;
    if (cbSize != VBOXVIDEOCM_SUBMITSIZE_DEFAULT)
    {
        cbSize = VBOXVIDEOCM_SENDSIZE_FROMBODYSIZE(cbSize);
        Assert(cbSize <= pHdr->cbMaxCmdSize);
        pHdr->CmdHdr.cbCmd = cbSize;
    }

    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
    ExAcquireFastMutex(&pSession->Mutex);

    InsertHeadList(&pSession->CommandsList, &pHdr->QueueList);
    if (pSession->bEventNeeded)
    {
        pSession->bEventNeeded = false;
        bSignalEvent = true;
    }

    ExReleaseFastMutex(&pSession->Mutex);

    if (bSignalEvent)
        KeSetEvent(pSession->pUmEvent, 0, FALSE);
}

void vboxVideoCmCmdRetain(void *pvCmd)
{
    PVBOXVIDEOCM_CMD_DR pHdr = VBOXVIDEOCM_HEAD(pvCmd);
    vboxVideoCmCmdRetainByHdr(pHdr);
}

void vboxVideoCmCmdRelease(void *pvCmd)
{
    PVBOXVIDEOCM_CMD_DR pHdr = VBOXVIDEOCM_HEAD(pvCmd);
    vboxVideoCmCmdReleaseByHdr(pHdr);
}

/**
 * @param pvCmd memory buffer returned by vboxVideoCmCmdCreate
 * @param cbSize should be <= cbSize posted to vboxVideoCmCmdCreate on command creation
 */
void vboxVideoCmCmdSubmit(void *pvCmd, uint32_t cbSize)
{
    PVBOXVIDEOCM_CMD_DR pHdr = VBOXVIDEOCM_HEAD(pvCmd);
    vboxVideoCmCmdPostByHdr(pHdr->pContext->pSession, pHdr, cbSize);
}

NTSTATUS vboxVideoCmCmdVisit(PVBOXVIDEOCM_CTX pContext, BOOL bEntireSession, PFNVBOXVIDEOCMCMDVISITOR pfnVisitor, PVOID pvVisitor)
{
    PVBOXVIDEOCM_SESSION pSession = pContext->pSession;
    PLIST_ENTRY pCurEntry = NULL;
    PVBOXVIDEOCM_CMD_DR pHdr;

    ExAcquireFastMutex(&pSession->Mutex);

    pCurEntry = pSession->CommandsList.Blink;
    do
    {
        if (pCurEntry != &pSession->CommandsList)
        {
            pHdr = VBOXCMENTRY_2_CMD(pCurEntry);
            pCurEntry = pHdr->QueueList.Blink;
            if (bEntireSession || pHdr->pContext == pContext)
            {
                void * pvBody = VBOXVIDEOCM_BODY(pHdr, void);
                UINT fRet = pfnVisitor(pHdr->pContext, pvBody, pHdr->CmdHdr.cbCmd, pvVisitor);
                if (fRet & VBOXVIDEOCMCMDVISITOR_RETURN_RMCMD)
                {
                    RemoveEntryList(&pHdr->QueueList);
                }
                if (!(fRet & VBOXVIDEOCMCMDVISITOR_RETURN_CONTINUE))
                    break;
            }
        }
        else
        {
            break;
        }
    } while (1);


    ExReleaseFastMutex(&pSession->Mutex);

    return STATUS_SUCCESS;
}

void vboxVideoCmCtxInitEmpty(PVBOXVIDEOCM_CTX pContext)
{
    InitializeListHead(&pContext->SessionEntry);
    pContext->pSession = NULL;
    pContext->u64UmData = 0ULL;
}

static void vboxVideoCmSessionCtxAddLocked(PVBOXVIDEOCM_SESSION pSession, PVBOXVIDEOCM_CTX pContext)
{
    InsertHeadList(&pSession->ContextList, &pContext->SessionEntry);
    pContext->pSession = pSession;
}

void vboxVideoCmSessionCtxAdd(PVBOXVIDEOCM_SESSION pSession, PVBOXVIDEOCM_CTX pContext)
{
    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
    ExAcquireFastMutex(&pSession->Mutex);
    vboxVideoCmSessionCtxAddLocked(pSession, pContext);
    ExReleaseFastMutex(&pSession->Mutex);

}

static void vboxVideoCmSessionDestroyLocked(PVBOXVIDEOCM_SESSION pSession)
{
    /* signal event so that user-space client can figure out the context is destroyed
     * in case the context destroyal is caused by Graphics device reset or miniport driver update */
    KeSetEvent(pSession->pUmEvent, 0, FALSE);
    ObDereferenceObject(pSession->pUmEvent);
    Assert(IsListEmpty(&pSession->ContextList));
    Assert(IsListEmpty(&pSession->CommandsList));
    RemoveEntryList(&pSession->QueueEntry);
    vboxWddmMemFree(pSession);
}

/**
 * @return true iff the given session is destroyed
 */
bool vboxVideoCmSessionCtxRemoveLocked(PVBOXVIDEOCM_SESSION pSession, PVBOXVIDEOCM_CTX pContext)
{
    bool bDestroy;
    LIST_ENTRY RemainedList;
    LIST_ENTRY *pCur;
    LIST_ENTRY *pPrev;
    InitializeListHead(&RemainedList);
    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
    ExAcquireFastMutex(&pSession->Mutex);
    pContext->pSession = NULL;
    RemoveEntryList(&pContext->SessionEntry);
    bDestroy = !!(IsListEmpty(&pSession->ContextList));
    /* ensure there are no commands left for the given context */
    if (bDestroy)
    {
        vboxVideoLeDetach(&pSession->CommandsList, &RemainedList);
    }
    else
    {
        pCur = pSession->CommandsList.Flink;
        pPrev = &pSession->CommandsList;
        while (pCur != &pSession->CommandsList)
        {
            PVBOXVIDEOCM_CMD_DR pCmd = VBOXCMENTRY_2_CMD(pCur);
            if (pCmd->pContext == pContext)
            {
                RemoveEntryList(pCur);
                InsertHeadList(&RemainedList, pCur);
                pCur = pPrev;
                /* pPrev - remains unchanged */
            }
            else
            {
                pPrev = pCur;
            }
            pCur = pCur->Flink;
        }
    }
    ExReleaseFastMutex(&pSession->Mutex);

    for (pCur = RemainedList.Flink; pCur != &RemainedList; pCur = RemainedList.Flink)
    {
        RemoveEntryList(pCur);
        PVBOXVIDEOCM_CMD_DR pCmd = VBOXCMENTRY_2_CMD(pCur);
        vboxVideoCmCmdCancel(pCmd);
    }

    if (bDestroy)
    {
        vboxVideoCmSessionDestroyLocked(pSession);
    }

    return bDestroy;
}

/* the session gets destroyed once the last context is removed from it */
NTSTATUS vboxVideoCmSessionCreateLocked(PVBOXVIDEOCM_MGR pMgr, PVBOXVIDEOCM_SESSION *ppSession, PKEVENT pUmEvent, PVBOXVIDEOCM_CTX pContext)
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    PVBOXVIDEOCM_SESSION pSession = (PVBOXVIDEOCM_SESSION)vboxWddmMemAllocZero(sizeof (VBOXVIDEOCM_SESSION));
    Assert(pSession);
    if (pSession)
    {
        InitializeListHead(&pSession->ContextList);
        InitializeListHead(&pSession->CommandsList);
        pSession->pUmEvent = pUmEvent;
        Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
        ExInitializeFastMutex(&pSession->Mutex);
        pSession->bEventNeeded = true;
        vboxVideoCmSessionCtxAddLocked(pSession, pContext);
        InsertHeadList(&pMgr->SessionList, &pSession->QueueEntry);
        *ppSession = pSession;
        return STATUS_SUCCESS;
//        vboxWddmMemFree(pSession);
    }
    else
    {
        Status = STATUS_NO_MEMORY;
    }
    return Status;
}

#define VBOXCMENTRY_2_SESSION(_pE) ((PVBOXVIDEOCM_SESSION)((uint8_t*)(_pE) - RT_OFFSETOF(VBOXVIDEOCM_SESSION, QueueEntry)))

NTSTATUS vboxVideoCmCtxAdd(PVBOXVIDEOCM_MGR pMgr, PVBOXVIDEOCM_CTX pContext, HANDLE hUmEvent, uint64_t u64UmData)
{
    PKEVENT pUmEvent = NULL;
    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);
    NTSTATUS Status = ObReferenceObjectByHandle(hUmEvent, EVENT_MODIFY_STATE, *ExEventObjectType, UserMode,
        (PVOID*)&pUmEvent,
        NULL);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = KeWaitForSingleObject(&pMgr->SynchEvent, Executive, KernelMode,
                FALSE, /* BOOLEAN Alertable */
                NULL /* PLARGE_INTEGER Timeout */
            );
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            bool bFound = false;
            PVBOXVIDEOCM_SESSION pSession = NULL;
            for (PLIST_ENTRY pEntry = pMgr->SessionList.Flink; pEntry != &pMgr->SessionList; pEntry = pEntry->Flink)
            {
                pSession = VBOXCMENTRY_2_SESSION(pEntry);
                if (pSession->pUmEvent == pUmEvent)
                {
                    bFound = true;
                    break;
                }
            }

            pContext->u64UmData = u64UmData;

            if (!bFound)
            {
                Status = vboxVideoCmSessionCreateLocked(pMgr, &pSession, pUmEvent, pContext);
                Assert(Status == STATUS_SUCCESS);
            }
            else
            {
                /* Status = */vboxVideoCmSessionCtxAdd(pSession, pContext);
                /*Assert(Status == STATUS_SUCCESS);*/
            }
            LONG tstL = KeSetEvent(&pMgr->SynchEvent, 0, FALSE);
            Assert(!tstL);

            if (Status == STATUS_SUCCESS)
            {
                return STATUS_SUCCESS;
            }
        }

        ObDereferenceObject(pUmEvent);
    }
    return Status;
}

NTSTATUS vboxVideoCmCtxRemove(PVBOXVIDEOCM_MGR pMgr, PVBOXVIDEOCM_CTX pContext)
{
    PVBOXVIDEOCM_SESSION pSession = pContext->pSession;
    if (!pSession)
        return STATUS_SUCCESS;

    NTSTATUS Status = KeWaitForSingleObject(&pMgr->SynchEvent, Executive, KernelMode,
                FALSE, /* BOOLEAN Alertable */
                NULL /* PLARGE_INTEGER Timeout */
    );
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        vboxVideoCmSessionCtxRemoveLocked(pSession, pContext);
        LONG tstL = KeSetEvent(&pMgr->SynchEvent, 0, FALSE);
        Assert(!tstL);
    }

    return Status;
}

NTSTATUS vboxVideoCmInit(PVBOXVIDEOCM_MGR pMgr)
{
    KeInitializeEvent(&pMgr->SynchEvent, SynchronizationEvent, TRUE);
    InitializeListHead(&pMgr->SessionList);
    return STATUS_SUCCESS;
}

NTSTATUS vboxVideoCmTerm(PVBOXVIDEOCM_MGR pMgr)
{
    Assert(IsListEmpty(&pMgr->SessionList));
    return STATUS_SUCCESS;
}

NTSTATUS vboxVideoCmEscape(PVBOXVIDEOCM_CTX pContext, PVBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD pCmd, uint32_t cbCmd)
{
    Assert(cbCmd >= sizeof (VBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD));
    if (cbCmd < sizeof (VBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD))
        return STATUS_BUFFER_TOO_SMALL;

    PVBOXVIDEOCM_SESSION pSession = pContext->pSession;
    PVBOXVIDEOCM_CMD_DR pHdr;
    LIST_ENTRY DetachedList;
    PLIST_ENTRY pCurEntry = NULL;
    uint32_t cbCmdsReturned = 0;
    uint32_t cbRemainingCmds = 0;
    uint32_t cbRemainingFirstCmd = 0;
    uint32_t cbData = cbCmd - sizeof (VBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD);
    uint8_t * pvData = ((uint8_t *)pCmd) + sizeof (VBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD);
    bool bDetachMode = true;
    InitializeListHead(&DetachedList);
//    PVBOXWDDM_GETVBOXVIDEOCMCMD_HDR *pvCmd

    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
    ExAcquireFastMutex(&pSession->Mutex);

    do
    {
        if (bDetachMode)
        {
            if (!IsListEmpty(&pSession->CommandsList))
            {
                Assert(!pCurEntry);
                pHdr = VBOXCMENTRY_2_CMD(pSession->CommandsList.Blink);
                Assert(pHdr->CmdHdr.cbCmd);
                if (cbData >= pHdr->CmdHdr.cbCmd)
                {
                    RemoveEntryList(&pHdr->QueueList);
                    InsertHeadList(&DetachedList, &pHdr->QueueList);
                    cbData -= pHdr->CmdHdr.cbCmd;
                }
                else
                {
                    cbRemainingFirstCmd = pHdr->CmdHdr.cbCmd;
                    cbRemainingCmds = pHdr->CmdHdr.cbCmd;
                    pCurEntry = pHdr->QueueList.Blink;
                    bDetachMode = false;
                }
            }
            else
            {
                pSession->bEventNeeded = true;
                break;
            }
        }
        else
        {
            Assert(pCurEntry);
            if (pCurEntry != &pSession->CommandsList)
            {
                pHdr = VBOXCMENTRY_2_CMD(pCurEntry);
                Assert(cbRemainingFirstCmd);
                cbRemainingCmds += pHdr->CmdHdr.cbCmd;
                pCurEntry = pHdr->QueueList.Blink;
            }
            else
            {
                Assert(cbRemainingFirstCmd);
                Assert(cbRemainingCmds);
                break;
            }
        }
    } while (1);

    ExReleaseFastMutex(&pSession->Mutex);

    pCmd->Hdr.cbCmdsReturned = 0;
    for (pCurEntry = DetachedList.Blink; pCurEntry != &DetachedList; pCurEntry = DetachedList.Blink)
    {
        pHdr = VBOXCMENTRY_2_CMD(pCurEntry);
        memcpy(pvData, &pHdr->CmdHdr, pHdr->CmdHdr.cbCmd);
        pvData += pHdr->CmdHdr.cbCmd;
        pCmd->Hdr.cbCmdsReturned += pHdr->CmdHdr.cbCmd;
        RemoveEntryList(pCurEntry);
        vboxVideoCmCmdReleaseByHdr(pHdr);
    }

    pCmd->Hdr.cbRemainingCmds = cbRemainingCmds;
    pCmd->Hdr.cbRemainingFirstCmd = cbRemainingFirstCmd;
    pCmd->Hdr.u32Reserved = 0;

    return STATUS_SUCCESS;
}

VOID vboxVideoCmLock(PVBOXVIDEOCM_CTX pContext)
{
    ExAcquireFastMutex(&pContext->pSession->Mutex);
}

VOID vboxVideoCmUnlock(PVBOXVIDEOCM_CTX pContext)
{
    ExReleaseFastMutex(&pContext->pSession->Mutex);
}

static BOOLEAN vboxVideoCmHasUncompletedCmdsLocked(PVBOXVIDEOCM_MGR pMgr)
{
    PVBOXVIDEOCM_SESSION pSession = NULL;
    for (PLIST_ENTRY pEntry = pMgr->SessionList.Flink; pEntry != &pMgr->SessionList; pEntry = pEntry->Flink)
    {
        pSession = VBOXCMENTRY_2_SESSION(pEntry);
        ExAcquireFastMutex(&pSession->Mutex);
        if (pSession->bEventNeeded)
        {
            /* commands still being processed */
            ExReleaseFastMutex(&pSession->Mutex);
            return TRUE;
        }
        ExReleaseFastMutex(&pSession->Mutex);
    }
    return FALSE;
}

/* waits for all outstanding commands to completed by client
 * assumptions here are:
 * 1. no new commands are submitted while we are waiting
 * 2. it is assumed that a client completes all previously received commands
 *    once it queries for the new set of commands */
NTSTATUS vboxVideoCmWaitCompletedCmds(PVBOXVIDEOCM_MGR pMgr, uint32_t msTimeout)
{
    LARGE_INTEGER Timeout;
    PLARGE_INTEGER pTimeout;
    uint32_t cIters;

    if (msTimeout != RT_INDEFINITE_WAIT)
    {
        uint32_t msIter = 2;
        cIters = msTimeout/msIter;
        if (!cIters)
        {
            msIter = msTimeout;
            cIters = 1;
        }
        Timeout.QuadPart = -(int64_t) msIter /* ms */ * 10000;
        pTimeout = &Timeout;
    }
    else
    {
        pTimeout = NULL;
        cIters = 1;
    }

    Assert(cIters);
    do
    {
        NTSTATUS Status = KeWaitForSingleObject(&pMgr->SynchEvent, Executive, KernelMode,
                    FALSE, /* BOOLEAN Alertable */
                    pTimeout /* PLARGE_INTEGER Timeout */
        );
        if (Status == STATUS_TIMEOUT)
        {
            --cIters;
        }
        else
        {
            if (!NT_SUCCESS(Status))
            {
                WARN(("KeWaitForSingleObject failed with Status (0x%x)", Status));
                return Status;
            }

            /* succeeded */
            if (!vboxVideoCmHasUncompletedCmdsLocked(pMgr))
            {
                LONG tstL = KeSetEvent(&pMgr->SynchEvent, 0, FALSE);
                Assert(!tstL);
                return STATUS_SUCCESS;
            }

            LONG tstL = KeSetEvent(&pMgr->SynchEvent, 0, FALSE);
            Assert(!tstL);
        }

        if (!cIters)
            break;

        KeDelayExecutionThread(KernelMode, FALSE, pTimeout);
        --cIters;
        if (!cIters)
            break;
    } while (0);

    return STATUS_TIMEOUT;
}

