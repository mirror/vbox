/* $Id$ */
/** @file
 *
 * Linux Additions X11 graphics driver, mode setting
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 * X11 VESA driver
 *
 * Copyright (c) 2000 by Conectiva S.A. (http://www.conectiva.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * CONECTIVA LINUX BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of Conectiva Linux shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from
 * Conectiva Linux.
 *
 * Authors: Paulo César Pereira de Andrade <pcpa@conectiva.com.br>
 */

#ifdef XORG_7X
# include "xorg-server.h"
# include <string.h>
#endif
#include "vboxvideo.h"
#include "version-generated.h"
#include "product-generated.h"
#include "xf86.h"

/* VGA hardware functions for setting and restoring text mode */
#include "vgaHW.h"

/** Clear the virtual framebuffer in VRAM.  Optionally also clear up to the
 * size of a new framebuffer.  Framebuffer sizes larger than available VRAM
 * be treated as zero and passed over. */
void vboxClearVRAM(ScrnInfoPtr pScrn, int32_t cNewX, int32_t cNewY)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    uint64_t cbOldFB, cbNewFB;

    cbOldFB = pVBox->cbLine * pScrn->virtualX;
    cbNewFB = vboxLineLength(pScrn, cNewX) * cNewY;
    if (cbOldFB > (uint64_t)pVBox->cbFBMax)
        cbOldFB = 0;
    if (cbNewFB > (uint64_t)pVBox->cbFBMax)
        cbNewFB = 0;
    memset(pVBox->base, 0, max(cbOldFB, cbNewFB));
}

/** Set a graphics mode.  Poke any required values into registers, do an HGSMI
 * mode set and tell the host we support advanced graphics functions.  This
 * procedure is complicated by the fact that X.Org can implicitly disable a
 * screen by resizing the virtual framebuffer so that the screen is no longer
 * inside it.  We have to spot and handle this.
 */
Bool VBOXSetMode(ScrnInfoPtr pScrn, unsigned cDisplay, unsigned cWidth,
                 unsigned cHeight, int x, int y)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    uint32_t offStart, cwReal = cWidth;

    TRACE_LOG("cDisplay=%u, cWidth=%u, cHeight=%u, x=%d, y=%d, displayWidth=%d\n",
              cDisplay, cWidth, cHeight, x, y, pScrn->displayWidth);
    pVBox->aScreenLocation[cDisplay].cx = cWidth;
    pVBox->aScreenLocation[cDisplay].cy = cHeight;
    pVBox->aScreenLocation[cDisplay].x = x;
    pVBox->aScreenLocation[cDisplay].y = y;
    offStart = y * pVBox->cbLine + x * vboxBPP(pScrn) / 8;
    /* Deactivate the screen if the mode - specifically the virtual width - is
     * too large for VRAM as we sometimes have to do this - see comments in
     * VBOXPreInit. */
    if (   offStart + pVBox->cbLine * cHeight > pVBox->cbFBMax
        || pVBox->cbLine * pScrn->virtualY > pVBox->cbFBMax)
        return FALSE;
    /* Deactivate the screen if it is outside of the virtual framebuffer and
     * clamp it to lie inside if it is partly outside. */
    if (x >= pScrn->displayWidth || x + (int) cWidth <= 0)
        return FALSE;
    else
        cwReal = RT_MIN((int) cWidth, pScrn->displayWidth - x);
    TRACE_LOG("pVBox->afDisabled[cDisplay]=%d\n",
              (int)pVBox->afDisabled[cDisplay]);
    /* Don't fiddle with the hardware if we are switched
     * to a virtual terminal. */
    if (pVBox->vtSwitch)
        return TRUE;
    if (cDisplay == 0)
        VBoxVideoSetModeRegisters(cwReal, cHeight, pScrn->displayWidth,
                                  vboxBPP(pScrn), 0, x, y);
    /* Tell the host we support graphics */
    if (vbox_device_available(pVBox))
        vboxEnableGraphicsCap(pVBox);
    if (pVBox->fHaveHGSMI)
    {
        uint16_t fFlags = VBVA_SCREEN_F_ACTIVE;
        fFlags |= (pVBox->afDisabled[cDisplay] ? VBVA_SCREEN_F_DISABLED : 0);
        VBoxHGSMIProcessDisplayInfo(&pVBox->guestCtx, cDisplay, x, y,
                                    offStart, pVBox->cbLine, cwReal, cHeight,
                                    vboxBPP(pScrn), fFlags);
    }
    return TRUE;
}

/** Resize the virtual framebuffer.  After resizing we reset all modes
 * (X.Org 1.3+) to adjust them to the new framebuffer.
 */
Bool VBOXAdjustScreenPixmap(ScrnInfoPtr pScrn, int width, int height)
{
    ScreenPtr pScreen = pScrn->pScreen;
    PixmapPtr pPixmap = pScreen->GetScreenPixmap(pScreen);
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    uint64_t cbLine = vboxLineLength(pScrn, width);

    TRACE_LOG("width=%d, height=%d\n", width, height);
    if (!pPixmap) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Failed to get the screen pixmap.\n");
        return FALSE;
    }
    if (cbLine > UINT32_MAX || cbLine * height >= pVBox->cbFBMax)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Unable to set up a virtual screen size of %dx%d with %lu of %d Kb of video memory available.  Please increase the video memory size.\n",
                   width, height, pVBox->cbFBMax / 1024, pScrn->videoRam);
        return FALSE;
    }
    pScreen->ModifyPixmapHeader(pPixmap, width, height,
                                pScrn->depth, vboxBPP(pScrn), cbLine,
                                pVBox->base);
    vboxClearVRAM(pScrn, width, height);
    pScrn->virtualX = width;
    pScrn->virtualY = height;
    pScrn->displayWidth = vboxDisplayPitch(pScrn, cbLine);
    pVBox->cbLine = cbLine;
#ifdef VBOX_DRI
    if (pVBox->useDRI)
        VBOXDRIUpdateStride(pScrn, pVBox);
#endif
#ifdef VBOXVIDEO_13
    /* Write the new values to the hardware */
    {
        unsigned i;
        for (i = 0; i < pVBox->cScreens; ++i)
            VBOXSetMode(pScrn, i, pVBox->aScreenLocation[i].cx,
                            pVBox->aScreenLocation[i].cy,
                            pVBox->aScreenLocation[i].x,
                            pVBox->aScreenLocation[i].y);
    }
#endif
    return TRUE;
}
