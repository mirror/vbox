/* $Id$ */
/** @file
 * VirtualBox COM NVRAM store class implementation
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_MAIN_UEFIVARIABLESTORE
#include "LoggingNew.h"

#include "UefiVariableStoreImpl.h"
#include "NvramStoreImpl.h"
#include "MachineImpl.h"

#include "AutoStateDep.h"
#include "AutoCaller.h"

#include "TrustAnchorsAndCerts.h"

#include <VBox/com/array.h>

#include <iprt/cpp/utils.h>
#include <iprt/efi.h>
#include <iprt/file.h>
#include <iprt/vfs.h>

#include <iprt/formats/efi-varstore.h>
#include <iprt/formats/efi-signature.h>

// defines
////////////////////////////////////////////////////////////////////////////////

// globals
////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// UefiVariableStore::Data structure
/////////////////////////////////////////////////////////////////////////////

struct UefiVariableStore::Data
{
    Data()
        : pParent(NULL),
          pMachine(NULL),
          hVfsUefiVarStore(NIL_RTVFS)
    { }

    /** The NVRAM store owning this UEFI variable store intstance. */
    NvramStore * const      pParent;
    /** The machine this UEFI variable store belongs to. */
    Machine    * const      pMachine;
    /** VFS handle to the UEFI variable store. */
    RTVFS                   hVfsUefiVarStore;
};

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(UefiVariableStore)

HRESULT UefiVariableStore::FinalConstruct()
{
    return BaseFinalConstruct();
}

void UefiVariableStore::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the UEFI variable store object.
 *
 * @returns COM result indicator.
 * @param   aParent                     The NVRAM store owning the UEFI NVRAM content.
 * @param   hVfsUefiVarStore            The UEFI variable store VFS handle.
 */
HRESULT UefiVariableStore::init(NvramStore *aParent, Machine *pMachine, RTVFS hVfsUefiVarStore)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aParent: %p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    /* share the parent weakly */
    unconst(m->pParent) = aParent;
    unconst(m->pMachine) = pMachine;
    m->hVfsUefiVarStore = hVfsUefiVarStore;

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}


/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void UefiVariableStore::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    RTVfsRelease(m->hVfsUefiVarStore);

    unconst(m->pParent) = NULL;
    unconst(m->pMachine) = NULL;

    delete m;
    m = NULL;

    LogFlowThisFuncLeave();
}


HRESULT UefiVariableStore::addVariable(const com::Utf8Str &aName, const com::Guid &aOwnerUuid, const std::vector<BYTE> &aData)
{
    RT_NOREF(aName, aOwnerUuid, aData);
    return E_NOTIMPL;
}


HRESULT UefiVariableStore::deleteVariable(const com::Utf8Str &aName, const com::Guid &aOwnerUuid)
{
    RT_NOREF(aName, aOwnerUuid);
    return E_NOTIMPL;
}


HRESULT UefiVariableStore::changeVariable(const com::Utf8Str &aName, const com::Guid &aOwnerUuid, const std::vector<BYTE> &aData)
{
    RT_NOREF(aName, aOwnerUuid, aData);
    return E_NOTIMPL;
}


HRESULT UefiVariableStore::queryVariableByName(const com::Utf8Str &aName, com::Guid &aOwnerUuid, std::vector<BYTE> &aData)
{
    RT_NOREF(aName, aOwnerUuid, aData);
    return E_NOTIMPL;
}


HRESULT UefiVariableStore::queryVariables(std::vector<com::Utf8Str> &aNames, std::vector<com::Guid> &aOwnerUuids)
{
    RT_NOREF(aNames, aOwnerUuids);
    return E_NOTIMPL;
}


HRESULT UefiVariableStore::enrollPlatformKey(const std::vector<BYTE> &aData, const com::Guid &aOwnerUuid)
{
    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock wlock(this COMMA_LOCKVAL_SRC_POS);

    EFI_GUID GuidGlobalVar = EFI_GLOBAL_VARIABLE_GUID;
    return i_uefiVarStoreAddSignatureToDbVec(&GuidGlobalVar, "PK", aData, aOwnerUuid, SignatureType_X509);
}


HRESULT UefiVariableStore::addKek(const std::vector<BYTE> &aData, const com::Guid &aOwnerUuid, SignatureType_T enmSignatureType)
{
    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock wlock(this COMMA_LOCKVAL_SRC_POS);

    EFI_GUID GuidGlobalVar = EFI_GLOBAL_VARIABLE_GUID;
    return i_uefiVarStoreAddSignatureToDbVec(&GuidGlobalVar, "KEK", aData, aOwnerUuid, enmSignatureType);
}


HRESULT UefiVariableStore::addSignatureToDb(const std::vector<BYTE> &aData, const com::Guid &aOwnerUuid, SignatureType_T enmSignatureType)
{
    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock wlock(this COMMA_LOCKVAL_SRC_POS);

    EFI_GUID GuidSecurityDb = EFI_GLOBAL_VARIABLE_GUID;
    return i_uefiVarStoreAddSignatureToDbVec(&GuidSecurityDb, "db", aData, aOwnerUuid, enmSignatureType);
}


HRESULT UefiVariableStore::addSignatureToDbx(const std::vector<BYTE> &aData, const com::Guid &aOwnerUuid, SignatureType_T enmSignatureType)
{
    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock wlock(this COMMA_LOCKVAL_SRC_POS);

    EFI_GUID GuidSecurityDb = EFI_IMAGE_SECURITY_DATABASE_GUID;
    return i_uefiVarStoreAddSignatureToDbVec(&GuidSecurityDb, "dbx", aData, aOwnerUuid, enmSignatureType);
}


HRESULT UefiVariableStore::enrollDefaultMsSignatures(void)
{
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock wlock(this COMMA_LOCKVAL_SRC_POS);

    EFI_GUID EfiGuidSecurityDb = EFI_IMAGE_SECURITY_DATABASE_GUID;
    EFI_GUID EfiGuidGlobalVar  = EFI_GLOBAL_VARIABLE_GUID;

    /** @todo This conversion from EFI GUID -> IPRT UUID -> Com GUID is nuts... */
    EFI_GUID EfiGuidMs      = EFI_SIGNATURE_OWNER_GUID_MICROSOFT;
    RTUUID   UuidMs;
    RTEfiGuidToUuid(&UuidMs, &EfiGuidMs);

    const com::Guid GuidMs(UuidMs);

    HRESULT hrc = i_uefiVarStoreAddSignatureToDb(&EfiGuidGlobalVar, "KEK", g_abUefiMicrosoftKek, g_cbUefiMicrosoftKek,
                                                 GuidMs, SignatureType_X509);
    if (SUCCEEDED(hrc))
    {
        hrc = i_uefiVarStoreAddSignatureToDb(&EfiGuidSecurityDb, "db", g_abUefiMicrosoftCa, g_cbUefiMicrosoftCa,
                                             GuidMs, SignatureType_X509);
        if (SUCCEEDED(hrc))
            hrc = i_uefiVarStoreAddSignatureToDb(&EfiGuidSecurityDb, "db", g_abUefiMicrosoftProPca, g_cbUefiMicrosoftProPca,
                                                 GuidMs, SignatureType_X509);
    }

    return hrc;
}


/**
 * Sets the given attributes for the given EFI variable store variable.
 *
 * @returns IPRT status code.
 * @param   hVfsVarStore        Handle of the EFI variable store VFS.
 * @param   pszVar              The variable to set the attributes for.
 * @param   fAttr               The attributes to set, see EFI_VAR_HEADER_ATTR_XXX.
 */
int UefiVariableStore::i_uefiVarStoreSetVarAttr(const char *pszVar, uint32_t fAttr)
{
    char szVarPath[_1K];
    ssize_t cch = RTStrPrintf2(szVarPath, sizeof(szVarPath), "/raw/%s/attr", pszVar);
    Assert(cch > 0); RT_NOREF(cch);

    RTVFSFILE hVfsFileAttr = NIL_RTVFSFILE;
    int vrc = RTVfsFileOpen(m->hVfsUefiVarStore, szVarPath,
                           RTFILE_O_READWRITE | RTFILE_O_DENY_NONE | RTFILE_O_OPEN,
                           &hVfsFileAttr);
    if (RT_SUCCESS(vrc))
    {
        uint32_t fAttrLe = RT_H2LE_U32(fAttr);
        vrc = RTVfsFileWrite(hVfsFileAttr, &fAttrLe, sizeof(fAttrLe), NULL /*pcbWritten*/);
        RTVfsFileRelease(hVfsFileAttr);
    }

    return vrc;
}


/**
 * Adds the given variable to the variable store.
 *
 * @returns IPRT status code.
 * @param   hVfsVarStore        Handle of the EFI variable store VFS.
 * @param   pGuid               The EFI GUID of the variable.
 * @param   pszVar              The variable name.
 * @param   fAttr               Attributes for the variable.
 * @param   phVfsFile           Where to return the VFS file handle to the created variable on success.
 */
HRESULT UefiVariableStore::i_uefiVarStoreAddVar(PCEFI_GUID pGuid, const char *pszVar, uint32_t fAttr, PRTVFSFILE phVfsFile)
{
    RTUUID UuidVar;
    RTEfiGuidToUuid(&UuidVar, pGuid);

    char szVarPath[_1K];
    ssize_t cch = RTStrPrintf2(szVarPath, sizeof(szVarPath), "/by-uuid/%RTuuid/%s", &UuidVar, pszVar);
    Assert(cch > 0); RT_NOREF(cch);

    HRESULT hrc = S_OK;
    int vrc = RTVfsFileOpen(m->hVfsUefiVarStore, szVarPath,
                            RTFILE_O_READWRITE | RTFILE_O_DENY_NONE | RTFILE_O_OPEN,
                            phVfsFile);
    if (   vrc == VERR_PATH_NOT_FOUND
        || vrc == VERR_FILE_NOT_FOUND)
    {
        /*
         * Try to create the owner GUID of the variable by creating the appropriate directory,
         * ignore error if it exists already.
         */
        RTVFSDIR hVfsDirRoot = NIL_RTVFSDIR;
        vrc = RTVfsOpenRoot(m->hVfsUefiVarStore, &hVfsDirRoot);
        if (RT_SUCCESS(vrc))
        {
            char szGuidPath[_1K];
            cch = RTStrPrintf2(szGuidPath, sizeof(szGuidPath), "by-uuid/%RTuuid", &UuidVar);
            Assert(cch > 0);

            RTVFSDIR hVfsDirGuid = NIL_RTVFSDIR;
            vrc = RTVfsDirCreateDir(hVfsDirRoot, szGuidPath, 0755, 0 /*fFlags*/, &hVfsDirGuid);
            if (RT_SUCCESS(vrc))
                RTVfsDirRelease(hVfsDirGuid);
            else if (vrc == VERR_ALREADY_EXISTS)
                vrc = VINF_SUCCESS;

            RTVfsDirRelease(hVfsDirRoot);
        }
        else
            hrc = setError(E_FAIL, tr("Opening variable storage root directory failed: %Rrc"), vrc);

        if (RT_SUCCESS(vrc))
        {
            vrc = RTVfsFileOpen(m->hVfsUefiVarStore, szVarPath,
                               RTFILE_O_READWRITE | RTFILE_O_DENY_NONE | RTFILE_O_CREATE,
                               phVfsFile);
            if (RT_SUCCESS(vrc))
                vrc = i_uefiVarStoreSetVarAttr(pszVar, fAttr);
        }

        if (RT_FAILURE(vrc))
            hrc = setError(E_FAIL, tr("Creating the variable '%s' failed: %Rrc"), pszVar, vrc);
    }

    return hrc;
}


HRESULT UefiVariableStore::i_uefiSigDbAddSig(RTEFISIGDB hEfiSigDb, const void *pvData, size_t cbData,
                                             const com::Guid &aOwnerUuid, SignatureType_T enmSignatureType)
{
    RTEFISIGTYPE enmSigType = RTEFISIGTYPE_INVALID;

    switch (enmSignatureType)
    {
        case SignatureType_X509:
            enmSigType = RTEFISIGTYPE_X509;
            break;
        case SignatureType_Sha256:
            enmSigType = RTEFISIGTYPE_SHA256;
            break;
        default:
            return setError(E_FAIL, tr("The given signature type is not supported"));
    }

    int vrc = RTEfiSigDbAddSignatureFromBuf(hEfiSigDb, enmSigType, aOwnerUuid.raw(), pvData, cbData);
    if (RT_SUCCESS(vrc))
        return S_OK;

    return setError(E_FAIL, tr("Failed to add signature to the database (%Rrc)"), vrc);
}


HRESULT UefiVariableStore::i_uefiVarStoreAddSignatureToDb(PCEFI_GUID pGuid, const char *pszDb, const void *pvData, size_t cbData,
                                                          const com::Guid &aOwnerUuid, SignatureType_T enmSignatureType)
{
    RTVFSFILE hVfsFileSigDb = NIL_RTVFSFILE;

    HRESULT hrc = i_uefiVarStoreAddVar(pGuid, pszDb,
                                     EFI_VAR_HEADER_ATTR_NON_VOLATILE
                                   | EFI_VAR_HEADER_ATTR_BOOTSERVICE_ACCESS
                                   | EFI_VAR_HEADER_ATTR_RUNTIME_ACCESS
                                   | EFI_AUTH_VAR_HEADER_ATTR_TIME_BASED_AUTH_WRITE_ACCESS,
                                   &hVfsFileSigDb);
    if (SUCCEEDED(hrc))
    {
        RTEFISIGDB hEfiSigDb;

        int vrc = RTEfiSigDbCreate(&hEfiSigDb);
        if (RT_SUCCESS(vrc))
        {
            vrc = RTEfiSigDbAddFromExistingDb(hEfiSigDb, hVfsFileSigDb);
            if (RT_SUCCESS(vrc))
            {
                hrc = i_uefiSigDbAddSig(hEfiSigDb, pvData, cbData, aOwnerUuid, enmSignatureType);
                if (SUCCEEDED(hrc))
                {
                    vrc = RTVfsFileSeek(hVfsFileSigDb, 0 /*offSeek*/, RTFILE_SEEK_BEGIN, NULL /*poffActual*/);
                    AssertRC(vrc);

                    vrc = RTEfiSigDbWriteToFile(hEfiSigDb, hVfsFileSigDb);
                    if (RT_FAILURE(vrc))
                        hrc = setError(E_FAIL, tr("Writing updated signature database failed: %Rrc"), vrc);
                }
            }
            else
                hrc = setError(E_FAIL, tr("Loading signature database failed: %Rrc"), vrc);

            RTEfiSigDbDestroy(hEfiSigDb);
        }
        else
            hrc = setError(E_FAIL, tr("Creating signature database failed: %Rrc"), vrc);

        RTVfsFileRelease(hVfsFileSigDb);
    }

    return hrc;
}


HRESULT UefiVariableStore::i_uefiVarStoreAddSignatureToDbVec(PCEFI_GUID pGuid, const char *pszDb, const std::vector<BYTE> &aData,
                                                             const com::Guid &aOwnerUuid, SignatureType_T enmSignatureType)
{
    return i_uefiVarStoreAddSignatureToDb(pGuid, pszDb, &aData.front(), aData.size(), aOwnerUuid, enmSignatureType);
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
