/* $Id$ */

/** @file
 * VBox Miniport video modes related functions
 */

/*
 * Copyright (C) 2011-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxMPCommon.h"

#if _MSC_VER >= 1400 /* bird: MS fixed swprintf to be standard-conforming... */
#define _INC_SWPRINTF_INL_
extern "C" int __cdecl swprintf(wchar_t *, const wchar_t *, ...);
#endif
#include <wchar.h>
#include <VBox/Hardware/VBoxVideoVBE.h>

#ifdef VBOX_WITH_WDDM
# define VBOX_WITHOUT_24BPP_MODES
#endif

/* Custom video modes which are being read from registry at driver startup. */
static VIDEO_MODE_INFORMATION g_CustomVideoModes[VBOX_VIDEO_MAX_SCREENS] = { 0 };

static BOOLEAN
VBoxMPValidateVideoModeParamsGuest(PVBOXMP_DEVEXT pExt, uint32_t iDisplay, uint32_t xres, uint32_t yres, uint32_t bpp)
{
    switch (bpp)
    {
        case 32:
            break;
        case 24:
#ifdef VBOX_WITHOUT_24BPP_MODES
            return FALSE;
#else
            break;
#endif
        case 16:
            break;
        case 8:
#ifndef VBOX_WITH_8BPP_MODES
            return FALSE;
#else
#ifdef VBOX_XPDM_MINIPORT
            if (pExt->iDevice != 0) /* Secondary monitors do not support 8 bit */
                return FALSE;
#endif
            break;
#endif
        default:
            WARN(("Unexpected bpp (%d)", bpp));
            return FALSE;
    }
    return TRUE;
}

/* Fills given video mode BPP related fields */
static void
VBoxFillVidModeBPP(VIDEO_MODE_INFORMATION *pMode, ULONG bitsR, ULONG bitsG, ULONG bitsB,
                   ULONG maskR, ULONG maskG, ULONG maskB)
{
    pMode->NumberRedBits   = bitsR;
    pMode->NumberGreenBits = bitsG;
    pMode->NumberBlueBits  = bitsB;
    pMode->RedMask         = maskR;
    pMode->GreenMask       = maskG;
    pMode->BlueMask        = maskB;
}

/* Fills given video mode structure */
static void
VBoxFillVidModeInfo(VIDEO_MODE_INFORMATION *pMode, ULONG xres, ULONG yres, ULONG bpp, ULONG index, ULONG yoffset)
{
    LOGF(("%dx%d:%d (idx=%d, yoffset=%d)", xres, yres, bpp, index, yoffset));

    memset(pMode, 0, sizeof(VIDEO_MODE_INFORMATION));

    /*Common entries*/
    pMode->Length                       = sizeof(VIDEO_MODE_INFORMATION);
    pMode->ModeIndex                    = index;
    pMode->VisScreenWidth               = xres;
    pMode->VisScreenHeight              = yres - yoffset;
    pMode->ScreenStride                 = xres * ((bpp + 7) / 8);
    pMode->NumberOfPlanes               = 1;
    pMode->BitsPerPlane                 = bpp;
    pMode->Frequency                    = 60;
    pMode->XMillimeter                  = 320;
    pMode->YMillimeter                  = 240;
    pMode->VideoMemoryBitmapWidth       = xres;
    pMode->VideoMemoryBitmapHeight      = yres - yoffset;
    pMode->DriverSpecificAttributeFlags = 0;
    pMode->AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;

    /*BPP related entries*/
    switch (bpp)
    {
#ifdef VBOX_WITH_8BPP_MODES
        case 8:
            VBoxFillVidModeBPP(pMode, 6, 6, 6, 0, 0, 0);

            pMode->AttributeFlags |= VIDEO_MODE_PALETTE_DRIVEN | VIDEO_MODE_MANAGED_PALETTE;
            break;
#endif
        case 16:
            VBoxFillVidModeBPP(pMode, 5, 6, 5, 0xF800, 0x7E0, 0x1F);
            break;
        case 24:
        case 32:
            VBoxFillVidModeBPP(pMode, 8, 8, 8, 0xFF0000, 0xFF00, 0xFF);
            break;
        default:
            Assert(0);
            break;
    }
}

void VBoxMPCmnInitCustomVideoModes(PVBOXMP_DEVEXT pExt)
{
    VBOXMPCMNREGISTRY Registry;
    VP_STATUS rc;
    int iMode;

    LOGF_ENTER();

    rc = VBoxMPCmnRegInit(pExt, &Registry);
    VBOXMP_WARN_VPS(rc);

    /* Initialize all custom modes to the 800x600x32 */
    VBoxFillVidModeInfo(&g_CustomVideoModes[0], 800, 600, 32, 0, 0);
    for (iMode=1; iMode<RT_ELEMENTS(g_CustomVideoModes); ++iMode)
    {
        g_CustomVideoModes[iMode] = g_CustomVideoModes[0];
    }

    /* Read stored custom resolution info from registry */
    for (iMode=0; iMode<VBoxCommonFromDeviceExt(pExt)->cDisplays; ++iMode)
    {
        uint32_t CustomXRes = 0, CustomYRes = 0, CustomBPP = 0;

        if (iMode==0)
        {
            /*First name without a suffix*/
            rc = VBoxMPCmnRegQueryDword(Registry, L"CustomXRes", &CustomXRes);
            VBOXMP_WARN_VPS_NOBP(rc);
            rc = VBoxMPCmnRegQueryDword(Registry, L"CustomYRes", &CustomYRes);
            VBOXMP_WARN_VPS_NOBP(rc);
            rc = VBoxMPCmnRegQueryDword(Registry, L"CustomBPP", &CustomBPP);
            VBOXMP_WARN_VPS_NOBP(rc);
        }
        else
        {
            wchar_t keyname[32];
            swprintf(keyname, L"CustomXRes%d", iMode);
            rc = VBoxMPCmnRegQueryDword(Registry, keyname, &CustomXRes);
            VBOXMP_WARN_VPS_NOBP(rc);
            swprintf(keyname, L"CustomYRes%d", iMode);
            rc = VBoxMPCmnRegQueryDword(Registry, keyname, &CustomYRes);
            VBOXMP_WARN_VPS_NOBP(rc);
            swprintf(keyname, L"CustomBPP%d", iMode);
            rc = VBoxMPCmnRegQueryDword(Registry, keyname, &CustomBPP);
            VBOXMP_WARN_VPS_NOBP(rc);
        }

        LOG(("got stored custom resolution[%d] %dx%dx%d", iMode, CustomXRes, CustomYRes, CustomBPP));

        if (CustomXRes || CustomYRes || CustomBPP)
        {
            if (CustomXRes == 0)
            {
                CustomXRes = g_CustomVideoModes[iMode].VisScreenWidth;
            }
            if (CustomYRes == 0)
            {
                CustomYRes = g_CustomVideoModes[iMode].VisScreenHeight;
            }
            if (CustomBPP == 0)
            {
                CustomBPP = g_CustomVideoModes[iMode].BitsPerPlane;
            }

            if (VBoxMPValidateVideoModeParamsGuest(pExt, iMode, CustomXRes, CustomYRes, CustomBPP))
            {
                VBoxFillVidModeInfo(&g_CustomVideoModes[iMode], CustomXRes, CustomYRes, CustomBPP, 0, 0);
            }
        }
    }

    rc = VBoxMPCmnRegFini(Registry);
    VBOXMP_WARN_VPS(rc);
    LOGF_LEAVE();
}

VIDEO_MODE_INFORMATION *VBoxMPCmnGetCustomVideoModeInfo(ULONG ulIndex)
{
    return (ulIndex<RT_ELEMENTS(g_CustomVideoModes)) ? &g_CustomVideoModes[ulIndex] : NULL;
}

#ifdef VBOX_XPDM_MINIPORT
VIDEO_MODE_INFORMATION* VBoxMPCmnGetVideoModeInfo(PVBOXMP_DEVEXT pExt, ULONG ulIndex)
{
    return (ulIndex<RT_ELEMENTS(pExt->aVideoModes)) ? &pExt->aVideoModes[ulIndex] : NULL;
}
#endif

static bool VBoxMPVideoModesMatch(const PVIDEO_MODE_INFORMATION pMode1, const PVIDEO_MODE_INFORMATION pMode2)
{
    return pMode1->VisScreenHeight == pMode2->VisScreenHeight
           && pMode1->VisScreenWidth == pMode2->VisScreenWidth
           && pMode1->BitsPerPlane == pMode2->BitsPerPlane;
}

static int
VBoxMPFindVideoMode(const PVIDEO_MODE_INFORMATION pModesTable, int cModes, const PVIDEO_MODE_INFORMATION pMode)
{
    for (int i = 0; i < cModes; ++i)
    {
        if (VBoxMPVideoModesMatch(pMode, &pModesTable[i]))
        {
            return i;
        }
    }
    return -1;
}

/* Helper function to dynamically build our table of standard video
 * modes. We take the amount of VRAM and create modes with standard
 * geometries until we've either reached the maximum number of modes
 * or the available VRAM does not allow for additional modes.
 * We also check registry for manually added video modes.
 * Returns number of modes added to the table.
 */
static uint32_t
VBoxMPFillModesTable(PVBOXMP_DEVEXT pExt, int iDisplay, PVIDEO_MODE_INFORMATION pModesTable, size_t tableSize,
                     int32_t *pPrefModeIdx)
{
    /* the resolution matrix */
    struct
    {
        uint16_t xRes;
        uint16_t yRes;
    } resolutionMatrix[] =
    {
        /* standard modes */
        { 640,   480 },
        { 800,   600 },
        { 1024,  768 },
        { 1152,  864 },
        { 1280,  960 },
        { 1280, 1024 },
        { 1400, 1050 },
        { 1600, 1200 },
        { 1920, 1440 },
#ifndef VBOX_WITH_WDDM
        /* multi screen modes with 1280x1024 */
        { 2560, 1024 },
        { 3840, 1024 },
        { 5120, 1024 },
        /* multi screen modes with 1600x1200 */
        { 3200, 1200 },
        { 4800, 1200 },
        { 6400, 1200 },
#endif
    };

#ifdef VBOX_XPDM_MINIPORT
    ULONG vramSize = pExt->pPrimary->u.primary.ulMaxFrameBufferSize;
#else
    ULONG vramSize = vboxWddmVramCpuVisibleSegmentSize(pExt);
    /* at least two surfaces will be needed: primary & shadow */
    vramSize /= 2 * pExt->u.primary.commonInfo.cDisplays;
#endif

    uint32_t iMode=0, iPrefIdx=0;
    /* there are 4 color depths: 8, 16, 24 and 32bpp and we reserve 50% of the modes for other sources */
    size_t   maxModesPerColorDepth = VBOXMP_MAX_VIDEO_MODES / 2 / 4;

    /* Always add 800x600 video modes. Windows XP+ needs at least 800x600 resolution
     * and fallbacks to 800x600x4bpp VGA mode if the driver did not report suitable modes.
     * This resolution could be rejected by a low resolution host (netbooks, etc).
     */
#ifdef VBOX_WITH_8BPP_MODES
    int bytesPerPixel=1;
#else
    int bytesPerPixel=2;
#endif
    for (; bytesPerPixel<=4; bytesPerPixel++)
    {
        int bitsPerPixel = 8*bytesPerPixel;

        if (800*600*bytesPerPixel > (LONG)vramSize)
        {
            /* we don't have enough VRAM for this mode */
            continue;
        }

        if (!VBoxMPValidateVideoModeParamsGuest(pExt, iMode, 800, 600, bitsPerPixel))
            continue;

        VBoxFillVidModeInfo(&pModesTable[iMode], 800, 600, bitsPerPixel, iMode+1, 0);

        if (32==bitsPerPixel)
        {
            iPrefIdx = iMode;
        }
        ++iMode;
    }

    /* Query yoffset from the host */
    ULONG yOffset = VBoxGetHeightReduction();

    /* Iterate through our static resolution table and add supported video modes for different bpp's */
#ifdef VBOX_WITH_8BPP_MODES
    bytesPerPixel=1;
#else
    bytesPerPixel=2;
#endif
    for (; bytesPerPixel<=4; bytesPerPixel++)
    {
        int bitsPerPixel = 8*bytesPerPixel;
        size_t cAdded, resIndex;

        for (cAdded=0, resIndex=0; resIndex<RT_ELEMENTS(resolutionMatrix) && cAdded<maxModesPerColorDepth; resIndex++)
        {
            if (resolutionMatrix[resIndex].xRes * resolutionMatrix[resIndex].yRes * bytesPerPixel > (LONG)vramSize)
            {
                /* we don't have enough VRAM for this mode */
                continue;
            }

            if (yOffset == 0 && resolutionMatrix[resIndex].xRes == 800 && resolutionMatrix[resIndex].yRes == 600)
            {
                /* this mode was already added */
                continue;
            }

            if (
#ifdef VBOX_WDDM_MINIPORT
                    /* 1024x768 resolution is a minimal resolutions for win8 to make most metro apps run.
                     * For small host display resolutions, host will dislike the mode 1024x768 and above
                     * if the framebuffer window requires scrolling to fit the guest resolution.
                     * So add 1024x768 resolution for win8 guest to allow user switch to it */
                       (   (VBoxQueryWinVersion() != WIN8 && VBoxQueryWinVersion() != WIN81)
                        || resolutionMatrix[resIndex].xRes != 1024
                        || resolutionMatrix[resIndex].yRes != 768)
                    &&
#endif
                       !VBoxLikesVideoMode(iDisplay, resolutionMatrix[resIndex].xRes,
                                           resolutionMatrix[resIndex].yRes - yOffset, bitsPerPixel))
            {
                /* host doesn't like this mode */
                continue;
            }

            if (!VBoxMPValidateVideoModeParamsGuest(pExt, iDisplay, resolutionMatrix[resIndex].xRes, resolutionMatrix[resIndex].yRes, bitsPerPixel))
            {
                /* guest does not like this mode */
                continue;
            }

            /* Sanity check, we shouldn't ever get here */
            if (iMode >= tableSize)
            {
                WARN(("video modes table overflow!"));
                break;
            }

            VBoxFillVidModeInfo(&pModesTable[iMode], resolutionMatrix[resIndex].xRes, resolutionMatrix[resIndex].yRes, bitsPerPixel, iMode+1, yOffset);
            ++iMode;
            ++cAdded;
        }
    }

    /* Check registry for manually added modes, up to 128 entries is supported
     * Give up on the first error encountered.
     */
    VBOXMPCMNREGISTRY Registry;
    int fPrefSet=0;
    VP_STATUS rc;

    rc = VBoxMPCmnRegInit(pExt, &Registry);
    VBOXMP_WARN_VPS(rc);

    for (int curKey=0; curKey<128; curKey++)
    {
        if (iMode>=tableSize)
        {
            WARN(("ignoring possible custom mode(s), table is full!"));
            break;
        }

        wchar_t keyname[24];
        uint32_t xres, yres, bpp = 0;

        swprintf(keyname, L"CustomMode%dWidth", curKey);
        rc = VBoxMPCmnRegQueryDword(Registry, keyname, &xres);
        VBOXMP_CHECK_VPS_BREAK(rc);

        swprintf(keyname, L"CustomMode%dHeight", curKey);
        rc = VBoxMPCmnRegQueryDword(Registry, keyname, &yres);
        VBOXMP_CHECK_VPS_BREAK(rc);

        swprintf(keyname, L"CustomMode%dBPP", curKey);
        rc = VBoxMPCmnRegQueryDword(Registry, keyname, &bpp);
        VBOXMP_CHECK_VPS_BREAK(rc);

        LOG(("got custom mode[%u]=%ux%u:%u", curKey, xres, yres, bpp));

        /* round down width to be a multiple of 8 if necessary */
        if (!VBoxCommonFromDeviceExt(pExt)->fAnyX)
        {
            xres &= 0xFFF8;
        }

        if (   (xres > (1 << 16))
            || (yres > (1 << 16))
            || (   (bpp != 16)
                && (bpp != 24)
                && (bpp != 32)))
        {
            /* incorrect values */
            break;
        }

        /* does it fit within our VRAM? */
        if (xres * yres * (bpp / 8) > vramSize)
        {
            /* we don't have enough VRAM for this mode */
            break;
        }

        if (!VBoxLikesVideoMode(iDisplay, xres, yres, bpp))
        {
            /* host doesn't like this mode */
            break;
        }

        if (!VBoxMPValidateVideoModeParamsGuest(pExt, iDisplay, xres, yres, bpp))
        {
            /* guest does not like this mode */
            continue;
        }

        LOG(("adding video mode from registry."));

        VBoxFillVidModeInfo(&pModesTable[iMode], xres, yres, bpp, iMode+1, yOffset);

        if (!fPrefSet)
        {
            fPrefSet = 1;
            iPrefIdx = iMode;
        }
#ifdef VBOX_WDDM_MINIPORT
        /*check if the same mode has been added to the table already*/
        int foundIdx = VBoxMPFindVideoMode(pModesTable, iMode, &pModesTable[iMode]);

        if (foundIdx>=0)
        {
            if (iPrefIdx==iMode)
            {
                iPrefIdx=foundIdx;
            }
        }
        else
#endif
        {
            ++iMode;
        }
    }

    rc = VBoxMPCmnRegFini(Registry);
    VBOXMP_WARN_VPS(rc);

    if (pPrefModeIdx)
    {
        *pPrefModeIdx = iPrefIdx;
    }

    return iMode;
}

/* Returns if we're in the first mode change, ie doesn't have valid video mode set yet */
static BOOLEAN VBoxMPIsStartingUp(PVBOXMP_DEVEXT pExt, uint32_t iDisplay)
{
#ifdef VBOX_XPDM_MINIPORT
    return (pExt->CurrentMode == 0);
#else
    VBOXWDDM_SOURCE *pSource = &pExt->aSources[iDisplay];
    return !pSource->AllocData.SurfDesc.width || !pSource->AllocData.SurfDesc.height;
#endif
}

#ifdef VBOX_WDDM_MINIPORT
static const uint32_t g_aVBoxVidModesSupportedBpps[] = {
        32
#ifndef VBOX_WITHOUT_24BPP_MODES
        , 24
#endif
        , 16
#ifdef VBOX_WITH_8BPP_MODES
        , 8
#endif
};
DECLINLINE(BOOLEAN) VBoxMPIsSupportedBpp(uint32_t bpp)
{
    for (int i = 0; i < RT_ELEMENTS(g_aVBoxVidModesSupportedBpps); ++i)
    {
        if (bpp == g_aVBoxVidModesSupportedBpps[i])
            return TRUE;
    }
    return FALSE;
}

DECLINLINE(uint32_t) VBoxMPAdjustBpp(uint32_t bpp)
{
    if (VBoxMPIsSupportedBpp(bpp))
        return bpp;
    Assert(g_aVBoxVidModesSupportedBpps[0] == 32);
    return g_aVBoxVidModesSupportedBpps[0];
}
#endif
/* Updates missing video mode params with current values,
 * Checks if resulting mode is liked by the host and fits into VRAM.
 * Returns TRUE if resulting mode could be used.
 */
static BOOLEAN
VBoxMPValidateVideoModeParams(PVBOXMP_DEVEXT pExt, uint32_t iDisplay, uint32_t &xres, uint32_t &yres, uint32_t &bpp)
{
    /* Make sure all important video mode values are set */
    if (VBoxMPIsStartingUp(pExt, iDisplay))
    {
        /* Use stored custom values only if nothing was read from host. */
        xres = xres ? xres:g_CustomVideoModes[iDisplay].VisScreenWidth;
        yres = yres ? yres:g_CustomVideoModes[iDisplay].VisScreenHeight;
        bpp  = bpp  ? bpp :g_CustomVideoModes[iDisplay].BitsPerPlane;
    }
    else
    {
        /* Use current values for field which weren't read from host. */
#ifdef VBOX_XPDM_MINIPORT
        xres = xres ? xres:pExt->CurrentModeWidth;
        yres = yres ? yres:pExt->CurrentModeHeight;
        bpp  = bpp  ? bpp :pExt->CurrentModeBPP;
#else
        PVBOXWDDM_ALLOC_DATA pAllocData = pExt->aSources[iDisplay].pPrimaryAllocation ?
                  &pExt->aSources[iDisplay].pPrimaryAllocation->AllocData
                : &pExt->aSources[iDisplay].AllocData;
        xres = xres ? xres:pAllocData->SurfDesc.width;
        yres = yres ? yres:pAllocData->SurfDesc.height;
        /* VBox WDDM driver does not allow 24 modes since OS could choose the 24bit mode as default in that case,
         * the pExt->aSources[iDisplay].AllocData.SurfDesc.bpp could be initially 24 though,
         * i.e. when driver occurs the current mode on driver load via DxgkCbAcquirePostDisplayOwnership
         * and until driver reports the supported modes
         * This is true for Win8 Display-Only driver currently since DxgkCbAcquirePostDisplayOwnership is only used by it
         *
         * This is why we need to adjust the current mode bpp to the value we actually report as supported */
        bpp  = bpp  ? bpp : VBoxMPAdjustBpp(pAllocData->SurfDesc.bpp);
#endif
    }

    /* Round down width to be a multiple of 8 if necessary */
    if (!VBoxCommonFromDeviceExt(pExt)->fAnyX)
    {
        xres &= 0xFFF8;
    }

    /* We always need bpp to be set */
    if (!bpp)
    {
        bpp=32;
    }

    if (!VBoxMPValidateVideoModeParamsGuest(pExt, iDisplay, xres, yres, bpp))
    {
        WARN(("GUEST does not like special mode %dx%d:%d for display %d", xres, yres, bpp, iDisplay));
        return FALSE;
    }

    /* Check if host likes this mode */
    if (!VBoxLikesVideoMode(iDisplay, xres, yres, bpp))
    {
        WARN_NOBP(("HOST does not like special mode %dx%d:%d for display %d", xres, yres, bpp, iDisplay));
        return FALSE;
    }

#ifdef VBOX_XPDM_MINIPORT
    ULONG vramSize = pExt->pPrimary->u.primary.ulMaxFrameBufferSize;
#else
    ULONG vramSize = vboxWddmVramCpuVisibleSegmentSize(pExt);
    vramSize /= pExt->u.primary.commonInfo.cDisplays;
# ifdef VBOX_WDDM_WIN8
    if (!g_VBoxDisplayOnly)
# endif
    {
        /* at least two surfaces will be needed: primary & shadow */
        vramSize /= 2;
    }
#endif

    /* Check that values are valid and mode fits into VRAM */
    if (!xres || !yres
        || !((bpp == 16)
#ifdef VBOX_WITH_8BPP_MODES
             || (bpp == 8)
#endif
             || (bpp == 24)
             || (bpp == 32)))
    {
        LOG(("invalid params for special mode %dx%d:%d", xres, yres, bpp));
        return FALSE;
    }



    if ((xres * yres * (bpp / 8) >= vramSize))
    {
        /* Store values of last reported release log message to avoid log flooding. */
        static uint32_t s_xresNoVRAM=0, s_yresNoVRAM=0, s_bppNoVRAM=0;

        LOG(("not enough VRAM for video mode %dx%dx%dbpp. Available: %d bytes. Required: more than %d bytes.",
             xres, yres, bpp, vramSize, xres * yres * (bpp / 8)));

        s_xresNoVRAM = xres;
        s_yresNoVRAM = yres;
        s_bppNoVRAM = bpp;

        return FALSE;
    }

    return TRUE;
}

/* Checks if there's a pending video mode change hint,
 * and fills pPendingMode with associated info.
 * returns TRUE if there's a pending change. Otherwise returns FALSE.
 */
static BOOLEAN
VBoxMPCheckPendingVideoMode(PVBOXMP_DEVEXT pExt, PVIDEO_MODE_INFORMATION pPendingMode)
{
    uint32_t xres=0, yres=0, bpp=0, display=0;

    /* Check if there's a pending display change request for this display */
    if (VBoxQueryDisplayRequest(&xres, &yres, &bpp, &display) && (xres || yres || bpp))
    {
        if (display>RT_ELEMENTS(g_CustomVideoModes))
        {
            /*display = RT_ELEMENTS(g_CustomVideoModes) - 1;*/
            WARN(("VBoxQueryDisplayRequest returned invalid display number %d", display));
            return FALSE;
        }
    }
    else
    {
        LOG(("no pending request"));
        return FALSE;
    }

    /* Correct video mode params and check if host likes it */
    if (VBoxMPValidateVideoModeParams(pExt, display, xres, yres, bpp))
    {
        VBoxFillVidModeInfo(pPendingMode, xres, yres, bpp, display, 0);
        return TRUE;
    }

    return FALSE;
}

/* Save custom mode info to registry */
static void VBoxMPRegSaveModeInfo(PVBOXMP_DEVEXT pExt, uint32_t iDisplay, PVIDEO_MODE_INFORMATION pMode)
{
    VBOXMPCMNREGISTRY Registry;
    VP_STATUS rc;

    rc = VBoxMPCmnRegInit(pExt, &Registry);
    VBOXMP_WARN_VPS(rc);

    if (iDisplay==0)
    {
        /*First name without a suffix*/
        rc = VBoxMPCmnRegSetDword(Registry, L"CustomXRes", pMode->VisScreenWidth);
        VBOXMP_WARN_VPS(rc);
        rc = VBoxMPCmnRegSetDword(Registry, L"CustomYRes", pMode->VisScreenHeight);
        VBOXMP_WARN_VPS(rc);
        rc = VBoxMPCmnRegSetDword(Registry, L"CustomBPP", pMode->BitsPerPlane);
        VBOXMP_WARN_VPS(rc);
    }
    else
    {
        wchar_t keyname[32];
        swprintf(keyname, L"CustomXRes%d", iDisplay);
        rc = VBoxMPCmnRegSetDword(Registry, keyname, pMode->VisScreenWidth);
        VBOXMP_WARN_VPS(rc);
        swprintf(keyname, L"CustomYRes%d", iDisplay);
        rc = VBoxMPCmnRegSetDword(Registry, keyname, pMode->VisScreenHeight);
        VBOXMP_WARN_VPS(rc);
        swprintf(keyname, L"CustomBPP%d", iDisplay);
        rc = VBoxMPCmnRegSetDword(Registry, keyname, pMode->BitsPerPlane);
        VBOXMP_WARN_VPS(rc);
    }

    rc = VBoxMPCmnRegFini(Registry);
    VBOXMP_WARN_VPS(rc);
}

#ifdef VBOX_XPDM_MINIPORT
VIDEO_MODE_INFORMATION* VBoxMPXpdmCurrentVideoMode(PVBOXMP_DEVEXT pExt)
{
    return VBoxMPCmnGetVideoModeInfo(pExt, pExt->CurrentMode - 1);
}

ULONG VBoxMPXpdmGetVideoModesCount(PVBOXMP_DEVEXT pExt)
{
    return pExt->cVideoModes;
}

/* Makes a table of video modes consisting of:
 * Default modes
 * Custom modes manually added to registry
 * Custom modes for all displays (either from a display change hint or stored in registry)
 * 2 special modes, for a pending display change for this adapter. See comments below.
 */
void VBoxMPXpdmBuildVideoModesTable(PVBOXMP_DEVEXT pExt)
{
    uint32_t cStandartModes;
    BOOLEAN bPending, bHaveSpecial;
    VIDEO_MODE_INFORMATION specialMode;

    /* Fill table with standart modes and ones manually added to registry.
     * Up to VBOXMP_MAX_VIDEO_MODES elements can be used, the rest is reserved
     * for custom mode alternating indexes.
     */
    cStandartModes = VBoxMPFillModesTable(pExt, pExt->iDevice, pExt->aVideoModes, VBOXMP_MAX_VIDEO_MODES, NULL);

    /* Add custom mode for this display to the table */
    /* Make 2 entries in the video mode table. */
    uint32_t iModeBase = cStandartModes;

    /* Take the alternating index into account. */
    BOOLEAN bAlternativeIndex = (pExt->iInvocationCounter % 2)? TRUE: FALSE;

    uint32_t iSpecialMode = iModeBase + (bAlternativeIndex? 1: 0);
    uint32_t iStandardMode = iModeBase + (bAlternativeIndex? 0: 1);

    /* Fill the special mode. */
    memcpy(&pExt->aVideoModes[iSpecialMode], &g_CustomVideoModes[pExt->iDevice], sizeof(VIDEO_MODE_INFORMATION));
    pExt->aVideoModes[iSpecialMode].ModeIndex = iSpecialMode + 1;

    /* Wipe the other entry so it is not selected. */
    memcpy(&pExt->aVideoModes[iStandardMode], &pExt->aVideoModes[3], sizeof(VIDEO_MODE_INFORMATION));
    pExt->aVideoModes[iStandardMode].ModeIndex = iStandardMode + 1;

    LOG(("added special mode[%d] %dx%d:%d for display %d\n",
         iSpecialMode,
         pExt->aVideoModes[iSpecialMode].VisScreenWidth,
         pExt->aVideoModes[iSpecialMode].VisScreenHeight,
         pExt->aVideoModes[iSpecialMode].BitsPerPlane,
         pExt->iDevice));

    /* Check if host wants us to switch video mode and it's for this adapter */
    bPending = VBoxMPCheckPendingVideoMode(pExt, &specialMode);
    bHaveSpecial = bPending && (pExt->iDevice == specialMode.ModeIndex);
    LOG(("bPending %d, pExt->iDevice %d, specialMode.ModeIndex %d",
          bPending, pExt->iDevice, specialMode.ModeIndex));

    /* Check the startup case */
    if (!bHaveSpecial && VBoxMPIsStartingUp(pExt, pExt->iDevice))
    {
        uint32_t xres=0, yres=0, bpp=0;
        LOG(("Startup for screen %d", pExt->iDevice));
        /* Check if we could make valid mode from values stored to registry */
        if (VBoxMPValidateVideoModeParams(pExt, pExt->iDevice, xres, yres, bpp))
        {
            LOG(("Startup for screen %d validated %dx%d %d", pExt->iDevice, xres, yres, bpp));
            VBoxFillVidModeInfo(&specialMode, xres, yres, bpp, 0, 0);
            bHaveSpecial = TRUE;
        }
    }

    /* Update number of modes. Each display has 2 entries for alternating custom mode index. */
    pExt->cVideoModes = cStandartModes + 2;

    if (bHaveSpecial)
    {
        /* We need to alternate mode index entry for a pending mode change,
         * else windows will ignore actual mode change call.
         * Only alternate index if one of mode parameters changed and
         * regardless of conditions always add 2 entries to the table.
         */
        BOOLEAN bAlternativeIndex = FALSE;

        BOOLEAN bChanged = (pExt->Prev_xres!=specialMode.VisScreenWidth
                            || pExt->Prev_yres!=specialMode.VisScreenHeight
                            || pExt->Prev_bpp!=specialMode.BitsPerPlane);

        LOG(("prev %dx%dx%d, special %dx%dx%d",
             pExt->Prev_xres, pExt->Prev_yres, pExt->Prev_bpp,
             specialMode.VisScreenWidth, specialMode.VisScreenHeight, specialMode.BitsPerPlane));

        if (bChanged)
        {
            pExt->Prev_xres = specialMode.VisScreenWidth;
            pExt->Prev_yres = specialMode.VisScreenHeight;
            pExt->Prev_bpp = specialMode.BitsPerPlane;
        }

        /* Check if we need to alternate the index */
        if (!VBoxMPIsStartingUp(pExt, pExt->iDevice))
        {
            if (bChanged)
            {
                pExt->iInvocationCounter++;
            }

            if (pExt->iInvocationCounter % 2)
            {
                bAlternativeIndex = TRUE;
            }
        }

        uint32_t iSpecialModeElement = cStandartModes + (bAlternativeIndex? 1: 0);
        uint32_t iSpecialModeElementOld = cStandartModes + (bAlternativeIndex? 0: 1);

        LOG(("add special mode[%d] %dx%d:%d for display %d (bChanged=%d, bAlternativeIndex=%d)",
             iSpecialModeElement, specialMode.VisScreenWidth, specialMode.VisScreenHeight, specialMode.BitsPerPlane,
             pExt->iDevice, bChanged, bAlternativeIndex));

        /* Add special mode to the table
         * Note: Y offset isn't used for a host-supplied modes
         */
        specialMode.ModeIndex = iSpecialModeElement + 1;
        memcpy(&pExt->aVideoModes[iSpecialModeElement], &specialMode, sizeof(VIDEO_MODE_INFORMATION));

        /* Save special mode in the custom modes table */
        memcpy(&g_CustomVideoModes[pExt->iDevice], &specialMode, sizeof(VIDEO_MODE_INFORMATION));

        /* Wipe the old entry so the special mode will be found in the new positions. */
        memcpy(&pExt->aVideoModes[iSpecialModeElementOld], &pExt->aVideoModes[3], sizeof(VIDEO_MODE_INFORMATION));
        pExt->aVideoModes[iSpecialModeElementOld].ModeIndex = iSpecialModeElementOld + 1;

        /* Save special mode info to registry */
        VBoxMPRegSaveModeInfo(pExt, pExt->iDevice, &specialMode);
    }

#if defined(LOG_ENABLED)
    do
    {
        LOG(("Filled %d modes for display %d", pExt->cVideoModes, pExt->iDevice));

        for (uint32_t i=0; i < pExt->cVideoModes; ++i)
        {
            LOG(("Mode[%2d]: %4dx%4d:%2d (idx=%d)",
                 i, pExt->aVideoModes[i].VisScreenWidth, pExt->aVideoModes[i].VisScreenHeight,
                 pExt->aVideoModes[i].BitsPerPlane, pExt->aVideoModes[i].ModeIndex));
        }
    } while (0);
#endif
}
#endif /*VBOX_XPDM_MINIPORT*/

#ifdef VBOX_WDDM_MINIPORT
static VBOXWDDM_VIDEOMODES_INFO g_aVBoxVideoModeInfos[VBOX_VIDEO_MAX_SCREENS] = {0};
static VBOXWDDM_VIDEOMODES_INFO g_VBoxVideoModeTmp;

bool VBoxWddmFillMode(PVBOXMP_DEVEXT pExt, uint32_t iDisplay, VIDEO_MODE_INFORMATION *pInfo, D3DDDIFORMAT enmFormat, ULONG w, ULONG h)
{
    switch (enmFormat)
    {
        case D3DDDIFMT_A8R8G8B8:
            if (!VBoxMPValidateVideoModeParamsGuest(pExt, iDisplay, w, h, 32))
            {
                WARN(("unsupported mode info for format(%d)", enmFormat));
                return false;
            }
            VBoxFillVidModeInfo(pInfo, w, h, 32, 0, 0);
            return true;
        case D3DDDIFMT_R8G8B8:
            if (!VBoxMPValidateVideoModeParamsGuest(pExt, iDisplay, w, h, 24))
            {
                WARN(("unsupported mode info for format(%d)", enmFormat));
                return false;
            }
            VBoxFillVidModeInfo(pInfo, w, h, 24, 0, 0);
            return true;
        case D3DDDIFMT_R5G6B5:
            if (!VBoxMPValidateVideoModeParamsGuest(pExt, iDisplay, w, h, 16))
            {
                WARN(("unsupported mode info for format(%d)", enmFormat));
                return false;
            }
            VBoxFillVidModeInfo(pInfo, w, h, 16, 0, 0);
            return true;
        case D3DDDIFMT_P8:
            if (!VBoxMPValidateVideoModeParamsGuest(pExt, iDisplay, w, h, 8))
            {
                WARN(("unsupported mode info for format(%d)", enmFormat));
                return false;
            }
            VBoxFillVidModeInfo(pInfo, w, h, 8, 0, 0);
            return true;
        default:
            WARN(("unsupported enmFormat(%d)", enmFormat));
            AssertBreakpoint();
            break;
    }

    return false;
}

static void
VBoxWddmBuildResolutionTable(PVIDEO_MODE_INFORMATION pModesTable, size_t tableSize, int iPreferredMode,
                             SIZE *pResolutions, uint32_t * pcResolutions, int *piPreferredResolution)
{
    uint32_t cResolutionsArray = *pcResolutions;
    uint32_t cResolutions = 0;

    *piPreferredResolution = -1;
    *pcResolutions = 0;

    for (uint32_t i=0; i<tableSize; ++i)
    {
        PVIDEO_MODE_INFORMATION pMode = &pModesTable[i];
        int iResolution = -1;

        for (uint32_t j=0; j<cResolutions; ++j)
        {
            if (pResolutions[j].cx == pMode->VisScreenWidth
                && pResolutions[j].cy == pMode->VisScreenHeight)
            {
                iResolution = j;
                break;
            }
        }

        if (iResolution < 0)
        {
            if (cResolutions == cResolutionsArray)
            {
                WARN(("table overflow!"));
                break;
            }

            iResolution = cResolutions;
            pResolutions[cResolutions].cx = pMode->VisScreenWidth;
            pResolutions[cResolutions].cy = pMode->VisScreenHeight;
            ++cResolutions;
        }

        Assert(iResolution >= 0);
        if (i == iPreferredMode)
        {
            Assert(*piPreferredResolution == -1);
            *piPreferredResolution = iResolution;
        }
    }

    *pcResolutions = cResolutions;
    Assert(*piPreferredResolution >= 0);
}

static void VBoxWddmBuildResolutionTableForModes(PVBOXWDDM_VIDEOMODES_INFO pModes)
{
    pModes->cResolutions = RT_ELEMENTS(pModes->aResolutions);
    VBoxWddmBuildResolutionTable(pModes->aModes, pModes->cModes, pModes->iPreferredMode,
            (SIZE*)((void*)pModes->aResolutions), &pModes->cResolutions, &pModes->iPreferredResolution);
    Assert(pModes->aResolutions[pModes->iPreferredResolution].cx == pModes->aModes[pModes->iPreferredMode].VisScreenWidth
            && pModes->aResolutions[pModes->iPreferredResolution].cy == pModes->aModes[pModes->iPreferredMode].VisScreenHeight);
    Assert(pModes->cModes >= pModes->cResolutions);
}

AssertCompile(sizeof (SIZE) == sizeof (D3DKMDT_2DREGION));
AssertCompile(RT_OFFSETOF(SIZE, cx) == RT_OFFSETOF(D3DKMDT_2DREGION, cx));
AssertCompile(RT_OFFSETOF(SIZE, cy) == RT_OFFSETOF(D3DKMDT_2DREGION, cy));
static void
VBoxWddmBuildVideoModesInfo(PVBOXMP_DEVEXT pExt, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId,
                            PVBOXWDDM_VIDEOMODES_INFO pModes, VIDEO_MODE_INFORMATION *paAddlModes,
                            UINT cAddlModes)
{
    pModes->cResolutions = RT_ELEMENTS(pModes->aResolutions);

    /* Add default modes and ones read from registry. */
    pModes->cModes = VBoxMPFillModesTable(pExt, VidPnTargetId, pModes->aModes, RT_ELEMENTS(pModes->aModes), &pModes->iPreferredMode);
    Assert(pModes->cModes<=RT_ELEMENTS(pModes->aModes));

    if (!VBoxMPIsStartingUp(pExt, VidPnTargetId))
    {
        /* make sure we keep the current mode to avoid mode flickering */
        PVBOXWDDM_ALLOC_DATA pAllocData = pExt->aSources[VidPnTargetId].pPrimaryAllocation ?
                  &pExt->aSources[VidPnTargetId].pPrimaryAllocation->AllocData
                : &pExt->aSources[VidPnTargetId].AllocData;
        if (pModes->cModes < RT_ELEMENTS(pModes->aModes))
        {
            /* VBox WDDM driver does not allow 24 modes since OS could choose the 24bit mode as default in that case,
             * the pExt->aSources[iDisplay].AllocData.SurfDesc.bpp could be initially 24 though,
             * i.e. when driver occurs the current mode on driver load via DxgkCbAcquirePostDisplayOwnership
             * and until driver reports the supported modes
             * This is true for Win8 Display-Only driver currently since DxgkCbAcquirePostDisplayOwnership is only used by it
             *
             * This is why we check the bpp to be supported here and add the current mode to the list only in that case */
            if (VBoxMPIsSupportedBpp(pAllocData->SurfDesc.bpp))
            {
                int foundIdx;
                VBoxFillVidModeInfo(&pModes->aModes[pModes->cModes], pAllocData->SurfDesc.width, pAllocData->SurfDesc.height, pAllocData->SurfDesc.bpp, 1/*index*/, 0);
                if ((foundIdx=VBoxMPFindVideoMode(pModes->aModes, pModes->cModes, &pModes->aModes[pModes->cModes]))>=0)
                {
                    pModes->iPreferredMode = foundIdx;
                }
                else
                {
                    pModes->iPreferredMode = pModes->cModes;
                    ++pModes->cModes;
                }

#ifdef VBOX_WITH_8BPP_MODES
                int bytesPerPixel=1;
#else
                int bytesPerPixel=2;
#endif
                for (; bytesPerPixel<=4; bytesPerPixel++)
                {
                    int bpp = 8*bytesPerPixel;

                    if (bpp == pAllocData->SurfDesc.bpp)
                        continue;

                    if (!VBoxMPValidateVideoModeParamsGuest(pExt, VidPnTargetId,
                            pAllocData->SurfDesc.width, pAllocData->SurfDesc.height,
                            bpp))
                        continue;

                    if (pModes->cModes >= RT_ELEMENTS(pModes->aModes))
                    {
                        WARN(("ran out of video modes 2"));
                        break;
                    }

                    VBoxFillVidModeInfo(&pModes->aModes[pModes->cModes],
                                            pAllocData->SurfDesc.width, pAllocData->SurfDesc.height,
                                            bpp, pModes->cModes, 0);
                    if (VBoxMPFindVideoMode(pModes->aModes, pModes->cModes, &pModes->aModes[pModes->cModes]) < 0)
                    {
                        ++pModes->cModes;
                    }
                }
            }
        }
        else
        {
            WARN(("ran out of video modes 1"));
        }
    }

    /* Check if there's a pending display change request for this adapter */
    VIDEO_MODE_INFORMATION specialMode;
    if (VBoxMPCheckPendingVideoMode(pExt, &specialMode) && (specialMode.ModeIndex==VidPnTargetId))
    {
        /*Minor hack, ModeIndex!=0 Means this mode has been validated already and not just read from registry */
        specialMode.ModeIndex = 1;
        memcpy(&g_CustomVideoModes[VidPnTargetId], &specialMode, sizeof(VIDEO_MODE_INFORMATION));

        /* Save mode to registry */
        VBoxMPRegSaveModeInfo(pExt, VidPnTargetId, &specialMode);
    }

    /* Validate the mode which has been read from registry */
    if (!g_CustomVideoModes[VidPnTargetId].ModeIndex)
    {
        uint32_t xres, yres, bpp;

        xres = g_CustomVideoModes[VidPnTargetId].VisScreenWidth;
        yres = g_CustomVideoModes[VidPnTargetId].VisScreenHeight;
        bpp = g_CustomVideoModes[VidPnTargetId].BitsPerPlane;

        if (VBoxMPValidateVideoModeParams(pExt, VidPnTargetId, xres, yres, bpp))
        {
            VBoxFillVidModeInfo(&g_CustomVideoModes[VidPnTargetId], xres, yres, bpp, 1/*index*/, 0);
            Assert(g_CustomVideoModes[VidPnTargetId].ModeIndex == 1);
        }
    }

    /* Add custom mode to the table */
    if (g_CustomVideoModes[VidPnTargetId].ModeIndex)
    {
        if (RT_ELEMENTS(pModes->aModes) > pModes->cModes)
        {
            g_CustomVideoModes[VidPnTargetId].ModeIndex = pModes->cModes;
            pModes->aModes[pModes->cModes] = g_CustomVideoModes[VidPnTargetId];

            /* Check if we already have this mode in the table */
            int foundIdx;
            if ((foundIdx=VBoxMPFindVideoMode(pModes->aModes, pModes->cModes, &pModes->aModes[pModes->cModes]))>=0)
            {
                pModes->iPreferredMode = foundIdx;
            }
            else
            {
                pModes->iPreferredMode = pModes->cModes;
                ++pModes->cModes;
            }

            /* Add other bpp modes for this custom resolution */
#ifdef VBOX_WITH_8BPP_MODES
        UINT bpp=8;
#else
        UINT bpp=16;
#endif
            for (; bpp<=32; bpp+=8)
            {
                if (RT_ELEMENTS(pModes->aModes) == pModes->cModes)
                {
                    WARN(("table full, can't add other bpp for specail mode!"));
#ifdef DEBUG_misha
                    /* this is definitely something we do not expect */
                    AssertFailed();
#endif
                    break;
                }

                AssertRelease(RT_ELEMENTS(pModes->aModes) > pModes->cModes); /* if not - the driver state is screwed up, @todo: better do KeBugCheckEx here */

                if (pModes->aModes[pModes->iPreferredMode].BitsPerPlane == bpp)
                    continue;

                if (!VBoxMPValidateVideoModeParamsGuest(pExt, VidPnTargetId,
                        pModes->aModes[pModes->iPreferredMode].VisScreenWidth,
                        pModes->aModes[pModes->iPreferredMode].VisScreenHeight,
                        bpp))
                    continue;

                VBoxFillVidModeInfo(&pModes->aModes[pModes->cModes],
                                        pModes->aModes[pModes->iPreferredMode].VisScreenWidth,
                                        pModes->aModes[pModes->iPreferredMode].VisScreenHeight,
                                        bpp, pModes->cModes, 0);
                if (VBoxMPFindVideoMode(pModes->aModes, pModes->cModes, &pModes->aModes[pModes->cModes]) < 0)
                {
                    ++pModes->cModes;
                }
            }
        }
        else
        {
            AssertRelease(RT_ELEMENTS(pModes->aModes) == pModes->cModes); /* if not - the driver state is screwed up, @todo: better do KeBugCheckEx here */
            WARN(("table full, can't add video mode for a host request!"));
#ifdef DEBUG_misha
            /* this is definitely something we do not expect */
            AssertFailed();
#endif
        }
    }

    /* Check and Add additional modes passed in paAddlModes */
    for (UINT i=0; i<cAddlModes; ++i)
    {
        if (RT_ELEMENTS(pModes->aModes) == pModes->cModes)
        {
           WARN(("table full, can't add addl modes!"));
#ifdef DEBUG_misha
            /* this is definitely something we do not expect */
            AssertFailed();
#endif
           break;
        }

        AssertRelease(RT_ELEMENTS(pModes->aModes) > pModes->cModes); /* if not - the driver state is screwed up, @todo: better do KeBugCheckEx here */

        if (!VBoxCommonFromDeviceExt(pExt)->fAnyX)
        {
            paAddlModes[i].VisScreenWidth &= 0xFFF8;
        }

        if (VBoxLikesVideoMode(VidPnTargetId, paAddlModes[i].VisScreenWidth, paAddlModes[i].VisScreenHeight, paAddlModes[i].BitsPerPlane))
        {
            int foundIdx;
            if ((foundIdx=VBoxMPFindVideoMode(pModes->aModes, pModes->cModes, &paAddlModes[i]))>=0)
            {
                pModes->iPreferredMode = foundIdx;
            }
            else
            {
                memcpy(&pModes->aModes[pModes->cModes], &paAddlModes[i], sizeof(VIDEO_MODE_INFORMATION));
                pModes->aModes[pModes->cModes].ModeIndex = pModes->cModes;
                ++pModes->cModes;
            }
        }
    }

    /* Build resolution table */
    VBoxWddmBuildResolutionTableForModes(pModes);
}

static void VBoxWddmInitVideoMode(PVBOXMP_DEVEXT pExt, int i)
{
    VBoxWddmBuildVideoModesInfo(pExt, (D3DDDI_VIDEO_PRESENT_TARGET_ID)i, &g_aVBoxVideoModeInfos[i], NULL, 0);
}

void VBoxWddmInitVideoModes(PVBOXMP_DEVEXT pExt)
{
    for (int i = 0; i < VBoxCommonFromDeviceExt(pExt)->cDisplays; ++i)
    {
        VBoxWddmInitVideoMode(pExt, i);
    }
}

static NTSTATUS vboxWddmChildStatusReportPerform(PVBOXMP_DEVEXT pDevExt, PVBOXVDMA_CHILD_STATUS pChildStatus, D3DDDI_VIDEO_PRESENT_TARGET_ID iChild)
{
    DXGK_CHILD_STATUS DdiChildStatus;

    Assert(iChild < UINT32_MAX/2);
    Assert(iChild < (UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays);

    PVBOXWDDM_TARGET pTarget = &pDevExt->aTargets[iChild];

    if ((pChildStatus->fFlags & VBOXVDMA_CHILD_STATUS_F_DISCONNECTED)
            && pTarget->fConnected)
    {
        /* report disconnected */
        memset(&DdiChildStatus, 0, sizeof (DdiChildStatus));
        DdiChildStatus.Type = StatusConnection;
        DdiChildStatus.ChildUid = iChild;
        DdiChildStatus.HotPlug.Connected = FALSE;

        LOG(("Reporting DISCONNECT to child %d", DdiChildStatus.ChildUid));

        NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbIndicateChildStatus(pDevExt->u.primary.DxgkInterface.DeviceHandle, &DdiChildStatus);
        if (!NT_SUCCESS(Status))
        {
            WARN(("DxgkCbIndicateChildStatus failed with Status (0x%x)", Status));
            return Status;
        }
        pTarget->fConnected = FALSE;
    }

    if ((pChildStatus->fFlags & VBOXVDMA_CHILD_STATUS_F_CONNECTED)
            && !pTarget->fConnected)
    {
        /* report disconnected */
        memset(&DdiChildStatus, 0, sizeof (DdiChildStatus));
        DdiChildStatus.Type = StatusConnection;
        DdiChildStatus.ChildUid = iChild;
        DdiChildStatus.HotPlug.Connected = TRUE;

        LOG(("Reporting CONNECT to child %d", DdiChildStatus.ChildUid));

        NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbIndicateChildStatus(pDevExt->u.primary.DxgkInterface.DeviceHandle, &DdiChildStatus);
        if (!NT_SUCCESS(Status))
        {
            WARN(("DxgkCbIndicateChildStatus failed with Status (0x%x)", Status));
            return Status;
        }
        pTarget->fConnected = TRUE;
    }

    if (pChildStatus->fFlags & VBOXVDMA_CHILD_STATUS_F_ROTATED)
    {
        /* report disconnected */
        memset(&DdiChildStatus, 0, sizeof (DdiChildStatus));
        DdiChildStatus.Type = StatusRotation;
        DdiChildStatus.ChildUid = iChild;
        DdiChildStatus.Rotation.Angle = pChildStatus->u8RotationAngle;

        LOG(("Reporting ROTATED to child %d", DdiChildStatus.ChildUid));

        NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbIndicateChildStatus(pDevExt->u.primary.DxgkInterface.DeviceHandle, &DdiChildStatus);
        if (!NT_SUCCESS(Status))
        {
            WARN(("DxgkCbIndicateChildStatus failed with Status (0x%x)", Status));
            return Status;
        }
    }

    return STATUS_SUCCESS;
}

static NTSTATUS vboxWddmChildStatusHandleRequest(PVBOXMP_DEVEXT pDevExt, VBOXVDMACMD_CHILD_STATUS_IRQ *pBody)
{
    NTSTATUS Status = STATUS_SUCCESS;

    for (UINT i = 0; i < pBody->cInfos; ++i)
    {
        PVBOXVDMA_CHILD_STATUS pInfo = &pBody->aInfos[i];
        if (pBody->fFlags & VBOXVDMACMD_CHILD_STATUS_IRQ_F_APPLY_TO_ALL)
        {
            for (D3DDDI_VIDEO_PRESENT_TARGET_ID iChild = 0; iChild < (UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays; ++iChild)
            {
                Status = vboxWddmChildStatusReportPerform(pDevExt, pInfo, iChild);
                if (!NT_SUCCESS(Status))
                {
                    WARN(("vboxWddmChildStatusReportPerform failed with Status (0x%x)", Status));
                    break;
                }
            }
        }
        else
        {
            Status = vboxWddmChildStatusReportPerform(pDevExt, pInfo, pInfo->iChild);
            if (!NT_SUCCESS(Status))
            {
                WARN(("vboxWddmChildStatusReportPerform failed with Status (0x%x)", Status));
                break;
            }
        }
    }

    return Status;
}

#ifdef VBOX_WDDM_MONITOR_REPLUG_IRQ
typedef struct VBOXWDDMCHILDSTATUSCB
{
    PVBOXVDMACBUF_DR pDr;
    PKEVENT pEvent;
} VBOXWDDMCHILDSTATUSCB, *PVBOXWDDMCHILDSTATUSCB;

static DECLCALLBACK(VOID) vboxWddmChildStatusReportCompletion(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext)
{
    /* we should be called from our DPC routine */
    Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

    PVBOXWDDMCHILDSTATUSCB pCtx = (PVBOXWDDMCHILDSTATUSCB)pvContext;
    PVBOXVDMACBUF_DR pDr = pCtx->pDr;
    PVBOXVDMACMD pHdr = VBOXVDMACBUF_DR_TAIL(pDr, VBOXVDMACMD);
    VBOXVDMACMD_CHILD_STATUS_IRQ *pBody = VBOXVDMACMD_BODY(pHdr, VBOXVDMACMD_CHILD_STATUS_IRQ);

    vboxWddmChildStatusHandleRequest(pDevExt, pBody);

    vboxVdmaCBufDrFree(&pDevExt->u.primary.Vdma, pDr);

    if (pCtx->pEvent)
    {
        KeSetEvent(pCtx->pEvent, 0, FALSE);
    }
}
#endif

static NTSTATUS vboxWddmChildStatusReportReconnected(PVBOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_TARGET_ID idTarget)
{
#ifdef VBOX_WDDM_MONITOR_REPLUG_IRQ
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    UINT cbCmd = VBOXVDMACMD_SIZE_FROMBODYSIZE(sizeof (VBOXVDMACMD_CHILD_STATUS_IRQ));

    PVBOXVDMACBUF_DR pDr = vboxVdmaCBufDrCreate(&pDevExt->u.primary.Vdma, cbCmd);
    if (pDr)
    {
        // vboxVdmaCBufDrCreate zero initializes the pDr
        /* the command data follows the descriptor */
        pDr->fFlags = VBOXVDMACBUF_FLAG_BUF_FOLLOWS_DR;
        pDr->cbBuf = cbCmd;
        pDr->rc = VERR_NOT_IMPLEMENTED;

        PVBOXVDMACMD pHdr = VBOXVDMACBUF_DR_TAIL(pDr, VBOXVDMACMD);
        pHdr->enmType = VBOXVDMACMD_TYPE_CHILD_STATUS_IRQ;
        pHdr->u32CmdSpecific = 0;
        PVBOXVDMACMD_CHILD_STATUS_IRQ pBody = VBOXVDMACMD_BODY(pHdr, VBOXVDMACMD_CHILD_STATUS_IRQ);
        pBody->cInfos = 1;
        if (idTarget == D3DDDI_ID_ALL)
        {
            pBody->fFlags |= VBOXVDMACMD_CHILD_STATUS_IRQ_F_APPLY_TO_ALL;
        }
        pBody->aInfos[0].iChild = idTarget;
        pBody->aInfos[0].fFlags = VBOXVDMA_CHILD_STATUS_F_DISCONNECTED | VBOXVDMA_CHILD_STATUS_F_CONNECTED;
        /* we're going to KeWaitForSingleObject */
        Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);

        PVBOXVDMADDI_CMD pDdiCmd = VBOXVDMADDI_CMD_FROM_BUF_DR(pDr);
        VBOXWDDMCHILDSTATUSCB Ctx;
        KEVENT Event;
        KeInitializeEvent(&Event, NotificationEvent, FALSE);
        Ctx.pDr = pDr;
        Ctx.pEvent = &Event;
        vboxVdmaDdiCmdInit(pDdiCmd, 0, 0, vboxWddmChildStatusReportCompletion, &Ctx);
        /* mark command as submitted & invisible for the dx runtime since dx did not originate it */
        vboxVdmaDdiCmdSubmittedNotDx(pDdiCmd);
        int rc = vboxVdmaCBufDrSubmit(pDevExt, &pDevExt->u.primary.Vdma, pDr);
        Assert(rc == VINF_SUCCESS);
        if (RT_SUCCESS(rc))
        {
            Status = KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            Assert(Status == STATUS_SUCCESS);
            return STATUS_SUCCESS;
        }

        Status = STATUS_UNSUCCESSFUL;

        vboxVdmaCBufDrFree(&pDevExt->u.primary.Vdma, pDr);
    }
    else
    {
        /* @todo: try flushing.. */
        WARN(("vboxVdmaCBufDrCreate returned NULL"));
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
#else
    VBOXVDMACMD_CHILD_STATUS_IRQ Body = {0};
    Body.cInfos = 1;
    if (idTarget == D3DDDI_ID_ALL)
    {
        Body.fFlags |= VBOXVDMACMD_CHILD_STATUS_IRQ_F_APPLY_TO_ALL;
    }
    Body.aInfos[0].iChild = idTarget;
    Body.aInfos[0].fFlags = VBOXVDMA_CHILD_STATUS_F_DISCONNECTED | VBOXVDMA_CHILD_STATUS_F_CONNECTED;
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    return vboxWddmChildStatusHandleRequest(pDevExt, &Body);
#endif
}

NTSTATUS vboxWddmChildStatusConnect(PVBOXMP_DEVEXT pDevExt, uint32_t iChild, BOOLEAN fConnect)
{
#ifdef VBOX_WDDM_MONITOR_REPLUG_IRQ
# error "port me!"
#else
    Assert(iChild < (uint32_t)VBoxCommonFromDeviceExt(pDevExt)->cDisplays);
    NTSTATUS Status = STATUS_SUCCESS;
    VBOXVDMACMD_CHILD_STATUS_IRQ Body = {0};
    Body.cInfos = 1;
    Body.aInfos[0].iChild = iChild;
    Body.aInfos[0].fFlags = fConnect ? VBOXVDMA_CHILD_STATUS_F_CONNECTED : VBOXVDMA_CHILD_STATUS_F_DISCONNECTED;
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    Status = vboxWddmChildStatusHandleRequest(pDevExt, &Body);
    if (!NT_SUCCESS(Status))
        WARN(("vboxWddmChildStatusHandleRequest failed Status 0x%x", Status));

    return Status;
#endif
}

void VBoxWddmUpdateVideoMode(PVBOXMP_DEVEXT pExt, int i)
{
    g_VBoxVideoModeTmp = g_aVBoxVideoModeInfos[i];

    Assert(g_aVBoxVideoModeInfos[i].cModes);

    VBoxWddmInitVideoMode(pExt, i);

    if (!memcmp(&g_VBoxVideoModeTmp, &g_aVBoxVideoModeInfos[i], sizeof (g_VBoxVideoModeTmp)))
        return;

#ifdef DEBUG_misha
    g_VBoxDbgBreakModes = 0;
#endif

#ifdef DEBUG_misha
    LOGREL(("modes changed for target %d", i));
#else
    LOG(("modes changed for target %d", i));
#endif
    vboxWddmChildStatusReportReconnected(pExt, (D3DDDI_VIDEO_PRESENT_TARGET_ID)i);
}

PVBOXWDDM_VIDEOMODES_INFO VBoxWddmUpdateVideoModesInfoByMask(PVBOXMP_DEVEXT pExt, uint8_t *pScreenIdMask)
{
    for (int i = 0; i < VBoxCommonFromDeviceExt(pExt)->cDisplays; ++i)
    {
        if (!pScreenIdMask || ASMBitTest(pScreenIdMask, i))
        {
            VBoxWddmUpdateVideoMode(pExt, i);
        }
    }

    return g_aVBoxVideoModeInfos;
}

void VBoxWddmAdjustMode(PVBOXMP_DEVEXT pExt, PVBOXWDDM_ADJUSTVIDEOMODE pMode)
{
    pMode->fFlags = 0;

    if (pMode->Mode.Id >= (UINT)VBoxCommonFromDeviceExt(pExt)->cDisplays)
    {
        WARN(("invalid screen id (%d)", pMode->Mode.Id));
        pMode->fFlags = VBOXWDDM_ADJUSTVIDEOMODE_F_INVALISCREENID;
        return;
    }

    PVBOXWDDM_TARGET pTarget = &pExt->aTargets[pMode->Mode.Id];
    /* @todo: this info should go from the target actually */
    PVBOXWDDM_SOURCE pSource = &pExt->aSources[pMode->Mode.Id];

    UINT newWidth = pMode->Mode.Width;
    UINT newHeight = pMode->Mode.Height;
    UINT newBpp = pMode->Mode.BitsPerPixel;

    if (!VBoxMPValidateVideoModeParams(pExt, pMode->Mode.Id, newWidth, newHeight, newBpp))
    {
        PVBOXWDDM_SOURCE pSource = &pExt->aSources[pMode->Mode.Id];
        pMode->fFlags |= VBOXWDDM_ADJUSTVIDEOMODE_F_UNSUPPORTED;
    }

    if (pMode->Mode.Width != newWidth
            || pMode->Mode.Height != newHeight
            || pMode->Mode.BitsPerPixel != newBpp)
    {
        pMode->fFlags |= VBOXWDDM_ADJUSTVIDEOMODE_F_ADJUSTED;
        pMode->Mode.Width = newWidth;
        pMode->Mode.Height = newHeight;
        pMode->Mode.BitsPerPixel = newBpp;
    }

    if (pTarget->HeightVisible /* <- active */
            && pSource->AllocData.SurfDesc.width == pMode->Mode.Width
            && pSource->AllocData.SurfDesc.height == pMode->Mode.Height
            && pSource->AllocData.SurfDesc.bpp == pMode->Mode.BitsPerPixel)
    {
        pMode->fFlags |= VBOXWDDM_ADJUSTVIDEOMODE_F_CURRENT;
        if (pMode->fFlags & VBOXWDDM_ADJUSTVIDEOMODE_F_UNSUPPORTED)
        {
            WARN(("current mode is reported as unsupported"));
        }
    }
}

void VBoxWddmAdjustModes(PVBOXMP_DEVEXT pExt, uint32_t cModes, PVBOXWDDM_ADJUSTVIDEOMODE aModes)
{
    for (UINT i = 0; i < cModes; ++i)
    {
        PVBOXWDDM_ADJUSTVIDEOMODE pMode = &aModes[i];
        VBoxWddmAdjustMode(pExt, pMode);
    }
}

NTSTATUS VBoxWddmGetModesForResolution(VIDEO_MODE_INFORMATION *pAllModes, uint32_t cAllModes, int iSearchPreferredMode,
        const D3DKMDT_2DREGION *pResolution, VIDEO_MODE_INFORMATION * pModes, uint32_t cModes, uint32_t *pcModes, int32_t *piPreferrableMode)
{
    NTSTATUS Status = STATUS_SUCCESS;
    uint32_t cFound = 0;
    int iFoundPreferrableMode = -1;
    for (uint32_t i = 0; i < cAllModes; ++i)
    {
        VIDEO_MODE_INFORMATION *pCur = &pAllModes[i];
        if (pResolution->cx == pCur->VisScreenWidth
                        && pResolution->cy == pCur->VisScreenHeight)
        {
            if (pModes && cModes > cFound)
                memcpy(&pModes[cFound], pCur, sizeof(VIDEO_MODE_INFORMATION));
            else
                Status = STATUS_BUFFER_TOO_SMALL;

            if (i == iSearchPreferredMode)
                iFoundPreferrableMode = cFound;

            ++cFound;
        }
    }

    Assert(iFoundPreferrableMode < 0 || cFound > (uint32_t)iFoundPreferrableMode);

    *pcModes = cFound;
    if (piPreferrableMode)
        *piPreferrableMode = iFoundPreferrableMode;

    return Status;
}

PVBOXWDDM_VIDEOMODES_INFO VBoxWddmGetAllVideoModesInfos(PVBOXMP_DEVEXT pExt)
{
    return g_aVBoxVideoModeInfos;
}

PVBOXWDDM_VIDEOMODES_INFO VBoxWddmGetVideoModesInfo(PVBOXMP_DEVEXT pExt, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    return &VBoxWddmGetAllVideoModesInfos(pExt)[VidPnTargetId];
}

#endif /*VBOX_WDDM_MINIPORT*/
