/** @file
 *
 * VBox frontends: VBoxHeadless (headless frontend):
 * Headless server executable
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * innotek GmbH confidential
 * All rights reserved
 */

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>

using namespace com;

#define LOG_GROUP LOG_GROUP_GUI

#include <VBox/log.h>
#include <VBox/version.h>
#ifdef VBOX_WITH_VRDP
# include <VBox/vrdpapi.h>
#endif
#include <iprt/runtime.h>
#include <iprt/stream.h>
#include <iprt/ldr.h>

#ifdef VBOX_FFMPEG
#include <cstdlib>
#include <cerrno>
#include "VBoxHeadless.h"
#include <iprt/env.h>
#include <iprt/param.h>
#include <iprt/process.h>
#endif

#ifdef VBOX_WITH_VRDP
# include "Framebuffer.h"
#endif

////////////////////////////////////////////////////////////////////////////////

#define LogError(m,rc) \
    if (1) { \
        Log (("VBoxHeadless: ERROR: " m " [rc=0x%08X]\n", rc)); \
        RTPrintf (m " (rc = 0x%08X)\n", rc); \
    }

////////////////////////////////////////////////////////////////////////////////

/* global weak references (for event handlers) */
static ComPtr <ISession, ComWeakRef> gSession;
static ComPtr <IConsole, ComWeakRef> gConsole;
static EventQueue *gEventQ = NULL;

////////////////////////////////////////////////////////////////////////////////

/**
 *  State change event.
 */
class StateChangeEvent : public Event
{
public:
    StateChangeEvent (MachineState_T state) : mState (state) {}
protected:
    void *handler()
    {
        LogFlow (("VBoxHeadless: StateChangeEvent: %d\n", mState));
        /* post the termination event if the machine has been PoweredDown/Saved/Aborted */
        if (mState < MachineState_Running)
            gEventQ->postEvent (NULL);
        return 0;
    }
private:
    MachineState_T mState;
};

/**
 *  Callback handler for machine events.
 */
class ConsoleCallback : public IConsoleCallback
{
public:

    ConsoleCallback ()
    {
#ifndef VBOX_WITH_XPCOM
        refcnt = 0;
#endif
    }

    virtual ~ConsoleCallback() {}

    NS_DECL_ISUPPORTS

#ifndef VBOX_WITH_XPCOM
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
    STDMETHOD(QueryInterface) (REFIID riid , void **ppObj)
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

    STDMETHOD(OnMousePointerShapeChange) (BOOL visible, BOOL alpha, ULONG xHot, ULONG yHot,
                                          ULONG width, ULONG height, BYTE *shape)
    {
        return S_OK;
    }

    STDMETHOD(OnMouseCapabilityChange) (BOOL supportsAbsolute, BOOL needsHostCursor)
    {
        /* Emit absolute mouse event to actually enable the host mouse cursor. */
        if (supportsAbsolute && gConsole)
        {
            ComPtr<IMouse> mouse;
            gConsole->COMGETTER(Mouse)(mouse.asOutParam());
            if (mouse)
            {
                mouse->PutMouseEventAbsolute(-1, -1, 0, 0);
            }
        }
        return S_OK;
    }

    STDMETHOD(OnKeyboardLedsChange)(BOOL fNumLock, BOOL fCapsLock, BOOL fScrollLock)
    {
        return S_OK;
    }

    STDMETHOD(OnStateChange) (MachineState_T machineState)
    {
        gEventQ->postEvent (new StateChangeEvent (machineState));
        return S_OK;
    }

    STDMETHOD(OnExtraDataChange) (BSTR key)
    {
        return S_OK;
    }

    STDMETHOD(OnAdditionsStateChange)()
    {
        return S_OK;
    }

    STDMETHOD(OnDVDDriveChange)()
    {
        return S_OK;
    }

    STDMETHOD(OnFloppyDriveChange)()
    {
        return S_OK;
    }

    STDMETHOD(OnNetworkAdapterChange) (INetworkAdapter *aNetworkAdapter)
    {
        return S_OK;
    }

    STDMETHOD(OnSerialPortChange) (ISerialPort *aSerialPort)
    {
        return S_OK;
    }

    STDMETHOD(OnParallelPortChange) (IParallelPort *aParallelPort)
    {
        return S_OK;
    }

    STDMETHOD(OnVRDPServerChange)()
    {
        return S_OK;
    }

    STDMETHOD(OnUSBControllerChange)()
    {
        return S_OK;
    }

    STDMETHOD(OnUSBDeviceStateChange) (IUSBDevice *aDevice, BOOL aAttached,
                                      IVirtualBoxErrorInfo *aError)
    {
        return S_OK;
    }

    STDMETHOD(OnSharedFolderChange) (Scope_T aScope)
    {
        return S_OK;
    }

    STDMETHOD(OnRuntimeError)(BOOL fatal, INPTR BSTR id, INPTR BSTR message)
    {
        return S_OK;
    }

    STDMETHOD(OnCanShowWindow)(BOOL *canShow)
    {
        if (!canShow)
            return E_POINTER;
        /* Headless windows should not be shown */
        *canShow = FALSE;
        return S_OK;
    }

    STDMETHOD(OnShowWindow) (ULONG64 *winId)
    {
        /* OnCanShowWindow() always returns FALSE, so this call should never
         * happen. */
        AssertFailed();
        if (!winId)
            return E_POINTER;
        *winId = 0;
        return E_NOTIMPL;
    }

private:

#ifndef VBOX_WITH_XPCOM
    long refcnt;
#endif
};

#ifdef VBOX_WITH_XPCOM
NS_DECL_CLASSINFO (ConsoleCallback)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI (ConsoleCallback, IConsoleCallback)
#endif

////////////////////////////////////////////////////////////////////////////////

static void show_usage()
{
    RTPrintf("Usage:\n"
             "   -startvm <name|uuid>     Start given VM (required argument)\n"
#ifdef VBOX_WITH_VRDP
             "   -vrdpport <port>         Port number the VRDP server will bind to\n"
             "   -vrdpaddress <ip>        Interface IP the VRDP will bind to \n"
#endif
#ifdef VBOX_FFMPEG
             "   -capture                 Record the VM screen output to a file\n"
             "\n"
             "When recording, the following optional environment variables are also\n"
             "recognized:\n"
             "\n"
             "   VBOX_CAPTUREWIDTH        Frame width\n"
             "   VBOX_CAPTUREHEIGHT       Frame height\n"
             "   VBOX_CAPTUREBITRATE      Recording bit rate\n"
             "   VBOX_CAPTUREBITRATE      Recording bit rate\n"
             "   VBOX_CAPTUREFILE         Specify a file name\n"
#endif
             "\n");
}

/**
 *  Entry point.
 */
int main (int argc, char **argv)
{
#ifdef VBOX_WITH_VRDP
    ULONG vrdpPort = ~0U;
    const char *vrdpAddress = NULL;
#endif
    unsigned fRawR0 = ~0U;
    unsigned fRawR3 = ~0U;
    unsigned fPATM  = ~0U;
    unsigned fCSAM  = ~0U;
#ifdef VBOX_FFMPEG
    unsigned fFFMPEG = 0;
    unsigned ulFrameWidth = 800;
    unsigned ulFrameHeight = 600;
    unsigned ulBitRate = 300000;
    char pszMPEGFile[RTPATH_MAX];
#endif /* VBOX_FFMPEG */

    // initialize VBox Runtime
    RTR3Init(true, ~(size_t)0);

    LogFlow (("VBoxHeadless STARTED.\n"));
    RTPrintf ("VirtualBox Headless Interface %s\n"
              "(C) 2005-2007 innotek GmbH\n"
              "All rights reserved\n\n",
              VBOX_VERSION_STRING);

    Guid id;
    /* the below cannot be Bstr because on Linux Bstr doesn't work until XPCOM (nsMemory) is initialized */
    const char *name = NULL;

    // parse the command line
    for (int curArg = 1; curArg < argc; curArg++)
    {
        if (strcmp(argv[curArg], "-startvm") == 0)
        {
            if (++curArg >= argc)
            {
                LogError("VBoxHeadless: ERROR: missing VM identifier!", 0);
                return -1;
            }
            id = argv[curArg];
            /* If the argument was not a UUID, then it must be a name. */
            if (!id)
            {
                name = argv[curArg];
            }
        }
#ifdef VBOX_WITH_VRDP
        else if (strcmp(argv[curArg], "-vrdpport") == 0)
        {
            if (++curArg >= argc)
            {
                LogError("VBoxHeadless: ERROR: missing VRDP port value!", 0);
                return -1;
            }
            vrdpPort = atoi(argv[curArg]);
        }
        else if (strcmp(argv[curArg], "-vrdpaddress") == 0)
        {
            if (++curArg >= argc)
            {
                LogError("VBoxHeadless: ERROR: missing VRDP address value!", 0);
                return -1;
            }
            vrdpAddress = argv[curArg];
        }
#endif
        else if (strcmp(argv[curArg], "-rawr0") == 0)
        {
            fRawR0 = true;
        }
        else if (strcmp(argv[curArg], "-norawr0") == 0)
        {
            fRawR0 = false;
        }
        else if (strcmp(argv[curArg], "-rawr3") == 0)
        {
            fRawR3 = true;
        }
        else if (strcmp(argv[curArg], "-norawr3") == 0)
        {
            fRawR3 = false;
        }
        else if (strcmp(argv[curArg], "-patm") == 0)
        {
            fPATM = true;
        }
        else if (strcmp(argv[curArg], "-nopatm") == 0)
        {
            fPATM = false;
        }
        else if (strcmp(argv[curArg], "-csam") == 0)
        {
            fCSAM = true;
        }
        else if (strcmp(argv[curArg], "-nocsam") == 0)
        {
            fCSAM = false;
        }
#ifdef VBOX_FFMPEG
        else if (strcmp(argv[curArg], "-capture") == 0)
        {
            fFFMPEG = true;
        }
#endif
        else if (strcmp(argv[curArg], "-comment") == 0)
        {
            /* We could just ignore this, but check the syntax for consistency... */
            if (++curArg >= argc)
            {
                LogError("VBoxHeadless: ERROR: missing comment!", 0);
                return -1;
            }
        }
        else
        {
            LogError("VBoxHeadless: ERROR: unknown option '%s'", argv[curArg]);
            show_usage();
            return -1;
        }
    }

#ifdef VBOX_FFMPEG
    const char *pszEnvTemp;
    if ((pszEnvTemp = RTEnvGet("VBOX_CAPTUREWIDTH")) != 0)
    {
        errno = 0;
        ulFrameWidth = strtoul(pszEnvTemp, 0, 10);
        if (errno != 0)
        {
            LogError("VBoxHeadless: ERROR: invalid VBOX_CAPTUREWIDTH environment variable", 0);
            return -1;
        }
        if (ulFrameWidth < 512 || ulFrameWidth > 2048 || ulFrameWidth % 2)
        {
            LogError("VBoxHeadless: ERROR: please specify an even VBOX_CAPTUREWIDTH variable between 512 and 2048", 0);
            return -1;
        }
    }

    if ((pszEnvTemp = RTEnvGet("VBOX_CAPTUREHEIGHT")) != 0)
    {
        errno = 0;
        ulFrameHeight = strtoul(pszEnvTemp, 0, 10);
        if (errno != 0)
        {
            LogError("VBoxHeadless: ERROR: invalid VBOX_CAPTUREHEIGHT environment variable", 0);
            return -1;
        }
        if (ulFrameHeight < 384 || ulFrameHeight > 1536 || ulFrameHeight % 2)
        {
            LogError("VBoxHeadless: ERROR: please specify an even VBOX_CAPTUREHEIGHT variable between 384 and 1536", 0);
            return -1;
        }
    }

    if ((pszEnvTemp = RTEnvGet("VBOX_CAPTUREBITRATE")) != 0)
    {
        errno = 0;
        ulBitRate = strtoul(pszEnvTemp, 0, 10);
        if (errno != 0)
        {
            LogError("VBoxHeadless: ERROR: invalid VBOX_CAPTUREBITRATE environment variable", 0);
            return -1;
        }
        if (ulBitRate < 300000 || ulBitRate > 1000000)
        {
            LogError("VBoxHeadless: ERROR: please specify an even VBOX_CAPTUREHEIGHT variable between 300000 and 1000000", 0);
            return -1;
        }
    }

    if ((pszEnvTemp = RTEnvGet("VBOX_CAPTUREFILE")) == 0)
        /* Standard base name */
        pszEnvTemp = "VBox-%d.vob";

    /* Make sure we only have %d or %u (or none) */
    char *pcPercent = (char*)strchr(pszEnvTemp, '%');
    if (pcPercent != 0 && *(pcPercent + 1) != 'd' && *(pcPercent + 1) != 'u')
    {
        LogError("VBoxHeadless: ERROR: Only %%d and %%u are allowed in the VBOX_CAPTUREFILE parameter.", -1);
        return -1;
    }

    /* And no more than one % in the name */
    if (pcPercent != 0 && strchr(pcPercent + 1, '%') != 0)
    {
        LogError("VBoxHeadless: ERROR: Only one format modifier is allowed in the VBOX_CAPTUREFILE parameter.", -1);
        return -1;
    }
    RTStrPrintf(&pszMPEGFile[0], RTPATH_MAX, pszEnvTemp, RTProcSelf());
#endif /* defined VBOX_FFMPEG */

    if (!id && !name)
    {
        LogError("VBoxHeadless: ERROR: invalid invocation!", -1);
        show_usage();
        return -1;
    }

    HRESULT rc;

    rc = com::Initialize();
    if (FAILED (rc))
        return rc;

    do
    {
        ComPtr <IVirtualBox> virtualBox;
        ComPtr <ISession> session;

        /* create VirtualBox object */
        rc = virtualBox.createLocalObject (CLSID_VirtualBox);
        if (FAILED (rc))
        {
            com::ErrorInfo info;
            if (info.isFullAvailable())
            {
                RTPrintf("Failed to create VirtualBox object! Error info: '%lS' (component %lS).\n",
                         info.getText().raw(), info.getComponent().raw());
            }
            else
                RTPrintf("Failed to create VirtualBox object! No error information available (rc = 0x%x).\n", rc);
            break;
        }

        /* create Session object */
        rc = session.createInprocObject (CLSID_Session);
        if (FAILED (rc))
        {
            LogError ("Cannot create Session object!", rc);
            break;
        }

        /* find ID by name */
        if (!id)
        {
            ComPtr <IMachine> m;
            rc = virtualBox->FindMachine (Bstr (name), m.asOutParam());
            if (FAILED (rc))
            {
                LogError ("Invalid machine name!\n", rc);
                break;
            }
            m->COMGETTER(Id) (id.asOutParam());
            AssertComRC (rc);
            if (FAILED (rc))
                break;
        }

        Log (("VBoxHeadless: Opening a session with machine (id={%s})...\n",
              id.toString().raw()));

        // open a session
        CHECK_ERROR_BREAK(virtualBox, OpenSession (session, id));

        /* get the console */
        ComPtr <IConsole> console;
        CHECK_ERROR_BREAK(session, COMGETTER (Console) (console.asOutParam()));

        /* get the machine */
        ComPtr <IMachine> machine;
        CHECK_ERROR_BREAK(console, COMGETTER(Machine) (machine.asOutParam()));

        ComPtr <IDisplay> display;
        CHECK_ERROR_BREAK(console, COMGETTER(Display) (display.asOutParam()));

#ifdef VBOX_FFMPEG
        IFramebuffer *pFramebuffer = 0;
        RTLDRMOD hLdrFFmpegFB;
        PFNREGISTERFFMPEGFB pfnRegisterFFmpegFB;

        if (fFFMPEG)
        {
            Log2(("VBoxHeadless: loading VBoxFFmpegFB shared library\n"));
            rc = RTLdrLoad("VBoxFFmpegFB", &hLdrFFmpegFB);

            if (rc == VINF_SUCCESS)
            {
                Log2(("VBoxHeadless: looking up symbol VBoxRegisterFFmpegFB\n"));
                rc = RTLdrGetSymbol(hLdrFFmpegFB, "VBoxRegisterFFmpegFB",
                                    reinterpret_cast<void **>(&pfnRegisterFFmpegFB));

                if (rc == VINF_SUCCESS)
                {
                    Log2(("VBoxHeadless: calling pfnRegisterFFmpegFB\n"));
                    rc = pfnRegisterFFmpegFB(ulFrameWidth, ulFrameHeight, ulBitRate,
                                             pszMPEGFile, &pFramebuffer);

                    if (rc == S_OK)
                    {
                        Log2(("VBoxHeadless: Registering framebuffer\n"));
                        pFramebuffer->AddRef();
                        display->RegisterExternalFramebuffer(pFramebuffer);
                    }
                }
            }
        }
        if (rc != S_OK)
        {
            LogError ("Failed to load VBoxFFmpegFB shared library\n", 0);
            return -1;
        }
#endif /* defined(VBOX_FFMPEG) */

        ULONG cMonitors = 1;
        machine->COMGETTER(MonitorCount)(&cMonitors);

#ifdef VBOX_WITH_VRDP
        unsigned uScreenId;
        for (uScreenId = 0; uScreenId < cMonitors; uScreenId++)
        {
#ifdef VBOX_FFMPEG
            if (fFFMPEG && uScreenId == 0)
            {
                /* Already registered. */
                continue;
            }
#endif /* defined(VBOX_FFMPEG) */
            VRDPFramebuffer *pFramebuffer = new VRDPFramebuffer ();
            if (!pFramebuffer)
            {
                RTPrintf("Error: could not create framebuffer object %d\n", uScreenId);
                return -1;
            }
            pFramebuffer->AddRef();
            display->SetFramebuffer(uScreenId, pFramebuffer);
        }
#endif

        /* get the machine debugger (isn't necessarily available) */
        ComPtr <IMachineDebugger> machineDebugger;
        console->COMGETTER(Debugger)(machineDebugger.asOutParam());
        if (machineDebugger)
        {
            Log(("Machine debugger available!\n"));
        }

        if (fRawR0 != ~0U)
        {
            if (!machineDebugger)
            {
                RTPrintf("Error: No debugger object; -%srawr0 cannot be executed!\n", fRawR0 ? "" : "no");
                break;
            }
            machineDebugger->COMSETTER(RecompileSupervisor)(!fRawR0);
        }
        if (fRawR3 != ~0U)
        {
            if (!machineDebugger)
            {
                RTPrintf("Error: No debugger object; -%srawr3 cannot be executed!\n", fRawR0 ? "" : "no");
                break;
            }
            machineDebugger->COMSETTER(RecompileUser)(!fRawR3);
        }
        if (fPATM != ~0U)
        {
            if (!machineDebugger)
            {
                RTPrintf("Error: No debugger object; -%spatm cannot be executed!\n", fRawR0 ? "" : "no");
                break;
            }
            machineDebugger->COMSETTER(PATMEnabled)(fPATM);
        }
        if (fCSAM != ~0U)
        {
            if (!machineDebugger)
            {
                RTPrintf("Error: No debugger object; -%scsam cannot be executed!\n", fRawR0 ? "" : "no");
                break;
            }
            machineDebugger->COMSETTER(CSAMEnabled)(fCSAM);
        }

        /* create an event queue */
        EventQueue eventQ;

        /* initialize global references */
        gSession = session;
        gConsole = console;
        gEventQ = &eventQ;

        /* register a callback for machine events */
        {
            ConsoleCallback *callback = new ConsoleCallback ();
            callback->AddRef();
            CHECK_ERROR(console, RegisterCallback (callback));
            callback->Release();
            if (FAILED (rc))
                break;
        }

#ifdef VBOX_WITH_VRDP
        Log (("VBoxHeadless: Enabling VRDP server...\n"));

        ComPtr <IVRDPServer> vrdpServer;
        CHECK_ERROR_BREAK(machine, COMGETTER (VRDPServer) (vrdpServer.asOutParam()));

        /* set VRDP port if requested by the user */
        if (vrdpPort != ~0U)
            CHECK_ERROR_BREAK(vrdpServer, COMSETTER(Port)(vrdpPort));
        else
            CHECK_ERROR_BREAK(vrdpServer, COMGETTER(Port)(&vrdpPort));
        /* set VRDP address if requested by the user */
        if (vrdpAddress != NULL)
            CHECK_ERROR_BREAK(vrdpServer, COMSETTER(NetAddress)(Bstr(vrdpAddress)));

        /* enable VRDP server (only if currently disabled) */
        BOOL fVRDPEnabled;
        CHECK_ERROR_BREAK(vrdpServer, COMGETTER(Enabled) (&fVRDPEnabled));
        if (!fVRDPEnabled)
        {
            CHECK_ERROR_BREAK(vrdpServer, COMSETTER(Enabled) (TRUE));
        }
#endif
        Log (("VBoxHeadless: Powering up the machine...\n"));
#ifdef VBOX_WITH_VRDP
        RTPrintf("Listening on port %d\n", !vrdpPort ? VRDP_DEFAULT_PORT : vrdpPort);
#endif

        ComPtr <IProgress> progress;
        CHECK_ERROR_BREAK(console, PowerUp (progress.asOutParam()));

        /* wait for result because there can be errors */
        if (SUCCEEDED(progress->WaitForCompletion (-1)))
        {
            progress->COMGETTER(ResultCode)(&rc);
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                {
                    RTPrintf("Error: failed to start machine. Error message: %lS\n", info.getText().raw());
                }
                else
                {
                    RTPrintf("Error: failed to start machine. No error message available!\n");
                }
            }
        }
        Log (("VBoxHeadless: Waiting for PowerDown...\n"));

        Event *e;

        while (eventQ.waitForEvent (&e) && e)
            eventQ.handleEvent (e);

        Log (("VBoxHeadless: event loop has terminated...\n"));

#ifdef VBOX_FFMPEG
        if (pFramebuffer)
        {
            pFramebuffer->Release ();
            Log(("Released framebuffer\n"));
            pFramebuffer = NULL;
        }
#endif /* defined(VBOX_FFMPEG) */

        /* we don't have to disable VRDP here because we don't save the settings of the VM */

        /*
         * Close the session. This will also uninitialize the console and
         * unregister the callback we've registered before.
         */
        Log (("VBoxHeadless: Closing the session...\n"));
        session->Close();
    }
    while (0);

    com::Shutdown();

    LogFlow (("VBoxHeadless FINISHED.\n"));

    return rc;
}

