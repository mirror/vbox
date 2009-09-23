/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#ifndef ____H_MEDIUMFORMAT
#define ____H_MEDIUMFORMAT

#include "VirtualBoxBase.h"

#include <VBox/com/array.h>

#include <list>

struct VDBACKENDINFO;

/**
 * The MediumFormat class represents the backend used to store medium data
 * (IMediumFormat interface).
 *
 * @note Instances of this class are permanently caller-referenced by Medium
 * objects (through addCaller()) so that an attempt to uninitialize or delete
 * them before all Medium objects are uninitialized will produce an endless
 * wait!
 */
class ATL_NO_VTABLE MediumFormat :
    public VirtualBoxBase,
    public VirtualBoxSupportErrorInfoImpl<MediumFormat, IMediumFormat>,
    public VirtualBoxSupportTranslation<MediumFormat>,
    VBOX_SCRIPTABLE_IMPL(IMediumFormat)
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

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (MediumFormat)

    DECLARE_NOT_AGGREGATABLE (MediumFormat)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(MediumFormat)
        COM_INTERFACE_ENTRY (ISupportErrorInfo)
        COM_INTERFACE_ENTRY (IMediumFormat)
        COM_INTERFACE_ENTRY (IDispatch)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (MediumFormat)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (const VDBACKENDINFO *aVDInfo);
    void uninit();

    // IMediumFormat properties
    STDMETHOD(COMGETTER(Id)) (BSTR *aId);
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMGETTER(FileExtensions)) (ComSafeArrayOut (BSTR, aFileExtensions));
    STDMETHOD(COMGETTER(Capabilities)) (ULONG *aCaps);

    // IMediumFormat methods
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
    static const wchar_t *getComponentName() { return L"MediumFormat"; }

private:

    Data m;
};

#endif // ____H_MEDIUMFORMAT

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
