/* $Id$ */

/** @file
 * Blitter API
 */
/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___cr_blitter_h__
#define ___cr_blitter_h__

#include <iprt/cdefs.h>
#include <iprt/asm.h>
#include "cr_spu.h"
#include "cr_vreg.h"

#ifndef IN_RING0
# define VBOXBLITTERDECL(_type) DECLEXPORT(_type)
#else
# define VBOXBLITTERDECL(_type) RTDECL(_type)
#endif

RT_C_DECLS_BEGIN
/* GLSL Cache */
typedef struct CR_GLSL_CACHE
{
    int iGlVersion;
    GLuint uNoAlpha2DProg;
    GLuint uNoAlpha2DRectProg;
    SPUDispatchTable *pDispatch;
} CR_GLSL_CACHE;

DECLINLINE(void) CrGlslInit(CR_GLSL_CACHE *pCache, SPUDispatchTable *pDispatch)
{
    memset(pCache, 0, sizeof (*pCache));
    pCache->pDispatch = pDispatch;
}

DECLINLINE(bool) CrGlslIsInited(const CR_GLSL_CACHE *pCache)
{
    return !!pCache->pDispatch;
}

/* clients should set proper context before calling these funcs */
VBOXBLITTERDECL(bool) CrGlslIsSupported(CR_GLSL_CACHE *pCache);
VBOXBLITTERDECL(int) CrGlslProgGenAllNoAlpha(CR_GLSL_CACHE *pCache);
VBOXBLITTERDECL(int) CrGlslProgGenNoAlpha(CR_GLSL_CACHE *pCache, GLenum enmTexTarget);
VBOXBLITTERDECL(int) CrGlslProgUseGenNoAlpha(CR_GLSL_CACHE *pCache, GLenum enmTexTarget);
VBOXBLITTERDECL(int) CrGlslProgUseNoAlpha(const CR_GLSL_CACHE *pCache, GLenum enmTexTarget);
VBOXBLITTERDECL(void) CrGlslProgClear(const CR_GLSL_CACHE *pCache);
VBOXBLITTERDECL(bool) CrGlslNeedsCleanup(const CR_GLSL_CACHE *pCache);
VBOXBLITTERDECL(void) CrGlslCleanup(CR_GLSL_CACHE *pCache);
VBOXBLITTERDECL(void) CrGlslTerm(CR_GLSL_CACHE *pCache);

/* BLITTER */
typedef struct CR_BLITTER_BUFFER
{
    GLuint cbBuffer;
    GLvoid * pvBuffer;
} CR_BLITTER_BUFFER, *PCR_BLITTER_BUFFER;

typedef union CR_BLITTER_FLAGS
{
    struct
    {
        uint32_t Initialized         : 1;
        uint32_t CtxCreated          : 1;
        uint32_t SupportsFBO         : 1;
        uint32_t SupportsPBO         : 1;
        uint32_t CurrentMuralChanged : 1;
        uint32_t LastWasFBODraw      : 1;
        uint32_t ForceDrawBlit       : 1;
        uint32_t ShadersGloal        : 1;
        uint32_t Reserved            : 24;
    };
    uint32_t Value;
} CR_BLITTER_FLAGS, *PCR_BLITTER_FLAGS;

struct CR_BLITTER;

typedef DECLCALLBACK(int) FNCRBLT_BLITTER(struct CR_BLITTER *pBlitter, const VBOXVR_TEXTURE *pSrc, const RTRECT *paSrcRect, const RTRECTSIZE *pDstSize, const RTRECT *paDstRect, uint32_t cRects, uint32_t fFlags);
typedef FNCRBLT_BLITTER *PFNCRBLT_BLITTER;

typedef struct CR_BLITTER_SPUITEM
{
    int id;
    GLint visualBits;
} CR_BLITTER_SPUITEM, *PCR_BLITTER_SPUITEM;

typedef struct CR_BLITTER_CONTEXT
{
    CR_BLITTER_SPUITEM Base;
} CR_BLITTER_CONTEXT, *PCR_BLITTER_CONTEXT;

typedef struct CR_BLITTER_WINDOW
{
    CR_BLITTER_SPUITEM Base;
    GLuint width, height;
} CR_BLITTER_WINDOW, *PCR_BLITTER_WINDOW;

typedef struct CR_BLITTER_IMG
{
    void *pvData;
    GLuint cbData;
    GLenum enmFormat;
    GLuint width, height;
    GLuint bpp;
    GLuint pitch;
} CR_BLITTER_IMG, *PCR_BLITTER_IMG;

typedef struct CR_BLITTER
{
    GLuint idFBO;
    CR_BLITTER_FLAGS Flags;
    uint32_t cEnters;
    PFNCRBLT_BLITTER pfnBlt;
    CR_BLITTER_BUFFER Verticies;
    CR_BLITTER_BUFFER Indicies;
    RTRECTSIZE CurrentSetSize;
    CR_BLITTER_WINDOW CurrentMural;
    CR_BLITTER_CONTEXT CtxInfo;
    int32_t i32MakeCurrentUserData;
    SPUDispatchTable *pDispatch;
    const CR_GLSL_CACHE *pGlslCache;
    CR_GLSL_CACHE LocalGlslCache;
} CR_BLITTER, *PCR_BLITTER;

DECLINLINE(GLboolean) CrBltIsInitialized(PCR_BLITTER pBlitter)
{
    return !!pBlitter->pDispatch;
}

VBOXBLITTERDECL(int) CrBltInit(PCR_BLITTER pBlitter, const CR_BLITTER_CONTEXT *pCtxBase, bool fCreateNewCtx, bool fForceDrawBlt, const CR_GLSL_CACHE *pShaders, SPUDispatchTable *pDispatch);

VBOXBLITTERDECL(void) CrBltTerm(PCR_BLITTER pBlitter);

VBOXBLITTERDECL(int) CrBltCleanup(PCR_BLITTER pBlitter);

DECLINLINE(GLboolean) CrBltSupportsTexTex(PCR_BLITTER pBlitter)
{
    return pBlitter->Flags.SupportsFBO;
}

DECLINLINE(GLboolean) CrBltIsEntered(PCR_BLITTER pBlitter)
{
    return !!pBlitter->cEnters;
}

DECLINLINE(GLint) CrBltGetVisBits(PCR_BLITTER pBlitter)
{
    return pBlitter->CtxInfo.Base.visualBits;
}


DECLINLINE(GLboolean) CrBltIsEverEntered(PCR_BLITTER pBlitter)
{
    return !!pBlitter->Flags.Initialized;
}

DECLINLINE(void) CrBltSetMakeCurrentUserData(PCR_BLITTER pBlitter, int32_t i32MakeCurrentUserData)
{
    pBlitter->i32MakeCurrentUserData = i32MakeCurrentUserData;
}

VBOXBLITTERDECL(int) CrBltMuralSetCurrentInfo(PCR_BLITTER pBlitter, const CR_BLITTER_WINDOW *pMural);
DECLINLINE(const CR_BLITTER_WINDOW *) CrBltMuralGetCurrentInfo(PCR_BLITTER pBlitter)
{
    return &pBlitter->CurrentMural;
}

VBOXBLITTERDECL(void) CrBltCheckUpdateViewport(PCR_BLITTER pBlitter);

VBOXBLITTERDECL(void) CrBltLeave(PCR_BLITTER pBlitter);
VBOXBLITTERDECL(int) CrBltEnter(PCR_BLITTER pBlitter);
VBOXBLITTERDECL(void) CrBltBlitTexMural(PCR_BLITTER pBlitter, bool fBb, const VBOXVR_TEXTURE *pSrc, const RTRECT *paSrcRects, const RTRECT *paDstRects, uint32_t cRects, uint32_t fFlags);
VBOXBLITTERDECL(void) CrBltBlitTexTex(PCR_BLITTER pBlitter, const VBOXVR_TEXTURE *pSrc, const RTRECT *pSrcRect, const VBOXVR_TEXTURE *pDst, const RTRECT *pDstRect, uint32_t cRects, uint32_t fFlags);
VBOXBLITTERDECL(int) CrBltImgGetTex(PCR_BLITTER pBlitter, const VBOXVR_TEXTURE *pSrc, GLenum enmFormat, CR_BLITTER_IMG *pDst);

VBOXBLITTERDECL(int) CrBltImgGetMural(PCR_BLITTER pBlitter, bool fBb, CR_BLITTER_IMG *pDst);
VBOXBLITTERDECL(void) CrBltImgFree(PCR_BLITTER pBlitter, CR_BLITTER_IMG *pDst);
VBOXBLITTERDECL(void) CrBltPresent(PCR_BLITTER pBlitter);
/* */
struct CR_TEXDATA;

typedef DECLCALLBACK(void) FNCRTEXDATA_RELEASED(struct CR_TEXDATA *pTexture);
typedef FNCRTEXDATA_RELEASED *PFNCRTEXDATA_RELEASED;

typedef union CR_TEXDATA_FLAGS
{
    struct
    {
        uint32_t DataValid           : 1;
        uint32_t DataAcquired        : 1;
        uint32_t DataInverted        : 1;
        uint32_t Entered             : 1;
        uint32_t Reserved            : 28;
    };
    uint32_t Value;
} CR_TEXDATA_FLAGS, *PCR_TEXDATA_FLAGS;


typedef struct CR_TEXDATA
{
    VBOXVR_TEXTURE Tex;
    volatile uint32_t cRefs;
    /* fields specific to texture data download */
    uint32_t idInvertTex;
    uint32_t idPBO;
    CR_TEXDATA_FLAGS Flags;
    PCR_BLITTER pBlitter;
    CR_BLITTER_IMG Img;
    /*dtor*/
    PFNCRTEXDATA_RELEASED pfnTextureReleased;
    struct CR_TEXDATA *pScaledCache;
} CR_TEXDATA, *PCR_TEXDATA;

DECLINLINE(void) CrTdInit(PCR_TEXDATA pTex, const VBOXVR_TEXTURE *pVrTex, PCR_BLITTER pBlitter, PFNCRTEXDATA_RELEASED pfnTextureReleased)
{
    memset(pTex, 0, sizeof (*pTex));
    pTex->Tex = *pVrTex;
    pTex->cRefs = 1;
    pTex->pBlitter = pBlitter;
    pTex->pfnTextureReleased = pfnTextureReleased;
}

DECLINLINE(const VBOXVR_TEXTURE*) CrTdTexGet(const CR_TEXDATA *pTex)
{
    return &pTex->Tex;
}

DECLINLINE(PCR_BLITTER) CrTdBlitterGet(CR_TEXDATA *pTex)
{
    return pTex->pBlitter;
}

DECLINLINE(int) CrTdBltEnter(PCR_TEXDATA pTex)
{
    int rc;
	if (pTex->Flags.Entered)
		return VERR_INVALID_STATE;
	rc = CrBltEnter(pTex->pBlitter);
    if (!RT_SUCCESS(rc))
    {
        WARN(("CrBltEnter failed rc %d", rc));
        return rc;
    }
	pTex->Flags.Entered = 1;
	return VINF_SUCCESS;
}

DECLINLINE(bool) CrTdBltIsEntered(PCR_TEXDATA pTex)
{
	return pTex->Flags.Entered;
}

DECLINLINE(void) CrTdBltLeave(PCR_TEXDATA pTex)
{
	if (!pTex->Flags.Entered)
	{
		WARN(("invalid Blt Leave"));
		return;
	}

	CrBltLeave(pTex->pBlitter);

	pTex->Flags.Entered = 0;
}

/* the CrTdBltXxx calls are done with the entered blitter */
/* acquire the texture data, returns the cached data in case it is cached.
 * the data remains cached in the CR_TEXDATA object until it is discarded with CrTdBltDataFree or CrTdBltDataCleanup.
 * */
VBOXBLITTERDECL(int) CrTdBltDataAcquire(PCR_TEXDATA pTex, GLenum enmFormat, bool fInverted, const CR_BLITTER_IMG**ppImg);

VBOXBLITTERDECL(int) CrTdBltDataAcquireScaled(PCR_TEXDATA pTex, GLenum enmFormat, bool fInverted, uint32_t width, uint32_t height, const CR_BLITTER_IMG**ppImg);

VBOXBLITTERDECL(int) CrTdBltDataReleaseScaled(PCR_TEXDATA pTex, const CR_BLITTER_IMG *pImg);

VBOXBLITTERDECL(void) CrTdBltScaleCacheMoveTo(PCR_TEXDATA pTex, PCR_TEXDATA pDstTex);

/* release the texture data, the data remains cached in the CR_TEXDATA object until it is discarded with CrTdBltDataFree or CrTdBltDataCleanup */
VBOXBLITTERDECL(int) CrTdBltDataRelease(PCR_TEXDATA pTex);
/* discard the texture data cached with previous CrTdBltDataAcquire.
 * Must be called wit data released (CrTdBltDataRelease) */
VBOXBLITTERDECL(int) CrTdBltDataFree(PCR_TEXDATA pTex);
VBOXBLITTERDECL(int) CrTdBltDataFreeNe(PCR_TEXDATA pTex);
VBOXBLITTERDECL(void) CrTdBltDataInvalidateNe(PCR_TEXDATA pTex);
/* does same as CrTdBltDataFree, and in addition cleans up.
 * this is kind of a texture destructor, which clients should call on texture object destruction, e.g. from the PFNCRTEXDATA_RELEASED callback */
VBOXBLITTERDECL(int) CrTdBltDataCleanup(PCR_TEXDATA pTex);

VBOXBLITTERDECL(int) CrTdBltDataCleanupNe(PCR_TEXDATA pTex);

DECLINLINE(uint32_t) CrTdAddRef(PCR_TEXDATA pTex)
{
    return ASMAtomicIncU32(&pTex->cRefs);
}

DECLINLINE(uint32_t) CrTdRelease(PCR_TEXDATA pTex)
{
    uint32_t cRefs = ASMAtomicDecU32(&pTex->cRefs);
    if (!cRefs)
    {
        if (pTex->pfnTextureReleased)
            pTex->pfnTextureReleased(pTex);
        else
            CrTdBltDataCleanupNe(pTex);
	}

    return cRefs;
}

RT_C_DECLS_END

#endif /* #ifndef ___cr_blitter_h__ */
