/** @file
 *
 * VirtualBox Windows NT/2000/XP guest OpenGL ICD
 *
 * ICD entry points.
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 *
 */

/*
 * Mesa 3-D graphics library
 * Version:  6.1
 *
 * Copyright (C) 1999-2004  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * File name: icd.c
 * Author:    Gregor Anich
 *
 * ICD (Installable Client Driver) interface.
 * Based on the windows GDI/WGL driver.
 */

#include "VBoxOGL.h"

#define GL_FUNC(func) gl##func

static ICDTABLE icdTable = { 336, {
#define ICD_ENTRY(func) (PROC)GL_FUNC(func),
#include "VBoxICDList.h"
#undef ICD_ENTRY
} };



PROC APIENTRY DrvGetProcAddress(LPCSTR lpszProc)
{
    PROC pfnProc = GetProcAddress(hDllVBoxOGL, lpszProc);
    if (pfnProc == NULL)
        DbgPrintf(("DrvGetProcAddress %s FAILED\n", lpszProc));
    else
        DbgPrintf(("DrvGetProcAddress %s\n", lpszProc));

    return pfnProc;
}

/* Test export for directly linking with vboxogl.dll */
PROC WINAPI wglGetProcAddress(LPCSTR lpszProc)
{
    return DrvGetProcAddress(lpszProc);
}

BOOL APIENTRY DrvValidateVersion(DWORD version)
{
    DbgPrintf(("DrvValidateVersion %x -> always TRUE\n", version));
    return TRUE;
}


PICDTABLE APIENTRY DrvSetContext(HDC hdc, HGLRC hglrc, void *callback)
{
    NOREF(callback);

    /* Note: we ignore the callback parameter here */
    VBOX_OGL_GEN_SYNC_OP2(DrvSetContext, hdc, hglrc);
    return &icdTable;
}

/* Test export for directly linking with vboxogl.dll */
BOOL WINAPI wglMakeCurrent(HDC hdc, HGLRC hglrc)
{
    DrvSetContext(hdc, hglrc, NULL);
    return TRUE;
}
