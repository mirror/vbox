/* $Id$ */
/** @file
 * Gallium D3D testcase. Interface for D3D9 tests.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __D3D9RENDER__H
#define __D3D9RENDER__H
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "d3dhlp.h"

class D3D9DeviceProvider
{
public:
    virtual ~D3D9DeviceProvider() {}
    virtual int DeviceCount() = 0;
    virtual IDirect3DDevice9 *Device(int index) = 0;
};

class D3D9Render
{
public:
    D3D9Render() {}
    virtual ~D3D9Render() {}
    virtual int RequiredDeviceCount() { return 1; }
    virtual HRESULT InitRender(D3D9DeviceProvider *pDP) = 0;
    virtual HRESULT DoRender(D3D9DeviceProvider *pDP) = 0;
    virtual void TimeAdvance(float dt) { (void)dt; return; }
};

D3D9Render *CreateRender(int iRenderId);
void DeleteRender(D3D9Render *pRender);

#endif
