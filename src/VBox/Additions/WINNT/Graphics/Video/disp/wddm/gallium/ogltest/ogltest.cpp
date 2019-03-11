/* $Id$ */
/** @file
 * OpenGL testcase. Win32 application to run Gallium OpenGL tests.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "oglrender.h"

class OGLTest
{
public:
    OGLTest();
    ~OGLTest();

    HRESULT Init(HINSTANCE hInstance, LPSTR lpCmdLine, int nCmdShow);
    int Run();

private:
    HRESULT initWindow(HINSTANCE hInstance, int nCmdShow);
    HRESULT initOGL();
    void parseCmdLine(LPSTR lpCmdLine);
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void setCurrentGLCtx(HGLRC hGLRC);

    int miRenderId;
    int miRenderStep;

    HWND mHwnd;
    HGLRC mhGLRC;

    OGLRender *mpRender;
};

OGLTest::OGLTest()
    :
    miRenderId(0),
    miRenderStep(1),
    mHwnd(0),
    mhGLRC(0),
    mpRender(0)
{
}

OGLTest::~OGLTest()
{
    if (mpRender)
    {
        delete mpRender;
        mpRender = 0;
    }

    setCurrentGLCtx(NULL);
    wglDeleteContext(mhGLRC);
}

void OGLTest::setCurrentGLCtx(HGLRC hGLRC)
{
    if (hGLRC)
    {
        HDC const hDC = GetDC(mHwnd);
        wglMakeCurrent(hDC, mhGLRC);
        ReleaseDC(mHwnd, hDC);
    }
    else
    {
        wglMakeCurrent(NULL, NULL);
    }
}


LRESULT CALLBACK OGLTest::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

HRESULT OGLTest::initWindow(HINSTANCE hInstance,
                            int nCmdShow)
{
    HRESULT hr = S_OK;

    WNDCLASSA wc = { 0 };
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = wndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName  = 0;
    wc.lpszClassName = "OGLTestWndClassName";

    if (RegisterClassA(&wc))
    {
       RECT r = {0, 0, 800, 600};
       AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, false);

       mHwnd = CreateWindowA("OGLTestWndClassName",
                             "OGL Test",
                             WS_OVERLAPPEDWINDOW,
                             100, 100, r.right, r.bottom,
                             0, 0, hInstance, 0);
       if (mHwnd)
       {
           ShowWindow(mHwnd, nCmdShow);
           UpdateWindow(mHwnd);
       }
       else
       {
           TestShowError(hr, "CreateWindow");
           hr = E_FAIL;
       }
    }
    else
    {
        TestShowError(hr, "RegisterClass");
        hr = E_FAIL;
    }

    return hr;
}

HRESULT OGLTest::initOGL()
{
    HRESULT hr = S_OK;

    HDC hDC = GetDC(mHwnd);

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    int pf = ChoosePixelFormat(hDC, &pfd);
    if (pf)
    {
        if (SetPixelFormat(hDC, pf, &pfd))
        {
            DescribePixelFormat(hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

            mhGLRC = wglCreateContext(hDC);
            setCurrentGLCtx(mhGLRC);
        }
        else
        {
            TestShowError(hr, "SetPixelFormat");
            hr = E_FAIL;
        }
    }
    else
    {
        TestShowError(hr, "ChoosePixelFormat");
        hr = E_FAIL;
    }

    ReleaseDC(mHwnd, hDC);

    return hr;
}

void OGLTest::parseCmdLine(LPSTR lpCmdLine)
{
    /* Very simple: test number followed by step flag.
     * Default is test 0, step mode: 1
     */
    if (!lpCmdLine)
        return;

    char *p = lpCmdLine;

    while (*p == ' ')
        ++p;

    if (!*p)
         return;

    /* First number is the render id. */
    miRenderId = atoi(p);

    while (*p == ' ' || ('0' <= *p && *p <= '9'))
        ++p;

    if (!*p)
         return;

    /* Second number is the step mode. */
    miRenderStep = atoi(p);
}

HRESULT OGLTest::Init(HINSTANCE hInstance,
                      LPSTR lpCmdLine,
                      int nCmdShow)
{
    parseCmdLine(lpCmdLine);

    HRESULT hr = initWindow(hInstance, nCmdShow);
    if (SUCCEEDED(hr))
    {
        mpRender = CreateRender(miRenderId);
        if (mpRender)
        {
            hr = initOGL();
            if (SUCCEEDED(hr))
            {
                setCurrentGLCtx(mhGLRC);

                hr = mpRender->InitRender();
                if (FAILED(hr))
                {
                    TestShowError(hr, "InitRender");
                }

                setCurrentGLCtx(NULL);
            }
        }
        else
        {
            hr = E_FAIL;
        }
    }

    return hr;
}

int OGLTest::Run()
{
    bool fFirst = true;
    MSG msg;
    do
    {
        BOOL fGotMessage;
        if (miRenderStep)
        {
            fGotMessage = GetMessageA(&msg, 0, 0, 0);
        }
        else
        {
            fGotMessage = PeekMessageA(&msg, 0, 0, 0, PM_REMOVE);
        }

        if (fGotMessage)
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        float dt = 0.0f; /* Time in seconds since last render step. @todo Measure. */

        BOOL fDoRender = FALSE;
        if (miRenderStep)
        {
            if (msg.message == WM_CHAR)
            {
                if (msg.wParam == ' ')
                {
                    fDoRender = TRUE;
                    dt = fFirst ? 0.0f : 0.1f; /* 0.1 second increment per step. */
                }
            }
        }
        else
        {
             fDoRender = TRUE;
        }

        if (fDoRender)
        {
            if (mpRender)
            {
                setCurrentGLCtx(mhGLRC);

                mpRender->TimeAdvance(dt);
                mpRender->DoRender();

                setCurrentGLCtx(NULL);

                fFirst = false;
            }
        }
    } while (msg.message != WM_QUIT);

    return msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow)
{
    (void)hPrevInstance;

    int result = 0;
    OGLTest test;

    HRESULT hr = test.Init(hInstance, lpCmdLine, nCmdShow);
    if (SUCCEEDED(hr))
    {
        result = test.Run();
    }

    return result;
}
