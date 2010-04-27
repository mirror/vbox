/** @file
 * VirtualBox X11 Additions graphics driver utility functions
 */

/*
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

#include <VBox/VMMDev.h>
#include <VBox/VBoxGuestLib.h>

#ifndef PCIACCESS
# include <xf86Pci.h>
# include <Pci.h>
#endif

#include "xf86.h"
#define NEED_XF86_TYPES
#ifdef NO_ANSIC
# include <string.h>
#else
# include "xf86_ansic.h"
#endif
#include "compiler.h"
#include "cursorstr.h"

#include "vboxvideo.h"

#define VBOX_MAX_CURSOR_WIDTH 64
#define VBOX_MAX_CURSOR_HEIGHT 64

/**************************************************************************
* Debugging functions and macros                                          *
**************************************************************************/

/* #define DEBUG_POINTER */

#ifdef DEBUG
# define PUT_PIXEL(c) ErrorF ("%c", c)
#else /* DEBUG_VIDEO not defined */
# define PUT_PIXEL(c) do { } while(0)
#endif /* DEBUG_VIDEO not defined */

/** Macro to printf an error message and return from a function */
#define RETERROR(scrnIndex, RetVal, ...) \
    do \
    { \
        xf86DrvMsg(scrnIndex, X_ERROR, __VA_ARGS__); \
        return RetVal; \
    } \
    while (0)

#ifdef DEBUG_POINTER
static void
vbox_show_shape(unsigned short w, unsigned short h, CARD32 bg, unsigned char *image)
{
    size_t x, y;
    unsigned short pitch;
    CARD32 *color;
    unsigned char *mask;
    size_t sizeMask;

    image    += offsetof(VMMDevReqMousePointer, pointerData);
    mask      = image;
    pitch     = (w + 7) / 8;
    sizeMask  = (pitch * h + 3) & ~3;
    color     = (CARD32 *)(image + sizeMask);

    TRACE_ENTRY();
    for (y = 0; y < h; ++y, mask += pitch, color += w)
    {
        for (x = 0; x < w; ++x)
        {
            if (mask[x / 8] & (1 << (7 - (x % 8))))
                ErrorF (" ");
            else
            {
                CARD32 c = color[x];
                if (c == bg)
                    ErrorF("Y");
                else
                    ErrorF("X");
            }
        }
        ErrorF("\n");
    }
}
#endif

/**************************************************************************
* Helper functions and macros                                             *
**************************************************************************/

/* This is called by the X server every time it loads a new cursor to see
 * whether our "cursor hardware" can handle the cursor.  This provides us with
 * a mechanism (the only one!) to switch back from a software to a hardware
 * cursor. */
static Bool
vbox_host_uses_hwcursor(ScrnInfoPtr pScrn)
{
    Bool rc = TRUE;
    uint32_t fFeatures = 0;
    VBOXPtr pVBox = pScrn->driverPrivate;

    TRACE_ENTRY();
    /* We may want to force the use of a software cursor.  Currently this is
     * needed if the guest uses a large virtual resolution, as in this case
     * the host and guest tend to disagree about the pointer location. */
    if (pVBox->forceSWCursor)
        rc = FALSE;
    /* Query information about mouse integration from the host. */
    if (rc) {
        int vrc = VbglR3GetMouseStatus(&fFeatures, NULL, NULL);
        if (RT_FAILURE(vrc)) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                     "Unable to determine whether the virtual machine supports mouse pointer integration - request initialization failed with return code %d\n", vrc);
            rc = FALSE;
        }
    }
    /* If we got the information from the host then make sure the host wants
     * to draw the pointer. */
    if (rc)
    {
        if (   (fFeatures & VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE)
#if GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) >= 5
                /* As of this version (server 1.6) all major Linux releases
                 * are known to handle USB tablets correctly. */
            || (fFeatures & VMMDEV_MOUSE_HOST_HAS_ABS_DEV)
#endif
            )
            /* Assume this will never be unloaded as long as the X session is
             * running. */
            pVBox->guestCanAbsolute = TRUE;
        if (   (fFeatures & VMMDEV_MOUSE_HOST_CANNOT_HWPOINTER)
            || !pVBox->guestCanAbsolute
            || !(fFeatures & VMMDEV_MOUSE_HOST_CAN_ABSOLUTE)
           )
            rc = FALSE;
    }
    TRACE_LOG("rc=%s\n", BOOL_STR(rc));
    return rc;
}

/**
 * Macro to disable VBVA extensions and return, for use when an
 * unexplained error occurs.
 */
#define DISABLE_VBVA_AND_RETURN(pScrn, ...) \
    do \
    { \
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, __VA_ARGS__); \
        vboxDisableVbva(pScrn); \
        pVBox->useVbva = FALSE; \
        return; \
    } \
    while (0)

/**************************************************************************
* Main functions                                                          *
**************************************************************************/

void
vbox_close(ScrnInfoPtr pScrn, VBOXPtr pVBox)
{
    TRACE_ENTRY();

    xfree (pVBox->reqp);
    pVBox->reqp = NULL;
    TRACE_EXIT();
}

/**
 * Callback function called by the X server to tell us about dirty
 * rectangles in the video buffer.
 *
 * @param pScreen pointer to the information structure for the current
 *                screen
 * @param iRects  Number of dirty rectangles to update
 * @param aRects  Array of structures containing the coordinates of the
 *                rectangles
 */
static void
vboxHandleDirtyRect(ScrnInfoPtr pScrn, int iRects, BoxPtr aRects)
{
    VBVACMDHDR cmdHdr;
    VBOXPtr pVBox;
    VBVARECORD *pRecord;
    VBVAMEMORY *pMem;
    CARD32 indexRecordNext;
    CARD32 off32Data;
    CARD32 off32Free;
    INT32 i32Diff;
    CARD32 cbHwBufferAvail;
    int scrnIndex;
    int i;

    pVBox = pScrn->driverPrivate;
    if (pVBox->useVbva == FALSE)
        return;
    pMem = pVBox->pVbvaMemory;
    /* Just return quietly if VBVA is not currently active. */
    if ((pMem->fu32ModeFlags & VBVA_F_MODE_ENABLED) == 0)
        return;
    scrnIndex = pScrn->scrnIndex;

    for (i = 0; i < iRects; i++)
    {
        cmdHdr.x = (int16_t)aRects[i].x1 - pVBox->viewportX;
        cmdHdr.y = (int16_t)aRects[i].y1 - pVBox->viewportY;
        cmdHdr.w = (uint16_t)(aRects[i].x2 - aRects[i].x1);
        cmdHdr.h = (uint16_t)(aRects[i].y2 - aRects[i].y1);

        /* Get the active record and move the pointer along */
        indexRecordNext = (pMem->indexRecordFree + 1) % VBVA_MAX_RECORDS;
        if (indexRecordNext == pMem->indexRecordFirst)
        {
            /* All slots in the records queue are used. */
            if (VbglR3VideoAccelFlush() < 0)
                DISABLE_VBVA_AND_RETURN(pScrn,
                    "Unable to clear the VirtualBox graphics acceleration queue "
                    "- the request to the virtual machine failed.  Switching to "
                    "unaccelerated mode.\n");
        }
        if (indexRecordNext == pMem->indexRecordFirst)
            DISABLE_VBVA_AND_RETURN(pScrn,
                "Failed to clear the VirtualBox graphics acceleration queue.  "
                "Switching to unaccelerated mode.\n");
        pRecord = &pMem->aRecords[pMem->indexRecordFree];
        /* Mark the record as being updated. */
        pRecord->cbRecord = VBVA_F_RECORD_PARTIAL;
        pMem->indexRecordFree = indexRecordNext;
        /* Compute how many bytes we have in the ring buffer. */
        off32Free = pMem->off32Free;
        off32Data = pMem->off32Data;
        /* Free is writing position. Data is reading position.
         * Data == Free means buffer is free.
         * There must be always gap between free and data when data
         * are in the buffer.
         * Guest only changes free, host only changes data.
         */
        i32Diff = off32Data - off32Free;
        cbHwBufferAvail = i32Diff > 0 ? i32Diff : VBVA_RING_BUFFER_SIZE + i32Diff;
        if (cbHwBufferAvail <= VBVA_RING_BUFFER_THRESHOLD)
        {
            if (VbglR3VideoAccelFlush() < 0)
                DISABLE_VBVA_AND_RETURN(pScrn,
                    "Unable to clear the VirtualBox graphics acceleration queue "
                    "- the request to the virtual machine failed.  Switching to "
                    "unaccelerated mode.\n");
            /* Calculate the free space again. */
            off32Free = pMem->off32Free;
            off32Data = pMem->off32Data;
            i32Diff = off32Data - off32Free;
            cbHwBufferAvail = i32Diff > 0? i32Diff:
                                  VBVA_RING_BUFFER_SIZE + i32Diff;
            if (cbHwBufferAvail <= VBVA_RING_BUFFER_THRESHOLD)
                DISABLE_VBVA_AND_RETURN(pScrn,
                    "No space left in the VirtualBox graphics acceleration command buffer, "
                    "despite clearing the queue.  Switching to unaccelerated mode.\n");
        }
        /* Now copy the data into the buffer */
        if (off32Free + sizeof(cmdHdr) < VBVA_RING_BUFFER_SIZE)
        {
            memcpy(&pMem->au8RingBuffer[off32Free], &cmdHdr, sizeof(cmdHdr));
            pMem->off32Free = pMem->off32Free + sizeof(cmdHdr);
        }
        else
        {
            CARD32 u32First = VBVA_RING_BUFFER_SIZE - off32Free;
            /* The following is impressively ugly! */
            CARD8 *pu8Second = (CARD8 *)&cmdHdr + u32First;
            CARD32 u32Second = sizeof(cmdHdr) - u32First;
            memcpy(&pMem->au8RingBuffer[off32Free], &cmdHdr, u32First);
            if (u32Second)
                memcpy(&pMem->au8RingBuffer[0], pu8Second, u32Second);
            pMem->off32Free = u32Second;
        }
        pRecord->cbRecord += sizeof(cmdHdr);
        /* Mark the record completed. */
        pRecord->cbRecord &= ~VBVA_F_RECORD_PARTIAL;
    }
}

#ifdef PCIACCESS
/* As of X.org server 1.5, we are using the pciaccess library functions to
 * access PCI.  This structure describes our VMM device. */
/** Structure describing the VMM device */
static const struct pci_id_match vboxVMMDevID =
{ VMMDEV_VENDORID, VMMDEV_DEVICEID, PCI_MATCH_ANY, PCI_MATCH_ANY,
  0, 0, 0 };
#endif

/**
 * Initialise VirtualBox's accelerated video extensions.
 *
 * @returns TRUE on success, FALSE on failure
 */
static Bool
vboxInitVbva(int scrnIndex, ScreenPtr pScreen, VBOXPtr pVBox)
{
#ifdef PCIACCESS
    struct pci_device_iterator *devIter = NULL;

    TRACE_ENTRY();
    pVBox->vmmDevInfo = NULL;
    devIter = pci_id_match_iterator_create(&vboxVMMDevID);
    if (devIter)
    {
        pVBox->vmmDevInfo = pci_device_next(devIter);
        pci_iterator_destroy(devIter);
    }
    if (pVBox->vmmDevInfo)
    {
        if (pci_device_probe(pVBox->vmmDevInfo) != 0)
        {
            xf86DrvMsg (scrnIndex, X_ERROR,
                        "Failed to probe VMM device (vendor=%04x, devid=%04x)\n",
                        pVBox->vmmDevInfo->vendor_id,
                        pVBox->vmmDevInfo->device_id);
        }
        else
        {
            if (pci_device_map_range(pVBox->vmmDevInfo,
                                     pVBox->vmmDevInfo->regions[1].base_addr,
                                     pVBox->vmmDevInfo->regions[1].size,
                                     PCI_DEV_MAP_FLAG_WRITABLE,
                                     (void **)&pVBox->pVMMDevMemory) != 0)
                xf86DrvMsg (scrnIndex, X_ERROR,
                            "Failed to map VMM device range\n");
        }
    }
#else
    PCITAG pciTagDev;
    ADDRESS pciAddrDev;

    TRACE_ENTRY();
    /* Locate the device.  It should already have been enabled by
       the kernel driver. */
    pciTagDev = pciFindFirst((unsigned) VMMDEV_DEVICEID << 16 | VMMDEV_VENDORID,
                             (CARD32) ~0);
    if (pciTagDev == PCI_NOT_FOUND)
    {
        xf86DrvMsg(scrnIndex, X_ERROR,
                   "Could not find the VirtualBox base device on the PCI bus.\n");
        return FALSE;
    }
    /* Read the address and size of the second I/O region. */
    pciAddrDev = pciReadLong(pciTagDev, PCI_MAP_REG_START + 4);
    if (pciAddrDev == 0 || pciAddrDev == (CARD32) ~0)
        RETERROR(scrnIndex, FALSE,
                 "The VirtualBox base device contains an invalid memory address.\n");
    if (PCI_MAP_IS64BITMEM(pciAddrDev))
        RETERROR(scrnIndex, FALSE,
                 "The VirtualBox base device has a 64bit mapping address.  "
                 "This is currently not supported.\n");
    /* Map it.  We hardcode the size as X does not export the
       function needed to determine it. */
    pVBox->pVMMDevMemory = xf86MapPciMem(scrnIndex, 0, pciTagDev, pciAddrDev,
                                         sizeof(VMMDevMemory));
#endif
    if (pVBox->pVMMDevMemory == NULL)
    {
        xf86DrvMsg(scrnIndex, X_ERROR,
                   "Failed to map VirtualBox video extension memory.\n");
        return FALSE;
    }
    pVBox->pVbvaMemory = &pVBox->pVMMDevMemory->vbvaMemory;
    /* Set up the dirty rectangle handler.  Since this seems to be a
       delicate operation, and removing it doubly so, this will
       remain in place whether it is needed or not, and will simply
       return if VBVA is not active.  I assume that it will be active
       most of the time. */
    if (ShadowFBInit2(pScreen, NULL, vboxHandleDirtyRect) != TRUE)
    {
        xf86DrvMsg(scrnIndex, X_ERROR,
                   "Unable to install dirty rectangle handler for VirtualBox graphics acceleration.\n");
        return FALSE;
    }
    return TRUE;
}

Bool
vbox_init(int scrnIndex, VBOXPtr pVBox)
{
    Bool rc = TRUE;
    int vrc;
    uint32_t fMouseFeatures = 0;

    TRACE_ENTRY();
    pVBox->useVbva = FALSE;
    vrc = VbglR3Init();
    if (RT_FAILURE(vrc))
    {
        xf86DrvMsg(scrnIndex, X_ERROR,
                   "Failed to initialize the VirtualBox device (rc=%d) - make sure that the VirtualBox guest additions are properly installed.  If you are not sure, try reinstalling them.  The X Window graphics drivers will run in compatibility mode.\n",
                   vrc);
        rc = FALSE;
    }
    pVBox->useDevice = rc;
    /* We can't switch to a software cursor at will without help from
     * VBoxClient.  So tell that to the host and wait for VBoxClient to
     * change this. */
    vrc = VbglR3GetMouseStatus(&fMouseFeatures, NULL, NULL);
    if (RT_SUCCESS(vrc))
        VbglR3SetMouseStatus(  fMouseFeatures
                             | VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR);
    return rc;
}

Bool
vbox_open(ScrnInfoPtr pScrn, ScreenPtr pScreen, VBOXPtr pVBox)
{
    int rc;
    void *p;
    size_t size;
    int scrnIndex = pScrn->scrnIndex;

    TRACE_ENTRY();

    if (!pVBox->useDevice)
        return FALSE;

    if (pVBox->reqp)
    {
        /* still open, just re-enable VBVA after CloseScreen was called */
        pVBox->useVbva = vboxInitVbva(scrnIndex, pScreen, pVBox);
        return TRUE;
    }

    size = vmmdevGetRequestSize(VMMDevReq_SetPointerShape);
    p = xcalloc(1, size);
    if (p)
    {
        rc = vmmdevInitRequest(p, VMMDevReq_SetPointerShape);
        if (RT_SUCCESS(rc))
        {
            pVBox->reqp = p;
            pVBox->pCurs = NULL;
            pVBox->pointerHeaderSize = size;
            pVBox->useVbva = vboxInitVbva(scrnIndex, pScreen, pVBox);
            return TRUE;
        }
        xf86DrvMsg(scrnIndex, X_ERROR, "Could not init VMM request: rc = %d\n", rc);
        xfree(p);
    }
    xf86DrvMsg(scrnIndex, X_ERROR, "Could not allocate %lu bytes for VMM request\n", (unsigned long)size);
    return FALSE;
}

Bool
vbox_device_available(VBOXPtr pVBox)
{
    return pVBox->useDevice;
}

static void
vbox_vmm_hide_cursor(ScrnInfoPtr pScrn, VBOXPtr pVBox)
{
    int rc;

    pVBox->reqp->fFlags = 0;
    rc = VbglR3SetPointerShapeReq(pVBox->reqp);
    if (RT_FAILURE(rc))
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Could not hide the virtual mouse pointer.\n");
        /* Play safe, and disable the hardware cursor until the next mode
         * switch, since obviously something happened that we didn't
         * anticipate. */
        pVBox->forceSWCursor = TRUE;
    }
}

static void
vbox_vmm_show_cursor(ScrnInfoPtr pScrn, VBOXPtr pVBox)
{
    int rc;

    if (vbox_host_uses_hwcursor(pScrn)) {
        pVBox->reqp->fFlags = VBOX_MOUSE_POINTER_VISIBLE;
        rc = VbglR3SetPointerShapeReq(pVBox->reqp);
        if (RT_FAILURE(rc)) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Could not unhide the virtual mouse pointer.\n");
            /* Play safe, and disable the hardware cursor until the next mode
             * switch, since obviously something happened that we didn't
             * anticipate. */
            pVBox->forceSWCursor = TRUE;
        }
    }
}

static void
vbox_vmm_load_cursor_image(ScrnInfoPtr pScrn, VBOXPtr pVBox,
                           unsigned char *image)
{
    int rc;
    VMMDevReqMousePointer *reqp;
    reqp = (VMMDevReqMousePointer *)image;

#ifdef DEBUG_POINTER
    vbox_show_shape(reqp->width, reqp->height, 0, image);
#endif

    rc = VbglR3SetPointerShapeReq(reqp);
    if (RT_FAILURE(rc)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,  "Unable to set the virtual mouse pointer image.\n");
        /* Play safe, and disable the hardware cursor until the next mode
         * switch, since obviously something happened that we didn't
         * anticipate. */
        pVBox->forceSWCursor = TRUE;
    }
}

static void
vbox_set_cursor_colors(ScrnInfoPtr pScrn, int bg, int fg)
{
    NOREF(pScrn);
    NOREF(bg);
    NOREF(fg);
    /* ErrorF("vbox_set_cursor_colors NOT IMPLEMENTED\n"); */
}


static void
vbox_set_cursor_position(ScrnInfoPtr pScrn, int x, int y)
{
    /* Nothing to do here, as we are telling the guest where the mouse is,
     * not vice versa. */
    NOREF(pScrn);
    NOREF(x);
    NOREF(y);
}

static void
vbox_hide_cursor(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = pScrn->driverPrivate;

    vbox_vmm_hide_cursor(pScrn, pVBox);
}

static void
vbox_show_cursor(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = pScrn->driverPrivate;

    vbox_vmm_show_cursor(pScrn, pVBox);
}

static void
vbox_load_cursor_image(ScrnInfoPtr pScrn, unsigned char *image)
{
    VBOXPtr pVBox = pScrn->driverPrivate;

    vbox_vmm_load_cursor_image(pScrn, pVBox, image);
}

static Bool
vbox_use_hw_cursor(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    return vbox_host_uses_hwcursor(pScrn);
}

static unsigned char
color_to_byte(unsigned c)
{
    return (c >> 8) & 0xff;
}

static unsigned char *
vbox_realize_cursor(xf86CursorInfoPtr infoPtr, CursorPtr pCurs)
{
    VBOXPtr pVBox;
    CursorBitsPtr bitsp;
    unsigned short w, h, x, y;
    unsigned char *c, *p, *pm, *ps, *m;
    size_t sizeRequest, sizeRgba, sizeMask, srcPitch, dstPitch;
    CARD32 fc, bc, *cp;
    int rc, scrnIndex = infoPtr->pScrn->scrnIndex;
    VMMDevReqMousePointer *reqp;

    pVBox = infoPtr->pScrn->driverPrivate;
    bitsp = pCurs->bits;
    w = bitsp->width;
    h = bitsp->height;

    if (!w || !h || w > VBOX_MAX_CURSOR_WIDTH || h > VBOX_MAX_CURSOR_HEIGHT)
        RETERROR(scrnIndex, NULL,
            "Error invalid cursor dimensions %dx%d\n", w, h);

    if ((bitsp->xhot > w) || (bitsp->yhot > h))
        RETERROR(scrnIndex, NULL,
            "Error invalid cursor hotspot location %dx%d (max %dx%d)\n",
            bitsp->xhot, bitsp->yhot, w, h);

    srcPitch = PixmapBytePad (bitsp->width, 1);
    dstPitch = (w + 7) / 8;
    sizeMask = ((dstPitch * h) + 3) & (size_t) ~3;
    sizeRgba = w * h * 4;
    pVBox->pointerSize = sizeMask + sizeRgba;
    sizeRequest = pVBox->pointerSize + pVBox->pointerHeaderSize;

    p = c = xcalloc (1, sizeRequest);
    if (!c)
        RETERROR(scrnIndex, NULL,
                 "Error failed to alloc %lu bytes for cursor\n",
                 (unsigned long) sizeRequest);

    rc = vmmdevInitRequest((VMMDevRequestHeader *)p, VMMDevReq_SetPointerShape);
    if (RT_FAILURE(rc))
    {
        xf86DrvMsg(scrnIndex, X_ERROR, "Could not init VMM request: rc = %d\n", rc);
        xfree(p);
        return NULL;
    }

    m = p + offsetof(VMMDevReqMousePointer, pointerData);
    cp = (CARD32 *)(m + sizeMask);

    TRACE_LOG ("w=%d h=%d sm=%d sr=%d p=%d\n",
           w, h, (int) sizeMask, (int) sizeRgba, (int) dstPitch);
    TRACE_LOG ("m=%p c=%p cp=%p\n", m, c, (void *)cp);

    fc = color_to_byte (pCurs->foreBlue)
      | (color_to_byte (pCurs->foreGreen) << 8)
      | (color_to_byte (pCurs->foreRed)   << 16);

    bc = color_to_byte (pCurs->backBlue)
      | (color_to_byte (pCurs->backGreen) << 8)
      | (color_to_byte (pCurs->backRed)   << 16);

    /*
     * Convert the Xorg source/mask bits to the and/xor bits VBox needs.
     * Xorg:
     *   The mask is a bitmap indicating which parts of the cursor are
     *   transparent and which parts are drawn. The source is a bitmap
     *   indicating which parts of the non-transparent portion of the
     *   the cursor should be painted in the foreground color and which
     *   should be painted in the background color. By default, set bits
     *   indicate the opaque part of the mask bitmap and clear bits
     *   indicate the transparent part.
     * VBox:
     *   The color data is the XOR mask. The AND mask bits determine
     *   which pixels of the color data (XOR mask) will replace (overwrite)
     *   the screen pixels (AND mask bit = 0) and which ones will be XORed
     *   with existing screen pixels (AND mask bit = 1).
     *   For example when you have the AND mask all 0, then you see the
     *   correct mouse pointer image surrounded by black square.
     */
    for (pm = bitsp->mask, ps = bitsp->source, y = 0;
         y < h;
         ++y, pm += srcPitch, ps += srcPitch, m += dstPitch)
    {
        for (x = 0; x < w; ++x)
        {
            if (pm[x / 8] & (1 << (x % 8)))
            {
                /* opaque, leave AND mask bit at 0 */
                if (ps[x / 8] & (1 << (x % 8)))
                {
                    *cp++ = fc;
                    PUT_PIXEL('X');
                }
                else
                {
                    *cp++ = bc;
                    PUT_PIXEL('*');
                }
            }
            else
            {
                /* transparent, set AND mask bit */
                m[x / 8] |= 1 << (7 - (x % 8));
                /* don't change the screen pixel */
                *cp++ = 0;
                PUT_PIXEL(' ');
            }
        }
        PUT_PIXEL('\n');
    }

    reqp = (VMMDevReqMousePointer *)p;
    reqp->width  = w;
    reqp->height = h;
    reqp->xHot   = bitsp->xhot;
    reqp->yHot   = bitsp->yhot;
    reqp->fFlags = VBOX_MOUSE_POINTER_VISIBLE | VBOX_MOUSE_POINTER_SHAPE;
    reqp->header.size = sizeRequest;

#ifdef DEBUG_POINTER
    ErrorF("shape = %p\n", p);
    vbox_show_shape(w, h, bc, c);
#endif

    return p;
}

#ifdef ARGB_CURSOR
static Bool
vbox_use_hw_cursor_argb(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    Bool rc = TRUE;

    if (!vbox_host_uses_hwcursor(pScrn))
        rc = FALSE;
    if (   rc
        && (   (pCurs->bits->height > VBOX_MAX_CURSOR_HEIGHT)
            || (pCurs->bits->width > VBOX_MAX_CURSOR_WIDTH)
            || (pScrn->bitsPerPixel <= 8)
           )
       )
        rc = FALSE;
#ifndef VBOXVIDEO_13
    /* Evil hack - we use this as another way of poking the driver to update
     * our list of video modes. */
    vboxWriteHostModes(pScrn, pScrn->currentMode);
#endif
    TRACE_LOG("rc=%s\n", BOOL_STR(rc));
    return rc;
}


static void
vbox_load_cursor_argb(ScrnInfoPtr pScrn, CursorPtr pCurs)
{
    VBOXPtr pVBox;
    VMMDevReqMousePointer *reqp;
    CursorBitsPtr bitsp;
    unsigned short w, h;
    unsigned short cx, cy;
    unsigned char *pm;
    CARD32 *pc;
    size_t sizeRequest, sizeMask;
    CARD8 *p;
    int scrnIndex;

    TRACE_ENTRY();
    pVBox = pScrn->driverPrivate;
    bitsp = pCurs->bits;
    w     = bitsp->width;
    h     = bitsp->height;
    scrnIndex = pScrn->scrnIndex;

    /* Mask must be generated for alpha cursors, that is required by VBox. */
    /* note: (michael) the next struct must be 32bit aligned. */
    sizeMask  = ((w + 7) / 8 * h + 3) & ~3;

    if (!w || !h || w > VBOX_MAX_CURSOR_WIDTH || h > VBOX_MAX_CURSOR_HEIGHT)
        RETERROR(scrnIndex, ,
                 "Error invalid cursor dimensions %dx%d\n", w, h);

    if ((bitsp->xhot > w) || (bitsp->yhot > h))
        RETERROR(scrnIndex, ,
                 "Error invalid cursor hotspot location %dx%d (max %dx%d)\n",
                 bitsp->xhot, bitsp->yhot, w, h);

    pVBox->pointerSize = w * h * 4 + sizeMask;
    sizeRequest = pVBox->pointerSize + pVBox->pointerHeaderSize;
    p = xcalloc(1, sizeRequest);
    if (!p)
        RETERROR(scrnIndex, ,
                 "Error failed to alloc %lu bytes for cursor\n",
                 (unsigned long)sizeRequest);

    reqp = (VMMDevReqMousePointer *)p;
    *reqp = *pVBox->reqp;
    reqp->width  = w;
    reqp->height = h;
    reqp->xHot   = bitsp->xhot;
    reqp->yHot   = bitsp->yhot;
    reqp->fFlags =   VBOX_MOUSE_POINTER_VISIBLE | VBOX_MOUSE_POINTER_SHAPE
                   | VBOX_MOUSE_POINTER_ALPHA;
    reqp->header.size = sizeRequest;

    memcpy(p + offsetof(VMMDevReqMousePointer, pointerData) + sizeMask, bitsp->argb, w * h * 4);

    /* Emulate the AND mask. */
    pm = p + offsetof(VMMDevReqMousePointer, pointerData);
    pc = bitsp->argb;

    /* Init AND mask to 1 */
    memset(pm, 0xFF, sizeMask);

    /*
     * The additions driver must provide the AND mask for alpha cursors. The host frontend
     * which can handle alpha channel, will ignore the AND mask and draw an alpha cursor.
     * But if the host does not support ARGB, then it simply uses the AND mask and the color
     * data to draw a normal color cursor.
     */
    for (cy = 0; cy < h; cy++)
    {
        unsigned char bitmask = 0x80;

        for (cx = 0; cx < w; cx++, bitmask >>= 1)
        {
            if (bitmask == 0)
                bitmask = 0x80;

            if (pc[cx] >= 0xF0000000)
                pm[cx / 8] &= ~bitmask;
        }

        /* Point to next source and dest scans */
        pc += w;
        pm += (w + 7) / 8;
    }

    VbglR3SetPointerShapeReq(reqp);
    xfree(p);
}
#endif

Bool
vbox_cursor_init(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VBOXPtr pVBox = pScrn->driverPrivate;
    xf86CursorInfoPtr pCurs = NULL;
    Bool rc = TRUE;

    TRACE_ENTRY();
    if (!pVBox->useDevice)
        return FALSE;
    pVBox->pCurs = pCurs = xf86CreateCursorInfoRec();
    if (!pCurs) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Failed to create X Window cursor information structures for virtual mouse.\n");
        rc = FALSE;
    }
    if (rc) {
        pCurs->MaxWidth = VBOX_MAX_CURSOR_WIDTH;
        pCurs->MaxHeight = VBOX_MAX_CURSOR_HEIGHT;
        pCurs->Flags =   HARDWARE_CURSOR_TRUECOLOR_AT_8BPP
                       | HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_1
                       | HARDWARE_CURSOR_BIT_ORDER_MSBFIRST;

        pCurs->SetCursorColors   = vbox_set_cursor_colors;
        pCurs->SetCursorPosition = vbox_set_cursor_position;
        pCurs->LoadCursorImage   = vbox_load_cursor_image;
        pCurs->HideCursor        = vbox_hide_cursor;
        pCurs->ShowCursor        = vbox_show_cursor;
        pCurs->UseHWCursor       = vbox_use_hw_cursor;
        pCurs->RealizeCursor     = vbox_realize_cursor;

#ifdef ARGB_CURSOR
        pCurs->UseHWCursorARGB   = vbox_use_hw_cursor_argb;
        pCurs->LoadCursorARGB    = vbox_load_cursor_argb;
#endif

        /* Hide the host cursor before we initialise if we wish to use a
         * software cursor. */
        if (pVBox->forceSWCursor)
            vbox_vmm_hide_cursor(pScrn, pVBox);
        rc = xf86InitCursor(pScreen, pCurs);
    }
    if (!rc)
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Failed to enable mouse pointer integration.\n");
    if (!rc && (pCurs != NULL))
        xf86DestroyCursorInfoRec(pCurs);
    return rc;
}

/**
 * Inform VBox that we will supply it with dirty rectangle information
 * and install the dirty rectangle handler.
 *
 * @returns TRUE for success, FALSE for failure
 * @param   pScrn   Pointer to a structure describing the X screen in use
 */
Bool
vboxEnableVbva(ScrnInfoPtr pScrn)
{
    bool rc = TRUE;
    int scrnIndex = pScrn->scrnIndex;
    VBOXPtr pVBox = pScrn->driverPrivate;

    TRACE_ENTRY();
    if (pVBox->useVbva != TRUE)
        rc = FALSE;
    if (rc && RT_FAILURE(VbglR3VideoAccelEnable(true)))
        /* Request not accepted - disable for old hosts. */
        xf86DrvMsg(scrnIndex, X_ERROR,
                   "Unable to activate VirtualBox graphics acceleration "
                   "- the request to the virtual machine failed.  "
                   "You may be running an old version of VirtualBox.\n");
    pVBox->useVbva = rc;
    if (!rc)
        VbglR3VideoAccelEnable(false);
    return rc;
}

/**
 * Inform VBox that we will stop supplying it with dirty rectangle
 * information. This function is intended to be called when an X
 * virtual terminal is disabled, or the X server is terminated.
 *
 * @returns TRUE for success, FALSE for failure
 * @param   pScrn   Pointer to a structure describing the X screen in use
 */
Bool
vboxDisableVbva(ScrnInfoPtr pScrn)
{
    int rc;
    int scrnIndex = pScrn->scrnIndex;
    VBOXPtr pVBox = pScrn->driverPrivate;

    TRACE_ENTRY();
    if (pVBox->useVbva != TRUE)  /* Ths function should not have been called */
        return FALSE;
    rc = VbglR3VideoAccelEnable(false);
    if (RT_FAILURE(rc))
    {
        xf86DrvMsg(scrnIndex, X_ERROR,
                   "Unable to disable VirtualBox graphics acceleration "
                   "- the request to the virtual machine failed.\n");
    }
    else
        memset(pVBox->pVbvaMemory, 0, sizeof(VBVAMEMORY));
    return TRUE;
}

/**
 * Inform VBox that we are aware of advanced graphics functions
 * (i.e. dynamic resizing, seamless).
 *
 * @returns TRUE for success, FALSE for failure
 */
Bool
vboxEnableGraphicsCap(VBOXPtr pVBox)
{
    TRACE_ENTRY();
    if (!pVBox->useDevice)
        return FALSE;
    return RT_SUCCESS(VbglR3SetGuestCaps(VMMDEV_GUEST_SUPPORTS_GRAPHICS, 0));
}

/**
 * Inform VBox that we are no longer aware of advanced graphics functions
 * (i.e. dynamic resizing, seamless).
 *
 * @returns TRUE for success, FALSE for failure
 */
Bool
vboxDisableGraphicsCap(VBOXPtr pVBox)
{
    TRACE_ENTRY();
    if (!pVBox->useDevice)
        return FALSE;
    return RT_SUCCESS(VbglR3SetGuestCaps(0, VMMDEV_GUEST_SUPPORTS_GRAPHICS));
}

/**
 * Query the last display change request.
 *
 * @returns boolean success indicator.
 * @param   pScrn       Pointer to the X screen info structure.
 * @param   pcx         Where to store the horizontal pixel resolution (0 = do not change).
 * @param   pcy         Where to store the vertical pixel resolution (0 = do not change).
 * @param   pcBits      Where to store the bits per pixel (0 = do not change).
 * @param   iDisplay    Where to store the display number the request was for - 0 for the
 *                      primary display, 1 for the first secondary, etc.
 */
Bool
vboxGetDisplayChangeRequest(ScrnInfoPtr pScrn, uint32_t *pcx, uint32_t *pcy,
                            uint32_t *pcBits, uint32_t *piDisplay)
{
    VBOXPtr pVBox = pScrn->driverPrivate;
    TRACE_ENTRY();
    if (!pVBox->useDevice)
        return FALSE;
    int rc = VbglR3GetDisplayChangeRequest(pcx, pcy, pcBits, piDisplay, true);
    if (RT_SUCCESS(rc))
        return TRUE;
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to obtain the last resolution requested by the guest, rc=%d.\n", rc);
    return FALSE;
}


/**
 * Query the host as to whether it likes a specific video mode.
 *
 * @returns the result of the query
 * @param   cx     the width of the mode being queried
 * @param   cy     the height of the mode being queried
 * @param   cBits  the bpp of the mode being queried
 */
Bool
vboxHostLikesVideoMode(ScrnInfoPtr pScrn, uint32_t cx, uint32_t cy, uint32_t cBits)
{
    VBOXPtr pVBox = pScrn->driverPrivate;
    TRACE_ENTRY();
    if (!pVBox->useDevice)
        return TRUE;  /* If we can't ask the host then we like everything. */
    return VbglR3HostLikesVideoMode(cx, cy, cBits);
}

/**
 * Check if any seamless mode is enabled.
 * Seamless is only relevant for the newer Xorg modules.
 *
 * @returns the result of the query
 * (true = seamless enabled, false = seamless not enabled)
 * @param   pScrn  Screen info pointer.
 */
Bool
vboxGuestIsSeamless(ScrnInfoPtr pScrn)
{
    VMMDevSeamlessMode mode;
    VBOXPtr pVBox = pScrn->driverPrivate;
    TRACE_ENTRY();
    if (!pVBox->useDevice)
        return FALSE;
    if (RT_FAILURE(VbglR3SeamlessGetLastEvent(&mode)))
        return FALSE;
    return (mode != VMMDev_Seamless_Disabled);
}

/**
 * Save video mode parameters to the registry.
 *
 * @returns iprt status value
 * @param   pszName the name to save the mode parameters under
 * @param   cx      mode width
 * @param   cy      mode height
 * @param   cBits   bits per pixel for the mode
 */
Bool
vboxSaveVideoMode(ScrnInfoPtr pScrn, uint32_t cx, uint32_t cy, uint32_t cBits)
{
    VBOXPtr pVBox = pScrn->driverPrivate;
    TRACE_ENTRY();
    if (!pVBox->useDevice)
        return FALSE;
    return RT_SUCCESS(VbglR3SaveVideoMode("SavedMode", cx, cy, cBits));
}

/**
 * Retrieve video mode parameters from the registry.
 *
 * @returns iprt status value
 * @param   pszName the name under which the mode parameters are saved
 * @param   pcx     where to store the mode width
 * @param   pcy     where to store the mode height
 * @param   pcBits  where to store the bits per pixel for the mode
 */
Bool
vboxRetrieveVideoMode(ScrnInfoPtr pScrn, uint32_t *pcx, uint32_t *pcy, uint32_t *pcBits)
{
    VBOXPtr pVBox = pScrn->driverPrivate;
    TRACE_ENTRY();
    if (!pVBox->useDevice)
        return FALSE;
    int rc = VbglR3RetrieveVideoMode("SavedMode", pcx, pcy, pcBits);
    if (RT_SUCCESS(rc))
        TRACE_LOG("Retrieved a video mode of %dx%dx%d\n", *pcx, *pcy, *pcBits);
    else
        TRACE_LOG("Failed to retrieve video mode, error %d\n", rc);
    return (RT_SUCCESS(rc));
}

/**
 * Fills a display mode M with a built-in mode of name pszName and dimensions
 * cx and cy.
 */
static void vboxFillDisplayMode(DisplayModePtr m, const char *pszName,
                                unsigned cx, unsigned cy)
{
    TRACE_LOG("pszName=%s, cx=%u, cy=%u\n", pszName, cx, cy);
    m->status        = MODE_OK;
    m->type          = M_T_BUILTIN;
    /* VBox only supports screen widths which are a multiple of 8 */
    m->HDisplay      = cx & ~7;
    m->HSyncStart    = m->HDisplay + 2;
    m->HSyncEnd      = m->HDisplay + 4;
    m->HTotal        = m->HDisplay + 6;
    m->VDisplay      = cy;
    m->VSyncStart    = m->VDisplay + 2;
    m->VSyncEnd      = m->VDisplay + 4;
    m->VTotal        = m->VDisplay + 6;
    m->Clock         = m->HTotal * m->VTotal * 60 / 1000; /* kHz */
    if (pszName)
    {
        if (m->name)
            xfree(m->name);
        m->name      = xnfstrdup(pszName);
    }
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
static unsigned vboxNextStandardMode(ScrnInfoPtr pScrn, unsigned cIndex,
                                     uint32_t *pcx, uint32_t *pcy,
                                     uint32_t *pcBits)
{
    XF86ASSERT(cIndex < vboxNumStdModes,
               ("cIndex = %d, vboxNumStdModes = %d\n", cIndex,
                vboxNumStdModes));
    for (unsigned i = cIndex; i < vboxNumStdModes - 1; ++i)
    {
        uint32_t cBits = pScrn->bitsPerPixel;
        uint32_t cx = vboxStandardModes[i].cx;
        uint32_t cy = vboxStandardModes[i].cy;

        if (cBits != 0 && !vboxHostLikesVideoMode(pScrn, cx, cy, cBits))
            continue;
        if (vboxHostLikesVideoMode(pScrn, cx, cy, 32))
            cBits = 32;
        else if (vboxHostLikesVideoMode(pScrn, cx, cy, 16))
            cBits = 16;
        else
            continue;
        if (pcx)
            *pcx = cx;
        if (pcy)
            *pcy = cy;
        if (pcBits)
            *pcBits = cBits;
        return i + 1;
    }
    return 0;
}

/**
 * Returns the preferred video mode.  The current order of preference is
 * (from highest to least preferred):
 *  - The mode corresponding to the last size hint from the host
 *  - The video mode saved from the last session
 *  - The largest standard mode which the host likes, falling back to
 *    640x480x32 as a worst case
 *  - If the host can't be contacted at all, we return 1024x768x32
 *
 * The return type is void as we guarantee we will return some mode.
 */
void vboxGetPreferredMode(ScrnInfoPtr pScrn, uint32_t *pcx,
                          uint32_t *pcy, uint32_t *pcBits)
{
    /* Query the host for the preferred resolution and colour depth */
    uint32_t cx = 0, cy = 0, iDisplay = 0, cBits = 32;
    VBOXPtr pVBox = pScrn->driverPrivate;

    TRACE_ENTRY();
    if (pVBox->useDevice)
    {
        bool found = vboxGetDisplayChangeRequest(pScrn, &cx, &cy, &cBits, &iDisplay);
        if ((cx == 0) || (cy == 0))
            found = false;
        if (!found)
            found = vboxRetrieveVideoMode(pScrn, &cx, &cy, &cBits);
        if ((cx == 0) || (cy == 0))
            found = false;
        if (found)
            /* Adjust to a multiple of eight */
            cx &= ~7;
        if (!found)
            found = (vboxNextStandardMode(pScrn, 0, &cx, &cy, &cBits) != 0);
        if (!found)
        {
            /* Last resort */
            cx = 640;
            cy = 480;
            cBits = 32;
        }
    }
    else
    {
        cx = 1024;
        cy = 768;
    }
    if (pcx)
        *pcx = cx;
    if (pcy)
        *pcy = cy;
    if (pcx)
        *pcBits = cBits;
}

/* Move a screen mode found to the end of the list, so that RandR will give
 * it the highest priority when a mode switch is requested.  Returns the mode
 * that was previously before the mode in the list in order to allow the
 * caller to continue walking the list. */
static DisplayModePtr vboxMoveModeToFront(ScrnInfoPtr pScrn,
                                          DisplayModePtr pMode)
{
    DisplayModePtr pPrev = pMode->prev;
    if (pMode != pScrn->modes)
    {
        pMode->prev->next = pMode->next;
        pMode->next->prev = pMode->prev;
        pMode->next = pScrn->modes;
        pMode->prev = pScrn->modes->prev;
        pMode->next->prev = pMode;
        pMode->prev->next = pMode;
        pScrn->modes = pMode;
    }
    return pPrev;
}

/**
 * Rewrites the first dynamic mode found which is not the current screen mode
 * to contain the host's currently preferred screen size, then moves that
 * mode to the front of the screen information structure's mode list.
 * Additionally, if the current mode is not dynamic, the second dynamic mode
 * will be set to match the current mode and also added to the front.  This
 * ensures that the user can always reset the current size to kick the driver
 * to update its mode list.
 */
void vboxWriteHostModes(ScrnInfoPtr pScrn, DisplayModePtr pCurrent)
{
    uint32_t cx = 0, cy = 0, iDisplay = 0, cBits = 0;
    DisplayModePtr pMode;
    bool found = false;

    TRACE_ENTRY();
    vboxGetPreferredMode(pScrn, &cx, &cy, &cBits);
#ifdef DEBUG
    /* Count the number of modes for sanity */
    unsigned cModes = 1, cMode = 0;
    DisplayModePtr pCount;
    for (pCount = pScrn->modes; ; pCount = pCount->next, ++cModes)
        if (pCount->next == pScrn->modes)
            break;
#endif
    for (pMode = pScrn->modes; ; pMode = pMode->next)
    {
#ifdef DEBUG
        XF86ASSERT (cMode++ < cModes, (NULL));
#endif
        if (   pMode != pCurrent
            && !strcmp(pMode->name, "VBoxDynamicMode"))
        {
            if (!found)
                vboxFillDisplayMode(pMode, NULL, cx, cy);
            else if (pCurrent)
                vboxFillDisplayMode(pMode, NULL, pCurrent->HDisplay,
                                    pCurrent->VDisplay);
            found = true;
            pMode = vboxMoveModeToFront(pScrn, pMode);
        }
        if (pMode->next == pScrn->modes)
            break;
    }
    XF86ASSERT (found,
                ("vboxvideo: no free dynamic mode found.  Exiting.\n"));
    XF86ASSERT (   (pScrn->modes->HDisplay == (long) cx)
                || (   (pScrn->modes->HDisplay == pCurrent->HDisplay)
                    && (pScrn->modes->next->HDisplay == (long) cx)),
                ("pScrn->modes->HDisplay=%u, pScrn->modes->next->HDisplay=%u\n",
                 pScrn->modes->HDisplay, pScrn->modes->next->HDisplay));
    XF86ASSERT (   (pScrn->modes->VDisplay == (long) cy)
                || (   (pScrn->modes->VDisplay == pCurrent->VDisplay)
                    && (pScrn->modes->next->VDisplay == (long) cy)),
                ("pScrn->modes->VDisplay=%u, pScrn->modes->next->VDisplay=%u\n",
                 pScrn->modes->VDisplay, pScrn->modes->next->VDisplay));
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
 * of the initial graphics modes that we wish to support.  This includes:
 *  - An initial mode, of the size requested by the caller
 *  - Two dynamic modes, one of which will be updated to match the last size
 *    hint from the host on each mode switch, but initially also of the
 *    requested size
 *  - Several standard modes, if possible ones that the host likes
 *  - Any modes that the user requested in xorg.conf/XFree86Config
 */
void vboxAddModes(ScrnInfoPtr pScrn, uint32_t cxInit, uint32_t cyInit)
{
    unsigned cx = 0, cy = 0, cIndex = 0;
    /* For reasons related to the way RandR 1.1 is implemented, we need to
     * make sure that the initial mode (more precisely, a mode equal to the
     * initial virtual resolution) is always present in the mode list.  RandR
     * has the assumption build in that there will either be a mode of that
     * size present at all times, or that the first mode in the list will
     * always be smaller than the initial virtual resolution.  Since our
     * approach to dynamic resizing isn't quite the way RandR was intended to
     * be, and breaks the second assumption, we guarantee the first. */
    DisplayModePtr pMode = vboxAddEmptyScreenMode(pScrn);
    vboxFillDisplayMode(pMode, "VBoxInitialMode", cxInit, cyInit);
    /* Create our two dynamic modes. */
    pMode = vboxAddEmptyScreenMode(pScrn);
    vboxFillDisplayMode(pMode, "VBoxDynamicMode", cxInit, cyInit);
    pMode = vboxAddEmptyScreenMode(pScrn);
    vboxFillDisplayMode(pMode, "VBoxDynamicMode", cxInit, cyInit);
    /* Add standard modes supported by the host */
    for ( ; ; )
    {
        char szName[256];
        cIndex = vboxNextStandardMode(pScrn, cIndex, &cx, &cy, NULL);
        if (cIndex == 0)
            break;
        sprintf(szName, "VBox-%ux%u", cx, cy);
        pMode = vboxAddEmptyScreenMode(pScrn);
        vboxFillDisplayMode(pMode, szName, cx, cy);
    }
    /* And finally any modes specified by the user.  We assume here that
     * the mode names reflect the mode sizes. */
    for (unsigned i = 0;    pScrn->display->modes != NULL
                         && pScrn->display->modes[i] != NULL; i++)
    {
        if (sscanf(pScrn->display->modes[i], "%ux%u", &cx, &cy) == 2)
        {
            pMode = vboxAddEmptyScreenMode(pScrn);
            vboxFillDisplayMode(pMode, pScrn->display->modes[i], cx, cy);
        }
    }
}
