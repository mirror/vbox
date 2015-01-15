/* $Id$ */

/** @file
 * Presenter API: window class implementation.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "server_presenter.h"


CrFbWindow::CrFbWindow(uint64_t parentId) :
    mSpuWindow(0),
    mpCompositor(NULL),
    mcUpdates(0),
    mxPos(0),
    myPos(0),
    mWidth(0),
    mHeight(0),
    mParentId(parentId),
    mScaleFactorStorage(1)
{
    mFlags.Value = 0;
}


bool CrFbWindow::IsCreated() const
{
    return !!mSpuWindow;
}

bool CrFbWindow::IsVisivle() const
{
    return mFlags.fVisible;
}


void CrFbWindow::Destroy()
{
    CRASSERT(!mcUpdates);

    if (!mSpuWindow)
        return;

    cr_server.head_spu->dispatch_table.WindowDestroy(mSpuWindow);

    mSpuWindow = 0;
    mFlags.fDataPresented = 0;
}


int CrFbWindow::Reparent(uint64_t parentId)
{
    if (!checkInitedUpdating())
    {
        WARN(("err"));
        return VERR_INVALID_STATE;
    }

    crDebug("CrFbWindow: reparent to %p (current mxPos=%d, myPos=%d, mWidth=%u, mHeight=%u)",
        parentId, mxPos, myPos, mWidth, mHeight);

    uint64_t oldParentId = mParentId;

    mParentId = parentId;

    if (mSpuWindow)
    {
        if (oldParentId && !parentId && mFlags.fVisible)
            cr_server.head_spu->dispatch_table.WindowShow(mSpuWindow, false);

        renderspuSetWindowId(mParentId);
        renderspuReparentWindow(mSpuWindow);
        renderspuSetWindowId(cr_server.screen[0].winID);

        if (parentId)
        {
            if (mFlags.fVisible)
                cr_server.head_spu->dispatch_table.WindowPosition(mSpuWindow, mxPos, myPos);
            cr_server.head_spu->dispatch_table.WindowShow(mSpuWindow, mFlags.fVisible);
        }
    }

    return VINF_SUCCESS;
}


int CrFbWindow::SetVisible(bool fVisible)
{
    if (!checkInitedUpdating())
    {
        WARN(("err"));
        return VERR_INVALID_STATE;
    }

    LOG(("CrWIN: Visible [%d]", fVisible));

    if (!fVisible != !mFlags.fVisible)
    {
        mFlags.fVisible = fVisible;
        if (mSpuWindow && mParentId)
        {
            if (fVisible)
                cr_server.head_spu->dispatch_table.WindowPosition(mSpuWindow, mxPos, myPos);
            cr_server.head_spu->dispatch_table.WindowShow(mSpuWindow, fVisible);
        }
    }

    return VINF_SUCCESS;
}


int CrFbWindow::SetSize(uint32_t width, uint32_t height)
{
    if (!checkInitedUpdating())
    {
        WARN(("err"));
        return VERR_INVALID_STATE;
    }

    if (mWidth != width || mHeight != height)
    {
        GLdouble scaleFactor = GetScaleFactor();

        mFlags.fCompositoEntriesModified = 1;
        mWidth  = scaleFactor ? (uint32_t)((GLdouble)width  * scaleFactor) : width;
        mHeight = scaleFactor ? (uint32_t)((GLdouble)height * scaleFactor) : height;

        LOG(("CrWIN: Size [%d ; %d]", width, height));

        if (mSpuWindow)
            cr_server.head_spu->dispatch_table.WindowSize(mSpuWindow, mWidth, mHeight);
    }

    return VINF_SUCCESS;
}


int CrFbWindow::SetPosition(int32_t x, int32_t y)
{
    if (!checkInitedUpdating())
    {
        WARN(("err"));
        return VERR_INVALID_STATE;
    }

    LOG(("CrWIN: Pos [%d ; %d]", x, y));
//      always do WindowPosition to ensure window is adjusted properly
//        if (x != mxPos || y != myPos)
    {
        mxPos = x;
        myPos = y;
        if (mSpuWindow)
            cr_server.head_spu->dispatch_table.WindowPosition(mSpuWindow, x, y);
    }

    return VINF_SUCCESS;
}


int CrFbWindow::SetVisibleRegionsChanged()
{
    if (!checkInitedUpdating())
    {
        WARN(("err"));
        return VERR_INVALID_STATE;
    }

    mFlags.fCompositoEntriesModified = 1;
    return VINF_SUCCESS;
}


int CrFbWindow::SetCompositor(const struct VBOXVR_SCR_COMPOSITOR * pCompositor)
{
    if (!checkInitedUpdating())
    {
        WARN(("err"));
        return VERR_INVALID_STATE;
    }

    mpCompositor = pCompositor;
    mFlags.fCompositoEntriesModified = 1;

    return VINF_SUCCESS;
}


void CrFbWindow::SetScaleFactor(GLdouble scaleFactor)
{
    ASMAtomicWriteU32(&mScaleFactorStorage, (uint32_t)scaleFactor);
}


GLdouble CrFbWindow::GetScaleFactor()
{
    return (GLdouble)ASMAtomicReadU32(&mScaleFactorStorage);
}


int CrFbWindow::UpdateBegin()
{
    ++mcUpdates;
    if (mcUpdates > 1)
        return VINF_SUCCESS;

    Assert(!mFlags.fForcePresentOnReenable);

    if (mFlags.fDataPresented)
    {
        Assert(mSpuWindow);
        cr_server.head_spu->dispatch_table.VBoxPresentComposition(mSpuWindow, NULL, NULL);
        mFlags.fForcePresentOnReenable = isPresentNeeded();
    }

    return VINF_SUCCESS;
}


void CrFbWindow::UpdateEnd()
{
    --mcUpdates;
    Assert(mcUpdates < UINT32_MAX/2);
    if (mcUpdates)
        return;

    checkRegions();

    if (mSpuWindow)
    {
        bool fPresentNeeded = isPresentNeeded();
        if (fPresentNeeded || mFlags.fForcePresentOnReenable)
        {
            GLdouble scaleFactor = GetScaleFactor();

            mFlags.fForcePresentOnReenable = false;
            if (mpCompositor)
            {
                CrVrScrCompositorSetStretching((VBOXVR_SCR_COMPOSITOR *)mpCompositor, scaleFactor, scaleFactor);
                cr_server.head_spu->dispatch_table.VBoxPresentComposition(mSpuWindow, mpCompositor, NULL);
            }
            else
            {
                VBOXVR_SCR_COMPOSITOR TmpCompositor;
                RTRECT Rect;
                Rect.xLeft = 0;
                Rect.yTop = 0;
                Rect.xRight = mWidth;
                Rect.yBottom = mHeight;
                CrVrScrCompositorInit(&TmpCompositor, &Rect);
                CrVrScrCompositorSetStretching((VBOXVR_SCR_COMPOSITOR *)&TmpCompositor, scaleFactor, scaleFactor);
                /* this is a cleanup operation
                 * empty compositor is guarantid to be released on VBoxPresentComposition return */
                cr_server.head_spu->dispatch_table.VBoxPresentComposition(mSpuWindow, &TmpCompositor, NULL);
            }
            g_pLed->Asserted.s.fWriting = 1;
        }

        /* even if the above branch is entered due to mFlags.fForcePresentOnReenable,
         * the backend should clean up the compositor as soon as presentation is performed */
        mFlags.fDataPresented = fPresentNeeded;
    }
    else
    {
        Assert(!mFlags.fDataPresented);
        Assert(!mFlags.fForcePresentOnReenable);
    }
}


uint64_t CrFbWindow::GetParentId()
{
    return mParentId;
}


int CrFbWindow::Create()
{
    if (mSpuWindow)
    {
        //WARN(("window already created"));
        return VINF_ALREADY_INITIALIZED;
    }

    CRASSERT(cr_server.fVisualBitsDefault);
    renderspuSetWindowId(mParentId);
    mSpuWindow = cr_server.head_spu->dispatch_table.WindowCreate("", cr_server.fVisualBitsDefault);
    renderspuSetWindowId(cr_server.screen[0].winID);
    if (mSpuWindow < 0) {
        WARN(("WindowCreate failed"));
        return VERR_GENERAL_FAILURE;
    }

    cr_server.head_spu->dispatch_table.WindowSize(mSpuWindow, mWidth, mHeight);
    cr_server.head_spu->dispatch_table.WindowPosition(mSpuWindow, mxPos, myPos);

    checkRegions();

    if (mParentId && mFlags.fVisible)
        cr_server.head_spu->dispatch_table.WindowShow(mSpuWindow, true);

    return VINF_SUCCESS;
}


CrFbWindow::~CrFbWindow()
{
    Destroy();
}


void CrFbWindow::checkRegions()
{
    if (!mSpuWindow)
        return;

    if (!mFlags.fCompositoEntriesModified)
        return;

    uint32_t cRects;
    const RTRECT *pRects;
    if (mpCompositor)
    {
        int rc = CrVrScrCompositorRegionsGet(mpCompositor, &cRects, NULL, &pRects, NULL);
        if (!RT_SUCCESS(rc))
        {
            WARN(("CrVrScrCompositorRegionsGet failed rc %d", rc));
            cRects = 0;
            pRects = NULL;
        }
    }
    else
    {
        cRects = 0;
        pRects = NULL;
    }

    cr_server.head_spu->dispatch_table.WindowVisibleRegion(mSpuWindow, cRects, (const GLint*)pRects);

    mFlags.fCompositoEntriesModified = 0;
}


bool CrFbWindow::isPresentNeeded()
{
    return mFlags.fVisible && mWidth && mHeight && mpCompositor && !CrVrScrCompositorIsEmpty(mpCompositor);
}


bool CrFbWindow::checkInitedUpdating()
{
    if (!mcUpdates)
    {
        WARN(("not updating"));
        return false;
    }

    return true;
}

