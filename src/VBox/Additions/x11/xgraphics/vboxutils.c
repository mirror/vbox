/** @file
 * VirtualBox X11 Additions graphics driver utility functions
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

#include <VBox/VBoxGuest.h>
#include <VBox/VBoxDev.h>

#include <xf86Pci.h>
#include <Pci.h>

#include "xf86.h"
#define NEED_XF86_TYPES
#include "xf86_ansic.h"
#include "compiler.h"
#include "cursorstr.h"

#include "vboxvideo.h"

#define VBOX_MAX_CURSOR_WIDTH 64
#define VBOX_MAX_CURSOR_HEIGHT 64

#if 0
#define DEBUG_X
#endif
#ifdef DEBUG_X
#define TRACE_ENTRY() do \
    { \
        ErrorF ("%s\n", __FUNCTION__); \
    } while(0)
#define TRACE_LINE() do \
    { \
        ErrorF ("%s: line %d\n", __FUNCTION__, __LINE__); \
    } while(0)
#define PUT_PIXEL(c) ErrorF ("%c", c)
#define dolog(...) ErrorF  (__VA_ARGS__)
#else
#define PUT_PIXEL(c) do { } while(0)
#define TRACE_ENTRY() do { } while(0)
#define TRACE_LINE() do { } while(0)
#define dolog(...) do { } while(0)
#endif

/** Macro to printf an error message and return from a function */
#define RETERROR(scrnIndex, RetVal, ...) \
    do \
    { \
        xf86DrvMsg(scrnIndex, X_ERROR, __VA_ARGS__); \
        return RetVal; \
    } \
    while (0)

#ifdef DEBUG_X
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


static Bool
vbox_host_uses_hwcursor(ScrnInfoPtr pScrn)
{
    Bool rc = FALSE;
    uint32_t fFeatures = 0;

    TRACE_ENTRY();
    int vrc = VbglR3GetMouseStatus(&fFeatures, NULL, NULL);
    if (VBOX_FAILURE(vrc))
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                 "Unable to determine whether the virtual machine supports mouse pointer integration - request initialization failed with return code %d\n", vrc);
    else
    {
        if (   !(fFeatures & VBOXGUEST_MOUSE_HOST_CANNOT_HWPOINTER)
            && (fFeatures & VBOXGUEST_MOUSE_GUEST_CAN_ABSOLUTE))
            rc = TRUE;
    }
    return rc;
}


void
vbox_close(ScrnInfoPtr pScrn, VBOXPtr pVBox)
{
    TRACE_ENTRY();

    xfree (pVBox->reqp);
    pVBox->reqp = NULL;
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
        cmdHdr.x = (int16_t)aRects[i].x1;
        cmdHdr.y = (int16_t)aRects[i].y1;
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


/**
 * Initialise VirtualBox's accelerated video extensions.
 * Note that we assume that the PCI memory is 32bit mapped,
 * as X doesn't seem to support mapping 64bit memory.
 *
 * @returns TRUE on success, FALSE on failure
 */
static Bool
vboxInitVbva(int scrnIndex, ScreenPtr pScreen, VBOXPtr pVBox)
{
    PCITAG pciTag;
    ADDRESS pciAddress;

    /* Locate the device.  It should already have been enabled by
       the kernel driver. */
    pciTag = pciFindFirst((unsigned) VMMDEV_DEVICEID << 16 | VMMDEV_VENDORID,
                          (CARD32) ~0);
    if (pciTag == PCI_NOT_FOUND)
    {
        xf86DrvMsg(scrnIndex, X_ERROR,
                   "Could not find the VirtualBox base device on the PCI bus.\n");
        return FALSE;
    }
    /* Read the address and size of the second I/O region. */
    pciAddress = pciReadLong(pciTag, PCI_MAP_REG_START + 4);
    if (pciAddress == 0 || pciAddress == (CARD32) ~0)
        RETERROR(scrnIndex, FALSE,
                 "The VirtualBox base device contains an invalid memory address.\n");
    if (PCI_MAP_IS64BITMEM(pciAddress))
        RETERROR(scrnIndex, FALSE,
                 "The VirtualBox base device has a 64bit mapping address.  "
                 "This is currently not supported.\n");
    /* Map it.  We hardcode the size as X does not export the
       function needed to determine it. */
    pVBox->pVMMDevMemory = xf86MapPciMem(scrnIndex, 0, pciTag, pciAddress,
                                         sizeof(VMMDevMemory));
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
    int vrc = VbglR3Init();
    if (RT_FAILURE(vrc))
    {
        xf86DrvMsg(scrnIndex, X_ERROR,
                   "Failed to initialize the VirtualBox device (rc=%d) - make sure that the VirtualBox guest additions are properly installed.  If you are not sure, try reinstalling them.  The X Window graphics drivers will run in compatibility mode.\n",
                   vrc);
        rc = FALSE;
    }
    pVBox->useDevice = rc;
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
    pVBox->useVbva = FALSE;

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
            pVBox->pointerOffscreen = FALSE;
            pVBox->useVbva = vboxInitVbva(scrnIndex, pScreen, pVBox);
            return TRUE;
        }
        xf86DrvMsg(scrnIndex, X_ERROR, "Could not init VMM request: rc = %d\n", rc);
        xfree(p);
    }
    xf86DrvMsg(scrnIndex, X_ERROR, "Could not allocate %lu bytes for VMM request\n", (unsigned long)size);
    return FALSE;
}

static void
vbox_vmm_hide_cursor(ScrnInfoPtr pScrn, VBOXPtr pVBox)
{
    int rc;
    pVBox->reqp->fFlags = 0;
    rc = VbglR3SetPointerShapeReq(pVBox->reqp);
    if (RT_FAILURE(rc))
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Could not hide the virtual mouse pointer.\n");
}

static void
vbox_vmm_show_cursor(ScrnInfoPtr pScrn, VBOXPtr pVBox)
{
    int rc;
    pVBox->reqp->fFlags = VBOX_MOUSE_POINTER_VISIBLE;
    rc = VbglR3SetPointerShapeReq(pVBox->reqp);
    if (RT_FAILURE(rc))
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Could not unhide the virtual mouse pointer.\n");
}

static void
vbox_vmm_load_cursor_image(ScrnInfoPtr pScrn, VBOXPtr pVBox,
                           unsigned char *image)
{
    int rc;
    VMMDevReqMousePointer *reqp;
    reqp = (VMMDevReqMousePointer *)image;

    dolog("w=%d h=%d size=%d\n", reqp->width, reqp->height, reqp->header.size);
#ifdef DEBUG_X
    vbox_show_shape(reqp->width, reqp->height, 0, image);
#endif

    rc = VbglR3SetPointerShapeReq(reqp);
    if (RT_FAILURE(rc))
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,  "Unable to set the virtual mouse pointer image.\n");
}

static void
vbox_set_cursor_colors(ScrnInfoPtr pScrn, int bg, int fg)
{
    TRACE_ENTRY();

    NOREF(pScrn);
    NOREF(bg);
    NOREF(fg);
    /* ErrorF("vbox_set_cursor_colors NOT IMPLEMENTED\n"); */
}

static void
vbox_set_cursor_position(ScrnInfoPtr pScrn, int x, int y)
{
    /* VBOXPtr pVBox = pScrn->driverPrivate; */

    /* TRACE_ENTRY(); */

    /* don't disable the mouse cursor if we go out of our visible area
     * since the mouse cursor is drawn by the host anyway */
#if 0
    if (   (x < 0 || x > pScrn->pScreen->width)
        || (y < 0 || y > pScrn->pScreen->height))
    {
        if (!pVBox->pointerOffscreen)
        {
            pVBox->pointerOffscreen = TRUE;
            vbox_vmm_hide_cursor(pScrn, pVBox);
        }
    }
    else
    {
        if (pVBox->pointerOffscreen)
        {
            pVBox->pointerOffscreen = FALSE;
            vbox_vmm_show_cursor(pScrn, pVBox);
        }
    }
#endif
}

static void
vbox_hide_cursor(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = pScrn->driverPrivate;

    TRACE_ENTRY();

    vbox_vmm_hide_cursor(pScrn, pVBox);
}

static void
vbox_show_cursor(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = pScrn->driverPrivate;

    TRACE_ENTRY();

    vbox_vmm_show_cursor(pScrn, pVBox);
}

static void
vbox_load_cursor_image(ScrnInfoPtr pScrn, unsigned char *image)
{
    VBOXPtr pVBox = pScrn->driverPrivate;

    TRACE_ENTRY();

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
    if (VBOX_FAILURE(rc))
    {
        xf86DrvMsg(scrnIndex, X_ERROR, "Could not init VMM request: rc = %d\n", rc);
        xfree(p);
        return NULL;
    }

    m = p + offsetof(VMMDevReqMousePointer, pointerData);
    cp = (CARD32 *)(m + sizeMask);

    dolog ("w=%d h=%d sm=%d sr=%d p=%d\n",
           w, h, (int) sizeMask, (int) sizeRgba, (int) dstPitch);
    dolog ("m=%p c=%p cp=%p\n", m, c, (void *)cp);

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
    reqp->fFlags = VBOX_MOUSE_POINTER_SHAPE;
    reqp->header.size = sizeRequest;

#ifdef DEBUG_X
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
    reqp->fFlags = VBOX_MOUSE_POINTER_SHAPE | VBOX_MOUSE_POINTER_ALPHA;
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
    xf86CursorInfoPtr pCurs;
    Bool rc;

    if (!pVBox->useDevice)
        return FALSE;
    pVBox->pCurs = pCurs = xf86CreateCursorInfoRec();
    if (!pCurs)
        RETERROR(pScrn->scrnIndex, FALSE,
                 "Failed to create X Window cursor information structures for virtual mouse.\n");

    pCurs->MaxWidth = VBOX_MAX_CURSOR_WIDTH;
    pCurs->MaxHeight = VBOX_MAX_CURSOR_HEIGHT;
    pCurs->Flags = HARDWARE_CURSOR_TRUECOLOR_AT_8BPP
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

    rc = xf86InitCursor(pScreen, pCurs);
    if (rc == TRUE)
        return TRUE;
    RETERROR(pScrn->scrnIndex, FALSE, "Failed to enable mouse pointer integration.\n");
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
    int rc;
    int scrnIndex = pScrn->scrnIndex;
    VBOXPtr pVBox = pScrn->driverPrivate;

    if (pVBox->useVbva != TRUE)
        return FALSE;
    rc = VbglR3VideoAccelEnable(true);
    if (RT_FAILURE(rc))
    {
        /* Request not accepted - disable for old hosts. */
        xf86DrvMsg(scrnIndex, X_ERROR,
                   "Unable to activate VirtualBox graphics acceleration "
                   "- the request to the virtual machine failed.  "
                   "You may be running an old version of VirtualBox.\n");
        pVBox->useVbva = FALSE;
        VbglR3VideoAccelEnable(false);
        return FALSE;
    }
    return TRUE;
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
                            uint32_t *pcBits, uint32_t *piDisplay,
                            VBOXPtr pVBox)
{
    if (!pVBox->useDevice)
        return FALSE;
    int rc = VbglR3GetLastDisplayChangeRequest(pcx, pcy, pcBits, piDisplay);
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
vboxHostLikesVideoMode(uint32_t cx, uint32_t cy, uint32_t cBits)
{
    return VbglR3HostLikesVideoMode(cx, cy, cBits);
}
