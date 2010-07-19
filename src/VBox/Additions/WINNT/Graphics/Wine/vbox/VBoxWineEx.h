#ifndef ___VBoxWineEx_h__
#define ___VBoxWineEx_h__

#ifndef IN_VBOXWINEEX
#  define VBOXWINEEX_DECL(_type) __declspec(dllimport) _type WINAPI
# else
#  define VBOXWINEEX_DECL(_type) __declspec(dllexport) _type WINAPI
#endif

typedef VBOXWINEEX_DECL(HRESULT) FNVBOXWINEEXD3DDEV9_CREATETEXTURE(IDirect3DDevice9Ex *iface,
            UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format,
            D3DPOOL pool, IDirect3DTexture9 **texture, HANDLE *shared_handle,
            void *pvClientMem);
typedef FNVBOXWINEEXD3DDEV9_CREATETEXTURE *PFNVBOXWINEEXD3DDEV9_CREATETEXTURE;

#ifdef __cplusplus
extern "C"
{
#endif
VBOXWINEEX_DECL(HRESULT) VBoxWineExD3DDev9CreateTexture(IDirect3DDevice9Ex *iface,
            UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format,
            D3DPOOL pool, IDirect3DTexture9 **texture, HANDLE *shared_handle,
            void *pvClientMem); /* <- extension arg to pass in the client memory buffer,
                                 *    applicable ONLY for SYSMEM textures */
#ifdef __cplusplus
}
#endif

#endif
