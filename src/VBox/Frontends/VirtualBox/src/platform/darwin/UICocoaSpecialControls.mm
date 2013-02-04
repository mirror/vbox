/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UICocoaSpecialControls implementation
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

/* Qt includes */
#include <QApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QMacCocoaViewContainer>

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

-(IBAction)clicked:(id)sender;
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

-(IBAction)segControlClicked:(id)sender;
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
UICocoaWrapper::UICocoaWrapper(QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pContainer(0)
{
}

void UICocoaWrapper::resizeEvent(QResizeEvent *pEvent)
{
    if (m_pContainer)
        m_pContainer->resize(pEvent->size());
    QWidget::resizeEvent(pEvent);
}

UICocoaButton::UICocoaButton(CocoaButtonType aType, QWidget *pParent /* = 0 */)
  : UICocoaWrapper(pParent)
{
    NSRect initFrame;

    switch (aType)
    {
        case HelpButton:
        {
            m_pNativeRef = [[NSButton alloc] init];
            [m_pNativeRef setTitle: @""];
            [m_pNativeRef setBezelStyle: NSHelpButtonBezelStyle];
            [m_pNativeRef setBordered: YES];
            [m_pNativeRef setAlignment: NSCenterTextAlignment];
            [m_pNativeRef sizeToFit];
            initFrame = [m_pNativeRef frame];
            initFrame.size.width += 12; /* Margin */
            [m_pNativeRef setFrame:initFrame];
            break;
        };
        case CancelButton:
        {
            m_pNativeRef = [[NSButton alloc] initWithFrame: NSMakeRect(0, 0, 13, 13)];
            [m_pNativeRef setTitle: @""];
            [m_pNativeRef setBezelStyle:NSShadowlessSquareBezelStyle];
            [m_pNativeRef setButtonType:NSMomentaryChangeButton];
            [m_pNativeRef setImage: [NSImage imageNamed: NSImageNameStopProgressFreestandingTemplate]];
            [m_pNativeRef setBordered: NO];
            [[m_pNativeRef cell] setImageScaling: NSImageScaleProportionallyDown];
            initFrame = [m_pNativeRef frame];
            break;
        }
        case ResetButton:
        {
            m_pNativeRef = [[NSButton alloc] initWithFrame: NSMakeRect(0, 0, 13, 13)];
            [m_pNativeRef setTitle: @""];
            [m_pNativeRef setBezelStyle:NSShadowlessSquareBezelStyle];
            [m_pNativeRef setButtonType:NSMomentaryChangeButton];
            [m_pNativeRef setImage: [NSImage imageNamed: NSImageNameRefreshFreestandingTemplate]];
            [m_pNativeRef setBordered: NO];
            [[m_pNativeRef cell] setImageScaling: NSImageScaleProportionallyDown];
            initFrame = [m_pNativeRef frame];
            break;
        }
    }

    UIButtonTargetPrivate *bt = [[UIButtonTargetPrivate alloc] initWithObjectAndLionTrouble:this];
    [m_pNativeRef setTarget:bt];
    [m_pNativeRef setAction:@selector(clicked:)];

    /* Create the container widget and attach the native view. */
    m_pContainer = new QMacCocoaViewContainer(m_pNativeRef, this);
    /* Finally resize the widget */
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(NSWidth(initFrame), NSHeight(initFrame));
}

UICocoaButton::~UICocoaButton()
{
}

QSize UICocoaButton::sizeHint() const
{
    NSRect frame = [m_pNativeRef frame];
    return QSize(frame.size.width, frame.size.height);
}

void UICocoaButton::setText(const QString& strText)
{
    QString s(strText);
    /* Set it for accessibility reasons as alternative title */
    [m_pNativeRef setAlternateTitle: ::darwinQStringToNSString(s.remove('&'))];
}

void UICocoaButton::setToolTip(const QString& strTip)
{
    [m_pNativeRef setToolTip: ::darwinQStringToNSString(strTip)];
}

void UICocoaButton::onClicked()
{
    emit clicked(false);
}

UICocoaSegmentedButton::UICocoaSegmentedButton(int count, CocoaSegmentType type /* = RoundRectSegment */, QWidget *pParent /* = 0 */)
  : UICocoaWrapper(pParent)
{
    NSRect initFrame;

    m_pNativeRef = [[NSSegmentedControl alloc] init];
    [m_pNativeRef setSegmentCount:count];
    switch (type)
    {
        case RoundRectSegment:
        {
            [[m_pNativeRef cell] setTrackingMode: NSSegmentSwitchTrackingMomentary];
            [m_pNativeRef setFont: [NSFont controlContentFontOfSize:
                [NSFont systemFontSizeForControlSize: NSSmallControlSize]]];
            [m_pNativeRef setSegmentStyle:NSSegmentStyleRoundRect];
            break;
        }
        case TexturedRoundedSegment:
        {
            [m_pNativeRef setSegmentStyle:NSSegmentStyleTexturedRounded];
            [m_pNativeRef setFont: [NSFont controlContentFontOfSize:
                [NSFont systemFontSizeForControlSize: NSSmallControlSize]]];
            break;
        }
    }
    [m_pNativeRef sizeToFit];

    UISegmentedButtonTargetPrivate *bt = [[UISegmentedButtonTargetPrivate alloc] initWithObject1:this];
    [m_pNativeRef setTarget:bt];
    [m_pNativeRef setAction:@selector(segControlClicked:)];

    initFrame = [m_pNativeRef frame];
    /* Create the container widget and attach the native view. */
    m_pContainer = new QMacCocoaViewContainer(m_pNativeRef, this);
    /* Finally resize the widget */
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(NSWidth(initFrame), NSHeight(initFrame));
}

UICocoaSegmentedButton::~UICocoaSegmentedButton()
{
}

QSize UICocoaSegmentedButton::sizeHint() const
{
    NSRect frame = [m_pNativeRef frame];
    return QSize(frame.size.width, frame.size.height);
}

void UICocoaSegmentedButton::setTitle(int iSegment, const QString &strTitle)
{
    QString s(strTitle);
    [m_pNativeRef setLabel: ::darwinQStringToNSString(s.remove('&')) forSegment: iSegment];
    [m_pNativeRef sizeToFit];
    NSRect frame = [m_pNativeRef frame];
    setFixedSize(NSWidth(frame), NSHeight(frame));
}

void UICocoaSegmentedButton::setToolTip(int iSegment, const QString &strTip)
{
    [[m_pNativeRef cell] setToolTip: ::darwinQStringToNSString(strTip) forSegment: iSegment];
}

void UICocoaSegmentedButton::setIcon(int iSegment, const QIcon& icon)
{
    QImage image = toGray(icon.pixmap(icon.actualSize(QSize(13, 13))).toImage());

    NSImage *pNSimage = [::darwinToNSImageRef(&image) autorelease];
    [m_pNativeRef setImage: pNSimage forSegment: iSegment];
    [m_pNativeRef sizeToFit];
    NSRect frame = [m_pNativeRef frame];
    setFixedSize(NSWidth(frame), NSHeight(frame));
}

void UICocoaSegmentedButton::setEnabled(int iSegment, bool fEnabled)
{
    [[m_pNativeRef cell] setEnabled: fEnabled forSegment: iSegment];
}

void UICocoaSegmentedButton::animateClick(int iSegment)
{
    [m_pNativeRef setSelectedSegment: iSegment];
    [[m_pNativeRef cell] performClick: m_pNativeRef];
}

void UICocoaSegmentedButton::onClicked(int iSegment)
{
    emit clicked(iSegment, false);
}

UICocoaSearchField::UICocoaSearchField(QWidget *pParent /* = 0 */)
  : UICocoaWrapper(pParent)
{
    NSRect initFrame;

    m_pNativeRef = [[UISearchFieldPrivate alloc] initWithObject2: this];
    [m_pNativeRef setFont: [NSFont controlContentFontOfSize:
        [NSFont systemFontSizeForControlSize: NSMiniControlSize]]];
    [[m_pNativeRef cell] setControlSize: NSMiniControlSize];
    [m_pNativeRef sizeToFit];
    initFrame = [m_pNativeRef frame];
    initFrame.size.width = 180;
    [m_pNativeRef setFrame: initFrame];

    /* Use our own delegate */
    UISearchFieldDelegatePrivate *sfd = [[UISearchFieldDelegatePrivate alloc] init];
    [m_pNativeRef setDelegate: sfd];

    /* Create the container widget and attach the native view. */
    m_pContainer = new QMacCocoaViewContainer(m_pNativeRef, this);

    /* Finally resize the widget */
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setMinimumWidth(NSWidth(initFrame));
    setFixedHeight(NSHeight(initFrame));
    setFocusPolicy(Qt::StrongFocus);
}

UICocoaSearchField::~UICocoaSearchField()
{
}

QSize UICocoaSearchField::sizeHint() const
{
    NSRect frame = [m_pNativeRef frame];
    return QSize(frame.size.width, frame.size.height);
}

QString UICocoaSearchField::text() const
{
    return ::darwinNSStringToQString([m_pNativeRef stringValue]);
}

void UICocoaSearchField::insert(const QString &strText)
{
    [[m_pNativeRef currentEditor] setString: [[m_pNativeRef stringValue] stringByAppendingString: ::darwinQStringToNSString(strText)]];
}

void UICocoaSearchField::setToolTip(const QString &strTip)
{
    [m_pNativeRef setToolTip: ::darwinQStringToNSString(strTip)];
}

void UICocoaSearchField::selectAll()
{
    [m_pNativeRef selectText: m_pNativeRef];
}

void UICocoaSearchField::markError()
{
    [[m_pNativeRef cell] setBackgroundColor: [[NSColor redColor] colorWithAlphaComponent: 0.3]];
}

void UICocoaSearchField::unmarkError()
{
    [[m_pNativeRef cell] setBackgroundColor: nil];
}

void UICocoaSearchField::onTextChanged(const QString &strText)
{
    emit textChanged(strText);
}

