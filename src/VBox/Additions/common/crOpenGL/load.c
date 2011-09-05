/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_spu.h"
#include "cr_net.h"
#include "cr_error.h"
#include "cr_mem.h"
#include "cr_string.h"
#include "cr_net.h"
#include "cr_environment.h"
#include "cr_process.h"
#include "cr_rand.h"
#include "cr_netserver.h"
#include "stub.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <iprt/initterm.h>
#include <iprt/thread.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#ifndef WINDOWS
# include <sys/types.h>
# include <unistd.h>
#endif
#ifdef CHROMIUM_THREADSAFE
#include "cr_threads.h"
#endif

#ifdef VBOX_WITH_WDDM
#include <d3d9types.h>
#include <D3dumddi.h>
#include "../../WINNT/Graphics/Video/common/wddm/VBoxMPIf.h"
#include "../../WINNT/Graphics/Video/disp/wddm/VBoxDispMp.h"
#endif

/**
 * If you change this, see the comments in tilesortspu_context.c
 */
#define MAGIC_CONTEXT_BASE 500

#define CONFIG_LOOKUP_FILE ".crconfigs"

#ifdef WINDOWS
#define PYTHON_EXE "python.exe"
#else
#define PYTHON_EXE "python"
#endif

#ifdef WINDOWS
static char* gsViewportHackApps[] = {"googleearth.exe", NULL};
#endif

static int stub_initialized = 0;

/* NOTE: 'SPUDispatchTable glim' is declared in NULLfuncs.py now */
/* NOTE: 'SPUDispatchTable stubThreadsafeDispatch' is declared in tsfuncs.c */
Stub stub;


static void stubInitNativeDispatch( void )
{
#define MAX_FUNCS 1000
    SPUNamedFunctionTable gl_funcs[MAX_FUNCS];
    int numFuncs;

    numFuncs = crLoadOpenGL( &stub.wsInterface, gl_funcs );

    stub.haveNativeOpenGL = (numFuncs > 0);

    /* XXX call this after context binding */
    numFuncs += crLoadOpenGLExtensions( &stub.wsInterface, gl_funcs + numFuncs );

    CRASSERT(numFuncs < MAX_FUNCS);

    crSPUInitDispatchTable( &stub.nativeDispatch );
    crSPUInitDispatch( &stub.nativeDispatch, gl_funcs );
    crSPUInitDispatchNops( &stub.nativeDispatch );
#undef MAX_FUNCS
}


/** Pointer to the SPU's real glClear and glViewport functions */
static ClearFunc_t origClear;
static ViewportFunc_t origViewport;
static SwapBuffersFunc_t origSwapBuffers;
static DrawBufferFunc_t origDrawBuffer;
static ScissorFunc_t origScissor;

static void stubCheckWindowState(WindowInfo *window, GLboolean bFlushOnChange)
{
    bool bForceUpdate = false;
    bool bChanged = false;

#ifdef WINDOWS
    /* @todo install hook and track for WM_DISPLAYCHANGE */
    {
        DEVMODE devMode;

        devMode.dmSize = sizeof(DEVMODE);
        EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode);

        if (devMode.dmPelsWidth!=window->dmPelsWidth || devMode.dmPelsHeight!=window->dmPelsHeight)
        {
            crDebug("Resolution changed(%d,%d), forcing window Pos/Size update", devMode.dmPelsWidth, devMode.dmPelsHeight);
            window->dmPelsWidth = devMode.dmPelsWidth;
            window->dmPelsHeight = devMode.dmPelsHeight;
            bForceUpdate = true;
        }
    }
#endif

    bChanged = stubUpdateWindowGeometry(window, bForceUpdate) || bForceUpdate;

#if defined(GLX) || defined (WINDOWS)
    if (stub.trackWindowVisibleRgn)
    {
        bChanged = stubUpdateWindowVisibileRegions(window) || bChanged;
    }
#endif

    if (stub.trackWindowVisibility && window->type == CHROMIUM && window->drawable) {
        const int mapped = stubIsWindowVisible(window);
        if (mapped != window->mapped) {
            crDebug("Dispatched: WindowShow(%i, %i)", window->spuWindow, mapped);
            stub.spu->dispatch_table.WindowShow(window->spuWindow, mapped);
            window->mapped = mapped;
            bChanged = true;
        }
    }

    if (bFlushOnChange && bChanged)
    {
        stub.spu->dispatch_table.Flush();
    }
}

static bool stubSystemWindowExist(WindowInfo *pWindow)
{
#ifdef WINDOWS
    if (pWindow->hWnd!=WindowFromDC(pWindow->drawable))
    {
        return false;
    }
#else
    Window root;
    int x, y;
    unsigned int border, depth, w, h;
    Display *dpy;

    dpy = stubGetWindowDisplay(pWindow);

    XLOCK(dpy);
    if (!XGetGeometry(dpy, pWindow->drawable, &root, &x, &y, &w, &h, &border, &depth))
    {
        XUNLOCK(dpy);
        return false;
    }
    XUNLOCK(dpy);
#endif

    return true;
}

static void stubCheckWindowsCB(unsigned long key, void *data1, void *data2)
{
    WindowInfo *pWindow = (WindowInfo *) data1;
    ContextInfo *pCtx = (ContextInfo *) data2;

    if (pWindow == pCtx->currentDrawable
        || pWindow->type!=CHROMIUM
        || pWindow->pOwner!=pCtx)
    {
        return;
    }

    if (!stubSystemWindowExist(pWindow))
    {
#ifdef WINDOWS
        crWindowDestroy((GLint)pWindow->hWnd);
#else
        crWindowDestroy((GLint)pWindow->drawable);
#endif
        return;
    }

    stubCheckWindowState(pWindow, GL_FALSE);
}

static void stubCheckWindowsState(void)
{
    CRASSERT(stub.trackWindowSize || stub.trackWindowPos);

    if (!stub.currentContext)
        return;

#if defined(WINDOWS) && defined(VBOX_WITH_WDDM)
    if (stub.bRunningUnderWDDM)
        return;
#endif

#if defined(CR_NEWWINTRACK) && !defined(WINDOWS)
    crLockMutex(&stub.mutex);
#endif

    stubCheckWindowState(stub.currentContext->currentDrawable, GL_TRUE);
    crHashtableWalk(stub.windowTable, stubCheckWindowsCB, stub.currentContext);

#if defined(CR_NEWWINTRACK) && !defined(WINDOWS)
    crUnlockMutex(&stub.mutex);
#endif
}


/**
 * Override the head SPU's glClear function.
 * We're basically trapping this function so that we can poll the
 * application window size at a regular interval.
 */
static void SPU_APIENTRY trapClear(GLbitfield mask)
{
    stubCheckWindowsState();
    /* call the original SPU glClear function */
    origClear(mask);
}

/**
 * As above, but for glViewport.  Most apps call glViewport before
 * glClear when a window is resized.
 */
static void SPU_APIENTRY trapViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    stubCheckWindowsState();
    /* call the original SPU glViewport function */
    if (!stub.viewportHack)
    {
        origViewport(x, y, w, h);
    }
    else
    {
        int winX, winY;
        unsigned int winW, winH;
        WindowInfo *pWindow;
        pWindow = stub.currentContext->currentDrawable;
        stubGetWindowGeometry(pWindow, &winX, &winY, &winW, &winH);
        origViewport(0, 0, winW, winH);
    }
}

static void SPU_APIENTRY trapSwapBuffers(GLint window, GLint flags)
{
    stubCheckWindowsState();
    origSwapBuffers(window, flags);
}

static void SPU_APIENTRY trapDrawBuffer(GLenum buf)
{
    stubCheckWindowsState();
    origDrawBuffer(buf);
}

static void SPU_APIENTRY trapScissor(GLint x, GLint y, GLsizei w, GLsizei h)
{
    int winX, winY;
    unsigned int winW, winH;
    WindowInfo *pWindow;
    pWindow = stub.currentContext->currentDrawable;
    stubGetWindowGeometry(pWindow, &winX, &winY, &winW, &winH);
    origScissor(0, 0, winW, winH);
}

/**
 * Use the GL function pointers in <spu> to initialize the static glim
 * dispatch table.
 */
static void stubInitSPUDispatch(SPU *spu)
{
    crSPUInitDispatchTable( &stub.spuDispatch );
    crSPUCopyDispatchTable( &stub.spuDispatch, &(spu->dispatch_table) );

    if (stub.trackWindowSize || stub.trackWindowPos || stub.trackWindowVisibleRgn) {
        /* patch-in special glClear/Viewport function to track window sizing */
        origClear = stub.spuDispatch.Clear;
        origViewport = stub.spuDispatch.Viewport;
        origSwapBuffers = stub.spuDispatch.SwapBuffers;
        origDrawBuffer = stub.spuDispatch.DrawBuffer;
        origScissor = stub.spuDispatch.Scissor;
        stub.spuDispatch.Clear = trapClear;
        stub.spuDispatch.Viewport = trapViewport;

        if (stub.viewportHack)
            stub.spuDispatch.Scissor = trapScissor;
        /*stub.spuDispatch.SwapBuffers = trapSwapBuffers;
        stub.spuDispatch.DrawBuffer = trapDrawBuffer;*/
    }

    crSPUCopyDispatchTable( &glim, &stub.spuDispatch );
}

// Callback function, used to destroy all created contexts
static void hsWalkStubDestroyContexts(unsigned long key, void *data1, void *data2)
{
    stubDestroyContext(key);
}

/**
 * This is called when we exit.
 * We call all the SPU's cleanup functions.
 */
static void stubSPUTearDown(void)
{
    crDebug("stubSPUTearDown");
    if (!stub_initialized) return;

    stub_initialized = 0;

#ifdef WINDOWS
# ifndef CR_NEWWINTRACK
    stubUninstallWindowMessageHook();
# endif
#endif

#ifdef CR_NEWWINTRACK
    ASMAtomicWriteBool(&stub.bShutdownSyncThread, true);
#endif

    //delete all created contexts
    stubMakeCurrent( NULL, NULL);
    crHashtableWalk(stub.contextTable, hsWalkStubDestroyContexts, NULL);

    /* shutdown, now trap any calls to a NULL dispatcher */
    crSPUCopyDispatchTable(&glim, &stubNULLDispatch);

    crSPUUnloadChain(stub.spu);
    stub.spu = NULL;

#ifndef Linux
    crUnloadOpenGL();
#endif

#ifndef WINDOWS
    crNetTearDown();
#endif

#ifdef GLX
    if (stub.xshmSI.shmid>=0)
    {
        shmctl(stub.xshmSI.shmid, IPC_RMID, 0);
        shmdt(stub.xshmSI.shmaddr);
    }
    crFreeHashtable(stub.pGLXPixmapsHash, crFree);
#endif

    crFreeHashtable(stub.windowTable, crFree);
    crFreeHashtable(stub.contextTable, NULL);

    crMemset(&stub, 0, sizeof(stub));
}

static void stubSPUSafeTearDown(void)
{
#ifdef CHROMIUM_THREADSAFE
    CRmutex *mutex;
#endif

    if (!stub_initialized) return;
    stub_initialized = 0;

#ifdef CHROMIUM_THREADSAFE
    mutex = &stub.mutex;
    crLockMutex(mutex);
#endif
    crDebug("stubSPUSafeTearDown");

#ifdef WINDOWS
# ifndef CR_NEWWINTRACK
    stubUninstallWindowMessageHook();
# endif
#endif

#if defined(CR_NEWWINTRACK)
    crUnlockMutex(mutex);
# if defined(WINDOWS)
    if (RTThreadGetState(stub.hSyncThread)!=RTTHREADSTATE_TERMINATED)
    {
        HANDLE hNative;
        DWORD ec=0;

        hNative = OpenThread(SYNCHRONIZE|THREAD_QUERY_INFORMATION|THREAD_TERMINATE,
                             false, RTThreadGetNative(stub.hSyncThread));
        if (!hNative)
        {
            crWarning("Failed to get handle for sync thread(%#x)", GetLastError());
        }
        else
        {
            crDebug("Got handle %p for thread %#x", hNative, RTThreadGetNative(stub.hSyncThread));
        }

        ASMAtomicWriteBool(&stub.bShutdownSyncThread, true);

        if (PostThreadMessage(RTThreadGetNative(stub.hSyncThread), WM_QUIT, 0, 0))
        {
            RTThreadWait(stub.hSyncThread, 1000, NULL);

            /*Same issue as on linux, RTThreadWait exits before system thread is terminated, which leads
             * to issues as our dll goes to be unloaded.
             *@todo
             *We usually call this function from DllMain which seems to be holding some lock and thus we have to
             * kill thread via TerminateThread.
             */
            if (WaitForSingleObject(hNative, 100)==WAIT_TIMEOUT)
            {
                crDebug("Wait failed, terminating");
                if (!TerminateThread(hNative, 1))
                {
                    crDebug("TerminateThread failed");
                }
            }
            if (GetExitCodeThread(hNative, &ec))
            {
                crDebug("Thread %p exited with ec=%i", hNative, ec);
            }
            else
            {
                crDebug("GetExitCodeThread failed(%#x)", GetLastError());
            }
        }
        else
        {
            crDebug("Sync thread killed before DLL_PROCESS_DETACH");
        }

        if (hNative)
        {
            CloseHandle(hNative);
        }
    }
#else
    if (stub.hSyncThread!=NIL_RTTHREAD)
    {
        ASMAtomicWriteBool(&stub.bShutdownSyncThread, true);
        {
            /*RTThreadWait might return too early, which cause our code being unloaded while RT thread wrapper is still running*/
            int rc = pthread_join(RTThreadGetNative(stub.hSyncThread), NULL);
            if (!rc)
            {
                crDebug("pthread_join failed %i", rc);
            }
        }
    }
#endif
    crLockMutex(mutex);
#endif

#ifndef WINDOWS
    crNetTearDown();
#endif

#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex(mutex);
    crFreeMutex(mutex);
#endif
    crMemset(&stub, 0, sizeof(stub));
}


static void stubExitHandler(void)
{
    stubSPUSafeTearDown();
}

/**
 * Called when we receive a SIGTERM signal.
 */
static void stubSignalHandler(int signo)
{
    stubSPUSafeTearDown();
    exit(0);  /* this causes stubExitHandler() to be called */
}


/**
 * Init variables in the stub structure, install signal handler.
 */
static void stubInitVars(void)
{
    WindowInfo *defaultWin;

#ifdef CHROMIUM_THREADSAFE
    crInitMutex(&stub.mutex);
#endif

    /* At the very least we want CR_RGB_BIT. */
    stub.haveNativeOpenGL = GL_FALSE;
    stub.spu = NULL;
    stub.appDrawCursor = 0;
    stub.minChromiumWindowWidth = 0;
    stub.minChromiumWindowHeight = 0;
    stub.maxChromiumWindowWidth = 0;
    stub.maxChromiumWindowHeight = 0;
    stub.matchChromiumWindowCount = 0;
    stub.matchChromiumWindowID = NULL;
    stub.matchWindowTitle = NULL;
    stub.ignoreFreeglutMenus = 0;
    stub.threadSafe = GL_FALSE;
    stub.trackWindowSize = 0;
    stub.trackWindowPos = 0;
    stub.trackWindowVisibility = 0;
    stub.trackWindowVisibleRgn = 0;
    stub.mothershipPID = 0;
    stub.spu_dir = NULL;

    stub.freeContextNumber = MAGIC_CONTEXT_BASE;
    stub.contextTable = crAllocHashtable();
    stub.currentContext = NULL;

    stub.windowTable = crAllocHashtable();

#ifdef CR_NEWWINTRACK
    stub.bShutdownSyncThread = false;
    stub.hSyncThread = NIL_RTTHREAD;
#endif

    defaultWin = (WindowInfo *) crCalloc(sizeof(WindowInfo));
    defaultWin->type = CHROMIUM;
    defaultWin->spuWindow = 0;  /* window 0 always exists */
#ifdef WINDOWS
    defaultWin->hVisibleRegion = INVALID_HANDLE_VALUE;
#elif defined(GLX)
    defaultWin->pVisibleRegions = NULL;
    defaultWin->cVisibleRegions = 0;
#endif
    crHashtableAdd(stub.windowTable, 0, defaultWin);

#if 1
    atexit(stubExitHandler);
    signal(SIGTERM, stubSignalHandler);
    signal(SIGINT, stubSignalHandler);
#ifndef WINDOWS
    signal(SIGPIPE, SIG_IGN); /* the networking code should catch this */
#endif
#else
    (void) stubExitHandler;
    (void) stubSignalHandler;
#endif
}


/**
 * Return a free port number for the mothership to use, or -1 if we
 * can't find one.
 */
static int
GenerateMothershipPort(void)
{
    const int MAX_PORT = 10100;
    unsigned short port;

    /* generate initial port number randomly */
    crRandAutoSeed();
    port = (unsigned short) crRandInt(10001, MAX_PORT);

#ifdef WINDOWS
    /* XXX should implement a free port check here */
    return port;
#else
    /*
     * See if this port number really is free, try another if needed.
     */
    {
        struct sockaddr_in servaddr;
        int so_reuseaddr = 1;
        int sock, k;

        /* create socket */
        sock = socket(AF_INET, SOCK_STREAM, 0);
        CRASSERT(sock > 2);

        /* deallocate socket/port when we exit */
        k = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                                     (char *) &so_reuseaddr, sizeof(so_reuseaddr));
        CRASSERT(k == 0);

        /* initialize the servaddr struct */
        crMemset(&servaddr, 0, sizeof(servaddr) );
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

        while (port < MAX_PORT) {
            /* Bind to the given port number, return -1 if we fail */
            servaddr.sin_port = htons((unsigned short) port);
            k = bind(sock, (struct sockaddr *) &servaddr, sizeof(servaddr));
            if (k) {
                /* failed to create port. try next one. */
                port++;
            }
            else {
                /* free the socket/port now so mothership can make it */
                close(sock);
                return port;
            }
        }
    }
#endif /* WINDOWS */
    return -1;
}


/**
 * Try to determine which mothership configuration to use for this program.
 */
static char **
LookupMothershipConfig(const char *procName)
{
    const int procNameLen = crStrlen(procName);
    FILE *f;
    const char *home;
    char configPath[1000];

    /* first, check if the CR_CONFIG env var is set */
    {
        const char *conf = crGetenv("CR_CONFIG");
        if (conf && crStrlen(conf) > 0)
            return crStrSplit(conf, " ");
    }

    /* second, look up config name from config file */
    home = crGetenv("HOME");
    if (home)
        sprintf(configPath, "%s/%s", home, CONFIG_LOOKUP_FILE);
    else
        crStrcpy(configPath, CONFIG_LOOKUP_FILE); /* from current dir */
    /* Check if the CR_CONFIG_PATH env var is set. */
    {
        const char *conf = crGetenv("CR_CONFIG_PATH");
        if (conf)
            crStrcpy(configPath, conf); /* from env var */
    }

    f = fopen(configPath, "r");
    if (!f) {
        return NULL;
    }

    while (!feof(f)) {
        char line[1000];
        char **args;
        fgets(line, 999, f);
        line[crStrlen(line) - 1] = 0; /* remove trailing newline */
        if (crStrncmp(line, procName, procNameLen) == 0 &&
            (line[procNameLen] == ' ' || line[procNameLen] == '\t'))
        {
            crWarning("Using Chromium configuration for %s from %s",
                                procName, configPath);
            args = crStrSplit(line + procNameLen + 1, " ");
            return args;
        }
    }
    fclose(f);
    return NULL;
}


static int Mothership_Awake = 0;


/**
 * Signal handler to determine when mothership is ready.
 */
static void
MothershipPhoneHome(int signo)
{
    crDebug("Got signal %d: mothership is awake!", signo);
    Mothership_Awake = 1;
}

void stubSetDefaultConfigurationOptions(void)
{
    unsigned char key[16]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

    stub.appDrawCursor = 0;
    stub.minChromiumWindowWidth = 0;
    stub.minChromiumWindowHeight = 0;
    stub.maxChromiumWindowWidth = 0;
    stub.maxChromiumWindowHeight = 0;
    stub.matchChromiumWindowID = NULL;
    stub.numIgnoreWindowID = 0;
    stub.matchWindowTitle = NULL;
    stub.ignoreFreeglutMenus = 0;
    stub.trackWindowSize = 1;
    stub.trackWindowPos = 1;
    stub.trackWindowVisibility = 1;
    stub.trackWindowVisibleRgn = 1;
    stub.matchChromiumWindowCount = 0;
    stub.spu_dir = NULL;
    crNetSetRank(0);
    crNetSetContextRange(32, 35);
    crNetSetNodeRange("iam0", "iamvis20");
    crNetSetKey(key,sizeof(key));
    stub.force_pbuffers = 0;
    stub.viewportHack = 0;

#ifdef WINDOWS
    {
        char name[1000];
        int i;

# ifdef VBOX_WITH_WDDM
        stub.bRunningUnderWDDM = false;
# endif
        /* Apply viewport hack only if we're running under wine */
        if (NULL!=GetModuleHandle("wined3d.dll") || NULL != GetModuleHandle("wined3dwddm.dll"))
        {
            crGetProcName(name, 1000);
            for (i=0; gsViewportHackApps[i]; ++i)
            {
                if (!stricmp(name, gsViewportHackApps[i]))
                {
                    stub.viewportHack = 1;
                    break;
                }
            }
        }
    }
#endif
}

#ifdef CR_NEWWINTRACK
# ifdef VBOX_WITH_WDDM
static stubDispatchVisibleRegions(WindowInfo *pWindow)
{
    DWORD dwCount;
    LPRGNDATA lpRgnData;

    dwCount = GetRegionData(pWindow->hVisibleRegion, 0, NULL);
    lpRgnData = crAlloc(dwCount);

    if (lpRgnData)
    {
        GetRegionData(pWindow->hVisibleRegion, dwCount, lpRgnData);
        crDebug("Dispatched WindowVisibleRegion (%i, cRects=%i)", pWindow->spuWindow, lpRgnData->rdh.nCount);
        stub.spuDispatch.WindowVisibleRegion(pWindow->spuWindow, lpRgnData->rdh.nCount, (GLint*) lpRgnData->Buffer);
        crFree(lpRgnData);
    }
    else crWarning("GetRegionData failed, VisibleRegions update failed");
}

static HRGN stubMakeRegionFromRects(PVBOXVIDEOCM_CMD_RECTS pRegions, uint32_t start)
{
    HRGN hRgn, hTmpRgn;
    uint32_t i;

    if (pRegions->RectsInfo.cRects<=start)
    {
        return INVALID_HANDLE_VALUE;
    }

    hRgn = CreateRectRgn(0, 0, 0, 0);
    for (i=start; i<pRegions->RectsInfo.cRects; ++i)
    {
        hTmpRgn = CreateRectRgnIndirect(&pRegions->RectsInfo.aRects[i]);
        CombineRgn(hRgn, hRgn, hTmpRgn, RGN_OR);
        DeleteObject(hTmpRgn);
    }
    return hRgn;
}

typedef struct VBOXCR_UPDATEWNDCB
{
    VBOXDISPMP_REGIONS Regions;
    bool fSendUpdateMsg;
} VBOXCR_UPDATEWNDCB, *PVBOXCR_UPDATEWNDCB;

static void stubSyncTrUpdateWindowCB(unsigned long key, void *data1, void *data2)
{
    WindowInfo *pWindow = (WindowInfo *) data1;
    PVBOXCR_UPDATEWNDCB pCbData = (PVBOXCR_UPDATEWNDCB) data2;
    VBOXDISPMP_REGIONS *pRegions = &pCbData->Regions;
    bool bChanged = false;
    HRGN hNewRgn = INVALID_HANDLE_VALUE;

    if (pRegions->hWnd != pWindow->hWnd)
    {
        return;
    }

    stub.spu->dispatch_table.VBoxPackSetInjectID(pWindow->u32ClientID);

    if (!stubSystemWindowExist(pWindow))
    {
        crWindowDestroy((GLint)pWindow->hWnd);
        return;
    }

    if (!pWindow->mapped)
    {
        pWindow->mapped = GL_TRUE;
        bChanged = true;
        crDebug("Dispatched: WindowShow(%i, %i)", pWindow->spuWindow, pWindow->mapped);
        stub.spu->dispatch_table.WindowShow(pWindow->spuWindow, pWindow->mapped);
    }

    if (pRegions->pRegions->fFlags.bSetVisibleRects || pRegions->pRegions->fFlags.bSetViewRect)
    {
        /* ensure data integrity */
        Assert(!pRegions->pRegions->fFlags.bAddHiddenRects);

        if (pRegions->pRegions->fFlags.bSetViewRect)
        {
            int winX, winY;
            unsigned int winW, winH;
            BOOL bRc;

            winX = pRegions->pRegions->RectsInfo.aRects[0].left;
            winY = pRegions->pRegions->RectsInfo.aRects[0].top;
            winW = pRegions->pRegions->RectsInfo.aRects[0].right - winX;
            winH = pRegions->pRegions->RectsInfo.aRects[0].bottom - winY;

            if (stub.trackWindowPos && (winX!=pWindow->x || winY!=pWindow->y))
            {
                crDebug("Dispatched WindowPosition (%i)", pWindow->spuWindow);
                stub.spuDispatch.WindowPosition(pWindow->spuWindow, winX, winY);
                pWindow->x = winX;
                pWindow->y = winY;
                bChanged = true;
            }

            if (stub.trackWindowSize && (winW!=pWindow->width || winH!=pWindow->height))
            {
                crDebug("Dispatched WindowSize (%i)", pWindow->spuWindow);
                stub.spuDispatch.WindowSize(pWindow->spuWindow, winW, winH);
                pWindow->width = winW;
                pWindow->height = winH;
                bChanged = true;
            }

            bRc = MoveWindow(pRegions->hWnd, winX, winY, winW, winH, FALSE /*BOOL bRepaint*/);
            if (!bRc)
            {
                DWORD winEr = GetLastError();
                crWarning("stubSyncTrUpdateWindowCB: MoveWindow failed winEr(%d)", winEr);
            }
        }

        if (pRegions->pRegions->fFlags.bSetVisibleRects)
        {
            hNewRgn = stubMakeRegionFromRects(pRegions->pRegions, pRegions->pRegions->fFlags.bSetViewRect ? 1 : 0);
        }
    }
    else if (!pRegions->pRegions->fFlags.bHide)
    {
        Assert(pRegions->pRegions->fFlags.bAddHiddenRects);
        hNewRgn = stubMakeRegionFromRects(pRegions->pRegions, 0);
    }
    else
    {
        Assert(pRegions->pRegions->fFlags.bAddHiddenRects);
        hNewRgn = CreateRectRgn(pWindow->x, pWindow->y, pWindow->x + pWindow->width, pWindow->y + pWindow->height);
    }

    if (hNewRgn!=INVALID_HANDLE_VALUE)
    {
        if (pRegions->pRegions->fFlags.bSetVisibleRects)
        {
            HRGN hEmptyRgn = CreateRectRgn(0, 0, 0, 0);

            if (hEmptyRgn!=INVALID_HANDLE_VALUE)
            {
                if (pWindow->hVisibleRegion==INVALID_HANDLE_VALUE || EqualRgn(pWindow->hVisibleRegion, hEmptyRgn))
                {
                    pCbData->fSendUpdateMsg = true;
                }

                DeleteObject(hEmptyRgn);
            }
            else
            {
                crWarning("Failed to created empty region!");
            }
        }

        OffsetRgn(hNewRgn, -pWindow->x, -pWindow->y);

        if (pWindow->hVisibleRegion!=INVALID_HANDLE_VALUE)
        {
            CombineRgn(hNewRgn, pWindow->hVisibleRegion, hNewRgn,
                       pRegions->pRegions->fFlags.bAddHiddenRects ? RGN_DIFF:RGN_OR);

            if (!EqualRgn(pWindow->hVisibleRegion, hNewRgn))
            {
                DeleteObject(pWindow->hVisibleRegion);
                pWindow->hVisibleRegion = hNewRgn;
                stubDispatchVisibleRegions(pWindow);
                bChanged = true;
            }
            else
            {
                DeleteObject(hNewRgn);
            }
        }
        else
        {
            if (pRegions->pRegions->fFlags.bSetVisibleRects)
            {
                pWindow->hVisibleRegion = hNewRgn;
                stubDispatchVisibleRegions(pWindow);
                bChanged = true;
            }
        }
    }

    if (bChanged)
    {
        stub.spu->dispatch_table.Flush();
    }
}
# endif

static void stubSyncTrCheckWindowsCB(unsigned long key, void *data1, void *data2)
{
    WindowInfo *pWindow = (WindowInfo *) data1;
    (void) data2;

    if (pWindow->type!=CHROMIUM || pWindow->spuWindow==0)
    {
        return;
    }

    stub.spu->dispatch_table.VBoxPackSetInjectID(pWindow->u32ClientID);

    if (!stubSystemWindowExist(pWindow))
    {
#ifdef WINDOWS
        crWindowDestroy((GLint)pWindow->hWnd);
#else
        crWindowDestroy((GLint)pWindow->drawable);
#endif
        /*No need to flush here as crWindowDestroy does it*/
        return;
    }

#if defined(WINDOWS) && defined(VBOX_WITH_WDDM)
    if (stub.bRunningUnderWDDM)
        return;
#endif
    stubCheckWindowState(pWindow, GL_TRUE);
}

static DECLCALLBACK(int) stubSyncThreadProc(RTTHREAD ThreadSelf, void *pvUser)
{
#ifdef WINDOWS
    MSG msg;
# ifdef VBOX_WITH_WDDM
    static VBOXDISPMP_CALLBACKS VBoxDispMpTstCallbacks = {NULL, NULL, NULL};
    HMODULE hVBoxD3D = NULL;
    VBOXCR_UPDATEWNDCB RegionsData;
    HRESULT hr;
# endif
#endif

    (void) pvUser;

    crDebug("Sync thread started");
#ifdef WINDOWS
    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
# ifdef VBOX_WITH_WDDM
    hVBoxD3D = GetModuleHandle("VBoxDispD3D");
    if (hVBoxD3D)
    {
        hVBoxD3D = LoadLibrary("VBoxDispD3D");
    }

    if (hVBoxD3D)
    {
        PFNVBOXDISPMP_GETCALLBACKS pfnVBoxDispMpGetCallbacks;
        pfnVBoxDispMpGetCallbacks = (PFNVBOXDISPMP_GETCALLBACKS)GetProcAddress(hVBoxD3D, TEXT("VBoxDispMpGetCallbacks"));
        if (pfnVBoxDispMpGetCallbacks)
        {
            hr = pfnVBoxDispMpGetCallbacks(VBOXDISPMP_VERSION, &VBoxDispMpTstCallbacks);
            if (S_OK==hr)
            {
                CRASSERT(VBoxDispMpTstCallbacks.pfnEnableEvents);
                CRASSERT(VBoxDispMpTstCallbacks.pfnDisableEvents);
                CRASSERT(VBoxDispMpTstCallbacks.pfnGetRegions);

                hr = VBoxDispMpTstCallbacks.pfnEnableEvents();
                if (hr != S_OK)
                {
                    crWarning("VBoxDispMpTstCallbacks.pfnEnableEvents failed");
                }
                else
                {
                    crDebug("running with VBoxDispD3D");
                    stub.trackWindowVisibleRgn = 0;
                    stub.bRunningUnderWDDM = true;
                }
            }
            else
            {
                crWarning("VBoxDispMpGetCallbacks failed");
            }
        }
    }
# endif
#endif

    crLockMutex(&stub.mutex);
    stub.spu->dispatch_table.VBoxPackSetInjectThread();
    crUnlockMutex(&stub.mutex);

    RTThreadUserSignal(ThreadSelf);

    while(!stub.bShutdownSyncThread)
    {
#ifdef WINDOWS
        if (!PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
# ifdef VBOX_WITH_WDDM
            if (VBoxDispMpTstCallbacks.pfnGetRegions)
            {
                hr = VBoxDispMpTstCallbacks.pfnGetRegions(&RegionsData.Regions, 50);
                if (S_OK==hr)
                {
                    RegionsData.fSendUpdateMsg = false;
#  if 0
                    uint32_t i;
                    crDebug(">>>Regions for HWND(0x%x)>>>", RegionsData.Regions.hWnd);
                    crDebug("Flags(0x%x)", RegionsData.Regions.pRegions->fFlags.Value);
                    for (i = 0; i < RegionsData.Regions.pRegions->RectsInfo.cRects; ++i)
                    {
                        RECT *pRect = &RegionsData.Regions.pRegions->RectsInfo.aRects[i];
                        crDebug("Rect(%d): left(%d), top(%d), right(%d), bottom(%d)", i, pRect->left, pRect->top, pRect->right, pRect->bottom);
                    }
                    crDebug("<<<<<");
#  endif
                    /*hacky way to make sure window wouldn't be deleted in another thread as we hold hashtable lock here*/
                    crHashtableWalk(stub.windowTable, stubSyncTrUpdateWindowCB, &RegionsData);
                    if (RegionsData.fSendUpdateMsg)
                    {
                        SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, 0, SMTO_NORMAL, 1000, NULL);
                    }
                }
                else
                {
                    if (WAIT_TIMEOUT!=hr)
                    {
                        crWarning("VBoxDispMpTstCallbacks.pfnGetRegions failed with 0x%x", hr);
                    }
                    crHashtableWalk(stub.windowTable, stubSyncTrCheckWindowsCB, NULL);
                }
            }
            else
# endif
            {
                crHashtableWalk(stub.windowTable, stubSyncTrCheckWindowsCB, NULL);
                RTThreadSleep(50);
            }
        }
        else
        {
            if (WM_QUIT==msg.message)
            {
                crDebug("Sync thread got WM_QUIT");
                break;
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
#else
        crLockMutex(&stub.mutex);
        crHashtableWalk(stub.windowTable, stubSyncTrCheckWindowsCB, NULL);
        crUnlockMutex(&stub.mutex);
        RTThreadSleep(50);
#endif
    }

#ifdef VBOX_WITH_WDDM
    if (VBoxDispMpTstCallbacks.pfnDisableEvents)
    {
        VBoxDispMpTstCallbacks.pfnDisableEvents();
    }
    if (hVBoxD3D)
    {
        FreeLibrary(hVBoxD3D);
    }
#endif
    crDebug("Sync thread stopped");
    return 0;
}
#endif

/**
 * Do one-time initializations for the faker.
 * Returns TRUE on success, FALSE otherwise.
 */
bool
stubInit(void)
{
    /* Here is where we contact the mothership to find out what we're supposed
     * to  be doing.  Networking code in a DLL initializer.  I sure hope this
     * works :)
     *
     * HOW can I pass the mothership address to this if I already know it?
     */

    CRConnection *conn = NULL;
    char response[1024];
    char **spuchain;
    int num_spus;
    int *spu_ids;
    char **spu_names;
    const char *app_id;
    int i;
    int disable_sync = 0;

    if (stub_initialized)
        return true;

    stubInitVars();

    crGetProcName(response, 1024);
    crDebug("Stub launched for %s", response);

#if defined(CR_NEWWINTRACK) && !defined(WINDOWS)
    /*@todo when vm boots with compiz turned on, new code causes hang in xcb_wait_for_reply in the sync thread*/
    if (!crStrcmp(response, "compiz") || !crStrcmp(response, "compiz_real")
	|| !crStrcmp(response, "compiz-bin"))
    {
        disable_sync = 1;
    }
#endif

    /* @todo check if it'd be of any use on other than guests, no use for windows */
    app_id = crGetenv( "CR_APPLICATION_ID_NUMBER" );

    crNetInit( NULL, NULL );

#ifndef WINDOWS
    {
        CRNetServer ns;

        ns.name = "vboxhgcm://host:0";
        ns.buffer_size = 1024;
        crNetServerConnect(&ns);
        if (!ns.conn)
        {
            crWarning("Failed to connect to host. Make sure 3D acceleration is enabled for this VM.");
            return false;
        }
        else
        {
            crNetFreeConnection(ns.conn);
        }
#if 0 && defined(CR_NEWWINTRACK)
        {
            Status st = XInitThreads();
            if (st==0)
            {
                crWarning("XInitThreads returned %i", (int)st);
            }
        }
#endif
    }
#endif

    strcpy(response, "2 0 feedback 1 pack");
    spuchain = crStrSplit( response, " " );
    num_spus = crStrToInt( spuchain[0] );
    spu_ids = (int *) crAlloc( num_spus * sizeof( *spu_ids ) );
    spu_names = (char **) crAlloc( num_spus * sizeof( *spu_names ) );
    for (i = 0 ; i < num_spus ; i++)
    {
        spu_ids[i] = crStrToInt( spuchain[2*i+1] );
        spu_names[i] = crStrdup( spuchain[2*i+2] );
        crDebug( "SPU %d/%d: (%d) \"%s\"", i+1, num_spus, spu_ids[i], spu_names[i] );
    }

    stubSetDefaultConfigurationOptions();

    stub.spu = crSPULoadChain( num_spus, spu_ids, spu_names, stub.spu_dir, NULL );

    crFree( spuchain );
    crFree( spu_ids );
    for (i = 0; i < num_spus; ++i)
        crFree(spu_names[i]);
    crFree( spu_names );

    // spu chain load failed somewhere
    if (!stub.spu) {
        return false;
    }

    crSPUInitDispatchTable( &glim );

    /* This is unlikely to change -- We still want to initialize our dispatch
     * table with the functions of the first SPU in the chain. */
    stubInitSPUDispatch( stub.spu );

    /* we need to plug one special stub function into the dispatch table */
    glim.GetChromiumParametervCR = stub_GetChromiumParametervCR;

#if !defined(VBOX_NO_NATIVEGL)
    /* Load pointers to native OpenGL functions into stub.nativeDispatch */
    stubInitNativeDispatch();
#endif

/*crDebug("stub init");
raise(SIGINT);*/

#ifdef WINDOWS
# ifndef CR_NEWWINTRACK
    stubInstallWindowMessageHook();
# endif
#endif

#ifdef CR_NEWWINTRACK
    {
        int rc;

        RTR3InitDll(0);

        if (!disable_sync)
        {
            crDebug("Starting sync thread");

            rc = RTThreadCreate(&stub.hSyncThread, stubSyncThreadProc, NULL, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "Sync");
            if (RT_FAILURE(rc))
            {
                crError("Failed to start sync thread! (%x)", rc);
            }
            RTThreadUserWait(stub.hSyncThread, 60 * 1000);
            RTThreadUserReset(stub.hSyncThread);

            crDebug("Going on");
        }
    }
#endif

#ifdef GLX
    stub.xshmSI.shmid = -1;
    stub.bShmInitFailed = GL_FALSE;
    stub.pGLXPixmapsHash = crAllocHashtable();

    stub.bXExtensionsChecked = GL_FALSE;
    stub.bHaveXComposite = GL_FALSE;
    stub.bHaveXFixes = GL_FALSE;
#endif

    stub_initialized = 1;
    return true;
}

/* Sigh -- we can't do initialization at load time, since Windows forbids
 * the loading of other libraries from DLLMain. */

#ifdef LINUX
/* GCC crap
 *void (*stub_init_ptr)(void) __attribute__((section(".ctors"))) = __stubInit; */
#endif

#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Windows crap */
BOOL WINAPI DllMain(HINSTANCE hDLLInst, DWORD fdwReason, LPVOID lpvReserved)
{
    (void) lpvReserved;

    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
    {
        CRNetServer ns;

        crNetInit(NULL, NULL);
        ns.name = "vboxhgcm://host:0";
        ns.buffer_size = 1024;
        crNetServerConnect(&ns);
        if (!ns.conn)
        {
            crDebug("Failed to connect to host (is guest 3d acceleration enabled?), aborting ICD load.");
            return FALSE;
        }
        else
            crNetFreeConnection(ns.conn);

        break;
    }

    case DLL_PROCESS_DETACH:
    {
        stubSPUSafeTearDown();
        break;
    }

#if 0
    case DLL_THREAD_ATTACH:
    {
        if (stub_initialized)
        {
            CRASSERT(stub.spu);
            stub.spu->dispatch_table.VBoxPackAttachThread();
        }
        break;
    }

    case DLL_THREAD_DETACH:
    {
        if (stub_initialized)
        {
            CRASSERT(stub.spu);
            stub.spu->dispatch_table.VBoxPackDetachThread();
        }
        break;
    }
#endif

    default:
        break;
    }

    return TRUE;
}
#endif
