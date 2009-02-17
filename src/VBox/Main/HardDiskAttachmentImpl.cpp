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

#include "HardDiskAttachmentImpl.h"

#include "Logging.h"

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT HardDiskAttachment::FinalConstruct()
{
    return S_OK;
}

void HardDiskAttachment::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the hard disk attachment object.
 *
 * @param aHD       Hard disk object.
 * @param aBus      Bus type.
 * @param aChannel  Channel number.
 * @param aDevice   Device number on the channel.
 * @param aImplicit Wether the attachment contains an implicitly created diff.
 */
HRESULT HardDiskAttachment::init(HardDisk *aHD, StorageBus_T aBus, LONG aChannel,
                                 LONG aDevice, bool aImplicit /*= false*/)
{
    AssertReturn (aHD, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    m.hardDisk = aHD;
    unconst (m.bus) = aBus;
    unconst (m.channel) = aChannel;
    unconst (m.device) = aDevice;

    m.implicit = aImplicit;

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void HardDiskAttachment::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;
}

// IHardDiskAttachment properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HardDiskAttachment::COMGETTER(HardDisk) (IHardDisk **aHardDisk)
{
    CheckComArgOutPointerValid(aHardDisk);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    m.hardDisk.queryInterfaceTo (aHardDisk);

    return S_OK;
}

STDMETHODIMP HardDiskAttachment::COMGETTER(Bus) (StorageBus_T *aBus)
{
    CheckComArgOutPointerValid(aBus);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* m.bus is constant during life time, no need to lock */
    *aBus = m.bus;

    return S_OK;
}

STDMETHODIMP HardDiskAttachment::COMGETTER(Channel) (LONG *aChannel)
{
    CheckComArgOutPointerValid(aChannel);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* m.channel is constant during life time, no need to lock */
    *aChannel = m.channel;

    return S_OK;
}

STDMETHODIMP HardDiskAttachment::COMGETTER(Device) (LONG *aDevice)
{
    CheckComArgOutPointerValid(aDevice);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* m.device is constant during life time, no need to lock */
    *aDevice = m.device;

    return S_OK;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
