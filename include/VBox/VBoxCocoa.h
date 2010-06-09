/** @file
 *
 * VBoxCocoa Helper
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

#ifndef __VBoxCocoa_h__
#define __VBoxCocoa_h__

/* Macro which add a typedef of the given Cocoa class in an appropriate form
 * for the current context. This means void* in the C/CPP context and
 * NSWhatever* in the ObjC/ObjCPP context. Use
 * NativeNSWhateverRef/ConstNativeNSWhateverRef when you reference the Cocoa
 * type somewhere. The use of this prevents extensive casting of void* to the
 * right type in the Cocoa context. */
#ifdef __OBJC__
#define ADD_COCOA_NATIVE_REF(CocoaClass) \
@class CocoaClass; \
typedef CocoaClass *Native##CocoaClass##Ref; \
typedef const CocoaClass *ConstNative##CocoaClass##Ref
#else /* __OBJC__ */
#define ADD_COCOA_NATIVE_REF(CocoaClass) \
typedef void *Native##CocoaClass##Ref; \
typedef const void *ConstNative##CocoaClass##Ref
#endif /* __OBJC__ */

/* Check for OBJC++ */
#if defined(__OBJC__) && defined (__cplusplus)

/* Global includes */
#import <Foundation/NSAutoreleasePool.h>

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

#endif /* __VBoxCocoa_h__ */

