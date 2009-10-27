/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxCocoa Helper
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

#ifndef ___darwin_VBoxCocoaHelper_h
#define ___darwin_VBoxCocoaHelper_h

/* Macro which add a typedef of the given Cocoa class in an appropriate form
 * for the current context. This means void* in the C/CPP context and
 * NSWhatever* in the ObjC/ObjCPP context. Use NativeNSWhateverRef when you
 * reference the Cocoa type somewhere. The use of this prevents extensive
 * casting of void* to the right type in the Cocoa context. */
#ifdef __OBJC__
#define ADD_COCOA_NATIVE_REF(CocoaClass) \
@class CocoaClass; \
typedef CocoaClass *Native##CocoaClass##Ref
#else /* __OBJC__ */
#define ADD_COCOA_NATIVE_REF(CocoaClass) typedef void *Native##CocoaClass##Ref
#endif /* __OBJC__ */

#ifdef __OBJC__

/* System includes */
#import <Appkit/NSImage.h>
#import <Foundation/NSAutoreleasePool.h>
#import <CoreFoundation/CFString.h>

/* Qt includes */
#include <QString>
#include <QVarLengthArray>

inline NSString *darwinQStringToNSString (const QString &aString)
{
    return [reinterpret_cast<const NSString *>(CFStringCreateWithCharacters (0, reinterpret_cast<const UniChar *> (aString.unicode()),
                                                                             aString.length())) autorelease];
}

inline QString darwinNSStringToQString (const NSString *aString)
{
    CFStringRef str = reinterpret_cast<const CFStringRef> (aString);
    if(!str)
        return QString();
    CFIndex length = CFStringGetLength (str);
    const UniChar *chars = CFStringGetCharactersPtr (str);
    if (chars)
        return QString (reinterpret_cast<const QChar *> (chars), length);

    QVarLengthArray<UniChar> buffer (length);
    CFStringGetCharacters (str, CFRangeMake (0, length), buffer.data());
    return QString (reinterpret_cast<const QChar *> (buffer.constData()), length);
}

inline NSImage *darwinCGImageToNSImage (const CGImageRef aImage)
{
    /* Create a bitmap rep from the image. */
    NSBitmapImageRep *bitmapRep = [[NSBitmapImageRep alloc] initWithCGImage:aImage];
    /* Create an NSImage and add the bitmap rep to it */
    NSImage *image = [[NSImage alloc] init];
    [image addRepresentation:bitmapRep];
    [bitmapRep release];
    return image;
}

/* Helper class for automatic creation & destroying of a cocoa auto release
   pool. */
class CocoaAutoreleasePool
{
public:
    inline CocoaAutoreleasePool()
    {
         mPool = [[NSAutoreleasePool alloc] init];
    }
    inline ~CocoaAutoreleasePool()
    {
        [mPool release];
    }

private:
    NSAutoreleasePool *mPool;
};

#endif /* __OBJC__ */

#endif /* ___darwin_VBoxCocoaHelper_h */

