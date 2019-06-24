/* $Id$ */
/** @file
 * Shared Clipboard - Provider implementation for host service (host side).
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

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/errcore.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/string.h>

#include <VBox/log.h>




SharedClipboardProviderHostService::SharedClipboardProviderHostService(SharedClipboardArea *pArea)
    : m_pArea(pArea)
{
    LogFlowFuncEnter();

    Reset();

    m_pArea->AddRef();
}

SharedClipboardProviderHostService::~SharedClipboardProviderHostService(void)
{
    m_pArea->Release();
}

int SharedClipboardProviderHostService::Prepare(void)
{
    LogFlowFuncEnter();

    /*return vboxSvcClipboardReportMsg(,
                                     VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA, );*/
    return 0;
}

int SharedClipboardProviderHostService::ReadDataHdr(PVBOXCLIPBOARDDATAHDR *ppDataHdr)
{
    AssertPtrReturn(ppDataHdr, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    /*return eventWait(VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA_HDR, m_Callbacks.pfnReadDataHdr, m_uTimeoutMs,
                     (void **)ppDataHdr);*/
    return 0;
}

int SharedClipboardProviderHostService::WriteDataHdr(const PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    AssertPtrReturn(pDataHdr, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProviderHostService::ReadDataChunk(const PVBOXCLIPBOARDDATAHDR pDataHdr, void *pvChunk, uint32_t cbChunk,
                                                      uint32_t fFlags /* = 0 */, uint32_t *pcbRead /* = NULL*/)
{
    RT_NOREF(pDataHdr, pvChunk, cbChunk, fFlags, pcbRead);

    /*return eventWait(VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA_CHUNK, m_Callbacks.pfnReadDataChunk, m_uTimeoutMs,
                     (void **)ppDataChunk);*/
    return 0;
}

int SharedClipboardProviderHostService::WriteDataChunk(const PVBOXCLIPBOARDDATAHDR pDataHdr, const void *pvChunk, uint32_t cbChunk,
                                                       uint32_t fFlags /* = 0 */, uint32_t *pcbWritten /* = NULL */)
{
    RT_NOREF(pDataHdr, pvChunk, cbChunk, fFlags, pcbWritten);
    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProviderHostService::ReadDirectory(PVBOXCLIPBOARDDIRDATA *ppDirData)
{
    //return eventWait(VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DIR, NULL, m_uTimeoutMs, (void **)ppDirData);
    RT_NOREF(ppDirData);
    return 0;
}

int SharedClipboardProviderHostService::WriteDirectory(const PVBOXCLIPBOARDDIRDATA pDirData)
{
    RT_NOREF(pDirData);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderHostService::ReadFileHdr(PVBOXCLIPBOARDFILEHDR *ppFileHdr)
{
    RT_NOREF(ppFileHdr);
    //return eventWait(VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_FILE_HDR, NULL, m_uTimeoutMs, (void **)ppFileHdr);
    return 0;
}

int SharedClipboardProviderHostService::WriteFileHdr(const PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    RT_NOREF(pFileHdr);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderHostService::ReadFileData(void *pvData, uint32_t cbData, uint32_t fFlags /* = 0 */,
                                                     uint32_t *pcbRead /* = NULL */)
{
    RT_NOREF(pvData, cbData, fFlags, pcbRead);
    //return eventWait(VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_FILE_DATA, NULL, m_uTimeoutMs, (void **)ppFileData);
    return 0;
}

int SharedClipboardProviderHostService::WriteFileData(void *pvData, uint32_t cbData, uint32_t fFlags /* = 0 */,
                                                      uint32_t *pcbWritten /* = NULL */)
{
    RT_NOREF(pvData, cbData, fFlags, pcbWritten);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}
void SharedClipboardProviderHostService::Reset(void)
{
    LogFlowFuncEnter();

    eventUnregisterAll();

    uint32_t aMsgIDs[] = { VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_DATA_HDR,
                           VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA_HDR,
                           VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_DATA_CHUNK,
                           VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA_CHUNK,
                           VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_DIR,
                           VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DIR,
                           VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_FILE_HDR,
                           VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_FILE_HDR,
                           VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_FILE_DATA,
                           VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_FILE_DATA };

    for (unsigned i = 0; i < RT_ELEMENTS(aMsgIDs); i++)
    {
        int rc2 = eventRegister(aMsgIDs[i]);
        AssertRC(rc2);
    }
}

int SharedClipboardProviderHostService::OnWrite(PSHAREDCLIPBOARDPROVIDERWRITEPARMS pParms)
{
    RT_NOREF(pParms);

    LogFlowFuncEnter();

    int rc = VERR_NOT_SUPPORTED;

#if 0
    switch (pParms->u.HostService.uMsg)
    {
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA_HDR:
        {
            VBOXCLIPBOARDDATAHDR dataHdr;
            rc = VBoxSvcClipboardURIReadDataHdr(pParms->u.HostService.cParms, pParms->u.HostService.paParms, &dataHdr);
            if (RT_SUCCESS(rc))
            {
                /** @todo Handle compression type. */
                /** @todo Handle checksum type. */
        #if 0
                Event *pEvent = eventGet(pParms->u.HostService.uMsg);
                if (pEvent)
                    rc = pEvent->SetData(SharedClipboardURIDataHdrDup(&dataHdr),
                                         sizeof(dataHdr) + dataHdr.cbMetaFmt + dataHdr.cbChecksum);
        #endif
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA_CHUNK:
        {
            VBOXCLIPBOARDDATACHUNK dataChunk;
            rc = VBoxSvcClipboardURIReadDataChunk(pParms->u.HostService.cParms, pParms->u.HostService.paParms, &dataChunk);
            if (RT_SUCCESS(rc))
            {
                Event *pEvent = eventGet(pParms->u.HostService.uMsg);
                if (pEvent)
                    rc = pEvent->SetData(SharedClipboardURIDataChunkDup(&dataChunk),
                                         sizeof(dataChunk) + dataChunk.cbData + dataChunk.cbChecksum);
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DIR:
        {
            VBOXCLIPBOARDDIRDATA dirData;
            rc = VBoxSvcClipboardURIReadDir(pParms->u.HostService.cParms, pParms->u.HostService.paParms, &dirData);
            if (RT_SUCCESS(rc))
            {
                Event *pEvent = eventGet(pParms->u.HostService.uMsg);
                if (pEvent)
                    rc = pEvent->SetData(SharedClipboardURIDirDataDup(&dirData),
                                         sizeof(dirData) + dirData.cbPath);
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_FILE_HDR:
        {
            VBOXCLIPBOARDFILEHDR fileHdr;
            rc = VBoxSvcClipboardURIReadFileHdr(pParms->u.HostService.cParms, pParms->u.HostService.paParms, &fileHdr);
            if (RT_SUCCESS(rc))
            {
                Event *pEvent = eventGet(pParms->u.HostService.uMsg);
                if (pEvent)
                    rc = pEvent->SetData(SharedClipboardURIFileHdrDup(&fileHdr),
                                         sizeof(fileHdr) + fileHdr.cbFilePath);
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_FILE_DATA:
        {
            VBOXCLIPBOARDFILEDATA fileData;
            rc = VBoxSvcClipboardURIReadFileData(pParms->u.HostService.cParms, pParms->u.HostService.paParms, &fileData);
            if (RT_SUCCESS(rc))
            {
                Event *pEvent = eventGet(pParms->u.HostService.uMsg);
                if (pEvent)
                    rc = pEvent->SetData(SharedClipboardURIFileDataDup(&fileData),
                                         sizeof(fileData) + fileData.cbData + fileData.cbChecksum);
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_CANCEL:
        {
            AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_ERROR:
        {
            AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);
            break;
        }

        default:
            break;
    }

    if (RT_SUCCESS(rc))
    {
        rc = eventSignal(pParms->u.HostService.uMsg);
        if (rc == VERR_NOT_FOUND) /* Having an event is optional, so don't fail here. */
            rc = VINF_SUCCESS;
    }
#endif

    LogFlowFuncLeaveRC(rc);
    return rc;
}

