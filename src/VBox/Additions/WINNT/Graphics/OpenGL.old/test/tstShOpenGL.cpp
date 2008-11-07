/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

// Enable OpenGL

void EnableOpenGL(HWND hWnd, HDC * hDC, HGLRC * hRC)
{
    PIXELFORMATDESCRIPTOR pfd;
    int format;
    
    // get the device context (DC)
    *hDC = GetDC( hWnd );
    
    // set the pixel format for the DC
    ZeroMemory( &pfd, sizeof( pfd ) );
    pfd.nSize = sizeof( pfd );
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;
    format = ChoosePixelFormat( *hDC, &pfd );
    if (!SetPixelFormat( *hDC, format, &pfd ))
    {
        printf("SetPixelFormat(%d) failed, rc = %u\n", format, (unsigned)GetLastError());
    }
    
    // create and enable the render context (RC)
    *hRC = wglCreateContext( *hDC );
    if (hRC == INVALID_HANDLE_VALUE)
    {
        printf("wglCreateContext failed, rc = %d\n", GetLastError());
    }

    if (!wglMakeCurrent( *hDC, *hRC ))
    {
        printf("wglMakeCurrent failed, rc = %d\n", GetLastError());
    }
}

// Disable OpenGL

void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC)
{
    wglMakeCurrent( NULL, NULL );
    wglDeleteContext( hRC );
    ReleaseDC( hWnd, hDC );
}

HWND CreateInvisWindow(HINSTANCE hInstance)
{
    WNDCLASS wc;
    HWND hWnd;

    // register window class
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = NULL;
    wc.hCursor = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "ShOpenGL";
    RegisterClass( &wc );

    // create main window
    hWnd = CreateWindow( 
        "ShOpenGL", "", 
        WS_POPUPWINDOW,
        0, 0, 0, 0,
        NULL, NULL, hInstance, NULL );

    return hWnd;
}

void RedirectIOToConsole()
{
    int hConHandle;
    HANDLE StdHandle;
    FILE *fp;

    AllocConsole();

    StdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    hConHandle = _open_osfhandle((long)StdHandle, _O_TEXT);
    fp = _fdopen( hConHandle, "w" );
    *stdout = *fp;

    StdHandle = GetStdHandle(STD_INPUT_HANDLE);
    hConHandle = _open_osfhandle((long)StdHandle, _O_TEXT);
    fp = _fdopen( hConHandle, "r" );
    *stdin = *fp;
}

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
                      LPSTR lpszCmdLine,
                      int nCmdShow )
{
    HDC hdc;
    HGLRC hglrc;
    HWND hWnd;

    RedirectIOToConsole();

    /* Make dummy window, NVIDIA drivers fail if OpenGL is used with DC from a window owned by different process */
    hWnd = CreateInvisWindow(hInstance);
    if (hWnd == INVALID_HANDLE_VALUE)
    {
      printf("CreateWindow failed\n");
    }

    EnableOpenGL(hWnd, &hdc, &hglrc);

    printf("GL_VENDOR       %s\n", glGetString(GL_VENDOR));
    printf("GL_RENDERER     %s\n", glGetString(GL_RENDERER));
    printf("GL_VERSION      %s\n", glGetString(GL_VERSION));
    printf("GL_EXTENSIONS   %s\n", glGetString(GL_EXTENSIONS));

    DisableOpenGL(hWnd, hdc, hglrc);

    printf("Press Ender to finish...");
    getchar();

    return 0;
}
