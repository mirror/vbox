/* $Id$ */

/** @file
 * VBoxVideo Display D3D User mode dll
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxDispDbg_h__
#define ___VBoxDispDbg_h__

#ifdef DEBUG
/* debugging configuration flags */

/* generic debugging facilities & extra data checks */
# define VBOXWDDMDISP_DEBUG
# if defined(DEBUG_misha) || defined(DEBUG_leo)
/* for some reason when debugging with VirtualKD, user-mode DbgPrint's are discarded
 * the workaround so far is to pass the log info to the kernel driver and DbgPrint'ed from there,
 * which is enabled by this define */
#  define VBOXWDDMDISP_DEBUG_PRINTDRV
/* use OutputDebugString */
#  define VBOXWDDMDISP_DEBUG_PRINT
/* adds vectored exception handler to be able to catch non-debug UM exceptions in kernel debugger */
#  define VBOXWDDMDISP_DEBUG_VEHANDLER
# endif
#endif

#if 0
# ifdef Assert
#  undef Assert
#  define Assert(_a) do{}while(0)
# endif
# ifdef AssertBreakpoint
#  undef AssertBreakpoint
#  define AssertBreakpoint() do{}while(0)
# endif
# ifdef AssertFailed
#  undef AssertFailed
#  define AssertFailed() do{}while(0)
# endif
#endif

#ifdef VBOXWDDMDISP_DEBUG_VEHANDLER
void vboxVDbgVEHandlerRegister();
void vboxVDbgVEHandlerUnregister();
#endif

#ifdef VBOXWDDMDISP_DEBUG_PRINTDRV
# define DbgPrintDrv(_m) do { vboxDispLogDrvF _m; } while (0)
# define DbgPrintDrvRel(_m) do { vboxDispLogDrvF _m; } while (0)
# define DbgPrintDrvFlow(_m) do { } while (0)
#else
# define DbgPrintDrv(_m) do { } while (0)
# define DbgPrintDrvRel(_m) do { } while (0)
# define DbgPrintDrvFlow(_m) do { } while (0)
#endif

#ifdef VBOXWDDMDISP_DEBUG_PRINT
# define DbgPrintUsr(_m) do { vboxDispLogDbgPrintF _m; } while (0)
# define DbgPrintUsrRel(_m) do { vboxDispLogDbgPrintF _m; } while (0)
# define DbgPrintUsrFlow(_m) do { } while (0)
#else
# define DbgPrintUsr(_m) do { } while (0)
# define DbgPrintUsrRel(_m) do { } while (0)
# define DbgPrintUsrFlow(_m) do { } while (0)
#endif
#ifdef DEBUG_misha
# define WARN_BREAK() do { AssertFailed(); } while (0)
#else
# define WARN_BREAK() do { } while (0)
#endif
#define WARN(_m) do { \
        Log(_m); \
        DbgPrintUsr(_m); \
        DbgPrintDrv(_m); \
        WARN_BREAK(); \
    } while (0)
#define vboxVDbgPrint(_m) do { \
        Log(_m); \
        DbgPrintUsr(_m); \
        DbgPrintDrv(_m); \
    } while (0)
#define vboxVDbgPrintF(_m)  do { \
        LogFlow(_m); \
        DbgPrintUsrFlow(_m); \
        DbgPrintDrvFlow(_m); \
    } while (0)
#define vboxVDbgPrintR(_m)  do { \
        LogRel(_m); \
        DbgPrintUsrRel(_m); \
        DbgPrintDrvRel(_m); \
    } while (0)

#ifdef VBOXWDDMDISP_DEBUG
extern DWORD g_VBoxVDbgFDumpSetTexture;
extern DWORD g_VBoxVDbgFDumpDrawPrim;
extern DWORD g_VBoxVDbgFDumpTexBlt;
extern DWORD g_VBoxVDbgFDumpBlt;
extern DWORD g_VBoxVDbgFDumpRtSynch;
extern DWORD g_VBoxVDbgFDumpFlush;
extern DWORD g_VBoxVDbgFDumpShared;

void vboxDispLogDrvF(char * szString, ...);
void vboxDispLogDrv(char * szString);
void vboxDispLogDbgPrintF(char * szString, ...);

typedef struct VBOXWDDMDISP_ALLOCATION *PVBOXWDDMDISP_ALLOCATION;
typedef struct VBOXWDDMDISP_RESOURCE *PVBOXWDDMDISP_RESOURCE;

VOID vboxVDbgDoDumpSurfRectByAlloc(const char * pPrefix, PVBOXWDDMDISP_ALLOCATION pAlloc, const RECT *pRect, const char* pSuffix);
VOID vboxVDbgDoDumpAllocRect(const char * pPrefix, PVBOXWDDMDISP_ALLOCATION pAlloc, const RECT *pRect, const char* pSuffix);
VOID vboxVDbgDoDumpSurfRectByRc(const char * pPrefix, const PVBOXWDDMDISP_RESOURCE pRc, uint32_t iAlloc, const RECT *pRect, const char* pSuffix);
VOID vboxVDbgDoDumpSurfRect(const char * pPrefix, IDirect3DSurface9 *pSurf, const RECT *pRect, const char * pSuffix, bool bBreak);
VOID vboxVDbgDoDumpSurf(const char * pPrefix, IDirect3DSurface9 *pSurf, const char * pSuffix);
VOID vboxVDbgDoDumpRcRect(const char * pPrefix, IDirect3DResource9 *pRc, const RECT *pRect, const char * pSuffix);
VOID vboxVDbgDoDumpRcRectByRc(const char * pPrefix, const PVBOXWDDMDISP_RESOURCE pRc, const RECT *pRect, const char* pSuffix);
VOID vboxVDbgDoDumpTex(const char * pPrefix, IDirect3DBaseTexture9 *pTexBase, const char * pSuffix);
VOID vboxVDbgDoDumpRt(const char * pPrefix, IDirect3DDevice9 *pDevice, const char * pSuffix);

void vboxVDbgDoPrintRect(const char * pPrefix, const RECT *pRect, const char * pSuffix);
void vboxVDbgDoPrintAlloc(const char * pPrefix, const PVBOXWDDMDISP_RESOURCE pRc, uint32_t iAlloc, const char * pSuffix);
VOID vboxVDbgDoDumpRtData(char * pPrefix, IDirect3DDevice9 *pDevice, char * pSuffix);

extern DWORD g_VBoxVDbgPid;
#define VBOXVDBG_IS_PID(_pid) ((_pid) == (g_VBoxVDbgPid ? g_VBoxVDbgPid : (g_VBoxVDbgPid = GetCurrentProcessId())))
#define VBOXVDBG_IS_DUMP_ALLOWED_PID(_pid) (((int)(_pid)) > 0 ? VBOXVDBG_IS_PID(_pid) : !VBOXVDBG_IS_PID(-((int)(_pid))))

#define VBOXVDBG_IS_DUMP_ALLOWED(_type) ( \
        g_VBoxVDbgFDump##_type \
        && (g_VBoxVDbgFDump##_type == 1 \
                || VBOXVDBG_IS_DUMP_ALLOWED_PID(g_VBoxVDbgFDump##_type) \
           ) \
        )

#define VBOXVDBG_IS_DUMP_SHARED_ALLOWED(_pRc) (\
        (_pRc)->RcDesc.fFlags.SharedResource \
        && VBOXVDBG_IS_DUMP_ALLOWED(Shared) \
        )

#define VBOXVDBG_DUMP_DRAWPRIM_ENTER(_pDevice) do { \
        if (VBOXVDBG_IS_DUMP_ALLOWED(DrawPrim)) \
        { \
            vboxVDbgDoDumpRt("==>"__FUNCTION__": RenderTarget Dump\n", (_pDevice), "\n"); \
        }\
    } while (0)

#define VBOXVDBG_DUMP_DRAWPRIM_LEAVE(_pDevice) do { \
        if (VBOXVDBG_IS_DUMP_ALLOWED(DrawPrim)) \
        { \
            vboxVDbgDoDumpRt("<=="__FUNCTION__": RenderTarget Dump\n", (_pDevice), "\n"); \
        }\
    } while (0)

#define VBOXVDBG_DUMP_SETTEXTURE(_pRc) do { \
        if (VBOXVDBG_IS_DUMP_ALLOWED(SetTexture) \
                || VBOXVDBG_IS_DUMP_SHARED_ALLOWED(_pRc) \
                ) \
        { \
            vboxVDbgDoDumpRcRectByRc("== "__FUNCTION__": Texture Dump\n", _pRc, NULL, "\n"); \
        } \
    } while (0)

#define VBOXVDBG_DUMP_TEXBLT_ENTER(_pSrcRc, _pSrcRect, _pDstRc, _pDstPoint) do { \
        if (VBOXVDBG_IS_DUMP_ALLOWED(TexBlt) \
                || VBOXVDBG_IS_DUMP_SHARED_ALLOWED(_pSrcRc) \
                || VBOXVDBG_IS_DUMP_SHARED_ALLOWED(_pDstRc) \
                ) \
        { \
            RECT _DstRect; \
            vboxWddmRectMoved(&_DstRect, (_pSrcRect), (_pDstPoint)->x, (_pDstPoint)->y); \
            vboxVDbgDoDumpRcRectByRc("==>"__FUNCTION__" Src:\n", (_pSrcRc), (_pSrcRect), "\n"); \
            vboxVDbgDoDumpRcRectByRc("==>"__FUNCTION__" Dst:\n", (_pDstRc), &_DstRect, "\n"); \
        } \
    } while (0)

#define VBOXVDBG_DUMP_TEXBLT_LEAVE(_pSrcRc, _pSrcRect, _pDstRc, _pDstPoint) do { \
        if (VBOXVDBG_IS_DUMP_ALLOWED(TexBlt) \
                || VBOXVDBG_IS_DUMP_SHARED_ALLOWED(_pSrcRc) \
                || VBOXVDBG_IS_DUMP_SHARED_ALLOWED(_pDstRc) \
                ) \
        { \
            RECT _DstRect; \
            vboxWddmRectMoved(&_DstRect, (_pSrcRect), (_pDstPoint)->x, (_pDstPoint)->y); \
            vboxVDbgDoDumpRcRectByRc("<=="__FUNCTION__" Src:\n", (_pSrcRc), (_pSrcRect), "\n"); \
            vboxVDbgDoDumpRcRectByRc("<=="__FUNCTION__" Dst:\n", (_pDstRc), &_DstRect, "\n"); \
        } \
    } while (0)

#define VBOXVDBG_DUMP_BLT_ENTER(_pSrcRc, _pSrcSurf, _pSrcRect, _pDstRc, _pDstSurf, _pDstRect) do { \
        if (VBOXVDBG_IS_DUMP_ALLOWED(Blt) \
                || VBOXVDBG_IS_DUMP_SHARED_ALLOWED(_pSrcRc) \
                || VBOXVDBG_IS_DUMP_SHARED_ALLOWED(_pDstRc) \
                ) \
        { \
            vboxVDbgDoDumpSurfRect("==>"__FUNCTION__" Src:\n", (_pSrcSurf), (_pSrcRect), "\n", true); \
            vboxVDbgDoDumpSurfRect("==>"__FUNCTION__" Dst:\n", (_pDstSurf), (_pDstRect), "\n", true); \
        } \
    } while (0)

#define VBOXVDBG_DUMP_BLT_LEAVE(_pSrcRc, _pSrcSurf, _pSrcRect, _pDstRc, _pDstSurf, _pDstRect) do { \
        if (VBOXVDBG_IS_DUMP_ALLOWED(Blt) \
                || VBOXVDBG_IS_DUMP_SHARED_ALLOWED(_pSrcRc) \
                || VBOXVDBG_IS_DUMP_SHARED_ALLOWED(_pDstRc) \
                ) \
        { \
            vboxVDbgDoDumpSurfRect("<=="__FUNCTION__" Src:\n", (_pSrcSurf), (_pSrcRect), "\n", true); \
            vboxVDbgDoDumpSurfRect("<=="__FUNCTION__" Dst:\n", (_pDstSurf), (_pDstRect), "\n", true); \
        } \
    } while (0)

#define VBOXVDBG_DUMP_SYNC_RT(_pBbSurf) do { \
        if (VBOXVDBG_IS_DUMP_ALLOWED(RtSynch)) \
        { \
            vboxVDbgDoDumpSurfRect("== "__FUNCTION__" Bb:\n", (_pBbSurf), NULL, "\n", true); \
        } \
    } while (0)

#define VBOXVDBG_DUMP_FLUSH(_pDevice) do { \
        if (VBOXVDBG_IS_DUMP_ALLOWED(Flush)) \
        { \
            vboxVDbgDoDumpRt("== "__FUNCTION__": RenderTarget Dump\n", (_pDevice), "\n"); \
        }\
    } while (0)
#else
#define VBOXVDBG_DUMP_DRAWPRIM_ENTER(_pDevice) do { } while (0)
#define VBOXVDBG_DUMP_DRAWPRIM_LEAVE(_pDevice) do { } while (0)
#define VBOXVDBG_DUMP_SETTEXTURE(_pRc) do { } while (0)
#define VBOXVDBG_DUMP_TEXBLT_ENTER(_pSrcRc, _pSrcRect, _pDstRc, _pDstPoint) do { } while (0)
#define VBOXVDBG_DUMP_TEXBLT_LEAVE(_pSrcRc, _pSrcRect, _pDstRc, _pDstPoint) do { } while (0)
#define VBOXVDBG_DUMP_BLT_ENTER(_pSrcRc, _pSrcSurf, _pSrcRect, _pDstRc, _pDstSurf, _pDstRect) do { } while (0)
#define VBOXVDBG_DUMP_BLT_LEAVE(_pSrcRc, _pSrcSurf, _pSrcRect, _pDstRc, _pDstSurf, _pDstRect) do { } while (0)
#define VBOXVDBG_DUMP_SYNC_RT(_pBbSurf) do { } while (0)
#define VBOXVDBG_DUMP_FLUSH(_pDevice) do { } while (0)
#endif


#endif /* #ifndef ___VBoxDispDbg_h__ */
