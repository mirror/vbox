/* $Id$ */
/** @file
 * DevVMWare - VMWare SVGA device
 */

/*
 * Copyright (C) 2013-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_VMSVGA
#include <VBox/vmm/pdmdev.h>
#include <VBox/version.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/vmm/pgm.h>

#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/uuid.h>
#include <iprt/mem.h>
#include <iprt/avl.h>

#include <VBox/VMMDev.h>
#include <VBox/VBoxVideo.h>
#include <VBox/bioslogo.h>

/* should go BEFORE any other DevVGA include to make all DevVGA.h config defines be visible */
#include "DevVGA.h"

#include "DevVGA-SVGA.h"
#include "DevVGA-SVGA3d.h"
#include "vmsvga/svga_reg.h"
#include "vmsvga/svga3d_reg.h"
#include "vmsvga/svga3d_shaderdefs.h"


#ifdef RT_OS_WINDOWS
/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
#define     VMSVGA3D_WNDCLASSNAME       "VMSVGA3DWNDCLS"

static LONG WINAPI vmsvga3dWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif

#ifdef RT_OS_WINDOWS

/**
 * Send a message to the async window thread and wait for a reply
 *
 * @returns VBox status code.
 * @param   pWindowThread   Thread handle
 * @param   WndRequestSem   Semaphore handle for waiting
 * @param   msg             Message id
 * @param   wParam          First parameter
 * @param   lParam          Second parameter
 */
int vmsvga3dSendThreadMessage(RTTHREAD pWindowThread, RTSEMEVENT WndRequestSem, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int  rc;
    BOOL ret;

    ret = PostThreadMessage(RTThreadGetNative(pWindowThread), msg, wParam, lParam);
    AssertMsgReturn(ret, ("PostThreadMessage %x failed with %d\n", RTThreadGetNative(pWindowThread), GetLastError()), VERR_INTERNAL_ERROR);

    rc = RTSemEventWait(WndRequestSem, RT_INDEFINITE_WAIT);
    Assert(RT_SUCCESS(rc));

    return rc;
}

/**
 * The async window handling thread
 *
 * @returns VBox status code.
 * @param   pDevIns     The VGA device instance.
 * @param   pThread     The send thread.
 */
DECLCALLBACK(int) vmsvga3dWindowThread(RTTHREAD ThreadSelf, void *pvUser)
{
    RTSEMEVENT      WndRequestSem = (RTSEMEVENT)pvUser;
    WNDCLASSEX      wc;

    /* Register our own window class. */
    wc.cbSize           = sizeof(wc);
    wc.style            = CS_OWNDC;
    wc.lpfnWndProc      = (WNDPROC) vmsvga3dWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = GetModuleHandle("VBoxDD.dll");    /* @todo hardcoded name.. */
    wc.hIcon            = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = NULL;
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = VMSVGA3D_WNDCLASSNAME;
    wc.hIconSm          = NULL;

    if (!RegisterClassEx(&wc))
    {
        Log(("RegisterClass failed with %x\n", GetLastError()));
        return VERR_INTERNAL_ERROR;
    }

    LogFlow(("vmsvga3dWindowThread: started loop\n"));
    while (true)
    {
        MSG msg;

        if (GetMessage(&msg, 0, 0, 0))
        {
            if (msg.message == WM_VMSVGA3D_EXIT)
            {
                /* Signal to the caller that we're done. */
                RTSemEventSignal(WndRequestSem);
                break;
            }

            if (msg.message == WM_VMSVGA3D_WAKEUP)
            {
                continue;
            }
            if (msg.message == WM_VMSVGA3D_CREATEWINDOW)
            {
                HWND          *pHwnd = (HWND *)msg.wParam;
                LPCREATESTRUCT pCS = (LPCREATESTRUCT) msg.lParam;

#ifdef DEBUG_GFX_WINDOW
                RECT rectClient;

                rectClient.left     = 0;
                rectClient.top      = 0;
                rectClient.right    = pCS->cx;
                rectClient.bottom   = pCS->cy;
                AdjustWindowRectEx(&rectClient, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE | WS_CAPTION, FALSE, WS_EX_NOACTIVATE | WS_EX_NOPARENTNOTIFY | WS_EX_TRANSPARENT);
                pCS->cx = rectClient.right - rectClient.left;
                pCS->cy = rectClient.bottom - rectClient.top;
#endif
                *pHwnd = CreateWindowEx(pCS->dwExStyle,
                                        VMSVGA3D_WNDCLASSNAME,
                                        pCS->lpszName,
                                        pCS->style,
#ifdef DEBUG_GFX_WINDOW
                                        0,
                                        0,
#else
                                        pCS->x,
                                        pCS->y,
#endif
                                        pCS->cx,
                                        pCS->cy,
#ifdef DEBUG_GFX_WINDOW
                                        0,
#else
                                        pCS->hwndParent,
#endif
                                        pCS->hMenu,
                                        pCS->hInstance,
                                        NULL);
                AssertMsg(*pHwnd, ("CreateWindowEx %x %s %s %x (%d,%d)(%d,%d), %x %x %x error=%x\n", pCS->dwExStyle, pCS->lpszName, VMSVGA3D_WNDCLASSNAME, pCS->style, pCS->x,
                                    pCS->y, pCS->cx, pCS->cy,pCS->hwndParent, pCS->hMenu, pCS->hInstance, GetLastError()));

                /* Signal to the caller that we're done. */
                RTSemEventSignal(WndRequestSem);
                continue;
            }
            else
            if (msg.message == WM_VMSVGA3D_DESTROYWINDOW)
            {
                BOOL ret = DestroyWindow((HWND)msg.wParam);
                Assert(ret);
                /* Signal to the caller that we're done. */
                RTSemEventSignal(WndRequestSem);
                continue;
            }
            else
            if (msg.message == WM_VMSVGA3D_RESIZEWINDOW)
            {
                HWND hwnd = (HWND)msg.wParam;
                LPCREATESTRUCT pCS = (LPCREATESTRUCT) msg.lParam;

#ifdef DEBUG_GFX_WINDOW
                RECT rectClient;

                rectClient.left     = 0;
                rectClient.top      = 0;
                rectClient.right    = pCS->cx;
                rectClient.bottom   = pCS->cy;
                AdjustWindowRectEx(&rectClient, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE | WS_CAPTION, FALSE, WS_EX_NOACTIVATE | WS_EX_NOPARENTNOTIFY | WS_EX_TRANSPARENT);
                pCS->cx = rectClient.right - rectClient.left;
                pCS->cy = rectClient.bottom - rectClient.top;
#endif
                BOOL ret = SetWindowPos(hwnd, 0, pCS->x, pCS->y, pCS->cx, pCS->cy, SWP_NOZORDER | SWP_NOMOVE);
                Assert(ret);

                /* Signal to the caller that we're done. */
                RTSemEventSignal(WndRequestSem);
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Log(("GetMessage failed with %x\n", GetLastError()));
            break;
        }
    }

    Log(("vmsvga3dWindowThread: end loop\n"));
    return VINF_SUCCESS;
}

/* Window procedure for our top level window overlays. */
static LONG WINAPI vmsvga3dWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CLOSE:
            break;

        case WM_DESTROY:
            break;

        case WM_NCHITTEST:
            return HTNOWHERE;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

#endif /* RT_OS_WINDOWS */


void vmsvga3dInfoU32Flags(PCDBGFINFOHLP pHlp, uint32_t fFlags, const char *pszPrefix, PCVMSVGAINFOFLAGS32 paFlags, uint32_t cFlags)
{
    for (uint32_t i = 0; i < cFlags; i++)
        if ((paFlags[i].fFlags & fFlags) == paFlags[i].fFlags)
        {
            Assert(paFlags[i].fFlags);
            pHlp->pfnPrintf(pHlp, " %s%s", pszPrefix, paFlags[i].pszJohnny);
            fFlags &= ~paFlags[i].fFlags;
            if (!fFlags)
                return;
        }
    if (fFlags)
        pHlp->pfnPrintf(pHlp, " UNKNOWN_%#x", fFlags);
}


/**
 * Worker for vmsvgaR3Info that display details of a host window.
 *
 * @param   pHlp            The output methods.
 * @param   idHostWindow    The host window handle/id/whatever.
 */
void vmsvga3dInfoHostWindow(PCDBGFINFOHLP pHlp, uint64_t idHostWindow)
{
#ifdef RT_OS_WINDOWS
    HWND hwnd = (HWND)(uintptr_t)idHostWindow;
    Assert((uintptr_t)hwnd == idHostWindow);
    if (hwnd != NULL)
    {
        WINDOWINFO Info;
        RT_ZERO(Info);
        Info.cbSize = sizeof(Info);
        if (GetWindowInfo(hwnd, &Info))
        {
            pHlp->pfnPrintf(pHlp, "     Window rect:   xLeft=%d, yTop=%d, xRight=%d, yBottom=%d (cx=%d, cy=%d)\n",
                            Info.rcWindow.left, Info.rcWindow.top, Info.rcWindow.right, Info.rcWindow.bottom,
                            Info.rcWindow.right - Info.rcWindow.left, Info.rcWindow.bottom - Info.rcWindow.top);
            pHlp->pfnPrintf(pHlp, "     Client rect:   xLeft=%d, yTop=%d, xRight=%d, yBottom=%d (cx=%d, cy=%d)\n",
                            Info.rcClient.left, Info.rcClient.top, Info.rcClient.right, Info.rcClient.bottom,
                            Info.rcClient.right - Info.rcClient.left, Info.rcClient.bottom - Info.rcClient.top);
            pHlp->pfnPrintf(pHlp, "     Style:         %#x", Info.dwStyle);
            static const VMSVGAINFOFLAGS32 g_aStyles[] =
            {
                { WS_POPUP        , "POPUP" },
                { WS_CHILD        , "CHILD" },
                { WS_MINIMIZE     , "MINIMIZE" },
                { WS_VISIBLE      , "VISIBLE" },
                { WS_DISABLED     , "DISABLED" },
                { WS_CLIPSIBLINGS , "CLIPSIBLINGS" },
                { WS_CLIPCHILDREN , "CLIPCHILDREN" },
                { WS_MAXIMIZE     , "MAXIMIZE" },
                { WS_BORDER       , "BORDER" },
                { WS_DLGFRAME     , "DLGFRAME" },
                { WS_VSCROLL      , "VSCROLL" },
                { WS_HSCROLL      , "HSCROLL" },
                { WS_SYSMENU      , "SYSMENU" },
                { WS_THICKFRAME   , "THICKFRAME" },
                { WS_GROUP        , "GROUP" },
                { WS_TABSTOP      , "TABSTOP" },
            };
            vmsvga3dInfoU32Flags(pHlp, Info.dwStyle, "", g_aStyles, RT_ELEMENTS(g_aStyles));
            pHlp->pfnPrintf(pHlp, "\n");

            pHlp->pfnPrintf(pHlp, "     ExStyle:       %#x", Info.dwExStyle);
            static const VMSVGAINFOFLAGS32 g_aExStyles[] =
            {
                { WS_EX_DLGMODALFRAME,   "DLGMODALFRAME" },
                { 0x00000002,            "DRAGDETECT" },
                { WS_EX_NOPARENTNOTIFY,  "NOPARENTNOTIFY" },
                { WS_EX_TOPMOST,         "TOPMOST" },
                { WS_EX_ACCEPTFILES,     "ACCEPTFILES" },
                { WS_EX_TRANSPARENT,     "TRANSPARENT" },
                { WS_EX_MDICHILD,        "MDICHILD" },
                { WS_EX_TOOLWINDOW,      "TOOLWINDOW" },
                { WS_EX_WINDOWEDGE,      "WINDOWEDGE" },
                { WS_EX_CLIENTEDGE,      "CLIENTEDGE" },
                { WS_EX_CONTEXTHELP,     "CONTEXTHELP" },
                { WS_EX_RIGHT,           "RIGHT" },
                /*{ WS_EX_LEFT,            "LEFT" }, = 0 */
                { WS_EX_RTLREADING,      "RTLREADING" },
                /*{ WS_EX_LTRREADING,      "LTRREADING" }, = 0 */
                { WS_EX_LEFTSCROLLBAR,   "LEFTSCROLLBAR" },
                /*{ WS_EX_RIGHTSCROLLBAR,  "RIGHTSCROLLBAR" }, = 0 */
                { WS_EX_CONTROLPARENT,   "CONTROLPARENT" },
                { WS_EX_STATICEDGE,      "STATICEDGE" },
                { WS_EX_APPWINDOW,       "APPWINDOW" },
                { WS_EX_LAYERED,         "LAYERED" },
                { WS_EX_NOINHERITLAYOUT, "NOINHERITLAYOUT" },
                { WS_EX_LAYOUTRTL,       "LAYOUTRTL" },
                { WS_EX_COMPOSITED,      "COMPOSITED" },
                { WS_EX_NOACTIVATE,      "NOACTIVATE" },
            };
            vmsvga3dInfoU32Flags(pHlp, Info.dwExStyle, "", g_aExStyles, RT_ELEMENTS(g_aExStyles));
            pHlp->pfnPrintf(pHlp, "\n");

            pHlp->pfnPrintf(pHlp, "     Window Status: %#x\n", Info.dwWindowStatus);
            if (Info.cxWindowBorders || Info.cyWindowBorders)
                pHlp->pfnPrintf(pHlp, "     Borders:       cx=%u, cy=%u\n", Info.cxWindowBorders, Info.cyWindowBorders);
            pHlp->pfnPrintf(pHlp, "     Window Type:   %#x\n", Info.atomWindowType);
            pHlp->pfnPrintf(pHlp, "     Creator Ver:   %#x\n", Info.wCreatorVersion);
        }
        else
            pHlp->pfnPrintf(pHlp, "     GetWindowInfo: last error %d\n", GetLastError());
    }
#else
    pHlp->pfnPrintf(pHlp, "    Windows info:   Not implemented on this platform\n");
#endif

}


/**
 * Formats an enum value as a string, sparse mapping table.
 *
 * @returns pszBuffer.
 * @param   pszBuffer           The output buffer.
 * @param   cbBuffer            The size of the output buffer.
 * @param   pszName             The variable name, optional.
 * @param   iValue              The enum value.
 * @param   pszPrefix           The prefix of the enum values.  Empty string if
 *                              none.  This helps reduce the memory footprint
 *                              as well as the source code size.
 * @param   papszValues         One to one string mapping of the enum values.
 * @param   cValues             The number of values in the mapping.
 */
char *vmsvgaFormatEnumValueEx(char *pszBuffer, size_t cbBuffer, const char *pszName, int32_t iValue,
                              const char *pszPrefix, PCVMSVGAINFOENUM paValues, size_t cValues)
{
    for (uint32_t i = 0; i < cValues; i++)
        if (paValues[i].iValue == iValue)
        {
            if (pszName)
                RTStrPrintf(pszBuffer, cbBuffer, "%s = %s%s (%#x)", pszName, pszPrefix, paValues[i].pszName, iValue);
            else
                RTStrPrintf(pszBuffer, cbBuffer, "%s%s (%#x)", pszPrefix, paValues[i].pszName, iValue);
            return pszBuffer;
        }

    if (pszName)
        RTStrPrintf(pszBuffer, cbBuffer, "%s = %sUNKNOWN_%d (%#x)", pszName, pszPrefix, iValue, iValue);
    else
        RTStrPrintf(pszBuffer, cbBuffer, "%sUNKNOWN_%d (%#x)", pszPrefix, iValue, iValue);
    return pszBuffer;
}


/**
 * Formats an enum value as a string.
 *
 * @returns pszBuffer.
 * @param   pszBuffer           The output buffer.
 * @param   cbBuffer            The size of the output buffer.
 * @param   pszName             The variable name, optional.
 * @param   uValue              The enum value.
 * @param   pszPrefix           The prefix of the enum values.  Empty string if
 *                              none.  This helps reduce the memory footprint
 *                              as well as the source code size.
 * @param   papszValues         One to one string mapping of the enum values.
 * @param   cValues             The number of values in the mapping.
 */
char *vmsvgaFormatEnumValue(char *pszBuffer, size_t cbBuffer, const char *pszName, uint32_t uValue,
                            const char *pszPrefix, const char * const *papszValues, size_t cValues)
{
    if (uValue < cValues)
    {
        if (pszName)
            RTStrPrintf(pszBuffer, cbBuffer, "%s = %s%s (%#x)", pszName, pszPrefix, papszValues[uValue], uValue);
        else
            RTStrPrintf(pszBuffer, cbBuffer, "%s%s (%#x)", pszPrefix, papszValues[uValue], uValue);
    }
    else
    {
        if (pszName)
            RTStrPrintf(pszBuffer, cbBuffer, "%s = %sUNKNOWN_%d (%#x)", pszName, pszPrefix, uValue, uValue);
        else
            RTStrPrintf(pszBuffer, cbBuffer, "%sUNKNOWN_%d (%#x)", pszPrefix, uValue, uValue);
    }
    return pszBuffer;
}


/**
 * DBGF info printer for vmsvga3dAsciiPrint.
 *
 * @param   pszLine             The line to print.
 * @param   pvUser              The debug info helpers.
 */
DECLCALLBACK(void) vmsvga3dAsciiPrintlnInfo(const char *pszLine, void *pvUser)
{
    PCDBGFINFOHLP pHlp = (PCDBGFINFOHLP)pvUser;
    pHlp->pfnPrintf(pHlp, ">%s<\n", pszLine);
}


/**
 * Log printer for vmsvga3dAsciiPrint.
 *
 * @param   pszLine             The line to print.
 * @param   pvUser              Ignored.
 */
DECLCALLBACK(void) vmsvga3dAsciiPrintlnLog(const char *pszLine, void *pvUser)
{
    size_t cch = strlen(pszLine);
    while (cch > 0 && pszLine[cch - 1] == ' ')
        cch--;
    RTLogPrintf("%.*s\n", cch, pszLine);
    NOREF(pvUser);
}


void vmsvga3dAsciiPrint(PFMVMSVGAASCIIPRINTLN pfnPrintLine, void *pvUser, void const *pvImage, size_t cbImage,
                        uint32_t cx, uint32_t cy, uint32_t cbScanline, SVGA3dSurfaceFormat enmFormat, bool fInvY,
                        uint32_t cchMaxX, uint32_t cchMaxY)
{
    /*
     * Skip stuff we can't or won't need to handle.
     */
    if (!cx || !cy)
        return;
    switch (enmFormat)
    {
        /* Compressed. */
        case SVGA3D_DXT1:
        case SVGA3D_DXT2:
        case SVGA3D_DXT3:
        case SVGA3D_DXT4:
        case SVGA3D_DXT5:
            return;
        /* Generic. */
        case SVGA3D_BUFFER:
            return;
    }

    /*
     * Figure the pixel conversion factors.
     */
    uint32_t cxPerChar = cx / cchMaxX + 1;
    uint32_t cyPerChar = cy / cchMaxY + 1;
    /** @todo try keep aspect...   */
    uint32_t const cchLine = (cx + cxPerChar - 1) / cxPerChar;
    uint32_t const cbSrcPixel = vmsvga3dSurfaceFormatSize(enmFormat);

    /*
     * The very simple conversion we're doing in this function is based on
     * mapping a block of converted pixels to an ASCII character of similar
     * weigth.  We do that by summing up all the 8-bit gray scale pixels in
     * that block, applying a conversion factor and getting an index into an
     * array of increasingly weighty characters.
     */
    static const char       s_szPalette[] = "   ..`',:;icodxkO08XNWM";
    static const uint32_t   s_cchPalette  = sizeof(s_szPalette) - 1;
    uint32_t const          cPixelsWeightPerChar = cxPerChar * cyPerChar * 256;

    /*
     * Do the work
     */
    uint32_t *pauScanline = (uint32_t *)RTMemTmpAllocZ(sizeof(pauScanline[0]) * cchLine + cchLine + 1);
    if (!pauScanline)
        return;
    char *pszLine = (char *)&pauScanline[cchLine];
    RTCPTRUNION uSrc;
    uSrc.pv              = pvImage;
    if (fInvY)
        uSrc.pu8 += (cy - 1) * cbScanline;
    uint32_t cyLeft = cy;
    uint32_t cyLeftInScanline = cyPerChar;
    bool     fHitFormatAssert = false;
    for (;;)
    {
        /*
         * Process the scanline.  This is tedious because of all the
         * different formats.  We generally ignore alpha, unless it's
         * all we've got to work with.
         * Color to 8-bit grayscale conversion is done by averaging.
         */
#define CONVERT_SCANLINE(a_AddExpr) \
            do { \
                for (uint32_t xSrc = 0, xDst = 0, cxLeftInChar = cxPerChar; xSrc < cx; xSrc++) \
                { \
                    pauScanline[xDst] += (a_AddExpr) & 0xff; \
                    Assert(pauScanline[xDst] <= cPixelsWeightPerChar); \
                    if (--cxLeftInChar == 0) \
                    { \
                        xDst++; \
                        cxLeftInChar = cxPerChar; \
                    } \
                } \
            } while (0)

        uint32_t u32Tmp;
        switch (enmFormat)
        {
            /* Unsigned RGB and super/subsets. */
            case SVGA3D_X8R8G8B8:
            case SVGA3D_A8R8G8B8:
                CONVERT_SCANLINE( (  ((u32Tmp = uSrc.pu32[xSrc]) & 0xff) /* B */
                                   + ((u32Tmp >>  8) & 0xff)             /* G */
                                   + ((u32Tmp >> 16) & 0xff)             /* R */) / 3);
                break;
            case SVGA3D_R5G6B5:
                CONVERT_SCANLINE( (  ( uSrc.pu16[xSrc]         & 0x1f) * 8
                                   + ((uSrc.pu16[xSrc] >>  5)  & 0x3f) * 4
                                   + ( uSrc.pu16[xSrc] >> 11)          * 8 ) / 3 );
                break;
            case SVGA3D_X1R5G5B5:
            case SVGA3D_A1R5G5B5:
                CONVERT_SCANLINE( (  ( uSrc.pu16[xSrc]        & 0x1f) * 8
                                   + ((uSrc.pu16[xSrc] >> 5)  & 0x1f) * 8
                                   + ((uSrc.pu16[xSrc] >> 10) & 0x1f) * 8) / 3 );
                break;
            case SVGA3D_A4R4G4B4:
                CONVERT_SCANLINE( (  ( uSrc.pu16[xSrc]        & 0xf) * 16
                                   + ((uSrc.pu16[xSrc] >> 4)  & 0xf) * 16
                                   + ((uSrc.pu16[xSrc] >> 8)  & 0xf) * 16) / 3 );
                break;
            case SVGA3D_A16B16G16R16:
                CONVERT_SCANLINE( (  ((uSrc.pu64[xSrc] >>  8) & 0xff) /* R */
                                   + ((uSrc.pu64[xSrc] >> 24) & 0xff) /* G */
                                   + ((uSrc.pu64[xSrc] >> 40) & 0xff) /* B */ ) / 3);
                break;
            case SVGA3D_A2R10G10B10:
                CONVERT_SCANLINE( (  ((uSrc.pu32[xSrc]      ) & 0x3ff) /* B */
                                   + ((uSrc.pu32[xSrc] >> 10) & 0x3ff) /* G */
                                   + ((uSrc.pu32[xSrc] >> 20) & 0x3ff) /* R */ ) / (3 * 4));
                break;
            case SVGA3D_G16R16:
                CONVERT_SCANLINE( (  (uSrc.pu32[xSrc] & 0xffff) /* R */
                                   + (uSrc.pu32[xSrc] >> 16 )   /* G */) / 0x200);
                break;

            /* Depth. */
            case SVGA3D_Z_D32:
                CONVERT_SCANLINE(  (((u32Tmp = ~(( uSrc.pu32[xSrc] >> 1) | uSrc.pu32[xSrc]) & UINT32_C(0x44444444)) >> 2) & 1)
                                 | ((u32Tmp >> ( 6 - 1)) & RT_BIT_32(1))
                                 | ((u32Tmp >> (10 - 2)) & RT_BIT_32(2))
                                 | ((u32Tmp >> (14 - 3)) & RT_BIT_32(3))
                                 | ((u32Tmp >> (18 - 4)) & RT_BIT_32(4))
                                 | ((u32Tmp >> (22 - 5)) & RT_BIT_32(5))
                                 | ((u32Tmp >> (26 - 6)) & RT_BIT_32(6))
                                 | ((u32Tmp >> (30 - 7)) & RT_BIT_32(7)) );
                break;
            case SVGA3D_Z_D16:
                CONVERT_SCANLINE(  (((u32Tmp = ~uSrc.pu16[xSrc]) >> 1) & 1)
                                 | ((u32Tmp >> ( 3 - 1)) & RT_BIT_32(1))
                                 | ((u32Tmp >> ( 5 - 2)) & RT_BIT_32(2))
                                 | ((u32Tmp >> ( 7 - 3)) & RT_BIT_32(3))
                                 | ((u32Tmp >> ( 9 - 4)) & RT_BIT_32(4))
                                 | ((u32Tmp >> (11 - 5)) & RT_BIT_32(5))
                                 | ((u32Tmp >> (13 - 6)) & RT_BIT_32(6))
                                 | ((u32Tmp >> (15 - 7)) & RT_BIT_32(7)) );
                break;
            case SVGA3D_Z_D24S8:
                CONVERT_SCANLINE(  ((u32Tmp = uSrc.pu16[xSrc]) & 0xff) /* stencile */
                                 | ((~u32Tmp >> 18) & 0x3f));
                break;
            case SVGA3D_Z_D15S1:
                CONVERT_SCANLINE(  (((u32Tmp = uSrc.pu16[xSrc]) & 0x01) << 7) /* stencile */
                                 | ((~u32Tmp >> 8) & 0x7f));
                break;

            /* Pure alpha. */
            case SVGA3D_ALPHA8:
                CONVERT_SCANLINE(uSrc.pu8[xSrc]);
                break;

            /* Luminance */
            case SVGA3D_LUMINANCE8:
                CONVERT_SCANLINE(uSrc.pu8[xSrc]);
                break;
            case SVGA3D_LUMINANCE4_ALPHA4:
                CONVERT_SCANLINE(uSrc.pu8[xSrc] & 0xf0);
                break;
            case SVGA3D_LUMINANCE16:
                CONVERT_SCANLINE(uSrc.pu16[xSrc] >> 8);
                break;
            case SVGA3D_LUMINANCE8_ALPHA8:
                CONVERT_SCANLINE(uSrc.pu16[xSrc] >> 8);
                break;

            /* Not supported. */
            case SVGA3D_DXT1:
            case SVGA3D_DXT2:
            case SVGA3D_DXT3:
            case SVGA3D_DXT4:
            case SVGA3D_DXT5:
            case SVGA3D_BUFFER:
                AssertFailedBreak();

            /* Not considered for implementation yet. */
            case SVGA3D_BUMPU8V8:
            case SVGA3D_BUMPL6V5U5:
            case SVGA3D_BUMPX8L8V8U8:
            case SVGA3D_BUMPL8V8U8:
            case SVGA3D_ARGB_S10E5:
            case SVGA3D_ARGB_S23E8:
            case SVGA3D_V8U8:
            case SVGA3D_Q8W8V8U8:
            case SVGA3D_CxV8U8:
            case SVGA3D_X8L8V8U8:
            case SVGA3D_A2W10V10U10:
            case SVGA3D_R_S10E5:
            case SVGA3D_R_S23E8:
            case SVGA3D_RG_S10E5:
            case SVGA3D_RG_S23E8:
            case SVGA3D_Z_D24X8:
            case SVGA3D_V16U16:
            case SVGA3D_UYVY:
            case SVGA3D_YUY2:
            case SVGA3D_NV12:
            case SVGA3D_AYUV:
            case SVGA3D_BC4_UNORM:
            case SVGA3D_BC5_UNORM:
            case SVGA3D_Z_DF16:
            case SVGA3D_Z_DF24:
            case SVGA3D_Z_D24S8_INT:
                if (!fHitFormatAssert)
                {
                    AssertMsgFailed(("%s is not implemented\n", vmsvgaSurfaceType2String(enmFormat)));
                    fHitFormatAssert = true;
                }
                /* fall thru */
            default:
                /* Lazy programmer fallbacks. */
                if (cbSrcPixel == 4)
                    CONVERT_SCANLINE( (  ((u32Tmp = uSrc.pu32[xSrc]) & 0xff)
                                       + ((u32Tmp >>  8) & 0xff)
                                       + ((u32Tmp >> 16) & 0xff)
                                       + ((u32Tmp >> 24) & 0xff) ) / 4);
                else if (cbSrcPixel == 3)
                    CONVERT_SCANLINE( (  (uint32_t)uSrc.pu8[xSrc * 4]
                                       + (uint32_t)uSrc.pu8[xSrc * 4 + 1]
                                       + (uint32_t)uSrc.pu8[xSrc * 4 + 2] ) / 3);
                else if (cbSrcPixel == 2)
                    CONVERT_SCANLINE( (  ((u32Tmp = uSrc.pu16[xSrc]) & 0xf) * 16
                                       + ((u32Tmp >>  4) & 0xf)             * 16
                                       + ((u32Tmp >>  8) & 0xf)             * 16
                                       + ((u32Tmp >> 12) & 0xf)             * 16 ) / 4);
                else if (cbSrcPixel == 1)
                    CONVERT_SCANLINE(uSrc.pu8[xSrc]);
                else
                    AssertFailed();
                break;

        }

        /*
         * Print we've reached the end of a block in y direction or if we're at
         * the end of the image.
         */
        cyLeft--;
        if (--cyLeftInScanline == 0 || cyLeft == 0)
        {
            for (uint32_t i = 0; i < cchLine; i++)
            {
                uint32_t off = pauScanline[i] * s_cchPalette / cPixelsWeightPerChar; Assert(off < s_cchPalette);
                pszLine[i] = s_szPalette[off < sizeof(s_szPalette) - 1 ? off : sizeof(s_szPalette) - 1];
            }
            pszLine[cchLine] = '\0';
            pfnPrintLine(pszLine, pvUser);

            if (!cyLeft)
                break;
            cyLeftInScanline = cyPerChar;
            RT_BZERO(pauScanline, sizeof(pauScanline[0]) * cchLine);
        }

        /*
         * Advance.
         */
        if (!fInvY)
            uSrc.pu8 += cbScanline;
        else
            uSrc.pu8 -= cbScanline;
    }
}


/**
 * List of render state names with type prefix.
 *
 * First char in the name is a type indicator:
 *      - '*' = requires special handling.
 *      - 'f' = SVGA3dbool
 *      - 'd' = uint32_t
 *      - 'r' = float
 *      - 'b' = SVGA3dBlendOp
 *      - 'c' = SVGA3dColor, SVGA3dColorMask
 *      - 'e' = SVGA3dBlendEquation
 *      - 'm' = SVGA3dColorMask
 *      - 'p' = SVGA3dCmpFunc
 *      - 's' = SVGA3dStencilOp
 *      - 'v' = SVGA3dVertexMaterial
 *      - 'w' = SVGA3dWrapFlags
 */
static const char * const g_apszRenderStateNamesAndType[] =
{
   "*" "INVALID",                       /*  invalid  */
   "f" "ZENABLE",                       /*  SVGA3dBool  */
   "f" "ZWRITEENABLE",                  /*  SVGA3dBool  */
   "f" "ALPHATESTENABLE",               /*  SVGA3dBool  */
   "f" "DITHERENABLE",                  /*  SVGA3dBool  */
   "f" "BLENDENABLE",                   /*  SVGA3dBool  */
   "f" "FOGENABLE",                     /*  SVGA3dBool  */
   "f" "SPECULARENABLE",                /*  SVGA3dBool  */
   "f" "STENCILENABLE",                 /*  SVGA3dBool  */
   "f" "LIGHTINGENABLE",                /*  SVGA3dBool  */
   "f" "NORMALIZENORMALS",              /*  SVGA3dBool  */
   "f" "POINTSPRITEENABLE",             /*  SVGA3dBool  */
   "f" "POINTSCALEENABLE",              /*  SVGA3dBool  */
   "x" "STENCILREF",                    /*  uint32_t  */
   "x" "STENCILMASK",                   /*  uint32_t  */
   "x" "STENCILWRITEMASK",              /*  uint32_t  */
   "r" "FOGSTART",                      /*  float  */
   "r" "FOGEND",                        /*  float  */
   "r" "FOGDENSITY",                    /*  float  */
   "r" "POINTSIZE",                     /*  float  */
   "r" "POINTSIZEMIN",                  /*  float  */
   "r" "POINTSIZEMAX",                  /*  float  */
   "r" "POINTSCALE_A",                  /*  float  */
   "r" "POINTSCALE_B",                  /*  float  */
   "r" "POINTSCALE_C",                  /*  float  */
   "c" "FOGCOLOR",                      /*  SVGA3dColor  */
   "c" "AMBIENT",                       /*  SVGA3dColor  */
   "*" "CLIPPLANEENABLE",               /*  SVGA3dClipPlanes  */
   "*" "FOGMODE",                       /*  SVGA3dFogMode  */
   "*" "FILLMODE",                      /*  SVGA3dFillMode  */
   "*" "SHADEMODE",                     /*  SVGA3dShadeMode  */
   "*" "LINEPATTERN",                   /*  SVGA3dLinePattern  */
   "b" "SRCBLEND",                      /*  SVGA3dBlendOp  */
   "b" "DSTBLEND",                      /*  SVGA3dBlendOp  */
   "e" "BLENDEQUATION",                 /*  SVGA3dBlendEquation  */
   "*" "CULLMODE",                      /*  SVGA3dFace  */
   "p" "ZFUNC",                         /*  SVGA3dCmpFunc  */
   "p" "ALPHAFUNC",                     /*  SVGA3dCmpFunc  */
   "p" "STENCILFUNC",                   /*  SVGA3dCmpFunc  */
   "s" "STENCILFAIL",                   /*  SVGA3dStencilOp  */
   "s" "STENCILZFAIL",                  /*  SVGA3dStencilOp  */
   "s" "STENCILPASS",                   /*  SVGA3dStencilOp  */
   "r" "ALPHAREF",                      /*  float  */
   "*" "FRONTWINDING",                  /*  SVGA3dFrontWinding  */
   "*" "COORDINATETYPE",                /*  SVGA3dCoordinateType  */
   "r" "ZBIAS",                         /*  float  */
   "f" "RANGEFOGENABLE",                /*  SVGA3dBool  */
   "c" "COLORWRITEENABLE",              /*  SVGA3dColorMask  */
   "f" "VERTEXMATERIALENABLE",          /*  SVGA3dBool  */
   "v" "DIFFUSEMATERIALSOURCE",         /*  SVGA3dVertexMaterial  */
   "v" "SPECULARMATERIALSOURCE",        /*  SVGA3dVertexMaterial  */
   "v" "AMBIENTMATERIALSOURCE",         /*  SVGA3dVertexMaterial  */
   "v" "EMISSIVEMATERIALSOURCE",        /*  SVGA3dVertexMaterial  */
   "c" "TEXTUREFACTOR",                 /*  SVGA3dColor  */
   "f" "LOCALVIEWER",                   /*  SVGA3dBool  */
   "f" "SCISSORTESTENABLE",             /*  SVGA3dBool  */
   "c" "BLENDCOLOR",                    /*  SVGA3dColor  */
   "f" "STENCILENABLE2SIDED",           /*  SVGA3dBool  */
   "p" "CCWSTENCILFUNC",                /*  SVGA3dCmpFunc  */
   "s" "CCWSTENCILFAIL",                /*  SVGA3dStencilOp  */
   "s" "CCWSTENCILZFAIL",               /*  SVGA3dStencilOp  */
   "s" "CCWSTENCILPASS",                /*  SVGA3dStencilOp  */
   "*" "VERTEXBLEND",                   /*  SVGA3dVertexBlendFlags  */
   "r" "SLOPESCALEDEPTHBIAS",           /*  float  */
   "r" "DEPTHBIAS",                     /*  float  */
   "r" "OUTPUTGAMMA",                   /*  float  */
   "f" "ZVISIBLE",                      /*  SVGA3dBool  */
   "f" "LASTPIXEL",                     /*  SVGA3dBool  */
   "f" "CLIPPING",                      /*  SVGA3dBool  */
   "w" "WRAP0",                         /*  SVGA3dWrapFlags  */
   "w" "WRAP1",                         /*  SVGA3dWrapFlags  */
   "w" "WRAP2",                         /*  SVGA3dWrapFlags  */
   "w" "WRAP3",                         /*  SVGA3dWrapFlags  */
   "w" "WRAP4",                         /*  SVGA3dWrapFlags  */
   "w" "WRAP5",                         /*  SVGA3dWrapFlags  */
   "w" "WRAP6",                         /*  SVGA3dWrapFlags  */
   "w" "WRAP7",                         /*  SVGA3dWrapFlags  */
   "w" "WRAP8",                         /*  SVGA3dWrapFlags  */
   "w" "WRAP9",                         /*  SVGA3dWrapFlags  */
   "w" "WRAP10",                        /*  SVGA3dWrapFlags  */
   "w" "WRAP11",                        /*  SVGA3dWrapFlags  */
   "w" "WRAP12",                        /*  SVGA3dWrapFlags  */
   "w" "WRAP13",                        /*  SVGA3dWrapFlags  */
   "w" "WRAP14",                        /*  SVGA3dWrapFlags  */
   "w" "WRAP15",                        /*  SVGA3dWrapFlags  */
   "f" "MULTISAMPLEANTIALIAS",          /*  SVGA3dBool  */
   "x" "MULTISAMPLEMASK",               /*  uint32_t  */
   "f" "INDEXEDVERTEXBLENDENABLE",      /*  SVGA3dBool  */
   "r" "TWEENFACTOR",                   /*  float  */
   "f" "ANTIALIASEDLINEENABLE",         /*  SVGA3dBool  */
   "c" "COLORWRITEENABLE1",             /*  SVGA3dColorMask  */
   "c" "COLORWRITEENABLE2",             /*  SVGA3dColorMask  */
   "c" "COLORWRITEENABLE3",             /*  SVGA3dColorMask  */
   "f" "SEPARATEALPHABLENDENABLE",      /*  SVGA3dBool  */
   "b" "SRCBLENDALPHA",                 /*  SVGA3dBlendOp  */
   "b" "DSTBLENDALPHA",                 /*  SVGA3dBlendOp  */
   "e" "BLENDEQUATIONALPHA",            /*  SVGA3dBlendEquation  */
   "*" "TRANSPARENCYANTIALIAS",         /*  SVGA3dTransparencyAntialiasType  */
   "f" "LINEAA",                        /*  SVGA3dBool  */
   "r" "LINEWIDTH",                     /*  float  */
};


/**
 * Formats a SVGA3dRenderState structure as a string.
 *
 * @returns pszBuffer.
 * @param   pszBuffer       Output string buffer.
 * @param   cbBuffer        Size of output buffer.
 * @param   pRenderState    The SVGA3d render state to format.
 */
char *vmsvga3dFormatRenderState(char *pszBuffer, size_t cbBuffer, SVGA3dRenderState const *pRenderState)
{
    uint32_t iState = pRenderState->state;
    if (iState != SVGA3D_RS_INVALID)
    {
        if (iState < RT_ELEMENTS(g_apszRenderStateNamesAndType))
        {
            const char *pszName = g_apszRenderStateNamesAndType[iState];
            char const  chType  = *pszName++;

            union
            {
               uint32_t  u;
               float     r;
               SVGA3dColorMask Color;
            } uValue;
            uValue.u = pRenderState->uintValue;

            switch (chType)
            {
                case 'f':
                    if (uValue.u == 0)
                        RTStrPrintf(pszBuffer, cbBuffer, "%s = false", pszName);
                    else if (uValue.u == 1)
                        RTStrPrintf(pszBuffer, cbBuffer, "%s = true", pszName);
                    else
                        RTStrPrintf(pszBuffer, cbBuffer, "%s = true (%#x)", pszName, uValue.u);
                    break;
                case 'x':
                    RTStrPrintf(pszBuffer, cbBuffer, "%s = %#x (%d)", pszName, uValue.u, uValue.u);
                    break;
                case 'r':
                    RTStrPrintf(pszBuffer, cbBuffer, "%s = %d.%06u (%#x)",
                                pszName, (int)uValue.r, (unsigned)(uValue.r * 1000000) % 1000000U, uValue.u);
                    break;
                case 'c': //SVGA3dColor, SVGA3dColorMask
                    RTStrPrintf(pszBuffer, cbBuffer, "%s = RGBA(%d,%d,%d,%d) (%#x)", pszName,
                                uValue.Color.s.red, uValue.Color.s.green, uValue.Color.s.blue, uValue.Color.s.alpha, uValue.u);
                    break;
                case 'w': //SVGA3dWrapFlags
                    RTStrPrintf(pszBuffer, cbBuffer, "%s = %#x%s", pszName, uValue.u,
                                uValue.u <= SVGA3D_WRAPCOORD_ALL ? " (out of bounds" : "");
                    break;
                default:
                    AssertFailed();
                case 'b': //SVGA3dBlendOp
                case 'e': //SVGA3dBlendEquation
                case 'p': //SVGA3dCmpFunc
                case 's': //SVGA3dStencilOp
                case 'v': //SVGA3dVertexMaterial
                case '*':
                    RTStrPrintf(pszBuffer, cbBuffer, "%s = %#x", pszName, uValue.u);
                    break;
            }
        }
        else
            RTStrPrintf(pszBuffer, cbBuffer, "UNKNOWN_%d_%#x = %#x", iState, iState, pRenderState->uintValue);
    }
    else
        RTStrPrintf(pszBuffer, cbBuffer, "INVALID");
    return pszBuffer;
}

/**
 * Texture state names with type prefix.
 */
static const char * const g_apszTextureStateNamesAndType[] =
{
    "*" "INVALID",                      /*  invalid  */
    "x" "BIND_TEXTURE",                 /*  SVGA3dSurfaceId  */
    "m" "COLOROP",                      /*  SVGA3dTextureCombiner  */
    "a" "COLORARG1",                    /*  SVGA3dTextureArgData  */
    "a" "COLORARG2",                    /*  SVGA3dTextureArgData  */
    "m" "ALPHAOP",                      /*  SVGA3dTextureCombiner  */
    "a" "ALPHAARG1",                    /*  SVGA3dTextureArgData  */
    "a" "ALPHAARG2",                    /*  SVGA3dTextureArgData  */
    "e" "ADDRESSU",                     /*  SVGA3dTextureAddress  */
    "e" "ADDRESSV",                     /*  SVGA3dTextureAddress  */
    "l" "MIPFILTER",                    /*  SVGA3dTextureFilter  */
    "l" "MAGFILTER",                    /*  SVGA3dTextureFilter  */
    "m" "MINFILTER",                    /*  SVGA3dTextureFilter  */
    "c" "BORDERCOLOR",                  /*  SVGA3dColor  */
    "r" "TEXCOORDINDEX",                /*  uint32_t  */
    "t" "TEXTURETRANSFORMFLAGS",        /*  SVGA3dTexTransformFlags  */
    "g" "TEXCOORDGEN",                  /*  SVGA3dTextureCoordGen  */
    "r" "BUMPENVMAT00",                 /*  float  */
    "r" "BUMPENVMAT01",                 /*  float  */
    "r" "BUMPENVMAT10",                 /*  float  */
    "r" "BUMPENVMAT11",                 /*  float  */
    "x" "TEXTURE_MIPMAP_LEVEL",         /*  uint32_t  */
    "r" "TEXTURE_LOD_BIAS",             /*  float  */
    "x" "TEXTURE_ANISOTROPIC_LEVEL",    /*  uint32_t  */
    "e" "ADDRESSW",                     /*  SVGA3dTextureAddress  */
    "r" "GAMMA",                        /*  float  */
    "r" "BUMPENVLSCALE",                /*  float  */
    "r" "BUMPENVLOFFSET",               /*  float  */
    "a" "COLORARG0",                    /*  SVGA3dTextureArgData  */
    "a" "ALPHAARG0"                     /*  SVGA3dTextureArgData */
};

/**
 * Formats a SVGA3dTextureState structure as a string.
 *
 * @returns pszBuffer.
 * @param   pszBuffer       Output string buffer.
 * @param   cbBuffer        Size of output buffer.
 * @param   pTextureState   The SVGA3d texture state to format.
 */
char *vmsvga3dFormatTextureState(char *pszBuffer, size_t cbBuffer, SVGA3dTextureState const *pTextureState)
{
    /*
     * Format the stage first.
     */
    char  *pszRet    = pszBuffer;
    size_t cchPrefix = RTStrPrintf(pszBuffer, cbBuffer, "[%u] ", pTextureState->stage);
    if (cchPrefix < cbBuffer)
    {
        cbBuffer  -= cchPrefix;
        pszBuffer += cchPrefix;
    }
    else
        cbBuffer = 0;

    /*
     * Format the name and value.
     */
    uint32_t iName = pTextureState->name;
    if (iName != SVGA3D_TS_INVALID)
    {
        if (iName < RT_ELEMENTS(g_apszTextureStateNamesAndType))
        {
            const char *pszName = g_apszTextureStateNamesAndType[iName];
            char        chType  = *pszName++;

            union
            {
               uint32_t  u;
               float     r;
               SVGA3dColorMask Color;
            } uValue;
            uValue.u = pTextureState->value;

            switch (chType)
            {
                case 'x':
                    RTStrPrintf(pszBuffer, cbBuffer, "%s = %#x (%d)", pszName, uValue.u, uValue.u);
                    break;

                case 'r':
                    RTStrPrintf(pszBuffer, cbBuffer, "%s = %d.%06u (%#x)",
                                pszName, (int)uValue.r, (unsigned)(uValue.r * 1000000) % 1000000U, uValue.u);
                    break;

                case 'a': //SVGA3dTextureArgData
                {
                    static const char * const s_apszValues[] =
                    {
                        "INVALID", "CONSTANT", "PREVIOUS", "DIFFUSE", "TEXTURE", "SPECULAR"
                    };
                    vmsvgaFormatEnumValue(pszBuffer, cbBuffer, pszName, uValue.u,
                                          "SVGA3D_TA_", s_apszValues, RT_ELEMENTS(s_apszValues));
                    break;
                }

                case 'c': //SVGA3dColor, SVGA3dColorMask
                    RTStrPrintf(pszBuffer, cbBuffer, "%s = RGBA(%d,%d,%d,%d) (%#x)", pszName,
                                uValue.Color.s.red, uValue.Color.s.green, uValue.Color.s.blue, uValue.Color.s.alpha, uValue.u);
                    break;

                case 'e': //SVGA3dTextureAddress
                {
                    static const char * const s_apszValues[] =
                    {
                        "INVALID", "WRAP", "MIRROR", "CLAMP", "BORDER", "MIRRORONCE", "EDGE",
                    };
                    vmsvgaFormatEnumValue(pszBuffer, cbBuffer, pszName, uValue.u,
                                          "SVGA3D_TEX_ADDRESS_", s_apszValues, RT_ELEMENTS(s_apszValues));
                    break;
                }

                case 'l': //SVGA3dTextureFilter
                {
                    static const char * const s_apszValues[] =
                    {
                        "NONE", "NEAREST", "LINEAR",  "ANISOTROPIC", "FLATCUBIC", "GAUSSIANCUBIC", "PYRAMIDALQUAD", "GAUSSIANQUAD",
                    };
                    vmsvgaFormatEnumValue(pszBuffer, cbBuffer, pszName, uValue.u,
                                          "SVGA3D_TEX_FILTER_", s_apszValues, RT_ELEMENTS(s_apszValues));
                    break;
                }

                case 'g': //SVGA3dTextureCoordGen
                {
                    static const char * const s_apszValues[] =
                    {
                        "OFF", "EYE_POSITION", "EYE_NORMAL", "REFLECTIONVECTOR", "SPHERE",
                    };
                    vmsvgaFormatEnumValue(pszBuffer, cbBuffer, pszName, uValue.u,
                                          "SVGA3D_TEXCOORD_GEN_", s_apszValues, RT_ELEMENTS(s_apszValues));
                    break;
                }

                case 'm': //SVGA3dTextureCombiner
                {
                    static const char * const s_apszValues[] =
                    {
                        "INVALID", "DISABLE", "SELECTARG1", "SELECTARG2", "MODULATE", "ADD", "ADDSIGNED", "SUBTRACT",
                        "BLENDTEXTUREALPHA", "BLENDDIFFUSEALPHA", "BLENDCURRENTALPHA", "BLENDFACTORALPHA", "MODULATE2X",
                        "MODULATE4X", "DSDT", "DOTPRODUCT3", "BLENDTEXTUREALPHAPM", "ADDSIGNED2X", "ADDSMOOTH", "PREMODULATE",
                        "MODULATEALPHA_ADDCOLOR", "MODULATECOLOR_ADDALPHA", "MODULATEINVALPHA_ADDCOLOR",
                        "MODULATEINVCOLOR_ADDALPHA", "BUMPENVMAPLUMINANCE", "MULTIPLYADD", "LERP",
                    };
                    vmsvgaFormatEnumValue(pszBuffer, cbBuffer, pszName, uValue.u,
                                          "SVGA3D_TC_", s_apszValues, RT_ELEMENTS(s_apszValues));
                    break;
                }

                default:
                    AssertFailed();
                    RTStrPrintf(pszBuffer, cbBuffer, "%s = %#x\n", pszName, uValue.u);
                    break;
            }
        }
        else
            RTStrPrintf(pszBuffer, cbBuffer, "UNKNOWN_%d_%#x = %#x\n", iName, iName, pTextureState->value);
    }
    else
        RTStrPrintf(pszBuffer, cbBuffer, "INVALID");
    return pszRet;
}




/**
 * Calculate the size of one pixel
 */
uint32_t vmsvga3dSurfaceFormatSize(SVGA3dSurfaceFormat format)
{
    switch (format)
    {
    case SVGA3D_X8R8G8B8:
    case SVGA3D_A8R8G8B8:
        return 4;

    case SVGA3D_R5G6B5:
    case SVGA3D_X1R5G5B5:
    case SVGA3D_A1R5G5B5:
    case SVGA3D_A4R4G4B4:
        return 2;

    case SVGA3D_Z_D32:
    case SVGA3D_Z_D24S8:
    case SVGA3D_Z_D24X8:
    case SVGA3D_Z_DF24:
    case SVGA3D_Z_D24S8_INT:
        return 4;

    case SVGA3D_Z_D16:
    case SVGA3D_Z_DF16:
    case SVGA3D_Z_D15S1:
        return 2;

    case SVGA3D_LUMINANCE8:
    case SVGA3D_LUMINANCE4_ALPHA4:
        return 1;

    case SVGA3D_LUMINANCE16:
    case SVGA3D_LUMINANCE8_ALPHA8:
        return 2;

    case SVGA3D_DXT1:
    case SVGA3D_DXT2:
        return 8;

    case SVGA3D_DXT3:
    case SVGA3D_DXT4:
    case SVGA3D_DXT5:
        return 16;

    case SVGA3D_BUMPU8V8:
    case SVGA3D_BUMPL6V5U5:
        return 2;

    case SVGA3D_BUMPX8L8V8U8:
    case SVGA3D_Q8W8V8U8:
        return 4;

    case SVGA3D_V8U8:
    case SVGA3D_CxV8U8:
        return 2;

    case SVGA3D_X8L8V8U8:
    case SVGA3D_A2W10V10U10:
        return 4;

    case SVGA3D_ARGB_S10E5:   /* 16-bit floating-point ARGB */
        return 2;
    case SVGA3D_ARGB_S23E8:   /* 32-bit floating-point ARGB */
        return 4;

    case SVGA3D_A2R10G10B10:
        return 4;

    case SVGA3D_ALPHA8:
        return 1;

    case SVGA3D_R_S10E5:
        return 2;

    case SVGA3D_R_S23E8:
    case SVGA3D_RG_S10E5:
        return 4;

    case SVGA3D_RG_S23E8:
        return 8;

    /*
     * Any surface can be used as a buffer object, but SVGA3D_BUFFER is
     * the most efficient format to use when creating new surfaces
     * expressly for index or vertex data.
     */
    case SVGA3D_BUFFER:
        return 1;

    case SVGA3D_NV12:
        return 1;

    case SVGA3D_V16U16:
        return 4;

    case SVGA3D_G16R16:
        return 32;
    case SVGA3D_A16B16G16R16:
        return 8;

    default:
        AssertFailedReturn(4);
    }
}

#ifdef LOG_ENABLED

const char *vmsvga3dGetCapString(uint32_t idxCap)
{
    switch (idxCap)
    {
    case SVGA3D_DEVCAP_3D:
        return "SVGA3D_DEVCAP_3D";
    case SVGA3D_DEVCAP_MAX_LIGHTS:
        return "SVGA3D_DEVCAP_MAX_LIGHTS";
    case SVGA3D_DEVCAP_MAX_TEXTURES:
        return "SVGA3D_DEVCAP_MAX_TEXTURES";
    case SVGA3D_DEVCAP_MAX_CLIP_PLANES:
        return "SVGA3D_DEVCAP_MAX_CLIP_PLANES";
    case SVGA3D_DEVCAP_VERTEX_SHADER_VERSION:
        return "SVGA3D_DEVCAP_VERTEX_SHADER_VERSION";
    case SVGA3D_DEVCAP_VERTEX_SHADER:
        return "SVGA3D_DEVCAP_VERTEX_SHADER";
    case SVGA3D_DEVCAP_FRAGMENT_SHADER_VERSION:
        return "SVGA3D_DEVCAP_FRAGMENT_SHADER_VERSION";
    case SVGA3D_DEVCAP_FRAGMENT_SHADER:
        return "SVGA3D_DEVCAP_FRAGMENT_SHADER";
    case SVGA3D_DEVCAP_MAX_RENDER_TARGETS:
        return "SVGA3D_DEVCAP_MAX_RENDER_TARGETS";
    case SVGA3D_DEVCAP_S23E8_TEXTURES:
        return "SVGA3D_DEVCAP_S23E8_TEXTURES";
    case SVGA3D_DEVCAP_S10E5_TEXTURES:
        return "SVGA3D_DEVCAP_S10E5_TEXTURES";
    case SVGA3D_DEVCAP_MAX_FIXED_VERTEXBLEND:
        return "SVGA3D_DEVCAP_MAX_FIXED_VERTEXBLEND";
    case SVGA3D_DEVCAP_D16_BUFFER_FORMAT:
        return "SVGA3D_DEVCAP_D16_BUFFER_FORMAT";
    case SVGA3D_DEVCAP_D24S8_BUFFER_FORMAT:
        return "SVGA3D_DEVCAP_D24S8_BUFFER_FORMAT";
    case SVGA3D_DEVCAP_D24X8_BUFFER_FORMAT:
        return "SVGA3D_DEVCAP_D24X8_BUFFER_FORMAT";
    case SVGA3D_DEVCAP_QUERY_TYPES:
        return "SVGA3D_DEVCAP_QUERY_TYPES";
    case SVGA3D_DEVCAP_TEXTURE_GRADIENT_SAMPLING:
        return "SVGA3D_DEVCAP_TEXTURE_GRADIENT_SAMPLING";
    case SVGA3D_DEVCAP_MAX_POINT_SIZE:
        return "SVGA3D_DEVCAP_MAX_POINT_SIZE";
    case SVGA3D_DEVCAP_MAX_SHADER_TEXTURES:
        return "SVGA3D_DEVCAP_MAX_SHADER_TEXTURES";
    case SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH:
        return "SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH";
    case SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT:
        return "SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT";
    case SVGA3D_DEVCAP_MAX_VOLUME_EXTENT:
        return "SVGA3D_DEVCAP_MAX_VOLUME_EXTENT";
    case SVGA3D_DEVCAP_MAX_TEXTURE_REPEAT:
        return "SVGA3D_DEVCAP_MAX_TEXTURE_REPEAT";
    case SVGA3D_DEVCAP_MAX_TEXTURE_ASPECT_RATIO:
        return "SVGA3D_DEVCAP_MAX_TEXTURE_ASPECT_RATIO";
    case SVGA3D_DEVCAP_MAX_TEXTURE_ANISOTROPY:
        return "SVGA3D_DEVCAP_MAX_TEXTURE_ANISOTROPY";
    case SVGA3D_DEVCAP_MAX_PRIMITIVE_COUNT:
        return "SVGA3D_DEVCAP_MAX_PRIMITIVE_COUNT";
    case SVGA3D_DEVCAP_MAX_VERTEX_INDEX:
        return "SVGA3D_DEVCAP_MAX_VERTEX_INDEX";
    case SVGA3D_DEVCAP_MAX_VERTEX_SHADER_INSTRUCTIONS:
        return "SVGA3D_DEVCAP_MAX_VERTEX_SHADER_INSTRUCTIONS";
    case SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_INSTRUCTIONS:
        return "SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_INSTRUCTIONS";
    case SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEMPS:
        return "SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEMPS";
    case SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_TEMPS:
        return "SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_TEMPS";
    case SVGA3D_DEVCAP_TEXTURE_OPS:
        return "SVGA3D_DEVCAP_TEXTURE_OPS";
    case SVGA3D_DEVCAP_MULTISAMPLE_NONMASKABLESAMPLES:
        return "SVGA3D_DEVCAP_MULTISAMPLE_NONMASKABLESAMPLES";
    case SVGA3D_DEVCAP_MULTISAMPLE_MASKABLESAMPLES:
        return "SVGA3D_DEVCAP_MULTISAMPLE_MASKABLESAMPLES";
    case SVGA3D_DEVCAP_ALPHATOCOVERAGE:
        return "SVGA3D_DEVCAP_ALPHATOCOVERAGE";
    case SVGA3D_DEVCAP_SUPERSAMPLE:
        return "SVGA3D_DEVCAP_SUPERSAMPLE";
    case SVGA3D_DEVCAP_AUTOGENMIPMAPS:
        return "SVGA3D_DEVCAP_AUTOGENMIPMAPS";
    case SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEXTURES:
        return "SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEXTURES";
    case SVGA3D_DEVCAP_MAX_SIMULTANEOUS_RENDER_TARGETS:
        return "SVGA3D_DEVCAP_MAX_SIMULTANEOUS_RENDER_TARGETS";
    case SVGA3D_DEVCAP_MAX_CONTEXT_IDS:
        return "SVGA3D_DEVCAP_MAX_CONTEXT_IDS";
    case SVGA3D_DEVCAP_MAX_SURFACE_IDS:
        return "SVGA3D_DEVCAP_MAX_SURFACE_IDS";
    case SVGA3D_DEVCAP_SURFACEFMT_X8R8G8B8:
        return "SVGA3D_DEVCAP_SURFACEFMT_X8R8G8B8";
    case SVGA3D_DEVCAP_SURFACEFMT_A8R8G8B8:
        return "SVGA3D_DEVCAP_SURFACEFMT_A8R8G8B8";
    case SVGA3D_DEVCAP_SURFACEFMT_A2R10G10B10:
        return "SVGA3D_DEVCAP_SURFACEFMT_A2R10G10B10";
    case SVGA3D_DEVCAP_SURFACEFMT_X1R5G5B5:
        return "SVGA3D_DEVCAP_SURFACEFMT_X1R5G5B5";
    case SVGA3D_DEVCAP_SURFACEFMT_A1R5G5B5:
        return "SVGA3D_DEVCAP_SURFACEFMT_A1R5G5B5";
    case SVGA3D_DEVCAP_SURFACEFMT_A4R4G4B4:
        return "SVGA3D_DEVCAP_SURFACEFMT_A4R4G4B4";
    case SVGA3D_DEVCAP_SURFACEFMT_R5G6B5:
        return "SVGA3D_DEVCAP_SURFACEFMT_R5G6B5";
    case SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE16:
        return "SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE16";
    case SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8_ALPHA8:
        return "SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8_ALPHA8";
    case SVGA3D_DEVCAP_SURFACEFMT_ALPHA8:
        return "SVGA3D_DEVCAP_SURFACEFMT_ALPHA8";
    case SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8:
        return "SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8";
    case SVGA3D_DEVCAP_SURFACEFMT_Z_D16:
        return "SVGA3D_DEVCAP_SURFACEFMT_Z_D16";
    case SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8:
        return "SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8";
    case SVGA3D_DEVCAP_SURFACEFMT_Z_D24X8:
        return "SVGA3D_DEVCAP_SURFACEFMT_Z_D24X8";
    case SVGA3D_DEVCAP_SURFACEFMT_Z_DF16:
        return "SVGA3D_DEVCAP_SURFACEFMT_Z_DF16";
    case SVGA3D_DEVCAP_SURFACEFMT_Z_DF24:
        return "SVGA3D_DEVCAP_SURFACEFMT_Z_DF24";
    case SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8_INT:
        return "SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8_INT";
    case SVGA3D_DEVCAP_SURFACEFMT_DXT1:
        return "SVGA3D_DEVCAP_SURFACEFMT_DXT1";
    case SVGA3D_DEVCAP_SURFACEFMT_DXT2:
        return "SVGA3D_DEVCAP_SURFACEFMT_DXT2";
    case SVGA3D_DEVCAP_SURFACEFMT_DXT3:
        return "SVGA3D_DEVCAP_SURFACEFMT_DXT3";
    case SVGA3D_DEVCAP_SURFACEFMT_DXT4:
        return "SVGA3D_DEVCAP_SURFACEFMT_DXT4";
    case SVGA3D_DEVCAP_SURFACEFMT_DXT5:
        return "SVGA3D_DEVCAP_SURFACEFMT_DXT5";
    case SVGA3D_DEVCAP_SURFACEFMT_BUMPX8L8V8U8:
        return "SVGA3D_DEVCAP_SURFACEFMT_BUMPX8L8V8U8";
    case SVGA3D_DEVCAP_SURFACEFMT_A2W10V10U10:
        return "SVGA3D_DEVCAP_SURFACEFMT_A2W10V10U10";
    case SVGA3D_DEVCAP_SURFACEFMT_BUMPU8V8:
        return "SVGA3D_DEVCAP_SURFACEFMT_BUMPU8V8";
    case SVGA3D_DEVCAP_SURFACEFMT_Q8W8V8U8:
        return "SVGA3D_DEVCAP_SURFACEFMT_Q8W8V8U8";
    case SVGA3D_DEVCAP_SURFACEFMT_CxV8U8:
        return "SVGA3D_DEVCAP_SURFACEFMT_CxV8U8";
    case SVGA3D_DEVCAP_SURFACEFMT_R_S10E5:
        return "SVGA3D_DEVCAP_SURFACEFMT_R_S10E5";
    case SVGA3D_DEVCAP_SURFACEFMT_R_S23E8:
        return "SVGA3D_DEVCAP_SURFACEFMT_R_S23E8";
    case SVGA3D_DEVCAP_SURFACEFMT_RG_S10E5:
        return "SVGA3D_DEVCAP_SURFACEFMT_RG_S10E5";
    case SVGA3D_DEVCAP_SURFACEFMT_RG_S23E8:
        return "SVGA3D_DEVCAP_SURFACEFMT_RG_S23E8";
    case SVGA3D_DEVCAP_SURFACEFMT_ARGB_S10E5:
        return "SVGA3D_DEVCAP_SURFACEFMT_ARGB_S10E5";
    case SVGA3D_DEVCAP_SURFACEFMT_ARGB_S23E8:
        return "SVGA3D_DEVCAP_SURFACEFMT_ARGB_S23E8";
    case SVGA3D_DEVCAP_SURFACEFMT_V16U16:
        return "SVGA3D_DEVCAP_SURFACEFMT_V16U16";
    case SVGA3D_DEVCAP_SURFACEFMT_G16R16:
        return "SVGA3D_DEVCAP_SURFACEFMT_G16R16";
    case SVGA3D_DEVCAP_SURFACEFMT_A16B16G16R16:
        return "SVGA3D_DEVCAP_SURFACEFMT_A16B16G16R16";
    case SVGA3D_DEVCAP_SURFACEFMT_UYVY:
        return "SVGA3D_DEVCAP_SURFACEFMT_UYVY";
    case SVGA3D_DEVCAP_SURFACEFMT_YUY2:
        return "SVGA3D_DEVCAP_SURFACEFMT_YUY2";
    case SVGA3D_DEVCAP_SURFACEFMT_NV12:
        return "SVGA3D_DEVCAP_SURFACEFMT_NV12";
    case SVGA3D_DEVCAP_SURFACEFMT_AYUV:
        return "SVGA3D_DEVCAP_SURFACEFMT_AYUV";
    case SVGA3D_DEVCAP_SURFACEFMT_BC4_UNORM:
        return "SVGA3D_DEVCAP_SURFACEFMT_BC4_UNORM";
    case SVGA3D_DEVCAP_SURFACEFMT_BC5_UNORM:
        return "SVGA3D_DEVCAP_SURFACEFMT_BC5_UNORM";
    default:
        return "UNEXPECTED";
    }
}

const char *vmsvga3dGet3dFormatString(uint32_t format)
{
    static char szFormat[1024];

    szFormat[0] = 0;

    if (format & SVGA3DFORMAT_OP_TEXTURE)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_TEXTURE\n");
    if (format & SVGA3DFORMAT_OP_VOLUMETEXTURE)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_VOLUMETEXTURE\n");
    if (format & SVGA3DFORMAT_OP_CUBETEXTURE)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_CUBETEXTURE\n");
    if (format & SVGA3DFORMAT_OP_OFFSCREEN_RENDERTARGET)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_OFFSCREEN_RENDERTARGET\n");
    if (format & SVGA3DFORMAT_OP_SAME_FORMAT_RENDERTARGET)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_SAME_FORMAT_RENDERTARGET\n");
    if (format & SVGA3DFORMAT_OP_ZSTENCIL)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_ZSTENCIL\n");
    if (format & SVGA3DFORMAT_OP_ZSTENCIL_WITH_ARBITRARY_COLOR_DEPTH)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_ZSTENCIL_WITH_ARBITRARY_COLOR_DEPTH\n");
    if (format & SVGA3DFORMAT_OP_SAME_FORMAT_UP_TO_ALPHA_RENDERTARGET)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_SAME_FORMAT_UP_TO_ALPHA_RENDERTARGET\n");
    if (format & SVGA3DFORMAT_OP_DISPLAYMODE)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_DISPLAYMODE\n");
    if (format & SVGA3DFORMAT_OP_3DACCELERATION)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_3DACCELERATION\n");
    if (format & SVGA3DFORMAT_OP_PIXELSIZE)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_PIXELSIZE\n");
    if (format & SVGA3DFORMAT_OP_CONVERT_TO_ARGB)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_CONVERT_TO_ARGB\n");
    if (format & SVGA3DFORMAT_OP_OFFSCREENPLAIN)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_OFFSCREENPLAIN\n");
    if (format & SVGA3DFORMAT_OP_SRGBREAD)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_SRGBREAD\n");
    if (format & SVGA3DFORMAT_OP_BUMPMAP)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_BUMPMAP\n");
    if (format & SVGA3DFORMAT_OP_DMAP)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_DMAP\n");
    if (format & SVGA3DFORMAT_OP_NOFILTER)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_NOFILTER\n");
    if (format & SVGA3DFORMAT_OP_MEMBEROFGROUP_ARGB)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_MEMBEROFGROUP_ARGB\n");
    if (format & SVGA3DFORMAT_OP_SRGBWRITE)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_SRGBWRITE\n");
    if (format & SVGA3DFORMAT_OP_NOALPHABLEND)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_NOALPHABLEND\n");
    if (format & SVGA3DFORMAT_OP_AUTOGENMIPMAP)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_AUTOGENMIPMAP\n");
    if (format & SVGA3DFORMAT_OP_VERTEXTEXTURE)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_VERTEXTEXTURE\n");
    if (format & SVGA3DFORMAT_OP_NOTEXCOORDWRAPNORMIP)
        strcat(szFormat, "   - SVGA3DFORMAT_OP_NOTEXCOORDWRAPNORMIP\n");
   return szFormat;
}

const char *vmsvga3dGetRenderStateName(uint32_t state)
{
    switch (state)
    {
    case SVGA3D_RS_ZENABLE:                /* SVGA3dBool */
        return "SVGA3D_RS_ZENABLE";
    case SVGA3D_RS_ZWRITEENABLE:           /* SVGA3dBool */
        return "SVGA3D_RS_ZWRITEENABLE";
    case SVGA3D_RS_ALPHATESTENABLE:        /* SVGA3dBool */
        return "SVGA3D_RS_ALPHATESTENABLE";
    case SVGA3D_RS_DITHERENABLE:           /* SVGA3dBool */
        return "SVGA3D_RS_DITHERENABLE";
    case SVGA3D_RS_BLENDENABLE:            /* SVGA3dBool */
        return "SVGA3D_RS_BLENDENABLE";
    case SVGA3D_RS_FOGENABLE:              /* SVGA3dBool */
        return "SVGA3D_RS_FOGENABLE";
    case SVGA3D_RS_SPECULARENABLE:         /* SVGA3dBool */
        return "SVGA3D_RS_SPECULARENABLE";
    case SVGA3D_RS_STENCILENABLE:          /* SVGA3dBool */
        return "SVGA3D_RS_STENCILENABLE";
    case SVGA3D_RS_LIGHTINGENABLE:         /* SVGA3dBool */
        return "SVGA3D_RS_LIGHTINGENABLE";
    case SVGA3D_RS_NORMALIZENORMALS:       /* SVGA3dBool */
        return "SVGA3D_RS_NORMALIZENORMALS";
    case SVGA3D_RS_POINTSPRITEENABLE:      /* SVGA3dBool */
        return "SVGA3D_RS_POINTSPRITEENABLE";
    case SVGA3D_RS_POINTSCALEENABLE:       /* SVGA3dBool */
        return "SVGA3D_RS_POINTSCALEENABLE";
    case SVGA3D_RS_STENCILREF:             /* uint32_t */
        return "SVGA3D_RS_STENCILREF";
    case SVGA3D_RS_STENCILMASK:            /* uint32_t */
        return "SVGA3D_RS_STENCILMASK";
    case SVGA3D_RS_STENCILWRITEMASK:       /* uint32_t */
        return "SVGA3D_RS_STENCILWRITEMASK";
    case SVGA3D_RS_POINTSIZE:              /* float */
        return "SVGA3D_RS_POINTSIZE";
    case SVGA3D_RS_POINTSIZEMIN:           /* float */
        return "SVGA3D_RS_POINTSIZEMIN";
    case SVGA3D_RS_POINTSIZEMAX:           /* float */
        return "SVGA3D_RS_POINTSIZEMAX";
    case SVGA3D_RS_POINTSCALE_A:           /* float */
        return "SVGA3D_RS_POINTSCALE_A";
    case SVGA3D_RS_POINTSCALE_B:           /* float */
        return "SVGA3D_RS_POINTSCALE_B";
    case SVGA3D_RS_POINTSCALE_C:           /* float */
        return "SVGA3D_RS_POINTSCALE_C";
    case SVGA3D_RS_AMBIENT:                /* SVGA3dColor - identical */
        return "SVGA3D_RS_AMBIENT";
    case SVGA3D_RS_CLIPPLANEENABLE:        /* SVGA3dClipPlanes - identical */
        return "SVGA3D_RS_CLIPPLANEENABLE";
    case SVGA3D_RS_FOGCOLOR:               /* SVGA3dColor - identical */
        return "SVGA3D_RS_FOGCOLOR";
    case SVGA3D_RS_FOGSTART:               /* float */
        return "SVGA3D_RS_FOGSTART";
    case SVGA3D_RS_FOGEND:                 /* float */
        return "SVGA3D_RS_FOGEND";
    case SVGA3D_RS_FOGDENSITY:             /* float */
        return "SVGA3D_RS_FOGDENSITY";
    case SVGA3D_RS_RANGEFOGENABLE:         /* SVGA3dBool */
        return "SVGA3D_RS_RANGEFOGENABLE";
    case SVGA3D_RS_FOGMODE:                /* SVGA3dFogMode */
        return "SVGA3D_RS_FOGMODE";
    case SVGA3D_RS_FILLMODE:               /* SVGA3dFillMode */
        return "SVGA3D_RS_FILLMODE";
    case SVGA3D_RS_SHADEMODE:              /* SVGA3dShadeMode */
        return "SVGA3D_RS_SHADEMODE";
    case SVGA3D_RS_LINEPATTERN:            /* SVGA3dLinePattern */
        return "SVGA3D_RS_LINEPATTERN";
    case SVGA3D_RS_SRCBLEND:               /* SVGA3dBlendOp */
        return "SVGA3D_RS_SRCBLEND";
    case SVGA3D_RS_DSTBLEND:               /* SVGA3dBlendOp */
        return "SVGA3D_RS_DSTBLEND";
    case SVGA3D_RS_BLENDEQUATION:          /* SVGA3dBlendEquation */
        return "SVGA3D_RS_BLENDEQUATION";
    case SVGA3D_RS_CULLMODE:               /* SVGA3dFace */
        return "SVGA3D_RS_CULLMODE";
    case SVGA3D_RS_ZFUNC:                  /* SVGA3dCmpFunc */
        return "SVGA3D_RS_ZFUNC";
    case SVGA3D_RS_ALPHAFUNC:              /* SVGA3dCmpFunc */
        return "SVGA3D_RS_ALPHAFUNC";
    case SVGA3D_RS_STENCILFUNC:            /* SVGA3dCmpFunc */
        return "SVGA3D_RS_STENCILFUNC";
    case SVGA3D_RS_STENCILFAIL:            /* SVGA3dStencilOp */
        return "SVGA3D_RS_STENCILFAIL";
    case SVGA3D_RS_STENCILZFAIL:           /* SVGA3dStencilOp */
        return "SVGA3D_RS_STENCILZFAIL";
    case SVGA3D_RS_STENCILPASS:            /* SVGA3dStencilOp */
        return "SVGA3D_RS_STENCILPASS";
    case SVGA3D_RS_ALPHAREF:               /* float (0.0 .. 1.0) */
        return "SVGA3D_RS_ALPHAREF";
    case SVGA3D_RS_FRONTWINDING:           /* SVGA3dFrontWinding */
        return "SVGA3D_RS_FRONTWINDING";
    case SVGA3D_RS_COORDINATETYPE:         /* SVGA3dCoordinateType */
        return "SVGA3D_RS_COORDINATETYPE";
    case SVGA3D_RS_ZBIAS:                  /* float */
        return "SVGA3D_RS_ZBIAS";
    case SVGA3D_RS_COLORWRITEENABLE:       /* SVGA3dColorMask */
        return "SVGA3D_RS_COLORWRITEENABLE";
    case SVGA3D_RS_VERTEXMATERIALENABLE:   /* SVGA3dBool */
        return "SVGA3D_RS_VERTEXMATERIALENABLE";
    case SVGA3D_RS_DIFFUSEMATERIALSOURCE:  /* SVGA3dVertexMaterial */
        return "SVGA3D_RS_DIFFUSEMATERIALSOURCE";
    case SVGA3D_RS_SPECULARMATERIALSOURCE: /* SVGA3dVertexMaterial */
        return "SVGA3D_RS_SPECULARMATERIALSOURCE";
    case SVGA3D_RS_AMBIENTMATERIALSOURCE:  /* SVGA3dVertexMaterial */
        return "SVGA3D_RS_AMBIENTMATERIALSOURCE";
    case SVGA3D_RS_EMISSIVEMATERIALSOURCE: /* SVGA3dVertexMaterial */
        return "SVGA3D_RS_EMISSIVEMATERIALSOURCE";
    case SVGA3D_RS_TEXTUREFACTOR:          /* SVGA3dColor */
        return "SVGA3D_RS_TEXTUREFACTOR";
    case SVGA3D_RS_LOCALVIEWER:            /* SVGA3dBool */
        return "SVGA3D_RS_LOCALVIEWER";
    case SVGA3D_RS_SCISSORTESTENABLE:      /* SVGA3dBool */
        return "SVGA3D_RS_SCISSORTESTENABLE";
    case SVGA3D_RS_BLENDCOLOR:             /* SVGA3dColor */
        return "SVGA3D_RS_BLENDCOLOR";
    case SVGA3D_RS_STENCILENABLE2SIDED:    /* SVGA3dBool */
        return "SVGA3D_RS_STENCILENABLE2SIDED";
    case SVGA3D_RS_CCWSTENCILFUNC:         /* SVGA3dCmpFunc */
        return "SVGA3D_RS_CCWSTENCILFUNC";
    case SVGA3D_RS_CCWSTENCILFAIL:         /* SVGA3dStencilOp */
        return "SVGA3D_RS_CCWSTENCILFAIL";
    case SVGA3D_RS_CCWSTENCILZFAIL:        /* SVGA3dStencilOp */
        return "SVGA3D_RS_CCWSTENCILZFAIL";
    case SVGA3D_RS_CCWSTENCILPASS:         /* SVGA3dStencilOp */
        return "SVGA3D_RS_CCWSTENCILPASS";
    case SVGA3D_RS_VERTEXBLEND:            /* SVGA3dVertexBlendFlags */
        return "SVGA3D_RS_VERTEXBLEND";
    case SVGA3D_RS_SLOPESCALEDEPTHBIAS:    /* float */
        return "SVGA3D_RS_SLOPESCALEDEPTHBIAS";
    case SVGA3D_RS_DEPTHBIAS:              /* float */
        return "SVGA3D_RS_DEPTHBIAS";
    case SVGA3D_RS_OUTPUTGAMMA:            /* float */
        return "SVGA3D_RS_OUTPUTGAMMA";
    case SVGA3D_RS_ZVISIBLE:               /* SVGA3dBool */
        return "SVGA3D_RS_ZVISIBLE";
    case SVGA3D_RS_LASTPIXEL:              /* SVGA3dBool */
        return "SVGA3D_RS_LASTPIXEL";
    case SVGA3D_RS_CLIPPING:               /* SVGA3dBool */
        return "SVGA3D_RS_CLIPPING";
    case SVGA3D_RS_WRAP0:                  /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP0";
    case SVGA3D_RS_WRAP1:                  /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP1";
    case SVGA3D_RS_WRAP2:                  /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP2";
    case SVGA3D_RS_WRAP3:                  /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP3";
    case SVGA3D_RS_WRAP4:                  /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP4";
    case SVGA3D_RS_WRAP5:                  /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP5";
    case SVGA3D_RS_WRAP6:                  /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP6";
    case SVGA3D_RS_WRAP7:                  /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP7";
    case SVGA3D_RS_WRAP8:                  /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP8";
    case SVGA3D_RS_WRAP9:                  /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP9";
    case SVGA3D_RS_WRAP10:                 /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP10";
    case SVGA3D_RS_WRAP11:                 /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP11";
    case SVGA3D_RS_WRAP12:                 /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP12";
    case SVGA3D_RS_WRAP13:                 /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP13";
    case SVGA3D_RS_WRAP14:                 /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP14";
    case SVGA3D_RS_WRAP15:                 /* SVGA3dWrapFlags */
        return "SVGA3D_RS_WRAP15";
    case SVGA3D_RS_MULTISAMPLEANTIALIAS:   /* SVGA3dBool */
        return "SVGA3D_RS_MULTISAMPLEANTIALIAS";
    case SVGA3D_RS_MULTISAMPLEMASK:        /* uint32_t */
        return "SVGA3D_RS_MULTISAMPLEMASK";
    case SVGA3D_RS_INDEXEDVERTEXBLENDENABLE: /* SVGA3dBool */
        return "SVGA3D_RS_INDEXEDVERTEXBLENDENABLE";
    case SVGA3D_RS_TWEENFACTOR:            /* float */
        return "SVGA3D_RS_TWEENFACTOR";
    case SVGA3D_RS_ANTIALIASEDLINEENABLE:  /* SVGA3dBool */
        return "SVGA3D_RS_ANTIALIASEDLINEENABLE";
    case SVGA3D_RS_COLORWRITEENABLE1:      /* SVGA3dColorMask */
        return "SVGA3D_RS_COLORWRITEENABLE1";
    case SVGA3D_RS_COLORWRITEENABLE2:      /* SVGA3dColorMask */
        return "SVGA3D_RS_COLORWRITEENABLE2";
    case SVGA3D_RS_COLORWRITEENABLE3:      /* SVGA3dColorMask */
        return "SVGA3D_RS_COLORWRITEENABLE3";
    case SVGA3D_RS_SEPARATEALPHABLENDENABLE: /* SVGA3dBool */
        return "SVGA3D_RS_SEPARATEALPHABLENDENABLE";
    case SVGA3D_RS_SRCBLENDALPHA:          /* SVGA3dBlendOp */
        return "SVGA3D_RS_SRCBLENDALPHA";
    case SVGA3D_RS_DSTBLENDALPHA:          /* SVGA3dBlendOp */
        return "SVGA3D_RS_DSTBLENDALPHA";
    case SVGA3D_RS_BLENDEQUATIONALPHA:     /* SVGA3dBlendEquation */
        return "SVGA3D_RS_BLENDEQUATIONALPHA";
    case SVGA3D_RS_TRANSPARENCYANTIALIAS:  /* SVGA3dTransparencyAntialiasType */
        return "SVGA3D_RS_TRANSPARENCYANTIALIAS";
    case SVGA3D_RS_LINEAA:                 /* SVGA3dBool */
        return "SVGA3D_RS_LINEAA";
    case SVGA3D_RS_LINEWIDTH:              /* float */
        return "SVGA3D_RS_LINEWIDTH";
    default:
        return "UNKNOWN";
    }
}

const char *vmsvga3dTextureStateToString(SVGA3dTextureStateName textureState)
{
    switch (textureState)
    {
    case SVGA3D_TS_BIND_TEXTURE:
        return "SVGA3D_TS_BIND_TEXTURE";
    case SVGA3D_TS_COLOROP:
        return "SVGA3D_TS_COLOROP";
    case SVGA3D_TS_COLORARG1:
        return "SVGA3D_TS_COLORARG1";
    case SVGA3D_TS_COLORARG2:
        return "SVGA3D_TS_COLORARG2";
    case SVGA3D_TS_ALPHAOP:
        return "SVGA3D_TS_ALPHAOP";
    case SVGA3D_TS_ALPHAARG1:
        return "SVGA3D_TS_ALPHAARG1";
    case SVGA3D_TS_ALPHAARG2:
        return "SVGA3D_TS_ALPHAARG2";
    case SVGA3D_TS_ADDRESSU:
        return "SVGA3D_TS_ADDRESSU";
    case SVGA3D_TS_ADDRESSV:
        return "SVGA3D_TS_ADDRESSV";
    case SVGA3D_TS_MIPFILTER:
        return "SVGA3D_TS_MIPFILTER";
    case SVGA3D_TS_MAGFILTER:
        return "SVGA3D_TS_MAGFILTER";
    case SVGA3D_TS_MINFILTER:
        return "SVGA3D_TS_MINFILTER";
    case SVGA3D_TS_BORDERCOLOR:
        return "SVGA3D_TS_BORDERCOLOR";
    case SVGA3D_TS_TEXCOORDINDEX:
        return "SVGA3D_TS_TEXCOORDINDEX";
    case SVGA3D_TS_TEXTURETRANSFORMFLAGS:
        return "SVGA3D_TS_TEXTURETRANSFORMFLAGS";
    case SVGA3D_TS_TEXCOORDGEN:
        return "SVGA3D_TS_TEXCOORDGEN";
    case SVGA3D_TS_BUMPENVMAT00:
        return "SVGA3D_TS_BUMPENVMAT00";
    case SVGA3D_TS_BUMPENVMAT01:
        return "SVGA3D_TS_BUMPENVMAT01";
    case SVGA3D_TS_BUMPENVMAT10:
        return "SVGA3D_TS_BUMPENVMAT10";
    case SVGA3D_TS_BUMPENVMAT11:
        return "SVGA3D_TS_BUMPENVMAT11";
    case SVGA3D_TS_TEXTURE_MIPMAP_LEVEL:
        return "SVGA3D_TS_TEXTURE_MIPMAP_LEVEL";
    case SVGA3D_TS_TEXTURE_LOD_BIAS:
        return "SVGA3D_TS_TEXTURE_LOD_BIAS";
    case SVGA3D_TS_TEXTURE_ANISOTROPIC_LEVEL:
        return "SVGA3D_TS_TEXTURE_ANISOTROPIC_LEVEL";
    case SVGA3D_TS_ADDRESSW:
        return "SVGA3D_TS_ADDRESSW";
    case SVGA3D_TS_GAMMA:
        return "SVGA3D_TS_GAMMA";
    case SVGA3D_TS_BUMPENVLSCALE:
        return "SVGA3D_TS_BUMPENVLSCALE";
    case SVGA3D_TS_BUMPENVLOFFSET:
        return "SVGA3D_TS_BUMPENVLOFFSET";
    case SVGA3D_TS_COLORARG0:
        return "SVGA3D_TS_COLORARG0";
    case SVGA3D_TS_ALPHAARG0:
        return "SVGA3D_TS_ALPHAARG0";
    default:
        return "UNKNOWN";
    }
}

const char *vmsvgaTransformToString(SVGA3dTransformType type)
{
    switch (type)
    {
    case SVGA3D_TRANSFORM_INVALID:
        return "SVGA3D_TRANSFORM_INVALID";
    case SVGA3D_TRANSFORM_WORLD:
        return "SVGA3D_TRANSFORM_WORLD";
    case SVGA3D_TRANSFORM_VIEW:
        return "SVGA3D_TRANSFORM_VIEW";
    case SVGA3D_TRANSFORM_PROJECTION:
        return "SVGA3D_TRANSFORM_PROJECTION";
    case SVGA3D_TRANSFORM_TEXTURE0:
        return "SVGA3D_TRANSFORM_TEXTURE0";
    case SVGA3D_TRANSFORM_TEXTURE1:
        return "SVGA3D_TRANSFORM_TEXTURE1";
    case SVGA3D_TRANSFORM_TEXTURE2:
        return "SVGA3D_TRANSFORM_TEXTURE2";
    case SVGA3D_TRANSFORM_TEXTURE3:
        return "SVGA3D_TRANSFORM_TEXTURE3";
    case SVGA3D_TRANSFORM_TEXTURE4:
        return "SVGA3D_TRANSFORM_TEXTURE4";
    case SVGA3D_TRANSFORM_TEXTURE5:
        return "SVGA3D_TRANSFORM_TEXTURE5";
    case SVGA3D_TRANSFORM_TEXTURE6:
        return "SVGA3D_TRANSFORM_TEXTURE6";
    case SVGA3D_TRANSFORM_TEXTURE7:
        return "SVGA3D_TRANSFORM_TEXTURE7";
    case SVGA3D_TRANSFORM_WORLD1:
        return "SVGA3D_TRANSFORM_WORLD1";
    case SVGA3D_TRANSFORM_WORLD2:
        return "SVGA3D_TRANSFORM_WORLD2";
    case SVGA3D_TRANSFORM_WORLD3:
        return "SVGA3D_TRANSFORM_WORLD3";
    default:
        return "UNKNOWN";
    }
}

const char *vmsvgaDeclUsage2String(SVGA3dDeclUsage usage)
{
    switch (usage)
    {
    case SVGA3D_DECLUSAGE_POSITION:
        return "SVGA3D_DECLUSAGE_POSITION";
    case SVGA3D_DECLUSAGE_BLENDWEIGHT:
        return "SVGA3D_DECLUSAGE_BLENDWEIGHT";
    case SVGA3D_DECLUSAGE_BLENDINDICES:
        return "SVGA3D_DECLUSAGE_BLENDINDICES";
    case SVGA3D_DECLUSAGE_NORMAL:
        return "SVGA3D_DECLUSAGE_NORMAL";
    case SVGA3D_DECLUSAGE_PSIZE:
        return "SVGA3D_DECLUSAGE_PSIZE";
    case SVGA3D_DECLUSAGE_TEXCOORD:
        return "SVGA3D_DECLUSAGE_TEXCOORD";
    case SVGA3D_DECLUSAGE_TANGENT:
        return "SVGA3D_DECLUSAGE_TANGENT";
    case SVGA3D_DECLUSAGE_BINORMAL:
        return "SVGA3D_DECLUSAGE_BINORMAL";
    case SVGA3D_DECLUSAGE_TESSFACTOR:
        return "SVGA3D_DECLUSAGE_TESSFACTOR";
    case SVGA3D_DECLUSAGE_POSITIONT:
        return "SVGA3D_DECLUSAGE_POSITIONT";
    case SVGA3D_DECLUSAGE_COLOR:
        return "SVGA3D_DECLUSAGE_COLOR";
    case SVGA3D_DECLUSAGE_FOG:
        return "SVGA3D_DECLUSAGE_FOG";
    case SVGA3D_DECLUSAGE_DEPTH:
        return "SVGA3D_DECLUSAGE_DEPTH";
    case SVGA3D_DECLUSAGE_SAMPLE:
        return "SVGA3D_DECLUSAGE_SAMPLE";
    default:
        return "UNKNOWN!!";
    }
}

const char *vmsvgaDeclMethod2String(SVGA3dDeclMethod method)
{
    switch (method)
    {
    case SVGA3D_DECLMETHOD_DEFAULT:
        return "SVGA3D_DECLMETHOD_DEFAULT";
    case SVGA3D_DECLMETHOD_PARTIALU:
        return "SVGA3D_DECLMETHOD_PARTIALU";
    case SVGA3D_DECLMETHOD_PARTIALV:
        return "SVGA3D_DECLMETHOD_PARTIALV";
    case SVGA3D_DECLMETHOD_CROSSUV:
        return "SVGA3D_DECLMETHOD_CROSSUV";
    case SVGA3D_DECLMETHOD_UV:
        return "SVGA3D_DECLMETHOD_UV";
    case SVGA3D_DECLMETHOD_LOOKUP:
        return "SVGA3D_DECLMETHOD_LOOKUP";
    case SVGA3D_DECLMETHOD_LOOKUPPRESAMPLED:
        return "SVGA3D_DECLMETHOD_LOOKUPPRESAMPLED";
    default:
        return "UNKNOWN!!";
    }
}

const char *vmsvgaDeclType2String(SVGA3dDeclType type)
{
    switch (type)
    {
    case SVGA3D_DECLTYPE_FLOAT1:
        return "SVGA3D_DECLTYPE_FLOAT1";
    case SVGA3D_DECLTYPE_FLOAT2:
        return "SVGA3D_DECLTYPE_FLOAT2";
    case SVGA3D_DECLTYPE_FLOAT3:
        return "SVGA3D_DECLTYPE_FLOAT3";
    case SVGA3D_DECLTYPE_FLOAT4:
        return "SVGA3D_DECLTYPE_FLOAT4";
    case SVGA3D_DECLTYPE_D3DCOLOR:
        return "SVGA3D_DECLTYPE_D3DCOLOR";
    case SVGA3D_DECLTYPE_UBYTE4:
        return "SVGA3D_DECLTYPE_UBYTE4";
    case SVGA3D_DECLTYPE_SHORT2:
        return "SVGA3D_DECLTYPE_SHORT2";
    case SVGA3D_DECLTYPE_SHORT4:
        return "SVGA3D_DECLTYPE_SHORT4";
    case SVGA3D_DECLTYPE_UBYTE4N:
        return "SVGA3D_DECLTYPE_UBYTE4N";
    case SVGA3D_DECLTYPE_SHORT2N:
        return "SVGA3D_DECLTYPE_SHORT2N";
    case SVGA3D_DECLTYPE_SHORT4N:
        return "SVGA3D_DECLTYPE_SHORT4N";
    case SVGA3D_DECLTYPE_USHORT2N:
        return "SVGA3D_DECLTYPE_USHORT2N";
    case SVGA3D_DECLTYPE_USHORT4N:
        return "SVGA3D_DECLTYPE_USHORT4N";
    case SVGA3D_DECLTYPE_UDEC3:
        return "SVGA3D_DECLTYPE_UDEC3";
    case SVGA3D_DECLTYPE_DEC3N:
        return "SVGA3D_DECLTYPE_DEC3N";
    case SVGA3D_DECLTYPE_FLOAT16_2:
        return "SVGA3D_DECLTYPE_FLOAT16_2";
    case SVGA3D_DECLTYPE_FLOAT16_4:
        return "SVGA3D_DECLTYPE_FLOAT16_4";
    default:
        return "UNKNOWN!!";
    }
}

const char *vmsvgaSurfaceType2String(SVGA3dSurfaceFormat format)
{
    switch (format)
    {
    case SVGA3D_X8R8G8B8:
        return "SVGA3D_X8R8G8B8";
    case SVGA3D_A8R8G8B8:
        return "SVGA3D_A8R8G8B8";
    case SVGA3D_R5G6B5:
        return "SVGA3D_R5G6B5";
    case SVGA3D_X1R5G5B5:
        return "SVGA3D_X1R5G5B5";
    case SVGA3D_A1R5G5B5:
        return "SVGA3D_A1R5G5B5";
    case SVGA3D_A4R4G4B4:
        return "SVGA3D_A4R4G4B4";
    case SVGA3D_Z_D32:
        return "SVGA3D_Z_D32";
    case SVGA3D_Z_D16:
        return "SVGA3D_Z_D16";
    case SVGA3D_Z_D24S8:
        return "SVGA3D_Z_D24S8";
    case SVGA3D_Z_D15S1:
        return "SVGA3D_Z_D15S1";
    case SVGA3D_Z_D24X8:
        return "SVGA3D_Z_D24X8";
    case SVGA3D_Z_DF16:
        return "SVGA3D_Z_DF16";
    case SVGA3D_Z_DF24:
        return "SVGA3D_Z_DF24";
    case SVGA3D_Z_D24S8_INT:
        return "SVGA3D_Z_D24S8_INT";
    case SVGA3D_LUMINANCE8:
        return "SVGA3D_LUMINANCE8";
    case SVGA3D_LUMINANCE4_ALPHA4:
        return "SVGA3D_LUMINANCE4_ALPHA4";
    case SVGA3D_LUMINANCE16:
        return "SVGA3D_LUMINANCE16";
    case SVGA3D_LUMINANCE8_ALPHA8:
        return "SVGA3D_LUMINANCE8_ALPHA8";
    case SVGA3D_DXT1:
        return "SVGA3D_DXT1";
    case SVGA3D_DXT2:
        return "SVGA3D_DXT2";
    case SVGA3D_DXT3:
        return "SVGA3D_DXT3";
    case SVGA3D_DXT4:
        return "SVGA3D_DXT4";
    case SVGA3D_DXT5:
        return "SVGA3D_DXT5";
    case SVGA3D_BUMPU8V8:
        return "SVGA3D_BUMPU8V8";
    case SVGA3D_BUMPL6V5U5:
        return "SVGA3D_BUMPL6V5U5";
    case SVGA3D_BUMPX8L8V8U8:
        return "SVGA3D_BUMPX8L8V8U8";
    case SVGA3D_BUMPL8V8U8:
        return "SVGA3D_BUMPL8V8U8";
    case SVGA3D_V8U8:
        return "SVGA3D_V8U8";
    case SVGA3D_Q8W8V8U8:
        return "SVGA3D_Q8W8V8U8";
    case SVGA3D_CxV8U8:
        return "SVGA3D_CxV8U8";
    case SVGA3D_X8L8V8U8:
        return "SVGA3D_X8L8V8U8";
    case SVGA3D_A2W10V10U10:
        return "SVGA3D_A2W10V10U10";
    case SVGA3D_ARGB_S10E5:
        return "SVGA3D_ARGB_S10E5";
    case SVGA3D_ARGB_S23E8:
        return "SVGA3D_ARGB_S23E8";
    case SVGA3D_A2R10G10B10:
        return "SVGA3D_A2R10G10B10";
    case SVGA3D_ALPHA8:
        return "SVGA3D_ALPHA8";
    case SVGA3D_R_S10E5:
        return "SVGA3D_R_S10E5";
    case SVGA3D_R_S23E8:
        return "SVGA3D_R_S23E8";
    case SVGA3D_RG_S10E5:
        return "SVGA3D_RG_S10E5";
    case SVGA3D_RG_S23E8:
        return "SVGA3D_RG_S23E8";
    case SVGA3D_BUFFER:
        return "SVGA3D_BUFFER";
    case SVGA3D_V16U16:
        return "SVGA3D_V16U16";
    case SVGA3D_G16R16:
        return "SVGA3D_G16R16";
    case SVGA3D_A16B16G16R16:
        return "SVGA3D_A16B16G16R16";
    case SVGA3D_UYVY:
        return "SVGA3D_UYVY";
    case SVGA3D_YUY2:
        return "SVGA3D_YUY2";
    case SVGA3D_NV12:
        return "SVGA3D_NV12";
    case SVGA3D_AYUV:
        return "SVGA3D_AYUV";
    case SVGA3D_BC4_UNORM:
        return "SVGA3D_BC4_UNORM";
    case SVGA3D_BC5_UNORM:
        return "SVGA3D_BC5_UNORM";
    }
    return "UNKNOWN!!";
}

const char *vmsvga3dPrimitiveType2String(SVGA3dPrimitiveType PrimitiveType)
{
    switch (PrimitiveType)
    {
    case SVGA3D_PRIMITIVE_TRIANGLELIST:
        return "SVGA3D_PRIMITIVE_TRIANGLELIST";
    case SVGA3D_PRIMITIVE_POINTLIST:
        return "SVGA3D_PRIMITIVE_POINTLIST";
    case SVGA3D_PRIMITIVE_LINELIST:
        return "SVGA3D_PRIMITIVE_LINELIST";
    case SVGA3D_PRIMITIVE_LINESTRIP:
        return "SVGA3D_PRIMITIVE_LINESTRIP";
    case SVGA3D_PRIMITIVE_TRIANGLESTRIP:
        return "SVGA3D_PRIMITIVE_TRIANGLESTRIP";
    case SVGA3D_PRIMITIVE_TRIANGLEFAN:
        return "SVGA3D_PRIMITIVE_TRIANGLEFAN";
    default:
        return "UNKNOWN";
    }
}

#endif /* LOG_ENABLED */

