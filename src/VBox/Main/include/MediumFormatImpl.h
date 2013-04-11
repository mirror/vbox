/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2008-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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
    VBOX_SCRIPTABLE_IMPL(IMediumFormat)
{
public:

    struct Property
    {
        Utf8Str     strName;
        Utf8Str     strDescription;
        DataType_T  type;
        ULONG       flags;
        Utf8Str     strDefaultValue;
    };

    typedef std::list<Utf8Str>      StrList;
    typedef std::list<DeviceType_T> DeviceTypeList;
    typedef std::list<Property>     PropertyList;

    struct Data
    {
        Data() : capabilities((MediumFormatCapabilities_T)0) {}

        const Utf8Str        strId;
        const Utf8Str        strName;
        const StrList        llFileExtensions;
        const DeviceTypeList llDeviceTypes;
        const MediumFormatCapabilities_T capabilities;
        const PropertyList   llProperties;
    };

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(MediumFormat, IMediumFormat)

    DECLARE_NOT_AGGREGATABLE(MediumFormat)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(MediumFormat)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IMediumFormat)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(MediumFormat)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(const VDBACKENDINFO *aVDInfo);
    void uninit();

    // IMediumFormat properties
    STDMETHOD(COMGETTER(Id))(BSTR *aId);
    STDMETHOD(COMGETTER(Name))(BSTR *aName);
    STDMETHOD(COMGETTER(Capabilities))(ComSafeArrayOut(MediumFormatCapabilities_T, aCaps));

    // IMediumFormat methods
    STDMETHOD(DescribeFileExtensions)(ComSafeArrayOut(BSTR, aFileExtensions),
                                      ComSafeArrayOut(DeviceType_T, aDeviceTypes));
    STDMETHOD(DescribeProperties)(ComSafeArrayOut(BSTR, aNames),
                                  ComSafeArrayOut(BSTR, aDescriptions),
                                  ComSafeArrayOut(DataType_T, aTypes),
                                  ComSafeArrayOut(ULONG, aFlags),
                                  ComSafeArrayOut(BSTR, aDefaults));

    // public methods only for internal purposes

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    /** Const, no need to lock */
    const Utf8Str& getId() const { return m.strId; }
    /** Const, no need to lock */
    const StrList& getFileExtensions() const { return m.llFileExtensions; }
    /** Const, no need to lock */
    MediumFormatCapabilities_T getCapabilities() const { return m.capabilities; }
    /** Const, no need to lock */
    const PropertyList& getProperties() const { return m.llProperties; }

private:

    Data m;
};

#endif // ____H_MEDIUMFORMAT

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
