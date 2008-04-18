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

#include "BIOSSettingsImpl.h"
#include "MachineImpl.h"
#include "Logging.h"
#include <iprt/cpputils.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT BIOSSettings::FinalConstruct()
{
    return S_OK;
}

void BIOSSettings::FinalRelease()
{
    uninit ();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the audio adapter object.
 *
 * @returns COM result indicator
 */
HRESULT BIOSSettings::init (Machine *aParent)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aParent: %p\n", aParent));

    ComAssertRet (aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    /* share the parent weakly */
    unconst (mParent) = aParent;

    mData.allocate();

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Initializes the audio adapter object given another audio adapter object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 */
HRESULT BIOSSettings::init (Machine *aParent, BIOSSettings *that)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aParent: %p, that: %p\n", aParent, that));

    ComAssertRet (aParent && that, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    mParent = aParent;
    mPeer = that;

    AutoWriteLock thatlock (that);
    mData.share (that->mData);

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT BIOSSettings::initCopy (Machine *aParent, BIOSSettings *that)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aParent: %p, that: %p\n", aParent, that));

    ComAssertRet (aParent && that, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    mParent = aParent;
    // mPeer is left null

    AutoWriteLock thatlock (that);
    mData.attachCopy (that->mData);

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void BIOSSettings::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    mData.free();

    mPeer.setNull();
    mParent.setNull();

    LogFlowThisFuncLeave();
}

// IBIOSSettings properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP BIOSSettings::COMGETTER(LogoFadeIn)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *enabled = mData->mLogoFadeIn;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(LogoFadeIn)(BOOL enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    mData.backup();
    mData->mLogoFadeIn = enable;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(LogoFadeOut)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *enabled = mData->mLogoFadeOut;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(LogoFadeOut)(BOOL enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    mData.backup();
    mData->mLogoFadeOut = enable;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(LogoDisplayTime)(ULONG *displayTime)
{
    if (!displayTime)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *displayTime = mData->mLogoDisplayTime;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(LogoDisplayTime)(ULONG displayTime)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    mData.backup();
    mData->mLogoDisplayTime = displayTime;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(LogoImagePath)(BSTR *imagePath)
{
    if (!imagePath)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mData->mLogoImagePath.cloneTo(imagePath);
    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(LogoImagePath)(INPTR BSTR imagePath)
{
    /* empty strings are not allowed as path names */
    if (imagePath && !(*imagePath))
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    mData.backup();
    mData->mLogoImagePath = imagePath;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(BootMenuMode)(BIOSBootMenuMode_T *bootMenuMode)
{
    if (!bootMenuMode)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *bootMenuMode = mData->mBootMenuMode;
    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(BootMenuMode)(BIOSBootMenuMode_T bootMenuMode)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    mData.backup();
    mData->mBootMenuMode = bootMenuMode;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(ACPIEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *enabled = mData->mACPIEnabled;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(ACPIEnabled)(BOOL enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    mData.backup();
    mData->mACPIEnabled = enable;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(IOAPICEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *enabled = mData->mIOAPICEnabled;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(IOAPICEnabled)(BOOL enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    mData.backup();
    mData->mIOAPICEnabled = enable;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(PXEDebugEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *enabled = mData->mPXEDebugEnabled;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(PXEDebugEnabled)(BOOL enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    mData.backup();
    mData->mPXEDebugEnabled = enable;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(IDEControllerType)(IDEControllerType_T *aControllerType)
{
    if (!aControllerType)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aControllerType = mData->mIDEControllerType;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(IDEControllerType)(IDEControllerType_T aControllerType)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    /* make sure the value is allowed */
    switch (aControllerType)
    {
        case IDEControllerType_PIIX3:
        case IDEControllerType_PIIX4:
            break;
        default:
            return setError (E_FAIL,
                tr("Invalid IDE controller type '%d'"),
                aControllerType);
    }

    mData.backup();

    mData->mIDEControllerType = aControllerType;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(TimeOffset)(LONG64 *offset)
{
    if (!offset)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *offset = mData->mTimeOffset;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(TimeOffset)(LONG64 offset)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    mData.backup();
    mData->mTimeOffset = offset;

    return S_OK;
}


// IBIOSSettings methods
/////////////////////////////////////////////////////////////////////////////

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 *  Loads settings from the given machine node.
 *  May be called once right after this object creation.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for writing.
 */
HRESULT BIOSSettings::loadSettings (const settings::Key &aMachineNode)
{
    using namespace settings;

    AssertReturn (!aMachineNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* Note: we assume that the default values for attributes of optional
     * nodes are assigned in the Data::Data() constructor and don't do it
     * here. It implies that this method may only be called after constructing
     * a new BIOSSettings object while all its data fields are in the default
     * values. Exceptions are fields whose creation time defaults don't match
     * values that should be applied when these fields are not explicitly set
     * in the settings file (for backwards compatibility reasons). This takes
     * place when a setting of a newly created object must default to A while
     * the same setting of an object loaded from the old settings file must
     * default to B. */

    /* BIOS node (required) */
    Key biosNode = aMachineNode.key ("BIOS");

    /* ACPI (required) */
    {
        Key acpiNode = biosNode.key ("ACPI");

        mData->mACPIEnabled = acpiNode.value <bool> ("enabled");
    }

    /* IOAPIC (optional) */
    {
        Key ioapicNode = biosNode.findKey ("IOAPIC");
        if (!ioapicNode.isNull())
            mData->mIOAPICEnabled = ioapicNode.value <bool> ("enabled");
    }

    /* Logo (optional) */
    {
        Key logoNode = biosNode.findKey ("Logo");
        if (!logoNode.isNull())
        {
            mData->mLogoFadeIn = logoNode.value <bool> ("fadeIn");
            mData->mLogoFadeOut = logoNode.value <bool> ("fadeOut");
            mData->mLogoDisplayTime = logoNode.value <ULONG> ("displayTime");
            mData->mLogoImagePath = logoNode.stringValue ("imagePath");
        }
    }

    /* boot menu (optional) */
    {
        Key bootMenuNode = biosNode.findKey ("BootMenu");
        if (!bootMenuNode.isNull())
        {
            mData->mBootMenuMode = BIOSBootMenuMode_MessageAndMenu;
            const char *modeStr = bootMenuNode.stringValue ("mode");

            if (strcmp (modeStr, "Disabled") == 0)
                mData->mBootMenuMode = BIOSBootMenuMode_Disabled;
            else if (strcmp (modeStr, "MenuOnly") == 0)
                mData->mBootMenuMode = BIOSBootMenuMode_MenuOnly;
            else if (strcmp (modeStr, "MessageAndMenu") == 0)
                mData->mBootMenuMode = BIOSBootMenuMode_MessageAndMenu;
            else
                ComAssertMsgFailedRet (("Invalid boot menu mode '%s'\n", modeStr),
                                       E_FAIL);
        }
    }

    /* PXE debug logging (optional) */
    {
        Key pxedebugNode = biosNode.findKey ("PXEDebug");
        if (!pxedebugNode.isNull())
            mData->mPXEDebugEnabled = pxedebugNode.value <bool> ("enabled");
    }

    /* time offset (optional) */
    {
        Key timeOffsetNode = biosNode.findKey ("TimeOffset");
        if (!timeOffsetNode.isNull())
            mData->mTimeOffset = timeOffsetNode.value <LONG64> ("value");
    }

    /* IDE controller type (optional, for old machines that lack this node,
     * defaults to PIIX3) */
    {
        mData->mIDEControllerType = IDEControllerType_PIIX3;

        Key ideControllerNode = biosNode.findKey ("IDEController");
        if (!ideControllerNode.isNull())
        {
            const char *typeStr = ideControllerNode.stringValue ("type");
            if (strcmp (typeStr, "PIIX3") == 0)
                mData->mIDEControllerType = IDEControllerType_PIIX3;
            else if (strcmp (typeStr, "PIIX4") == 0)
                mData->mIDEControllerType = IDEControllerType_PIIX4;
            else
                ComAssertMsgFailedRet (("Invalid boot menu mode '%s'\n", typeStr),
                                       E_FAIL);
        }
    }

    return S_OK;
}

/**
 *  Saves settings to the given machine node.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for reading.
 */
HRESULT BIOSSettings::saveSettings (settings::Key &aMachineNode)
{
    using namespace settings;

    AssertReturn (!aMachineNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Key biosNode = aMachineNode.createKey ("BIOS");

    /* ACPI */
    {
        Key acpiNode = biosNode.createKey ("ACPI");
        acpiNode.setValue <bool> ("enabled", !!mData->mACPIEnabled);
    }

    /* IOAPIC */
    {
        Key ioapicNode = biosNode.createKey ("IOAPIC");
        ioapicNode.setValue <bool> ("enabled", !!mData->mIOAPICEnabled);
    }

    /* BIOS logo (optional) **/
    {
        Key logoNode = biosNode.createKey ("Logo");
        logoNode.setValue <bool> ("fadeIn", !!mData->mLogoFadeIn);
        logoNode.setValue <bool> ("fadeOut", !!mData->mLogoFadeOut);
        logoNode.setValue <ULONG> ("displayTime", mData->mLogoDisplayTime);
        logoNode.setValueOr <Bstr> ("imagePath", mData->mLogoImagePath, Bstr::Null);
    }

    /* boot menu (optional) */
    {
        Key bootMenuNode = biosNode.createKey ("BootMenu");
        const char *modeStr = NULL;
        switch (mData->mBootMenuMode)
        {
            case BIOSBootMenuMode_Disabled:
                modeStr = "Disabled";
                break;
            case BIOSBootMenuMode_MenuOnly:
                modeStr = "MenuOnly";
                break;
            case BIOSBootMenuMode_MessageAndMenu:
                modeStr = "MessageAndMenu";
                break;
            default:
                ComAssertMsgFailedRet (("Invalid boot menu type: %d\n",
                                        mData->mBootMenuMode),
                                       E_FAIL);
        }
        bootMenuNode.setStringValue ("mode", modeStr);
    }

    /* time offset (optional) */
    {
        Key timeOffsetNode = biosNode.createKey ("TimeOffset");
        timeOffsetNode.setValue <LONG64> ("value", mData->mTimeOffset);
    }

    /* PXE debug flag (optional) */
    {
        Key pxedebugNode = biosNode.createKey ("PXEDebug");
        pxedebugNode.setValue <bool> ("enabled", !!mData->mPXEDebugEnabled);
    }

    /* IDE controller type */
    {
        Key ideControllerNode = biosNode.createKey ("IDEController");
        const char *ideControllerTypeStr = NULL;
        switch (mData->mIDEControllerType)
        {
            case IDEControllerType_PIIX3:
                ideControllerTypeStr = "PIIX3";
                break;
            case IDEControllerType_PIIX4:
                ideControllerTypeStr = "PIIX4";
                break;
            default:
                ComAssertMsgFailedRet (("Invalid IDE Controller type: %d\n",
                                        mData->mIDEControllerType),
                                       E_FAIL);
        }
        ideControllerNode.setStringValue ("type", ideControllerTypeStr);
    }

    return S_OK;
}

void BIOSSettings::commit()
{
    AutoWriteLock alock (this);
    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            // attach new data to the peer and reshare it
            AutoWriteLock peerlock (mPeer);
            mPeer->mData.attach (mData);
        }
    }
}

void BIOSSettings::copyFrom (BIOSSettings *aThat)
{
    AutoWriteLock alock (this);

    // this will back up current data
    mData.assignCopy (aThat->mData);
}

