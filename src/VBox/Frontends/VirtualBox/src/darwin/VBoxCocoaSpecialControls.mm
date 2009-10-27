/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxCocoaSpecialControls implementation
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

/* VBox includes */
#include "VBoxCocoaSpecialControls.h"

/* System includes */
#import <AppKit/NSButton.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSSegmentedControl.h>
#import <AppKit/NSBezierPath.h>

/* Qt includes */
#include <QApplication>
#include <QKeyEvent>

@interface NSButtonTarget: NSObject 
{
    VBoxCocoaButton *mRealTarget;
}
-(id)initWithObject:(VBoxCocoaButton*)object;
-(IBAction)clicked:(id)sender;
@end

@implementation NSButtonTarget
-(id)initWithObject:(VBoxCocoaButton*)object
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

@interface NSSegmentedButtonTarget: NSObject 
{
    VBoxCocoaSegmentedButton *mRealTarget;
}
-(id)initWithObject1:(VBoxCocoaSegmentedButton*)object;
-(IBAction)segControlClicked:(id)sender;
@end

@implementation NSSegmentedButtonTarget
-(id)initWithObject1:(VBoxCocoaSegmentedButton*)object
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

@interface VBSearchField: NSSearchField 
{
    VBoxCocoaSearchField *mRealTarget;
}
-(id)initWithObject2:(VBoxCocoaSearchField*)object;
@end

@implementation VBSearchField
-(id)initWithObject2:(VBoxCocoaSearchField*)object
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
        QApplication::sendEvent (mRealTarget, &ke);
    } 
    else if (ch == 27) /* Escape */
    {
        QKeyEvent ke(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent (mRealTarget, &ke);
    } 
    else if (ch == NSF3FunctionKey)
    {
        QKeyEvent ke(QEvent::KeyPress, Qt::Key_F3, [theEvent modifierFlags] & NSShiftKeyMask ? Qt::ShiftModifier : Qt::NoModifier);
        QApplication::sendEvent (mRealTarget, &ke);
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
    mRealTarget->onTextChanged (::darwinNSStringToQString ([[aNotification object] string]));
}
@end

#if MAC_OS_X_VERSION_MIN_ALLOWED >= MAC_OS_X_VERSION_10_6
@interface VBSearchFieldDelegate: NSObject<NSTextFieldDelegate>
#else
@interface VBSearchFieldDelegate: NSObject
#endif
{}
@end
@implementation VBSearchFieldDelegate
-(BOOL)control:(NSControl*)control textView:(NSTextView*)textView doCommandBySelector:(SEL)commandSelector 
{
//    NSLog (NSStringFromSelector (commandSelector));
    /* Don't execute the selector for Enter & Escape. */
    if (   commandSelector == @selector(insertNewline:) 
	    || commandSelector == @selector(cancelOperation:)) 
		return YES;
    return NO;
}
@end

@interface VBSearchFieldCell: NSSearchFieldCell 
{
    NSColor *mBGColor;
}
- (void)setBackgroundColor:(NSColor*)aBGColor;
@end

@implementation VBSearchFieldCell
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
    if (mBGColor)
    {
        [mBGColor setFill];
        NSRect frame = cellFrame;
        frame.size.height -= 1;
        frame.origin.y += 1;
        double radius = MIN(frame.size.width, frame.size.height) / 2.0;
        [[NSBezierPath bezierPathWithRoundedRect:frame xRadius:radius yRadius:radius] fill];
    }

    [super drawInteriorWithFrame:cellFrame inView:controlView];
}
@end

NSRect darwinCenterRectVerticalTo (NSRect aRect, const NSRect& aToRect)
{
    aRect.origin.y = (aToRect.size.height - aRect.size.height) / 2.0;
    return aRect;
}

VBoxCocoaButton::VBoxCocoaButton (CocoaButtonType aType, QWidget *aParent /* = 0 */)
  : QMacCocoaViewContainer (0, aParent)
{
    switch (aType)
    {
        case HelpButton:
        {
            mNativeRef = [[NSButton alloc] init];
            [mNativeRef setTitle: @""];
            [mNativeRef setBezelStyle: NSHelpButtonBezelStyle];
            [mNativeRef setBordered: YES];
            [mNativeRef setAlignment: NSCenterTextAlignment];
            [mNativeRef sizeToFit];
            NSRect frame = [mNativeRef frame];
            frame.size.width += 12; /* Margin */
            [mNativeRef setFrame:frame];
            break;
        };
        case CancelButton:
        {
            mNativeRef = [[NSButton alloc] initWithFrame: NSMakeRect(0, 0, 13, 13)];
            [mNativeRef setTitle: @""];
            [mNativeRef setBezelStyle:NSShadowlessSquareBezelStyle];
            [mNativeRef setButtonType:NSMomentaryChangeButton];
            [mNativeRef setImage: [NSImage imageNamed: NSImageNameStopProgressFreestandingTemplate]];
            [mNativeRef setBordered: NO];
            [[mNativeRef cell] setImageScaling: NSImageScaleProportionallyDown];
            break;
        }
    }

    NSButtonTarget *bt = [[NSButtonTarget alloc] initWithObject:this];
    [mNativeRef setTarget:bt];
    [mNativeRef setAction:@selector(clicked:)];

    NSRect frame = [mNativeRef frame];
    setFixedSize (frame.size.width, frame.size.height);

    setCocoaView (mNativeRef);
}

QSize VBoxCocoaButton::sizeHint() const
{
    NSRect frame = [mNativeRef frame];
    return QSize (frame.size.width, frame.size.height);
}

void VBoxCocoaButton::setToolTip (const QString& aTip)
{
    [mNativeRef setToolTip: ::darwinQStringToNSString (aTip)];
}

void VBoxCocoaButton::onClicked()
{
    emit clicked (false);
}

VBoxCocoaSegmentedButton::VBoxCocoaSegmentedButton (int aCount, QWidget *aParent /* = 0 */)
  : QMacCocoaViewContainer (0, aParent)
{
    mNativeRef = [[NSSegmentedControl alloc] init];
    [mNativeRef setSegmentCount:aCount];
    [mNativeRef setSegmentStyle:NSSegmentStyleRoundRect];
    [[mNativeRef cell] setTrackingMode: NSSegmentSwitchTrackingMomentary];
    [mNativeRef setFont: [NSFont controlContentFontOfSize: 
        [NSFont systemFontSizeForControlSize: NSSmallControlSize]]];
    [mNativeRef sizeToFit];

    NSSegmentedButtonTarget *bt = [[NSSegmentedButtonTarget alloc] initWithObject1:this];
    [mNativeRef setTarget:bt];
    [mNativeRef setAction:@selector(segControlClicked:)];

    setCocoaView (mNativeRef);

    NSRect frame = [mNativeRef frame];
    resize (frame.size.width, frame.size.height);
}

QSize VBoxCocoaSegmentedButton::sizeHint() const
{
    NSRect frame = [mNativeRef frame];
    return QSize (frame.size.width, frame.size.height);
}

void VBoxCocoaSegmentedButton::setTitle (int aSegment, const QString &aTitle)
{
    QString s (aTitle);
    [mNativeRef setLabel: ::darwinQStringToNSString (s.remove ('&')) forSegment: aSegment];
    [mNativeRef sizeToFit];
    NSRect frame = [mNativeRef frame];
    resize (frame.size.width, frame.size.height);
}

void VBoxCocoaSegmentedButton::setToolTip (int aSegment, const QString &aTip)
{
    [[mNativeRef cell] setToolTip: ::darwinQStringToNSString (aTip) forSegment: aSegment];
}

void VBoxCocoaSegmentedButton::setEnabled (int aSegment, bool fEnabled)
{
    [[mNativeRef cell] setEnabled: fEnabled forSegment: aSegment];
}

void VBoxCocoaSegmentedButton::animateClick (int aSegment)
{
    [mNativeRef setSelectedSegment: aSegment];
    [[mNativeRef cell] performClick: mNativeRef];
}

void VBoxCocoaSegmentedButton::onClicked (int aSegment)
{
    emit clicked (aSegment, false);
}

VBoxCocoaSearchField::VBoxCocoaSearchField (QWidget *aParent /* = 0 */)
  : QMacCocoaViewContainer (0, aParent)
{
    mNativeRef = [[VBSearchField alloc] initWithObject2: this];
    [[mNativeRef cell] setControlSize: NSSmallControlSize];
    [mNativeRef setFont: [NSFont controlContentFontOfSize: 
        [NSFont systemFontSizeForControlSize: NSSmallControlSize]]];
    [mNativeRef sizeToFit];
    NSRect f = [mNativeRef frame];
    f.size.width = 180;
    [mNativeRef setFrame: f];

    setCocoaView (mNativeRef);
    setFocusPolicy(Qt::StrongFocus);

    /* Replace the cell class used for the NSSearchField */
    [NSKeyedArchiver setClassName:@"VBSearchFieldCell" forClass:[NSSearchFieldCell class]];
    [mNativeRef setCell:[NSKeyedUnarchiver unarchiveObjectWithData:[NSKeyedArchiver archivedDataWithRootObject:[mNativeRef cell]]]];
    /* Get the original behavior back */
    [NSKeyedArchiver setClassName:@"NSSearchFieldCell" forClass:[NSSearchFieldCell class]];

    /* Use our own delegate */
    VBSearchFieldDelegate *sfd = [[VBSearchFieldDelegate alloc] init];
    [mNativeRef setDelegate: sfd];

    NSRect frame = [mNativeRef frame];
    resize (frame.size.width, frame.size.height + 1);
}

QSize VBoxCocoaSearchField::sizeHint() const
{
    NSRect frame = [mNativeRef frame];
    return QSize (frame.size.width, frame.size.height);
}

QString VBoxCocoaSearchField::text() const
{
    return ::darwinNSStringToQString ([mNativeRef stringValue]);
}

void VBoxCocoaSearchField::insert (const QString &aText)
{
    [[mNativeRef currentEditor] setString: [[mNativeRef stringValue] stringByAppendingString: ::darwinQStringToNSString (aText)]];
}

void VBoxCocoaSearchField::setToolTip (const QString &aTip)
{
    [mNativeRef setToolTip: ::darwinQStringToNSString (aTip)];
}

void VBoxCocoaSearchField::selectAll()
{
    [mNativeRef selectText: mNativeRef];
}

void VBoxCocoaSearchField::markError()
{
    [[mNativeRef cell] setBackgroundColor: [[NSColor redColor] colorWithAlphaComponent: 0.3]];
}

void VBoxCocoaSearchField::unmarkError()
{
    [[mNativeRef cell] setBackgroundColor: nil];
}

void VBoxCocoaSearchField::onTextChanged (const QString &aText)
{
    emit textChanged (aText);
}

