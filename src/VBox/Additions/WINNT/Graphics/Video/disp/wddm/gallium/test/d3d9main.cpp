/* $Id$ */
/** @file
 * Gallium D3D testcase. Win32 application to run Gallium D3D9 tests.
 */

/*
 * Copyright (C) 2017-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "d3d9render.h"

#include <stdio.h>

#define D3D9TEST_MAX_DEVICES 2

class D3D9Test : public D3D9DeviceProvider
{
public:
    D3D9Test();
    ~D3D9Test();

    HRESULT Init(HINSTANCE hInstance, LPSTR lpCmdLine, int nCmdShow);
    int Run();

    virtual int DeviceCount();
    virtual IDirect3DDevice9 *Device(int index);

private:
    HRESULT initWindow(HINSTANCE hInstance, int nCmdShow);
    HRESULT initDirect3D9(int cDevices);
    void parseCmdLine(LPSTR lpCmdLine);
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    int miRenderId;
    enum
    {
        RenderModeStep = 0,
        RenderModeContinuous = 1,
        RenderModeFPS = 2
    } miRenderMode;
    HWND mHwnd;
    IDirect3D9Ex *mpD3D9;
    int mcDevices;
    IDirect3DDevice9 *mpaDevices[D3D9TEST_MAX_DEVICES];
    D3D9Render *mpRender;

    D3DPRESENT_PARAMETERS mPP;
};

D3D9Test::D3D9Test()
    :
    mHwnd(0),
    mpD3D9(0),
    mcDevices(1),
    mpRender(0),
    miRenderId(3),
    miRenderMode(RenderModeStep)
{
    memset(&mpaDevices, 0, sizeof(mpaDevices));
    memset(&mPP, 0, sizeof(mPP));
}

D3D9Test::~D3D9Test()
{
    if (mpRender)
    {
        delete mpRender;
        mpRender = 0;
    }
    for (int i = 0; i < mcDevices; ++i)
    {
        if (mpaDevices[i])
        {
            mpaDevices[i]->Release();
            mpaDevices[i] = 0;
        }
    }

    if (mpD3D9)
    {
        mpD3D9->Release();
        mpD3D9 = 0;
    }
}

LRESULT CALLBACK D3D9Test::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
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

HRESULT D3D9Test::initWindow(HINSTANCE hInstance,
                             int nCmdShow)
{
    HRESULT hr = S_OK;

    WNDCLASSA wc = { 0 };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = wndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName  = 0;
    wc.lpszClassName = "D3D9TestWndClassName";

    if (RegisterClassA(&wc))
    {
       RECT r = {0, 0, 800, 600};
       AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, false);

       mHwnd = CreateWindowA("D3D9TestWndClassName",
                             "D3D9 Test",
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
           D3DTestShowError(hr, "CreateWindow");
           hr = E_FAIL;
       }
    }
    else
    {
        D3DTestShowError(hr, "RegisterClass");
        hr = E_FAIL;
    }

    return hr;
}

HRESULT D3D9Test::initDirect3D9(int cDevices)
{
    mcDevices = cDevices;

    HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &mpD3D9);
    if (FAILED(hr))
    {
        D3DTestShowError(hr, "Direct3DCreate9Ex");
        return hr;
    }

    /* Verify hardware support for current screen mode. */
    D3DDISPLAYMODE mode;
    hr = mpD3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode);
    if (FAILED(hr))
    {
        D3DTestShowError(hr, "GetAdapterDisplayMode");
        return hr;
    }

    hr = mpD3D9->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, mode.Format, mode.Format, true);
    if (FAILED(hr))
    {
        D3DTestShowError(hr, "CheckDeviceType");
        return hr;
    }

    /* Create identical devices. */
    for (int i = 0; i < mcDevices; ++i)
    {
#if 0
        mPP.BackBufferWidth            = 0;
        mPP.BackBufferHeight           = 0;
        mPP.BackBufferFormat           = D3DFMT_UNKNOWN;
#else
        mPP.BackBufferWidth            = 640;
        mPP.BackBufferHeight           = 480;
        mPP.BackBufferFormat           = D3DFMT_X8R8G8B8;
#endif
        mPP.BackBufferCount            = 1;
        mPP.MultiSampleType            = D3DMULTISAMPLE_NONE;
        mPP.MultiSampleQuality         = 0;
        mPP.SwapEffect                 = D3DSWAPEFFECT_DISCARD;
        mPP.hDeviceWindow              = mHwnd;
        mPP.Windowed                   = true;
        mPP.EnableAutoDepthStencil     = true;
        mPP.AutoDepthStencilFormat     = D3DFMT_D24S8;
        mPP.Flags                      = 0;
        mPP.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
        mPP.PresentationInterval       = D3DPRESENT_INTERVAL_IMMEDIATE;

        hr = mpD3D9->CreateDevice(D3DADAPTER_DEFAULT,
                                  D3DDEVTYPE_HAL,
                                  mHwnd,
                                  D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                  &mPP,
                                  &mpaDevices[i]);
        if (FAILED(hr))
        {
            D3DTestShowError(hr, "CreateDevice");
        }
    }

    return hr;
}

void D3D9Test::parseCmdLine(LPSTR lpCmdLine)
{
    /* Very simple: test number followed by step flag.
     * Default is test 0, step mode: 0 1
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

    while ('0' <= *p && *p <= '9')
        ++p;

    while (*p == ' ')
        ++p;

    if (!*p)
         return;

    /* Second number is the render/step mode. */
    int i = atoi(p);
    switch (i)
    {
        default:
        case 0: miRenderMode = RenderModeStep;       break;
        case 1: miRenderMode = RenderModeContinuous; break;
        case 2: miRenderMode = RenderModeFPS;        break;
    }
}

HRESULT D3D9Test::Init(HINSTANCE hInstance,
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
            const int cDevices = mpRender->RequiredDeviceCount();
            hr = initDirect3D9(cDevices);
            if (SUCCEEDED(hr))
            {
                hr = mpRender->InitRender(this);
                if (FAILED(hr))
                {
                    D3DTestShowError(hr, "InitRender");
                }
            }
        }
        else
        {
            hr = E_FAIL;
        }
    }

    return hr;
}

int D3D9Test::Run()
{
    bool fFirst = true;
    MSG msg;

    LARGE_INTEGER PerfFreq;
    QueryPerformanceFrequency(&PerfFreq);
    float const PerfPeriod = 1.0f / PerfFreq.QuadPart; /* Period in seconds. */

    LARGE_INTEGER PrevTS;
    QueryPerformanceCounter(&PrevTS);

    int cFrames = 0;
    float elapsed = 0;

    do
    {
        BOOL fGotMessage;
        if (miRenderMode == RenderModeStep)
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

        BOOL fDoRender = FALSE;
        if (miRenderMode == RenderModeStep)
        {
            if (msg.message == WM_CHAR)
            {
                if (msg.wParam == ' ')
                {
                    fDoRender = TRUE;
                }
            }
        }
        else
        {
             fDoRender = TRUE;
        }

        if (fDoRender)
        {
            LARGE_INTEGER CurrTS;
            QueryPerformanceCounter(&CurrTS);

            /* Time in seconds since the previous render step. */
            float dt = fFirst ? 0.0f : (CurrTS.QuadPart - PrevTS.QuadPart) * PerfPeriod;
            if (mpRender)
            {
                mpRender->TimeAdvance(dt);
                mpRender->DoRender(this);
                fFirst = false;
            }

            if (miRenderMode == RenderModeFPS)
            {
                ++cFrames;
                elapsed += dt;
                if (elapsed > 1.0f)
                {
                    float msPerFrame = elapsed * 1000.0f / cFrames;
                    char sz[256];
                    _snprintf(sz, sizeof(sz), "D3D9 Test FPS %d Frame Time %fms", cFrames, msPerFrame);
                    SetWindowTextA(mHwnd, sz);

                    cFrames = 0;
                    elapsed = 0.0f;
                }
            }

            PrevTS = CurrTS;
        }
    } while (msg.message != WM_QUIT);

    return msg.wParam;
}

int D3D9Test::DeviceCount()
{
    return mcDevices;
}

IDirect3DDevice9 *D3D9Test::Device(int index)
{
    if (index < mcDevices)
        return mpaDevices[index];
    return NULL;
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow)
{
    (void)hPrevInstance;

    int result = 0;
    D3D9Test test;

    HRESULT hr = test.Init(hInstance, lpCmdLine, nCmdShow);
    if (SUCCEEDED(hr))
    {
        result = test.Run();
    }

    return result;
}
