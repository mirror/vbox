/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ____H_FLOPPYIMAGEIMPL
#define ____H_FLOPPYIMAGEIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"

#include <iprt/path.h>

class VirtualBox;

class ATL_NO_VTABLE FloppyImage :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <FloppyImage, IFloppyImage>,
    public VirtualBoxSupportTranslation <FloppyImage>,
    public IFloppyImage
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (FloppyImage)

    DECLARE_NOT_AGGREGATABLE(FloppyImage)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(FloppyImage)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IFloppyImage)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (FloppyImage)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (VirtualBox *aParent, const BSTR aFilePath,
                  BOOL aRegistered, const Guid &aId);
    void uninit();

    // IFloppyImage properties
    STDMETHOD(COMGETTER(Id)) (GUIDPARAMOUT aId);
    STDMETHOD(COMGETTER(FilePath)) (BSTR *aFilePath);
    STDMETHOD(COMGETTER(Accessible)) (BOOL *aAccessible);
    STDMETHOD(COMGETTER(Size)) (ULONG *aSize);

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    const Bstr &filePath() { return mImageFile; }
    const Bstr &filePathFull() { return mImageFileFull; }
    const Guid &id() { return mUuid; }

    void updatePath (const char *aNewFullPath, const char *aNewPath);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"FloppyImage"; }

private:

    /** weak parent */
    const ComObjPtr <VirtualBox, ComWeakRef> mParent;

    BOOL mAccessible;

    const Guid mUuid;
    const Bstr mImageFile;
    const Bstr mImageFileFull;
};

COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN (FloppyImage)

    STDMETHOD(FindByPath) (INPTR BSTR aPath, IFloppyImage **aFloppyImage)
    {
        if (!aPath)
            return E_INVALIDARG;
        if (!aFloppyImage)
            return E_POINTER;

        Utf8Str path = aPath;
        *aFloppyImage = NULL;
        Vector::value_type found;
        Vector::iterator it = vec.begin();
        while (it != vec.end() && !found)
        {
            Bstr filePath;
            (*it)->COMGETTER(FilePath) (filePath.asOutParam());
            if (RTPathCompare (Utf8Str (filePath), path) == 0)
                found = *it;
            ++ it;
        }

        if (!found)
            return setError (E_INVALIDARG, FloppyImageCollection::tr (
                "The floppy image named '%ls' could not be found"), aPath);

        return found.queryInterfaceTo (aFloppyImage);
    }

COM_DECL_READONLY_ENUM_AND_COLLECTION_END (FloppyImage)


#endif // ____H_FLOPPYIMAGEIMPL
