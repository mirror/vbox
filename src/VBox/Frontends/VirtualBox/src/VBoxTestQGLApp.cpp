/* $Id: $ */
/** @file
 * VBox host opengl support test application.
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

#ifdef RT_OS_WINDOWS
#include <Windows.h>
#endif

#include <iprt/initterm.h>
//#include <iprt/assert.h>

#include "VBoxGLSupportInfo.h"

#include <QApplication>

#ifndef RT_OS_WINDOWS
int main(int argc, char **argv)
{
#else
extern "C" int WINAPI WinMain(HINSTANCE hInstance,
    HINSTANCE /*hPrevInstance*/, LPSTR lpCmdLine, int /*nShowCmd*/)
{
    int argc = __argc;
    char **argv =  __argv;
#endif

    RTR3Init();

    int rc=1;

    QApplication app (argc, argv);

    VBoxGLTmpContext ctx;
    const QGLContext *pContext = ctx.makeCurrent();
    if(pContext)
    {
        VBoxVHWAInfo supportInfo;
        supportInfo.init(pContext);
        if(supportInfo.isVHWASupported())
            rc = 0;
    }

    return rc;
}

