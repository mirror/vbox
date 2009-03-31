/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Declarations of utility classes and functions for handling Darwin Carbon
 * specific tasks
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#include "VBoxUtils-darwin.h"

#include <iprt/assert.h>

#include <Carbon/Carbon.h>

NativeWindowRef darwinToNativeWindowImpl (NativeViewRef aView)
{
    if (aView)
        return reinterpret_cast<WindowRef> (::HIViewGetWindow (aView));
    return NULL;
}

NativeViewRef darwinToNativeViewImpl (NativeWindowRef aWindow)
{
    NativeViewRef view = NULL;
    if (aWindow)
    {
        OSStatus result = GetRootControl (aWindow, &view);
        AssertCarbonOSStatus (result);
    }
    return view;
}

void darwinSetShowsToolbarButtonImpl (NativeWindowRef aWindow, bool aEnabled)
{
    OSStatus result = ::ChangeWindowAttributes (aWindow, aEnabled ? kWindowToolbarButtonAttribute : kWindowNoAttributes,
                                                         aEnabled ? kWindowNoAttributes : kWindowToolbarButtonAttribute);
    AssertCarbonOSStatus (result);
}

void darwinSetShowsResizeIndicatorImpl (NativeWindowRef aWindow, bool aEnabled)
{
    OSStatus result = ::ChangeWindowAttributes (aWindow, aEnabled ? kWindowResizableAttribute : kWindowNoAttributes,
                                                         aEnabled ? kWindowNoAttributes : kWindowResizableAttribute);
    AssertCarbonOSStatus (result);
}

void darwinSetShowsWindowTransparentImpl (NativeWindowRef aWindow, bool aEnabled)
{
    Assert (VALID_PTR (aWindow));
    OSStatus result;
    if (aEnabled)
    {
        HIViewRef viewRef = ::darwinToNativeViewImpl (aWindow);
        Assert (VALID_PTR (viewRef));
        /* @todo=poetzsch: Currently this isn't necessary. I should
         * investigate if we can/should use this. */
        /*
           EventTypeSpec wCompositingEvent = { kEventClassWindow, kEventWindowGetRegion };
           status = InstallWindowEventHandler ((WindowPtr)winId(), DarwinRegionHandler, GetEventTypeCount (wCompositingEvent), &wCompositingEvent, &mCurrRegion, &mDarwinRegionEventHandlerRef);
           AssertCarbonOSStatus (status);
           HIViewRef contentView = 0;
           status = HIViewFindByID(HIViewGetRoot(windowRef), kHIViewWindowContentID, &contentView);
           AssertCarbonOSStatus (status);
           EventTypeSpec drawEvent = { kEventClassControl, kEventControlDraw };
           status = InstallControlEventHandler (contentView, DarwinRegionHandler, GetEventTypeCount (drawEvent), &drawEvent, &contentView, NULL);
           AssertCarbonOSStatus (status);
         */
        UInt32 features;
        result = ::GetWindowFeatures (aWindow, &features);
        AssertCarbonOSStatus (result);
        if (( features & kWindowIsOpaque ) != 0)
        {
            result = ::HIWindowChangeFeatures (aWindow, 0, kWindowIsOpaque);
            AssertCarbonOSStatus (result);
        }
        result = ::HIViewReshapeStructure (viewRef);
        AssertCarbonOSStatus (result);
        result = ::SetWindowAlpha (aWindow, 0.999);
        AssertCarbonOSStatus (result);
        /* For now disable the shadow of the window. This feature cause errors
         * if a window in vbox looses focus, is reselected and than moved. */
        /** @todo Search for an option to enable this again. A shadow on every
         * window has a big coolness factor. */
        result = ::ChangeWindowAttributes (aWindow, kWindowNoShadowAttribute, 0);
        AssertCarbonOSStatus (result);
    }
    else
    {
        /* See above.
           status = RemoveEventHandler (mDarwinRegionEventHandlerRef);
           AssertCarbonOSStatus (status);
         */
        result = ::ReshapeCustomWindow (aWindow);
        AssertCarbonOSStatus (result);
        result = ::SetWindowAlpha (aWindow, 1.0);
        AssertCarbonOSStatus (result);
    }
}

void darwinSetMouseCoalescingEnabled (bool aEnabled)
{
    OSStatus result = ::SetMouseCoalescingEnabled (aEnabled, NULL);
    AssertCarbonOSStatus (result);
}

void darwinWindowAnimateResizeImpl (NativeWindowRef aWidget, int x, int y, int width, int height)
{
    HIRect r = CGRectMake (x, y, width, height);
    OSStatus result = ::TransitionWindowWithOptions (aWidget,
                                                     kWindowSlideTransitionEffect,
                                                     kWindowResizeTransitionAction,
                                                     &r,
                                                     false,
                                                     NULL);
    AssertCarbonOSStatus (result);
}

void darwinWindowInvalidateShapeImpl (NativeWindowRef aWindow)
{
    OSStatus result = HIViewReshapeStructure (::darwinToNativeViewImpl (aWindow));
    AssertCarbonOSStatus (result);
//    HIWindowInvalidateShadow (::darwinToWindowRef (console->viewport()));
//    ReshapeCustomWindow (::darwinToWindowRef (this));
}

/********************************************************************************
 *
 * Old carbon stuff. Have to converted soon!
 *
 ********************************************************************************/
#include "VBoxConsoleView.h"

bool darwinIsMenuOpen (void)
{
    MenuTrackingData outData;
    return (GetMenuTrackingData (NULL, &outData) != menuNotFoundErr);
}

/* Currently not used! */
OSStatus darwinRegionHandler (EventHandlerCallRef aInHandlerCallRef, EventRef aInEvent, void *aInUserData)
{
    NOREF (aInHandlerCallRef);

    OSStatus status = eventNotHandledErr;

    switch (GetEventKind (aInEvent))
    {
        case kEventWindowGetRegion:
        {
            WindowRegionCode code;
            RgnHandle rgn;

            /* which region code is being queried? */
            GetEventParameter (aInEvent, kEventParamWindowRegionCode, typeWindowRegionCode, NULL, sizeof (code), NULL, &code);

            /* if it is the opaque region code then set the region to Empty and return noErr to stop the propagation */
            if (code == kWindowOpaqueRgn)
            {
                printf("test1\n");
                GetEventParameter (aInEvent, kEventParamRgnHandle, typeQDRgnHandle, NULL, sizeof (rgn), NULL, &rgn);
                SetEmptyRgn (rgn);
                status = noErr;
            }
            /* if the content of the whole window is queried return a copy of our saved region. */
            else if (code == (kWindowStructureRgn))// || kWindowGlobalPortRgn || kWindowUpdateRgn))
            {
                printf("test2\n");
                GetEventParameter (aInEvent, kEventParamRgnHandle, typeQDRgnHandle, NULL, sizeof (rgn), NULL, &rgn);
                QRegion *pRegion = static_cast <QRegion*> (aInUserData);
                if (!pRegion->isEmpty() && pRegion)
                {
                    //CopyRgn (pRegion->handle(), rgn);
                    status = noErr;
                }
            }
            break;
        }
        case kEventControlDraw:
        {
            printf("test3\n");
            CGContextRef ctx;
            HIRect bounds;

            GetEventParameter (aInEvent, kEventParamCGContextRef, typeCGContextRef, NULL, sizeof (ctx), NULL, &ctx);
            HIViewGetBounds ((HIViewRef)aInUserData, &bounds);

            CGContextClearRect (ctx, bounds);
            status = noErr;
            break;
        }
    }

    return status;
}

OSStatus darwinOverlayWindowHandler (EventHandlerCallRef aInHandlerCallRef, EventRef aInEvent, void *aInUserData)
{
    if (!aInUserData)
        return ::CallNextEventHandler (aInHandlerCallRef, aInEvent);

    UInt32 eventClass = ::GetEventClass (aInEvent);
    UInt32 eventKind = ::GetEventKind (aInEvent);
    /* For debugging events */
    /*
    if (!(eventClass == 'cute'))
        ::darwinDebugPrintEvent ("view: ", aInEvent);
    */
    VBoxConsoleView *view = static_cast<VBoxConsoleView *> (aInUserData);

    if (eventClass == kEventClassVBox)
    {
        if (eventKind == kEventVBoxShowWindow)
        {
//            printf ("ShowWindow requested\n");
            WindowRef w;
            if (GetEventParameter (aInEvent, kEventParamWindowRef, typeWindowRef, NULL, sizeof (w), NULL, &w) != noErr)
                return noErr;
            ShowWindow (w);
            SelectWindow (w);
            return noErr;
        }
        if (eventKind == kEventVBoxHideWindow)
        {
//            printf ("HideWindow requested\n");
            WindowPtr w;
            if (GetEventParameter (aInEvent, kEventParamWindowRef, typeWindowRef, NULL, sizeof (w), NULL, &w) != noErr)
                return noErr;
            HideWindow (w);
            return noErr;
        }
        if (eventKind == kEventVBoxMoveWindow)
        {
//            printf ("MoveWindow requested\n");
            WindowPtr w;
            if (GetEventParameter (aInEvent, kEventParamWindowRef, typeWindowRef, NULL, sizeof (w), NULL, &w) != noErr)
                return noErr;
            HIPoint p;
            if (GetEventParameter (aInEvent, kEventParamOrigin, typeHIPoint, NULL, sizeof (p), NULL, &p) != noErr)
                return noErr;
            ChangeWindowGroupAttributes (GetWindowGroup (w), 0, kWindowGroupAttrMoveTogether);
            QPoint p1 = view->mapToGlobal (QPoint (p.x, p.y));
//            printf ("Pos: %d %d\n", p1.x(), p1.y());
            MoveWindow (w, p1.x(), p1.y(), true);
            ChangeWindowGroupAttributes (GetWindowGroup (w), kWindowGroupAttrMoveTogether, 0);
            return noErr;
        }
        if (eventKind == kEventVBoxResizeWindow)
        {
//            printf ("ResizeWindow requested\n");
            WindowPtr w;
            if (GetEventParameter (aInEvent, kEventParamWindowRef, typeWindowRef, NULL, sizeof (w), NULL, &w) != noErr)
                return noErr;
            HISize s;
            if (GetEventParameter (aInEvent, kEventParamDimensions, typeHISize, NULL, sizeof (s), NULL, &s) != noErr)
                return noErr;
            ChangeWindowGroupAttributes (GetWindowGroup (w), 0, kWindowGroupAttrMoveTogether);
//            printf ("Size: %f %f\n", s.width, s.height);
            SizeWindow (w, s.width, s.height, true);
            ChangeWindowGroupAttributes (GetWindowGroup (w), kWindowGroupAttrMoveTogether, 0);
            return noErr;
        }
        if (eventKind == kEventVBoxDisposeWindow)
        {
//            printf ("DisposeWindow requested\n");
            WindowPtr w;
            if (GetEventParameter (aInEvent, kEventParamWindowRef, typeWindowRef, NULL, sizeof (w), NULL, &w) != noErr)
                return noErr;
            DisposeWindow (w);
            return noErr;
        }
        if (eventKind == kEventVBoxUpdateDock)
        {
//            printf ("UpdateDock requested\n");
            view->updateDockIcon();
            return noErr;
        }
    }

    return ::CallNextEventHandler (aInHandlerCallRef, aInEvent);
}

