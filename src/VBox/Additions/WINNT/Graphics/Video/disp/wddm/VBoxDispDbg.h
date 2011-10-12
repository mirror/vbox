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
//#  define VBOXWDDMDISP_DEBUG_PRINTDRV
/* use OutputDebugString */
#  define VBOXWDDMDISP_DEBUG_PRINT
/* adds vectored exception handler to be able to catch non-debug UM exceptions in kernel debugger */
#  define VBOXWDDMDISP_DEBUG_VEHANDLER
/* disable shared resource creation with wine */
//#  define VBOXWDDMDISP_DEBUG_NOSHARED
# endif

/* debug config vars */
extern DWORD g_VBoxVDbgFDumpSetTexture;
extern DWORD g_VBoxVDbgFDumpDrawPrim;
extern DWORD g_VBoxVDbgFDumpTexBlt;
extern DWORD g_VBoxVDbgFDumpBlt;
extern DWORD g_VBoxVDbgFDumpRtSynch;
extern DWORD g_VBoxVDbgFDumpFlush;
extern DWORD g_VBoxVDbgFDumpShared;
extern DWORD g_VBoxVDbgFDumpLock;
extern DWORD g_VBoxVDbgFDumpUnlock;

extern DWORD g_VBoxVDbgFBreakShared;
extern DWORD g_VBoxVDbgFBreakDdi;

extern DWORD g_VBoxVDbgFCheckSysMemSync;

/* log enable flags */
extern DWORD g_VBoxVDbgFLogRel;
extern DWORD g_VBoxVDbgFLog;
extern DWORD g_VBoxVDbgFLogFlow;


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
# define DbgPrintUsrFlow(_m) do { vboxDispLogDbgPrintF _m; } while (0)
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

#ifdef VBOXWDDMDISP_DEBUG
#define vboxVDbgInternalLog(_p) if (g_VBoxVDbgFLog) { _p }
#define vboxVDbgInternalLogFlow(_p) if (g_VBoxVDbgFLogFlow) { _p }
#define vboxVDbgInternalLogRel(_p) if (g_VBoxVDbgFLogRel) { _p }
#else
#define vboxVDbgInternalLog(_p) do {} while (0)
#define vboxVDbgInternalLogFlow(_p) do {} while (0)
#define vboxVDbgInternalLogRel(_p) do { _p } while (0)
#endif

#define WARN(_m) do { \
        vboxVDbgInternalLog( \
            Log(_m); \
            DbgPrintUsr(_m); \
            DbgPrintDrv(_m); \
        ); \
        WARN_BREAK(); \
    } while (0)
#define vboxVDbgPrint(_m) do { \
        vboxVDbgInternalLog( \
            Log(_m); \
            DbgPrintUsr(_m); \
            DbgPrintDrv(_m); \
        ); \
    } while (0)
#define vboxVDbgPrintF(_m)  do { \
        vboxVDbgInternalLogFlow( \
            LogFlow(_m); \
            DbgPrintUsrFlow(_m); \
            DbgPrintDrvFlow(_m); \
        ); \
    } while (0)
#define vboxVDbgPrintR(_m)  do { \
        vboxVDbgInternalLogRel( \
            LogRel(_m); \
            DbgPrintUsrRel(_m); \
            DbgPrintDrvRel(_m); \
        ); \
    } while (0)

#define LOG vboxVDbgPrint
#define LOGREL vboxVDbgPrintR
#define LOGFLOW vboxVDbgPrintF

#ifdef VBOXWDDMDISP_DEBUG

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
VOID vboxVDbgDoDumpRcRectByAlloc(const char * pPrefix, const PVBOXWDDMDISP_ALLOCATION pAlloc, IDirect3DResource9 *pD3DIf, const RECT *pRect, const char* pSuffix);
VOID vboxVDbgDoDumpTex(const char * pPrefix, IDirect3DBaseTexture9 *pTexBase, const char * pSuffix);
VOID vboxVDbgDoDumpRt(const char * pPrefix, struct VBOXWDDMDISP_DEVICE *pDevice, const char * pSuffix);

void vboxVDbgDoPrintRect(const char * pPrefix, const RECT *pRect, const char * pSuffix);
void vboxVDbgDoPrintAlloc(const char * pPrefix, const PVBOXWDDMDISP_RESOURCE pRc, uint32_t iAlloc, const char * pSuffix);

VOID vboxVDbgDoDumpLockSurfTex(const char * pPrefix, const D3DDDIARG_LOCK* pData, const char * pSuffix, bool fBreak);
VOID vboxVDbgDoDumpUnlockSurfTex(const char * pPrefix, const D3DDDIARG_UNLOCK* pData, const char * pSuffix, bool fBreak);


extern DWORD g_VBoxVDbgPid;
#define VBOXVDBG_IS_PID(_pid) ((_pid) == (g_VBoxVDbgPid ? g_VBoxVDbgPid : (g_VBoxVDbgPid = GetCurrentProcessId())))
#define VBOXVDBG_IS_DUMP_ALLOWED_PID(_pid) (((int)(_pid)) > 0 ? VBOXVDBG_IS_PID(_pid) : !VBOXVDBG_IS_PID(-((int)(_pid))))

#define VBOXVDBG_IS_DUMP_ALLOWED(_type) ( \
        g_VBoxVDbgFDump##_type \
        && (g_VBoxVDbgFDump##_type == 1 \
                || VBOXVDBG_IS_DUMP_ALLOWED_PID(g_VBoxVDbgFDump##_type) \
           ) \
        )

#define VBOXVDBG_IS_BREAK_ALLOWED(_type) ( \
        g_VBoxVDbgFBreak##_type \
        && (g_VBoxVDbgFBreak##_type == 1 \
                || VBOXVDBG_IS_DUMP_ALLOWED_PID(g_VBoxVDbgFBreak##_type) \
           ) \
        )

#define VBOXVDBG_IS_CHECK_ALLOWED(_type) ( \
        g_VBoxVDbgFCheck##_type \
        && (g_VBoxVDbgFCheck##_type == 1 \
                || VBOXVDBG_IS_DUMP_ALLOWED_PID(g_VBoxVDbgFCheck##_type) \
           ) \
        )

#define VBOXVDBG_IS_DUMP_SHARED_ALLOWED(_pRc) (\
        (_pRc)->RcDesc.fFlags.SharedResource \
        && VBOXVDBG_IS_DUMP_ALLOWED(Shared) \
        )

#define VBOXVDBG_IS_BREAK_SHARED_ALLOWED(_pRc) (\
        (_pRc)->RcDesc.fFlags.SharedResource \
        && VBOXVDBG_IS_BREAK_ALLOWED(Shared) \
        )

#define VBOXVDBG_BREAK_SHARED(_pRc) do { \
        if (VBOXVDBG_IS_BREAK_SHARED_ALLOWED(_pRc)) { \
            vboxVDbgPrint(("Break on shared access: Rc(0x%p), SharedHandle(0x%p)\n", (_pRc), (_pRc)->aAllocations[0].hSharedHandle)); \
            AssertFailed(); \
        } \
    } while (0)

#define VBOXVDBG_BREAK_DDI() do { \
        if (VBOXVDBG_IS_BREAK_ALLOWED(Ddi)) { \
            AssertFailed(); \
        } \
    } while (0)

#define VBOXVDBG_CHECK_SMSYNC(_pRc) do { \
        if (VBOXVDBG_IS_CHECK_ALLOWED(SysMemSync)) { \
            vboxWddmDbgRcSynchMemCheck((_pRc)); \
        } \
    } while (0)

#define VBOXVDBG_DEV_CHECK_SHARED(_pDevice, _pIsShared) do { \
        *(_pIsShared) = FALSE; \
        for (UINT i = 0; i < (_pDevice)->cRTs; ++i) { \
            PVBOXWDDMDISP_ALLOCATION pRtVar = (_pDevice)->apRTs[i]; \
            if (pRtVar->pRc->RcDesc.fFlags.SharedResource) { *(_pIsShared) = TRUE; break; } \
        } \
    } while (0)

#define VBOXVDBG_IS_DUMP_SHARED_ALLOWED_DEV(_pDevice, _pIsAllowed) do { \
        VBOXVDBG_DEV_CHECK_SHARED(_pDevice, _pIsAllowed); \
        if (*(_pIsAllowed)) \
        { \
            *(_pIsAllowed) = VBOXVDBG_IS_DUMP_ALLOWED(Shared); \
        } \
    } while (0)

#define VBOXVDBG_DUMP_DRAWPRIM_ENTER(_pDevice) do { \
        BOOL fDumpShaded = FALSE; \
        VBOXVDBG_IS_DUMP_SHARED_ALLOWED_DEV(_pDevice, &fDumpShaded); \
        if (fDumpShaded \
                || VBOXVDBG_IS_DUMP_ALLOWED(DrawPrim)) \
        { \
            vboxVDbgDoDumpRt("==>"__FUNCTION__": RenderTarget Dump\n", (_pDevice), "\n"); \
        }\
    } while (0)

#define VBOXVDBG_DUMP_DRAWPRIM_LEAVE(_pDevice) do { \
        BOOL fDumpShaded = FALSE; \
        VBOXVDBG_IS_DUMP_SHARED_ALLOWED_DEV(_pDevice, &fDumpShaded); \
        if (fDumpShaded \
                || VBOXVDBG_IS_DUMP_ALLOWED(DrawPrim)) \
        { \
            vboxVDbgDoDumpRt("<=="__FUNCTION__": RenderTarget Dump\n", (_pDevice), "\n"); \
        }\
    } while (0)

#define VBOXVDBG_DUMP_SETTEXTURE(_pRc) do { \
        if (VBOXVDBG_IS_DUMP_ALLOWED(SetTexture) \
                || VBOXVDBG_IS_DUMP_SHARED_ALLOWED(_pRc) \
                ) \
        { \
            vboxVDbgDoDumpRcRectByRc("== "__FUNCTION__": ", _pRc, NULL, "\n"); \
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
            vboxVDbgDoDumpRcRectByRc("==>"__FUNCTION__" Src: ", (_pSrcRc), (_pSrcRect), "\n"); \
            vboxVDbgDoDumpRcRectByRc("==>"__FUNCTION__" Dst: ", (_pDstRc), &_DstRect, "\n"); \
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
            vboxVDbgDoDumpRcRectByRc("<=="__FUNCTION__" Src: ", (_pSrcRc), (_pSrcRect), "\n"); \
            vboxVDbgDoDumpRcRectByRc("<=="__FUNCTION__" Dst: ", (_pDstRc), &_DstRect, "\n"); \
        } \
    } while (0)

#define VBOXVDBG_DUMP_BLT_ENTER(_pSrcAlloc, _pSrcSurf, _pSrcRect, _pDstAlloc, _pDstSurf, _pDstRect) do { \
        if (VBOXVDBG_IS_DUMP_ALLOWED(Blt) \
                || VBOXVDBG_IS_DUMP_SHARED_ALLOWED((_pSrcAlloc)->pRc) \
                || VBOXVDBG_IS_DUMP_SHARED_ALLOWED((_pDstAlloc)->pRc) \
                ) \
        { \
            vboxVDbgDoDumpRcRectByAlloc("==>"__FUNCTION__" Src: ", (_pSrcAlloc), (_pSrcSurf), (_pSrcRect), "\n"); \
            vboxVDbgDoDumpRcRectByAlloc("==>"__FUNCTION__" Dst: ", (_pDstAlloc), (_pDstSurf), (_pDstRect), "\n"); \
        } \
    } while (0)

#define VBOXVDBG_DUMP_BLT_LEAVE(_pSrcAlloc, _pSrcSurf, _pSrcRect, _pDstAlloc, _pDstSurf, _pDstRect) do { \
        if (VBOXVDBG_IS_DUMP_ALLOWED(Blt) \
                || VBOXVDBG_IS_DUMP_SHARED_ALLOWED((_pSrcAlloc)->pRc) \
                || VBOXVDBG_IS_DUMP_SHARED_ALLOWED((_pDstAlloc)->pRc) \
                ) \
        { \
            vboxVDbgDoDumpRcRectByAlloc("<=="__FUNCTION__" Src: ", (_pSrcAlloc), (_pSrcSurf), (_pSrcRect), "\n"); \
            vboxVDbgDoDumpRcRectByAlloc("<=="__FUNCTION__" Dst: ", (_pDstAlloc), (_pDstSurf), (_pDstRect), "\n"); \
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

#define VBOXVDBG_DUMP_LOCK_ST(_pData) do { \
        if (VBOXVDBG_IS_DUMP_ALLOWED(Lock) \
                || VBOXVDBG_IS_DUMP_ALLOWED(Unlock) \
                ) \
        { \
            vboxVDbgDoDumpLockSurfTex("== "__FUNCTION__": ", (_pData), "\n", VBOXVDBG_IS_DUMP_ALLOWED(Lock)); \
        } \
    } while (0)

#define VBOXVDBG_DUMP_UNLOCK_ST(_pData) do { \
        if (VBOXVDBG_IS_DUMP_ALLOWED(Unlock) \
                ) \
        { \
            vboxVDbgDoDumpUnlockSurfTex("== "__FUNCTION__": ", (_pData), "\n", true); \
        } \
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
#define VBOXVDBG_DUMP_LOCK_ST(_pData) do { } while (0)
#define VBOXVDBG_DUMP_UNLOCK_ST(_pData) do { } while (0)
#define VBOXVDBG_BREAK_SHARED(_pRc) do { } while (0)
#define VBOXVDBG_BREAK_DDI() do { } while (0)
#define VBOXVDBG_CHECK_SMSYNC(_pRc) do { } while (0)
#endif


#endif /* #ifndef ___VBoxDispDbg_h__ */
