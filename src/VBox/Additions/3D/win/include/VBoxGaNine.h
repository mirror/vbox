/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver interface for WDDM user mode driver.
 */

/*
 * Copyright (C) 2016-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxGaNine_h__
#define ___VBoxGaNine_h__
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/win/d3d9.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pipe_screen;
struct pipe_resource;
struct pipe_context;
typedef struct ID3DAdapter9 ID3DAdapter9;

typedef HRESULT WINAPI FNGaNineD3DAdapter9Create(struct pipe_screen *s, ID3DAdapter9 **ppOut);
typedef FNGaNineD3DAdapter9Create *PFNGaNineD3DAdapter9Create;

typedef struct pipe_resource * WINAPI FNGaNinePipeResourceFromSurface(IUnknown *pSurface);
typedef FNGaNinePipeResourceFromSurface *PFNGaNinePipeResourceFromSurface;

typedef struct pipe_context * WINAPI FNGaNinePipeContextFromDevice(IDirect3DDevice9 *pDevice);
typedef FNGaNinePipeContextFromDevice *PFNGaNinePipeContextFromDevice;

#ifdef __cplusplus
}
#endif

#endif
