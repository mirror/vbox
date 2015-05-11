/* $Id$ */
/** @file
 * DnD: URI object class. For handling creation/reading/writing to files and directories
 *      on host or guest side.
 */

/*
 * Copyright (C) 2014-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/******************************************************************************
 *   Header Files                                                             *
 ******************************************************************************/

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
    : m_Type(Unknown)
    , m_fMode(0)
    , m_cbSize(0)
    , m_cbProcessed(0)
{
    RT_ZERO(u);
}

DnDURIObject::DnDURIObject(Type type,
                           const RTCString &strSrcPath /* = 0 */,
                           const RTCString &strDstPath /* = 0 */,
                           uint32_t fMode  /* = 0 */, uint64_t cbSize  /* = 0 */)
    : m_Type(type)
    , m_strSrcPath(strSrcPath)
    , m_strTgtPath(strDstPath)
    , m_fMode(fMode)
    , m_cbSize(cbSize)
    , m_cbProcessed(0)
{
    switch (m_Type)
    {
        case File:
            u.m_hFile = NULL;
            break;

        case Directory:
            break;

        default:
            break;
    }
}

DnDURIObject::~DnDURIObject(void)
{
    closeInternal();
}

void DnDURIObject::closeInternal(void)
{
    if (m_Type == File)
    {
        if (u.m_hFile)
        {
            RTFileClose(u.m_hFile);
            u.m_hFile = NULL;
        }
    }
}

void DnDURIObject::Close(void)
{
    switch (m_Type)
    {
        case File:
        {
            if (u.m_hFile != NULL)
            {
                int rc2 = RTFileClose(u.m_hFile);
                AssertRC(rc2);

                u.m_hFile = NULL;
            }
            break;
        }

        case Directory:
            break;

        default:
            break;
    }
}

bool DnDURIObject::IsComplete(void) const
{
    bool fComplete;

    switch (m_Type)
    {
        case File:
            Assert(m_cbProcessed <= m_cbSize);
            fComplete = m_cbProcessed == m_cbSize;
            break;

        case Directory:
            fComplete = true;
            break;

        default:
            fComplete = true;
            break;
    }

    return fComplete;
}

bool DnDURIObject::IsOpen(void) const
{
    bool fIsOpen;

    switch (m_Type)
    {
        case File:
            fIsOpen = u.m_hFile != NULL;
            break;

        case Directory:
            fIsOpen = true;
            break;

        default:
            fIsOpen = false;
            break;
    }

    return fIsOpen;
}

int DnDURIObject::Open(Dest enmDest, uint64_t fOpen /* = 0 */, uint32_t fMode /* = 0 */)
{
    return OpenEx(  enmDest == Source
                  ? m_strSrcPath : m_strTgtPath
                  , m_Type, enmDest, fOpen, fMode, 0 /* fFlags */);
}

int DnDURIObject::OpenEx(const RTCString &strPath, Type enmType, Dest enmDest,
                         uint64_t fOpen /* = 0 */, uint32_t fMode /* = 0 */, uint32_t fFlags /* = 0 */)
{
    int rc = VINF_SUCCESS;

    switch (enmDest)
    {
        case Source:
            m_strSrcPath = strPath;
            break;

        case Target:
            m_strTgtPath = strPath;
            break;

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (   RT_SUCCESS(rc)
        && fOpen) /* Opening mode specified? */
    {
        switch (enmType)
        {
            case File:
            {
                if (!u.m_hFile)
                {
                    /* Open files on the source with RTFILE_O_DENY_WRITE to prevent races
                     * where the OS writes to the file while the destination side transfers
                     * it over. */
                    rc = RTFileOpen(&u.m_hFile, strPath.c_str(), fOpen);
                    LogFlowFunc(("strPath=%s, enmType=%RU32, enmDest=%RU32, rc=%Rrc\n", strPath.c_str(), enmType, enmDest, rc));
                    if (RT_SUCCESS(rc))
                        rc = RTFileGetSize(u.m_hFile, &m_cbSize);
                    if (RT_SUCCESS(rc)
                        && fMode)
                    {
                        rc = RTFileSetMode(u.m_hFile, fMode);
                    }
                    if (RT_SUCCESS(rc))
                    {
                        LogFlowFunc(("cbSize=%RU64, fMode=%RU32\n", m_cbSize, m_fMode));
                        m_cbProcessed = 0;
                    }
                }
                else
                    rc = VINF_SUCCESS;

                break;
            }

            case Directory:
                rc = VINF_SUCCESS;
                break;

            default:
                rc = VERR_NOT_IMPLEMENTED;
                break;
        }
    }

    if (RT_SUCCESS(rc))
        m_Type = enmType;

    return rc;
}

/* static */
/** @todo Put this into an own class like DnDURIPath : public RTCString? */
int DnDURIObject::RebaseURIPath(RTCString &strPath,
                                const RTCString &strBaseOld /* = "" */,
                                const RTCString &strBaseNew /* = "" */)
{
    int rc;
    const char *pszPath = RTUriPath(strPath.c_str());
    if (!pszPath)
        pszPath = strPath.c_str();
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
                    LogFlowFunc(("Rebasing \"%s\" to \"%s\"\n", strPath.c_str(), pszPathURI));

                    strPath = RTCString(pszPathURI) + "\r\n";
                    RTStrFree(pszPathURI);

                    rc = VINF_SUCCESS;
                }
                else
                    rc = VERR_INVALID_PARAMETER;

                RTStrFree(pszPathNew);
            }
            else
                rc = VERR_NO_MEMORY;
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    return rc;
}

int DnDURIObject::Read(void *pvBuf, size_t cbBuf, uint32_t *pcbRead)
{
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    /* pcbRead is optional. */

    size_t cbRead = 0;

    int rc;
    switch (m_Type)
    {
        case File:
        {
            rc = OpenEx(m_strSrcPath, File, Source,
                        /* Use some sensible defaults. */
                        RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE, 0 /* fFlags */);
            if (RT_SUCCESS(rc))
            {
                rc = RTFileRead(u.m_hFile, pvBuf, cbBuf, &cbRead);
                if (RT_SUCCESS(rc))
                {
                    m_cbProcessed += cbRead;
                    Assert(m_cbProcessed <= m_cbSize);

                    /* End of file reached or error occurred? */
                    if (   m_cbSize
                        && m_cbProcessed == m_cbSize)
                    {
                        rc = VINF_EOF;
                    }
                }
            }

            break;
        }

        case Directory:
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

    LogFlowFunc(("Returning strSourcePath=%s, cbRead=%zu, rc=%Rrc\n", m_strSrcPath.c_str(), cbRead, rc));
    return rc;
}

void DnDURIObject::Reset(void)
{
    Close();

    m_Type          = Unknown;
    m_strSrcPath    = "";
    m_strTgtPath    = "";
    m_fMode = 0;
    m_cbSize        = 0;
    m_cbProcessed   = 0;
}

int DnDURIObject::Write(const void *pvBuf, size_t cbBuf, uint32_t *pcbWritten)
{
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    size_t cbWritten = 0;

    int rc;
    switch (m_Type)
    {
        case File:
        {
            rc = OpenEx(m_strTgtPath, File, Target,
                        /* Use some sensible defaults. */
                        RTFILE_O_OPEN_CREATE | RTFILE_O_DENY_WRITE | RTFILE_O_WRITE, 0 /* fFlags */);
            if (RT_SUCCESS(rc))
            {
                rc = RTFileWrite(u.m_hFile, pvBuf, cbBuf, &cbWritten);
                if (RT_SUCCESS(rc))
                    m_cbProcessed += cbWritten;
            }

            break;
        }

        case Directory:
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

    LogFlowFunc(("Returning strSourcePath=%s, cbWritten=%zu, rc=%Rrc\n", m_strSrcPath.c_str(), cbWritten, rc));
    return rc;
}

