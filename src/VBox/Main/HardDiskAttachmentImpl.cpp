/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "HardDiskAttachmentImpl.h"
#include "HardDiskImpl.h"

#include "Logging.h"

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (HardDiskAttachment)

HRESULT HardDiskAttachment::FinalConstruct()
{
    mBus = StorageBus_Null;
    mChannel = 0;
    mDevice = 0;

    return S_OK;
}

void HardDiskAttachment::FinalRelease()
{
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the hard disk attachment object.
 *  The initialized object becomes immediately dirty
 *
 *  @param aHD      hard disk object
 *  @param aBus     bus type
 *  @param aChannel channel number
 *  @param aDevice  device number on the channel
 *  @param aDirty   whether the attachment is initially dirty or not
 */
HRESULT HardDiskAttachment::init (HardDisk *aHD, StorageBus_T aBus, LONG aChannel, LONG aDevice,
                                  BOOL aDirty)
{
    ComAssertRet (aHD, E_INVALIDARG);

    if (aBus == StorageBus_IDE)
    {
        if (aChannel < 0 || aChannel > 1)
            return setError (E_FAIL,
                tr ("Invalid IDE channel for hard disk '%ls': %d. "
                    "IDE channel number must be in range [0,1]"),
                aHD->toString().raw(), aChannel);
        if (aDevice < 0 || aDevice > 1 || (aChannel == 1 && aDevice == 0))
            return setError (E_FAIL,
                tr ("Invalid IDE device slot for hard disk '%ls': %d. "
                    "IDE device slot number must be in range [0,1] for "
                    "channel 0 and always 1 for channel 1"),
                aHD->toString().raw(), aDevice);
    }

    AutoWriteLock alock (this);

    mDirty = aDirty;

    mHardDisk = aHD;
    mBus = aBus;
    mChannel = aChannel;
    mDevice = aDevice;

    setReady (true);
    return S_OK;
}

// IHardDiskAttachment properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HardDiskAttachment::COMGETTER(HardDisk) (IHardDisk **aHardDisk)
{
    if (!aHardDisk)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    ComAssertRet (!!mHardDisk, E_FAIL);

    mHardDisk.queryInterfaceTo (aHardDisk);
    return S_OK;
}

STDMETHODIMP HardDiskAttachment::COMGETTER(Bus) (StorageBus_T *aBus)
{
    if (!aBus)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    *aBus = mBus;
    return S_OK;
}

STDMETHODIMP HardDiskAttachment::COMGETTER(Channel) (LONG *aChannel)
{
    if (!aChannel)
        return E_INVALIDARG;

    AutoWriteLock alock (this);
    CHECK_READY();

    *aChannel = mChannel;
    return S_OK;
}

STDMETHODIMP HardDiskAttachment::COMGETTER(Device) (LONG *aDevice)
{
    if (!aDevice)
        return E_INVALIDARG;

    AutoWriteLock alock (this);
    CHECK_READY();

    *aDevice = mDevice;
    return S_OK;
}

