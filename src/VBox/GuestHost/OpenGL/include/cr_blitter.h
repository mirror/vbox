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
#include "cr_spu.h"
#include "cr_vreg.h"

#ifndef IN_RING0
# define VBOXBLITTERDECL(_type) DECLEXPORT(_type)
#else
# define VBOXBLITTERDECL(_type) RTDECL(_type)
#endif

RT_C_DECLS_BEGIN

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
        uint32_t SupportsFBOBlit     : 1;
        uint32_t CurrentMuralChanged : 1;
        uint32_t LastWasFBODraw      : 1;
        uint32_t Reserved            : 26;
    };
    uint32_t Value;
} CR_BLITTER_FLAGS, *PCR_BLITTER_FLAGS;

struct CR_BLITTER;

typedef DECLCALLBACK(int) FNCRBLT_BLITTER(struct CR_BLITTER *pBlitter, VBOXVR_TEXTURE *pSrc, const RTRECT *paSrcRect, const PRTRECTSIZE pDstSize, const RTRECT *paDstRect, uint32_t cRects, uint32_t fFlags);
typedef FNCRBLT_BLITTER *PFNCRBLT_BLITTER;

#define CRBLT_F_LINEAR 0x00000001

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

typedef struct CR_BLITTER
{
    GLuint idFBO;
    CR_BLITTER_FLAGS Flags;
    PFNCRBLT_BLITTER pfnBlt;
    CR_BLITTER_BUFFER Verticies;
    CR_BLITTER_BUFFER Indicies;
    RTRECTSIZE CurrentSetSize;
    CR_BLITTER_WINDOW *pCurrentMural;
    CR_BLITTER_CONTEXT CtxInfo;
    CR_BLITTER_CONTEXT *pRestoreCtxInfo;
    CR_BLITTER_WINDOW *pRestoreMural;
    int32_t i32MakeCurrentUserData;
    SPUDispatchTable *pDispatch;
} CR_BLITTER, *PCR_BLITTER;

VBOXBLITTERDECL(int) CrBltInit(PCR_BLITTER pBlitter, CR_BLITTER_CONTEXT *pCtxBase, bool fCreateNewCtx, SPUDispatchTable *pDispatch);
VBOXBLITTERDECL(void) CrBltTerm(PCR_BLITTER pBlitter);

DECLINLINE(GLboolean) CrBltSupportsTexTex(PCR_BLITTER pBlitter)
{
    return pBlitter->Flags.SupportsFBO;
}

DECLINLINE(GLboolean) CrBltIsEntered(PCR_BLITTER pBlitter)
{
    return !!pBlitter->pRestoreCtxInfo;
}

DECLINLINE(GLboolean) CrBltIsEverEntered(PCR_BLITTER pBlitter)
{
    return !!pBlitter->Flags.Initialized;
}

DECLINLINE(void) CrBltSetMakeCurrentUserData(PCR_BLITTER pBlitter, int32_t i32MakeCurrentUserData)
{
    pBlitter->i32MakeCurrentUserData = i32MakeCurrentUserData;
}

VBOXBLITTERDECL(void) CrBltMuralSetCurrent(PCR_BLITTER pBlitter, CR_BLITTER_WINDOW *pMural);
DECLINLINE(CR_BLITTER_WINDOW *) CrBltMuralGetCurrent(PCR_BLITTER pBlitter)
{
    return pBlitter->pCurrentMural;
}

VBOXBLITTERDECL(void) CrBltLeave(PCR_BLITTER pBlitter);
VBOXBLITTERDECL(int) CrBltEnter(PCR_BLITTER pBlitter, CR_BLITTER_CONTEXT *pRestoreCtxInfo, CR_BLITTER_WINDOW *pRestoreMural);
VBOXBLITTERDECL(void) CrBltBlitTexMural(PCR_BLITTER pBlitter, VBOXVR_TEXTURE *pSrc, const RTRECT *paSrcRects, const RTRECT *paDstRects, uint32_t cRects, uint32_t fFlags);
VBOXBLITTERDECL(void) CrBltBlitTexTex(PCR_BLITTER pBlitter, VBOXVR_TEXTURE *pSrc, const RTRECT *pSrcRect, VBOXVR_TEXTURE *pDst, const RTRECT *pDstRect, uint32_t cRects, uint32_t fFlags);
VBOXBLITTERDECL(void) CrBltPresent(PCR_BLITTER pBlitter);
/* */

RT_C_DECLS_END

#endif /* #ifndef ___cr_blitter_h__ */
