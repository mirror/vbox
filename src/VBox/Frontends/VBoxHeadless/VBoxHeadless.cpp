/** @file
 *
 * VBox frontends: VBoxHeadless (headless frontend):
 * Headless server executable
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint2.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>

using namespace com;

#define LOG_GROUP LOG_GROUP_GUI

#include <VBox/log.h>
#include <VBox/version.h>
#ifdef VBOX_WITH_VRDP
# include <VBox/vrdpapi.h>
#endif
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/ldr.h>
#include <iprt/getopt.h>
#include <iprt/env.h>

#ifdef VBOX_FFMPEG
#include <cstdlib>
#include <cerrno>
#include "VBoxHeadless.h"
#include <iprt/env.h>
#include <iprt/param.h>
#include <iprt/process.h>
#include <VBox/sup.h>
#endif

//#define VBOX_WITH_SAVESTATE_ON_SIGNAL
#ifdef VBOX_WITH_SAVESTATE_ON_SIGNAL
#include <signal.h>
#endif

#ifdef VBOX_WITH_VRDP
# include "Framebuffer.h"
#endif

////////////////////////////////////////////////////////////////////////////////

#define LogError(m,rc) \
    do { \
        Log (("VBoxHeadless: ERROR: " m " [rc=0x%08X]\n", rc)); \
        RTPrintf ("%s", m); \
    } while (0)

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

    STDMETHOD(OnStorageControllerChange)()
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

    STDMETHOD(OnRuntimeError)(BOOL fatal, IN_BSTR id, IN_BSTR message)
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

#ifdef VBOX_WITH_SAVESTATE_ON_SIGNAL
static void SaveState(int sig)
{
    ComPtr <IProgress> progress = NULL;

/** @todo Deal with nested signals, multithreaded signal dispatching (esp. on windows),
 * and multiple signals (both SIGINT and SIGTERM in some order).
 * Consider processing the signal request asynchronously since there are lots of things
 * which aren't safe (like RTPrintf and printf IIRC) in a signal context. */

    RTPrintf("Signal received, saving state.\n");

    HRESULT rc = gConsole->SaveState(progress.asOutParam());
    if (FAILED(S_OK))
    {
        RTPrintf("Error saving state! rc = 0x%x\n", rc);
        return;
    }
    Assert(progress);
    LONG cPercent = 0;

    RTPrintf("0%%");
    RTStrmFlush(g_pStdOut);
    for (;;)
    {
        BOOL fCompleted = false;
        rc = progress->COMGETTER(Completed)(&fCompleted);
        if (FAILED(rc) || fCompleted)
            break;
        LONG cPercentNow;
        rc = progress->COMGETTER(Percent)(&cPercentNow);
        if (FAILED(rc))
            break;
        if ((cPercentNow / 10) != (cPercent / 10))
        {
            cPercent = cPercentNow;
            RTPrintf("...%d%%", cPercentNow);
            RTStrmFlush(g_pStdOut);
        }

        /* wait */
        rc = progress->WaitForCompletion(100);
    }

    HRESULT lrc;
    rc = progress->COMGETTER(ResultCode)(&lrc);
    if (FAILED(rc))
        lrc = ~0;
    if (!lrc)
    {
        RTPrintf(" -- Saved the state successfully.\n");
        RTThreadYield();
    }
    else
        RTPrintf("-- Error saving state, lrc=%d (%#x)\n", lrc, lrc);

}
#endif /* VBOX_WITH_SAVESTATE_ON_SIGNAL */

////////////////////////////////////////////////////////////////////////////////

static void show_usage()
{
    RTPrintf("Usage:\n"
             "   -s, -startvm, --startvm <name|uuid>   Start given VM (required argument)\n"
#ifdef VBOX_WITH_VRDP
             "   -v, -vrdp, --vrdp on|off|config       Enable (default) or disable the VRDP\n"
             "                                         server or don't change the setting\n"
             "   -p, -vrdpport, --vrdpport <port>      Port number the VRDP server will bind\n"
             "                                         to\n"
             "   -a, -vrdpaddress, --vrdpaddress <ip>  Interface IP the VRDP will bind to \n"
#endif
#ifdef VBOX_FFMPEG
             "   -c, -capture, --capture               Record the VM screen output to a file\n"
             "   -w, --width                           Frame width when recording\n"
             "   -h, --height                          Frame height when recording\n"
             "   -r, --bitrate                         Recording bit rate when recording\n"
             "   -f, --filename                        File name when recording.  The codec\n"
             "                                         used will be chosen based on the\n"
             "                                         file extension\n"
#endif
             "\n");
}

#ifdef VBOX_FFMPEG
/**
 * Parse the environment for variables which can influence the FFMPEG settings.
 * purely for backwards compatibility.
 * @param pulFrameWidth may be updated with a desired frame width
 * @param pulFrameHeight may be updated with a desired frame height
 * @param pulBitRate may be updated with a desired bit rate
 * @param ppszFileName may be updated with a desired file name
 */
static void parse_environ(unsigned long *pulFrameWidth, unsigned long *pulFrameHeight,
                          unsigned long *pulBitRate, const char **ppszFileName)
{
    const char *pszEnvTemp;

    if ((pszEnvTemp = RTEnvGet("VBOX_CAPTUREWIDTH")) != 0)
    {
        errno = 0;
        unsigned long ulFrameWidth = strtoul(pszEnvTemp, 0, 10);
        if (errno != 0)
            LogError("VBoxHeadless: ERROR: invalid VBOX_CAPTUREWIDTH environment variable", 0);
        else
            *pulFrameWidth = ulFrameWidth;
    }
    if ((pszEnvTemp = RTEnvGet("VBOX_CAPTUREHEIGHT")) != 0)
    {
        errno = 0;
        unsigned long ulFrameHeight = strtoul(pszEnvTemp, 0, 10);
        if (errno != 0)
            LogError("VBoxHeadless: ERROR: invalid VBOX_CAPTUREHEIGHT environment variable", 0);
        else
            *pulFrameHeight = ulFrameHeight;
    }
    if ((pszEnvTemp = RTEnvGet("VBOX_CAPTUREBITRATE")) != 0)
    {
        errno = 0;
        unsigned long ulBitRate = strtoul(pszEnvTemp, 0, 10);
        if (errno != 0)
            LogError("VBoxHeadless: ERROR: invalid VBOX_CAPTUREBITRATE environment variable", 0);
        else
            *pulBitRate = ulBitRate;
    }
    if ((pszEnvTemp = RTEnvGet("VBOX_CAPTUREFILE")) != 0)
        *ppszFileName = pszEnvTemp;
}
#endif /* VBOX_FFMPEG defined */

/**
 *  Entry point.
 */
extern "C" DECLEXPORT (int) TrustedMain (int argc, char **argv, char **envp)
{
#ifdef VBOX_WITH_VRDP
    ULONG vrdpPort = ~0U;
    const char *vrdpAddress = NULL;
    const char *vrdpEnabled = NULL;
#endif
    unsigned fRawR0 = ~0U;
    unsigned fRawR3 = ~0U;
    unsigned fPATM  = ~0U;
    unsigned fCSAM  = ~0U;
#ifdef VBOX_FFMPEG
    unsigned fFFMPEG = 0;
    unsigned long ulFrameWidth = 800;
    unsigned long ulFrameHeight = 600;
    unsigned long ulBitRate = 300000;
    char pszMPEGFile[RTPATH_MAX];
    const char *pszFileNameParam = "VBox-%d.vob";
#endif /* VBOX_FFMPEG */

    /* Make sure that DISPLAY is unset, so that X11 bits do not get initialised
     * on X11-using OSes. */
    /** @todo this should really be taken care of in Main. */
    RTEnvUnset("DISPLAY");

    LogFlow (("VBoxHeadless STARTED.\n"));
    RTPrintf ("VirtualBox Headless Interface %s\n"
              "(C) 2008-2009 Sun Microsystems, Inc.\n"
              "All rights reserved.\n\n",
              VBOX_VERSION_STRING);

    Guid id;
    /* the below cannot be Bstr because on Linux Bstr doesn't work until XPCOM (nsMemory) is initialized */
    const char *name = NULL;

#ifdef VBOX_FFMPEG
    /* Parse the environment */
    parse_environ(&ulFrameWidth, &ulFrameHeight, &ulBitRate, &pszFileNameParam);
#endif

    enum eHeadlessOptions
    {
        OPT_RAW_R0 = 0x100,
        OPT_NO_RAW_R0,
        OPT_RAW_R3,
        OPT_NO_RAW_R3,
        OPT_PATM,
        OPT_NO_PATM,
        OPT_CSAM,
        OPT_NO_CSAM,
        OPT_COMMENT,
    };

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "-startvm", 's', RTGETOPT_REQ_STRING },
        { "--startvm", 's', RTGETOPT_REQ_STRING },
#ifdef VBOX_WITH_VRDP
        { "-vrdpport", 'p', RTGETOPT_REQ_UINT32 },
        { "--vrdpport", 'p', RTGETOPT_REQ_UINT32 },
        { "-vrdpaddress", 'a', RTGETOPT_REQ_STRING },
        { "--vrdpaddress", 'a', RTGETOPT_REQ_STRING },
        { "-vrdp", 'v', RTGETOPT_REQ_STRING },
        { "--vrdp", 'v', RTGETOPT_REQ_STRING },
#endif /* VBOX_WITH_VRDP defined */
        { "-rawr0", OPT_RAW_R0, 0 },
        { "--rawr0", OPT_RAW_R0, 0 },
        { "-norawr0", OPT_NO_RAW_R0, 0 },
        { "--norawr0", OPT_NO_RAW_R0, 0 },
        { "-rawr3", OPT_RAW_R3, 0 },
        { "--rawr3", OPT_RAW_R3, 0 },
        { "-norawr3", OPT_NO_RAW_R3, 0 },
        { "--norawr3", OPT_NO_RAW_R3, 0 },
        { "-patm", OPT_PATM, 0 },
        { "--patm", OPT_PATM, 0 },
        { "-nopatm", OPT_NO_PATM, 0 },
        { "--nopatm", OPT_NO_PATM, 0 },
        { "-csam", OPT_CSAM, 0 },
        { "--csam", OPT_CSAM, 0 },
        { "-nocsam", OPT_NO_CSAM, 0 },
        { "--nocsam", OPT_NO_CSAM, 0 },
#ifdef VBOX_FFMPEG
        { "-capture", 'c', 0 },
        { "--capture", 'c', 0 },
        { "--width", 'w', RTGETOPT_REQ_UINT32 },
        { "--height", 'h', RTGETOPT_REQ_UINT32 },
        { "--bitrate", 'r', RTGETOPT_REQ_UINT32 },
        { "--filename", 'f', RTGETOPT_REQ_STRING },
#endif /* VBOX_FFMPEG defined */
        { "-comment", OPT_COMMENT, RTGETOPT_REQ_STRING },
        { "--comment", OPT_COMMENT, RTGETOPT_REQ_STRING }
    };

    // parse the command line
    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0 /* fFlags */);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        if (ch < 0)
        {
            show_usage();
            exit(-1);
        }
        switch(ch)
        {
            case 's':
                id = ValueUnion.psz;
                /* If the argument was not a UUID, then it must be a name. */
                if (!id)
                    name = ValueUnion.psz;
                break;
#ifdef VBOX_WITH_VRDP
            case 'p':
                vrdpPort = ValueUnion.u32;
                break;
            case 'a':
                vrdpAddress = ValueUnion.psz;
                break;
            case 'v':
                vrdpEnabled = ValueUnion.psz;
                break;
#endif /* VBOX_WITH_VRDP defined */
            case OPT_RAW_R0:
                fRawR0 = true;
                break;
            case OPT_NO_RAW_R0:
                fRawR0 = false;
                break;
            case OPT_RAW_R3:
                fRawR3 = true;
                break;
            case OPT_NO_RAW_R3:
                fRawR3 = false;
                break;
            case OPT_PATM:
                fPATM = true;
                break;
            case OPT_NO_PATM:
                fPATM = false;
                break;
            case OPT_CSAM:
                fCSAM = true;
                break;
            case OPT_NO_CSAM:
                fCSAM = false;
                break;
#ifdef VBOX_FFMPEG
            case 'c':
                fFFMPEG = true;
                break;
            case 'w':
                ulFrameWidth = ValueUnion.u32;
                break;
            case 'h':
                ulFrameHeight = ValueUnion.u32;
                break;
            case 'r':
                ulBitRate = ValueUnion.u32;
                break;
            case 'f':
                pszFileNameParam = ValueUnion.psz;
                break;
#endif /* VBOX_FFMPEG defined */
            case VINF_GETOPT_NOT_OPTION:
                /* ignore */
                break;
            default: /* comment */
                /** @todo If we would not ignore this, that would be really really nice... */
                break;
        }
    }

#ifdef VBOX_FFMPEG
    if (ulFrameWidth < 512 || ulFrameWidth > 2048 || ulFrameWidth % 2)
    {
        LogError("VBoxHeadless: ERROR: please specify an even frame width between 512 and 2048", 0);
        return -1;
    }
    if (ulFrameHeight < 384 || ulFrameHeight > 1536 || ulFrameHeight % 2)
    {
        LogError("VBoxHeadless: ERROR: please specify an even frame height between 384 and 1536", 0);
        return -1;
    }
    if (ulBitRate < 300000 || ulBitRate > 1000000)
    {
        LogError("VBoxHeadless: ERROR: please specify an even bitrate between 300000 and 1000000", 0);
        return -1;
    }
    /* Make sure we only have %d or %u (or none) in the file name specified */
    char *pcPercent = (char*)strchr(pszFileNameParam, '%');
    if (pcPercent != 0 && *(pcPercent + 1) != 'd' && *(pcPercent + 1) != 'u')
    {
        LogError("VBoxHeadless: ERROR: Only %%d and %%u are allowed in the capture file name.", -1);
        return -1;
    }
    /* And no more than one % in the name */
    if (pcPercent != 0 && strchr(pcPercent + 1, '%') != 0)
    {
        LogError("VBoxHeadless: ERROR: Only one format modifier is allowed in the capture file name.", -1);
        return -1;
    }
    RTStrPrintf(&pszMPEGFile[0], RTPATH_MAX, pszFileNameParam, RTProcSelf());
#endif /* defined VBOX_FFMPEG */

    if (!id && !name)
    {
        show_usage();
        return -1;
    }

    HRESULT rc;

    rc = com::Initialize();
    if (FAILED(rc))
    {
        RTPrintf("ERROR: failed to initialize COM!\n");
        return rc;
    }

    do
    {
        ComPtr<IVirtualBox> virtualBox;
        ComPtr<ISession> session;

        rc = virtualBox.createLocalObject(CLSID_VirtualBox);
        if (FAILED(rc))
            RTPrintf("ERROR: failed to create the VirtualBox object!\n");
        else
        {
            rc = session.createInprocObject(CLSID_Session);
            if (FAILED(rc))
                RTPrintf("ERROR: failed to create a session object!\n");
        }

        if (FAILED(rc))
        {
            com::ErrorInfo info;
            if (!info.isFullAvailable() && !info.isBasicAvailable())
            {
                com::GluePrintRCMessage(rc);
                RTPrintf("Most likely, the VirtualBox COM server is not running or failed to start.\n");
            }
            else
                GluePrintErrorInfo(info);
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
            int rrc = VINF_SUCCESS, rcc = S_OK;

            Log2(("VBoxHeadless: loading VBoxFFmpegFB shared library\n"));
            rrc = SUPR3HardenedLdrLoadAppPriv("VBoxFFmpegFB", &hLdrFFmpegFB);

            if (RT_SUCCESS(rrc))
            {
                Log2(("VBoxHeadless: looking up symbol VBoxRegisterFFmpegFB\n"));
                rrc = RTLdrGetSymbol(hLdrFFmpegFB, "VBoxRegisterFFmpegFB",
                                     reinterpret_cast<void **>(&pfnRegisterFFmpegFB));
                if (RT_FAILURE(rrc))
                    LogError("Failed to load the video capture extension, possibly due to a damaged file\n", rrc);
            }
            else
                LogError("Failed to load the video capture extension\n", rrc);
            if (RT_SUCCESS(rrc))
            {
                Log2(("VBoxHeadless: calling pfnRegisterFFmpegFB\n"));
                rcc = pfnRegisterFFmpegFB(ulFrameWidth, ulFrameHeight, ulBitRate,
                                         pszMPEGFile, &pFramebuffer);
                if (rcc != S_OK)
                    LogError("Failed to initialise video capturing - make sure that the file format\n"
                             "you wish to use is supported on your system\n", rcc);
            }
            if (RT_SUCCESS(rrc) && (S_OK == rcc))
            {
                Log2(("VBoxHeadless: Registering framebuffer\n"));
                pFramebuffer->AddRef();
                display->RegisterExternalFramebuffer(pFramebuffer);
            }
            if (!RT_SUCCESS(rrc) || (rcc != S_OK))
                rc = E_FAIL;
        }
        if (rc != S_OK)
        {
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
        /* default is to enable the RDP server (backward compatibility) */
        BOOL fVRDPEnable = true;
        BOOL fVRDPEnabled;
        ComPtr <IVRDPServer> vrdpServer;
        CHECK_ERROR_BREAK(machine, COMGETTER (VRDPServer) (vrdpServer.asOutParam()));
        CHECK_ERROR_BREAK(vrdpServer, COMGETTER(Enabled) (&fVRDPEnabled));

        if (vrdpEnabled != NULL)
        {
            /* -vrdp on|off|config */
            if (!strcmp(vrdpEnabled, "off") || !strcmp(vrdpEnabled, "disable"))
                fVRDPEnable = false;
            else if (!strcmp(vrdpEnabled, "config"))
            {
                if (!fVRDPEnabled)
                    fVRDPEnable = false;
            }
            else if (strcmp(vrdpEnabled, "on") && strcmp(vrdpEnabled, "enable"))
            {
                RTPrintf("-vrdp requires an argument (on|off|config)\n");
                break;
            }
        }

        if (fVRDPEnable)
        {
            Log (("VBoxHeadless: Enabling VRDP server...\n"));

            /* set VRDP port if requested by the user */
            if (vrdpPort != ~0U)
                CHECK_ERROR_BREAK(vrdpServer, COMSETTER(Port)(vrdpPort));
            else
                CHECK_ERROR_BREAK(vrdpServer, COMGETTER(Port)(&vrdpPort));
            /* set VRDP address if requested by the user */
            if (vrdpAddress != NULL)
            {
                CHECK_ERROR_BREAK(vrdpServer, COMSETTER(NetAddress)(Bstr(vrdpAddress)));
            }
            /* enable VRDP server (only if currently disabled) */
            if (!fVRDPEnabled)
            {
                CHECK_ERROR_BREAK(vrdpServer, COMSETTER(Enabled) (TRUE));
            }
        }
        else
        {
            /* disable VRDP server (only if currently enabled */
            if (fVRDPEnabled)
            {
                CHECK_ERROR_BREAK(vrdpServer, COMSETTER(Enabled) (FALSE));
            }
        }
#endif
        Log (("VBoxHeadless: Powering up the machine...\n"));
#ifdef VBOX_WITH_VRDP
        if (fVRDPEnable)
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

#ifdef VBOX_WITH_SAVESTATE_ON_SIGNAL
        signal(SIGINT, SaveState);
        signal(SIGTERM, SaveState);
#endif

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


#ifndef VBOX_WITH_HARDENING
/**
 * Main entry point.
 */
int main (int argc, char **argv, char **envp)
{
    // initialize VBox Runtime
    RTR3InitAndSUPLib();
    return TrustedMain (argc, argv, envp);
}
#endif /* !VBOX_WITH_HARDENING */

