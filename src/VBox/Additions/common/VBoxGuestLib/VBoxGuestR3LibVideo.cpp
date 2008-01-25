/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Video.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/string.h>
#include <iprt/mem.h>

#include "VBGLR3Internal.h"


/**
 * Enable or disable video acceleration.
 *
 * @returns VBox status code.
 *
 * @param   fEnable       Pass zero to disable, any other value to enable.
 */
VBGLR3DECL(int) VbglR3VideoAccelEnable(bool fEnable)
{
    VMMDevVideoAccelEnable Req;
    vmmdevInitRequest(&Req.header, VMMDevReq_VideoAccelEnable);
    Req.u32Enable = fEnable;
    Req.cbRingBuffer = VBVA_RING_BUFFER_SIZE;
    Req.fu32Status = 0;
    return vbglR3GRPerform(&Req.header);
}


/**
 * Flush the video buffer.
 *
 * @returns VBox status code.
 */
VBGLR3DECL(int) VbglR3VideoAccelFlush(void)
{
    VMMDevVideoAccelFlush Req;
    vmmdevInitRequest(&Req.header, VMMDevReq_VideoAccelFlush);
    return vbglR3GRPerform(&Req.header);
}


/**
 * Send mouse pointer shape information to the host.
 *
 * @returns VBox status code.
 *
 * @param   fFlags      Mouse pointer flags.
 * @param   xHot        X coordinate of hot spot.
 * @param   yHot        Y coordinate of hot spot.
 * @param   cx          Pointer width.
 * @param   cy          Pointer height.
 * @param   pvImg       Pointer to the image data (can be NULL).
 * @param   cbImg       The size of the image data not including the pre-allocated area.
 *                      This should be the size of the image data exceeding the size of
 *                      the VMMDevReqMousePointer structure.
 */
VBGLR3DECL(int) VbglR3SetPointerShape(uint32_t fFlags, uint32_t xHot, uint32_t yHot, uint32_t cx, uint32_t cy, const void *pvImg, size_t cbImg)
{
    VMMDevReqMousePointer *pReq;
    int rc = vbglR3GRAlloc((VMMDevRequestHeader **)&pReq, sizeof(VMMDevReqMousePointer) + cbImg, VMMDevReq_SetPointerShape);
    if (RT_SUCCESS(rc))
    {
        pReq->fFlags = fFlags;
        pReq->xHot = xHot;
        pReq->yHot = yHot;
        pReq->width = cx;
        pReq->height = cy;
        if (pvImg)
            memcpy(pReq->pointerData, pvImg, sizeof(pReq->pointerData) + cbImg);

        rc = vbglR3GRPerform(&pReq->header);
        vbglR3GRFree(&pReq->header);
        if (RT_SUCCESS(rc))
        {
            if (RT_SUCCESS(pReq->header.rc))
                return VINF_SUCCESS;

            rc = pReq->header.rc;
        }
    }
    return rc;
}

