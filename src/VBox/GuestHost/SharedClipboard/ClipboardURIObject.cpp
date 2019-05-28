/* $Id$ */
/** @file
 * Shared Clipboard - URI object class. For handling creation/reading/writing to files and directories on host or guest side.
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

#include <iprt/dir.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/fs.h>
#include <iprt/path.h>
#include <iprt/uri.h>

#include <VBox/log.h>


SharedClipboardURIObject::SharedClipboardURIObject(void)
    : m_enmType(Type_Unknown)
    , m_enmView(View_Unknown)
{
    RT_ZERO(u);
}

SharedClipboardURIObject::SharedClipboardURIObject(Type enmType,
                                                   const RTCString &strSrcPathAbs /* = 0 */,
                                                   const RTCString &strDstPathAbs /* = 0 */)
    : m_enmType(enmType)
    , m_enmView(View_Unknown)
    , m_strSrcPathAbs(strSrcPathAbs)
    , m_strTgtPathAbs(strDstPathAbs)
{
    RT_ZERO(u);

    switch (m_enmType)
    {
        case Type_File:
        {
            u.File.hFile = NIL_RTFILE;
            break;
        }

        case Type_Directory:
        {
            u.Dir.hDir = NIL_RTDIR;
            break;
        }

        default:
            break;
    }
}

SharedClipboardURIObject::~SharedClipboardURIObject(void)
{
    closeInternal();
}

/**
 * Closes the object's internal handles (to files / ...).
 *
 */
void SharedClipboardURIObject::closeInternal(void)
{
    LogFlowThisFuncEnter();

    switch (m_enmType)
    {
        case Type_File:
        {
            if (RTFileIsValid(u.File.hFile))
            {
                RTFileClose(u.File.hFile);
                u.File.hFile = NIL_RTFILE;
            }
            RT_ZERO(u.File.objInfo);
            break;
        }

        case Type_Directory:
        {
            if (RTDirIsValid(u.Dir.hDir))
            {
                RTDirClose(u.Dir.hDir);
                u.Dir.hDir = NIL_RTDIR;
            }
            RT_ZERO(u.Dir.objInfo);
            break;
        }

        default:
            break;
    }
}

/**
 * Closes the object.
 * This also closes the internal handles associated with the object (to files / ...).
 */
void SharedClipboardURIObject::Close(void)
{
    closeInternal();
}

/**
 * Returns the directory / file mode of the object.
 *
 * @return  File / directory mode.
 */
RTFMODE SharedClipboardURIObject::GetMode(void) const
{
    switch (m_enmType)
    {
        case Type_File:
            return u.File.objInfo.Attr.fMode;

        case Type_Directory:
            return u.Dir.objInfo.Attr.fMode;

        default:
            break;
    }

    AssertFailed();
    return 0;
}

/**
 * Returns the bytes already processed (read / written).
 *
 * Note: Only applies if the object is of type SharedClipboardURIObject::Type_File.
 *
 * @return  Bytes already processed (read / written).
 */
uint64_t SharedClipboardURIObject::GetProcessed(void) const
{
    if (m_enmType == Type_File)
        return u.File.cbProcessed;

    return 0;
}

/**
 * Returns the file's logical size (in bytes).
 *
 * Note: Only applies if the object is of type SharedClipboardURIObject::Type_File.
 *
 * @return  The file's logical size (in bytes).
 */
uint64_t SharedClipboardURIObject::GetSize(void) const
{
    if (m_enmType == Type_File)
        return u.File.cbToProcess;

    return 0;
}

/**
 * Returns whether the processing of the object is complete or not.
 * For file objects this means that all bytes have been processed.
 *
 * @return  True if complete, False if not.
 */
bool SharedClipboardURIObject::IsComplete(void) const
{
    bool fComplete;

    switch (m_enmType)
    {
        case Type_File:
            Assert(u.File.cbProcessed <= u.File.cbToProcess);
            fComplete = u.File.cbProcessed == u.File.cbToProcess;
            break;

        case Type_Directory:
            fComplete = true;
            break;

        default:
            fComplete = true;
            break;
    }

    return fComplete;
}

/**
 * Returns whether the object is in an open state or not.
 */
bool SharedClipboardURIObject::IsOpen(void) const
{
    switch (m_enmType)
    {
        case Type_File:      return RTFileIsValid(u.File.hFile);
        case Type_Directory: return RTDirIsValid(u.Dir.hDir);
        default:             break;
    }

    return false;
}

/**
 * (Re-)Opens the object with a specific view, open and file mode.
 *
 * @return  IPRT status code.
 * @param   enmView             View to use for opening the object.
 * @param   fOpen               File open flags to use.
 * @param   fMode               File mode to use.
 */
int SharedClipboardURIObject::Open(View enmView, uint64_t fOpen /* = 0 */, RTFMODE fMode /* = 0 */)
{
    return OpenEx(  enmView == View_Source
                  ? m_strSrcPathAbs : m_strTgtPathAbs
                  , enmView, fOpen, fMode, SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE);
}

/**
 * Open the object with a specific file type, and, depending on the type, specifying additional parameters.
 *
 * @return  IPRT status code.
 * @param   strPathAbs          Absolute path of the object (file / directory / ...).
 * @param   enmView             View of the object.
 * @param   fOpen               Open mode to use; only valid for file objects.
 * @param   fMode               File mode to use; only valid for file objects.
 * @param   fFlags              Additional Shared Clipboard URI object flags.
 */
int SharedClipboardURIObject::OpenEx(const RTCString &strPathAbs, View enmView,
                                     uint64_t fOpen /* = 0 */, RTFMODE fMode /* = 0 */, SHAREDCLIPBOARDURIOBJECTFLAGS fFlags /* = SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE */)
{
    AssertReturn(!(fFlags & ~SHAREDCLIPBOARDURIOBJECT_FLAGS_VALID_MASK), VERR_INVALID_FLAGS);
    RT_NOREF1(fFlags);

    int rc = VINF_SUCCESS;

    switch (enmView)
    {
        case View_Source:
            m_strSrcPathAbs = strPathAbs;
            break;

        case View_Target:
            m_strTgtPathAbs = strPathAbs;
            break;

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (   RT_SUCCESS(rc)
        && fOpen) /* Opening mode specified? */
    {
        LogFlowThisFunc(("strPath=%s, enmView=%RU32, fOpen=0x%x, fMode=0x%x, fFlags=0x%x\n",
                         strPathAbs.c_str(), enmView, fOpen, fMode, fFlags));
        switch (m_enmType)
        {
            case Type_File:
            {
                /*
                 * Open files on the source with RTFILE_O_DENY_WRITE to prevent races
                 * where the OS writes to the file while the destination side transfers
                 * it over.
                 */
                LogFlowThisFunc(("Opening ...\n"));
                rc = RTFileOpen(&u.File.hFile, strPathAbs.c_str(), fOpen);
                if (RT_SUCCESS(rc))
                {
                    if (   (fOpen & RTFILE_O_WRITE) /* Only set the file mode on write. */
                        &&  fMode                   /* Some file mode to set specified? */)
                    {
                        rc = RTFileSetMode(u.File.hFile, fMode);
                    }
                    else if (fOpen & RTFILE_O_READ)
                    {
                        rc = queryInfoInternal(enmView);
                    }
                }

                if (RT_SUCCESS(rc))
                {
                    LogFlowThisFunc(("File cbObject=%RU64, fMode=0x%x\n",
                                     u.File.objInfo.cbObject, u.File.objInfo.Attr.fMode));
                    u.File.cbToProcess = u.File.objInfo.cbObject;
                    u.File.cbProcessed = 0;
                }

                break;
            }

            case Type_Directory:
            {
                rc = RTDirOpen(&u.Dir.hDir, strPathAbs.c_str());
                if (RT_SUCCESS(rc))
                    rc = queryInfoInternal(enmView);
                break;
            }

            default:
                rc = VERR_NOT_IMPLEMENTED;
                break;
        }
    }

    if (RT_SUCCESS(rc))
    {
        m_enmView = enmView;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Queries information about the object using a specific view, internal version.
 *
 * @return  IPRT status code.
 * @param   enmView             View to use for querying information. Currently ignored.
 */
int SharedClipboardURIObject::queryInfoInternal(View enmView)
{
    RT_NOREF(enmView);

    int rc;

    switch (m_enmType)
    {
        case Type_File:
            AssertMsgReturn(RTFileIsValid(u.File.hFile), ("Object has invalid file handle\n"), VERR_INVALID_STATE);
            rc = RTFileQueryInfo(u.File.hFile, &u.File.objInfo, RTFSOBJATTRADD_NOTHING);
            break;

        case Type_Directory:
            AssertMsgReturn(RTDirIsValid(u.Dir.hDir), ("Object has invalid directory handle\n"), VERR_INVALID_STATE);
            rc = RTDirQueryInfo(u.Dir.hDir, &u.Dir.objInfo, RTFSOBJATTRADD_NOTHING);
            break;

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    return rc;
}

/**
 * Queries information about the object using a specific view.
 *
 * @return  IPRT status code.
 * @param   enmView             View to use for querying information.
 */
int SharedClipboardURIObject::QueryInfo(View enmView)
{
    return queryInfoInternal(enmView);
}

/**
 * Rebases an absolute URI path from an old path base to a new path base.
 * This function is needed in order to transform path from the source side to the target side.
 *
 * @return  IPRT status code.
 * @param   strPathAbs          Absolute URI path to rebase.
 * @param   strBaseOld          Old base path to rebase from.
 * @param   strBaseNew          New base path to rebase to.
 *
 ** @todo Put this into an own class like SharedClipboardURIPath : public RTCString?
 */
/* static */
int SharedClipboardURIObject::RebaseURIPath(RTCString &strPathAbs,
                                const RTCString &strBaseOld /* = "" */,
                                const RTCString &strBaseNew /* = "" */)
{
    char *pszPath = RTUriFilePath(strPathAbs.c_str());
    if (!pszPath) /* No URI? */
         pszPath = RTStrDup(strPathAbs.c_str());

    int rc;

    if (pszPath)
    {
        const char *pszPathStart = pszPath;
        const char *pszBaseOld = strBaseOld.c_str();
        if (   pszBaseOld
            && RTPathStartsWith(pszPath, pszBaseOld))
        {
            pszPathStart += strlen(pszBaseOld);
        }

        rc = VINF_SUCCESS;

        if (RT_SUCCESS(rc))
        {
            char *pszPathNew = RTPathJoinA(strBaseNew.c_str(), pszPathStart);
            if (pszPathNew)
            {
                char *pszPathURI = RTUriCreate("file" /* pszScheme */, "/" /* pszAuthority */,
                                               pszPathNew /* pszPath */,
                                               NULL /* pszQuery */, NULL /* pszFragment */);
                if (pszPathURI)
                {
                    LogFlowFunc(("Rebasing \"%s\" to \"%s\"\n", strPathAbs.c_str(), pszPathURI));

                    strPathAbs = RTCString(pszPathURI) + "\r\n";
                    RTStrFree(pszPathURI);
                }
                else
                    rc = VERR_INVALID_PARAMETER;

                RTStrFree(pszPathNew);
            }
            else
                rc = VERR_NO_MEMORY;
        }

        RTStrFree(pszPath);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

/**
 * Reads data from the object. Only applies to files objects.
 *
 * @return  IPRT status code.
 * @param   pvBuf               Buffer where to store the read data.
 * @param   cbBuf               Size (in bytes) of the buffer.
 * @param   pcbRead             Pointer where to store how many bytes were read. Optional.
 */
int SharedClipboardURIObject::Read(void *pvBuf, size_t cbBuf, uint32_t *pcbRead)
{
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    /* pcbRead is optional. */

    AssertMsgReturn(m_enmView == View_Source, ("Cannot write to an object which is not in target view\n"),
                    VERR_INVALID_STATE);

    size_t cbRead = 0;

    int rc;
    switch (m_enmType)
    {
        case Type_File:
        {
            rc = RTFileRead(u.File.hFile, pvBuf, cbBuf, &cbRead);
            if (RT_SUCCESS(rc))
            {
                u.File.cbProcessed += cbRead;
                Assert(u.File.cbProcessed <= u.File.cbToProcess);

                /* End of file reached or error occurred? */
                if (   u.File.cbToProcess
                    && u.File.cbProcessed == u.File.cbToProcess)
                {
                    rc = VINF_EOF;
                }
            }
            break;
        }

        case Type_Directory:
        {
            rc = VINF_SUCCESS;
            break;
        }

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        if (pcbRead)
            *pcbRead = (uint32_t)cbRead;
    }

    LogFlowFunc(("Returning strSourcePath=%s, cbRead=%zu, rc=%Rrc\n", m_strSrcPathAbs.c_str(), cbRead, rc));
    return rc;
}

/**
 * Resets the object's state and closes all related handles.
 */
void SharedClipboardURIObject::Reset(void)
{
    LogFlowThisFuncEnter();

    Close();

    m_enmType       = Type_Unknown;
    m_enmView       = View_Unknown;
    m_strSrcPathAbs = "";
    m_strTgtPathAbs = "";

    RT_ZERO(u);
}

/**
 * Sets the bytes to process by the object.
 *
 * Note: Only applies if the object is of type SharedClipboardURIObject::Type_File.
 *
 * @return  IPRT return code.
 * @param   cbSize          Size (in bytes) to process.
 */
int SharedClipboardURIObject::SetSize(uint64_t cbSize)
{
    AssertReturn(m_enmType == Type_File, VERR_INVALID_PARAMETER);

    /** @todo Implement sparse file support here. */

    u.File.cbToProcess = cbSize;
    return VINF_SUCCESS;
}

/**
 * Writes data to an object. Only applies to file objects.
 *
 * @return  IPRT status code.
 * @param   pvBuf               Buffer of data to write.
 * @param   cbBuf               Size (in bytes) of data to write.
 * @param   pcbWritten          Pointer where to store how many bytes were written. Optional.
 */
int SharedClipboardURIObject::Write(const void *pvBuf, size_t cbBuf, uint32_t *pcbWritten)
{
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    AssertMsgReturn(m_enmView == View_Target, ("Cannot write to an object which is not in target view\n"),
                    VERR_INVALID_STATE);

    size_t cbWritten = 0;

    int rc;
    switch (m_enmType)
    {
        case Type_File:
        {
            rc = RTFileWrite(u.File.hFile, pvBuf, cbBuf, &cbWritten);
            if (RT_SUCCESS(rc))
                u.File.cbProcessed += cbWritten;
            break;
        }

        case Type_Directory:
        {
            rc = VINF_SUCCESS;
            break;
        }

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        if (pcbWritten)
            *pcbWritten = (uint32_t)cbWritten;
    }

    LogFlowThisFunc(("Returning strSourcePathAbs=%s, cbWritten=%zu, rc=%Rrc\n", m_strSrcPathAbs.c_str(), cbWritten, rc));
    return rc;
}

