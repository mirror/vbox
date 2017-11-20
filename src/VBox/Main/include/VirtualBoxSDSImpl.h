/* $Id$ */
/** @file
 * VBox Global COM Class definition
 */

/*
 * Copyright (C) 2015-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_VIRTUALBOXSDSIMPL
#define ____H_VIRTUALBOXSDSIMPL

#include "VirtualBoxSDSWrap.h"
#include "TokenWrap.h"

#ifdef RT_OS_WINDOWS
# include "win/resource.h"
#endif


/**
 * Per user data.
 */
class VBoxSDSPerUserData
{
public:
    /** The SID (secure identifier) for the user.  This is the key. */
    com::Utf8Str                 m_strUserSid;
    /** The user name (if we could get it). */
    com::Utf8Str                 m_strUsername;
    /** The VBoxSVC chosen to instantiate CLSID_VirtualBox.
     * This is NULL if not set. */
    ComPtr<IVBoxSVCRegistration> m_ptrTheChosenOne;
    /** Critical section protecting everything here. */
    RTCRITSECT                   m_Lock;


public:
    VBoxSDSPerUserData(com::Utf8Str const &a_rStrUserSid, com::Utf8Str const &a_rStrUsername)
        : m_strUserSid(a_rStrUserSid), m_strUsername(a_rStrUsername)
    {
        RTCritSectInit(&m_Lock);
    }

    ~VBoxSDSPerUserData()
    {
        RTCritSectDelete(&m_Lock);
    }

    void i_lock()
    {
        RTCritSectEnter(&m_Lock);
    }

    void i_unlock()
    {
        RTCritSectLeave(&m_Lock);
    }
};


class ATL_NO_VTABLE VirtualBoxSDS :
    public VirtualBoxSDSWrap
#ifdef RT_OS_WINDOWS
    , public ATL::CComCoClass<VirtualBoxSDS, &CLSID_VirtualBoxSDS>
#endif
{
private:
    typedef std::map<com::Utf8Str, VBoxSDSPerUserData *> UserDataMap_T;
    /** Per user data map (key is SID string). */
    UserDataMap_T       m_UserDataMap;
    /** Lock protecting m_UserDataMap.*/
    RTCRITSECTRW        m_MapCritSect;

public:

    DECLARE_CLASSFACTORY_SINGLETON(VirtualBoxSDS)

    //DECLARE_REGISTRY_RESOURCEID(IDR_VIRTUALBOX)

    // Kind of redundant (VirtualBoxSDSWrap declares itself not aggregatable
    // and CComCoClass<VirtualBoxSDS, &CLSID_VirtualBoxSDS> as aggregatable,
    // the former is the first inheritance), but the C multiple inheritance
    // rules and the class factory in VBoxSDS.cpp needs this to disambiguate.
    DECLARE_NOT_AGGREGATABLE(VirtualBoxSDS)

    DECLARE_EMPTY_CTOR_DTOR(VirtualBoxSDS)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializers/uninitializers for internal purposes only
    HRESULT init();
    void uninit();

private:

    // Wrapped IVirtualBoxSDS methods
    HRESULT registerVBoxSVC(const ComPtr<IVBoxSVCRegistration> &aVBoxSVC, LONG aPid, ComPtr<IUnknown> &aExistingVirtualBox);
    HRESULT deregisterVBoxSVC(const ComPtr<IVBoxSVCRegistration> &aVBoxSVC, LONG aPid);


    // Private methods

    /**
     * Gets the client user SID of the
     */
    static bool i_getClientUserSid(com::Utf8Str *a_pStrSid, com::Utf8Str *a_pStrUsername);

    /**
     * Looks up the given user.
     *
     * @returns Pointer to the LOCKED per user data.  NULL if not found.
     * @param   a_rStrUserSid   The user SID.
     */
    VBoxSDSPerUserData *i_lookupPerUserData(com::Utf8Str const &a_rStrUserSid);

    /**
     * Looks up the given user, creating it if not found
     *
     * @returns Pointer to the LOCKED per user data.  NULL on allocation error.
     * @param   a_rStrUserSid   The user SID.
     * @param   a_rStrUsername  The user name if available.
     */
    VBoxSDSPerUserData *i_lookupOrCreatePerUserData(com::Utf8Str const &a_rStrUserSid, com::Utf8Str const &a_rStrUsername);
};


#endif // !____H_VIRTUALBOXSDSIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
