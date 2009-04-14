/* $Id$ */

/** @file
 * VBox OpenGL list of opengl functions common in Mesa and vbox opengl stub
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#ifndef GLXAPI_ENTRY
#error GLXAPI_ENTRY should be defined.
#endif

/*This should match glX* entries which are exported by Mesa's libGL.so,
 * use something like the following to get the list:
 * objdump -T libGL.so|grep glX|grep -v __|awk '{print $7;};'|sed 's/glX//'|awk '{OFS=""; print "GLXAPI_ENTRY(",$1,")"}'
 */

/* #######Note: if you change the list, don't forget to change Linux_i386_glxapi_exports.py####### */

GLXAPI_ENTRY(CopyContext)
GLXAPI_ENTRY(UseXFont)
/*GLXAPI_ENTRY(GetDriverConfig)*/
GLXAPI_ENTRY(GetProcAddress)
GLXAPI_ENTRY(FreeMemoryMESA)
GLXAPI_ENTRY(QueryExtension)
GLXAPI_ENTRY(IsDirect)
GLXAPI_ENTRY(DestroyGLXPbufferSGIX)
GLXAPI_ENTRY(QueryGLXPbufferSGIX)
GLXAPI_ENTRY(CreateGLXPixmap)
GLXAPI_ENTRY(CreateGLXPixmapWithConfigSGIX)
GLXAPI_ENTRY(QueryContext)
GLXAPI_ENTRY(CreateContextWithConfigSGIX)
GLXAPI_ENTRY(SwapBuffers)
GLXAPI_ENTRY(CreateNewContext)
GLXAPI_ENTRY(SelectEventSGIX)
GLXAPI_ENTRY(QueryContextInfoEXT)
GLXAPI_ENTRY(GetCurrentDrawable)
GLXAPI_ENTRY(ChooseFBConfig)
GLXAPI_ENTRY(WaitGL)
GLXAPI_ENTRY(GetFBConfigs)
GLXAPI_ENTRY(CreatePixmap)
GLXAPI_ENTRY(GetSelectedEventSGIX)
GLXAPI_ENTRY(GetCurrentReadDrawable)
GLXAPI_ENTRY(ImportContextEXT)
GLXAPI_ENTRY(GetContextIDEXT)
GLXAPI_ENTRY(GetCurrentDisplay)
GLXAPI_ENTRY(QueryServerString)
GLXAPI_ENTRY(CreateWindow)
GLXAPI_ENTRY(SelectEvent)
GLXAPI_ENTRY(GetVisualFromFBConfigSGIX)
GLXAPI_ENTRY(MakeCurrentReadSGI)
GLXAPI_ENTRY(GetFBConfigFromVisualSGIX)
GLXAPI_ENTRY(QueryDrawable)
GLXAPI_ENTRY(CreateContext)
GLXAPI_ENTRY(GetConfig)
GLXAPI_ENTRY(CreateGLXPbufferSGIX)
GLXAPI_ENTRY(AllocateMemoryMESA)
GLXAPI_ENTRY(GetMemoryOffsetMESA)
GLXAPI_ENTRY(CreatePbuffer)
GLXAPI_ENTRY(ChooseFBConfigSGIX)
GLXAPI_ENTRY(WaitX)
GLXAPI_ENTRY(GetVisualFromFBConfig)
GLXAPI_ENTRY(CreateGLXPixmapMESA)
/*GLXAPI_ENTRY(GetScreenDriver)*/
GLXAPI_ENTRY(GetFBConfigAttrib)
GLXAPI_ENTRY(GetCurrentContext)
GLXAPI_ENTRY(GetClientString)
GLXAPI_ENTRY(GetCurrentDisplayEXT)
GLXAPI_ENTRY(DestroyPixmap)
GLXAPI_ENTRY(MakeCurrent)
GLXAPI_ENTRY(DestroyContext)
GLXAPI_ENTRY(GetProcAddressARB)
GLXAPI_ENTRY(GetSelectedEvent)
GLXAPI_ENTRY(FreeContextEXT)
GLXAPI_ENTRY(DestroyPbuffer)
GLXAPI_ENTRY(DestroyWindow)
GLXAPI_ENTRY(DestroyGLXPixmap)
GLXAPI_ENTRY(QueryVersion)
GLXAPI_ENTRY(ChooseVisual)
GLXAPI_ENTRY(MakeContextCurrent)
GLXAPI_ENTRY(QueryExtensionsString)
GLXAPI_ENTRY(GetFBConfigAttribSGIX)

