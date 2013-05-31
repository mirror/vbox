/* $Id$ */

/** @file
 * Blitter API implementation
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
#include "cr_blitter.h"
#include "cr_spu.h"
#include "chromium.h"
#include "cr_error.h"
#include "cr_net.h"
#include "cr_rand.h"
#include "cr_mem.h"
#include "cr_string.h"
#include <cr_dump.h>
#include "cr_pixeldata.h"

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/mem.h>

#include <stdio.h>

/* dump stuff */
#pragma pack(1)
typedef struct VBOX_BITMAPFILEHEADER {
        uint16_t    bfType;
        uint32_t   bfSize;
        uint16_t    bfReserved1;
        uint16_t    bfReserved2;
        uint32_t   bfOffBits;
} VBOX_BITMAPFILEHEADER;

typedef struct VBOX_BITMAPINFOHEADER {
        uint32_t      biSize;
        int32_t       biWidth;
        int32_t       biHeight;
        uint16_t       biPlanes;
        uint16_t       biBitCount;
        uint32_t      biCompression;
        uint32_t      biSizeImage;
        int32_t       biXPelsPerMeter;
        int32_t       biYPelsPerMeter;
        uint32_t      biClrUsed;
        uint32_t      biClrImportant;
} VBOX_BITMAPINFOHEADER;
#pragma pack()

void crDmpImgBmp(CR_BLITTER_IMG *pImg, const char *pszFilename)
{
    static int sIdx = 0;

    if (   pImg->bpp != 16
        && pImg->bpp != 24
        && pImg->bpp != 32)
    {
        crWarning("not supported bpp %d", pImg->bpp);
        return;
    }

    FILE *f = fopen (pszFilename, "wb");
    if (!f)
    {
        crWarning("fopen failed");
        return;
    }

    VBOX_BITMAPFILEHEADER bf;

    bf.bfType = 'MB';
    bf.bfSize = sizeof (VBOX_BITMAPFILEHEADER) + sizeof (VBOX_BITMAPINFOHEADER) + pImg->cbData;
    bf.bfReserved1 = 0;
    bf.bfReserved2 = 0;
    bf.bfOffBits = sizeof (VBOX_BITMAPFILEHEADER) + sizeof (VBOX_BITMAPINFOHEADER);

    VBOX_BITMAPINFOHEADER bi;

    bi.biSize = sizeof (bi);
    bi.biWidth = pImg->width;
    bi.biHeight = pImg->height;
    bi.biPlanes = 1;
    bi.biBitCount = pImg->bpp;
    bi.biCompression = 0;
    bi.biSizeImage = pImg->cbData;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    fwrite (&bf, 1, sizeof (bf), f);
    fwrite (&bi, 1, sizeof (bi), f);
    fwrite (pImg->pvData, 1, pImg->cbData, f);

    fclose (f);
}

#ifdef VBOX_WITH_CRDUMPER
#if 0
typedef struct CR_SERVER_DUMP_FIND_TEX
{
    GLint hwid;
    CRTextureObj *pTobj
} CR_SERVER_DUMP_FIND_TEX;

void crServerDumpFindTexCb(unsigned long key, void *data1, void *data2)
{
    CR_SERVER_DUMP_FIND_TEX *pTex = (CR_SERVER_DUMP_FIND_TEX*)data2;
    CRTextureObj *pTobj = (CRTextureObj *)data1;
    if (pTobj->hwid == pTex->hwid)
        pTex->pTobj = pTobj;
}
#endif

void crRecDumpBuffer(CR_RECORDER *pRec, CRContext *ctx, CR_BLITTER_CONTEXT *pCurCtx, CR_BLITTER_WINDOW *pCurWin, GLint idRedirFBO, VBOXVR_TEXTURE *pRedirTex)
{
    GLenum texTarget = 0;
    GLint hwBuf = 0, hwDrawBuf = 0;
    GLint hwTex = 0, hwObjType = 0, hwTexLevel = 0, hwCubeFace = 0;
    GLint width = 0, height = 0, depth = 0;
    CR_BLITTER_IMG Img = {0};
    VBOXVR_TEXTURE Tex;
    int rc;

    Assert(0);

    pRec->pDispatch->GetIntegerv(GL_DRAW_BUFFER, &hwDrawBuf);
    pRec->pDispatch->GetIntegerv(GL_FRAMEBUFFER_BINDING, &hwBuf);
    if (hwBuf)
    {
        pRec->pDispatch->GetFramebufferAttachmentParameterivEXT(GL_DRAW_FRAMEBUFFER, hwDrawBuf, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &hwTex);
        pRec->pDispatch->GetFramebufferAttachmentParameterivEXT(GL_DRAW_FRAMEBUFFER, hwDrawBuf, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &hwObjType);
        if (hwObjType == GL_TEXTURE)
        {
            pRec->pDispatch->GetFramebufferAttachmentParameterivEXT(GL_DRAW_FRAMEBUFFER, hwDrawBuf, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &hwTexLevel);
            pRec->pDispatch->GetFramebufferAttachmentParameterivEXT(GL_DRAW_FRAMEBUFFER, hwDrawBuf, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, &hwCubeFace);
            if (hwCubeFace)
            {
                crWarning("cube face: unsupported");
                return;
            }

            if (hwTexLevel)
            {
                crWarning("non-zero tex level attached, unsupported");
                return;
            }
        }
        else
        {
            crWarning("unsupported");
            return;
        }
    }
    else
    {
        crWarning("no buffer attached: unsupported");
        return;
    }

    if (ctx->framebufferobject.drawFB)
    {
        GLuint iColor = (hwDrawBuf - GL_COLOR_ATTACHMENT0_EXT);
        CRTextureObj *pTobj = (CRTextureObj *)crHashtableSearch(ctx->shared->textureTable, ctx->framebufferobject.drawFB->color[iColor].name);
        CRTextureLevel *pTl = NULL;

        Assert(iColor < RT_ELEMENTS(ctx->framebufferobject.drawFB->color));

        if (!pTobj)
        {
            crWarning("no tobj");
            return;
        }
        Assert(pTobj->hwid == hwTex);
        Assert(pTobj);
        Assert(ctx->framebufferobject.drawFB->hwid);
        Assert(ctx->framebufferobject.drawFB->hwid == hwBuf);
        Assert(ctx->framebufferobject.drawFB->drawbuffer[0] == hwDrawBuf);

        Assert(ctx->framebufferobject.drawFB->color[iColor].level == hwTexLevel);
        Assert(ctx->framebufferobject.drawFB->color[iColor].type == hwObjType);

        texTarget = pTobj->target;

        Assert(texTarget == GL_TEXTURE_2D);

        pTl = &pTobj->level[0][hwTexLevel];

        rc = CrBltEnter(pRec->pBlitter, pCurCtx, pCurWin);
        if (!RT_SUCCESS(rc))
        {
            crWarning("CrBltEnter failed, rc %d", rc);
            return;
        }

        pRec->pDispatch->BindTexture(texTarget, hwTex);

        pRec->pDispatch->GetTexLevelParameteriv(texTarget, hwTexLevel, GL_TEXTURE_WIDTH, &width);
        pRec->pDispatch->GetTexLevelParameteriv(texTarget, hwTexLevel, GL_TEXTURE_HEIGHT, &height);
        pRec->pDispatch->GetTexLevelParameteriv(texTarget, hwTexLevel, GL_TEXTURE_DEPTH, &depth);

        Assert(width == pTl->width);
        Assert(height == pTl->height);
        Assert(depth == pTl->depth);

        pRec->pDispatch->BindTexture(texTarget, 0);
    }
    else
    {
        Assert(hwBuf == idRedirFBO);
        if (!pRedirTex)
        {
            crWarning("pRedirTex is expected for non-FBO state!");
            return;
        }

        Assert(hwTex == pRedirTex->hwid);

        texTarget = pRedirTex->target;

        width = pRedirTex->width;
        height = pRedirTex->height;

        rc = CrBltEnter(pRec->pBlitter, pCurCtx, pCurWin);
        if (!RT_SUCCESS(rc))
        {
            crWarning("CrBltEnter failed, rc %d", rc);
            return;
        }
    }

    Tex.width = width;
    Tex.height = height;
    Tex.target = texTarget;
    Tex.hwid = hwTex;

    rc = CrBltImgGetTex(pRec->pBlitter, &Tex, GL_BGRA, &Img);
    if (RT_SUCCESS(rc))
    {
        crDmpImg(pRec->pDumper, "buffer_data", &Img);
        CrBltImgFree(pRec->pBlitter, &Img);
    }
    else
    {
        crWarning("CrBltImgGetTex failed, rc %d", rc);
    }

    CrBltLeave(pRec->pBlitter);
}

void crRecDumpTextures(CR_RECORDER *pRec, CRContext *ctx, CR_BLITTER_CONTEXT *pCurCtx, CR_BLITTER_WINDOW *pCurWin)
{
    GLint maxUnits = 0;
    GLint curTexUnit = 0;
    GLint restoreTexUnit = 0;
    GLint curProgram = 0;
    int rc;
    int i;

    Assert(0);

    pRec->pDispatch->GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxUnits);
    maxUnits = RT_MIN(CR_MAX_TEXTURE_UNITS, maxUnits);

    pRec->pDispatch->GetIntegerv(GL_CURRENT_PROGRAM, &curProgram);
    Assert(curProgram);
    Assert(ctx->glsl.activeProgram && ctx->glsl.activeProgram->hwid == curProgram);

    Assert(maxUnits);
    pRec->pDispatch->GetIntegerv(GL_ACTIVE_TEXTURE, &curTexUnit);
    restoreTexUnit = curTexUnit;
    Assert(curTexUnit >= GL_TEXTURE0);
    Assert(curTexUnit < GL_TEXTURE0 + maxUnits);

    Assert(ctx->texture.curTextureUnit == restoreTexUnit - GL_TEXTURE0);

    for (i = 0; i < maxUnits; ++i)
    {
        GLboolean enabled1D;
        GLboolean enabled2D;
        GLboolean enabled3D;
        GLboolean enabledCubeMap;
        GLboolean enabledRect;
        CRTextureUnit *tu = &ctx->texture.unit[i];

        if (curTexUnit != i + GL_TEXTURE0)
        {
            pRec->pDispatch->ActiveTextureARB(i + GL_TEXTURE0);
            curTexUnit = i + GL_TEXTURE0;
        }

        enabled1D = pRec->pDispatch->IsEnabled(GL_TEXTURE_1D);
        enabled2D = pRec->pDispatch->IsEnabled(GL_TEXTURE_2D);
        enabled3D = pRec->pDispatch->IsEnabled(GL_TEXTURE_3D);
        enabledCubeMap = pRec->pDispatch->IsEnabled(GL_TEXTURE_CUBE_MAP_ARB);
        enabledRect = pRec->pDispatch->IsEnabled(GL_TEXTURE_RECTANGLE_NV);

        Assert(enabled1D == tu->enabled1D);
        Assert(enabled2D == tu->enabled2D);
        Assert(enabled3D == tu->enabled3D);
        Assert(enabledCubeMap == tu->enabledCubeMap);
        Assert(enabledRect == tu->enabledRect);

        if (enabled1D)
        {
            crWarning("GL_TEXTURE_1D: unsupported");
        }

        if (enabled2D)
        {
            GLint hwTex = 0;
            CR_BLITTER_IMG Img = {0};
            VBOXVR_TEXTURE Tex;

            GLint width = 0, height = 0, depth = 0;
            CRTextureObj *pTobj = tu->currentTexture2D;

            pRec->pDispatch->GetIntegerv(GL_TEXTURE_BINDING_2D, &hwTex);
            if (hwTex)
            {
                CRTextureLevel *pTl = &pTobj->level[0][0 /* level */];
                Assert(pTobj
                        && pTobj->hwid == hwTex);

                pRec->pDispatch->GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
                pRec->pDispatch->GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
                pRec->pDispatch->GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_DEPTH, &depth);

                Assert(width == pTl->width);
                Assert(height == pTl->height);
                Assert(depth == pTl->depth);

                Tex.width = width;
                Tex.height = height;
                Tex.target = GL_TEXTURE_2D;
                Tex.hwid = hwTex;

                rc = CrBltEnter(pRec->pBlitter, pCurCtx, pCurWin);
                if (RT_SUCCESS(rc))
                {
                    rc = CrBltImgGetTex(pRec->pBlitter, &Tex, GL_BGRA, &Img);
                    if (RT_SUCCESS(rc))
                    {
                        crDmpImg(pRec->pDumper, "TEXTURE_2D data", &Img);
                        CrBltImgFree(pRec->pBlitter, &Img);
                    }
                    else
                    {
                        crWarning("CrBltImgGetTex failed, rc %d", rc);
                    }
                    CrBltLeave(pRec->pBlitter);
                }
                else
                {
                    crWarning("CrBltEnter failed, rc %d", rc);
                }
            }
            else
            {
                Assert(!pTobj || pTobj->hwid == 0);
                crWarning("no TEXTURE_2D bound!");
            }
        }

        if (enabled3D)
        {
            crWarning("GL_TEXTURE_3D: unsupported");
        }

        if (enabledCubeMap)
        {
            crWarning("GL_TEXTURE_CUBE_MAP_ARB: unsupported");
        }

        if (enabledRect)
        {

            GLint hwTex = 0;
            CR_BLITTER_IMG Img = {0};
            VBOXVR_TEXTURE Tex;

            GLint width = 0, height = 0, depth = 0;
            CRTextureObj *pTobj = tu->currentTextureRect;

            pRec->pDispatch->GetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_NV, &hwTex);
            if (hwTex)
            {
                CRTextureLevel *pTl = &pTobj->level[0][0 /* level */];
                Assert(pTobj
                        && pTobj->hwid == hwTex);

                pRec->pDispatch->GetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_NV, 0, GL_TEXTURE_WIDTH, &width);
                pRec->pDispatch->GetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_NV, 0, GL_TEXTURE_HEIGHT, &height);
                pRec->pDispatch->GetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_NV, 0, GL_TEXTURE_DEPTH, &depth);

                Assert(width == pTl->width);
                Assert(height == pTl->height);
                Assert(depth == pTl->depth);

                Tex.width = width;
                Tex.height = height;
                Tex.target = GL_TEXTURE_RECTANGLE_NV;
                Tex.hwid = hwTex;

                rc = CrBltEnter(pRec->pBlitter, pCurCtx, pCurWin);
                if (RT_SUCCESS(rc))
                {
                    rc = CrBltImgGetTex(pRec->pBlitter, &Tex, GL_BGRA, &Img);
                    if (RT_SUCCESS(rc))
                    {
                        crDmpImg(pRec->pDumper, "TEXTURE_RECTANGLE data", &Img);
                        CrBltImgFree(pRec->pBlitter, &Img);
                    }
                    else
                    {
                        crWarning("CrBltImgGetTex failed, rc %d", rc);
                    }
                    CrBltLeave(pRec->pBlitter);
                }
                else
                {
                    crWarning("CrBltEnter failed, rc %d", rc);
                }
            }
            else
            {
                Assert(!pTobj || pTobj->hwid == 0);
                crWarning("no TEXTURE_2D bound!");
            }
        }
    }

    if (curTexUnit != restoreTexUnit)
    {
        pRec->pDispatch->ActiveTextureARB(restoreTexUnit);
        curTexUnit = restoreTexUnit;
    }
}

static void crDmpPrint(const char* szString, ...)
{
    char szBuffer[4096] = {0};
    va_list pArgList;
    va_start(pArgList, szString);
    RTStrPrintfV(szBuffer, sizeof (szBuffer), szString, pArgList);
    va_end(pArgList);

    OutputDebugStringA(szBuffer);
}

static void crDmpPrintDmlCmd(const char* pszDesc, const char* pszCmd)
{
    crDmpPrint("<?dml?><exec cmd=\"%s\">%s</exec>, ( %s )\n", pszCmd, pszDesc, pszCmd);
}

void crDmpPrintDumpDmlCmd(const char* pszDesc, const void *pvData, uint32_t width, uint32_t height, uint32_t bpp, uint32_t pitch)
{
    char Cmd[1024];
    sprintf(Cmd, "!vbvdbg.ms 0x%p 0n%d 0n%d 0n%d 0n%d", pvData, width, height, bpp, pitch);
    crDmpPrintDmlCmd(pszDesc, Cmd);
}

DECLCALLBACK(void) crDmpDumpImgDmlBreak(struct CR_DUMPER * pDumper, const char*pszEntryDesc, CR_BLITTER_IMG *pImg)
{
    crDmpPrintDumpDmlCmd(pszEntryDesc, pImg->pvData, pImg->width, pImg->height, pImg->bpp, pImg->pitch);
    RT_BREAKPOINT();
}

DECLCALLBACK(void) crDmpDumpStrDbgPrint(struct CR_DUMPER * pDumper, const char*pszStr)
{
    OutputDebugStringA(pszStr);
}

static void crDmpHtmlDumpStrExact(struct CR_HTML_DUMPER * pDumper, const char *pszStr)
{
    fprintf(pDumper->pFile, "%s", pszStr);
}

static DECLCALLBACK(void) crDmpHtmlDumpStr(struct CR_DUMPER * pDumper, const char*pszStr)
{
    CR_HTML_DUMPER * pHtmlDumper = (CR_HTML_DUMPER*)pDumper;
    fprintf(pHtmlDumper->pFile, "<pre>%s</pre><br>", pszStr);
}

static DECLCALLBACK(void) crDmpHtmlDumpImg(struct CR_DUMPER * pDumper, const char*pszEntryDesc, CR_BLITTER_IMG *pImg)
{
    CR_HTML_DUMPER * pHtmlDumper = (CR_HTML_DUMPER*)pDumper;
    char szBuffer[4096] = {0};
    size_t cbWritten = RTStrPrintf(szBuffer, sizeof(szBuffer), "%s/", pHtmlDumper->pszDir);
    char *pszFileName = szBuffer + cbWritten;
    RTStrPrintf(pszFileName, sizeof(szBuffer) - cbWritten, "img%d.tga", ++pHtmlDumper->cImg);
    crDmpImgBmp(pImg, szBuffer);
    fprintf(pHtmlDumper->pFile, "<a href=\"%s\"><pre>%s</pre><img src=\"%s\" alt=\"%s\" width=\"150\" height=\"100\" /></a><br>",
            pszFileName, pszEntryDesc, pszFileName, pszEntryDesc);
}

static void crDmpHtmlPrintHeader(struct CR_HTML_DUMPER * pDumper)
{
    fprintf(pDumper->pFile, "<html><body>");
}

static void crDmpHtmlPrintFooter(struct CR_HTML_DUMPER * pDumper)
{
    fprintf(pDumper->pFile, "</body></html>");
}

DECLEXPORT(int) crDmpHtmlInit(struct CR_HTML_DUMPER * pDumper, const char *pszDir, const char *pszFile)
{
    int rc = VERR_NO_MEMORY;
    pDumper->Base.pfnDumpImg = crDmpHtmlDumpImg;
    pDumper->Base.pfnDumpStr = crDmpHtmlDumpStr;
    pDumper->cImg = 0;
    pDumper->pszDir = crStrdup(pszDir);
    if (pDumper->pszDir)
    {
        pDumper->pszFile = crStrdup(pszFile);
        if (pDumper->pszFile)
        {
            char szBuffer[4096] = {0};
            RTStrPrintf(szBuffer, sizeof(szBuffer), "%s/%s", pszDir, pszFile);

            pDumper->pszFile = crStrdup(pszFile);
            pDumper->pFile = fopen(szBuffer, "w");
            if (pDumper->pFile)
            {
                crDmpHtmlPrintHeader(pDumper);
                return VINF_SUCCESS;
            }
            else
            {
                crWarning("open failed");
                rc = VERR_OPEN_FAILED;
            }
            crFree((void*)pDumper->pszFile);
        }
        else
        {
            crWarning("open failed");
        }
        crFree((void*)pDumper->pszDir);
    }
    else
    {
        crWarning("open failed");
    }
    return rc;
}
#endif
