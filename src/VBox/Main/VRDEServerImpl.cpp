/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VRDEServerImpl.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"

#include <iprt/cpp/utils.h>
#include <iprt/ctype.h>
#include <iprt/ldr.h>

#include <VBox/err.h>

#include <VBox/RemoteDesktop/VRDE.h>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Logging.h"

// defines
/////////////////////////////////////////////////////////////////////////////
#define VRDP_DEFAULT_PORT_STR "3389"

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

VRDEServer::VRDEServer()
    : mParent(NULL)
{
}

VRDEServer::~VRDEServer()
{
}

HRESULT VRDEServer::FinalConstruct()
{
    return S_OK;
}

void VRDEServer::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the VRDP server object.
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT VRDEServer::init (Machine *aParent)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    /* mPeer is left null */

    mData.allocate();

    mData->mAuthType             = AuthType_Null;
    mData->mAuthTimeout          = 0;
    mData->mEnabled              = FALSE;
    mData->mAllowMultiConnection = FALSE;
    mData->mReuseSingleConnection = FALSE;
    mData->mVideoChannel         = FALSE;
    mData->mVideoChannelQuality  = 75;
    mData->mVRDELibrary          = (const char *)NULL;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the object given another object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT VRDEServer::init (Machine *aParent, VRDEServer *aThat)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    unconst(mPeer) = aThat;

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
    mData.share (aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT VRDEServer::initCopy (Machine *aParent, VRDEServer *aThat)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    /* mPeer is left null */

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
    mData.attachCopy (aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void VRDEServer::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    mData.free();

    unconst(mPeer) = NULL;
    unconst(mParent) = NULL;
}

/**
 *  Loads settings from the given machine node.
 *  May be called once right after this object creation.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for writing.
 */
HRESULT VRDEServer::loadSettings(const settings::VRDESettings &data)
{
    using namespace settings;

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->mEnabled = data.fEnabled;
    mData->mAuthType = data.authType;
    mData->mAuthTimeout = data.ulAuthTimeout;
    mData->mAllowMultiConnection = data.fAllowMultiConnection;
    mData->mReuseSingleConnection = data.fReuseSingleConnection;
    mData->mVideoChannel = data.fVideoChannel;
    mData->mVideoChannelQuality = data.ulVideoChannelQuality;
    mData->mVRDELibrary = data.strVRDELibrary;
    mData->mProperties = data.mapProperties;

    return S_OK;
}

/**
 *  Saves settings to the given machine node.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for reading.
 */
HRESULT VRDEServer::saveSettings(settings::VRDESettings &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data.fEnabled = !!mData->mEnabled;
    data.authType = mData->mAuthType;
    data.ulAuthTimeout = mData->mAuthTimeout;
    data.fAllowMultiConnection = !!mData->mAllowMultiConnection;
    data.fReuseSingleConnection = !!mData->mReuseSingleConnection;
    data.fVideoChannel = !!mData->mVideoChannel;
    data.ulVideoChannelQuality = mData->mVideoChannelQuality;
    data.strVRDELibrary = mData->mVRDELibrary;
    data.mapProperties = mData->mProperties;

    return S_OK;
}

// IVRDEServer properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VRDEServer::COMGETTER(Enabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aEnabled = mData->mEnabled;

    return S_OK;
}

STDMETHODIMP VRDEServer::COMSETTER(Enabled) (BOOL aEnabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine can also be in saved state for this property to change */
    AutoMutableOrSavedStateDependency adep (mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mEnabled != aEnabled)
    {
        mData.backup();
        mData->mEnabled = aEnabled;

        /* leave the lock before informing callbacks */
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, needs no locking
        mParent->setModified(Machine::IsModified_VRDEServer);
        mlock.release();

        /* Avoid deadlock when onVRDEServerChange eventually calls SetExtraData. */
        adep.release();

        mParent->onVRDEServerChange(/* aRestart */ TRUE);
    }

    return S_OK;
}

static int portParseNumber(uint16_t *pu16Port, const char *pszStart, const char *pszEnd)
{
    /* Gets a string of digits, converts to 16 bit port number.
     * Note: pszStart <= pszEnd is expected, the string contains
     *       only digits and pszEnd points to the char after last
     *       digit.
     */
    int cch = pszEnd - pszStart;
    if (cch > 0 && cch <= 5) /* Port is up to 5 decimal digits. */
    {
        unsigned uPort = 0;
        while (pszStart != pszEnd)
        {
            uPort = uPort * 10 + *pszStart - '0';
            pszStart++;
        }

        if (uPort != 0 && uPort < 0x10000)
        {
            if (pu16Port)
                *pu16Port = (uint16_t)uPort;
            return VINF_SUCCESS;
        }
    }

    return VERR_INVALID_PARAMETER;
}

static int vrdpServerVerifyPortsString(Bstr ports)
{
    com::Utf8Str portRange = ports;

    const char *pszPortRange = portRange.c_str();

    if (!pszPortRange || *pszPortRange == 0) /* Reject empty string. */
        return VERR_INVALID_PARAMETER;

    /* The string should be like "1000-1010,1020,2000-2003" */
    while (*pszPortRange)
    {
        const char *pszStart = pszPortRange;
        const char *pszDash = NULL;
        const char *pszEnd = pszStart;

        while (*pszEnd && *pszEnd != ',')
        {
            if (*pszEnd == '-')
            {
                if (pszDash != NULL)
                    return VERR_INVALID_PARAMETER; /* More than one '-'. */

                pszDash = pszEnd;
            }
            else if (!RT_C_IS_DIGIT(*pszEnd))
                return VERR_INVALID_PARAMETER;

            pszEnd++;
        }

        /* Update the next range pointer. */
        pszPortRange = pszEnd;
        if (*pszPortRange == ',')
        {
            pszPortRange++;
        }

        /* A probably valid range. Verify and parse it. */
        int rc;
        if (pszDash)
        {
            rc = portParseNumber(NULL, pszStart, pszDash);
            if (RT_SUCCESS(rc))
                rc = portParseNumber(NULL, pszDash + 1, pszEnd);
        }
        else
            rc = portParseNumber(NULL, pszStart, pszEnd);

        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}

STDMETHODIMP VRDEServer::SetVRDEProperty (IN_BSTR aKey, IN_BSTR aValue)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* The machine needs to be mutable. */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    Bstr key = aKey;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Special processing for some "standard" properties. */
    if (key == Bstr("TCP/Ports"))
    {
        Bstr ports = aValue;

        /* Verify the string. */
        int vrc = vrdpServerVerifyPortsString(ports);
        if (RT_FAILURE(vrc))
            return E_INVALIDARG;

        if (ports != mData->mProperties["TCP/Ports"])
        {
            /* Port value is not verified here because it is up to VRDP transport to
             * use it. Specifying a wrong port number will cause a running server to
             * stop. There is no fool proof here.
             */
            mData.backup();
            if (ports == Bstr("0"))
                mData->mProperties["TCP/Ports"] = VRDP_DEFAULT_PORT_STR;
            else
                mData->mProperties["TCP/Ports"] = ports;

            /* leave the lock before informing callbacks */
            alock.release();

            AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, needs no locking
            mParent->setModified(Machine::IsModified_VRDEServer);
            mlock.release();

            /* Avoid deadlock when onVRDEServerChange eventually calls SetExtraData. */
            adep.release();

            mParent->onVRDEServerChange(/* aRestart */ TRUE);
        }
    }
    else
    {
        /* Generic properties processing.
         * Look up the old value first; if nothing's changed then do nothing.
         */
        Utf8Str strValue(aValue);
        Utf8Str strKey(aKey);
        Utf8Str strOldValue;

        settings::StringsMap::const_iterator it = mData->mProperties.find(strKey);
        if (it != mData->mProperties.end())
            strOldValue = it->second;

        if (strOldValue != strValue)
        {
            if (strValue.isEmpty())
                mData->mProperties.erase(strKey);
            else
                mData->mProperties[strKey] = strValue;

            /* leave the lock before informing callbacks */
            alock.release();

            AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);
            mParent->setModified(Machine::IsModified_VRDEServer);
            mlock.release();

            /* Avoid deadlock when onVRDEServerChange eventually calls SetExtraData. */
            adep.release();

            mParent->onVRDEServerChange(/* aRestart */ TRUE);
        }
    }

    return S_OK;
}

STDMETHODIMP VRDEServer::GetVRDEProperty (IN_BSTR aKey, BSTR *aValue)
{
    CheckComArgOutPointerValid(aValue);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    Bstr key = aKey;
    Bstr value;

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Utf8Str strKey(key);
    settings::StringsMap::const_iterator it = mData->mProperties.find(strKey);
    if (it != mData->mProperties.end())
    {
        value = it->second; // source is a Utf8Str
        value.cloneTo(aValue);
    }

    return S_OK;
}


STDMETHODIMP VRDEServer::COMGETTER(AuthType) (AuthType_T *aType)
{
    CheckComArgOutPointerValid(aType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aType = mData->mAuthType;

    return S_OK;
}

STDMETHODIMP VRDEServer::COMSETTER(AuthType) (AuthType_T aType)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mAuthType != aType)
    {
        mData.backup();
        mData->mAuthType = aType;

        /* leave the lock before informing callbacks */
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, needs no locking
        mParent->setModified(Machine::IsModified_VRDEServer);
        mlock.release();

        mParent->onVRDEServerChange(/* aRestart */ TRUE);
    }

    return S_OK;
}

STDMETHODIMP VRDEServer::COMGETTER(AuthTimeout) (ULONG *aTimeout)
{
    CheckComArgOutPointerValid(aTimeout);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aTimeout = mData->mAuthTimeout;

    return S_OK;
}

STDMETHODIMP VRDEServer::COMSETTER(AuthTimeout) (ULONG aTimeout)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (aTimeout != mData->mAuthTimeout)
    {
        mData.backup();
        mData->mAuthTimeout = aTimeout;

        /* leave the lock before informing callbacks */
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, needs no locking
        mParent->setModified(Machine::IsModified_VRDEServer);
        mlock.release();

        /* sunlover 20060131: This setter does not require the notification
         * really */
#if 0
        mParent->onVRDEServerChange();
#endif
    }

    return S_OK;
}

STDMETHODIMP VRDEServer::COMGETTER(AllowMultiConnection) (
    BOOL *aAllowMultiConnection)
{
    CheckComArgOutPointerValid(aAllowMultiConnection);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aAllowMultiConnection = mData->mAllowMultiConnection;

    return S_OK;
}

STDMETHODIMP VRDEServer::COMSETTER(AllowMultiConnection) (
    BOOL aAllowMultiConnection)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mAllowMultiConnection != aAllowMultiConnection)
    {
        mData.backup();
        mData->mAllowMultiConnection = aAllowMultiConnection;

        /* leave the lock before informing callbacks */
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, needs no locking
        mParent->setModified(Machine::IsModified_VRDEServer);
        mlock.release();

        mParent->onVRDEServerChange(/* aRestart */ TRUE); // @todo does it need a restart?
    }

    return S_OK;
}

STDMETHODIMP VRDEServer::COMGETTER(ReuseSingleConnection) (
    BOOL *aReuseSingleConnection)
{
    CheckComArgOutPointerValid(aReuseSingleConnection);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aReuseSingleConnection = mData->mReuseSingleConnection;

    return S_OK;
}

STDMETHODIMP VRDEServer::COMSETTER(ReuseSingleConnection) (
    BOOL aReuseSingleConnection)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mReuseSingleConnection != aReuseSingleConnection)
    {
        mData.backup();
        mData->mReuseSingleConnection = aReuseSingleConnection;

        /* leave the lock before informing callbacks */
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, needs no locking
        mParent->setModified(Machine::IsModified_VRDEServer);
        mlock.release();

        mParent->onVRDEServerChange(/* aRestart */ TRUE); // @todo needs a restart?
    }

    return S_OK;
}

STDMETHODIMP VRDEServer::COMGETTER(VideoChannel) (
    BOOL *aVideoChannel)
{
    CheckComArgOutPointerValid(aVideoChannel);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aVideoChannel = mData->mVideoChannel;

    return S_OK;
}

STDMETHODIMP VRDEServer::COMSETTER(VideoChannel) (
    BOOL aVideoChannel)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mVideoChannel != aVideoChannel)
    {
        mData.backup();
        mData->mVideoChannel = aVideoChannel;

        /* leave the lock before informing callbacks */
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, needs no locking
        mParent->setModified(Machine::IsModified_VRDEServer);
        mlock.release();

        mParent->onVRDEServerChange(/* aRestart */ TRUE);
    }

    return S_OK;
}

STDMETHODIMP VRDEServer::COMGETTER(VideoChannelQuality) (
    ULONG *aVideoChannelQuality)
{
    CheckComArgOutPointerValid(aVideoChannelQuality);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aVideoChannelQuality = mData->mVideoChannelQuality;

    return S_OK;
}

STDMETHODIMP VRDEServer::COMSETTER(VideoChannelQuality) (
    ULONG aVideoChannelQuality)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    aVideoChannelQuality = RT_CLAMP(aVideoChannelQuality, 10, 100);

    if (mData->mVideoChannelQuality != aVideoChannelQuality)
    {
        mData.backup();
        mData->mVideoChannelQuality = aVideoChannelQuality;

        /* leave the lock before informing callbacks */
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, needs no locking
        mParent->setModified(Machine::IsModified_VRDEServer);
        mlock.release();

        mParent->onVRDEServerChange(/* aRestart */ FALSE);
    }

    return S_OK;
}

STDMETHODIMP VRDEServer::COMGETTER(VRDELibrary) (BSTR *aVRDELibrary)
{
    CheckComArgOutPointerValid(aVRDELibrary);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    Bstr bstrLibrary;

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    bstrLibrary = mData->mVRDELibrary;
    alock.release();

    if (!bstrLibrary.isEmpty())
    {
        BOOL fRegistered = FALSE;
        HRESULT hrc = mParent->getVirtualBox()->VRDEIsLibraryRegistered(bstrLibrary.raw(), &fRegistered);
        if (FAILED(hrc) || !fRegistered)
            return setError(E_FAIL, "The library is not registered\n");
    }
    else
    {
        /* Get the global setting. */
        ComPtr<ISystemProperties> systemProperties;
        HRESULT hrc = mParent->getVirtualBox()->COMGETTER(SystemProperties)(systemProperties.asOutParam());

        if (SUCCEEDED(hrc))
            hrc = systemProperties->COMGETTER(DefaultVRDELibrary)(bstrLibrary.asOutParam());

        if (FAILED(hrc))
            return setError(hrc, "failed to query the library setting\n");
    }

    bstrLibrary.cloneTo(aVRDELibrary);

    return S_OK;
}

STDMETHODIMP VRDEServer::COMSETTER(VRDELibrary) (IN_BSTR aVRDELibrary)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    Bstr bstrLibrary(aVRDELibrary);

    if (!bstrLibrary.isEmpty())
    {
        BOOL fRegistered = FALSE;
        HRESULT rc = mParent->getVirtualBox()->VRDEIsLibraryRegistered(bstrLibrary.raw(), &fRegistered);
        if (FAILED(rc) || !fRegistered)
            return setError(E_FAIL, "The library is not registered\n");
    }

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mVRDELibrary != aVRDELibrary)
    {
        mData.backup();
        mData->mVRDELibrary = aVRDELibrary;

        /* leave the lock before informing callbacks */
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);
        mParent->setModified(Machine::IsModified_VRDEServer);
        mlock.release();

        mParent->onVRDEServerChange(/* aRestart */ TRUE);
    }

    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 *  @note Locks this object for writing.
 */
void VRDEServer::rollback()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.rollback();
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void VRDEServer::commit()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller (mPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(mPeer, this COMMA_LOCKVAL_SRC_POS);

    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            /* attach new data to the peer and reshare it */
            mPeer->mData.attach (mData);
        }
    }
}

/**
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void VRDEServer::copyFrom (VRDEServer *aThat)
{
    AssertReturnVoid (aThat != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnVoid (thatCaller.rc());

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoReadLock rl(aThat COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    /* this will back up current data */
    mData.assignCopy (aThat->mData);
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
