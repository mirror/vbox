/* $Id$ */
/** @file
 * VBox Qt GUI - UICocoaSpecialControls implementation.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QMacCocoaViewContainer>

/* GUI includes: */
#include "VBoxUtils-darwin.h"
#include "UICocoaSpecialControls.h"

/* System includes: */
#import <AppKit/NSButton.h>


/** Private button-target interface. */
@interface UIButtonTargetPrivate : NSObject
{
    UICocoaButton *m_pRealTarget;
}
// WORKAROUND:
// The next method used to be called initWithObject, but Xcode 4.1 preview 5
// cannot cope with that for some reason.  Hope this doesn't break anything.
-(id)initWithObjectAndLionTrouble:(UICocoaButton*)pObject;
-(IBAction)clicked:(id)pSender;
@end


/*********************************************************************************************************************************
*   Class UIButtonTargetPrivate implementation.                                                                                  *
*********************************************************************************************************************************/

@implementation UIButtonTargetPrivate
-(id)initWithObjectAndLionTrouble:(UICocoaButton*)pObject
{
    self = [super init];

    m_pRealTarget = pObject;

    return self;
}

-(IBAction)clicked:(id)pSender
{
    Q_UNUSED(pSender);
    m_pRealTarget->onClicked();
}
@end


/*********************************************************************************************************************************
*   Class UICocoaButton implementation.                                                                                          *
*********************************************************************************************************************************/

UICocoaButton::UICocoaButton(QWidget *pParent, CocoaButtonType enmType)
    : QMacCocoaViewContainer(0, pParent)
{
    /* Prepare auto-release pool: */
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    /* Prepare native button reference: */
    NativeNSButtonRef pNativeRef;
    NSRect initFrame;

    /* Configure button: */
    switch (enmType)
    {
        case HelpButton:
        {
            pNativeRef = [[NSButton alloc] init];
            [pNativeRef setTitle: @""];
            [pNativeRef setBezelStyle: NSHelpButtonBezelStyle];
            [pNativeRef setBordered: YES];
            [pNativeRef setAlignment: NSCenterTextAlignment];
            [pNativeRef sizeToFit];
            initFrame = [pNativeRef frame];
            initFrame.size.width += 12; /* Margin */
            [pNativeRef setFrame:initFrame];
            break;
        };
        case CancelButton:
        {
            pNativeRef = [[NSButton alloc] initWithFrame: NSMakeRect(0, 0, 13, 13)];
            [pNativeRef setTitle: @""];
            [pNativeRef setBezelStyle:NSShadowlessSquareBezelStyle];
            [pNativeRef setButtonType:NSMomentaryChangeButton];
            [pNativeRef setImage: [NSImage imageNamed: NSImageNameStopProgressFreestandingTemplate]];
            [pNativeRef setBordered: NO];
            [[pNativeRef cell] setImageScaling: NSImageScaleProportionallyDown];
            initFrame = [pNativeRef frame];
            break;
        }
        case ResetButton:
        {
            pNativeRef = [[NSButton alloc] initWithFrame: NSMakeRect(0, 0, 13, 13)];
            [pNativeRef setTitle: @""];
            [pNativeRef setBezelStyle:NSShadowlessSquareBezelStyle];
            [pNativeRef setButtonType:NSMomentaryChangeButton];
            [pNativeRef setImage: [NSImage imageNamed: NSImageNameRefreshFreestandingTemplate]];
            [pNativeRef setBordered: NO];
            [[pNativeRef cell] setImageScaling: NSImageScaleProportionallyDown];
            initFrame = [pNativeRef frame];
            break;
        }
    }

    /* Install click listener: */
    UIButtonTargetPrivate *bt = [[UIButtonTargetPrivate alloc] initWithObjectAndLionTrouble:this];
    [pNativeRef setTarget:bt];
    [pNativeRef setAction:@selector(clicked:)];

    /* Put the button to the QCocoaViewContainer: */
    setCocoaView(pNativeRef);
    /* Release our reference, since our super class
     * takes ownership and we don't need it anymore. */
    [pNativeRef release];

    /* Finally resize the widget: */
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(NSWidth(initFrame), NSHeight(initFrame));

    /* Cleanup auto-release pool: */
    [pPool release];
}

UICocoaButton::~UICocoaButton()
{
}

QSize UICocoaButton::sizeHint() const
{
    NSRect frame = [nativeRef() frame];
    return QSize(frame.size.width, frame.size.height);
}

void UICocoaButton::setText(const QString &strText)
{
    QString s(strText);
    /* Set it for accessibility reasons as alternative title: */
    [nativeRef() setAlternateTitle: ::darwinQStringToNSString(s.remove('&'))];
}

void UICocoaButton::setToolTip(const QString &strToolTip)
{
    [nativeRef() setToolTip: ::darwinQStringToNSString(strToolTip)];
}

void UICocoaButton::onClicked()
{
    emit clicked(false);
}

