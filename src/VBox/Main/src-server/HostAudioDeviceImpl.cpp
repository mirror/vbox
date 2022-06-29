/* $Id$ */
/** @file
 * VirtualBox COM class implementation - Host audio device implementation.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_MAIN_HOSTAUDIODEVICE
#include "HostAudioDeviceImpl.h"
#include "VirtualBoxImpl.h"

#include <iprt/cpp/utils.h>

#include <VBox/settings.h>

#include "AutoCaller.h"
#include "LoggingNew.h"


// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

HostAudioDevice::HostAudioDevice()
{
}

HostAudioDevice::~HostAudioDevice()
{
}

HRESULT HostAudioDevice::FinalConstruct()
{
    return BaseFinalConstruct();
}

void HostAudioDevice::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}


// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the audio adapter object.
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT HostAudioDevice::init(void)
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void HostAudioDevice::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
}


// IHostAudioDevice properties
////////////////////////////////////////////////////////////////////////////////


