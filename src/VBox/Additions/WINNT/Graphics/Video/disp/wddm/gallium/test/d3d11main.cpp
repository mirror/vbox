/* $Id$ */
/** @file
 * D3D testcase. Win32 application to run D3D11 tests.
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

#include "d3d11render.h"

#include <stdio.h>

class D3D11Test : public D3D11DeviceProvider
{
public:
    D3D11Test();
    ~D3D11Test();

    HRESULT Init(HINSTANCE hInstance, LPSTR lpCmdLine, int nCmdShow);
    int Run();

    virtual ID3D11Device *Device();
    virtual ID3D11DeviceContext *ImmediateContext();
    virtual ID3D11RenderTargetView *RenderTargetView();

private:
    HRESULT initWindow(HINSTANCE hInstance, int nCmdShow);
    HRESULT initDirect3D11();
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

    struct
    {
        ID3D11Device           *pDevice;               /* Device for rendering. */
        ID3D11DeviceContext    *pImmediateContext;     /* Associated context. */
        IDXGIFactory           *pDxgiFactory;          /* DXGI Factory associated with the rendering device. */
        ID3D11Texture2D        *pRenderTarget;         /* The render target. */
        ID3D11RenderTargetView *pRenderTargetView;     /* The render target view. */
        IDXGIResource          *pDxgiResource;         /* Interface of the render target. */
        IDXGIKeyedMutex        *pDXGIKeyedMutex;       /* Synchronization interface for the render device. */
    } mRender;

    HANDLE mSharedHandle;

    struct
    {
        ID3D11Device           *pDevice;               /* Device for the output drawing. */
        ID3D11DeviceContext    *pImmediateContext;     /* Corresponding context. */
        IDXGIFactory           *pDxgiFactory;          /* DXGI Factory associated with the output device. */
        IDXGISwapChain         *pSwapChain;            /* Swap chain for displaying in the mHwnd window. */
        ID3D11Texture2D        *pSharedTexture;        /* The texture to draw. Shared resource of mRender.pRenderTarget */
        IDXGIKeyedMutex        *pDXGIKeyedMutex;       /* Synchronization interface for the render device. */
    } mOutput;

    D3D11Render *mpRender;
};

D3D11Test::D3D11Test()
    :
    miRenderId(1),
    miRenderMode(RenderModeStep),
    mHwnd(0),
    mSharedHandle(0),
    mpRender(0)
{
    RT_ZERO(mRender);
    RT_ZERO(mOutput);
}

D3D11Test::~D3D11Test()
{
    if (mpRender)
    {
        delete mpRender;
        mpRender = 0;
    }

    if (mOutput.pImmediateContext)
        mOutput.pImmediateContext->ClearState();
    if (mRender.pImmediateContext)
        mRender.pImmediateContext->ClearState();

    D3D_RELEASE(mOutput.pDevice);
    D3D_RELEASE(mOutput.pImmediateContext);
    D3D_RELEASE(mOutput.pDxgiFactory);
    D3D_RELEASE(mOutput.pSwapChain);
    D3D_RELEASE(mOutput.pSharedTexture);
    D3D_RELEASE(mOutput.pDXGIKeyedMutex);

    D3D_RELEASE(mRender.pDevice);
    D3D_RELEASE(mRender.pImmediateContext);
    D3D_RELEASE(mRender.pDxgiFactory);
    D3D_RELEASE(mRender.pRenderTarget);
    D3D_RELEASE(mRender.pRenderTargetView);
    D3D_RELEASE(mRender.pDxgiResource);
    D3D_RELEASE(mRender.pDXGIKeyedMutex);
}

LRESULT CALLBACK D3D11Test::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
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

HRESULT D3D11Test::initWindow(HINSTANCE hInstance,
                             int nCmdShow)
{
    HRESULT hr = S_OK;

    WNDCLASSA wc;
    RT_ZERO(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = wndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName  = 0;
    wc.lpszClassName = "D3D11TestWndClassName";

    if (RegisterClassA(&wc))
    {
       RECT r = {0, 0, 800, 600};
       AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, false);

       mHwnd = CreateWindowA("D3D11TestWndClassName",
                             "D3D11 Test",
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

static HRESULT d3d11TestCreateDevice(ID3D11Device **ppDevice,
                                     ID3D11DeviceContext **ppImmediateContext,
                                     IDXGIFactory **ppDxgiFactory)
{
    HRESULT hr = S_OK;

    IDXGIAdapter *pAdapter = NULL; /* Default adapter. */
    static const D3D_FEATURE_LEVEL aFeatureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_9_3,
    };
    UINT Flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef DEBUG
    Flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_9_1;

    HTEST(D3D11CreateDevice(pAdapter,
                            D3D_DRIVER_TYPE_HARDWARE,
                            NULL,
                            Flags,
                            aFeatureLevels,
                            RT_ELEMENTS(aFeatureLevels),
                            D3D11_SDK_VERSION,
                            ppDevice,
                            &FeatureLevel,
                            ppImmediateContext));

    if (FeatureLevel != D3D_FEATURE_LEVEL_11_1)
    {
        D3DTestShowError(hr, "FeatureLevel");
    }

    IDXGIDevice *pDxgiDevice = 0;
    hr = (*ppDevice)->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDxgiDevice);
    if (SUCCEEDED(hr))
    {
        IDXGIAdapter *pDxgiAdapter = 0;
        hr = pDxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&pDxgiAdapter);
        if (SUCCEEDED(hr))
        {
            HTEST(pDxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)ppDxgiFactory));
            D3D_RELEASE(pDxgiAdapter);
        }
        else
            D3DTestShowError(hr, "IDXGIAdapter");

        D3D_RELEASE(pDxgiDevice);
    }
    else
        D3DTestShowError(hr, "IDXGIDevice");

    return hr;
}

HRESULT D3D11Test::initDirect3D11()
{
    HRESULT hr = S_OK;

    /*
     * Render.
     */
    d3d11TestCreateDevice(&mRender.pDevice,
                          &mRender.pImmediateContext,
                          &mRender.pDxgiFactory);
    if (mRender.pDevice)
    {
        D3D11_TEXTURE2D_DESC texDesc;
        RT_ZERO(texDesc);
        texDesc.Width     = 800;
        texDesc.Height    = 600;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format    = DXGI_FORMAT_B8G8R8A8_UNORM;
        texDesc.SampleDesc.Count   = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage          = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags      = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags      = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

        HTEST(mRender.pDevice->CreateTexture2D(&texDesc, 0, &mRender.pRenderTarget));
        HTEST(mRender.pDevice->CreateRenderTargetView(mRender.pRenderTarget, 0, &mRender.pRenderTargetView));

        /* Get the shared handle. */
        HTEST(mRender.pRenderTarget->QueryInterface(__uuidof(IDXGIResource), (void**)&mRender.pDxgiResource));
        HTEST(mRender.pDxgiResource->GetSharedHandle(&mSharedHandle));

        HTEST(mRender.pRenderTarget->QueryInterface(__uuidof(IDXGIKeyedMutex), (LPVOID*)&mRender.pDXGIKeyedMutex));
    }

    if (mRender.pImmediateContext)
    {
        // Set the viewport transform.
        D3D11_VIEWPORT mScreenViewport;
        mScreenViewport.TopLeftX = 0;
        mScreenViewport.TopLeftY = 0;
        mScreenViewport.Width    = static_cast<float>(800);
        mScreenViewport.Height   = static_cast<float>(600);
        mScreenViewport.MinDepth = 0.0f;
        mScreenViewport.MaxDepth = 1.0f;

        mRender.pImmediateContext->RSSetViewports(1, &mScreenViewport);
    }

    /*
     * Output.
     */
    d3d11TestCreateDevice(&mOutput.pDevice,
                          &mOutput.pImmediateContext,
                          &mOutput.pDxgiFactory);
    if (mOutput.pDxgiFactory)
    {
        DXGI_SWAP_CHAIN_DESC sd;
        RT_ZERO(sd);
        sd.BufferDesc.Width  = 800;
        sd.BufferDesc.Height = 600;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        sd.SampleDesc.Count   = 1;
        sd.SampleDesc.Quality = 0;
        sd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount  = 1;
        sd.OutputWindow = mHwnd;
        sd.Windowed     = true;
        sd.SwapEffect   = DXGI_SWAP_EFFECT_DISCARD;
        sd.Flags        = 0;

        HTEST(mOutput.pDxgiFactory->CreateSwapChain(mOutput.pDevice, &sd, &mOutput.pSwapChain));

        HTEST(mOutput.pSwapChain->ResizeBuffers(1, 800, 600, DXGI_FORMAT_B8G8R8A8_UNORM, 0));

        HTEST(mOutput.pDevice->OpenSharedResource(mSharedHandle, __uuidof(ID3D11Texture2D), (void**)&mOutput.pSharedTexture));
        HTEST(mOutput.pSharedTexture->QueryInterface(__uuidof(IDXGIKeyedMutex), (LPVOID*)&mOutput.pDXGIKeyedMutex));
    }

    return hr;
}

void D3D11Test::parseCmdLine(LPSTR lpCmdLine)
{
    /* Very simple: a test identifier followed by the render mode. */
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

HRESULT D3D11Test::Init(HINSTANCE hInstance,
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
            hr = initDirect3D11();
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
            D3DTestShowError(0, "No render");
            hr = E_FAIL;
        }
    }

    return hr;
}

int D3D11Test::Run()
{
    HRESULT hr = S_OK;

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
                /*
                 * Render the scene.
                 */
                mpRender->TimeAdvance(dt);

                DWORD result = mRender.pDXGIKeyedMutex->AcquireSync(0, 1000);
                if (result == WAIT_OBJECT_0)
                {
                    /*
                     * Use the shared texture from the render device.
                     */
                    mRender.pImmediateContext->OMSetRenderTargets(1, &mRender.pRenderTargetView, NULL);
                    mpRender->DoRender(this);
                }
                else
                    D3DTestShowError(hr, "Render.AcquireSync(0)");
                result = mRender.pDXGIKeyedMutex->ReleaseSync(1);
                if (result == WAIT_OBJECT_0)
                { }
                else
                    D3DTestShowError(hr, "Render.ReleaseSync(1)");

                /*
                 * Copy the rendered scene to the backbuffer and present.
                 */
                ID3D11Texture2D *pBackBuffer = NULL;
                HTEST(mOutput.pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer)));
                if (pBackBuffer)
                {
                    result = mOutput.pDXGIKeyedMutex->AcquireSync(1, 1000);
                    if (result == WAIT_OBJECT_0)
                    {
                        /*
                         * Use the shared texture from the output device.
                         */
                        mOutput.pImmediateContext->CopyResource(pBackBuffer, mOutput.pSharedTexture);
                    }
                    else
                        D3DTestShowError(hr, "Output.AcquireSync(1)");
                    result = mOutput.pDXGIKeyedMutex->ReleaseSync(0);
                    if (result == WAIT_OBJECT_0)
                    { }
                    else
                        D3DTestShowError(hr, "Output.ReleaseSync(0)");

                    D3D_RELEASE(pBackBuffer);
                }

                HTEST(mOutput.pSwapChain->Present(0, 0));

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
                    _snprintf(sz, sizeof(sz), "D3D11 Test FPS %d Frame Time %fms", cFrames, msPerFrame);
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

ID3D11Device *D3D11Test::Device()
{
    return mRender.pDevice;
}

ID3D11DeviceContext *D3D11Test::ImmediateContext()
{
    return mRender.pImmediateContext;
}

ID3D11RenderTargetView *D3D11Test::RenderTargetView()
{
    return mRender.pRenderTargetView;
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow)
{
    (void)hPrevInstance;

    int result = 0;
    D3D11Test test;

    HRESULT hr = test.Init(hInstance, lpCmdLine, nCmdShow);
    if (SUCCEEDED(hr))
    {
        result = test.Run();
    }

    return result;
}
