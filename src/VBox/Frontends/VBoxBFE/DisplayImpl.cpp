/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Implementation of VMDisplay class
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
 */

#define LOG_GROUP LOG_GROUP_MAIN

#ifdef VBOXBFE_WITHOUT_COM
# include "COMDefs.h"
# include <iprt/string.h>
#else
# include <VBox/com/defs.h>
#endif

#include <iprt/alloc.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <VBox/pdm.h>
#include <VBox/VBoxGuest.h>
#include <VBox/cfgm.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <VBox/log.h>
#include <iprt/asm.h>

#ifdef RT_OS_L4
#include <stdio.h>
#include <l4/util/util.h>
#include <l4/log/l4log.h>
#endif

#include "DisplayImpl.h"
#include "Framebuffer.h"
#include "VMMDevInterface.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * VMDisplay driver instance data.
 */
typedef struct DRVMAINDISPLAY
{
    /** Pointer to the display object. */
    VMDisplay                    *pDisplay;
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Pointer to the keyboard port interface of the driver/device above us. */
    PPDMIDISPLAYPORT            pUpPort;
    /** Our display connector interface. */
    PDMIDISPLAYCONNECTOR        Connector;
} DRVMAINDISPLAY, *PDRVMAINDISPLAY;

/** Converts PDMIDISPLAYCONNECTOR pointer to a DRVMAINDISPLAY pointer. */
#define PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface) ( (PDRVMAINDISPLAY) ((uintptr_t)pInterface - RT_OFFSETOF(DRVMAINDISPLAY, Connector)) )


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

VMDisplay::VMDisplay()
{
    mpDrv = NULL;

    mpVbvaMemory = NULL;
    mfVideoAccelEnabled = false;

    mpPendingVbvaMemory = NULL;
    mfPendingVideoAccelEnable = false;

    mfMachineRunning = false;

    mpu8VbvaPartial = NULL;
    mcbVbvaPartial = 0;

    RTSemEventMultiCreate(&mUpdateSem);

    // reset the event sems
    RTSemEventMultiReset(mUpdateSem);

    // by default, we have an internal Framebuffer which is
    // NULL, i.e. a black hole for no display output
    mFramebuffer = 0;
    mInternalFramebuffer = true;
    mFramebufferOpened = false;

    mu32ResizeStatus = ResizeStatus_Void;
}

VMDisplay::~VMDisplay()
{
    mFramebuffer = 0;
    RTSemEventMultiDestroy(mUpdateSem);
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Handle display resize event.
 *
 * @returns COM status code
 * @param w New display width
 * @param h New display height
 */
int VMDisplay::handleDisplayResize (int w, int h)
{
    LogFlow(("VMDisplay::handleDisplayResize(): w=%d, h=%d\n", w, h));

    // if there is no Framebuffer, this call is not interesting
    if (mFramebuffer == NULL)
        return VINF_SUCCESS;

    /* Atomically set the resize status before calling the framebuffer. The new InProgress status will
     * disable access to the VGA device by the EMT thread.
     */
    bool f = ASMAtomicCmpXchgU32 (&mu32ResizeStatus, ResizeStatus_InProgress, ResizeStatus_Void);
    AssertRelease(f);NOREF(f);

    // callback into the Framebuffer to notify it
    BOOL finished;

    mFramebuffer->Lock();

    mFramebuffer->RequestResize(w, h, &finished);

    if (!finished)
    {
        LogFlow(("VMDisplay::handleDisplayResize: external framebuffer wants us to wait!\n"));

        /* Note: The previously obtained framebuffer lock must be preserved. 
         *       The EMT keeps the framebuffer lock until the resize process completes.
         */

        return VINF_VGA_RESIZE_IN_PROGRESS;
    }

    /* Set the status so the 'handleResizeCompleted' would work.  */
    f = ASMAtomicCmpXchgU32 (&mu32ResizeStatus, ResizeStatus_UpdateDisplayData, ResizeStatus_InProgress);
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
void VMDisplay::handleResizeCompletedEMT (void)
{
    LogFlowFunc(("\n"));
    if (mFramebuffer)
    {
        /* Framebuffer has completed the resize. Update the connector data. */
        updateDisplayData();
    
        mpDrv->pUpPort->pfnSetRenderVRAM (mpDrv->pUpPort, true);

        /* Unlock framebuffer. */
        mFramebuffer->Unlock();
    }

    /* Go into non resizing state. */
    bool f = ASMAtomicCmpXchgU32 (&mu32ResizeStatus, ResizeStatus_Void, ResizeStatus_UpdateDisplayData);
    AssertRelease(f);NOREF(f);
}

/**
 * Notification that the framebuffer has completed the
 * asynchronous resize processing
 *
 * @returns COM status code
 */
STDMETHODIMP VMDisplay::ResizeCompleted()
{
    LogFlow(("VMDisplay::ResizeCompleted\n"));

    // this is only valid for external framebuffers
    if (mInternalFramebuffer)
        return E_FAIL;

    /* Set the flag indicating that the resize has completed and display data need to be updated. */
    bool f = ASMAtomicCmpXchgU32 (&mu32ResizeStatus, ResizeStatus_UpdateDisplayData, ResizeStatus_InProgress);
    AssertRelease(f);NOREF(f);

    return S_OK;
}

static void checkCoordBounds (int *px, int *py, int *pw, int *ph, int cx, int cy)
{
    /* Correct negative x and y coordinates. */
    if (*px < 0)
    {
        *px += *pw; /* Compute xRight which is also the new width. */
        *pw = (*px < 0) ? 0: *px;
        *px = 0;
    }

    if (*py < 0)
    {
        *py += *ph; /* Compute xBottom, which is also the new height. */
        *ph = (*py < 0) ? 0: *py;
        *py = 0;
    }

    /* Also check if coords are greater than the display resolution. */
    if (*px + *pw > cx)
        *pw = cx > *px ? cx - *px: 0;

    if (*py + *ph > cy)
        *ph = cy > *py ? cy - *py: 0;
}

/**
 * Handle display update
 *
 * @returns COM status code
 * @param w New display width
 * @param h New display height
 */
void VMDisplay::handleDisplayUpdate (int x, int y, int w, int h)
{
    // if there is no Framebuffer, this call is not interesting
    if (mFramebuffer == NULL)
        return;

    mFramebuffer->Lock();

    checkCoordBounds (&x, &y, &w, &h, mpDrv->Connector.cx, mpDrv->Connector.cy);

    if (w == 0 || h == 0)
    {
        mFramebuffer->Unlock();
        return;
    }

    // special processing for the internal Framebuffer
    if (mInternalFramebuffer)
    {
        mFramebuffer->Unlock();
    }
    else
    {
        // callback into the Framebuffer to notify it
        BOOL finished;

        RTSemEventMultiReset(mUpdateSem);

        mFramebuffer->NotifyUpdate(x, y, w, h, &finished);
        mFramebuffer->Unlock();

        if (!finished)
        {
            // the Framebuffer needs more time to process
            // the event so we have to halt the VM until it's done
            RTSemEventMultiWait(mUpdateSem, RT_INDEFINITE_WAIT);
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
uint32_t VMDisplay::getWidth()
{
    Assert(mpDrv);
    return mpDrv->Connector.cx;
}

/**
 * Returns the current display height in pixel
 *
 * @returns COM status code
 * @param height Address of result variable.
 */
uint32_t VMDisplay::getHeight()
{
    Assert(mpDrv);
    return mpDrv->Connector.cy;
}

/**
 * Returns the current display color depth in bits
 *
 * @returns COM status code
 * @param bitsPerPixel Address of result variable.
 */
uint32_t VMDisplay::getBitsPerPixel()
{
    Assert(mpDrv);
    return mpDrv->Connector.cBits;
}

void VMDisplay::updatePointerShape(bool fVisible, bool fAlpha, uint32_t xHot, uint32_t yHot, uint32_t width, uint32_t height, void *pShape)
{
}


// IDisplay methods
/////////////////////////////////////////////////////////////////////////////

/**
 * Registers an external Framebuffer
 *
 * @returns COM status code
 * @param Framebuffer external Framebuffer object
 */
STDMETHODIMP VMDisplay::RegisterExternalFramebuffer(Framebuffer *Framebuffer)
{
    if (!Framebuffer)
        return E_POINTER;

    // free current Framebuffer (if there is any)
    mFramebuffer = 0;
    mInternalFramebuffer = false;
    mFramebuffer = Framebuffer;
    updateDisplayData();
    return S_OK;
}

/* InvalidateAndUpdate schedules a request that eventually calls   */
/* mpDrv->pUpPort->pfnUpdateDisplayAll which in turns accesses the */
/* framebuffer. In order to synchronize with other framebuffer     */
/* related activities this call needs to be framed by Lock/Unlock. */
void
VMDisplay::doInvalidateAndUpdate(struct DRVMAINDISPLAY  *mpDrv)
{
    mpDrv->pDisplay->mFramebuffer->Lock();
    mpDrv->pUpPort->pfnUpdateDisplayAll( mpDrv->pUpPort);
    mpDrv->pDisplay->mFramebuffer->Unlock();
}

/**
 * Does a full invalidation of the VM display and instructs the VM
 * to update it immediately.
 *
 * @returns COM status code
 */
STDMETHODIMP VMDisplay::InvalidateAndUpdate()
{
    LogFlow (("VMDisplay::InvalidateAndUpdate(): BEGIN\n"));

    HRESULT rc = S_OK;

    LogFlow (("VMDisplay::InvalidateAndUpdate(): sending DPYUPDATE request\n"));

    Assert(pVM);
    /* pdm.h says that this has to be called from the EMT thread */
    PVMREQ pReq;
    int rcVBox = VMR3ReqCallVoid(pVM, &pReq, RT_INDEFINITE_WAIT,
                                 (PFNRT)VMDisplay::doInvalidateAndUpdate, 1, mpDrv);
    if (VBOX_SUCCESS(rcVBox))
        VMR3ReqFree(pReq);

    if (VBOX_FAILURE(rcVBox))
        rc = E_FAIL;

    LogFlow (("VMDisplay::InvalidateAndUpdate(): END: rc=%08X\n", rc));
    return rc;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/**
 * Helper to update the display information from the Framebuffer
 *
 */
void VMDisplay::updateDisplayData()
{

    while(!mFramebuffer)
    {
#if RT_OS_L4
      asm volatile ("nop":::"memory");
      l4_sleep(5);
#else
      RTThreadYield();
#endif
    }
    Assert(mFramebuffer);
    // the driver might not have been constructed yet
    if (mpDrv)
    {
        mFramebuffer->getAddress ((uintptr_t *)&mpDrv->Connector.pu8Data);
        mFramebuffer->getLineSize ((ULONG*)&mpDrv->Connector.cbScanline);
        mFramebuffer->getBitsPerPixel ((ULONG*)&mpDrv->Connector.cBits);
        mFramebuffer->getWidth ((ULONG*)&mpDrv->Connector.cx);
        mFramebuffer->getHeight ((ULONG*)&mpDrv->Connector.cy);
        mpDrv->pUpPort->pfnSetRenderVRAM (mpDrv->pUpPort,
                                          !!(mpDrv->Connector.pu8Data != (uint8_t*)~0UL));
    }
}

void VMDisplay::resetFramebuffer()
{
    if (!mFramebuffer)
        return;

    // the driver might not have been constructed yet
    if (mpDrv)
    {
        mFramebuffer->getAddress ((uintptr_t *)&mpDrv->Connector.pu8Data);
        mFramebuffer->getBitsPerPixel ((ULONG*)&mpDrv->Connector.cBits);
        mpDrv->pUpPort->pfnSetRenderVRAM (mpDrv->pUpPort,
                                          !!(mpDrv->Connector.pu8Data != (uint8_t*)~0UL));
    }
}

/**
 * Handle display resize event
 *
 * @param  pInterface VMDisplay connector.
 * @param  cx         New width in pixels.
 * @param  cy         New height in pixels.
 */
DECLCALLBACK(int) VMDisplay::displayResizeCallback(PPDMIDISPLAYCONNECTOR pInterface, uint32_t bpp, void *pvVRAM, uint32_t cbLine, uint32_t cx, uint32_t cy)
{
    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);

    // forward call to instance handler
    return pDrv->pDisplay->handleDisplayResize(cx, cy);
}

/**
 * Handle display update
 *
 * @param  pInterface VMDisplay connector.
 * @param  x          Left upper boundary x.
 * @param  y          Left upper boundary y.
 * @param  cx         Update rect width.
 * @param  cy         Update rect height.
 */
DECLCALLBACK(void) VMDisplay::displayUpdateCallback(PPDMIDISPLAYCONNECTOR pInterface,
                                                  uint32_t x, uint32_t y, uint32_t cx, uint32_t cy)
{
    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);

    // forward call to instance handler
    pDrv->pDisplay->handleDisplayUpdate(x, y, cx, cy);
}

/**
 * Periodic display refresh callback.
 *
 * @param  pInterface VMDisplay connector.
 */
DECLCALLBACK(void) VMDisplay::displayRefreshCallback(PPDMIDISPLAYCONNECTOR pInterface)
{
    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);


    /* Contrary to displayUpdateCallback and displayResizeCallback
     * the framebuffer lock must be taken since the the function
     * pointed to by pDrv->pUpPort->pfnUpdateDisplay is anaware
     * of any locking issues. */

    VMDisplay *pDisplay = pDrv->pDisplay;

    uint32_t u32ResizeStatus = pDisplay->mu32ResizeStatus;
    
    if (u32ResizeStatus == ResizeStatus_UpdateDisplayData)
    {
#ifdef DEBUG_sunlover
        LogFlowFunc (("ResizeStatus_UpdateDisplayData\n"));
#endif /* DEBUG_sunlover */
        /* The framebuffer was resized and display data need to be updated. */
        pDisplay->handleResizeCompletedEMT ();
        /* Continue with normal processing because the status here is ResizeStatus_Void. */
        Assert (pDisplay->mu32ResizeStatus == ResizeStatus_Void);
        /* Repaint the display because VM continued to run during the framebuffer resize. */
        pDrv->pUpPort->pfnUpdateDisplayAll(pDrv->pUpPort);
        /* Ignore the refresh to replay the logic. */
        return;
    }
    else if (u32ResizeStatus == ResizeStatus_InProgress)
    {
#ifdef DEBUG_sunlover
        LogFlowFunc (("ResizeStatus_InProcess\n"));
#endif /* DEBUG_sunlover */
        /* The framebuffer is being resized. Do not call the VGA device back. Immediately return. */
        return;
    }

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
            pDisplay->VideoAccelEnable (pDisplay->mfPendingVideoAccelEnable, pDisplay->mpPendingVbvaMemory);

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
            pDisplay->mFramebuffer->Lock();
            pDrv->pUpPort->pfnUpdateDisplay(pDrv->pUpPort);
            pDisplay->mFramebuffer->Unlock();
        }
    }
}

/**
 * Reset notification
 *
 * @param  pInterface Display connector.
 */
DECLCALLBACK(void) VMDisplay::displayResetCallback(PPDMIDISPLAYCONNECTOR pInterface)
{
    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);

    LogFlow(("Display::displayResetCallback\n"));

    /* Disable VBVA mode. */
    pDrv->pDisplay->VideoAccelEnable (false, NULL);
}

/**
 * LFBModeChange notification
 *
 * @see PDMIDISPLAYCONNECTOR::pfnLFBModeChange
 */
DECLCALLBACK(void) VMDisplay::displayLFBModeChangeCallback(PPDMIDISPLAYCONNECTOR pInterface, bool fEnabled)
{
    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);

    LogFlow(("Display::displayLFBModeChangeCallback: %d\n", fEnabled));

    NOREF(fEnabled);

    /**
     * @todo: If we got the callback then VM if definitely running.
     *        But a better method should be implemented.
     */
    pDrv->pDisplay->mfMachineRunning = true;

    /* Disable VBVA mode in any case. The guest driver reenables VBVA mode if necessary. */
    pDrv->pDisplay->VideoAccelEnable (false, NULL);
}

DECLCALLBACK(void) VMDisplay::displayProcessAdapterDataCallback(PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, uint32_t u32VRAMSize)
{
    NOREF(pInterface);
    NOREF(pvVRAM);
    NOREF(u32VRAMSize);
}

DECLCALLBACK(void) VMDisplay::displayProcessDisplayDataCallback(PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, unsigned uScreenId)
{
    NOREF(pInterface);
    NOREF(pvVRAM);
    NOREF(uScreenId);
}


typedef struct _VBVADIRTYREGION
{
    /* Copies of object's pointers used by vbvaRgn functions. */
    Framebuffer     *pFramebuffer;
    VMDisplay        *pDisplay;
    PPDMIDISPLAYPORT  pPort;

    /* Merged rectangles. */
    int32_t xLeft;
    int32_t xRight;
    int32_t yTop;
    int32_t yBottom;

} VBVADIRTYREGION;

void vbvaRgnInit (VBVADIRTYREGION *prgn, Framebuffer *pfb, VMDisplay *pd, PPDMIDISPLAYPORT pp)
{
    memset (prgn, 0, sizeof (VBVADIRTYREGION));

    prgn->pFramebuffer = pfb;
    prgn->pDisplay = pd;
    prgn->pPort = pp;

    return;
}

void vbvaRgnDirtyRect (VBVADIRTYREGION *prgn, VBVACMDHDR *phdr)
{
    LogFlow(("vbvaRgnDirtyRect: x = %d, y = %d, w = %d, h = %d\n", phdr->x, phdr->y, phdr->w, phdr->h));

    /*
     * Here update rectangles are accumulated to form an update area.
     * @todo
     * Now the simplies method is used which builds one rectangle that
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

    if (prgn->xRight == 0)
    {
        /* This is the first rectangle to be added. */
        prgn->xLeft   = phdr->x;
        prgn->yTop    = phdr->y;
        prgn->xRight  = xRight;
        prgn->yBottom = yBottom;
    }
    else
    {
        /* Adjust region coordinates. */
        if (prgn->xLeft > phdr->x)
            prgn->xLeft = phdr->x;

        if (prgn->yTop > phdr->y)
            prgn->yTop = phdr->y;

        if (prgn->xRight < xRight)
            prgn->xRight = xRight;

        if (prgn->yBottom < yBottom)
            prgn->yBottom = yBottom;
    }
}

void vbvaRgnUpdateFramebuffer (VBVADIRTYREGION *prgn)
{
    uint32_t w = prgn->xRight - prgn->xLeft;
    uint32_t h = prgn->yBottom - prgn->yTop;

    if (prgn->pFramebuffer && w != 0 && h != 0)
    {
        prgn->pPort->pfnUpdateDisplayRect (prgn->pPort, prgn->xLeft, prgn->yTop, w, h);
        prgn->pDisplay->handleDisplayUpdate (prgn->xLeft, prgn->yTop, w, h);
    }
}

static void vbvaSetMemoryFlags (VBVAMEMORY *pVbvaMemory, bool fVideoAccelEnabled, bool fVideoAccelVRDP)
{
    if (pVbvaMemory)
    {
        /* This called only on changes in mode. So reset VRDP always. */
        uint32_t fu32Flags = VBVA_F_MODE_VRDP_RESET;

        if (fVideoAccelEnabled)
        {
            fu32Flags |= VBVA_F_MODE_ENABLED;

            if (fVideoAccelVRDP)
                fu32Flags |= VBVA_F_MODE_VRDP;
        }

        pVbvaMemory->fu32ModeFlags = fu32Flags;
    }
}

bool VMDisplay::VideoAccelAllowed (void)
{
    return true;
}

/**
 * @thread EMT
 */
int VMDisplay::VideoAccelEnable (bool fEnable, VBVAMEMORY *pVbvaMemory)
{
    int rc = VINF_SUCCESS;

    /* Called each time the guest wants to use acceleration,
     * or when the VGA device disables acceleration,
     * or when restoring the saved state with accel enabled.
     *
     * VGA device disables acceleration on each video mode change
     * and on reset.
     *
     * Guest enabled acceleration at will. And it needs to enable
     * acceleration after a mode change.
     */
    LogFlow(("Display::VideoAccelEnable: mfVideoAccelEnabled = %d, fEnable = %d, pVbvaMemory = %p\n",
              mfVideoAccelEnabled, fEnable, pVbvaMemory));

    /* Strictly check parameters. Callers must not pass anything in the case. */
    Assert((fEnable && pVbvaMemory) || (!fEnable && pVbvaMemory == NULL));

    if (!VideoAccelAllowed ())
        return VERR_NOT_SUPPORTED;

    /*
     * Verify that the VM is in running state. If it is not,
     * then this must be postponed until it goes to running.
     */
    if (!mfMachineRunning)
    {
        Assert (!mfVideoAccelEnabled);

        LogFlow(("Display::VideoAccelEnable: Machine is not yet running.\n"));

        if (fEnable)
        {
            mfPendingVideoAccelEnable = fEnable;
            mpPendingVbvaMemory = pVbvaMemory;
        }

        return rc;
    }

    /* Check that current status is not being changed */
    if (mfVideoAccelEnabled == fEnable)
        return rc;

    if (mfVideoAccelEnabled)
    {
        /* Process any pending orders and empty the VBVA ring buffer. */
        VideoAccelFlush ();
    }

    if (!fEnable && mpVbvaMemory)
        mpVbvaMemory->fu32ModeFlags &= ~VBVA_F_MODE_ENABLED;

    /* Safety precaution. There is no more VBVA until everything is setup! */
    mpVbvaMemory = NULL;
    mfVideoAccelEnabled = false;

    /* Update entire display. */
    mpDrv->pUpPort->pfnUpdateDisplayAll(mpDrv->pUpPort);

    /* Everything OK. VBVA status can be changed. */

    /* Notify the VMMDev, which saves VBVA status in the saved state,
     * and needs to know current status.
     */
    PPDMIVMMDEVPORT pVMMDevPort = gVMMDev->getVMMDevPort ();

    if (pVMMDevPort)
        pVMMDevPort->pfnVBVAChange (pVMMDevPort, fEnable);

    if (fEnable)
    {
        mpVbvaMemory = pVbvaMemory;
        mfVideoAccelEnabled = true;

        /* Initialize the hardware memory. */
        vbvaSetMemoryFlags (mpVbvaMemory, mfVideoAccelEnabled, false);
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

    LogFlow(("Display::VideoAccelEnable: rc = %Vrc.\n", rc));

    return rc;
}

static bool vbvaVerifyRingBuffer (VBVAMEMORY *pVbvaMemory)
{
    return true;
}

static void vbvaFetchBytes (VBVAMEMORY *pVbvaMemory, uint8_t *pu8Dst, uint32_t cbDst)
{
    if (cbDst >= VBVA_RING_BUFFER_SIZE)
    {
        AssertFailed ();
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

void VMDisplay::SetVideoModeHint(ULONG aWidth, ULONG aHeight, ULONG aBitsPerPixel, ULONG aDisplay)
{
    PPDMIVMMDEVPORT pVMMDevPort = gVMMDev->getVMMDevPort ();

    if (pVMMDevPort)
        pVMMDevPort->pfnRequestDisplayChange(pVMMDevPort, aWidth, aHeight, aBitsPerPixel, aDisplay);
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
            RTMemFree (*ppu8);

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
bool VMDisplay::vbvaFetchCmd (VBVACMDHDR **ppHdr, uint32_t *pcbCmd)
{
    uint32_t indexRecordFirst = mpVbvaMemory->indexRecordFirst;
    uint32_t indexRecordFree = mpVbvaMemory->indexRecordFree;

#ifdef DEBUG_sunlover
    LogFlow(("MAIN::DisplayImpl::vbvaFetchCmd:first = %d, free = %d\n",
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
    LogFlow(("MAIN::DisplayImpl::vbvaFetchCmd: cbRecord = 0x%08X\n",
             pRecord->cbRecord));
#endif /* DEBUG_sunlover */

    uint32_t cbRecord = pRecord->cbRecord & ~VBVA_F_RECORD_PARTIAL;

    if (mcbVbvaPartial)
    {
        /* There is a partial read in process. Continue with it. */

        Assert (mpu8VbvaPartial);

        LogFlow(("MAIN::DisplayImpl::vbvaFetchCmd: continue partial record mcbVbvaPartial = %d cbRecord 0x%08X, first = %d, free = %d\n",
                 mcbVbvaPartial, pRecord->cbRecord, indexRecordFirst, indexRecordFree));

        if (cbRecord > mcbVbvaPartial)
        {
            /* New data has been added to the record. */
            if (!vbvaPartialRead (&mpu8VbvaPartial, &mcbVbvaPartial, cbRecord, mpVbvaMemory))
                return false;
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
            LogFlow(("MAIN::DisplayImpl::vbvaFetchBytes: partial done ok, data = %d, free = %d\n",
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
                return false;

            LogFlow(("MAIN::DisplayImpl::vbvaFetchCmd: started partial record mcbVbvaPartial = 0x%08X cbRecord 0x%08X, first = %d, free = %d\n",
                     mcbVbvaPartial, pRecord->cbRecord, indexRecordFirst, indexRecordFree));
        }

        return true;
    }

    /* Current record is complete. */

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
            LogFlow(("MAIN::DisplayImpl::vbvaFetchCmd: could not allocate %d bytes from heap!!!\n", cbRecord));
            mpVbvaMemory->off32Data = (mpVbvaMemory->off32Data + cbRecord) % VBVA_RING_BUFFER_SIZE;
            return false;
        }

        vbvaFetchBytes (mpVbvaMemory, dst, cbRecord);

        *ppHdr = (VBVACMDHDR *)dst;

#ifdef DEBUG_sunlover
        LogFlow(("MAIN::DisplayImpl::vbvaFetchBytes: Allocated from heap %p\n", dst));
#endif /* DEBUG_sunlover */
    }

    *pcbCmd = cbRecord;

    /* Advance the record index. */
    mpVbvaMemory->indexRecordFirst = (indexRecordFirst + 1) % VBVA_MAX_RECORDS;

#ifdef DEBUG_sunlover
    LogFlow(("MAIN::DisplayImpl::vbvaFetchBytes: done ok, data = %d, free = %d\n",
              mpVbvaMemory->off32Data, mpVbvaMemory->off32Free));
#endif /* DEBUG_sunlover */

    return true;
}

void VMDisplay::vbvaReleaseCmd (VBVACMDHDR *pHdr, int32_t cbCmd)
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
        LogFlow(("MAIN::DisplayImpl::vbvaReleaseCmd: Free heap %p\n", pHdr));
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
void VMDisplay::VideoAccelFlush (void)
{
#ifdef DEBUG_sunlover
    LogFlow(("Display::VideoAccelFlush: mfVideoAccelEnabled = %d\n", mfVideoAccelEnabled));
#endif /* DEBUG_sunlover */

    if (!mfVideoAccelEnabled)
    {
        Log(("Display::VideoAccelFlush: called with disabled VBVA!!! Ignoring.\n"));
        return;
    }

    /* Here VBVA is enabled and we have the accelerator memory pointer. */
    Assert(mpVbvaMemory);

#ifdef DEBUG_sunlover
    LogFlow(("Display::VideoAccelFlush: indexRecordFirst = %d, indexRecordFree = %d, off32Data = %d, off32Free = %d\n",
              mpVbvaMemory->indexRecordFirst, mpVbvaMemory->indexRecordFree, mpVbvaMemory->off32Data, mpVbvaMemory->off32Free));
#endif /* DEBUG_sunlover */

    /* Quick check for "nothing to update" case. */
    if (mpVbvaMemory->indexRecordFirst == mpVbvaMemory->indexRecordFree)
        return;

    /* Process the ring buffer */

    bool fFramebufferIsNull = (mFramebuffer == NULL);

    if (!fFramebufferIsNull)
        mFramebuffer->Lock();

    /* Initialize dirty rectangles accumulator. */
    VBVADIRTYREGION rgn;
    vbvaRgnInit (&rgn, mFramebuffer, this, mpDrv->pUpPort);

    for (;;)
    {
        VBVACMDHDR *phdr = NULL;
        uint32_t cbCmd = 0;

        /* Fetch the command data. */
        if (!vbvaFetchCmd (&phdr, &cbCmd))
        {
            Log(("Display::VideoAccelFlush: unable to fetch command. off32Data = %d, off32Free = %d. Disabling VBVA!!!\n",
                  mpVbvaMemory->off32Data, mpVbvaMemory->off32Free));

            /* Disable VBVA on those processing errors. */
            VideoAccelEnable (false, NULL);

            break;
        }

        if (!cbCmd)
        {
            /* No more commands yet in the queue. */
            break;
        }

        if (!fFramebufferIsNull)
        {
#ifdef DEBUG_sunlover
            LogFlow(("MAIN::DisplayImpl::VideoAccelFlush: hdr: cbCmd = %d, x=%d, y=%d, w=%d, h=%d\n", cbCmd, phdr->x, phdr->y, phdr->w, phdr->h));
#endif /* DEBUG_sunlover */

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
            vbvaRgnDirtyRect (&rgn, phdr);

//            /* Forward the command to VRDP server. */
//            mParent->consoleVRDPServer()->SendUpdate (phdr, cbCmd);
        }

        vbvaReleaseCmd (phdr, cbCmd);
    }

    if (!fFramebufferIsNull)
        mFramebuffer->Unlock ();

    /* Draw the framebuffer. */
    vbvaRgnUpdateFramebuffer (&rgn);
}

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 */
DECLCALLBACK(void *)  VMDisplay::drvQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
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
 * Construct a display driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
DECLCALLBACK(int) VMDisplay::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVMAINDISPLAY pData = PDMINS2DATA(pDrvIns, PDRVMAINDISPLAY);
    LogFlow(("VMDisplay::drvConstruct: iInstance=%d\n", pDrvIns->iInstance));


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
    pDrvIns->IBase.pfnQueryInterface    = VMDisplay::drvQueryInterface;

    pData->Connector.pfnResize          = VMDisplay::displayResizeCallback;
    pData->Connector.pfnUpdateRect      = VMDisplay::displayUpdateCallback;
    pData->Connector.pfnRefresh         = VMDisplay::displayRefreshCallback;
    pData->Connector.pfnReset           = VMDisplay::displayResetCallback;
    pData->Connector.pfnLFBModeChange   = VMDisplay::displayLFBModeChangeCallback;
    pData->Connector.pfnProcessAdapterData = VMDisplay::displayProcessAdapterDataCallback;
    pData->Connector.pfnProcessDisplayData = VMDisplay::displayProcessDisplayDataCallback;

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
     * Get the VMDisplay object pointer and update the mpDrv member.
     */
    void *pv;
    rc = CFGMR3QueryPtr(pCfgHandle, "Object", &pv);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: No/bad \"Object\" value! rc=%Vrc\n", rc));
        return rc;
    }
    pData->pDisplay = (VMDisplay *)pv;        /** @todo Check this cast! */
    pData->pDisplay->mpDrv = pData;

    /*
     * If there is a Framebuffer, we have to update our display information
     */
    if (pData->pDisplay->mFramebuffer)
        pData->pDisplay->updateDisplayData();

    /*
     * Start periodic screen refreshes
     */
    pData->pUpPort->pfnSetRefreshRate(pData->pUpPort, 50);

    return VINF_SUCCESS;
}


/**
 * VMDisplay driver registration record.
 */
const PDMDRVREG VMDisplay::DrvReg =
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
    VMDisplay::drvConstruct,
    /* pfnDestruct */
    NULL,
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
