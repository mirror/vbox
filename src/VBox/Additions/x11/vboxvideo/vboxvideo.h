/** @file
 *
 * VirtualBox X11 Additions graphics driver
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/drivers/vesa/vesa.h,v 1.9 2001/05/04 19:05:49 dawes Exp $
 */

#ifndef _VBOXVIDEO_H_
#define _VBOXVIDEO_H_

#include <VBox/VBoxVideoGuest.h>
#include <VBox/VBoxVideo.h>

#ifdef DEBUG

#define TRACE_ENTRY() \
do { \
    vbvxMsg(__PRETTY_FUNCTION__); \
    vbvxMsg(": entering\n"); \
} while(0)
#define TRACE_EXIT() \
do { \
    vbvxMsg(__PRETTY_FUNCTION__); \
    vbvxMsg(": leaving\n"); \
} while(0)
#define TRACE_LOG(...) \
do { \
    vbvxMsg("%s: ", __PRETTY_FUNCTION__); \
    vbvxMsg(__VA_ARGS__); \
} while(0)
# define TRACE_LINE() do \
{ \
    vbvxMsg("%s: line %d\n", __FUNCTION__, __LINE__); \
} while(0)
#else  /* !DEBUG */

#define TRACE_ENTRY()         do { } while (0)
#define TRACE_EXIT()          do { } while (0)
#define TRACE_LOG(...)        do { } while (0)

#endif  /* !DEBUG */

/* Not just for debug builds.  If something is wrong we want to know at once. */
#define VBVXASSERT(expr, out) \
if (!(expr)) \
{ \
    vbvxMsg("\nAssertion failed!\n\n"); \
    vbvxMsg("%s\n", #expr); \
    vbvxMsg("at %s (%s:%d)\n", __PRETTY_FUNCTION__, __FILE__, __LINE__); \
    vbvxMsg out; \
    vbvxAbortServer(); \
}

#define VBVXCAST(type, val) \
         ((long long)(type)(val) == (long long)(val) \
      && (unsigned long long)(type)(val) == (unsigned long long)(val) \
    ? ((type)(val)) \
    : (vbvxMsg("Cannot convert " #val " to " #type ".\n"), vbvxAbortServer(), 0))

#define BOOL_STR(a) ((a) ? "TRUE" : "FALSE")

#include <VBox/Hardware/VBoxVideoVBE.h>

#include "xf86str.h"
#include "xf86Cursor.h"

#define VBOX_VERSION            4000  /* Why? */
#define VBOX_NAME               "VBoxVideo"
#define VBOX_DRIVER_NAME        "vboxvideo"

#ifdef VBOX_DRI_OLD
/* DRI support */
#define _XF86DRI_SERVER_
/* Hack to work around a libdrm header which is broken on Solaris */
#define u_int64_t uint64_t
/* Get rid of a warning due to a broken header file */
enum drm_bo_type { DRM_BO_TYPE };
#include "dri.h"
#undef u_int64_t
#include "sarea.h"
#include "GL/glxint.h"

/* For some reason this is not in the header files. */
extern void GlxSetVisualConfigs(int nconfigs, __GLXvisualConfig *configs,
                                void **configprivs);
#endif

#define VBOX_VIDEO_MAJOR  1
#define VBOX_VIDEO_MINOR  0
#define VBOX_DRM_DRIVER_NAME  "vboxvideo"  /* For now, as this driver is basically a stub. */
#define VBOX_DRI_DRIVER_NAME  "vboxvideo"  /* For starters. */
#define VBOX_MAX_DRAWABLES    256          /* At random. */

#define VBOXPTR(p) ((VBOXPtr)((p)->driverPrivate))

/** Helper to work round different ways of getting the root window in different
 * server versions. */
#if defined(XORG_VERSION_CURRENT) && XORG_VERSION_CURRENT < 70000000 \
    && XORG_VERSION_CURRENT >= 10900000
# define ROOT_WINDOW(pScrn) screenInfo.screens[(pScrn)->scrnIndex]->root
#else
# define ROOT_WINDOW(pScrn) WindowTable[(pScrn)->scrnIndex]
#endif

/** Structure containing all virtual monitor-specific information. */
struct VBoxScreen
{
    /** Position information for each virtual screen for the purposes of
     * sending dirty rectangle information to the right one. */
    RTRECT2 aScreenLocation;
    /** Is this CRTC enabled or in DPMS off state? */
    Bool fPowerOn;
#ifdef VBOXVIDEO_13
    /** The virtual crtcs. */
    struct _xf86Crtc *paCrtcs;
    /** The virtual outputs, logically not distinct from crtcs. */
    struct _xf86Output *paOutputs;
#endif
    /** Offsets of VBVA buffers in video RAM */
    uint32_t aoffVBVABuffer;
    /** Context information about the VBVA buffers for each screen */
    struct VBVABUFFERCONTEXT aVbvaCtx;
    /** The current preferred resolution for the screen */
    RTRECTSIZE aPreferredSize;
    /** Has this screen been enabled by the host? */
    Bool afConnected;
    Bool afLastConnected;
};

typedef struct VBOXRec
{
    EntityInfoPtr pEnt;
#ifdef PCIACCESS
    struct pci_device *pciInfo;
#else
    pciVideoPtr pciInfo;
    PCITAG pciTag;
#endif
    void *base;
    /** The amount of VRAM available for use as a framebuffer */
    unsigned long cbFBMax;
    /** The size of the framebuffer and the VBVA buffers at the end of it. */
    unsigned long cbView;
    /** The current line size in bytes */
    uint32_t cbLine;
    /** Whether the pre-X-server mode was a VBE mode */
    bool fSavedVBEMode;
    /** Paramters of the saved pre-X-server VBE mode, invalid if there is none
     */
    uint16_t cSavedWidth, cSavedHeight, cSavedPitch, cSavedBPP, fSavedFlags;
    CloseScreenProcPtr CloseScreen;
    /** Default X server procedure for enabling and disabling framebuffer access */
    xf86EnableDisableFBAccessProc *EnableDisableFBAccess;
    OptionInfoPtr Options;
    /** @todo we never actually free this */
    xf86CursorInfoPtr pCurs;
    /** Do we currently want to use the host cursor? */
    Bool fUseHardwareCursor;
    /** Number of screens attached */
    uint32_t cScreens;
    /** Information about each virtual screen. */
    struct VBoxScreen *pScreens;
    /** Can we get mode hint and cursor integration information from HGSMI? */
    bool fHaveHGSMIModeHints;
    /** Array of structures for receiving mode hints. */
    VBVAMODEHINT *paVBVAModeHints;
    /** Should we force a screen mode update next time the handler is called? */
    bool fForceModeUpdate;
    /** HGSMI guest heap context */
    HGSMIGUESTCOMMANDCONTEXT guestCtx;
    /** Unrestricted horizontal resolution flag. */
    Bool fAnyX;
#ifdef VBOX_DRI
    Bool useDRI;
#ifdef VBOX_DRI_OLD
    int cVisualConfigs;
    __GLXvisualConfig *pVisualConfigs;
    DRIInfoRec *pDRIInfo;
# endif
    int drmFD;
#endif
} VBOXRec, *VBOXPtr;

/* helpers.c */
extern void vbvxMsg(const char *pszFormat, ...);
extern void vbvxMsgV(const char *pszFormat, va_list args);
extern void vbvxAbortServer(void);
extern VBOXPtr vbvxGetRec(ScrnInfoPtr pScrn);
#define VBOXGetRec vbvxGetRec  /* Temporary */
extern int vbvxGetIntegerPropery(ScrnInfoPtr pScrn, char *pszName, size_t *pcData, int32_t **ppaData);
extern void vbvxSetIntegerPropery(ScrnInfoPtr pScrn, char *pszName, size_t cData, int32_t *paData, Bool fSendEvent);
extern void vbvxReprobeCursor(ScrnInfoPtr pScrn);

/* setmode.c */

/** Structure describing the virtual frame buffer.  It starts at the beginning
 * of the video RAM. */
struct vbvxFrameBuffer {
    /** X offset of first screen in frame buffer. */
    int x0;
    /** Y offset of first screen in frame buffer. */
    int y0;
    /** Frame buffer virtual width. */
    unsigned cWidth;
    /** Frame buffer virtual height. */
    unsigned cHeight;
    /** Bits per pixel. */
    unsigned cBPP;
};

extern void vbvxClearVRAM(ScrnInfoPtr pScrn, size_t cbOldSize, size_t cbNewSize);
extern void vbvxSetMode(ScrnInfoPtr pScrn, unsigned cDisplay, unsigned cWidth, unsigned cHeight, int x, int y, bool fEnabled,
                        bool fConnected, struct vbvxFrameBuffer *pFrameBuffer);
extern void vbvxSetSolarisMouseRange(int width, int height);

/* pointer.c */
extern Bool vbox_cursor_init (ScreenPtr pScreen);
extern void vbox_close (ScrnInfoPtr pScrn, VBOXPtr pVBox);

/* vbva.c */
extern void vbvxHandleDirtyRect(ScrnInfoPtr pScrn, int iRects, BoxPtr aRects);
extern void vbvxSetUpHGSMIHeapInGuest(VBOXPtr pVBox, uint32_t cbVRAM);
extern Bool vboxEnableVbva(ScrnInfoPtr pScrn);
extern void vboxDisableVbva(ScrnInfoPtr pScrn);

/* getmode.c */
extern void vboxAddModes(ScrnInfoPtr pScrn);
extern void VBoxInitialiseSizeHints(ScrnInfoPtr pScrn);
extern void vbvxReadSizesAndCursorIntegrationFromProperties(ScrnInfoPtr pScrn, bool *pfNeedUpdate);
extern void vbvxReadSizesAndCursorIntegrationFromHGSMI(ScrnInfoPtr pScrn, bool *pfNeedUpdate);

/* DRI stuff */
extern Bool VBOXDRIScreenInit(ScrnInfoPtr pScrn, ScreenPtr pScreen,
                              VBOXPtr pVBox);
extern Bool VBOXDRIFinishScreenInit(ScreenPtr pScreen);
extern void VBOXDRIUpdateStride(ScrnInfoPtr pScrn, VBOXPtr pVBox);
extern void VBOXDRICloseScreen(ScreenPtr pScreen, VBOXPtr pVBox);

/* EDID generation */
#ifdef VBOXVIDEO_13
extern Bool VBOXEDIDSet(struct _xf86Output *output, DisplayModePtr pmode);
#endif

#endif /* _VBOXVIDEO_H_ */

