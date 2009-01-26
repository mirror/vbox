/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#ifndef ____H_APPLIANCEIMPL
#define ____H_APPLIANCEIMPL

#include "VirtualBoxBase.h"

class VirtualBox;

class ATL_NO_VTABLE Appliance :
    public VirtualBoxBaseWithChildrenNEXT,
    public VirtualBoxSupportErrorInfoImpl <Appliance, IAppliance>,
    public VirtualBoxSupportTranslation <Appliance>,
    public IAppliance
{

public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (Appliance)

    DECLARE_NOT_AGGREGATABLE(Appliance)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Appliance)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IAppliance)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (Appliance)

    // public initializer/uninitializer for internal purposes only
    HRESULT FinalConstruct() { return S_OK; }
    void FinalRelease() { uninit(); }

    HRESULT init (VirtualBox *aVirtualBox, IN_BSTR &path);
    void uninit();

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Appliance"; }

    /* IAppliance properties */
    STDMETHOD(COMGETTER(Path)) (BSTR *aPath);

    /* IAppliance methods */
    /* void getDisks (out unsigned long aDisksSize, [array, size_is (aDisksSize)] out wstring aDisks, [retval] out unsigned long cDisks); */
    STDMETHOD(GetDisks) (ComSafeArrayOut(BSTR, aDisks), ULONG *cDisks);

    STDMETHOD (COMGETTER (VirtualSystemDescriptions)) (ComSafeArrayOut (IVirtualSystemDescription*, aVirtualSystemDescriptions));

    /* public methods only for internal purposes */
    STDMETHOD (ImportAppliance)();

    /* private instance data */
private:
    /** weak VirtualBox parent */
    const ComObjPtr <VirtualBox, ComWeakRef> mVirtualBox;

    struct Data;            // obscure, defined in AppliannceImpl.cpp
    Data *m;

    HRESULT LoopThruSections(const char *pcszPath, const xml::Node *pReferencesElem, const xml::Node *pCurElem);
    HRESULT HandleDiskSection(const char *pcszPath, const xml::Node *pReferencesElem, const xml::Node *pSectionElem);
    HRESULT HandleNetworkSection(const char *pcszPath, const xml::Node *pSectionElem);
    HRESULT HandleVirtualSystemContent(const char *pcszPath, const xml::Node *pContentElem);

    HRESULT construeAppliance();
    HRESULT searchUniqueVMName (std::string& aName);
};


#include <string>
struct VirtualSystemDescriptionEntry
{
    VirtualSystemDescriptionType_T type; /* Of which type is this value */
    uint64_t ref; /* Reference value to the internal implementation */
    std::string strOriginalValue; /* The original ovf value */
    std::string strAutoValue; /* The value which vbox suggest */
    std::string strFinalValue; /* The value the user select */
    std::string strConfiguration; /* Additional configuration data for this type */
};

class ATL_NO_VTABLE VirtualSystemDescription :
    public VirtualBoxBaseWithChildrenNEXT,
    public VirtualBoxSupportErrorInfoImpl <VirtualSystemDescription, IVirtualSystemDescription>,
    public VirtualBoxSupportTranslation <VirtualSystemDescription>,
    public IVirtualSystemDescription
{
    friend class Appliance;
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (VirtualSystemDescription)

    DECLARE_NOT_AGGREGATABLE(VirtualSystemDescription)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VirtualSystemDescription)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IVirtualSystemDescription)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (VirtualSystemDescription)

    // public initializer/uninitializer for internal purposes only
    HRESULT FinalConstruct() { return S_OK; }
    void FinalRelease() { uninit(); }

    HRESULT init();
    void uninit();

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"VirtualSystemDescription"; }

    /* IVirtualSystemDescription properties */

    /* IVirtualSystemDescription methods */
    STDMETHOD(GetDescription) (ComSafeArrayOut(VirtualSystemDescriptionType_T, aTypes),
                               ComSafeArrayOut(ULONG, aRefs),
                               ComSafeArrayOut(BSTR, aOrigValues),
                               ComSafeArrayOut(BSTR, aAutoValues),
                               ComSafeArrayOut(BSTR, aConfigurations));
    STDMETHOD(SetFinalValues) (ComSafeArrayIn (IN_BSTR, aFinalValues));

    /* public methods only for internal purposes */

    /* private instance data */
private:
    void addEntry (VirtualSystemDescriptionType_T aType, ULONG aRef, std::string aOrigValue, std::string aAutoValue);
    std::list<VirtualSystemDescriptionEntry> findByType (VirtualSystemDescriptionType_T aType);

    struct Data;
    Data *m;
};

#endif // ____H_APPLIANCEIMPL
