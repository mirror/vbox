/* $Id$ */
/** @file
 * DnD: URI object class. For handling creation/reading/writing to files and directories
 *      on host or guest side.
 */

/*
 * Copyright (C) 2014-2018 Oracle Corporation
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

#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/fs.h>
#include <iprt/path.h>
#include <iprt/uri.h>

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>

#include <VBox/GuestHost/DragAndDrop.h>

DnDURIObject::DnDURIObject(void)
    : m_Type(Type_Unknown)
    , m_fIsOpen(false)
{
    RT_ZERO(u);
}

DnDURIObject::DnDURIObject(Type enmType,
                           const RTCString &strSrcPathAbs /* = 0 */,
                           const RTCString &strDstPathAbs /* = 0 */,
                           uint32_t fMode  /* = 0 */, uint64_t cbSize  /* = 0 */)
    : m_Type(enmType)
    , m_strSrcPathAbs(strSrcPathAbs)
    , m_strTgtPathAbs(strDstPathAbs)
    , m_fIsOpen(false)
{
    RT_ZERO(u);

    switch (m_Type)
    {
        case Type_File:
            u.File.fMode = fMode;
            u.File.cbSize = cbSize;
            break;

        default:
            break;
    }
}

DnDURIObject::~DnDURIObject(void)
{
    closeInternal();
}

/**
 * Closes the object's internal handles (to files / ...).
 *
 */
void DnDURIObject::closeInternal(void)
{
    LogFlowThisFuncEnter();

    if (!m_fIsOpen)
        return;

    switch (m_Type)
    {
        case Type_File:
        {
            RTFileClose(u.File.hFile);
            u.File.hFile = NIL_RTFILE;
            u.File.fMode = 0;
            break;
        }

        case Type_Directory:
            break;

        default:
            break;
    }

    m_fIsOpen = false;
}

/**
 * Closes the object.
 * This also closes the internal handles associated with the object (to files / ...).
 */
void DnDURIObject::Close(void)
{
    closeInternal();
}

/**
 * Returns whether the processing of the object is complete or not.
 * For file objects this means that all bytes have been processed.
 *
 * @return  True if complete, False if not.
 */
bool DnDURIObject::IsComplete(void) const
{
    bool fComplete;

    switch (m_Type)
    {
        case Type_File:
            Assert(u.File.cbProcessed <= u.File.cbSize);
            fComplete = u.File.cbProcessed == u.File.cbSize;
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
bool DnDURIObject::IsOpen(void) const
{
    return m_fIsOpen;
}

/**
 * (Re-)Opens the object with a specific view, open and file mode.
 *
 * @return  IPRT status code.
 * @param   enmView             View to use for opening the object.
 * @param   fOpen               File open flags to use.
 * @param   fMode
 *
 * @remark
 */
int DnDURIObject::Open(View enmView, uint64_t fOpen /* = 0 */, uint32_t fMode /* = 0 */)
{
    return OpenEx(  enmView == View_Source
                  ? m_strSrcPathAbs : m_strTgtPathAbs
                  , m_Type, enmView, fOpen, fMode, 0 /* fFlags */);
}

/**
 * Open the object with a specific file type, and, depending on the type, specifying additional parameters.
 *
 * @return  IPRT status code.
 * @param   strPathAbs          Absolute path of the object (file / directory / ...).
 * @param   enmType             Type of the object.
 * @param   enmView             View of the object.
 * @param   fOpen               Open mode to use; only valid for file objects.
 * @param   fMode               File mode to use; only valid for file objects.
 * @param   fFlags              Additional DnD URI object flags.
 */
int DnDURIObject::OpenEx(const RTCString &strPathAbs, Type enmType, View enmView,
                         uint64_t fOpen /* = 0 */, uint32_t fMode /* = 0 */, DNDURIOBJECTFLAGS fFlags /* = DNDURIOBJECT_FLAGS_NONE */)
{
    AssertReturn(!(fFlags & ~DNDURIOBJECT_FLAGS_VALID_MASK), VERR_INVALID_FLAGS);
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
        LogFlowThisFunc(("strPath=%s, enmType=%RU32, enmView=%RU32, fOpen=0x%x, fMode=0x%x, fFlags=0x%x\n",
                         strPathAbs.c_str(), enmType, enmView, fOpen, fMode, fFlags));
        switch (enmType)
        {
            case Type_File:
            {
                if (!m_fIsOpen)
                {
                    /*
                     * Open files on the source with RTFILE_O_DENY_WRITE to prevent races
                     * where the OS writes to the file while the destination side transfers
                     * it over.
                     */
                    LogFlowThisFunc(("Opening ...\n"));
                    rc = RTFileOpen(&u.File.hFile, strPathAbs.c_str(), fOpen);
                    if (RT_SUCCESS(rc))
                        rc = RTFileGetSize(u.File.hFile, &u.File.cbSize);

                    if (RT_SUCCESS(rc))
                    {
                        if (   (fOpen & RTFILE_O_WRITE) /* Only set the file mode on write. */
                            &&  fMode                   /* Some file mode to set specified? */)
                        {
                            rc = RTFileSetMode(u.File.hFile, fMode);
                            if (RT_SUCCESS(rc))
                                u.File.fMode = fMode;
                        }
                        else if (fOpen & RTFILE_O_READ)
                        {
#if 0 /** @todo Enable this as soon as RTFileGetMode is implemented. */
                            rc = RTFileGetMode(u.m_hFile, &m_fMode);
#else
                            RTFSOBJINFO ObjInfo;
                            rc = RTFileQueryInfo(u.File.hFile, &ObjInfo, RTFSOBJATTRADD_NOTHING);
                            if (RT_SUCCESS(rc))
                                u.File.fMode = ObjInfo.Attr.fMode;
#endif
                        }
                    }

                    if (RT_SUCCESS(rc))
                    {
                        LogFlowThisFunc(("cbSize=%RU64, fMode=0x%x\n", u.File.cbSize, u.File.fMode));
                        u.File.cbProcessed = 0;
                    }
                }
                else
                    rc = VINF_SUCCESS;

                break;
            }

            case Type_Directory:
                rc = VINF_SUCCESS;
                break;

            default:
                rc = VERR_NOT_IMPLEMENTED;
                break;
        }
    }

    if (RT_SUCCESS(rc))
    {
        m_Type  = enmType;
        m_fIsOpen = true;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
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
 ** @todo Put this into an own class like DnDURIPath : public RTCString?
 */
/* static */
int DnDURIObject::RebaseURIPath(RTCString &strPathAbs,
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
int DnDURIObject::Read(void *pvBuf, size_t cbBuf, uint32_t *pcbRead)
{
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    /* pcbRead is optional. */

    size_t cbRead = 0;

    int rc;
    switch (m_Type)
    {
        case Type_File:
        {
            rc = OpenEx(m_strSrcPathAbs, Type_File, View_Source,
                        /* Use some sensible defaults. */
                        RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE, 0 /* fFlags */);
            if (RT_SUCCESS(rc))
            {
                rc = RTFileRead(u.File.hFile, pvBuf, cbBuf, &cbRead);
                if (RT_SUCCESS(rc))
                {
                    u.File.cbProcessed += cbRead;
                    Assert(u.File.cbProcessed <= u.File.cbSize);

                    /* End of file reached or error occurred? */
                    if (   u.File.cbSize
                        && u.File.cbProcessed == u.File.cbSize)
                    {
                        rc = VINF_EOF;
                    }
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
void DnDURIObject::Reset(void)
{
    LogFlowThisFuncEnter();

    Close();

    m_Type          = Type_Unknown;
    m_strSrcPathAbs = "";
    m_strTgtPathAbs = "";

    RT_ZERO(u);
}

/**
 * Writes data to an object. Only applies to file objects.
 *
 * @return  IPRT status code.
 * @param   pvBuf               Buffer of data to write.
 * @param   cbBuf               Size (in bytes) of data to write.
 * @param   pcbWritten          Pointer where to store how many bytes were written. Optional.
 */
int DnDURIObject::Write(const void *pvBuf, size_t cbBuf, uint32_t *pcbWritten)
{
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    size_t cbWritten = 0;

    int rc;
    switch (m_Type)
    {
        case Type_File:
        {
            rc = OpenEx(m_strTgtPathAbs, Type_File, View_Target,
                        /* Use some sensible defaults. */
                        RTFILE_O_OPEN_CREATE | RTFILE_O_DENY_WRITE | RTFILE_O_WRITE, 0 /* fFlags */);
            if (RT_SUCCESS(rc))
            {
                rc = RTFileWrite(u.File.hFile, pvBuf, cbBuf, &cbWritten);
                if (RT_SUCCESS(rc))
                    u.File.cbProcessed += cbWritten;
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
        if (pcbWritten)
            *pcbWritten = (uint32_t)cbWritten;
    }

    LogFlowThisFunc(("Returning strSourcePathAbs=%s, cbWritten=%zu, rc=%Rrc\n", m_strSrcPathAbs.c_str(), cbWritten, rc));
    return rc;
}

