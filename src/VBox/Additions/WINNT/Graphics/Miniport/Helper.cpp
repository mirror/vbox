/** @file
 *
 * VBoxGuest -- VirtualBox Win 2000/XP guest video driver
 *
 * Copyright (C) 2006-2007 innotek GmbH
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

extern "C"
{
#include <ntddk.h>
}

#include <VBox/err.h>

#include <VBox/VBoxGuestLib.h>

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

#include "Helper.h"

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

BOOLEAN vboxQueryDisplayRequest(uint32_t *xres, uint32_t *yres, uint32_t *bpp)
{
    BOOLEAN bRC = FALSE;

    dprintf(("VBoxVideo::vboxQueryDisplayRequest: xres = %p, yres = %p bpp = %p\n", xres, yres, bpp));

    VMMDevDisplayChangeRequest *req = NULL;

    int rc = VbglGRAlloc ((VMMDevRequestHeader **)&req, sizeof (VMMDevDisplayChangeRequest), VMMDevReq_GetDisplayChangeRequest);

    if (VBOX_FAILURE(rc))
    {
        dprintf(("VBoxVideo::vboxQueryDisplayRequest: ERROR allocating request, rc = %Vrc\n", rc));
    }
    else
    {
        req->eventAck = 0;

        rc = VbglGRPerform (&req->header);

        if (VBOX_SUCCESS(rc) && VBOX_SUCCESS(req->header.rc))
        {
            if (xres)
                *xres = req->xres;
            if (yres)
                *yres = req->yres;
            if (bpp)
                *bpp  = req->bpp;
            bRC = TRUE;
        }
        else
        {
            dprintf(("VBoxVideo::vboxQueryDisplayRequest: ERROR querying display request from VMMDev."
                     "rc = %Vrc, VMMDev rc = %Vrc\n", rc, req->header.rc));
        }

        VbglGRFree (&req->header);
    }

    return bRC;
}

BOOLEAN vboxLikesVideoMode(uint32_t width, uint32_t height, uint32_t bpp)
{
    BOOLEAN bRC = FALSE;

    dprintf(("VBoxVideo::vboxLikesVideoMode: width: %d, height: %d, bpp: %d\n", width, height, bpp));

    VMMDevVideoModeSupportedRequest *req = NULL;

    int rc = VbglGRAlloc((VMMDevRequestHeader**)&req, sizeof(VMMDevVideoModeSupportedRequest), VMMDevReq_VideoModeSupported);
    if (VBOX_FAILURE(rc))
    {
        dprintf(("VBoxVideo::vboxLikesVideoMode: ERROR allocating request, rc = %Vrc\n", rc));
        /* Most likely the VBoxGuest driver is not loaded.
         * To get at least the video working, report the mode as supported.
         */
        bRC = TRUE;
    }
    else
    {
        req->width  = width;
        req->height = height;
        req->bpp    = bpp;
        rc = VbglGRPerform(&req->header);
        if (VBOX_SUCCESS(rc) && VBOX_SUCCESS(req->header.rc))
        {
            bRC = req->fSupported;
        }
        else
        {
            dprintf(("VBoxVideo::vboxLikesVideoMode: ERROR querying video mode supported status from VMMDev."
                     "rc = %Vrc, VMMDev rc = %Vrc\n", rc, req->header.rc));
        }
        VbglGRFree(&req->header);
    }

    dprintf(("VBoxVideoMode::vboxLikesVideoMode: returning %d\n", bRC));
    return bRC;
}

ULONG vboxGetHeightReduction()
{
    ULONG retHeight = 0;

    dprintf(("VBoxVideo::vboxGetHeightReduction\n"));

    VMMDevGetHeightReductionRequest *req = NULL;

    int rc = VbglGRAlloc((VMMDevRequestHeader**)&req, sizeof(VMMDevGetHeightReductionRequest), VMMDevReq_GetHeightReduction);
    if (VBOX_FAILURE(rc))
    {
        dprintf(("VBoxVideo::vboxGetHeightReduction: ERROR allocating request, rc = %Vrc\n", rc));
    }
    else
    {
        rc = VbglGRPerform(&req->header);
        if (VBOX_SUCCESS(rc) && VBOX_SUCCESS(req->header.rc))
        {
            retHeight = (ULONG)req->heightReduction;
        }
        else
        {
            dprintf(("VBoxVideo::vboxGetHeightReduction: ERROR querying height reduction value from VMMDev."
                     "rc = %Vrc, VMMDev rc = %Vrc\n", rc, req->header.rc));
        }
        VbglGRFree(&req->header);
    }

    dprintf(("VBoxVideoMode::vboxGetHeightReduction: returning %d\n", retHeight));
    return retHeight;
}

static BOOLEAN vboxQueryPointerPosInternal (uint16_t *pointerXPos, uint16_t *pointerYPos)
{
    BOOLEAN bRC = FALSE;

    dprintf(("VBoxVideo::vboxQueryPointerPosInternal: pointerXPos = %p, pointerYPos = %p\n", pointerXPos, pointerYPos));

    VMMDevReqMouseStatus *req = NULL;

    int rc = VbglGRAlloc ((VMMDevRequestHeader **)&req, sizeof (VMMDevReqMouseStatus), VMMDevReq_GetMouseStatus);

    if (VBOX_FAILURE(rc))
    {
        dprintf(("VBoxVideo::vboxQueryPointerPosInternal: ERROR allocating request, rc = %Vrc\n", rc));
    }
    else
    {
        rc = VbglGRPerform (&req->header);

        if (VBOX_SUCCESS(rc) && VBOX_SUCCESS(req->header.rc))
        {
            if (req->mouseFeatures & VBOXGUEST_MOUSE_HOST_CAN_ABSOLUTE)
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
            dprintf(("VBoxVideo::vboxQueryPointerPosInternal: ERROR querying mouse capabilities from VMMDev."
                     "rc = %Vrc, VMMDev rc = %Vrc\n", rc, req->header.rc));
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

    if (majorVersion >= 5)
    {
        if (minorVersion >= 1)
        {
            winVersion = WINXP;
        } else
        {
            winVersion = WIN2K;
        }
    } else
    if (majorVersion == 4)
    {
        winVersion = WINNT4;
    } else
    {
        dprintf(("VBoxVideo::vboxQueryWinVersion: NT4 required!\n"));
    }
    return winVersion;
}

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

    if (VBOX_FAILURE(rc))
    {
        dprintf(("VBoxVideo::vboxUpdatePointerShape: ERROR allocating request, rc = %Vrc\n", rc));
    }
    else
    {
        dprintf(("VBoxVideo::vboxUpdatePointerShape: req->u32Version = %08X\n", req->header.version));

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

        if (VBOX_SUCCESS(rc) && VBOX_SUCCESS(req->header.rc))
        {
            bRC = TRUE;
        }
        else
        {
            dprintf(("VBoxVideo::vboxUpdatePointerShape: ERROR querying mouse capabilities from VMMDev."
                     "rc = %Vrc, VMMDev rc = %Vrc\n", rc, req->header.rc));
        }

        dprintf(("VBoxVideo::vboxUpdatePointerShape: req->u32Version = %08X\n", req->header.version));

        VbglGRFree (&req->header);
    }

    return bRC;
}
