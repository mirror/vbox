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

#include <iprt/path.h>
#include <iprt/uri.h>

/**
 * Initializes a clipboard meta data struct.
 *
 * @returns VBox status code.
 * @param   pMeta               Meta data struct to initialize.
 * @param   enmFmt              Meta data format to use.
 */
int SharedClipboardMetaDataInit(PSHAREDCLIPBOARDMETADATA pMeta, SHAREDCLIPBOARDMETADATAFMT enmFmt)
{
    AssertPtrReturn(pMeta, VERR_INVALID_POINTER);

    pMeta->enmFmt = enmFmt;
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
 * Converts data to a specific meta data format.
 *
 * @returns VBox status code. VERR_NOT_SUPPORTED if the format is unknown.
 * @param   pvData              Pointer to data to convert.
 * @param   cbData              Size (in bytes) of data to convert.
 * @param   enmFmt              Meta data format to convert data to.
 * @param   ppvData             Where to store the converted data on success. Caller must free the data accordingly.
 * @param   pcbData             Where to store the size (in bytes) of the converted data. Optional.
 */
int SharedClipboardMetaDataConvertToFormat(const void *pvData, size_t cbData, SHAREDCLIPBOARDMETADATAFMT enmFmt,
                                           void **ppvData, uint32_t *pcbData)
{
    AssertPtrReturn(pvData,  VERR_INVALID_POINTER);
    AssertReturn   (cbData,  VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvData, VERR_INVALID_POINTER);

    /* pcbData is optional. */

    RT_NOREF(cbData);

    int rc = VERR_INVALID_PARAMETER;

    switch (enmFmt)
    {
        case SHAREDCLIPBOARDMETADATAFMT_URI_LIST:
        {
            char *pszTmp = RTStrDup((char *)pvData);
            if (!pszTmp)
            {
                rc = VERR_NO_MEMORY;
                break;
            }

            RTPathChangeToUnixSlashes(pszTmp, true /* fForce */);

            char  *pszURI;
            rc = RTUriFileCreateEx(pszTmp, RTPATH_STR_F_STYLE_UNIX, &pszURI, 0 /*cbUri*/, NULL /* pcchUri */);
            if (RT_SUCCESS(rc))
            {
                *ppvData = pszURI;
                if (pcbData)
                    *pcbData = (uint32_t)strlen(pszURI);

                rc = VINF_SUCCESS;
            }

            RTStrFree(pszTmp);
            break;
        }

        default:
            rc = VERR_NOT_SUPPORTED;
            AssertFailed();
            break;
    }

    LogFlowFuncLeaveRC(rc);
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

