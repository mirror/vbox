/* $Id$ */
/** @file
 * Implementation of IGraphicsAdapter in VBoxSVC.
 */

/*
 * Copyright (C) 2004-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#define LOG_GROUP LOG_GROUP_MAIN_GRAPHICSADAPTER

#include "LoggingNew.h"

#include "GraphicsAdapterImpl.h"
#include "MachineImpl.h"

#include "AutoStateDep.h"
#include "AutoCaller.h"

#include <iprt/cpp/utils.h>


/** @def MY_VECTOR_ASSIGN_ARRAY
 * Safe way to copy an array (static + const) into a vector w/ minimal typing.
 *
 * @param a_rVector     The destination vector reference.
 * @param a_aSrcArray   The source array to assign to the vector.
 */
#if RT_GNUC_PREREQ(13, 0) && !RT_GNUC_PREREQ(14, 0) && defined(VBOX_WITH_GCC_SANITIZER)
/* Workaround for g++ 13.2 incorrectly failing on arrays with a single entry in ASAN builds.
   This is restricted to [13.0, 14.0), assuming the issue was introduced in the 13 cycle
   and will be fixed by the time 14 is done.  If 14 doesn't fix it, extend the range
   version by version till it is fixed. */
# define MY_VECTOR_ASSIGN_ARRAY(a_rVector, a_aSrcArray) do { \
        _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wstringop-overread\""); \
        (a_rVector).assign(&a_aSrcArray[0], &a_aSrcArray[RT_ELEMENTS(a_aSrcArray)]); \
        _Pragma("GCC diagnostic pop"); \
    } while (0)
#else
# define MY_VECTOR_ASSIGN_ARRAY(a_rVector, a_aSrcArray) do { \
        (a_rVector).assign(&a_aSrcArray[0], &a_aSrcArray[RT_ELEMENTS(a_aSrcArray)]); \
    } while (0)
#endif


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

GraphicsAdapter::GraphicsAdapter() :
    mParent(NULL)
{}

GraphicsAdapter::~GraphicsAdapter()
{}

HRESULT GraphicsAdapter::FinalConstruct()
{
    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void GraphicsAdapter::FinalRelease()
{
    LogFlowThisFunc(("\n"));
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the graphics adapter object.
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT GraphicsAdapter::init(Machine *aParent)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    /* mPeer is left null */

    mData.allocate();

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the graphics adapter object given another graphics adapter
 *  object (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT GraphicsAdapter::init(Machine *aParent, GraphicsAdapter *aThat)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    unconst(mPeer) = aThat;

    AutoCaller thatCaller(aThat);
    AssertComRCReturnRC(thatCaller.hrc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
    mData.share(aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the graphics adapter object given another graphics adapter
 *  object (a kind of copy constructor). This object makes a private copy
 *  of data of the original object passed as an argument.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT GraphicsAdapter::initCopy(Machine *aParent, GraphicsAdapter *aThat)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    /* mPeer is left null */

    AutoCaller thatCaller(aThat);
    AssertComRCReturnRC(thatCaller.hrc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
    mData.attachCopy(aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void GraphicsAdapter::uninit()
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

// Wrapped IGraphicsAdapter properties
/////////////////////////////////////////////////////////////////////////////

HRESULT GraphicsAdapter::getGraphicsControllerType(GraphicsControllerType_T *aGraphicsControllerType)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aGraphicsControllerType = mData->graphicsControllerType;

    return S_OK;
}

HRESULT GraphicsAdapter::setGraphicsControllerType(GraphicsControllerType_T aGraphicsControllerType)
{
    switch (aGraphicsControllerType)
    {
        case GraphicsControllerType_Null:
        case GraphicsControllerType_VBoxVGA:
#ifdef VBOX_WITH_VMSVGA
        case GraphicsControllerType_VMSVGA:
        case GraphicsControllerType_VBoxSVGA:
#endif
        case GraphicsControllerType_QemuRamFB:
            break;
        default:
            return setError(E_INVALIDARG, tr("The graphics controller type (%d) is invalid"), aGraphicsControllerType);
    }

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.hrc())) return adep.hrc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mParent->i_setModified(Machine::IsModified_GraphicsAdapter);
    mData.backup();
    mData->graphicsControllerType = aGraphicsControllerType;

    return S_OK;
}

HRESULT GraphicsAdapter::getVRAMSize(ULONG *aVRAMSize)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aVRAMSize = mData->ulVRAMSizeMB;

    return S_OK;
}

HRESULT GraphicsAdapter::setVRAMSize(ULONG aVRAMSize)
{
    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.hrc())) return adep.hrc();

    ULONG uMin, uMax;
    HRESULT hrc = PlatformProperties::s_getSupportedVRAMRange(mData->graphicsControllerType, mData->fAccelerate3D,
                                                              &uMin, &uMax, NULL /* aStrideSizeMB */);
    if (FAILED(hrc))
        return setError(hrc,
                        tr("Error getting VRAM range for selected graphics controller"));

    /* check VRAM limits */
    if (   aVRAMSize < uMin
        || aVRAMSize > uMax)
        return setError(E_INVALIDARG,
                        tr("Invalid VRAM size: %lu MB (must be in range [%lu, %lu] MB)"),
                        aVRAMSize, uMin, uMax);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mParent->i_setModified(Machine::IsModified_GraphicsAdapter);
    mData.backup();
    mData->ulVRAMSizeMB = aVRAMSize;

    return S_OK;
}

/**
 * Static helper function to return all supported features for a given graphics controller.
 *
 * @returns VBox status code.
 * @param   enmController                          Graphics controller to return supported features for.
 * @param   vecSupportedGraphicsControllerFeatures Returned features on success.
 */
/* static */
int GraphicsAdapter::s_getSupportedFeatures(GraphicsControllerType_T enmController,
                                            std::vector<GraphicsFeature_T> &vecSupportedGraphicsFeatures)
{
    switch (enmController)
    {
#ifdef VBOX_WITH_VMSVGA
        case GraphicsControllerType_VBoxSVGA:
        {
            static const GraphicsFeature_T s_aGraphicsFeatures[] =
            {
# ifdef VBOX_WITH_VIDEOHWACCEL
                GraphicsFeature_Acceleration2DVideo,
# endif
# ifdef VBOX_WITH_3D_ACCELERATION
                GraphicsFeature_Acceleration3D
# endif
            };
            MY_VECTOR_ASSIGN_ARRAY(vecSupportedGraphicsFeatures, s_aGraphicsFeatures);
            break;
        }
#endif
        case GraphicsControllerType_VBoxVGA:
            RT_FALL_THROUGH();
        case GraphicsControllerType_QemuRamFB:
        {
            static const GraphicsFeature_T s_aGraphicsFeatures[] =
            {
                GraphicsFeature_None
            };
            MY_VECTOR_ASSIGN_ARRAY(vecSupportedGraphicsFeatures, s_aGraphicsFeatures);
            break;
        }

        default:
            return VERR_INVALID_PARAMETER;
    }

    return VINF_SUCCESS;
}

/**
 * Static helper function to return whether a given graphics feature for a graphics controller is enabled or not.
 *
 * @returns \c true if the given feature is supported, or \c false if not.
 * @param   enmController           Graphics controlller to query a feature for.
 * @param   enmFeature              Feature to query.
 */
/* static */
bool GraphicsAdapter::s_isFeatureSupported(GraphicsControllerType_T enmController, GraphicsFeature_T enmFeature)
{
    std::vector<GraphicsFeature_T> vecSupportedGraphicsFeatures;
    int vrc = GraphicsAdapter::s_getSupportedFeatures(enmController, vecSupportedGraphicsFeatures);
    if (RT_SUCCESS(vrc))
        return std::find(vecSupportedGraphicsFeatures.begin(),
                         vecSupportedGraphicsFeatures.end(), enmFeature) != vecSupportedGraphicsFeatures.end();
    return false;
}

HRESULT GraphicsAdapter::setFeature(GraphicsFeature_T aFeature, BOOL aEnabled)
{
    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.hrc())) return adep.hrc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Validate if the given feature is supported by this graphics controller. */
    if (!GraphicsAdapter::s_isFeatureSupported(mData->graphicsControllerType, aFeature))
        return setError(VBOX_E_NOT_SUPPORTED, tr("The graphics controller does not support the given feature"));

    bool *pfSetting = NULL;
    switch (aFeature)
    {
        case GraphicsFeature_Acceleration2DVideo:
            pfSetting = &mData->fAccelerate2DVideo;
            break;

        case GraphicsFeature_Acceleration3D:
            pfSetting = &mData->fAccelerate3D;
            break;

        default:
            break;
    }

    if (!pfSetting)
        return setError(E_NOTIMPL, tr("The given feature is not implemented"));

    if (*pfSetting != RT_BOOL(aEnabled))
    {
        mParent->i_setModified(Machine::IsModified_GraphicsAdapter);
        mData.backup();

        *pfSetting = RT_BOOL(aEnabled);
    }

    return S_OK;
}

HRESULT GraphicsAdapter::isFeatureEnabled(GraphicsFeature_T aFeature, BOOL *aEnabled)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    bool *pfSetting = NULL;

    switch (aFeature)
    {
#ifndef VBOX_WITH_VIRT_ARMV8 /* On macOS (ARM) we don't support any 2D/3D acceleration for now. */
        case GraphicsFeature_Acceleration2DVideo:
            pfSetting = &mData->fAccelerate2DVideo;
            *pfSetting = false; /* @bugref{9691} -- The legacy VHWA acceleration has been disabled completely. */
            break;

        case GraphicsFeature_Acceleration3D:
            pfSetting = &mData->fAccelerate3D;
            break;
#endif
        default:
            break;
    }

    if (!pfSetting)
        return VBOX_E_NOT_SUPPORTED;

    *aEnabled = *pfSetting;

    return S_OK;
}

HRESULT GraphicsAdapter::getMonitorCount(ULONG *aMonitorCount)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMonitorCount = mData->cMonitors;

    return S_OK;
}

HRESULT GraphicsAdapter::setMonitorCount(ULONG aMonitorCount)
{
    /* make sure monitor count is a sensible number */
    if (aMonitorCount < 1 || aMonitorCount > SchemaDefs::MaxGuestMonitors)
        return setError(E_INVALIDARG,
                        tr("Invalid monitor count: %lu (must be in range [%lu, %lu])"),
                        aMonitorCount, 1, SchemaDefs::MaxGuestMonitors);

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.hrc())) return adep.hrc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mParent->i_setModified(Machine::IsModified_GraphicsAdapter);
    mData.backup();
    mData->cMonitors = aMonitorCount;

    return S_OK;
}

// Wrapped IGraphicsAdapter methods
/////////////////////////////////////////////////////////////////////////////

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 *  Loads settings from the given machine node.
 *  May be called once right after this object creation.
 *
 *  @param data Configuration settings.
 *
 *  @note Locks this object for writing.
 */
HRESULT GraphicsAdapter::i_loadSettings(const settings::GraphicsAdapter &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.hrc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.assignCopy(&data);

    return S_OK;
}

/**
 *  Saves settings to the given machine node.
 *
 *  @param data Configuration settings.
 *
 *  @note Locks this object for reading.
 */
HRESULT GraphicsAdapter::i_saveSettings(settings::GraphicsAdapter &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.hrc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data = *mData.data();

    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
void GraphicsAdapter::i_rollback()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.hrc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.rollback();
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void GraphicsAdapter::i_commit()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.hrc());

    /* sanity too */
    AutoCaller peerCaller(mPeer);
    AssertComRCReturnVoid(peerCaller.hrc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(mPeer, this COMMA_LOCKVAL_SRC_POS);

    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            /* attach new data to the peer and reshare it */
            mPeer->mData.attach(mData);
        }
    }
}

/**
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void GraphicsAdapter::i_copyFrom(GraphicsAdapter *aThat)
{
    AssertReturnVoid(aThat != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.hrc());

    /* sanity too */
    AutoCaller thatCaller(aThat);
    AssertComRCReturnVoid(thatCaller.hrc());

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoReadLock rl(aThat COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    /* this will back up current data */
    mData.assignCopy(aThat->mData);
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
