/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_APPLIANCEIMPL
#define ____H_APPLIANCEIMPL

/* VBox includes */
#include "VirtualBoxBase.h"

/* VBox forward declarations */
class Progress;
class VirtualSystemDescription;
struct VirtualSystemDescriptionEntry;

namespace ovf
{
    struct HardDiskController;
    struct VirtualSystem;
    class OVFReader;
    struct DiskImage;
}

namespace xml
{
    class ElementNode;
}

namespace settings
{
    class MachineConfigFile;
}

class ATL_NO_VTABLE Appliance :
    public VirtualBoxBase,
    public VirtualBoxSupportErrorInfoImpl<Appliance, IAppliance>,
    public VirtualBoxSupportTranslation<Appliance>,
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

    DECLARE_EMPTY_CTOR_DTOR (Appliance)

    enum OVFFormat
    {
        unspecified,
        OVF_0_9,
        OVF_1_0
    };

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
    /* Import methods */
    STDMETHOD(Read)(IN_BSTR path, IProgress **aProgress);
    STDMETHOD(Interpret)(void);
    STDMETHOD(ImportMachines)(IProgress **aProgress);
    /* Export methods */
    STDMETHOD(CreateVFSExplorer)(IN_BSTR aURI, IVFSExplorer **aExplorer);
    STDMETHOD(Write)(IN_BSTR format, IN_BSTR path, IProgress **aProgress);

    STDMETHOD(GetWarnings)(ComSafeArrayOut(BSTR, aWarnings));

    /* public methods only for internal purposes */

    /* private instance data */
private:
    /** weak VirtualBox parent */
    VirtualBox* const   mVirtualBox;

    struct Data;            // opaque, defined in ApplianceImpl.cpp
    Data *m;

    bool isApplianceIdle() const;

    HRESULT searchUniqueVMName(Utf8Str& aName) const;
    HRESULT searchUniqueDiskImageFilePath(Utf8Str& aName) const;
    HRESULT getDefaultHardDiskFolder(Utf8Str &str) const;
    void waitForAsyncProgress(ComObjPtr<Progress> &pProgressThis, ComPtr<IProgress> &pProgressAsync);
    void addWarning(const char* aWarning, ...);

    void disksWeight();
    enum SetUpProgressMode { Regular, ImportS3, WriteS3 };
    HRESULT setUpProgress(ComObjPtr<Progress> &pProgress,
                          const Bstr &bstrDescription,
                          SetUpProgressMode mode);

    struct LocationInfo;
    void parseURI(Utf8Str strUri, LocationInfo &locInfo) const;
    void parseBucket(Utf8Str &aPath, Utf8Str &aBucket) const;
    Utf8Str manifestFileName(Utf8Str aPath) const;

    HRESULT readImpl(const LocationInfo &aLocInfo, ComObjPtr<Progress> &aProgress);

    struct TaskOVF;
    static DECLCALLBACK(int) taskThreadImportOrExport(RTTHREAD aThread, void *pvUser);

    HRESULT readFS(const LocationInfo &locInfo);
    HRESULT readS3(TaskOVF *pTask);

    void convertDiskAttachmentValues(const ovf::HardDiskController &hdc,
                                     uint32_t ulAddressOnParent,
                                     Bstr &controllerType,
                                     int32_t &lChannel,
                                     int32_t &lDevice);

    HRESULT importImpl(const LocationInfo &aLocInfo, ComObjPtr<Progress> &aProgress);
    HRESULT manifestVerify(const LocationInfo &locInfo, const ovf::OVFReader &reader);

    HRESULT importFS(const LocationInfo &locInfo, ComObjPtr<Progress> &aProgress);

    struct ImportStack;
    void importOneDiskImage(const ovf::DiskImage &di,
                            const Utf8Str &strTargetPath,
                            ComPtr<IMedium> &pTargetHD,
                            ImportStack &stack);
    void importMachineGeneric(const ovf::VirtualSystem &vsysThis,
                              ComObjPtr<VirtualSystemDescription> &vsdescThis,
                              ComPtr<IMachine> &pNewMachine,
                              ImportStack &stack);
    void importVBoxMachine(ComObjPtr<VirtualSystemDescription> &vsdescThis,
                           ComPtr<IMachine> &pNewMachine,
                           ImportStack &stack);

    HRESULT importS3(TaskOVF *pTask);

    HRESULT writeImpl(OVFFormat aFormat, const LocationInfo &aLocInfo, ComObjPtr<Progress> &aProgress);

    struct XMLStack;
    void buildXMLForOneVirtualSystem(xml::ElementNode &elmToAddVirtualSystemsTo,
                                     ComObjPtr<VirtualSystemDescription> &vsdescThis,
                                     OVFFormat enFormat,
                                     XMLStack &stack);

    HRESULT writeFS(const LocationInfo &locInfo, const OVFFormat enFormat, ComObjPtr<Progress> &pProgress);
    HRESULT writeS3(TaskOVF *pTask);

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

    uint32_t ulSizeMB;                      // hard disk images only: a copy of ovf::DiskImage::ulSuggestedSizeMB
};

class ATL_NO_VTABLE VirtualSystemDescription :
    public VirtualBoxBase,
    public VirtualBoxSupportErrorInfoImpl<VirtualSystemDescription, IVirtualSystemDescription>,
    public VirtualBoxSupportTranslation<VirtualSystemDescription>,
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
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

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
                  const Utf8Str &aOvfValue,
                  const Utf8Str &aVboxValue,
                  uint32_t ulSizeMB = 0,
                  const Utf8Str &strExtraConfig = "");

    std::list<VirtualSystemDescriptionEntry*> findByType(VirtualSystemDescriptionType_T aType);
    const VirtualSystemDescriptionEntry* findControllerFromID(uint32_t id);

    void importVboxMachineXML(const xml::ElementNode &elmMachine);
    const settings::MachineConfigFile* getMachineConfig() const;

    /* private instance data */
private:
    struct Data;
    Data *m;

    friend class Machine;
};

#endif // ____H_APPLIANCEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
