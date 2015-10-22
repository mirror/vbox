/* $Id$ */
/** @file
 * VBox Qt GUI - Realtime Dock Icon Preview
 */

/*
 * Copyright (C) 2009-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
#include <QStyle>

/* GUI includes: */
# include "UIAbstractDockIconPreview.h"
# include "UIConverter.h"
# include "UIExtraDataManager.h"
# include "UIFrameBuffer.h"
# include "UIMachineLogic.h"
# include "UIMachineView.h"
# include "UISession.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "COMEnums.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIAbstractDockIconPreview::UIAbstractDockIconPreview(UISession * /* pSession */, const QPixmap& /* overlayImage */)
{
}

void UIAbstractDockIconPreview::updateDockPreview(UIFrameBuffer *pFrameBuffer)
{
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    Assert(cs);
    /* Create the image copy of the framebuffer */
    CGDataProviderRef dp = CGDataProviderCreateWithData(pFrameBuffer, pFrameBuffer->address(), pFrameBuffer->bitsPerPixel() / 8 * pFrameBuffer->width() * pFrameBuffer->height(), NULL);
    Assert(dp);
    CGImageRef ir = CGImageCreate(pFrameBuffer->width(), pFrameBuffer->height(), 8, 32, pFrameBuffer->bytesPerLine(), cs,
                                  kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host, dp, 0, false,
                                  kCGRenderingIntentDefault);
    Assert(ir);

    /* Update the dock preview icon */
    updateDockPreview(ir);

    /* Release the temp data and image */
    CGImageRelease(ir);
    CGDataProviderRelease(dp);
    CGColorSpaceRelease(cs);
}

UIAbstractDockIconPreviewHelper::UIAbstractDockIconPreviewHelper(UISession *pSession, const QPixmap& overlayImage)
    : m_pSession(pSession)
    , m_dockIconRect(CGRectMake(0, 0, 128, 128))
    , m_dockMonitor(NULL)
    , m_dockMonitorGlossy(NULL)
    , m_updateRect(CGRectMake(0, 0, 0, 0))
    , m_monitorRect(CGRectMake(0, 0, 0, 0))
{
    m_overlayImage   = ::darwinToCGImageRef(&overlayImage);
    Assert(m_overlayImage);

    /* Determine desired icon size for the state-overlay: */
    const QStyle *pStyle = QApplication::style();
    const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);
    const QSize iconSize = QSize(iIconMetric, iIconMetric);

    /* Prepare 'Paused' state-overlay: */
    const QPixmap statePaused = gpConverter->toIcon(KMachineState_Paused).pixmap(iconSize);
    m_statePaused = ::darwinToCGImageRef(&statePaused);
    Assert(m_statePaused);

    /* Prepare 'Saving' state-overlay: */
    const QPixmap stateSaving = gpConverter->toIcon(KMachineState_Saving).pixmap(iconSize);
    m_stateSaving = ::darwinToCGImageRef(&stateSaving);
    Assert(m_stateSaving);

    /* Prepare 'Restoring' state-overlay: */
    const QPixmap stateRestoring = gpConverter->toIcon(KMachineState_Restoring).pixmap(iconSize);
    m_stateRestoring = ::darwinToCGImageRef(&stateRestoring);
    Assert(m_stateRestoring);
}

void* UIAbstractDockIconPreviewHelper::currentPreviewWindowId() const
{
    /* Get the MachineView which is currently previewed and return the win id
       of the viewport. */
    UIMachineView* pView = m_pSession->machineLogic()->dockPreviewView();
    if (pView)
        return (void*)pView->viewport()->winId();
    return 0;
}

UIAbstractDockIconPreviewHelper::~UIAbstractDockIconPreviewHelper()
{
    CGImageRelease(m_overlayImage);
    if (m_dockMonitor)
        CGImageRelease(m_dockMonitor);
    if (m_dockMonitorGlossy)
        CGImageRelease(m_dockMonitorGlossy);

    CGImageRelease(m_statePaused);
    CGImageRelease(m_stateSaving);
    CGImageRelease(m_stateRestoring);
}

void UIAbstractDockIconPreviewHelper::initPreviewImages()
{
    if (!m_dockMonitor)
    {
        m_dockMonitor = ::darwinToCGImageRef("monitor.png");
        Assert(m_dockMonitor);
        /* Center it on the dock icon context */
        m_monitorRect = centerRect(CGRectMake(0, 0,
                                              CGImageGetWidth(m_dockMonitor),
                                              CGImageGetWidth(m_dockMonitor)));
    }

    if (!m_dockMonitorGlossy)
    {
        m_dockMonitorGlossy = ::darwinToCGImageRef("monitor_glossy.png");
        Assert(m_dockMonitorGlossy);
        /* This depends on the content of monitor.png */
        m_updateRect = CGRectMake(m_monitorRect.origin.x + 7 + 1,
                                  m_monitorRect.origin.y + 8 + 1,
                                  118 - 7 - 2,
                                  103 - 8 - 2);
    }
}

CGImageRef UIAbstractDockIconPreviewHelper::stateImage() const
{
    CGImageRef img;
    if (   m_pSession->machineState() == KMachineState_Paused
        || m_pSession->machineState() == KMachineState_TeleportingPausedVM)
        img = m_statePaused;
    else if (   m_pSession->machineState() == KMachineState_Restoring
             || m_pSession->machineState() == KMachineState_TeleportingIn)
        img = m_stateRestoring;
    else if (   m_pSession->machineState() == KMachineState_Saving
             || m_pSession->machineState() == KMachineState_LiveSnapshotting)
        img = m_stateSaving;
    else
        img = NULL;
    return img;
}

void UIAbstractDockIconPreviewHelper::drawOverlayIcons(CGContextRef context)
{
    /* Determine whether dock icon overlay enabled: */
    if (gEDataManager->dockIconOverlayEnabled(vboxGlobal().managedVMUuid()))
    {
        /* Initialize overlayrect: */
        CGRect overlayRect = CGRectMake(0, 0, 0, 0);
        /* Make sure overlay image is valid: */
        if (m_overlayImage)
        {
            /* Draw overlay image at bottom-right of dock icon: */
            overlayRect = CGRectMake(m_dockIconRect.size.width - CGImageGetWidth(m_overlayImage),
                                     m_dockIconRect.size.height - CGImageGetHeight(m_overlayImage),
                                     CGImageGetWidth(m_overlayImage),
                                     CGImageGetHeight(m_overlayImage));
            CGContextDrawImage(context, flipRect(overlayRect), m_overlayImage);
        }

        /* Determine correct state-overlay image: */
        CGImageRef sImage = stateImage();
        /* Make sure state-overlay image is valid: */
        if (sImage)
        {
            /* Draw state overlay image at top-left of guest-os overlay image: */
            CGRect stateRect = CGRectMake(overlayRect.origin.x - CGImageGetWidth(sImage) / 2.0,
                                          overlayRect.origin.y - CGImageGetHeight(sImage) / 2.0,
                                          CGImageGetWidth(sImage),
                                          CGImageGetHeight(sImage));
            CGContextDrawImage(context, flipRect(stateRect), sImage);
        }
    }
}

