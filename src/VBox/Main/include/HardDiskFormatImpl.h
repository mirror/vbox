/* $Id $ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#ifndef ____H_HARDDISKFORMAT
#define ____H_HARDDISKFORMAT

#include "VirtualBoxBase.h"

#include <VBox/com/array.h>

#include <list>

struct VDBACKENDINFO;

class ATL_NO_VTABLE HardDiskFormat :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <HardDiskFormat, IHardDiskFormat>,
    public VirtualBoxSupportTranslation <HardDiskFormat>,
    public IHardDiskFormat
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (HardDiskFormat)

    DECLARE_NOT_AGGREGATABLE(HardDiskFormat)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HardDiskFormat)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHardDiskFormat)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (HardDiskFormat)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (VDBACKENDINFO *aVDInfo);
    void uninit();

    // IHardDiskFormat properties
    STDMETHOD(COMGETTER(Id)) (BSTR *aId);

    STDMETHOD(COMGETTER(SupportUuid) (BOOL *aBool));
    STDMETHOD(COMGETTER(SupportCreateFixed) (BOOL *aBool));
    STDMETHOD(COMGETTER(SupportCreateDynamic) (BOOL *aBool));
    STDMETHOD(COMGETTER(SupportCreateSplit2G) (BOOL *aBool));
    STDMETHOD(COMGETTER(SupportDiff) (BOOL *aBool));
    STDMETHOD(COMGETTER(SupportASync) (BOOL *aBool));
    STDMETHOD(COMGETTER(SupportFile) (BOOL *aBool));
    STDMETHOD(COMGETTER(SupportConfig) (BOOL *aBool));

    STDMETHOD(COMGETTER(FileExtensions)) (ComSafeArrayOut (BSTR, aFileExtensions));

    // public methods only for internal purposes

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HardDiskFormat"; }

private:

    typedef std::list <Bstr> BstrList;

    struct Data
    {
        Data() : capabilities (0) {}

        const Bstr id;
        const uint64_t capabilities;
        const BstrList fileExtensions;
    };

    Data mData;
};

#endif // ____H_HARDDISKFORMAT

