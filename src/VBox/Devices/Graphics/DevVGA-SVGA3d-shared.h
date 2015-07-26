/* $Id$ */
/** @file
 * DevVMWare - VMWare SVGA device, shared part.
 *
 * @remarks This is code that's very similar on both for OpenGL and D3D
 *          backends, so instead of moving backend specific structures
 *          into header files with #ifdefs and stuff, we just include
 *          the code into the backend implementation source file.
 */

/*
 * Copyright (C) 2013-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___DevVGA_SVGA3d_shared_h___
#define ___DevVGA_SVGA3d_shared_h___

/**
 * Worker for vmsvga3dUpdateHeapBuffersForSurfaces.
 *
 * This will allocate heap buffers if necessary, thus increasing the memory
 * usage of the process.
 *
 * @todo Would be interesting to share this code with the saved state code.
 *
 * @returns VBox status code.
 * @param   pState              The 3D state structure.
 * @param   pSurface            The surface to refresh the heap buffers for.
 */
static int vmsvga3dSurfaceUpdateHeapBuffers(PVMSVGA3DSTATE pState, PVMSVGA3DSURFACE pSurface)
{
    /*
     * Currently we've got trouble retreving bit for DEPTHSTENCIL
     * surfaces both for OpenGL and D3D, so skip these here (don't
     * wast memory on them).
     */
    uint32_t const fSwitchFlags = pSurface->flags
                                & (  SVGA3D_SURFACE_HINT_INDEXBUFFER  | SVGA3D_SURFACE_HINT_VERTEXBUFFER
                                   | SVGA3D_SURFACE_HINT_TEXTURE      | SVGA3D_SURFACE_HINT_RENDERTARGET
                                   | SVGA3D_SURFACE_HINT_DEPTHSTENCIL | SVGA3D_SURFACE_CUBEMAP);
    if (   fSwitchFlags != SVGA3D_SURFACE_HINT_DEPTHSTENCIL
        && fSwitchFlags != (SVGA3D_SURFACE_HINT_DEPTHSTENCIL | SVGA3D_SURFACE_HINT_TEXTURE))
    {

#ifdef VMSVGA3D_OPENGL
        /*
         * Change OpenGL context to the one the surface is associated with.
         */
# ifdef VMSVGA3D_OGL_WITH_SHARED_CTX
        PVMSVGA3DCONTEXT pContext = &pState->SharedCtx;
        VMSVGA3D_SET_CURRENT_CONTEXT(pState, pContext);
# else
        /** @todo stricter checks for associated context */
        uint32_t cid = pSurface->idAssociatedContext;
        if (    cid >= pState->cContexts
            ||  pState->papContexts[cid]->id != cid)
        {
            Log(("vmsvga3dSurfaceUpdateHeapBuffers: invalid context id (%x - %x)!\n", cid, (cid >= pState->cContexts) ? -1 : pState->papContexts[cid]->id));
            AssertFailedReturn(VERR_INVALID_PARAMETER);
        }
        PVMSVGA3DCONTEXT pContext = pState->papContexts[cid];
        VMSVGA3D_SET_CURRENT_CONTEXT(pState, pContext);
# endif
#endif

        /*
         * Work thru each mipmap level for each face.
         */
        for (uint32_t iFace = 0; iFace < pSurface->cFaces; iFace++)
        {
            Assert(pSurface->faces[iFace].numMipLevels <= pSurface->faces[0].numMipLevels);
            PVMSVGA3DMIPMAPLEVEL pMipmapLevel = &pSurface->pMipmapLevels[iFace * pSurface->faces[0].numMipLevels];
            for (uint32_t i = 0; i < pSurface->faces[iFace].numMipLevels; i++, pMipmapLevel++)
            {
#ifdef VMSVGA3D_DIRECT3D
                if (pSurface->u.pSurface)
#else
                if (pSurface->oglId.texture != OPENGL_INVALID_ID)
#endif
                {
                    Assert(pMipmapLevel->cbSurface);
                    Assert(pMipmapLevel->cbSurface == pMipmapLevel->cbSurfacePitch * pMipmapLevel->size.height); /* correct for depth stuff? */

                    /*
                     * Make sure we've got surface memory buffer.
                     */
                    uint8_t *pbDst = (uint8_t *)pMipmapLevel->pSurfaceData;
                    if (!pbDst)
                    {
                        pMipmapLevel->pSurfaceData = pbDst = (uint8_t *)RTMemAllocZ(pMipmapLevel->cbSurface);
                        AssertReturn(pbDst, VERR_NO_MEMORY);
                    }

#ifdef VMSVGA3D_DIRECT3D
                    /*
                     * D3D specifics.
                     */
                    HRESULT  hr;
                    switch (fSwitchFlags)
                    {
                        case SVGA3D_SURFACE_HINT_TEXTURE:
                        case SVGA3D_SURFACE_HINT_RENDERTARGET:
                        case SVGA3D_SURFACE_HINT_TEXTURE | SVGA3D_SURFACE_HINT_RENDERTARGET:
                        {
                            /*
                             * Lock the buffer and make it accessible to memcpy.
                             */
                            D3DLOCKED_RECT LockedRect;
                            if (fSwitchFlags & SVGA3D_SURFACE_HINT_TEXTURE)
                            {
                                if (pSurface->bounce.pTexture)
                                {
                                    if (    !pSurface->fDirty
                                        &&  fSwitchFlags == (SVGA3D_SURFACE_HINT_TEXTURE | SVGA3D_SURFACE_HINT_RENDERTARGET)
                                        &&  i == 0 /* only the first time */)
                                    {
                                        /** @todo stricter checks for associated context */
                                        uint32_t cid = pSurface->idAssociatedContext;
                                        if (   cid >= pState->cContexts
                                            || pState->papContexts[cid]->id != cid)
                                        {
                                            Log(("vmsvga3dSurfaceUpdateHeapBuffers: invalid context id (%x - %x)!\n", cid, (cid >= pState->cContexts) ? -1 : pState->papContexts[cid]->id));
                                            AssertFailedReturn(VERR_INVALID_PARAMETER);
                                        }
                                        PVMSVGA3DCONTEXT pContext = pState->papContexts[cid];

                                        IDirect3DSurface9 *pDst = NULL;
                                        hr = pSurface->bounce.pTexture->GetSurfaceLevel(i, &pDst);
                                        AssertMsgReturn(hr == D3D_OK, ("GetSurfaceLevel failed with %#x\n", hr), VERR_INTERNAL_ERROR);

                                        IDirect3DSurface9 *pSrc = NULL;
                                        hr = pSurface->u.pTexture->GetSurfaceLevel(i, &pSrc);
                                        AssertMsgReturn(hr == D3D_OK, ("GetSurfaceLevel failed with %#x\n", hr), VERR_INTERNAL_ERROR);

                                        hr = pContext->pDevice->GetRenderTargetData(pSrc, pDst);
                                        AssertMsgReturn(hr == D3D_OK, ("GetRenderTargetData failed with %#x\n", hr), VERR_INTERNAL_ERROR);

                                        pSrc->Release();
                                        pDst->Release();
                                    }

                                    hr = pSurface->bounce.pTexture->LockRect(i, /* texture level */
                                                                             &LockedRect,
                                                                             NULL,
                                                                             D3DLOCK_READONLY);
                                }
                                else
                                    hr = pSurface->u.pTexture->LockRect(i, /* texture level */
                                                                        &LockedRect,
                                                                        NULL,
                                                                        D3DLOCK_READONLY);
                            }
                            else
                                hr = pSurface->u.pSurface->LockRect(&LockedRect,
                                                                    NULL,
                                                                    D3DLOCK_READONLY);
                            AssertMsgReturn(hr == D3D_OK, ("LockRect failed with %x\n", hr), VERR_INTERNAL_ERROR);

                            /*
                             * Copy the data.  Take care in case the pitch differs.
                             */
                            if (pMipmapLevel->cbSurfacePitch == (uint32_t)LockedRect.Pitch)
                                memcpy(pbDst, LockedRect.pBits, pMipmapLevel->cbSurface);
                            else
                                for (uint32_t j = 0; j < pMipmapLevel->size.height; j++)
                                    memcpy(pbDst + j * pMipmapLevel->cbSurfacePitch,
                                           (uint8_t *)LockedRect.pBits + j * LockedRect.Pitch,
                                           pMipmapLevel->cbSurfacePitch);

                            /*
                             * Release the buffer.
                             */
                            if (fSwitchFlags & SVGA3D_SURFACE_HINT_TEXTURE)
                            {
                                if (pSurface->bounce.pTexture)
                                {
                                    hr = pSurface->bounce.pTexture->UnlockRect(i);
                                    AssertMsgReturn(hr == D3D_OK, ("UnlockRect failed with %#x\n", hr), VERR_INTERNAL_ERROR);
                                }
                                else
                                    hr = pSurface->u.pTexture->UnlockRect(i);
                            }
                            else
                                hr = pSurface->u.pSurface->UnlockRect();
                            AssertMsgReturn(hr == D3D_OK, ("UnlockRect failed with %#x\n", hr), VERR_INTERNAL_ERROR);
                            break;
                        }

                        case SVGA3D_SURFACE_HINT_VERTEXBUFFER:
                        {
                            void *pvD3DData = NULL;
                            hr = pSurface->u.pVertexBuffer->Lock(0, 0, &pvD3DData, D3DLOCK_READONLY);
                            AssertMsgReturn(hr == D3D_OK, ("Lock vertex failed with %x\n", hr), VERR_INTERNAL_ERROR);

                            memcpy(pbDst, pvD3DData, pMipmapLevel->cbSurface);

                            hr = pSurface->u.pVertexBuffer->Unlock();
                            AssertMsg(hr == D3D_OK, ("Unlock vertex failed with %x\n", hr));
                            break;
                        }

                        case SVGA3D_SURFACE_HINT_INDEXBUFFER:
                        {
                            void *pvD3DData = NULL;
                            hr = pSurface->u.pIndexBuffer->Lock(0, 0, &pvD3DData, D3DLOCK_READONLY);
                            AssertMsgReturn(hr == D3D_OK, ("Lock index failed with %x\n", hr), VERR_INTERNAL_ERROR);

                            memcpy(pbDst, pvD3DData, pMipmapLevel->cbSurface);

                            hr = pSurface->u.pIndexBuffer->Unlock();
                            AssertMsg(hr == D3D_OK, ("Unlock index failed with %x\n", hr));
                            break;
                        }

                        default:
                            AssertMsgFailed(("%#x\n", fSwitchFlags));
                    }

#elif defined(VMSVGA3D_OPENGL)
                    /*
                     * OpenGL specifics.
                     */
                    switch (fSwitchFlags)
                    {
                        case SVGA3D_SURFACE_HINT_TEXTURE:
                        case SVGA3D_SURFACE_HINT_RENDERTARGET:
                        case SVGA3D_SURFACE_HINT_TEXTURE | SVGA3D_SURFACE_HINT_RENDERTARGET:
                        {
                            GLint activeTexture;
                            glGetIntegerv(GL_TEXTURE_BINDING_2D, &activeTexture);
                            VMSVGA3D_CHECK_LAST_ERROR_WARN(pState, pContext);

                            glBindTexture(GL_TEXTURE_2D, pSurface->oglId.texture);
                            VMSVGA3D_CHECK_LAST_ERROR_WARN(pState, pContext);

                            /* Set row length and alignment of the output data. */
                            VMSVGAPACKPARAMS SavedParams;
                            vmsvga3dSetPackParams(pState, pContext, pSurface, &SavedParams);

                            glGetTexImage(GL_TEXTURE_2D,
                                          i,
                                          pSurface->formatGL,
                                          pSurface->typeGL,
                                          pbDst);
                            VMSVGA3D_CHECK_LAST_ERROR_WARN(pState, pContext);

                            vmsvga3dRestorePackParams(pState, pContext, pSurface, &SavedParams);

                            /* Restore the old active texture. */
                            glBindTexture(GL_TEXTURE_2D, activeTexture);
                            VMSVGA3D_CHECK_LAST_ERROR_WARN(pState, pContext);
                            break;
                        }

                        case SVGA3D_SURFACE_HINT_VERTEXBUFFER:
                        case SVGA3D_SURFACE_HINT_INDEXBUFFER:
                        {
                            pState->ext.glBindBuffer(GL_ARRAY_BUFFER, pSurface->oglId.buffer);
                            VMSVGA3D_CHECK_LAST_ERROR(pState, pContext);

                            void *pvSrc = pState->ext.glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
                            VMSVGA3D_CHECK_LAST_ERROR(pState, pContext);
                            if (RT_VALID_PTR(pvSrc))
                                memcpy(pbDst, pvSrc, pMipmapLevel->cbSurface);
                            else
                                AssertPtr(pvSrc);

                            pState->ext.glUnmapBuffer(GL_ARRAY_BUFFER);
                            VMSVGA3D_CHECK_LAST_ERROR(pState, pContext);

                            pState->ext.glBindBuffer(GL_ARRAY_BUFFER, 0);
                            VMSVGA3D_CHECK_LAST_ERROR(pState, pContext);
                            break;
                        }

                        default:
                            AssertMsgFailed(("%#x\n", fSwitchFlags));
                    }
#else
# error "misconfigured"
#endif
                }
                /* else: There is no data in hardware yet, so whatever we got is already current. */
            }
        }
    }

    return VINF_SUCCESS;
}


/**
 * Updates the heap buffers for all surfaces or one specific one.
 *
 * @param   pThis               The VGA device instance data.
 * @param   sid                 The surface ID, UINT32_MAX if all.
 * @thread  VMSVGAFIFO
 */
void vmsvga3dUpdateHeapBuffersForSurfaces(PVGASTATE pThis, uint32_t sid)
{
    PVMSVGA3DSTATE pState = pThis->svga.p3dState;
    AssertReturnVoid(pState);

    if (sid == UINT32_MAX)
    {
        uint32_t cSurfaces = pState->cSurfaces;
        for (sid = 0; sid < cSurfaces; sid++)
        {
            PVMSVGA3DSURFACE pSurface = pState->papSurfaces[sid];
            if (pSurface && pSurface->id == sid)
                vmsvga3dSurfaceUpdateHeapBuffers(pState, pSurface);
        }
    }
    else if (sid < pState->cSurfaces)
    {
        PVMSVGA3DSURFACE pSurface = pState->papSurfaces[sid];
        if (pSurface && pSurface->id == sid)
            vmsvga3dSurfaceUpdateHeapBuffers(pState, pSurface);
    }
}


int vmsvga3dLoadExec(PVGASTATE pThis, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PVMSVGA3DSTATE pState = pThis->svga.p3dState;
    AssertReturn(pState, VERR_NO_MEMORY);
    int            rc;
    uint32_t       cContexts, cSurfaces;

    /* Must initialize now as the recreation calls below rely on an initialized 3d subsystem. */
    vmsvga3dPowerOn(pThis);

    /* Get the generic 3d state first. */
    rc = SSMR3GetStructEx(pSSM, pState, sizeof(*pState), 0, g_aVMSVGA3DSTATEFields, NULL);
    AssertRCReturn(rc, rc);

    cContexts                           = pState->cContexts;
    cSurfaces                           = pState->cSurfaces;
    pState->cContexts                   = 0;
    pState->cSurfaces                   = 0;

    /* Fetch all active contexts. */
    for (uint32_t i = 0; i < cContexts; i++)
    {
        PVMSVGA3DCONTEXT pContext;
        uint32_t         cid;

        /* Get the context id */
        rc = SSMR3GetU32(pSSM, &cid);
        AssertRCReturn(rc, rc);

        if (cid != SVGA3D_INVALID_ID)
        {
            uint32_t cPixelShaderConst, cVertexShaderConst, cPixelShaders, cVertexShaders;

            rc = vmsvga3dContextDefine(pThis, cid);
            AssertRCReturn(rc, rc);

            pContext = pState->papContexts[i];
            AssertReturn(pContext->id == cid, VERR_INTERNAL_ERROR);

            rc = SSMR3GetStructEx(pSSM, pContext, sizeof(*pContext), 0, g_aVMSVGA3DCONTEXTFields, NULL);
            AssertRCReturn(rc, rc);

            cPixelShaders                       = pContext->cPixelShaders;
            cVertexShaders                      = pContext->cVertexShaders;
            cPixelShaderConst                   = pContext->state.cPixelShaderConst;
            cVertexShaderConst                  = pContext->state.cVertexShaderConst;
            pContext->cPixelShaders             = 0;
            pContext->cVertexShaders            = 0;
            pContext->state.cPixelShaderConst   = 0;
            pContext->state.cVertexShaderConst  = 0;

            /* Fetch all pixel shaders. */
            for (uint32_t j = 0; j < cPixelShaders; j++)
            {
                VMSVGA3DSHADER  shader;
                uint32_t        shid;

                /* Fetch the id first. */
                rc = SSMR3GetU32(pSSM, &shid);
                AssertRCReturn(rc, rc);

                if (shid != SVGA3D_INVALID_ID)
                {
                    uint32_t *pData;

                    /* Fetch a copy of the shader struct. */
                    rc = SSMR3GetStructEx(pSSM, &shader, sizeof(shader), 0, g_aVMSVGA3DSHADERFields, NULL);
                    AssertRCReturn(rc, rc);

                    pData = (uint32_t *)RTMemAlloc(shader.cbData);
                    AssertReturn(pData, VERR_NO_MEMORY);

                    rc = SSMR3GetMem(pSSM, pData, shader.cbData);
                    AssertRCReturn(rc, rc);

                    rc = vmsvga3dShaderDefine(pThis, cid, shid, shader.type, shader.cbData, pData);
                    AssertRCReturn(rc, rc);

                    RTMemFree(pData);
                }
            }

            /* Fetch all vertex shaders. */
            for (uint32_t j = 0; j < cVertexShaders; j++)
            {
                VMSVGA3DSHADER  shader;
                uint32_t        shid;

                /* Fetch the id first. */
                rc = SSMR3GetU32(pSSM, &shid);
                AssertRCReturn(rc, rc);

                if (shid != SVGA3D_INVALID_ID)
                {
                    uint32_t *pData;

                    /* Fetch a copy of the shader struct. */
                    rc = SSMR3GetStructEx(pSSM, &shader, sizeof(shader), 0, g_aVMSVGA3DSHADERFields, NULL);
                    AssertRCReturn(rc, rc);

                    pData = (uint32_t *)RTMemAlloc(shader.cbData);
                    AssertReturn(pData, VERR_NO_MEMORY);

                    rc = SSMR3GetMem(pSSM, pData, shader.cbData);
                    AssertRCReturn(rc, rc);

                    rc = vmsvga3dShaderDefine(pThis, cid, shid, shader.type, shader.cbData, pData);
                    AssertRCReturn(rc, rc);

                    RTMemFree(pData);
                }
            }

            /* Fetch pixel shader constants. */
            for (uint32_t j = 0; j < cPixelShaderConst; j++)
            {
                VMSVGASHADERCONST ShaderConst;

                rc = SSMR3GetStructEx(pSSM, &ShaderConst, sizeof(ShaderConst), 0, g_aVMSVGASHADERCONSTFields, NULL);
                AssertRCReturn(rc, rc);

                if (ShaderConst.fValid)
                {
                    rc = vmsvga3dShaderSetConst(pThis, cid, j, SVGA3D_SHADERTYPE_PS, ShaderConst.ctype, 1, ShaderConst.value);
                    AssertRCReturn(rc, rc);
                }
            }

            /* Fetch vertex shader constants. */
            for (uint32_t j = 0; j < cVertexShaderConst; j++)
            {
                VMSVGASHADERCONST ShaderConst;

                rc = SSMR3GetStructEx(pSSM, &ShaderConst, sizeof(ShaderConst), 0, g_aVMSVGASHADERCONSTFields, NULL);
                AssertRCReturn(rc, rc);

                if (ShaderConst.fValid)
                {
                    rc = vmsvga3dShaderSetConst(pThis, cid, j, SVGA3D_SHADERTYPE_VS, ShaderConst.ctype, 1, ShaderConst.value);
                    AssertRCReturn(rc, rc);
                }
            }

        }
    }

    /* Fetch all surfaces. */
    for (uint32_t i = 0; i < cSurfaces; i++)
    {
        uint32_t sid;

        /* Fetch the id first. */
        rc = SSMR3GetU32(pSSM, &sid);
        AssertRCReturn(rc, rc);

        if (sid != SVGA3D_INVALID_ID)
        {
            VMSVGA3DSURFACE  surface;

            /* Fetch the surface structure first. */
            rc = SSMR3GetStructEx(pSSM, &surface, sizeof(surface), 0, g_aVMSVGA3DSURFACEFields, NULL);
            AssertRCReturn(rc, rc);

            {
                uint32_t             cMipLevels = surface.faces[0].numMipLevels * surface.cFaces;
                PVMSVGA3DMIPMAPLEVEL pMipmapLevel = (PVMSVGA3DMIPMAPLEVEL)RTMemAlloc(cMipLevels * sizeof(VMSVGA3DMIPMAPLEVEL));
                AssertReturn(pMipmapLevel, VERR_NO_MEMORY);
                SVGA3dSize *pMipmapLevelSize = (SVGA3dSize *)RTMemAlloc(cMipLevels * sizeof(SVGA3dSize));
                AssertReturn(pMipmapLevelSize, VERR_NO_MEMORY);

                /* Load the mip map level info. */
                for (uint32_t face=0; face < surface.cFaces; face++)
                {
                    for (uint32_t j = 0; j < surface.faces[0].numMipLevels; j++)
                    {
                        uint32_t idx = j + face * surface.faces[0].numMipLevels;
                        /* Load the mip map level struct. */
                        rc = SSMR3GetStructEx(pSSM, &pMipmapLevel[idx], sizeof(pMipmapLevel[idx]), 0, g_aVMSVGA3DMIPMAPLEVELFields, NULL);
                        AssertRCReturn(rc, rc);

                        pMipmapLevelSize[idx] = pMipmapLevel[idx].size;
                    }
                }

                rc = vmsvga3dSurfaceDefine(pThis, sid, surface.flags, surface.format, surface.faces, surface.multiSampleCount, surface.autogenFilter, cMipLevels, pMipmapLevelSize);
                AssertRCReturn(rc, rc);

                RTMemFree(pMipmapLevelSize);
                RTMemFree(pMipmapLevel);
            }

            PVMSVGA3DSURFACE pSurface = pState->papSurfaces[sid];
            Assert(pSurface->id == sid);

            pSurface->fDirty = false;

            /* Load the mip map level data. */
            for (uint32_t j = 0; j < pSurface->faces[0].numMipLevels * pSurface->cFaces; j++)
            {
                PVMSVGA3DMIPMAPLEVEL pMipmapLevel = &pSurface->pMipmapLevels[j];
                bool fDataPresent = false;

                Assert(pMipmapLevel->cbSurface);
                pMipmapLevel->pSurfaceData = RTMemAllocZ(pMipmapLevel->cbSurface);
                AssertReturn(pMipmapLevel->pSurfaceData, VERR_NO_MEMORY);

                /* Fetch the data present boolean first. */
                rc = SSMR3GetBool(pSSM, &fDataPresent);
                AssertRCReturn(rc, rc);

                Log(("Surface sid=%x: load mipmap level %d with %x bytes data (present=%d).\n", sid, j, pMipmapLevel->cbSurface, fDataPresent));

                if (fDataPresent)
                {
                    rc = SSMR3GetMem(pSSM, pMipmapLevel->pSurfaceData, pMipmapLevel->cbSurface);
                    AssertRCReturn(rc, rc);
                    pMipmapLevel->fDirty = true;
                    pSurface->fDirty     = true;
                }
                else
                {
                    pMipmapLevel->fDirty = false;
                }
            }
        }
    }

    /* Reinitialize all active contexts. */
    for (uint32_t i = 0; i < pState->cContexts; i++)
    {
        PVMSVGA3DCONTEXT pContext = pState->papContexts[i];
        uint32_t cid = pContext->id;

        if (cid != SVGA3D_INVALID_ID)
        {
            /* First set the render targets as they change the internal state (reset viewport etc) */
            Log(("vmsvga3dLoadExec: Recreate render targets BEGIN\n"));
            for (uint32_t j = 0; j < RT_ELEMENTS(pContext->state.aRenderTargets); j++)
            {
                if (pContext->state.aRenderTargets[j] != SVGA3D_INVALID_ID)
                {
                    SVGA3dSurfaceImageId target;

                    target.sid      = pContext->state.aRenderTargets[j];
                    target.face     = 0;
                    target.mipmap   = 0;
                    rc = vmsvga3dSetRenderTarget(pThis, cid, (SVGA3dRenderTargetType)j, target);
                    AssertRCReturn(rc, rc);
                }
            }
            Log(("vmsvga3dLoadExec: Recreate render targets END\n"));

            /* Recreate the render state */
            Log(("vmsvga3dLoadExec: Recreate render state BEGIN\n"));
            for (uint32_t j = 0; j < RT_ELEMENTS(pContext->state.aRenderState); j++)
            {
                SVGA3dRenderState *pRenderState = &pContext->state.aRenderState[j];

                if (pRenderState->state != SVGA3D_RS_INVALID)
                    vmsvga3dSetRenderState(pThis, pContext->id, 1, pRenderState);
            }
            Log(("vmsvga3dLoadExec: Recreate render state END\n"));

            /* Recreate the texture state */
            Log(("vmsvga3dLoadExec: Recreate texture state BEGIN\n"));
            for (uint32_t iStage = 0; iStage < SVGA3D_MAX_TEXTURE_STAGE; iStage++)
            {
                for (uint32_t j = 0; j < SVGA3D_TS_MAX; j++)
                {
                    SVGA3dTextureState *pTextureState = &pContext->state.aTextureState[iStage][j];

                    if (pTextureState->name != SVGA3D_TS_INVALID)
                        vmsvga3dSetTextureState(pThis, pContext->id, 1, pTextureState);
                }
            }
            Log(("vmsvga3dLoadExec: Recreate texture state END\n"));

            /* Reprogram the clip planes. */
            for (uint32_t j = 0; j < RT_ELEMENTS(pContext->state.aClipPlane); j++)
            {
                if (pContext->state.aClipPlane[j].fValid == true)
                    vmsvga3dSetClipPlane(pThis, cid, j, pContext->state.aClipPlane[j].plane);
            }

            /* Reprogram the light data. */
            for (uint32_t j = 0; j < RT_ELEMENTS(pContext->state.aLightData); j++)
            {
                if (pContext->state.aLightData[j].fValidData == true)
                    vmsvga3dSetLightData(pThis, cid, j, &pContext->state.aLightData[j].data);
                if (pContext->state.aLightData[j].fEnabled)
                    vmsvga3dSetLightEnabled(pThis, cid, j, true);
            }

            /* Recreate the transform state. */
            if (pContext->state.u32UpdateFlags & VMSVGA3D_UPDATE_TRANSFORM)
            {
                for (uint32_t j = 0; j < RT_ELEMENTS(pContext->state.aTransformState); j++)
                {
                    if (pContext->state.aTransformState[j].fValid == true)
                        vmsvga3dSetTransform(pThis, cid, (SVGA3dTransformType)j, pContext->state.aTransformState[j].matrix);
                }
            }

            /* Reprogram the material data. */
            if (pContext->state.u32UpdateFlags & VMSVGA3D_UPDATE_MATERIAL)
            {
                for (uint32_t j = 0; j < RT_ELEMENTS(pContext->state.aMaterial); j++)
                {
                    if (pContext->state.aMaterial[j].fValid == true)
                        vmsvga3dSetMaterial(pThis, cid, (SVGA3dFace)j, &pContext->state.aMaterial[j].material);
                }
            }

            if (pContext->state.u32UpdateFlags & VMSVGA3D_UPDATE_SCISSORRECT)
                vmsvga3dSetScissorRect(pThis, cid, &pContext->state.RectScissor);
            if (pContext->state.u32UpdateFlags & VMSVGA3D_UPDATE_ZRANGE)
                vmsvga3dSetZRange(pThis, cid, pContext->state.zRange);
            if (pContext->state.u32UpdateFlags & VMSVGA3D_UPDATE_VIEWPORT)
                vmsvga3dSetViewPort(pThis, cid, &pContext->state.RectViewPort);
            if (pContext->state.u32UpdateFlags & VMSVGA3D_UPDATE_VERTEXSHADER)
                vmsvga3dShaderSet(pThis, pContext, cid, SVGA3D_SHADERTYPE_VS, pContext->state.shidVertex);
            if (pContext->state.u32UpdateFlags & VMSVGA3D_UPDATE_PIXELSHADER)
                vmsvga3dShaderSet(pThis, pContext, cid, SVGA3D_SHADERTYPE_PS, pContext->state.shidPixel);
        }
    }
    return VINF_SUCCESS;
}

int vmsvga3dSaveExec(PVGASTATE pThis, PSSMHANDLE pSSM)
{
    PVMSVGA3DSTATE pState = pThis->svga.p3dState;
    AssertReturn(pState, VERR_NO_MEMORY);
    int            rc;

    /* Save a copy of the generic 3d state first. */
    rc = SSMR3PutStructEx(pSSM, pState, sizeof(*pState), 0, g_aVMSVGA3DSTATEFields, NULL);
    AssertRCReturn(rc, rc);

    /* Save all active contexts. */
    for (uint32_t i = 0; i < pState->cContexts; i++)
    {
        PVMSVGA3DCONTEXT pContext = pState->papContexts[i];
        uint32_t cid = pContext->id;

        /* Save the id first. */
        rc = SSMR3PutU32(pSSM, cid);
        AssertRCReturn(rc, rc);

        if (cid != SVGA3D_INVALID_ID)
        {
            /* Save a copy of the context structure first. */
            rc = SSMR3PutStructEx(pSSM, pContext, sizeof(*pContext), 0, g_aVMSVGA3DCONTEXTFields, NULL);
            AssertRCReturn(rc, rc);

            /* Save all pixel shaders. */
            for (uint32_t j = 0; j < pContext->cPixelShaders; j++)
            {
                PVMSVGA3DSHADER pShader = &pContext->paPixelShader[j];

                /* Save the id first. */
                rc = SSMR3PutU32(pSSM, pShader->id);
                AssertRCReturn(rc, rc);

                if (pShader->id != SVGA3D_INVALID_ID)
                {
                    uint32_t cbData = pShader->cbData;

                    /* Save a copy of the shader struct. */
                    rc = SSMR3PutStructEx(pSSM, pShader, sizeof(*pShader), 0, g_aVMSVGA3DSHADERFields, NULL);
                    AssertRCReturn(rc, rc);

                    Log(("Save pixelshader shid=%d with %x bytes code.\n", pShader->id, cbData));
                    rc = SSMR3PutMem(pSSM, pShader->pShaderProgram, cbData);
                    AssertRCReturn(rc, rc);
                }
            }

            /* Save all vertex shaders. */
            for (uint32_t j = 0; j < pContext->cVertexShaders; j++)
            {
                PVMSVGA3DSHADER pShader = &pContext->paVertexShader[j];

                /* Save the id first. */
                rc = SSMR3PutU32(pSSM, pShader->id);
                AssertRCReturn(rc, rc);

                if (pShader->id != SVGA3D_INVALID_ID)
                {
                    uint32_t cbData = pShader->cbData;

                    /* Save a copy of the shader struct. */
                    rc = SSMR3PutStructEx(pSSM, pShader, sizeof(*pShader), 0, g_aVMSVGA3DSHADERFields, NULL);
                    AssertRCReturn(rc, rc);

                    Log(("Save vertex shader shid=%d with %x bytes code.\n", pShader->id, cbData));
                    /* Fetch the shader code and save it. */
                    rc = SSMR3PutMem(pSSM, pShader->pShaderProgram, cbData);
                    AssertRCReturn(rc, rc);
                }
            }

            /* Save pixel shader constants. */
            for (uint32_t j = 0; j < pContext->state.cPixelShaderConst; j++)
            {
                rc = SSMR3PutStructEx(pSSM, &pContext->state.paPixelShaderConst[j], sizeof(pContext->state.paPixelShaderConst[j]), 0, g_aVMSVGASHADERCONSTFields, NULL);
                AssertRCReturn(rc, rc);
            }

            /* Save vertex shader constants. */
            for (uint32_t j = 0; j < pContext->state.cVertexShaderConst; j++)
            {
                rc = SSMR3PutStructEx(pSSM, &pContext->state.paVertexShaderConst[j], sizeof(pContext->state.paVertexShaderConst[j]), 0, g_aVMSVGASHADERCONSTFields, NULL);
                AssertRCReturn(rc, rc);
            }
        }
    }

    /* Save all active surfaces. */
    for (uint32_t sid = 0; sid < pState->cSurfaces; sid++)
    {
        PVMSVGA3DSURFACE pSurface = pState->papSurfaces[sid];

        /* Save the id first. */
        rc = SSMR3PutU32(pSSM, pSurface->id);
        AssertRCReturn(rc, rc);

        if (pSurface->id != SVGA3D_INVALID_ID)
        {
            /* Save a copy of the surface structure first. */
            rc = SSMR3PutStructEx(pSSM, pSurface, sizeof(*pSurface), 0, g_aVMSVGA3DSURFACEFields, NULL);
            AssertRCReturn(rc, rc);

            /* Save the mip map level info. */
            for (uint32_t face=0; face < pSurface->cFaces; face++)
            {
                for (uint32_t i = 0; i < pSurface->faces[0].numMipLevels; i++)
                {
                    uint32_t idx = i + face * pSurface->faces[0].numMipLevels;
                    PVMSVGA3DMIPMAPLEVEL pMipmapLevel = &pSurface->pMipmapLevels[idx];

                    /* Save a copy of the mip map level struct. */
                    rc = SSMR3PutStructEx(pSSM, pMipmapLevel, sizeof(*pMipmapLevel), 0, g_aVMSVGA3DMIPMAPLEVELFields, NULL);
                    AssertRCReturn(rc, rc);
                }
            }

            /* Save the mip map level data. */
            for (uint32_t face=0; face < pSurface->cFaces; face++)
            {
                for (uint32_t i = 0; i < pSurface->faces[0].numMipLevels; i++)
                {
                    uint32_t idx = i + face * pSurface->faces[0].numMipLevels;
                    PVMSVGA3DMIPMAPLEVEL pMipmapLevel = &pSurface->pMipmapLevels[idx];

                    Log(("Surface sid=%d: save mipmap level %d with %x bytes data.\n", sid, i, pMipmapLevel->cbSurface));

#ifdef VMSVGA3D_DIRECT3D
                    if (!pSurface->u.pSurface)
#else
                    if (pSurface->oglId.texture == OPENGL_INVALID_ID)
#endif
                    {
                        if (pMipmapLevel->fDirty)
                        {
                            /* Data follows */
                            rc = SSMR3PutBool(pSSM, true);
                            AssertRCReturn(rc, rc);

                            Assert(pMipmapLevel->cbSurface);
                            rc = SSMR3PutMem(pSSM, pMipmapLevel->pSurfaceData, pMipmapLevel->cbSurface);
                            AssertRCReturn(rc, rc);
                        }
                        else
                        {
                            /* No data follows */
                            rc = SSMR3PutBool(pSSM, false);
                            AssertRCReturn(rc, rc);
                        }
                    }
                    else
                    {
#ifdef VMSVGA3D_DIRECT3D
                        void            *pData;
                        bool             fRenderTargetTexture = false;
                        bool             fTexture = false;
                        bool             fVertex = false;
                        bool             fSkipSave = false;
                        HRESULT          hr;

                        Assert(pMipmapLevel->cbSurface);
                        pData = RTMemAllocZ(pMipmapLevel->cbSurface);
                        AssertReturn(pData, VERR_NO_MEMORY);

                        switch (pSurface->flags & (SVGA3D_SURFACE_HINT_INDEXBUFFER | SVGA3D_SURFACE_HINT_VERTEXBUFFER | SVGA3D_SURFACE_HINT_TEXTURE | SVGA3D_SURFACE_HINT_RENDERTARGET | SVGA3D_SURFACE_HINT_DEPTHSTENCIL | SVGA3D_SURFACE_CUBEMAP))
                        {
                        case SVGA3D_SURFACE_HINT_DEPTHSTENCIL:
                        case SVGA3D_SURFACE_HINT_DEPTHSTENCIL | SVGA3D_SURFACE_HINT_TEXTURE:
                            /* @todo unable to easily fetch depth surface data in d3d 9 */
                            fSkipSave = true;
                            break;
                        case SVGA3D_SURFACE_HINT_TEXTURE | SVGA3D_SURFACE_HINT_RENDERTARGET:
                            fRenderTargetTexture = true;
                            /* no break */
                        case SVGA3D_SURFACE_HINT_TEXTURE:
                            fTexture = true;
                            /* no break */
                        case SVGA3D_SURFACE_HINT_RENDERTARGET:
                        {
                            D3DLOCKED_RECT LockedRect;

                            if (fTexture)
                            {
                                if (pSurface->bounce.pTexture)
                                {
                                    if (    !pSurface->fDirty
                                        &&  fRenderTargetTexture
                                        &&  i == 0 /* only the first time */)
                                    {
                                        IDirect3DSurface9 *pSrc, *pDest;

                                        /* @todo stricter checks for associated context */
                                        uint32_t cid = pSurface->idAssociatedContext;
                                        if (    cid >= pState->cContexts
                                            ||  pState->papContexts[cid]->id != cid)
                                        {
                                            Log(("vmsvga3dSaveExec invalid context id (%x - %x)!\n", cid, (cid >= pState->cContexts) ? -1 : pState->papContexts[cid]->id));
                                            AssertFailedReturn(VERR_INVALID_PARAMETER);
                                        }
                                        PVMSVGA3DCONTEXT pContext = pState->papContexts[cid];

                                        hr = pSurface->bounce.pTexture->GetSurfaceLevel(i, &pDest);
                                        AssertMsgReturn(hr == D3D_OK, ("vmsvga3dSaveExec: GetSurfaceLevel failed with %x\n", hr), VERR_INTERNAL_ERROR);

                                        hr = pSurface->u.pTexture->GetSurfaceLevel(i, &pSrc);
                                        AssertMsgReturn(hr == D3D_OK, ("vmsvga3dSaveExec: GetSurfaceLevel failed with %x\n", hr), VERR_INTERNAL_ERROR);

                                        hr = pContext->pDevice->GetRenderTargetData(pSrc, pDest);
                                        AssertMsgReturn(hr == D3D_OK, ("vmsvga3dSaveExec: GetRenderTargetData failed with %x\n", hr), VERR_INTERNAL_ERROR);

                                        pSrc->Release();
                                        pDest->Release();
                                    }

                                    hr = pSurface->bounce.pTexture->LockRect(i, /* texture level */
                                                                             &LockedRect,
                                                                             NULL,
                                                                             D3DLOCK_READONLY);
                                }
                                else
                                    hr = pSurface->u.pTexture->LockRect(i, /* texture level */
                                                                        &LockedRect,
                                                                        NULL,
                                                                        D3DLOCK_READONLY);
                            }
                            else
                                hr = pSurface->u.pSurface->LockRect(&LockedRect,
                                                                    NULL,
                                                                    D3DLOCK_READONLY);
                            AssertMsgReturn(hr == D3D_OK, ("vmsvga3dSaveExec: LockRect failed with %x\n", hr), VERR_INTERNAL_ERROR);

                            /* Copy the data one line at a time in case the internal pitch is different. */
                            for (uint32_t j = 0; j < pMipmapLevel->size.height; j++)
                            {
                                memcpy((uint8_t *)pData + j * pMipmapLevel->cbSurfacePitch, (uint8_t *)LockedRect.pBits + j * LockedRect.Pitch, pMipmapLevel->cbSurfacePitch);
                            }

                            if (fTexture)
                            {
                                if (pSurface->bounce.pTexture)
                                {
                                    hr = pSurface->bounce.pTexture->UnlockRect(i);
                                    AssertMsgReturn(hr == D3D_OK, ("vmsvga3dSaveExec: UnlockRect failed with %x\n", hr), VERR_INTERNAL_ERROR);
                                }
                                else
                                    hr = pSurface->u.pTexture->UnlockRect(i);
                            }
                            else
                                hr = pSurface->u.pSurface->UnlockRect();
                            AssertMsgReturn(hr == D3D_OK, ("vmsvga3dSaveExec: UnlockRect failed with %x\n", hr), VERR_INTERNAL_ERROR);
                            break;
                        }

                        case SVGA3D_SURFACE_HINT_VERTEXBUFFER:
                            fVertex = true;
                            /* no break */

                        case SVGA3D_SURFACE_HINT_INDEXBUFFER:
                        {
                            uint8_t *pD3DData;

                            if (fVertex)
                                hr = pSurface->u.pVertexBuffer->Lock(0, 0, (void **)&pD3DData, D3DLOCK_READONLY);
                            else
                                hr = pSurface->u.pIndexBuffer->Lock(0, 0, (void **)&pD3DData, D3DLOCK_READONLY);
                            AssertMsg(hr == D3D_OK, ("vmsvga3dSaveExec: Lock %s failed with %x\n", (fVertex) ? "vertex" : "index", hr));

                            memcpy(pData, pD3DData, pMipmapLevel->cbSurface);

                            if (fVertex)
                                hr = pSurface->u.pVertexBuffer->Unlock();
                            else
                                hr = pSurface->u.pIndexBuffer->Unlock();
                            AssertMsg(hr == D3D_OK, ("vmsvga3dSaveExec: Unlock %s failed with %x\n", (fVertex) ? "vertex" : "index", hr));
                            break;
                        }

                        default:
                            AssertFailed();
                            break;
                        }

                        if (!fSkipSave)
                        {
                            /* Data follows */
                            rc = SSMR3PutBool(pSSM, true);
                            AssertRCReturn(rc, rc);

                            /* And write the surface data. */
                            rc = SSMR3PutMem(pSSM, pData, pMipmapLevel->cbSurface);
                            AssertRCReturn(rc, rc);
                        }
                        else
                        {
                            /* No data follows */
                            rc = SSMR3PutBool(pSSM, false);
                            AssertRCReturn(rc, rc);
                        }

                        RTMemFree(pData);
#elif defined(VMSVGA3D_OPENGL)
                        void *pData = NULL;

# ifdef VMSVGA3D_OGL_WITH_SHARED_CTX
                        PVMSVGA3DCONTEXT pContext = &pState->SharedCtx;
                        VMSVGA3D_SET_CURRENT_CONTEXT(pState, pContext);
# else
                        /* @todo stricter checks for associated context */
                        uint32_t cid = pSurface->idAssociatedContext;
                        if (    cid >= pState->cContexts
                            ||  pState->papContexts[cid]->id != cid)
                        {
                            Log(("vmsvga3dSaveExec: invalid context id (%x - %x)!\n", cid, (cid >= pState->cContexts) ? -1 : pState->papContexts[cid]->id));
                            AssertFailedReturn(VERR_INVALID_PARAMETER);
                        }
                        PVMSVGA3DCONTEXT pContext = pState->papContexts[cid];
                        VMSVGA3D_SET_CURRENT_CONTEXT(pState, pContext);
# endif

                        Assert(pMipmapLevel->cbSurface);

                        switch (pSurface->flags & (SVGA3D_SURFACE_HINT_INDEXBUFFER | SVGA3D_SURFACE_HINT_VERTEXBUFFER | SVGA3D_SURFACE_HINT_TEXTURE | SVGA3D_SURFACE_HINT_RENDERTARGET | SVGA3D_SURFACE_HINT_DEPTHSTENCIL | SVGA3D_SURFACE_CUBEMAP))
                        {
                        default:
                            AssertFailed();
                            /* no break */
                        case SVGA3D_SURFACE_HINT_DEPTHSTENCIL:
                        case SVGA3D_SURFACE_HINT_DEPTHSTENCIL | SVGA3D_SURFACE_HINT_TEXTURE:
                            /* @todo fetch data from the renderbuffer */
                            /* No data follows */
                            rc = SSMR3PutBool(pSSM, false);
                            AssertRCReturn(rc, rc);
                            break;

                        case SVGA3D_SURFACE_HINT_TEXTURE | SVGA3D_SURFACE_HINT_RENDERTARGET:
                        case SVGA3D_SURFACE_HINT_TEXTURE:
                        case SVGA3D_SURFACE_HINT_RENDERTARGET:
                        {
                            GLint activeTexture;

                            pData = RTMemAllocZ(pMipmapLevel->cbSurface);
                            AssertReturn(pData, VERR_NO_MEMORY);

                            glGetIntegerv(GL_TEXTURE_BINDING_2D, &activeTexture);
                            VMSVGA3D_CHECK_LAST_ERROR_WARN(pState, pContext);

                            glBindTexture(GL_TEXTURE_2D, pSurface->oglId.texture);
                            VMSVGA3D_CHECK_LAST_ERROR_WARN(pState, pContext);

                            /* Set row length and alignment of the output data. */
                            VMSVGAPACKPARAMS SavedParams;
                            vmsvga3dSetPackParams(pState, pContext, pSurface, &SavedParams);

                            glGetTexImage(GL_TEXTURE_2D,
                                          i,
                                          pSurface->formatGL,
                                          pSurface->typeGL,
                                          pData);
                            VMSVGA3D_CHECK_LAST_ERROR_WARN(pState, pContext);

                            vmsvga3dRestorePackParams(pState, pContext, pSurface, &SavedParams);

                            /* Data follows */
                            rc = SSMR3PutBool(pSSM, true);
                            AssertRCReturn(rc, rc);

                            /* And write the surface data. */
                            rc = SSMR3PutMem(pSSM, pData, pMipmapLevel->cbSurface);
                            AssertRCReturn(rc, rc);

                            /* Restore the old active texture. */
                            glBindTexture(GL_TEXTURE_2D, activeTexture);
                            VMSVGA3D_CHECK_LAST_ERROR_WARN(pState, pContext);
                            break;
                        }

                        case SVGA3D_SURFACE_HINT_VERTEXBUFFER:
                        case SVGA3D_SURFACE_HINT_INDEXBUFFER:
                        {
                            uint8_t *pBufferData;

                            pState->ext.glBindBuffer(GL_ARRAY_BUFFER, pSurface->oglId.buffer);
                            VMSVGA3D_CHECK_LAST_ERROR(pState, pContext);

                            pBufferData = (uint8_t *)pState->ext.glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
                            VMSVGA3D_CHECK_LAST_ERROR(pState, pContext);
                            Assert(pBufferData);

                            /* Data follows */
                            rc = SSMR3PutBool(pSSM, true);
                            AssertRCReturn(rc, rc);

                            /* And write the surface data. */
                            rc = SSMR3PutMem(pSSM, pBufferData, pMipmapLevel->cbSurface);
                            AssertRCReturn(rc, rc);

                            pState->ext.glUnmapBuffer(GL_ARRAY_BUFFER);
                            VMSVGA3D_CHECK_LAST_ERROR(pState, pContext);

                            pState->ext.glBindBuffer(GL_ARRAY_BUFFER, 0);
                            VMSVGA3D_CHECK_LAST_ERROR(pState, pContext);

                        }
                        }
                        if (pData)
                            RTMemFree(pData);
#else
#error "Unexpected 3d backend"
#endif
                    }
                }
            }
        }
    }
    return VINF_SUCCESS;
}

static uint32_t vmsvga3dSaveShaderConst(PVMSVGA3DCONTEXT pContext, uint32_t reg, SVGA3dShaderType type, SVGA3dShaderConstType ctype, uint32_t val1, uint32_t val2, uint32_t val3, uint32_t val4)
{
    /* Choose a sane upper limit. */
    AssertReturn(reg < _32K, VERR_INVALID_PARAMETER);

    if (type == SVGA3D_SHADERTYPE_VS)
    {
        if (pContext->state.cVertexShaderConst <= reg)
        {
            pContext->state.paVertexShaderConst = (PVMSVGASHADERCONST)RTMemRealloc(pContext->state.paVertexShaderConst, sizeof(VMSVGASHADERCONST) * (reg + 1));
            AssertReturn(pContext->state.paVertexShaderConst, VERR_NO_MEMORY);
            for (uint32_t i = pContext->state.cVertexShaderConst; i < reg + 1; i++)
                pContext->state.paVertexShaderConst[i].fValid = false;
            pContext->state.cVertexShaderConst = reg + 1;
        }

        pContext->state.paVertexShaderConst[reg].fValid   = true;
        pContext->state.paVertexShaderConst[reg].ctype    = ctype;
        pContext->state.paVertexShaderConst[reg].value[0] = val1;
        pContext->state.paVertexShaderConst[reg].value[1] = val2;
        pContext->state.paVertexShaderConst[reg].value[2] = val3;
        pContext->state.paVertexShaderConst[reg].value[3] = val4;
    }
    else
    {
        Assert(type == SVGA3D_SHADERTYPE_PS);
        if (pContext->state.cPixelShaderConst <= reg)
        {
            pContext->state.paPixelShaderConst = (PVMSVGASHADERCONST)RTMemRealloc(pContext->state.paPixelShaderConst, sizeof(VMSVGASHADERCONST) * (reg + 1));
            AssertReturn(pContext->state.paPixelShaderConst, VERR_NO_MEMORY);
            for (uint32_t i = pContext->state.cPixelShaderConst; i < reg + 1; i++)
                pContext->state.paPixelShaderConst[i].fValid = false;
            pContext->state.cPixelShaderConst = reg + 1;
        }

        pContext->state.paPixelShaderConst[reg].fValid   = true;
        pContext->state.paPixelShaderConst[reg].ctype    = ctype;
        pContext->state.paPixelShaderConst[reg].value[0] = val1;
        pContext->state.paPixelShaderConst[reg].value[1] = val2;
        pContext->state.paPixelShaderConst[reg].value[2] = val3;
        pContext->state.paPixelShaderConst[reg].value[3] = val4;
    }

    return VINF_SUCCESS;
}



static const char * const g_apszTransformTypes[] =
{
    "SVGA3D_TRANSFORM_INVALID",
    "SVGA3D_TRANSFORM_WORLD",
    "SVGA3D_TRANSFORM_VIEW",
    "SVGA3D_TRANSFORM_PROJECTION",
    "SVGA3D_TRANSFORM_TEXTURE0",
    "SVGA3D_TRANSFORM_TEXTURE1",
    "SVGA3D_TRANSFORM_TEXTURE2",
    "SVGA3D_TRANSFORM_TEXTURE3",
    "SVGA3D_TRANSFORM_TEXTURE4",
    "SVGA3D_TRANSFORM_TEXTURE5",
    "SVGA3D_TRANSFORM_TEXTURE6",
    "SVGA3D_TRANSFORM_TEXTURE7",
    "SVGA3D_TRANSFORM_WORLD1",
    "SVGA3D_TRANSFORM_WORLD2",
    "SVGA3D_TRANSFORM_WORLD3",
};

static const char * const g_apszFaces[] =
{
    "SVGA3D_FACE_INVALID",
    "SVGA3D_FACE_NONE",
    "SVGA3D_FACE_FRONT",
    "SVGA3D_FACE_BACK",
    "SVGA3D_FACE_FRONT_BACK",
};

static const char * const g_apszLightTypes[] =
{
    "SVGA3D_LIGHTTYPE_INVALID",
    "SVGA3D_LIGHTTYPE_POINT",
    "SVGA3D_LIGHTTYPE_SPOT1",
    "SVGA3D_LIGHTTYPE_SPOT2",
    "SVGA3D_LIGHTTYPE_DIRECTIONAL",
};

static const char * const g_apszRenderTargets[] =
{
    "SVGA3D_RT_DEPTH",
    "SVGA3D_RT_STENCIL",
    "SVGA3D_RT_COLOR0",
    "SVGA3D_RT_COLOR1",
    "SVGA3D_RT_COLOR2",
    "SVGA3D_RT_COLOR3",
    "SVGA3D_RT_COLOR4",
    "SVGA3D_RT_COLOR5",
    "SVGA3D_RT_COLOR6",
    "SVGA3D_RT_COLOR7",
};

static void vmsvga3dInfoContextWorkerOne(PCDBGFINFOHLP pHlp, PVMSVGA3DCONTEXT pContext, bool fVerbose)
{
    char szTmp[128];

    pHlp->pfnPrintf(pHlp, "*** VMSVGA 3d context %#x (%d) ***\n", pContext->id, pContext->id);
#ifdef RT_OS_WINDOWS
    pHlp->pfnPrintf(pHlp, "hwnd:                    %p\n", pContext->hwnd);
    if (fVerbose)
        vmsvga3dInfoHostWindow(pHlp, (uintptr_t)pContext->hwnd);
# ifdef VMSVGA3D_DIRECT3D
    pHlp->pfnPrintf(pHlp, "pDevice:                 %p\n", pContext->pDevice);
# else
    pHlp->pfnPrintf(pHlp, "hdc:                     %p\n", pContext->hdc);
    pHlp->pfnPrintf(pHlp, "hglrc:                   %p\n", pContext->hglrc);
# endif
#else
/** @todo Other hosts... */
#endif
    pHlp->pfnPrintf(pHlp, "sidRenderTarget:         %#x\n", pContext->sidRenderTarget);

    for (uint32_t i = 0; i < RT_ELEMENTS(pContext->aSidActiveTexture); i++)
        if (pContext->aSidActiveTexture[i] != SVGA3D_INVALID_ID)
            pHlp->pfnPrintf(pHlp, "aSidActiveTexture[%u]:    %#x\n", i, pContext->aSidActiveTexture[i]);

    pHlp->pfnPrintf(pHlp, "fUpdateFlags:            %#x\n", pContext->state.u32UpdateFlags);

    for (uint32_t i = 0; i < RT_ELEMENTS(pContext->state.aRenderState); i++)
        if (pContext->state.aRenderState[i].state != SVGA3D_RS_INVALID)
            pHlp->pfnPrintf(pHlp, "aRenderState[%3d]: %s\n", i,
                            vmsvga3dFormatRenderState(szTmp, sizeof(szTmp), &pContext->state.aRenderState[i]));

    for (uint32_t i = 0; i < RT_ELEMENTS(pContext->state.aTextureState); i++)
        for (uint32_t j = 0; j < RT_ELEMENTS(pContext->state.aTextureState[i]); j++)
            if (pContext->state.aTextureState[i][j].name != SVGA3D_TS_INVALID)
                pHlp->pfnPrintf(pHlp, "aTextureState[%3d][%3d]: %s\n", i, j,
                                vmsvga3dFormatTextureState(szTmp, sizeof(szTmp), &pContext->state.aTextureState[i][j]));

    AssertCompile(RT_ELEMENTS(g_apszTransformTypes) == SVGA3D_TRANSFORM_MAX);
    for (uint32_t i = 0; i < RT_ELEMENTS(pContext->state.aTransformState); i++)
        if (pContext->state.aTransformState[i].fValid)
        {
            pHlp->pfnPrintf(pHlp, "aTransformState[%s(%u)]:\n", g_apszTransformTypes[i], i);
            for (uint32_t j = 0; j < RT_ELEMENTS(pContext->state.aTransformState[i].matrix); j++)
                pHlp->pfnPrintf(pHlp,
                                (j % 4) == 0 ? "    [ " FLOAT_FMT_STR : (j % 4) < 3  ? ", " FLOAT_FMT_STR : ", " FLOAT_FMT_STR "]\n",
                                FLOAT_FMT_ARGS(pContext->state.aTransformState[i].matrix[j]));
        }

    AssertCompile(RT_ELEMENTS(g_apszFaces) == SVGA3D_FACE_MAX);
    for (uint32_t i = 0; i < RT_ELEMENTS(pContext->state.aMaterial); i++)
        if (pContext->state.aMaterial[i].fValid)
        {
            pHlp->pfnPrintf(pHlp, "aTransformState[%s(%u)]: shininess=" FLOAT_FMT_STR "\n",
                            g_apszFaces[i], i, FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.shininess));
            pHlp->pfnPrintf(pHlp, "    diffuse =[ " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR " ]\n",
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.diffuse[0]),
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.diffuse[1]),
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.diffuse[2]),
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.diffuse[3]));
            pHlp->pfnPrintf(pHlp, "    ambient =[ " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR " ]\n",
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.ambient[0]),
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.ambient[1]),
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.ambient[2]),
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.ambient[3]));
            pHlp->pfnPrintf(pHlp, "    specular=[ " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR " ]\n",
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.specular[0]),
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.specular[1]),
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.specular[2]),
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.specular[3]));
            pHlp->pfnPrintf(pHlp, "    emissive=[ " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR " ]\n",
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.emissive[0]),
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.emissive[1]),
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.emissive[2]),
                            FLOAT_FMT_ARGS(pContext->state.aMaterial[i].material.emissive[3]));
        }

    for (uint32_t i = 0; i < RT_ELEMENTS(pContext->state.aClipPlane); i++)
        if (pContext->state.aClipPlane[i].fValid)
            pHlp->pfnPrintf(pHlp, "aClipPlane[%#04x]: [ " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR " ]\n",
                            FLOAT_FMT_ARGS(pContext->state.aClipPlane[i].plane[0]),
                            FLOAT_FMT_ARGS(pContext->state.aClipPlane[i].plane[1]),
                            FLOAT_FMT_ARGS(pContext->state.aClipPlane[i].plane[2]),
                            FLOAT_FMT_ARGS(pContext->state.aClipPlane[i].plane[3]));

    for (uint32_t i = 0; i < RT_ELEMENTS(pContext->state.aLightData); i++)
        if (pContext->state.aLightData[i].fValidData)
        {
            pHlp->pfnPrintf(pHlp, "aLightData[%#04x]: enabled=%RTbool inWorldSpace=%RTbool type=%s(%u)\n",
                            i,
                            pContext->state.aLightData[i].fEnabled,
                            pContext->state.aLightData[i].data.inWorldSpace,
                            (uint32_t)pContext->state.aLightData[i].data.type < RT_ELEMENTS(g_apszLightTypes)
                            ? g_apszLightTypes[pContext->state.aLightData[i].data.type] : "UNKNOWN",
                            pContext->state.aLightData[i].data.type);
            pHlp->pfnPrintf(pHlp, "    diffuse  =[ " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR " ]\n",
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.diffuse[0]),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.diffuse[1]),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.diffuse[2]),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.diffuse[3]));
            pHlp->pfnPrintf(pHlp, "    specular =[ " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR " ]\n",
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.specular[0]),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.specular[1]),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.specular[2]),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.specular[3]));
            pHlp->pfnPrintf(pHlp, "    ambient  =[ " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR " ]\n",
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.ambient[0]),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.ambient[1]),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.ambient[2]),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.ambient[3]));
            pHlp->pfnPrintf(pHlp, "    position =[ " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR " ]\n",
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.position[0]),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.position[1]),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.position[2]),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.position[3]));
            pHlp->pfnPrintf(pHlp, "    direction=[ " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR " ]\n",
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.direction[0]),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.direction[1]),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.direction[2]),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.direction[3]));
            pHlp->pfnPrintf(pHlp, "    range=" FLOAT_FMT_STR "  falloff=" FLOAT_FMT_STR "\n",
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.range),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.falloff));
            pHlp->pfnPrintf(pHlp, "    attenuation0=" FLOAT_FMT_STR "  attenuation1=" FLOAT_FMT_STR "  attenuation2=" FLOAT_FMT_STR "\n",
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.attenuation0),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.attenuation1),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.attenuation2));
            pHlp->pfnPrintf(pHlp, "    theta=" FLOAT_FMT_STR "  phi=" FLOAT_FMT_STR "\n",
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.theta),
                            FLOAT_FMT_ARGS(pContext->state.aLightData[i].data.phi));
        }

    for (uint32_t i = 0; i < RT_ELEMENTS(pContext->state.aRenderTargets); i++)
        if (pContext->state.aRenderTargets[i] != SVGA3D_INVALID_ID)
            pHlp->pfnPrintf(pHlp, "aRenderTargets[%s/%u] = %#x (%d)\n",
                            i < RT_ELEMENTS(g_apszRenderTargets) ? g_apszRenderTargets[i] : "UNKNOWN", i,
                            pContext->state.aRenderTargets[i], pContext->state.aRenderTargets[i]);

    pHlp->pfnPrintf(pHlp, "RectScissor: (x,y,cx,cy)=(%u,%u,%u,%u)\n",
                    pContext->state.RectViewPort.x, pContext->state.RectViewPort.y,
                    pContext->state.RectViewPort.w, pContext->state.RectViewPort.h);
    pHlp->pfnPrintf(pHlp, "zRange:        (min,max)=(" FLOAT_FMT_STR ", " FLOAT_FMT_STR ")\n",
                    FLOAT_FMT_ARGS(pContext->state.zRange.min),
                    FLOAT_FMT_ARGS(pContext->state.zRange.max));
    pHlp->pfnPrintf(pHlp, "fUpdateFlags:            %#x\n", pContext->state.u32UpdateFlags);
    pHlp->pfnPrintf(pHlp, "shidPixel:               %#x (%d)\n", pContext->state.shidPixel, pContext->state.shidPixel);
    pHlp->pfnPrintf(pHlp, "shidVertex:              %#x (%d)\n", pContext->state.shidVertex, pContext->state.shidVertex);

    for (uint32_t iWhich = 0; iWhich < 2; iWhich++)
    {
        uint32_t            cConsts  = iWhich == 0 ? pContext->state.cPixelShaderConst   : pContext->state.cVertexShaderConst;
        PVMSVGASHADERCONST  paConsts = iWhich == 0 ? pContext->state.paPixelShaderConst  : pContext->state.paVertexShaderConst;
        const char         *pszName  = iWhich      ?                "paPixelShaderConst" :                "paVertexShaderConst";

        for (uint32_t i = 0; i < cConsts; i++)
            if (paConsts[i].fValid)
            {
                if (paConsts[i].ctype == SVGA3D_CONST_TYPE_FLOAT)
                    pHlp->pfnPrintf(pHlp, "%s[%#x(%u)] = [" FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR ", " FLOAT_FMT_STR "] ctype=FLOAT\n",
                                    pszName, i, i,
                                    FLOAT_FMT_ARGS(paConsts[i].value[0]), FLOAT_FMT_ARGS(paConsts[i].value[1]),
                                    FLOAT_FMT_ARGS(paConsts[i].value[2]), FLOAT_FMT_ARGS(paConsts[i].value[3]));
                else
                    pHlp->pfnPrintf(pHlp, "%s[%#x(%u)] = [%#x, %#x, %#x, %#x] ctype=%s\n",
                                    pszName, i, i,
                                    paConsts[i].value[0], paConsts[i].value[1],
                                    paConsts[i].value[2], paConsts[i].value[3],
                                    paConsts[i].ctype == SVGA3D_CONST_TYPE_INT ? "INT"
                                    : paConsts[i].ctype == SVGA3D_CONST_TYPE_BOOL ? "BOOL" : "UNKNOWN");
            }
    }

    for (uint32_t iWhich = 0; iWhich < 2; iWhich++)
    {
        uint32_t        cShaders  = iWhich == 0 ? pContext->cPixelShaders : pContext->cVertexShaders;
        PVMSVGA3DSHADER paShaders = iWhich == 0 ? pContext->paPixelShader : pContext->paVertexShader;
        const char     *pszName   = iWhich == 0 ? "paPixelShaders" : "paVertexShaders";
        for (uint32_t i = 0; i < cShaders; i++)
            if (paShaders[i].id == i)
            {
                pHlp->pfnPrintf(pHlp, "%s[%u]:   id=%#x cid=%#x type=%s(%d) cbData=%#x pvData=%p\n",
                                pszName, i,
                                paShaders[i].id,
                                paShaders[i].cid,
                                paShaders[i].type == SVGA3D_SHADERTYPE_VS ? "VS"
                                : paShaders[i].type == SVGA3D_SHADERTYPE_PS ? "PS" : "UNKNOWN",
                                paShaders[i].type,
                                paShaders[i].cbData,
                                paShaders[i].pShaderProgram);
            }
    }
}


void vmsvga3dInfoContextWorker(PVGASTATE pThis, PCDBGFINFOHLP pHlp, uint32_t cid, bool fVerbose)
{
    /* Warning! This code is currently racing papContexts reallocation! */
    /* Warning! This code is currently racing papContexts reallocation! */
    /* Warning! This code is currently racing papContexts reallocation! */
    VMSVGA3DSTATE volatile *pState = pThis->svga.p3dState;
    if (pState)
    {
        /*
         * Deal with a specific request first.
         */
        if (cid != UINT32_MAX)
        {
            if (cid < pState->cContexts)
            {
                PVMSVGA3DCONTEXT pContext = pState->papContexts[cid];
                if (pContext && pContext->id == cid)
                {
                    vmsvga3dInfoContextWorkerOne(pHlp, pContext, fVerbose);
                    return;
                }
            }
            pHlp->pfnPrintf(pHlp, "Context ID %#x not found.\n", cid);
        }
        else
        {
            /*
             * Dump all.
             */
            uint32_t cContexts = pState->cContexts;
            pHlp->pfnPrintf(pHlp, "cContexts=%d\n", cContexts);
            for (cid = 0; cid < cContexts; cid++)
            {
                PVMSVGA3DCONTEXT pContext = pState->papContexts[cid];
                if (pContext && pContext->id == cid)
                {
                    pHlp->pfnPrintf(pHlp, "\n");
                    vmsvga3dInfoContextWorkerOne(pHlp, pContext, fVerbose);
                }
            }
        }
    }
}

/** Values for SVGA3dTextureFilter, prefix SVGA3D_TEX_FILTER_. */
static const char * const g_apszTexureFilters[] =
{
    "NONE",
    "NEAREST",
    "LINEAR",
    "ANISOTROPIC",
    "FLATCUBIC",
    "GAUSSIANCUBIC",
    "PYRAMIDALQUAD",
    "GAUSSIANQUAD",
};

/** SVGA3dSurfaceFlags values, prefix SVGA3D_SURFACE_. */
static VMSVGAINFOFLAGS32 const g_aSvga3DSurfaceFlags[] =
{
    { SVGA3D_SURFACE_CUBEMAP            , "CUBEMAP" },
    { SVGA3D_SURFACE_HINT_STATIC        , "HINT_STATIC" },
    { SVGA3D_SURFACE_HINT_DYNAMIC       , "HINT_DYNAMIC" },
    { SVGA3D_SURFACE_HINT_INDEXBUFFER   , "HINT_INDEXBUFFER" },
    { SVGA3D_SURFACE_HINT_VERTEXBUFFER  , "HINT_VERTEXBUFFER" },
    { SVGA3D_SURFACE_HINT_TEXTURE       , "HINT_TEXTURE" },
    { SVGA3D_SURFACE_HINT_RENDERTARGET  , "HINT_RENDERTARGET" },
    { SVGA3D_SURFACE_HINT_DEPTHSTENCIL  , "HINT_DEPTHSTENCIL" },
    { SVGA3D_SURFACE_HINT_WRITEONLY     , "HINT_WRITEONLY" },
    { SVGA3D_SURFACE_MASKABLE_ANTIALIAS , "MASKABLE_ANTIALIAS" },
    { SVGA3D_SURFACE_AUTOGENMIPMAPS     , "AUTOGENMIPMAPS" },
};


#ifdef VMSVGA3D_DIRECT3D

/** Values for D3DFORMAT, prefix D3DFMT_. */
static VMSVGAINFOENUM const g_aD3DFormats[] =
{
    { D3DFMT_UNKNOWN        , "UNKNOWN" },
    { D3DFMT_R8G8B8         , "R8G8B8" },
    { D3DFMT_A8R8G8B8       , "A8R8G8B8" },
    { D3DFMT_X8R8G8B8       , "X8R8G8B8" },
    { D3DFMT_R5G6B5         , "R5G6B5" },
    { D3DFMT_X1R5G5B5       , "X1R5G5B5" },
    { D3DFMT_A1R5G5B5       , "A1R5G5B5" },
    { D3DFMT_A4R4G4B4       , "A4R4G4B4" },
    { D3DFMT_R3G3B2         , "R3G3B2" },
    { D3DFMT_A8             , "A8" },
    { D3DFMT_A8R3G3B2       , "A8R3G3B2" },
    { D3DFMT_X4R4G4B4       , "X4R4G4B4" },
    { D3DFMT_A2B10G10R10    , "A2B10G10R10" },
    { D3DFMT_A8B8G8R8       , "A8B8G8R8" },
    { D3DFMT_X8B8G8R8       , "X8B8G8R8" },
    { D3DFMT_G16R16         , "G16R16" },
    { D3DFMT_A2R10G10B10    , "A2R10G10B10" },
    { D3DFMT_A16B16G16R16   , "A16B16G16R16" },
    { D3DFMT_A8P8           , "A8P8" },
    { D3DFMT_P8             , "P8" },
    { D3DFMT_L8             , "L8" },
    { D3DFMT_A8L8           , "A8L8" },
    { D3DFMT_A4L4           , "A4L4" },
    { D3DFMT_V8U8           , "V8U8" },
    { D3DFMT_L6V5U5         , "L6V5U5" },
    { D3DFMT_X8L8V8U8       , "X8L8V8U8" },
    { D3DFMT_Q8W8V8U8       , "Q8W8V8U8" },
    { D3DFMT_V16U16         , "V16U16" },
    { D3DFMT_A2W10V10U10    , "A2W10V10U10" },
    { D3DFMT_D16_LOCKABLE   , "D16_LOCKABLE" },
    { D3DFMT_D32            , "D32" },
    { D3DFMT_D15S1          , "D15S1" },
    { D3DFMT_D24S8          , "D24S8" },
    { D3DFMT_D24X8          , "D24X8" },
    { D3DFMT_D24X4S4        , "D24X4S4" },
    { D3DFMT_D16            , "D16" },
    { D3DFMT_L16            , "L16" },
    { D3DFMT_D32F_LOCKABLE  , "D32F_LOCKABLE" },
    { D3DFMT_D24FS8         , "D24FS8" },
    { D3DFMT_VERTEXDATA     , "VERTEXDATA" },
    { D3DFMT_INDEX16        , "INDEX16" },
    { D3DFMT_INDEX32        , "INDEX32" },
    { D3DFMT_Q16W16V16U16   , "Q16W16V16U16" },
    { D3DFMT_R16F           , "R16F" },
    { D3DFMT_G16R16F        , "G16R16F" },
    { D3DFMT_A16B16G16R16F  , "A16B16G16R16F" },
    { D3DFMT_R32F           , "R32F" },
    { D3DFMT_G32R32F        , "G32R32F" },
    { D3DFMT_A32B32G32R32F  , "A32B32G32R32F" },
    { D3DFMT_CxV8U8         , "CxV8U8" },
    { D3DFMT_UYVY           , "UYVY" },
    { D3DFMT_YUY2           , "YUY2" },
    { D3DFMT_DXT1           , "DXT1" },
    { D3DFMT_DXT2           , "DXT2" },
    { D3DFMT_DXT3           , "DXT3" },
    { D3DFMT_DXT4           , "DXT4" },
    { D3DFMT_DXT5           , "DXT5" },
    { D3DFMT_MULTI2_ARGB8   , "MULTI2_ARGB8" },
    { D3DFMT_G8R8_G8B8      , "G8R8_G8B8" },
    { D3DFMT_R8G8_B8G8      , "R8G8_B8G8" },
    { D3DFMT_FORCE_DWORD    , "FORCE_DWORD" },
};
VMSVGAINFOENUMMAP_MAKE(static, g_D3DFormat2String, g_aD3DFormats, "D3DFMT_");

/** Values for D3DMULTISAMPLE_TYPE, prefix D3DMULTISAMPLE_. */
static VMSVGAINFOENUM const g_aD3DMultiSampleTypes[] =
{
    { D3DMULTISAMPLE_NONE           , "NONE" },
    { D3DMULTISAMPLE_NONMASKABLE    , "NONMASKABLE" },
    { D3DMULTISAMPLE_2_SAMPLES      , "2_SAMPLES" },
    { D3DMULTISAMPLE_3_SAMPLES      , "3_SAMPLES" },
    { D3DMULTISAMPLE_4_SAMPLES      , "4_SAMPLES" },
    { D3DMULTISAMPLE_5_SAMPLES      , "5_SAMPLES" },
    { D3DMULTISAMPLE_6_SAMPLES      , "6_SAMPLES" },
    { D3DMULTISAMPLE_7_SAMPLES      , "7_SAMPLES" },
    { D3DMULTISAMPLE_8_SAMPLES      , "8_SAMPLES" },
    { D3DMULTISAMPLE_9_SAMPLES      , "9_SAMPLES" },
    { D3DMULTISAMPLE_10_SAMPLES     , "10_SAMPLES" },
    { D3DMULTISAMPLE_11_SAMPLES     , "11_SAMPLES" },
    { D3DMULTISAMPLE_12_SAMPLES     , "12_SAMPLES" },
    { D3DMULTISAMPLE_13_SAMPLES     , "13_SAMPLES" },
    { D3DMULTISAMPLE_14_SAMPLES     , "14_SAMPLES" },
    { D3DMULTISAMPLE_15_SAMPLES     , "15_SAMPLES" },
    { D3DMULTISAMPLE_16_SAMPLES     , "16_SAMPLES" },
    { D3DMULTISAMPLE_FORCE_DWORD    , "FORCE_DWORD" },
};
VMSVGAINFOENUMMAP_MAKE(static, g_D3DMultiSampleType2String, g_aD3DMultiSampleTypes, "D3DMULTISAMPLE_");

/** D3DUSAGE_XXX flag value, prefix D3DUSAGE_. */
static VMSVGAINFOFLAGS32 const g_aD3DUsageFlags[] =
{
    { D3DUSAGE_RENDERTARGET                     , "RENDERTARGET" },
    { D3DUSAGE_DEPTHSTENCIL                     , "DEPTHSTENCIL" },
    { D3DUSAGE_WRITEONLY                        , "WRITEONLY" },
    { D3DUSAGE_SOFTWAREPROCESSING               , "SOFTWAREPROCESSING" },
    { D3DUSAGE_DONOTCLIP                        , "DONOTCLIP" },
    { D3DUSAGE_POINTS                           , "POINTS" },
    { D3DUSAGE_RTPATCHES                        , "RTPATCHES" },
    { D3DUSAGE_NPATCHES                         , "NPATCHES" },
    { D3DUSAGE_DYNAMIC                          , "DYNAMIC" },
    { D3DUSAGE_AUTOGENMIPMAP                    , "AUTOGENMIPMAP" },
    { D3DUSAGE_RESTRICTED_CONTENT               , "RESTRICTED_CONTENT" },
    { D3DUSAGE_RESTRICT_SHARED_RESOURCE_DRIVER  , "RESTRICT_SHARED_RESOURCE_DRIVER" },
    { D3DUSAGE_RESTRICT_SHARED_RESOURCE         , "RESTRICT_SHARED_RESOURCE" },
    { D3DUSAGE_DMAP                             , "DMAP" },
    { D3DUSAGE_NONSECURE                        , "NONSECURE" },
    { D3DUSAGE_TEXTAPI                          , "TEXTAPI" },
};


/**
 * Release all shared surface objects.
 */
static DECLCALLBACK(int) vmsvga3dInfoSharedObjectCallback(PAVLU32NODECORE pNode, void *pvUser)
{
    PVMSVGA3DSHAREDSURFACE  pSharedSurface = (PVMSVGA3DSHAREDSURFACE)pNode;
    PCDBGFINFOHLP           pHlp           = (PCDBGFINFOHLP)pvUser;

    pHlp->pfnPrintf(pHlp, "Shared surface:          %#x  pv=%p\n", pSharedSurface->Core.Key, pSharedSurface->u.pCubeTexture);

    return 0;
}

#elif defined(VMSVGA3D_OPENGL)
    /** @todo   */

#else
# error "Build config error."
#endif


static void vmsvga3dInfoSurfaceWorkerOne(PCDBGFINFOHLP pHlp, PVMSVGA3DSURFACE pSurface,
                                         bool fVerbose, uint32_t cxAscii, bool fInvY)
{
    char szTmp[128];

    pHlp->pfnPrintf(pHlp, "*** VMSVGA 3d surface %#x (%d)%s ***\n", pSurface->id, pSurface->id, pSurface->fDirty ? " - dirty" : "");
#if defined(VMSVGA3D_OPENGL) && defined(VMSVGA3D_OGL_WITH_SHARED_CTX)
    pHlp->pfnPrintf(pHlp, "idWeakContextAssociation: %#x\n", pSurface->idWeakContextAssociation);
#else
    pHlp->pfnPrintf(pHlp, "idAssociatedContext:     %#x\n", pSurface->idAssociatedContext);
#endif
    pHlp->pfnPrintf(pHlp, "Format:                  %s\n",
                    vmsvgaFormatEnumValueEx(szTmp, sizeof(szTmp), NULL, (int)pSurface->format, false, &g_SVGA3dSurfaceFormat2String));
    pHlp->pfnPrintf(pHlp, "Flags:                   %#x", pSurface->flags);
    vmsvga3dInfoU32Flags(pHlp, pSurface->flags, "SVGA3D_SURFACE_", g_aSvga3DSurfaceFlags, RT_ELEMENTS(g_aSvga3DSurfaceFlags));
    pHlp->pfnPrintf(pHlp, "\n");
    if (pSurface->cFaces == 0)
        pHlp->pfnPrintf(pHlp, "Faces:                   %u\n", pSurface->cFaces);
    for (uint32_t iFace = 0; iFace < pSurface->cFaces; iFace++)
    {
        Assert(pSurface->faces[iFace].numMipLevels <= pSurface->faces[0].numMipLevels);
        if (pSurface->faces[iFace].numMipLevels == 0)
            pHlp->pfnPrintf(pHlp, "Faces[%u] Mipmap levels:  %u\n", iFace, pSurface->faces[iFace].numMipLevels);

        uint32_t iMipmap = iFace * pSurface->faces[0].numMipLevels;
        for (uint32_t iLevel = 0; iLevel < pSurface->faces[iFace].numMipLevels; iLevel++, iMipmap++)
        {
            pHlp->pfnPrintf(pHlp, "Face #%u, mipmap #%u[%u]:%s  cx=%u, cy=%u, cz=%u, cbSurface=%#x, cbPitch=%#x",
                            iFace, iLevel, iMipmap, iMipmap < 10 ? " " : "",
                            pSurface->pMipmapLevels[iMipmap].size.width,
                            pSurface->pMipmapLevels[iMipmap].size.height,
                            pSurface->pMipmapLevels[iMipmap].size.depth,
                            pSurface->pMipmapLevels[iMipmap].cbSurface,
                            pSurface->pMipmapLevels[iMipmap].cbSurfacePitch);
            if (pSurface->pMipmapLevels[iMipmap].pSurfaceData)
                pHlp->pfnPrintf(pHlp, " pvData=%p", pSurface->pMipmapLevels[iMipmap].pSurfaceData);
            if (pSurface->pMipmapLevels[iMipmap].fDirty)
                pHlp->pfnPrintf(pHlp, " dirty");
            pHlp->pfnPrintf(pHlp, "\n");
        }
    }

    pHlp->pfnPrintf(pHlp, "cbBlock:                 %u (%#x)\n", pSurface->cbBlock, pSurface->cbBlock);
    pHlp->pfnPrintf(pHlp, "Multi-sample count:      %u\n", pSurface->multiSampleCount);
    pHlp->pfnPrintf(pHlp, "Autogen filter:          %s\n",
                    vmsvgaFormatEnumValue(szTmp, sizeof(szTmp), NULL, pSurface->autogenFilter,
                                          "SVGA3D_TEX_FILTER_", g_apszTexureFilters, RT_ELEMENTS(g_apszTexureFilters)));

#ifdef VMSVGA3D_DIRECT3D
    pHlp->pfnPrintf(pHlp, "formatD3D:               %s\n",
                    vmsvgaFormatEnumValueEx(szTmp, sizeof(szTmp), NULL, pSurface->formatD3D, true, &g_D3DFormat2String));
    pHlp->pfnPrintf(pHlp, "fUsageD3D:               %#x", pSurface->fUsageD3D);
    vmsvga3dInfoU32Flags(pHlp, pSurface->fUsageD3D, "D3DUSAGE_", g_aD3DUsageFlags, RT_ELEMENTS(g_aD3DUsageFlags));
    pHlp->pfnPrintf(pHlp, "\n");
    pHlp->pfnPrintf(pHlp, "multiSampleTypeD3D:      %s\n",
                    vmsvgaFormatEnumValueEx(szTmp, sizeof(szTmp), NULL, pSurface->multiSampleTypeD3D,
                                            true, &g_D3DMultiSampleType2String));
    if (pSurface->hSharedObject != NULL)
        pHlp->pfnPrintf(pHlp, "hSharedObject:           %p\n", pSurface->hSharedObject);
    if (pSurface->pQuery)
        pHlp->pfnPrintf(pHlp, "pQuery:                  %p\n", pSurface->pQuery);
    if (pSurface->u.pSurface)
        pHlp->pfnPrintf(pHlp, "u.pXxxx:                 %p\n", pSurface->u.pSurface);
    if (pSurface->bounce.pTexture)
        pHlp->pfnPrintf(pHlp, "bounce.pXxxx:            %p\n", pSurface->bounce.pTexture);
    RTAvlU32DoWithAll(&pSurface->pSharedObjectTree, true /*fFromLeft*/, vmsvga3dInfoSharedObjectCallback, (void *)pHlp);
    pHlp->pfnPrintf(pHlp, "fStencilAsTexture:       %RTbool\n", pSurface->fStencilAsTexture);

#elif defined(VMSVGA3D_OPENGL)
    /** @todo   */
#else
# error "Build config error."
#endif

    if (fVerbose)
        for (uint32_t iFace = 0; iFace < pSurface->cFaces; iFace++)
        {
            uint32_t iMipmap = iFace * pSurface->faces[0].numMipLevels;
            for (uint32_t iLevel = 0; iLevel < pSurface->faces[iFace].numMipLevels; iLevel++, iMipmap++)
                if (pSurface->pMipmapLevels[iMipmap].pSurfaceData)
                {
                    if (  ASMMemIsAll8(pSurface->pMipmapLevels[iMipmap].pSurfaceData,
                                       pSurface->pMipmapLevels[iMipmap].cbSurface, 0) == NULL)
                        pHlp->pfnPrintf(pHlp, "--- Face #%u, mipmap #%u[%u]: all zeros ---\n", iFace, iLevel, iMipmap);
                    else
                    {
                        pHlp->pfnPrintf(pHlp, "--- Face #%u, mipmap #%u[%u]: cx=%u, cy=%u, cz=%u ---\n",
                                        iFace, iLevel, iMipmap,
                                        pSurface->pMipmapLevels[iMipmap].size.width,
                                        pSurface->pMipmapLevels[iMipmap].size.height,
                                        pSurface->pMipmapLevels[iMipmap].size.depth);
                        vmsvga3dAsciiPrint(vmsvga3dAsciiPrintlnInfo, (void *)pHlp,
                                           pSurface->pMipmapLevels[iMipmap].pSurfaceData,
                                           pSurface->pMipmapLevels[iMipmap].cbSurface,
                                           pSurface->pMipmapLevels[iMipmap].size.width,
                                           pSurface->pMipmapLevels[iMipmap].size.height,
                                           pSurface->pMipmapLevels[iMipmap].cbSurfacePitch,
                                           pSurface->format,
                                           fInvY,
                                           cxAscii, cxAscii * 3 / 4);
                    }
                }
        }
}


void vmsvga3dInfoSurfaceWorker(PVGASTATE pThis, PCDBGFINFOHLP pHlp, uint32_t sid, bool fVerbose, uint32_t cxAscii, bool fInvY)
{
    /* Warning! This code is currently racing papSurfaces reallocation! */
    /* Warning! This code is currently racing papSurfaces reallocation! */
    /* Warning! This code is currently racing papSurfaces reallocation! */
    VMSVGA3DSTATE volatile *pState = pThis->svga.p3dState;
    if (pState)
    {
        /*
         * Deal with a specific request first.
         */
        if (sid != UINT32_MAX)
        {
            if (sid < pState->cSurfaces)
            {
                PVMSVGA3DSURFACE pSurface = pState->papSurfaces[sid];
                if (pSurface && pSurface->id == sid)
                {
                    if (fVerbose)
                        vmsvga3dSurfaceUpdateHeapBuffersOnFifoThread(pThis, sid);
                    vmsvga3dInfoSurfaceWorkerOne(pHlp, pSurface, fVerbose, cxAscii, fInvY);
                    return;
                }
            }
            pHlp->pfnPrintf(pHlp, "Surface ID %#x not found.\n", sid);
        }
        else
        {
            /*
             * Dump all.
             */
            if (fVerbose)
                vmsvga3dSurfaceUpdateHeapBuffersOnFifoThread(pThis, UINT32_MAX);
            uint32_t cSurfaces = pState->cSurfaces;
            pHlp->pfnPrintf(pHlp, "cSurfaces=%d\n", cSurfaces);
            for (sid = 0; sid < cSurfaces; sid++)
            {
                PVMSVGA3DSURFACE pSurface = pState->papSurfaces[sid];
                if (pSurface && pSurface->id == sid)
                {
                    pHlp->pfnPrintf(pHlp, "\n");
                    vmsvga3dInfoSurfaceWorkerOne(pHlp, pSurface, fVerbose, cxAscii, fInvY);
                }
            }
        }
    }

}


#endif /* !___DevVGA_SVGA3d_shared_h___ */

