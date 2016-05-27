/* $Id$ */
/** @file
 * VBox Qt GUI - UICocoaSpecialControls implementation.
 */

/*
 * Copyright (C) 2009-2011 Oracle Corporation
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
#include "UICocoaSpecialControls.h"
#include "VBoxUtils-darwin.h"
#include "UIImageTools.h"
#include <VBox/cdefs.h>

/* System includes */
#import <AppKit/NSApplication.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSButton.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSSegmentedControl.h>
#if QT_VERSION >= 0x050000
# import <AppKit/NSEvent.h>
# import <AppKit/NSColor.h>
# import <AppKit/NSSearchFieldCell.h>
# import <AppKit/NSSearchField.h>
# import <AppKit/NSSegmentedCell.h>
#endif /* QT_VERSION >= 0x050000 */

/* Qt includes */
#include <QApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QMacCocoaViewContainer>

/* Interface Builder Constant,
 * hmm, where is it declared with Qt4? */
#if QT_VERSION >= 0x050000
# define IBAction void
#endif /* QT_VERSION >= 0x050000 */

/*
 * Private interfaces
 */
@interface UIButtonTargetPrivate: NSObject
{
    UICocoaButton *mRealTarget;
}
/* The next method used to be called initWithObject, but Xcode 4.1 preview 5
   cannot cope with that for some reason.  Hope this doesn't break anything... */
-(id)initWithObjectAndLionTrouble:(UICocoaButton*)object;
-(IBAction)clicked:(id)sender;
@end

@interface UISegmentedButtonTargetPrivate: NSObject
{
    UICocoaSegmentedButton *mRealTarget;
}
-(id)initWithObject1:(UICocoaSegmentedButton*)object;
-(IBAction)segControlClicked:(id)sender;
@end

@interface UISearchFieldCellPrivate: NSSearchFieldCell
{
    NSColor *mBGColor;
}
- (void)setBackgroundColor:(NSColor*)aBGColor;
@end

@interface UISearchFieldPrivate: NSSearchField
{
    UICocoaSearchField *mRealTarget;
}
-(id)initWithObject2:(UICocoaSearchField*)object;
@end

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
@interface UISearchFieldDelegatePrivate: NSObject<NSTextFieldDelegate>
#else
@interface UISearchFieldDelegatePrivate: NSObject
#endif
{}
@end

/*
 * Implementation of the private interfaces
 */
@implementation UIButtonTargetPrivate
-(id)initWithObjectAndLionTrouble:(UICocoaButton*)object
{
    self = [super init];

    mRealTarget = object;

    return self;
}

-(IBAction)clicked:(id)sender
{
    mRealTarget->onClicked();
}
@end

@implementation UISegmentedButtonTargetPrivate
-(id)initWithObject1:(UICocoaSegmentedButton*)object
{
    self = [super init];

    mRealTarget = object;

    return self;
}

-(IBAction)segControlClicked:(id)sender
{
    mRealTarget->onClicked([sender selectedSegment]);
}
@end

@implementation UISearchFieldCellPrivate
-(id)init
{
    if ((self = [super init]))
        mBGColor = Nil;
    return self;
}

- (void)dealloc
{
    [mBGColor release];
    [super dealloc];
}

- (void)setBackgroundColor:(NSColor*)aBGColor
{
    if (mBGColor != aBGColor)
    {
        [mBGColor release];
        mBGColor = [aBGColor retain];
    }
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    if (mBGColor != Nil)
    {
        [mBGColor setFill];
        NSRect frame = cellFrame;
        double radius = RT_MIN(NSWidth(frame), NSHeight(frame)) / 2.0;
        [[NSBezierPath bezierPathWithRoundedRect:frame xRadius:radius yRadius:radius] fill];
    }

    [super drawInteriorWithFrame:cellFrame inView:controlView];
}
@end

@implementation UISearchFieldPrivate
+ (Class)cellClass
{
    return [UISearchFieldCellPrivate class];
}

-(id)initWithObject2:(UICocoaSearchField*)object
{
    self = [super init];

    mRealTarget = object;


    return self;
}

- (void)keyUp:(NSEvent *)theEvent
{
    /* This here is a little bit hacky. Grab important keys & forward they to
       the parent Qt widget. There a special key handling is done. */
    NSString *str = [theEvent charactersIgnoringModifiers];
    unichar ch = 0;

    /* Get the pressed character */
    if ([str length] > 0)
        ch = [str characterAtIndex:0];

    if (ch == NSCarriageReturnCharacter || ch == NSEnterCharacter)
    {
        QKeyEvent ke(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
        QApplication::sendEvent(mRealTarget, &ke);
    }
    else if (ch == 27) /* Escape */
    {
        QKeyEvent ke(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(mRealTarget, &ke);
    }
    else if (ch == NSF3FunctionKey)
    {
        QKeyEvent ke(QEvent::KeyPress, Qt::Key_F3, [theEvent modifierFlags] & NSShiftKeyMask ? Qt::ShiftModifier : Qt::NoModifier);
        QApplication::sendEvent(mRealTarget, &ke);
    }

    [super keyUp:theEvent];
}

//{
//    QWidget *w = QApplication::focusWidget();
//    if (w)
//        w->clearFocus();
//}

- (void)textDidChange:(NSNotification *)aNotification
{
    mRealTarget->onTextChanged(::darwinNSStringToQString([[aNotification object] string]));
}
@end

@implementation UISearchFieldDelegatePrivate
-(BOOL)control:(NSControl*)control textView:(NSTextView*)textView doCommandBySelector:(SEL)commandSelector
{
//    NSLog(NSStringFromSelector(commandSelector));
    /* Don't execute the selector for Enter & Escape. */
    if (   commandSelector == @selector(insertNewline:)
            || commandSelector == @selector(cancelOperation:))
                return YES;
    return NO;
}
@end


/*
 * Helper functions
 */
NSRect darwinCenterRectVerticalTo(NSRect aRect, const NSRect& aToRect)
{
    aRect.origin.y = (aToRect.size.height - aRect.size.height) / 2.0;
    return aRect;
}

/*
 * Public classes
 */
UICocoaButton::UICocoaButton(QWidget *pParent, CocoaButtonType type)
    : QMacCocoaViewContainer(0, pParent)
{
    /* Prepare auto-release pool: */
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    /* Prepare native button reference: */
    NativeNSButtonRef pNativeRef;
    NSRect initFrame;

    /* Configure button: */
    switch (type)
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
    [pool release];
}

UICocoaButton::~UICocoaButton()
{
}

QSize UICocoaButton::sizeHint() const
{
    NSRect frame = [nativeRef() frame];
    return QSize(frame.size.width, frame.size.height);
}

void UICocoaButton::setText(const QString& strText)
{
    QString s(strText);
    /* Set it for accessibility reasons as alternative title */
    [nativeRef() setAlternateTitle: ::darwinQStringToNSString(s.remove('&'))];
}

void UICocoaButton::setToolTip(const QString& strTip)
{
    [nativeRef() setToolTip: ::darwinQStringToNSString(strTip)];
}

void UICocoaButton::onClicked()
{
    emit clicked(false);
}

UICocoaSegmentedButton::UICocoaSegmentedButton(QWidget *pParent, int count, CocoaSegmentType type /* = RoundRectSegment */)
    : QMacCocoaViewContainer(0, pParent)
{
    /* Prepare auto-release pool: */
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    /* Prepare native segmented-button reference: */
    NativeNSSegmentedControlRef pNativeRef;
    NSRect initFrame;

    /* Configure segmented-button: */
    pNativeRef = [[NSSegmentedControl alloc] init];
    [pNativeRef setSegmentCount:count];
    switch (type)
    {
        case RoundRectSegment:
        {
            [pNativeRef setSegmentStyle: NSSegmentStyleRoundRect];
            [pNativeRef setFont: [NSFont controlContentFontOfSize:
                [NSFont systemFontSizeForControlSize: NSSmallControlSize]]];
            [[pNativeRef cell] setTrackingMode: NSSegmentSwitchTrackingMomentary];
            break;
        }
        case TexturedRoundedSegment:
        {
            [pNativeRef setSegmentStyle: NSSegmentStyleTexturedRounded];
            [pNativeRef setFont: [NSFont controlContentFontOfSize:
                [NSFont systemFontSizeForControlSize: NSSmallControlSize]]];
            break;
        }
    }

    /* Calculate corresponding size: */
    [pNativeRef sizeToFit];
    initFrame = [pNativeRef frame];

    /* Install click listener: */
    UISegmentedButtonTargetPrivate *bt = [[UISegmentedButtonTargetPrivate alloc] initWithObject1:this];
    [pNativeRef setTarget:bt];
    [pNativeRef setAction:@selector(segControlClicked:)];

    /* Put the button to the QCocoaViewContainer: */
    setCocoaView(pNativeRef);
    /* Release our reference, since our super class
     * takes ownership and we don't need it anymore. */
    [pNativeRef release];

    /* Finally resize the widget: */
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(NSWidth(initFrame), NSHeight(initFrame));

    /* Cleanup auto-release pool: */
    [pool release];
}

UICocoaSegmentedButton::~UICocoaSegmentedButton()
{
}

QSize UICocoaSegmentedButton::sizeHint() const
{
    NSRect frame = [nativeRef() frame];
    return QSize(frame.size.width, frame.size.height);
}

void UICocoaSegmentedButton::setTitle(int iSegment, const QString &strTitle)
{
    QString s(strTitle);
    [nativeRef() setLabel: ::darwinQStringToNSString(s.remove('&')) forSegment: iSegment];
    [nativeRef() sizeToFit];
    NSRect frame = [nativeRef() frame];
    setFixedSize(NSWidth(frame), NSHeight(frame));
}

void UICocoaSegmentedButton::setToolTip(int iSegment, const QString &strTip)
{
    [[nativeRef() cell] setToolTip: ::darwinQStringToNSString(strTip) forSegment: iSegment];
}

void UICocoaSegmentedButton::setIcon(int iSegment, const QIcon& icon)
{
    QImage image = toGray(icon.pixmap(icon.availableSizes().first()).toImage());

    NSImage *pNSimage = [::darwinToNSImageRef(&image) autorelease];
    [nativeRef() setImage: pNSimage forSegment: iSegment];
    [nativeRef() sizeToFit];
    NSRect frame = [nativeRef() frame];
    setFixedSize(NSWidth(frame), NSHeight(frame));
}

void UICocoaSegmentedButton::setEnabled(int iSegment, bool fEnabled)
{
    [[nativeRef() cell] setEnabled: fEnabled forSegment: iSegment];
}

void UICocoaSegmentedButton::animateClick(int iSegment)
{
    [nativeRef() setSelectedSegment: iSegment];
    [[nativeRef() cell] performClick: nativeRef()];
}

void UICocoaSegmentedButton::onClicked(int iSegment)
{
    emit clicked(iSegment, false);
}

UICocoaSearchField::UICocoaSearchField(QWidget *pParent)
    : QMacCocoaViewContainer(0, pParent)
{
    /* Prepare auto-release pool: */
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    /* Prepare native search-field reference: */
    NativeNSSearchFieldRef pNativeRef;
    NSRect initFrame;

    /* Configure search-field: */
    pNativeRef = [[UISearchFieldPrivate alloc] initWithObject2: this];
    [pNativeRef setFont: [NSFont controlContentFontOfSize:
        [NSFont systemFontSizeForControlSize: NSMiniControlSize]]];
    [[pNativeRef cell] setControlSize: NSMiniControlSize];
    [pNativeRef sizeToFit];
    initFrame = [pNativeRef frame];
    initFrame.size.width = 180;
    [pNativeRef setFrame: initFrame];

    /* Use our own delegate: */
    UISearchFieldDelegatePrivate *sfd = [[UISearchFieldDelegatePrivate alloc] init];
    [pNativeRef setDelegate: sfd];

    /* Put the button to the QCocoaViewContainer: */
    setCocoaView(pNativeRef);
    /* Release our reference, since our super class
     * takes ownership and we don't need it anymore. */
    [pNativeRef release];

    /* Finally resize the widget: */
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setMinimumWidth(NSWidth(initFrame));
    setFixedHeight(NSHeight(initFrame));
    setFocusPolicy(Qt::StrongFocus);

    /* Cleanup auto-release pool: */
    [pool release];
}

UICocoaSearchField::~UICocoaSearchField()
{
}

QSize UICocoaSearchField::sizeHint() const
{
    NSRect frame = [nativeRef() frame];
    return QSize(frame.size.width, frame.size.height);
}

QString UICocoaSearchField::text() const
{
    return ::darwinNSStringToQString([nativeRef() stringValue]);
}

void UICocoaSearchField::insert(const QString &strText)
{
    [[nativeRef() currentEditor] setString: [[nativeRef() stringValue] stringByAppendingString: ::darwinQStringToNSString(strText)]];
}

void UICocoaSearchField::setToolTip(const QString &strTip)
{
    [nativeRef() setToolTip: ::darwinQStringToNSString(strTip)];
}

void UICocoaSearchField::selectAll()
{
    [nativeRef() selectText: nativeRef()];
}

void UICocoaSearchField::markError()
{
    [[nativeRef() cell] setBackgroundColor: [[NSColor redColor] colorWithAlphaComponent: 0.3]];
}

void UICocoaSearchField::unmarkError()
{
    [[nativeRef() cell] setBackgroundColor: nil];
}

void UICocoaSearchField::onTextChanged(const QString &strText)
{
    emit textChanged(strText);
}

