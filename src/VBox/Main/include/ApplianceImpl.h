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

namespace xml
{
    class Node;
}

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

    HRESULT init(VirtualBox *aVirtualBox, IN_BSTR &path);
    void uninit();

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Appliance"; }

    /* IAppliance properties */
    STDMETHOD(COMGETTER(Path))(BSTR *aPath);
    STDMETHOD(COMGETTER(Disks))(ComSafeArrayOut(BSTR, aDisks));
    STDMETHOD(COMGETTER(VirtualSystemDescriptions))(ComSafeArrayOut(IVirtualSystemDescription*, aVirtualSystemDescriptions));

    /* IAppliance methods */
    /* void interpret (); */
    STDMETHOD(Interpret)(void);
    STDMETHOD(ImportAppliance)(IProgress **aProgress);

    /* public methods only for internal purposes */

    /* private instance data */
private:
    /** weak VirtualBox parent */
    const ComObjPtr <VirtualBox, ComWeakRef> mVirtualBox;

    struct Task; /* Worker thread for import */

    struct Data;            // obscure, defined in AppliannceImpl.cpp
    Data *m;

    HRESULT LoopThruSections(const char *pcszPath, const xml::Node *pReferencesElem, const xml::Node *pCurElem);
    HRESULT HandleDiskSection(const char *pcszPath, const xml::Node *pReferencesElem, const xml::Node *pSectionElem);
    HRESULT HandleNetworkSection(const char *pcszPath, const xml::Node *pSectionElem);
    HRESULT HandleVirtualSystemContent(const char *pcszPath, const xml::Node *pContentElem);

    HRESULT searchUniqueVMName(Utf8Str& aName) const;
    HRESULT searchUniqueDiskImageFilePath(Utf8Str& aName) const;

    static DECLCALLBACK(int) taskThread(RTTHREAD thread, void *pvUser);
};

struct VirtualSystemDescriptionEntry
{
    uint32_t ulIndex;                       // zero-based index of this entry within array
    VirtualSystemDescriptionType_T type;    // type of this entry
    Utf8Str strRef;                         // reference number (hard disk controllers only)
    Utf8Str strOrig;                        // original OVF value (type-dependent)
    Utf8Str strConfig;                      // configuration value (type-dependent)
    Utf8Str strExtraConfig;                 // extra configuration key=value strings (type-dependent)
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
    STDMETHOD(GetDescription)(ComSafeArrayOut(VirtualSystemDescriptionType_T, aTypes),
                              ComSafeArrayOut(BSTR, aRefs),
                              ComSafeArrayOut(BSTR, aOrigValues),
                              ComSafeArrayOut(BSTR, aConfigValues),
                              ComSafeArrayOut(BSTR, aExtraConfigValues));
    STDMETHOD(DisableItem)(ULONG index);
    STDMETHOD(SetFinalValues)(ComSafeArrayIn(IN_BSTR, aFinalValues));

    /* public methods only for internal purposes */

    /* private instance data */
private:
    void addEntry(VirtualSystemDescriptionType_T aType,
                  const Utf8Str &strRef,
                  const Utf8Str &aOrigValue,
                  const Utf8Str &aAutoValue,
                  const Utf8Str &strExtraConfig = "");

    std::list<VirtualSystemDescriptionEntry*> findByType(VirtualSystemDescriptionType_T aType);
    const VirtualSystemDescriptionEntry* findControllerFromID(uint32_t id);

    struct Data;
    Data *m;
};

#endif // ____H_APPLIANCEIMPL
