/* $Id$ */

/** @file
 * VBox host opengl support test
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#include <OpenGL/OpenGL.h>
#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/gl.h>
#ifdef VBOX_WITH_COCOA_QT
# include <OpenGL/glu.h>
# include <iprt/log.h>
#endif /* VBOX_WITH_COCOA_QT */
#include <iprt/env.h>
#include <iprt/log.h>

#include <VBox/VBoxOGLTest.h>

bool RTCALL VBoxOglIs3DAccelerationSupported()
{
    if (RTEnvGet("VBOX_CROGL_FORCE_SUPPORTED"))
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

