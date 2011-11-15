/* $Id$ */

/** @file
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include <windows.h>
#include <d3d9types.h>
#include <d3d9caps.h>
#include <stdio.h>

#define MISSING_FLAGS(_dw1, _dw2) ((_dw2) & ((_dw1) ^ (_dw2)))

#define Log(_m) do { printf _m ; } while (0)

#define DUMP_STRCASE(_t) \
        case _t: { Log(("%s", #_t"")); break; }
#define DUMP_STRCASE_DEFAULT_DWORD(_dw) \
        default: { Log(("0x%08", (_dw))); break; }

#define DUMP_STRIF_INIT(_ps, _t) \
        const char * _pSep = (_ps); \
        bool _fSep = false; \
        _t _fFlags = 0; \

#define DUMP_STRIF(_v, _t) do { \
        if ((_v) & _t) { \
            if (_fSep) { \
                Log(("%s%s", _pSep ,#_t"")); \
            } \
            else { \
                Log(("%s", #_t"")); \
                _fSep = !!_pSep; \
            } \
            _fFlags |= _t; \
        } \
    } while (0)

#define DUMP_STRIF_MISSED(_dw) do { \
        _fFlags = MISSING_FLAGS(_fFlags, _dw); \
        if (_fFlags) { \
            if (_fSep) { \
                Log(("%s0x%08x", _pSep, (_fFlags))); \
            } \
            else { \
                Log(("0x%08x", (_fFlags))); \
                _fSep = !!_pSep; \
            } \
        } \
        _fFlags = _dw & ~(_fFlags); /* revert the flags valus back */ \
    } while (0)

/*
#define DUMP_DIFF_CAPS_VAL(_f, _name, _c1, _c2) do { \
        DWORD dwTmp =  MISSING_FLAGS((_c1), (_c2)); \
        if (dwTmp) {  _f(_name " |= ", " | ", dwTmp, ";\n"); } \
        dwTmp =  MISSING_FLAGS((_c2), (_c1)); \
        if (dwTmp) {  _f("// " _name " &= ~(", " | ", dwTmp, ");\n"); } \
    } while (0)

#define DUMP_DIFF_CAPS_FIELD(_f, _field, _name, _c1, _c2) DUMP_DIFF_CAPS_VAL(_f, _name""_field, (_c1)->_field, (_c2)->_field)
*/
#define DUMP_DIFF_CAPS(_f, _field) do { \
        DWORD dwTmp =  MISSING_FLAGS((pCaps1->_field), (pCaps2->_field)); \
        if (dwTmp) {  _f("pCaps->" #_field " |= ", " | ", dwTmp, ";\n"); } \
        dwTmp =  MISSING_FLAGS((pCaps2->_field), (pCaps1->_field)); \
        if (dwTmp) {  _f("// pCaps->" #_field " &= ~(", " | ", dwTmp, ");\n"); } \
    } while (0)

#define DUMP_DIFF_VAL(_field, _format) do { \
        if (pCaps1->_field != pCaps2->_field) { Log(("pCaps->" #_field " = " _format "; // " _format " \n", pCaps2->_field, pCaps1->_field)); } \
    } while (0)

static void printDeviceType(const char* pszPrefix, D3DDEVTYPE DeviceType, const char* pszSuffix)
{
    Log(("%s", pszPrefix));
    switch(DeviceType)
    {
        DUMP_STRCASE(D3DDEVTYPE_HAL)
        DUMP_STRCASE(D3DDEVTYPE_REF)
        DUMP_STRCASE(D3DDEVTYPE_SW)
        DUMP_STRCASE(D3DDEVTYPE_NULLREF)
        DUMP_STRCASE_DEFAULT_DWORD(DeviceType)
    }
    Log(("%s", pszSuffix));
}

static void printCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
//    DUMP_STRIF(Caps, D3DCAPS_OVERLAY);
    DUMP_STRIF(Caps, D3DCAPS_READ_SCANLINE);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}


static void printCaps2(const char* pszPrefix, const char* pszSeparator, DWORD Caps2, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps2, D3DCAPS2_FULLSCREENGAMMA);
    DUMP_STRIF(Caps2, D3DCAPS2_CANCALIBRATEGAMMA);
    DUMP_STRIF(Caps2, D3DCAPS2_RESERVED);
    DUMP_STRIF(Caps2, D3DCAPS2_CANMANAGERESOURCE);
    DUMP_STRIF(Caps2, D3DCAPS2_DYNAMICTEXTURES);
    DUMP_STRIF(Caps2, D3DCAPS2_CANAUTOGENMIPMAP);
    DUMP_STRIF(Caps2, D3DCAPS2_CANSHARERESOURCE);
    DUMP_STRIF_MISSED(Caps2);
    Log(("%s", pszSuffix));
}

static void printCaps3(const char* pszPrefix, const char* pszSeparator, DWORD Caps3, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps3, D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD);
    DUMP_STRIF(Caps3, D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION);
    DUMP_STRIF(Caps3, D3DCAPS3_COPY_TO_VIDMEM);
    DUMP_STRIF(Caps3, D3DCAPS3_COPY_TO_SYSTEMMEM);
//    DUMP_STRIF(Caps3, D3DCAPS3_DXVAHD);
    DUMP_STRIF_MISSED(Caps3);
    Log(("%s", pszSuffix));
}

static void printPresentationIntervals(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DPRESENT_INTERVAL_ONE);
    DUMP_STRIF(Caps, D3DPRESENT_INTERVAL_TWO);
    DUMP_STRIF(Caps, D3DPRESENT_INTERVAL_THREE);
    DUMP_STRIF(Caps, D3DPRESENT_INTERVAL_FOUR);
    DUMP_STRIF(Caps, D3DPRESENT_INTERVAL_IMMEDIATE);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printCursorCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DCURSORCAPS_COLOR);
    DUMP_STRIF(Caps, D3DCURSORCAPS_LOWRES);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printDevCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DDEVCAPS_EXECUTESYSTEMMEMORY);
    DUMP_STRIF(Caps, D3DDEVCAPS_EXECUTEVIDEOMEMORY);
    DUMP_STRIF(Caps, D3DDEVCAPS_TLVERTEXSYSTEMMEMORY);
    DUMP_STRIF(Caps, D3DDEVCAPS_TLVERTEXVIDEOMEMORY);
    DUMP_STRIF(Caps, D3DDEVCAPS_TEXTURESYSTEMMEMORY);
    DUMP_STRIF(Caps, D3DDEVCAPS_TEXTUREVIDEOMEMORY);
    DUMP_STRIF(Caps, D3DDEVCAPS_DRAWPRIMTLVERTEX);
    DUMP_STRIF(Caps, D3DDEVCAPS_CANRENDERAFTERFLIP);
    DUMP_STRIF(Caps, D3DDEVCAPS_TEXTURENONLOCALVIDMEM);
    DUMP_STRIF(Caps, D3DDEVCAPS_DRAWPRIMITIVES2);
    DUMP_STRIF(Caps, D3DDEVCAPS_SEPARATETEXTUREMEMORIES);
    DUMP_STRIF(Caps, D3DDEVCAPS_DRAWPRIMITIVES2EX);
    DUMP_STRIF(Caps, D3DDEVCAPS_HWTRANSFORMANDLIGHT);
    DUMP_STRIF(Caps, D3DDEVCAPS_CANBLTSYSTONONLOCAL);
    DUMP_STRIF(Caps, D3DDEVCAPS_HWRASTERIZATION);
    DUMP_STRIF(Caps, D3DDEVCAPS_PUREDEVICE);
    DUMP_STRIF(Caps, D3DDEVCAPS_QUINTICRTPATCHES);
    DUMP_STRIF(Caps, D3DDEVCAPS_RTPATCHES);
    DUMP_STRIF(Caps, D3DDEVCAPS_RTPATCHHANDLEZERO);
    DUMP_STRIF(Caps, D3DDEVCAPS_NPATCHES);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printPrimitiveMiscCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DPMISCCAPS_MASKZ);
    DUMP_STRIF(Caps, D3DPMISCCAPS_CULLNONE);
    DUMP_STRIF(Caps, D3DPMISCCAPS_CULLCW);
    DUMP_STRIF(Caps, D3DPMISCCAPS_CULLCCW);
    DUMP_STRIF(Caps, D3DPMISCCAPS_COLORWRITEENABLE);
    DUMP_STRIF(Caps, D3DPMISCCAPS_CLIPPLANESCALEDPOINTS);
    DUMP_STRIF(Caps, D3DPMISCCAPS_CLIPTLVERTS);
    DUMP_STRIF(Caps, D3DPMISCCAPS_TSSARGTEMP);
    DUMP_STRIF(Caps, D3DPMISCCAPS_BLENDOP);
    DUMP_STRIF(Caps, D3DPMISCCAPS_NULLREFERENCE);
    DUMP_STRIF(Caps, D3DPMISCCAPS_INDEPENDENTWRITEMASKS);
    DUMP_STRIF(Caps, D3DPMISCCAPS_PERSTAGECONSTANT);
    DUMP_STRIF(Caps, D3DPMISCCAPS_FOGANDSPECULARALPHA);
    DUMP_STRIF(Caps, D3DPMISCCAPS_SEPARATEALPHABLEND);
    DUMP_STRIF(Caps, D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS);
    DUMP_STRIF(Caps, D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING);
    DUMP_STRIF(Caps, D3DPMISCCAPS_FOGVERTEXCLAMPED);
    DUMP_STRIF(Caps, D3DPMISCCAPS_POSTBLENDSRGBCONVERT);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printRasterCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DPRASTERCAPS_DITHER);
    DUMP_STRIF(Caps, D3DPRASTERCAPS_ZTEST);
    DUMP_STRIF(Caps, D3DPRASTERCAPS_FOGVERTEX);
    DUMP_STRIF(Caps, D3DPRASTERCAPS_FOGTABLE);
    DUMP_STRIF(Caps, D3DPRASTERCAPS_MIPMAPLODBIAS);
    DUMP_STRIF(Caps, D3DPRASTERCAPS_ZBUFFERLESSHSR);
    DUMP_STRIF(Caps, D3DPRASTERCAPS_FOGRANGE);
    DUMP_STRIF(Caps, D3DPRASTERCAPS_ANISOTROPY);
    DUMP_STRIF(Caps, D3DPRASTERCAPS_WBUFFER);
    DUMP_STRIF(Caps, D3DPRASTERCAPS_WFOG);
    DUMP_STRIF(Caps, D3DPRASTERCAPS_ZFOG);
    DUMP_STRIF(Caps, D3DPRASTERCAPS_COLORPERSPECTIVE);
    DUMP_STRIF(Caps, D3DPRASTERCAPS_SCISSORTEST);
    DUMP_STRIF(Caps, D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS);
    DUMP_STRIF(Caps, D3DPRASTERCAPS_DEPTHBIAS);
    DUMP_STRIF(Caps, D3DPRASTERCAPS_MULTISAMPLE_TOGGLE);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printCmpCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DPCMPCAPS_NEVER);
    DUMP_STRIF(Caps, D3DPCMPCAPS_LESS);
    DUMP_STRIF(Caps, D3DPCMPCAPS_EQUAL);
    DUMP_STRIF(Caps, D3DPCMPCAPS_LESSEQUAL);
    DUMP_STRIF(Caps, D3DPCMPCAPS_GREATER);
    DUMP_STRIF(Caps, D3DPCMPCAPS_NOTEQUAL);
    DUMP_STRIF(Caps, D3DPCMPCAPS_GREATEREQUAL);
    DUMP_STRIF(Caps, D3DPCMPCAPS_ALWAYS);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printBlendCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DPBLENDCAPS_ZERO);
    DUMP_STRIF(Caps, D3DPBLENDCAPS_ONE);
    DUMP_STRIF(Caps, D3DPBLENDCAPS_SRCCOLOR);
    DUMP_STRIF(Caps, D3DPBLENDCAPS_INVSRCCOLOR);
    DUMP_STRIF(Caps, D3DPBLENDCAPS_SRCALPHA);
    DUMP_STRIF(Caps, D3DPBLENDCAPS_INVSRCALPHA);
    DUMP_STRIF(Caps, D3DPBLENDCAPS_DESTALPHA);
    DUMP_STRIF(Caps, D3DPBLENDCAPS_INVDESTALPHA);
    DUMP_STRIF(Caps, D3DPBLENDCAPS_DESTCOLOR);
    DUMP_STRIF(Caps, D3DPBLENDCAPS_INVDESTCOLOR);
    DUMP_STRIF(Caps, D3DPBLENDCAPS_SRCALPHASAT);
    DUMP_STRIF(Caps, D3DPBLENDCAPS_BOTHSRCALPHA);
    DUMP_STRIF(Caps, D3DPBLENDCAPS_BOTHINVSRCALPHA);
    DUMP_STRIF(Caps, D3DPBLENDCAPS_BLENDFACTOR);
    DUMP_STRIF(Caps, D3DPBLENDCAPS_SRCCOLOR2);
    DUMP_STRIF(Caps, D3DPBLENDCAPS_INVSRCCOLOR2);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printShadeCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DPSHADECAPS_COLORGOURAUDRGB);
    DUMP_STRIF(Caps, D3DPSHADECAPS_SPECULARGOURAUDRGB);
    DUMP_STRIF(Caps, D3DPSHADECAPS_ALPHAGOURAUDBLEND);
    DUMP_STRIF(Caps, D3DPSHADECAPS_FOGGOURAUD);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printTextureCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_PERSPECTIVE);
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_POW2);
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_ALPHA);
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_SQUAREONLY);
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE);
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_ALPHAPALETTE);
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_NONPOW2CONDITIONAL);
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_PROJECTED);
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_CUBEMAP);
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_VOLUMEMAP);
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_MIPMAP);
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_MIPVOLUMEMAP);
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_MIPCUBEMAP);
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_CUBEMAP_POW2);
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_VOLUMEMAP_POW2);
    DUMP_STRIF(Caps, D3DPTEXTURECAPS_NOPROJECTEDBUMPENV);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printFilterCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DPTFILTERCAPS_MINFPOINT);
    DUMP_STRIF(Caps, D3DPTFILTERCAPS_MINFLINEAR);
    DUMP_STRIF(Caps, D3DPTFILTERCAPS_MINFANISOTROPIC);
    DUMP_STRIF(Caps, D3DPTFILTERCAPS_MINFPYRAMIDALQUAD);
    DUMP_STRIF(Caps, D3DPTFILTERCAPS_MINFGAUSSIANQUAD);
    DUMP_STRIF(Caps, D3DPTFILTERCAPS_MIPFPOINT);
    DUMP_STRIF(Caps, D3DPTFILTERCAPS_MIPFLINEAR);
    DUMP_STRIF(Caps, D3DPTFILTERCAPS_CONVOLUTIONMONO);
    DUMP_STRIF(Caps, D3DPTFILTERCAPS_MAGFPOINT);
    DUMP_STRIF(Caps, D3DPTFILTERCAPS_MAGFLINEAR);
    DUMP_STRIF(Caps, D3DPTFILTERCAPS_MAGFANISOTROPIC);
    DUMP_STRIF(Caps, D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD);
    DUMP_STRIF(Caps, D3DPTFILTERCAPS_MAGFGAUSSIANQUAD);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printTextureAddressCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DPTADDRESSCAPS_WRAP);
    DUMP_STRIF(Caps, D3DPTADDRESSCAPS_MIRROR);
    DUMP_STRIF(Caps, D3DPTADDRESSCAPS_CLAMP);
    DUMP_STRIF(Caps, D3DPTADDRESSCAPS_BORDER);
    DUMP_STRIF(Caps, D3DPTADDRESSCAPS_INDEPENDENTUV);
    DUMP_STRIF(Caps, D3DPTADDRESSCAPS_MIRRORONCE);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printLineCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DLINECAPS_TEXTURE);
    DUMP_STRIF(Caps, D3DLINECAPS_ZTEST);
    DUMP_STRIF(Caps, D3DLINECAPS_BLEND);
    DUMP_STRIF(Caps, D3DLINECAPS_ALPHACMP);
    DUMP_STRIF(Caps, D3DLINECAPS_FOG);
    DUMP_STRIF(Caps, D3DLINECAPS_ANTIALIAS);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printStencilCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DSTENCILCAPS_KEEP);
    DUMP_STRIF(Caps, D3DSTENCILCAPS_ZERO);
    DUMP_STRIF(Caps, D3DSTENCILCAPS_REPLACE);
    DUMP_STRIF(Caps, D3DSTENCILCAPS_INCRSAT);
    DUMP_STRIF(Caps, D3DSTENCILCAPS_DECRSAT);
    DUMP_STRIF(Caps, D3DSTENCILCAPS_INVERT);
    DUMP_STRIF(Caps, D3DSTENCILCAPS_INCR);
    DUMP_STRIF(Caps, D3DSTENCILCAPS_DECR);
    DUMP_STRIF(Caps, D3DSTENCILCAPS_TWOSIDED);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printFVFCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DFVFCAPS_TEXCOORDCOUNTMASK);
    DUMP_STRIF(Caps, D3DFVFCAPS_DONOTSTRIPELEMENTS);
    DUMP_STRIF(Caps, D3DFVFCAPS_PSIZE);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printTextureOpCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DTEXOPCAPS_DISABLE);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_SELECTARG1);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_SELECTARG2);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_MODULATE);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_MODULATE2X);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_MODULATE4X);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_ADD);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_ADDSIGNED);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_ADDSIGNED2X);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_SUBTRACT);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_ADDSMOOTH);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_BLENDDIFFUSEALPHA);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_BLENDTEXTUREALPHA);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_BLENDFACTORALPHA);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_BLENDTEXTUREALPHAPM);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_BLENDCURRENTALPHA);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_PREMODULATE);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_BUMPENVMAP);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_BUMPENVMAPLUMINANCE);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_DOTPRODUCT3);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_MULTIPLYADD);
    DUMP_STRIF(Caps, D3DTEXOPCAPS_LERP);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printVertexProcessingCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DVTXPCAPS_TEXGEN);
    DUMP_STRIF(Caps, D3DVTXPCAPS_MATERIALSOURCE7);
    DUMP_STRIF(Caps, D3DVTXPCAPS_DIRECTIONALLIGHTS);
    DUMP_STRIF(Caps, D3DVTXPCAPS_POSITIONALLIGHTS);
    DUMP_STRIF(Caps, D3DVTXPCAPS_LOCALVIEWER);
    DUMP_STRIF(Caps, D3DVTXPCAPS_TWEENING);
    DUMP_STRIF(Caps, D3DVTXPCAPS_TEXGEN_SPHEREMAP);
    DUMP_STRIF(Caps, D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printDevCaps2(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DDEVCAPS2_STREAMOFFSET);
    DUMP_STRIF(Caps, D3DDEVCAPS2_DMAPNPATCH);
    DUMP_STRIF(Caps, D3DDEVCAPS2_ADAPTIVETESSRTPATCH);
    DUMP_STRIF(Caps, D3DDEVCAPS2_ADAPTIVETESSNPATCH);
    DUMP_STRIF(Caps, D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES);
    DUMP_STRIF(Caps, D3DDEVCAPS2_PRESAMPLEDDMAPNPATCH);
    DUMP_STRIF(Caps, D3DDEVCAPS2_VERTEXELEMENTSCANSHARESTREAMOFFSET);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

static void printDeclTypes(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, D3DDTCAPS_UBYTE4);
    DUMP_STRIF(Caps, D3DDTCAPS_UBYTE4N);
    DUMP_STRIF(Caps, D3DDTCAPS_SHORT2N);
    DUMP_STRIF(Caps, D3DDTCAPS_SHORT4N);
    DUMP_STRIF(Caps, D3DDTCAPS_USHORT2N);
    DUMP_STRIF(Caps, D3DDTCAPS_USHORT4N);
    DUMP_STRIF(Caps, D3DDTCAPS_UDEC3);
    DUMP_STRIF(Caps, D3DDTCAPS_DEC3N);
    DUMP_STRIF(Caps, D3DDTCAPS_FLOAT16_2);
    DUMP_STRIF(Caps, D3DDTCAPS_FLOAT16_4);
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}

#if 0
static void printXxxCaps(const char* pszPrefix, const char* pszSeparator, DWORD Caps, const char* pszSuffix)
{
    DUMP_STRIF_INIT(pszSeparator, DWORD);
    Log(("%s", pszPrefix));
    DUMP_STRIF(Caps, );
    DUMP_STRIF(Caps, );
    DUMP_STRIF(Caps, );
    DUMP_STRIF(Caps, );
    DUMP_STRIF(Caps, );
    DUMP_STRIF(Caps, );
    DUMP_STRIF(Caps, );
    DUMP_STRIF(Caps, );
    DUMP_STRIF(Caps, );
    DUMP_STRIF(Caps, );
    DUMP_STRIF(Caps, );
    DUMP_STRIF(Caps, );
    DUMP_STRIF(Caps, );
    DUMP_STRIF(Caps, );
    DUMP_STRIF(Caps, );
    DUMP_STRIF(Caps, );
    DUMP_STRIF_MISSED(Caps);
    Log(("%s", pszSuffix));
}
#endif

static void diffCaps(D3DCAPS9 *pCaps1, D3DCAPS9 *pCaps2)
{
    if (pCaps1->DeviceType != pCaps2->DeviceType)
    {
        printDeviceType("pCaps->DeviceType = ", pCaps2->DeviceType, ";\n");
    }

    DUMP_DIFF_VAL(AdapterOrdinal, "%d");

    DUMP_DIFF_CAPS(printCaps, Caps);
    DUMP_DIFF_CAPS(printCaps2, Caps2);
    DUMP_DIFF_CAPS(printCaps3, Caps3);
    DUMP_DIFF_CAPS(printPresentationIntervals, PresentationIntervals);
    DUMP_DIFF_CAPS(printCursorCaps, CursorCaps);
    DUMP_DIFF_CAPS(printDevCaps, DevCaps);
    DUMP_DIFF_CAPS(printPrimitiveMiscCaps, PrimitiveMiscCaps);
    DUMP_DIFF_CAPS(printRasterCaps, RasterCaps);
    DUMP_DIFF_CAPS(printCmpCaps, ZCmpCaps);
    DUMP_DIFF_CAPS(printBlendCaps, SrcBlendCaps);
    DUMP_DIFF_CAPS(printBlendCaps, DestBlendCaps);
    DUMP_DIFF_CAPS(printCmpCaps, AlphaCmpCaps);
    DUMP_DIFF_CAPS(printShadeCaps, ShadeCaps);
    DUMP_DIFF_CAPS(printTextureCaps, TextureCaps);
    DUMP_DIFF_CAPS(printFilterCaps, TextureFilterCaps);
    DUMP_DIFF_CAPS(printFilterCaps, CubeTextureFilterCaps);
    DUMP_DIFF_CAPS(printFilterCaps, VolumeTextureFilterCaps);
    DUMP_DIFF_CAPS(printTextureAddressCaps, TextureAddressCaps);
    DUMP_DIFF_CAPS(printTextureAddressCaps, VolumeTextureAddressCaps);
    DUMP_DIFF_CAPS(printLineCaps, LineCaps);

    /* non-caps */
    DUMP_DIFF_VAL(MaxTextureWidth, "%d");
    DUMP_DIFF_VAL(MaxTextureHeight, "%d");
    DUMP_DIFF_VAL(MaxVolumeExtent, "%d");
    DUMP_DIFF_VAL(MaxTextureRepeat, "%d");
    DUMP_DIFF_VAL(MaxTextureAspectRatio, "%d");
    DUMP_DIFF_VAL(MaxAnisotropy, "%d");
    DUMP_DIFF_VAL(MaxVertexW, "%f");
    DUMP_DIFF_VAL(GuardBandLeft, "%f");
    DUMP_DIFF_VAL(GuardBandTop, "%f");
    DUMP_DIFF_VAL(GuardBandRight, "%f");
    DUMP_DIFF_VAL(GuardBandBottom, "%f");
    DUMP_DIFF_VAL(ExtentsAdjust, "%f");

    /* caps */
    DUMP_DIFF_CAPS(printStencilCaps, StencilCaps);
    DUMP_DIFF_CAPS(printFVFCaps, FVFCaps);
    DUMP_DIFF_CAPS(printTextureOpCaps, TextureOpCaps);

    /* non-caps */
    DUMP_DIFF_VAL(MaxTextureBlendStages, "%d");
    DUMP_DIFF_VAL(MaxSimultaneousTextures, "%d");

    /* caps */
    DUMP_DIFF_CAPS(printVertexProcessingCaps, VertexProcessingCaps);

    /* non-caps */
    DUMP_DIFF_VAL(MaxActiveLights, "%d");
    DUMP_DIFF_VAL(MaxUserClipPlanes, "%d");
    DUMP_DIFF_VAL(MaxVertexBlendMatrices, "%d");
    DUMP_DIFF_VAL(MaxVertexBlendMatrixIndex, "%d");
    DUMP_DIFF_VAL(MaxPointSize, "%f");
    DUMP_DIFF_VAL(MaxPrimitiveCount, "%d");
    DUMP_DIFF_VAL(MaxVertexIndex, "%d");
    DUMP_DIFF_VAL(MaxStreams, "%d");
    DUMP_DIFF_VAL(MaxStreamStride, "%d");
    DUMP_DIFF_VAL(VertexShaderVersion, "%d");
    DUMP_DIFF_VAL(MaxVertexShaderConst, "%d");
    DUMP_DIFF_VAL(PixelShaderVersion, "%d");
    DUMP_DIFF_VAL(PixelShader1xMaxValue, "%f");

    /* D3D9 */
    /* caps */
    DUMP_DIFF_CAPS(printDevCaps2, DevCaps2);

    /* non-caps */
    DUMP_DIFF_VAL(MaxNpatchTessellationLevel, "%f");
    DUMP_DIFF_VAL(Reserved5, "%d");
    DUMP_DIFF_VAL(MasterAdapterOrdinal, "%d");
    DUMP_DIFF_VAL(AdapterOrdinalInGroup, "%d");
    DUMP_DIFF_VAL(NumberOfAdaptersInGroup, "%d");

    /* caps */
    DUMP_DIFF_CAPS(printDeclTypes, DeclTypes);

    /* non-caps */
    DUMP_DIFF_VAL(NumSimultaneousRTs, "%d");

    /* caps */
    DUMP_DIFF_CAPS(printFilterCaps, StretchRectFilterCaps);

    /* non-caps */
    DUMP_DIFF_VAL(VS20Caps.Caps, "0x%x");
    DUMP_DIFF_VAL(VS20Caps.DynamicFlowControlDepth, "%d");
    DUMP_DIFF_VAL(VS20Caps.NumTemps, "%d");
    DUMP_DIFF_VAL(VS20Caps.StaticFlowControlDepth, "%d");

    DUMP_DIFF_VAL(PS20Caps.Caps, "0x%x");
    DUMP_DIFF_VAL(PS20Caps.DynamicFlowControlDepth, "%d");
    DUMP_DIFF_VAL(PS20Caps.NumTemps, "%d");
    DUMP_DIFF_VAL(PS20Caps.StaticFlowControlDepth, "%d");
    DUMP_DIFF_VAL(PS20Caps.NumInstructionSlots, "%d");

    DUMP_DIFF_CAPS(printFilterCaps, VertexTextureFilterCaps);
    DUMP_DIFF_VAL(MaxVShaderInstructionsExecuted, "%d");
    DUMP_DIFF_VAL(MaxPShaderInstructionsExecuted, "%d");
    DUMP_DIFF_VAL(MaxVertexShader30InstructionSlots, "%d");
    DUMP_DIFF_VAL(MaxPixelShader30InstructionSlots, "%d");
}

static DWORD g_aCaps1[] = {
        0x00000001, 0x00000000, 0x00000000, 0xe00a0000,
        0x00000320, 0x80000001, 0x00000003, 0x0059aff1,
        0x000e6ff2, 0x077363b1, 0x000000ff, 0x00003fff,
        0x000023ff, 0x000000ff, 0x00084208, 0x0007eccd,
        0x07030700, 0x07030700, 0x03030300, 0x0000003f,
        0x0000003f, 0x0000001f, 0x00002000, 0x00002000,
        0x00000800, 0x00008000, 0x00002000, 0x00000010,
        0x3f800000, 0xc6000000, 0xc6000000, 0x46000000,
        0x46000000, 0x00000000, 0x000001ff, 0x00100008,
        0x03feffff, 0x00000008, 0x00000008, 0x0000003b,
        0x00000008, 0x00000006, 0x00000001, 0x00000000,
        0x427c0000, 0x000fffff, 0x000fffff, 0x00000010,
        0x00000400, 0xfffe0200, 0x00000100, 0xffff0200,
        0x41000000, 0x00000051, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000001, 0x0000030f,
        0x00000001, 0x03000300, 0x00000000, 0x00000018,
        0x00000020, 0x00000001, 0x00000000, 0x00000018,
        0x00000020, 0x00000000, 0x00000060, 0x01000100,
        0x0000ffff, 0x00000200, 0x00000000, 0x00000000
};


static DWORD g_aCaps2[] = {
        0x00000001, 0x00000000, 0x00020000, 0xe00a0000,
        0x00000320, 0x80000001, 0x00000003, 0x0059aff1,
        0x000e6ff2, 0x077263b1, 0x000000ff, 0x00003fff,
        0x000023ff, 0x000000ff, 0x00084208, 0x0007eccd,
        0x07030700, 0x07030700, 0x03030300, 0x0000003f,
        0x0000003f, 0x0000001f, 0x00002000, 0x00002000,
        0x00002000, 0x00008000, 0x00002000, 0x00000010,
        0x3f800000, 0xc6000000, 0xc6000000, 0x46000000,
        0x46000000, 0x00000000, 0x000001ff, 0x00100008,
        0x03feffff, 0x00000008, 0x00000008, 0x0000003b,
        0x00000008, 0x00000008, 0x00000001, 0x00000000,
        0x46000000, 0x000fffff, 0x000fffff, 0x00000010,
        0x00000400, 0xfffe0300, 0x00000100, 0xffff0300,
        0x41000000, 0x00000051, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000001, 0x0000030f,
        0x00000001, 0x03000300, 0x00000001, 0x00000018,
        0x00000020, 0x00000004, 0x0000001f, 0x00000018,
        0x00000020, 0x00000004, 0x00000200, 0x01000100,
        0x0000ffff, 0x0000ffff, 0x00008000, 0x00008000
};

int main()
{
    if (sizeof (g_aCaps1) != sizeof (D3DCAPS9))
    {
        Log(("incorrect caps 1 size (%d), expected(%d)", sizeof (g_aCaps1), sizeof (D3DCAPS9)));
        return 1;
    }

    if (sizeof (g_aCaps2) != sizeof (D3DCAPS9))
    {
        Log(("incorrect caps 2 size (%d), expected(%d)", sizeof (g_aCaps2), sizeof (D3DCAPS9)));
        return 1;
    }

    diffCaps((D3DCAPS9*)g_aCaps1, (D3DCAPS9*)g_aCaps2);
    return 0;
}
