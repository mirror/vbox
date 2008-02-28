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
    mController = DiskControllerType_Null;
    mDeviceNumber = 0;

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
 *  @param aCtl     controller type
 *  @param aDev     device number on the controller
 *  @param aDirty   whether the attachment is initially dirty or not
 */
HRESULT HardDiskAttachment::init (HardDisk *aHD, DiskControllerType_T aCtl, LONG aDev,
                                  BOOL aDirty)
{
    ComAssertRet (aHD, E_INVALIDARG);

    AutoLock alock (this);

    mDirty = aDirty;

    mHardDisk = aHD;
    mController = aCtl;
    mDeviceNumber = aDev;

    setReady (true);
    return S_OK;
}

// IHardDiskAttachment properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HardDiskAttachment::COMGETTER(HardDisk) (IHardDisk **aHardDisk)
{
    if (!aHardDisk)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    ComAssertRet (!!mHardDisk, E_FAIL);

    mHardDisk.queryInterfaceTo (aHardDisk);
    return S_OK;
}

STDMETHODIMP HardDiskAttachment::COMGETTER(Controller) (DiskControllerType_T *aController)
{
    if (!aController)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aController = mController;
    return S_OK;
}

STDMETHODIMP HardDiskAttachment::COMGETTER(DeviceNumber) (LONG *aDeviceNumber)
{
    if (!aDeviceNumber)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    *aDeviceNumber = mDeviceNumber;
    return S_OK;
}

