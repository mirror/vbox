/* $Id$ */

/** @file
 * VBox host opengl support test
 */

/*
 * Copyright (C) 2009-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#include <IOKit/IOKitLib.h>
#include <OpenGL/OpenGL.h>
#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/gl.h>
#ifdef VBOX_WITH_COCOA_QT
# include <OpenGL/glu.h>
# include <iprt/log.h>
#endif /* VBOX_WITH_COCOA_QT */
#include <iprt/env.h>
#include <iprt/log.h>

#include <iprt/asm.h>
#include <iprt/thread.h>

#include <VBox/VBoxOGLTest.h>

bool RTCALL VBoxOglIsOfflineRenderingAppropriate()
{
    /* It is assumed that it is makes sense to enable offline rendering
       only in case if host has more than one GPU installed. This routine
       counts all the PCI devices in IORegistry which have IOName property
       set to "display". If the amount of such devices if greater than one,
       it returns TRUE or FALSE otherwise. */

    kern_return_t   krc;
    io_iterator_t   matchingServices;
    CFDictionaryRef pMatchingDictionary;
    static bool     fAppropriate = false;

    /* In order to do not slowdown 3D engine which can ask about offline rendering several times,
       let's cache the result and assume that renderers amount value is constant. Also prevent threads race
       on startup. */

    static bool volatile fState = false;
    if (!ASMAtomicCmpXchgBool(&fState, true, false))
    {
        while (ASMAtomicReadBool(&fState) != true)
            RTThreadSleep(5);

        return fAppropriate;
    }

#define VBOX_OGL_RENDERER_MATCH_KEYS_NUM    (2)

    CFStringRef ppDictionaryKeys[VBOX_OGL_RENDERER_MATCH_KEYS_NUM] = { CFSTR(kIOProviderClassKey), CFSTR(kIONameMatchKey) };
    CFStringRef ppDictionaryVals[VBOX_OGL_RENDERER_MATCH_KEYS_NUM] = { CFSTR("IOPCIDevice"),       CFSTR("display") };

    pMatchingDictionary = CFDictionaryCreate(kCFAllocatorDefault,
                                             (const void **)ppDictionaryKeys,
                                             (const void **)ppDictionaryVals,
                                             VBOX_OGL_RENDERER_MATCH_KEYS_NUM,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);
    if (pMatchingDictionary)
    {
        /* The reference to pMatchingDictionary is consumed by the function below => no IORelease(pMatchingDictionary)! */
        krc = IOServiceGetMatchingServices(kIOMasterPortDefault, pMatchingDictionary, &matchingServices);
        if (krc == kIOReturnSuccess)
        {
            io_object_t matchingService;
            int         cMatchingServices = 0;

            while ((matchingService = IOIteratorNext(matchingServices)) != 0)
            {
                cMatchingServices++;
                IOObjectRelease(matchingService);
            }

            fAppropriate = (cMatchingServices > 1);

            IOObjectRelease(matchingServices);
        }

    }

    LogRel(("OpenGL: Offline rendering support is %s (PID=%d)\n", fAppropriate ? "ON" : "OFF", (int)getpid()));

    return fAppropriate;
}

bool RTCALL VBoxOglIs3DAccelerationSupported()
{
    if (RTEnvExist("VBOX_CROGL_FORCE_SUPPORTED"))
    {
        LogRel(("VBOX_CROGL_FORCE_SUPPORTED is specified, skipping 3D test, and treating as supported\n"));
        return true;
    }

    CGDirectDisplayID   display = CGMainDisplayID ();
    CGOpenGLDisplayMask cglDisplayMask = CGDisplayIDToOpenGLDisplayMask (display);
    CGLPixelFormatObj   pixelFormat = NULL;
    GLint numPixelFormats = 0;

    CGLPixelFormatAttribute attribs[] = {
        kCGLPFADisplayMask,
        (CGLPixelFormatAttribute)cglDisplayMask,
        kCGLPFAAccelerated,
        kCGLPFADoubleBuffer,
        kCGLPFAWindow,
        VBoxOglIsOfflineRenderingAppropriate() ? kCGLPFAAllowOfflineRenderers : (CGLPixelFormatAttribute)NULL,
        (CGLPixelFormatAttribute)NULL
    };

    display = CGMainDisplayID();
    cglDisplayMask = CGDisplayIDToOpenGLDisplayMask(display);
    CGLChoosePixelFormat(attribs, &pixelFormat, &numPixelFormats);

    if (pixelFormat)
    {
        CGLContextObj cglContext = 0;
        CGLCreateContext(pixelFormat, NULL, &cglContext);
        CGLDestroyPixelFormat(pixelFormat);
        if (cglContext)
        {
            GLboolean isSupported = GL_TRUE;
#ifdef VBOX_WITH_COCOA_QT
            /* On the Cocoa port we depend on the GL_EXT_framebuffer_object &
             * the GL_EXT_texture_rectangle extension. If they are not
             * available, disable 3D support. */
            CGLSetCurrentContext(cglContext);
            const GLubyte* strExt;
            strExt = glGetString(GL_EXTENSIONS);
            isSupported = gluCheckExtension((const GLubyte*)"GL_EXT_framebuffer_object", strExt);
            if (isSupported)
            {
                isSupported = gluCheckExtension((const GLubyte*)"GL_EXT_texture_rectangle", strExt);
                if (!isSupported)
                    LogRel(("OpenGL Info: GL_EXT_texture_rectangle extension not supported\n"));
            }
            else
                LogRel(("OpenGL Info: GL_EXT_framebuffer_object extension not supported\n"));
#endif /* VBOX_WITH_COCOA_QT */
            CGLDestroyContext(cglContext);
            return isSupported == GL_TRUE ? true : false;
        }
    }

    return false;
}

