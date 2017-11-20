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
 * The MediumLockToken class automates cleanup of a Medium lock.
 */
class ATL_NO_VTABLE VirtualBoxToken :
    public TokenWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(VirtualBoxToken)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(const std::wstring& wstrUserName);
    void uninit();

    /**
    * Fabrique method to create and initialize COM token object.
    *
    * @param aToken         Pointer to result Token COM object.
    * @param wstrUserName   User name of client for that we create the token
    */
    static HRESULT CreateToken(ComPtr<IToken>& aToken, const std::wstring& wstrUserName);

    // Trace COM reference counting in debug mode
#ifdef RT_STRICT
    virtual ULONG InternalAddRef();
    virtual ULONG InternalRelease();
#endif

private:

    // wrapped IToken methods
    HRESULT abandon(AutoCaller &aAutoCaller);
    HRESULT dummy();

    // data
    struct Data
    {
        std::wstring wstrUserName;
    };

    Data m;
};

#ifdef VBOX_WITH_SDS_PLAN_B
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
#endif


class ATL_NO_VTABLE VirtualBoxSDS :
    public VirtualBoxSDSWrap
#ifdef RT_OS_WINDOWS
    , public ATL::CComCoClass<VirtualBoxSDS, &CLSID_VirtualBoxSDS>
#endif
{
#ifdef VBOX_WITH_SDS_PLAN_B
private:
    typedef std::map<com::Utf8Str, VBoxSDSPerUserData *> UserDataMap_T;
    /** Per user data map (key is SID string). */
    UserDataMap_T       m_UserDataMap;
    /** Lock protecting m_UserDataMap.*/
    RTCRITSECTRW        m_MapCritSect;
#endif

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

    // Wrapped IVirtualBoxSDS properties

    /**
    *    Returns the instance of VirtualBox for client.
    *    It impersonates client and returns his VirtualBox instance
    *    either from cahce or makes new one.
    */
    HRESULT getVirtualBox(ComPtr<IVirtualBox> &aVirtualBox, ComPtr<IToken> &aToken);

    /* SDS plan B interfaces: */
    HRESULT registerVBoxSVC(const ComPtr<IVBoxSVCRegistration> &aVBoxSVC, LONG aPid, ComPtr<IUnknown> &aExistingVirtualBox);
    HRESULT deregisterVBoxSVC(const ComPtr<IVBoxSVCRegistration> &aVBoxSVC, LONG aPid);

    // Wrapped IVirtualBoxSDS methods

    // Private methods

    /**
     * Gets the cliedt user SID of the
     */
    static bool i_getClientUserSID(com::Utf8Str *a_pStrSid, com::Utf8Str *a_pStrUsername);

#ifdef VBOX_WITH_SDS_PLAN_B
    /**
     * Looks up the given user.
     *
     * @returns Pointer to the LOCKED per user data.  NULL if not found.
     * @param   a_rStrUserSid   The user SID.
     */
    VBoxSDSPerUserData *i_lookupPerUserData(com::Utf8Str const &a_rUserSid);

    /**
     * Looks up the given user, creating it if not found
     *
     * @returns Pointer to the LOCKED per user data.  NULL on allocation error.
     * @param   a_rStrUserSid   The user SID.
     * @param   a_rStrUsername  The user name if available.
     */
    VBoxSDSPerUserData *i_lookupOrCreatePerUserData(com::Utf8Str const &a_rStrUserSid, com::Utf8Str const &a_rStrUsername);

#else
    /**
    *  Gets the current user name of current thread
    */
    int GetCurrentUserName(std::wstring& wstrUserName);

    /**
    *  Prints current user name of this thread to the log
    *  @param prefix    string fragment that will be inserted at the beginning
    *                   of the logging line
    */
    void LogUserName(char *prefix);

    /**
     * Thread that periodically checks items in cache and cleans obsolete items
     */
    static DWORD WINAPI CheckCacheThread(LPVOID);

    // data fields
    class VirtualBoxCache;
    static VirtualBoxCache m_cache;
    friend VirtualBoxToken;
#endif
};


#endif // !____H_VIRTUALBOXSDSIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
