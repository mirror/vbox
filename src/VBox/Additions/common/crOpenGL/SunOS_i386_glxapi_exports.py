#
# Copyright (C) 2009 Sun Microsystems, Inc.
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
# Clara, CA 95054 USA or visit http://www.sun.com if you need
# additional information or have any questions.

import sys

#Note, this should match the fakedri_glxfuncsList.h order
glx_functions = [
"CopyContext",
"UseXFont",
#"GetDriverConfig",
"GetProcAddress",
"FreeMemoryMESA",
"QueryExtension",
"IsDirect",
"DestroyGLXPbufferSGIX",
"QueryGLXPbufferSGIX",
"CreateGLXPixmap",
"CreateGLXPixmapWithConfigSGIX",
"QueryContext",
"CreateContextWithConfigSGIX",
"SwapBuffers",
"CreateNewContext",
"SelectEventSGIX",
"QueryContextInfoEXT",
"GetCurrentDrawable",
"ChooseFBConfig",
"WaitGL",
"GetFBConfigs",
"CreatePixmap",
"GetSelectedEventSGIX",
"GetCurrentReadDrawable",
"ImportContextEXT",
"GetContextIDEXT",
"GetCurrentDisplay",
"QueryServerString",
"CreateWindow",
"SelectEvent",
"GetVisualFromFBConfigSGIX",
"MakeCurrentReadSGI",
"GetFBConfigFromVisualSGIX",
"QueryDrawable",
"CreateContext",
"GetConfig",
"CreateGLXPbufferSGIX",
"AllocateMemoryMESA",
"GetMemoryOffsetMESA",
"CreatePbuffer",
"ChooseFBConfigSGIX",
"WaitX",
"GetVisualFromFBConfig",
"CreateGLXPixmapMESA",
#"GetScreenDriver",
"GetFBConfigAttrib",
"GetCurrentContext",
"GetClientString",
"GetCurrentDisplayEXT",
"DestroyPixmap",
"MakeCurrent",
"DestroyContext",
"GetProcAddressARB",
"GetSelectedEvent",
"FreeContextEXT",
"DestroyPbuffer",
"DestroyWindow",
"DestroyGLXPixmap",
"QueryVersion",
"ChooseVisual",
"MakeContextCurrent",
"QueryExtensionsString",
"GetFBConfigAttribSGIX"
];

print '%include "iprt/asmdefs.mac"'
print ""
print "%ifdef RT_ARCH_AMD64"
print "extern glxim"
print "%else ; X86"
print "extern glxim"
print "%endif"
print ""

for index in range(len(glx_functions)):
    func_name = glx_functions[index]

    print "BEGINPROC_EXPORTED vbox_glX%s" % func_name
    print "%ifdef RT_ARCH_AMD64"
    print "\tjmp \t[glxim+%d wrt rip wrt ..gotpcrel]" % (8*index)
    print "%else ; X86"
    print "\tjmp \t[glxim+%d wrt rip wrt ..gotpcrel]" % (4*index)
    print "%endif"
    print "ENDPROC vbox_glX%s" % func_name
    print ""

