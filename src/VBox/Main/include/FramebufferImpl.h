/* $Id$ */

/** @file
 *
 * Frame buffer COM class implementation
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_H_FRAMEBUFFERIMPL
#define ____H_H_FRAMEBUFFERIMPL

#include "VirtualBoxBase.h"
#include "VirtualBoxImpl.h"

class ATL_NO_VTABLE Framebuffer :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IFramebuffer)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(Framebuffer, IFramebuffer)

    DECLARE_NOT_AGGREGATABLE (Framebuffer)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (Framebuffer)
        VBOX_DEFAULT_INTERFACE_ENTRIES (IFramebuffer)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (Framebuffer)

    /* IFramebuffer properties */
    STDMETHOD(COMGETTER(Address)) (BYTE **aAddress) = 0;
    STDMETHOD(COMGETTER(Width)) (ULONG *aWidth) = 0;
    STDMETHOD(COMGETTER(Height)) (ULONG *aHeight) = 0;
    STDMETHOD(COMGETTER(BitsPerPixel)) (ULONG *aBitsPerPixel) = 0;
    STDMETHOD(COMGETTER(BytesPerLine)) (ULONG *aBytesPerLine) = 0;
    STDMETHOD(COMGETTER(PixelFormat)) (ULONG *aPixelFormat) = 0;
    STDMETHOD(COMGETTER(UsesGuestVRAM)) (BOOL *aUsesGuestVRAM) = 0;
    STDMETHOD(COMGETTER(HeightReduction)) (ULONG *aHeightReduction) = 0;
    STDMETHOD(COMGETTER(Overlay)) (IFramebufferOverlay **aOverlay) = 0;
    STDMETHOD(COMGETTER(WinId)) (ULONG64 *winId) = 0;

    /* IFramebuffer methods */
    STDMETHOD(Lock)() = 0;
    STDMETHOD(Unlock)() = 0;

    STDMETHOD(RequestResize) (ULONG aScreenId, ULONG aPixelFormat,
                              BYTE *aVRAM, ULONG aBitsPerPixel, ULONG aBytesPerLine,
                              ULONG aWidth, ULONG aHeight,
                              BOOL *aFinished) = 0;

    STDMETHOD(VideoModeSupported) (ULONG aWidth, ULONG aHeight, ULONG aBPP,
                                   BOOL *aSupported) = 0;

    STDMETHOD(GetVisibleRegion)(BYTE *aRectangles, ULONG aCount,
                               ULONG *aCountCopied) = 0;
    STDMETHOD(SetVisibleRegion)(BYTE *aRectangles, ULONG aCount) = 0;

    STDMETHOD(ProcessVHWACommand)(BYTE *pCommand) = 0;
};

#endif // ____H_H_FRAMEBUFFERIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
