/* $Id$ */

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

#ifndef ____H_SYSTEMPROPERTIESIMPL
#define ____H_SYSTEMPROPERTIESIMPL

#include "VirtualBoxBase.h"
#include "HardDiskFormatImpl.h"

#include <VBox/com/array.h>

#include <list>

class VirtualBox;

class ATL_NO_VTABLE SystemProperties :
    public VirtualBoxSupportErrorInfoImpl <SystemProperties, ISystemProperties>,
    public VirtualBoxSupportTranslation <SystemProperties>,
    public VirtualBoxBase,
    public ISystemProperties
{
public:

    DECLARE_NOT_AGGREGATABLE(SystemProperties)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(SystemProperties)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(ISystemProperties)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

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
    STDMETHOD(COMGETTER(MaxGuestMonitors) (ULONG *maxMonitors));
    STDMETHOD(COMGETTER(MaxVDISize) (ULONG64 *maxVDISize));
    STDMETHOD(COMGETTER(NetworkAdapterCount) (ULONG *count));
    STDMETHOD(COMGETTER(SerialPortCount) (ULONG *count));
    STDMETHOD(COMGETTER(ParallelPortCount) (ULONG *count));
    STDMETHOD(COMGETTER(MaxBootPosition) (ULONG *aMaxBootPosition));
    STDMETHOD(COMGETTER(DefaultMachineFolder)) (BSTR *aDefaultMachineFolder);
    STDMETHOD(COMSETTER(DefaultMachineFolder)) (INPTR BSTR aDefaultMachineFolder);
    STDMETHOD(COMGETTER(DefaultHardDiskFolder)) (BSTR *aDefaultHardDiskFolder);
    STDMETHOD(COMSETTER(DefaultHardDiskFolder)) (INPTR BSTR aDefaultHardDiskFolder);
    STDMETHOD(COMGETTER(HardDiskFormats)) (ComSafeArrayOut (IHardDiskFormat *, aHardDiskFormats));
    STDMETHOD(COMGETTER(RemoteDisplayAuthLibrary)) (BSTR *aRemoteDisplayAuthLibrary);
    STDMETHOD(COMSETTER(RemoteDisplayAuthLibrary)) (INPTR BSTR aRemoteDisplayAuthLibrary);
    STDMETHOD(COMGETTER(WebServiceAuthLibrary)) (BSTR *aWebServiceAuthLibrary);
    STDMETHOD(COMSETTER(WebServiceAuthLibrary)) (INPTR BSTR aWebServiceAuthLibrary);
    STDMETHOD(COMGETTER(HWVirtExEnabled)) (BOOL *enabled);
    STDMETHOD(COMSETTER(HWVirtExEnabled)) (BOOL enabled);
    STDMETHOD(COMGETTER(LogHistoryCount)) (ULONG *count);
    STDMETHOD(COMSETTER(LogHistoryCount)) (ULONG count);

    // public methods only for internal purposes

    HRESULT loadSettings (const settings::Key &aGlobal);
    HRESULT saveSettings (settings::Key &aGlobal);

    /** Default Machine path (as is, not full). Not thread safe (use object lock). */
    const Bstr &defaultMachineFolder() { return mDefaultMachineFolder; }
    /** Default Machine path (full). Not thread safe (use object lock). */
    const Bstr &defaultMachineFolderFull() { return mDefaultMachineFolderFull; }
    /** Default hard disk path (as is, not full). Not thread safe (use object lock). */
    const Bstr &defaultHardDiskFolder() { return mDefaultHardDiskFolder; }
    /** Default hard disk path (full). Not thread safe (use object lock). */
    const Bstr &defaultHardDiskFolderFull() { return mDefaultHardDiskFolderFull; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"SystemProperties"; }

private:

    typedef std::list <ComObjPtr <HardDiskFormat> > HardDiskFormatList;

    HRESULT setDefaultMachineFolder (const BSTR aPath);
    HRESULT setDefaultHardDiskFolder (const BSTR aPath);
    HRESULT setRemoteDisplayAuthLibrary (const BSTR aPath);
    HRESULT setWebServiceAuthLibrary (const BSTR aPath);

    ComObjPtr <VirtualBox, ComWeakRef> mParent;

    Bstr mDefaultMachineFolder;
    Bstr mDefaultMachineFolderFull;
    Bstr mDefaultHardDiskFolder;
    Bstr mDefaultHardDiskFolderFull;

    HardDiskFormatList mHardDiskFormats;

    Bstr mRemoteDisplayAuthLibrary;
    Bstr mWebServiceAuthLibrary;
    BOOL mHWVirtExEnabled;
    ULONG mLogHistoryCount;
};

#endif // ____H_SYSTEMPROPERTIESIMPL
