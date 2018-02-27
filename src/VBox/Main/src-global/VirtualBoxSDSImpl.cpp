/* $Id$ */
/** @file
 * VBox Global COM Class implementation.
 */

/*
 * Copyright(C) 2017 - 2018 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/com/VirtualBox.h>
#include "VirtualBoxSDSImpl.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/critsect.h>

#include <rpcasync.h>
#include <rpcdcep.h>
#include <sddl.h>
#include <lmcons.h> /* UNLEN */


/**
 * Per user data.
 *
 * @note We never delete instances of this class, except in case of an insertion
 *       race.  This allows us to separate the map lock from the user data lock
 *       and avoid DoS issues.
 */
class VBoxSDSPerUserData
{
public:
    /** The SID (secure identifier) for the user.  This is the key. */
    com::Utf8Str                    m_strUserSid;
    /** The user name (if we could get it). */
    com::Utf8Str                    m_strUsername;
    /** The VBoxSVC chosen to instantiate CLSID_VirtualBox.
     * This is NULL if not set. */
    ComPtr<IVBoxSVCRegistration>    m_ptrTheChosenOne;
    ComPtr<IVirtualBoxClientList>   m_ptrClientList;
private:
    /** Reference count to make destruction safe wrt hung callers.
     * (References are retain while holding the map lock in some form, but
     * released while holding no locks.) */
    uint32_t volatile               m_cRefs;
    /** Critical section protecting everything here. */
    RTCRITSECT                      m_Lock;

public:
    VBoxSDSPerUserData(com::Utf8Str const &a_rStrUserSid, com::Utf8Str const &a_rStrUsername)
        : m_strUserSid(a_rStrUserSid), m_strUsername(a_rStrUsername), m_cRefs(1)
    {
        RTCritSectInit(&m_Lock);
    }

    ~VBoxSDSPerUserData()
    {
        RTCritSectDelete(&m_Lock);
    }

    uint32_t i_retain()
    {
        uint32_t cRefs = ASMAtomicIncU32(&m_cRefs);
        Assert(cRefs > 1);
        return cRefs;
    }

    uint32_t i_release()
    {
        uint32_t cRefs = ASMAtomicDecU32(&m_cRefs);
        Assert(cRefs < _1K);
        if (cRefs == 0)
            delete this;
        return cRefs;
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




// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(VirtualBoxSDS)

HRESULT VirtualBoxSDS::FinalConstruct()
{
    LogRelFlowThisFuncEnter();

    int vrc = RTCritSectRwInit(&m_MapCritSect);
    AssertLogRelRCReturn(vrc, E_FAIL);

    LogRelFlowThisFuncLeave();
    return S_OK;
}


void VirtualBoxSDS::FinalRelease()
{
    LogRelFlowThisFuncEnter();

    RTCritSectRwDelete(&m_MapCritSect);

    for (UserDataMap_T::iterator it = m_UserDataMap.begin(); it != m_UserDataMap.end(); ++it)
    {
        VBoxSDSPerUserData *pUserData = it->second;
        if (pUserData)
        {
            it->second = NULL;
            pUserData->i_release();
        }
    }

    LogRelFlowThisFuncLeave();
}



// IVirtualBoxSDS methods
/////////////////////////////////////////////////////////////////////////////


/* SDS plan B interfaces: */
STDMETHODIMP_(HRESULT) VirtualBoxSDS::RegisterVBoxSVC(IVBoxSVCRegistration *aVBoxSVC, LONG aPid, IUnknown **aExistingVirtualBox)
{
    LogRel(("VirtualBoxSDS::registerVBoxSVC: aVBoxSVC=%p aPid=%u\n", (IVBoxSVCRegistration *)aVBoxSVC, aPid));
    HRESULT hrc;
    if (   RT_VALID_PTR(aVBoxSVC)
        && RT_VALID_PTR(aExistingVirtualBox))
    {
        *aExistingVirtualBox = NULL;

        /* Get the client user SID and name. */
        com::Utf8Str strSid;
        com::Utf8Str strUsername;
        if (i_getClientUserSid(&strSid, &strUsername))
        {
            VBoxSDSPerUserData *pUserData = i_lookupOrCreatePerUserData(strSid, strUsername);
            if (pUserData)
            {
                if (pUserData->m_ptrTheChosenOne.isNotNull())
                {
                    try
                    {
                        hrc = pUserData->m_ptrTheChosenOne->GetVirtualBox(aExistingVirtualBox);
                    }
                    catch (...)
                    {
                        LogRel(("VirtualBoxSDS::registerVBoxSVC: unexpected exception calling GetVirtualBox.\n"));
                        hrc = E_FAIL;
                    }
                    if (FAILED_DEAD_INTERFACE(hrc))
                    {
                        LogRel(("VirtualBoxSDS::registerVBoxSVC: Seems VBoxSVC instance died.  Dropping it and letting caller take over.\n"));
                        pUserData->m_ptrTheChosenOne.setNull();
                        /* Release the client list and stop client list watcher thread*/
                        pUserData->m_ptrClientList.setNull();
                    }
                }
                else
                    hrc = S_OK;

                if (pUserData->m_ptrTheChosenOne.isNull())
                {
                    LogRel(("VirtualBoxSDS::registerVBoxSVC: Making aPid=%u the chosen one for user %s (%s)!\n",
                            aPid, pUserData->m_strUserSid.c_str(), pUserData->m_strUsername.c_str()));
                    try
                    {
                        pUserData->m_ptrTheChosenOne = aVBoxSVC;
                        /*
                        * Create instance of ClientList
                        */
                        HRESULT hrc = CoCreateInstance(CLSID_VirtualBoxClientList, NULL, CLSCTX_LOCAL_SERVER,
                            IID_IVirtualBoxClientList,
                            (void **)pUserData->m_ptrClientList.asOutParam());
                        if (SUCCEEDED(hrc))
                        {
                            LogFunc(("Created API client list instance in VBoxSDS : hr=%Rhrf\n", hrc));
                        }
                        else
                        {
                            LogFunc(("Error in creating API client list instance: hr=%Rhrf\n", hrc));
                        }

                        hrc = S_OK;
                    }
                    catch (...)
                    {
                        LogRel(("VirtualBoxSDS::registerVBoxSVC: unexpected exception setting the chosen one.\n"));
                        hrc = E_FAIL;
                    }
                }

                pUserData->i_unlock();
                pUserData->i_release();
            }
            else
                hrc = E_OUTOFMEMORY;
        }
        else
            hrc = E_FAIL;
    }
    else
        hrc = E_INVALIDARG;
    LogRel2(("VirtualBoxSDS::registerVBoxSVC: returns %Rhrc aExistingVirtualBox=%p\n", hrc, (IUnknown *)aExistingVirtualBox));
    return hrc;
}

STDMETHODIMP_(HRESULT) VirtualBoxSDS::DeregisterVBoxSVC(IVBoxSVCRegistration *aVBoxSVC, LONG aPid)
{
    LogRel(("VirtualBoxSDS::deregisterVBoxSVC: aVBoxSVC=%p aPid=%u\n", (IVBoxSVCRegistration *)aVBoxSVC, aPid));
    HRESULT hrc;
    if (RT_VALID_PTR(aVBoxSVC))
    {
        /* Get the client user SID and name. */
        com::Utf8Str strSid;
        com::Utf8Str strUsername;
        if (i_getClientUserSid(&strSid, &strUsername))
        {
            VBoxSDSPerUserData *pUserData = i_lookupPerUserData(strSid);
            if (pUserData)
            {
                if (   (IVBoxSVCRegistration *)aVBoxSVC
                    == (IVBoxSVCRegistration *)pUserData->m_ptrTheChosenOne)
                {
                    LogRel(("VirtualBoxSDS::deregisterVBoxSVC: It's the chosen one for %s (%s)!\n",
                            pUserData->m_strUserSid.c_str(), pUserData->m_strUsername.c_str()));
                    pUserData->m_ptrTheChosenOne.setNull();
                    /** @todo consider evicting the user from the table...   */
                    /* Release the client list and stop client list watcher thread*/
                    pUserData->m_ptrClientList.setNull();
                }
                else
                    LogRel(("VirtualBoxSDS::deregisterVBoxSVC: not the choosen one (%p != %p)\n",
                            (IVBoxSVCRegistration *)aVBoxSVC, (IVBoxSVCRegistration *)pUserData->m_ptrTheChosenOne));
                pUserData->i_unlock();
                pUserData->i_release();

                hrc = S_OK;
            }
            else
            {
                LogRel(("VirtualBoxSDS::deregisterVBoxSVC: Found no user data for %s (%s) (pid %u)\n",
                        strSid.c_str(), strUsername.c_str(), aPid));
                hrc = S_OK;
            }
        }
        else
            hrc = E_FAIL;
    }
    else
        hrc = E_INVALIDARG;
    LogRel2(("VirtualBoxSDS::deregisterVBoxSVC: returns %Rhrc\n", hrc));
    return hrc;
}


STDMETHODIMP_(HRESULT) VirtualBoxSDS::NotifyClientsFinished()
{
    LogRelFlowThisFuncEnter();

    int vrc = RTCritSectRwEnterShared(&m_MapCritSect);
    if (RT_SUCCESS(vrc))
    {
        for (UserDataMap_T::iterator it = m_UserDataMap.begin(); it != m_UserDataMap.end(); ++it)
        {
            VBoxSDSPerUserData *pUserData = it->second;
            if (pUserData && pUserData->m_ptrTheChosenOne)
            {
                LogRelFunc(("Notify VBoxSVC that all clients finished\n"));
                /* Notify VBoxSVC about finishing all API clients it should free references to VBoxSDS
                   and clean up itself */
                if (pUserData->m_ptrClientList)
                    pUserData->m_ptrClientList.setNull();
                pUserData->m_ptrTheChosenOne->NotifyClientsFinished();
            }
        }
        RTCritSectRwLeaveShared(&m_MapCritSect);
    }

    LogRelFlowThisFuncLeave();
    return S_OK;
}

// private methods
///////////////////////////////////////////////////////////////////////////////

/*static*/ bool VirtualBoxSDS::i_getClientUserSid(com::Utf8Str *a_pStrSid, com::Utf8Str *a_pStrUsername)
{
    bool fRet = false;
    a_pStrSid->setNull();
    a_pStrUsername->setNull();

    CoInitializeEx(NULL, COINIT_MULTITHREADED); // is this necessary?
    HRESULT hrc = CoImpersonateClient();
    if (SUCCEEDED(hrc))
    {
        HANDLE hToken = INVALID_HANDLE_VALUE;
        if (::OpenThreadToken(GetCurrentThread(), TOKEN_READ, TRUE /*OpenAsSelf*/, &hToken))
        {
            CoRevertToSelf();

            union
            {
                TOKEN_USER  TokenUser;
                uint8_t     abPadding[SECURITY_MAX_SID_SIZE + 256];
                WCHAR       wszUsername[UNLEN + 1];
            } uBuf;
            RT_ZERO(uBuf);
            DWORD cbActual = 0;
            if (::GetTokenInformation(hToken, TokenUser, &uBuf, sizeof(uBuf), &cbActual))
            {
                WCHAR *pwszString;
                if (ConvertSidToStringSidW(uBuf.TokenUser.User.Sid, &pwszString))
                {
                    try
                    {
                        *a_pStrSid = pwszString;
                        a_pStrSid->toUpper(); /* (just to be on the safe side) */
                        fRet = true;
                    }
                    catch (std::bad_alloc)
                    {
                        LogRel(("VirtualBoxSDS::i_GetClientUserSID: std::bad_alloc setting rstrSid.\n"));
                    }
                    LocalFree((HLOCAL)pwszString);

                    /*
                     * Get the username too.  We don't care if this step fails.
                     */
                    if (fRet)
                    {
                        WCHAR           wszUsername[UNLEN * 2 + 1];
                        DWORD           cwcUsername = RT_ELEMENTS(wszUsername);
                        WCHAR           wszDomain[UNLEN * 2 + 1];
                        DWORD           cwcDomain = RT_ELEMENTS(wszDomain);
                        SID_NAME_USE    enmNameUse;
                        if (LookupAccountSidW(NULL, uBuf.TokenUser.User.Sid, wszUsername, &cwcUsername,
                                              wszDomain, &cwcDomain, &enmNameUse))
                        {
                            wszUsername[RT_ELEMENTS(wszUsername) - 1] = '\0';
                            wszDomain[RT_ELEMENTS(wszDomain) - 1] = '\0';
                            try
                            {
                                *a_pStrUsername = wszDomain;
                                a_pStrUsername->append('/');
                                a_pStrUsername->append(Utf8Str(wszUsername));
                            }
                            catch (std::bad_alloc)
                            {
                                LogRel(("VirtualBoxSDS::i_GetClientUserSID: std::bad_alloc setting rStrUsername.\n"));
                                a_pStrUsername->setNull();
                            }
                        }
                        else
                            LogRel(("VirtualBoxSDS::i_GetClientUserSID: LookupAccountSidW failed: %u/%x (cwcUsername=%u, cwcDomain=%u)\n",
                                   GetLastError(), cwcUsername, cwcDomain));
                    }
                }
                else
                    LogRel(("VirtualBoxSDS::i_GetClientUserSID: ConvertSidToStringSidW failed: %u\n", GetLastError()));
            }
            else
                LogRel(("VirtualBoxSDS::i_GetClientUserSID: GetTokenInformation/TokenUser failed: %u\n", GetLastError()));
            CloseHandle(hToken);
        }
        else
        {
            CoRevertToSelf();
            LogRel(("VirtualBoxSDS::i_GetClientUserSID: OpenThreadToken failed: %u\n", GetLastError()));
        }
    }
    else
        LogRel(("VirtualBoxSDS::i_GetClientUserSID: CoImpersonateClient failed: %Rhrc\n", hrc));
    CoUninitialize();
    return fRet;
}


/**
 * Looks up the given user.
 *
 * @returns Pointer to the LOCKED and RETAINED per user data.
 *          NULL if not found.
 * @param   a_rStrUserSid   The user SID.
 */
VBoxSDSPerUserData *VirtualBoxSDS::i_lookupPerUserData(com::Utf8Str const &a_rStrUserSid)
{
    int vrc = RTCritSectRwEnterShared(&m_MapCritSect);
    if (RT_SUCCESS(vrc))
    {

        UserDataMap_T::iterator it = m_UserDataMap.find(a_rStrUserSid);
        if (it != m_UserDataMap.end())
        {
            VBoxSDSPerUserData *pUserData = it->second;
            pUserData->i_retain();

            RTCritSectRwLeaveShared(&m_MapCritSect);

            pUserData->i_lock();
            return pUserData;
        }

        RTCritSectRwLeaveShared(&m_MapCritSect);
    }
    return NULL;
}


/**
 * Looks up the given user, creating it if not found
 *
 * @returns Pointer to the LOCKED and RETAINED per user data.
 *          NULL on allocation error.
 * @param   a_rStrUserSid   The user SID.
 * @param   a_rStrUsername  The user name if available.
 */
VBoxSDSPerUserData *VirtualBoxSDS::i_lookupOrCreatePerUserData(com::Utf8Str const &a_rStrUserSid,
                                                               com::Utf8Str const &a_rStrUsername)
{
    /*
     * Try do a simple lookup first.
     */
    VBoxSDSPerUserData *pUserData = i_lookupPerUserData(a_rStrUserSid);
    if (!pUserData)
    {
        /*
         * SID is not in map, create a new one.
         */
        try
        {
            pUserData = new VBoxSDSPerUserData(a_rStrUserSid, a_rStrUsername);
        }
        catch (std::bad_alloc)
        {
            pUserData = NULL;
        }
        if (pUserData)
        {
            /*
             * Insert it.  We must check if someone raced us here.
             */
            VBoxSDSPerUserData *pUserDataFree = pUserData;
            pUserData->i_lock();

            int vrc = RTCritSectRwEnterExcl(&m_MapCritSect);
            if (RT_SUCCESS(vrc))
            {

                UserDataMap_T::iterator it = m_UserDataMap.find(a_rStrUserSid);
                if (it == m_UserDataMap.end())
                {
                    try
                    {
                        m_UserDataMap[a_rStrUserSid] = pUserData;
                        pUserData->i_retain();
                    }
                    catch (std::bad_alloc)
                    {
                        pUserData = NULL;
                    }
                }
                else
                    pUserData = NULL;

                RTCritSectRwLeaveExcl(&m_MapCritSect);

                if (pUserData)
                    LogRel(("VirtualBoxSDS::i_lookupOrCreatePerUserData: Created new entry for %s (%s)\n",
                            pUserData->m_strUserSid.c_str(), pUserData->m_strUsername.c_str() ));
                else
                {
                    pUserDataFree->i_unlock();
                    delete pUserDataFree;
                }
            }
        }
    }

    return pUserData;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
