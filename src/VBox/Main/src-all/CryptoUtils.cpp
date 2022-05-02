/* $Id$ */
/** @file
 * Main - Cryptographic utility functions used by both VBoxSVC and VBoxC.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/vfs.h>

#include "CryptoUtils.h"


/*static*/
DECLCALLBACK(int) SsmStream::i_ssmCryptoWrite(void *pvUser, uint64_t offStream, const void *pvBuf, size_t cbToWrite)
{
    SsmStream *pThis = static_cast<SsmStream *>(pvUser);

    return RTVfsFileWriteAt(pThis->m_hVfsFile, (RTFOFF)offStream, pvBuf, cbToWrite, NULL /*pcbWritten*/);
}


/*static*/
DECLCALLBACK(int) SsmStream::i_ssmCryptoRead(void *pvUser, uint64_t offStream, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    SsmStream *pThis = static_cast<SsmStream *>(pvUser);

    return RTVfsFileReadAt(pThis->m_hVfsFile, (RTFOFF)offStream, pvBuf, cbToRead, pcbRead);
}


/*static*/
DECLCALLBACK(int) SsmStream::i_ssmCryptoSeek(void *pvUser, int64_t offSeek, unsigned uMethod, uint64_t *poffActual)
{
    SsmStream *pThis = static_cast<SsmStream *>(pvUser);

    return RTVfsFileSeek(pThis->m_hVfsFile, (RTFOFF)offSeek, uMethod, poffActual);
}


/*static*/
DECLCALLBACK(uint64_t) SsmStream::i_ssmCryptoTell(void *pvUser)
{
    SsmStream *pThis = static_cast<SsmStream *>(pvUser);

    return (uint64_t)RTVfsFileTell(pThis->m_hVfsFile);
}


/*static*/
DECLCALLBACK(int) SsmStream::i_ssmCryptoSize(void *pvUser, uint64_t *pcb)
{
    SsmStream *pThis = static_cast<SsmStream *>(pvUser);

    return RTVfsFileQuerySize(pThis->m_hVfsFile, pcb);
}


/*static*/
DECLCALLBACK(int) SsmStream::i_ssmCryptoIsOk(void *pvUser)
{
    RT_NOREF(pvUser);

    /** @todo */
    return VINF_SUCCESS;
}


/*static*/
DECLCALLBACK(int) SsmStream::i_ssmCryptoClose(void *pvUser, bool fCancelled)
{
    SsmStream *pThis = static_cast<SsmStream *>(pvUser);

    RT_NOREF(fCancelled); /** @todo */
    RTVfsFileRelease(pThis->m_hVfsFile);
    pThis->m_hVfsFile = NIL_RTVFSFILE;
    return VINF_SUCCESS;
}


#ifdef VBOX_COM_INPROC
SsmStream::SsmStream(Console *pParent, SecretKeyStore *pKeyStore, const Utf8Str &strKeyId, const Utf8Str &strKeyStore)
#else
SsmStream::SsmStream(VirtualBox *pParent, SecretKeyStore *pKeyStore, const Utf8Str &strKeyId, const Utf8Str &strKeyStore)
#endif
{
    m_StrmOps.u32Version    = SSMSTRMOPS_VERSION;
    m_StrmOps.pfnWrite      = SsmStream::i_ssmCryptoWrite;
    m_StrmOps.pfnRead       = SsmStream::i_ssmCryptoRead;
    m_StrmOps.pfnSeek       = SsmStream::i_ssmCryptoSeek;
    m_StrmOps.pfnTell       = SsmStream::i_ssmCryptoTell;
    m_StrmOps.pfnSize       = SsmStream::i_ssmCryptoSize;
    m_StrmOps.pfnIsOk       = SsmStream::i_ssmCryptoIsOk;
    m_StrmOps.pfnClose      = SsmStream::i_ssmCryptoClose;
    m_StrmOps.u32EndVersion = SSMSTRMOPS_VERSION;

    m_pKeyStore             = pKeyStore;
    m_strKeyId              = strKeyId;
    m_strKeyStore           = strKeyStore;
    m_pParent               = pParent;
    m_hVfsFile              = NIL_RTVFSFILE;
    m_pSsm                  = NULL;
    m_pCryptoIf             = NULL;
}


SsmStream::~SsmStream()
{
    close();

    if (m_pCryptoIf)
        m_pParent->i_releaseCryptoIf(m_pCryptoIf);

    m_pCryptoIf = NULL;
    m_pKeyStore = NULL;
}


int SsmStream::open(const Utf8Str &strFilename, bool fWrite, PSSMHANDLE *ppSsmHandle)
{
    /* Fast path, if the saved state is not encrypted we can skip everything and let SSM handle the file. */
    if (m_strKeyId.isEmpty())
    {
        AssertReturn(!fWrite, VERR_NOT_SUPPORTED);

        int rc = SSMR3Open(strFilename.c_str(), NULL /*pStreamOps*/, NULL /*pvStreamOps*/,
                           0 /*fFlags*/, &m_pSsm);
        if (RT_SUCCESS(rc))
            *ppSsmHandle = m_pSsm;

        return rc;
    }

    int rc = VINF_SUCCESS;
    if (!m_pCryptoIf)
    {
#ifdef VBOX_COM_INPROC
        rc = m_pParent->i_retainCryptoIf(&m_pCryptoIf);
        if (RT_FAILURE(rc))
            return rc;
#else
        HRESULT hrc = m_pParent->i_retainCryptoIf(&m_pCryptoIf);
        if (FAILED(hrc))
            return VERR_COM_IPRT_ERROR;
#endif
    }

    SecretKey *pKey;
    rc = m_pKeyStore->retainSecretKey(m_strKeyId, &pKey);
    if (RT_SUCCESS(rc))
    {
        RTVFSFILE hVfsFileSsm = NIL_RTVFSFILE;
        uint32_t fOpen =   fWrite
                         ? RTFILE_O_READWRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_WRITE
                         : RTFILE_O_READ      | RTFILE_O_OPEN           | RTFILE_O_DENY_WRITE;

        rc = RTVfsFileOpenNormal(strFilename.c_str(), fOpen, &hVfsFileSsm);
        if (RT_SUCCESS(rc))
        {
            const char *pszPassword = (const char *)pKey->getKeyBuffer();

            rc = m_pCryptoIf->pfnCryptoFileFromVfsFile(hVfsFileSsm, m_strKeyStore.c_str(), pszPassword, &m_hVfsFile);
            if (RT_SUCCESS(rc))
            {
                rc = SSMR3Open(NULL /*pszFilename*/, &m_StrmOps, this, 0 /*fFlags*/, &m_pSsm);
                if (RT_SUCCESS(rc))
                    *ppSsmHandle = m_pSsm;

                if (RT_FAILURE(rc))
                {
                    RTVfsFileRelease(m_hVfsFile);
                    m_hVfsFile = NIL_RTVFSFILE;
                }
            }

            /* Also release in success case because the encrypted file handle retained a new reference to it. */
            RTVfsFileRelease(hVfsFileSsm);
        }

        pKey->release();
    }

    return rc;
}


int SsmStream::close(void)
{
    if (m_pSsm)
    {
        int rc = SSMR3Close(m_pSsm);
        AssertRCReturn(rc, rc);
    }

    if (m_hVfsFile != NIL_RTVFSFILE)
        RTVfsFileRelease(m_hVfsFile);

    m_hVfsFile = NIL_RTVFSFILE;
    m_pSsm     = NULL;

    return VINF_SUCCESS;
}
