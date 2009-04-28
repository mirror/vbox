/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#ifndef ____H_SYSTEMPROPERTIESIMPL
#define ____H_SYSTEMPROPERTIESIMPL

#include "VirtualBoxBase.h"
#include "HardDiskFormatImpl.h"

#include <VBox/com/array.h>

#include <list>

class VirtualBox;

class ATL_NO_VTABLE SystemProperties :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <SystemProperties, ISystemProperties>,
    public VirtualBoxSupportTranslation <SystemProperties>,
    VBOX_SCRIPTABLE_IMPL(ISystemProperties)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (SystemProperties)

    DECLARE_NOT_AGGREGATABLE(SystemProperties)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(SystemProperties)
        COM_INTERFACE_ENTRY  (ISupportErrorInfo)
        COM_INTERFACE_ENTRY  (ISystemProperties)
        COM_INTERFACE_ENTRY2 (IDispatch, ISystemProperties)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (SystemProperties)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (VirtualBox *aParent);
    void uninit();

    // ISystemProperties properties
    STDMETHOD(COMGETTER(MinGuestRAM) (ULONG *minRAM));
    STDMETHOD(COMGETTER(MaxGuestRAM) (ULONG *maxRAM));
    STDMETHOD(COMGETTER(MinGuestVRAM) (ULONG *minVRAM));
    STDMETHOD(COMGETTER(MaxGuestVRAM) (ULONG *maxVRAM));
    STDMETHOD(COMGETTER(MinGuestCPUCount) (ULONG *minCPUCount));
    STDMETHOD(COMGETTER(MaxGuestCPUCount) (ULONG *maxCPUCount));
    STDMETHOD(COMGETTER(MaxGuestMonitors) (ULONG *maxMonitors));
    STDMETHOD(COMGETTER(MaxVDISize) (ULONG64 *maxVDISize));
    STDMETHOD(COMGETTER(NetworkAdapterCount) (ULONG *count));
    STDMETHOD(COMGETTER(SerialPortCount) (ULONG *count));
    STDMETHOD(COMGETTER(ParallelPortCount) (ULONG *count));
    STDMETHOD(COMGETTER(MaxBootPosition) (ULONG *aMaxBootPosition));
    STDMETHOD(COMGETTER(DefaultMachineFolder)) (BSTR *aDefaultMachineFolder);
    STDMETHOD(COMSETTER(DefaultMachineFolder)) (IN_BSTR aDefaultMachineFolder);
    STDMETHOD(COMGETTER(DefaultHardDiskFolder)) (BSTR *aDefaultHardDiskFolder);
    STDMETHOD(COMSETTER(DefaultHardDiskFolder)) (IN_BSTR aDefaultHardDiskFolder);
    STDMETHOD(COMGETTER(HardDiskFormats)) (ComSafeArrayOut (IHardDiskFormat *, aHardDiskFormats));
    STDMETHOD(COMGETTER(DefaultHardDiskFormat)) (BSTR *aDefaultHardDiskFolder);
    STDMETHOD(COMSETTER(DefaultHardDiskFormat)) (IN_BSTR aDefaultHardDiskFolder);
    STDMETHOD(COMGETTER(RemoteDisplayAuthLibrary)) (BSTR *aRemoteDisplayAuthLibrary);
    STDMETHOD(COMSETTER(RemoteDisplayAuthLibrary)) (IN_BSTR aRemoteDisplayAuthLibrary);
    STDMETHOD(COMGETTER(WebServiceAuthLibrary)) (BSTR *aWebServiceAuthLibrary);
    STDMETHOD(COMSETTER(WebServiceAuthLibrary)) (IN_BSTR aWebServiceAuthLibrary);
    STDMETHOD(COMGETTER(HWVirtExEnabled)) (BOOL *enabled);
    STDMETHOD(COMSETTER(HWVirtExEnabled)) (BOOL enabled);
    STDMETHOD(COMGETTER(LogHistoryCount)) (ULONG *count);
    STDMETHOD(COMSETTER(LogHistoryCount)) (ULONG count);

    // public methods only for internal purposes

    HRESULT loadSettings (const settings::Key &aGlobal);
    HRESULT saveSettings (settings::Key &aGlobal);

    ComObjPtr <HardDiskFormat> hardDiskFormat (CBSTR aFormat);

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    /** Default Machine path (as is, not full). Not thread safe (use object lock). */
    const Bstr &defaultMachineFolder() const { return mDefaultMachineFolder; }
    /** Default Machine path (full). Not thread safe (use object lock). */
    const Bstr &defaultMachineFolderFull() const { return mDefaultMachineFolderFull; }
    /** Default hard disk path (as is, not full). Not thread safe (use object lock). */
    const Bstr &defaultHardDiskFolder() const { return mDefaultHardDiskFolder; }
    /** Default hard disk path (full). Not thread safe (use object lock). */
    const Bstr &defaultHardDiskFolderFull() const { return mDefaultHardDiskFolderFull; }

    /** Default hard disk format. Not thread safe (use object lock). */
    const Bstr &defaultHardDiskFormat() const { return mDefaultHardDiskFormat; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"SystemProperties"; }

private:

    typedef std::list <ComObjPtr <HardDiskFormat> > HardDiskFormatList;

    HRESULT setDefaultMachineFolder (CBSTR aPath);
    HRESULT setDefaultHardDiskFolder (CBSTR aPath);
    HRESULT setDefaultHardDiskFormat (CBSTR aFormat);

    HRESULT setRemoteDisplayAuthLibrary (CBSTR aPath);
    HRESULT setWebServiceAuthLibrary (CBSTR aPath);

    const ComObjPtr <VirtualBox, ComWeakRef> mParent;

    Bstr mDefaultMachineFolder;
    Bstr mDefaultMachineFolderFull;
    Bstr mDefaultHardDiskFolder;
    Bstr mDefaultHardDiskFolderFull;
    Bstr mDefaultHardDiskFormat;

    HardDiskFormatList mHardDiskFormats;

    Bstr mRemoteDisplayAuthLibrary;
    Bstr mWebServiceAuthLibrary;
    BOOL mHWVirtExEnabled;
    ULONG mLogHistoryCount;
};

#endif // ____H_SYSTEMPROPERTIESIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
