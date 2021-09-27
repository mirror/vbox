/* $Id$ */
/** @file
 * VirtualBox COM UEFI variable store class implementation
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

#ifndef MAIN_INCLUDED_UefiVariableStoreImpl_h
#define MAIN_INCLUDED_UefiVariableStoreImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "UefiVariableStoreWrap.h"
#include <iprt/types.h>

#include <iprt/formats/efi-common.h>

class NvramStore;
class Machine;

class ATL_NO_VTABLE UefiVariableStore :
    public UefiVariableStoreWrap
{
public:

    DECLARE_COMMON_CLASS_METHODS(UefiVariableStore)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(NvramStore *aParent, Machine *pMachine, RTVFS hVfsUefiVarStore);
    void uninit();

    // public methods for internal purposes only

private:

    // Wrapped NVRAM store properties

    // Wrapped NVRAM store members
    HRESULT addVariable(const com::Utf8Str &aName, const com::Guid &aOwnerUuid, const std::vector<BYTE> &aData);
    HRESULT deleteVariable(const com::Utf8Str &aName, const com::Guid &aOwnerUuid);
    HRESULT changeVariable(const com::Utf8Str &aName, const com::Guid &aOwnerUuid, const std::vector<BYTE> &aData);
    HRESULT queryVariableByName(const com::Utf8Str &aName, com::Guid &aOwnerUuid, std::vector<BYTE> &aData);
    HRESULT queryVariables(std::vector<com::Utf8Str> &aNames, std::vector<com::Guid> &aOwnerUuids);
    HRESULT enrollPlatformKey(const std::vector<BYTE> &aData, const com::Guid &aOwnerUuid);
    HRESULT addKek(const std::vector<BYTE> &aData, const com::Guid &aOwnerUuid, SignatureType_T enmSignatureType);
    HRESULT addSignatureToDb(const std::vector<BYTE> &aData, const com::Guid &aOwnerUuid, SignatureType_T enmSignatureType);
    HRESULT addSignatureToDbx(const std::vector<BYTE> &aData, const com::Guid &aOwnerUuid, SignatureType_T enmSignatureType);
    HRESULT enrollDefaultMsSignatures(void);

    int i_uefiVarStoreSetVarAttr(const char *pszVar, uint32_t fAttr);

    HRESULT i_uefiVarStoreAddVar(PCEFI_GUID pGuid, const char *pszVar, uint32_t fAttr, PRTVFSFILE phVfsFile);
    HRESULT i_uefiSigDbAddSig(RTEFISIGDB hEfiSigDb, const void *pvData, size_t cbData, const com::Guid &aOwnerUuid, SignatureType_T enmSignatureType);
    HRESULT i_uefiVarStoreAddSignatureToDbVec(PCEFI_GUID pGuid, const char *pszDb, const std::vector<BYTE> &aData,
                                              const com::Guid &aOwnerUuid, SignatureType_T enmSignatureType);
    HRESULT i_uefiVarStoreAddSignatureToDb(PCEFI_GUID pGuid, const char *pszDb, const void *pvData, size_t cbData,
                                           const com::Guid &aOwnerUuid, SignatureType_T enmSignatureType);

    struct Data;            // opaque data struct, defined in UefiVariableStoreImpl.cpp
    Data *m;
};

#endif /* !MAIN_INCLUDED_UefiVariableStoreImpl_h */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
