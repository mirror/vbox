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
#ifdef VBOX_WITH_EXTPACK
# include "ExtPackManagerImpl.h"
#endif

#include <iprt/cpp/utils.h>
#include <iprt/ctype.h>
#include <iprt/ldr.h>
#include <iprt/path.h>

#include <VBox/err.h>
#include <VBox/sup.h>

#include <VBox/RemoteDesktop/VRDE.h>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Global.h"
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
    return BaseFinalConstruct();
}

void VRDEServer::FinalRelease()
{
    uninit();
    BaseFinalRelease();
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
    mData->mAuthLibrary.setNull();
    mData->mEnabled              = FALSE;
    mData->mAllowMultiConnection = FALSE;
    mData->mReuseSingleConnection = FALSE;
    mData->mVrdeExtPack.setNull();

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
    mData->mAuthLibrary = data.strAuthLibrary;
    mData->mAllowMultiConnection = data.fAllowMultiConnection;
    mData->mReuseSingleConnection = data.fReuseSingleConnection;
    mData->mVrdeExtPack = data.strVrdeExtPack;
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
    data.strAuthLibrary = mData->mAuthLibrary;
    data.ulAuthTimeout = mData->mAuthTimeout;
    data.fAllowMultiConnection = !!mData->mAllowMultiConnection;
    data.fReuseSingleConnection = !!mData->mReuseSingleConnection;
    data.strVrdeExtPack = mData->mVrdeExtPack;
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
        value = it->second; // source is a Utf8Str
    else if (strKey == "TCP/Ports")
        value = VRDP_DEFAULT_PORT_STR;
    value.cloneTo(aValue);

    return S_OK;
}

static int loadVRDELibrary(const char *pszLibraryName, RTLDRMOD *phmod, PFNVRDESUPPORTEDPROPERTIES *ppfn)
{
    int rc = VINF_SUCCESS;

    RTLDRMOD hmod = NIL_RTLDRMOD;

    RTERRINFOSTATIC ErrInfo;
    RTErrInfoInitStatic(&ErrInfo);
    if (RTPathHavePath(pszLibraryName))
        rc = SUPR3HardenedLdrLoadPlugIn(pszLibraryName, &hmod, &ErrInfo.Core);
    else
        rc = SUPR3HardenedLdrLoadAppPriv(pszLibraryName, &hmod, RTLDRLOAD_FLAGS_LOCAL, &ErrInfo.Core);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(hmod, "VRDESupportedProperties", (void **)ppfn);

        if (RT_FAILURE(rc) && rc != VERR_SYMBOL_NOT_FOUND)
            LogRel(("VRDE: Error resolving symbol '%s', rc %Rrc.\n", "VRDESupportedProperties", rc));
    }
    else
    {
        if (RTErrInfoIsSet(&ErrInfo.Core))
            LogRel(("VRDE: Error loading the library '%s': %s (%Rrc)\n", pszLibraryName, ErrInfo.Core.pszMsg, rc));
        else
            LogRel(("VRDE: Error loading the library '%s' rc = %Rrc.\n", pszLibraryName, rc));

        hmod = NIL_RTLDRMOD;
    }

    if (RT_SUCCESS(rc))
    {
        *phmod = hmod;
    }
    else
    {
        if (hmod != NIL_RTLDRMOD)
        {
            RTLdrClose(hmod);
            hmod = NIL_RTLDRMOD;
        }
    }

    return rc;
}

STDMETHODIMP VRDEServer::COMGETTER(VRDEProperties)(ComSafeArrayOut(BSTR, aProperties))
{
    if (ComSafeArrayOutIsNull(aProperties))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    size_t cProperties = 0;

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->mEnabled)
    {
        com::SafeArray<BSTR> properties(cProperties);
        properties.detachTo(ComSafeArrayOutArg(aProperties));
        return S_OK;
    }
    alock.release();

    /*
     * Check that a VRDE extension pack name is set and resolve it into a
     * library path.
     */
    Bstr bstrExtPack;
    HRESULT hrc = COMGETTER(VRDEExtPack)(bstrExtPack.asOutParam());
    Log(("VRDEPROP: get extpack hrc 0x%08X, isEmpty %d\n", hrc, bstrExtPack.isEmpty()));
    if (FAILED(hrc))
        return hrc;
    if (bstrExtPack.isEmpty())
        return E_FAIL;

    Utf8Str strExtPack(bstrExtPack);
    Utf8Str strVrdeLibrary;
    int vrc = VINF_SUCCESS;
    if (strExtPack.equals(VBOXVRDP_KLUDGE_EXTPACK_NAME))
        strVrdeLibrary = "VBoxVRDP";
    else
    {
#ifdef VBOX_WITH_EXTPACK
        VirtualBox *pVirtualBox = mParent->getVirtualBox();
        ExtPackManager *pExtPackMgr = pVirtualBox->getExtPackManager();
        vrc = pExtPackMgr->getVrdeLibraryPathForExtPack(&strExtPack, &strVrdeLibrary);
#else
        vrc = VERR_FILE_NOT_FOUND;
#endif
    }
    Log(("VRDEPROP: library get rc %Rrc\n", vrc));

    if (RT_SUCCESS(vrc))
    {
        /*
         * Load the VRDE library and start the server, if it is enabled.
         */
        PFNVRDESUPPORTEDPROPERTIES pfn = NULL;
        RTLDRMOD hmod = NIL_RTLDRMOD;
        vrc = loadVRDELibrary(strVrdeLibrary.c_str(), &hmod, &pfn);
        Log(("VRDEPROP: load library [%s] rc %Rrc\n", strVrdeLibrary.c_str(), vrc));
        if (RT_SUCCESS(vrc))
        {
            const char * const *papszNames = pfn();

            if (papszNames)
            {
                size_t i;
                for (i = 0; papszNames[i] != NULL; ++i)
                {
                    cProperties++;
                }
            }
            Log(("VRDEPROP: %d properties\n", cProperties));

            com::SafeArray<BSTR> properties(cProperties);

            if (cProperties > 0)
            {
                size_t i;
                for (i = 0; papszNames[i] != NULL && i < cProperties; ++i)
                {
                    Bstr tmp(papszNames[i]);
                    tmp.cloneTo(&properties[i]);
                }
            }

            /* Do not forget to unload the library. */
            RTLdrClose(hmod);
            hmod = NIL_RTLDRMOD;

            properties.detachTo(ComSafeArrayOutArg(aProperties));
        }
    }

    if (RT_FAILURE(vrc))
    {
        return E_FAIL;
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

STDMETHODIMP VRDEServer::COMGETTER(AuthLibrary) (BSTR *aLibrary)
{
    CheckComArgOutPointerValid(aLibrary);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    Bstr bstrLibrary;

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    bstrLibrary = mData->mAuthLibrary;
    alock.release();

    if (bstrLibrary.isEmpty())
    {
        /* Get the global setting. */
        ComPtr<ISystemProperties> systemProperties;
        HRESULT hrc = mParent->getVirtualBox()->COMGETTER(SystemProperties)(systemProperties.asOutParam());

        if (SUCCEEDED(hrc))
            hrc = systemProperties->COMGETTER(VRDEAuthLibrary)(bstrLibrary.asOutParam());

        if (FAILED(hrc))
            return setError(hrc, "failed to query the library setting\n");
    }

    bstrLibrary.cloneTo(aLibrary);

    return S_OK;
}

STDMETHODIMP VRDEServer::COMSETTER(AuthLibrary) (IN_BSTR aLibrary)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    Bstr bstrLibrary(aLibrary);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mAuthLibrary != bstrLibrary)
    {
        mData.backup();
        mData->mAuthLibrary = bstrLibrary;

        /* leave the lock before informing callbacks */
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);
        mParent->setModified(Machine::IsModified_VRDEServer);
        mlock.release();

        mParent->onVRDEServerChange(/* aRestart */ TRUE);
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

STDMETHODIMP VRDEServer::COMGETTER(VRDEExtPack) (BSTR *aExtPack)
{
    CheckComArgOutPointerValid(aExtPack);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        Utf8Str strExtPack = mData->mVrdeExtPack;
        alock.release();

        if (strExtPack.isNotEmpty())
        {
            if (strExtPack.equals(VBOXVRDP_KLUDGE_EXTPACK_NAME))
                hrc = S_OK;
            else
            {
#ifdef VBOX_WITH_EXTPACK
                ExtPackManager *pExtPackMgr = mParent->getVirtualBox()->getExtPackManager();
                hrc = pExtPackMgr->checkVrdeExtPack(&strExtPack);
#else
                hrc = setError(E_FAIL, tr("Extension pack '%s' does not exist"), strExtPack.c_str());
#endif
            }
            if (SUCCEEDED(hrc))
                strExtPack.cloneTo(aExtPack);
        }
        else
        {
            /* Get the global setting. */
            ComPtr<ISystemProperties> systemProperties;
            hrc = mParent->getVirtualBox()->COMGETTER(SystemProperties)(systemProperties.asOutParam());
            if (SUCCEEDED(hrc))
                hrc = systemProperties->COMGETTER(DefaultVRDEExtPack)(aExtPack);
        }
    }

    return hrc;
}

STDMETHODIMP VRDEServer::COMSETTER(VRDEExtPack)(IN_BSTR aExtPack)
{
    CheckComArgNotNull(aExtPack);
    Utf8Str strExtPack(aExtPack);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        /* the machine needs to be mutable */
        AutoMutableStateDependency adep(mParent);
        hrc = adep.rc();
        if (SUCCEEDED(hrc))
        {
            /*
             * If not empty, check the specific extension pack.
             */
            if (!strExtPack.isEmpty())
            {
                if (strExtPack.equals(VBOXVRDP_KLUDGE_EXTPACK_NAME))
                    hrc = S_OK;
                else
                {
#ifdef VBOX_WITH_EXTPACK
                    ExtPackManager *pExtPackMgr = mParent->getVirtualBox()->getExtPackManager();
                    hrc = pExtPackMgr->checkVrdeExtPack(&strExtPack);
#else
                    hrc = setError(E_FAIL, tr("Extension pack '%s' does not exist"), strExtPack.c_str());
#endif
                }
            }
            if (SUCCEEDED(hrc))
            {
                /*
                 * Update the setting if there is an actual change, post an
                 * change event to trigger a VRDE server restart.
                 */
                AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
                if (strExtPack != mData->mVrdeExtPack)
                {
                    mData.backup();
                    mData->mVrdeExtPack = strExtPack;

                    /* leave the lock before informing callbacks */
                    alock.release();

                    AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);
                    mParent->setModified(Machine::IsModified_VRDEServer);
                    mlock.release();

                    mParent->onVRDEServerChange(/* aRestart */ TRUE);
                }
            }
        }
    }

    return hrc;
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
