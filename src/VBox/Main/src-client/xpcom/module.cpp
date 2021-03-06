/** @file
 *
 * XPCOM module implementation functions
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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

#include "SessionImpl.h"
#include "VirtualBoxClientImpl.h"
#include "RemoteUSBDeviceImpl.h"
#include "USBDeviceImpl.h"

#include "Logging.h"

// XPCOM glue code unfolding

#ifndef VBOX_COM_INPROC_API_CLIENT
NS_DECL_CLASSINFO(RemoteUSBDevice)
NS_IMPL_THREADSAFE_ISUPPORTS2_CI(RemoteUSBDevice, IHostUSBDevice, IUSBDevice)
#endif /* VBOX_COM_INPROC_API_CLIENT */

/*
 * Declare extern variables here to tell the compiler that
 * NS_DECL_CLASSINFO(SessionWrap)
 * already exists in the VBoxAPIWrap library.
 */
NS_DECL_CI_INTERFACE_GETTER(SessionWrap)
extern nsIClassInfo *NS_CLASSINFO_NAME(SessionWrap);

/*
 * Declare extern variables here to tell the compiler that
 * NS_DECL_CLASSINFO(VirtualBoxClientWrap)
 * already exists in the VBoxAPIWrap library.
 */
NS_DECL_CI_INTERFACE_GETTER(VirtualBoxClientWrap)
extern nsIClassInfo *NS_CLASSINFO_NAME(VirtualBoxClientWrap);

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
        NS_CI_INTERFACE_GETTER_NAME(SessionWrap), // interfaces function
        NULL, // language helper
        &NS_CLASSINFO_NAME(SessionWrap) // global class info & flags
    },
    {
        "VirtualBoxClient component", // description
        NS_VIRTUALBOXCLIENT_CID, NS_VIRTUALBOXCLIENT_CONTRACTID, // CID/ContractID
        VirtualBoxClientConstructor, // constructor function
        NULL, // registration function
        NULL, // deregistration function
        NULL, // destructor function
        NS_CI_INTERFACE_GETTER_NAME(VirtualBoxClientWrap), // interfaces function
        NULL, // language helper
        &NS_CLASSINFO_NAME(VirtualBoxClientWrap) // global class info & flags
    },
};

NS_IMPL_NSGETMODULE (VirtualBox_Client_Module, components)
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
