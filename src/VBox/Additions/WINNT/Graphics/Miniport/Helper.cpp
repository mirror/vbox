/** @file
 *
 * VBoxGuest -- VirtualBox Win 2000/XP guest video driver
 *
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

// enable backdoor logging
//#define LOG_ENABLED
#ifndef VBOXWDDM
extern "C"
{
# include <ntddk.h>
}
#else
# include "VBoxVideo.h"
#endif

#include <VBox/err.h>

#include <VBox/VBoxGuestLib.h>

#ifndef VBOXWDDM
/* the video miniport headers not compatible with the NT DDK headers */
typedef struct _VIDEO_POINTER_ATTRIBUTES
{
    ULONG Flags;
    ULONG Width;
    ULONG Height;
    ULONG WidthInBytes;
    ULONG Enable;
    SHORT Column;
    SHORT Row;
    UCHAR Pixels[1];
} VIDEO_POINTER_ATTRIBUTES, *PVIDEO_POINTER_ATTRIBUTES;
#define VIDEO_MODE_COLOR_POINTER  0x04 // 1 if a color hardware pointer is
                                       // supported.
#endif /* #ifndef VBOXWDDM */

#include "Helper.h"

#ifdef DEBUG_misha
bool g_bVBoxVDbgBreakF = true;
bool g_bVBoxVDbgBreakFv = false;
#endif

/**
 * Globals
 */
/* note: should not do that but we can't use these datatypes in the global header */

#pragma alloc_text(PAGE, vboxQueryDisplayRequest)
#pragma alloc_text(PAGE, vboxLikesVideoMode)
#pragma alloc_text(PAGE, vboxGetHeightReduction)
#pragma alloc_text(PAGE, vboxQueryPointerPos)
#pragma alloc_text(PAGE, vboxQueryHostWantsAbsolute)
#pragma alloc_text(PAGE, vboxQueryWinVersion)

BOOLEAN vboxQueryDisplayRequest(uint32_t *xres, uint32_t *yres, uint32_t *bpp, uint32_t *pDisplayId)
{
    BOOLEAN bRC = FALSE;

    dprintf(("VBoxVideo::vboxQueryDisplayRequest: xres = 0x%p, yres = 0x%p bpp = 0x%p\n", xres, yres, bpp));

    VMMDevDisplayChangeRequest2 *req = NULL;

    int rc = VbglGRAlloc ((VMMDevRequestHeader **)&req, sizeof (VMMDevDisplayChangeRequest2), VMMDevReq_GetDisplayChangeRequest2);

    if (RT_FAILURE(rc))
    {
        dprintf(("VBoxVideo::vboxQueryDisplayRequest: ERROR allocating request, rc = %Rrc\n", rc));
    }
    else
    {
        req->eventAck = 0;

        rc = VbglGRPerform (&req->header);

        if (RT_SUCCESS(rc))
        {
            if (xres)
                *xres = req->xres;
            if (yres)
                *yres = req->yres;
            if (bpp)
                *bpp  = req->bpp;
            if (pDisplayId)
                *pDisplayId  = req->display;
            dprintf(("VBoxVideo::vboxQueryDisplayRequest: returning %d x %d @ %d for %d\n",
                     req->xres, req->yres, req->bpp, req->display));
            bRC = TRUE;
        }
        else
        {
            dprintf(("VBoxVideo::vboxQueryDisplayRequest: ERROR querying display request from VMMDev. "
                     "rc = %Rrc\n", rc));
        }

        VbglGRFree (&req->header);
    }

    return bRC;
}

BOOLEAN vboxLikesVideoMode(uint32_t display, uint32_t width, uint32_t height, uint32_t bpp)
{
    BOOLEAN bRC = FALSE;

    VMMDevVideoModeSupportedRequest2 *req2 = NULL;

    int rc = VbglGRAlloc((VMMDevRequestHeader**)&req2, sizeof(VMMDevVideoModeSupportedRequest2), VMMDevReq_VideoModeSupported2);
    if (RT_FAILURE(rc))
    {
        dprintf(("VBoxVideo::vboxLikesVideoMode: ERROR allocating request, rc = %Rrc\n", rc));
        /* Most likely the VBoxGuest driver is not loaded.
         * To get at least the video working, report the mode as supported.
         */
        bRC = TRUE;
    }
    else
    {
        req2->display = display;
        req2->width  = width;
        req2->height = height;
        req2->bpp    = bpp;
        rc = VbglGRPerform(&req2->header);
        if (RT_SUCCESS(rc) && RT_SUCCESS(req2->header.rc))
        {
            bRC = req2->fSupported;
        }
        else
        {
            /* Retry using old inteface. */
            AssertCompile(sizeof(VMMDevVideoModeSupportedRequest2) >= sizeof(VMMDevVideoModeSupportedRequest));
            VMMDevVideoModeSupportedRequest *req = (VMMDevVideoModeSupportedRequest *)req2;
            req->header.size        = sizeof(VMMDevVideoModeSupportedRequest);
            req->header.version     = VMMDEV_REQUEST_HEADER_VERSION;
            req->header.requestType = VMMDevReq_VideoModeSupported;
            req->header.rc          = VERR_GENERAL_FAILURE;
            req->header.reserved1   = 0;
            req->header.reserved2   = 0;
            req->width  = width;
            req->height = height;
            req->bpp    = bpp;

            rc = VbglGRPerform(&req->header);
            if (RT_SUCCESS(rc) && RT_SUCCESS(req->header.rc))
            {
                bRC = req->fSupported;
            }
            else
            {
                dprintf(("VBoxVideo::vboxLikesVideoMode: ERROR querying video mode supported status from VMMDev."
                         "rc = %Rrc\n", rc));
            }
        }
        VbglGRFree(&req2->header);
    }

    dprintf(("VBoxVideo::vboxLikesVideoMode: width: %d, height: %d, bpp: %d -> %s\n", width, height, bpp, (bRC == 1) ? "OK" : "FALSE"));

    return bRC;
}

ULONG vboxGetHeightReduction()
{
    ULONG retHeight = 0;

    dprintf(("VBoxVideo::vboxGetHeightReduction\n"));

    VMMDevGetHeightReductionRequest *req = NULL;

    int rc = VbglGRAlloc((VMMDevRequestHeader**)&req, sizeof(VMMDevGetHeightReductionRequest), VMMDevReq_GetHeightReduction);
    if (RT_FAILURE(rc))
    {
        dprintf(("VBoxVideo::vboxGetHeightReduction: ERROR allocating request, rc = %Rrc\n", rc));
    }
    else
    {
        rc = VbglGRPerform(&req->header);
        if (RT_SUCCESS(rc))
        {
            retHeight = (ULONG)req->heightReduction;
        }
        else
        {
            dprintf(("VBoxVideo::vboxGetHeightReduction: ERROR querying height reduction value from VMMDev. "
                     "rc = %Rrc\n", rc));
        }
        VbglGRFree(&req->header);
    }

    dprintf(("VBoxVideoMode::vboxGetHeightReduction: returning %d\n", retHeight));
    return retHeight;
}

static BOOLEAN vboxQueryPointerPosInternal (uint16_t *pointerXPos, uint16_t *pointerYPos)
{
    BOOLEAN bRC = FALSE;

    /* Activate next line only when really needed; floods the log very quickly! */
    /*dprintf(("VBoxVideo::vboxQueryPointerPosInternal: pointerXPos = %p, pointerYPos = %p\n", pointerXPos, pointerYPos));*/

    VMMDevReqMouseStatus *req = NULL;

    int rc = VbglGRAlloc ((VMMDevRequestHeader **)&req, sizeof (VMMDevReqMouseStatus), VMMDevReq_GetMouseStatus);

    if (RT_FAILURE(rc))
    {
        dprintf(("VBoxVideo::vboxQueryPointerPosInternal: ERROR allocating request, rc = %Rrc\n", rc));
    }
    else
    {
        rc = VbglGRPerform (&req->header);

        if (RT_SUCCESS(rc))
        {
            if (req->mouseFeatures & VMMDEV_MOUSE_HOST_CAN_ABSOLUTE)
            {
                if (pointerXPos)
                {
                    *pointerXPos = req->pointerXPos;
                }

                if (pointerYPos)
                {
                    *pointerYPos = req->pointerYPos;
                }

                bRC = TRUE;
            }
        }
        else
        {
            dprintf(("VBoxVideo::vboxQueryPointerPosInternal: ERROR querying mouse capabilities from VMMDev. "
                     "rc = %Rrc\n", rc));
        }

        VbglGRFree (&req->header);
    }

    return bRC;
}

/**
 * Return the current absolute mouse position in normalized format
 * (between 0 and 0xFFFF).
 *
 * @returns BOOLEAN     success indicator
 * @param   pointerXPos address of result variable for x pos
 * @param   pointerYPos address of result variable for y pos
 */
BOOLEAN vboxQueryPointerPos(uint16_t *pointerXPos, uint16_t *pointerYPos)
{
    if (!pointerXPos || !pointerYPos)
    {
        return FALSE;
    }

    return vboxQueryPointerPosInternal (pointerXPos, pointerYPos);
}

/**
 * Returns whether the host wants us to take absolute coordinates.
 *
 * @returns BOOLEAN TRUE if the host wants to send absolute coordinates.
 */
BOOLEAN vboxQueryHostWantsAbsolute (void)
{
    return vboxQueryPointerPosInternal (NULL, NULL);
}

winVersion_t vboxQueryWinVersion()
{
    static winVersion_t winVersion = UNKNOWN_WINVERSION;
    ULONG majorVersion;
    ULONG minorVersion;
    ULONG buildNumber;

    if (winVersion != UNKNOWN_WINVERSION)
        return winVersion;

    PsGetVersion(&majorVersion, &minorVersion, &buildNumber, NULL);

    dprintf(("VBoxVideo::vboxQueryWinVersion: running on Windows NT version %d.%d, build %d\n",
             majorVersion, minorVersion, buildNumber));
    if(majorVersion == 6)
    {
        if (minorVersion == 1)
            winVersion = WIN7;
        else if (minorVersion == 0)
            winVersion = WINVISTA; /* Or Windows Server 2008. */
    }
    else if (majorVersion == 5)
    {
        if (minorVersion >= 1)
        {
            winVersion = WINXP;
        } else
        {
            winVersion = WIN2K;
        }
    }
    else if (majorVersion == 4)
    {
        winVersion = WINNT4;
    }
    else
    {
        dprintf(("VBoxVideo::vboxQueryWinVersion: NT4 required!\n"));
    }
    return winVersion;
}

#ifndef VBOX_WITH_HGSMI
/**
 * Sends the pointer shape to the VMMDev
 *
 * @returns success indicator
 * @param pointerAttr pointer description
 */
BOOLEAN vboxUpdatePointerShape(PVIDEO_POINTER_ATTRIBUTES pointerAttr, uint32_t cbLength)
{
    uint32_t cbData = 0;

    if (pointerAttr->Enable & VBOX_MOUSE_POINTER_SHAPE)
    {
         cbData = ((((pointerAttr->Width + 7) / 8) * pointerAttr->Height + 3) & ~3)
                  + pointerAttr->Width * 4 * pointerAttr->Height;
    }

    if (cbData > cbLength - sizeof(VIDEO_POINTER_ATTRIBUTES))
    {
        dprintf(("vboxUpdatePointerShape: calculated pointer data size is too big (%d bytes, limit %d)\n",
                 cbData, cbLength - sizeof(VIDEO_POINTER_ATTRIBUTES)));
        return FALSE;
    }

    BOOLEAN bRC = FALSE;

    VMMDevReqMousePointer *req = NULL;

    int rc = VbglGRAlloc ((VMMDevRequestHeader **)&req, sizeof (VMMDevReqMousePointer) + cbData, VMMDevReq_SetPointerShape);

    if (RT_FAILURE(rc))
    {
        dprintf(("VBoxVideo::vboxUpdatePointerShape: ERROR allocating request, rc = %Rrc\n", rc));
    }
    else
    {
        /* Activate next line only when really needed; floods the log very quickly! */
        /*dprintf(("VBoxVideo::vboxUpdatePointerShape: req->u32Version = %08X\n", req->header.version));*/

        /* We have our custom flags in the field */
        req->fFlags = pointerAttr->Enable & 0xFFFF;

        /* Even if pointer is invisible, we have to pass following data,
         * so host could create the pointer with initial status - invisible
         */
        req->xHot   = (pointerAttr->Enable >> 16) & 0xFF;
        req->yHot   = (pointerAttr->Enable >> 24) & 0xFF;
        req->width  = pointerAttr->Width;
        req->height = pointerAttr->Height;

        if (req->fFlags & VBOX_MOUSE_POINTER_SHAPE)
        {
            /* copy the actual pointer data */
            memcpy (req->pointerData, pointerAttr->Pixels, cbData);
        }

        rc = VbglGRPerform (&req->header);

        if (RT_SUCCESS(rc) && RT_SUCCESS(req->header.rc))
        {
            bRC = TRUE;
        }
        else
        {
            dprintf(("VBoxVideo::vboxUpdatePointerShape: ERROR querying mouse capabilities from VMMDev. "
                     "rc = %Rrc\n", rc));
        }

        dprintf(("VBoxVideo::vboxUpdatePointerShape: req->u32Version = %08X\n", req->header.version));

        VbglGRFree (&req->header);
    }

    return bRC;
}
#endif /* VBOX_WITH_HGSMI */
