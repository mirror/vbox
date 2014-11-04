/** @file
 * shaderlib -- interface to WINE's Direct3D shader functions
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include "wined3d_private.h"

#include "shaderlib.h"

#ifdef RT_OS_WINDOWS
#define OGLGETPROCADDRESS       wglGetProcAddress
#elif RT_OS_DARWIN
#include <mach-o/dyld.h>
void *MyNSGLGetProcAddress(const char *name)
{
    NSSymbol symbol = NULL;
    char *symbolName = malloc(strlen(name) + 2);
    strcpy(symbolName + 1, name);
    symbolName[0] = '_';
    if (NSIsSymbolNameDefined(symbolName))
        symbol = NSLookupAndBindSymbol(symbolName);
    free(symbolName);
    return symbol ? NSAddressOfSymbol(symbol) : NULL;
}
#define OGLGETPROCADDRESS(x)   MyNSGLGetProcAddress((const char *)x)
#else
extern void (*glXGetProcAddress(const GLubyte *procname))( void );
#define OGLGETPROCADDRESS(x)    glXGetProcAddress((const GLubyte *)x)
#endif

#undef GL_EXT_FUNCS_GEN 
#define GL_EXT_FUNCS_GEN \
    /* GL_ARB_shader_objects */ \
    USE_GL_FUNC(WINED3D_PFNGLGETOBJECTPARAMETERIVARBPROC, \
            glGetObjectParameterivARB,                  ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETOBJECTPARAMETERFVARBPROC, \
            glGetObjectParameterfvARB,                  ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETUNIFORMLOCATIONARBPROC, \
            glGetUniformLocationARB,                    ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETACTIVEUNIFORMARBPROC, \
            glGetActiveUniformARB,                      ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM1IARBPROC, \
            glUniform1iARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM2IARBPROC, \
            glUniform2iARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM3IARBPROC, \
            glUniform3iARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM4IARBPROC, \
            glUniform4iARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM1FARBPROC, \
            glUniform1fARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM2FARBPROC, \
            glUniform2fARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM3FARBPROC, \
            glUniform3fARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM4FARBPROC, \
            glUniform4fARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM1FVARBPROC, \
            glUniform1fvARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM2FVARBPROC, \
            glUniform2fvARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM3FVARBPROC, \
            glUniform3fvARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM4FVARBPROC, \
            glUniform4fvARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM1IVARBPROC, \
            glUniform1ivARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM2IVARBPROC, \
            glUniform2ivARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM3IVARBPROC, \
            glUniform3ivARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM4IVARBPROC, \
            glUniform4ivARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORMMATRIX2FVARBPROC, \
            glUniformMatrix2fvARB,                      ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORMMATRIX3FVARBPROC, \
            glUniformMatrix3fvARB,                      ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORMMATRIX4FVARBPROC, \
            glUniformMatrix4fvARB,                      ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETUNIFORMFVARBPROC, \
            glGetUniformfvARB,                          ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETUNIFORMIVARBPROC, \
            glGetUniformivARB,                          ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETINFOLOGARBPROC, \
            glGetInfoLogARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUSEPROGRAMOBJECTARBPROC, \
            glUseProgramObjectARB,                      ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLCREATESHADEROBJECTARBPROC, \
            glCreateShaderObjectARB,                    ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLSHADERSOURCEARBPROC, \
            glShaderSourceARB,                          ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLCOMPILESHADERARBPROC, \
            glCompileShaderARB,                         ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLCREATEPROGRAMOBJECTARBPROC, \
            glCreateProgramObjectARB,                   ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLATTACHOBJECTARBPROC, \
            glAttachObjectARB,                          ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLLINKPROGRAMARBPROC, \
            glLinkProgramARB,                           ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLDETACHOBJECTARBPROC, \
            glDetachObjectARB,                          ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLDELETEOBJECTARBPROC, \
            glDeleteObjectARB,                          ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLVALIDATEPROGRAMARBPROC, \
            glValidateProgramARB,                       ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETATTACHEDOBJECTSARBPROC, \
            glGetAttachedObjectsARB,                    ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETHANDLEARBPROC, \
            glGetHandleARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETSHADERSOURCEARBPROC, \
            glGetShaderSourceARB,                       ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLBINDATTRIBLOCATIONARBPROC, \
            glBindAttribLocationARB,                    ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETATTRIBLOCATIONARBPROC, \
            glGetAttribLocationARB,                     ARB_SHADER_OBJECTS,             NULL) \

extern BOOL IWineD3DImpl_FillGLCaps(struct wined3d_adapter *adapter);

static struct wined3d_context *pCurrentContext = NULL;
static struct wined3d_adapter adapter = {0};
static bool fInitializedLibrary = false;

#define SHADER_SET_CURRENT_CONTEXT(ctx) \
    pCurrentContext = (struct wined3d_context *)ctx;

SHADERDECL(int) ShaderInitLib()
{
    struct wined3d_gl_info *gl_info = &adapter.gl_info;

#ifdef RT_OS_WINDOWS
#define USE_GL_FUNC(pfn) pfn = (void*)GetProcAddress(GetModuleHandle("opengl32.dll"), #pfn);
#else
#define USE_GL_FUNC(pfn) pfn = (void*)OGLGETPROCADDRESS(#pfn);
#endif

    /* Dynamically load all GL core functions */
    GL_FUNCS_GEN;
#undef USE_GL_FUNC

#define USE_GL_FUNC(type, pfn, ext, replace) \
{ \
    gl_info->pfn = (void*)OGLGETPROCADDRESS(#pfn); \
}
    GL_EXT_FUNCS_GEN;

    IWineD3DImpl_FillGLCaps(&adapter);
    fInitializedLibrary = true;
    return VINF_SUCCESS;
}

SHADERDECL(int) ShaderDestroyLib()
{
    return VINF_SUCCESS;
}

struct IWineD3DDeviceImpl *context_get_device(const struct wined3d_context *context)
{
    return context->pDeviceContext;
}

struct wined3d_context *context_get_current(void)
{
    return pCurrentContext;
}

struct wined3d_context *context_acquire(IWineD3DDeviceImpl *This, IWineD3DSurface *target, enum ContextUsage usage)
{
    return pCurrentContext;
}

SHADERDECL(int) ShaderContextCreate(void **ppShaderContext)
{
    struct wined3d_context *pContext;
    HRESULT hr;

    pContext = (struct wined3d_context *)RTMemAllocZ(sizeof(struct wined3d_context));
    AssertReturn(pContext, VERR_NO_MEMORY);
    pContext->pDeviceContext = (IWineD3DDeviceImpl *)RTMemAllocZ(sizeof(IWineD3DDeviceImpl));
    AssertReturn(pContext->pDeviceContext, VERR_NO_MEMORY);

    pContext->gl_info = &adapter.gl_info;

    pContext->pDeviceContext->adapter = &adapter;
    pContext->pDeviceContext->shader_backend = &glsl_shader_backend;
    pContext->pDeviceContext->ps_selected_mode = SHADER_GLSL;
    pContext->pDeviceContext->vs_selected_mode = SHADER_GLSL;
    pContext->render_offscreen = false;

    list_init(&pContext->pDeviceContext->shaders);

    if (fInitializedLibrary)
    {
        struct shader_caps shader_caps;
        uint32_t state;

        /* Initialize the shader backend. */
        hr = pContext->pDeviceContext->shader_backend->shader_alloc_private((IWineD3DDevice *)pContext->pDeviceContext);
        AssertReturn(hr == S_OK, VERR_INTERNAL_ERROR);

        memset(&shader_caps, 0, sizeof(shader_caps));
        pContext->pDeviceContext->shader_backend->shader_get_caps(&adapter.gl_info, &shader_caps);
        pContext->pDeviceContext->d3d_vshader_constantF = shader_caps.MaxVertexShaderConst;
        pContext->pDeviceContext->d3d_pshader_constantF = shader_caps.MaxPixelShaderConst;
        pContext->pDeviceContext->vs_clipping = shader_caps.VSClipping;

        pContext->pDeviceContext->stateBlock = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*pContext->pDeviceContext->stateBlock));
        AssertReturn(pContext->pDeviceContext->stateBlock, VERR_NO_MEMORY);
        hr = stateblock_init(pContext->pDeviceContext->stateBlock, pContext->pDeviceContext, 0);
        AssertReturn(hr == S_OK, VERR_INTERNAL_ERROR);
        pContext->pDeviceContext->updateStateBlock = pContext->pDeviceContext->stateBlock;

        pContext->pDeviceContext->stateBlock->vertexDecl = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IWineD3DVertexDeclarationImpl));
        AssertReturn(pContext->pDeviceContext->stateBlock->vertexDecl, VERR_NO_MEMORY);

        /* Initialize the texture unit mapping to a 1:1 mapping */
        for (state = 0; state < MAX_COMBINED_SAMPLERS; ++state)
        {
            if (state < pContext->gl_info->limits.fragment_samplers)
            {
                pContext->pDeviceContext->texUnitMap[state] = state;
                pContext->pDeviceContext->rev_tex_unit_map[state] = state;
            } else {
                pContext->pDeviceContext->texUnitMap[state] = WINED3D_UNMAPPED_STAGE;
                pContext->pDeviceContext->rev_tex_unit_map[state] = WINED3D_UNMAPPED_STAGE;
            }
        }
    }

    *ppShaderContext = (void *)pContext;
    return VINF_SUCCESS;
}

SHADERDECL(int) ShaderContextDestroy(void *pShaderContext)
{
    struct wined3d_context *pContext = (struct wined3d_context *)pShaderContext;

    if (pContext->pDeviceContext)
    {
        IWineD3DStateBlockImpl *This = pContext->pDeviceContext->stateBlock;

        /* Fails during init only. */
        if (pContext->pDeviceContext->shader_priv)
            pContext->pDeviceContext->shader_backend->shader_free_private((IWineD3DDevice *)pContext->pDeviceContext);

        if (This)
        {
            if (This->vertexShaderConstantF)
                HeapFree(GetProcessHeap(), 0, This->vertexShaderConstantF);
            if (This->changed.vertexShaderConstantsF)
                HeapFree(GetProcessHeap(), 0, This->changed.vertexShaderConstantsF);
            if (This->pixelShaderConstantF)
                HeapFree(GetProcessHeap(), 0, This->pixelShaderConstantF);
            if (This->changed.pixelShaderConstantsF)
                HeapFree(GetProcessHeap(), 0, This->changed.pixelShaderConstantsF);
            if (This->contained_vs_consts_f)
                HeapFree(GetProcessHeap(), 0, This->contained_vs_consts_f);
            if (This->contained_ps_consts_f)
                HeapFree(GetProcessHeap(), 0, This->contained_ps_consts_f);
            if (This->vertexDecl)
                HeapFree(GetProcessHeap(), 0, This->vertexDecl);
            HeapFree(GetProcessHeap(), 0, This);
        }

        RTMemFree(pContext->pDeviceContext);
    }
    RTMemFree(pShaderContext);
    return VINF_SUCCESS;
}

SHADERDECL(int) ShaderCreateVertexShader(void *pShaderContext, const uint32_t *pShaderData, void **pShaderObj)
{
    IWineD3DDeviceImpl *This;
    IWineD3DVertexShaderImpl *object;
    HRESULT hr;

    SHADER_SET_CURRENT_CONTEXT(pShaderContext);
    This = pCurrentContext->pDeviceContext;

    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object)
    {
        Log(("Failed to allocate shader memory.\n"));
        return VERR_NO_MEMORY;
    }

    hr = vertexshader_init(object, This, pShaderData, NULL, NULL, NULL);
    if (FAILED(hr))
    {
        Log(("Failed to initialize vertex shader, hr %#x.\n", hr));
        HeapFree(GetProcessHeap(), 0, object);
        return VERR_INTERNAL_ERROR;
    }

#ifdef VBOX_WINE_WITH_SHADER_CACHE
    object = vertexshader_check_cached(This, object);
#endif

    Log(("Created vertex shader %p.\n", object));
    *pShaderObj = (void *)object;

    return VINF_SUCCESS;
}

SHADERDECL(int) ShaderCreatePixelShader(void *pShaderContext, const uint32_t *pShaderData, void **pShaderObj)
{
    IWineD3DDeviceImpl *This;
    IWineD3DPixelShaderImpl *object;
    HRESULT hr;

    SHADER_SET_CURRENT_CONTEXT(pShaderContext);
    This = pCurrentContext->pDeviceContext;

    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object)
    {
        Log(("Failed to allocate shader memory.\n"));
        return VERR_NO_MEMORY;
    }

    hr = pixelshader_init(object, This, pShaderData, NULL, NULL, NULL);
    if (FAILED(hr))
    {
        Log(("Failed to initialize pixel shader, hr %#x.\n", hr));
        HeapFree(GetProcessHeap(), 0, object);
        return VERR_INTERNAL_ERROR;
    }

#ifdef VBOX_WINE_WITH_SHADER_CACHE
    object = pixelshader_check_cached(This, object);
#endif

    Log(("Created pixel shader %p.\n", object));
    *pShaderObj = (void *)object;
    return VINF_SUCCESS;
}

SHADERDECL(int) ShaderDestroyVertexShader(void *pShaderContext, void *pShaderObj)
{
    IWineD3DVertexShaderImpl *object = (IWineD3DVertexShaderImpl *)pShaderObj;
    AssertReturn(pShaderObj, VERR_INVALID_PARAMETER);

    SHADER_SET_CURRENT_CONTEXT(pShaderContext);

    object->lpVtbl->Release((IWineD3DVertexShader *)object);
	return VINF_SUCCESS;
}

SHADERDECL(int) ShaderDestroyPixelShader(void *pShaderContext, void *pShaderObj)
{
    IWineD3DPixelShaderImpl *object = (IWineD3DPixelShaderImpl *)pShaderObj;
    AssertReturn(pShaderObj, VERR_INVALID_PARAMETER);

    SHADER_SET_CURRENT_CONTEXT(pShaderContext);

    object->lpVtbl->Release((IWineD3DPixelShader *)object);
	return VINF_SUCCESS;
}

SHADERDECL(int) ShaderSetVertexShader(void *pShaderContext, void *pShaderObj)
{
    IWineD3DDeviceImpl *This;
    IWineD3DVertexShader* pShader;
    IWineD3DVertexShader* oldShader;

    SHADER_SET_CURRENT_CONTEXT(pShaderContext);
    This = pCurrentContext->pDeviceContext;
    pShader   = (IWineD3DVertexShader* )pShaderObj;
    oldShader = This->updateStateBlock->vertexShader;

    if(oldShader == pShader) {
        /* Checked here to allow proper stateblock recording */
        Log(("App is setting the old shader over, nothing to do\n"));
        return VINF_SUCCESS;
    }

    This->updateStateBlock->vertexShader         = pShader;
    This->updateStateBlock->changed.vertexShader = TRUE;

    Log(("(%p) : setting pShader(%p)\n", This, pShader));
    if(pShader) IWineD3DVertexShader_AddRef(pShader);
    if(oldShader) IWineD3DVertexShader_Release(oldShader);

    pCurrentContext->fChangedVertexShader = true;
    pCurrentContext->fChangedVertexShaderConstant = true;    /* force constant reload. */

    return VINF_SUCCESS;
}

SHADERDECL(int) ShaderSetPixelShader(void *pShaderContext, void *pShaderObj)
{
    IWineD3DDeviceImpl *This;
    IWineD3DPixelShader* pShader;
    IWineD3DPixelShader* oldShader;

    SHADER_SET_CURRENT_CONTEXT(pShaderContext);
    This = pCurrentContext->pDeviceContext;
    pShader   = (IWineD3DPixelShader* )pShaderObj;
    oldShader = This->updateStateBlock->pixelShader;

    if(oldShader == pShader) {
        /* Checked here to allow proper stateblock recording */
        Log(("App is setting the old shader over, nothing to do\n"));
        return VINF_SUCCESS;
    }

    This->updateStateBlock->pixelShader         = pShader;
    This->updateStateBlock->changed.pixelShader = TRUE;

    Log(("(%p) : setting pShader(%p)\n", This, pShader));
    if(pShader) IWineD3DPixelShader_AddRef(pShader);
    if(oldShader) IWineD3DPixelShader_Release(oldShader);

    pCurrentContext->fChangedPixelShader = true;
    pCurrentContext->fChangedPixelShaderConstant = true;    /* force constant reload. */
    return VINF_SUCCESS;
}

SHADERDECL(int) ShaderSetVertexShaderConstantB(void *pShaderContext, uint32_t start, const uint8_t *srcData, uint32_t count)
{
    IWineD3DDeviceImpl *This;
    unsigned int i, cnt = min(count, MAX_CONST_B - start);

    SHADER_SET_CURRENT_CONTEXT(pShaderContext);
    This = pCurrentContext->pDeviceContext;

    Log(("(ShaderSetVertexShaderConstantB %p, srcData %p, start %d, count %d)\n",
            srcData, start, count));

    if (!srcData || start >= MAX_CONST_B)
    {
        Log(("incorrect vertex shader const data: start(%u), srcData(0x%p), count(%u)", start, srcData, count));
        return VERR_INVALID_PARAMETER;
    }

    memcpy(&This->updateStateBlock->vertexShaderConstantB[start], srcData, cnt * sizeof(BOOL));
    for (i = 0; i < cnt; i++)
        Log(("Set BOOL constant %u to %s\n", start + i, srcData[i]? "true":"false"));

    for (i = start; i < cnt + start; ++i) {
        This->updateStateBlock->changed.vertexShaderConstantsB |= (1 << i);
    }

    pCurrentContext->fChangedVertexShaderConstant = true;

    return VINF_SUCCESS;
}

SHADERDECL(int) ShaderSetVertexShaderConstantI(void *pShaderContext, uint32_t start, const int32_t *srcData, uint32_t count)
{
    IWineD3DDeviceImpl *This;
    unsigned int i, cnt = min(count, MAX_CONST_I - start);

    SHADER_SET_CURRENT_CONTEXT(pShaderContext);
    This = pCurrentContext->pDeviceContext;

    Log(("(ShaderSetVertexShaderConstantI %p, srcData %p, start %d, count %d)\n",
            srcData, start, count));

    if (!srcData || start >= MAX_CONST_I)
    {
        Log(("incorrect vertex shader const data: start(%u), srcData(0x%p), count(%u)", start, srcData, count));
        return VERR_INVALID_PARAMETER;
    }

    memcpy(&This->updateStateBlock->vertexShaderConstantI[start * 4], srcData, cnt * sizeof(int32_t) * 4);

    for (i = start; i < cnt + start; ++i) {
        This->updateStateBlock->changed.vertexShaderConstantsI |= (1 << i);
    }

    pCurrentContext->fChangedVertexShaderConstant = true;

    return VINF_SUCCESS;
}

SHADERDECL(int) ShaderSetVertexShaderConstantF(void *pShaderContext, uint32_t start, const float *srcData, uint32_t count)
{
    IWineD3DDeviceImpl *This;

    SHADER_SET_CURRENT_CONTEXT(pShaderContext);
    This = pCurrentContext->pDeviceContext;

    Log(("(ShaderSetVertexShaderConstantF %p, srcData %p, start %d, count %d)\n",
            srcData, start, count));

    if (srcData == NULL || start + count > This->d3d_vshader_constantF || start > This->d3d_vshader_constantF)
    {
        Log(("incorrect vertex shader const data: start(%u), srcData(0x%p), count(%u)", start, srcData, count));
        return VERR_INVALID_PARAMETER;
    }
    memcpy(&This->updateStateBlock->vertexShaderConstantF[start * 4], srcData, count * sizeof(float) * 4);

    This->shader_backend->shader_update_float_vertex_constants((IWineD3DDevice *)This, start, count);

    memset(This->updateStateBlock->changed.vertexShaderConstantsF + start, 1,
           sizeof(*This->updateStateBlock->changed.vertexShaderConstantsF) * count);

    pCurrentContext->fChangedVertexShaderConstant = true;

    return VINF_SUCCESS;
}

SHADERDECL(int) ShaderSetPixelShaderConstantB(void *pShaderContext, uint32_t start, const uint8_t *srcData, uint32_t count)
{
    IWineD3DDeviceImpl *This;
    unsigned int i, cnt = min(count, MAX_CONST_B - start);

    SHADER_SET_CURRENT_CONTEXT(pShaderContext);
    This = pCurrentContext->pDeviceContext;

    Log(("(ShaderSetPixelShaderConstantB %p, srcData %p, start %d, count %d)\n",
            srcData, start, count));

    if (!srcData || start >= MAX_CONST_B)
    {
        Log(("incorrect pixel shader const data: start(%u), srcData(0x%p), count(%u)", start, srcData, count));
        return VERR_INVALID_PARAMETER;
    }

    memcpy(&This->updateStateBlock->pixelShaderConstantB[start], srcData, cnt * sizeof(BOOL));
    for (i = 0; i < cnt; i++)
        Log(("Set BOOL constant %u to %s\n", start + i, srcData[i]? "true":"false"));

    for (i = start; i < cnt + start; ++i) {
        This->updateStateBlock->changed.pixelShaderConstantsB |= (1 << i);
    }

    pCurrentContext->fChangedPixelShaderConstant = true;

    return VINF_SUCCESS;
}

SHADERDECL(int) ShaderSetPixelShaderConstantI(void *pShaderContext, uint32_t start, const int32_t *srcData, uint32_t count)
{
    IWineD3DDeviceImpl *This;
    unsigned int i, cnt = min(count, MAX_CONST_I - start);

    SHADER_SET_CURRENT_CONTEXT(pShaderContext);
    This = pCurrentContext->pDeviceContext;

    Log(("(ShaderSetPixelShaderConstantI %p, srcData %p, start %d, count %d)\n",
            srcData, start, count));

    if (!srcData || start >= MAX_CONST_I)
    {
        Log(("incorrect pixel shader const data: start(%u), srcData(0x%p), count(%u)", start, srcData, count));
        return VERR_INVALID_PARAMETER;
    }

    memcpy(&This->updateStateBlock->pixelShaderConstantI[start * 4], srcData, cnt * sizeof(int32_t) * 4);

    for (i = start; i < cnt + start; ++i) {
        This->updateStateBlock->changed.pixelShaderConstantsI |= (1 << i);
    }

    pCurrentContext->fChangedPixelShaderConstant = true;

    return VINF_SUCCESS;
}

SHADERDECL(int) ShaderSetPixelShaderConstantF(void *pShaderContext, uint32_t start, const float *srcData, uint32_t count)
{
    IWineD3DDeviceImpl *This;

    SHADER_SET_CURRENT_CONTEXT(pShaderContext);
    This = pCurrentContext->pDeviceContext;

    Log(("(ShaderSetPixelShaderConstantF %p, srcData %p, start %d, count %d)\n",
            srcData, start, count));

    if (srcData == NULL || start + count > This->d3d_pshader_constantF || start > This->d3d_pshader_constantF)
    {
        Log(("incorrect pixel shader const data: start(%u), srcData(0x%p), count(%u)", start, srcData, count));
        return VERR_INVALID_PARAMETER;
    }

    memcpy(&This->updateStateBlock->pixelShaderConstantF[start * 4], srcData, count * sizeof(float) * 4);

    This->shader_backend->shader_update_float_pixel_constants((IWineD3DDevice *)This, start, count);

    memset(This->updateStateBlock->changed.pixelShaderConstantsF + start, 1,
            sizeof(*This->updateStateBlock->changed.pixelShaderConstantsF) * count);

    pCurrentContext->fChangedPixelShaderConstant = true;

    return VINF_SUCCESS;
}

SHADERDECL(int) ShaderUpdateState(void *pShaderContext, uint32_t rtHeight)
{
    IWineD3DDeviceImpl *pThis;
    GLfloat yoffset;
    GLint viewport[4];

    SHADER_SET_CURRENT_CONTEXT(pShaderContext);
    pThis = pCurrentContext->pDeviceContext;

    glGetIntegerv(GL_VIEWPORT, viewport);
#ifdef DEBUG
    AssertReturn(glGetError() == GL_NO_ERROR, VERR_INTERNAL_ERROR);
#endif

    yoffset = -(63.0f / 64.0f) / viewport[3] /* height */;
    pThis->posFixup[0] = 1.0f;  /* This is needed to get the x coord unmodified through a MAD. */
    pThis->posFixup[1] = -1.0f;  /* y-inversion */
    pThis->posFixup[2] = (63.0f / 64.0f) / viewport[2] /* width */;
    pThis->posFixup[3] = pThis->posFixup[1] * yoffset;

    pThis->rtHeight = rtHeight;

    /* @todo missing state:
     * - fog enable (stateblock->renderState[WINED3DRS_FOGENABLE])
     * - fog mode (stateblock->renderState[WINED3DRS_FOGTABLEMODE])
     * - stateblock->vertexDecl->position_transformed
     */

    if (    pCurrentContext->fChangedPixelShader 
        ||  pCurrentContext->fChangedVertexShader)
        pThis->shader_backend->shader_select(pCurrentContext, !!pThis->updateStateBlock->pixelShader, !!pThis->updateStateBlock->vertexShader);
    pCurrentContext->fChangedPixelShader = pCurrentContext->fChangedVertexShader = false;

    if (    pCurrentContext->fChangedPixelShaderConstant 
        ||  pCurrentContext->fChangedVertexShaderConstant)
        pThis->shader_backend->shader_load_constants(pCurrentContext, !!pThis->updateStateBlock->pixelShader, !!pThis->updateStateBlock->vertexShader);
    pCurrentContext->fChangedPixelShaderConstant  = false;
    pCurrentContext->fChangedVertexShaderConstant = false;

    return VINF_SUCCESS;
}

SHADERDECL(int) ShaderTransformProjection(unsigned cxViewPort, unsigned cyViewPort, float matrix[16])
{
#ifdef DEBUG
    GLenum lastError;
#endif
    GLfloat xoffset, yoffset;

    /* Assumes OpenGL context has been activated. */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    /* The rule is that the window coordinate 0 does not correspond to the
       beginning of the first pixel, but the center of the first pixel.
       As a consequence if you want to correctly draw one line exactly from
       the left to the right end of the viewport (with all matrices set to
       be identity), the x coords of both ends of the line would be not
       -1 and 1 respectively but (-1-1/viewport_widh) and (1-1/viewport_width)
       instead.

       1.0 / Width is used because the coord range goes from -1.0 to 1.0, then we
       divide by the Width/Height, so we need the half range(1.0) to translate by
       half a pixel.

       The other fun is that d3d's output z range after the transformation is [0;1],
       but opengl's is [-1;1]. Since the z buffer is in range [0;1] for both, gl
       scales [-1;1] to [0;1]. This would mean that we end up in [0.5;1] and loose a lot
       of Z buffer precision and the clear values do not match in the z test. Thus scale
       [0;1] to [-1;1], so when gl undoes that we utilize the full z range
    */

    /*
     * Careful with the order of operations here, we're essentially working backwards:
     * x = x + 1/w;
     * y = (y - 1/h) * flip;
     * z = z * 2 - 1;
     *
     * Becomes:
     * glTranslatef(0.0, 0.0, -1.0);
     * glScalef(1.0, 1.0, 2.0);
     *
     * glScalef(1.0, flip, 1.0);
     * glTranslatef(1/w, -1/h, 0.0);
     *
     * This is equivalent to:
     * glTranslatef(1/w, -flip/h, -1.0)
     * glScalef(1.0, flip, 2.0);
     */
    /* Translate by slightly less than a half pixel to force a top-left
     * filling convention. We want the difference to be large enough that
     * it doesn't get lost due to rounding inside the driver, but small
     * enough to prevent it from interfering with any anti-aliasing. */
    xoffset = (63.0f / 64.0f) / cxViewPort;
    yoffset = -(63.0f / 64.0f) / cyViewPort;

    glTranslatef(xoffset, -yoffset, -1.0f);
    /* flip y coordinate origin too */
    glScalef(1.0f, -1.0f, 2.0f);

    glMultMatrixf(matrix);

#ifdef DEBUG
    lastError = glGetError();                                     \
    AssertMsgReturn(lastError == GL_NO_ERROR, ("%s (%d): last error 0x%x\n", __FUNCTION__, __LINE__, lastError), VERR_INTERNAL_ERROR);
#endif
    return VINF_SUCCESS;
}
