/* $Id$ */
/** @file
 * Qt GUI - Realtime Dock Icon Preview
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* VBox includes */
#include "UIAbstractDockIconPreview.h"
#include "UIFrameBuffer.h"
#include "UIMachineLogic.h"
#include "UIMachineView.h"
#include "UISession.h"

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

    m_statePaused    = ::darwinToCGImageRef("state_paused_16px.png");
    Assert(m_statePaused);
    m_stateSaving    = ::darwinToCGImageRef("state_saving_16px.png");
    Assert(m_stateSaving);
    m_stateRestoring = ::darwinToCGImageRef("state_restoring_16px.png");
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
    CGRect overlayRect = CGRectMake(0, 0, 0, 0);
    /* The overlay image at bottom/right */
    if (m_overlayImage)
    {
        overlayRect = CGRectMake(m_dockIconRect.size.width - CGImageGetWidth(m_overlayImage),
                                 m_dockIconRect.size.height - CGImageGetHeight(m_overlayImage),
                                 CGImageGetWidth(m_overlayImage),
                                 CGImageGetHeight(m_overlayImage));
        CGContextDrawImage(context, flipRect(overlayRect), m_overlayImage);
    }
    CGImageRef sImage = stateImage();
    /* The state image at bottom/right */
    if (sImage)
    {
        CGRect stateRect = CGRectMake(overlayRect.origin.x - CGImageGetWidth(sImage) / 2.0,
                                      overlayRect.origin.y - CGImageGetHeight(sImage) / 2.0,
                                      CGImageGetWidth(sImage),
                                      CGImageGetHeight(sImage));
        CGContextDrawImage(context, flipRect(stateRect), sImage);
    }
}

