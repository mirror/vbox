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

#ifndef MAIN_INCLUDED_HostAudioDeviceImpl_h
#define MAIN_INCLUDED_HostAudioDeviceImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "HostAudioDeviceWrap.h"

class ATL_NO_VTABLE HostAudioDevice :
    public HostAudioDeviceWrap
{
public:

    DECLARE_COMMON_CLASS_METHODS(HostAudioDevice)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init();
    void uninit();

private:

    // wrapped IHostAudioDevice properties

};

#endif /* !MAIN_INCLUDED_HostAudioDeviceImpl_h */

