/******************************Module*Header*******************************\
*
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
/*
* Based in part on Microsoft DDK sample code
*
*                           *******************
*                           * GDI SAMPLE CODE *
*                           *******************
*
* Module Name: pointer.c                                                   *
*                                                                          *
* This module contains the hardware Pointer support for the framebuffer    *
*                                                                          *
* Copyright (c) 1992-1998 Microsoft Corporation                            *
\**************************************************************************/

#include "driver.h"

#include <VBox/VBoxGuest.h> /* for VBOX_MOUSE_POINTER_* flags */

#ifndef SPS_ALPHA
#define SPS_ALPHA 0x00000010L
#endif

BOOL bCopyColorPointer(
PPDEV ppdev,
SURFOBJ *psoScreen,
SURFOBJ *psoMask,
SURFOBJ *psoColor,
XLATEOBJ *pxlo,
FLONG fl);

BOOL bCopyMonoPointer(
PPDEV ppdev,
SURFOBJ *psoMask);

BOOL bSetHardwarePointerShape(
SURFOBJ  *pso,
SURFOBJ  *psoMask,
SURFOBJ  *psoColor,
XLATEOBJ *pxlo,
LONG      x,
LONG      y,
FLONG     fl);

/******************************Public*Routine******************************\
* DrvMovePointer
*
* Moves the hardware pointer to a new position.
*
\**************************************************************************/

VOID DrvMovePointer
(
    SURFOBJ *pso,
    LONG     x,
    LONG     y,
    RECTL   *prcl
)
{
    PPDEV ppdev = (PPDEV) pso->dhpdev;
    DWORD returnedDataLength;
    VIDEO_POINTER_POSITION NewPointerPosition;

    // We don't use the exclusion rectangle because we only support
    // hardware Pointers. If we were doing our own Pointer simulations
    // we would want to update prcl so that the engine would call us
    // to exclude out pointer before drawing to the pixels in prcl.

    UNREFERENCED_PARAMETER(prcl);

    // Convert the pointer's position from relative to absolute
    // coordinates (this is only significant for multiple board
    // support).

    x -= ppdev->ptlOrg.x;
    y -= ppdev->ptlOrg.y;

    // If x is -1 after the offset then take down the cursor.

    if (x == -1)
    {
        //
        // A new position of (-1,-1) means hide the pointer.
        //

        if (EngDeviceIoControl(ppdev->hDriver,
                               IOCTL_VIDEO_DISABLE_POINTER,
                               NULL,
                               0,
                               NULL,
                               0,
                               &returnedDataLength))
        {
            //
            // Not the end of the world, print warning in checked build.
            //

            DISPDBG((1, "DISP vMoveHardwarePointer failed IOCTL_VIDEO_DISABLE_POINTER\n"));
        }
    }
    else
    {
        NewPointerPosition.Column = (SHORT) x - (SHORT) (ppdev->ptlHotSpot.x);
        NewPointerPosition.Row    = (SHORT) y - (SHORT) (ppdev->ptlHotSpot.y);

        //
        // Call miniport driver to move Pointer.
        //

        if (EngDeviceIoControl(ppdev->hDriver,
                               IOCTL_VIDEO_SET_POINTER_POSITION,
                               &NewPointerPosition,
                               sizeof(VIDEO_POINTER_POSITION),
                               NULL,
                               0,
                               &returnedDataLength))
        {
            //
            // Not the end of the world, print warning in checked build.
            //

            DISPDBG((1, "DISP vMoveHardwarePointer failed IOCTL_VIDEO_SET_POINTER_POSITION\n"));
        }
    }
}

/******************************Public*Routine******************************\
* DrvSetPointerShape
*
* Sets the new pointer shape.
*
\**************************************************************************/

ULONG DrvSetPointerShape
(
    SURFOBJ  *pso,
    SURFOBJ  *psoMask,
    SURFOBJ  *psoColor,
    XLATEOBJ *pxlo,
    LONG      xHot,
    LONG      yHot,
    LONG      x,
    LONG      y,
    RECTL    *prcl,
    FLONG     fl
)
{
    PPDEV   ppdev = (PPDEV) pso->dhpdev;
    DWORD   returnedDataLength;

    DISPDBG((0, "DISP bSetHardwarePointerShape SPS_ALPHA = %d\n", fl & SPS_ALPHA));
    
    // We don't use the exclusion rectangle because we only support
    // hardware Pointers. If we were doing our own Pointer simulations
    // we would want to update prcl so that the engine would call us
    // to exclude out pointer before drawing to the pixels in prcl.
    UNREFERENCED_PARAMETER(prcl);

    if (ppdev->pPointerAttributes == (PVIDEO_POINTER_ATTRIBUTES) NULL)
    {
        // Mini-port has no hardware Pointer support.
        return(SPS_ERROR);
    }

    ppdev->ptlHotSpot.x = xHot;
    ppdev->ptlHotSpot.y = yHot;

    if (!bSetHardwarePointerShape(pso,psoMask,psoColor,pxlo,x,y,fl))
    {
            if (ppdev->fHwCursorActive) {
                ppdev->fHwCursorActive = FALSE;

                if (EngDeviceIoControl(ppdev->hDriver,
                                       IOCTL_VIDEO_DISABLE_POINTER,
                                       NULL,
                                       0,
                                       NULL,
                                       0,
                                       &returnedDataLength)) {

                    DISPDBG((1, "DISP bSetHardwarePointerShape failed IOCTL_VIDEO_DISABLE_POINTER\n"));
                }
            }

            //
            // Mini-port declines to realize this Pointer
            //

            return(SPS_DECLINE);
    }
    else
    {
        ppdev->fHwCursorActive = TRUE;
    }

    return(SPS_ACCEPT_NOEXCLUDE);
}

/******************************Public*Routine******************************\
* bSetHardwarePointerShape
*
* Changes the shape of the Hardware Pointer.
*
* Returns: True if successful, False if Pointer shape can't be hardware.
*
\**************************************************************************/

BOOL bSetHardwarePointerShape(
SURFOBJ  *pso,
SURFOBJ  *psoMask,
SURFOBJ  *psoColor,
XLATEOBJ *pxlo,
LONG      x,
LONG      y,
FLONG     fl)
{
    PPDEV     ppdev = (PPDEV) pso->dhpdev;
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = ppdev->pPointerAttributes;
    DWORD     returnedDataLength;

    if (psoColor != (SURFOBJ *) NULL)
    {
        if ((ppdev->PointerCapabilities.Flags & VIDEO_MODE_COLOR_POINTER) &&
                bCopyColorPointer(ppdev, pso, psoMask, psoColor, pxlo, fl))
        {
            pPointerAttributes->Flags = VIDEO_MODE_COLOR_POINTER;
        } else {
            return(FALSE);
        }

    } else {

        if ((ppdev->PointerCapabilities.Flags & VIDEO_MODE_MONO_POINTER) &&
                bCopyMonoPointer(ppdev, psoMask))
        {
            pPointerAttributes->Flags = VIDEO_MODE_MONO_POINTER;
        } else {
            return(FALSE);
        }
    }

    //
    // Initialize Pointer attributes and position
    //

    if (x == -1)
    {
        /* Pointer should be created invisible */
        pPointerAttributes->Column = -1;
        pPointerAttributes->Row    = -1;
        pPointerAttributes->Enable = VBOX_MOUSE_POINTER_SHAPE;
    }
    else
    {
        /* New coordinates of pointer's hot spot */
        pPointerAttributes->Column = (SHORT)(x) - (SHORT)(ppdev->ptlHotSpot.x);
        pPointerAttributes->Row    = (SHORT)(y) - (SHORT)(ppdev->ptlHotSpot.y);
        pPointerAttributes->Enable = VBOX_MOUSE_POINTER_VISIBLE | VBOX_MOUSE_POINTER_SHAPE;
    }


    /* VBOX: We have to pass to miniport hot spot coordinates and alpha flag.
     * They will be encoded in the pPointerAttributes::Enable field.
     * High word will contain hot spot info and low word - flags.
     */
    
    pPointerAttributes->Enable |= (ppdev->ptlHotSpot.y & 0xFF) << 24;
    pPointerAttributes->Enable |= (ppdev->ptlHotSpot.x & 0xFF) << 16;
    
    if (fl & SPS_ALPHA)
    {
        pPointerAttributes->Enable |= VBOX_MOUSE_POINTER_ALPHA;
    }
    
    //
    // set animate flags
    //

    if (fl & SPS_ANIMATESTART) {
        pPointerAttributes->Flags |= VIDEO_MODE_ANIMATE_START;
    } else if (fl & SPS_ANIMATEUPDATE) {
        pPointerAttributes->Flags |= VIDEO_MODE_ANIMATE_UPDATE;
    }
    

    //
    // Set the new Pointer shape.
    //

    if (EngDeviceIoControl(ppdev->hDriver,
                           IOCTL_VIDEO_SET_POINTER_ATTR,
                           pPointerAttributes,
                           ppdev->cjPointerAttributes,
                           NULL,
                           0,
                           &returnedDataLength)) {

        DISPDBG((1, "DISP:Failed IOCTL_VIDEO_SET_POINTER_ATTR call\n"));
        return(FALSE);
    }
    
    //
    // Set new pointer position
    //
    
    if (x != -1) {
        VIDEO_POINTER_POSITION vpp;
        
        vpp.Column = pPointerAttributes->Column;
        vpp.Row = pPointerAttributes->Row;
        
        if (EngDeviceIoControl(ppdev->hDriver,
                               IOCTL_VIDEO_SET_POINTER_POSITION,
                               &vpp,
                               sizeof (vpp),
                               NULL,
                               0,
                               &returnedDataLength)) {
            
            // Should never fail, informational message.
            DISPDBG((1, "DISP:Failed IOCTL_VIDEO_SET_POINTER_POSITION call\n"));
        }    
    }
    
    return(TRUE);
}

/******************************Public*Routine******************************\
* bCopyMonoPointer
*
* Copies two monochrome masks into a buffer of the maximum size handled by the
* miniport, with any extra bits set to 0.  The masks are converted to topdown
* form if they aren't already.  Returns TRUE if we can handle this pointer in
* hardware, FALSE if not.
*
\**************************************************************************/

BOOL bCopyMonoPointer(
    PPDEV    ppdev,
    SURFOBJ *psoMask)
{
    PBYTE pjSrc = NULL;
    
    ULONG cy = 0;
    
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = ppdev->pPointerAttributes;
    
    PBYTE pjDstAnd = pPointerAttributes->Pixels;
    ULONG cjAnd = 0;
    PBYTE pjDstXor = pPointerAttributes->Pixels;

    ULONG cxSrc = psoMask->sizlBitmap.cx;
    ULONG cySrc = psoMask->sizlBitmap.cy / 2; /* /2 because both masks are in there */
    
    // Make sure the new pointer isn't too big to handle,
    // strip the size to 64x64 if necessary
    if (cxSrc > ppdev->PointerCapabilities.MaxWidth)
    {
        cxSrc = ppdev->PointerCapabilities.MaxWidth;
    }
    
    if (cySrc > ppdev->PointerCapabilities.MaxHeight)
    {
        cySrc = ppdev->PointerCapabilities.MaxWidth;
    }

    /* Size of AND mask in bytes */
    cjAnd = ((cxSrc + 7) / 8) * cySrc;
    
    /* Pointer to XOR mask is 4-bytes aligned */
    pjDstXor += (cjAnd + 3) & ~3;
    
    pPointerAttributes->Width = cxSrc;
    pPointerAttributes->Height = cySrc;
    pPointerAttributes->WidthInBytes = cxSrc * 4;
    
    /* Init AND mask to 1 */
    RtlFillMemory (pjDstAnd, cjAnd, 0xFF);
    
    /* 
     * Copy AND mask.
     */
         
    DISPDBG((0, "DISP bCopyMonoPointer going to copy AND mask\n"));
        
    pjSrc = (PBYTE)psoMask->pvScan0;
        
    for (cy = 0; cy < cySrc; cy++)
    {
        RtlCopyMemory (pjDstAnd, pjSrc, (cxSrc + 7) / 8);
    
        // Point to next source and dest scans
        pjSrc += psoMask->lDelta;
        pjDstAnd += (cxSrc + 7) / 8;
    }
        
    DISPDBG((0, "DISP bCopyMonoPointer AND mask copied\n"));
        
    DISPDBG((0, "DISP bCopyMonoPointer going to create RGB0 XOR mask\n"));
    
    for (cy = 0; cy < cySrc; ++cy)
    {
        ULONG cx;
        
        UCHAR bitmask = 0x80;
                        
        for (cx = 0; cx < cxSrc; cx++, bitmask >>= 1)
        {
            if (bitmask == 0)
            {
                bitmask = 0x80;
            }
        
            if (pjSrc[cx / 8] & bitmask)
            {
                *(ULONG *)&pjDstXor[cx * 4] = 0x00FFFFFF;
            }
            else
            {
                *(ULONG *)&pjDstXor[cx * 4] = 0;
            }
        }

        // Point to next source and dest scans
        pjSrc += psoMask->lDelta;
        pjDstXor += cxSrc * 4;
    }
    
    DISPDBG((0, "DISP bCopyMonoPointer created RGB0 XOR mask\n"));

    return(TRUE);
}

/******************************Public*Routine******************************\
* bCopyColorPointer
*
* Copies the mono and color masks into the buffer of maximum size
* handled by the miniport with any extra bits set to 0. Color translation
* is handled at this time. The masks are converted to topdown form if they
* aren't already.  Returns TRUE if we can handle this pointer in  hardware,
* FALSE if not.
*
\**************************************************************************/
BOOL bCopyColorPointer(
PPDEV ppdev,
SURFOBJ *psoScreen,
SURFOBJ *psoMask,
SURFOBJ *psoColor,
XLATEOBJ *pxlo,
FLONG fl)
{
    /* Format of "hardware" pointer is:
     * 1 bpp AND mask with byte aligned scanlines,
     * B G R A bytes of XOR mask that starts on the next 4 byte aligned offset after AND mask.
     *
     * If fl & SPS_ALPHA then A bytes contain alpha channel information.
     * Otherwise A bytes are undefined (but will be 0).
     *
     */
     
    /* To simplify this function we use the following method:
     *   for pointers with alpha channel
     *     we have BGRA values in psoColor and will simply copy them to pPointerAttributes->Pixels
     *   for color pointers
     *     always convert supplied bitmap to 32 bit BGR0
     *     copy AND mask and new BGR0 XOR mask to pPointerAttributes->Pixels
     */
    
    HSURF hsurf32bpp  = NULL;
    SURFOBJ *pso32bpp = NULL;

    PBYTE pjSrcAnd = NULL;
    PBYTE pjSrcXor = NULL;
    
    ULONG cy = 0;
    
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = ppdev->pPointerAttributes;
    
    PBYTE pjDstAnd = pPointerAttributes->Pixels;
    ULONG cjAnd = 0;
    PBYTE pjDstXor = pPointerAttributes->Pixels;

    ULONG cxSrc = psoColor->sizlBitmap.cx;
    ULONG cySrc = psoColor->sizlBitmap.cy;
    
    // Make sure the new pointer isn't too big to handle,
    // strip the size to 64x64 if necessary
    if (cxSrc > ppdev->PointerCapabilities.MaxWidth)
    {
        cxSrc = ppdev->PointerCapabilities.MaxWidth;
    }
    
    if (cySrc > ppdev->PointerCapabilities.MaxHeight)
    {
        cySrc = ppdev->PointerCapabilities.MaxWidth;
    }

    /* Size of AND mask in bytes */
    cjAnd = ((cxSrc + 7) / 8) * cySrc;
    
    /* Pointer to XOR mask is 4-bytes aligned */
    pjDstXor += (cjAnd + 3) & ~3;
    
    pPointerAttributes->Width = cxSrc;
    pPointerAttributes->Height = cySrc;
    pPointerAttributes->WidthInBytes = cxSrc * 4;
    
    /* Init AND mask to 1 */
    RtlFillMemory (pjDstAnd, cjAnd, 0xFF);

    if (fl & SPS_ALPHA)
    {
        PBYTE pjSrcAlpha = (PBYTE)psoColor->pvScan0;
        
        DISPDBG((0, "DISP bCopyColorPointer SPS_ALPHA\n"));
        
        pso32bpp = psoColor;
        
        /* 
         * Emulate AND mask to provide viewable mouse pointer for 
         * hardware which does not support alpha channel.
         */
        
        DISPDBG((0, "DISP bCopyColorPointer going to emulate AND mask\n"));
        
        for (cy = 0; cy < cySrc; cy++)
        {
            ULONG cx;
            
            UCHAR bitmask = 0x80;
            
            for (cx = 0; cx < cxSrc; cx++, bitmask >>= 1)
            {
                if (bitmask == 0)
                {
                    bitmask = 0x80;
                }
                
                if (pjSrcAlpha[cx * 4 + 3] > 0x7f)
                {
                    pjDstAnd[cx / 8] &= ~bitmask;
                }
            }
    
            // Point to next source and dest scans
            pjSrcAlpha += pso32bpp->lDelta;
            pjDstAnd += (cxSrc + 7) / 8;
        }
        
        DISPDBG((0, "DISP bCopyColorPointer AND mask emulated\n"));
    }
    else
    {
        if (!psoMask)
        {
            /* This can not be, mask must be supplied for a color pointer. */
            return (FALSE);
        }
        
        /* 
         * Copy AND mask.
         */
         
        DISPDBG((0, "DISP bCopyColorPointer going to copy AND mask\n"));
        
        pjSrcAnd = (PBYTE)psoMask->pvScan0;
        
        for (cy = 0; cy < cySrc; cy++)
        {
            RtlCopyMemory (pjDstAnd, pjSrcAnd, (cxSrc + 7) / 8);
    
            // Point to next source and dest scans
            pjSrcAnd += psoMask->lDelta;
            pjDstAnd += (cxSrc + 7) / 8;
        }
        
        DISPDBG((0, "DISP bCopyColorPointer AND mask copied\n"));
        
        /* 
         * Convert given psoColor to 32 bit BGR0.
         */
         
        DISPDBG((0, "DISP bCopyColorPointer psoScreen t = %d, f = %d, psoColor t = %d, f = %d, pxlo = %p, psoColor->lDelta = %d, ->cx = %d\n",
                 psoScreen->iType, psoScreen->iBitmapFormat, psoColor->iType, psoColor->iBitmapFormat, pxlo, psoColor->lDelta, psoColor->sizlBitmap.cx));
        
        if (psoColor->iType == STYPE_BITMAP
            && psoColor->iBitmapFormat == BMF_32BPP)
        {
            /* The psoColor is already in desired format */
            DISPDBG((0, "DISP bCopyColorPointer XOR mask already in 32 bpp\n"));
            pso32bpp = psoColor;
        }
        else
        {
            HSURF hsurfBitmap  = NULL;
            SURFOBJ *psoBitmap = NULL;
            
            SIZEL sizl = psoColor->sizlBitmap;
            
            if ((pxlo != NULL && pxlo->flXlate != XO_TRIVIAL)
                || (psoColor->iType != STYPE_BITMAP))
            {
                /* Convert the unknown format to a screen format bitmap. */
                
                RECTL rclDst;
                POINTL ptlSrc;
            
                DISPDBG((0, "DISP bCopyColorPointer going to convert XOR mask to bitmap\n"));
            
                hsurfBitmap = (HSURF)EngCreateBitmap (sizl, 0, psoScreen->iBitmapFormat, BMF_TOPDOWN, NULL);
            
                if (hsurfBitmap == NULL)
                {
                    return FALSE;
                }
             
                psoBitmap = EngLockSurface (hsurfBitmap);
            
                if (psoBitmap == NULL)
                {
                    EngDeleteSurface (hsurfBitmap);
                    return FALSE;
                }
            
                /* Now do the bitmap conversion using EngCopyBits(). */
            
                rclDst.left = 0;
                rclDst.top = 0;
                rclDst.right = sizl.cx;
                rclDst.bottom = sizl.cy;
            
                ptlSrc.x = 0;
                ptlSrc.y = 0;
                
                if (!EngCopyBits (psoBitmap, psoColor, NULL, pxlo, &rclDst, &ptlSrc))
                {
                    EngUnlockSurface (psoBitmap);
                    EngDeleteSurface (hsurfBitmap);
                    return FALSE;
                }
            
                DISPDBG((0, "DISP bCopyColorPointer XOR mask converted to bitmap\n"));
            }
            else
            {
                DISPDBG((0, "DISP bCopyColorPointer XOR mask is already a bitmap\n"));
                psoBitmap = psoColor;
            }
            
            /* Create 32 bpp surface for XOR mask */
            hsurf32bpp = (HSURF)EngCreateBitmap (sizl, 0, BMF_32BPP, BMF_TOPDOWN, NULL);
            
            if (hsurf32bpp != NULL)
            {
                pso32bpp = EngLockSurface (hsurf32bpp);
            
                if (pso32bpp == NULL)
                {
                    EngDeleteSurface (hsurf32bpp);
                    hsurf32bpp = NULL;
                }
            }
            
            if (pso32bpp)
            {
                /* Convert psoBitmap bits to pso32bpp bits for known formats */
                if (psoBitmap->iBitmapFormat == BMF_8BPP && ppdev->pPal)
                {
                    PBYTE src = (PBYTE)psoBitmap->pvScan0;
                    PBYTE dst = (PBYTE)pso32bpp->pvScan0;
                    
                    PPALETTEENTRY pPal = ppdev->pPal;
                    ULONG cPalette = 256; /* 256 is hardcoded in the driver in palette.c */
                    
                    DISPDBG((0, "DISP bCopyColorPointer XOR mask conv 8 bpp to 32 bpp palette: %d entries\n", cPalette));
                    
                    for (cy = 0; cy < (ULONG)sizl.cy; cy++)
                    {
                        ULONG cx;
                        
                        PBYTE d = dst;
                        
                        for (cx = 0; cx < (ULONG)sizl.cx; cx++)
                        {
                            BYTE index = src[cx];
                            
                            *d++ = pPal[index].peBlue;  /* B */
                            *d++ = pPal[index].peGreen; /* G */
                            *d++ = pPal[index].peRed;   /* R */
                            *d++ = 0;                   /* destination is 32 bpp */
                        }

                        /* Point to next source and dest scans */
                        src += psoBitmap->lDelta;
                        dst += pso32bpp->lDelta;
                    }
                    
                    DISPDBG((0, "DISP bCopyColorPointer XOR mask conv 8 bpp to 32 bpp completed\n"));
                }
                else if (psoBitmap->iBitmapFormat == BMF_16BPP)
                {
                    PBYTE src = (PBYTE)psoBitmap->pvScan0;
                    PBYTE dst = (PBYTE)pso32bpp->pvScan0;
                    
                    DISPDBG((0, "DISP bCopyColorPointer XOR mask conv 16 bpp to 32 bpp\n"));
                    
                    for (cy = 0; cy < (ULONG)sizl.cy; cy++)
                    {
                        ULONG cx;
                    
                        PBYTE d = dst;
                        
                        for (cx = 0; cx < (ULONG)sizl.cx; cx++)
                        {
                            USHORT usSrc = *(USHORT *)&src[cx * 2];
                            
                            *d++ = (BYTE)( usSrc        << 3); /* B */
                            *d++ = (BYTE)((usSrc >> 5)  << 2); /* G */
                            *d++ = (BYTE)((usSrc >> 11) << 3); /* R */
                            *d++ = 0;                          /* destination is 32 bpp */
                        }

                        /* Point to next source and dest scans */
                        src += psoBitmap->lDelta;
                        dst += pso32bpp->lDelta;
                    }
                    
                    DISPDBG((0, "DISP bCopyColorPointer XOR mask conv 16 bpp to 32 bpp completed\n"));
                }
                else if (psoBitmap->iBitmapFormat == BMF_24BPP)
                {
                    PBYTE src = (PBYTE)psoBitmap->pvScan0;
                    PBYTE dst = (PBYTE)pso32bpp->pvScan0;
                    
                    DISPDBG((0, "DISP bCopyColorPointer XOR mask conv 24 bpp to 32 bpp\n"));
                    
                    for (cy = 0; cy < (ULONG)sizl.cy; cy++)
                    {
                        ULONG cx;
                        
                        PBYTE s = src;
                        PBYTE d = dst;
                        
                        for (cx = 0; cx < (ULONG)sizl.cx; cx++)
                        {
                            *d++ = *s++; /* B */
                            *d++ = *s++; /* G */
                            *d++ = *s++; /* R */
                            *d++ = 0;    /* destination is 32 bpp */
                        }

                        /* Point to next source and dest scans */
                        src += psoBitmap->lDelta;
                        dst += pso32bpp->lDelta;
                    }
                    
                    DISPDBG((0, "DISP bCopyColorPointer XOR mask conv 24 bpp to 32 bpp completed\n"));
                }
                else if (psoBitmap->iBitmapFormat == BMF_32BPP)
                {
                    DISPDBG((0, "DISP bCopyColorPointer XOR mask conv 32 bpp to 32 bpp, pso32bpp->cjBits = %d, psoBitmap->cjBits = %d\n", pso32bpp->cjBits, psoBitmap->cjBits));
                    
                    RtlCopyMemory (pso32bpp->pvBits, psoBitmap->pvBits, min(pso32bpp->cjBits, psoBitmap->cjBits));
                    
                    DISPDBG((0, "DISP bCopyColorPointer XOR mask conv 32 bpp to 32 bpp completed\n"));
                }
                else
                {
                    DISPDBG((0, "DISP bCopyColorPointer XOR mask unsupported bpp\n"));
                    
                    EngUnlockSurface (pso32bpp);
                    pso32bpp = NULL;
                    EngDeleteSurface (hsurf32bpp);
                    hsurf32bpp = NULL;
                }
            }
            
            if (hsurfBitmap)
            {
                EngUnlockSurface (psoBitmap);
                psoBitmap = NULL;
                EngDeleteSurface (hsurfBitmap);
                hsurfBitmap = NULL;
            }
        }
    }
    
    if (!pso32bpp)
    {
         return (FALSE);
    }
    
    /* 
     * pso is 32 bit BGRX bitmap. Copy it to Pixels 
     */
    
    pjSrcXor = (PBYTE)pso32bpp->pvScan0;
    
    for (cy = 0; cy < cySrc; cy++)
    {
        /* 32 bit bitmap is being copied */
        RtlCopyMemory (pjDstXor, pjSrcXor, cxSrc * 4); 

        /* Point to next source and dest scans */
        pjSrcXor += pso32bpp->lDelta;
        pjDstXor += pPointerAttributes->WidthInBytes;
    }
    
    if (pso32bpp != psoColor)
    {
        /* Deallocate the temporary 32 bit pso */
        EngUnlockSurface (pso32bpp);
        EngDeleteSurface (hsurf32bpp);
    }
    
    return (TRUE);
}


/******************************Public*Routine******************************\
* bInitPointer
*
* Initialize the Pointer attributes.
*
\**************************************************************************/

BOOL bInitPointer(PPDEV ppdev, DEVINFO *pdevinfo)
{
    DWORD    returnedDataLength;

    ppdev->pPointerAttributes = (PVIDEO_POINTER_ATTRIBUTES) NULL;
    ppdev->cjPointerAttributes = 0; // initialized in screen.c

    //
    // Ask the miniport whether it provides pointer support.
    //

    if (EngDeviceIoControl(ppdev->hDriver,
                           IOCTL_VIDEO_QUERY_POINTER_CAPABILITIES,
                           &ppdev->ulMode,
                           sizeof(PVIDEO_MODE),
                           &ppdev->PointerCapabilities,
                           sizeof(ppdev->PointerCapabilities),
                           &returnedDataLength))
    {
         return(FALSE);
    }

    //
    // If neither mono nor color hardware pointer is supported, there's no
    // hardware pointer support and we're done.
    //

    if ((!(ppdev->PointerCapabilities.Flags & VIDEO_MODE_MONO_POINTER)) &&
        (!(ppdev->PointerCapabilities.Flags & VIDEO_MODE_COLOR_POINTER)))
    {
        return(TRUE);
    }

    //
    // Note: The buffer itself is allocated after we set the
    // mode. At that time we know the pixel depth and we can
    // allocate the correct size for the color pointer if supported.
    //

    //
    // Set the asynchronous support status (async means miniport is capable of
    // drawing the Pointer at any time, with no interference with any ongoing
    // drawing operation)
    //

    if (ppdev->PointerCapabilities.Flags & VIDEO_MODE_ASYNC_POINTER)
    {
       pdevinfo->flGraphicsCaps |= GCAPS_ASYNCMOVE;
    }
    else
    {
       pdevinfo->flGraphicsCaps &= ~GCAPS_ASYNCMOVE;
    }
    
    /* VBOX supports pointers with alpha channel */
    pdevinfo->flGraphicsCaps2 |= GCAPS2_ALPHACURSOR;

    return(TRUE);
}

