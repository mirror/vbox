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
    class ElementNode;
}

class VirtualBox;
class Progress;

class ATL_NO_VTABLE Appliance :
    public VirtualBoxBaseWithChildrenNEXT,
    public VirtualBoxSupportErrorInfoImpl <Appliance, IAppliance>,
    public VirtualBoxSupportTranslation <Appliance>,
    VBOX_SCRIPTABLE_IMPL(IAppliance)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (Appliance)

    DECLARE_NOT_AGGREGATABLE(Appliance)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Appliance)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IAppliance)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (Appliance)

    // public initializer/uninitializer for internal purposes only
    HRESULT FinalConstruct() { return S_OK; }
    void FinalRelease() { uninit(); }

    HRESULT init(VirtualBox *aVirtualBox);
    void uninit();

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Appliance"; }

    /* IAppliance properties */
    STDMETHOD(COMGETTER(Path))(BSTR *aPath);
    STDMETHOD(COMGETTER(Disks))(ComSafeArrayOut(BSTR, aDisks));
    STDMETHOD(COMGETTER(VirtualSystemDescriptions))(ComSafeArrayOut(IVirtualSystemDescription*, aVirtualSystemDescriptions));

    /* IAppliance methods */
    STDMETHOD(Read)(IN_BSTR path);
    STDMETHOD(Interpret)(void);
    STDMETHOD(ImportMachines)(IProgress **aProgress);
    STDMETHOD(Write)(IN_BSTR format, IN_BSTR path, IProgress **aProgress);
    STDMETHOD(GetWarnings)(ComSafeArrayOut(BSTR, aWarnings));

    /* public methods only for internal purposes */

    /* private instance data */
private:
    /** weak VirtualBox parent */
    const ComObjPtr <VirtualBox, ComWeakRef> mVirtualBox;

    struct Data;            // obscure, defined in AppliannceImpl.cpp
    Data *m;

    HRESULT LoopThruSections(const char *pcszPath, const xml::ElementNode *pReferencesElem, const xml::ElementNode *pCurElem);
    HRESULT HandleDiskSection(const char *pcszPath, const xml::ElementNode *pReferencesElem, const xml::ElementNode *pSectionElem);
    HRESULT HandleNetworkSection(const char *pcszPath, const xml::ElementNode *pSectionElem);
    HRESULT HandleVirtualSystemContent(const char *pcszPath, const xml::ElementNode *pContentElem);

    HRESULT searchUniqueVMName(Utf8Str& aName) const;
    HRESULT searchUniqueDiskImageFilePath(Utf8Str& aName) const;
    HRESULT setUpProgress(ComObjPtr<Progress> &pProgress, const Bstr &bstrDescription);
    void waitForAsyncProgress(ComObjPtr<Progress> &pProgressThis, ComPtr<IProgress> &pProgressAsync);
    void addWarning(const char* aWarning, ...);

    struct TaskImportMachines;  /* Worker thread for import */
    static DECLCALLBACK(int) taskThreadImportMachines(RTTHREAD thread, void *pvUser);

    struct TaskWriteOVF;       /* Worker thread for export */
    static DECLCALLBACK(int) taskThreadWriteOVF(RTTHREAD thread, void *pvUser);

    friend class Machine;
};

struct VirtualSystemDescriptionEntry
{
    uint32_t ulIndex;                       // zero-based index of this entry within array
    VirtualSystemDescriptionType_T type;    // type of this entry
    Utf8Str strRef;                         // reference number (hard disk controllers only)
    Utf8Str strOvf;                         // original OVF value (type-dependent)
    Utf8Str strVbox;                        // configuration value (type-dependent)
    Utf8Str strExtraConfig;                 // extra configuration key=value strings (type-dependent)

    uint32_t ulSizeMB;                      // hard disk images only: size of the uncompressed image in MB
};

class ATL_NO_VTABLE VirtualSystemDescription :
    public VirtualBoxBaseWithChildrenNEXT,
    public VirtualBoxSupportErrorInfoImpl <VirtualSystemDescription, IVirtualSystemDescription>,
    public VirtualBoxSupportTranslation <VirtualSystemDescription>,
    VBOX_SCRIPTABLE_IMPL(IVirtualSystemDescription)
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
    STDMETHOD(COMGETTER(Count))(ULONG *aCount);

    /* IVirtualSystemDescription methods */
    STDMETHOD(GetDescription)(ComSafeArrayOut(VirtualSystemDescriptionType_T, aTypes),
                              ComSafeArrayOut(BSTR, aRefs),
                              ComSafeArrayOut(BSTR, aOvfValues),
                              ComSafeArrayOut(BSTR, aVboxValues),
                              ComSafeArrayOut(BSTR, aExtraConfigValues));

    STDMETHOD(GetDescriptionByType)(VirtualSystemDescriptionType_T aType,
                                    ComSafeArrayOut(VirtualSystemDescriptionType_T, aTypes),
                                    ComSafeArrayOut(BSTR, aRefs),
                                    ComSafeArrayOut(BSTR, aOvfValues),
                                    ComSafeArrayOut(BSTR, aVboxValues),
                                    ComSafeArrayOut(BSTR, aExtraConfigValues));

    STDMETHOD(GetValuesByType)(VirtualSystemDescriptionType_T aType,
                               VirtualSystemDescriptionValueType_T aWhich,
                               ComSafeArrayOut(BSTR, aValues));

    STDMETHOD(SetFinalValues)(ComSafeArrayIn(BOOL, aEnabled),
                              ComSafeArrayIn(IN_BSTR, aVboxValues),
                              ComSafeArrayIn(IN_BSTR, aExtraConfigValues));

    STDMETHOD(AddDescription)(VirtualSystemDescriptionType_T aType,
                              IN_BSTR aVboxValue,
                              IN_BSTR aExtraConfigValue);

    /* public methods only for internal purposes */

    void addEntry(VirtualSystemDescriptionType_T aType,
                  const Utf8Str &strRef,
                  const Utf8Str &aOrigValue,
                  const Utf8Str &aAutoValue,
                  uint32_t ulSizeMB = 0,
                  const Utf8Str &strExtraConfig = "");

    std::list<VirtualSystemDescriptionEntry*> findByType(VirtualSystemDescriptionType_T aType);
    const VirtualSystemDescriptionEntry* findControllerFromID(uint32_t id);

    /* private instance data */
private:
    struct Data;
    Data *m;

    friend class Machine;
};

#endif // ____H_APPLIANCEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
