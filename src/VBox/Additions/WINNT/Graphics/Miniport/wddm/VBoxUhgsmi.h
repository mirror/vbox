/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___VBoxUhgsmi_h__
#define ___VBoxUhgsmi_h__

#include <iprt/cdefs.h>
#include <iprt/types.h>

typedef struct VBOXUHGSMI *PVBOXUHGSMI;

typedef struct VBOXUHGSMI_BUFFER *PVBOXUHGSMI_BUFFER;
typedef void* HVBOXUHGSMI_SYNCHOBJECT;

typedef enum
{
    VBOXUHGSMI_SYNCHOBJECT_TYPE_UNKNOWN = 0,
    VBOXUHGSMI_SYNCHOBJECT_TYPE_EVENT,
    VBOXUHGSMI_SYNCHOBJECT_TYPE_SEMAPHORE
} VBOXUHGSMI_SYNCHOBJECT_TYPE;

typedef struct VBOXUHGSMI_BUFFER_LOCK_FLAGS
{
    union
    {
        struct
        {
            uint32_t bReadOnly  : 1;
            uint32_t bWriteOnly : 1;
            uint32_t bDonotWait : 1;
            uint32_t bDiscard   : 1;
            uint32_t Reserved   : 28;
        };
        uint32_t Value;
    };
} VBOXUHGSMI_BUFFER_LOCK_FLAGS;

typedef struct VBOXUHGSMI_BUFFER_SUBMIT_FLAGS
{
    union
    {
        struct
        {
            uint32_t bHostReadOnly          : 1;
            uint32_t bHostWriteOnly         : 1;
            uint32_t bDoNotRetire           : 1; /* <- the buffer will be used in a subsequent command */
            uint32_t bDoNotSignalCompletion : 1; /* <- do not signal notification object on completion for this alloc */
            uint32_t Reserved               : 28;
        };
        uint32_t Value;
    };
} VBOXUHGSMI_BUFFER_SUBMIT_FLAGS, *PVBOXUHGSMI_BUFFER_SUBMIT_FLAGS;

/* the caller can specify NULL as a hSynch and specify a valid enmSynchType to make UHGSMI create a proper object itself,
 *  */
typedef DECLCALLBACK(int) FNVBOXUHGSMI_BUFFER_CREATE(PVBOXUHGSMI pHgsmi, uint32_t cbBuf,
        VBOXUHGSMI_SYNCHOBJECT_TYPE enmSynchType, HVBOXUHGSMI_SYNCHOBJECT hSynch,
        PVBOXUHGSMI_BUFFER* ppBuf);
typedef FNVBOXUHGSMI_BUFFER_CREATE *PFNVBOXUHGSMI_BUFFER_CREATE;

typedef struct VBOXUHGSMI_BUFFER_SUBMIT
{
    PVBOXUHGSMI_BUFFER pBuf;
    uint32_t offData;
    uint32_t cbData;
    VBOXUHGSMI_BUFFER_SUBMIT_FLAGS fFlags;
} VBOXUHGSMI_BUFFER_SUBMIT, *PVBOXUHGSMI_BUFFER_SUBMIT;

typedef DECLCALLBACK(int) FNVBOXUHGSMI_BUFFER_SUBMIT_ASYNCH(PVBOXUHGSMI pHgsmi, PVBOXUHGSMI_BUFFER_SUBMIT aBuffers, uint32_t cBuffers);
typedef FNVBOXUHGSMI_BUFFER_SUBMIT_ASYNCH *PFNVBOXUHGSMI_BUFFER_SUBMIT_ASYNCH;

typedef DECLCALLBACK(int) FNVBOXUHGSMI_BUFFER_DESTROY(PVBOXUHGSMI_BUFFER pBuf);
typedef FNVBOXUHGSMI_BUFFER_DESTROY *PFNVBOXUHGSMI_BUFFER_DESTROY;

typedef DECLCALLBACK(int) FNVBOXUHGSMI_BUFFER_LOCK(PVBOXUHGSMI_BUFFER pBuf, uint32_t offLock, uint32_t cbLock, VBOXUHGSMI_BUFFER_LOCK_FLAGS fFlags, void**pvLock);
typedef FNVBOXUHGSMI_BUFFER_LOCK *PFNVBOXUHGSMI_BUFFER_LOCK;

typedef DECLCALLBACK(int) FNVBOXUHGSMI_BUFFER_UNLOCK(PVBOXUHGSMI_BUFFER pBuf);
typedef FNVBOXUHGSMI_BUFFER_UNLOCK *PFNVBOXUHGSMI_BUFFER_UNLOCK;

typedef struct VBOXUHGSMI
{
    PFNVBOXUHGSMI_BUFFER_CREATE pfnBufferCreate;
    PFNVBOXUHGSMI_BUFFER_SUBMIT_ASYNCH pfnBufferSubmitAsynch;
} VBOXUHGSMI, *PVBOXUHGSMI;

typedef struct VBOXUHGSMI_BUFFER
{
    PFNVBOXUHGSMI_BUFFER_LOCK pfnLock;
    PFNVBOXUHGSMI_BUFFER_UNLOCK pfnUnlock;
    PFNVBOXUHGSMI_BUFFER_DESTROY pfnDestroy;

    /* r/o data added for ease of access and simplicity
     * modifying it leads to unpredictable behavior */
    HVBOXUHGSMI_SYNCHOBJECT hSynch;
    VBOXUHGSMI_SYNCHOBJECT_TYPE enmSynchType;
    uint32_t cbBuffer;
    bool bSynchCreated;
} VBOXUHGSMI_BUFFER, *PVBOXUHGSMI_BUFFER;

#endif /* #ifndef ___VBoxUhgsmi_h__ */
