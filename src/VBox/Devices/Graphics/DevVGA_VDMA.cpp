/** @file
 * Video DMA (VDMA) support.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
//#include <VBox/VMMDev.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/VBoxVideo.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/mem.h>
#include <iprt/asm.h>
#include <iprt/list.h>

#include "DevVGA.h"
#include "HGSMI/SHGSMIHost.h"

#include <VBox/VBoxVideo3D.h>
#include <VBox/VBoxVideoHost3D.h>

#ifdef DEBUG_misha
# define VBOXVDBG_MEMCACHE_DISABLE
#endif

#ifndef VBOXVDBG_MEMCACHE_DISABLE
# include <iprt/memcache.h>
#endif

#ifdef DEBUG_misha
#define WARN_BP() do { AssertFailed(); } while (0)
#else
#define WARN_BP() do { } while (0)
#endif
#define WARN(_msg) do { \
        LogRel(_msg); \
        WARN_BP(); \
    } while (0)

#define VBOXVDMATHREAD_STATE_TERMINATED     0
#define VBOXVDMATHREAD_STATE_CREATED        1
#define VBOXVDMATHREAD_STATE_TERMINATING    2

typedef struct VBOXVDMATHREAD
{
    RTTHREAD hWorkerThread;
    RTSEMEVENT hEvent;
    RTSEMEVENT hClientEvent;
    volatile uint32_t u32State;
} VBOXVDMATHREAD, *PVBOXVDMATHREAD;


/* state transformations:
 *
 *   submitter   |    processor
 *
 *  LISTENING   --->  PROCESSING
 *
 *  */
#define VBVAEXHOSTCONTEXT_STATE_LISTENING      0
#define VBVAEXHOSTCONTEXT_STATE_PROCESSING     1

#define VBVAEXHOSTCONTEXT_ESTATE_DISABLED     -1
#define VBVAEXHOSTCONTEXT_ESTATE_PAUSED        0
#define VBVAEXHOSTCONTEXT_ESTATE_ENABLED       1

typedef struct VBVAEXHOSTCONTEXT
{
    VBVABUFFER *pVBVA;
    volatile int32_t i32State;
    volatile int32_t i32EnableState;
    volatile uint32_t u32cCtls;
    /* critical section for accessing ctl lists */
    RTCRITSECT CltCritSect;
    RTLISTANCHOR GuestCtlList;
    RTLISTANCHOR HostCtlList;
#ifndef VBOXVDBG_MEMCACHE_DISABLE
    RTMEMCACHE CtlCache;
#endif
} VBVAEXHOSTCONTEXT;

typedef enum
{
    VBVAEXHOSTCTL_TYPE_UNDEFINED = 0,
    VBVAEXHOSTCTL_TYPE_HH_INTERNAL_PAUSE,
    VBVAEXHOSTCTL_TYPE_HH_INTERNAL_RESUME,
    VBVAEXHOSTCTL_TYPE_HH_ENABLE,
    VBVAEXHOSTCTL_TYPE_HH_TERM,
    VBVAEXHOSTCTL_TYPE_HH_RESET,
    VBVAEXHOSTCTL_TYPE_HH_SAVESTATE,
    VBVAEXHOSTCTL_TYPE_HH_LOADSTATE,
    VBVAEXHOSTCTL_TYPE_HH_BE_OPAQUE,
    VBVAEXHOSTCTL_TYPE_GHH_BE_OPAQUE,
    VBVAEXHOSTCTL_TYPE_GH_ENABLE_DISABLE
} VBVAEXHOSTCTL_TYPE;

struct VBVAEXHOSTCTL;

typedef DECLCALLBACKPTR(void, PFNVBVAEXHOSTCTL_COMPLETE)(VBVAEXHOSTCONTEXT *pVbva, struct VBVAEXHOSTCTL *pCtl, int rc, void *pvComplete);

typedef struct VBVAEXHOSTCTL
{
    RTLISTNODE Node;
    VBVAEXHOSTCTL_TYPE enmType;
    union
    {
        struct
        {
            uint8_t * pu8Cmd;
            uint32_t cbCmd;
        } cmd;

        struct
        {
            PSSMHANDLE pSSM;
            uint32_t u32Version;
        } state;
    } u;
    PFNVBVAEXHOSTCTL_COMPLETE pfnComplete;
    void *pvComplete;
} VBVAEXHOSTCTL;

/* VBoxVBVAExHP**, i.e. processor functions, can NOT be called concurrently with each other,
 * but can be called with other VBoxVBVAExS** (submitter) functions except Init/Start/Term aparently.
 * Can only be called be the processor, i.e. the entity that acquired the processor state by direct or indirect call to the VBoxVBVAExHSCheckCommands
 * see mor edetailed comments in headers for function definitions */
typedef enum
{
    VBVAEXHOST_DATA_TYPE_NO_DATA = 0,
    VBVAEXHOST_DATA_TYPE_CMD,
    VBVAEXHOST_DATA_TYPE_HOSTCTL,
    VBVAEXHOST_DATA_TYPE_GUESTCTL
} VBVAEXHOST_DATA_TYPE;
static VBVAEXHOST_DATA_TYPE VBoxVBVAExHPDataGet(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint8_t **ppCmd, uint32_t *pcbCmd);

static void VBoxVBVAExHPDataCompleteCmd(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint32_t cbCmd);
static void VBoxVBVAExHPDataCompleteCtl(struct VBVAEXHOSTCONTEXT *pCmdVbva, VBVAEXHOSTCTL *pCtl, int rc);

/* VBoxVBVAExHP**, i.e. processor functions, can NOT be called concurrently with each other,
 * can be called concurrently with istelf as well as with other VBoxVBVAEx** functions except Init/Start/Term aparently */
static int VBoxVBVAExHSCheckCommands(struct VBVAEXHOSTCONTEXT *pCmdVbva);

static int VBoxVBVAExHSInit(struct VBVAEXHOSTCONTEXT *pCmdVbva);
static int VBoxVBVAExHSEnable(struct VBVAEXHOSTCONTEXT *pCmdVbva, VBVABUFFER *pVBVA);
static int VBoxVBVAExHSDisable(struct VBVAEXHOSTCONTEXT *pCmdVbva);
static void VBoxVBVAExHSTerm(struct VBVAEXHOSTCONTEXT *pCmdVbva);
static int VBoxVBVAExHSSaveState(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint8_t* pu8VramBase, PSSMHANDLE pSSM);
static int VBoxVBVAExHSLoadState(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint8_t* pu8VramBase, PSSMHANDLE pSSM, uint32_t u32Version);

static VBVAEXHOSTCTL* VBoxVBVAExHCtlAlloc(VBVAEXHOSTCONTEXT *pCmdVbva)
{
#ifndef VBOXVDBG_MEMCACHE_DISABLE
    return (VBVAEXHOSTCTL*)RTMemCacheAlloc(pCmdVbva->CtlCache);
#else
    return (VBVAEXHOSTCTL*)RTMemAlloc(sizeof (VBVAEXHOSTCTL));
#endif
}

static void VBoxVBVAExHCtlFree(VBVAEXHOSTCONTEXT *pCmdVbva, VBVAEXHOSTCTL *pCtl)
{
#ifndef VBOXVDBG_MEMCACHE_DISABLE
    RTMemCacheFree(pCmdVbva->CtlCache, pCtl);
#else
    RTMemFree(pCtl);
#endif
}

static VBVAEXHOSTCTL* VBoxVBVAExHCtlCreate(VBVAEXHOSTCONTEXT *pCmdVbva, VBVAEXHOSTCTL_TYPE enmType)
{
    VBVAEXHOSTCTL* pCtl = VBoxVBVAExHCtlAlloc(pCmdVbva);
    if (!pCtl)
    {
        WARN(("VBoxVBVAExHCtlAlloc failed\n"));
        return NULL;
    }

    pCtl->enmType = enmType;
    return pCtl;
}

static int vboxVBVAExHSProcessorAcquire(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    Assert(pCmdVbva->i32State >= VBVAEXHOSTCONTEXT_STATE_LISTENING);

    if (ASMAtomicCmpXchgS32(&pCmdVbva->i32State, VBVAEXHOSTCONTEXT_STATE_PROCESSING, VBVAEXHOSTCONTEXT_STATE_LISTENING))
            return VINF_SUCCESS;
    return VERR_SEM_BUSY;
}

static VBVAEXHOSTCTL* vboxVBVAExHPCheckCtl(struct VBVAEXHOSTCONTEXT *pCmdVbva, bool *pfHostCtl, bool fHostOnlyMode)
{
    Assert(pCmdVbva->i32State == VBVAEXHOSTCONTEXT_STATE_PROCESSING);

    if(!fHostOnlyMode && !ASMAtomicUoReadU32(&pCmdVbva->u32cCtls))
        return NULL;

    int rc = RTCritSectEnter(&pCmdVbva->CltCritSect);
    if (RT_SUCCESS(rc))
    {
        VBVAEXHOSTCTL* pCtl = RTListGetFirst(&pCmdVbva->HostCtlList, VBVAEXHOSTCTL, Node);
        if (pCtl)
            *pfHostCtl = true;
        else if (!fHostOnlyMode)
        {
            if (ASMAtomicUoReadS32(&pCmdVbva->i32EnableState) > VBVAEXHOSTCONTEXT_ESTATE_PAUSED)
            {
                pCtl = RTListGetFirst(&pCmdVbva->GuestCtlList, VBVAEXHOSTCTL, Node);
                /* pCtl can not be null here since pCmdVbva->u32cCtls is not null,
                 * and there are no HostCtl commands*/
                Assert(pCtl);
                *pfHostCtl = false;
            }
        }

        if (pCtl)
        {
            RTListNodeRemove(&pCtl->Node);
            ASMAtomicDecU32(&pCmdVbva->u32cCtls);
        }

        RTCritSectLeave(&pCmdVbva->CltCritSect);

        return pCtl;
    }
    else
        WARN(("RTCritSectEnter failed %d\n", rc));

    return NULL;
}

static VBVAEXHOSTCTL* VBoxVBVAExHPCheckHostCtlOnDisable(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    bool fHostCtl;
    return vboxVBVAExHPCheckCtl(pCmdVbva, &fHostCtl, true);
}


static bool vboxVBVAExHPCheckProcessCtlInternal(struct VBVAEXHOSTCONTEXT *pCmdVbva, VBVAEXHOSTCTL* pCtl)
{
    switch (pCtl->enmType)
    {
        case VBVAEXHOSTCTL_TYPE_HH_INTERNAL_PAUSE:
            if (pCmdVbva->i32EnableState > VBVAEXHOSTCONTEXT_ESTATE_PAUSED)
                ASMAtomicWriteS32(&pCmdVbva->i32EnableState, VBVAEXHOSTCONTEXT_ESTATE_PAUSED);
            return true;
        case VBVAEXHOSTCTL_TYPE_HH_INTERNAL_RESUME:
            if (pCmdVbva->i32EnableState == VBVAEXHOSTCONTEXT_ESTATE_PAUSED)
                ASMAtomicWriteS32(&pCmdVbva->i32EnableState, VBVAEXHOSTCONTEXT_ESTATE_ENABLED);
            return true;
        default:
            return false;
    }
}

static void vboxVBVAExHPProcessorRelease(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    Assert(pCmdVbva->i32State == VBVAEXHOSTCONTEXT_STATE_PROCESSING);

    ASMAtomicWriteS32(&pCmdVbva->i32State, VBVAEXHOSTCONTEXT_STATE_LISTENING);
}

static void vboxVBVAExHPHgEventSet(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    Assert(pCmdVbva->i32State == VBVAEXHOSTCONTEXT_STATE_PROCESSING);
    if (pCmdVbva->pVBVA)
        ASMAtomicOrU32(&pCmdVbva->pVBVA->hostFlags.u32HostEvents, VBVA_F_STATE_PROCESSING);
}

static void vboxVBVAExHPHgEventClear(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    Assert(pCmdVbva->i32State == VBVAEXHOSTCONTEXT_STATE_PROCESSING);
    if (pCmdVbva->pVBVA)
        ASMAtomicAndU32(&pCmdVbva->pVBVA->hostFlags.u32HostEvents, ~VBVA_F_STATE_PROCESSING);
}

static int vboxVBVAExHPCmdGet(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint8_t **ppCmd, uint32_t *pcbCmd)
{
    Assert(pCmdVbva->i32State == VBVAEXHOSTCONTEXT_STATE_PROCESSING);
    Assert(pCmdVbva->i32EnableState > VBVAEXHOSTCONTEXT_ESTATE_PAUSED);

    VBVABUFFER *pVBVA = pCmdVbva->pVBVA;

    uint32_t indexRecordFirst = pVBVA->indexRecordFirst;
    uint32_t indexRecordFree = pVBVA->indexRecordFree;

    Log(("first = %d, free = %d\n",
                   indexRecordFirst, indexRecordFree));

    if (indexRecordFirst == indexRecordFree)
    {
        /* No records to process. Return without assigning output variables. */
        return VINF_EOF;
    }

    uint32_t cbRecordCurrent = ASMAtomicReadU32(&pVBVA->aRecords[indexRecordFirst].cbRecord);

    /* A new record need to be processed. */
    if (cbRecordCurrent & VBVA_F_RECORD_PARTIAL)
    {
        /* the record is being recorded, try again */
        return VINF_TRY_AGAIN;
    }

    uint32_t cbRecord = cbRecordCurrent & ~VBVA_F_RECORD_PARTIAL;

    if (!cbRecord)
    {
        /* the record is being recorded, try again */
        return VINF_TRY_AGAIN;
    }

    /* we should not get partial commands here actually */
    Assert(cbRecord);

    /* The size of largest contiguous chunk in the ring biffer. */
    uint32_t u32BytesTillBoundary = pVBVA->cbData - pVBVA->off32Data;

    /* The pointer to data in the ring buffer. */
    uint8_t *pSrc = &pVBVA->au8Data[pVBVA->off32Data];

    /* Fetch or point the data. */
    if (u32BytesTillBoundary >= cbRecord)
    {
        /* The command does not cross buffer boundary. Return address in the buffer. */
        *ppCmd = pSrc;
        *pcbCmd = cbRecord;
        return VINF_SUCCESS;
    }

    LogRel(("CmdVbva: cross-bound writes unsupported\n"));
    return VERR_INVALID_STATE;
}

static void VBoxVBVAExHPDataCompleteCmd(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint32_t cbCmd)
{
    VBVABUFFER *pVBVA = pCmdVbva->pVBVA;
    pVBVA->off32Data = (pVBVA->off32Data + cbCmd) % pVBVA->cbData;

    pVBVA->indexRecordFirst = (pVBVA->indexRecordFirst + 1) % RT_ELEMENTS(pVBVA->aRecords);
}

static void VBoxVBVAExHPDataCompleteCtl(struct VBVAEXHOSTCONTEXT *pCmdVbva, VBVAEXHOSTCTL *pCtl, int rc)
{
    if (pCtl->pfnComplete)
        pCtl->pfnComplete(pCmdVbva, pCtl, rc, pCtl->pvComplete);
    else
        VBoxVBVAExHCtlFree(pCmdVbva, pCtl);
}

static VBVAEXHOST_DATA_TYPE vboxVBVAExHPDataGet(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint8_t **ppCmd, uint32_t *pcbCmd)
{
    Assert(pCmdVbva->i32State == VBVAEXHOSTCONTEXT_STATE_PROCESSING);
    VBVAEXHOSTCTL*pCtl;
    bool fHostClt;

    for(;;)
    {
        pCtl = vboxVBVAExHPCheckCtl(pCmdVbva, &fHostClt, false);
        if (pCtl)
        {
            if (fHostClt)
            {
                if (!vboxVBVAExHPCheckProcessCtlInternal(pCmdVbva, pCtl))
                {
                    *ppCmd = (uint8_t*)pCtl;
                    *pcbCmd = sizeof (*pCtl);
                    return VBVAEXHOST_DATA_TYPE_HOSTCTL;
                }
            }
            else
            {
                *ppCmd = (uint8_t*)pCtl;
                *pcbCmd = sizeof (*pCtl);
                return VBVAEXHOST_DATA_TYPE_GUESTCTL;
            }
        }

        if (ASMAtomicUoReadS32(&pCmdVbva->i32EnableState) <= VBVAEXHOSTCONTEXT_ESTATE_PAUSED)
            return VBVAEXHOST_DATA_TYPE_NO_DATA;

        int rc = vboxVBVAExHPCmdGet(pCmdVbva, ppCmd, pcbCmd);
        switch (rc)
        {
            case VINF_SUCCESS:
                return VBVAEXHOST_DATA_TYPE_CMD;
            case VINF_EOF:
                return VBVAEXHOST_DATA_TYPE_NO_DATA;
            case VINF_TRY_AGAIN:
                RTThreadSleep(1);
                continue;
            default:
                /* this is something really unexpected, i.e. most likely guest has written something incorrect to the VBVA buffer */
                WARN(("Warning: vboxVBVAExHCmdGet returned unexpected status %d\n", rc));
                return VBVAEXHOST_DATA_TYPE_NO_DATA;
        }
    }

    WARN(("Warning: VBoxVBVAExHCmdGet unexpected state\n"));
    return VBVAEXHOST_DATA_TYPE_NO_DATA;
}

static VBVAEXHOST_DATA_TYPE VBoxVBVAExHPDataGet(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint8_t **ppCmd, uint32_t *pcbCmd)
{
    VBVAEXHOST_DATA_TYPE enmType = vboxVBVAExHPDataGet(pCmdVbva, ppCmd, pcbCmd);
    if (enmType == VBVAEXHOST_DATA_TYPE_NO_DATA)
    {
        vboxVBVAExHPHgEventClear(pCmdVbva);
        vboxVBVAExHPProcessorRelease(pCmdVbva);
        /* we need to prevent racing between us clearing the flag and command check/submission thread, i.e.
         * 1. we check the queue -> and it is empty
         * 2. submitter adds command to the queue
         * 3. submitter checks the "processing" -> and it is true , thus it does not submit a notification
         * 4. we clear the "processing" state
         * 5. ->here we need to re-check the queue state to ensure we do not leak the notification of the above command
         * 6. if the queue appears to be not-empty set the "processing" state back to "true"
         **/
        int rc = vboxVBVAExHSProcessorAcquire(pCmdVbva);
        if (RT_SUCCESS(rc))
        {
            /* we are the processor now */
            enmType = vboxVBVAExHPDataGet(pCmdVbva, ppCmd, pcbCmd);
            if (enmType == VBVAEXHOST_DATA_TYPE_NO_DATA)
            {
                vboxVBVAExHPProcessorRelease(pCmdVbva);
                return VBVAEXHOST_DATA_TYPE_NO_DATA;
            }

            vboxVBVAExHPHgEventSet(pCmdVbva);
        }
    }

    return enmType;
}

DECLINLINE(bool) vboxVBVAExHSHasCommands(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    VBVABUFFER *pVBVA = pCmdVbva->pVBVA;

    if (pVBVA)
    {
        uint32_t indexRecordFirst = pVBVA->indexRecordFirst;
        uint32_t indexRecordFree = pVBVA->indexRecordFree;

        if (indexRecordFirst != indexRecordFree)
            return true;
    }

    return !!ASMAtomicReadU32(&pCmdVbva->u32cCtls);
}

/* Checks whether the new commands are ready for processing
 * @returns
 *   VINF_SUCCESS - there are commands are in a queue, and the given thread is now the processor (i.e. typically it would delegate processing to a worker thread)
 *   VINF_EOF - no commands in a queue
 *   VINF_ALREADY_INITIALIZED - another thread already processing the commands
 *   VERR_INVALID_STATE - the VBVA is paused or pausing */
static int VBoxVBVAExHSCheckCommands(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    int rc = vboxVBVAExHSProcessorAcquire(pCmdVbva);
    if (RT_SUCCESS(rc))
    {
        /* we are the processor now */
        if (vboxVBVAExHSHasCommands(pCmdVbva))
        {
            vboxVBVAExHPHgEventSet(pCmdVbva);
            return VINF_SUCCESS;
        }

        vboxVBVAExHPProcessorRelease(pCmdVbva);
        return VINF_EOF;
    }
    if (rc == VERR_SEM_BUSY)
        return VINF_ALREADY_INITIALIZED;
    return VERR_INVALID_STATE;
}

static int VBoxVBVAExHSInit(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    memset(pCmdVbva, 0, sizeof (*pCmdVbva));
    int rc = RTCritSectInit(&pCmdVbva->CltCritSect);
    if (RT_SUCCESS(rc))
    {
#ifndef VBOXVDBG_MEMCACHE_DISABLE
        rc = RTMemCacheCreate(&pCmdVbva->CtlCache, sizeof (VBVAEXHOSTCTL),
                                0, /* size_t cbAlignment */
                                UINT32_MAX, /* uint32_t cMaxObjects */
                                NULL, /* PFNMEMCACHECTOR pfnCtor*/
                                NULL, /* PFNMEMCACHEDTOR pfnDtor*/
                                NULL, /* void *pvUser*/
                                0 /* uint32_t fFlags*/
                                );
        if (RT_SUCCESS(rc))
#endif
        {
            RTListInit(&pCmdVbva->GuestCtlList);
            RTListInit(&pCmdVbva->HostCtlList);
            pCmdVbva->i32State = VBVAEXHOSTCONTEXT_STATE_PROCESSING;
            pCmdVbva->i32EnableState = VBVAEXHOSTCONTEXT_ESTATE_DISABLED;
            return VINF_SUCCESS;
        }
#ifndef VBOXVDBG_MEMCACHE_DISABLE
        else
            WARN(("RTMemCacheCreate failed %d\n", rc));
#endif
    }
    else
        WARN(("RTCritSectInit failed %d\n", rc));

    return rc;
}

DECLINLINE(bool) VBoxVBVAExHSIsEnabled(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    return (ASMAtomicUoReadS32(&pCmdVbva->i32EnableState) >= VBVAEXHOSTCONTEXT_ESTATE_PAUSED);
}

static int VBoxVBVAExHSEnable(struct VBVAEXHOSTCONTEXT *pCmdVbva, VBVABUFFER *pVBVA)
{
    if (VBoxVBVAExHSIsEnabled(pCmdVbva))
        return VINF_ALREADY_INITIALIZED;

    pCmdVbva->pVBVA = pVBVA;
    pCmdVbva->pVBVA->hostFlags.u32HostEvents = 0;
    ASMAtomicWriteS32(&pCmdVbva->i32EnableState, VBVAEXHOSTCONTEXT_ESTATE_ENABLED);
    return VINF_SUCCESS;
}

static int VBoxVBVAExHSDisable(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    if (!VBoxVBVAExHSIsEnabled(pCmdVbva))
        return VINF_SUCCESS;

    ASMAtomicWriteS32(&pCmdVbva->i32EnableState, VBVAEXHOSTCONTEXT_ESTATE_DISABLED);
    return VINF_SUCCESS;
}

static void VBoxVBVAExHSTerm(struct VBVAEXHOSTCONTEXT *pCmdVbva)
{
    /* ensure the processor is stopped */
    Assert(pCmdVbva->i32State >= VBVAEXHOSTCONTEXT_STATE_LISTENING);

    /* ensure no one tries to submit the command */
    if (pCmdVbva->pVBVA)
        pCmdVbva->pVBVA->hostFlags.u32HostEvents = 0;

    Assert(RTListIsEmpty(&pCmdVbva->GuestCtlList));
    Assert(RTListIsEmpty(&pCmdVbva->HostCtlList));

    RTCritSectDelete(&pCmdVbva->CltCritSect);

#ifndef VBOXVDBG_MEMCACHE_DISABLE
    RTMemCacheDestroy(pCmdVbva->CtlCache);
#endif

    memset(pCmdVbva, 0, sizeof (*pCmdVbva));
}

/* Saves state
 * @returns - same as VBoxVBVAExHSCheckCommands, or failure on load state fail
 */
static int VBoxVBVAExHSSaveState(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint8_t* pu8VramBase, PSSMHANDLE pSSM)
{
    int rc;

    int32_t i32EnableState = ASMAtomicUoReadS32(&pCmdVbva->i32EnableState);
    if (i32EnableState >= VBVAEXHOSTCONTEXT_ESTATE_PAUSED)
    {
        if (i32EnableState != VBVAEXHOSTCONTEXT_ESTATE_PAUSED)
        {
            WARN(("vbva not paused\n"));
            return VERR_INVALID_STATE;
        }

        rc = SSMR3PutU32(pSSM, (uint32_t)(((uint8_t*)pCmdVbva->pVBVA) - pu8VramBase));
        AssertRCReturn(rc, rc);
        return VINF_SUCCESS;
    }

    rc = SSMR3PutU32(pSSM, 0xffffffff);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

typedef enum
{
    VBVAEXHOSTCTL_SOURCE_GUEST = 0,
    VBVAEXHOSTCTL_SOURCE_HOST_ANY,
    VBVAEXHOSTCTL_SOURCE_HOST_ENABLED
} VBVAEXHOSTCTL_SOURCE;


static int VBoxVBVAExHCtlSubmit(VBVAEXHOSTCONTEXT *pCmdVbva, VBVAEXHOSTCTL* pCtl, VBVAEXHOSTCTL_SOURCE enmSource, PFNVBVAEXHOSTCTL_COMPLETE pfnComplete, void *pvComplete)
{
    if ((enmSource == VBVAEXHOSTCTL_SOURCE_HOST_ENABLED) && !VBoxVBVAExHSIsEnabled(pCmdVbva))
    {
        WARN(("cmd vbva not enabled\n"));
        return VERR_INVALID_STATE;
    }

    pCtl->pfnComplete = pfnComplete;
    pCtl->pvComplete = pvComplete;

    int rc = RTCritSectEnter(&pCmdVbva->CltCritSect);
    if (RT_SUCCESS(rc))
    {
        if (enmSource > VBVAEXHOSTCTL_SOURCE_GUEST)
        {
            if ((enmSource == VBVAEXHOSTCTL_SOURCE_HOST_ENABLED) && !VBoxVBVAExHSIsEnabled(pCmdVbva))
            {
                WARN(("cmd vbva not enabled\n"));
                RTCritSectLeave(&pCmdVbva->CltCritSect);
                return VERR_INVALID_STATE;
            }
            RTListAppend(&pCmdVbva->HostCtlList, &pCtl->Node);
        }
        else
            RTListAppend(&pCmdVbva->GuestCtlList, &pCtl->Node);

        ASMAtomicIncU32(&pCmdVbva->u32cCtls);

        RTCritSectLeave(&pCmdVbva->CltCritSect);

        rc = VBoxVBVAExHSCheckCommands(pCmdVbva);
    }
    else
        WARN(("RTCritSectEnter failed %d\n", rc));

    return rc;
}


/* Loads state
 * @returns - same as VBoxVBVAExHSCheckCommands, or failure on load state fail
 */
static int VBoxVBVAExHSLoadState(struct VBVAEXHOSTCONTEXT *pCmdVbva, uint8_t* pu8VramBase, PSSMHANDLE pSSM, uint32_t u32Version)
{
    AssertMsgFailed(("implement!\n"));
    uint32_t u32;
    int rc = SSMR3GetU32(pSSM, &u32);
    AssertRCReturn(rc, rc);
    if (u32 != 0xffffffff)
    {
        VBVABUFFER *pVBVA = (VBVABUFFER*)pu8VramBase + u32;
        rc = VBoxVBVAExHSEnable(pCmdVbva, pVBVA);
        AssertRCReturn(rc, rc);
        return VBoxVBVAExHSCheckCommands(pCmdVbva);
    }

    return VINF_SUCCESS;
}

typedef struct VBOXVDMAHOST
{
    PHGSMIINSTANCE pHgsmi;
    PVGASTATE pVGAState;
    VBVAEXHOSTCONTEXT CmdVbva;
    VBOXVDMATHREAD Thread;
    VBOXCRCMD_SVRINFO CrSrvInfo;
    VBVAEXHOSTCTL* pCurRemainingHostCtl;
#ifdef VBOX_VDMA_WITH_WATCHDOG
    PTMTIMERR3 WatchDogTimer;
#endif
} VBOXVDMAHOST, *PVBOXVDMAHOST;

int VBoxVDMAThreadNotifyConstructSucceeded(PVBOXVDMATHREAD pThread)
{
    Assert(pThread->u32State == VBOXVDMATHREAD_STATE_TERMINATED);
    int rc = RTSemEventSignal(pThread->hClientEvent);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        pThread->u32State = VBOXVDMATHREAD_STATE_CREATED;
        return VINF_SUCCESS;
    }
    return rc;
}

int VBoxVDMAThreadNotifyConstructFailed(PVBOXVDMATHREAD pThread)
{
    Assert(pThread->u32State == VBOXVDMATHREAD_STATE_TERMINATED);
    int rc = RTSemEventSignal(pThread->hClientEvent);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;
    return rc;
}

DECLINLINE(bool) VBoxVDMAThreadIsTerminating(PVBOXVDMATHREAD pThread)
{
    return ASMAtomicUoReadU32(&pThread->u32State) == VBOXVDMATHREAD_STATE_TERMINATING;
}

int VBoxVDMAThreadCreate(PVBOXVDMATHREAD pThread, PFNRTTHREAD pfnThread, void *pvThread)
{
    int rc = RTSemEventCreate(&pThread->hEvent);
    if (RT_SUCCESS(rc))
    {
        rc = RTSemEventCreate(&pThread->hClientEvent);
        if (RT_SUCCESS(rc))
        {
            pThread->u32State = VBOXVDMATHREAD_STATE_TERMINATED;
            rc = RTThreadCreate(&pThread->hWorkerThread, pfnThread, pvThread, 0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "VDMA");
            if (RT_SUCCESS(rc))
            {
                rc = RTSemEventWait(pThread->hClientEvent, RT_INDEFINITE_WAIT);
                if (RT_SUCCESS(rc))
                {
                    if (pThread->u32State == VBOXVDMATHREAD_STATE_CREATED)
                        return VINF_SUCCESS;
                    WARN(("thread routine failed the initialization\n"));
                    rc = VERR_INVALID_STATE;
                }
                else
                    WARN(("RTSemEventWait failed %d\n", rc));

                RTThreadWait(pThread->hWorkerThread, RT_INDEFINITE_WAIT, NULL);
            }
            else
                WARN(("RTThreadCreate failed %d\n", rc));

            RTSemEventDestroy(pThread->hClientEvent);
        }
        else
            WARN(("RTSemEventCreate failed %d\n", rc));

        RTSemEventDestroy(pThread->hEvent);
    }
    else
        WARN(("RTSemEventCreate failed %d\n", rc));

    return rc;
}

DECLINLINE(int) VBoxVDMAThreadEventNotify(PVBOXVDMATHREAD pThread)
{
    int rc = RTSemEventSignal(pThread->hEvent);
    AssertRC(rc);
    return rc;
}

DECLINLINE(int) VBoxVDMAThreadEventWait(PVBOXVDMATHREAD pThread, RTMSINTERVAL cMillies)
{
    int rc = RTSemEventWait(pThread->hEvent, cMillies);
    AssertRC(rc);
    return rc;
}

void VBoxVDMAThreadMarkTerminating(PVBOXVDMATHREAD pThread)
{
    Assert(pThread->u32State == VBOXVDMATHREAD_STATE_CREATED);
    ASMAtomicWriteU32(&pThread->u32State, VBOXVDMATHREAD_STATE_TERMINATING);
}

void VBoxVDMAThreadTerm(PVBOXVDMATHREAD pThread)
{
    int rc;
    if (ASMAtomicReadU32(&pThread->u32State) != VBOXVDMATHREAD_STATE_TERMINATING)
    {
        VBoxVDMAThreadMarkTerminating(pThread);
        rc = VBoxVDMAThreadEventNotify(pThread);
        AssertRC(rc);
    }
    rc = RTThreadWait(pThread->hWorkerThread, RT_INDEFINITE_WAIT, NULL);
    AssertRC(rc);
    RTSemEventDestroy(pThread->hClientEvent);
    RTSemEventDestroy(pThread->hEvent);
}

static int vdmaVBVACtlSubmitSync(PVBOXVDMAHOST pVdma, VBVAEXHOSTCTL* pCtl, VBVAEXHOSTCTL_SOURCE enmSource);

#ifdef VBOX_WITH_CRHGSMI

typedef DECLCALLBACK(void) FNVBOXVDMACRCTL_CALLBACK(PVGASTATE pVGAState, PVBOXVDMACMD_CHROMIUM_CTL pCmd, void* pvContext);
typedef FNVBOXVDMACRCTL_CALLBACK *PFNVBOXVDMACRCTL_CALLBACK;

typedef struct VBOXVDMACMD_CHROMIUM_CTL_PRIVATE
{
    uint32_t cRefs;
    int32_t rc;
    PFNVBOXVDMACRCTL_CALLBACK pfnCompletion;
    void *pvCompletion;
    VBOXVDMACMD_CHROMIUM_CTL Cmd;
} VBOXVDMACMD_CHROMIUM_CTL_PRIVATE, *PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE;

#define VBOXVDMACMD_CHROMIUM_CTL_PRIVATE_FROM_CTL(_p) ((PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE)(((uint8_t*)(_p)) - RT_OFFSETOF(VBOXVDMACMD_CHROMIUM_CTL_PRIVATE, Cmd)))

static PVBOXVDMACMD_CHROMIUM_CTL vboxVDMACrCtlCreate(VBOXVDMACMD_CHROMIUM_CTL_TYPE enmCmd, uint32_t cbCmd)
{
    PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE pHdr = (PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE)RTMemAllocZ(cbCmd + RT_OFFSETOF(VBOXVDMACMD_CHROMIUM_CTL_PRIVATE, Cmd));
    Assert(pHdr);
    if (pHdr)
    {
        pHdr->cRefs = 1;
        pHdr->rc = VERR_NOT_IMPLEMENTED;
        pHdr->Cmd.enmType = enmCmd;
        pHdr->Cmd.cbCmd = cbCmd;
        return &pHdr->Cmd;
    }

    return NULL;
}

DECLINLINE(void) vboxVDMACrCtlRelease (PVBOXVDMACMD_CHROMIUM_CTL pCmd)
{
    PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE pHdr = VBOXVDMACMD_CHROMIUM_CTL_PRIVATE_FROM_CTL(pCmd);
    uint32_t cRefs = ASMAtomicDecU32(&pHdr->cRefs);
    if(!cRefs)
    {
        RTMemFree(pHdr);
    }
}

DECLINLINE(void) vboxVDMACrCtlRetain (PVBOXVDMACMD_CHROMIUM_CTL pCmd)
{
    PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE pHdr = VBOXVDMACMD_CHROMIUM_CTL_PRIVATE_FROM_CTL(pCmd);
    ASMAtomicIncU32(&pHdr->cRefs);
}

DECLINLINE(int) vboxVDMACrCtlGetRc (PVBOXVDMACMD_CHROMIUM_CTL pCmd)
{
    PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE pHdr = VBOXVDMACMD_CHROMIUM_CTL_PRIVATE_FROM_CTL(pCmd);
    return pHdr->rc;
}

static DECLCALLBACK(void) vboxVDMACrCtlCbSetEvent(PVGASTATE pVGAState, PVBOXVDMACMD_CHROMIUM_CTL pCmd, void* pvContext)
{
    RTSemEventSignal((RTSEMEVENT)pvContext);
}

static DECLCALLBACK(void) vboxVDMACrCtlCbReleaseCmd(PVGASTATE pVGAState, PVBOXVDMACMD_CHROMIUM_CTL pCmd, void* pvContext)
{
    vboxVDMACrCtlRelease(pCmd);
}


static int vboxVDMACrCtlPostAsync (PVGASTATE pVGAState, PVBOXVDMACMD_CHROMIUM_CTL pCmd, uint32_t cbCmd, PFNVBOXVDMACRCTL_CALLBACK pfnCompletion, void *pvCompletion)
{
    if (   pVGAState->pDrv
        && pVGAState->pDrv->pfnCrHgsmiControlProcess)
    {
        PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE pHdr = VBOXVDMACMD_CHROMIUM_CTL_PRIVATE_FROM_CTL(pCmd);
        pHdr->pfnCompletion = pfnCompletion;
        pHdr->pvCompletion = pvCompletion;
        pVGAState->pDrv->pfnCrHgsmiControlProcess(pVGAState->pDrv, pCmd, cbCmd);
        return VINF_SUCCESS;
    }
#ifdef DEBUG_misha
    Assert(0);
#endif
    return VERR_NOT_SUPPORTED;
}

static int vboxVDMACrCtlPost(PVGASTATE pVGAState, PVBOXVDMACMD_CHROMIUM_CTL pCmd, uint32_t cbCmd)
{
    RTSEMEVENT hComplEvent;
    int rc = RTSemEventCreate(&hComplEvent);
    AssertRC(rc);
    if(RT_SUCCESS(rc))
    {
        rc = vboxVDMACrCtlPostAsync(pVGAState, pCmd, cbCmd, vboxVDMACrCtlCbSetEvent, (void*)hComplEvent);
#ifdef DEBUG_misha
        AssertRC(rc);
#endif
        if (RT_SUCCESS(rc))
        {
            rc = RTSemEventWaitNoResume(hComplEvent, RT_INDEFINITE_WAIT);
            AssertRC(rc);
            if(RT_SUCCESS(rc))
            {
                RTSemEventDestroy(hComplEvent);
            }
        }
        else
        {
            /* the command is completed */
            RTSemEventDestroy(hComplEvent);
        }
    }
    return rc;
}

typedef struct VDMA_VBVA_CTL_CYNC_COMPLETION
{
    int rc;
    RTSEMEVENT hEvent;
} VDMA_VBVA_CTL_CYNC_COMPLETION;

static DECLCALLBACK(void) vboxVDMACrHgcmSubmitSyncCompletion(struct VBOXCRCMDCTL* pCmd, uint32_t cbCmd, int rc, void *pvCompletion)
{
    VDMA_VBVA_CTL_CYNC_COMPLETION *pData = (VDMA_VBVA_CTL_CYNC_COMPLETION*)pvCompletion;
    pData->rc = rc;
    rc = RTSemEventSignal(pData->hEvent);
    if (!RT_SUCCESS(rc))
        WARN(("RTSemEventSignal failed %d\n", rc));
}

static int vboxVDMACrHgcmSubmitSync(struct VBOXVDMAHOST *pVdma, VBOXCRCMDCTL* pCtl, uint32_t cbCtl)
{
    VDMA_VBVA_CTL_CYNC_COMPLETION Data;
    Data.rc = VERR_NOT_IMPLEMENTED;
    int rc = RTSemEventCreate(&Data.hEvent);
    if (!RT_SUCCESS(rc))
    {
        WARN(("RTSemEventCreate failed %d\n", rc));
        return rc;
    }

    PVGASTATE pVGAState = pVdma->pVGAState;
    rc = pVGAState->pDrv->pfnCrHgcmCtlSubmit(pVGAState->pDrv, pCtl, cbCtl, vboxVDMACrHgcmSubmitSyncCompletion, &Data);
    if (RT_SUCCESS(rc))
    {
        rc = RTSemEventWait(Data.hEvent, RT_INDEFINITE_WAIT);
        if (RT_SUCCESS(rc))
        {
            rc = Data.rc;
            if (!RT_SUCCESS(rc))
            {
                WARN(("pfnCrHgcmCtlSubmit command failed %d\n", rc));
            }

        }
        else
            WARN(("RTSemEventWait failed %d\n", rc));
    }
    else
        WARN(("pfnCrHgcmCtlSubmit failed %d\n", rc));


    RTSemEventDestroy(Data.hEvent);

    return rc;
}

static DECLCALLBACK(VBOXCRCMDCTL*) vboxVDMACrHgcmHandleEnableRemainingHostCommand(HVBOXCRCMDCTL_REMAINING_HOST_COMMAND hClient, uint32_t *pcbCtl, int prevCmdRc)
{
    struct VBOXVDMAHOST *pVdma = hClient;
    if (!pVdma->pCurRemainingHostCtl)
    {
        /* disable VBVA, all subsequent host commands will go HGCM way */
        VBoxVBVAExHSDisable(&pVdma->CmdVbva);
    }
    else
    {
        VBoxVBVAExHPDataCompleteCtl(&pVdma->CmdVbva, pVdma->pCurRemainingHostCtl, prevCmdRc);
    }

    pVdma->pCurRemainingHostCtl = VBoxVBVAExHPCheckHostCtlOnDisable(&pVdma->CmdVbva);
    if (pVdma->pCurRemainingHostCtl)
    {
        *pcbCtl = pVdma->pCurRemainingHostCtl->u.cmd.cbCmd;
        return (VBOXCRCMDCTL*)pVdma->pCurRemainingHostCtl->u.cmd.pu8Cmd;
    }

    *pcbCtl = 0;
    return NULL;
}

static int vboxVDMACrHgcmHandleEnable(struct VBOXVDMAHOST *pVdma)
{
    VBOXCRCMDCTL_ENABLE Enable;
    Enable.Hdr.enmType = VBOXCRCMDCTL_TYPE_ENABLE;
    Enable.hRHCmd = pVdma;
    Enable.pfnRHCmd = vboxVDMACrHgcmHandleEnableRemainingHostCommand;

    int rc = vboxVDMACrHgcmSubmitSync(pVdma, &Enable.Hdr, sizeof (Enable));
    Assert(!pVdma->pCurRemainingHostCtl);
    if (RT_SUCCESS(rc))
    {
        Assert(!VBoxVBVAExHSIsEnabled(&pVdma->CmdVbva));
        return VINF_SUCCESS;
    }

    Assert(VBoxVBVAExHSIsEnabled(&pVdma->CmdVbva));
    WARN(("vboxVDMACrHgcmSubmitSync failed %d\n", rc));

    return rc;
}

static int vdmaVBVAEnableProcess(struct VBOXVDMAHOST *pVdma, uint32_t u32Offset)
{
    if (VBoxVBVAExHSIsEnabled(&pVdma->CmdVbva))
    {
        WARN(("vdma VBVA is already enabled\n"));
        return VERR_INVALID_STATE;
    }

    VBVABUFFER *pVBVA = (VBVABUFFER *)HGSMIOffsetToPointerHost(pVdma->pHgsmi, u32Offset);
    if (!pVBVA)
    {
        WARN(("invalid offset %d\n", u32Offset));
        return VERR_INVALID_PARAMETER;
    }

    if (!pVdma->CrSrvInfo.pfnEnable)
    {
#ifdef DEBUG_misha
        WARN(("pfnEnable is NULL\n"));
        return VERR_NOT_SUPPORTED;
#endif
    }

    int rc = VBoxVBVAExHSEnable(&pVdma->CmdVbva, pVBVA);
    if (RT_SUCCESS(rc))
    {
        VBOXCRCMDCTL Ctl;
        Ctl.enmType = VBOXCRCMDCTL_TYPE_DISABLE;
        rc = vboxVDMACrHgcmSubmitSync(pVdma, &Ctl, sizeof (Ctl));
        if (RT_SUCCESS(rc))
        {
            PVGASTATE pVGAState = pVdma->pVGAState;
            VBOXCRCMD_SVRENABLE_INFO Info;
            Info.hCltScr = pVGAState->pDrv;
            Info.pfnCltScrUpdateBegin = pVGAState->pDrv->pfnVBVAUpdateBegin;
            Info.pfnCltScrUpdateProcess = pVGAState->pDrv->pfnVBVAUpdateProcess;
            Info.pfnCltScrUpdateEnd = pVGAState->pDrv->pfnVBVAUpdateEnd;
            rc = pVdma->CrSrvInfo.pfnEnable(pVdma->CrSrvInfo.hSvr, &Info);
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;
            else
                WARN(("pfnEnable failed %d\n", rc));

            vboxVDMACrHgcmHandleEnable(pVdma);
        }
        else
            WARN(("vboxVDMACrHgcmSubmitSync failed %d\n", rc));

        VBoxVBVAExHSDisable(&pVdma->CmdVbva);
    }
    else
        WARN(("VBoxVBVAExHSEnable failed %d\n", rc));

    return rc;
}

static int vdmaVBVADisableProcess(struct VBOXVDMAHOST *pVdma)
{
    if (!VBoxVBVAExHSIsEnabled(&pVdma->CmdVbva))
    {
        Log(("vdma VBVA is already disabled\n"));
        return VINF_SUCCESS;
    }

    int rc = pVdma->CrSrvInfo.pfnDisable(pVdma->CrSrvInfo.hSvr);
    if (RT_SUCCESS(rc))
    {
        /* disable is a bit tricky
         * we need to ensure the host ctl commands do not come out of order
         * and do not come over HGCM channel until after it is enabled */
        rc = vboxVDMACrHgcmHandleEnable(pVdma);
        if (RT_SUCCESS(rc))
            return rc;

        PVGASTATE pVGAState = pVdma->pVGAState;
        VBOXCRCMD_SVRENABLE_INFO Info;
        Info.hCltScr = pVGAState->pDrv;
        Info.pfnCltScrUpdateBegin = pVGAState->pDrv->pfnVBVAUpdateBegin;
        Info.pfnCltScrUpdateProcess = pVGAState->pDrv->pfnVBVAUpdateProcess;
        Info.pfnCltScrUpdateEnd = pVGAState->pDrv->pfnVBVAUpdateEnd;
        pVdma->CrSrvInfo.pfnEnable(pVdma->CrSrvInfo.hSvr, &Info);
    }
    else
        WARN(("pfnDisable failed %d\n", rc));

    return rc;
}

static int vboxVDMACrHostCtlProcess(struct VBOXVDMAHOST *pVdma, VBVAEXHOSTCTL *pCmd)
{
    switch (pCmd->enmType)
    {
        case VBVAEXHOSTCTL_TYPE_HH_SAVESTATE:
            if (!VBoxVBVAExHSIsEnabled(&pVdma->CmdVbva))
            {
                WARN(("VBVAEXHOSTCTL_TYPE_HH_SAVESTATE for disabled vdma VBVA\n"));
                return VERR_INVALID_STATE;
            }
            return pVdma->CrSrvInfo.pfnSaveState(pVdma->CrSrvInfo.hSvr, pCmd->u.state.pSSM);
        case VBVAEXHOSTCTL_TYPE_HH_LOADSTATE:
            if (!VBoxVBVAExHSIsEnabled(&pVdma->CmdVbva))
            {
                WARN(("VBVAEXHOSTCTL_TYPE_HH_LOADSTATE for disabled vdma VBVA\n"));
                return VERR_INVALID_STATE;
            }
            return pVdma->CrSrvInfo.pfnLoadState(pVdma->CrSrvInfo.hSvr, pCmd->u.state.pSSM, pCmd->u.state.u32Version);
        case VBVAEXHOSTCTL_TYPE_GHH_BE_OPAQUE:
            if (!VBoxVBVAExHSIsEnabled(&pVdma->CmdVbva))
            {
                WARN(("VBVAEXHOSTCTL_TYPE_GHH_BE_OPAQUE for disabled vdma VBVA\n"));
                return VERR_INVALID_STATE;
            }
            return pVdma->CrSrvInfo.pfnHostCtl(pVdma->CrSrvInfo.hSvr, pCmd->u.cmd.pu8Cmd, pCmd->u.cmd.cbCmd);
        case VBVAEXHOSTCTL_TYPE_HH_TERM:
        {
            int rc = vdmaVBVADisableProcess(pVdma);
            if (!RT_SUCCESS(rc))
            {
                WARN(("vdmaVBVADisableProcess failed %d\n", rc));
                return rc;
            }

            VBoxVDMAThreadMarkTerminating(&pVdma->Thread);
            return VINF_SUCCESS;
        }
        case VBVAEXHOSTCTL_TYPE_HH_RESET:
        {
            int rc = vdmaVBVADisableProcess(pVdma);
            if (!RT_SUCCESS(rc))
            {
                WARN(("vdmaVBVADisableProcess failed %d\n", rc));
                return rc;
            }
            return VINF_SUCCESS;
        }
        default:
            WARN(("unexpected host ctl type %d\n", pCmd->enmType));
            return VERR_INVALID_PARAMETER;
    }
}

static int vboxVDMACrGuestCtlProcess(struct VBOXVDMAHOST *pVdma, VBVAEXHOSTCTL *pCmd)
{
    switch (pCmd->enmType)
    {
       case VBVAEXHOSTCTL_TYPE_GHH_BE_OPAQUE:
           if (!VBoxVBVAExHSIsEnabled(&pVdma->CmdVbva))
           {
               WARN(("VBVAEXHOSTCTL_TYPE_GHH_BE_OPAQUE for disabled vdma VBVA\n"));
               return VERR_INVALID_STATE;
           }
           return pVdma->CrSrvInfo.pfnGuestCtl(pVdma->CrSrvInfo.hSvr, pCmd->u.cmd.pu8Cmd, pCmd->u.cmd.cbCmd);
        case VBVAEXHOSTCTL_TYPE_GH_ENABLE_DISABLE:
        {
            VBVAENABLE *pEnable = (VBVAENABLE *)pCmd->u.cmd.pu8Cmd;
            Assert(pCmd->u.cmd.cbCmd == sizeof (VBVAENABLE));
            if ((pEnable->u32Flags & (VBVA_F_ENABLE | VBVA_F_DISABLE)) == VBVA_F_ENABLE)
            {
                uint32_t u32Offset = pEnable->u32Offset;
                return vdmaVBVAEnableProcess(pVdma, u32Offset);
            }

            return vdmaVBVADisableProcess(pVdma);
        }
        default:
            WARN(("unexpected ctl type %d\n", pCmd->enmType));
            return VERR_INVALID_PARAMETER;
    }
}


/*
 * @returns
 *
 */
static int vboxVDMACrCmdProcess(struct VBOXVDMAHOST *pVdma, uint8_t* pu8Cmd, uint32_t cbCmd)
{
    if (*pu8Cmd == VBOXCMDVBVA_OPTYPE_NOP)
        return VINF_EOF;

    PVBOXCMDVBVA_HDR pCmd = (PVBOXCMDVBVA_HDR)pu8Cmd;

    /* check if the command is cancelled */
    if (!ASMAtomicCmpXchgU8(&pCmd->u8State, VBOXCMDVBVA_STATE_IN_PROGRESS, VBOXCMDVBVA_STATE_SUBMITTED))
    {
        Assert(pCmd->u8State == VBOXCMDVBVA_STATE_CANCELLED);
        return VINF_EOF;
    }

    /* come commands can be handled right away? */
    switch (pCmd->u8OpCode)
    {
        case VBOXCMDVBVA_OPTYPE_NOPCMD:
            pCmd->i8Result = 0;
            return VINF_EOF;
        default:
            return VINF_SUCCESS;
    }
}

static DECLCALLBACK(int) vboxVDMACrCmdEnable(HVBOXCRCMDSVR hSvr, VBOXCRCMD_SVRENABLE_INFO *pInfo)
{
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) vboxVDMACrCmdDisable(HVBOXCRCMDSVR hSvr)
{
}

static DECLCALLBACK(int) vboxVDMACrCmdCtl(HVBOXCRCMDSVR hSvr, uint8_t* pCmd, uint32_t cbCmd)
{
    return VERR_NOT_SUPPORTED;
}

static DECLCALLBACK(int) vboxVDMACrCmdCmd(HVBOXCRCMDSVR hSvr, PVBOXCMDVBVA_HDR pCmd, uint32_t cbCmd)
{
    switch (pCmd->u8OpCode)
    {
#if 0
        case VBOXCMDVBVA_OPTYPE_BLT_OFFPRIMSZFMT_OR_ID:
        {
            crVBoxServerCrCmdBltProcess(pCmd, cbCmd);
            break;
        }
#endif
        default:
            WARN(("unsupported command\n"));
            pCmd->i8Result = -1;
    }
    return VINF_SUCCESS;
}

static int vboxVDMACrCtlHgsmiSetup(struct VBOXVDMAHOST *pVdma)
{
    PVBOXVDMACMD_CHROMIUM_CTL_CRHGSMI_SETUP pCmd = (PVBOXVDMACMD_CHROMIUM_CTL_CRHGSMI_SETUP)
            vboxVDMACrCtlCreate (VBOXVDMACMD_CHROMIUM_CTL_TYPE_CRHGSMI_SETUP, sizeof (*pCmd));
    int rc = VERR_NO_MEMORY;
    if (pCmd)
    {
        PVGASTATE pVGAState = pVdma->pVGAState;
        pCmd->pvVRamBase = pVGAState->vram_ptrR3;
        pCmd->cbVRam = pVGAState->vram_size;
        rc = vboxVDMACrCtlPost(pVGAState, &pCmd->Hdr, sizeof (*pCmd));
        if (RT_SUCCESS(rc))
        {
            rc = vboxVDMACrCtlGetRc(&pCmd->Hdr);
            if (RT_SUCCESS(rc))
                pVdma->CrSrvInfo = pCmd->CrCmdServerInfo;
            else if (rc != VERR_NOT_SUPPORTED)
                WARN(("vboxVDMACrCtlGetRc returned %d\n", rc));
        }
        else
            WARN(("vboxVDMACrCtlPost failed %d\n", rc));

        vboxVDMACrCtlRelease(&pCmd->Hdr);
    }

    if (!RT_SUCCESS(rc))
        memset(&pVdma->CrSrvInfo, 0, sizeof (pVdma->CrSrvInfo));

    return rc;
}

static int vboxVDMACmdExecBpbTransfer(PVBOXVDMAHOST pVdma, const PVBOXVDMACMD_DMA_BPB_TRANSFER pTransfer, uint32_t cbBuffer);

/* check if this is external cmd to be passed to chromium backend */
static int vboxVDMACmdCheckCrCmd(struct VBOXVDMAHOST *pVdma, PVBOXVDMACBUF_DR pCmdDr, uint32_t cbCmdDr)
{
    PVBOXVDMACMD pDmaCmd = NULL;
    uint32_t cbDmaCmd = 0;
    uint8_t * pvRam = pVdma->pVGAState->vram_ptrR3;
    int rc = VINF_NOT_SUPPORTED;

    cbDmaCmd = pCmdDr->cbBuf;

    if (pCmdDr->fFlags & VBOXVDMACBUF_FLAG_BUF_FOLLOWS_DR)
    {
        if (cbCmdDr < sizeof (*pCmdDr) + VBOXVDMACMD_HEADER_SIZE())
        {
            AssertMsgFailed(("invalid buffer data!"));
            return VERR_INVALID_PARAMETER;
        }

        if (cbDmaCmd < cbCmdDr - sizeof (*pCmdDr) - VBOXVDMACMD_HEADER_SIZE())
        {
            AssertMsgFailed(("invalid command buffer data!"));
            return VERR_INVALID_PARAMETER;
        }

        pDmaCmd = VBOXVDMACBUF_DR_TAIL(pCmdDr, VBOXVDMACMD);
    }
    else if (pCmdDr->fFlags & VBOXVDMACBUF_FLAG_BUF_VRAM_OFFSET)
    {
        VBOXVIDEOOFFSET offBuf = pCmdDr->Location.offVramBuf;
        if (offBuf + cbDmaCmd > pVdma->pVGAState->vram_size)
        {
            AssertMsgFailed(("invalid command buffer data from offset!"));
            return VERR_INVALID_PARAMETER;
        }
        pDmaCmd = (VBOXVDMACMD*)(pvRam + offBuf);
    }

    if (pDmaCmd)
    {
        Assert(cbDmaCmd >= VBOXVDMACMD_HEADER_SIZE());
        uint32_t cbBody = VBOXVDMACMD_BODY_SIZE(cbDmaCmd);

        switch (pDmaCmd->enmType)
        {
            case VBOXVDMACMD_TYPE_CHROMIUM_CMD:
            {
                PVBOXVDMACMD_CHROMIUM_CMD pCrCmd = VBOXVDMACMD_BODY(pDmaCmd, VBOXVDMACMD_CHROMIUM_CMD);
                if (cbBody < sizeof (*pCrCmd))
                {
                    AssertMsgFailed(("invalid chromium command buffer size!"));
                    return VERR_INVALID_PARAMETER;
                }
                PVGASTATE pVGAState = pVdma->pVGAState;
                rc = VINF_SUCCESS;
                if (pVGAState->pDrv->pfnCrHgsmiCommandProcess)
                {
                    VBoxSHGSMICommandMarkAsynchCompletion(pCmdDr);
                    pVGAState->pDrv->pfnCrHgsmiCommandProcess(pVGAState->pDrv, pCrCmd, cbBody);
                    break;
                }
                else
                {
                    Assert(0);
                }

                int tmpRc = VBoxSHGSMICommandComplete (pVdma->pHgsmi, pCmdDr);
                AssertRC(tmpRc);
                break;
            }
            case VBOXVDMACMD_TYPE_DMA_BPB_TRANSFER:
            {
                PVBOXVDMACMD_DMA_BPB_TRANSFER pTransfer = VBOXVDMACMD_BODY(pDmaCmd, VBOXVDMACMD_DMA_BPB_TRANSFER);
                if (cbBody < sizeof (*pTransfer))
                {
                    AssertMsgFailed(("invalid bpb transfer buffer size!"));
                    return VERR_INVALID_PARAMETER;
                }

                rc = vboxVDMACmdExecBpbTransfer(pVdma, pTransfer, sizeof (*pTransfer));
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                {
                    pCmdDr->rc = VINF_SUCCESS;
                    rc = VBoxSHGSMICommandComplete (pVdma->pHgsmi, pCmdDr);
                    AssertRC(rc);
                    rc = VINF_SUCCESS;
                }
                break;
            }
            default:
                break;
        }
    }
    return rc;
}

int vboxVDMACrHgsmiCommandCompleteAsync(PPDMIDISPLAYVBVACALLBACKS pInterface, PVBOXVDMACMD_CHROMIUM_CMD pCmd, int rc)
{
    PVGASTATE pVGAState = PPDMIDISPLAYVBVACALLBACKS_2_PVGASTATE(pInterface);
    PHGSMIINSTANCE pIns = pVGAState->pHGSMI;
    VBOXVDMACMD *pDmaHdr = VBOXVDMACMD_FROM_BODY(pCmd);
    VBOXVDMACBUF_DR *pDr = VBOXVDMACBUF_DR_FROM_TAIL(pDmaHdr);
    AssertRC(rc);
    pDr->rc = rc;

    Assert(pVGAState->fGuestCaps & VBVACAPS_COMPLETEGCMD_BY_IOREAD);
    rc = VBoxSHGSMICommandComplete(pIns, pDr);
    AssertRC(rc);
    return rc;
}

int vboxVDMACrHgsmiControlCompleteAsync(PPDMIDISPLAYVBVACALLBACKS pInterface, PVBOXVDMACMD_CHROMIUM_CTL pCmd, int rc)
{
    PVGASTATE pVGAState = PPDMIDISPLAYVBVACALLBACKS_2_PVGASTATE(pInterface);
    PVBOXVDMACMD_CHROMIUM_CTL_PRIVATE pCmdPrivate = VBOXVDMACMD_CHROMIUM_CTL_PRIVATE_FROM_CTL(pCmd);
    pCmdPrivate->rc = rc;
    if (pCmdPrivate->pfnCompletion)
    {
        pCmdPrivate->pfnCompletion(pVGAState, pCmd, pCmdPrivate->pvCompletion);
    }
    return VINF_SUCCESS;
}

#endif

#ifdef VBOX_VDMA_WITH_WORKERTHREAD
/* to simplify things and to avoid extra backend if modifications we assume the VBOXVDMA_RECTL is the same as VBVACMDHDR */
AssertCompile(sizeof(VBOXVDMA_RECTL) == sizeof(VBVACMDHDR));
AssertCompile(RT_SIZEOFMEMB(VBOXVDMA_RECTL, left) == RT_SIZEOFMEMB(VBVACMDHDR, x));
AssertCompile(RT_SIZEOFMEMB(VBOXVDMA_RECTL, top) == RT_SIZEOFMEMB(VBVACMDHDR, y));
AssertCompile(RT_SIZEOFMEMB(VBOXVDMA_RECTL, width) == RT_SIZEOFMEMB(VBVACMDHDR, w));
AssertCompile(RT_SIZEOFMEMB(VBOXVDMA_RECTL, height) == RT_SIZEOFMEMB(VBVACMDHDR, h));
AssertCompile(RT_OFFSETOF(VBOXVDMA_RECTL, left) == RT_OFFSETOF(VBVACMDHDR, x));
AssertCompile(RT_OFFSETOF(VBOXVDMA_RECTL, top) == RT_OFFSETOF(VBVACMDHDR, y));
AssertCompile(RT_OFFSETOF(VBOXVDMA_RECTL, width) == RT_OFFSETOF(VBVACMDHDR, w));
AssertCompile(RT_OFFSETOF(VBOXVDMA_RECTL, height) == RT_OFFSETOF(VBVACMDHDR, h));

static int vboxVDMANotifyPrimaryUpdate (PVGASTATE pVGAState, unsigned uScreenId, const VBOXVDMA_RECTL * pRectl)
{
    pVGAState->pDrv->pfnVBVAUpdateBegin (pVGAState->pDrv, uScreenId);

    /* Updates the rectangle and sends the command to the VRDP server. */
    pVGAState->pDrv->pfnVBVAUpdateProcess (pVGAState->pDrv, uScreenId,
            (const PVBVACMDHDR)pRectl /* <- see above AssertCompile's and comments */,
            sizeof (VBOXVDMA_RECTL));

    pVGAState->pDrv->pfnVBVAUpdateEnd (pVGAState->pDrv, uScreenId, pRectl->left, pRectl->top,
                                               pRectl->width, pRectl->height);

    return VINF_SUCCESS;
}
#endif

static int vboxVDMACmdExecBltPerform(PVBOXVDMAHOST pVdma,
        uint8_t *pvDstSurf, const uint8_t *pvSrcSurf,
        const PVBOXVDMA_SURF_DESC pDstDesc, const PVBOXVDMA_SURF_DESC pSrcDesc,
        const VBOXVDMA_RECTL * pDstRectl, const VBOXVDMA_RECTL * pSrcRectl)
{
    /* we do not support color conversion */
    Assert(pDstDesc->format == pSrcDesc->format);
    /* we do not support stretching */
    Assert(pDstRectl->height == pSrcRectl->height);
    Assert(pDstRectl->width == pSrcRectl->width);
    if (pDstDesc->format != pSrcDesc->format)
        return VERR_INVALID_FUNCTION;
    if (pDstDesc->width == pDstRectl->width
            && pSrcDesc->width == pSrcRectl->width
            && pSrcDesc->width == pDstDesc->width)
    {
        Assert(!pDstRectl->left);
        Assert(!pSrcRectl->left);
        uint32_t cbOff = pDstDesc->pitch * pDstRectl->top;
        uint32_t cbSize = pDstDesc->pitch * pDstRectl->height;
        memcpy(pvDstSurf + cbOff, pvSrcSurf + cbOff, cbSize);
    }
    else
    {
        uint32_t offDstLineStart = pDstRectl->left * pDstDesc->bpp >> 3;
        uint32_t offDstLineEnd = ((pDstRectl->left * pDstDesc->bpp + 7) >> 3) + ((pDstDesc->bpp * pDstRectl->width + 7) >> 3);
        uint32_t cbDstLine = offDstLineEnd - offDstLineStart;
        uint32_t offDstStart = pDstDesc->pitch * pDstRectl->top + offDstLineStart;
        Assert(cbDstLine <= pDstDesc->pitch);
        uint32_t cbDstSkip = pDstDesc->pitch;
        uint8_t * pvDstStart = pvDstSurf + offDstStart;

        uint32_t offSrcLineStart = pSrcRectl->left * pSrcDesc->bpp >> 3;
        uint32_t offSrcLineEnd = ((pSrcRectl->left * pSrcDesc->bpp + 7) >> 3) + ((pSrcDesc->bpp * pSrcRectl->width + 7) >> 3);
        uint32_t cbSrcLine = offSrcLineEnd - offSrcLineStart;
        uint32_t offSrcStart = pSrcDesc->pitch * pSrcRectl->top + offSrcLineStart;
        Assert(cbSrcLine <= pSrcDesc->pitch);
        uint32_t cbSrcSkip = pSrcDesc->pitch;
        const uint8_t * pvSrcStart = pvSrcSurf + offSrcStart;

        Assert(cbDstLine == cbSrcLine);

        for (uint32_t i = 0; ; ++i)
        {
            memcpy (pvDstStart, pvSrcStart, cbDstLine);
            if (i == pDstRectl->height)
                break;
            pvDstStart += cbDstSkip;
            pvSrcStart += cbSrcSkip;
        }
    }
    return VINF_SUCCESS;
}

static void vboxVDMARectlUnite(VBOXVDMA_RECTL * pRectl1, const VBOXVDMA_RECTL * pRectl2)
{
    if (!pRectl1->width)
        *pRectl1 = *pRectl2;
    else
    {
        int16_t x21 = pRectl1->left + pRectl1->width;
        int16_t x22 = pRectl2->left + pRectl2->width;
        if (pRectl1->left > pRectl2->left)
        {
            pRectl1->left = pRectl2->left;
            pRectl1->width = x21 < x22 ? x22 - pRectl1->left : x21 - pRectl1->left;
        }
        else if (x21 < x22)
            pRectl1->width = x22 - pRectl1->left;

        x21 = pRectl1->top + pRectl1->height;
        x22 = pRectl2->top + pRectl2->height;
        if (pRectl1->top > pRectl2->top)
        {
            pRectl1->top = pRectl2->top;
            pRectl1->height = x21 < x22 ? x22 - pRectl1->top : x21 - pRectl1->top;
        }
        else if (x21 < x22)
            pRectl1->height = x22 - pRectl1->top;
    }
}

/*
 * @return on success the number of bytes the command contained, otherwise - VERR_xxx error code
 */
static int vboxVDMACmdExecBlt(PVBOXVDMAHOST pVdma, const PVBOXVDMACMD_DMA_PRESENT_BLT pBlt, uint32_t cbBuffer)
{
    const uint32_t cbBlt = VBOXVDMACMD_BODY_FIELD_OFFSET(uint32_t, VBOXVDMACMD_DMA_PRESENT_BLT, aDstSubRects[pBlt->cDstSubRects]);
    Assert(cbBlt <= cbBuffer);
    if (cbBuffer < cbBlt)
        return VERR_INVALID_FUNCTION;

    /* we do not support stretching for now */
    Assert(pBlt->srcRectl.width == pBlt->dstRectl.width);
    Assert(pBlt->srcRectl.height == pBlt->dstRectl.height);
    if (pBlt->srcRectl.width != pBlt->dstRectl.width)
        return VERR_INVALID_FUNCTION;
    if (pBlt->srcRectl.height != pBlt->dstRectl.height)
        return VERR_INVALID_FUNCTION;
    Assert(pBlt->cDstSubRects);

    uint8_t * pvRam = pVdma->pVGAState->vram_ptrR3;
    VBOXVDMA_RECTL updateRectl = {0, 0, 0, 0};

    if (pBlt->cDstSubRects)
    {
        VBOXVDMA_RECTL dstRectl, srcRectl;
        const VBOXVDMA_RECTL *pDstRectl, *pSrcRectl;
        for (uint32_t i = 0; i < pBlt->cDstSubRects; ++i)
        {
            pDstRectl = &pBlt->aDstSubRects[i];
            if (pBlt->dstRectl.left || pBlt->dstRectl.top)
            {
                dstRectl.left = pDstRectl->left + pBlt->dstRectl.left;
                dstRectl.top = pDstRectl->top + pBlt->dstRectl.top;
                dstRectl.width = pDstRectl->width;
                dstRectl.height = pDstRectl->height;
                pDstRectl = &dstRectl;
            }

            pSrcRectl = &pBlt->aDstSubRects[i];
            if (pBlt->srcRectl.left || pBlt->srcRectl.top)
            {
                srcRectl.left = pSrcRectl->left + pBlt->srcRectl.left;
                srcRectl.top = pSrcRectl->top + pBlt->srcRectl.top;
                srcRectl.width = pSrcRectl->width;
                srcRectl.height = pSrcRectl->height;
                pSrcRectl = &srcRectl;
            }

            int rc = vboxVDMACmdExecBltPerform(pVdma, pvRam + pBlt->offDst, pvRam + pBlt->offSrc,
                    &pBlt->dstDesc, &pBlt->srcDesc,
                    pDstRectl,
                    pSrcRectl);
            AssertRC(rc);
            if (!RT_SUCCESS(rc))
                return rc;

            vboxVDMARectlUnite(&updateRectl, pDstRectl);
        }
    }
    else
    {
        int rc = vboxVDMACmdExecBltPerform(pVdma, pvRam + pBlt->offDst, pvRam + pBlt->offSrc,
                &pBlt->dstDesc, &pBlt->srcDesc,
                &pBlt->dstRectl,
                &pBlt->srcRectl);
        AssertRC(rc);
        if (!RT_SUCCESS(rc))
            return rc;

        vboxVDMARectlUnite(&updateRectl, &pBlt->dstRectl);
    }

#ifdef VBOX_VDMA_WITH_WORKERTHREAD
    int iView = 0;
    /* @todo: fixme: check if update is needed and get iView */
    vboxVDMANotifyPrimaryUpdate (pVdma->pVGAState, iView, &updateRectl);
#endif

    return cbBlt;
}

static int vboxVDMACmdExecBpbTransfer(PVBOXVDMAHOST pVdma, const PVBOXVDMACMD_DMA_BPB_TRANSFER pTransfer, uint32_t cbBuffer)
{
    if (cbBuffer < sizeof (*pTransfer))
        return VERR_INVALID_PARAMETER;

    PVGASTATE pVGAState = pVdma->pVGAState;
    uint8_t * pvRam = pVGAState->vram_ptrR3;
    PGMPAGEMAPLOCK SrcLock;
    PGMPAGEMAPLOCK DstLock;
    PPDMDEVINS pDevIns = pVdma->pVGAState->pDevInsR3;
    const void * pvSrc;
    void * pvDst;
    int rc = VINF_SUCCESS;
    uint32_t cbTransfer = pTransfer->cbTransferSize;
    uint32_t cbTransfered = 0;
    bool bSrcLocked = false;
    bool bDstLocked = false;
    do
    {
        uint32_t cbSubTransfer = cbTransfer;
        if (pTransfer->fFlags & VBOXVDMACMD_DMA_BPB_TRANSFER_F_SRC_VRAMOFFSET)
        {
            pvSrc  = pvRam + pTransfer->Src.offVramBuf + cbTransfered;
        }
        else
        {
            RTGCPHYS phPage = pTransfer->Src.phBuf;
            phPage += cbTransfered;
            rc = PDMDevHlpPhysGCPhys2CCPtrReadOnly(pDevIns, phPage, 0, &pvSrc, &SrcLock);
            AssertRC(rc);
            if (RT_SUCCESS(rc))
            {
                bSrcLocked = true;
                cbSubTransfer = RT_MIN(cbSubTransfer, 0x1000);
            }
            else
            {
                break;
            }
        }

        if (pTransfer->fFlags & VBOXVDMACMD_DMA_BPB_TRANSFER_F_DST_VRAMOFFSET)
        {
            pvDst  = pvRam + pTransfer->Dst.offVramBuf + cbTransfered;
        }
        else
        {
            RTGCPHYS phPage = pTransfer->Dst.phBuf;
            phPage += cbTransfered;
            rc = PDMDevHlpPhysGCPhys2CCPtr(pDevIns, phPage, 0, &pvDst, &DstLock);
            AssertRC(rc);
            if (RT_SUCCESS(rc))
            {
                bDstLocked = true;
                cbSubTransfer = RT_MIN(cbSubTransfer, 0x1000);
            }
            else
            {
                break;
            }
        }

        if (RT_SUCCESS(rc))
        {
            memcpy(pvDst, pvSrc, cbSubTransfer);
            cbTransfer -= cbSubTransfer;
            cbTransfered += cbSubTransfer;
        }
        else
        {
            cbTransfer = 0; /* to break */
        }

        if (bSrcLocked)
            PDMDevHlpPhysReleasePageMappingLock(pDevIns, &SrcLock);
        if (bDstLocked)
            PDMDevHlpPhysReleasePageMappingLock(pDevIns, &DstLock);
    } while (cbTransfer);

    if (RT_SUCCESS(rc))
        return sizeof (*pTransfer);
    return rc;
}

static int vboxVDMACmdExec(PVBOXVDMAHOST pVdma, const uint8_t *pvBuffer, uint32_t cbBuffer)
{
    do
    {
        Assert(pvBuffer);
        Assert(cbBuffer >= VBOXVDMACMD_HEADER_SIZE());

        if (!pvBuffer)
            return VERR_INVALID_PARAMETER;
        if (cbBuffer < VBOXVDMACMD_HEADER_SIZE())
            return VERR_INVALID_PARAMETER;

        PVBOXVDMACMD pCmd = (PVBOXVDMACMD)pvBuffer;
        uint32_t cbCmd = 0;
        switch (pCmd->enmType)
        {
            case VBOXVDMACMD_TYPE_CHROMIUM_CMD:
            {
#ifdef VBOXWDDM_TEST_UHGSMI
                static int count = 0;
                static uint64_t start, end;
                if (count==0)
                {
                    start = RTTimeNanoTS();
                }
                ++count;
                if (count==100000)
                {
                    end = RTTimeNanoTS();
                    float ems = (end-start)/1000000.f;
                    LogRel(("100000 calls took %i ms, %i cps\n", (int)ems, (int)(100000.f*1000.f/ems) ));
                }
#endif
                /* todo: post the buffer to chromium */
                return VINF_SUCCESS;
            }
            case VBOXVDMACMD_TYPE_DMA_PRESENT_BLT:
            {
                const PVBOXVDMACMD_DMA_PRESENT_BLT pBlt = VBOXVDMACMD_BODY(pCmd, VBOXVDMACMD_DMA_PRESENT_BLT);
                int cbBlt = vboxVDMACmdExecBlt(pVdma, pBlt, cbBuffer);
                Assert(cbBlt >= 0);
                Assert((uint32_t)cbBlt <= cbBuffer);
                if (cbBlt >= 0)
                {
                    if ((uint32_t)cbBlt == cbBuffer)
                        return VINF_SUCCESS;
                    else
                    {
                        cbBuffer -= (uint32_t)cbBlt;
                        pvBuffer -= cbBlt;
                    }
                }
                else
                    return cbBlt; /* error */
                break;
            }
            case VBOXVDMACMD_TYPE_DMA_BPB_TRANSFER:
            {
                const PVBOXVDMACMD_DMA_BPB_TRANSFER pTransfer = VBOXVDMACMD_BODY(pCmd, VBOXVDMACMD_DMA_BPB_TRANSFER);
                int cbTransfer = vboxVDMACmdExecBpbTransfer(pVdma, pTransfer, cbBuffer);
                Assert(cbTransfer >= 0);
                Assert((uint32_t)cbTransfer <= cbBuffer);
                if (cbTransfer >= 0)
                {
                    if ((uint32_t)cbTransfer == cbBuffer)
                        return VINF_SUCCESS;
                    else
                    {
                        cbBuffer -= (uint32_t)cbTransfer;
                        pvBuffer -= cbTransfer;
                    }
                }
                else
                    return cbTransfer; /* error */
                break;
            }
            case VBOXVDMACMD_TYPE_DMA_NOP:
                return VINF_SUCCESS;
            case VBOXVDMACMD_TYPE_CHILD_STATUS_IRQ:
                return VINF_SUCCESS;
            default:
                AssertBreakpoint();
                return VERR_INVALID_FUNCTION;
        }
    } while (1);

    /* we should not be here */
    AssertBreakpoint();
    return VERR_INVALID_STATE;
}

static DECLCALLBACK(int) vboxVDMAWorkerThread(RTTHREAD ThreadSelf, void *pvUser)
{
    PVBOXVDMAHOST pVdma = (PVBOXVDMAHOST)pvUser;
    PVGASTATE pVGAState = pVdma->pVGAState;
    VBVAEXHOSTCONTEXT *pCmdVbva = &pVdma->CmdVbva;
    PHGSMIINSTANCE pHgsmi = pVdma->pHgsmi;
    uint8_t *pCmd;
    uint32_t cbCmd;

    int rc = VBoxVDMAThreadNotifyConstructSucceeded(&pVdma->Thread);
    if (!RT_SUCCESS(rc))
    {
        WARN(("VBoxVDMAThreadNotifyConstructSucceeded failed %d\n", rc));
        return rc;
    }

    while (!VBoxVDMAThreadIsTerminating(&pVdma->Thread))
    {
        VBVAEXHOST_DATA_TYPE enmType = VBoxVBVAExHPDataGet(pCmdVbva, &pCmd, &cbCmd);
        switch (enmType)
        {
            case VBVAEXHOST_DATA_TYPE_CMD:
                vboxVDMACrCmdProcess(pVdma, pCmd, cbCmd);
                VBoxVBVAExHPDataCompleteCmd(pCmdVbva, cbCmd);
                VBVARaiseIrqNoWait(pVGAState, 0);
                break;
            case VBVAEXHOST_DATA_TYPE_HOSTCTL:
                rc = vboxVDMACrHostCtlProcess(pVdma, (VBVAEXHOSTCTL*)pCmd);
                VBoxVBVAExHPDataCompleteCtl(pCmdVbva, (VBVAEXHOSTCTL*)pCmd, rc);
                break;
            case VBVAEXHOST_DATA_TYPE_GUESTCTL:
                rc = vboxVDMACrGuestCtlProcess(pVdma, (VBVAEXHOSTCTL*)pCmd);
                VBoxVBVAExHPDataCompleteCtl(pCmdVbva, (VBVAEXHOSTCTL*)pCmd, rc);
                break;
            case VBVAEXHOST_DATA_TYPE_NO_DATA:
                rc = VBoxVDMAThreadEventWait(&pVdma->Thread, RT_INDEFINITE_WAIT);
                AssertRC(rc);
                break;
            default:
                WARN(("unexpected type %d\n", enmType));
                break;
        }
    }

    return VINF_SUCCESS;
}

static void vboxVDMACommandProcess(PVBOXVDMAHOST pVdma, PVBOXVDMACBUF_DR pCmd, uint32_t cbCmd)
{
    PHGSMIINSTANCE pHgsmi = pVdma->pHgsmi;
    const uint8_t * pvBuf;
    PGMPAGEMAPLOCK Lock;
    int rc;
    bool bReleaseLocked = false;

    do
    {
        PPDMDEVINS pDevIns = pVdma->pVGAState->pDevInsR3;

        if (pCmd->fFlags & VBOXVDMACBUF_FLAG_BUF_FOLLOWS_DR)
            pvBuf = VBOXVDMACBUF_DR_TAIL(pCmd, const uint8_t);
        else if (pCmd->fFlags & VBOXVDMACBUF_FLAG_BUF_VRAM_OFFSET)
        {
            uint8_t * pvRam = pVdma->pVGAState->vram_ptrR3;
            pvBuf = pvRam + pCmd->Location.offVramBuf;
        }
        else
        {
            RTGCPHYS phPage = pCmd->Location.phBuf & ~0xfffULL;
            uint32_t offset = pCmd->Location.phBuf & 0xfff;
            Assert(offset + pCmd->cbBuf <= 0x1000);
            if (offset + pCmd->cbBuf > 0x1000)
            {
                /* @todo: more advanced mechanism of command buffer proc is actually needed */
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            const void * pvPageBuf;
            rc = PDMDevHlpPhysGCPhys2CCPtrReadOnly(pDevIns, phPage, 0, &pvPageBuf, &Lock);
            AssertRC(rc);
            if (!RT_SUCCESS(rc))
            {
                /* @todo: if (rc == VERR_PGM_PHYS_PAGE_RESERVED) -> fall back on using PGMPhysRead ?? */
                break;
            }

            pvBuf = (const uint8_t *)pvPageBuf;
            pvBuf += offset;

            bReleaseLocked = true;
        }

        rc = vboxVDMACmdExec(pVdma, pvBuf, pCmd->cbBuf);
        AssertRC(rc);

        if (bReleaseLocked)
            PDMDevHlpPhysReleasePageMappingLock(pDevIns, &Lock);
    } while (0);

    pCmd->rc = rc;

    rc = VBoxSHGSMICommandComplete (pHgsmi, pCmd);
    AssertRC(rc);
}

static void vboxVDMAControlProcess(PVBOXVDMAHOST pVdma, PVBOXVDMA_CTL pCmd)
{
    PHGSMIINSTANCE pHgsmi = pVdma->pHgsmi;
    pCmd->i32Result = VINF_SUCCESS;
    int rc = VBoxSHGSMICommandComplete (pHgsmi, pCmd);
    AssertRC(rc);
}

#ifdef VBOX_VDMA_WITH_WATCHDOG
static DECLCALLBACK(void) vboxVDMAWatchDogTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    VBOXVDMAHOST *pVdma = (VBOXVDMAHOST *)pvUser;
    PVGASTATE pVGAState = pVdma->pVGAState;
    VBVARaiseIrq(pVGAState, HGSMIHOSTFLAGS_WATCHDOG);
}

static int vboxVDMAWatchDogCtl(struct VBOXVDMAHOST *pVdma, uint32_t cMillis)
{
    PPDMDEVINS pDevIns = pVdma->pVGAState->pDevInsR3;
    if (cMillis)
        TMTimerSetMillies(pVdma->WatchDogTimer, cMillis);
    else
        TMTimerStop(pVdma->WatchDogTimer);
    return VINF_SUCCESS;
}
#endif

int vboxVDMAConstruct(PVGASTATE pVGAState, uint32_t cPipeElements)
{
    int rc;
#ifdef VBOX_VDMA_WITH_WORKERTHREAD
    PVBOXVDMAHOST pVdma = (PVBOXVDMAHOST)RTMemAllocZ(RT_OFFSETOF(VBOXVDMAHOST, CmdPool.aCmds[cPipeElements]));
#else
    PVBOXVDMAHOST pVdma = (PVBOXVDMAHOST)RTMemAllocZ(sizeof(*pVdma));
#endif
    Assert(pVdma);
    if (pVdma)
    {
        pVdma->pHgsmi = pVGAState->pHGSMI;
        pVdma->pVGAState = pVGAState;

#ifdef VBOX_VDMA_WITH_WATCHDOG
        rc = PDMDevHlpTMTimerCreate(pVGAState->pDevInsR3, TMCLOCK_REAL, vboxVDMAWatchDogTimer,
                                    pVdma, TMTIMER_FLAGS_NO_CRIT_SECT,
                                    "VDMA WatchDog Timer", &pVdma->WatchDogTimer);
        AssertRC(rc);
#endif
        rc = VBoxVBVAExHSInit(&pVdma->CmdVbva);
        if (RT_SUCCESS(rc))
        {
            rc = VBoxVDMAThreadCreate(&pVdma->Thread, vboxVDMAWorkerThread, pVdma);
            if (RT_SUCCESS(rc))
            {
                pVGAState->pVdma = pVdma;
#ifdef VBOX_WITH_CRHGSMI
                int rcIgnored = vboxVDMACrCtlHgsmiSetup(pVdma); NOREF(rcIgnored); /** @todo is this ignoring intentional? */
#endif
                return VINF_SUCCESS;
            }
            else
                WARN(("VBoxVDMAThreadCreate faile %d\n", rc));

            VBoxVBVAExHSTerm(&pVdma->CmdVbva);
        }
        else
            WARN(("VBoxVBVAExHSInit faile %d\n", rc));

        RTMemFree(pVdma);
    }
    else
        rc = VERR_OUT_OF_RESOURCES;

    return rc;
}

int vboxVDMAReset(struct VBOXVDMAHOST *pVdma)
{
    VBVAEXHOSTCTL Ctl;
    Ctl.enmType = VBVAEXHOSTCTL_TYPE_HH_RESET;
    int rc = vdmaVBVACtlSubmitSync(pVdma, &Ctl, VBVAEXHOSTCTL_SOURCE_HOST_ANY);
    if (!RT_SUCCESS(rc))
    {
        WARN(("vdmaVBVACtlSubmitSync failed %d\n", rc));
        return rc;
    }
    return VINF_SUCCESS;
}

int vboxVDMADestruct(struct VBOXVDMAHOST *pVdma)
{
    VBVAEXHOSTCTL Ctl;
    Ctl.enmType = VBVAEXHOSTCTL_TYPE_HH_TERM;
    int rc = vdmaVBVACtlSubmitSync(pVdma, &Ctl, VBVAEXHOSTCTL_SOURCE_HOST_ANY);
    if (!RT_SUCCESS(rc))
    {
        WARN(("vdmaVBVACtlSubmitSync failed %d\n", rc));
        return rc;
    }
    VBoxVDMAThreadTerm(&pVdma->Thread);
    VBoxVBVAExHSTerm(&pVdma->CmdVbva);
    RTMemFree(pVdma);
    return VINF_SUCCESS;
}

int vboxVDMASaveStateExecPrep(struct VBOXVDMAHOST *pVdma, PSSMHANDLE pSSM)
{
#ifdef VBOX_WITH_CRHGSMI
    PVGASTATE pVGAState = pVdma->pVGAState;
    PVBOXVDMACMD_CHROMIUM_CTL pCmd = (PVBOXVDMACMD_CHROMIUM_CTL)vboxVDMACrCtlCreate(
            VBOXVDMACMD_CHROMIUM_CTL_TYPE_SAVESTATE_BEGIN, sizeof (*pCmd));
    Assert(pCmd);
    if (pCmd)
    {
        int rc = vboxVDMACrCtlPost(pVGAState, pCmd, sizeof (*pCmd));
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            rc = vboxVDMACrCtlGetRc(pCmd);
        }
        vboxVDMACrCtlRelease(pCmd);
        return rc;
    }
    return VERR_NO_MEMORY;
#else
    return VINF_SUCCESS;
#endif
}

int vboxVDMASaveStateExecDone(struct VBOXVDMAHOST *pVdma, PSSMHANDLE pSSM)
{
#ifdef VBOX_WITH_CRHGSMI
    PVGASTATE pVGAState = pVdma->pVGAState;
    PVBOXVDMACMD_CHROMIUM_CTL pCmd = (PVBOXVDMACMD_CHROMIUM_CTL)vboxVDMACrCtlCreate(
            VBOXVDMACMD_CHROMIUM_CTL_TYPE_SAVESTATE_END, sizeof (*pCmd));
    Assert(pCmd);
    if (pCmd)
    {
        int rc = vboxVDMACrCtlPost(pVGAState, pCmd, sizeof (*pCmd));
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            rc = vboxVDMACrCtlGetRc(pCmd);
        }
        vboxVDMACrCtlRelease(pCmd);
        return rc;
    }
    return VERR_NO_MEMORY;
#else
    return VINF_SUCCESS;
#endif
}

void vboxVDMAControl(struct VBOXVDMAHOST *pVdma, PVBOXVDMA_CTL pCmd, uint32_t cbCmd)
{
#if 1
    PHGSMIINSTANCE pIns = pVdma->pHgsmi;

    switch (pCmd->enmCtl)
    {
        case VBOXVDMA_CTL_TYPE_ENABLE:
            pCmd->i32Result = VINF_SUCCESS;
            break;
        case VBOXVDMA_CTL_TYPE_DISABLE:
            pCmd->i32Result = VINF_SUCCESS;
            break;
        case VBOXVDMA_CTL_TYPE_FLUSH:
            pCmd->i32Result = VINF_SUCCESS;
            break;
#ifdef VBOX_VDMA_WITH_WATCHDOG
        case VBOXVDMA_CTL_TYPE_WATCHDOG:
            pCmd->i32Result = vboxVDMAWatchDogCtl(pVdma, pCmd->u32Offset);
            break;
#endif
        default:
            AssertBreakpoint();
            pCmd->i32Result = VERR_NOT_SUPPORTED;
    }

    int rc = VBoxSHGSMICommandComplete (pIns, pCmd);
    AssertRC(rc);
#else
    /* test asinch completion */
    VBOXVDMACMD_SUBMIT_CONTEXT Context;
    Context.pVdma = pVdma;
    Context.Cmd.enmType = VBOXVDMAPIPE_CMD_TYPE_DMACTL;
    Context.Cmd.u.pCtl = pCmd;

    int rc = vboxVDMAPipeModifyClient(&pVdma->Pipe, vboxVDMACommandSubmitCb, &Context);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        Assert(Context.bQueued);
        if (Context.bQueued)
        {
            /* success */
            return;
        }
        rc = VERR_OUT_OF_RESOURCES;
    }

    /* failure */
    Assert(RT_FAILURE(rc));
    PHGSMIINSTANCE pIns = pVdma->pHgsmi;
    pCmd->i32Result = rc;
    int tmpRc = VBoxSHGSMICommandComplete (pIns, pCmd);
    AssertRC(tmpRc);

#endif
}

void vboxVDMACommand(struct VBOXVDMAHOST *pVdma, PVBOXVDMACBUF_DR pCmd, uint32_t cbCmd)
{
    int rc = VERR_NOT_IMPLEMENTED;

#ifdef VBOX_WITH_CRHGSMI
    /* chromium commands are processed by crhomium hgcm thread independently from our internal cmd processing pipeline
     * this is why we process them specially */
    rc = vboxVDMACmdCheckCrCmd(pVdma, pCmd, cbCmd);
    if (rc == VINF_SUCCESS)
        return;

    if (RT_FAILURE(rc))
    {
        pCmd->rc = rc;
        rc = VBoxSHGSMICommandComplete (pVdma->pHgsmi, pCmd);
        AssertRC(rc);
        return;
    }
#endif

#ifndef VBOX_VDMA_WITH_WORKERTHREAD
    vboxVDMACommandProcess(pVdma, pCmd, cbCmd);
#else

# ifdef DEBUG_misha
    Assert(0);
# endif

    VBOXVDMACMD_SUBMIT_CONTEXT Context;
    Context.pVdma = pVdma;
    Context.Cmd.enmType = VBOXVDMAPIPE_CMD_TYPE_DMACMD;
    Context.Cmd.u.pDr = pCmd;

    rc = vboxVDMAPipeModifyClient(&pVdma->Pipe, vboxVDMACommandSubmitCb, &Context);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        Assert(Context.bQueued);
        if (Context.bQueued)
        {
            /* success */
            return;
        }
        rc = VERR_OUT_OF_RESOURCES;
    }
    /* failure */
    Assert(RT_FAILURE(rc));
    PHGSMIINSTANCE pIns = pVdma->pHgsmi;
    pCmd->rc = rc;
    int tmpRc = VBoxSHGSMICommandComplete (pIns, pCmd);
    AssertRC(tmpRc);
#endif
}

/**/

static int vdmaVBVACtlSubmit(PVBOXVDMAHOST pVdma, VBVAEXHOSTCTL* pCtl, VBVAEXHOSTCTL_SOURCE enmSource, PFNVBVAEXHOSTCTL_COMPLETE pfnComplete, void *pvComplete)
{
    int rc = VBoxVBVAExHCtlSubmit(&pVdma->CmdVbva, pCtl, enmSource, pfnComplete, pvComplete);
    if (RT_SUCCESS(rc))
    {
        if (rc == VINF_SUCCESS)
            return VBoxVDMAThreadEventNotify(&pVdma->Thread);
        else
            Assert(rc == VINF_ALREADY_INITIALIZED);
    }
    else
        WARN(("VBoxVBVAExHCtlSubmit failed %d\n", rc));

    return rc;
}

static DECLCALLBACK(void) vboxCmdVBVACmdCtlGuestCompletion(VBVAEXHOSTCONTEXT *pVbva, struct VBVAEXHOSTCTL *pCtl, int rc, void *pvContext)
{
    PVBOXVDMAHOST pVdma = (PVBOXVDMAHOST)pvContext;
    VBOXCMDVBVA_CTL *pGCtl = (VBOXCMDVBVA_CTL*)(pCtl->u.cmd.pu8Cmd - sizeof (VBOXCMDVBVA_CTL));
    AssertRC(rc);
    pGCtl->i32Result = rc;

    Assert(pVdma->pVGAState->fGuestCaps & VBVACAPS_COMPLETEGCMD_BY_IOREAD);
    rc = VBoxSHGSMICommandComplete(pVdma->pHgsmi, pGCtl);
    AssertRC(rc);

    VBoxVBVAExHCtlFree(pVbva, pCtl);
}

static int vdmaVBVACtlOpaqueSubmit(PVBOXVDMAHOST pVdma, VBVAEXHOSTCTL_SOURCE enmSource, uint8_t* pu8Cmd, uint32_t cbCmd, PFNVBVAEXHOSTCTL_COMPLETE pfnComplete, void *pvComplete)
{
    VBVAEXHOSTCTL* pHCtl = VBoxVBVAExHCtlCreate(&pVdma->CmdVbva, VBVAEXHOSTCTL_TYPE_GHH_BE_OPAQUE);
    if (!pHCtl)
    {
        WARN(("VBoxVBVAExHCtlCreate failed\n"));
        return VERR_NO_MEMORY;
    }

    pHCtl->u.cmd.pu8Cmd = pu8Cmd;
    pHCtl->u.cmd.cbCmd = cbCmd;
    int rc = vdmaVBVACtlSubmit(pVdma, pHCtl, enmSource, pfnComplete, pvComplete);
    if (!RT_SUCCESS(rc))
    {
        WARN(("vdmaVBVACtlSubmit failed rc %d\n", rc));
        return rc;;
    }
    return VINF_SUCCESS;
}

static int vdmaVBVACtlOpaqueGuestSubmit(PVBOXVDMAHOST pVdma, VBOXCMDVBVA_CTL *pCtl, uint32_t cbCtl)
{
    Assert(cbCtl >= sizeof (VBOXCMDVBVA_CTL));
    VBoxSHGSMICommandMarkAsynchCompletion(pCtl);
    int rc = vdmaVBVACtlOpaqueSubmit(pVdma, VBVAEXHOSTCTL_SOURCE_GUEST, (uint8_t*)(pCtl+1), cbCtl - sizeof (VBOXCMDVBVA_CTL), vboxCmdVBVACmdCtlGuestCompletion, pVdma);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    WARN(("vdmaVBVACtlOpaqueSubmit failed %d\n", rc));
    pCtl->i32Result = rc;
    rc = VBoxSHGSMICommandComplete(pVdma->pHgsmi, pCtl);
    AssertRC(rc);
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) vboxCmdVBVACmdCtlHostCompletion(VBVAEXHOSTCONTEXT *pVbva, struct VBVAEXHOSTCTL *pCtl, int rc, void *pvCompletion)
{
    VBOXCRCMDCTL* pVboxCtl = (VBOXCRCMDCTL*)pCtl->u.cmd.pu8Cmd;
    if (pVboxCtl->pfnInternal)
        ((PFNCRCTLCOMPLETION)pVboxCtl->pfnInternal)(pVboxCtl, pCtl->u.cmd.cbCmd, rc, pvCompletion);
    VBoxVBVAExHCtlFree(pVbva, pCtl);
}

static int vdmaVBVACtlOpaqueHostSubmit(PVBOXVDMAHOST pVdma, struct VBOXCRCMDCTL* pCmd, uint32_t cbCmd,
        PFNCRCTLCOMPLETION pfnCompletion,
        void *pvCompletion)
{
    pCmd->pfnInternal = (void(*)())pfnCompletion;
    int rc = vdmaVBVACtlOpaqueSubmit(pVdma, VBVAEXHOSTCTL_SOURCE_HOST_ENABLED, (uint8_t*)pCmd, cbCmd, vboxCmdVBVACmdCtlHostCompletion, pvCompletion);
    if (!RT_SUCCESS(rc))
    {
        if (rc == VERR_INVALID_STATE)
        {
            pCmd->pfnInternal = NULL;
            PVGASTATE pVGAState = pVdma->pVGAState;
            rc = pVGAState->pDrv->pfnCrHgcmCtlSubmit(pVGAState->pDrv, pCmd, cbCmd, pfnCompletion, pvCompletion);
            if (!RT_SUCCESS(rc))
                WARN(("pfnCrHgsmiControlProcess failed %d\n", rc));

            return rc;
        }
        WARN(("vdmaVBVACtlOpaqueSubmit failed %d\n", rc));
        return rc;
    }

    return VINF_SUCCESS;
}

static int vdmaVBVACtlEnableDisableSubmitInternal(PVBOXVDMAHOST pVdma, VBOXCMDVBVA_CTL_ENABLE *pEnable, PFNVBVAEXHOSTCTL_COMPLETE pfnComplete, void *pvComplete)
{
    VBVAEXHOSTCTL* pHCtl = VBoxVBVAExHCtlCreate(&pVdma->CmdVbva, VBVAEXHOSTCTL_TYPE_GH_ENABLE_DISABLE);
    if (!pHCtl)
    {
        WARN(("VBoxVBVAExHCtlCreate failed\n"));
        return VERR_NO_MEMORY;
    }

    pHCtl->u.cmd.pu8Cmd = (uint8_t*)&pEnable->Enable;
    pHCtl->u.cmd.cbCmd = sizeof (pEnable->Enable);
    int rc = vdmaVBVACtlSubmit(pVdma, pHCtl, VBVAEXHOSTCTL_SOURCE_GUEST, pfnComplete, pvComplete);
    if (!RT_SUCCESS(rc))
    {
        WARN(("vdmaVBVACtlSubmit failed rc %d\n", rc));
        return rc;;
    }
    return VINF_SUCCESS;
}

static int vdmaVBVACtlEnableDisableSubmit(PVBOXVDMAHOST pVdma, VBOXCMDVBVA_CTL_ENABLE *pEnable)
{
    VBoxSHGSMICommandMarkAsynchCompletion(&pEnable->Hdr);
    int rc = vdmaVBVACtlEnableDisableSubmitInternal(pVdma, pEnable, vboxCmdVBVACmdCtlGuestCompletion, pVdma);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    WARN(("vdmaVBVACtlEnableDisableSubmitInternal failed %d\n", rc));
    pEnable->Hdr.i32Result = rc;
    rc = VBoxSHGSMICommandComplete(pVdma->pHgsmi, &pEnable->Hdr);
    AssertRC(rc);
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) vdmaVBVACtlSubmitSyncCompletion(VBVAEXHOSTCONTEXT *pVbva, struct VBVAEXHOSTCTL *pCtl, int rc, void *pvContext)
{
    VDMA_VBVA_CTL_CYNC_COMPLETION *pData = (VDMA_VBVA_CTL_CYNC_COMPLETION*)pvContext;
    pData->rc = rc;
    rc = RTSemEventSignal(pData->hEvent);
    if (!RT_SUCCESS(rc))
        WARN(("RTSemEventSignal failed %d\n", rc));
}

static int vdmaVBVACtlSubmitSync(PVBOXVDMAHOST pVdma, VBVAEXHOSTCTL* pCtl, VBVAEXHOSTCTL_SOURCE enmSource)
{
    VDMA_VBVA_CTL_CYNC_COMPLETION Data;
    Data.rc = VERR_NOT_IMPLEMENTED;
    int rc = RTSemEventCreate(&Data.hEvent);
    if (!RT_SUCCESS(rc))
    {
        WARN(("RTSemEventCreate failed %d\n", rc));
        return rc;
    }

    rc = vdmaVBVACtlSubmit(pVdma, pCtl, enmSource, vdmaVBVACtlSubmitSyncCompletion, &Data);
    if (RT_SUCCESS(rc))
    {
        rc = RTSemEventWait(Data.hEvent, RT_INDEFINITE_WAIT);
        if (RT_SUCCESS(rc))
        {
            rc = Data.rc;
            if (!RT_SUCCESS(rc))
                WARN(("vdmaVBVACtlSubmitSyncCompletion returned %d\n", rc));
        }
        else
            WARN(("RTSemEventWait failed %d\n", rc));
    }
    else
        WARN(("vdmaVBVACtlSubmit failed %d\n", rc));

    RTSemEventDestroy(Data.hEvent);

    return rc;
}

static int vdmaVBVAPause(PVBOXVDMAHOST pVdma)
{
    VBVAEXHOSTCTL Ctl;
    Ctl.enmType = VBVAEXHOSTCTL_TYPE_HH_INTERNAL_PAUSE;
    return vdmaVBVACtlSubmitSync(pVdma, &Ctl, VBVAEXHOSTCTL_SOURCE_HOST_ANY);
}

static int vdmaVBVAResume(PVBOXVDMAHOST pVdma)
{
    VBVAEXHOSTCTL Ctl;
    Ctl.enmType = VBVAEXHOSTCTL_TYPE_HH_INTERNAL_RESUME;
    return vdmaVBVACtlSubmitSync(pVdma, &Ctl, VBVAEXHOSTCTL_SOURCE_HOST_ANY);
}

static int vboxVDMACmdSubmitPerform(struct VBOXVDMAHOST *pVdma)
{
    int rc = VBoxVBVAExHSCheckCommands(&pVdma->CmdVbva);
    switch (rc)
    {
        case VINF_SUCCESS:
            return VBoxVDMAThreadEventNotify(&pVdma->Thread);
        case VINF_ALREADY_INITIALIZED:
        case VINF_EOF:
        case VERR_INVALID_STATE:
            return VINF_SUCCESS;
        default:
            Assert(!RT_FAILURE(rc));
            return RT_FAILURE(rc) ? rc : VERR_INTERNAL_ERROR;
    }
}


int vboxCmdVBVACmdHostCtl(PPDMIDISPLAYVBVACALLBACKS pInterface,
                                                               struct VBOXCRCMDCTL* pCmd, uint32_t cbCmd,
                                                               PFNCRCTLCOMPLETION pfnCompletion,
                                                               void *pvCompletion)
{
    PVGASTATE pVGAState = PPDMIDISPLAYVBVACALLBACKS_2_PVGASTATE(pInterface);
    struct VBOXVDMAHOST *pVdma = pVGAState->pVdma;
    return vdmaVBVACtlOpaqueHostSubmit(pVdma, pCmd, cbCmd, pfnCompletion, pvCompletion);
}

int vboxCmdVBVACmdCtl(PVGASTATE pVGAState, VBOXCMDVBVA_CTL *pCtl, uint32_t cbCtl)
{
    struct VBOXVDMAHOST *pVdma = pVGAState->pVdma;
    int rc = VINF_SUCCESS;
    switch (pCtl->u32Type)
    {
        case VBOXCMDVBVACTL_TYPE_3DCTL:
            return vdmaVBVACtlOpaqueGuestSubmit(pVdma, pCtl, cbCtl);
        case VBOXCMDVBVACTL_TYPE_ENABLE:
            if (cbCtl != sizeof (VBOXCMDVBVA_CTL_ENABLE))
            {
                WARN(("incorrect enable size\n"));
                rc = VERR_INVALID_PARAMETER;
                break;
            }
            return vdmaVBVACtlEnableDisableSubmit(pVdma, (VBOXCMDVBVA_CTL_ENABLE*)pCtl);
        default:
            WARN(("unsupported type\n"));
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    pCtl->i32Result = rc;
    rc = VBoxSHGSMICommandComplete(pVdma->pHgsmi, pCtl);
    AssertRC(rc);
    return VINF_SUCCESS;
}

int vboxCmdVBVACmdSubmit(PVGASTATE pVGAState)
{
    if (!VBoxVBVAExHSIsEnabled(&pVGAState->pVdma->CmdVbva))
    {
        WARN(("vdma VBVA is disabled\n"));
        return VERR_INVALID_STATE;
    }

    return vboxVDMACmdSubmitPerform(pVGAState->pVdma);
}

int vboxCmdVBVACmdFlush(PVGASTATE pVGAState)
{
    WARN(("flush\n"));
    if (!VBoxVBVAExHSIsEnabled(&pVGAState->pVdma->CmdVbva))
    {
        WARN(("vdma VBVA is disabled\n"));
        return VERR_INVALID_STATE;
    }
    return vboxVDMACmdSubmitPerform(pVGAState->pVdma);
}

void vboxCmdVBVACmdTimer(PVGASTATE pVGAState)
{
    if (!VBoxVBVAExHSIsEnabled(&pVGAState->pVdma->CmdVbva))
        return;
    vboxVDMACmdSubmitPerform(pVGAState->pVdma);
}
