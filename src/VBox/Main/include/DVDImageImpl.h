/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef ____H_DVDIMAGEIMPL
#define ____H_DVDIMAGEIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"

class VirtualBox;

class ATL_NO_VTABLE DVDImage :
    public VirtualBoxSupportErrorInfoImpl <DVDImage, IDVDImage>,
    public VirtualBoxSupportTranslation <DVDImage>,
    public VirtualBoxBase,
    public IDVDImage
{
public:

    // to satisfy the ComObjPtr template (we have const members)
    DVDImage() {}

    DECLARE_NOT_AGGREGATABLE(DVDImage)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(DVDImage)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IDVDImage)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (VirtualBox *parent, const BSTR filePath,
                  BOOL isRegistered, const Guid &id);
    void uninit();

    // IDVDImage properties
    STDMETHOD(COMGETTER(Id)) (GUIDPARAMOUT id);
    STDMETHOD(COMGETTER(FilePath)) (BSTR *filePath);
    STDMETHOD(COMGETTER(Accessible)) (BOOL *accessible);
    STDMETHOD(COMGETTER(Size)) (ULONG64 *size);

    // public methods for internal purposes only

    const Bstr &filePath() { return mImageFile; }
    const Bstr &filePathFull() { return mImageFileFull; }
    const Guid &id() { return mUuid; }

    void updatePath (const char *aNewFullPath, const char *aNewPath);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"DVDImage"; }

private:

    /** weak parent */
    ComObjPtr <VirtualBox, ComWeakRef> mParent;

    BOOL mAccessible;

    const Guid mUuid;
    const Bstr mImageFile;
    const Bstr mImageFileFull;
};

COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN (DVDImage)

    STDMETHOD(FindByPath) (INPTR BSTR path, IDVDImage **dvdImage)
    {
        if (!path)
            return E_INVALIDARG;
        if (!dvdImage)
            return E_POINTER;

        *dvdImage = NULL;
        Vector::value_type found;
        Vector::iterator it = vec.begin();
        while (it != vec.end() && !found)
        {
            Bstr filePath;
            (*it)->COMGETTER(FilePath) (filePath.asOutParam());
            if (filePath == path)
                found = *it;
            ++ it;
        }

        if (!found)
            return setError (E_INVALIDARG, DVDImageCollection::tr (
                "The DVD image named '%ls' could not be found"), path);

        return found.queryInterfaceTo (dvdImage);
    }

COM_DECL_READONLY_ENUM_AND_COLLECTION_END (DVDImage)


#endif // ____H_DVDIMAGEIMPL
