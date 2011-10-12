/** @file
 *
 * VBox extension to Wine D3D
 *
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___VBoxWineEx_h__
#define ___VBoxWineEx_h__

#define VBOXWINEEX_VERSION 1

#ifndef IN_VBOXWINEEX
# define VBOXWINEEX_DECL(_type)   __declspec(dllimport) _type WINAPI
# else
# define VBOXWINEEX_DECL(_type)  __declspec(dllexport) _type WINAPI
#endif

typedef VBOXWINEEX_DECL(HRESULT) FNVBOXWINEEXD3DDEV9_CREATETEXTURE(IDirect3DDevice9Ex *iface,
            UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format,
            D3DPOOL pool, IDirect3DTexture9 **texture, HANDLE *shared_handle,
            void **pavClientMem);
typedef FNVBOXWINEEXD3DDEV9_CREATETEXTURE *PFNVBOXWINEEXD3DDEV9_CREATETEXTURE;

typedef VBOXWINEEX_DECL(HRESULT) FNVBOXWINEEXD3DDEV9_CREATECUBETEXTURE(IDirect3DDevice9Ex *iface,
            UINT edge_length, UINT levels, DWORD usage, D3DFORMAT format, 
            D3DPOOL pool, IDirect3DCubeTexture9 **texture, HANDLE *shared_handle,
            void **pavClientMem);
typedef FNVBOXWINEEXD3DDEV9_CREATECUBETEXTURE *PFNVBOXWINEEXD3DDEV9_CREATECUBETEXTURE;

typedef VBOXWINEEX_DECL(HRESULT) FNVBOXWINEEXD3DDEV9_FLUSH(IDirect3DDevice9Ex *iface);
typedef FNVBOXWINEEXD3DDEV9_FLUSH *PFNVBOXWINEEXD3DDEV9_FLUSH;

typedef VBOXWINEEX_DECL(HRESULT) FNVBOXWINEEXD3DDEV9_UPDATE(IDirect3DDevice9Ex *iface, D3DPRESENT_PARAMETERS * pParams, IDirect3DDevice9Ex **outIface);
typedef FNVBOXWINEEXD3DDEV9_UPDATE *PFNVBOXWINEEXD3DDEV9_UPDATE;

typedef VBOXWINEEX_DECL(HRESULT) FNVBOXWINEEXD3DRC9_SETDONTDELETEGL(IDirect3DResource9 *iface);
typedef FNVBOXWINEEXD3DRC9_SETDONTDELETEGL *PFNVBOXWINEEXD3DRC9_SETDONTDELETEGL;

typedef VBOXWINEEX_DECL(HRESULT) FNVBOXWINEEXD3DSWAPCHAIN9_PRESENT(IDirect3DSwapChain9 *iface, IDirect3DSurface9 *surf);
typedef FNVBOXWINEEXD3DSWAPCHAIN9_PRESENT *PFNVBOXWINEEXD3DSWAPCHAIN9_PRESENT;

#ifdef __cplusplus
extern "C"
{
#endif
VBOXWINEEX_DECL(HRESULT) VBoxWineExD3DDev9CreateTexture(IDirect3DDevice9Ex *iface,
            UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format,
            D3DPOOL pool, IDirect3DTexture9 **texture, HANDLE *shared_handle,
            void **pavClientMem); /* <- extension arg to pass in the client memory buffer,
                                 *    applicable ONLY for SYSMEM textures */

VBOXWINEEX_DECL(HRESULT) VBoxWineExD3DDev9CreateCubeTexture(IDirect3DDevice9Ex *iface,
            UINT edge_length, UINT levels, DWORD usage, D3DFORMAT format,
            D3DPOOL pool, IDirect3DCubeTexture9 **texture, HANDLE *shared_handle,
            void **pavClientMem); /* <- extension arg to pass in the client memory buffer,
                                 *    applicable ONLY for SYSMEM textures */

VBOXWINEEX_DECL(HRESULT) VBoxWineExD3DDev9Flush(IDirect3DDevice9Ex *iface); /* perform glFlush */

VBOXWINEEX_DECL(HRESULT) VBoxWineExD3DDev9Update(IDirect3DDevice9Ex *iface, D3DPRESENT_PARAMETERS * pParams,
                                                    IDirect3DDevice9Ex **outIface); /* update device parameters */

VBOXWINEEX_DECL(HRESULT) VBoxWineExD3DRc9SetDontDeleteGl(IDirect3DResource9 *iface);

VBOXWINEEX_DECL(HRESULT) VBoxWineExD3DSwapchain9Present(IDirect3DSwapChain9 *iface,
                                IDirect3DSurface9 *surf); /* use the given surface as a frontbuffer content source */
#ifdef __cplusplus
}
#endif

#endif
