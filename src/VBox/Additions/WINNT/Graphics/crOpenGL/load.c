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
#include "stub.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifndef WINDOWS
#include <sys/types.h>
#include <unistd.h>
#else
#include "cr_netserver.h"
#endif
#ifdef CHROMIUM_THREADSAFE
#include "cr_threads.h"
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

static void stubCheckWindowState(void)
{
    int winX, winY;
    unsigned int winW, winH;
    WindowInfo *window;
    bool bForceUpdate = false;

    CRASSERT(stub.trackWindowSize || stub.trackWindowPos);

    if (!stub.currentContext)
        return;

    window = stub.currentContext->currentDrawable;

    stubGetWindowGeometry( window, &winX, &winY, &winW, &winH );

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

    stubUpdateWindowGeometry(window, bForceUpdate);

#if defined(GLX) || defined (WINDOWS)
    if (stub.trackWindowVisibleRgn)
    {
        stubUpdateWindowVisibileRegions(window);
    }
#endif

    if (stub.trackWindowVisibility && window->type == CHROMIUM && window->drawable) {
        const int mapped = stubIsWindowVisible(window);
        if (mapped != window->mapped) {
            crDebug("Dispatched: WindowShow(%i, %i)", window->spuWindow, mapped);
            stub.spu->dispatch_table.WindowShow(window->spuWindow, mapped);
            window->mapped = mapped;
        }
    }
}


/**
 * Override the head SPU's glClear function.
 * We're basically trapping this function so that we can poll the
 * application window size at a regular interval.
 */
static void SPU_APIENTRY trapClear(GLbitfield mask)
{
    stubCheckWindowState();
    /* call the original SPU glClear function */
    origClear(mask);
}

/**
 * As above, but for glViewport.  Most apps call glViewport before
 * glClear when a window is resized.
 */
static void SPU_APIENTRY trapViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    stubCheckWindowState();
    /* call the original SPU glViewport function */
    origViewport(x, y, w, h);
}

static void SPU_APIENTRY trapSwapBuffers(GLint window, GLint flags)
{
    stubCheckWindowState();
    origSwapBuffers(window, flags);
}

static void SPU_APIENTRY trapDrawBuffer(GLenum buf)
{
    stubCheckWindowState();
    origDrawBuffer(buf);
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
        stub.spuDispatch.Clear = trapClear;
        stub.spuDispatch.Viewport = trapViewport;
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
    stubUninstallWindowMessageHook();
#endif
  
    //delete all created contexts
    stubMakeCurrent( NULL, NULL);
    crHashtableWalk(stub.contextTable, hsWalkStubDestroyContexts, NULL);

    /* shutdown, now trap any calls to a NULL dispatcher */
    crSPUCopyDispatchTable(&glim, &stubNULLDispatch);

    crSPUUnloadChain(stub.spu);
    stub.spu = NULL;

    crUnloadOpenGL();

    crNetTearDown();

#ifdef GLX
    if (stub.xshmSI.shmid>=0)
    {
        shmctl(stub.xshmSI.shmid, IPC_RMID, 0);
        shmdt(stub.xshmSI.shmaddr);
    }
#endif

    crMemset(&stub, 0, sizeof(stub) );
}

static void stubSPUSafeTearDown(void)
{
#ifdef CHROMIUM_THREADSAFE
    CRmutex *mutex = &stub.mutex;
    crLockMutex(mutex);
#endif
    crDebug("stubSPUSafeTearDown");
    crNetTearDown();
#ifdef WINDOWS
    stubUninstallWindowMessageHook();
#endif
    crMemset(&stub, 0, sizeof(stub));
#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex(mutex);
    crFreeMutex(mutex);
#endif
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
    stub.ignoreFreeglutMenus = 1;
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
    stub.ignoreFreeglutMenus = 1;
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
}

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

    if (stub_initialized)
        return true;
    stub_initialized = 1;
    
    stubInitVars();

    /* @todo check if it'd be of any use on other than guests, no use for windows */
    app_id = crGetenv( "CR_APPLICATION_ID_NUMBER" );

    crNetInit( NULL, NULL );
    strcpy(response, "3 0 array 1 feedback 2 pack");
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
        stub_initialized = 0;
        return false;
    }

    crSPUInitDispatchTable( &glim );

    /* This is unlikely to change -- We still want to initialize our dispatch 
     * table with the functions of the first SPU in the chain. */
    stubInitSPUDispatch( stub.spu );

    /* we need to plug one special stub function into the dispatch table */
    glim.GetChromiumParametervCR = stub_GetChromiumParametervCR;

    /* Load pointers to native OpenGL functions into stub.nativeDispatch */
    stubInitNativeDispatch();


/*crDebug("stub init");
raise(SIGINT);*/

#ifdef WINDOWS
    stubInstallWindowMessageHook();
#endif

#ifdef GLX
    stub.xshmSI.shmid = -1;
    stub.bShmInitFailed = GL_FALSE;
#endif

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

    case DLL_THREAD_ATTACH:
        break;

    case DLL_THREAD_DETACH:
        break;

    default:
        break;
    }

    return TRUE;
}
#endif
