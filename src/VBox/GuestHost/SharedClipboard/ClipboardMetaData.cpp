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

/**
 * Initializes a clipboard meta data struct.
 *
 * @returns VBox status code.
 * @param   pMeta               Meta data struct to initialize.
 */
int SharedClipboardMetaDataInit(PSHAREDCLIPBOARDMETADATA pMeta)
{
    AssertPtrReturn(pMeta, VERR_INVALID_POINTER);

    pMeta->pvMeta = NULL;
    pMeta->cbMeta = 0;
    pMeta->cbUsed = 0;

    return VINF_SUCCESS;
}

/**
 * Destroys a clipboard meta data struct by free'ing all its data.
 *
 * @param   pMeta               Meta data struct to destroy.
 */
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

    pMeta->cbUsed = 0;
}

/**
 * Adds new meta data to a meta data struct.
 *
 * @returns VBox status code.
 * @param   pMeta               Meta data struct to add data to.
 */
int SharedClipboardMetaDataAdd(PSHAREDCLIPBOARDMETADATA pMeta, const void *pvDataAdd, uint32_t cbDataAdd)
{
    AssertPtrReturn(pMeta, VERR_INVALID_POINTER);

    LogFlowFunc(("pvDataAdd=%p, cbDatadd=%RU32\n", pvDataAdd, cbDataAdd));

    if (!cbDataAdd)
        return VINF_SUCCESS;
    AssertPtrReturn(pvDataAdd, VERR_INVALID_POINTER);

    int rc = SharedClipboardMetaDataResize(pMeta, pMeta->cbMeta + cbDataAdd);
    if (RT_FAILURE(rc))
        return rc;

    Assert(pMeta->cbMeta >= pMeta->cbUsed + cbDataAdd);
    memcpy((uint8_t *)pMeta->pvMeta + pMeta->cbUsed, pvDataAdd, cbDataAdd);

    pMeta->cbUsed += cbDataAdd;

    return rc;
}

/**
 * Resizes the data buffer of a meta data struct.
 * Note: At the moment only supports growing the data buffer.
 *
 * @returns VBox status code.
 * @param   pMeta               Meta data struct to resize.
 * @param   cbNewSize           New size (in bytes) to use for resizing.
 */
int SharedClipboardMetaDataResize(PSHAREDCLIPBOARDMETADATA pMeta, uint32_t cbNewSize)
{
    AssertPtrReturn(pMeta, VERR_INVALID_POINTER);

    if (!cbNewSize)
    {
        SharedClipboardMetaDataDestroy(pMeta);
        return VINF_SUCCESS;
    }

    if (cbNewSize == pMeta->cbMeta)
        return VINF_SUCCESS;

    if (cbNewSize > _32M) /* Meta data can be up to 32MB. */
        return VERR_INVALID_PARAMETER;

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

/**
 * Returns the actual free bytes of a meta data struct.
 *
 * @returns Actual free bytes of a meta data struct.
 * @param   pMeta               Meta data struct to return free bytes for.
 */
size_t SharedClipboardMetaDataGetFree(PSHAREDCLIPBOARDMETADATA pMeta)
{
    AssertPtrReturn(pMeta, 0);
    return pMeta->cbMeta - pMeta->cbUsed;
}

/**
 * Returns the actual used bytes of a meta data struct.
 *
 * @returns Actual used bytes of a meta data struct.
 * @param   pMeta               Meta data struct to return used bytes for.
 */
size_t SharedClipboardMetaDataGetUsed(PSHAREDCLIPBOARDMETADATA pMeta)
{
    AssertPtrReturn(pMeta, 0);
    return pMeta->cbUsed;
}

/**
 * Returns the overall (allocated) size in bytes of a meta data struct.
 *
 * @returns Overall (allocated) size of a meta data struct.
 * @param   pMeta               Meta data struct to return size for.
 */
size_t SharedClipboardMetaDataGetSize(PSHAREDCLIPBOARDMETADATA pMeta)
{
    AssertPtrReturn(pMeta, 0);
    return pMeta->cbMeta;
}

/**
 * Returns the a mutable raw pointer to the actual meta data.
 *
 * @returns Mutable raw pointer to the actual meta data.
 * @param   pMeta               Meta data struct to return mutable data pointer for.
 */
void *SharedClipboardMetaDataMutableRaw(PSHAREDCLIPBOARDMETADATA pMeta)
{
    return pMeta->pvMeta;
}

/**
 * Returns the a const'ed raw pointer to the actual meta data.
 *
 * @returns Const'ed raw pointer to the actual meta data.
 * @param   pMeta               Meta data struct to return const'ed data pointer for.
 */
const void *SharedClipboardMetaDataRaw(PSHAREDCLIPBOARDMETADATA pMeta)
{
    return pMeta->pvMeta;
}

