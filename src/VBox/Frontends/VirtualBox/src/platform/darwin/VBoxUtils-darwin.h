/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Declarations of utility classes and functions for handling Darwin specific
 * tasks
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxUtils_darwin_h
#define ___VBoxUtils_darwin_h

#include <VBox/VBoxCocoa.h>
#include <iprt/cdefs.h> /* for RT_C_DECLS_BEGIN/RT_C_DECLS_END & stuff */

#include <ApplicationServices/ApplicationServices.h>
#include <QRect>

ADD_COCOA_NATIVE_REF(NSEvent);
ADD_COCOA_NATIVE_REF(NSImage);
ADD_COCOA_NATIVE_REF(NSView);
ADD_COCOA_NATIVE_REF(NSWindow);
ADD_COCOA_NATIVE_REF(NSString);

class QImage;
class QMainWindow;
class QMenu;
class QPixmap;
class QToolBar;
class QWidget;

RT_C_DECLS_BEGIN

/********************************************************************************
 *
 * Window/View management (OS System native)
 *
 ********************************************************************************/
NativeNSWindowRef darwinToNativeWindowImpl(NativeNSViewRef pView);
NativeNSViewRef darwinToNativeViewImpl(NativeNSWindowRef pWindow);
NativeNSStringRef darwinToNativeString(const char* pcszString);

/********************************************************************************
 *
 * Simple setter methods (OS System native)
 *
 ********************************************************************************/
void darwinSetShowsToolbarButtonImpl(NativeNSWindowRef pWindow, bool fEnabled);
void darwinSetShowsResizeIndicatorImpl(NativeNSWindowRef pWindow, bool fEnabled);
void darwinSetHidesAllTitleButtonsImpl(NativeNSWindowRef pWindow);
void darwinLabelWindow(NativeNSWindowRef pWindow, NativeNSImageRef pImage, bool fCenter);
void darwinSetShowsWindowTransparentImpl(NativeNSWindowRef pWindow, bool fEnabled);
void darwinSetMouseCoalescingEnabled(bool fEnabled);

void darwintest(NativeNSWindowRef pWindow);
/********************************************************************************
 *
 * Simple helper methods (OS System native)
 *
 ********************************************************************************/
void darwinWindowAnimateResizeImpl(NativeNSWindowRef pWindow, int x, int y, int width, int height);
void darwinWindowAnimateResizeNewImpl(NativeNSWindowRef pWindow, int height, bool fAnimate);
void darwinTest(NativeNSViewRef pView, NativeNSViewRef pView1, int h);
void darwinWindowInvalidateShapeImpl(NativeNSWindowRef pWindow);
void darwinWindowInvalidateShadowImpl(NativeNSWindowRef pWindow);
int  darwinWindowToolBarHeight(NativeNSWindowRef pWindow);
bool darwinIsToolbarVisible(NativeNSWindowRef pWindow);
bool darwinIsWindowMaximized(NativeNSWindowRef pWindow);
void darwinMinaturizeWindow(NativeNSWindowRef pWindow);

bool darwinOpenFile(NativeNSStringRef pstrFile);

float darwinSmallFontSize();
bool darwinSetFrontMostProcess();
uint64_t darwinGetCurrentProcessId();

void darwinInstallResizeDelegate(NativeNSWindowRef pWindow);
void darwinUninstallResizeDelegate(NativeNSWindowRef pWindow);

bool darwinUnifiedToolbarEvents(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser);
bool darwinMouseGrabEvents(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser);
void darwinCreateContextMenuEvent(void *pvWin, int x, int y);

bool darwinIsApplicationCommand(ConstNativeNSEventRef pEvent);

void darwinRetranslateAppMenu();

void darwinSendMouseGrabEvents(QWidget *pWidget, int type, int button, int buttons, int x, int y);

QString darwinResolveAlias(const QString &strFile);

RT_C_DECLS_END

DECLINLINE(CGRect) darwinToCGRect(const QRect& aRect) { return CGRectMake(aRect.x(), aRect.y(), aRect.width(), aRect.height()); }
DECLINLINE(CGRect) darwinFlipCGRect(CGRect aRect, double aTargetHeight) { aRect.origin.y = aTargetHeight - aRect.origin.y - aRect.size.height; return aRect; }
DECLINLINE(CGRect) darwinFlipCGRect(CGRect aRect, const CGRect &aTarget) { return darwinFlipCGRect(aRect, aTarget.size.height); }
DECLINLINE(CGRect) darwinCenterRectTo(CGRect aRect, const CGRect& aToRect)
{
    aRect.origin.x = aToRect.origin.x + (aToRect.size.width  - aRect.size.width)  / 2.0;
    aRect.origin.y = aToRect.origin.y + (aToRect.size.height - aRect.size.height) / 2.0;
    return aRect;
}

/********************************************************************************
 *
 * Window/View management (Qt Wrapper)
 *
 ********************************************************************************/

/**
 * Returns a reference to the native View of the QWidget.
 *
 * @returns either HIViewRef or NSView* of the QWidget.
 * @param   pWidget   Pointer to the QWidget
 */
NativeNSViewRef darwinToNativeView(QWidget *pWidget);

/**
 * Returns a reference to the native Window of the QWidget.
 *
 * @returns either WindowRef or NSWindow* of the QWidget.
 * @param   pWidget   Pointer to the QWidget
 */
NativeNSWindowRef darwinToNativeWindow(QWidget *pWidget);

/* This is necessary because of the C calling convention. Its a simple wrapper
   for darwinToNativeWindowImpl to allow operator overloading which isn't
   allowed in C. */
/**
 * Returns a reference to the native Window of the View..
 *
 * @returns either WindowRef or NSWindow* of the View.
 * @param   pWidget   Pointer to the native View
 */
NativeNSWindowRef darwinToNativeWindow(NativeNSViewRef pView);

/**
 * Returns a reference to the native View of the Window.
 *
 * @returns either HIViewRef or NSView* of the Window.
 * @param   pWidget   Pointer to the native Window
 */
NativeNSViewRef darwinToNativeView(NativeNSWindowRef pWindow);

/********************************************************************************
 *
 * Graphics stuff (Qt Wrapper)
 *
 ********************************************************************************/
/**
 * Returns a reference to the CGContext of the QWidget.
 *
 * @returns CGContextRef of the QWidget.
 * @param   pWidget      Pointer to the QWidget
 */
CGContextRef darwinToCGContextRef(QWidget *pWidget);
CGImageRef darwinToCGImageRef(const QImage *pImage);
CGImageRef darwinToCGImageRef(const QPixmap *pPixmap);
CGImageRef darwinToCGImageRef(const char *pczSource);

NativeNSImageRef darwinToNSImageRef(const CGImageRef pImage);
NativeNSImageRef darwinToNSImageRef(const QImage *pImage);
NativeNSImageRef darwinToNSImageRef(const QPixmap *pPixmap);
NativeNSImageRef darwinToNSImageRef(const char *pczSource);

#ifndef __OBJC__

#include <QEvent>
class UIGrabMouseEvent: public QEvent
{
public:
    enum { GrabMouseEvent = QEvent::User + 200 };

    UIGrabMouseEvent(QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons, int x, int y, int wheelDelta, Qt::Orientation o)
      : QEvent((QEvent::Type)GrabMouseEvent)
      , m_type(type)
      , m_button(button)
      , m_buttons(buttons)
      , m_x(x)
      , m_y(y)
      , m_wheelDelta(wheelDelta)
      , m_orientation(o)
    {}
    QEvent::Type mouseEventType() const { return m_type; }
    Qt::MouseButton button() const { return m_button; }
    Qt::MouseButtons buttons() const { return m_buttons; }
    int xDelta() const { return m_x; }
    int yDelta() const { return m_y; }
    int wheelDelta() const { return m_wheelDelta; }
    Qt::Orientation orientation() const { return m_orientation; }

private:
    /* Private members */
    QEvent::Type m_type;
    Qt::MouseButton m_button;
    Qt::MouseButtons m_buttons;
    int m_x;
    int m_y;
    int m_wheelDelta;
    Qt::Orientation m_orientation;
};

/********************************************************************************
 *
 * Simple setter methods (Qt Wrapper)
 *
 ********************************************************************************/
void darwinSetShowsToolbarButton(QToolBar *aToolBar, bool fEnabled);
void darwinLabelWindow(QWidget *pWidget, QPixmap *pPixmap, bool fCenter);
void darwinSetShowsResizeIndicator(QWidget *pWidget, bool fEnabled);
void darwinSetHidesAllTitleButtons(QWidget *pWidget);
void darwinSetShowsWindowTransparent(QWidget *pWidget, bool fEnabled);
void darwinSetDockIconMenu(QMenu *pMenu);
void darwinDisableIconsInMenus(void);

void darwinTest(QWidget *pWidget1, QWidget *pWidget2, int h);

/********************************************************************************
 *
 * Simple helper methods (Qt Wrapper)
 *
 ********************************************************************************/
void darwinWindowAnimateResize(QWidget *pWidget, const QRect &aTarget);
void darwinWindowAnimateResizeNew(QWidget *pWidget, int h, bool fAnimate);
void darwinWindowInvalidateShape(QWidget *pWidget);
void darwinWindowInvalidateShadow(QWidget *pWidget);
int  darwinWindowToolBarHeight(QWidget *pWidget);
bool darwinIsToolbarVisible(QToolBar *pToolBar);
bool darwinIsWindowMaximized(QWidget *pWidget);
void darwinMinaturizeWindow(QWidget *pWidget);
bool darwinOpenFile(const QString &strFile);

QString darwinSystemLanguage(void);
QPixmap darwinCreateDragPixmap(const QPixmap& aPixmap, const QString &aText);

void darwinInstallResizeDelegate(QWidget *pWidget);
void darwinUninstallResizeDelegate(QWidget *pWidget);

void darwinRegisterForUnifiedToolbarContextMenuEvents(QMainWindow *pWindow);
void darwinUnregisterForUnifiedToolbarContextMenuEvents(QMainWindow *pWindow);

void darwinMouseGrab(QWidget *pWidget);
void darwinMouseRelease(QWidget *pWidget);
#endif /* !__OBJC__ */

#endif /* !___VBoxUtils_darwin_h */

