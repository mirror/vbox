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

#ifndef ____H_FLOPPYIMAGEIMPL
#define ____H_FLOPPYIMAGEIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"

class VirtualBox;

class ATL_NO_VTABLE FloppyImage :
    public VirtualBoxSupportErrorInfoImpl <FloppyImage, IFloppyImage>,
    public VirtualBoxSupportTranslation <FloppyImage>,
    public VirtualBoxBase,
    public IFloppyImage
{
public:

    // to satisfy the ComObjPtr template (we have const members)
    FloppyImage() {}

    DECLARE_NOT_AGGREGATABLE(FloppyImage)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(FloppyImage)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IFloppyImage)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (VirtualBox *parent, const BSTR filePath,
                  BOOL isRegistered, const Guid &id);
    void uninit();

    // IFloppyImage properties
    STDMETHOD(COMGETTER(Id)) (GUIDPARAMOUT id);
    STDMETHOD(COMGETTER(FilePath)) (BSTR *filePath);
    STDMETHOD(COMGETTER(Accessible)) (BOOL *accessible);
    STDMETHOD(COMGETTER(Size)) (ULONG *size);

    // public methods for internal purposes only

    const Bstr &filePath() { return mImageFile; }
    const Bstr &filePathFull() { return mImageFileFull; }
    const Guid &id() { return mUuid; }

    void updatePath (const char *aNewFullPath, const char *aNewPath);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"FloppyImage"; }

private:

    /** weak parent */
    ComObjPtr <VirtualBox, ComWeakRef> mParent;

    BOOL mAccessible;

    const Guid mUuid;
    const Bstr mImageFile;
    const Bstr mImageFileFull;
};

COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN (FloppyImage)

    STDMETHOD(FindByPath) (INPTR BSTR path, IFloppyImage **floppyImage)
    {
        if (!path)
            return E_INVALIDARG;
        if (!floppyImage)
            return E_POINTER;

        *floppyImage = NULL;
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
            return setError (E_INVALIDARG, FloppyImageCollection::tr (
                "The floppy image named '%ls' could not be found"), path);

        return found.queryInterfaceTo (floppyImage);
    }

COM_DECL_READONLY_ENUM_AND_COLLECTION_END (FloppyImage)


#endif // ____H_FLOPPYIMAGEIMPL
