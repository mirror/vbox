/** @file
 *
 * VirtualBox X11 Additions graphics driver
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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
 * Authors: Paulo C�ar Pereira de Andrade <pcpa@conectiva.com.br>
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/drivers/vesa/vesa.h,v 1.9 2001/05/04 19:05:49 dawes Exp $
 */

#ifndef _VBOXVIDEO_H_
#define _VBOXVIDEO_H_

#include <VBox/VBoxGuest.h>

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"

/* All drivers need this */
#include "xf86_ansic.h"

#include "compiler.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

#include "vgaHW.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

/* VBE/DDC support */
#include "vbe.h"
#include "vbeModes.h"
#include "xf86DDC.h"

/* ShadowFB support */
#include "shadow.h"

#include "shadowfb.h"

/* VBox video related defines */

#define VBE_DISPI_IOPORT_INDEX          0x01CE
#define VBE_DISPI_IOPORT_DATA           0x01CF
#define VBE_DISPI_INDEX_ID              0x0
#define VBE_DISPI_INDEX_XRES            0x1
#define VBE_DISPI_INDEX_YRES            0x2
#define VBE_DISPI_INDEX_BPP             0x3
#define VBE_DISPI_INDEX_ENABLE          0x4
#define VBE_DISPI_INDEX_VIRT_WIDTH      0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT     0x7
#define VBE_DISPI_ID2                   0xB0C2
#define VBE_DISPI_DISABLED              0x00
#define VBE_DISPI_ENABLED               0x01
#define VBE_DISPI_LFB_ENABLED           0x40

/* Int 10 support */
#include "xf86int10.h"

/* bank switching */
#include "mibank.h"

/* Dga definitions */
#include "dgaproc.h"

#include "xf86Resources.h"
#include "xf86RAC.h"

#include "xf1bpp.h"
#include "xf4bpp.h"
#include "fb.h"
#include "afb.h"
#include "mfb.h"

#define VBOX_VERSION		4000
#include "xf86Cursor.h"
#define VBOX_NAME		"VBoxVideo"
#define VBOX_DRIVER_NAME	"vboxvideo"

/*XXX*/

typedef struct _VBOXRec
{
    vbeInfoPtr pVbe;
    EntityInfoPtr pEnt;
    VbeInfoBlock *vbeInfo;
    pciVideoPtr pciInfo;
    PCITAG pciTag;
    CARD16 maxBytesPerScanline;
    unsigned long mapPhys, mapOff;
    int mapSize;	/* video memory */
    void *base, *VGAbase;
    CARD8 *state, *pstate;	/* SVGA state */
    int statePage, stateSize, stateMode;
    CARD32 *savedPal;
    CARD8 *fonts;
    /* DGA info */
    DGAModePtr pDGAMode;
    int nDGAMode;
    CloseScreenProcPtr CloseScreen;
    OptionInfoPtr Options;
    IOADDRESS ioBase;
    VMMDevReqMousePointer *reqp;
    xf86CursorInfoPtr pCurs;
    size_t pointerHeaderSize;
    size_t pointerSize;
    Bool pointerOffscreen;
    Bool useDevice;
    Bool useVbva;
    VMMDevMemory *pVMMDevMemory;
    VBVAMEMORY *pVbvaMemory;
} VBOXRec, *VBOXPtr;

extern Bool vbox_init(int scrnIndex, VBOXPtr pVBox);
extern Bool vbox_cursor_init (ScreenPtr pScreen);
extern Bool vbox_open (ScrnInfoPtr pScrn, ScreenPtr pScreen, VBOXPtr pVBox);
extern void vbox_close (ScrnInfoPtr pScrn, VBOXPtr pVBox);

extern Bool vboxEnableVbva(ScrnInfoPtr pScrn);
extern Bool vboxDisableVbva(ScrnInfoPtr pScrn);

extern Bool vboxEnableGraphicsCap(VBOXPtr pVBox);
extern Bool vboxDisableGraphicsCap(VBOXPtr pVBox);

extern Bool vboxGetDisplayChangeRequest(ScrnInfoPtr pScrn, uint32_t *pcx,
                                        uint32_t *pcy, uint32_t *pcBits,
                                        uint32_t *piDisplay, VBOXPtr pVBox);

extern Bool vboxHostLikesVideoMode(uint32_t cx, uint32_t cy, uint32_t cBits);
extern Bool vboxReportMaxGuestResolution(uint32_t u32Width, uint32_t u32Height);

#endif /* _VBOXVIDEO_H_ */
