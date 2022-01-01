/* $Id$ */
/** @file
 * VirtualBox COM NVRAM store class implementation
 */

/*
 * Copyright (C) 2021-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_NvramStoreImpl_h
#define MAIN_INCLUDED_NvramStoreImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "NvramStoreWrap.h"
#include <VBox/vmm/pdmdrv.h>

#ifdef VBOX_COM_INPROC
class Console;
#else
class GuestOSType;

namespace settings
{
    struct NvramSettings;
}
#endif

class ATL_NO_VTABLE NvramStore :
    public NvramStoreWrap
{
public:

    DECLARE_COMMON_CLASS_METHODS(NvramStore)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
#ifdef VBOX_COM_INPROC
    HRESULT init(Console *aParent, const com::Utf8Str &strNonVolatileStorageFile);
#else
    HRESULT init(Machine *parent);
    HRESULT init(Machine *parent, NvramStore *that);
    HRESULT initCopy(Machine *parent, NvramStore *that);
#endif
    void uninit();

    // public methods for internal purposes only
#ifndef VBOX_COM_INPROC
    HRESULT i_loadSettings(const settings::NvramSettings &data);
    HRESULT i_saveSettings(settings::NvramSettings &data);
#endif

#ifdef VBOX_COM_INPROC
    static const PDMDRVREG  DrvReg;
#else
    void i_rollback();
    void i_commit();
    void i_copyFrom(NvramStore *aThat);
    HRESULT i_applyDefaults(GuestOSType *aOSType);
#endif

    com::Utf8Str i_getNonVolatileStorageFile();
    void i_updateNonVolatileStorageFile(const com::Utf8Str &aNonVolatileStorageFile);

    int i_loadStore(const char *pszPath);
    int i_saveStore(void);

#ifndef VBOX_COM_INPROC
    HRESULT i_retainUefiVarStore(PRTVFS phVfs, bool fReadonly);
    HRESULT i_releaseUefiVarStore(RTVFS hVfs);
#endif

private:

    // Wrapped NVRAM store properties
    HRESULT getNonVolatileStorageFile(com::Utf8Str &aNonVolatileStorageFile);
    HRESULT getUefiVariableStore(ComPtr<IUefiVariableStore> &aUefiVarStore);

    // Wrapped NVRAM store members
    HRESULT initUefiVariableStore(ULONG aSize);

    int i_loadStoreFromTar(RTVFSFSSTREAM hVfsFssTar);
    int i_saveStoreAsTar(const char *pszPath);

#ifdef VBOX_COM_INPROC
    static DECLCALLBACK(int)    i_nvramStoreQuerySize(PPDMIVFSCONNECTOR pInterface, const char *pszNamespace, const char *pszPath,
                                                      uint64_t *pcb);
    static DECLCALLBACK(int)    i_nvramStoreReadAll(PPDMIVFSCONNECTOR pInterface, const char *pszNamespace, const char *pszPath,
                                                    void *pvBuf, size_t cbRead);
    static DECLCALLBACK(int)    i_nvramStoreWriteAll(PPDMIVFSCONNECTOR pInterface, const char *pszNamespace, const char *pszPath,
                                                     const void *pvBuf, size_t cbWrite);
    static DECLCALLBACK(int)    i_nvramStoreDelete(PPDMIVFSCONNECTOR pInterface, const char *pszNamespace, const char *pszPath);
    static DECLCALLBACK(void *) i_drvQueryInterface(PPDMIBASE pInterface, const char *pszIID);
    static DECLCALLBACK(int)    i_drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags);
    static DECLCALLBACK(void)   i_drvDestruct(PPDMDRVINS pDrvIns);
#endif

    struct Data;            // opaque data struct, defined in NvramStoreImpl.cpp
    Data *m;
};

#endif /* !MAIN_INCLUDED_NvramStoreImpl_h */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
