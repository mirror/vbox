/** @file
 *
 * XPCOM module implementation functions
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

/* Make sure all the stdint.h macros are included - must come first! */
#ifndef __STDC_LIMIT_MACROS
# define __STDC_LIMIT_MACROS
#endif
#ifndef __STDC_CONSTANT_MACROS
# define __STDC_CONSTANT_MACROS
#endif

#include <nsIGenericFactory.h>

// generated file
#include "VirtualBox_XPCOM.h"

#include "AdditionsFacilityImpl.h"
#include "ConsoleImpl.h"
#include "ConsoleVRDPServer.h"
#include "DisplayImpl.h"
#ifdef VBOX_WITH_EXTPACK
# include "ExtPackManagerImpl.h"
#endif
#include "GuestImpl.h"
#ifdef VBOX_WITH_GUEST_CONTROL
# include "GuestDirectoryImpl.h"
# include "GuestDirEntryImpl.h"
# include "GuestFileImpl.h"
# include "GuestFsObjInfoImpl.h"
# include "GuestProcessImpl.h"
# include "GuestSessionImpl.h"
#endif
#include "KeyboardImpl.h"
#include "MachineDebuggerImpl.h"
#include "MouseImpl.h"
#include "NATEngineImpl.h"
#include "NetworkAdapterImpl.h"
#include "ProgressCombinedImpl.h"
#include "ProgressImpl.h"
#include "RemoteUSBDeviceImpl.h"
#include "SessionImpl.h"
#include "SharedFolderImpl.h"
#include "USBDeviceImpl.h"
#include "VirtualBoxClientImpl.h"

#include "Logging.h"

// XPCOM glue code unfolding

NS_DECL_CLASSINFO(Guest)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Guest, IGuest)
#ifdef VBOX_WITH_GUEST_CONTROL
NS_DECL_CLASSINFO(GuestDirectory)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(GuestDirectory, IGuestDirectory)
NS_DECL_CLASSINFO(GuestDirEntry)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(GuestDirEntry, IGuestDirEntry)
NS_DECL_CLASSINFO(GuestFile)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(GuestFile, IGuestFile)
NS_DECL_CLASSINFO(GuestFsObjInfo)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(GuestFsObjInfo, IGuestFsObjInfo)
NS_DECL_CLASSINFO(GuestProcess)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(GuestProcess, IGuestProcess)
NS_DECL_CLASSINFO(GuestSession)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(GuestSession, IGuestSession)
#endif
NS_DECL_CLASSINFO(Keyboard)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Keyboard, IKeyboard)
NS_DECL_CLASSINFO(Mouse)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Mouse, IMouse)
NS_DECL_CLASSINFO(Display)
NS_IMPL_THREADSAFE_ISUPPORTS2_CI(Display, IDisplay, IEventListener)
NS_DECL_CLASSINFO(MachineDebugger)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(MachineDebugger, IMachineDebugger)
NS_DECL_CLASSINFO(Progress)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Progress, IProgress)
NS_DECL_CLASSINFO(CombinedProgress)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(CombinedProgress, IProgress)
NS_DECL_CLASSINFO(OUSBDevice)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(OUSBDevice, IUSBDevice)
NS_DECL_CLASSINFO(RemoteUSBDevice)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(RemoteUSBDevice, IHostUSBDevice)
NS_DECL_CLASSINFO(SharedFolder)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(SharedFolder, ISharedFolder)
NS_DECL_CLASSINFO(VRDEServerInfo)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(VRDEServerInfo, IVRDEServerInfo)
#ifdef VBOX_WITH_EXTPACK
NS_DECL_CLASSINFO(ExtPackFile)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(ExtPackFile, IExtPackFile)
NS_DECL_CLASSINFO(ExtPack)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(ExtPack, IExtPack)
NS_DECL_CLASSINFO(ExtPackManager)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(ExtPackManager, IExtPackManager)
#endif
NS_DECL_CLASSINFO(AdditionsFacility)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(AdditionsFacility, IAdditionsFacility)

NS_DECL_CLASSINFO(Session)
NS_IMPL_THREADSAFE_ISUPPORTS2_CI(Session, ISession, IInternalSessionControl)
NS_DECL_CLASSINFO(Console)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Console, IConsole)

NS_DECL_CLASSINFO(VirtualBoxClient)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(VirtualBoxClient, IVirtualBoxClient)

/**
 *  Singleton class factory that holds a reference to the created instance
 *  (preventing it from being destroyed) until the module is explicitly
 *  unloaded by the XPCOM shutdown code.
 *
 *  Suitable for IN-PROC components.
 */
class SessionClassFactory : public Session
{
public:
    virtual ~SessionClassFactory() {
        FinalRelease();
        instance = 0;
    }
    static nsresult getInstance (Session **inst) {
        int rv = NS_OK;
        if (instance == 0) {
            instance = new SessionClassFactory();
            if (instance) {
                instance->AddRef(); // protect FinalConstruct()
                rv = instance->FinalConstruct();
                if (NS_FAILED(rv))
                    instance->Release();
                else
                    instance->AddRef(); // self-reference
            } else {
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
        } else {
            instance->AddRef();
        }
        *inst = instance;
        return rv;
    }
    static nsresult releaseInstance () {
        if (instance)
            instance->Release();
        return NS_OK;
    }

private:
    static Session *instance;
};

/** @note this is for singleton; disabled for now */
//
//Session *SessionClassFactory::instance = 0;
//
//NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR_WITH_RC (
//    Session, SessionClassFactory::getInstance
//)

NS_GENERIC_FACTORY_CONSTRUCTOR_WITH_RC(Session)

NS_GENERIC_FACTORY_CONSTRUCTOR_WITH_RC(VirtualBoxClient)

/**
 *  Component definition table.
 *  Lists all components defined in this module.
 */
static const nsModuleComponentInfo components[] =
{
    {
        "Session component", // description
        NS_SESSION_CID, NS_SESSION_CONTRACTID, // CID/ContractID
        SessionConstructor, // constructor function
        NULL, // registration function
        NULL, // deregistration function
/** @note this is for singleton; disabled for now */
//        SessionClassFactory::releaseInstance,
        NULL, // destructor function
        NS_CI_INTERFACE_GETTER_NAME(Session), // interfaces function
        NULL, // language helper
        &NS_CLASSINFO_NAME(Session) // global class info & flags
    },
    {
        "VirtualBoxClient component", // description
        NS_VIRTUALBOXCLIENT_CID, NS_VIRTUALBOXCLIENT_CONTRACTID, // CID/ContractID
        VirtualBoxClientConstructor, // constructor function
        NULL, // registration function
        NULL, // deregistration function
        NULL, // destructor function
        NS_CI_INTERFACE_GETTER_NAME(VirtualBoxClient), // interfaces function
        NULL, // language helper
        &NS_CLASSINFO_NAME(VirtualBoxClient) // global class info & flags
    },
};

NS_IMPL_NSGETMODULE (VirtualBox_Client_Module, components)
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
