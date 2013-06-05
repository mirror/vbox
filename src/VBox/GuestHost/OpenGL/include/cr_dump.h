/* $Id$ */

/** @file
 * Debugging: Dump API
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
#ifndef ___cr_dump_h__
#define ___cr_dump_h__

/* dump stuff */
//#define VBOX_WITH_CRDUMPER
#ifdef VBOX_WITH_CRDUMPER

#include <iprt/cdefs.h>
#include <iprt/string.h>
#include <cr_spu.h>
#include <cr_glstate.h>
#include <cr_blitter.h>

# define VBOXDUMPDECL(_type) DECLEXPORT(_type)

RT_C_DECLS_BEGIN

DECLEXPORT(void) crDmpDumpImgDmlBreak(struct CR_DUMPER * pDumper, CR_BLITTER_IMG *pImg, const char*pszEntryDesc);

DECLEXPORT(void) crDmpDumpStrDbgPrint(struct CR_DUMPER * pDumper, const char*pszStr);

struct CR_DUMPER;

typedef DECLCALLBACKPTR(void, PFNCRDUMPIMG)(struct CR_DUMPER * pDumper, CR_BLITTER_IMG *pImg, const char*pszEntryDesc);
typedef DECLCALLBACKPTR(void, PFNCRDUMPSTR)(struct CR_DUMPER * pDumper, const char*pszStr);

typedef struct CR_DUMPER
{
    PFNCRDUMPIMG pfnDumpImg;
    PFNCRDUMPSTR pfnDumpStr;
} CR_DUMPER;

#define crDmpImg(_pDumper, _pImg, _pDesc) do { \
            (_pDumper)->pfnDumpImg((_pDumper), (_pImg), (_pDesc)); \
        } while (0)

#define crDmpStr(_pDumper, _pDesc) do { \
            (_pDumper)->pfnDumpStr((_pDumper), (_pDesc)); \
        } while (0)

DECLINLINE(void) crDmpStrV(CR_DUMPER *pDumper, const char *pszStr, va_list pArgList)
{
    char szBuffer[4096] = {0};
    vsprintf_s(szBuffer, sizeof (szBuffer), pszStr, pArgList);
    crDmpStr(pDumper, szBuffer);
}

DECLINLINE(void) crDmpStrF(CR_DUMPER *pDumper, const char *pszStr, ...)
{
    va_list pArgList;
    va_start(pArgList, pszStr);
    crDmpStrV(pDumper, pszStr, pArgList);
    va_end(pArgList);
}

DECLINLINE(void) crDmpImgV(CR_DUMPER *pDumper, CR_BLITTER_IMG *pImg, const char *pszStr, va_list pArgList)
{
    char szBuffer[4096] = {0};
    vsprintf_s(szBuffer, sizeof (szBuffer), pszStr, pArgList);
    crDmpImg(pDumper, pImg, szBuffer);
}

DECLINLINE(void) crDmpImgF(CR_DUMPER *pDumper, CR_BLITTER_IMG *pImg, const char *pszStr, ...)
{
    va_list pArgList;
    va_start(pArgList, pszStr);
    crDmpImgV(pDumper, pImg, pszStr, pArgList);
    va_end(pArgList);
}

VBOXDUMPDECL(size_t) crDmpFormatArrayf(char *pString, size_t cbString, const float *pVal, uint32_t cVal);
VBOXDUMPDECL(size_t) crDmpFormatRawArrayf(char *pString, size_t cbString, const float *pVal, uint32_t cVal);
VBOXDUMPDECL(size_t) crDmpFormatMatrixArrayf(char *pString, size_t cbString, const float *pVal, uint32_t cX, uint32_t cY);

typedef struct CR_DBGPRINT_DUMPER
{
    CR_DUMPER Base;
} CR_DBGPRINT_DUMPER;

typedef struct CR_HTML_DUMPER
{
    CR_DUMPER Base;
    const char* pszFile;
    const char* pszDir;
    FILE *pFile;
    uint32_t cImg;
} CR_HTML_DUMPER;

DECLEXPORT(int) crDmpHtmlInit(struct CR_HTML_DUMPER * pDumper, const char *pszDir, const char *pszFile);

DECLINLINE(void) crDmpDbgPrintInit(CR_DBGPRINT_DUMPER *pDumper)
{
    pDumper->Base.pfnDumpImg = crDmpDumpImgDmlBreak;
    pDumper->Base.pfnDumpStr = crDmpDumpStrDbgPrint;
}

typedef struct CR_RECORDER
{
    CR_BLITTER *pBlitter;
    SPUDispatchTable *pDispatch;
    CR_DUMPER *pDumper;
} CR_RECORDER;

DECLINLINE(void) crRecInit(CR_RECORDER *pRec, CR_BLITTER *pBlitter, SPUDispatchTable *pDispatch, CR_DUMPER *pDumper)
{
    pRec->pBlitter = pBlitter;
    pRec->pDispatch = pDispatch;
    pRec->pDumper = pDumper;
}

VBOXDUMPDECL(void) crRecDumpBuffer(CR_RECORDER *pRec, CRContext *ctx, CR_BLITTER_CONTEXT *pCurCtx, CR_BLITTER_WINDOW *pCurWin, GLint idRedirFBO, VBOXVR_TEXTURE *pRedirTex);
VBOXDUMPDECL(void) crRecDumpTextures(CR_RECORDER *pRec, CRContext *ctx, CR_BLITTER_CONTEXT *pCurCtx, CR_BLITTER_WINDOW *pCurWin);
VBOXDUMPDECL(void) crRecDumpShader(CR_RECORDER *pRec, CRContext *ctx, GLint id, GLint hwid);
VBOXDUMPDECL(void) crRecDumpProgram(CR_RECORDER *pRec, CRContext *ctx, GLint id, GLint hwid);
VBOXDUMPDECL(void) crRecDumpCurrentProgram(CR_RECORDER *pRec, CRContext *ctx);
VBOXDUMPDECL(void) crRecDumpProgramUniforms(CR_RECORDER *pRec, CRContext *ctx, GLint id, GLint hwid);
VBOXDUMPDECL(void) crRecDumpCurrentProgramUniforms(CR_RECORDER *pRec, CRContext *ctx);
VBOXDUMPDECL(void) crRecDumpGlGetState(CR_RECORDER *pRec, CRContext *ctx);
VBOXDUMPDECL(void) crRecDumpGlEnableState(CR_RECORDER *pRec, CRContext *ctx);

typedef DECLCALLBACKPTR(GLuint, PFNCRDUMPGETHWID)(void *pvObj);
void* crDmpHashtableSearchByHwid(CRHashTable *pHash, GLuint hwid, PFNCRDUMPGETHWID pfnGetHwid, unsigned long *pKey);

RT_C_DECLS_END

#endif /* VBOX_WITH_CRDUMPER */
/* */

#endif /* #ifndef ___cr_dump_h__ */
