/* $Id$ */
/** @file
 * VirtualBox X11 Additions graphics driver dynamic video mode functions.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "vboxvideo.h"

#define NEED_XF86_TYPES
#include <iprt/string.h>

#include "xf86.h"
#include "dixstruct.h"
#ifdef VBOX_GUESTR3XF86MOD
# define EXTENSION_PROC_ARGS char *name, GCPtr pGC
#endif
#include "extnsionst.h"
#include "windowstr.h"
#include <X11/extensions/randrproto.h>

#ifdef XORG_7X
# include <stdio.h>
# include <stdlib.h>
#endif

#ifdef VBOXVIDEO_13
# ifdef RT_OS_LINUX
# include "randrstr.h"
# include "xf86_OSproc.h"
#  include <linux/input.h>
#  ifndef EVIOCGRAB
#   define EVIOCGRAB _IOW('E', 0x90, int)
#  endif
#  ifndef KEY_SWITCHVIDEOMODE
#   define KEY_SWITCHVIDEOMODE 227
#  endif
#  include <dirent.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <unistd.h>
# endif /* RT_OS_LINUX */
#endif /* VBOXVIDEO_13 */
/**************************************************************************
* Main functions                                                          *
**************************************************************************/

/**
 * Fills a display mode M with a built-in mode of name pszName and dimensions
 * cx and cy.
 */
static void vboxFillDisplayMode(ScrnInfoPtr pScrn, DisplayModePtr m,
                                const char *pszName, unsigned cx, unsigned cy)
{
    VBOXPtr pVBox = pScrn->driverPrivate;
    char szName[256];
    DisplayModePtr pPrev = m->prev;
    DisplayModePtr pNext = m->next;

    if (!pszName)
    {
        sprintf(szName, "%ux%u", cx, cy);
        pszName = szName;
    }
    TRACE_LOG("pszName=%s, cx=%u, cy=%u\n", pszName, cx, cy);
    if (m->name)
        free((void*)m->name);
    memset(m, '\0', sizeof(*m));
    m->prev          = pPrev;
    m->next          = pNext;
    m->status        = MODE_OK;
    m->type          = M_T_BUILTIN;
    /* Older versions of VBox only support screen widths which are a multiple
     * of 8 */
    if (pVBox->fAnyX)
        m->HDisplay  = cx;
    else
        m->HDisplay  = cx & ~7;
    m->HSyncStart    = m->HDisplay + 2;
    m->HSyncEnd      = m->HDisplay + 4;
    m->HTotal        = m->HDisplay + 6;
    m->VDisplay      = cy;
    m->VSyncStart    = m->VDisplay + 2;
    m->VSyncEnd      = m->VDisplay + 4;
    m->VTotal        = m->VDisplay + 6;
    m->Clock         = m->HTotal * m->VTotal * 60 / 1000; /* kHz */
    m->name      = xnfstrdup(pszName);
}

/** vboxvideo's list of standard video modes */
struct
{
    /** mode width */
    uint32_t cx;
    /** mode height */
    uint32_t cy;
} vboxStandardModes[] =
{
    { 1600, 1200 },
    { 1440, 1050 },
    { 1280, 960 },
    { 1024, 768 },
    { 800, 600 },
    { 640, 480 },
    { 0, 0 }
};
enum
{
    vboxNumStdModes = sizeof(vboxStandardModes) / sizeof(vboxStandardModes[0])
};

/**
 * Returns a standard mode which the host likes.  Can be called multiple
 * times with the index returned by the previous call to get a list of modes.
 * @returns  the index of the mode in the list, or 0 if no more modes are
 *           available
 * @param    pScrn   the screen information structure
 * @param    pScrn->bitsPerPixel
 *                   if this is non-null, only modes with this BPP will be
 *                   returned
 * @param    cIndex  the index of the last mode queried, or 0 to query the
 *                   first mode available.  Note: the first index is 1
 * @param    pcx     where to store the mode's width
 * @param    pcy     where to store the mode's height
 * @param    pcBits  where to store the mode's BPP
 */
unsigned vboxNextStandardMode(ScrnInfoPtr pScrn, unsigned cIndex,
                              uint32_t *pcx, uint32_t *pcy)
{
    unsigned i;

    VBVXASSERT(cIndex < vboxNumStdModes,
               ("cIndex = %d, vboxNumStdModes = %d\n", cIndex,
                vboxNumStdModes));
    for (i = cIndex; i < vboxNumStdModes - 1; ++i)
    {
        uint32_t cx = vboxStandardModes[i].cx;
        uint32_t cy = vboxStandardModes[i].cy;

        if (pcx)
            *pcx = cx;
        if (pcy)
            *pcy = cy;
        return i + 1;
    }
    return 0;
}

/**
 * Allocates an empty display mode and links it into the doubly linked list of
 * modes pointed to by pScrn->modes.  Returns a pointer to the newly allocated
 * memory.
 */
static DisplayModePtr vboxAddEmptyScreenMode(ScrnInfoPtr pScrn)
{
    DisplayModePtr pMode = xnfcalloc(sizeof(DisplayModeRec), 1);

    TRACE_ENTRY();
    if (!pScrn->modes)
    {
        pScrn->modes = pMode;
        pMode->next = pMode;
        pMode->prev = pMode;
    }
    else
    {
        pMode->next = pScrn->modes;
        pMode->prev = pScrn->modes->prev;
        pMode->next->prev = pMode;
        pMode->prev->next = pMode;
    }
    return pMode;
}

/**
 * Create display mode entries in the screen information structure for each
 * of the graphics modes that we wish to support, that is:
 *  - A dynamic mode in first place which will be updated by the RandR code.
 *  - Several standard modes.
 *  - Any modes that the user requested in xorg.conf/XFree86Config.
 */
void vboxAddModes(ScrnInfoPtr pScrn)
{
    unsigned cx = 0, cy = 0, cIndex = 0;
    unsigned i;
    DisplayModePtr pMode;

    /* Add two dynamic mode entries.  When we receive a new size hint we will
     * update whichever of these is not current. */
    pMode = vboxAddEmptyScreenMode(pScrn);
    vboxFillDisplayMode(pScrn, pMode, NULL, 1024, 768);
    pMode = vboxAddEmptyScreenMode(pScrn);
    vboxFillDisplayMode(pScrn, pMode, NULL, 1024, 768);
    /* Add standard modes supported by the host */
    for ( ; ; )
    {
        cIndex = vboxNextStandardMode(pScrn, cIndex, &cx, &cy);
        if (cIndex == 0)
            break;
        pMode = vboxAddEmptyScreenMode(pScrn);
        vboxFillDisplayMode(pScrn, pMode, NULL, cx, cy);
    }
    /* And finally any modes specified by the user.  We assume here that
     * the mode names reflect the mode sizes. */
    for (i = 0; pScrn->display->modes && pScrn->display->modes[i]; i++)
    {
        if (sscanf(pScrn->display->modes[i], "%ux%u", &cx, &cy) == 2)
        {
            pMode = vboxAddEmptyScreenMode(pScrn);
            vboxFillDisplayMode(pScrn, pMode, pScrn->display->modes[i], cx, cy);
        }
    }
}

/** Set the initial values for the guest screen size hints by reading saved
 * values from files. */
/** @todo Actually read the files instead of setting dummies. */
void VBoxInitialiseSizeHints(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    DisplayModePtr pMode;
    unsigned i;

    for (i = 0; i < pVBox->cScreens; ++i)
    {
        pVBox->pScreens[i].aPreferredSize.cx = 1024;
        pVBox->pScreens[i].aPreferredSize.cy = 768;
        pVBox->pScreens[i].afConnected       = true;
    }
    /* Set up the first mode correctly to match the requested initial mode. */
    pScrn->modes->HDisplay = pVBox->pScreens[0].aPreferredSize.cx;
    pScrn->modes->VDisplay = pVBox->pScreens[0].aPreferredSize.cy;
    /* RandR 1.1 quirk: make sure that the initial resolution is always present
     * in the mode list as RandR will always advertise a mode of the initial
     * virtual resolution via GetScreenInfo. */
    pMode = vboxAddEmptyScreenMode(pScrn);
    vboxFillDisplayMode(pScrn, pMode, NULL, pVBox->pScreens[0].aPreferredSize.cx,
                        pVBox->pScreens[0].aPreferredSize.cy);
}

# define SIZE_HINTS_PROPERTY "VBOX_SIZE_HINTS"

/** Read in information about the most recent size hints requested for the
 * guest screens.  A client application sets the hint information as a root
 * window property. */
void VBoxUpdateSizeHints(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    size_t cModes;
    int32_t *paModes;
    unsigned i;

#ifdef VBOXVIDEO_13
    if (RT_SUCCESS(VBoxHGSMIGetModeHints(&pVBox->guestCtx, pVBox->cScreens,
                                         pVBox->paVBVAModeHints)))
    {
        for (i = 0; i < pVBox->cScreens; ++i)
        {
            if (pVBox->paVBVAModeHints[i].magic == VBVAMODEHINT_MAGIC)
            {
                pVBox->pScreens[i].aPreferredSize.cx =
                    pVBox->paVBVAModeHints[i].cx;
                pVBox->pScreens[i].aPreferredSize.cy =
                    pVBox->paVBVAModeHints[i].cy;
                pVBox->pScreens[i].afConnected =
                    pVBox->paVBVAModeHints[i].fEnabled;
            }
        }
        return;
    }
#endif
    if (vbvxGetIntegerPropery(pScrn, SIZE_HINTS_PROPERTY, &cModes, &paModes) != VINF_SUCCESS)
        return;
    for (i = 0; i < cModes && i < pVBox->cScreens; ++i)
    {
        if (paModes[i] != 0)
        {
            pVBox->pScreens[i].aPreferredSize.cx =
                paModes[i] >> 16;
            pVBox->pScreens[i].aPreferredSize.cy =
                paModes[i] & 0x8fff;
        }
    }
}

#ifndef VBOXVIDEO_13

/** The RandR "proc" vector, which we wrap with our own in order to notice
 * when a client sends a GetScreenInfo request. */
static int (*g_pfnVBoxRandRProc)(ClientPtr) = NULL;
/** The swapped RandR "proc" vector. */
static int (*g_pfnVBoxRandRSwappedProc)(ClientPtr) = NULL;

static void vboxRandRDispatchCore(ClientPtr pClient)
{
    xRRGetScreenInfoReq *pReq = (xRRGetScreenInfoReq *)pClient->requestBuffer;
    WindowPtr pWin;
    ScrnInfoPtr pScrn;
    VBOXPtr pVBox;
    DisplayModePtr pMode;

    if (pClient->req_len != sizeof(xRRGetScreenInfoReq) >> 2)
        return;
    pWin = (WindowPtr)SecurityLookupWindow(pReq->window, pClient,
                                           SecurityReadAccess);
    if (!pWin)
        return;
    pScrn = xf86Screens[pWin->drawable.pScreen->myNum];
    pVBox = VBOXGetRec(pScrn);
    VBoxUpdateSizeHints(pScrn);
    pMode = pScrn->modes;
    if (pScrn->currentMode == pMode)
        pMode = pMode->next;
    pMode->HDisplay = pVBox->pScreens[0].aPreferredSize.cx;
    pMode->VDisplay = pVBox->pScreens[0].aPreferredSize.cy;
}

static int vboxRandRDispatch(ClientPtr pClient)
{
    xReq *pReq = (xReq *)pClient->requestBuffer;

    if (pReq->data == X_RRGetScreenInfo)
        vboxRandRDispatchCore(pClient);
    return g_pfnVBoxRandRProc(pClient);
}

static int vboxRandRSwappedDispatch(ClientPtr pClient)
{
    xReq *pReq = (xReq *)pClient->requestBuffer;

    if (pReq->data == X_RRGetScreenInfo)
        vboxRandRDispatchCore(pClient);
    return g_pfnVBoxRandRSwappedProc(pClient);
}

static Bool vboxRandRCreateScreenResources(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    ExtensionEntry *pExt;

    pScreen->CreateScreenResources = pVBox->pfnCreateScreenResources;
    if (!pScreen->CreateScreenResources(pScreen))
        return FALSE;
    /* I doubt we can be loaded twice - should I fail here? */
    if (g_pfnVBoxRandRProc)
        return TRUE;
    pExt = CheckExtension(RANDR_NAME);
    if (!pExt)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                   "RandR extension not found, disabling dynamic resizing.\n");
        return TRUE;
    }
    if (   !ProcVector[pExt->base]
#if    !defined(XF86_VERSION_CURRENT) \
    || XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4, 3, 99, 0, 0)
    /* SwappedProcVector is not exported in XFree86, so we will not support
     * swapped byte order clients.  I doubt this is a big issue. */
        || !SwappedProcVector[pExt->base]
#endif
        )
        FatalError("RandR \"proc\" vector not initialised\n");
    g_pfnVBoxRandRProc = ProcVector[pExt->base];
    ProcVector[pExt->base] = vboxRandRDispatch;
#if    !defined(XF86_VERSION_CURRENT) \
    || XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4, 3, 99, 0, 0)
    g_pfnVBoxRandRSwappedProc = SwappedProcVector[pExt->base];
    SwappedProcVector[pExt->base] = vboxRandRSwappedDispatch;
#endif
    return TRUE;
}

/** Install our private RandR hook procedure, so that we can detect
 * GetScreenInfo requests from clients to update our dynamic mode.  This works
 * by installing a wrapper around CreateScreenResources(), which will be called
 * after RandR is initialised.  The wrapper then in turn wraps the RandR "proc"
 * vectors with its own handlers which will get called on any client RandR
 * request.  This should not be used in conjunction with RandR 1.2 or later.
 * A couple of points of interest in our RandR 1.1 support:
 *  * We use the first two screen modes as dynamic modes.  When a new mode hint
 *    arrives we update the first of the two which is not the current mode with
 *    the new size.
 *  * RandR 1.1 always advertises a mode of the size of the initial virtual
 *    resolution via GetScreenInfo(), so we make sure that a mode of that size
 *    is always present in the list.
 *  * RandR adds each new mode it sees to an internal array, but never removes
 *    entries.  This array might end up getting rather long given that we can
 *    report a lot more modes than physical hardware.
 */
void VBoxSetUpRandR11(ScreenPtr pScreen)
{
    VBOXPtr pVBox = VBOXGetRec(xf86Screens[pScreen->myNum]);

    if (!pScreen->CreateScreenResources)
        FatalError("called to early: CreateScreenResources not yet initialised\n");
    pVBox->pfnCreateScreenResources = pScreen->CreateScreenResources;
    pScreen->CreateScreenResources = vboxRandRCreateScreenResources;
}

#endif /* !VBOXVIDEO_13 */

#ifdef VBOXVIDEO_13
# ifdef RT_OS_LINUX
static void acpiEventHandler(int fd, void *pvData)
{
    struct input_event event;
    ssize_t rc;

    RRGetInfo((ScreenPtr)pvData
# if GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) >= 5
              , TRUE
# endif
             );
    do
        rc = read(fd, &event, sizeof(event));
    while (rc > 0 || (rc == -1 && errno == EINTR));
    if (rc == -1 && errno != EAGAIN)  /* Why not just return 0? */
        FatalError("Reading ACPI input event failed.\n");
}

void VBoxSetUpLinuxACPI(ScreenPtr pScreen)
{
    VBOXPtr pVBox = VBOXGetRec(xf86Screens[pScreen->myNum]);
    struct dirent *pDirent;
    DIR *pDir;
    int fd = -1;

    if (pVBox->fdACPIDevices != -1 || pVBox->hACPIEventHandler != NULL)
        FatalError("ACPI input file descriptor not initialised correctly.\n");
    pDir = opendir("/dev/input");
    if (pDir == NULL)
        return;
    for (pDirent = readdir(pDir); pDirent != NULL; pDirent = readdir(pDir))
    {
        if (strncmp(pDirent->d_name, "event", sizeof("event") - 1) == 0)
        {
#define BITS_PER_BLOCK (sizeof(unsigned long) * 8)
            char pszFile[64] = "/dev/input/";
            char pszDevice[64] = "";
            unsigned long afKeys[KEY_MAX / BITS_PER_BLOCK];

            strncat(pszFile, pDirent->d_name, sizeof(pszFile));
            if (fd != -1)
                close(fd);
            fd = open(pszFile, O_RDONLY | O_NONBLOCK);
            if (   fd == -1
                || ioctl(fd, EVIOCGNAME(sizeof(pszDevice)), pszDevice) == -1
                || strcmp(pszDevice, "Video Bus") != 0)
                continue;
            if (   ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(afKeys)), afKeys) == -1
                || ((   afKeys[KEY_SWITCHVIDEOMODE / BITS_PER_BLOCK]
                     >> KEY_SWITCHVIDEOMODE % BITS_PER_BLOCK) & 1) == 0)
                break;
            if (ioctl(fd, EVIOCGRAB, (void *)1) != 0)
                break;
            pVBox->hACPIEventHandler
                = xf86AddGeneralHandler(fd, acpiEventHandler, pScreen);
            if (pVBox->hACPIEventHandler == NULL)
                break;
            pVBox->fdACPIDevices = fd;
            fd = -1;
            break;
#undef BITS_PER_BLOCK
        }
    }
    if (fd != -1)
        close(fd);
    closedir(pDir);
}

void VBoxCleanUpLinuxACPI(ScreenPtr pScreen)
{
    VBOXPtr pVBox = VBOXGetRec(xf86Screens[pScreen->myNum]);
    if (pVBox->fdACPIDevices != -1)
        close(pVBox->fdACPIDevices);
    pVBox->fdACPIDevices = -1;
    xf86RemoveGeneralHandler(pVBox->hACPIEventHandler);
    pVBox->hACPIEventHandler = NULL;
}
# endif /* RT_OS_LINUX */
#endif /* VBOXVIDEO_13 */
