/* $Id*/
/** @file
 * Shared Clipboard - Meta data handling functions.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <VBox/GuestHost/SharedClipboard-uri.h>

int SharedClipboardMetaDataInit(PSHAREDCLIPBOARDMETADATA pMeta)
{
    AssertPtrReturn(pMeta, VERR_INVALID_POINTER);

    Assert(pMeta->cbMeta == 0);
    Assert(pMeta->pvMeta == NULL);

    pMeta->pvMeta = NULL;
    pMeta->cbMeta = 0;

    return VINF_SUCCESS;
}

void SharedClipboardMetaDataDestroy(PSHAREDCLIPBOARDMETADATA pMeta)
{
    AssertPtrReturnVoid(pMeta);

    if (!pMeta)
        return;

    if (pMeta->pvMeta)
    {
        Assert(pMeta->cbMeta);

        RTMemFree(pMeta->pvMeta);

        pMeta->pvMeta = NULL;
        pMeta->cbMeta = 0;
    }
}

int SharedClipboardMetaDataAdd(PSHAREDCLIPBOARDMETADATA pMeta, const void *pvDataAdd, uint32_t cbDataAdd)
{
    AssertPtrReturn(pMeta, VERR_INVALID_POINTER);

    if (!cbDataAdd)
        return VINF_SUCCESS;
    AssertPtrReturn(pvDataAdd, VERR_INVALID_POINTER);

    int rc = SharedClipboardMetaDataResize(pMeta, pMeta->cbMeta + cbDataAdd);
    if (RT_FAILURE(rc))
        return rc;

    Assert(pMeta->cbMeta >= pMeta->cbUsed + cbDataAdd);
    memcpy((uint8_t *)pMeta->pvMeta + pMeta->cbUsed, pvDataAdd, cbDataAdd);

    pMeta->cbUsed += cbDataAdd;

    return cbDataAdd;
}

int SharedClipboardMetaDataResize(PSHAREDCLIPBOARDMETADATA pMeta, size_t cbNewSize)
{
    AssertPtrReturn(pMeta, VERR_INVALID_POINTER);

    if (!cbNewSize)
    {
        SharedClipboardMetaDataDestroy(pMeta);
        return VINF_SUCCESS;
    }

    if (cbNewSize == pMeta->cbMeta)
        return VINF_SUCCESS;

    void *pvTmp = NULL;
    if (!pMeta->cbMeta)
    {
        Assert(pMeta->cbUsed == 0);
        pvTmp = RTMemAllocZ(cbNewSize);
    }
    else
    {
        AssertPtr(pMeta->pvMeta);
        pvTmp = RTMemRealloc(pMeta->pvMeta, cbNewSize);
        RT_BZERO(pvTmp, cbNewSize);
    }

    if (pvTmp)
    {
        pMeta->pvMeta = pvTmp;
        pMeta->cbMeta = cbNewSize;
        return VINF_SUCCESS;
    }

    return VERR_NO_MEMORY;
}

size_t SharedClipboardMetaDataGetUsed(PSHAREDCLIPBOARDMETADATA pMeta)
{
    AssertPtrReturn(pMeta, VERR_INVALID_POINTER);
    return pMeta->cbUsed;
}

size_t SharedClipboardMetaDataGetSize(PSHAREDCLIPBOARDMETADATA pMeta)
{
    AssertPtrReturn(pMeta, VERR_INVALID_POINTER);
    return pMeta->cbMeta;
}

const void *SharedClipboardMetaDataRaw(PSHAREDCLIPBOARDMETADATA pMeta)
{
    return pMeta->pvMeta;
}

