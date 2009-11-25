/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#include "DisplayImpl.h"
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

#ifdef VBOX_WITH_VIDEOHWACCEL
# include <VBox/VBoxVideo.h>
#endif

#include <VBox/com/array.h>
#include <png.h>

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
#if defined(VBOX_WITH_VIDEOHWACCEL)
    /** VBVA callbacks */
    PPDMDDISPLAYVBVACALLBACKS   pVBVACallbacks;
#endif
} DRVMAINDISPLAY, *PDRVMAINDISPLAY;

/** Converts PDMIDISPLAYCONNECTOR pointer to a DRVMAINDISPLAY pointer. */
#define PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface) ( (PDRVMAINDISPLAY) ((uintptr_t)pInterface - RT_OFFSETOF(DRVMAINDISPLAY, Connector)) )

#ifdef DEBUG_sunlover
static STAMPROFILE StatDisplayRefresh;
static int stam = 0;
#endif /* DEBUG_sunlover */

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (Display)

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

    mpDrv = NULL;
    mpVMMDev = NULL;
    mfVMMDevInited = false;

    mLastAddress = NULL;
    mLastBytesPerLine = 0;
    mLastBitsPerPixel = 0,
    mLastWidth = 0;
    mLastHeight = 0;

#ifdef VBOX_WITH_OLD_VBVA_LOCK
    int rc = RTCritSectInit (&mVBVALock);
    AssertRC (rc);
    mfu32PendingVideoAccelDisable = false;
#endif /* VBOX_WITH_OLD_VBVA_LOCK */

    return S_OK;
}

void Display::FinalRelease()
{
    uninit();

#ifdef VBOX_WITH_OLD_VBVA_LOCK
    if (RTCritSectIsInitialized (&mVBVALock))
    {
        RTCritSectDelete (&mVBVALock);
        memset (&mVBVALock, 0, sizeof (mVBVALock));
    }
#endif /* VBOX_WITH_OLD_VBVA_LOCK */
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

#define sSSMDisplayScreenshotVer 0x00010001
#define sSSMDisplayVer 0x00010001

#define kMaxSizePNG 1024
#define kMaxSizeThumbnail 64

/**
 * Save thumbnail and screenshot of the guest screen.
 */
static int displayMakeThumbnail(uint8_t *pu8Data, uint32_t cx, uint32_t cy,
                                uint8_t **ppu8Thumbnail, uint32_t *pcbThumbnail, uint32_t *pcxThumbnail, uint32_t *pcyThumbnail)
{
    int rc = VINF_SUCCESS;

    uint8_t *pu8Thumbnail = NULL;
    uint32_t cbThumbnail = 0;
    uint32_t cxThumbnail = 0;
    uint32_t cyThumbnail = 0;

    if (cx > cy)
    {
        cxThumbnail = kMaxSizeThumbnail;
        cyThumbnail = (kMaxSizeThumbnail * cy) / cx;
    }
    else
    {
        cyThumbnail = kMaxSizeThumbnail;
        cxThumbnail = (kMaxSizeThumbnail * cx) / cy;
    }

    LogFlowFunc(("%dx%d -> %dx%d\n", cx, cy, cxThumbnail, cyThumbnail));

    cbThumbnail = cxThumbnail * 4 * cyThumbnail;
    pu8Thumbnail = (uint8_t *)RTMemAlloc(cbThumbnail);

    if (pu8Thumbnail)
    {
        uint8_t *dst = pu8Thumbnail;
        uint8_t *src = pu8Data;
        int dstX = 0;
        int dstY = 0;
        int srcX = 0;
        int srcY = 0;
        int dstW = cxThumbnail;
        int dstH = cyThumbnail;
        int srcW = cx;
        int srcH = cy;
        gdImageCopyResampled (dst,
                              src,
                              dstX, dstY,
                              srcX, srcY,
                              dstW, dstH, srcW, srcH);

        *ppu8Thumbnail = pu8Thumbnail;
        *pcbThumbnail = cbThumbnail;
        *pcxThumbnail = cxThumbnail;
        *pcyThumbnail = cyThumbnail;
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    return rc;
}

typedef struct PNGWriteCtx
{
    uint8_t *pu8PNG;
    uint32_t cbPNG;
    uint32_t cbAllocated;
    int rc;
} PNGWriteCtx;

static void PNGAPI png_write_data_fn(png_structp png_ptr, png_bytep p, png_size_t cb)
{
    PNGWriteCtx *pCtx = (PNGWriteCtx *)png_get_io_ptr(png_ptr);
    LogFlowFunc(("png_ptr %p, p %p, cb %d, pCtx %p\n", png_ptr, p, cb, pCtx));

    if (pCtx && RT_SUCCESS(pCtx->rc))
    {
        if (pCtx->cbAllocated - pCtx->cbPNG < cb)
        {
            uint32_t cbNew = pCtx->cbPNG + (uint32_t)cb;
            cbNew = RT_ALIGN_32(cbNew, 4096) + 4096;

            void *pNew = RTMemRealloc(pCtx->pu8PNG, cbNew);
            if (!pNew)
            {
                pCtx->rc = VERR_NO_MEMORY;
                return;
            }
            
            pCtx->pu8PNG = (uint8_t *)pNew;
            pCtx->cbAllocated = cbNew;
        }

        memcpy(pCtx->pu8PNG + pCtx->cbPNG, p, cb);
        pCtx->cbPNG += cb;
    }
}

static void PNGAPI png_output_flush_fn(png_structp png_ptr)
{
    NOREF(png_ptr);
    /* Do nothing. */
}

static int displayMakePNG(uint8_t *pu8Data, uint32_t cx, uint32_t cy,
                          uint8_t **ppu8PNG, uint32_t *pcbPNG, uint32_t *pcxPNG, uint32_t *pcyPNG)
{
    int rc = VINF_SUCCESS;

    uint8_t *pu8Bitmap = NULL;
    uint32_t cbBitmap = 0;
    uint32_t cxBitmap = 0;
    uint32_t cyBitmap = 0;

    if (cx < kMaxSizePNG && cy < kMaxSizePNG)
    {
        /* Save unscaled screenshot. */
        pu8Bitmap = pu8Data;
        cbBitmap = cx * 4 * cy;
        cxBitmap = cx;
        cyBitmap = cy;
    }
    else
    {
        /* Large screenshot, scale. */
        if (cx > cy)
        {
            cxBitmap = kMaxSizePNG;
            cyBitmap = (kMaxSizePNG * cy) / cx;
        }
        else
        {
            cyBitmap = kMaxSizePNG;
            cxBitmap = (kMaxSizePNG * cx) / cy;
        }

        cbBitmap = cxBitmap * 4 * cyBitmap;

        pu8Bitmap = (uint8_t *)RTMemAlloc(cbBitmap);

        if (pu8Bitmap)
        {
            uint8_t *dst = pu8Bitmap;
            uint8_t *src = pu8Data;
            int dstX = 0;
            int dstY = 0;
            int srcX = 0;
            int srcY = 0;
            int dstW = cxBitmap;
            int dstH = cyBitmap;
            int srcW = cx;
            int srcH = cy;
            gdImageCopyResampled (dst,
                                  src,
                                  dstX, dstY,
                                  srcX, srcY,
                                  dstW, dstH, srcW, srcH);
        }
        else
        {
            rc = VERR_NO_MEMORY;
        }
    }

    LogFlowFunc(("%dx%d -> %dx%d\n", cx, cy, cxBitmap, cyBitmap));

    if (RT_SUCCESS(rc))
    {
        png_bytep *row_pointers = (png_bytep *)RTMemAlloc(cyBitmap * sizeof(png_bytep));
        if (row_pointers)
        {
            png_infop info_ptr = NULL;
            png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                          png_voidp_NULL, /* error/warning context pointer */
                                                          png_error_ptr_NULL /* error function */,
                                                          png_error_ptr_NULL /* warning function */);
            if (png_ptr)
            {
                info_ptr = png_create_info_struct(png_ptr);
                if (info_ptr)
                {
                    if (!setjmp(png_jmpbuf(png_ptr)))
                    {
                        PNGWriteCtx ctx;
                        ctx.pu8PNG = NULL;
                        ctx.cbPNG = 0;
                        ctx.cbAllocated = 0;
                        ctx.rc = VINF_SUCCESS;

                        png_set_write_fn(png_ptr,
                                         (voidp)&ctx,
                                         png_write_data_fn,
                                         png_output_flush_fn);

                        png_set_IHDR(png_ptr, info_ptr,
                                     cxBitmap, cyBitmap,
                                     8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                                     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

                        png_bytep row_pointer = (png_bytep)pu8Bitmap;
                        unsigned i = 0;
                        for (; i < cyBitmap; i++, row_pointer += cxBitmap * 4)
                        {
                            row_pointers[i] = row_pointer;
                        }
                        png_set_rows(png_ptr, info_ptr, &row_pointers[0]);

                        png_write_info(png_ptr, info_ptr);
                        png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
                        png_set_bgr(png_ptr);

                        if (info_ptr->valid & PNG_INFO_IDAT)
                            png_write_image(png_ptr, info_ptr->row_pointers);

                        png_write_end(png_ptr, info_ptr);

                        rc = ctx.rc;

                        if (RT_SUCCESS(rc))
                        {
                            *ppu8PNG = ctx.pu8PNG;
                            *pcbPNG = ctx.cbPNG;
                            *pcxPNG = cxBitmap;
                            *pcyPNG = cyBitmap;
                            LogFlowFunc(("PNG %d bytes, bitmap %d bytes\n", ctx.cbPNG, cbBitmap));
                        }
                    }
                    else
                    {
                        rc = VERR_GENERAL_FAILURE; /* Something within libpng. */
                    }
                }
                else
                {
                    rc = VERR_NO_MEMORY;
                }

                png_destroy_write_struct(&png_ptr, info_ptr? &info_ptr: png_infopp_NULL);
            }
            else
            {
                rc = VERR_NO_MEMORY;
            }

            RTMemFree(row_pointers);
        }
        else
        {
            rc = VERR_NO_MEMORY;
        }
    }

    if (pu8Bitmap && pu8Bitmap != pu8Data)
    {
        RTMemFree(pu8Bitmap);
    }

    return rc;

}

DECLCALLBACK(void)
Display::displaySSMSaveScreenshot(PSSMHANDLE pSSM, void *pvUser)
{
    Display *that = static_cast<Display*>(pvUser);

    /* 32bpp small RGB image. */
    uint8_t *pu8Thumbnail = NULL;
    uint32_t cbThumbnail = 0;
    uint32_t cxThumbnail = 0;
    uint32_t cyThumbnail = 0;

    /* PNG screenshot. */
    uint8_t *pu8PNG = NULL;
    uint32_t cbPNG = 0;
    uint32_t cxPNG = 0;
    uint32_t cyPNG = 0;

    Console::SafeVMPtr pVM (that->mParent);
    if (SUCCEEDED(pVM.rc()))
    {
        /* Query RGB bitmap. */
        uint8_t *pu8Data = NULL;
        size_t cbData = 0;
        uint32_t cx = 0;
        uint32_t cy = 0;

        /* SSM code is executed on EMT(0), therefore no need to use VMR3ReqCallWait. */
#ifdef VBOX_WITH_OLD_VBVA_LOCK
        int rc = Display::displayTakeScreenshotEMT(that, &pu8Data, &cbData, &cx, &cy);
#else
        int rc = that->mpDrv->pUpPort->pfnTakeScreenshot (that->mpDrv->pUpPort, &pu8Data, &cbData, &cx, &cy);
#endif /* !VBOX_WITH_OLD_VBVA_LOCK */

        if (RT_SUCCESS(rc))
        {
            /* Prepare a small thumbnail and a PNG screenshot. */
            displayMakeThumbnail(pu8Data, cx, cy, &pu8Thumbnail, &cbThumbnail, &cxThumbnail, &cyThumbnail);
            displayMakePNG(pu8Data, cx, cy, &pu8PNG, &cbPNG, &cxPNG, &cyPNG);

            /* This can be called from any thread. */
            that->mpDrv->pUpPort->pfnFreeScreenshot (that->mpDrv->pUpPort, pu8Data);
        }
    }
    else
    {
        LogFunc(("Failed to get VM pointer 0x%x\n", pVM.rc()));
    }

    /* Regardless of rc, save what is available:
     * Data format:
     *    uint32_t cBlocks;
     *    [blocks]
     *
     *  Each block is:
     *    uint32_t cbBlock;        if 0 - no 'block data'.
     *    uint32_t typeOfBlock;    0 - 32bpp RGB bitmap, 1 - PNG, ignored if 'cbBlock' is 0.
     *    [block data]
     *
     *  Block data for bitmap and PNG:
     *    uint32_t cx;
     *    uint32_t cy;
     *    [image data]
     */
    SSMR3PutU32(pSSM, 2); /* Write thumbnail and PNG screenshot. */

    /* First block. */
    SSMR3PutU32(pSSM, cbThumbnail + 2 * sizeof (uint32_t));
    SSMR3PutU32(pSSM, 0); /* Block type: thumbnail. */

    if (cbThumbnail)
    {
        SSMR3PutU32(pSSM, cxThumbnail);
        SSMR3PutU32(pSSM, cyThumbnail);
        SSMR3PutMem(pSSM, pu8Thumbnail, cbThumbnail);
    }

    /* Second block. */
    SSMR3PutU32(pSSM, cbPNG + 2 * sizeof (uint32_t));
    SSMR3PutU32(pSSM, 1); /* Block type: png. */

    if (cbPNG)
    {
        SSMR3PutU32(pSSM, cxPNG);
        SSMR3PutU32(pSSM, cyPNG);
        SSMR3PutMem(pSSM, pu8PNG, cbPNG);
    }

    RTMemFree(pu8PNG);
    RTMemFree(pu8Thumbnail);
}

DECLCALLBACK(int)
Display::displaySSMLoadScreenshot(PSSMHANDLE pSSM, void *pvUser, uint32_t uVersion, uint32_t uPass)
{
    Display *that = static_cast<Display*>(pvUser);

    if (uVersion != sSSMDisplayScreenshotVer)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    /* Skip data. */
    uint32_t cBlocks;
    int rc = SSMR3GetU32(pSSM, &cBlocks);
    AssertRCReturn(rc, rc);

    for (uint32_t i = 0; i < cBlocks; i++)
    {
        uint32_t cbBlock;
        rc = SSMR3GetU32(pSSM, &cbBlock);
        AssertRCBreak(rc);

        uint32_t typeOfBlock;
        rc = SSMR3GetU32(pSSM, &typeOfBlock);
        AssertRCBreak(rc);

        LogFlowFunc(("[%d] type %d, size %d bytes\n", i, typeOfBlock, cbBlock));

        if (cbBlock != 0)
        {
            rc = SSMR3Skip(pSSM, cbBlock);
            AssertRCBreak(rc);
        }
    }

    return rc;
}

/**
 * Save/Load some important guest state
 */
DECLCALLBACK(void)
Display::displaySSMSave(PSSMHANDLE pSSM, void *pvUser)
{
    Display *that = static_cast<Display*>(pvUser);

    SSMR3PutU32(pSSM, that->mcMonitors);
    for (unsigned i = 0; i < that->mcMonitors; i++)
    {
        SSMR3PutU32(pSSM, that->maFramebuffers[i].u32Offset);
        SSMR3PutU32(pSSM, that->maFramebuffers[i].u32MaxFramebufferSize);
        SSMR3PutU32(pSSM, that->maFramebuffers[i].u32InformationSize);
    }
}

DECLCALLBACK(int)
Display::displaySSMLoad(PSSMHANDLE pSSM, void *pvUser, uint32_t uVersion, uint32_t uPass)
{
    Display *that = static_cast<Display*>(pvUser);

    if (uVersion != sSSMDisplayVer)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    uint32_t cMonitors;
    int rc = SSMR3GetU32(pSSM, &cMonitors);
    if (cMonitors != that->mcMonitors)
        return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Number of monitors changed (%d->%d)!"), cMonitors, that->mcMonitors);

    for (uint32_t i = 0; i < cMonitors; i++)
    {
        SSMR3GetU32(pSSM, &that->maFramebuffers[i].u32Offset);
        SSMR3GetU32(pSSM, &that->maFramebuffers[i].u32MaxFramebufferSize);
        SSMR3GetU32(pSSM, &that->maFramebuffers[i].u32InformationSize);
    }

    return VINF_SUCCESS;
}

/**
 * Initializes the display object.
 *
 * @returns COM result indicator
 * @param parent          handle of our parent object
 * @param qemuConsoleData address of common console data structure
 */
HRESULT Display::init (Console *aParent)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet (aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;

    // by default, we have an internal framebuffer which is
    // NULL, i.e. a black hole for no display output
    mFramebufferOpened = false;

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
        memset (&maFramebuffers[ul].pendingResize, 0 , sizeof (maFramebuffers[ul].pendingResize));
#ifdef VBOX_WITH_HGSMI
        maFramebuffers[ul].fVBVAEnabled = false;
        maFramebuffers[ul].cVBVASkipUpdate = 0;
        memset (&maFramebuffers[ul].vbvaSkippedRect, 0, sizeof (maFramebuffers[ul].vbvaSkippedRect));
#endif /* VBOX_WITH_HGSMI */
    }

    mParent->RegisterCallback (this);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void Display::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    ULONG ul;
    for (ul = 0; ul < mcMonitors; ul++)
        maFramebuffers[ul].pFramebuffer = NULL;

    if (mParent)
        mParent->UnregisterCallback (this);

    unconst(mParent).setNull();

    if (mpDrv)
        mpDrv->pDisplay = NULL;

    mpDrv = NULL;
    mpVMMDev = NULL;
    mfVMMDevInited = true;
}

/**
 * Register the SSM methods. Called by the power up thread to be able to
 * pass pVM
 */
int Display::registerSSM(PVM pVM)
{
    int rc = SSMR3RegisterExternal(pVM, "DisplayData", 0, sSSMDisplayVer,
                                   mcMonitors * sizeof(uint32_t) * 3 + sizeof(uint32_t),
                                   NULL, NULL, NULL,
                                   NULL, displaySSMSave, NULL,
                                   NULL, displaySSMLoad, NULL, this);

    AssertRCReturn(rc, rc);

    /*
     * Register loaders for old saved states where iInstance was 3 * sizeof(uint32_t *).
     */
    rc = SSMR3RegisterExternal(pVM, "DisplayData", 12 /*uInstance*/, sSSMDisplayVer, 0 /*cbGuess*/,
                               NULL, NULL, NULL,
                               NULL, NULL, NULL,
                               NULL, displaySSMLoad, NULL, this);
    AssertRCReturn(rc, rc);

    rc = SSMR3RegisterExternal(pVM, "DisplayData", 24 /*uInstance*/, sSSMDisplayVer, 0 /*cbGuess*/,
                               NULL, NULL, NULL,
                               NULL, NULL, NULL,
                               NULL, displaySSMLoad, NULL, this);
    AssertRCReturn(rc, rc);

    /* uInstance is an arbitrary value greater than 1024. Such a value will ensure a quick seek in saved state file. */
    rc = SSMR3RegisterExternal(pVM, "DisplayScreenshot", 1100 /*uInstance*/, sSSMDisplayScreenshotVer, 0 /*cbGuess*/,
                               NULL, NULL, NULL,
                               NULL, displaySSMSaveScreenshot, NULL,
                               NULL, displaySSMLoadScreenshot, NULL, this);

    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

// IConsoleCallback method
STDMETHODIMP Display::OnStateChange(MachineState_T machineState)
{
    if (   machineState == MachineState_Running
        || machineState == MachineState_Teleporting
        || machineState == MachineState_LiveSnapshotting
       )
    {
        LogFlowFunc(("Machine is running.\n"));

        mfMachineRunning = true;
    }
    else
        mfMachineRunning = false;

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
    if (!f)
    {
        /* This could be a result of the screenshot taking call Display::TakeScreenShot:
         * if the framebuffer is processing the resize request and GUI calls the TakeScreenShot
         * and the guest has reprogrammed the virtual VGA devices again so a new resize is required.
         *
         * Save the resize information and return the pending status code.
         *
         * Note: the resize information is only accessed on EMT so no serialization is required.
         */
        LogRel (("Display::handleDisplayResize(): Warning: resize postponed.\n"));

        maFramebuffers[uScreenId].pendingResize.fPending    = true;
        maFramebuffers[uScreenId].pendingResize.pixelFormat = pixelFormat;
        maFramebuffers[uScreenId].pendingResize.pvVRAM      = pvVRAM;
        maFramebuffers[uScreenId].pendingResize.bpp         = bpp;
        maFramebuffers[uScreenId].pendingResize.cbLine      = cbLine;
        maFramebuffers[uScreenId].pendingResize.w           = w;
        maFramebuffers[uScreenId].pendingResize.h           = h;

        return VINF_VGA_RESIZE_IN_PROGRESS;
    }

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

    AssertRelease(!maFramebuffers[uScreenId].pendingResize.fPending);

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

        /* Check whether a resize is pending for this framebuffer. */
        if (pFBInfo->pendingResize.fPending)
        {
            /* Reset the condition, call the display resize with saved data and continue.
             *
             * Note: handleDisplayResize can call handleResizeCompletedEMT back,
             *       but infinite recursion is not possible, because when the handleResizeCompletedEMT
             *       is called, the pFBInfo->pendingResize.fPending is equal to false.
             */
            pFBInfo->pendingResize.fPending = false;
            handleDisplayResize (uScreenId, pFBInfo->pendingResize.bpp, pFBInfo->pendingResize.pvVRAM,
                                 pFBInfo->pendingResize.cbLine, pFBInfo->pendingResize.w, pFBInfo->pendingResize.h);
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
#ifdef VBOX_WITH_OLD_VBVA_LOCK
    /*
     * Always runs under either VBVA lock or, for HGSMI, DevVGA lock.
     * Safe to use VBVA vars and take the framebuffer lock.
     */
#endif /* VBOX_WITH_OLD_VBVA_LOCK */

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

    if (uScreenId == VBOX_VIDEO_PRIMARY_SCREEN)
        checkCoordBounds (&x, &y, &w, &h, mpDrv->Connector.cx, mpDrv->Connector.cy);
    else
        checkCoordBounds (&x, &y, &w, &h, maFramebuffers[uScreenId].w,
                                          maFramebuffers[uScreenId].h);

    if (w != 0 && h != 0)
        pFramebuffer->NotifyUpdate(x, y, w, h);

    pFramebuffer->Unlock();

#ifndef VBOX_WITH_HGSMI
    if (!mfVideoAccelEnabled)
    {
#else
    if (!mfVideoAccelEnabled && !maFramebuffers[uScreenId].fVBVAEnabled)
    {
#endif /* VBOX_WITH_HGSMI */
        /* When VBVA is enabled, the VRDP server is informed in the VideoAccelFlush.
         * Inform the server here only if VBVA is disabled.
         */
        if (maFramebuffers[uScreenId].u32ResizeStatus == ResizeStatus_Void)
            mParent->consoleVRDPServer()->SendUpdateBitmap(uScreenId, x, y, w, h);
    }
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
        prgn->pDisplay->handleDisplayUpdate (phdr->x + pFBInfo->xOrigin,
                                             phdr->y + pFBInfo->yOrigin, phdr->w, phdr->h);
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
        prgn->pDisplay->handleDisplayUpdate (pFBInfo->dirtyRect.xLeft + pFBInfo->xOrigin,
                                             pFBInfo->dirtyRect.yTop + pFBInfo->yOrigin, w, h);
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

#ifdef VBOX_WITH_OLD_VBVA_LOCK
int Display::vbvaLock(void)
{
    return RTCritSectEnter(&mVBVALock);
}

void Display::vbvaUnlock(void)
{
    RTCritSectLeave(&mVBVALock);
}
#endif /* VBOX_WITH_OLD_VBVA_LOCK */
    
/**
 * @thread EMT
 */
#ifdef VBOX_WITH_OLD_VBVA_LOCK
int Display::VideoAccelEnable (bool fEnable, VBVAMEMORY *pVbvaMemory)
{
    int rc;
    vbvaLock();
    rc = videoAccelEnable (fEnable, pVbvaMemory);
    vbvaUnlock();
    return rc;
}
#endif /* VBOX_WITH_OLD_VBVA_LOCK */

#ifdef VBOX_WITH_OLD_VBVA_LOCK
int Display::videoAccelEnable (bool fEnable, VBVAMEMORY *pVbvaMemory)
#else
int Display::VideoAccelEnable (bool fEnable, VBVAMEMORY *pVbvaMemory)
#endif /* !VBOX_WITH_OLD_VBVA_LOCK */
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
#ifdef VBOX_WITH_OLD_VBVA_LOCK
        videoAccelFlush ();
#else
        VideoAccelFlush ();
#endif /* !VBOX_WITH_OLD_VBVA_LOCK */
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

#ifdef VBOX_WITH_OLD_VBVA_LOCK
        mfu32PendingVideoAccelDisable = false;
#endif /* VBOX_WITH_OLD_VBVA_LOCK */

        LogRel(("VBVA: Enabled.\n"));
    }
    else
    {
        LogRel(("VBVA: Disabled.\n"));
    }

    LogFlowFunc (("VideoAccelEnable: rc = %Rrc.\n", rc));

    return rc;
}

#ifdef VBOX_WITH_VRDP
/* Called always by one VRDP server thread. Can be thread-unsafe.
 */
void Display::VideoAccelVRDP (bool fEnable)
{
#ifdef VBOX_WITH_OLD_VBVA_LOCK
    vbvaLock();
#endif /* VBOX_WITH_OLD_VBVA_LOCK */

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
#ifdef VBOX_WITH_OLD_VBVA_LOCK
    vbvaUnlock();
#endif /* VBOX_WITH_OLD_VBVA_LOCK */
}
#endif /* VBOX_WITH_VRDP */

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
#ifdef VBOX_WITH_OLD_VBVA_LOCK
void Display::VideoAccelFlush (void)
{
    vbvaLock();
    videoAccelFlush();
    vbvaUnlock();
}
#endif /* VBOX_WITH_OLD_VBVA_LOCK */

#ifdef VBOX_WITH_OLD_VBVA_LOCK
/* Under VBVA lock. DevVGA is not taken. */
void Display::videoAccelFlush (void)
#else
void Display::VideoAccelFlush (void)
#endif /* !VBOX_WITH_OLD_VBVA_LOCK */
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
#ifndef VBOX_WITH_OLD_VBVA_LOCK
    for (uScreenId = 0; uScreenId < mcMonitors; uScreenId++)
    {
        if (!maFramebuffers[uScreenId].pFramebuffer.isNull())
        {
            maFramebuffers[uScreenId].pFramebuffer->Lock ();
        }
    }
#endif /* !VBOX_WITH_OLD_VBVA_LOCK */

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
#ifdef VBOX_WITH_OLD_VBVA_LOCK
            videoAccelEnable (false, NULL);
#else
            VideoAccelEnable (false, NULL);
#endif /* !VBOX_WITH_OLD_VBVA_LOCK */

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
#ifndef VBOX_WITH_OLD_VBVA_LOCK
        if (!maFramebuffers[uScreenId].pFramebuffer.isNull())
        {
            maFramebuffers[uScreenId].pFramebuffer->Unlock ();
        }
#endif /* !VBOX_WITH_OLD_VBVA_LOCK */

        if (maFramebuffers[uScreenId].u32ResizeStatus == ResizeStatus_Void)
        {
            /* Draw the framebuffer. */
            vbvaRgnUpdateFramebuffer (&rgn, uScreenId);
        }
    }
}

#ifdef VBOX_WITH_OLD_VBVA_LOCK
int Display::videoAccelRefreshProcess(void)
{
    int rc = VWRN_INVALID_STATE; /* Default is to do a display update in VGA device. */

#ifdef DEBUG_sunlover
    LogFlowFunc(("\n"));
#endif

    vbvaLock();

    if (ASMAtomicCmpXchgU32(&mfu32PendingVideoAccelDisable, false, true))
    {
        videoAccelEnable (false, NULL);
    }
    else if (mfPendingVideoAccelEnable)
    {
        /* Acceleration was enabled while machine was not yet running
         * due to restoring from saved state. Update entire display and
         * actually enable acceleration.
         */
        Assert(mpPendingVbvaMemory);

        /* Acceleration can not be yet enabled.*/
        Assert(mpVbvaMemory == NULL);
        Assert(!mfVideoAccelEnabled);

        if (mfMachineRunning)
        {
            videoAccelEnable (mfPendingVideoAccelEnable,
                              mpPendingVbvaMemory);

            /* Reset the pending state. */
            mfPendingVideoAccelEnable = false;
            mpPendingVbvaMemory = NULL;
        }

        rc = VINF_TRY_AGAIN;
    }
    else
    {
        Assert(mpPendingVbvaMemory == NULL);

        if (mfVideoAccelEnabled)
        {
            Assert(mpVbvaMemory);
            videoAccelFlush ();

            rc = VINF_SUCCESS; /* VBVA processed, no need to a display update. */
        }
    }

    vbvaUnlock();

#ifdef DEBUG_sunlover
    LogFlowFunc(("rc = %d\n"));
#endif

    return rc;
}
#endif /* VBOX_WITH_OLD_VBVA_LOCK */


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
    CheckComArgNotNull(width);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

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
    CheckComArgNotNull(height);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

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

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    CHECK_CONSOLE_DRV (mpDrv);

    uint32_t cBits = 0;
    int rc = mpDrv->pUpPort->pfnQueryColorDepth(mpDrv->pUpPort, &cBits);
    AssertRC(rc);
    *bitsPerPixel = cBits;

    return S_OK;
}


// IDisplay methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Display::SetFramebuffer (ULONG aScreenId,
    IFramebuffer *aFramebuffer)
{
    LogFlowFunc (("\n"));

    if (aFramebuffer != NULL)
        CheckComArgOutPointerValid(aFramebuffer);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    Console::SafeVMPtrQuiet pVM (mParent);
    if (pVM.isOk())
    {
        /* Must leave the lock here because the changeFramebuffer will
         * also obtain it. */
        alock.leave ();

        /* send request to the EMT thread */
        int vrc = VMR3ReqCallWait (pVM, VMCPUID_ANY,
                                   (PFNRT) changeFramebuffer, 3, this, aFramebuffer, aScreenId);

        alock.enter ();

        ComAssertRCRet (vrc, E_FAIL);
    }
    else
    {
        /* No VM is created (VM is powered off), do a direct call */
        int vrc = changeFramebuffer (this, aFramebuffer, aScreenId);
        ComAssertRCRet (vrc, E_FAIL);
    }

    return S_OK;
}

STDMETHODIMP Display::GetFramebuffer (ULONG aScreenId,
    IFramebuffer **aFramebuffer, LONG *aXOrigin, LONG *aYOrigin)
{
    LogFlowFunc (("aScreenId = %d\n", aScreenId));

    CheckComArgOutPointerValid(aFramebuffer);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

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

STDMETHODIMP Display::SetVideoModeHint(ULONG aWidth, ULONG aHeight,
    ULONG aBitsPerPixel, ULONG aDisplay)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

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

    /* Have to leave the lock because the pfnRequestDisplayChange
     * will call EMT.  */
    alock.leave ();
    if (mParent->getVMMDev())
        mParent->getVMMDev()->getVMMDevPort()->
            pfnRequestDisplayChange (mParent->getVMMDev()->getVMMDevPort(),
                                     aWidth, aHeight, aBitsPerPixel, aDisplay);
    return S_OK;
}

STDMETHODIMP Display::SetSeamlessMode (BOOL enabled)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    /* Have to leave the lock because the pfnRequestSeamlessChange will call EMT.  */
    alock.leave ();
    if (mParent->getVMMDev())
        mParent->getVMMDev()->getVMMDevPort()->
            pfnRequestSeamlessChange (mParent->getVMMDev()->getVMMDevPort(),
                                      !!enabled);
    return S_OK;
}

#ifdef VBOX_WITH_OLD_VBVA_LOCK
int Display::displayTakeScreenshotEMT(Display *pDisplay, uint8_t **ppu8Data, size_t *pcbData, uint32_t *pu32Width, uint32_t *pu32Height)
{
   int rc;
   pDisplay->vbvaLock();
   rc = pDisplay->mpDrv->pUpPort->pfnTakeScreenshot(pDisplay->mpDrv->pUpPort, ppu8Data, pcbData, pu32Width, pu32Height);
   pDisplay->vbvaUnlock();
   return rc;
}
#endif /* VBOX_WITH_OLD_VBVA_LOCK */

#ifdef VBOX_WITH_OLD_VBVA_LOCK
static int displayTakeScreenshot(PVM pVM, Display *pDisplay, struct DRVMAINDISPLAY *pDrv, BYTE *address, ULONG width, ULONG height)
#else
static int displayTakeScreenshot(PVM pVM, struct DRVMAINDISPLAY *pDrv, BYTE *address, ULONG width, ULONG height)
#endif /* !VBOX_WITH_OLD_VBVA_LOCK */
{
    uint8_t *pu8Data = NULL;
    size_t cbData = 0;
    uint32_t cx = 0;
    uint32_t cy = 0;

#ifdef VBOX_WITH_OLD_VBVA_LOCK
    int vrc = VMR3ReqCallWait(pVM, VMCPUID_ANY, (PFNRT)Display::displayTakeScreenshotEMT, 5,
                              pDisplay, &pu8Data, &cbData, &cx, &cy);
#else
    /* @todo pfnTakeScreenshot is probably callable from any thread, because it uses the VGA device lock. */
    int vrc = VMR3ReqCallWait(pVM, VMCPUID_ANY, (PFNRT)pDrv->pUpPort->pfnTakeScreenshot, 5,
                              pDrv->pUpPort, &pu8Data, &cbData, &cx, &cy);
#endif /* !VBOX_WITH_OLD_VBVA_LOCK */

    if (RT_SUCCESS(vrc))
    {
        if (cx == width && cy == height)
        {
            /* No scaling required. */
            memcpy(address, pu8Data, cbData);
        }
        else
        {
            /* Scale. */
            LogFlowFunc(("SCALE: %dx%d -> %dx%d\n", cx, cy, width, height));

            uint8_t *dst = address;
            uint8_t *src = pu8Data;
            int dstX = 0;
            int dstY = 0;
            int srcX = 0;
            int srcY = 0;
            int dstW = width;
            int dstH = height;
            int srcW = cx;
            int srcH = cy;
            gdImageCopyResampled (dst,
                                  src,
                                  dstX, dstY,
                                  srcX, srcY,
                                  dstW, dstH, srcW, srcH);
        }

        /* This can be called from any thread. */
        pDrv->pUpPort->pfnFreeScreenshot (pDrv->pUpPort, pu8Data);
    }

    return vrc;
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

    CheckComArgNotNull(address);
    CheckComArgExpr(width, width != 0);
    CheckComArgExpr(height, height != 0);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    CHECK_CONSOLE_DRV (mpDrv);

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC(pVM.rc());

    HRESULT rc = S_OK;

    LogFlowFunc (("Sending SCREENSHOT request\n"));

    /* Leave lock because other thread (EMT) is called and it may initiate a resize
     * which also needs lock.
     *
     * This method does not need the lock anymore.
     */
    alock.leave();

#ifdef VBOX_WITH_OLD_VBVA_LOCK
    int vrc = displayTakeScreenshot(pVM, this, mpDrv, address, width, height);
#else
    int vrc = displayTakeScreenshot(pVM, mpDrv, address, width, height);
#endif /* !VBOX_WITH_OLD_VBVA_LOCK */

    if (vrc == VERR_NOT_IMPLEMENTED)
        rc = setError (E_NOTIMPL,
                       tr ("This feature is not implemented"));
    else if (RT_FAILURE(vrc))
        rc = setError (VBOX_E_IPRT_ERROR,
                       tr ("Could not take a screenshot (%Rrc)"), vrc);

    LogFlowFunc (("rc=%08X\n", rc));
    LogFlowFuncLeave();
    return rc;
}

STDMETHODIMP Display::TakeScreenShotSlow (ULONG width, ULONG height,
                                          ComSafeArrayOut(BYTE, aScreenData))
{
    LogFlowFuncEnter();
    LogFlowFunc (("width=%d, height=%d\n",
                  width, height));

    CheckComArgSafeArrayNotNull(aScreenData);
    CheckComArgExpr(width, width != 0);
    CheckComArgExpr(height, height != 0);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    CHECK_CONSOLE_DRV (mpDrv);

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC(pVM.rc());

    HRESULT rc = S_OK;

    LogFlowFunc (("Sending SCREENSHOT request\n"));

    /* Leave lock because other thread (EMT) is called and it may initiate a resize
     * which also needs lock.
     *
     * This method does not need the lock anymore.
     */
    alock.leave();

    size_t cbData = width * 4 * height;
    uint8_t *pu8Data = (uint8_t *)RTMemAlloc(cbData);

    if (!pu8Data)
        return E_OUTOFMEMORY;

#ifdef VBOX_WITH_OLD_VBVA_LOCK
    int vrc = displayTakeScreenshot(pVM, this, mpDrv, pu8Data, width, height);
#else
    int vrc = displayTakeScreenshot(pVM, mpDrv, pu8Data, width, height);
#endif /* !VBOX_WITH_OLD_VBVA_LOCK */

    if (RT_SUCCESS(vrc))
    {
        /* Convert pixels to format expected by the API caller: [0] R, [1] G, [2] B, [3] A. */
        uint8_t *pu8 = pu8Data;
        unsigned cPixels = width * height;
        while (cPixels)
        {
            uint8_t u8 = pu8[0];
            pu8[0] = pu8[2];
            pu8[2] = u8;
            pu8[3] = 0xff;
            cPixels--;
            pu8 += 4;
        }

        com::SafeArray<BYTE> screenData (cbData);
        for (unsigned i = 0; i < cbData; i++)
            screenData[i] = pu8Data[i];
        screenData.detachTo(ComSafeArrayOutArg(aScreenData));
    }
    else if (vrc == VERR_NOT_IMPLEMENTED)
        rc = setError (E_NOTIMPL,
                       tr ("This feature is not implemented"));
    else
        rc = setError (VBOX_E_IPRT_ERROR,
                       tr ("Could not take a screenshot (%Rrc)"), vrc);

    LogFlowFunc (("rc=%08X\n", rc));
    LogFlowFuncLeave();
    return rc;
}

#ifdef VBOX_WITH_OLD_VBVA_LOCK
int Display::DrawToScreenEMT(Display *pDisplay, BYTE *address, ULONG x, ULONG y, ULONG width, ULONG height)
{
    int rc;
    pDisplay->vbvaLock();
    rc = pDisplay->mpDrv->pUpPort->pfnDisplayBlt(pDisplay->mpDrv->pUpPort, address, x, y, width, height);
    pDisplay->vbvaUnlock();
    return rc;
}
#endif /* VBOX_WITH_OLD_VBVA_LOCK */

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
                  (void *)address, x, y, width, height));

    CheckComArgNotNull(address);
    CheckComArgExpr(width, width != 0);
    CheckComArgExpr(height, height != 0);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    CHECK_CONSOLE_DRV (mpDrv);

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC(pVM.rc());

    /*
     * Again we're lazy and make the graphics device do all the
     * dirty conversion work.
     */
#ifdef VBOX_WITH_OLD_VBVA_LOCK
    int rcVBox = VMR3ReqCallWait(pVM, VMCPUID_ANY, (PFNRT)Display::DrawToScreenEMT, 6,
                                 this, address, x, y, width, height);
#else
    int rcVBox = VMR3ReqCallWait(pVM, VMCPUID_ANY, (PFNRT)mpDrv->pUpPort->pfnDisplayBlt, 6,
                                 mpDrv->pUpPort, address, x, y, width, height);
#endif /* !VBOX_WITH_OLD_VBVA_LOCK */

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
    else if (RT_FAILURE(rcVBox))
        rc = setError (VBOX_E_IPRT_ERROR,
            tr ("Could not draw to the screen (%Rrc)"), rcVBox);
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

#ifdef VBOX_WITH_OLD_VBVA_LOCK
void Display::InvalidateAndUpdateEMT(Display *pDisplay)
{
    pDisplay->vbvaLock();
    pDisplay->mpDrv->pUpPort->pfnUpdateDisplayAll(pDisplay->mpDrv->pUpPort);
    pDisplay->vbvaUnlock();
}
#endif /* VBOX_WITH_OLD_VBVA_LOCK */

/**
 * Does a full invalidation of the VM display and instructs the VM
 * to update it immediately.
 *
 * @returns COM status code
 */
STDMETHODIMP Display::InvalidateAndUpdate()
{
    LogFlowFuncEnter();

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    CHECK_CONSOLE_DRV (mpDrv);

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC(pVM.rc());

    HRESULT rc = S_OK;

    LogFlowFunc (("Sending DPYUPDATE request\n"));

    /* Have to leave the lock when calling EMT.  */
    alock.leave ();

    /* pdm.h says that this has to be called from the EMT thread */
#ifdef VBOX_WITH_OLD_VBVA_LOCK
    int rcVBox = VMR3ReqCallVoidWait(pVM, VMCPUID_ANY, (PFNRT)Display::InvalidateAndUpdateEMT,
                                     1, this);
#else
    int rcVBox = VMR3ReqCallVoidWait(pVM, VMCPUID_ANY,
                                     (PFNRT)mpDrv->pUpPort->pfnUpdateDisplayAll, 1, mpDrv->pUpPort);
#endif /* !VBOX_WITH_OLD_VBVA_LOCK */
    alock.enter ();

    if (RT_FAILURE(rcVBox))
        rc = setError (VBOX_E_IPRT_ERROR,
            tr ("Could not invalidate and update the screen (%Rrc)"), rcVBox);

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

    /// @todo (dmik) can we AutoWriteLock alock(this); here?
    //  do it when we switch this class to VirtualBoxBase_NEXT.
    //  This will require general code review and may add some details.
    //  In particular, we may want to check whether EMT is really waiting for
    //  this notification, etc. It might be also good to obey the caller to make
    //  sure this method is not called from more than one thread at a time
    //  (and therefore don't use Display lock at all here to save some
    //  milliseconds).
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* this is only valid for external framebuffers */
    if (maFramebuffers[aScreenId].pFramebuffer == NULL)
        return setError (VBOX_E_NOT_SUPPORTED,
            tr ("Resize completed notification is valid only "
                "for external framebuffers"));

    /* Set the flag indicating that the resize has completed and display
     * data need to be updated. */
    bool f = ASMAtomicCmpXchgU32 (&maFramebuffers[aScreenId].u32ResizeStatus,
        ResizeStatus_UpdateDisplayData, ResizeStatus_InProgress);
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

    /// @todo (dmik) can we AutoWriteLock alock(this); here?
    //  do it when we switch this class to VirtualBoxBase_NEXT.
    //  Tthis will require general code review and may add some details.
    //  In particular, we may want to check whether EMT is really waiting for
    //  this notification, etc. It might be also good to obey the caller to make
    //  sure this method is not called from more than one thread at a time
    //  (and therefore don't use Display lock at all here to save some
    //  milliseconds).
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* this is only valid for external framebuffers */
    if (maFramebuffers[VBOX_VIDEO_PRIMARY_SCREEN].pFramebuffer == NULL)
        return setError (VBOX_E_NOT_SUPPORTED,
            tr ("Resize completed notification is valid only "
                "for external framebuffers"));

    return S_OK;
}

STDMETHODIMP Display::CompleteVHWACommand(BYTE *pCommand)
{
#ifdef VBOX_WITH_VIDEOHWACCEL
    mpDrv->pVBVACallbacks->pfnVHWACommandCompleteAsynch(mpDrv->pVBVACallbacks, (PVBOXVHWACMD)pCommand);
    return S_OK;
#else
    return E_NOTIMPL;
#endif
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
                                              unsigned uScreenId)
{
    LogFlowFunc (("uScreenId = %d\n", uScreenId));

    AssertReturn(that, VERR_INVALID_PARAMETER);
    AssertReturn(uScreenId < that->mcMonitors, VERR_INVALID_PARAMETER);

    AutoCaller autoCaller(that);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(that);

    DISPLAYFBINFO *pDisplayFBInfo = &that->maFramebuffers[uScreenId];
    pDisplayFBInfo->pFramebuffer = aFB;

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
    bool fNoUpdate = false; /* Do not update the display if any of the framebuffers is being resized. */
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
            fNoUpdate = true; /* Always set it here, because pfnUpdateDisplayAll can cause a new resize. */
            /* The framebuffer was resized and display data need to be updated. */
            pDisplay->handleResizeCompletedEMT ();
            if (pFBInfo->u32ResizeStatus != ResizeStatus_Void)
            {
                /* The resize status could be not Void here because a pending resize is issued. */
                continue;
            }
            /* Continue with normal processing because the status here is ResizeStatus_Void. */
            if (uScreenId == VBOX_VIDEO_PRIMARY_SCREEN)
            {
                /* Repaint the display because VM continued to run during the framebuffer resize. */
                if (!pFBInfo->pFramebuffer.isNull())
#ifdef VBOX_WITH_OLD_VBVA_LOCK
                {
                    pDisplay->vbvaLock();
#endif /* VBOX_WITH_OLD_VBVA_LOCK */
                    pDrv->pUpPort->pfnUpdateDisplayAll(pDrv->pUpPort);
#ifdef VBOX_WITH_OLD_VBVA_LOCK
                    pDisplay->vbvaUnlock();
                }
#endif /* VBOX_WITH_OLD_VBVA_LOCK */
            }
        }
        else if (u32ResizeStatus == ResizeStatus_InProgress)
        {
            /* The framebuffer is being resized. Do not call the VGA device back. Immediately return. */
            LogFlowFunc (("ResizeStatus_InProcess\n"));
            fNoUpdate = true;
            continue;
        }
    }

    if (!fNoUpdate)
    {
#ifdef VBOX_WITH_OLD_VBVA_LOCK
        int rc = pDisplay->videoAccelRefreshProcess();

        if (rc != VINF_TRY_AGAIN) /* Means 'do nothing' here. */
        {
            if (rc == VWRN_INVALID_STATE)
            {
                /* No VBVA do a display update. */
                DISPLAYFBINFO *pFBInfo = &pDisplay->maFramebuffers[VBOX_VIDEO_PRIMARY_SCREEN];
                if (!pFBInfo->pFramebuffer.isNull() && pFBInfo->u32ResizeStatus == ResizeStatus_Void)
                {
                    Assert(pDrv->Connector.pu8Data);
                    pDisplay->vbvaLock();
                    pDrv->pUpPort->pfnUpdateDisplay(pDrv->pUpPort);
                    pDisplay->vbvaUnlock();
                }
            }

            /* Inform the VRDP server that the current display update sequence is
             * completed. At this moment the framebuffer memory contains a definite
             * image, that is synchronized with the orders already sent to VRDP client.
             * The server can now process redraw requests from clients or initial
             * fullscreen updates for new clients.
             */
            for (uScreenId = 0; uScreenId < pDisplay->mcMonitors; uScreenId++)
            {
                DISPLAYFBINFO *pFBInfo = &pDisplay->maFramebuffers[uScreenId];

                if (!pFBInfo->pFramebuffer.isNull() && pFBInfo->u32ResizeStatus == ResizeStatus_Void)
                {
                    Assert (pDisplay->mParent && pDisplay->mParent->consoleVRDPServer());
                    pDisplay->mParent->consoleVRDPServer()->SendUpdate (uScreenId, NULL, 0);
                }
            }
        }
#else
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
                DISPLAYFBINFO *pFBInfo = &pDisplay->maFramebuffers[VBOX_VIDEO_PRIMARY_SCREEN];
                if (!pFBInfo->pFramebuffer.isNull())
                {
                    Assert(pDrv->Connector.pu8Data);
                    Assert(pFBInfo->u32ResizeStatus == ResizeStatus_Void);
                    pDrv->pUpPort->pfnUpdateDisplay(pDrv->pUpPort);
                }
            }

            /* Inform the VRDP server that the current display update sequence is
             * completed. At this moment the framebuffer memory contains a definite
             * image, that is synchronized with the orders already sent to VRDP client.
             * The server can now process redraw requests from clients or initial
             * fullscreen updates for new clients.
             */
            for (uScreenId = 0; uScreenId < pDisplay->mcMonitors; uScreenId++)
            {
                DISPLAYFBINFO *pFBInfo = &pDisplay->maFramebuffers[uScreenId];

                if (!pFBInfo->pFramebuffer.isNull() && pFBInfo->u32ResizeStatus == ResizeStatus_Void)
                {
                    Assert (pDisplay->mParent && pDisplay->mParent->consoleVRDPServer());
                    pDisplay->mParent->consoleVRDPServer()->SendUpdate (uScreenId, NULL, 0);
                }
            }
        }
#endif /* !VBOX_WITH_OLD_VBVA_LOCK */
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
#ifdef VBOX_WITH_OLD_VBVA_LOCK
    /* This is called under DevVGA lock. Postpone disabling VBVA, do it in the refresh timer. */
    ASMAtomicWriteU32(&pDrv->pDisplay->mfu32PendingVideoAccelDisable, true);
#else
    pDrv->pDisplay->VideoAccelEnable (false, NULL);
#endif /* !VBOX_WITH_OLD_VBVA_LOCK */
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
#ifndef VBOX_WITH_HGSMI
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
             else if (pHdr->u8Type != VBOX_VIDEO_INFO_TYPE_NV_HEAP) /** @todo why is Additions/WINNT/Graphics/Miniport/VBoxVideo.cpp pushing this to us? */
             {
                 LogRel(("Guest adapter information contains unsupported type %d. The block has been skipped.\n", pHdr->u8Type));
             }

             pu8 += pHdr->u16Length;
         }
    }
#endif /* !VBOX_WITH_HGSMI */
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

    /* Get the display information structure. */
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

#ifdef VBOX_WITH_VIDEOHWACCEL

void Display::handleVHWACommandProcess(PPDMIDISPLAYCONNECTOR pInterface, PVBOXVHWACMD pCommand)
{
    unsigned id = (unsigned)pCommand->iDisplay;
    int rc = VINF_SUCCESS;
    if(id < mcMonitors)
    {
        IFramebuffer *pFramebuffer = maFramebuffers[id].pFramebuffer;

        if (pFramebuffer != NULL)
        {
            pFramebuffer->Lock();

            HRESULT hr = pFramebuffer->ProcessVHWACommand((BYTE*)pCommand);
            if(FAILED(hr))
            {
                rc = (hr == E_NOTIMPL) ? VERR_NOT_IMPLEMENTED : VERR_GENERAL_FAILURE;
            }

            pFramebuffer->Unlock();
        }
        else
        {
            rc = VERR_NOT_IMPLEMENTED;
        }
    }
    else
    {
        rc = VERR_INVALID_PARAMETER;
    }

    if(RT_FAILURE(rc))
    {
        /* tell the guest the command is complete */
        pCommand->Flags &= (~VBOXVHWACMD_FLAG_HG_ASYNCH);
        pCommand->rc = rc;
    }
}

DECLCALLBACK(void) Display::displayVHWACommandProcess(PPDMIDISPLAYCONNECTOR pInterface, PVBOXVHWACMD pCommand)
{
    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);

    pDrv->pDisplay->handleVHWACommandProcess(pInterface, pCommand);
}
#endif

#ifdef VBOX_WITH_HGSMI
DECLCALLBACK(int) Display::displayVBVAEnable(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId)
{
    LogFlowFunc(("uScreenId %d\n", uScreenId));

    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);
    Display *pThis = pDrv->pDisplay;

    pThis->maFramebuffers[uScreenId].fVBVAEnabled = true;

    return VINF_SUCCESS;
}

DECLCALLBACK(void) Display::displayVBVADisable(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId)
{
    LogFlowFunc(("uScreenId %d\n", uScreenId));

    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);
    Display *pThis = pDrv->pDisplay;

    pThis->maFramebuffers[uScreenId].fVBVAEnabled = false;
}

DECLCALLBACK(void) Display::displayVBVAUpdateBegin(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId)
{
    LogFlowFunc(("uScreenId %d\n", uScreenId));

    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);
    Display *pThis = pDrv->pDisplay;
    DISPLAYFBINFO *pFBInfo = &pThis->maFramebuffers[uScreenId];

    if (RT_LIKELY(pFBInfo->u32ResizeStatus == ResizeStatus_Void))
    {
        if (RT_UNLIKELY(pFBInfo->cVBVASkipUpdate != 0))
        {
            /* Some updates were skipped. Note: displayVBVAUpdate* callbacks are called
             * under display device lock, so thread safe.
             */
            pFBInfo->cVBVASkipUpdate = 0;
            pThis->handleDisplayUpdate(pFBInfo->vbvaSkippedRect.xLeft,
                                       pFBInfo->vbvaSkippedRect.yTop,
                                       pFBInfo->vbvaSkippedRect.xRight - pFBInfo->vbvaSkippedRect.xLeft,
                                       pFBInfo->vbvaSkippedRect.yBottom - pFBInfo->vbvaSkippedRect.yTop);
        }
    }
    else
    {
        /* The framebuffer is being resized. */
        pFBInfo->cVBVASkipUpdate++;
    }
}

DECLCALLBACK(void) Display::displayVBVAUpdateProcess(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId, const PVBVACMDHDR pCmd, size_t cbCmd)
{
    LogFlowFunc(("uScreenId %d pCmd %p cbCmd %d\n", uScreenId, pCmd, cbCmd));

    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);
    Display *pThis = pDrv->pDisplay;
    DISPLAYFBINFO *pFBInfo = &pThis->maFramebuffers[uScreenId];

    if (RT_LIKELY(pFBInfo->cVBVASkipUpdate == 0))
    {
         pThis->mParent->consoleVRDPServer()->SendUpdate (uScreenId, pCmd, cbCmd);
    }
}

DECLCALLBACK(void) Display::displayVBVAUpdateEnd(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId, int32_t x, int32_t y, uint32_t cx, uint32_t cy)
{
    LogFlowFunc(("uScreenId %d %d,%d %dx%d\n", uScreenId, x, y, cx, cy));

    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);
    Display *pThis = pDrv->pDisplay;
    DISPLAYFBINFO *pFBInfo = &pThis->maFramebuffers[uScreenId];

    /* @todo handleFramebufferUpdate (uScreenId,
     *                                x - pThis->maFramebuffers[uScreenId].xOrigin,
     *                                y - pThis->maFramebuffers[uScreenId].yOrigin,
     *                                cx, cy);
     */
    if (RT_LIKELY(pFBInfo->cVBVASkipUpdate == 0))
    {
        pThis->handleDisplayUpdate(x, y, cx, cy);
    }
    else
    {
        /* Save the updated rectangle. */
        int32_t xRight = x + cx;
        int32_t yBottom = y + cy;

        if (pFBInfo->cVBVASkipUpdate == 1)
        {
            pFBInfo->vbvaSkippedRect.xLeft = x;
            pFBInfo->vbvaSkippedRect.yTop = y;
            pFBInfo->vbvaSkippedRect.xRight = xRight;
            pFBInfo->vbvaSkippedRect.yBottom = yBottom;
        }
        else
        {
            if (pFBInfo->vbvaSkippedRect.xLeft > x)
            {
                pFBInfo->vbvaSkippedRect.xLeft = x;
            }
            if (pFBInfo->vbvaSkippedRect.yTop > y)
            {
                pFBInfo->vbvaSkippedRect.yTop = y;
            }
            if (pFBInfo->vbvaSkippedRect.xRight < xRight)
            {
                pFBInfo->vbvaSkippedRect.xRight = xRight;
            }
            if (pFBInfo->vbvaSkippedRect.yBottom < yBottom)
            {
                pFBInfo->vbvaSkippedRect.yBottom = yBottom;
            }
        }
    }
}

DECLCALLBACK(int) Display::displayVBVAResize(PPDMIDISPLAYCONNECTOR pInterface, const PVBVAINFOVIEW pView, const PVBVAINFOSCREEN pScreen, void *pvVRAM)
{
    LogFlowFunc(("pScreen %p, pvVRAM %p\n", pScreen, pvVRAM));

    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);
    Display *pThis = pDrv->pDisplay;

    DISPLAYFBINFO *pFBInfo = &pThis->maFramebuffers[pScreen->u32ViewIndex];

    pFBInfo->u32Offset = pView->u32ViewOffset; /* Not used in HGSMI. */
    pFBInfo->u32MaxFramebufferSize = pView->u32MaxScreenSize; /* Not used in HGSMI. */
    pFBInfo->u32InformationSize = 0; /* Not used in HGSMI. */

    pFBInfo->xOrigin = pScreen->i32OriginX;
    pFBInfo->yOrigin = pScreen->i32OriginY;

    pFBInfo->w = pScreen->u32Width;
    pFBInfo->h = pScreen->u32Height;

    return pThis->handleDisplayResize(pScreen->u32ViewIndex, pScreen->u16BitsPerPixel,
                                      (uint8_t *)pvVRAM + pScreen->u32StartOffset,
                                      pScreen->u32LineSize, pScreen->u32Width, pScreen->u32Height);
}

DECLCALLBACK(int) Display::displayVBVAMousePointerShape(PPDMIDISPLAYCONNECTOR pInterface, bool fVisible, bool fAlpha,
                                                        uint32_t xHot, uint32_t yHot,
                                                        uint32_t cx, uint32_t cy,
                                                        const void *pvShape)
{
    LogFlowFunc(("\n"));

    PDRVMAINDISPLAY pDrv = PDMIDISPLAYCONNECTOR_2_MAINDISPLAY(pInterface);
    Display *pThis = pDrv->pDisplay;

    /* Tell the console about it */
    pDrv->pDisplay->mParent->onMousePointerShapeChange(fVisible, fAlpha,
                                                       xHot, yHot, cx, cy, (void *)pvShape);

    return VINF_SUCCESS;
}
#endif /* VBOX_WITH_HGSMI */

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
    PDRVMAINDISPLAY pDrv = PDMINS_2_DATA(pDrvIns, PDRVMAINDISPLAY);
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
    PDRVMAINDISPLAY pData = PDMINS_2_DATA(pDrvIns, PDRVMAINDISPLAY);
    LogFlowFunc (("iInstance=%d\n", pDrvIns->iInstance));
    if (pData->pDisplay)
    {
        AutoWriteLock displayLock (pData->pDisplay);
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
 * @copydoc FNPDMDRVCONSTRUCT
 */
DECLCALLBACK(int) Display::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle, uint32_t fFlags)
{
    PDRVMAINDISPLAY pData = PDMINS_2_DATA(pDrvIns, PDRVMAINDISPLAY);
    LogFlowFunc (("iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Object\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;
    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /*
     * Init Interfaces.
     */
    pDrvIns->IBase.pfnQueryInterface       = Display::drvQueryInterface;

    pData->Connector.pfnResize             = Display::displayResizeCallback;
    pData->Connector.pfnUpdateRect         = Display::displayUpdateCallback;
    pData->Connector.pfnRefresh            = Display::displayRefreshCallback;
    pData->Connector.pfnReset              = Display::displayResetCallback;
    pData->Connector.pfnLFBModeChange      = Display::displayLFBModeChangeCallback;
    pData->Connector.pfnProcessAdapterData = Display::displayProcessAdapterDataCallback;
    pData->Connector.pfnProcessDisplayData = Display::displayProcessDisplayDataCallback;
#ifdef VBOX_WITH_VIDEOHWACCEL
    pData->Connector.pfnVHWACommandProcess = Display::displayVHWACommandProcess;
#endif
#ifdef VBOX_WITH_HGSMI
    pData->Connector.pfnVBVAEnable         = Display::displayVBVAEnable;
    pData->Connector.pfnVBVADisable        = Display::displayVBVADisable;
    pData->Connector.pfnVBVAUpdateBegin    = Display::displayVBVAUpdateBegin;
    pData->Connector.pfnVBVAUpdateProcess  = Display::displayVBVAUpdateProcess;
    pData->Connector.pfnVBVAUpdateEnd      = Display::displayVBVAUpdateEnd;
    pData->Connector.pfnVBVAResize         = Display::displayVBVAResize;
    pData->Connector.pfnVBVAMousePointerShape = Display::displayVBVAMousePointerShape;
#endif


    /*
     * Get the IDisplayPort interface of the above driver/device.
     */
    pData->pUpPort = (PPDMIDISPLAYPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_DISPLAY_PORT);
    if (!pData->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No display port interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }
#if defined(VBOX_WITH_VIDEOHWACCEL)
    pData->pVBVACallbacks = (PPDMDDISPLAYVBVACALLBACKS)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_DISPLAY_VBVA_CALLBACKS);
    if (!pData->pVBVACallbacks)
    {
        AssertMsgFailed(("Configuration error: No VBVA callback interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }
#endif
    /*
     * Get the Display object pointer and update the mpDrv member.
     */
    void *pv;
    int rc = CFGMR3QueryPtr(pCfgHandle, "Object", &pv);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: No/bad \"Object\" value! rc=%Rrc\n", rc));
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
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
