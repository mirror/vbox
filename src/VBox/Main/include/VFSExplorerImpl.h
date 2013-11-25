/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2009-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_VFSEXPLORERIMPL
#define ____H_VFSEXPLORERIMPL

#include "VFSExplorerWrap.h"

class ATL_NO_VTABLE VFSExplorer :
    public VFSExplorerWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(VFSExplorer)

    // public initializer/uninitializer for internal purposes only
    HRESULT FinalConstruct() { return BaseFinalConstruct(); }
    void FinalRelease() { uninit(); BaseFinalRelease(); }

    HRESULT init(VFSType_T aType, Utf8Str aFilePath, Utf8Str aHostname, Utf8Str aUsername, Utf8Str aPassword, VirtualBox *aVirtualBox);
    void uninit();

    /* public methods only for internal purposes */
    static HRESULT setErrorStatic(HRESULT aResultCode,
                                  const Utf8Str &aText)
    {
        return setErrorInternal(aResultCode, getStaticClassIID(), getStaticComponentName(), aText, false, true);
    }

private:

    // wrapped IVFSExplorer properties
    HRESULT getPath(com::Utf8Str &aPath);
    HRESULT getType(VFSType_T *aType);

   // wrapped IVFSExplorer methods
    HRESULT update(ComPtr<IProgress> &aProgress);
    HRESULT cd(const com::Utf8Str &aDir, ComPtr<IProgress> &aProgress);
    HRESULT cdUp(ComPtr<IProgress> &aProgress);
    HRESULT entryList(std::vector<com::Utf8Str> &aNames,
                      std::vector<ULONG> &aTypes,
                      std::vector<LONG64> &aSizes,
                      std::vector<ULONG> &aModes);
    HRESULT exists(const std::vector<com::Utf8Str> &aNames,
                   std::vector<com::Utf8Str> &aExists);
    HRESULT remove(const std::vector<com::Utf8Str> &aNames,
                   ComPtr<IProgress> &aProgress);

    /* Private member vars */
    VirtualBox * const  mVirtualBox;

    ////////////////////////////////////////////////////////////////////////////////
    ////
    //// VFSExplorer definitions
    ////
    //////////////////////////////////////////////////////////////////////////////////
    //
    struct TaskVFSExplorer;  /* Worker thread helper */
    struct Data
    {
        struct DirEntry
        {
            DirEntry(Utf8Str strName, VFSFileType_T fileType, uint64_t cbSize, uint32_t fMode)
               : name(strName)
               , type(fileType)
               , size(cbSize)
               , mode(fMode) {}

             Utf8Str name;
             VFSFileType_T type;
             uint64_t size;
             uint32_t mode;
        };

        VFSType_T storageType;
        Utf8Str strUsername;
        Utf8Str strPassword;
        Utf8Str strHostname;
        Utf8Str strPath;
        Utf8Str strBucket;
        std::list<DirEntry> entryList;
    };

    Data *m;

    /* Private member methods */
    VFSFileType_T i_RTToVFSFileType(int aType) const;

    HRESULT i_updateFS(TaskVFSExplorer *aTask);
    HRESULT i_deleteFS(TaskVFSExplorer *aTask);
    HRESULT i_updateS3(TaskVFSExplorer *aTask);
    HRESULT i_deleteS3(TaskVFSExplorer *aTask);

};

#endif /* ____H_VFSEXPLORERIMPL */

