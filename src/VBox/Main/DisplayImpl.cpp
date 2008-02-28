/** @file
 *
 * VirtualBox COM class implementation
 */

/*
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

#include "DisplayImpl.h"
#include "FramebufferImpl.h"
#include "ConsoleImpl.h"
#include "ConsoleVRDPServer.h"
#include "VMMDev.h"

#include "Logging.h"

#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/asm.h>

#include <VBox/pdmdrv.h>
#ifdef DEBUG /* for VM_ASSERT_EMT(). */
# include <VBox/vm.h>
#endif

/**
 * Display driver instance data.
 */
typedef struct DRVMAINDISPLAY
{
    /** Pointer to the display object. */
    Display                    *pDisplay;
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Pointer to the keyboard port interface of the driver/device above us. */
    PPDMIDISPLAYPORT            pUpPort;
    /** Our display connector interface. */
    PDMIDISPLAYCONNECTOR        Connector;
} DRVMAINDISPLAY, *PDRVMAINDISPLAY;

/** Converts PDMIDISPLAYCONNECTOR pointer to a DRVMAINDISPLAY pointer. */
#define PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface) ( (PDRVMAINDISPLAY) ((uintptr_t)pInterface - RT_OFFSETOF(DRVMAINDISPLAY, Connector)) )

#ifdef DEBUG_sunlover
static STAMPROFILE StatDisplayRefresh;
static int stam = 0;
#endif /* DEBUG_sunlover */

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT Display::FinalConstruct()
{
    mpVbvaMemory = NULL;
    mfVideoAccelEnabled = false;
    mfVideoAccelVRDP = false;
    mfu32SupportedOrders = 0;
    mcVideoAccelVRDPRefs = 0;

    mpPendingVbvaMemory = NULL;
    mfPendingVideoAccelEnable = false;

    mfMachineRunning = false;

    mpu8VbvaPartial = NULL;
    mcbVbvaPartial = 0;

    mParent = NULL;
    mpDrv = NULL;
    mpVMMDev = NULL;
    mfVMMDevInited = false;
    RTSemEventMultiCreate(&mUpdateSem);

    mLastAddress = NULL;
    mLastBytesPerLine = 0;
    mLastBitsPerPixel = 0,
    mLastWidth = 0;
    mLastHeight = 0;

//    mu32ResizeStatus = ResizeStatus_Void;

    return S_OK;
}

void Display::FinalRelease()
{
    if (isReady())
        uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the display object.
 *
 * @returns COM result indicator
 * @param parent          handle of our parent object
 * @param qemuConsoleData address of common console data structure
 */
HRESULT Display::init (Console *parent)
{
    LogFlowFunc (("isReady=%d", isReady()));

    ComAssertRet (parent, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = parent;

    /* reset the event sems */
    RTSemEventMultiReset(mUpdateSem);

    // by default, we have an internal framebuffer which is
    // NULL, i.e. a black hole for no display output
//    mFramebuffer = 0;
    mInternalFramebuffer = true;
    mFramebufferOpened = false;
    mSupportedAccelOps = 0;

    ULONG ul;
    mParent->machine()->COMGETTER(MonitorCount)(&ul);
    mcMonitors = ul;

    for (ul = 0; ul < mcMonitors; ul++)
    {
        maFramebuffers[ul].u32Offset = 0;
        maFramebuffers[ul].u32MaxFramebufferSize = 0;
        maFramebuffers[ul].u32InformationSize = 0;

        maFramebuffers[ul].pFramebuffer = NULL;

        maFramebuffers[ul].xOrigin = 0;
        maFramebuffers[ul].yOrigin = 0;

        maFramebuffers[ul].w = 0;
        maFramebuffers[ul].h = 0;

        maFramebuffers[ul].pHostEvents = NULL;

        maFramebuffers[ul].u32ResizeStatus = ResizeStatus_Void;

        maFramebuffers[ul].fDefaultFormat = false;

        memset (&maFramebuffers[ul].dirtyRect, 0 , sizeof (maFramebuffers[ul].dirtyRect));
    }

    mParent->RegisterCallback(this);

    setReady (true);
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void Display::uninit()
{
    LogFlowFunc (("isReady=%d\n", isReady()));

    AutoLock alock (this);
    AssertReturn (isReady(), (void) 0);

//    mFramebuffer.setNull();
    ULONG ul;
    for (ul = 0; ul < mcMonitors; ul++)
    {
        maFramebuffers[ul].pFramebuffer = NULL;
    }

    RTSemEventMultiDestroy(mUpdateSem);

    if (mParent)
    {
        mParent->UnregisterCallback(this);
    }

    if (mpDrv)
        mpDrv->pDisplay = NULL;
    mpDrv = NULL;
    mpVMMDev = NULL;
    mfVMMDevInited = true;

    setReady (false);
}

// IConsoleCallback method
STDMETHODIMP Display::OnStateChange(MachineState_T machineState)
{
    if (machineState == MachineState_Running)
    {
        LogFlowFunc (("Machine running\n"));

        mfMachineRunning = true;
    }
    else
    {
        mfMachineRunning = false;
    }
    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 *  @thread EMT
 */
static int callFramebufferResize (IFramebuffer *pFramebuffer, unsigned uScreenId,
                                  ULONG pixelFormat, void *pvVRAM,
                                  uint32_t bpp, uint32_t cbLine,
                                  int w, int h)
{
    Assert (pFramebuffer);

    /* Call the framebuffer to try and set required pixelFormat. */
    BOOL finished = TRUE;

    pFramebuffer->RequestResize (uScreenId, pixelFormat, (BYTE *) pvVRAM,
                                 bpp, cbLine, w, h, &finished);

    if (!finished)
    {
        LogFlowFunc (("External framebuffer wants us to wait!\n"));
        return VINF_VGA_RESIZE_IN_PROGRESS;
    }

    return VINF_SUCCESS;
}

/**
 *  Handles display resize event.
 *  Disables access to VGA device;
 *  calls the framebuffer RequestResize method;
 *  if framebuffer resizes synchronously,
 *      updates the display connector data and enables access to the VGA device.
 *
 *  @param w New display width
 *  @param h New display height
 *
 *  @thread EMT
 */
int Display::handleDisplayResize (unsigned uScreenId, uint32_t bpp, void *pvVRAM,
                                  uint32_t cbLine, int w, int h)
{
    LogRel (("Display::handleDisplayResize(): uScreenId = %d, pvVRAM=%p "
             "w=%d h=%d bpp=%d cbLine=0x%X\n",
             uScreenId, pvVRAM, w, h, bpp, cbLine));

    /* If there is no framebuffer, this call is not interesting. */
    if (   uScreenId >= mcMonitors
        || maFramebuffers[uScreenId].pFramebuffer.isNull())
    {
        return VINF_SUCCESS;
    }

    mLastAddress = pvVRAM;
    mLastBytesPerLine = cbLine;
    mLastBitsPerPixel = bpp,
    mLastWidth = w;
    mLastHeight = h;

    ULONG pixelFormat;

    switch (bpp)
    {
        case 32:
        case 24:
        case 16:
            pixelFormat = FramebufferPixelFormat_FOURCC_RGB;
            break;
        default:
            pixelFormat = FramebufferPixelFormat_Opaque;
            bpp = cbLine = 0;
            break;
    }

    /* Atomically set the resize status before calling the framebuffer. The new InProgress status will
     * disable access to the VGA device by the EMT thread.
     */
    bool f = ASMAtomicCmpXchgU32 (&maFramebuffers[uScreenId].u32ResizeStatus,
                                  ResizeStatus_InProgress, ResizeStatus_Void);
    AssertReleaseMsg(f, ("f = %d\n", f));NOREF(f);

    /* The framebuffer is locked in the state.
     * The lock is kept, because the framebuffer is in undefined state.
     */
    maFramebuffers[uScreenId].pFramebuffer->Lock();

    int rc = callFramebufferResize (maFramebuffers[uScreenId].pFramebuffer, uScreenId,
                                    pixelFormat, pvVRAM, bpp, cbLine, w, h);
    if (rc == VINF_VGA_RESIZE_IN_PROGRESS)
    {
        /* Immediately return to the caller. ResizeCompleted will be called back by the
         * GUI thread. The ResizeCompleted callback will change the resize status from
         * InProgress to UpdateDisplayData. The latter status will be checked by the
         * display timer callback on EMT and all required adjustments will be done there.
         */
        return rc;
    }

    /* Set the status so the 'handleResizeCompleted' would work.  */
    f = ASMAtomicCmpXchgU32 (&maFramebuffers[uScreenId].u32ResizeStatus,
                             ResizeStatus_UpdateDisplayData, ResizeStatus_InProgress);
    AssertRelease(f);NOREF(f);

    /* The method also unlocks the framebuffer. */
    handleResizeCompletedEMT();

    return VINF_SUCCESS;
}

/**
 *  Framebuffer has been resized.
 *  Read the new display data and unlock the framebuffer.
 *
 *  @thread EMT
 */
void Display::handleResizeCompletedEMT (void)
{
    LogFlowFunc(("\n"));

    unsigned uScreenId;
    for (uScreenId = 0; uScreenId < mcMonitors; uScreenId++)
    {
        DISPLAYFBINFO *pFBInfo = &maFramebuffers[uScreenId];

        /* Try to into non resizing state. */
        bool f = ASMAtomicCmpXchgU32 (&pFBInfo->u32ResizeStatus, ResizeStatus_Void, ResizeStatus_UpdateDisplayData);

        if (f == false)
        {
            /* This is not the display that has completed resizing. */
            continue;
        }

        if (uScreenId == VBOX_VIDEO_PRIMARY_SCREEN && !pFBInfo->pFramebuffer.isNull())
        {
            /* Primary framebuffer has completed the resize. Update the connector data for VGA device. */
            updateDisplayData();

            /* Check the framebuffer pixel format to setup the rendering in VGA device. */
            BOOL usesGuestVRAM = FALSE;
            pFBInfo->pFramebuffer->COMGETTER(UsesGuestVRAM) (&usesGuestVRAM);

            pFBInfo->fDefaultFormat = (usesGuestVRAM == FALSE);

            mpDrv->pUpPort->pfnSetRenderVRAM (mpDrv->pUpPort, pFBInfo->fDefaultFormat);
        }

#ifdef DEBUG_sunlover
        if (!stam)
        {
            /* protect mpVM */
            Console::SafeVMPtr pVM (mParent);
            AssertComRC (pVM.rc());

            STAM_REG(pVM, &StatDisplayRefresh, STAMTYPE_PROFILE, "/PROF/Display/Refresh", STAMUNIT_TICKS_PER_CALL, "Time spent in EMT for display updates.");
            stam = 1;
        }
#endif /* DEBUG_sunlover */

        /* Inform VRDP server about the change of display parameters. */
        LogFlowFunc (("Calling VRDP\n"));
        mParent->consoleVRDPServer()->SendResize();

        if (!pFBInfo->pFramebuffer.isNull())
        {
            /* Unlock framebuffer after evrything is done. */
            pFBInfo->pFramebuffer->Unlock();
        }
    }
}

static void checkCoordBounds (int *px, int *py, int *pw, int *ph, int cx, int cy)
{
    /* Correct negative x and y coordinates. */
    if (*px < 0)
    {
        *px += *pw; /* Compute xRight which is also the new width. */

        *pw = (*px < 0)? 0: *px;

        *px = 0;
    }

    if (*py < 0)
    {
        *py += *ph; /* Compute xBottom, which is also the new height. */

        *ph = (*py < 0)? 0: *py;

        *py = 0;
    }

    /* Also check if coords are greater than the display resolution. */
    if (*px + *pw > cx)
    {
        *pw = cx > *px? cx - *px: 0;
    }

    if (*py + *ph > cy)
    {
        *ph = cy > *py? cy - *py: 0;
    }
}

unsigned mapCoordsToScreen(DISPLAYFBINFO *pInfos, unsigned cInfos, int *px, int *py, int *pw, int *ph)
{
    DISPLAYFBINFO *pInfo = pInfos;
    unsigned uScreenId;
    LogSunlover (("mapCoordsToScreen: %d,%d %dx%d\n", *px, *py, *pw, *ph));
    for (uScreenId = 0; uScreenId < cInfos; uScreenId++, pInfo++)
    {
        LogSunlover (("    [%d] %d,%d %dx%d\n", uScreenId, pInfo->xOrigin, pInfo->yOrigin, pInfo->w, pInfo->h));
        if (   (pInfo->xOrigin <= *px && *px < pInfo->xOrigin + (int)pInfo->w)
            && (pInfo->yOrigin <= *py && *py < pInfo->yOrigin + (int)pInfo->h))
        {
            /* The rectangle belongs to the screen. Correct coordinates. */
            *px -= pInfo->xOrigin;
            *py -= pInfo->yOrigin;
            LogSunlover (("    -> %d,%d", *px, *py));
            break;
        }
    }
    if (uScreenId == cInfos)
    {
        /* Map to primary screen. */
        uScreenId = 0;
    }
    LogSunlover ((" scr %d\n", uScreenId));
    return uScreenId;
}


/**
 *  Handles display update event.
 *
 *  @param x Update area x coordinate
 *  @param y Update area y coordinate
 *  @param w Update area width
 *  @param h Update area height
 *
 *  @thread EMT
 */
void Display::handleDisplayUpdate (int x, int y, int w, int h)
{
#ifdef DEBUG_sunlover
    LogFlowFunc (("%d,%d %dx%d (%d,%d)\n",
                  x, y, w, h, mpDrv->Connector.cx, mpDrv->Connector.cy));
#endif /* DEBUG_sunlover */

    unsigned uScreenId = mapCoordsToScreen(maFramebuffers, mcMonitors, &x, &y, &w, &h);

#ifdef DEBUG_sunlover
    LogFlowFunc (("%d,%d %dx%d (checked)\n", x, y, w, h));
#endif /* DEBUG_sunlover */

    IFramebuffer *pFramebuffer = maFramebuffers[uScreenId].pFramebuffer;

    // if there is no framebuffer, this call is not interesting
    if (pFramebuffer == NULL)
        return;

    pFramebuffer->Lock();

    /* special processing for the internal framebuffer */
    if (mInternalFramebuffer)
    {
        pFramebuffer->Unlock();
    } else
    {
        /* callback into the framebuffer to notify it */
        BOOL finished = FALSE;

        RTSemEventMultiReset(mUpdateSem);

        checkCoordBounds (&x, &y, &w, &h, mpDrv->Connector.cx, mpDrv->Connector.cy);

        if (w == 0 || h == 0)
        {
            /* Nothing to be updated. */
            finished = TRUE;
        }
        else
        {
            pFramebuffer->NotifyUpdate(x, y, w, h, &finished);
        }

        if (!finished)
        {
            /*
             *  the framebuffer needs more time to process
             *  the event so we have to halt the VM until it's done
             */
            pFramebuffer->Unlock();
            RTSemEventMultiWait(mUpdateSem, RT_INDEFINITE_WAIT);
        } else
        {
            pFramebuffer->Unlock();
        }

        if (!mfVideoAccelEnabled)
        {
            /* When VBVA is enabled, the VRDP server is informed in the VideoAccelFlush.
             * Inform the server here only if VBVA is disabled.
             */
            if (maFramebuffers[uScreenId].u32ResizeStatus == ResizeStatus_Void)
            {
                mParent->consoleVRDPServer()->SendUpdateBitmap(uScreenId, x, y, w, h);
            }
        }
    }
    return;
}

typedef struct _VBVADIRTYREGION
{
    /* Copies of object's pointers used by vbvaRgn functions. */
    DISPLAYFBINFO    *paFramebuffers;
    unsigned          cMonitors;
    Display          *pDisplay;
    PPDMIDISPLAYPORT  pPort;

} VBVADIRTYREGION;

static void vbvaRgnInit (VBVADIRTYREGION *prgn, DISPLAYFBINFO *paFramebuffers, unsigned cMonitors, Display *pd, PPDMIDISPLAYPORT pp)
{
    prgn->paFramebuffers = paFramebuffers;
    prgn->cMonitors = cMonitors;
    prgn->pDisplay = pd;
    prgn->pPort = pp;

    unsigned uScreenId;
    for (uScreenId = 0; uScreenId < cMonitors; uScreenId++)
    {
        DISPLAYFBINFO *pFBInfo = &prgn->paFramebuffers[uScreenId];

        memset (&pFBInfo->dirtyRect, 0, sizeof (pFBInfo->dirtyRect));
    }
}

static void vbvaRgnDirtyRect (VBVADIRTYREGION *prgn, unsigned uScreenId, VBVACMDHDR *phdr)
{
    LogSunlover (("x = %d, y = %d, w = %d, h = %d\n",
                  phdr->x, phdr->y, phdr->w, phdr->h));

    /*
     * Here update rectangles are accumulated to form an update area.
     * @todo
     * Now the simpliest method is used which builds one rectangle that
     * includes all update areas. A bit more advanced method can be
     * employed here. The method should be fast however.
     */
    if (phdr->w == 0 || phdr->h == 0)
    {
        /* Empty rectangle. */
        return;
    }

    int32_t xRight  = phdr->x + phdr->w;
    int32_t yBottom = phdr->y + phdr->h;

    DISPLAYFBINFO *pFBInfo = &prgn->paFramebuffers[uScreenId];

    if (pFBInfo->dirtyRect.xRight == 0)
    {
        /* This is the first rectangle to be added. */
        pFBInfo->dirtyRect.xLeft   = phdr->x;
        pFBInfo->dirtyRect.yTop    = phdr->y;
        pFBInfo->dirtyRect.xRight  = xRight;
        pFBInfo->dirtyRect.yBottom = yBottom;
    }
    else
    {
        /* Adjust region coordinates. */
        if (pFBInfo->dirtyRect.xLeft > phdr->x)
        {
            pFBInfo->dirtyRect.xLeft = phdr->x;
        }

        if (pFBInfo->dirtyRect.yTop > phdr->y)
        {
            pFBInfo->dirtyRect.yTop = phdr->y;
        }

        if (pFBInfo->dirtyRect.xRight < xRight)
        {
            pFBInfo->dirtyRect.xRight = xRight;
        }

        if (pFBInfo->dirtyRect.yBottom < yBottom)
        {
            pFBInfo->dirtyRect.yBottom = yBottom;
        }
    }

    if (pFBInfo->fDefaultFormat)
    {
        //@todo pfnUpdateDisplayRect must take the vram offset parameter for the framebuffer
        prgn->pPort->pfnUpdateDisplayRect (prgn->pPort, phdr->x, phdr->y, phdr->w, phdr->h);
        prgn->pDisplay->handleDisplayUpdate (phdr->x, phdr->y, phdr->w, phdr->h);
    }

    return;
}

static void vbvaRgnUpdateFramebuffer (VBVADIRTYREGION *prgn, unsigned uScreenId)
{
    DISPLAYFBINFO *pFBInfo = &prgn->paFramebuffers[uScreenId];

    uint32_t w = pFBInfo->dirtyRect.xRight - pFBInfo->dirtyRect.xLeft;
    uint32_t h = pFBInfo->dirtyRect.yBottom - pFBInfo->dirtyRect.yTop;

    if (!pFBInfo->fDefaultFormat && pFBInfo->pFramebuffer && w != 0 && h != 0)
    {
        //@todo pfnUpdateDisplayRect must take the vram offset parameter for the framebuffer
        prgn->pPort->pfnUpdateDisplayRect (prgn->pPort, pFBInfo->dirtyRect.xLeft, pFBInfo->dirtyRect.yTop, w, h);
        prgn->pDisplay->handleDisplayUpdate (pFBInfo->dirtyRect.xLeft, pFBInfo->dirtyRect.yTop, w, h);
    }
}

static void vbvaSetMemoryFlags (VBVAMEMORY *pVbvaMemory,
                                bool fVideoAccelEnabled,
                                bool fVideoAccelVRDP,
                                uint32_t fu32SupportedOrders,
                                DISPLAYFBINFO *paFBInfos,
                                unsigned cFBInfos)
{
    if (pVbvaMemory)
    {
        /* This called only on changes in mode. So reset VRDP always. */
        uint32_t fu32Flags = VBVA_F_MODE_VRDP_RESET;

        if (fVideoAccelEnabled)
        {
            fu32Flags |= VBVA_F_MODE_ENABLED;

            if (fVideoAccelVRDP)
            {
                fu32Flags |= VBVA_F_MODE_VRDP | VBVA_F_MODE_VRDP_ORDER_MASK;

                pVbvaMemory->fu32SupportedOrders = fu32SupportedOrders;
            }
        }

        pVbvaMemory->fu32ModeFlags = fu32Flags;
    }

    unsigned uScreenId;
    for (uScreenId = 0; uScreenId < cFBInfos; uScreenId++)
    {
        if (paFBInfos[uScreenId].pHostEvents)
        {
            paFBInfos[uScreenId].pHostEvents->fu32Events |= VBOX_VIDEO_INFO_HOST_EVENTS_F_VRDP_RESET;
        }
    }
}

bool Display::VideoAccelAllowed (void)
{
    return true;
}

/**
 * @thread EMT
 */
int Display::VideoAccelEnable (bool fEnable, VBVAMEMORY *pVbvaMemory)
{
    int rc = VINF_SUCCESS;

    /* Called each time the guest wants to use acceleration,
     * or when the VGA device disables acceleration,
     * or when restoring the saved state with accel enabled.
     *
     * VGA device disables acceleration on each video mode change
     * and on reset.
     *
     * Guest enabled acceleration at will. And it has to enable
     * acceleration after a mode change.
     */
    LogFlowFunc (("mfVideoAccelEnabled = %d, fEnable = %d, pVbvaMemory = %p\n",
                  mfVideoAccelEnabled, fEnable, pVbvaMemory));

    /* Strictly check parameters. Callers must not pass anything in the case. */
    Assert((fEnable && pVbvaMemory) || (!fEnable && pVbvaMemory == NULL));

    if (!VideoAccelAllowed ())
    {
        return VERR_NOT_SUPPORTED;
    }

    /*
     * Verify that the VM is in running state. If it is not,
     * then this must be postponed until it goes to running.
     */
    if (!mfMachineRunning)
    {
        Assert (!mfVideoAccelEnabled);

        LogFlowFunc (("Machine is not yet running.\n"));

        if (fEnable)
        {
            mfPendingVideoAccelEnable = fEnable;
            mpPendingVbvaMemory = pVbvaMemory;
        }

        return rc;
    }

    /* Check that current status is not being changed */
    if (mfVideoAccelEnabled == fEnable)
    {
        return rc;
    }

    if (mfVideoAccelEnabled)
    {
        /* Process any pending orders and empty the VBVA ring buffer. */
        VideoAccelFlush ();
    }

    if (!fEnable && mpVbvaMemory)
    {
        mpVbvaMemory->fu32ModeFlags &= ~VBVA_F_MODE_ENABLED;
    }

    /* Safety precaution. There is no more VBVA until everything is setup! */
    mpVbvaMemory = NULL;
    mfVideoAccelEnabled = false;

    /* Update entire display. */
    if (maFramebuffers[VBOX_VIDEO_PRIMARY_SCREEN].u32ResizeStatus == ResizeStatus_Void)
    {
        mpDrv->pUpPort->pfnUpdateDisplayAll(mpDrv->pUpPort);
    }

    /* Everything OK. VBVA status can be changed. */

    /* Notify the VMMDev, which saves VBVA status in the saved state,
     * and needs to know current status.
     */
    PPDMIVMMDEVPORT pVMMDevPort = mParent->getVMMDev()->getVMMDevPort ();

    if (pVMMDevPort)
    {
        pVMMDevPort->pfnVBVAChange (pVMMDevPort, fEnable);
    }

    if (fEnable)
    {
        mpVbvaMemory = pVbvaMemory;
        mfVideoAccelEnabled = true;

        /* Initialize the hardware memory. */
        vbvaSetMemoryFlags (mpVbvaMemory, mfVideoAccelEnabled, mfVideoAccelVRDP, mfu32SupportedOrders, maFramebuffers, mcMonitors);
        mpVbvaMemory->off32Data = 0;
        mpVbvaMemory->off32Free = 0;

        memset (mpVbvaMemory->aRecords, 0, sizeof (mpVbvaMemory->aRecords));
        mpVbvaMemory->indexRecordFirst = 0;
        mpVbvaMemory->indexRecordFree = 0;

        LogRel(("VBVA: Enabled.\n"));
    }
    else
    {
        LogRel(("VBVA: Disabled.\n"));
    }

    LogFlowFunc (("VideoAccelEnable: rc = %Vrc.\n", rc));

    return rc;
}

#ifdef VBOX_VRDP
/* Called always by one VRDP server thread. Can be thread-unsafe.
 */
void Display::VideoAccelVRDP (bool fEnable)
{
    int c = fEnable?
                ASMAtomicIncS32 (&mcVideoAccelVRDPRefs):
                ASMAtomicDecS32 (&mcVideoAccelVRDPRefs);

    Assert (c >= 0);

    if (c == 0)
    {
        /* The last client has disconnected, and the accel can be
         * disabled.
         */
        Assert (fEnable == false);

        mfVideoAccelVRDP = false;
        mfu32SupportedOrders = 0;

        vbvaSetMemoryFlags (mpVbvaMemory, mfVideoAccelEnabled, mfVideoAccelVRDP, mfu32SupportedOrders, maFramebuffers, mcMonitors);

        LogRel(("VBVA: VRDP acceleration has been disabled.\n"));
    }
    else if (   c == 1
             && !mfVideoAccelVRDP)
    {
        /* The first client has connected. Enable the accel.
         */
        Assert (fEnable == true);

        mfVideoAccelVRDP = true;
        /* Supporting all orders. */
        mfu32SupportedOrders = ~0;

        vbvaSetMemoryFlags (mpVbvaMemory, mfVideoAccelEnabled, mfVideoAccelVRDP, mfu32SupportedOrders, maFramebuffers, mcMonitors);

        LogRel(("VBVA: VRDP acceleration has been requested.\n"));
    }
    else
    {
        /* A client is connected or disconnected but there is no change in the
         * accel state. It remains enabled.
         */
        Assert (mfVideoAccelVRDP == true);
    }
}
#endif /* VBOX_VRDP */

static bool vbvaVerifyRingBuffer (VBVAMEMORY *pVbvaMemory)
{
    return true;
}

static void vbvaFetchBytes (VBVAMEMORY *pVbvaMemory, uint8_t *pu8Dst, uint32_t cbDst)
{
    if (cbDst >= VBVA_RING_BUFFER_SIZE)
    {
        AssertMsgFailed (("cbDst = 0x%08X, ring buffer size 0x%08X", cbDst, VBVA_RING_BUFFER_SIZE));
        return;
    }

    uint32_t u32BytesTillBoundary = VBVA_RING_BUFFER_SIZE - pVbvaMemory->off32Data;
    uint8_t  *src                 = &pVbvaMemory->au8RingBuffer[pVbvaMemory->off32Data];
    int32_t i32Diff               = cbDst - u32BytesTillBoundary;

    if (i32Diff <= 0)
    {
        /* Chunk will not cross buffer boundary. */
        memcpy (pu8Dst, src, cbDst);
    }
    else
    {
        /* Chunk crosses buffer boundary. */
        memcpy (pu8Dst, src, u32BytesTillBoundary);
        memcpy (pu8Dst + u32BytesTillBoundary, &pVbvaMemory->au8RingBuffer[0], i32Diff);
    }

    /* Advance data offset. */
    pVbvaMemory->off32Data = (pVbvaMemory->off32Data + cbDst) % VBVA_RING_BUFFER_SIZE;

    return;
}


static bool vbvaPartialRead (uint8_t **ppu8, uint32_t *pcb, uint32_t cbRecord, VBVAMEMORY *pVbvaMemory)
{
    uint8_t *pu8New;

    LogFlow(("MAIN::DisplayImpl::vbvaPartialRead: p = %p, cb = %d, cbRecord 0x%08X\n",
             *ppu8, *pcb, cbRecord));

    if (*ppu8)
    {
        Assert (*pcb);
        pu8New = (uint8_t *)RTMemRealloc (*ppu8, cbRecord);
    }
    else
    {
        Assert (!*pcb);
        pu8New = (uint8_t *)RTMemAlloc (cbRecord);
    }

    if (!pu8New)
    {
        /* Memory allocation failed, fail the function. */
        Log(("MAIN::vbvaPartialRead: failed to (re)alocate memory for partial record!!! cbRecord 0x%08X\n",
             cbRecord));

        if (*ppu8)
        {
            RTMemFree (*ppu8);
        }

        *ppu8 = NULL;
        *pcb = 0;

        return false;
    }

    /* Fetch data from the ring buffer. */
    vbvaFetchBytes (pVbvaMemory, pu8New + *pcb, cbRecord - *pcb);

    *ppu8 = pu8New;
    *pcb = cbRecord;

    return true;
}

/* For contiguous chunks just return the address in the buffer.
 * For crossing boundary - allocate a buffer from heap.
 */
bool Display::vbvaFetchCmd (VBVACMDHDR **ppHdr, uint32_t *pcbCmd)
{
    uint32_t indexRecordFirst = mpVbvaMemory->indexRecordFirst;
    uint32_t indexRecordFree = mpVbvaMemory->indexRecordFree;

#ifdef DEBUG_sunlover
    LogFlowFunc (("first = %d, free = %d\n",
                  indexRecordFirst, indexRecordFree));
#endif /* DEBUG_sunlover */

    if (!vbvaVerifyRingBuffer (mpVbvaMemory))
    {
        return false;
    }

    if (indexRecordFirst == indexRecordFree)
    {
        /* No records to process. Return without assigning output variables. */
        return true;
    }

    VBVARECORD *pRecord = &mpVbvaMemory->aRecords[indexRecordFirst];

#ifdef DEBUG_sunlover
    LogFlowFunc (("cbRecord = 0x%08X\n", pRecord->cbRecord));
#endif /* DEBUG_sunlover */

    uint32_t cbRecord = pRecord->cbRecord & ~VBVA_F_RECORD_PARTIAL;

    if (mcbVbvaPartial)
    {
        /* There is a partial read in process. Continue with it. */

        Assert (mpu8VbvaPartial);

        LogFlowFunc (("continue partial record mcbVbvaPartial = %d cbRecord 0x%08X, first = %d, free = %d\n",
                      mcbVbvaPartial, pRecord->cbRecord, indexRecordFirst, indexRecordFree));

        if (cbRecord > mcbVbvaPartial)
        {
            /* New data has been added to the record. */
            if (!vbvaPartialRead (&mpu8VbvaPartial, &mcbVbvaPartial, cbRecord, mpVbvaMemory))
            {
                return false;
            }
        }

        if (!(pRecord->cbRecord & VBVA_F_RECORD_PARTIAL))
        {
            /* The record is completed by guest. Return it to the caller. */
            *ppHdr = (VBVACMDHDR *)mpu8VbvaPartial;
            *pcbCmd = mcbVbvaPartial;

            mpu8VbvaPartial = NULL;
            mcbVbvaPartial = 0;

            /* Advance the record index. */
            mpVbvaMemory->indexRecordFirst = (indexRecordFirst + 1) % VBVA_MAX_RECORDS;

#ifdef DEBUG_sunlover
            LogFlowFunc (("partial done ok, data = %d, free = %d\n",
                          mpVbvaMemory->off32Data, mpVbvaMemory->off32Free));
#endif /* DEBUG_sunlover */
        }

        return true;
    }

    /* A new record need to be processed. */
    if (pRecord->cbRecord & VBVA_F_RECORD_PARTIAL)
    {
        /* Current record is being written by guest. '=' is important here. */
        if (cbRecord >= VBVA_RING_BUFFER_SIZE - VBVA_RING_BUFFER_THRESHOLD)
        {
            /* Partial read must be started. */
            if (!vbvaPartialRead (&mpu8VbvaPartial, &mcbVbvaPartial, cbRecord, mpVbvaMemory))
            {
                return false;
            }

            LogFlowFunc (("started partial record mcbVbvaPartial = 0x%08X cbRecord 0x%08X, first = %d, free = %d\n",
                          mcbVbvaPartial, pRecord->cbRecord, indexRecordFirst, indexRecordFree));
        }

        return true;
    }

    /* Current record is complete. If it is not empty, process it. */
    if (cbRecord)
    {
        /* The size of largest contiguos chunk in the ring biffer. */
        uint32_t u32BytesTillBoundary = VBVA_RING_BUFFER_SIZE - mpVbvaMemory->off32Data;

        /* The ring buffer pointer. */
        uint8_t *au8RingBuffer = &mpVbvaMemory->au8RingBuffer[0];

        /* The pointer to data in the ring buffer. */
        uint8_t *src = &au8RingBuffer[mpVbvaMemory->off32Data];

        /* Fetch or point the data. */
        if (u32BytesTillBoundary >= cbRecord)
        {
            /* The command does not cross buffer boundary. Return address in the buffer. */
            *ppHdr = (VBVACMDHDR *)src;

            /* Advance data offset. */
            mpVbvaMemory->off32Data = (mpVbvaMemory->off32Data + cbRecord) % VBVA_RING_BUFFER_SIZE;
        }
        else
        {
            /* The command crosses buffer boundary. Rare case, so not optimized. */
            uint8_t *dst = (uint8_t *)RTMemAlloc (cbRecord);

            if (!dst)
            {
                LogFlowFunc (("could not allocate %d bytes from heap!!!\n", cbRecord));
                mpVbvaMemory->off32Data = (mpVbvaMemory->off32Data + cbRecord) % VBVA_RING_BUFFER_SIZE;
                return false;
            }

            vbvaFetchBytes (mpVbvaMemory, dst, cbRecord);

            *ppHdr = (VBVACMDHDR *)dst;

#ifdef DEBUG_sunlover
            LogFlowFunc (("Allocated from heap %p\n", dst));
#endif /* DEBUG_sunlover */
        }
    }

    *pcbCmd = cbRecord;

    /* Advance the record index. */
    mpVbvaMemory->indexRecordFirst = (indexRecordFirst + 1) % VBVA_MAX_RECORDS;

#ifdef DEBUG_sunlover
    LogFlowFunc (("done ok, data = %d, free = %d\n",
                  mpVbvaMemory->off32Data, mpVbvaMemory->off32Free));
#endif /* DEBUG_sunlover */

    return true;
}

void Display::vbvaReleaseCmd (VBVACMDHDR *pHdr, int32_t cbCmd)
{
    uint8_t *au8RingBuffer = mpVbvaMemory->au8RingBuffer;

    if (   (uint8_t *)pHdr >= au8RingBuffer
        && (uint8_t *)pHdr < &au8RingBuffer[VBVA_RING_BUFFER_SIZE])
    {
        /* The pointer is inside ring buffer. Must be continuous chunk. */
        Assert (VBVA_RING_BUFFER_SIZE - ((uint8_t *)pHdr - au8RingBuffer) >= cbCmd);

        /* Do nothing. */

        Assert (!mpu8VbvaPartial && mcbVbvaPartial == 0);
    }
    else
    {
        /* The pointer is outside. It is then an allocated copy. */

#ifdef DEBUG_sunlover
        LogFlowFunc (("Free heap %p\n", pHdr));
#endif /* DEBUG_sunlover */

        if ((uint8_t *)pHdr == mpu8VbvaPartial)
        {
            mpu8VbvaPartial = NULL;
            mcbVbvaPartial = 0;
        }
        else
        {
            Assert (!mpu8VbvaPartial && mcbVbvaPartial == 0);
        }

        RTMemFree (pHdr);
    }

    return;
}


/**
 * Called regularly on the DisplayRefresh timer.
 * Also on behalf of guest, when the ring buffer is full.
 *
 * @thread EMT
 */
void Display::VideoAccelFlush (void)
{
#ifdef DEBUG_sunlover_2
    LogFlowFunc (("mfVideoAccelEnabled = %d\n", mfVideoAccelEnabled));
#endif /* DEBUG_sunlover_2 */

    if (!mfVideoAccelEnabled)
    {
        Log(("Display::VideoAccelFlush: called with disabled VBVA!!! Ignoring.\n"));
        return;
    }

    /* Here VBVA is enabled and we have the accelerator memory pointer. */
    Assert(mpVbvaMemory);

#ifdef DEBUG_sunlover_2
    LogFlowFunc (("indexRecordFirst = %d, indexRecordFree = %d, off32Data = %d, off32Free = %d\n",
                  mpVbvaMemory->indexRecordFirst, mpVbvaMemory->indexRecordFree, mpVbvaMemory->off32Data, mpVbvaMemory->off32Free));
#endif /* DEBUG_sunlover_2 */

    /* Quick check for "nothing to update" case. */
    if (mpVbvaMemory->indexRecordFirst == mpVbvaMemory->indexRecordFree)
    {
        return;
    }

    /* Process the ring buffer */
    unsigned uScreenId;
    for (uScreenId = 0; uScreenId < mcMonitors; uScreenId++)
    {
        if (!maFramebuffers[uScreenId].pFramebuffer.isNull())
        {
            maFramebuffers[uScreenId].pFramebuffer->Lock ();
        }
    }

    /* Initialize dirty rectangles accumulator. */
    VBVADIRTYREGION rgn;
    vbvaRgnInit (&rgn, maFramebuffers, mcMonitors, this, mpDrv->pUpPort);

    for (;;)
    {
        VBVACMDHDR *phdr = NULL;
        uint32_t cbCmd = ~0;

        /* Fetch the command data. */
        if (!vbvaFetchCmd (&phdr, &cbCmd))
        {
            Log(("Display::VideoAccelFlush: unable to fetch command. off32Data = %d, off32Free = %d. Disabling VBVA!!!\n",
                  mpVbvaMemory->off32Data, mpVbvaMemory->off32Free));

            /* Disable VBVA on those processing errors. */
            VideoAccelEnable (false, NULL);

            break;
        }

        if (cbCmd == uint32_t(~0))
        {
            /* No more commands yet in the queue. */
            break;
        }

        if (cbCmd != 0)
        {
#ifdef DEBUG_sunlover
            LogFlowFunc (("hdr: cbCmd = %d, x=%d, y=%d, w=%d, h=%d\n",
                          cbCmd, phdr->x, phdr->y, phdr->w, phdr->h));
#endif /* DEBUG_sunlover */

            VBVACMDHDR hdrSaved = *phdr;

            int x = phdr->x;
            int y = phdr->y;
            int w = phdr->w;
            int h = phdr->h;

            uScreenId = mapCoordsToScreen(maFramebuffers, mcMonitors, &x, &y, &w, &h);

            phdr->x = (int16_t)x;
            phdr->y = (int16_t)y;
            phdr->w = (uint16_t)w;
            phdr->h = (uint16_t)h;

            DISPLAYFBINFO *pFBInfo = &maFramebuffers[uScreenId];

            if (pFBInfo->u32ResizeStatus == ResizeStatus_Void)
            {
                /* Handle the command.
                 *
                 * Guest is responsible for updating the guest video memory.
                 * The Windows guest does all drawing using Eng*.
                 *
                 * For local output, only dirty rectangle information is used
                 * to update changed areas.
                 *
                 * Dirty rectangles are accumulated to exclude overlapping updates and
                 * group small updates to a larger one.
                 */

                /* Accumulate the update. */
                vbvaRgnDirtyRect (&rgn, uScreenId, phdr);

                /* Forward the command to VRDP server. */
                mParent->consoleVRDPServer()->SendUpdate (uScreenId, phdr, cbCmd);

                *phdr = hdrSaved;
            }
        }

        vbvaReleaseCmd (phdr, cbCmd);
    }

    for (uScreenId = 0; uScreenId < mcMonitors; uScreenId++)
    {
        if (!maFramebuffers[uScreenId].pFramebuffer.isNull())
        {
            maFramebuffers[uScreenId].pFramebuffer->Unlock ();
        }

        if (maFramebuffers[uScreenId].u32ResizeStatus == ResizeStatus_Void)
        {
            /* Draw the framebuffer. */
            vbvaRgnUpdateFramebuffer (&rgn, uScreenId);
        }
    }
}


// IDisplay properties
/////////////////////////////////////////////////////////////////////////////

/**
 * Returns the current display width in pixel
 *
 * @returns COM status code
 * @param width Address of result variable.
 */
STDMETHODIMP Display::COMGETTER(Width) (ULONG *width)
{
    if (!width)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    CHECK_CONSOLE_DRV (mpDrv);

    *width = mpDrv->Connector.cx;
    return S_OK;
}

/**
 * Returns the current display height in pixel
 *
 * @returns COM status code
 * @param height Address of result variable.
 */
STDMETHODIMP Display::COMGETTER(Height) (ULONG *height)
{
    if (!height)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    CHECK_CONSOLE_DRV (mpDrv);

    *height = mpDrv->Connector.cy;
    return S_OK;
}

/**
 * Returns the current display color depth in bits
 *
 * @returns COM status code
 * @param bitsPerPixel Address of result variable.
 */
STDMETHODIMP Display::COMGETTER(BitsPerPixel) (ULONG *bitsPerPixel)
{
    if (!bitsPerPixel)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    CHECK_CONSOLE_DRV (mpDrv);

    uint32_t cBits = 0;
    int rc = mpDrv->pUpPort->pfnQueryColorDepth(mpDrv->pUpPort, &cBits);
    AssertRC(rc);
    *bitsPerPixel = cBits;
    return S_OK;
}


// IDisplay methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Display::SetupInternalFramebuffer (ULONG depth)
{
    LogFlowFunc (("\n"));

    AutoLock lock (this);
    CHECK_READY();

    /*
     *  Create an internal framebuffer only if depth is not zero. Otherwise, we
     *  reset back to the "black hole" state as it was at Display construction.
     */
    ComPtr <IFramebuffer> frameBuf;
    if (depth)
    {
        ComObjPtr <InternalFramebuffer> internal;
        internal.createObject();
        internal->init (640, 480, depth);
        frameBuf = internal; // query interface
    }

    Console::SafeVMPtrQuiet pVM (mParent);
    if (pVM.isOk())
    {
        /* Must leave the lock here because the changeFramebuffer will also obtain it. */
        lock.leave ();

        /* send request to the EMT thread */
        PVMREQ pReq = NULL;
        int vrc = VMR3ReqCall (pVM, &pReq, RT_INDEFINITE_WAIT,
                               (PFNRT) changeFramebuffer, 3,
                               this, static_cast <IFramebuffer *> (frameBuf),
                               true /* aInternal */, VBOX_VIDEO_PRIMARY_SCREEN);
        if (VBOX_SUCCESS (vrc))
            vrc = pReq->iStatus;
        VMR3ReqFree (pReq);

        lock.enter ();

        ComAssertRCRet (vrc, E_FAIL);
    }
    else
    {
        /* No VM is created (VM is powered off), do a direct call */
        int vrc = changeFramebuffer (this, frameBuf, true /* aInternal */, VBOX_VIDEO_PRIMARY_SCREEN);
        ComAssertRCRet (vrc, E_FAIL);
    }

    return S_OK;
}

STDMETHODIMP Display::LockFramebuffer (BYTE **address)
{
    if (!address)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    /* only allowed for internal framebuffers */
    if (mInternalFramebuffer && !mFramebufferOpened && !maFramebuffers[VBOX_VIDEO_PRIMARY_SCREEN].pFramebuffer.isNull())
    {
        CHECK_CONSOLE_DRV (mpDrv);

        maFramebuffers[VBOX_VIDEO_PRIMARY_SCREEN].pFramebuffer->Lock();
        mFramebufferOpened = true;
        *address = mpDrv->Connector.pu8Data;
        return S_OK;
    }

    return setError (E_FAIL,
        tr ("Framebuffer locking is allowed only for the internal framebuffer"));
}

STDMETHODIMP Display::UnlockFramebuffer()
{
    AutoLock lock(this);
    CHECK_READY();

    if (mFramebufferOpened)
    {
        CHECK_CONSOLE_DRV (mpDrv);

        maFramebuffers[VBOX_VIDEO_PRIMARY_SCREEN].pFramebuffer->Unlock();
        mFramebufferOpened = false;
        return S_OK;
    }

    return setError (E_FAIL,
        tr ("Framebuffer locking is allowed only for the internal framebuffer"));
}

STDMETHODIMP Display::RegisterExternalFramebuffer (IFramebuffer *frameBuf)
{
	LogFlowFunc (("\n"));

    if (!frameBuf)
        return E_POINTER;

    AutoLock lock (this);
    CHECK_READY();

    Console::SafeVMPtrQuiet pVM (mParent);
    if (pVM.isOk())
    {
        /* Must leave the lock here because the changeFramebuffer will also obtain it. */
        lock.leave ();

        /* send request to the EMT thread */
        PVMREQ pReq = NULL;
        int vrc = VMR3ReqCall (pVM, &pReq, RT_INDEFINITE_WAIT,
                               (PFNRT) changeFramebuffer, 3,
                               this, frameBuf, false /* aInternal */, VBOX_VIDEO_PRIMARY_SCREEN);
        if (VBOX_SUCCESS (vrc))
            vrc = pReq->iStatus;
        VMR3ReqFree (pReq);

        lock.enter ();

        ComAssertRCRet (vrc, E_FAIL);
    }
    else
    {
        /* No VM is created (VM is powered off), do a direct call */
        int vrc = changeFramebuffer (this, frameBuf, false /* aInternal */, VBOX_VIDEO_PRIMARY_SCREEN);
        ComAssertRCRet (vrc, E_FAIL);
    }

    return S_OK;
}

STDMETHODIMP Display::SetFramebuffer (ULONG aScreenId, IFramebuffer *aFramebuffer)
{
    LogFlowFunc (("\n"));

    if (!aFramebuffer)
        return E_POINTER;

    AutoLock lock (this);
    CHECK_READY();

    Console::SafeVMPtrQuiet pVM (mParent);
    if (pVM.isOk())
    {
        /* Must leave the lock here because the changeFramebuffer will also obtain it. */
        lock.leave ();

        /* send request to the EMT thread */
        PVMREQ pReq = NULL;
        int vrc = VMR3ReqCall (pVM, &pReq, RT_INDEFINITE_WAIT,
                               (PFNRT) changeFramebuffer, 3,
                               this, aFramebuffer, false /* aInternal */, aScreenId);
        if (VBOX_SUCCESS (vrc))
            vrc = pReq->iStatus;
        VMR3ReqFree (pReq);

        lock.enter ();

        ComAssertRCRet (vrc, E_FAIL);
    }
    else
    {
        /* No VM is created (VM is powered off), do a direct call */
        int vrc = changeFramebuffer (this, aFramebuffer, false /* aInternal */, aScreenId);
        ComAssertRCRet (vrc, E_FAIL);
    }

    return S_OK;
}

STDMETHODIMP Display::GetFramebuffer (ULONG aScreenId, IFramebuffer **aFramebuffer, LONG *aXOrigin, LONG *aYOrigin)
{
    LogFlowFunc (("aScreenId = %d\n", aScreenId));

    if (!aFramebuffer)
        return E_POINTER;

    AutoLock lock (this);
    CHECK_READY();

    /* @todo this should be actually done on EMT. */
    DISPLAYFBINFO *pFBInfo = &maFramebuffers[aScreenId];

    *aFramebuffer = pFBInfo->pFramebuffer;
    if (*aFramebuffer)
        (*aFramebuffer)->AddRef ();
    if (aXOrigin)
        *aXOrigin = pFBInfo->xOrigin;
    if (aYOrigin)
        *aYOrigin = pFBInfo->yOrigin;

    return S_OK;
}

STDMETHODIMP Display::SetVideoModeHint(ULONG aWidth, ULONG aHeight, ULONG aBitsPerPixel, ULONG aDisplay)
{
    AutoLock lock(this);
    CHECK_READY();

    CHECK_CONSOLE_DRV (mpDrv);

    /*
     * Do some rough checks for valid input
     */
    ULONG width  = aWidth;
    if (!width)
        width    = mpDrv->Connector.cx;
    ULONG height = aHeight;
    if (!height)
        height   = mpDrv->Connector.cy;
    ULONG bpp    = aBitsPerPixel;
    if (!bpp)
    {
        uint32_t cBits = 0;
        int rc = mpDrv->pUpPort->pfnQueryColorDepth(mpDrv->pUpPort, &cBits);
        AssertRC(rc);
        bpp = cBits;
    }
    ULONG cMonitors;
    mParent->machine()->COMGETTER(MonitorCount)(&cMonitors);
    if (cMonitors == 0 && aDisplay > 0)
        return E_INVALIDARG;
    if (aDisplay >= cMonitors)
        return E_INVALIDARG;

// sunlover 20070614: It is up to the guest to decide whether the hint is valid.
//    ULONG vramSize;
//    mParent->machine()->COMGETTER(VRAMSize)(&vramSize);
//    /* enough VRAM? */
//    if ((width * height * (bpp / 8)) > (vramSize * 1024 * 1024))
//        return setError(E_FAIL, tr("Not enough VRAM for the selected video mode"));

    /* Have to leave the lock because the pfnRequestDisplayChange will call EMT.  */
    lock.leave ();
    if (mParent->getVMMDev())
        mParent->getVMMDev()->getVMMDevPort()->
            pfnRequestDisplayChange (mParent->getVMMDev()->getVMMDevPort(),
                                     aWidth, aHeight, aBitsPerPixel, aDisplay);
    return S_OK;
}

STDMETHODIMP Display::SetSeamlessMode (BOOL enabled)
{
    AutoLock lock(this);
    CHECK_READY();

    /* Have to leave the lock because the pfnRequestSeamlessChange will call EMT.  */
    lock.leave ();
    if (mParent->getVMMDev())
        mParent->getVMMDev()->getVMMDevPort()->
            pfnRequestSeamlessChange (mParent->getVMMDev()->getVMMDevPort(),
                                      !!enabled);
    return S_OK;
}

STDMETHODIMP Display::TakeScreenShot (BYTE *address, ULONG width, ULONG height)
{
    /// @todo (r=dmik) this function may take too long to complete if the VM
    //  is doing something like saving state right now. Which, in case if it
    //  is called on the GUI thread, will make it unresponsive. We should
    //  check the machine state here (by enclosing the check and VMRequCall
    //  within the Console lock to make it atomic).

    LogFlowFuncEnter();
    LogFlowFunc (("address=%p, width=%d, height=%d\n",
                  address, width, height));

    if (!address)
        return E_POINTER;
    if (!width || !height)
        return E_INVALIDARG;

    AutoLock lock(this);
    CHECK_READY();

    CHECK_CONSOLE_DRV (mpDrv);

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    HRESULT rc = S_OK;

    LogFlowFunc (("Sending SCREENSHOT request\n"));

    /*
     * First try use the graphics device features for making a snapshot.
     * This does not support streatching, is an optional feature (returns not supported).
     *
     * Note: It may cause a display resize. Watch out for deadlocks.
     */
    int rcVBox = VERR_NOT_SUPPORTED;
    if (    mpDrv->Connector.cx == width
        &&  mpDrv->Connector.cy == height)
    {
        PVMREQ pReq;
        size_t cbData = RT_ALIGN_Z(width, 4) * 4 * height;
        rcVBox = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT,
                             (PFNRT)mpDrv->pUpPort->pfnSnapshot, 6, mpDrv->pUpPort,
                             address, cbData, NULL, NULL, NULL);
        if (VBOX_SUCCESS(rcVBox))
        {
            rcVBox = pReq->iStatus;
            VMR3ReqFree(pReq);
        }
    }

    /*
     * If the function returns not supported, or if streaching is requested,
     * we'll have to do all the work ourselves using the framebuffer data.
     */
    if (rcVBox == VERR_NOT_SUPPORTED || rcVBox == VERR_NOT_IMPLEMENTED)
    {
        /** @todo implement snapshot streching and generic snapshot fallback. */
        rc = setError (E_NOTIMPL, tr ("This feature is not implemented"));
    }
    else if (VBOX_FAILURE(rcVBox))
        rc = setError (E_FAIL,
            tr ("Could not take a screenshot (%Vrc)"), rcVBox);

    LogFlowFunc (("rc=%08X\n", rc));
    LogFlowFuncLeave();
    return rc;
}

STDMETHODIMP Display::DrawToScreen (BYTE *address, ULONG x, ULONG y,
                                    ULONG width, ULONG height)
{
    /// @todo (r=dmik) this function may take too long to complete if the VM
    //  is doing something like saving state right now. Which, in case if it
    //  is called on the GUI thread, will make it unresponsive. We should
    //  check the machine state here (by enclosing the check and VMRequCall
    //  within the Console lock to make it atomic).

    LogFlowFuncEnter();
    LogFlowFunc (("address=%p, x=%d, y=%d, width=%d, height=%d\n",
                  address, x, y, width, height));

    if (!address)
        return E_POINTER;
    if (!width || !height)
        return E_INVALIDARG;

    AutoLock lock(this);
    CHECK_READY();

    CHECK_CONSOLE_DRV (mpDrv);

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    /*
     * Again we're lazy and make the graphics device do all the
     * dirty convertion work.
     */
    PVMREQ pReq;
    int rcVBox = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT,
                             (PFNRT)mpDrv->pUpPort->pfnDisplayBlt, 6, mpDrv->pUpPort,
                             address, x, y, width, height);
    if (VBOX_SUCCESS(rcVBox))
    {
        rcVBox = pReq->iStatus;
        VMR3ReqFree(pReq);
    }

    /*
     * If the function returns not supported, we'll have to do all the
     * work ourselves using the framebuffer.
     */
    HRESULT rc = S_OK;
    if (rcVBox == VERR_NOT_SUPPORTED || rcVBox == VERR_NOT_IMPLEMENTED)
    {
        /** @todo implement generic fallback for screen blitting. */
        rc = E_NOTIMPL;
    }
    else if (VBOX_FAILURE(rcVBox))
        rc = setError (E_FAIL,
            tr ("Could not draw to the screen (%Vrc)"), rcVBox);
//@todo
//    else
//    {
//        /* All ok. Redraw the screen. */
//        handleDisplayUpdate (x, y, width, height);
//    }

    LogFlowFunc (("rc=%08X\n", rc));
    LogFlowFuncLeave();
    return rc;
}

/**
 * Does a full invalidation of the VM display and instructs the VM
 * to update it immediately.
 *
 * @returns COM status code
 */
STDMETHODIMP Display::InvalidateAndUpdate()
{
    LogFlowFuncEnter();

    AutoLock lock(this);
    CHECK_READY();

    CHECK_CONSOLE_DRV (mpDrv);

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    HRESULT rc = S_OK;

    LogFlowFunc (("Sending DPYUPDATE request\n"));

    /* pdm.h says that this has to be called from the EMT thread */
    PVMREQ pReq;
    int rcVBox = VMR3ReqCallVoid(pVM, &pReq, RT_INDEFINITE_WAIT,
                                 (PFNRT)mpDrv->pUpPort->pfnUpdateDisplayAll, 1, mpDrv->pUpPort);
    if (VBOX_SUCCESS(rcVBox))
        VMR3ReqFree(pReq);

    if (VBOX_FAILURE(rcVBox))
        rc = setError (E_FAIL,
            tr ("Could not invalidate and update the screen (%Vrc)"), rcVBox);

    LogFlowFunc (("rc=%08X\n", rc));
    LogFlowFuncLeave();
    return rc;
}

/**
 * Notification that the framebuffer has completed the
 * asynchronous resize processing
 *
 * @returns COM status code
 */
STDMETHODIMP Display::ResizeCompleted(ULONG aScreenId)
{
    LogFlowFunc (("\n"));

    /// @todo (dmik) can we AutoLock alock (this); here?
    //  do it when we switch this class to VirtualBoxBase_NEXT.
    //  This will require general code review and may add some details.
    //  In particular, we may want to check whether EMT is really waiting for
    //  this notification, etc. It might be also good to obey the caller to make
    //  sure this method is not called from more than one thread at a time
    //  (and therefore don't use Display lock at all here to save some
    //  milliseconds).
    CHECK_READY();

    /* this is only valid for external framebuffers */
    if (mInternalFramebuffer)
        return setError (E_FAIL,
            tr ("Resize completed notification is valid only "
                "for external framebuffers"));

    /* Set the flag indicating that the resize has completed and display data need to be updated. */
    bool f = ASMAtomicCmpXchgU32 (&maFramebuffers[aScreenId].u32ResizeStatus, ResizeStatus_UpdateDisplayData, ResizeStatus_InProgress);
    AssertRelease(f);NOREF(f);

    return S_OK;
}

/**
 * Notification that the framebuffer has completed the
 * asynchronous update processing
 *
 * @returns COM status code
 */
STDMETHODIMP Display::UpdateCompleted()
{
    LogFlowFunc (("\n"));

    /// @todo (dmik) can we AutoLock alock (this); here?
    //  do it when we switch this class to VirtualBoxBase_NEXT.
    //  Tthis will require general code review and may add some details.
    //  In particular, we may want to check whether EMT is really waiting for
    //  this notification, etc. It might be also good to obey the caller to make
    //  sure this method is not called from more than one thread at a time
    //  (and therefore don't use Display lock at all here to save some
    //  milliseconds).
    CHECK_READY();

    /* this is only valid for external framebuffers */
    if (mInternalFramebuffer)
        return setError (E_FAIL,
            tr ("Resize completed notification is valid only "
                "for external framebuffers"));

    maFramebuffers[VBOX_VIDEO_PRIMARY_SCREEN].pFramebuffer->Lock();
    /* signal our semaphore */
    RTSemEventMultiSignal(mUpdateSem);
    maFramebuffers[VBOX_VIDEO_PRIMARY_SCREEN].pFramebuffer->Unlock();

    return S_OK;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/**
 *  Helper to update the display information from the framebuffer.
 *
 *  @param aCheckParams true to compare the parameters of the current framebuffer
 *                      and the new one and issue handleDisplayResize()
 *                      if they differ.
 *  @thread EMT
 */
void Display::updateDisplayData (bool aCheckParams /* = false */)
{
    /* the driver might not have been constructed yet */
    if (!mpDrv)
        return;

#if DEBUG
    /*
     *  Sanity check. Note that this method may be called on EMT after Console
     *  has started the power down procedure (but before our #drvDestruct() is
     *  called, in which case pVM will aleady be NULL but mpDrv will not). Since
     *  we don't really need pVM to proceed, we avoid this check in the release
     *  build to save some ms (necessary to construct SafeVMPtrQuiet) in this
     *  time-critical method.
     */
    Console::SafeVMPtrQuiet pVM (mParent);
    if (pVM.isOk())
        VM_ASSERT_EMT (pVM.raw());
#endif

    /* The method is only relevant to the primary framebuffer. */
    IFramebuffer *pFramebuffer = maFramebuffers[VBOX_VIDEO_PRIMARY_SCREEN].pFramebuffer;

    if (pFramebuffer)
    {
        HRESULT rc;
        BYTE *address = 0;
        rc = pFramebuffer->COMGETTER(Address) (&address);
        AssertComRC (rc);
        ULONG bytesPerLine = 0;
        rc = pFramebuffer->COMGETTER(BytesPerLine) (&bytesPerLine);
        AssertComRC (rc);
        ULONG bitsPerPixel = 0;
        rc = pFramebuffer->COMGETTER(BitsPerPixel) (&bitsPerPixel);
        AssertComRC (rc);
        ULONG width = 0;
        rc = pFramebuffer->COMGETTER(Width) (&width);
        AssertComRC (rc);
        ULONG height = 0;
        rc = pFramebuffer->COMGETTER(Height) (&height);
        AssertComRC (rc);

        /*
         *  Check current parameters with new ones and issue handleDisplayResize()
         *  to let the new frame buffer adjust itself properly. Note that it will
         *  result into a recursive updateDisplayData() call but with
         *  aCheckOld = false.
         */
        if (aCheckParams &&
            (mLastAddress != address ||
             mLastBytesPerLine != bytesPerLine ||
             mLastBitsPerPixel != bitsPerPixel ||
             mLastWidth != (int) width ||
             mLastHeight != (int) height))
        {
            handleDisplayResize (VBOX_VIDEO_PRIMARY_SCREEN, mLastBitsPerPixel,
                                 mLastAddress,
                                 mLastBytesPerLine,
                                 mLastWidth,
                                 mLastHeight);
            return;
        }

        mpDrv->Connector.pu8Data = (uint8_t *) address;
        mpDrv->Connector.cbScanline = bytesPerLine;
        mpDrv->Connector.cBits = bitsPerPixel;
        mpDrv->Connector.cx = width;
        mpDrv->Connector.cy = height;
    }
    else
    {
        /* black hole */
        mpDrv->Connector.pu8Data = NULL;
        mpDrv->Connector.cbScanline = 0;
        mpDrv->Connector.cBits = 0;
        mpDrv->Connector.cx = 0;
        mpDrv->Connector.cy = 0;
    }
}

/**
 *  Changes the current frame buffer. Called on EMT to avoid both
 *  race conditions and excessive locking.
 *
 *  @note locks this object for writing
 *  @thread EMT
 */
/* static */
DECLCALLBACK(int) Display::changeFramebuffer (Display *that, IFramebuffer *aFB,
                                              bool aInternal, unsigned uScreenId)
{
    LogFlowFunc (("uScreenId = %d\n", uScreenId));

    AssertReturn (that, VERR_INVALID_PARAMETER);
    AssertReturn (aFB || aInternal, VERR_INVALID_PARAMETER);
    AssertReturn (uScreenId >= 0 && uScreenId < that->mcMonitors, VERR_INVALID_PARAMETER);

    /// @todo (r=dmik) AutoCaller

    AutoLock alock (that);

    DISPLAYFBINFO *pDisplayFBInfo = &that->maFramebuffers[uScreenId];
    pDisplayFBInfo->pFramebuffer = aFB;

    that->mInternalFramebuffer = aInternal;
    that->mSupportedAccelOps = 0;

    /* determine which acceleration functions are supported by this framebuffer */
    if (aFB && !aInternal)
    {
        HRESULT rc;
        BOOL accelSupported = FALSE;
        rc = aFB->OperationSupported (
            FramebufferAccelerationOperation_SolidFillAcceleration, &accelSupported);
        AssertComRC (rc);
        if (accelSupported)
            that->mSupportedAccelOps |=
                FramebufferAccelerationOperation_SolidFillAcceleration;
        accelSupported = FALSE;
        rc = aFB->OperationSupported (
            FramebufferAccelerationOperation_ScreenCopyAcceleration, &accelSupported);
        AssertComRC (rc);
        if (accelSupported)
            that->mSupportedAccelOps |=
                FramebufferAccelerationOperation_ScreenCopyAcceleration;
    }

    that->mParent->consoleVRDPServer()->SendResize ();

    that->updateDisplayData (true /* aCheckParams */);

    return VINF_SUCCESS;
}

/**
 * Handle display resize event issued by the VGA device for the primary screen.
 *
 * @see PDMIDISPLAYCONNECTOR::pfnResize
 */
DECLCALLBACK(int) Display::displayResizeCallback(PPDMIDISPLAYCONNECTOR pInterface,
                                                 uint32_t bpp, void *pvVRAM, uint32_t cbLine, uint32_t cx, uint32_t cy)
{
    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);

    LogFlowFunc (("bpp %d, pvVRAM %p, cbLine %d, cx %d, cy %d\n",
                  bpp, pvVRAM, cbLine, cx, cy));

    return pDrv->pDisplay->handleDisplayResize(VBOX_VIDEO_PRIMARY_SCREEN, bpp, pvVRAM, cbLine, cx, cy);
}

/**
 * Handle display update.
 *
 * @see PDMIDISPLAYCONNECTOR::pfnUpdateRect
 */
DECLCALLBACK(void) Display::displayUpdateCallback(PPDMIDISPLAYCONNECTOR pInterface,
                                                  uint32_t x, uint32_t y, uint32_t cx, uint32_t cy)
{
    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);

#ifdef DEBUG_sunlover
    LogFlowFunc (("mfVideoAccelEnabled = %d, %d,%d %dx%d\n",
                  pDrv->pDisplay->mfVideoAccelEnabled, x, y, cx, cy));
#endif /* DEBUG_sunlover */

    /* This call does update regardless of VBVA status.
     * But in VBVA mode this is called only as result of
     * pfnUpdateDisplayAll in the VGA device.
     */

    pDrv->pDisplay->handleDisplayUpdate(x, y, cx, cy);
}

/**
 * Periodic display refresh callback.
 *
 * @see PDMIDISPLAYCONNECTOR::pfnRefresh
 */
DECLCALLBACK(void) Display::displayRefreshCallback(PPDMIDISPLAYCONNECTOR pInterface)
{
    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);

#ifdef DEBUG_sunlover
    STAM_PROFILE_START(&StatDisplayRefresh, a);
#endif /* DEBUG_sunlover */

#ifdef DEBUG_sunlover_2
    LogFlowFunc (("pDrv->pDisplay->mfVideoAccelEnabled = %d\n",
                  pDrv->pDisplay->mfVideoAccelEnabled));
#endif /* DEBUG_sunlover_2 */

    Display *pDisplay = pDrv->pDisplay;

    unsigned uScreenId;
    for (uScreenId = 0; uScreenId < pDisplay->mcMonitors; uScreenId++)
    {
        DISPLAYFBINFO *pFBInfo = &pDisplay->maFramebuffers[uScreenId];

        /* Check the resize status. The status can be checked normally because
         * the status affects only the EMT.
         */
        uint32_t u32ResizeStatus = pFBInfo->u32ResizeStatus;

        if (u32ResizeStatus == ResizeStatus_UpdateDisplayData)
        {
            LogFlowFunc (("ResizeStatus_UpdateDisplayData %d\n", uScreenId));
            /* The framebuffer was resized and display data need to be updated. */
            pDisplay->handleResizeCompletedEMT ();
            /* Continue with normal processing because the status here is ResizeStatus_Void. */
            Assert (pFBInfo->u32ResizeStatus == ResizeStatus_Void);
            if (uScreenId == VBOX_VIDEO_PRIMARY_SCREEN)
            {
                /* Repaint the display because VM continued to run during the framebuffer resize. */
                if (!pFBInfo->pFramebuffer.isNull())
                    pDrv->pUpPort->pfnUpdateDisplayAll(pDrv->pUpPort);
            }
            /* Ignore the refresh for the screen to replay the logic. */
            continue;
        }
        else if (u32ResizeStatus == ResizeStatus_InProgress)
        {
            /* The framebuffer is being resized. Do not call the VGA device back. Immediately return. */
            LogFlowFunc (("ResizeStatus_InProcess\n"));
            continue;
        }

        if (pFBInfo->pFramebuffer.isNull())
        {
            /*
             *  Do nothing in the "black hole" mode to avoid copying guest
             *  video memory to the frame buffer
             */
        }
        else
        {
            if (pDisplay->mfPendingVideoAccelEnable)
            {
                /* Acceleration was enabled while machine was not yet running
                 * due to restoring from saved state. Update entire display and
                 * actually enable acceleration.
                 */
                Assert(pDisplay->mpPendingVbvaMemory);

                /* Acceleration can not be yet enabled.*/
                Assert(pDisplay->mpVbvaMemory == NULL);
                Assert(!pDisplay->mfVideoAccelEnabled);

                if (pDisplay->mfMachineRunning)
                {
                    pDisplay->VideoAccelEnable (pDisplay->mfPendingVideoAccelEnable,
                                                pDisplay->mpPendingVbvaMemory);

                    /* Reset the pending state. */
                    pDisplay->mfPendingVideoAccelEnable = false;
                    pDisplay->mpPendingVbvaMemory = NULL;
                }
            }
            else
            {
                Assert(pDisplay->mpPendingVbvaMemory == NULL);

                if (pDisplay->mfVideoAccelEnabled)
                {
                    Assert(pDisplay->mpVbvaMemory);
                    pDisplay->VideoAccelFlush ();
                }
                else
                {
                    Assert(pDrv->Connector.pu8Data);
                    pDrv->pUpPort->pfnUpdateDisplay(pDrv->pUpPort);
                }
            }
            /* Inform the VRDP server that the current display update sequence is
             * completed. At this moment the framebuffer memory contains a definite
             * image, that is synchronized with the orders already sent to VRDP client.
             * The server can now process redraw requests from clients or initial
             * fullscreen updates for new clients.
             */
            if (pFBInfo->u32ResizeStatus == ResizeStatus_Void)
            {
                Assert (pDisplay->mParent && pDisplay->mParent->consoleVRDPServer());
                pDisplay->mParent->consoleVRDPServer()->SendUpdate (uScreenId, NULL, 0);
            }
        }
    }

#ifdef DEBUG_sunlover
    STAM_PROFILE_STOP(&StatDisplayRefresh, a);
#endif /* DEBUG_sunlover */
#ifdef DEBUG_sunlover_2
    LogFlowFunc (("leave\n"));
#endif /* DEBUG_sunlover_2 */
}

/**
 * Reset notification
 *
 * @see PDMIDISPLAYCONNECTOR::pfnReset
 */
DECLCALLBACK(void) Display::displayResetCallback(PPDMIDISPLAYCONNECTOR pInterface)
{
    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);

    LogFlowFunc (("\n"));

    /* Disable VBVA mode. */
    pDrv->pDisplay->VideoAccelEnable (false, NULL);
}

/**
 * LFBModeChange notification
 *
 * @see PDMIDISPLAYCONNECTOR::pfnLFBModeChange
 */
DECLCALLBACK(void) Display::displayLFBModeChangeCallback(PPDMIDISPLAYCONNECTOR pInterface, bool fEnabled)
{
    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);

    LogFlowFunc (("fEnabled=%d\n", fEnabled));

    NOREF(fEnabled);

    /* Disable VBVA mode in any case. The guest driver reenables VBVA mode if necessary. */
    pDrv->pDisplay->VideoAccelEnable (false, NULL);
}

/**
 * Adapter information change notification.
 *
 * @see PDMIDISPLAYCONNECTOR::pfnProcessAdapterData
 */
DECLCALLBACK(void) Display::displayProcessAdapterDataCallback(PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, uint32_t u32VRAMSize)
{
    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);

    if (pvVRAM == NULL)
    {
        unsigned i;
        for (i = 0; i < pDrv->pDisplay->mcMonitors; i++)
        {
            DISPLAYFBINFO *pFBInfo = &pDrv->pDisplay->maFramebuffers[i];

            pFBInfo->u32Offset = 0;
            pFBInfo->u32MaxFramebufferSize = 0;
            pFBInfo->u32InformationSize = 0;
        }
    }
    else
    {
         uint8_t *pu8 = (uint8_t *)pvVRAM;
         pu8 += u32VRAMSize - VBOX_VIDEO_ADAPTER_INFORMATION_SIZE;

         // @todo
         uint8_t *pu8End = pu8 + VBOX_VIDEO_ADAPTER_INFORMATION_SIZE;

         VBOXVIDEOINFOHDR *pHdr;

         for (;;)
         {
             pHdr = (VBOXVIDEOINFOHDR *)pu8;
             pu8 += sizeof (VBOXVIDEOINFOHDR);

             if (pu8 >= pu8End)
             {
                 LogRel(("VBoxVideo: Guest adapter information overflow!!!\n"));
                 break;
             }

             if (pHdr->u8Type == VBOX_VIDEO_INFO_TYPE_DISPLAY)
             {
                 if (pHdr->u16Length != sizeof (VBOXVIDEOINFODISPLAY))
                 {
                     LogRel(("VBoxVideo: Guest adapter information %s invalid length %d!!!\n", "DISPLAY", pHdr->u16Length));
                     break;
                 }

                 VBOXVIDEOINFODISPLAY *pDisplay = (VBOXVIDEOINFODISPLAY *)pu8;

                 if (pDisplay->u32Index >= pDrv->pDisplay->mcMonitors)
                 {
                     LogRel(("VBoxVideo: Guest adapter information invalid display index %d!!!\n", pDisplay->u32Index));
                     break;
                 }

                 DISPLAYFBINFO *pFBInfo = &pDrv->pDisplay->maFramebuffers[pDisplay->u32Index];

                 pFBInfo->u32Offset = pDisplay->u32Offset;
                 pFBInfo->u32MaxFramebufferSize = pDisplay->u32FramebufferSize;
                 pFBInfo->u32InformationSize = pDisplay->u32InformationSize;

                 LogFlow(("VBOX_VIDEO_INFO_TYPE_DISPLAY: %d: at 0x%08X, size 0x%08X, info 0x%08X\n", pDisplay->u32Index, pDisplay->u32Offset, pDisplay->u32FramebufferSize, pDisplay->u32InformationSize));
             }
             else if (pHdr->u8Type == VBOX_VIDEO_INFO_TYPE_QUERY_CONF32)
             {
                 if (pHdr->u16Length != sizeof (VBOXVIDEOINFOQUERYCONF32))
                 {
                     LogRel(("VBoxVideo: Guest adapter information %s invalid length %d!!!\n", "CONF32", pHdr->u16Length));
                     break;
                 }

                 VBOXVIDEOINFOQUERYCONF32 *pConf32 = (VBOXVIDEOINFOQUERYCONF32 *)pu8;

                 switch (pConf32->u32Index)
                 {
                     case VBOX_VIDEO_QCI32_MONITOR_COUNT:
                     {
                         pConf32->u32Value = pDrv->pDisplay->mcMonitors;
                     } break;

                     case VBOX_VIDEO_QCI32_OFFSCREEN_HEAP_SIZE:
                     {
                         /* @todo make configurable. */
                         pConf32->u32Value = _1M;
                     } break;

                     default:
                         LogRel(("VBoxVideo: CONF32 %d not supported!!! Skipping.\n", pConf32->u32Index));
                 }
             }
             else if (pHdr->u8Type == VBOX_VIDEO_INFO_TYPE_END)
             {
                 if (pHdr->u16Length != 0)
                 {
                     LogRel(("VBoxVideo: Guest adapter information %s invalid length %d!!!\n", "END", pHdr->u16Length));
                     break;
                 }

                 break;
             }
             else
             {
                 LogRel(("Guest adapter information contains unsupported type %d. The block has been skipped.\n", pHdr->u8Type));
             }

             pu8 += pHdr->u16Length;
         }
    }
}

/**
 * Display information change notification.
 *
 * @see PDMIDISPLAYCONNECTOR::pfnProcessDisplayData
 */
DECLCALLBACK(void) Display::displayProcessDisplayDataCallback(PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, unsigned uScreenId)
{
    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);

    if (uScreenId >= pDrv->pDisplay->mcMonitors)
    {
        LogRel(("VBoxVideo: Guest display information invalid display index %d!!!\n", uScreenId));
        return;
    }

    /* Get the display information strcuture. */
    DISPLAYFBINFO *pFBInfo = &pDrv->pDisplay->maFramebuffers[uScreenId];

    uint8_t *pu8 = (uint8_t *)pvVRAM;
    pu8 += pFBInfo->u32Offset + pFBInfo->u32MaxFramebufferSize;

    // @todo
    uint8_t *pu8End = pu8 + pFBInfo->u32InformationSize;

    VBOXVIDEOINFOHDR *pHdr;

    for (;;)
    {
        pHdr = (VBOXVIDEOINFOHDR *)pu8;
        pu8 += sizeof (VBOXVIDEOINFOHDR);

        if (pu8 >= pu8End)
        {
            LogRel(("VBoxVideo: Guest display information overflow!!!\n"));
            break;
        }

        if (pHdr->u8Type == VBOX_VIDEO_INFO_TYPE_SCREEN)
        {
            if (pHdr->u16Length != sizeof (VBOXVIDEOINFOSCREEN))
            {
                LogRel(("VBoxVideo: Guest display information %s invalid length %d!!!\n", "SCREEN", pHdr->u16Length));
                break;
            }

            VBOXVIDEOINFOSCREEN *pScreen = (VBOXVIDEOINFOSCREEN *)pu8;

            pFBInfo->xOrigin = pScreen->xOrigin;
            pFBInfo->yOrigin = pScreen->yOrigin;

            pFBInfo->w = pScreen->u16Width;
            pFBInfo->h = pScreen->u16Height;

            LogFlow(("VBOX_VIDEO_INFO_TYPE_SCREEN: (%p) %d: at %d,%d, linesize 0x%X, size %dx%d, bpp %d, flags 0x%02X\n",
                     pHdr, uScreenId, pScreen->xOrigin, pScreen->yOrigin, pScreen->u32LineSize, pScreen->u16Width, pScreen->u16Height, pScreen->bitsPerPixel, pScreen->u8Flags));

            if (uScreenId != VBOX_VIDEO_PRIMARY_SCREEN)
            {
                /* Primary screen resize is initiated by the VGA device. */
                pDrv->pDisplay->handleDisplayResize(uScreenId, pScreen->bitsPerPixel, (uint8_t *)pvVRAM + pFBInfo->u32Offset, pScreen->u32LineSize, pScreen->u16Width, pScreen->u16Height);
            }
        }
        else if (pHdr->u8Type == VBOX_VIDEO_INFO_TYPE_END)
        {
            if (pHdr->u16Length != 0)
            {
                LogRel(("VBoxVideo: Guest adapter information %s invalid length %d!!!\n", "END", pHdr->u16Length));
                break;
            }

            break;
        }
        else if (pHdr->u8Type == VBOX_VIDEO_INFO_TYPE_HOST_EVENTS)
        {
            if (pHdr->u16Length != sizeof (VBOXVIDEOINFOHOSTEVENTS))
            {
                LogRel(("VBoxVideo: Guest display information %s invalid length %d!!!\n", "HOST_EVENTS", pHdr->u16Length));
                break;
            }

            VBOXVIDEOINFOHOSTEVENTS *pHostEvents = (VBOXVIDEOINFOHOSTEVENTS *)pu8;

            pFBInfo->pHostEvents = pHostEvents;

            LogFlow(("VBOX_VIDEO_INFO_TYPE_HOSTEVENTS: (%p)\n",
                     pHostEvents));
        }
        else if (pHdr->u8Type == VBOX_VIDEO_INFO_TYPE_LINK)
        {
            if (pHdr->u16Length != sizeof (VBOXVIDEOINFOLINK))
            {
                LogRel(("VBoxVideo: Guest adapter information %s invalid length %d!!!\n", "LINK", pHdr->u16Length));
                break;
            }

            VBOXVIDEOINFOLINK *pLink = (VBOXVIDEOINFOLINK *)pu8;
            pu8 += pLink->i32Offset;
        }
        else
        {
            LogRel(("Guest display information contains unsupported type %d\n", pHdr->u8Type));
        }

        pu8 += pHdr->u16Length;
    }
}

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 */
DECLCALLBACK(void *)  Display::drvQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVMAINDISPLAY pDrv = PDMINS2DATA(pDrvIns, PDRVMAINDISPLAY);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_DISPLAY_CONNECTOR:
            return &pDrv->Connector;
        default:
            return NULL;
    }
}


/**
 * Destruct a display driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
DECLCALLBACK(void) Display::drvDestruct(PPDMDRVINS pDrvIns)
{
    PDRVMAINDISPLAY pData = PDMINS2DATA(pDrvIns, PDRVMAINDISPLAY);
    LogFlowFunc (("iInstance=%d\n", pDrvIns->iInstance));
    if (pData->pDisplay)
    {
        AutoLock displayLock (pData->pDisplay);
        pData->pDisplay->mpDrv = NULL;
        pData->pDisplay->mpVMMDev = NULL;
        pData->pDisplay->mLastAddress = NULL;
        pData->pDisplay->mLastBytesPerLine = 0;
        pData->pDisplay->mLastBitsPerPixel = 0,
        pData->pDisplay->mLastWidth = 0;
        pData->pDisplay->mLastHeight = 0;
    }
}


/**
 * Construct a display driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
DECLCALLBACK(int) Display::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVMAINDISPLAY pData = PDMINS2DATA(pDrvIns, PDRVMAINDISPLAY);
    LogFlowFunc (("iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Object\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;
    PPDMIBASE pBaseIgnore;
    int rc = pDrvIns->pDrvHlp->pfnAttach(pDrvIns, &pBaseIgnore);
    if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Configuration error: Not possible to attach anything to this driver!\n"));
        return VERR_PDM_DRVINS_NO_ATTACH;
    }

    /*
     * Init Interfaces.
     */
    pDrvIns->IBase.pfnQueryInterface    = Display::drvQueryInterface;

    pData->Connector.pfnResize          = Display::displayResizeCallback;
    pData->Connector.pfnUpdateRect      = Display::displayUpdateCallback;
    pData->Connector.pfnRefresh         = Display::displayRefreshCallback;
    pData->Connector.pfnReset           = Display::displayResetCallback;
    pData->Connector.pfnLFBModeChange   = Display::displayLFBModeChangeCallback;
    pData->Connector.pfnProcessAdapterData = Display::displayProcessAdapterDataCallback;
    pData->Connector.pfnProcessDisplayData = Display::displayProcessDisplayDataCallback;

    /*
     * Get the IDisplayPort interface of the above driver/device.
     */
    pData->pUpPort = (PPDMIDISPLAYPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_DISPLAY_PORT);
    if (!pData->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No display port interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /*
     * Get the Display object pointer and update the mpDrv member.
     */
    void *pv;
    rc = CFGMR3QueryPtr(pCfgHandle, "Object", &pv);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: No/bad \"Object\" value! rc=%Vrc\n", rc));
        return rc;
    }
    pData->pDisplay = (Display *)pv;        /** @todo Check this cast! */
    pData->pDisplay->mpDrv = pData;

    /*
     * Update our display information according to the framebuffer
     */
    pData->pDisplay->updateDisplayData();

    /*
     * Start periodic screen refreshes
     */
    pData->pUpPort->pfnSetRefreshRate(pData->pUpPort, 20);

    return VINF_SUCCESS;
}


/**
 * Display driver registration record.
 */
const PDMDRVREG Display::DrvReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "MainDisplay",
    /* pszDescription */
    "Main display driver (Main as in the API).",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_DISPLAY,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVMAINDISPLAY),
    /* pfnConstruct */
    Display::drvConstruct,
    /* pfnDestruct */
    Display::drvDestruct,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnDetach */
    NULL
};
