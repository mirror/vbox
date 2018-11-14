/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium WDDM user mode driver functions.
 */

/*
 * Copyright (C) 2017-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___GaDdi_h__
#define ___GaDdi_h__

#include <d3dumddi.h>

/*
 * PFND3DDDI_* functions implemented for the Gallium based driver.
 */
HRESULT APIENTRY GaDdiDrawPrimitive(HANDLE hDevice, const D3DDDIARG_DRAWPRIMITIVE *pData, const UINT *pFlagBuffer);
HRESULT APIENTRY GaDdiDrawIndexedPrimitive(HANDLE hDevice, const D3DDDIARG_DRAWINDEXEDPRIMITIVE *pData);
HRESULT APIENTRY GaDdiDrawPrimitive2(HANDLE hDevice, const D3DDDIARG_DRAWPRIMITIVE2 *pData);
HRESULT APIENTRY GaDdiDrawIndexedPrimitive2(HANDLE hDevice, const D3DDDIARG_DRAWINDEXEDPRIMITIVE2 *pData,
                                            UINT dwIndicesSize, const VOID *pIndexBuffer, const UINT *pFlagBuffer);
HRESULT APIENTRY GaDdiBlt(HANDLE hDevice, const D3DDDIARG_BLT *pData);
HRESULT APIENTRY GaDdiTexBlt(HANDLE hDevice, const D3DDDIARG_TEXBLT *pData);
HRESULT APIENTRY GaDdiVolBlt(HANDLE hDevice, const D3DDDIARG_VOLUMEBLT *pData);
HRESULT APIENTRY GaDdiFlush(HANDLE hDevice);
HRESULT APIENTRY GaDdiPresent(HANDLE hDevice, const D3DDDIARG_PRESENT *pData);
HRESULT APIENTRY GaDdiLock(HANDLE hDevice, D3DDDIARG_LOCK *pData);
HRESULT APIENTRY GaDdiUnlock(HANDLE hDevice, const D3DDDIARG_UNLOCK *pData);
HRESULT APIENTRY GaDdiCreateVertexShaderFunc(HANDLE hDevice, D3DDDIARG_CREATEVERTEXSHADERFUNC *pData, const UINT *pCode);
HRESULT APIENTRY GaDdiCreatePixelShader(HANDLE hDevice, D3DDDIARG_CREATEPIXELSHADER *pData, const UINT *pCode);
HRESULT APIENTRY GaDdiCreateResource(HANDLE hDevice, D3DDDIARG_CREATERESOURCE *pResource);
HRESULT APIENTRY GaDdiDestroyResource(HANDLE hDevice, HANDLE hResource);
HRESULT APIENTRY GaDdiOpenResource(HANDLE hDevice, D3DDDIARG_OPENRESOURCE *pResource);

#endif
