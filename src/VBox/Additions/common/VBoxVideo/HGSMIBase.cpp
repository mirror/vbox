/* $Id$ */
/** @file
 * VirtualBox Video driver, common code - HGSMI guest-to-host communication.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <VBoxVideoGuest.h>
#include <VBoxVideoVBE.h>
#include <VBoxVideoIPRT.h>

/** Detect whether HGSMI is supported by the host. */
DECLHIDDEN(bool) VBoxHGSMIIsSupported(void)
{
    uint16_t DispiId;

    VBVO_PORT_WRITE_U16(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ID);
    VBVO_PORT_WRITE_U16(VBE_DISPI_IOPORT_DATA, VBE_DISPI_ID_HGSMI);

    DispiId = VBVO_PORT_READ_U16(VBE_DISPI_IOPORT_DATA);

    return (DispiId == VBE_DISPI_ID_HGSMI);
}


/**
 * Inform the host of the location of the host flags in VRAM via an HGSMI
 * command.
 * @returns  IPRT status value.
 * @returns  VERR_NOT_IMPLEMENTED  if the host does not support the command.
 * @returns  VERR_NO_MEMORY        if a heap allocation fails.
 * @param    pCtx                  the context of the guest heap to use.
 * @param    offLocation           the offset chosen for the flags withing guest
 *                                 VRAM.
 */
DECLHIDDEN(int) VBoxHGSMIReportFlagsLocation(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                                             HGSMIOFFSET offLocation)
{
    HGSMIBUFFERLOCATION *p;
    int rc = VINF_SUCCESS;

    /* Allocate the IO buffer. */
    p = (HGSMIBUFFERLOCATION *)VBoxHGSMIBufferAlloc(pCtx,
                                              sizeof(HGSMIBUFFERLOCATION),
                                              HGSMI_CH_HGSMI,
                                              HGSMI_CC_HOST_FLAGS_LOCATION);
    if (p)
    {
        /* Prepare data to be sent to the host. */
        p->offLocation = offLocation;
        p->cbLocation  = sizeof(HGSMIHOSTFLAGS);
        rc = VBoxHGSMIBufferSubmit(pCtx, p);
        /* Free the IO buffer. */
        VBoxHGSMIBufferFree(pCtx, p);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


/**
 * Notify the host of HGSMI-related guest capabilities via an HGSMI command.
 * @returns  IPRT status value.
 * @returns  VERR_NOT_IMPLEMENTED  if the host does not support the command.
 * @returns  VERR_NO_MEMORY        if a heap allocation fails.
 * @param    pCtx                  the context of the guest heap to use.
 * @param    fCaps                 the capabilities to report, see VBVACAPS.
 */
DECLHIDDEN(int) VBoxHGSMISendCapsInfo(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                                      uint32_t fCaps)
{
    VBVACAPS *pCaps;
    int rc = VINF_SUCCESS;

    /* Allocate the IO buffer. */
    pCaps = (VBVACAPS *)VBoxHGSMIBufferAlloc(pCtx,
                                       sizeof(VBVACAPS), HGSMI_CH_VBVA,
                                       VBVA_INFO_CAPS);

    if (pCaps)
    {
        /* Prepare data to be sent to the host. */
        pCaps->rc    = VERR_NOT_IMPLEMENTED;
        pCaps->fCaps = fCaps;
        rc = VBoxHGSMIBufferSubmit(pCtx, pCaps);
        if (RT_SUCCESS(rc))
        {
            AssertRC(pCaps->rc);
            rc = pCaps->rc;
        }
        /* Free the IO buffer. */
        VBoxHGSMIBufferFree(pCtx, pCaps);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


/**
 * Get the information needed to map the basic communication structures in
 * device memory into our address space.  All pointer parameters are optional.
 *
 * @param  cbVRAM               how much video RAM is allocated to the device
 * @param  poffVRAMBaseMapping  where to save the offset from the start of the
 *                              device VRAM of the whole area to map
 * @param  pcbMapping           where to save the mapping size
 * @param  poffGuestHeapMemory  where to save the offset into the mapped area
 *                              of the guest heap backing memory
 * @param  pcbGuestHeapMemory   where to save the size of the guest heap
 *                              backing memory
 * @param  poffHostFlags        where to save the offset into the mapped area
 *                              of the host flags
 */
DECLHIDDEN(void) VBoxHGSMIGetBaseMappingInfo(uint32_t cbVRAM,
                                             uint32_t *poffVRAMBaseMapping,
                                             uint32_t *pcbMapping,
                                             uint32_t *poffGuestHeapMemory,
                                             uint32_t *pcbGuestHeapMemory,
                                             uint32_t *poffHostFlags)
{
    AssertPtrNullReturnVoid(poffVRAMBaseMapping);
    AssertPtrNullReturnVoid(pcbMapping);
    AssertPtrNullReturnVoid(poffGuestHeapMemory);
    AssertPtrNullReturnVoid(pcbGuestHeapMemory);
    AssertPtrNullReturnVoid(poffHostFlags);
    if (poffVRAMBaseMapping)
        *poffVRAMBaseMapping = cbVRAM - VBVA_ADAPTER_INFORMATION_SIZE;
    if (pcbMapping)
        *pcbMapping = VBVA_ADAPTER_INFORMATION_SIZE;
    if (poffGuestHeapMemory)
        *poffGuestHeapMemory = 0;
    if (pcbGuestHeapMemory)
        *pcbGuestHeapMemory =   VBVA_ADAPTER_INFORMATION_SIZE
                              - sizeof(HGSMIHOSTFLAGS);
    if (poffHostFlags)
        *poffHostFlags =   VBVA_ADAPTER_INFORMATION_SIZE
                         - sizeof(HGSMIHOSTFLAGS);
}


/** Sanity test on first call.  We do not worry about concurrency issues. */
static int testQueryConf(PHGSMIGUESTCOMMANDCONTEXT pCtx)
{
    static bool cOnce = false;
    uint32_t ulValue = 0;
    int rc;

    if (cOnce)
        return VINF_SUCCESS;
    cOnce = true;
    rc = VBoxQueryConfHGSMI(pCtx, UINT32_MAX, &ulValue);
    if (RT_SUCCESS(rc) && ulValue == UINT32_MAX)
        return VINF_SUCCESS;
    cOnce = false;
    if (RT_FAILURE(rc))
        return rc;
    return VERR_INTERNAL_ERROR;
}


/**
 * Query the host for an HGSMI configuration parameter via an HGSMI command.
 * @returns iprt status value
 * @param  pCtx      the context containing the heap used
 * @param  u32Index  the index of the parameter to query,
 *                   @see VBVACONF32::u32Index
 * @param  u32DefValue defaut value
 * @param  pulValue  where to store the value of the parameter on success
 */
DECLHIDDEN(int) VBoxQueryConfHGSMIDef(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                                      uint32_t u32Index, uint32_t u32DefValue, uint32_t *pulValue)
{
    int rc = VINF_SUCCESS;
    VBVACONF32 *p;
    // LogFunc(("u32Index = %d\n", u32Index));

    rc = testQueryConf(pCtx);
    if (RT_FAILURE(rc))
        return rc;
    /* Allocate the IO buffer. */
    p = (VBVACONF32 *)VBoxHGSMIBufferAlloc(pCtx,
                                     sizeof(VBVACONF32), HGSMI_CH_VBVA,
                                     VBVA_QUERY_CONF32);
    if (p)
    {
        /* Prepare data to be sent to the host. */
        p->u32Index = u32Index;
        p->u32Value = u32DefValue;
        rc = VBoxHGSMIBufferSubmit(pCtx, p);
        if (RT_SUCCESS(rc))
        {
            *pulValue = p->u32Value;
            // LogFunc(("u32Value = %d\n", p->u32Value));
        }
        /* Free the IO buffer. */
        VBoxHGSMIBufferFree(pCtx, p);
    }
    else
        rc = VERR_NO_MEMORY;
    // LogFunc(("rc = %d\n", rc));
    return rc;
}

DECLHIDDEN(int) VBoxQueryConfHGSMI(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                                   uint32_t u32Index, uint32_t *pulValue)
{
    return VBoxQueryConfHGSMIDef(pCtx, u32Index, UINT32_MAX, pulValue);
}

/**
 * Pass the host a new mouse pointer shape via an HGSMI command.
 *
 * @returns  success or failure
 * @param  pCtx      the context containing the heap to be used
 * @param  fFlags    cursor flags, @see VMMDevReqMousePointer::fFlags
 * @param  cHotX     horizontal position of the hot spot
 * @param  cHotY     vertical position of the hot spot
 * @param  cWidth    width in pixels of the cursor
 * @param  cHeight   height in pixels of the cursor
 * @param  pPixels   pixel data, @see VMMDevReqMousePointer for the format
 * @param  cbLength  size in bytes of the pixel data
 */
DECLHIDDEN(int)  VBoxHGSMIUpdatePointerShape(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                                             uint32_t fFlags,
                                             uint32_t cHotX,
                                             uint32_t cHotY,
                                             uint32_t cWidth,
                                             uint32_t cHeight,
                                             uint8_t *pPixels,
                                             uint32_t cbLength)
{
    VBVAMOUSEPOINTERSHAPE *p;
    uint32_t cbData = 0;
    int rc = VINF_SUCCESS;

    if (fFlags & VBOX_MOUSE_POINTER_SHAPE)
    {
        /* Size of the pointer data: sizeof (AND mask) + sizeof (XOR_MASK) */
        cbData = ((((cWidth + 7) / 8) * cHeight + 3) & ~3)
                 + cWidth * 4 * cHeight;
        /* If shape is supplied, then always create the pointer visible.
         * See comments in 'vboxUpdatePointerShape'
         */
        fFlags |= VBOX_MOUSE_POINTER_VISIBLE;
    }
    // LogFlowFunc(("cbData %d, %dx%d\n", cbData, cWidth, cHeight));
    if (cbData > cbLength)
    {
        // LogFunc(("calculated pointer data size is too big (%d bytes, limit %d)\n",
        //          cbData, cbLength));
        return VERR_INVALID_PARAMETER;
    }
    /* Allocate the IO buffer. */
    p = (VBVAMOUSEPOINTERSHAPE *)VBoxHGSMIBufferAlloc(pCtx,
                                                  sizeof(VBVAMOUSEPOINTERSHAPE)
                                                + cbData,
                                                HGSMI_CH_VBVA,
                                                VBVA_MOUSE_POINTER_SHAPE);
    if (p)
    {
        /* Prepare data to be sent to the host. */
        /* Will be updated by the host. */
        p->i32Result = VINF_SUCCESS;
        /* We have our custom flags in the field */
        p->fu32Flags = fFlags;
        p->u32HotX   = cHotX;
        p->u32HotY   = cHotY;
        p->u32Width  = cWidth;
        p->u32Height = cHeight;
        if (p->fu32Flags & VBOX_MOUSE_POINTER_SHAPE)
            /* Copy the actual pointer data. */
            memcpy (p->au8Data, pPixels, cbData);
        rc = VBoxHGSMIBufferSubmit(pCtx, p);
        if (RT_SUCCESS(rc))
            rc = p->i32Result;
        /* Free the IO buffer. */
        VBoxHGSMIBufferFree(pCtx, p);
    }
    else
        rc = VERR_NO_MEMORY;
    // LogFlowFunc(("rc %d\n", rc));
    return rc;
}


/**
 * Report the guest cursor position.  The host may wish to use this information
 * to re-position its own cursor (though this is currently unlikely).  The
 * current host cursor position is returned.
 * @param  pCtx             The context containing the heap used.
 * @param  fReportPosition  Are we reporting a position?
 * @param  x                Guest cursor X position.
 * @param  y                Guest cursor Y position.
 * @param  pxHost           Host cursor X position is stored here.  Optional.
 * @param  pyHost           Host cursor Y position is stored here.  Optional.
 * @returns  iprt status code.
 * @returns  VERR_NO_MEMORY      HGSMI heap allocation failed.
 */
DECLHIDDEN(int) VBoxHGSMICursorPosition(PHGSMIGUESTCOMMANDCONTEXT pCtx, bool fReportPosition, uint32_t x, uint32_t y,
                                        uint32_t *pxHost, uint32_t *pyHost)
{
    int rc = VINF_SUCCESS;
    VBVACURSORPOSITION *p;
    // Log(("%s: x=%u, y=%u\n", __PRETTY_FUNCTION__, (unsigned)x, (unsigned)y));

    /* Allocate the IO buffer. */
    p = (VBVACURSORPOSITION *)VBoxHGSMIBufferAlloc(pCtx, sizeof(VBVACURSORPOSITION), HGSMI_CH_VBVA, VBVA_CURSOR_POSITION);
    if (p)
    {
        /* Prepare data to be sent to the host. */
        p->fReportPosition = fReportPosition ? 1 : 0;
        p->x = x;
        p->y = y;
        rc = VBoxHGSMIBufferSubmit(pCtx, p);
        if (RT_SUCCESS(rc))
        {
            if (pxHost)
                *pxHost = p->x;
            if (pyHost)
                *pyHost = p->y;
            // Log(("%s: return: x=%u, y=%u\n", __PRETTY_FUNCTION__, (unsigned)p->x, (unsigned)p->y));
        }
        /* Free the IO buffer. */
        VBoxHGSMIBufferFree(pCtx, p);
    }
    else
        rc = VERR_NO_MEMORY;
    // LogFunc(("rc = %d\n", rc));
    return rc;
}


/** @todo Mouse pointer position to be read from VMMDev memory, address of the memory region
 * can be queried from VMMDev via an IOCTL. This VMMDev memory region will contain
 * host information which is needed by the guest.
 *
 * Reading will not cause a switch to the host.
 *
 * Have to take into account:
 *  * synchronization: host must write to the memory only from EMT,
 *    large structures must be read under flag, which tells the host
 *    that the guest is currently reading the memory (OWNER flag?).
 *  * guest writes: may be allocate a page for the host info and make
 *    the page readonly for the guest.
 *  * the information should be available only for additions drivers.
 *  * VMMDev additions driver will inform the host which version of the info it expects,
 *    host must support all versions.
 *
 */
