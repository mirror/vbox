/* $Id$ */

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

/**
 * The HardDiskFormat class represents the backend used to store hard disk data
 * (IHardDiskFormat interface).
 *
 * @note Instances of this class are permanently caller-referenced by HardDisk
 * objects (through addCaller()) so that an attempt to uninitialize or delete
 * them before all HardDisk objects are uninitialized will produce an endless
 * wait!
 */
class ATL_NO_VTABLE HardDiskFormat :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <HardDiskFormat, IHardDiskFormat>,
    public VirtualBoxSupportTranslation <HardDiskFormat>,
    VBOX_SCRIPTABLE_IMPL(IHardDiskFormat)
{
public:

    struct Property
    {
        Bstr name;
        Bstr description;
        DataType_T type;
        ULONG flags;
        Bstr defaultValue;
    };

    typedef std::list <Bstr> BstrList;
    typedef std::list <Property> PropertyList;

    struct Data
    {
        Data() : capabilities (0) {}

        const Bstr id;
        const Bstr name;
        const BstrList fileExtensions;
        const uint64_t capabilities;
        const PropertyList properties;
    };

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (HardDiskFormat)

    DECLARE_NOT_AGGREGATABLE (HardDiskFormat)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HardDiskFormat)
        COM_INTERFACE_ENTRY (ISupportErrorInfo)
        COM_INTERFACE_ENTRY (IHardDiskFormat)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (HardDiskFormat)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (const VDBACKENDINFO *aVDInfo);
    void uninit();

    // IHardDiskFormat properties
    STDMETHOD(COMGETTER(Id)) (BSTR *aId);
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMGETTER(FileExtensions)) (ComSafeArrayOut (BSTR, aFileExtensions));
    STDMETHOD(COMGETTER(Capabilities)) (ULONG *aCaps);

    // IHardDiskFormat methods
    STDMETHOD(DescribeProperties) (ComSafeArrayOut (BSTR, aNames),
                                   ComSafeArrayOut (BSTR, aDescriptions),
                                   ComSafeArrayOut (DataType_T, aTypes),
                                   ComSafeArrayOut (ULONG, aFlags),
                                   ComSafeArrayOut (BSTR, aDefaults));

    // public methods only for internal purposes

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    /** Const, no need to lock */
    const Bstr &id() const { return m.id; }
    /** Const, no need to lock */
    const BstrList &fileExtensions() const { return m.fileExtensions; }
    /** Const, no need to lock */
    uint64_t capabilities() const { return m.capabilities; }
    /** Const, no need to lock */
    const PropertyList &properties() const { return m.properties; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HardDiskFormat"; }

private:

    Data m;
};

#endif // ____H_HARDDISKFORMAT

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
