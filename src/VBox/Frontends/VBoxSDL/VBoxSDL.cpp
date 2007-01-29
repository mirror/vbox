/** @file
 * VBox frontends: VBoxSDL (simple frontend based on SDL):
 * Main code
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_GUI

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/EventQueue.h>
#include <VBox/com/VirtualBox.h>

using namespace com;

#if defined (__LINUX__)
#include <X11/Xlib.h>
#include <X11/cursorfont.h>      /* for XC_left_ptr */
#include <X11/Xcursor/Xcursor.h>
#include <SDL_syswm.h>           /* for SDL_GetWMInfo() */
#endif

#include "VBoxSDL.h"
#include "Framebuffer.h"
#include "Helper.h"

#include <VBox/types.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/log.h>
#include <VBox/version.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/runtime.h>
#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/uuid.h>
#include <iprt/ldr.h>

#include <stdlib.h> /* for alloca */
#include <malloc.h> /* for alloca */
#include <signal.h>

#include <vector>

/* Xlib would re-define our enums */
#undef True
#undef False

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#ifdef VBOX_SECURELABEL
/** extra data key for the secure label */
#define VBOXSDL_SECURELABEL_EXTRADATA "VBoxSDL/SecureLabel"
/** label area height in pixels */
#define SECURE_LABEL_HEIGHT 20
#endif

/** Enables the rawr[0|3], patm, and casm options. */
#define VBOXSDL_ADVANCED_OPTIONS

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Pointer shape change event data strucure */
struct PointerShapeChangeData
{
    PointerShapeChangeData (BOOL aVisible, BOOL aAlpha, ULONG aXHot, ULONG aYHot,
                            ULONG aWidth, ULONG aHeight, const uint8_t *aShape)
        : visible (aVisible), alpha (aAlpha), xHot (aXHot), yHot (aYHot),
          width (aWidth), height (aHeight), shape (NULL)
    {
        // make a copy of the shape
        if (aShape)
        {
            uint32_t shapeSize = ((((aWidth + 7) / 8) * aHeight + 3) & ~3) + aWidth * 4 * aHeight;
            shape = new uint8_t [shapeSize];
            if (shape)
                memcpy ((void *) shape, (void *) aShape, shapeSize);
        }
    }

    ~PointerShapeChangeData()
    {
        if (shape) delete[] shape;
    }

    const BOOL visible;
    const BOOL alpha;
    const ULONG xHot;
    const ULONG yHot;
    const ULONG width;
    const ULONG height;
    const uint8_t *shape;
};

enum TitlebarMode
{
    TITLEBAR_NORMAL   = 1,
    TITLEBAR_STARTUP  = 2,
    TITLEBAR_SAVE     = 3,
    TITLEBAR_SNAPSHOT = 4
};

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static bool    UseAbsoluteMouse(void);
static void    ResetKeys(void);
static uint8_t Keyevent2Keycode(const SDL_KeyboardEvent *ev);
static void    ProcessKey(SDL_KeyboardEvent *ev);
static void    InputGrabStart(void);
static void    InputGrabEnd(void);
static void    SendMouseEvent(int dz, int button, int down);
static void    UpdateTitlebar(TitlebarMode mode, uint32_t u32User = 0);
static void    SetPointerShape(const PointerShapeChangeData *data);
static void    HandleGuestCapsChanged(void);
static int     HandleHostKey(const SDL_KeyboardEvent *pEv);
static Uint32  StartupTimer(Uint32 interval, void *param);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#if defined (DEBUG_dmik)
// my mini kbd doesn't have RCTRL...
static int gHostKeyMod  = KMOD_RSHIFT;
static int gHostKeySym1 = SDLK_RSHIFT;
static int gHostKeySym2 = SDLK_UNKNOWN;
#else
static int gHostKeyMod  = KMOD_RCTRL;
static int gHostKeySym1 = SDLK_RCTRL;
static int gHostKeySym2 = SDLK_UNKNOWN;
#endif
static BOOL gfGrabbed = FALSE;
static BOOL gfGrabOnMouseClick = TRUE;
static BOOL gfAllowFullscreenToggle = TRUE;
static BOOL gfAbsoluteMouseHost = FALSE;
static BOOL gfAbsoluteMouseGuest = FALSE;
static BOOL gfGuestNeedsHostCursor = FALSE;
static BOOL gfOffCursorActive = FALSE;
static BOOL gfGuestNumLockPressed = FALSE;
static BOOL gfGuestCapsLockPressed = FALSE;
static BOOL gfGuestScrollLockPressed = FALSE;
static int  gcGuestNumLockAdaptions = 2;
static int  gcGuestCapsLockAdaptions = 2;

/** modifier keypress status (scancode as index) */
static uint8_t gaModifiersState[256];

static ComPtr<IMachine> gMachine;
static ComPtr<IConsole> gConsole;
static ComPtr<IMachineDebugger> gMachineDebugger;
static ComPtr<IKeyboard> gKeyboard;
static ComPtr<IMouse> gMouse;
static ComPtr<IDisplay> gDisplay;
static ComPtr<IVRDPServer> gVrdpServer;
static ComPtr<IProgress> gProgress;

static VBoxSDLFB  *gpFrameBuffer = NULL;
static SDL_Cursor *gpDefaultCursor = NULL;
#ifdef __LINUX__
static Cursor      gpDefaultOrigX11Cursor;
#endif
static SDL_Cursor *gpCustomCursor = NULL;
static WMcursor   *gpCustomOrigWMcursor = NULL;
static SDL_Cursor *gpOffCursor = NULL;

#ifdef __LINUX__
static SDL_SysWMinfo gSdlInfo;
#endif

#ifdef VBOX_SECURELABEL
#ifdef __WIN__
#define LIBSDL_TTF_NAME "SDL_ttf"
#else
#define LIBSDL_TTF_NAME "libSDL_ttf"
#endif
RTLDRMOD gLibrarySDL_ttf = NIL_RTLDRMOD;
#endif

/**
 * Callback handler for VirtualBox events
 */
class VBoxSDLCallback :
    public IVirtualBoxCallback
{
public:
    VBoxSDLCallback()
    {
#if defined (__WIN__)
        refcnt = 0;
#endif
    }

    virtual ~VBoxSDLCallback()
    {
    }

#ifdef __WIN__
    STDMETHOD_(ULONG, AddRef)()
    {
        return ::InterlockedIncrement(&refcnt);
    }
    STDMETHOD_(ULONG, Release)()
    {
        long cnt = ::InterlockedDecrement(&refcnt);
        if (cnt == 0)
            delete this;
        return cnt;
    }
    STDMETHOD(QueryInterface)(REFIID riid , void **ppObj)
    {
        if (riid == IID_IUnknown)
        {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        if (riid == IID_IVirtualBoxCallback)
        {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        *ppObj = NULL;
        return E_NOINTERFACE;
    }
#endif

    NS_DECL_ISUPPORTS

    STDMETHOD(OnMachineStateChange)(INPTR GUIDPARAM machineId, MachineState_T state)
    {
        return S_OK;
    }

    STDMETHOD(OnMachineDataChange)(INPTR GUIDPARAM machineId)
    {
        return S_OK;
    }

    STDMETHOD(OnExtraDataCanChange)(INPTR GUIDPARAM machineId, INPTR BSTR key, INPTR BSTR value,
                                    BOOL *changeAllowed)
    {
        /* we never disagree */
        if (!changeAllowed)
            return E_INVALIDARG;
        *changeAllowed = true;
        return S_OK;
    }

    STDMETHOD(OnExtraDataChange)(INPTR GUIDPARAM machineId, INPTR BSTR key, INPTR BSTR value)
    {
#ifdef VBOX_SECURELABEL
        Assert(key);
        /*
         * check if we're interested in the message
         */
        Guid ourGuid;
        Guid messageGuid = machineId;
        gMachine->COMGETTER(Id)(ourGuid.asOutParam());
        if (ourGuid == messageGuid)
        {
            Bstr keyString = key;
            if (keyString && keyString == VBOXSDL_SECURELABEL_EXTRADATA)
            {
                /*
                 * Notify SDL thread of the string update
                 */
                SDL_Event event  = {0};
                event.type       = SDL_USEREVENT;
                event.user.type  = SDL_USER_EVENT_SECURELABEL_UPDATE;
                int rc = SDL_PushEvent(&event);
                NOREF(rc);
                AssertMsg(!rc, ("SDL_PushEvent returned with SDL error '%s'\n", SDL_GetError()));
            }
        }
#endif /* VBOX_SECURELABEL */
        return S_OK;
    }

    STDMETHOD(OnMachineRegistered)(INPTR GUIDPARAM machineId, BOOL registered)
    {
        return S_OK;
    }

    STDMETHOD(OnSessionStateChange)(INPTR GUIDPARAM machineId, SessionState_T state)
    {
        return S_OK;
    }

    STDMETHOD(OnSnapshotTaken) (INPTR GUIDPARAM aMachineId, INPTR GUIDPARAM aSnapshotId)
    {
        return S_OK;
    }

    STDMETHOD(OnSnapshotDiscarded) (INPTR GUIDPARAM aMachineId, INPTR GUIDPARAM aSnapshotId)
    {
        return S_OK;
    }

    STDMETHOD(OnSnapshotChange) (INPTR GUIDPARAM aMachineId, INPTR GUIDPARAM aSnapshotId)
    {
        return S_OK;
    }

private:
#ifdef __WIN__
    long refcnt;
#endif

};

/**
 * Callback handler for machine events
 */
class VBoxSDLConsoleCallback :
    public IConsoleCallback
{
public:
    VBoxSDLConsoleCallback() : m_fIgnorePowerOffEvents(false)
    {
#if defined (__WIN__)
        refcnt = 0;
#endif
    }

    virtual ~VBoxSDLConsoleCallback()
    {
    }

#ifdef __WIN__
    STDMETHOD_(ULONG, AddRef)()
    {
        return ::InterlockedIncrement(&refcnt);
    }
    STDMETHOD_(ULONG, Release)()
    {
        long cnt = ::InterlockedDecrement(&refcnt);
        if (cnt == 0)
            delete this;
        return cnt;
    }
    STDMETHOD(QueryInterface)(REFIID riid , void **ppObj)
    {
        if (riid == IID_IUnknown)
        {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        if (riid == IID_IConsoleCallback)
        {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        *ppObj = NULL;
        return E_NOINTERFACE;
    }
#endif

    NS_DECL_ISUPPORTS

    STDMETHOD(OnMousePointerShapeChange) (BOOL visible, BOOL alpha, ULONG xHot, ULONG yHot,
                                          ULONG width, ULONG height, ULONG shape)
    {
        PointerShapeChangeData *data;
        data = new PointerShapeChangeData (visible, alpha, xHot, yHot, width, height,
                                           (const uint8_t *) shape);
        Assert (data);
        if (!data)
            return E_FAIL;

        SDL_Event event  = {0};
        event.type       = SDL_USEREVENT;
        event.user.type  = SDL_USER_EVENT_POINTER_CHANGE;
        event.user.data1 = data;

        int rc = SDL_PushEvent (&event);
        AssertMsg(!rc, ("SDL_PushEvent returned with SDL error '%s'\n", SDL_GetError()));
        if (rc)
            delete data;

        return S_OK;
    }

    STDMETHOD(OnMouseCapabilityChange)(BOOL supportsAbsolute, BOOL needsHostCursor)
    {
        LogFlow(("OnMouseCapabilityChange: supportsAbsolute = %d\n", supportsAbsolute));
        gfAbsoluteMouseGuest   = supportsAbsolute;
        gfGuestNeedsHostCursor = needsHostCursor;

        SDL_Event event = {0};
        event.type      = SDL_USEREVENT;
        event.user.type = SDL_USER_EVENT_GUEST_CAP_CHANGED;

        int rc = SDL_PushEvent (&event);
        NOREF(rc);
        AssertMsg(!rc, ("SDL_PushEvent returned with SDL error '%s'\n", SDL_GetError()));
        return S_OK;
    }

    STDMETHOD(OnStateChange)(MachineState_T machineState)
    {
        LogFlow(("OnStateChange: machineState = %d (%s)\n", machineState, GetStateName(machineState)));
        SDL_Event event = {0};

        if (     machineState == MachineState_Aborted
            ||  (machineState == MachineState_Saved      && !m_fIgnorePowerOffEvents)
            ||  (machineState == MachineState_PoweredOff && !m_fIgnorePowerOffEvents))
        {
            /*
             * We have to inform the SDL thread that the application has be terminated
             */
            event.type      = SDL_USEREVENT;
            event.user.type = SDL_USER_EVENT_TERMINATE;
            event.user.code = machineState == MachineState_Aborted 
                                           ? VBOXSDL_TERM_ABEND
                                           : VBOXSDL_TERM_NORMAL;
        }
        else
        {
            /*
             * Inform the SDL thread to refresh the titlebar
             */
            event.type      = SDL_USEREVENT;
            event.user.type = SDL_USER_EVENT_UPDATE_TITLEBAR;
        }

        int rc = SDL_PushEvent(&event);
        NOREF(rc);
        AssertMsg(!rc, ("SDL_PushEvent returned with SDL error '%s'\n", SDL_GetError()));
        return S_OK;
    }

    STDMETHOD(OnAdditionsStateChange)()
    {
        return S_OK;
    }

    STDMETHOD(OnKeyboardLedsChange)(BOOL fNumLock, BOOL fCapsLock, BOOL fScrollLock)
    {
        /* Don't bother the guest with NumLock scancodes if he doesn't set the NumLock LED */
        if (gfGuestNumLockPressed != fNumLock)
            gcGuestNumLockAdaptions = 2;
        if (gfGuestCapsLockPressed != fCapsLock)
            gcGuestCapsLockAdaptions = 2;
        gfGuestNumLockPressed    = fNumLock;
        gfGuestCapsLockPressed   = fCapsLock;
        gfGuestScrollLockPressed = fScrollLock;
        return S_OK;
    }

    STDMETHOD(OnRuntimeError)(BOOL fatal, INPTR BSTR id, INPTR BSTR message)
    {
        return S_OK;
    }

    static const char *GetStateName(MachineState_T machineState)
    {
        switch (machineState)
        {
            case MachineState_InvalidMachineState: return "InvalidMachineState";
            case MachineState_Running:             return "Running";
            case MachineState_Restoring:           return "Restoring";
            case MachineState_Starting:            return "Starting";
            case MachineState_PoweredOff:          return "PoweredOff";
            case MachineState_Saved:               return "Saved";
            case MachineState_Aborted:             return "Aborted";
            case MachineState_Stopping:            return "Stopping";
            default:                               return "no idea";
        }
    }

    void ignorePowerOffEvents(bool fIgnore)
    {
        m_fIgnorePowerOffEvents = fIgnore;
    }

private:
#ifdef __WIN__
    long refcnt;
#endif
    bool m_fIgnorePowerOffEvents;
};

#ifdef __LINUX__
NS_DECL_CLASSINFO(VBoxSDLCallback)
NS_IMPL_ISUPPORTS1_CI(VBoxSDLCallback, IVirtualBoxCallback)
NS_DECL_CLASSINFO(VBoxSDLConsoleCallback)
NS_IMPL_ISUPPORTS1_CI(VBoxSDLConsoleCallback, IConsoleCallback)
#endif /* __LINUX__ */

static void show_usage()
{
    RTPrintf("Usage:\n"
             "  -list                    List all registered virtual machines and exit\n"
             "  -vm <id|name>            Virtual machine to start, either UUID or name\n"
             "  -hda <file>              Set temporary first hard disk to file\n"
             "  -fda <file>              Set temporary first floppy disk to file\n"
             "  -cdrom <file>            Set temporary CDROM/DVD to file/device ('none' to unmount)\n"
             "  -boot <a|c|d>            Set temporary boot device (a = floppy, c = first hard disk, d = DVD)\n"
             "  -m <size>                Set temporary memory size in megabytes\n"
             "  -vram <size>             Set temporary size of video memory in megabytes\n"
             "  -fullscreen              Start VM in fullscreen mode\n"
             "  -fixedmode <w> <h> <bpp> Use a fixed SDL video mode with given width, height and bits per pixel\n"
             "  -nofstoggle              Forbid switching to/from fullscreen mode\n"
             "  -noresize                Make the SDL frame non resizable\n"
             "  -nohostkey               Disable hostkey\n"
             "  -nograbonclick           Disable mouse/keyboard grabbing on mouse click w/o additions\n"
             "  -detecthostkey           Get the hostkey identifier and modifier state\n"
             "  -hostkey <key> {<key2>} <mod> Set the host key to the values obtained using -detecthostkey\n"
#ifdef __LINUX__
             "  -tapdev<1-N> <dev>       Use existing persistent TAP device with the given name\n"
             "  -tapfd<1-N> <fd>         Use existing TAP device, don't allocate\n"
#endif
#ifdef VBOX_VRDP
             "  -vrdp <port>             Listen for VRDP connections on port (default if not specified)\n"
#endif
             "  -discardstate            Discard saved state (if present) and revert to last snapshot (if present)\n"
#ifdef VBOX_SECURELABEL
             "  -securelabel             Display a secure VM label at the top of the screen\n"
             "  -seclabelfnt             TrueType (.ttf) font file for secure session label\n"
             "  -seclabelsiz             Font point size for secure session label (default 12)\n"
             "  -seclabelfgcol <rgb>     Secure label text color RGB value in 6 digit hexadecimal (eg: FFFF00)\n"
             "  -seclabelbgcol <rgb>     Secure label background color RGB value in 6 digit hexadecimal (eg: FF0000)\n"
#endif
#ifdef VBOXSDL_ADVANCED_OPTIONS
             "  -[no]rawr0               Enable or disable raw ring 3\n"
             "  -[no]rawr3               Enable or disable raw ring 0\n"
             "  -[no]patm                Enable or disable PATM\n"
             "  -[no]csam                Enable or disable CSAM\n"
             "  -[no]hwvirtex            Permit or deny the usage of VMX/SVN\n"
#endif
             "\n");
}

static void PrintError(const char *pszName, const BSTR pwszDescr, const BSTR pwszComponent=NULL)
{
    const char *pszFile, *pszFunc, *pszStat;
    char  pszBuffer[1024];
    com::ErrorInfo info;

    RTStrPrintf(pszBuffer, sizeof(pszBuffer), "%lS", pwszDescr);

    RTPrintf("\n%s! Error info:\n", pszName);
    if (   (pszFile = strstr(pszBuffer, "At '"))
        && (pszFunc = strstr(pszBuffer, ") in "))
        && (pszStat = strstr(pszBuffer, "VBox status code: ")))
        RTPrintf("  %.*s  %.*s\n  In%.*s  %s",
                 pszFile-pszBuffer, pszBuffer,
                 pszFunc-pszFile+1, pszFile,
                 pszStat-pszFunc-4, pszFunc+4,
                 pszStat);
    else
        RTPrintf("%s\n", pszBuffer);

    if (pwszComponent)
        RTPrintf("(component %lS).\n", pwszComponent);

    RTPrintf("\n");
}

#ifdef __LINUX__
/**
 * Custom signal handler. Currently it is only used to release modifier
 * keys when receiving the USR1 signal. When switching VTs, we might not
 * get release events for Ctrl-Alt and in case a savestate is performed
 * on the new VT, the VM will be saved with modifier keys stuck. This is
 * annoying enough for introducing this hack.
 */
void signal_handler(int sig, siginfo_t *info, void *secret)
{
    /* only SIGUSR1 is interesting */
    if (sig == SIGUSR1)
    {
        /* just release the modifiers */
        ResetKeys();
    }
}
#endif /* __LINUX__ */

/** entry point */
int main(int argc, char *argv[])
{
    /*
     * Before we do *anything*, we initialize the runtime.
     */
    int rcRT = RTR3Init(true, ~(size_t)0);
    if (VBOX_FAILURE(rcRT))
    {
        RTPrintf("Error: RTR3Init failed rcRC=%d\n", rcRT);
        return 1;
    }

    /*
     * the hostkey detection mode is unrelated to VM processing, so handle it before
     * we initialize anything COM related
     */
    if (argc == 2 && !strcmp(argv[1], "-detecthostkey"))
    {
        int rc = SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE);
        if (rc != 0)
        {
            RTPrintf("Error: SDL_InitSubSystem failed with message '%s'\n", SDL_GetError());
            return 1;
        }
        /* we need a video window for the keyboard stuff to work */
        if (!SDL_SetVideoMode(640, 480, 16, SDL_SWSURFACE))
        {
            RTPrintf("Error: could not set SDL video mode\n");
            return 1;
        }

        RTPrintf("Please hit one or two function key(s) to get the -hostkey value...\n");

        SDL_Event event1;
        while (SDL_WaitEvent(&event1))
        {
            if (event1.type == SDL_KEYDOWN)
            {
                SDL_Event event2;
                unsigned  mod = SDL_GetModState() & ~(KMOD_MODE | KMOD_NUM | KMOD_RESERVED);
                while (SDL_WaitEvent(&event2))
                {
                    if (event2.type == SDL_KEYDOWN || event2.type == SDL_KEYUP)
                    {
                        /* pressed additional host key */
                        RTPrintf("-hostkey %d", event1.key.keysym.sym);
                        if (event2.type == SDL_KEYDOWN)
                        {
                            RTPrintf(" %d", event2.key.keysym.sym);
                            RTPrintf(" %d\n", SDL_GetModState() & ~(KMOD_MODE | KMOD_NUM | KMOD_RESERVED));
                        }
                        else
                        {
                            RTPrintf(" %d\n", mod);
                        }
                        /* we're done */
                        break;
                    }
                }
                /* we're down */
                break;
            }
        }
        SDL_Quit();
        return 1;
    }

    HRESULT rc;
    Guid uuid;
    char *vmName = NULL;
    DeviceType_T bootDevice = DeviceType_NoDevice;
    uint32_t memorySize = 0;
    uint32_t vramSize = 0;
    VBoxSDLCallback *callback = NULL;
    VBoxSDLConsoleCallback *consoleCallback = NULL;
    bool fFullscreen = false;
    bool fResizable = true;
    bool fXPCOMEventThreadSignaled = false;
    bool fListVMs = false;
    char *hdaFile   = NULL;
    char *cdromFile = NULL;
    char *fdaFile   = NULL;
#ifdef VBOX_VRDP
    int portVRDP = ~0;
#endif
    bool fDiscardState = false;
#ifdef VBOX_SECURELABEL
    BOOL fSecureLabel = false;
    uint32_t secureLabelPointSize = 12;
    char *secureLabelFontFile = NULL;
    uint32_t secureLabelColorFG = 0x0000FF00;
    uint32_t secureLabelColorBG = 0x00FFFF00;
#endif
#ifdef VBOXSDL_ADVANCED_OPTIONS
    unsigned fRawR0 = ~0U;
    unsigned fRawR3 = ~0U;
    unsigned fPATM  = ~0U;
    unsigned fCSAM  = ~0U;
    TriStateBool_T fHWVirt = TriStateBool_Default;
#endif
#ifdef VBOX_WIN32_UI
    bool fWin32UI = false;
#endif
    bool fShowSDLConfig = false;
    uint32_t fixedWidth = ~(uint32_t)0;
    uint32_t fixedHeight = ~(uint32_t)0;
    uint32_t fixedBPP = ~(uint32_t)0;

    /* The damned GOTOs forces this to be up here - totally out of place. */
    /*
     * Host key handling.
     *
     * The golden rule is that host-key combinations should not be seen
     * by the guest. For instance a CAD should not have any extra RCtrl down
     * and RCtrl up around itself. Nor should a resume be followed by a Ctrl-P
     * that could encourage applications to start printing.
     *
     * We must not confuse the hostkey processing into any release sequences
     * either, the host key is supposed to be explicitly pressing one key.
     *
     * Quick state diagram:
     *
     *            host key down alone
     *  (Normal) ---------------
     *    ^ ^                  |
     *    | |                  v          host combination key down
     *    | |            (Host key down) ----------------
     *    | | host key up v    |                        |
     *    | |--------------    | other key down         v           host combination key down
     *    |                    |                  (host key used) -------------
     *    |                    |                        |      ^              |
     *    |              (not host key)--               |      |---------------
     *    |                    |     |  |               |
     *    |                    |     ---- other         |
     *    |  modifiers = 0     v                        v
     *    -----------------------------------------------
     */
    enum HKEYSTATE
    {
        /** The initial and most common state, pass keystrokes to the guest.
         * Next state: HKEYSTATE_DOWN
         * Prev state: Any */
        HKEYSTATE_NORMAL = 1,
        /** The first host key was pressed down
         */
        HKEYSTATE_DOWN_1ST,
        /** The second host key was pressed down (if gHostKeySym2 != SDLK_UNKNOWN)
         */
        HKEYSTATE_DOWN_2ND,
        /** The host key has been pressed down.
         * Prev state: HKEYSTATE_NORMAL
         * Next state: HKEYSTATE_NORMAL - host key up, capture toggle.
         * Next state: HKEYSTATE_USED   - host key combination down.
         * Next state: HKEYSTATE_NOT_IT - non-host key combination down.
         */
        HKEYSTATE_DOWN,
        /** A host key combination was pressed.
         * Prev state: HKEYSTATE_DOWN
         * Next state: HKEYSTATE_NORMAL - when modifiers are all 0
         */
        HKEYSTATE_USED,
        /** A non-host key combination was attempted. Send hostkey down to the
         * guest and continue until all modifiers have been released.
         * Prev state: HKEYSTATE_DOWN
         * Next state: HKEYSTATE_NORMAL - when modifiers are all 0
         */
        HKEYSTATE_NOT_IT
    } enmHKeyState = HKEYSTATE_NORMAL;
    /** The host key down event which we have been hiding from the guest.
     * Used when going from HKEYSTATE_DOWN to HKEYSTATE_NOT_IT. */
    SDL_Event EvHKeyDown1;
    SDL_Event EvHKeyDown2;

    LogFlow(("SDL GUI started\n"));
    RTPrintf("VirtualBox SDL GUI %d.%d.%d built %s %s\n",
             VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR, VBOX_VERSION_BUILD, __DATE__, __TIME__);

    // less than one parameter is not possible
    if (argc < 2)
    {
        show_usage();
        return 1;
    }

    rc = com::Initialize();
    if (FAILED(rc))
    {
        RTPrintf("Error: COM initialization failed, rc = 0x%x!\n", rc);
        return 1;
    }

    do
    {
    // scopes all the stuff till shutdown
    ////////////////////////////////////////////////////////////////////////////

    ComPtr <IVirtualBox> virtualBox;
    ComPtr <ISession> session;
    bool sessionOpened = false;

    rc = virtualBox.createLocalObject (CLSID_VirtualBox,
                                       "VirtualBoxServer");
    if (FAILED(rc))
    {
        com::ErrorInfo info;
        if (info.isFullAvailable())
            PrintError("Failed to create VirtualBox object",
                       info.getText().raw(), info.getComponent().raw());
        else
            RTPrintf("Failed to create VirtualBox object! No error information available (rc = 0x%x).\n", rc);
        break;
    }
    rc = session.createInprocObject (CLSID_Session);
    if (FAILED(rc))
    {
        RTPrintf("Failed to create session object, rc = 0x%x!\n", rc);
        break;
    }

    // create the event queue
    // (here it is necessary only to process remaining XPCOM/IPC events
    // after the session is closed)
    /// @todo
//    EventQueue eventQ;

#ifdef __LINUX__
    nsCOMPtr<nsIEventQueue> eventQ;
    NS_GetMainEventQ(getter_AddRefs(eventQ));
#endif /* __LINUX__ */

    /* Get the number of network adapters */
    ULONG NetworkAdapterCount = 0;
    ComPtr <ISystemProperties> sysInfo;
    virtualBox->COMGETTER(SystemProperties) (sysInfo.asOutParam());
    sysInfo->COMGETTER (NetworkAdapterCount) (&NetworkAdapterCount);

#ifdef __LINUX__
    std::vector <Bstr> tapdev (NetworkAdapterCount);
    std::vector <int> tapfd (NetworkAdapterCount, 0);
#endif

    // command line argument parsing stuff
    for (int curArg = 1; curArg < argc; curArg++)
    {
        if (strcmp(argv[curArg], "-list") == 0)
        {
            fListVMs = true;
        }
        else if (strcmp(argv[curArg], "-vm") == 0
              || strcmp(argv[curArg], "-startvm") == 0)
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: VM not specified (UUID or name)!\n");
                rc = E_FAIL;
                break;
            }
            // first check if a UUID was supplied
            if (VBOX_FAILURE(RTUuidFromStr(uuid.ptr(), argv[curArg])))
            {
                LogFlow(("invalid UUID format, assuming it's a VM name\n"));
                vmName = argv[curArg];
            }
        }
        else if (strcmp(argv[curArg], "-boot") == 0)
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing argument for boot drive!\n");
                rc = E_FAIL;
                break;
            }
            switch (argv[curArg][0])
            {
                case 'a':
                {
                    bootDevice = DeviceType_FloppyDevice;
                    break;
                }

                case 'c':
                {
                    bootDevice = DeviceType_HardDiskDevice;
                    break;
                }

                case 'd':
                {
                    bootDevice = DeviceType_DVDDevice;
                    break;
                }

                default:
                {
                    RTPrintf("Error: wrong argument for boot drive!\n");
                    rc = E_FAIL;
                    break;
                }
            }
            if (FAILED (rc))
                break;
        }
        else if (strcmp(argv[curArg], "-m") == 0)
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing argument for memory size!\n");
                rc = E_FAIL;
                break;
            }
            memorySize = atoi(argv[curArg]);
        }
        else if (strcmp(argv[curArg], "-vram") == 0)
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing argument for vram size!\n");
                rc = E_FAIL;
                break;
            }
            vramSize = atoi(argv[curArg]);
        }
        else if (strcmp(argv[curArg], "-fullscreen") == 0)
        {
            fFullscreen = true;
        }
        else if (strcmp(argv[curArg], "-fixedmode") == 0)
        {
            /* three parameters follow */
            if (curArg + 3 >= argc)
            {
                RTPrintf("Error: missing arguments for fixed video mode!\n");
                rc = E_FAIL;
                break;
            }
            fixedWidth  = atoi(argv[++curArg]);
            fixedHeight = atoi(argv[++curArg]);
            fixedBPP    = atoi(argv[++curArg]);
        }
        else if (strcmp(argv[curArg], "-nofstoggle") == 0)
        {
            gfAllowFullscreenToggle = FALSE;
        }
        else if (strcmp(argv[curArg], "-noresize") == 0)
        {
            fResizable = false;
        }
        else if (strcmp(argv[curArg], "-nohostkey") == 0)
        {
            gHostKeyMod  = 0;
            gHostKeySym1 = 0;
        }
        else if (strcmp(argv[curArg], "-nograbonclick") == 0)
        {
            gfGrabOnMouseClick = FALSE;
        }
        else if (strcmp(argv[curArg], "-hda") == 0)
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing file name for first hard disk!\n");
                rc = E_FAIL;
                break;
            }
            /* resolve it. */
            hdaFile = RTPathRealDup(argv[curArg]);
            if (!hdaFile)
            {
                RTPrintf("Error: The path to the specified harddisk, '%s', could not be resolved.\n", argv[curArg]);
                rc = E_FAIL;
                break;
            }
        }
        else if (strcmp(argv[curArg], "-fda") == 0)
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing file/device name for first floppy disk!\n");
                rc = E_FAIL;
                break;
            }
            /* resolve it. */
            fdaFile = RTPathRealDup(argv[curArg]);
            if (!fdaFile)
            {
                RTPrintf("Error: The path to the specified floppy disk, '%s', could not be resolved.\n", argv[curArg]);
                rc = E_FAIL;
                break;
            }
        }
        else if (strcmp(argv[curArg], "-cdrom") == 0)
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing file/device name for first hard disk!\n");
                rc = E_FAIL;
                break;
            }
            /* resolve it. */
            cdromFile = RTPathRealDup(argv[curArg]);
            if (!cdromFile)
            {
                RTPrintf("Error: The path to the specified cdrom, '%s', could not be resolved.\n", argv[curArg]);
                rc = E_FAIL;
                break;
            }
        }
#ifdef __LINUX__
        else if (strncmp(argv[curArg], "-tapdev", 7) == 0)
        {
            ULONG n = 0;
            if (!argv[curArg][7] || ((n = strtoul(&argv[curArg][7], NULL, 10)) < 1) ||
                (n > NetworkAdapterCount) || (argc <= (curArg + 1)))
            {
                RTPrintf("Error: invalid TAP device option!\n");
                rc = E_FAIL;
                break;
            }
            tapdev[n - 1] = argv[curArg + 1];
            curArg++;
        }
        else if (strncmp(argv[curArg], "-tapfd", 6) == 0)
        {
            ULONG n = 0;
            if (!argv[curArg][6] || ((n = strtoul(&argv[curArg][6], NULL, 10)) < 1) ||
                (n > NetworkAdapterCount) || (argc <= (curArg + 1)))
            {
                RTPrintf("Error: invalid TAP file descriptor option!\n");
                rc = E_FAIL;
                break;
            }
            tapfd[n - 1] = atoi(argv[curArg + 1]);
            curArg++;
        }
#endif /* __LINUX__ */
#ifdef VBOX_VRDP
        else if (strcmp(argv[curArg], "-vrdp") == 0)
        {
            // start with the standard VRDP port
            portVRDP = 0;

            // is there another argument
            if (argc > (curArg + 1))
            {
                // check if the next argument is a number
                int port = atoi(argv[curArg + 1]);
                if (port > 0)
                {
                    curArg++;
                    portVRDP = port;
                    LogFlow(("Using non standard VRDP port %d\n", portVRDP));
                }
            }
        }
#endif /* VBOX_VRDP */
        else if (strcmp(argv[curArg], "-discardstate") == 0)
        {
            fDiscardState = true;
        }
#ifdef VBOX_SECURELABEL
        else if (strcmp(argv[curArg], "-securelabel") == 0)
        {
            fSecureLabel = true;
            LogFlow(("Secure labelling turned on\n"));
        }
        else if (strcmp(argv[curArg], "-seclabelfnt") == 0)
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing font file name for secure label!\n");
                rc = E_FAIL;
                break;
            }
            secureLabelFontFile = argv[curArg];
        }
        else if (strcmp(argv[curArg], "-seclabelsiz") == 0)
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing font point size for secure label!\n");
                rc = E_FAIL;
                break;
            }
            secureLabelPointSize = atoi(argv[curArg]);
        }
        else if (strcmp(argv[curArg], "-seclabelfgcol") == 0)
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing text color value for secure label!\n");
                rc = E_FAIL;
                break;
            }
            sscanf(argv[curArg], "%X", &secureLabelColorFG);
        }
        else if (strcmp(argv[curArg], "-seclabelbgcol") == 0)
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing background color value for secure label!\n");
                rc = E_FAIL;
                break;
            }
            sscanf(argv[curArg], "%X", &secureLabelColorBG);
        }
#endif
#ifdef VBOXSDL_ADVANCED_OPTIONS
        else if (strcmp(argv[curArg], "-rawr0") == 0)
            fRawR0 = true;
        else if (strcmp(argv[curArg], "-norawr0") == 0)
            fRawR0 = false;
        else if (strcmp(argv[curArg], "-rawr3") == 0)
            fRawR3 = true;
        else if (strcmp(argv[curArg], "-norawr3") == 0)
            fRawR3 = false;
        else if (strcmp(argv[curArg], "-patm") == 0)
            fPATM = true;
        else if (strcmp(argv[curArg], "-nopatm") == 0)
            fPATM = false;
        else if (strcmp(argv[curArg], "-csam") == 0)
            fCSAM = true;
        else if (strcmp(argv[curArg], "-nocsam") == 0)
            fCSAM = false;
        else if (strcmp(argv[curArg], "-hwvirtex") == 0)
            fHWVirt = TriStateBool_True;
        else if (strcmp(argv[curArg], "-nohwvirtex") == 0)
            fHWVirt = TriStateBool_False;
#endif /* VBOXSDL_ADVANCED_OPTIONS */
#ifdef VBOX_WIN32_UI
        else if (strcmp(argv[curArg], "-win32ui") == 0)
            fWin32UI = true;
#endif
        else if (strcmp(argv[curArg], "-showsdlconfig") == 0)
            fShowSDLConfig = true;
        else if (strcmp(argv[curArg], "-hostkey") == 0)
        {
            if (++curArg + 1 >= argc)
            {
                RTPrintf("Error: not enough arguments for host keys!\n");
                rc = E_FAIL;
                break;
            }
            gHostKeySym1 = atoi(argv[curArg++]);
            if (curArg + 1 < argc && (argv[curArg+1][0] == '0' || atoi(argv[curArg+1]) > 0))
            {
                /* two-key sequence as host key specified */
                gHostKeySym2 = atoi(argv[curArg++]);
            }
            gHostKeyMod = atoi(argv[curArg]);
        }
        /* just show the help screen */
        else
        {
            RTPrintf("Error: unrecognized switch '%s'\n", argv[curArg]);
            show_usage();
            return 1;
        }
    }
    if (FAILED (rc))
        break;

    /*
     * Are we supposed to display the list of registered VMs?
     */
    if (fListVMs)
    {
        RTPrintf("\nList of registered VMs:\n");
        /*
         * Get the list of all registered VMs
         */
        ComPtr<IMachineCollection> collection;
        rc = virtualBox->COMGETTER(Machines)(collection.asOutParam());
        ComPtr<IMachineEnumerator> enumerator;
        if (SUCCEEDED(rc))
            rc = collection->Enumerate(enumerator.asOutParam());
        if (SUCCEEDED(rc))
        {
            /*
             * Iterate through the collection
             */
            BOOL hasMore = FALSE;
            while (enumerator->HasMore(&hasMore), hasMore)
            {
                ComPtr<IMachine> machine;
                rc =enumerator->GetNext(machine.asOutParam());
                if ((SUCCEEDED(rc)) && machine)
                {
                    Bstr machineName;
                    Guid machineGUID;
                    Bstr settingsFilePath;
                    ULONG memorySize;
                    ULONG vramSize;
                    machine->COMGETTER(Name)(machineName.asOutParam());
                    machine->COMGETTER(Id)(machineGUID.asOutParam());
                    machine->COMGETTER(SettingsFilePath)(settingsFilePath.asOutParam());
                    machine->COMGETTER(MemorySize)(&memorySize);
                    machine->COMGETTER(VRAMSize)(&vramSize);
                    Utf8Str machineNameUtf8(machineName);
                    Utf8Str settingsFilePathUtf8(settingsFilePath);
                    RTPrintf("\tName:        %s\n", machineNameUtf8.raw());
                    RTPrintf("\tUUID:        %s\n", machineGUID.toString().raw());
                    RTPrintf("\tConfig file: %s\n", settingsFilePathUtf8.raw());
                    RTPrintf("\tMemory size: %uMB\n", memorySize);
                    RTPrintf("\tVRAM size:   %uMB\n\n", vramSize);
                }
            }
        }
        /* terminate application */
        goto leave;
    }

    /*
     * Do we have a name but no UUID?
     */
    if (vmName && uuid.isEmpty())
    {
        ComPtr<IMachine> aMachine;
        Bstr  bstrVMName = vmName;
        rc = virtualBox->FindMachine(bstrVMName, aMachine.asOutParam());
        if ((rc == S_OK) && aMachine)
        {
            aMachine->COMGETTER(Id)(uuid.asOutParam());
        }
        else
        {
            RTPrintf("Error: machine with the given ID not found!\n");
            goto leave;
        }
    }
    else if (uuid.isEmpty())
    {
        RTPrintf("Error: no machine specified!\n");
        goto leave;
    }

    rc = virtualBox->OpenSession(session, uuid);
    if (FAILED(rc))
    {
        com::ErrorInfo info;
        if (info.isFullAvailable())
            PrintError("Could not open VirtualBox session",
                    info.getText().raw(), info.getComponent().raw());
        goto leave;
    }
    if (!session)
    {
        RTPrintf("Could not open VirtualBox session!\n");
        goto leave;
    }
    sessionOpened = true;
    // get the VM we're dealing with
    session->COMGETTER(Machine)(gMachine.asOutParam());
    if (!gMachine)
    {
        com::ErrorInfo info;
        if (info.isFullAvailable())
            PrintError("Cannot start VM!",
                       info.getText().raw(), info.getComponent().raw());
        else
            RTPrintf("Error: given machine not found!\n");
        goto leave;
    }
    // get the VM console
    session->COMGETTER(Console)(gConsole.asOutParam());
    if (!gConsole)
    {
        RTPrintf("Given console not found!\n");
        goto leave;
    }

    /*
     * Are we supposed to use a different hard disk file?
     */
    if (hdaFile)
    {
        /*
         * Strategy: iterate through all registered hard disk
         * and see if one of them points to the same file. If
         * so, assign it. If not, register a new image and assing
         * it to the VM.
         */
        Bstr hdaFileBstr = hdaFile;
        ComPtr<IHardDisk> hardDisk;
        ComPtr<IVirtualDiskImage> vdi;
        virtualBox->FindVirtualDiskImage(hdaFileBstr, vdi.asOutParam());
        if (vdi)
        {
            vdi.queryInterfaceTo (hardDisk.asOutParam());
        }
        else
        {
            /* we've not found the image */
            RTPrintf("Registering hard disk image %s\n", hdaFile);
            virtualBox->OpenVirtualDiskImage (hdaFileBstr, vdi.asOutParam());
            if (vdi)
            {
                vdi.queryInterfaceTo (hardDisk.asOutParam());
                virtualBox->RegisterHardDisk (hardDisk);
            }
        }
        /* do we have the right image now? */
        if (hardDisk)
        {
            /*
             * Go and attach it!
             */
            Guid uuid;
            hardDisk->COMGETTER(Id)(uuid.asOutParam());
            gMachine->DetachHardDisk(DiskControllerType_IDE0Controller, 0);
            gMachine->AttachHardDisk(uuid, DiskControllerType_IDE0Controller, 0);
            /// @todo why is this attachment saved?
        }
        else
        {
            RTPrintf("Error: failed to mount the specified hard disk image!\n");
            goto leave;
        }
    }

    if (fdaFile)
    {
        ComPtr<IFloppyDrive> floppyDrive;
        gMachine->COMGETTER(FloppyDrive)(floppyDrive.asOutParam());
        Assert(floppyDrive);

        ComPtr<IFloppyImageCollection> collection;
        virtualBox->COMGETTER(FloppyImages)(collection.asOutParam());
        Assert(collection);
        ComPtr<IFloppyImageEnumerator> enumerator;
        collection->Enumerate(enumerator.asOutParam());
        Assert(enumerator);
        ComPtr<IFloppyImage> floppyImage;
        BOOL hasMore = false;
        while (enumerator->HasMore(&hasMore), hasMore)
        {
            enumerator->GetNext(floppyImage.asOutParam());
            Assert(floppyImage);
            Bstr file;
            floppyImage->COMGETTER(FilePath)(file.asOutParam());
            Assert(file);
            /// @todo this will not work on case insensitive systems if the casing does not match the registration!!!
            if (file == fdaFile)
                break;
            else
                floppyImage = NULL;
        }
        /* we've not found the image? */
        if (!floppyImage)
        {
            RTPrintf("Registering floppy disk image %s\n", fdaFile);
            Guid uuid;
            Bstr fileBstr = fdaFile;
            virtualBox->OpenFloppyImage (fileBstr, uuid, floppyImage.asOutParam());
            virtualBox->RegisterFloppyImage (floppyImage);
        }
        /* do we have the right image now? */
        if (floppyImage)
        {
            /*
             * Go and attach it!
             */
            Guid uuid;
            floppyImage->COMGETTER(Id)(uuid.asOutParam());
            floppyDrive->MountImage(uuid);
        }
        else
        {
            RTPrintf("Error: failed to mount the specified floppy disk image!\n");
            goto leave;
        }
    }

    /*
     * Are we supposed to use a different CDROM image?
     */
    if (cdromFile)
    {
        ComPtr<IDVDDrive> dvdDrive;
        gMachine->COMGETTER(DVDDrive)(dvdDrive.asOutParam());
        Assert(dvdDrive);

        /*
         * First special case 'none' to unmount
         */
        if (strcmp(cdromFile, "none") == 0)
        {
            dvdDrive->Unmount();
        }
        else
        {
            /*
             * Determine if it's a host device or ISO image
             */
            bool fHostDrive = false;
#ifdef __WIN__
            /* two characters with the 2nd being a colon */
            if ((strlen(cdromFile) == 2) && (cdromFile[1] == ':'))
            {
                cdromFile[0] = toupper(cdromFile[0]);
                fHostDrive = true;
            }
#else /* !__WIN__ */
            /* it has to start with /dev/ */
            if (strncmp(cdromFile, "/dev/", 5) == 0)
                fHostDrive = true;
#endif /* !__WIN__ */
            if (fHostDrive)
            {
                ComPtr<IHost> host;
                virtualBox->COMGETTER(Host)(host.asOutParam());
                ComPtr<IHostDVDDriveCollection> collection;
                host->COMGETTER(DVDDrives)(collection.asOutParam());
                ComPtr<IHostDVDDriveEnumerator> enumerator;
                collection->Enumerate(enumerator.asOutParam());
                ComPtr<IHostDVDDrive> hostDVDDrive;
                BOOL hasMore = FALSE;
                while (enumerator->HasMore(&hasMore), hasMore)
                {
                    enumerator->GetNext(hostDVDDrive.asOutParam());
                    Bstr driveName;
                    hostDVDDrive->COMGETTER(Name)(driveName.asOutParam());
                    Utf8Str driveNameUtf8 = driveName;
                    char *driveNameStr = (char*)driveNameUtf8.raw();
                    if (strcmp(driveNameStr, cdromFile) == 0)
                    {
                        rc = dvdDrive->CaptureHostDrive(hostDVDDrive);
                        if (rc != S_OK)
                        {
                            RTPrintf("Error: could not mount host DVD drive %s! rc = 0x%x\n", driveNameStr, rc);
                        }
                        break;
                    }
                }
                if (!hasMore)
                    RTPrintf("Error: did not recognize DVD drive '%s'!\n", cdromFile);
            }
            else
            {
                /*
                 * Same strategy as with the HDD images: check if already registered,
                 * if not, register on the fly.
                 */
                ComPtr<IDVDImageCollection> collection;
                virtualBox->COMGETTER(DVDImages)(collection.asOutParam());
                Assert(collection);
                ComPtr<IDVDImageEnumerator> enumerator;
                collection->Enumerate(enumerator.asOutParam());
                Assert(enumerator);
                ComPtr<IDVDImage> dvdImage;
                BOOL hasMore = false;
                while (enumerator->HasMore(&hasMore), hasMore)
                {
                    enumerator->GetNext(dvdImage.asOutParam());
                    Assert(dvdImage);
                    Bstr dvdImageFile;
                    dvdImage->COMGETTER(FilePath)(dvdImageFile.asOutParam());
                    Assert(dvdImageFile);
                    /// @todo not correct for case insensitive platforms (win32)
                    /// See comment on hdaFile.
                    if (dvdImageFile == cdromFile)
                        break;
                    else
                        dvdImage = NULL;
                }
                /* we've not found the image? */
                if (!dvdImage)
                {
                    RTPrintf("Registering ISO image %s\n", cdromFile);
                    Guid uuid; // the system will generate UUID
                    Bstr cdImageFileBstr = cdromFile;
                    virtualBox->OpenDVDImage(cdImageFileBstr, uuid, dvdImage.asOutParam());
                    rc = virtualBox->RegisterDVDImage(dvdImage);
                    if (!SUCCEEDED(rc))
                    {
                       RTPrintf("Image registration failed with %08X\n", rc);
                    }
                }
                /* do we have the right image now? */
                if (dvdImage)
                {
                    /* attach */
                    Guid uuid;
                    dvdImage->COMGETTER(Id)(uuid.asOutParam());
                    dvdDrive->MountImage(uuid);
                }
                else
                {
                    RTPrintf("Error: failed to mount the specified ISO image!\n");
                    goto leave;
                }
            }
        }
    }

    if (fDiscardState)
    {
        /*
         * If the machine is currently saved,
         * discard the saved state first.
         */
        MachineState_T machineState;
        gMachine->COMGETTER(State)(&machineState);
        if (machineState == MachineState_Saved)
        {
            CHECK_ERROR(gConsole, DiscardSavedState());
        }
        /*
         * If there are snapshots, discard the current state,
         * i.e. revert to the last snapshot.
         */
        ULONG cSnapshots;
        gMachine->COMGETTER(SnapshotCount)(&cSnapshots);
        if (cSnapshots)
        {
            gProgress = NULL;
            CHECK_ERROR(gConsole, DiscardCurrentState(gProgress.asOutParam()));
            rc = gProgress->WaitForCompletion(-1);
        }
    }

    // get the machine debugger (does not have to be there)
    gConsole->COMGETTER(Debugger)(gMachineDebugger.asOutParam());
    if (gMachineDebugger)
    {
        Log(("Machine debugger available!\n"));
    }
    gConsole->COMGETTER(Display)(gDisplay.asOutParam());
    if (!gDisplay)
    {
        RTPrintf("Error: could not get display object!\n");
        goto leave;
    }

    // set the boot drive
    if (bootDevice != DeviceType_NoDevice)
    {
        rc = gMachine->SetBootOrder(1, bootDevice);
        if (rc != S_OK)
        {
            RTPrintf("Error: could not set boot device, using default.\n");
        }
    }

    // set the memory size if not default
    if (memorySize)
    {
        rc = gMachine->COMSETTER(MemorySize)(memorySize);
        if (rc != S_OK)
        {
            ULONG ramSize = 0;
            gMachine->COMGETTER(MemorySize)(&ramSize);
            RTPrintf("Error: could not set memory size, using current setting of %d MBytes\n", ramSize);
        }
    }

    if (vramSize)
    {
        rc = gMachine->COMSETTER(VRAMSize)(vramSize);
        if (rc != S_OK)
        {
            gMachine->COMGETTER(VRAMSize)((ULONG*)&vramSize);
            RTPrintf("Error: could not set VRAM size, using current setting of %d MBytes\n", vramSize);
        }
    }

    // we're always able to process absolute mouse events and we prefer that
    gfAbsoluteMouseHost = TRUE;

#ifdef VBOX_WIN32_UI
    if (fWin32UI)
    {
        /* initialize the Win32 user interface inside which SDL will be embedded */
        if (initUI(fResizable))
            return 1;
    }
#endif

    // create our SDL framebuffer instance
    gpFrameBuffer = new VBoxSDLFB(fFullscreen, fResizable, fShowSDLConfig,
                                  fixedWidth, fixedHeight, fixedBPP);

    if (!gpFrameBuffer)
    {
        RTPrintf("Error: could not create framebuffer object!\n");
        goto leave;
    }
    if (!gpFrameBuffer->initialized())
        goto leave;
    gpFrameBuffer->AddRef();
    if (fFullscreen)
    {
        gpFrameBuffer->setFullscreen(true);
    }
#ifdef VBOX_SECURELABEL
    if (fSecureLabel)
    {
        if (!secureLabelFontFile)
        {
            RTPrintf("Error: no font file specified for secure label!\n");
            goto leave;
        }
        /* load the SDL_ttf library and get the required imports */
        int rcVBox;
        rcVBox = RTLdrLoad(LIBSDL_TTF_NAME, &gLibrarySDL_ttf);
        if (VBOX_SUCCESS(rcVBox))
            rcVBox = RTLdrGetSymbol(gLibrarySDL_ttf, "TTF_Init", (void**)&pTTF_Init);
        if (VBOX_SUCCESS(rcVBox))
            rcVBox = RTLdrGetSymbol(gLibrarySDL_ttf, "TTF_OpenFont", (void**)&pTTF_OpenFont);
        if (VBOX_SUCCESS(rcVBox))
            rcVBox = RTLdrGetSymbol(gLibrarySDL_ttf, "TTF_RenderUTF8_Solid", (void**)&pTTF_RenderUTF8_Solid);
        if (VBOX_SUCCESS(rcVBox))
            rcVBox = RTLdrGetSymbol(gLibrarySDL_ttf, "TTF_CloseFont", (void**)&pTTF_CloseFont);
        if (VBOX_SUCCESS(rcVBox))
            rcVBox = RTLdrGetSymbol(gLibrarySDL_ttf, "TTF_Quit", (void**)&pTTF_Quit);
        if (VBOX_SUCCESS(rcVBox))
            rcVBox = gpFrameBuffer->initSecureLabel(SECURE_LABEL_HEIGHT, secureLabelFontFile, secureLabelPointSize);
        if (VBOX_FAILURE(rcVBox))
        {
            RTPrintf("Error: could not initialize secure labeling: rc = %Vrc\n", rcVBox);
            goto leave;
        }
        Bstr key = VBOXSDL_SECURELABEL_EXTRADATA;
        Bstr label;
        gMachine->GetExtraData(key, label.asOutParam());
        Utf8Str labelUtf8 = label;
        /*
         * Now update the label
         */
        gpFrameBuffer->setSecureLabelColor(secureLabelColorFG, secureLabelColorBG);
        gpFrameBuffer->setSecureLabelText(labelUtf8.raw());
    }
#endif

    // register our framebuffer
    rc = gDisplay->RegisterExternalFramebuffer(gpFrameBuffer);
    if (rc != S_OK)
    {
        RTPrintf("Error: could not register framebuffer object!\n");
        goto leave;
    }

    // register a callback for global events
    callback = new VBoxSDLCallback();
    callback->AddRef();
    virtualBox->RegisterCallback(callback);

    // register a callback for machine events
    consoleCallback = new VBoxSDLConsoleCallback();
    consoleCallback->AddRef();
    gConsole->RegisterCallback(consoleCallback);
    // until we've tried to to start the VM, ignore power off events
    consoleCallback->ignorePowerOffEvents(true);

#ifdef __LINUX__
    /*
     * Do we have a TAP device name or file descriptor? If so, communicate
     * it to the network adapter so that it doesn't allocate a new one
     * in case TAP is already configured.
     */
    {
        ComPtr<INetworkAdapter> networkAdapter;
        for (ULONG i = 0; i < NetworkAdapterCount; i++)
        {
            if (tapdev[i] || tapfd[i])
            {
                gMachine->GetNetworkAdapter(i, networkAdapter.asOutParam());
                if (networkAdapter)
                {
                    NetworkAttachmentType_T attachmentType;
                    networkAdapter->COMGETTER(AttachmentType)(&attachmentType);
                    if (attachmentType == NetworkAttachmentType_HostInterfaceNetworkAttachment)
                    {
                        if (tapdev[i])
                            networkAdapter->COMSETTER(HostInterface)(tapdev[i]);
                        else
                            networkAdapter->COMSETTER(TAPFileDescriptor)(tapfd[i]);
                    }
                    else
                    {
                        RTPrintf("Warning: network adapter %d is not configured for TAP. Command ignored!\n", i + 1);
                    }
                }
                else
                {
                    /* warning */
                    RTPrintf("Warning: network adapter %d not defined. Command ignored!\n", i + 1);
                }
            }
        }
    }
#endif /* __LINUX__ */

#ifdef VBOX_VRDP
    if (portVRDP != ~0)
    {
        rc = gMachine->COMGETTER(VRDPServer)(gVrdpServer.asOutParam());
        AssertMsg((rc == S_OK) && gVrdpServer, ("Could not get VRDP Server! rc = 0x%x\n", rc));
        if (gVrdpServer)
        {
            // has a non standard VRDP port been requested?
            if (portVRDP > 0)
            {
                rc = gVrdpServer->COMSETTER(Port)(portVRDP);
                if (rc != S_OK)
                {
                    RTPrintf("Error: could not set VRDP port! rc = 0x%x\n", rc);
                    goto leave;
                }
            }
            // now enable VRDP
            rc = gVrdpServer->COMSETTER(Enabled)(TRUE);
            if (rc != S_OK)
            {
                RTPrintf("Error: could not enable VRDP server! rc = 0x%x\n", rc);
                goto leave;
            }
        }
    }
#endif

    rc = E_FAIL;
#ifdef VBOXSDL_ADVANCED_OPTIONS
    if (fRawR0 != ~0U)
    {
        if (!gMachineDebugger)
        {
            RTPrintf("Error: No debugger object; -%srawr0 cannot be executed!\n", fRawR0 ? "" : "no");
            goto leave;
        }
        gMachineDebugger->COMSETTER(RecompileSupervisor)(!fRawR0);
    }
    if (fRawR3 != ~0U)
    {
        if (!gMachineDebugger)
        {
            RTPrintf("Error: No debugger object; -%srawr3 cannot be executed!\n", fRawR0 ? "" : "no");
            goto leave;
        }
        gMachineDebugger->COMSETTER(RecompileUser)(!fRawR3);
    }
    if (fPATM != ~0U)
    {
        if (!gMachineDebugger)
        {
            RTPrintf("Error: No debugger object; -%spatm cannot be executed!\n", fRawR0 ? "" : "no");
            goto leave;
        }
        gMachineDebugger->COMSETTER(PATMEnabled)(fPATM);
    }
    if (fCSAM != ~0U)
    {
        if (!gMachineDebugger)
        {
            RTPrintf("Error: No debugger object; -%scsam cannot be executed!\n", fRawR0 ? "" : "no");
            goto leave;
        }
        gMachineDebugger->COMSETTER(CSAMEnabled)(fCSAM);
    }
    if (fHWVirt != TriStateBool_Default)
    {
        gMachine->COMSETTER(HWVirtExEnabled)(fHWVirt);
    }
#endif /* VBOXSDL_ADVANCED_OPTIONS */

    /* start with something in the titlebar */
    UpdateTitlebar(TITLEBAR_NORMAL);

    /* memorize the default cursor */
    gpDefaultCursor = SDL_GetCursor();

#ifdef __LINUX__
    /* Get Window Manager info. We only need the X11 display. */
    SDL_VERSION(&gSdlInfo.version);
    if (!SDL_GetWMInfo(&gSdlInfo))
    {
        RTPrintf("Error: could not get SDL Window Manager info!\n");
        goto leave;
    }

    /* SDL uses its own (plain) default cursor. Use the left arrow cursor instead which might look
     * much better if a mouse cursor theme is installed. */
    gpDefaultOrigX11Cursor = *(Cursor*)gpDefaultCursor->wm_cursor;
    *(Cursor*)gpDefaultCursor->wm_cursor = XCreateFontCursor(gSdlInfo.info.x11.display, XC_left_ptr);
    SDL_SetCursor(gpDefaultCursor);
#endif /* __LINUX__ */

    /* create a fake empty cursor */
    {
        uint8_t cursorData[1] = {0};
        gpCustomCursor = SDL_CreateCursor(cursorData, cursorData, 8, 1, 0, 0);
        gpCustomOrigWMcursor = gpCustomCursor->wm_cursor;
        gpCustomCursor->wm_cursor = NULL;
    }

    /*
     * Register our user signal handler.
     */
#ifdef __LINUX__
    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction (SIGUSR1, &sa, NULL);
#endif /* __LINUX__ */

    /*
     * Start the VM execution thread. This has to be done
     * asynchronously as powering up can take some time
     * (accessing devices such as the host DVD drive). In
     * the meantime, we have to service the SDL event loop.
     */
    SDL_Event event;

    LogFlow(("Powering up the VM...\n"));
    rc = gConsole->PowerUp(gProgress.asOutParam());
    if (rc != S_OK)
    {
        com::ErrorInfo info(gConsole);
        if (info.isBasicAvailable())
            PrintError("Failed to power up VM", info.getText().raw());
        else
            RTPrintf("Error: failed to power up VM! No error text available.\n");
        goto leave;
    }

#ifdef __LINUX__
    /*
     * Before we starting to do stuff, we have to launch the XPCOM
     * event queue thread. It will wait for events and send messages
     * to the SDL thread. After having done this, we should fairly
     * quickly start to process the SDL event queue as an XPCOM
     * event storm might arrive. Stupid SDL has a ridiculously small
     * event queue buffer!
     */
    startXPCOMEventQueueThread(eventQ->GetEventQueueSelectFD());
#endif /** __LINUX__ */

    /* termination flag */
    bool fTerminateDuringStartup;
    fTerminateDuringStartup = false;

    /* start regular timer so we don't starve in the event loop */
    SDL_TimerID sdlTimer;
    sdlTimer = SDL_AddTimer(100, StartupTimer, NULL);

    /* loop until the powerup processing is done */
    MachineState_T machineState;
    do
    {
        rc = gMachine->COMGETTER(State)(&machineState);
        if (    rc == S_OK
            &&  (   machineState == MachineState_Starting
                 || machineState == MachineState_Restoring))
        {
            /*
             * wait for the next event. This is uncritical as
             * power up guarantees to change the machine state
             * to either running or aborted and a machine state
             * change will send us an event. However, we have to
             * service the XPCOM event queue!
             */
#ifdef __LINUX__
            if (!fXPCOMEventThreadSignaled)
            {
                signalXPCOMEventQueueThread();
                fXPCOMEventThreadSignaled = true;
            }
#endif
            /*
             * Wait for SDL events.
             */
            if (SDL_WaitEvent(&event))
            {
                switch (event.type)
                {
                    /*
                     * Timer event. Used to have the titlebar updated.
                     */
                    case SDL_USER_EVENT_TIMER:
                    {
                        /*
                         * Update the title bar.
                         */
                        UpdateTitlebar(TITLEBAR_STARTUP);
                        break;
                    }

                    /*
                     * User specific resize event.
                     */
                    case SDL_USER_EVENT_RESIZE:
                    {
                        LogFlow(("SDL_USER_EVENT_RESIZE\n"));
                        gpFrameBuffer->resizeGuest();
                        /* notify the display that the resize has been completed */
                        gDisplay->ResizeCompleted();
                        break;
                    }

#ifdef __LINUX__
                    /*
                     * User specific XPCOM event queue event
                     */
                    case SDL_USER_EVENT_XPCOM_EVENTQUEUE:
                    {
                        LogFlow(("SDL_USER_EVENT_XPCOM_EVENTQUEUE: processing XPCOM event queue...\n"));
                        eventQ->ProcessPendingEvents();
                        signalXPCOMEventQueueThread();
                        break;
                    }
#endif /* __LINUX__ */

                    /*
                     * Termination event from the on state change callback.
                     */
                    case SDL_USER_EVENT_TERMINATE:
                    {
                        if (event.user.code != VBOXSDL_TERM_NORMAL)
                        {
                            com::ProgressErrorInfo info(gProgress);
                            if (info.isBasicAvailable())
                                PrintError("Failed to power up VM", info.getText().raw());
                            else
                                RTPrintf("Error: failed to power up VM! No error text available.\n");
                        }
                        fTerminateDuringStartup = true;
                        break;
                    }

                    default:
                    {
                        LogBird(("VBoxSDL: Unknown SDL event %d (pre)\n", event.type));
                        break;
                    }
                }

            }
        }
    } while (   rc == S_OK
             && (   machineState == MachineState_Starting
                 || machineState == MachineState_Restoring));

    /* kill the timer again */
    SDL_RemoveTimer(sdlTimer);
    sdlTimer = 0;

    /* are we supposed to terminate the process? */
    if (fTerminateDuringStartup)
        goto leave;

    /* did the power up succeed? */
    if (machineState != MachineState_Running)
    {
        com::ProgressErrorInfo info(gProgress);
        if (info.isBasicAvailable())
            PrintError("Failed to power up VM", info.getText().raw());
        else
            RTPrintf("Error: failed to power up VM! No error text available (rc = 0x%x state = %d)\n", rc, machineState);
        goto leave;
    }

    // accept power off events from now on because we're running
    // note that there's a possible race condition here...
    consoleCallback->ignorePowerOffEvents(false);

    rc = gConsole->COMGETTER(Keyboard)(gKeyboard.asOutParam());
    if (!gKeyboard)
    {
        RTPrintf("Error: could not get keyboard object!\n");
        goto leave;
    }
    gConsole->COMGETTER(Mouse)(gMouse.asOutParam());
    if (!gMouse)
    {
        RTPrintf("Error: could not get mouse object!\n");
        goto leave;
    }

    UpdateTitlebar(TITLEBAR_NORMAL);

    /*
     * Enable keyboard repeats
     */
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

    /*
     * Main event loop
     */
#ifdef __LINUX__
    if (!fXPCOMEventThreadSignaled)
    {
        signalXPCOMEventQueueThread();
    }
#endif
    LogFlow(("VBoxSDL: Entering big event loop\n"));
    while (SDL_WaitEvent(&event))
    {
        switch (event.type)
        {
            /*
             * The screen needs to be repainted.
             */
            case SDL_VIDEOEXPOSE:
            {
                /// @todo that somehow doesn't seem to work!
                gpFrameBuffer->repaint();
                break;
            }

            /*
             * Keyboard events.
             */
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                SDLKey ksym = event.key.keysym.sym;

                switch (enmHKeyState)
                {
                    case HKEYSTATE_NORMAL:
                    {
                        if (   event.type == SDL_KEYDOWN
                            && ksym != SDLK_UNKNOWN
                            && (ksym == gHostKeySym1 || ksym == gHostKeySym2))
                        {
                            EvHKeyDown1  = event;
                            enmHKeyState = ksym == gHostKeySym1 ? HKEYSTATE_DOWN_1ST
                                                                : HKEYSTATE_DOWN_2ND;
                            break;
                        }
                        ProcessKey(&event.key);
                        break;
                    }

                    case HKEYSTATE_DOWN_1ST:
                    case HKEYSTATE_DOWN_2ND:
                    {
                        if (gHostKeySym2 != SDLK_UNKNOWN)
                        {
                            if (   event.type == SDL_KEYDOWN
                                && ksym != SDLK_UNKNOWN
                                && (   enmHKeyState == HKEYSTATE_DOWN_1ST && ksym == gHostKeySym2
                                    || enmHKeyState == HKEYSTATE_DOWN_2ND && ksym == gHostKeySym1))
                            {
                                EvHKeyDown2  = event;
                                enmHKeyState = HKEYSTATE_DOWN;
                                break;
                            }
                            enmHKeyState = event.type == SDL_KEYUP ? HKEYSTATE_NORMAL 
                                                                 : HKEYSTATE_NOT_IT;
                            ProcessKey(&EvHKeyDown1.key);
                            ProcessKey(&event.key);
                            break;
                        }
                        /* fall through if no two-key sequence is used */
                    }

                    case HKEYSTATE_DOWN:
                    {
                        if (event.type == SDL_KEYDOWN)
                        {
                            /* potential host key combination, try execute it */
                            int rc = HandleHostKey(&event.key);
                            if (rc == VINF_SUCCESS)
                            {
                                enmHKeyState = HKEYSTATE_USED;
                                break;
                            }
                            if (VBOX_SUCCESS(rc))
                                goto leave;
                        }
                        else /* SDL_KEYUP */
                        {
                            if (   ksym != SDLK_UNKNOWN
                                && (ksym == gHostKeySym1 || ksym == gHostKeySym2))
                            {
                                /* toggle grabbing state */
                                if (!gfGrabbed)
                                    InputGrabStart();
                                else
                                    InputGrabEnd();

                                /* SDL doesn't always reset the keystates, correct it */
                                ResetKeys();
                                enmHKeyState = HKEYSTATE_NORMAL;
                                break;
                            }
                        }

                        /* not host key */
                        enmHKeyState = HKEYSTATE_NOT_IT;
                        ProcessKey(&EvHKeyDown1.key);
                        if (gHostKeySym2 != SDLK_UNKNOWN)
                            ProcessKey(&EvHKeyDown2.key);
                        ProcessKey(&event.key);
                        break;
                    }

                    case HKEYSTATE_USED:
                    {
                        if ((SDL_GetModState() & ~(KMOD_MODE | KMOD_NUM | KMOD_RESERVED)) == 0)
                            enmHKeyState = HKEYSTATE_NORMAL;
                        if (event.type == SDL_KEYDOWN)
                        {
                            int rc = HandleHostKey(&event.key);
                            if (VBOX_SUCCESS(rc) && rc != VINF_SUCCESS)
                                goto leave;
                        }
                        break;
                    }

                    default:
                        AssertMsgFailed(("enmHKeyState=%d\n", enmHKeyState));
                        /* fall thru */
                    case HKEYSTATE_NOT_IT:
                    {
                        if ((SDL_GetModState() & ~(KMOD_MODE | KMOD_NUM | KMOD_RESERVED)) == 0)
                            enmHKeyState = HKEYSTATE_NORMAL;
                        ProcessKey(&event.key);
                        break;
                    }
                } /* state switch */
                break;
            }

            /*
             * The window was closed.
             */
            case SDL_QUIT:
            {
                goto leave;
                break;
            }

            /*
             * The mouse has moved
             */
            case SDL_MOUSEMOTION:
            {
                if (gfGrabbed || UseAbsoluteMouse())
                {
                    SendMouseEvent(0, 0, 0);
                }
                break;
            }

            /*
             * A mouse button has been clicked or released.
             */
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            {
                SDL_MouseButtonEvent *bev = &event.button;
                /* don't grab on mouse click if we have guest additions */
                if (!gfGrabbed && !UseAbsoluteMouse() && gfGrabOnMouseClick)
                {
                    if (event.type == SDL_MOUSEBUTTONDOWN && (bev->state & SDL_BUTTON_LMASK))
                    {
                        /* start grabbing all events */
                        InputGrabStart();
                    }
                }
                else
                {
                    int dz = bev->button == SDL_BUTTON_WHEELUP
                                         ? -1
                                         : bev->button == SDL_BUTTON_WHEELDOWN
                                                       ? +1
                                                       :  0;

                    /* end host key combination (CTRL+MouseButton) */
                    switch (enmHKeyState)
                    {
                        case HKEYSTATE_DOWN_1ST:
                        case HKEYSTATE_DOWN_2ND:
                            enmHKeyState = HKEYSTATE_NOT_IT;
                            ProcessKey(&EvHKeyDown1.key);
                            break;
                        case HKEYSTATE_DOWN:
                            enmHKeyState = HKEYSTATE_NOT_IT;
                            ProcessKey(&EvHKeyDown1.key);
                            if (gHostKeySym2 != SDLK_UNKNOWN)
                                ProcessKey(&EvHKeyDown2.key);
                            break;
                        default:
                            break;
                    }

                    SendMouseEvent(dz, event.type == SDL_MOUSEBUTTONDOWN, bev->button);
                }
                break;
            }

            /*
             * The window has gained or lost focus.
             */
            case SDL_ACTIVEEVENT:
            {
                /*
                 * There is a strange behaviour in SDL when running without a window
                 * manager: When SDL_WM_GrabInput(SDL_GRAB_ON) is called we receive two
                 * consecutive events SDL_ACTIVEEVENTs (input lost, input gained).
                 * Asking SDL_GetAppState() seems the better choice.
                 */
                if (gfGrabbed && (SDL_GetAppState() & SDL_APPINPUTFOCUS) == 0)
                {
                    /*
                     * another window has stolen the (keyboard) input focus
                     */
                    InputGrabEnd();
                }
                break;
            }

            /*
             * The SDL window was resized
             */
            case SDL_VIDEORESIZE:
            {
                if (gDisplay)
                {
#ifdef VBOX_SECURELABEL
                    /* communicate the resize event to the guest */
                    gDisplay->SetVideoModeHint(event.resize.w, RT_MAX(0, event.resize.h - SECURE_LABEL_HEIGHT), 0);
#else
                    /* communicate the resize event to the guest */
                    gDisplay->SetVideoModeHint(event.resize.w, event.resize.h, 0);
#endif
                }
                break;
            }

            /*
             * User specific update event.
             */
            /** @todo use a common user event handler so that SDL_PeepEvents() won't
             * possibly remove other events in the queue!
             */
            case SDL_USER_EVENT_UPDATERECT:
            {
                /*
                 * Decode event parameters.
                 */
                #define DECODEX(event) ((intptr_t)(event).user.data1 >> 16)
                #define DECODEY(event) ((intptr_t)(event).user.data1 & 0xFFFF)
                #define DECODEW(event) ((intptr_t)(event).user.data2 >> 16)
                #define DECODEH(event) ((intptr_t)(event).user.data2 & 0xFFFF)
                int x = DECODEX(event);
                int y = DECODEY(event);
                int w = DECODEW(event);
                int h = DECODEH(event);
                LogFlow(("SDL_USER_EVENT_UPDATERECT: x = %d, y = %d, w = %d, h = %d\n",
                        x, y, w, h));

                Assert(gpFrameBuffer);
                gpFrameBuffer->update(x, y, w, h, true /* fGuestRelative */);

                #undef DECODEX
                #undef DECODEY
                #undef DECODEW
                #undef DECODEH
                break;
            }

            /*
             * User specific resize event.
             */
            case SDL_USER_EVENT_RESIZE:
            {
                LogFlow(("SDL_USER_EVENT_RESIZE\n"));
                gpFrameBuffer->resizeGuest();
                /* notify the display that the resize has been completed */
                gDisplay->ResizeCompleted();
                break;
            }

#ifdef __LINUX__
            /*
             * User specific XPCOM event queue event
             */
            case SDL_USER_EVENT_XPCOM_EVENTQUEUE:
            {
                LogFlow(("SDL_USER_EVENT_XPCOM_EVENTQUEUE: processing XPCOM event queue...\n"));
                eventQ->ProcessPendingEvents();
                signalXPCOMEventQueueThread();
                break;
            }
#endif /* __LINUX__ */

            /*
             * User specific update title bar notification event
             */
            case SDL_USER_EVENT_UPDATE_TITLEBAR:
            {
                UpdateTitlebar(TITLEBAR_NORMAL);
                break;
            }

            /*
             * User specific termination event
             */
            case SDL_USER_EVENT_TERMINATE:
            {
                if (event.user.code != VBOXSDL_TERM_NORMAL)
                    RTPrintf("Error: VM terminated abnormally!\n");
                goto leave;
            }

#ifdef VBOX_SECURELABEL
            /*
             * User specific secure label update event
             */
            case SDL_USER_EVENT_SECURELABEL_UPDATE:
            {
                /*
                 * Query the new label text
                 */
                Bstr key = VBOXSDL_SECURELABEL_EXTRADATA;
                Bstr label;
                gMachine->GetExtraData(key, label.asOutParam());
                Utf8Str labelUtf8 = label;
                /*
                 * Now update the label
                 */
                gpFrameBuffer->setSecureLabelText(labelUtf8.raw());
                break;
            }
#endif /* VBOX_SECURELABEL */

            /*
             * User specific pointer shape change event
             */
            case SDL_USER_EVENT_POINTER_CHANGE:
            {
                PointerShapeChangeData *data = (PointerShapeChangeData *) event.user.data1;
                SetPointerShape (data);
                delete data;
                break;
            }

            /*
             * User specific guest capabilities changed
             */
            case SDL_USER_EVENT_GUEST_CAP_CHANGED:
            {
                HandleGuestCapsChanged();
                break;
            }

            default:
            {
                LogBird(("unknown SDL event %d\n", event.type));
                break;
            }
        }
    }

leave:
    LogFlow(("leaving...\n"));
#ifdef __LINUX__
    /* make sure the XPCOM event queue thread doesn't do anything harmful */
    terminateXPCOMQueueThread();
#endif /* __LINUX__ */

#ifdef VBOX_VRDP
    if (gVrdpServer)
        rc = gVrdpServer->COMSETTER(Enabled)(FALSE);
#endif

    /*
     * Get the machine state.
     */
    if (gMachine)
        gMachine->COMGETTER(State)(&machineState);
    else
        machineState = MachineState_Aborted;

    /*
     * Turn off the VM if it's running
     */
    if (   gConsole
        && machineState == MachineState_Running)
    {
        consoleCallback->ignorePowerOffEvents(true);
        rc = gConsole->PowerDown();
        if (FAILED(rc))
        {
            com::ErrorInfo info;
            if (info.isFullAvailable())
                PrintError("Failed to power down VM",
                           info.getText().raw(), info.getComponent().raw());
            else
                RTPrintf("Failed to power down virtual machine! No error information available (rc = 0x%x).\n", rc);
            break;
        }
    }

    /*
     * Now we discard all settings so that our changes will
     * not be flushed to the permanent configuration
     */
    if (   gMachine
        && machineState != MachineState_Saved)
    {
        rc = gMachine->DiscardSettings();
        AssertComRC(rc);
    }

    /* close the session */
    if (sessionOpened)
    {
        rc = session->Close();
        AssertComRC(rc);
    }

    /* restore the default cursor and free the custom one if any */
    if (gpDefaultCursor)
    {
#ifdef __LINUX__
        Cursor pDefaultTempX11Cursor = *(Cursor*)gpDefaultCursor->wm_cursor;
        *(Cursor*)gpDefaultCursor->wm_cursor = gpDefaultOrigX11Cursor;
#endif /* __LNUX__ */
        SDL_SetCursor(gpDefaultCursor);
#ifdef __LINUX__
        XFreeCursor(gSdlInfo.info.x11.display, pDefaultTempX11Cursor);
#endif /* __LINUX__ */
    }

    if (gpCustomCursor)
    {
        WMcursor *pCustomTempWMCursor = gpCustomCursor->wm_cursor;
        gpCustomCursor->wm_cursor = gpCustomOrigWMcursor;
        SDL_FreeCursor(gpCustomCursor);
        if (pCustomTempWMCursor)
        {
#if defined (__WIN__)
            ::DestroyCursor(*(HCURSOR *) pCustomTempWMCursor);
#elif defined (__LINUX__)
            XFreeCursor(gSdlInfo.info.x11.display, *(Cursor *) pCustomTempWMCursor);
#endif
            free(pCustomTempWMCursor);
        }
    }

    LogFlow(("Releasing mouse, keyboard, vrdpserver, display, console...\n"));
    gMouse = NULL;
    gKeyboard = NULL;
    gVrdpServer = NULL;
    gDisplay = NULL;
    gConsole = NULL;
    gMachineDebugger = NULL;
    gProgress = NULL;
    // we can only uninitialize SDL here because it is not threadsafe
    if (gpFrameBuffer)
    {
        LogFlow(("Releasing framebuffer...\n"));
        gpFrameBuffer->uninit();
        gpFrameBuffer->Release();
    }
#ifdef VBOX_SECURELABEL
    /* must do this after destructing the framebuffer */
    if (gLibrarySDL_ttf)
        RTLdrClose(gLibrarySDL_ttf);
#endif
    LogFlow(("Releasing machine, session...\n"));
    gMachine = NULL;
    session = NULL;
    LogFlow(("Releasing callback handlers...\n"));
    if (callback)
        callback->Release();
    if (consoleCallback)
        consoleCallback->Release();

    LogFlow(("Releasing VirtualBox object...\n"));
    virtualBox = NULL;

    // end "all-stuff" scope
    ////////////////////////////////////////////////////////////////////////////
    }
    while (0);

    LogFlow(("Uninitializing COM...\n"));
    com::Shutdown();

    LogFlow(("Returning from main()!\n"));
    RTLogFlush(NULL);
    return FAILED (rc) ? 1 : 0;
}

/**
 * Returns whether the absolute mouse is in use, i.e. both host
 * and guest have opted to enable it.
 *
 * @returns bool Flag whether the absolute mouse is in use
 */
static bool UseAbsoluteMouse(void)
{
    return (gfAbsoluteMouseHost && gfAbsoluteMouseGuest);
}

/**
 * Converts an SDL keyboard eventcode to a XT scancode.
 *
 * @returns XT scancode
 * @param   ev SDL scancode
 */
static uint8_t Keyevent2Keycode(const SDL_KeyboardEvent *ev)
{
    int keycode;

    // start with the scancode determined by SDL
    keycode = ev->keysym.scancode;

#ifdef __LINUX__
    // workaround for SDL keyboard translation issues on Linux
    // keycodes > 0x80 are sent as 0xe0 keycode
    static const uint8_t x_keycode_to_pc_keycode[61] =
    {
       0xc7,      /*  97  Home   */
       0xc8,      /*  98  Up     */
       0xc9,      /*  99  PgUp   */
       0xcb,      /* 100  Left   */
       0x4c,      /* 101  KP-5   */
       0xcd,      /* 102  Right  */
       0xcf,      /* 103  End    */
       0xd0,      /* 104  Down   */
       0xd1,      /* 105  PgDn   */
       0xd2,      /* 106  Ins    */
       0xd3,      /* 107  Del    */
       0x9c,      /* 108  Enter  */
       0x9d,      /* 109  Ctrl-R */
       0x0,       /* 110  Pause  */
       0xb7,      /* 111  Print  */
       0xb5,      /* 112  Divide */
       0xb8,      /* 113  Alt-R  */
       0xc6,      /* 114  Break  */
       0xdb,      /* 115  Win Left */
       0xdc,      /* 116  Win Right */
       0xdd,      /* 117  Win Menu */
       0x0,       /* 118 */
       0x0,       /* 119 */
       0x70,      /* 120 Hiragana_Katakana */
       0x0,       /* 121 */
       0x0,       /* 122 */
       0x73,      /* 123 backslash */
       0x0,       /* 124 */
       0x0,       /* 125 */
       0x0,       /* 126 */
       0x0,       /* 127 */
       0x0,       /* 128 */
       0x79,      /* 129 Henkan */
       0x0,       /* 130 */
       0x7b,      /* 131 Muhenkan */
       0x0,       /* 132 */
       0x7d,      /* 133 Yen */
       0x0,       /* 134 */
       0x0,       /* 135 */
       0x47,      /* 136 KP_7 */
       0x48,      /* 137 KP_8 */
       0x49,      /* 138 KP_9 */
       0x4b,      /* 139 KP_4 */
       0x4c,      /* 140 KP_5 */
       0x4d,      /* 141 KP_6 */
       0x4f,      /* 142 KP_1 */
       0x50,      /* 143 KP_2 */
       0x51,      /* 144 KP_3 */
       0x52,      /* 145 KP_0 */
       0x53,      /* 146 KP_. */
       0x47,      /* 147 KP_HOME */
       0x48,      /* 148 KP_UP */
       0x49,      /* 149 KP_PgUp */
       0x4b,      /* 150 KP_Left */
       0x4c,      /* 151 KP_ */
       0x4d,      /* 152 KP_Right */
       0x4f,      /* 153 KP_End */
       0x50,      /* 154 KP_Down */
       0x51,      /* 155 KP_PgDn */
       0x52,      /* 156 KP_Ins */
       0x53,      /* 157 KP_Del */
    };

    if (keycode < 9)
    {
        keycode = 0;
    }
    else if (keycode < 97)
    {
        // just an offset (Xorg MIN_KEYCODE)
        keycode -= 8;
    }
    else if (keycode < 158)
    {
        // apply conversion table
        keycode = x_keycode_to_pc_keycode[keycode - 97];
    }
    else
    {
        keycode = 0;
    }
#endif
    return keycode;
}

/**
 * Releases any modifier keys that are currently in pressed state.
 */
static void ResetKeys(void)
{
    int i;

    if (!gKeyboard)
        return;

    for(i = 0; i < 256; i++)
    {
        if (gaModifiersState[i])
        {
            if (i & 0x80)
                gKeyboard->PutScancode(0xe0);
            gKeyboard->PutScancode(i | 0x80);
            gaModifiersState[i] = 0;
        }
    }
}

/**
 * Keyboard event handler.
 *
 * @param ev SDL keyboard event.
 */
static void ProcessKey(SDL_KeyboardEvent *ev)
{
#if defined(DEBUG) || defined(VBOX_WITH_STATISTICS)
    if (gMachineDebugger && ev->type == SDL_KEYDOWN)
    {
        // first handle the debugger hotkeys
        uint8_t *keystate = SDL_GetKeyState(NULL);
#if 0
        // CTRL+ALT+Fn is not free on Linux hosts with Xorg ..
        if (keystate[SDLK_LALT] && !keystate[SDLK_LCTRL])
#else
        if (keystate[SDLK_LALT] && keystate[SDLK_LCTRL])
#endif
        {
            switch (ev->keysym.sym)
            {
                // pressing CTRL+ALT+F11 dumps the statistics counter
                case SDLK_F12:
                    RTPrintf("ResetStats\n"); /* Visual feedback in console window */
                    gMachineDebugger->ResetStats();
                    break;
                // pressing CTRL+ALT+F12 resets all statistics counter
                case SDLK_F11:
                    gMachineDebugger->DumpStats();
                    RTPrintf("DumpStats\n");  /* Vistual feedback in console window */
                    break;
                default:
                    break;
            }
        }
#if 1
        else if (keystate[SDLK_LALT] && !keystate[SDLK_LCTRL])
        {
            switch (ev->keysym.sym)
            {
                // pressing Alt-F12 toggles the supervisor recompiler
                case SDLK_F12:
                    {
                        BOOL recompileSupervisor;
                        gMachineDebugger->COMGETTER(RecompileSupervisor)(&recompileSupervisor);
                        gMachineDebugger->COMSETTER(RecompileSupervisor)(!recompileSupervisor);
                        break;
                    }
                    // pressing Alt-F11 toggles the user recompiler
                case SDLK_F11:
                    {
                        BOOL recompileUser;
                        gMachineDebugger->COMGETTER(RecompileUser)(&recompileUser);
                        gMachineDebugger->COMSETTER(RecompileUser)(!recompileUser);
                        break;
                    }
                    // pressing Alt-F10 toggles the patch manager
                case SDLK_F10:
                    {
                        BOOL patmEnabled;
                        gMachineDebugger->COMGETTER(PATMEnabled)(&patmEnabled);
                        gMachineDebugger->COMSETTER(PATMEnabled)(!patmEnabled);
                        break;
                    }
                    // pressing Alt-F9 toggles CSAM
                case SDLK_F9:
                    {
                        BOOL csamEnabled;
                        gMachineDebugger->COMGETTER(CSAMEnabled)(&csamEnabled);
                        gMachineDebugger->COMSETTER(CSAMEnabled)(!csamEnabled);
                        break;
                    }
                    // pressing Alt-F8 toggles singlestepping mode
                case SDLK_F8:
                    {
                        BOOL singlestepEnabled;
                        gMachineDebugger->COMGETTER(Singlestep)(&singlestepEnabled);
                        gMachineDebugger->COMSETTER(Singlestep)(!singlestepEnabled);
                        break;
                    }
                default:
                    break;
            }
        }
#endif
        // pressing Ctrl-F12 toggles the logger
        else if ((keystate[SDLK_RCTRL] || keystate[SDLK_LCTRL]) && ev->keysym.sym == SDLK_F12)
        {
            BOOL logEnabled = TRUE;
            gMachineDebugger->COMGETTER(LogEnabled)(&logEnabled);
            gMachineDebugger->COMSETTER(LogEnabled)(!logEnabled);
#ifdef DEBUG_bird
            return;
#endif
        }
        // pressing F12 sets a logmark
        else if (ev->keysym.sym == SDLK_F12)
        {
            RTLogPrintf("****** LOGGING MARK ******\n");
            RTLogFlush(NULL);
        }
        // now update the titlebar flags
        UpdateTitlebar(TITLEBAR_NORMAL);
    }
#endif // DEBUG || VBOX_WITH_STATISTICS

    // the pause key is the weirdest, needs special handling
    if (ev->keysym.sym == SDLK_PAUSE)
    {
        int v = 0;
        if (ev->type == SDL_KEYUP)
            v |= 0x80;
        gKeyboard->PutScancode(0xe1);
        gKeyboard->PutScancode(0x1d | v);
        gKeyboard->PutScancode(0x45 | v);
        return;
    }

    /*
     * Perform SDL key event to scancode conversion
     */
    int keycode = Keyevent2Keycode(ev);

    switch(keycode)
    {
        case 0x00:
        {
            /* sent when leaving window: reset the modifiers state */
            ResetKeys();
            return;
        }

        case 0x2a:  /* Left Shift */
        case 0x36:  /* Right Shift */
        case 0x1d:  /* Left CTRL */
        case 0x9d:  /* Right CTRL */
        case 0x38:  /* Left ALT */
        case 0xb8:  /* Right ALT */
        {
            if (ev->type == SDL_KEYUP)
                gaModifiersState[keycode] = 0;
            else
                gaModifiersState[keycode] = 1;
            break;
        }

        case 0x45: /* Num Lock */
        case 0x3a: /* Caps Lock */
        {
            /* SDL does not send the key up event, so we generate it.
             * r=frank: This is not true for never SDL versions. */
            if (ev->type == SDL_KEYDOWN)
            {
                gKeyboard->PutScancode(keycode);
                gKeyboard->PutScancode(keycode | 0x80);
            }
            return;
        }
    }

    if (ev->type != SDL_KEYDOWN)
    {
        /*
         * Some keyboards (e.g. the one of mine T60) don't send a NumLock scan code on every
         * press of the key. Both the guest and the host should agree on the NumLock state.
         * If they differ, we try to alter the guest NumLock state by sending the NumLock key
         * scancode. We will get a feedback through the KBD_CMD_SET_LEDS command if the guest
         * tries to set/clear the NumLock LED. If a (silly) guest doesn't change the LED, don't
         * bother him with NumLock scancodes. At least our BIOS, Linux and Windows handle the
         * NumLock LED well.
         */
        if (   gcGuestNumLockAdaptions
            && (gfGuestNumLockPressed ^ !!(SDL_GetModState() & KMOD_NUM)))
        {
            gcGuestNumLockAdaptions--;
            gKeyboard->PutScancode(0x45);
            gKeyboard->PutScancode(0x45 | 0x80);
        }
#if 0  /* For some reason SDL_GetModState() does not return KMOD_CAPS correctly */
        if (   gcGuestCapsLockAdaptions
            && (gfGuestCapsLockPressed ^ !!(SDL_GetModState() & KMOD_CAPS)))
        {
            gcGuestCapsLockAdaptions--;
            gKeyboard->PutScancode(0x3a);
            gKeyboard->PutScancode(0x3a | 0x80);
        }
#endif
    }

    /*
     * Now we send the event. Apply extended and release prefixes.
     */
    if (keycode & 0x80)
        gKeyboard->PutScancode(0xe0);

    gKeyboard->PutScancode(ev->type == SDL_KEYUP ? keycode | 0x80
                                                 : keycode & 0x7f);
}

/**
 * Start grabbing the mouse.
 */
static void InputGrabStart(void)
{
    if (!gfGuestNeedsHostCursor)
        SDL_ShowCursor(SDL_DISABLE);
    SDL_WM_GrabInput(SDL_GRAB_ON);
    // dummy read to avoid moving the mouse
    SDL_GetRelativeMouseState(NULL, NULL);
    gfGrabbed = TRUE;
    UpdateTitlebar(TITLEBAR_NORMAL);
}

/**
 * End mouse grabbing.
 */
static void InputGrabEnd(void)
{
    SDL_WM_GrabInput(SDL_GRAB_OFF);
    if (!gfGuestNeedsHostCursor)
        SDL_ShowCursor(SDL_ENABLE);
    gfGrabbed = FALSE;
    UpdateTitlebar(TITLEBAR_NORMAL);
}

/**
 * Query mouse position and button state from SDL and send to the VM
 *
 * @param dz  Relative mouse wheel movement
 */
static void SendMouseEvent(int dz, int down, int button)
{
    int  x, y, state, buttons;
    bool abs;

    /*
     * If supported and we're not in grabbed mode, we'll use the absolute mouse.
     * If we are in grabbed mode and the guest is not able to draw the mouse cursor
     * itself, we have to use absolute coordinates, otherwise the host cursor and
     * the coordinates the guest thinks the mouse is at could get out-of-sync. From
     * the SDL mailing list:
     *
     * "The event processing is usually asynchronous and so somewhat delayed, and
     * SDL_GetMouseState is returning the immediate mouse state. So at the time you
     * call SDL_GetMouseState, the "button" is already up."
     */
    abs = (UseAbsoluteMouse() && !gfGrabbed) || gfGuestNeedsHostCursor;

    /* only used if abs == TRUE */
    int  xMin = gpFrameBuffer->getXOffset();
    int  yMin = gpFrameBuffer->getYOffset();
    int  xMax = xMin + (int)gpFrameBuffer->getGuestXRes();
    int  yMax = yMin + (int)gpFrameBuffer->getGuestYRes();

    state = abs ? SDL_GetMouseState(&x, &y) : SDL_GetRelativeMouseState(&x, &y);

    /*
     * process buttons
     */
    buttons = 0;
    if (state & SDL_BUTTON(SDL_BUTTON_LEFT))
        buttons |= MouseButtonState_LeftButton;
    if (state & SDL_BUTTON(SDL_BUTTON_RIGHT))
        buttons |= MouseButtonState_RightButton;
    if (state & SDL_BUTTON(SDL_BUTTON_MIDDLE))
        buttons |= MouseButtonState_MiddleButton;

    if (abs)
    {
        /*
         * Check if the mouse event is inside the guest area. This solves the
         * following problem: Some guests switch off the VBox hardware mouse
         * cursor and draw the mouse cursor itself instead. Moving the mouse
         * outside the guest area then leads to annoying mouse hangs if we
         * don't pass mouse motion events into the guest.
         */
        if (x < xMin || y < yMin || x > xMax || y > yMax)
        {
            /*
             * Cursor outside of valid guest area (outside window or in secure
             * label area. Don't allow any mouse button press.
             */
            button   = 0;

            /*
             * Release any pressed button.
             */
#if 0
            /* disabled on customers request */
            buttons &= ~(MouseButtonState_LeftButton   |
                         MouseButtonState_MiddleButton |
                         MouseButtonState_RightButton);
#endif

            /*
             * Prevent negative coordinates.
             */
            if (x < xMin) x = xMin;
            if (x > xMax) x = xMax;
            if (y < yMin) y = yMin;
            if (y > yMax) y = yMax;

            if (!gpOffCursor)
            {
                gpOffCursor       = SDL_GetCursor();    /* Cursor image */
                gfOffCursorActive = SDL_ShowCursor(-1); /* enabled / disabled */
                SDL_SetCursor(gpDefaultCursor);
                SDL_ShowCursor (SDL_ENABLE);
            }
        }
        else
        {
            if (gpOffCursor)
            {
                /*
                 * We just entered the valid guest area. Restore the guest mouse
                 * cursor.
                 */
                SDL_SetCursor(gpOffCursor);
                SDL_ShowCursor(gfOffCursorActive ? SDL_ENABLE : SDL_DISABLE);
                gpOffCursor = NULL;
            }
        }
    }

    /*
     * Button was pressed but that press is not reflected in the button state?
     */
    if (down && !(state & SDL_BUTTON(button)))
    {
        /*
         * It can happen that a mouse up event follows a mouse down event immediately
         * and we see the events when the bit in the button state is already cleared
         * again. In that case we simulate the mouse down event.
         */
        int tmp_button = 0;
        switch (button)
        {
            case SDL_BUTTON_LEFT:   tmp_button = MouseButtonState_LeftButton;   break;
            case SDL_BUTTON_MIDDLE: tmp_button = MouseButtonState_MiddleButton; break;
            case SDL_BUTTON_RIGHT:  tmp_button = MouseButtonState_RightButton;  break;
        }

        if (abs)
        {
            /**
             * @todo
             * PutMouseEventAbsolute() expects x and y starting from 1,1.
             * should we do the increment internally in PutMouseEventAbsolute()
             * or state it in PutMouseEventAbsolute() docs?
             */
            gMouse->PutMouseEventAbsolute(x + 1 - xMin,
                                          y + 1 - yMin,
                                          dz, buttons | tmp_button);
        }
        else
        {
            gMouse->PutMouseEvent(0, 0, dz, buttons | tmp_button);
        }
    }

    // now send the mouse event
    if (abs)
    {
        /**
         * @todo
         * PutMouseEventAbsolute() expects x and y starting from 1,1.
         * should we do the increment internally in PutMouseEventAbsolute()
         * or state it in PutMouseEventAbsolute() docs?
         */
        gMouse->PutMouseEventAbsolute(x + 1 - xMin,
                                      y + 1 - yMin,
                                      dz, buttons);
    }
    else
    {
        gMouse->PutMouseEvent(x, y, dz, buttons);
    }
}

/**
 * Resets the VM
 */
void ResetVM(void)
{
    if (gConsole)
        gConsole->Reset();
}

/**
 * Initiates a saved state and updates the titlebar with progress information
 */
void SaveState(void)
{
    ResetKeys();
    RTThreadYield();
    if (gfGrabbed)
        InputGrabEnd();
    RTThreadYield();
    UpdateTitlebar(TITLEBAR_SAVE);
    gProgress = NULL;
    HRESULT rc = gConsole->SaveState(gProgress.asOutParam());
    if (FAILED(S_OK))
    {
        RTPrintf("Error saving state! rc = 0x%x\n", rc);
        return;
    }
    Assert(gProgress);

    /*
     * Wait for the operation to be completed and work
     * the title bar in the mean while.
     */
    LONG    cPercent = 0;
    for (;;)
    {
        BOOL fCompleted = false;
        rc = gProgress->COMGETTER(Completed)(&fCompleted);
        if (FAILED(rc) || fCompleted)
            break;
        LONG cPercentNow;
        rc = gProgress->COMGETTER(Percent)(&cPercentNow);
        if (FAILED(rc))
            break;
        if (cPercentNow != cPercent)
        {
            UpdateTitlebar(TITLEBAR_SAVE, cPercent);
            cPercent = cPercentNow;
        }

        /* wait */
        rc = gProgress->WaitForCompletion(100);
        if (FAILED(rc))
            break;
        /// @todo process gui events.
    }

    /*
     * What's the result of the operation?
     */
    HRESULT lrc;
    rc = gProgress->COMGETTER(ResultCode)(&lrc);
    if (FAILED(rc))
        lrc = ~0;
    if (!lrc)
    {
        UpdateTitlebar(TITLEBAR_SAVE, 100);
        RTThreadYield();
        RTPrintf("Saved the state successfully.\n");
    }
    else
        RTPrintf("Error saving state, lrc=%d (%#x)\n", lrc, lrc);
}

/**
 * Build the titlebar string
 */
static void UpdateTitlebar(TitlebarMode mode, uint32_t u32User)
{
    static char pszTitle[1024] = {0};

    /* back up current title */
    char pszPrevTitle[1024];
    strcpy(pszPrevTitle, pszTitle);


    strcpy(pszTitle, "InnoTek VirtualBox - ");

    Bstr name;
    gMachine->COMGETTER(Name)(name.asOutParam());
    if (name)
        strcat(pszTitle, Utf8Str(name).raw());
    else
        strcat(pszTitle, "<noname>");


    /* which mode are we in? */
    switch (mode)
    {
        case TITLEBAR_NORMAL:
        {
            MachineState_T machineState;
            gMachine->COMGETTER(State)(&machineState);
            if (machineState == MachineState_Paused)
                strcat(pszTitle, " - [Paused]");

            if (gfGrabbed)
                strcat(pszTitle, " - [Input captured]");

            // do we have a debugger interface
            if (gMachineDebugger)
            {
#if defined(DEBUG) || defined(VBOX_WITH_STATISTICS)
                // query the machine state
                BOOL recompileSupervisor = FALSE;
                BOOL recompileUser = FALSE;
                BOOL patmEnabled = FALSE;
                BOOL csamEnabled = FALSE;
                BOOL singlestepEnabled = FALSE;
                BOOL logEnabled = FALSE;
                BOOL hwVirtEnabled = FALSE;
                gMachineDebugger->COMGETTER(RecompileSupervisor)(&recompileSupervisor);
                gMachineDebugger->COMGETTER(RecompileUser)(&recompileUser);
                gMachineDebugger->COMGETTER(PATMEnabled)(&patmEnabled);
                gMachineDebugger->COMGETTER(CSAMEnabled)(&csamEnabled);
                gMachineDebugger->COMGETTER(LogEnabled)(&logEnabled);
                gMachineDebugger->COMGETTER(Singlestep)(&singlestepEnabled);
                gMachineDebugger->COMGETTER(HWVirtExEnabled)(&hwVirtEnabled);
                RTStrPrintf(pszTitle + strlen(pszTitle), sizeof(pszTitle) - strlen(pszTitle),
                            " [STEP=%d CS=%d PAT=%d RR0=%d RR3=%d LOG=%d HWVirt=%d]",
                            singlestepEnabled == TRUE, csamEnabled == TRUE, patmEnabled == TRUE,
                            recompileSupervisor == FALSE, recompileUser == FALSE,
                            logEnabled == TRUE, hwVirtEnabled == TRUE);
#else
                BOOL hwVirtEnabled = FALSE;
                gMachineDebugger->COMGETTER(HWVirtExEnabled)(&hwVirtEnabled);
                RTStrPrintf(pszTitle + strlen(pszTitle), sizeof(pszTitle) - strlen(pszTitle),
                            "%s", hwVirtEnabled ? " (HWVirtEx)" : "");
#endif /* DEBUG */
            }
            break;
        }

        case TITLEBAR_STARTUP:
        {
            /*
             * Format it.
             */
            MachineState_T machineState;
            gMachine->COMGETTER(State)(&machineState);
            if (machineState == MachineState_Starting)
                strcat(pszTitle, " - Starting...");
            else if (machineState == MachineState_Restoring)
            {
                LONG cPercentNow;
                HRESULT rc = gProgress->COMGETTER(Percent)(&cPercentNow);
                if (SUCCEEDED(rc))
                    RTStrPrintf(pszTitle + strlen(pszTitle), sizeof(pszTitle) - strlen(pszTitle),
                                " - Restoring %d%%...", (int)cPercentNow);
                else
                    RTStrPrintf(pszTitle + strlen(pszTitle), sizeof(pszTitle) - strlen(pszTitle),
                                " - Restoring...");
            }
            /* ignore other states, we could already be in running or aborted state */
            break;
        }

        case TITLEBAR_SAVE:
        {
            AssertMsg(u32User >= 0 && u32User <= 100, ("%d\n", u32User));
            RTStrPrintf(pszTitle + strlen(pszTitle), sizeof(pszTitle) - strlen(pszTitle),
                        " - Saving %d%%...", u32User);
            break;
        }

        case TITLEBAR_SNAPSHOT:
        {
            AssertMsg(u32User >= 0 && u32User <= 100, ("%d\n", u32User));
            RTStrPrintf(pszTitle + strlen(pszTitle), sizeof(pszTitle) - strlen(pszTitle),
                        " - Taking snapshot %d%%...", u32User);
            break;
        }

        default:
            RTPrintf("Error: Invalid title bar mode %d!\n", mode);
            return;
    }

    /*
     * Don't update if it didn't change.
     */
    if (strcmp(pszTitle, pszPrevTitle) == 0)
        return;

    /*
     * Set the new title
     */
#ifdef VBOX_WIN32_UI
    setUITitle(pszTitle);
#else
    SDL_WM_SetCaption(pszTitle, "InnoTek VirtualBox");
#endif
}

#if 0
static void vbox_show_shape (unsigned short w, unsigned short h,
                             uint32_t bg, const uint8_t *image)
{
    size_t x, y;
    unsigned short pitch;
    const uint32_t *color;
    const uint8_t *mask;
    size_t size_mask;

    mask = image;
    pitch = (w + 7) / 8;
    size_mask = (pitch * h + 3) & ~3;

    color = (const uint32_t *) (image + size_mask);

    printf ("show_shape %dx%d pitch %d size mask %d\n",
            w, h, pitch, size_mask);
    for (y = 0; y < h; ++y, mask += pitch, color += w)
    {
        for (x = 0; x < w; ++x) {
            if (mask[x / 8] & (1 << (7 - (x % 8))))
                printf (" ");
            else
            {
                uint32_t c = color[x];
                if (c == bg)
                    printf ("Y");
                else
                    printf ("X");
            }
        }
        printf ("\n");
    }
}
#endif

/**
 *  Sets the pointer shape according to parameters.
 *  Must be called only from the main SDL thread.
 */
static void SetPointerShape (const PointerShapeChangeData *data)
{
    /*
     * don't allow to change the pointer shape if we are outside the valid
     * guest area. In that case set standard mouse pointer is set and should
     * not get overridden.
     */
    if (gpOffCursor)
        return;

    if (data->shape)
    {
        bool ok = false;

        uint32_t andMaskSize = (data->width + 7) / 8 * data->height;
        uint32_t srcShapePtrScan = data->width * 4;

        const uint8_t *srcAndMaskPtr = data->shape;
        const uint8_t *srcShapePtr = data->shape + ((andMaskSize + 3) & ~3);

#if 0
        /* pointer debugging code */
        // vbox_show_shape(data->width, data->height, 0, data->shape);
        uint32_t shapeSize = ((((data->width + 7) / 8) * data->height + 3) & ~3) + data->width * 4 * data->height;
        printf("visible: %d\n", data->visible);
        printf("width = %d\n", data->width);
        printf("height = %d\n", data->height);
        printf("alpha = %d\n", data->alpha);
        printf("xhot = %d\n", data->xHot);
        printf("yhot = %d\n", data->yHot);
        printf("uint8_t pointerdata[] = { ");
        for (uint32_t i = 0; i < shapeSize; i++)
        {
            printf("0x%x, ", data->shape[i]);
        }
        printf("};\n");
#endif

#if defined (__WIN__)

        BITMAPV5HEADER bi;
        HBITMAP hBitmap;
        void *lpBits;
        HCURSOR hAlphaCursor = NULL;

        ::ZeroMemory (&bi, sizeof (BITMAPV5HEADER));
        bi.bV5Size = sizeof (BITMAPV5HEADER);
        bi.bV5Width = data->width;
        bi.bV5Height = - (LONG) data->height;
        bi.bV5Planes = 1;
        bi.bV5BitCount = 32;
        bi.bV5Compression = BI_BITFIELDS;
        // specifiy a supported 32 BPP alpha format for Windows XP
        bi.bV5RedMask   = 0x00FF0000;
        bi.bV5GreenMask = 0x0000FF00;
        bi.bV5BlueMask  = 0x000000FF;
        if (data->alpha)
            bi.bV5AlphaMask = 0xFF000000;
        else
            bi.bV5AlphaMask = 0;

        HDC hdc = ::GetDC (NULL);

        // create the DIB section with an alpha channel
        hBitmap = ::CreateDIBSection (hdc, (BITMAPINFO *) &bi, DIB_RGB_COLORS,
                                      (void **) &lpBits, NULL, (DWORD) 0);

        ::ReleaseDC (NULL, hdc);

        HBITMAP hMonoBitmap = NULL;
        if (data->alpha)
        {
            // create an empty mask bitmap
            hMonoBitmap = ::CreateBitmap (data->width, data->height, 1, 1, NULL);
        }
        else
        {
            /* Word aligned AND mask. Will be allocated and created if necessary. */
            uint8_t *pu8AndMaskWordAligned = NULL;

            /* Width in bytes of the original AND mask scan line. */
            uint32_t cbAndMaskScan = (data->width + 7) / 8;

            if (cbAndMaskScan & 1)
            {
                /* Original AND mask is not word aligned. */

                /* Allocate memory for aligned AND mask. */
                pu8AndMaskWordAligned = (uint8_t *)RTMemTmpAllocZ ((cbAndMaskScan + 1) * data->height);

                Assert(pu8AndMaskWordAligned);

                if (pu8AndMaskWordAligned)
                {
                    /* According to MSDN the padding bits must be 0.
                     * Compute the bit mask to set padding bits to 0 in the last byte of original AND mask.
                     */
                    uint32_t u32PaddingBits = cbAndMaskScan * 8  - data->width;
                    Assert(u32PaddingBits < 8);
                    uint8_t u8LastBytesPaddingMask = (uint8_t)(0xFF << u32PaddingBits);

                    Log(("u8LastBytesPaddingMask = %02X, aligned w = %d, width = %d, cbAndMaskScan = %d\n",
                          u8LastBytesPaddingMask, (cbAndMaskScan + 1) * 8, data->width, cbAndMaskScan));

                    uint8_t *src = (uint8_t *)srcAndMaskPtr;
                    uint8_t *dst = pu8AndMaskWordAligned;

                    unsigned i;
                    for (i = 0; i < data->height; i++)
                    {
                        memcpy (dst, src, cbAndMaskScan);

                        dst[cbAndMaskScan - 1] &= u8LastBytesPaddingMask;

                        src += cbAndMaskScan;
                        dst += cbAndMaskScan + 1;
                    }
                }
            }

            // create the AND mask bitmap
            hMonoBitmap = ::CreateBitmap (data->width, data->height, 1, 1,
                                          pu8AndMaskWordAligned? pu8AndMaskWordAligned: srcAndMaskPtr);

            if (pu8AndMaskWordAligned)
            {
                RTMemTmpFree (pu8AndMaskWordAligned);
            }
        }

        Assert (hBitmap);
        Assert (hMonoBitmap);
        if (hBitmap && hMonoBitmap)
        {
            DWORD *dstShapePtr = (DWORD *) lpBits;

            for (uint32_t y = 0; y < data->height; y ++)
            {
                memcpy (dstShapePtr, srcShapePtr, srcShapePtrScan);
                srcShapePtr += srcShapePtrScan;
                dstShapePtr += data->width;
            }

            ICONINFO ii;
            ii.fIcon = FALSE;
            ii.xHotspot = data->xHot;
            ii.yHotspot = data->yHot;
            ii.hbmMask = hMonoBitmap;
            ii.hbmColor = hBitmap;

            hAlphaCursor = ::CreateIconIndirect (&ii);
            Assert (hAlphaCursor);
            if (hAlphaCursor)
            {
                // here we do a dirty trick by substituting a Window Manager's
                // cursor handle with the handle we created

                WMcursor *pCustomTempWMCursor = gpCustomCursor->wm_cursor;

                // see SDL12/src/video/wincommon/SDL_sysmouse.c
                void *wm_cursor = malloc (sizeof (HCURSOR) + sizeof (uint8_t *) * 2);
                *(HCURSOR *) wm_cursor = hAlphaCursor;

                gpCustomCursor->wm_cursor = (WMcursor *) wm_cursor;
                SDL_SetCursor (gpCustomCursor);
                SDL_ShowCursor (SDL_ENABLE);

                if (pCustomTempWMCursor)
                {
                    ::DestroyCursor (* (HCURSOR *) pCustomTempWMCursor);
                    free (pCustomTempWMCursor);
                }

                ok = true;
            }
        }

        if (hMonoBitmap)
            ::DeleteObject (hMonoBitmap);
        if (hBitmap)
            ::DeleteObject (hBitmap);

#elif defined (__LINUX__)

        XcursorImage *img = XcursorImageCreate (data->width, data->height);
        Assert (img);
        if (img)
        {
            img->xhot = data->xHot;
            img->yhot = data->yHot;

            XcursorPixel *dstShapePtr = img->pixels;

            for (uint32_t y = 0; y < data->height; y ++)
            {
                memcpy (dstShapePtr, srcShapePtr, srcShapePtrScan);

                if (!data->alpha)
                {
                    // convert AND mask to the alpha channel
                    uint8_t byte = 0;
                    for (uint32_t x = 0; x < data->width; x ++)
                    {
                        if (!(x % 8))
                            byte = *(srcAndMaskPtr ++);
                        else
                            byte <<= 1;

                        if (byte & 0x80)
                        {
                            // Linux doesn't support inverted pixels (XOR ops,
                            // to be exact) in cursor shapes, so we detect such
                            // pixels and always replace them with black ones to
                            // make them visible at least over light colors
                            if (dstShapePtr [x] & 0x00FFFFFF)
                                dstShapePtr [x] = 0xFF000000;
                            else
                                dstShapePtr [x] = 0x00000000;
                        }
                        else
                            dstShapePtr [x] |= 0xFF000000;
                    }
                }

                srcShapePtr += srcShapePtrScan;
                dstShapePtr += data->width;
            }

            Cursor cur = XcursorImageLoadCursor (gSdlInfo.info.x11.display, img);
            Assert (cur);
            if (cur)
            {
                // here we do a dirty trick by substituting a Window Manager's
                // cursor handle with the handle we created

                WMcursor *pCustomTempWMCursor = gpCustomCursor->wm_cursor;

                // see SDL12/src/video/x11/SDL_x11mouse.c
                void *wm_cursor = malloc (sizeof (Cursor));
                *(Cursor *) wm_cursor = cur;

                gpCustomCursor->wm_cursor = (WMcursor *) wm_cursor;
                SDL_SetCursor (gpCustomCursor);
                SDL_ShowCursor (SDL_ENABLE);

                if (pCustomTempWMCursor)
                {
                    XFreeCursor (gSdlInfo.info.x11.display, *(Cursor *) pCustomTempWMCursor);
                    free (pCustomTempWMCursor);
                }

                ok = true;
            }

            XcursorImageDestroy (img);
        }

#endif

        if (!ok)
        {
            SDL_SetCursor (gpDefaultCursor);
            SDL_ShowCursor (SDL_ENABLE);
        }
    }
    else
    {
        if (data->visible)
            SDL_ShowCursor (SDL_ENABLE);
        else if (gfAbsoluteMouseGuest)
            /* Don't disable the cursor if the guest additions are not active (anymore) */
            SDL_ShowCursor (SDL_DISABLE);
    }
}

/**
 * Handle changed mouse capabilities
 */
static void HandleGuestCapsChanged(void)
{
    if (!gfAbsoluteMouseGuest)
    {
        // Cursor could be overwritten by the guest tools
        SDL_SetCursor(gpDefaultCursor);
        SDL_ShowCursor (SDL_ENABLE);
        gpOffCursor = NULL;
    }
    if (gMouse && UseAbsoluteMouse())
    {
        // Actually switch to absolute coordinates
        if (gfGrabbed)
            InputGrabEnd();
        gMouse->PutMouseEventAbsolute(-1, -1, 0, 0);
    }
}

/**
 * Handles a host key down event
 */
static int HandleHostKey(const SDL_KeyboardEvent *pEv)
{
    /*
     * Revalidate the host key modifier
     */
    if ((SDL_GetModState() & ~(KMOD_MODE | KMOD_NUM | KMOD_RESERVED)) != gHostKeyMod)
        return VERR_NOT_SUPPORTED;

    /*
     * What was pressed?
     */
    switch (pEv->keysym.sym)
    {
        /* Control-Alt-Delete */
        case SDLK_DELETE:
        {
            gKeyboard->PutCAD();
            break;
        }

        /*
         * Fullscreen / Windowed toggle.
         */
        case SDLK_f:
        {
            if (gfAllowFullscreenToggle)
            {
                /*
                 * We have to pause/resume the machine during this
                 * process because there might be a short moment
                 * without a valid framebuffer
                 */
                MachineState_T machineState;
                gMachine->COMGETTER(State)(&machineState);
                if (machineState == MachineState_Running)
                    gConsole->Pause();
                gpFrameBuffer->setFullscreen(!gpFrameBuffer->getFullscreen());
                if (machineState == MachineState_Running)
                    gConsole->Resume();

                /*
                 * We have switched from/to fullscreen, so request a full
                 * screen repaint, just to be sure.
                 */
                gDisplay->InvalidateAndUpdate();
            }
            break;
        }

        /*
         * Pause / Resume toggle.
         */
        case SDLK_p:
        {
            MachineState_T machineState;
            gMachine->COMGETTER(State)(&machineState);
            if (machineState == MachineState_Running)
            {
                if (gfGrabbed)
                    InputGrabEnd();
                gConsole->Pause();
            }
            else if (machineState == MachineState_Paused)
            {
                gConsole->Resume();
            }
            UpdateTitlebar(TITLEBAR_NORMAL);
            break;
        }

        /*
         * Reset the VM
         */
        case SDLK_r:
        {
            ResetVM();
            break;
        }

        /*
         * Terminate the VM
         */
        case SDLK_q:
        {
            return VINF_EM_TERMINATE;
            break;
        }

        /*
         * Save the machine's state and exit
         */
        case SDLK_s:
        {
            SaveState();
            return VINF_EM_TERMINATE;
        }

        case SDLK_h:
        {
            if (gConsole)
                gConsole->PowerButton();
            break;
        }

        /*
         * Perform an online snapshot. Continue operation.
         */
        case SDLK_n:
        {
            RTThreadYield();
            ULONG cSnapshots = 0;
            gMachine->COMGETTER(SnapshotCount)(&cSnapshots);
            char pszSnapshotName[20];
            RTStrPrintf(pszSnapshotName, sizeof(pszSnapshotName), "Snapshot %d", cSnapshots + 1);
            gProgress = NULL;
            HRESULT rc;
            CHECK_ERROR(gConsole, TakeSnapshot(Bstr(pszSnapshotName), Bstr("Taken by VBoxSDL"),
                                               gProgress.asOutParam()));
            if (FAILED(rc))
            {
                RTPrintf("Error taking snapshot! rc = 0x%x\n", rc);
                /* continue operation */
                return VINF_SUCCESS;
            }
            /*
             * Wait for the operation to be completed and work
             * the title bar in the mean while.
             */
            LONG    cPercent = 0;
            for (;;)
            {
                BOOL fCompleted = false;
                rc = gProgress->COMGETTER(Completed)(&fCompleted);
                if (FAILED(rc) || fCompleted)
                    break;
                LONG cPercentNow;
                rc = gProgress->COMGETTER(Percent)(&cPercentNow);
                if (FAILED(rc))
                    break;
                if (cPercentNow != cPercent)
                {
                    UpdateTitlebar(TITLEBAR_SNAPSHOT, cPercent);
                    cPercent = cPercentNow;
                }

                /* wait */
                rc = gProgress->WaitForCompletion(100);
                if (FAILED(rc))
                    break;
                /// @todo process gui events.
            }

            /* continue operation */
            return VINF_SUCCESS;
        }

        case SDLK_F1: case SDLK_F2: case SDLK_F3:
        case SDLK_F4: case SDLK_F5: case SDLK_F6:
        case SDLK_F7: case SDLK_F8: case SDLK_F9:
        case SDLK_F10: case SDLK_F11: case SDLK_F12:
        {
            /* send Ctrl-Alt-Fx to guest */
            static LONG keySequence[] = {
                0x1d, // Ctrl down
                0x38, // Alt down
                0x00, // Fx down (placeholder)
                0x00, // Fx up (placeholder)
                0xb8, // Alt up
                0x9d  // Ctrl up
            };

            /* put in the right Fx key */
            keySequence[2] = Keyevent2Keycode(pEv);
            keySequence[3] = keySequence[2] + 0x80;

            gKeyboard->PutScancodes(keySequence, ELEMENTS(keySequence), NULL);
            return VINF_SUCCESS;
        }

        /*
         * Not a host key combination.
         * Indicate this by returning false.
         */
        default:
            return VERR_NOT_SUPPORTED;
    }

    return VINF_SUCCESS;
}

/**
 * Timer callback function for startup processing
 */
static Uint32 StartupTimer(Uint32 interval, void *param)
{
    /* post message so we can do something in the startup loop */
    SDL_Event event = {0};
    event.type      = SDL_USEREVENT;
    event.user.type = SDL_USER_EVENT_TIMER;
    SDL_PushEvent(&event);
    return interval;
}
