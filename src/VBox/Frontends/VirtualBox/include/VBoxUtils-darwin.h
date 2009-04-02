/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Declarations of utility classes and functions for handling Darwin specific
 * tasks
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

#ifndef __VBoxUtils_darwin_h
#define __VBoxUtils_darwin_h

/*
 * Here is some really magic in. The "OS System native" methods are implemented
 * in the current OS specific way. This means either Carbon
 * (VBoxUtils-darwin-carbon.cpp) or Cocoa (VBoxUtils-darwin-cocoa.m). The Qt
 * wrapper methods handle the conversion from Q* data types to the native one
 * (VBoxUtils-darwin.cpp).
 */

#ifdef __OBJC__
#import <AppKit/NSWindow.h>

typedef NSWindow *NativeWindowRef;
typedef NSView *NativeViewRef;
#else
# include <iprt/cdefs.h> /* for __BEGIN_DECLS/__END_DECLS & stuff */

# include <qglobal.h> /* for QT_MAC_USE_COCOA */
# include <QRect>

# include <ApplicationServices/ApplicationServices.h>

class QWidget;
class QToolBar;
class QPixmap;
class QImage;

# ifdef QT_MAC_USE_COCOA
/* Cast this to void, cause Cocoa classes aren't usable in the C++ context. */
typedef void *NativeWindowRef;
typedef void *NativeViewRef;
# else /* QT_MAC_USE_COCOA */
#  include <Carbon/Carbon.h>
typedef WindowRef NativeWindowRef;
typedef HIViewRef NativeViewRef;
# endif /* QT_MAC_USE_COCOA */
#endif /* __OBJC__ */

__BEGIN_DECLS

/********************************************************************************
 *
 * Window/View management (OS System native)
 *
 ********************************************************************************/
NativeWindowRef darwinToNativeWindowImpl (NativeViewRef aView);
NativeViewRef darwinToNativeViewImpl (NativeWindowRef aWindow);

/********************************************************************************
 *
 * Simple setter methods (OS System native)
 *
 ********************************************************************************/
void darwinSetShowsToolbarButtonImpl (NativeWindowRef aWindow, bool aEnabled);
void darwinSetShowsResizeIndicatorImpl (NativeWindowRef aWindow, bool aEnabled);
void darwinSetHidesAllTitleButtonsImpl (NativeWindowRef aWindow);
void darwinSetShowsWindowTransparentImpl (NativeWindowRef aWindow, bool aEnabled);
void darwinSetMouseCoalescingEnabled (bool aEnabled);

/********************************************************************************
 *
 * Simple helper methods (OS System native)
 *
 ********************************************************************************/
void darwinWindowAnimateResizeImpl (NativeWindowRef aWindow, int x, int y, int width, int height);
void darwinWindowInvalidateShapeImpl (NativeWindowRef aWindow);
void darwinWindowInvalidateShadowImpl (NativeWindowRef aWindow);

__END_DECLS

#ifndef __OBJC__
/********************************************************************************
 *
 * Window/View management (Qt Wrapper)
 *
 ********************************************************************************/

/**
 * Returns a reference to the native View of the QWidget.
 *
 * @returns either HIViewRef or NSView* of the QWidget.
 * @param   aWidget   Pointer to the QWidget
 */
NativeViewRef darwinToNativeView (QWidget *aWidget);

/**
 * Returns a reference to the native Window of the QWidget.
 *
 * @returns either WindowRef or NSWindow* of the QWidget.
 * @param   aWidget   Pointer to the QWidget
 */
NativeWindowRef darwinToNativeWindow (QWidget *aWidget);

/* This is necessary because of the C calling convention. Its a simple wrapper
   for darwinToNativeWindowImpl to allow operator overloading which isn't
   allowed in C. */
/**
 * Returns a reference to the native Window of the View..
 *
 * @returns either WindowRef or NSWindow* of the View.
 * @param   aWidget   Pointer to the native View
 */
NativeWindowRef darwinToNativeWindow (NativeViewRef aView);

/**
 * Returns a reference to the native View of the Window.
 *
 * @returns either HIViewRef or NSView* of the Window.
 * @param   aWidget   Pointer to the native Window
 */
NativeViewRef darwinToNativeView (NativeWindowRef aWindow);

/********************************************************************************
 *
 * Simple setter methods (Qt Wrapper)
 *
 ********************************************************************************/
void darwinSetShowsToolbarButton (QToolBar *aToolBar, bool aEnabled);
void darwinSetShowsResizeIndicator (QWidget *aWidget, bool aEnabled);
void darwinSetHidesAllTitleButtons (QWidget *aWidget);
void darwinSetShowsWindowTransparent (QWidget *aWidget, bool aEnabled);
void darwinDisableIconsInMenus (void);

/********************************************************************************
 *
 * Simple helper methods (Qt Wrapper)
 *
 ********************************************************************************/
void darwinWindowAnimateResize (QWidget *aWidget, const QRect &aTarget);
void darwinWindowInvalidateShape (QWidget *aWidget);
void darwinWindowInvalidateShadow (QWidget *aWidget);
QString darwinSystemLanguage (void);
QPixmap darwinCreateDragPixmap (const QPixmap& aPixmap, const QString &aText);


/********************************************************************************
 *
 * Graphics stuff (Qt Wrapper)
 *
 ********************************************************************************/
/**
 * Returns a reference to the CGContext of the QWidget.
 *
 * @returns CGContextRef of the QWidget.
 * @param   aWidget      Pointer to the QWidget
 */
CGContextRef darwinToCGContextRef (QWidget *aWidget);

CGImageRef darwinToCGImageRef (const QImage *aImage);
CGImageRef darwinToCGImageRef (const QPixmap *aPixmap);
CGImageRef darwinToCGImageRef (const char *aSource);

DECLINLINE(CGRect) darwinToCGRect (const QRect& aRect) { return CGRectMake (aRect.x(), aRect.y(), aRect.width(), aRect.height()); }
DECLINLINE(CGRect) darwinFlipCGRect (CGRect aRect, int aTargetHeight) { aRect.origin.y = aTargetHeight - aRect.origin.y - aRect.size.height; return aRect; }
DECLINLINE(CGRect) darwinFlipCGRect (CGRect aRect, const CGRect &aTarget) { return darwinFlipCGRect (aRect, aTarget.size.height); }
DECLINLINE(CGRect) darwinCenterRectTo (CGRect aRect, const CGRect& aToRect)
{
    aRect.origin.x = aToRect.origin.x + (aToRect.size.width  - aRect.size.width)  / 2.0;
    aRect.origin.y = aToRect.origin.y + (aToRect.size.height - aRect.size.height) / 2.0;
    return aRect;
}





/********************************************************************************
 *
 * Old carbon stuff. Have to be converted soon!
 *
 ********************************************************************************/

#include <QWidget>

# ifndef QT_MAC_USE_COCOA

/* Asserts if a != noErr and prints the error code */
#  define AssertCarbonOSStatus(a) AssertMsg ((a) == noErr, ("Carbon OSStatus: %d\n", static_cast<int> (a)))


/**
 * Converts a QRect to a HIRect.
 *
 * @returns HIRect for the converted QRect.
 * @param   aRect  the QRect to convert
 */
DECLINLINE(HIRect) darwinToHIRect (const QRect &aRect)
{
    return CGRectMake (aRect.x(), aRect.y(), aRect.width(), aRect.height());
}

/* Experimental region handler for the seamless mode */
OSStatus darwinRegionHandler (EventHandlerCallRef aInHandlerCallRef, EventRef aInEvent, void *aInUserData);

/* Handler for the OpenGL overlay window stuff & the possible messages. */
enum
{
    /* Event classes */
    kEventClassVBox         = 'vbox',
    /* Event kinds */
    kEventVBoxShowWindow    = 'swin',
    kEventVBoxHideWindow    = 'hwin',
    kEventVBoxMoveWindow    = 'mwin',
    kEventVBoxResizeWindow  = 'rwin',
    kEventVBoxDisposeWindow = 'dwin',
    kEventVBoxUpdateDock    = 'udck',
    kEventVBoxUpdateContext = 'uctx',
    kEventVBoxBoundsChanged = 'bchg'
};

void PostBoundsChanged (const QRect& rect);
OSStatus darwinOverlayWindowHandler (EventHandlerCallRef aInHandlerCallRef, EventRef aInEvent, void *aInUserData);

bool darwinIsMenuOpen (void);

# endif /* !QT_MAC_USE_COCOA */

# ifdef DEBUG
void darwinDebugPrintEvent (const char *aPrefix, EventRef aEvent);
# endif

#endif /* !__OBJC__ */

#endif /* __VBoxUtils_darwin_h */

