/* $Id$ */
/** @file
 *
 * VirtualBox COM class implementation: MediumIO
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#include "MediumIOImpl.h"
#include "MediumImpl.h"
#include "MediumLock.h"
#include "Global.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <iprt/fsvfs.h>
#include <iprt/dvm.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Private member data.
 */
struct MediumIO::Data
{
    Data(Medium * const a_pMedium = NULL, bool a_fWritable = false, uint32_t a_cbSector = 512)
        : ptrMedium(a_pMedium)
        , fWritable(a_fWritable)
        , cbSector(a_cbSector)
        , SecretKeyStore(false /*fKeyBufNonPageable*/)
        , pHdd(NULL)
        , hVfsFile(NIL_RTVFSFILE)
    {
    }

    /** Reference to the medium we're accessing. */
    ComPtr<Medium>                  ptrMedium;
    /** Set if writable, clear if readonly. */
    bool                            fWritable;
    /** The sector size. */
    uint32_t                        cbSector;
    /** Secret key store used to hold the passwords for encrypted medium. */
    SecretKeyStore                  SecretKeyStore;
    /** Crypto filter settings. */
    MediumCryptoFilterSettings      CryptoSettings;
    /** Medium lock list.  */
    MediumLockList                  LockList;
    /** The HDD instance. */
    PVDISK                          pHdd;
    /** VFS file for the HDD instance. */
    RTVFSFILE                       hVfsFile;
};


/*********************************************************************************************************************************
*   Boilerplate constructor & destructor                                                                                         *
*********************************************************************************************************************************/

DEFINE_EMPTY_CTOR_DTOR(MediumIO)

HRESULT MediumIO::FinalConstruct()
{
    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void MediumIO::FinalRelease()
{
    LogFlowThisFuncEnter();
    uninit();
    BaseFinalRelease();
    LogFlowThisFuncLeave();
}


/*********************************************************************************************************************************
*   Initializer & uninitializer                                                                                                  *
*********************************************************************************************************************************/

/**
 * Initializes the medium I/O object.
 *
 * @param   pMedium         Pointer to the medium to access.
 * @param   fWritable       Read-write (true) or readonly (false) access.
 * @param   rStrKeyId       The key ID for an encrypted medium.  Empty if not
 *                          encrypted.
 * @param   rStrPassword    The password for an encrypted medium.  Empty if not
 *                          encrypted.
 *
 */
HRESULT MediumIO::initForMedium(Medium *pMedium, bool fWritable, com::Utf8Str const &rStrKeyId, com::Utf8Str const &rStrPassword)
{
    LogFlowThisFunc(("pMedium=%p fWritable=%RTbool\n", pMedium, fWritable));
    CheckComArgExpr(rStrPassword, rStrPassword.isEmpty() == rStrKeyId.isEmpty()); /* checked by caller */

    /*
     * Enclose the state transition NotReady->InInit->Ready
     */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /*
     * Allocate data instance.
     */
    HRESULT hrc = S_OK;
    m = new(std::nothrow) Data(pMedium, fWritable);
    if (m)
    {
        /*
         * Add the password to the keystore if specified.
         */
        if (rStrKeyId.isNotEmpty())
        {
            int vrc = m->SecretKeyStore.addSecretKey(rStrKeyId, (const uint8_t *)rStrPassword.c_str(),
                                                     rStrPassword.length() + 1 /*including the Schwarzenegger character*/);
            if (vrc == VERR_NO_MEMORY)
                hrc = setError(E_OUTOFMEMORY, tr("Failed to allocate enough secure memory for the key/password"));
            else if (RT_FAILURE(vrc))
                hrc = setErrorBoth(E_FAIL, vrc, tr("Unknown error happened while adding a password (%Rrc)"), vrc);
        }

        /*
         * Try open the medium and then get a VFS file handle for it.
         */
        if (SUCCEEDED(hrc))
        {
            hrc = pMedium->i_openHddForIO(fWritable, &m->SecretKeyStore, &m->pHdd, &m->LockList, &m->CryptoSettings);
            if (SUCCEEDED(hrc))
            {
                int vrc = VDCreateVfsFileFromDisk(m->pHdd, 0 /*fFlags*/, &m->hVfsFile);
                if (RT_FAILURE(vrc))
                {
                    hrc = setErrorBoth(E_FAIL, vrc, tr("VDCreateVfsFileFromDisk failed: %Rrc"), vrc);
                    m->hVfsFile = NIL_RTVFSFILE;
                }
            }
        }
    }
    else
        hrc = E_OUTOFMEMORY;

    /*
     * Done. Just update object readiness state.
     */
    if (SUCCEEDED(hrc))
        autoInitSpan.setSucceeded();
    else
    {
        if (m)
            i_close(); /* Free password and whatever i_openHddForIO() may accidentally leave around on failure. */
        autoInitSpan.setFailed(hrc);
    }

    LogFlowThisFunc(("returns %Rhrc\n", hrc));
    return hrc;
}

/**
 * Uninitializes the instance (called from FinalRelease()).
 */
void MediumIO::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (!autoUninitSpan.uninitDone())
    {
        if (m)
        {
            i_close();

            delete m;
            m = NULL;
        }
    }

    LogFlowThisFuncLeave();
}


/*********************************************************************************************************************************
*   IMediumIO attributes                                                                                                         *
*********************************************************************************************************************************/

HRESULT MediumIO::getMedium(ComPtr<IMedium> &a_rPtrMedium)
{
    a_rPtrMedium = m->ptrMedium;
    return S_OK;
}

HRESULT MediumIO::getWritable(BOOL *a_fWritable)
{
    *a_fWritable = m->fWritable;
    return S_OK;
}

HRESULT MediumIO::getExplorer(ComPtr<IVFSExplorer> &a_rPtrExplorer)
{
    RT_NOREF_PV(a_rPtrExplorer);
    return E_NOTIMPL;
}


/*********************************************************************************************************************************
*   IMediumIO methods                                                                                                            *
*********************************************************************************************************************************/

HRESULT MediumIO::read(LONG64 a_off, ULONG a_cbRead, std::vector<BYTE> &a_rData)
{
    /*
     * Validate input.
     */
    if (a_cbRead > _256K)
        return setError(E_INVALIDARG, tr("Max read size is 256KB, given: %u"), a_cbRead);
    if (a_cbRead == 0)
        return setError(E_INVALIDARG, tr("Zero byte read is not supported."));

    /*
     * Allocate return buffer.
     */
    try
    {
        a_rData.resize(a_cbRead);
    }
    catch (std::bad_alloc)
    {
        return E_OUTOFMEMORY;
    }

    /*
     * Do the reading.  To play safe we exclusivly lock the object while doing this.
     */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    size_t cbActual = 0;
    int vrc = RTVfsFileReadAt(m->hVfsFile, a_off, &a_rData.front(), a_cbRead, &cbActual);
    alock.release();

    /*
     * Manage the result.
     */
    HRESULT hrc;
    if (RT_SUCCESS(vrc))
    {
        if (cbActual != a_cbRead)
        {
            Assert(cbActual < a_cbRead);
            a_rData.resize(cbActual);
        }
        hrc = S_OK;
    }
    else
    {
        a_rData.resize(0);
        hrc = setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Error reading %u bytes at %RU64: %Rrc"), a_cbRead, a_off, vrc);
    }

    return hrc;
}

HRESULT MediumIO::write(LONG64 a_off, const std::vector<BYTE> &a_rData, ULONG *a_pcbWritten)
{
    /*
     * Validate input.
     */
    size_t cbToWrite = a_rData.size();
    if (cbToWrite == 0)
        return setError(E_INVALIDARG, tr("Zero byte write is not supported."));
    if (!m->fWritable)
        return setError(E_ACCESSDENIED, tr("Medium not opened for writing."));
    CheckComArgPointerValid(a_pcbWritten);
    *a_pcbWritten = 0;

    /*
     * Do the writing.  To play safe we exclusivly lock the object while doing this.
     */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    size_t cbActual = 0;
    int vrc = RTVfsFileWriteAt(m->hVfsFile, a_off, &a_rData.front(), cbToWrite, &cbActual);
    alock.release();

    /*
     * Manage the result.
     */
    HRESULT hrc;
    if (RT_SUCCESS(vrc))
    {
        *a_pcbWritten = (ULONG)cbActual;
        hrc = S_OK;
    }
    else
        hrc = setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Error writing %zu bytes at %RU64: %Rrc"), cbToWrite, a_off, vrc);

    return hrc;
}

HRESULT MediumIO::formatFAT(BOOL a_fQuick)
{
    /*
     * Validate input.
     */
    if (!m->fWritable)
        return setError(E_ACCESSDENIED, tr("Medium not opened for writing."));

    /*
     * Format the medium as FAT and let the format API figure the parameters.
     * We exclusivly lock the object while doing this as concurrent medium access makes no sense.
     */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    RTERRINFOSTATIC ErrInfo;
    int vrc = RTFsFatVolFormat(m->hVfsFile, 0, 0, a_fQuick ? RTFSFATVOL_FMT_F_QUICK : RTFSFATVOL_FMT_F_FULL, m->cbSector, 0,
                               RTFSFATTYPE_INVALID, 0, 0, 0, 0, 0, RTErrInfoInitStatic(&ErrInfo));
    alock.release();

    /*
     * Manage the result.
     */
    HRESULT hrc;
    if (RT_SUCCESS(vrc))
        hrc = S_OK;
    else if (RTErrInfoIsSet(&ErrInfo.Core))
        hrc = setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Error formatting (%Rrc): %s"), vrc, ErrInfo.Core.pszMsg);
    else
        hrc = setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Error formatting: %Rrc"), vrc);

    return hrc;
}

HRESULT MediumIO::initializePartitionTable(PartitionTableType_T a_enmFormat, BOOL a_fWholeDiskInOneEntry)
{
    /*
     * Validate input.
     */
    const char *pszFormat;
    if (a_enmFormat == PartitionTableType_MBR)
        pszFormat = "MBR"; /* RTDVMFORMATTYPE_MBR */
    else if (a_enmFormat == PartitionTableType_GPT)
        pszFormat = "GPT"; /* RTDVMFORMATTYPE_GPT */
    else
        return setError(E_INVALIDARG, tr("Invalid partition format type: %d"), a_enmFormat);
    if (!m->fWritable)
        return setError(E_ACCESSDENIED, tr("Medium not opened for writing."));
    if (a_fWholeDiskInOneEntry)
        return setError(E_NOTIMPL, tr("whole-disk-in-one-entry is not implemented yet, sorry."));

    /*
     * Do the partitioning.
     * We exclusivly lock the object while doing this as concurrent medium access makes little sense.
     */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    RTDVM hVolMgr;
    int vrc = RTDvmCreate(&hVolMgr, m->hVfsFile, m->cbSector, 0 /*fFlags*/);
    HRESULT hrc;
    if (RT_SUCCESS(vrc))
    {
        vrc = RTDvmMapInitialize(hVolMgr, pszFormat); /** @todo Why doesn't RTDvmMapInitialize take RTDVMFORMATTYPE? */
        if (RT_SUCCESS(vrc))
        {
            /*
             * Create a partition for the whole disk?
             */
            hrc = S_OK; /** @todo a_fWholeDiskInOneEntry requies RTDvm to get a function for creating partitions. */
        }
        else
            hrc = setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("RTDvmMapInitialize failed: %Rrc"), vrc);
        RTDvmRelease(hVolMgr);
    }
    else
        hrc = setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("RTDvmCreate failed: %Rrc"), vrc);

    return hrc;
}

HRESULT MediumIO::close()
{
    /*
     * We need a write lock here to exclude all other access.
     */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    i_close();
    return S_OK;
}



/*********************************************************************************************************************************
*   IMediumIO internal methods                                                                                                   *
*********************************************************************************************************************************/

/**
 * This is used by both uninit and close().
 *
 * Expects exclusive access (write lock or autouninit) to the object.
 */
void MediumIO::i_close()
{
    if (m->hVfsFile != NIL_RTVFSFILE)
    {
        uint32_t cRefs = RTVfsFileRelease(m->hVfsFile);
        Assert(cRefs == 0);
        NOREF(cRefs);

        m->hVfsFile = NIL_RTVFSFILE;
    }

    if (m->pHdd)
    {
        VDDestroy(m->pHdd);
        m->pHdd = NULL;
    }

    m->LockList.Clear();
    m->ptrMedium.setNull();
    m->SecretKeyStore.deleteAllSecretKeys(false /* fSuspend */, true /* fForce */);
}

