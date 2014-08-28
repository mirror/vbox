/* $Id$ */
/** @file
 * UICocoaApplication - C++ interface to NSApplication for handling -sendEvent.
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

/* GUI includes: */
#include "UICocoaApplication.h"
#include "VBoxUtils-darwin.h"

/* Global includes */
#import <AppKit/NSEvent.h>
#import <AppKit/NSApplication.h>
#import <Foundation/NSArray.h>
#import <AppKit/NSWindow.h>

#include <iprt/assert.h>

/** Class for tracking a callback. */
@interface CallbackData: NSObject
{
@public
    /** Mask of events to send to this callback. */
    uint32_t            fMask;
    /** The callback. */
    PFNVBOXCACALLBACK   pfnCallback;
    /** The user argument. */
    void               *pvUser;
}
- (id) initWithMask:(uint32)mask callback:(PFNVBOXCACALLBACK)callback user:(void*)user;
@end /* @interface CallbackData  */

@implementation CallbackData
- (id) initWithMask:(uint32)mask callback:(PFNVBOXCACALLBACK)callback user:(void*)user
{
    self = [super init];
    if (self)
    {
        fMask = mask;
        pfnCallback = callback;
        pvUser =  user;
    }
    return self;
}
@end /* @implementation CallbackData  */

/** Class for event handling */
@interface UICocoaApplicationPrivate: NSApplication
{
    /** The event mask for which there currently are callbacks. */
    uint32_t        m_fMask;
    /** Array of callbacks. */
    NSMutableArray *m_pCallbacks;
}
- (id)init;
- (void)sendEvent:(NSEvent *)theEvent;
- (void)setCallback:(uint32_t)fMask :(PFNVBOXCACALLBACK)pfnCallback :(void *)pvUser;
- (void)unsetCallback:(uint32_t)fMask :(PFNVBOXCACALLBACK)pfnCallback :(void *)pvUser;

- (void)registerToNotificationOfWindow :(NSString*)pstrNotificationName :(NSWindow*)pWindow;
- (void)unregisterFromNotificationOfWindow :(NSString*)pstrNotificationName :(NSWindow*)pWindow;

- (void)notificationCallbackOfWindow :(NSNotification*)notification;
@end /* @interface UICocoaApplicationPrivate */

@implementation UICocoaApplicationPrivate
-(id) init
{
    self = [super init];
    if (self)
        m_pCallbacks = [[NSMutableArray alloc] init];

    return self;
}

-(void) sendEvent:(NSEvent *)pEvent
{
    /*
     * Check if the type matches any of the registered callbacks.
     */
    uint32_t const fMask = m_fMask;
#if 0 /* for debugging */
    ::darwinPrintEvent("sendEvent: ", pEvent);
#endif
    if (fMask != 0)
    {
        NSEventType EvtType = [pEvent type];
        uint32_t fEvtMask = RT_LIKELY(EvtType < 32) ? RT_BIT_32(EvtType) : 0;
        if (fMask & fEvtMask)
        {
            /*
             * Do the callouts in LIFO order.
             */
            for (CallbackData *pData in [m_pCallbacks reverseObjectEnumerator])
            {
                if (pData->fMask & fEvtMask)
                {
                    if (pData->pfnCallback(pEvent, [pEvent eventRef], pData->pvUser))
                        return;
                }

            }
        }
    }

    /*
     * Get on with it.
     */
    [super sendEvent:pEvent];
}

/**
 * Register an event callback.
 *
 * @param   fMask           The event mask for which the callback is to be invoked.
 * @param   pfnCallback     The callback function.
 * @param   pvUser          The user argument.
 */
-(void) setCallback: (uint32_t)fMask :(PFNVBOXCACALLBACK)pfnCallback :(void *)pvUser
{
    /* Add the callback data to the array */
    CallbackData *pData = [[[CallbackData alloc] initWithMask: fMask callback: pfnCallback user: pvUser] autorelease];
    [m_pCallbacks addObject: pData];

    /* Update the global mask */
    m_fMask |= fMask;
}

/**
 * Deregister an event callback.
 *
 * @param   fMask           Same as setCallback.
 * @param   pfnCallback     Same as setCallback.
 * @param   pvUser          Same as setCallback.
 */
-(void) unsetCallback: (uint32_t)fMask :(PFNVBOXCACALLBACK)pfnCallback :(void *)pvUser
{
    /*
     * Loop the event array LIFO fashion searching for a matching callback.
     */
    for (CallbackData *pData in [m_pCallbacks reverseObjectEnumerator])
    {
        if (   pData->pfnCallback == pfnCallback
            && pData->pvUser      == pvUser
            && pData->fMask       == fMask)
        {
            [m_pCallbacks removeObject: pData];
            break;
        }
    }
    uint32_t fNewMask = 0;
    for (CallbackData *pData in m_pCallbacks)
        fNewMask |= pData->fMask;
    m_fMask = fNewMask;
}

/** Register @a pWindow to cocoa notification @a pstrNotificationName. */
- (void) registerToNotificationOfWindow :(NSString*)pstrNotificationName :(NSWindow*)pWindow
{
    /* Register notification observer: */
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notificationCallbackOfWindow:)
                                                 name:pstrNotificationName
                                               object:pWindow];
}

/** Unregister @a pWindow from cocoa notification @a pstrNotificationName. */
- (void) unregisterFromNotificationOfWindow :(NSString*)pstrNotificationName :(NSWindow*)pWindow
{
    /* Uninstall notification observer: */
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:pstrNotificationName
                                                  object:pWindow];
}

/** Redirects cocoa @a notification to UICocoaApplication instance. */
- (void) notificationCallbackOfWindow :(NSNotification*)notification
{
    /* Get current notification name: */
    NSString *pstrName = [notification name];

    /* Redirect known notifications to widgets: */
    UICocoaApplication::instance()->nativeNotificationProxyForWidget(pstrName, [notification object]);
}
@end /* @implementation UICocoaApplicationPrivate */

/* C++ singleton for our private NSApplication object */
UICocoaApplication* UICocoaApplication::m_pInstance = 0;

/* static */
UICocoaApplication* UICocoaApplication::instance()
{
    if (!m_pInstance)
        m_pInstance = new UICocoaApplication();

    return m_pInstance;
}

void UICocoaApplication::hide()
{
    [m_pNative hide:m_pNative];
}

UICocoaApplication::UICocoaApplication()
{
    /* Make sure our private NSApplication object is created */
    m_pNative = (UICocoaApplicationPrivate*)[UICocoaApplicationPrivate sharedApplication];
    /* Create one auto release pool which is in place for all the
       initialization and deinitialization stuff. That is when the
       NSApplication is not running the run loop (there is a separate auto
       release pool defined). */
    m_pPool = [[NSAutoreleasePool alloc] init];
}

UICocoaApplication::~UICocoaApplication()
{
    [m_pNative release];
    [m_pPool release];
}

void UICocoaApplication::registerForNativeEvents(uint32_t fMask, PFNVBOXCACALLBACK pfnCallback, void *pvUser)
{
    [m_pNative setCallback:fMask :pfnCallback :pvUser];
}

void UICocoaApplication::unregisterForNativeEvents(uint32_t fMask, PFNVBOXCACALLBACK pfnCallback, void *pvUser)
{
    [m_pNative unsetCallback:fMask :pfnCallback :pvUser];
}

void UICocoaApplication::registerToNotificationOfWindow(const QString &strNativeNotificationName, QWidget *pWidget,
                                                        PfnNativeNotificationCallbackForQWidget pCallback)
{
    /* Make sure it is not registered yet: */
    AssertReturnVoid(!m_widgetCallbacks.contains(pWidget) || !m_widgetCallbacks[pWidget].contains(strNativeNotificationName));

    /* Remember callback: */
    m_widgetCallbacks[pWidget][strNativeNotificationName] = pCallback;

    /* Register observer: */
    NativeNSStringRef pstrNativeNotificationName = darwinToNativeString(strNativeNotificationName.toLatin1().constData());
    NativeNSWindowRef pWindow = darwinToNativeWindow(pWidget);
    [m_pNative registerToNotificationOfWindow :pstrNativeNotificationName :pWindow];
}

void UICocoaApplication::unregisterFromNotificationOfWindow(const QString &strNativeNotificationName, QWidget *pWidget)
{
    /* Make sure it is registered yet: */
    AssertReturnVoid(m_widgetCallbacks.contains(pWidget) && m_widgetCallbacks[pWidget].contains(strNativeNotificationName));

    /* Forget callback: */
    m_widgetCallbacks[pWidget].remove(strNativeNotificationName);
    if (m_widgetCallbacks[pWidget].isEmpty())
        m_widgetCallbacks.remove(pWidget);

    /* Unregister observer: */
    NativeNSStringRef pstrNativeNotificationName = darwinToNativeString(strNativeNotificationName.toLatin1().constData());
    NativeNSWindowRef pWindow = darwinToNativeWindow(pWidget);
    [m_pNative unregisterFromNotificationOfWindow :pstrNativeNotificationName :pWindow];
}

void UICocoaApplication::nativeNotificationProxyForWidget(NativeNSStringRef pstrNotificationName, NativeNSWindowRef pWindow)
{
    /* Get notification name: */
    QString strNotificationName = darwinFromNativeString(pstrNotificationName);

    /* Check if existing widget(s) have corresponding notification handler: */
    foreach (QWidget *pWidget, m_widgetCallbacks.keys())
    {
        if (darwinToNativeWindow(pWidget) == pWindow)
        {
            const QMap<QString, PfnNativeNotificationCallbackForQWidget> &callbacks = m_widgetCallbacks[pWidget];
            if (callbacks.contains(strNotificationName))
                callbacks[strNotificationName](strNotificationName, pWidget);
        }
    }
}

