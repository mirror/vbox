/* $Id$ */
/** @file
 * VBoxHeadless - The VirtualBox Headless frontend for running VMs on servers.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/array.h>
#include <VBox/com/Guid.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>
#include <VBox/com/listeners.h>

using namespace com;

#define LOG_GROUP LOG_GROUP_GUI

#include <VBox/log.h>
#include <VBox/version.h>
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/ldr.h>
#include <iprt/getopt.h>
#include <iprt/env.h>
#include <VBox/err.h>
#include <VBox/VBoxVideo.h>

#ifdef VBOX_WITH_VIDEO_REC
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

#include "Framebuffer.h"

#include "NullFramebuffer.h"

////////////////////////////////////////////////////////////////////////////////

#define LogError(m,rc) \
    do { \
        Log(("VBoxHeadless: ERROR: " m " [rc=0x%08X]\n", rc)); \
        RTPrintf("%s\n", m); \
    } while (0)

////////////////////////////////////////////////////////////////////////////////

/* global weak references (for event handlers) */
static IConsole *gConsole = NULL;
static EventQueue *gEventQ = NULL;

/* flag whether frontend should terminate */
static volatile bool g_fTerminateFE = false;

////////////////////////////////////////////////////////////////////////////////

/**
 *  Handler for VirtualBoxClient events.
 */
class VirtualBoxClientEventListener
{
public:
    VirtualBoxClientEventListener()
    {
    }

    virtual ~VirtualBoxClientEventListener()
    {
    }

    HRESULT init()
    {
        return S_OK;
    }

    void uninit()
    {
    }

    STDMETHOD(HandleEvent)(VBoxEventType_T aType, IEvent *aEvent)
    {
        switch (aType)
        {
            case VBoxEventType_OnVBoxSVCAvailabilityChanged:
            {
                ComPtr<IVBoxSVCAvailabilityChangedEvent> pVSACEv = aEvent;
                Assert(pVSACEv);
                BOOL fAvailable = FALSE;
                pVSACEv->COMGETTER(Available)(&fAvailable);
                if (!fAvailable)
                {
                    LogRel(("VBoxHeadless: VBoxSVC became unavailable, exiting.\n"));
                    RTPrintf("VBoxSVC became unavailable, exiting.\n");
                    /* Terminate the VM as cleanly as possible given that VBoxSVC
                     * is no longer present. */
                    g_fTerminateFE = true;
                    gEventQ->interruptEventQueueProcessing();
                }
                break;
            }
            default:
                AssertFailed();
        }

        return S_OK;
    }

private:
};

/**
 *  Handler for global events.
 */
class VirtualBoxEventListener
{
public:
    VirtualBoxEventListener()
    {
        mfNoLoggedInUsers = true;
    }

    virtual ~VirtualBoxEventListener()
    {
    }

    HRESULT init()
    {
        return S_OK;
    }

    void uninit()
    {
    }

    STDMETHOD(HandleEvent)(VBoxEventType_T aType, IEvent *aEvent)
    {
        switch (aType)
        {
            case VBoxEventType_OnGuestPropertyChanged:
            {
                ComPtr<IGuestPropertyChangedEvent> gpcev = aEvent;
                Assert(gpcev);

                Bstr aKey;
                gpcev->COMGETTER(Name)(aKey.asOutParam());

                if (aKey == Bstr("/VirtualBox/GuestInfo/OS/NoLoggedInUsers"))
                {
                    /* Check if this is our machine and the "disconnect on logout feature" is enabled. */
                    BOOL fProcessDisconnectOnGuestLogout = FALSE;
                    ComPtr <IMachine> machine;
                    HRESULT hrc = S_OK;

                    if (gConsole)
                    {
                        hrc = gConsole->COMGETTER(Machine)(machine.asOutParam());
                        if (SUCCEEDED(hrc) && machine)
                        {
                            Bstr id, machineId;
                            hrc = machine->COMGETTER(Id)(id.asOutParam());
                            gpcev->COMGETTER(MachineId)(machineId.asOutParam());
                            if (id == machineId)
                            {
                                Bstr value1;
                                hrc = machine->GetExtraData(Bstr("VRDP/DisconnectOnGuestLogout").raw(),
                                                            value1.asOutParam());
                                if (SUCCEEDED(hrc) && value1 == "1")
                                {
                                    fProcessDisconnectOnGuestLogout = TRUE;
                                }
                            }
                        }
                    }

                    if (fProcessDisconnectOnGuestLogout)
                    {
                        bool fDropConnection = false;

                        Bstr value;
                        gpcev->COMGETTER(Value)(value.asOutParam());
                        Utf8Str utf8Value = value;

                        if (!mfNoLoggedInUsers) /* Only if the property really changes. */
                        {
                            if (   utf8Value == "true"
                                /* Guest property got deleted due to reset,
                                 * so it has no value anymore. */
                                || utf8Value.isEmpty())
                            {
                                mfNoLoggedInUsers = true;
                                fDropConnection = true;
                            }
                        }
                        else if (utf8Value == "false")
                            mfNoLoggedInUsers = false;
                        /* Guest property got deleted due to reset,
                         * take the shortcut without touching the mfNoLoggedInUsers
                         * state. */
                        else if (utf8Value.isEmpty())
                            fDropConnection = true;

                        if (fDropConnection)
                        {
                            /* If there is a connection, drop it. */
                            ComPtr<IVRDEServerInfo> info;
                            hrc = gConsole->COMGETTER(VRDEServerInfo)(info.asOutParam());
                            if (SUCCEEDED(hrc) && info)
                            {
                                ULONG cClients = 0;
                                hrc = info->COMGETTER(NumberOfClients)(&cClients);
                                if (SUCCEEDED(hrc) && cClients > 0)
                                {
                                    ComPtr <IVRDEServer> vrdeServer;
                                    hrc = machine->COMGETTER(VRDEServer)(vrdeServer.asOutParam());
                                    if (SUCCEEDED(hrc) && vrdeServer)
                                    {
                                        LogRel(("VRDE: the guest user has logged out, disconnecting remote clients.\n"));
                                        vrdeServer->COMSETTER(Enabled)(FALSE);
                                        vrdeServer->COMSETTER(Enabled)(TRUE);
                                    }
                                }
                            }
                        }
                    }
                }
                break;
            }
            default:
                AssertFailed();
        }

        return S_OK;
    }

private:
    bool mfNoLoggedInUsers;
};

/**
 *  Handler for machine events.
 */
class ConsoleEventListener
{
public:
    ConsoleEventListener() :
        mLastVRDEPort(-1),
        m_fIgnorePowerOffEvents(false)
    {
    }

    virtual ~ConsoleEventListener()
    {
    }

    HRESULT init()
    {
        return S_OK;
    }

    void uninit()
    {
    }

    STDMETHOD(HandleEvent)(VBoxEventType_T aType, IEvent *aEvent)
    {
        switch (aType)
        {
            case VBoxEventType_OnMouseCapabilityChanged:
            {

                ComPtr<IMouseCapabilityChangedEvent> mccev = aEvent;
                Assert(mccev);

                BOOL fSupportsAbsolute = false;
                mccev->COMGETTER(SupportsAbsolute)(&fSupportsAbsolute);

                /* Emit absolute mouse event to actually enable the host mouse cursor. */
                if (fSupportsAbsolute && gConsole)
                {
                    ComPtr<IMouse> mouse;
                    gConsole->COMGETTER(Mouse)(mouse.asOutParam());
                    if (mouse)
                    {
                        mouse->PutMouseEventAbsolute(-1, -1, 0, 0 /* Horizontal wheel */, 0);
                    }
                }
                break;
            }
            case VBoxEventType_OnStateChanged:
            {
                ComPtr<IStateChangedEvent> scev = aEvent;
                Assert(scev);

                MachineState_T machineState;
                scev->COMGETTER(State)(&machineState);

                /* Terminate any event wait operation if the machine has been
                 * PoweredDown/Saved/Aborted. */
                if (machineState < MachineState_Running && !m_fIgnorePowerOffEvents)
                {
                    g_fTerminateFE = true;
                    gEventQ->interruptEventQueueProcessing();
                }

                break;
            }
            case VBoxEventType_OnVRDEServerInfoChanged:
            {
                ComPtr<IVRDEServerInfoChangedEvent> rdicev = aEvent;
                Assert(rdicev);

                if (gConsole)
                {
                    ComPtr<IVRDEServerInfo> info;
                    gConsole->COMGETTER(VRDEServerInfo)(info.asOutParam());
                    if (info)
                    {
                        LONG port;
                        info->COMGETTER(Port)(&port);
                        if (port != mLastVRDEPort)
                        {
                            if (port == -1)
                                RTPrintf("VRDE server is inactive.\n");
                            else if (port == 0)
                                RTPrintf("VRDE server failed to start.\n");
                            else
                                RTPrintf("VRDE server is listening on port %d.\n", port);

                            mLastVRDEPort = port;
                        }
                    }
                }
                break;
            }
            case VBoxEventType_OnCanShowWindow:
            {
                ComPtr<ICanShowWindowEvent> cswev = aEvent;
                Assert(cswev);
                cswev->AddVeto(NULL);
                break;
            }
            case VBoxEventType_OnShowWindow:
            {
                ComPtr<IShowWindowEvent> swev = aEvent;
                Assert(swev);
                swev->COMSETTER(WinId)(0);
                break;
            }
            default:
                AssertFailed();
        }
        return S_OK;
    }

    void ignorePowerOffEvents(bool fIgnore)
    {
        m_fIgnorePowerOffEvents = fIgnore;
    }

private:

    long mLastVRDEPort;
    bool m_fIgnorePowerOffEvents;
};

typedef ListenerImpl<VirtualBoxClientEventListener> VirtualBoxClientEventListenerImpl;
typedef ListenerImpl<VirtualBoxEventListener> VirtualBoxEventListenerImpl;
typedef ListenerImpl<ConsoleEventListener> ConsoleEventListenerImpl;

VBOX_LISTENER_DECLARE(VirtualBoxClientEventListenerImpl)
VBOX_LISTENER_DECLARE(VirtualBoxEventListenerImpl)
VBOX_LISTENER_DECLARE(ConsoleEventListenerImpl)

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
    if (FAILED(rc))
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
        ULONG cPercentNow;
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
             "   -v, -vrde, --vrde on|off|config       Enable (default) or disable the VRDE\n"
             "                                         server or don't change the setting\n"
             "   -e, -vrdeproperty, --vrdeproperty <name=[value]> Set a VRDE property:\n"
             "                                         \"TCP/Ports\" - comma-separated list of ports\n"
             "                                         the VRDE server can bind to. Use a dash between\n"
             "                                         two port numbers to specify a range\n"
             "                                         \"TCP/Address\" - interface IP the VRDE server\n"
             "                                         will bind to\n"
             "   --settingspw                          Specify the settings password\n"
             "   --settingspwfile                      Specify a file containing the settings password\n"
#ifdef VBOX_WITH_VIDEO_REC
             "   -c, -capture, --capture               Record the VM screen output to a file\n"
             "   -w, --width                           Frame width when recording\n"
             "   -h, --height                          Frame height when recording\n"
             "   -r, --bitrate                         Recording bit rate when recording\n"
             "   -f, --filename                        File name when recording. The codec used\n"
             "                                         will be chosen based on the file extension\n"
#endif
             "\n");
}

#ifdef VBOX_WITH_VIDEO_REC
/**
 * Parse the environment for variables which can influence the VIDEOREC settings.
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
#endif /* VBOX_WITH_VIDEO_REC defined */

static RTEXITCODE readPasswordFile(const char *pszFilename, com::Utf8Str *pPasswd)
{
    size_t cbFile;
    char szPasswd[512];
    int vrc = VINF_SUCCESS;
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    bool fStdIn = !strcmp(pszFilename, "stdin");
    PRTSTREAM pStrm;
    if (!fStdIn)
        vrc = RTStrmOpen(pszFilename, "r", &pStrm);
    else
        pStrm = g_pStdIn;
    if (RT_SUCCESS(vrc))
    {
        vrc = RTStrmReadEx(pStrm, szPasswd, sizeof(szPasswd)-1, &cbFile);
        if (RT_SUCCESS(vrc))
        {
            if (cbFile >= sizeof(szPasswd)-1)
            {
                RTPrintf("Provided password in file '%s' is too long\n", pszFilename);
                rcExit = RTEXITCODE_FAILURE;
            }
            else
            {
                unsigned i;
                for (i = 0; i < cbFile && !RT_C_IS_CNTRL(szPasswd[i]); i++)
                    ;
                szPasswd[i] = '\0';
                *pPasswd = szPasswd;
            }
        }
        else
        {
            RTPrintf("Cannot read password from file '%s': %Rrc\n", pszFilename, vrc);
            rcExit = RTEXITCODE_FAILURE;
        }
        if (!fStdIn)
            RTStrmClose(pStrm);
    }
    else
    {
        RTPrintf("Cannot open password file '%s' (%Rrc)\n", pszFilename, vrc);
        rcExit = RTEXITCODE_FAILURE;
    }

    return rcExit;
}

static RTEXITCODE settingsPasswordFile(ComPtr<IVirtualBox> virtualBox, const char *pszFilename)
{
    com::Utf8Str passwd;
    RTEXITCODE rcExit = readPasswordFile(pszFilename, &passwd);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        int rc;
        CHECK_ERROR(virtualBox, SetSettingsSecret(com::Bstr(passwd).raw()));
        if (FAILED(rc))
            rcExit = RTEXITCODE_FAILURE;
    }

    return rcExit;
}

#ifdef RT_OS_WINDOWS
// Required for ATL
static CComModule _Module;
#endif

/**
 *  Entry point.
 */
extern "C" DECLEXPORT(int) TrustedMain(int argc, char **argv, char **envp)
{
    const char *vrdePort = NULL;
    const char *vrdeAddress = NULL;
    const char *vrdeEnabled = NULL;
    unsigned cVRDEProperties = 0;
    const char *aVRDEProperties[16];
    unsigned fRawR0 = ~0U;
    unsigned fRawR3 = ~0U;
    unsigned fPATM  = ~0U;
    unsigned fCSAM  = ~0U;
#ifdef VBOX_WITH_VIDEO_REC
    unsigned fVIDEOREC = 0;
    unsigned long ulFrameWidth = 800;
    unsigned long ulFrameHeight = 600;
    unsigned long ulBitRate = 300000;
    char pszMPEGFile[RTPATH_MAX];
    const char *pszFileNameParam = "VBox-%d.vob";
#endif /* VBOX_WITH_VIDEO_REC */

    LogFlow (("VBoxHeadless STARTED.\n"));
    RTPrintf (VBOX_PRODUCT " Headless Interface " VBOX_VERSION_STRING "\n"
              "(C) 2008-" VBOX_C_YEAR " " VBOX_VENDOR "\n"
              "All rights reserved.\n\n");

#ifdef VBOX_WITH_VIDEO_REC
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
        OPT_SETTINGSPW,
        OPT_SETTINGSPW_FILE,
        OPT_COMMENT
    };

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "-startvm", 's', RTGETOPT_REQ_STRING },
        { "--startvm", 's', RTGETOPT_REQ_STRING },
        { "-vrdpport", 'p', RTGETOPT_REQ_STRING },     /* VRDE: deprecated. */
        { "--vrdpport", 'p', RTGETOPT_REQ_STRING },    /* VRDE: deprecated. */
        { "-vrdpaddress", 'a', RTGETOPT_REQ_STRING },  /* VRDE: deprecated. */
        { "--vrdpaddress", 'a', RTGETOPT_REQ_STRING }, /* VRDE: deprecated. */
        { "-vrdp", 'v', RTGETOPT_REQ_STRING },         /* VRDE: deprecated. */
        { "--vrdp", 'v', RTGETOPT_REQ_STRING },        /* VRDE: deprecated. */
        { "-vrde", 'v', RTGETOPT_REQ_STRING },
        { "--vrde", 'v', RTGETOPT_REQ_STRING },
        { "-vrdeproperty", 'e', RTGETOPT_REQ_STRING },
        { "--vrdeproperty", 'e', RTGETOPT_REQ_STRING },
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
        { "--settingspw", OPT_SETTINGSPW, RTGETOPT_REQ_STRING },
        { "--settingspwfile", OPT_SETTINGSPW_FILE, RTGETOPT_REQ_STRING },
#ifdef VBOX_WITH_VIDEO_REC
        { "-capture", 'c', 0 },
        { "--capture", 'c', 0 },
        { "--width", 'w', RTGETOPT_REQ_UINT32 },
        { "--height", 'h', RTGETOPT_REQ_UINT32 }, /* great choice of short option! */
        { "--bitrate", 'r', RTGETOPT_REQ_UINT32 },
        { "--filename", 'f', RTGETOPT_REQ_STRING },
#endif /* VBOX_WITH_VIDEO_REC defined */
        { "-comment", OPT_COMMENT, RTGETOPT_REQ_STRING },
        { "--comment", OPT_COMMENT, RTGETOPT_REQ_STRING }
    };

    const char *pcszNameOrUUID = NULL;

    // parse the command line
    int ch;
    const char *pcszSettingsPw = NULL;
    const char *pcszSettingsPwFile = NULL;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0 /* fFlags */);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch(ch)
        {
            case 's':
                pcszNameOrUUID = ValueUnion.psz;
                break;
            case 'p':
                RTPrintf("Warning: '-p' or '-vrdpport' are deprecated. Use '-e \"TCP/Ports=%s\"'\n", ValueUnion.psz);
                vrdePort = ValueUnion.psz;
                break;
            case 'a':
                RTPrintf("Warning: '-a' or '-vrdpaddress' are deprecated. Use '-e \"TCP/Address=%s\"'\n", ValueUnion.psz);
                vrdeAddress = ValueUnion.psz;
                break;
            case 'v':
                vrdeEnabled = ValueUnion.psz;
                break;
            case 'e':
                if (cVRDEProperties < RT_ELEMENTS(aVRDEProperties))
                    aVRDEProperties[cVRDEProperties++] = ValueUnion.psz;
                else
                     RTPrintf("Warning: too many VRDE properties. Ignored: '%s'\n", ValueUnion.psz);
                break;
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
            case OPT_SETTINGSPW:
                pcszSettingsPw = ValueUnion.psz;
                break;
            case OPT_SETTINGSPW_FILE:
                pcszSettingsPwFile = ValueUnion.psz;
                break;
#ifdef VBOX_WITH_VIDEO_REC
            case 'c':
                fVIDEOREC = true;
                break;
            case 'w':
                ulFrameWidth = ValueUnion.u32;
                break;
            case 'r':
                ulBitRate = ValueUnion.u32;
                break;
            case 'f':
                pszFileNameParam = ValueUnion.psz;
                break;
#endif /* VBOX_WITH_VIDEO_REC defined */
            case 'h':
#ifdef VBOX_WITH_VIDEO_REC
                if ((GetState.pDef->fFlags & RTGETOPT_REQ_MASK) != RTGETOPT_REQ_NOTHING)
                {
                    ulFrameHeight = ValueUnion.u32;
                    break;
                }
#endif
                show_usage();
                return 0;
            case OPT_COMMENT:
                /* nothing to do */
                break;
            case 'V':
                RTPrintf("%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());
                return 0;
            default:
                ch = RTGetOptPrintError(ch, &ValueUnion);
                show_usage();
                return ch;
        }
    }

#ifdef VBOX_WITH_VIDEO_REC
    if (ulFrameWidth < 512 || ulFrameWidth > 2048 || ulFrameWidth % 2)
    {
        LogError("VBoxHeadless: ERROR: please specify an even frame width between 512 and 2048", 0);
        return 1;
    }
    if (ulFrameHeight < 384 || ulFrameHeight > 1536 || ulFrameHeight % 2)
    {
        LogError("VBoxHeadless: ERROR: please specify an even frame height between 384 and 1536", 0);
        return 1;
    }
    if (ulBitRate < 300000 || ulBitRate > 1000000)
    {
        LogError("VBoxHeadless: ERROR: please specify an even bitrate between 300000 and 1000000", 0);
        return 1;
    }
    /* Make sure we only have %d or %u (or none) in the file name specified */
    char *pcPercent = (char*)strchr(pszFileNameParam, '%');
    if (pcPercent != 0 && *(pcPercent + 1) != 'd' && *(pcPercent + 1) != 'u')
    {
        LogError("VBoxHeadless: ERROR: Only %%d and %%u are allowed in the capture file name.", -1);
        return 1;
    }
    /* And no more than one % in the name */
    if (pcPercent != 0 && strchr(pcPercent + 1, '%') != 0)
    {
        LogError("VBoxHeadless: ERROR: Only one format modifier is allowed in the capture file name.", -1);
        return 1;
    }
    RTStrPrintf(&pszMPEGFile[0], RTPATH_MAX, pszFileNameParam, RTProcSelf());
#endif /* defined VBOX_WITH_VIDEO_REC */

    if (!pcszNameOrUUID)
    {
        show_usage();
        return 1;
    }

    HRESULT rc;

    rc = com::Initialize();
#ifdef VBOX_WITH_XPCOM
    if (rc == NS_ERROR_FILE_ACCESS_DENIED)
    {
        char szHome[RTPATH_MAX] = "";
        com::GetVBoxUserHomeDirectory(szHome, sizeof(szHome));
        RTPrintf("Failed to initialize COM because the global settings directory '%s' is not accessible!", szHome);
        return 1;
    }
#endif
    if (FAILED(rc))
    {
        RTPrintf("VBoxHeadless: ERROR: failed to initialize COM!\n");
        return 1;
    }

    ComPtr<IVirtualBoxClient> pVirtualBoxClient;
    ComPtr<IVirtualBox> virtualBox;
    ComPtr<ISession> session;
    ComPtr<IMachine> machine;
    bool fSessionOpened = false;
    ComPtr<IEventListener> vboxClientListener;
    ComPtr<IEventListener> vboxListener;
    ComObjPtr<ConsoleEventListenerImpl> consoleListener;

    do
    {
        rc = pVirtualBoxClient.createInprocObject(CLSID_VirtualBoxClient);
        if (FAILED(rc))
        {
            RTPrintf("VBoxHeadless: ERROR: failed to create the VirtualBoxClient object!\n");
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

        rc = pVirtualBoxClient->COMGETTER(VirtualBox)(virtualBox.asOutParam());
        if (FAILED(rc))
        {
            RTPrintf("Failed to get VirtualBox object (rc=%Rhrc)!\n", rc);
            break;
        }
        rc = pVirtualBoxClient->COMGETTER(Session)(session.asOutParam());
        if (FAILED(rc))
        {
            RTPrintf("Failed to get session object (rc=%Rhrc)!\n", rc);
            break;
        }

        if (pcszSettingsPw)
        {
            CHECK_ERROR(virtualBox, SetSettingsSecret(Bstr(pcszSettingsPw).raw()));
            if (FAILED(rc))
                break;
        }
        else if (pcszSettingsPwFile)
        {
            int rcExit = settingsPasswordFile(virtualBox, pcszSettingsPwFile);
            if (rcExit != RTEXITCODE_SUCCESS)
                break;
        }

        ComPtr<IMachine> m;

        rc = virtualBox->FindMachine(Bstr(pcszNameOrUUID).raw(), m.asOutParam());
        if (FAILED(rc))
        {
            LogError("Invalid machine name or UUID!\n", rc);
            break;
        }
        Bstr id;
        m->COMGETTER(Id)(id.asOutParam());
        AssertComRC(rc);
        if (FAILED(rc))
            break;

        Log(("VBoxHeadless: Opening a session with machine (id={%s})...\n",
              Utf8Str(id).c_str()));

        // open a session
        CHECK_ERROR_BREAK(m, LockMachine(session, LockType_VM));
        fSessionOpened = true;

        /* get the console */
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK(session, COMGETTER(Console)(console.asOutParam()));

        /* get the mutable machine */
        CHECK_ERROR_BREAK(console, COMGETTER(Machine)(machine.asOutParam()));

        ComPtr<IDisplay> display;
        CHECK_ERROR_BREAK(console, COMGETTER(Display)(display.asOutParam()));

#ifdef VBOX_WITH_VIDEO_REC
        IFramebuffer *pFramebuffer = 0;
        RTLDRMOD hLdrVideoRecFB;
        PFNREGISTERVIDEORECFB pfnRegisterVideoRecFB;

        if (fVIDEOREC)
        {
            HRESULT         rcc = S_OK;
            int             rrc = VINF_SUCCESS;
            RTERRINFOSTATIC ErrInfo;

            Log2(("VBoxHeadless: loading VBoxVideoRecFB and libvpx shared library\n"));
            RTErrInfoInitStatic(&ErrInfo);
            rrc = SUPR3HardenedLdrLoadAppPriv("VBoxVideoRecFB", &hLdrVideoRecFB, RTLDRLOAD_FLAGS_LOCAL, &ErrInfo.Core);

            if (RT_SUCCESS(rrc))
            {
                Log2(("VBoxHeadless: looking up symbol VBoxRegisterVideoRecFB\n"));
                rrc = RTLdrGetSymbol(hLdrVideoRecFB, "VBoxRegisterVideoRecFB",
                                     reinterpret_cast<void **>(&pfnRegisterVideoRecFB));
                if (RT_FAILURE(rrc))
                    LogError("Failed to load the video capture extension, possibly due to a damaged file\n", rrc);
            }
            else
                LogError("Failed to load the video capture extension\n", rrc); /** @todo stupid function, no formatting options. */
            if (RT_SUCCESS(rrc))
            {
                Log2(("VBoxHeadless: calling pfnRegisterVideoRecFB\n"));
                rcc = pfnRegisterVideoRecFB(ulFrameWidth, ulFrameHeight, ulBitRate,
                                         pszMPEGFile, &pFramebuffer);
                if (rcc != S_OK)
                    LogError("Failed to initialise video capturing - make sure that the file format\n"
                             "you wish to use is supported on your system\n", rcc);
            }
            if (RT_SUCCESS(rrc) && rcc == S_OK)
            {
                Log2(("VBoxHeadless: Registering framebuffer\n"));
                pFramebuffer->AddRef();
                display->SetFramebuffer(VBOX_VIDEO_PRIMARY_SCREEN, pFramebuffer);
            }
            if (!RT_SUCCESS(rrc) || rcc != S_OK)
                rc = E_FAIL;
        }
        if (rc != S_OK)
        {
            break;
        }
#endif /* defined(VBOX_WITH_VIDEO_REC) */
        ULONG cMonitors = 1;
        machine->COMGETTER(MonitorCount)(&cMonitors);

        unsigned uScreenId;
        for (uScreenId = 0; uScreenId < cMonitors; uScreenId++)
        {
# ifdef VBOX_WITH_VIDEO_REC
            if (fVIDEOREC && uScreenId == 0)
            {
                /* Already registered. */
                continue;
            }
# endif
            VRDPFramebuffer *pVRDPFramebuffer = new VRDPFramebuffer();
            if (!pVRDPFramebuffer)
            {
                RTPrintf("Error: could not create framebuffer object %d\n", uScreenId);
                break;
            }
            pVRDPFramebuffer->AddRef();
            display->SetFramebuffer(uScreenId, pVRDPFramebuffer);
        }
        if (uScreenId < cMonitors)
        {
            break;
        }

        // fill in remaining slots with null framebuffers
        for (uScreenId = 0; uScreenId < cMonitors; uScreenId++)
        {
            ComPtr<IFramebuffer> fb;
            LONG xOrigin, yOrigin;
            HRESULT hrc2 = display->GetFramebuffer(uScreenId,
                                                   fb.asOutParam(),
                                                   &xOrigin, &yOrigin);
            if (hrc2 == S_OK && fb.isNull())
            {
                NullFB *pNullFB =  new NullFB();
                pNullFB->AddRef();
                pNullFB->init();
                display->SetFramebuffer(uScreenId, pNullFB);
            }
        }

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
                RTPrintf("Error: No debugger object; -%srawr3 cannot be executed!\n", fRawR3 ? "" : "no");
                break;
            }
            machineDebugger->COMSETTER(RecompileUser)(!fRawR3);
        }
        if (fPATM != ~0U)
        {
            if (!machineDebugger)
            {
                RTPrintf("Error: No debugger object; -%spatm cannot be executed!\n", fPATM ? "" : "no");
                break;
            }
            machineDebugger->COMSETTER(PATMEnabled)(fPATM);
        }
        if (fCSAM != ~0U)
        {
            if (!machineDebugger)
            {
                RTPrintf("Error: No debugger object; -%scsam cannot be executed!\n", fCSAM ? "" : "no");
                break;
            }
            machineDebugger->COMSETTER(CSAMEnabled)(fCSAM);
        }

        /* initialize global references */
        gConsole = console;
        gEventQ = com::EventQueue::getMainEventQueue();

        /* VirtualBoxClient events registration. */
        {
            ComPtr<IEventSource> pES;
            CHECK_ERROR(pVirtualBoxClient, COMGETTER(EventSource)(pES.asOutParam()));
            ComObjPtr<VirtualBoxClientEventListenerImpl> listener;
            listener.createObject();
            listener->init(new VirtualBoxClientEventListener());
            vboxClientListener = listener;
            com::SafeArray<VBoxEventType_T> eventTypes;
            eventTypes.push_back(VBoxEventType_OnVBoxSVCAvailabilityChanged);
            CHECK_ERROR(pES, RegisterListener(vboxClientListener, ComSafeArrayAsInParam(eventTypes), true));
        }

        /* Console events registration. */
        {
            ComPtr<IEventSource> es;
            CHECK_ERROR(console, COMGETTER(EventSource)(es.asOutParam()));
            consoleListener.createObject();
            consoleListener->init(new ConsoleEventListener());
            com::SafeArray<VBoxEventType_T> eventTypes;
            eventTypes.push_back(VBoxEventType_OnMouseCapabilityChanged);
            eventTypes.push_back(VBoxEventType_OnStateChanged);
            eventTypes.push_back(VBoxEventType_OnVRDEServerInfoChanged);
            eventTypes.push_back(VBoxEventType_OnCanShowWindow);
            eventTypes.push_back(VBoxEventType_OnShowWindow);
            CHECK_ERROR(es, RegisterListener(consoleListener, ComSafeArrayAsInParam(eventTypes), true));
        }

        /* default is to enable the remote desktop server (backward compatibility) */
        BOOL fVRDEEnable = true;
        BOOL fVRDEEnabled;
        ComPtr <IVRDEServer> vrdeServer;
        CHECK_ERROR_BREAK(machine, COMGETTER(VRDEServer)(vrdeServer.asOutParam()));
        CHECK_ERROR_BREAK(vrdeServer, COMGETTER(Enabled)(&fVRDEEnabled));

        if (vrdeEnabled != NULL)
        {
            /* -vrdeServer on|off|config */
            if (!strcmp(vrdeEnabled, "off") || !strcmp(vrdeEnabled, "disable"))
                fVRDEEnable = false;
            else if (!strcmp(vrdeEnabled, "config"))
            {
                if (!fVRDEEnabled)
                    fVRDEEnable = false;
            }
            else if (strcmp(vrdeEnabled, "on") && strcmp(vrdeEnabled, "enable"))
            {
                RTPrintf("-vrdeServer requires an argument (on|off|config)\n");
                break;
            }
        }

        if (fVRDEEnable)
        {
            Log(("VBoxHeadless: Enabling VRDE server...\n"));

            /* set VRDE port if requested by the user */
            if (vrdePort != NULL)
            {
                Bstr bstr = vrdePort;
                CHECK_ERROR_BREAK(vrdeServer, SetVRDEProperty(Bstr("TCP/Ports").raw(), bstr.raw()));
            }
            /* set VRDE address if requested by the user */
            if (vrdeAddress != NULL)
            {
                CHECK_ERROR_BREAK(vrdeServer, SetVRDEProperty(Bstr("TCP/Address").raw(), Bstr(vrdeAddress).raw()));
            }

            /* Set VRDE properties. */
            if (cVRDEProperties > 0)
            {
                for (unsigned i = 0; i < cVRDEProperties; i++)
                {
                    /* Parse 'name=value' */
                    char *pszProperty = RTStrDup(aVRDEProperties[i]);
                    if (pszProperty)
                    {
                        char *pDelimiter = strchr(pszProperty, '=');
                        if (pDelimiter)
                        {
                            *pDelimiter = '\0';

                            Bstr bstrName = pszProperty;
                            Bstr bstrValue = &pDelimiter[1];
                            CHECK_ERROR_BREAK(vrdeServer, SetVRDEProperty(bstrName.raw(), bstrValue.raw()));
                        }
                        else
                        {
                            RTPrintf("Error: Invalid VRDE property '%s'\n", aVRDEProperties[i]);
                            RTStrFree(pszProperty);
                            rc = E_INVALIDARG;
                            break;
                        }
                        RTStrFree(pszProperty);
                    }
                    else
                    {
                        RTPrintf("Error: Failed to allocate memory for VRDE property '%s'\n", aVRDEProperties[i]);
                        rc = E_OUTOFMEMORY;
                        break;
                    }
                }
                if (FAILED(rc))
                    break;
            }

            /* enable VRDE server (only if currently disabled) */
            if (!fVRDEEnabled)
            {
                CHECK_ERROR_BREAK(vrdeServer, COMSETTER(Enabled)(TRUE));
            }
        }
        else
        {
            /* disable VRDE server (only if currently enabled */
            if (fVRDEEnabled)
            {
                CHECK_ERROR_BREAK(vrdeServer, COMSETTER(Enabled)(FALSE));
            }
        }

        /* Disable the host clipboard before powering up */
        console->COMSETTER(UseHostClipboard)(false);

        Log(("VBoxHeadless: Powering up the machine...\n"));

        ComPtr <IProgress> progress;
        CHECK_ERROR_BREAK(console, PowerUp(progress.asOutParam()));

        /*
         * Wait for the result because there can be errors.
         *
         * It's vital to process events while waiting (teleportation deadlocks),
         * so we'll poll for the completion instead of waiting on it.
         */
        for (;;)
        {
            BOOL fCompleted;
            rc = progress->COMGETTER(Completed)(&fCompleted);
            if (FAILED(rc) || fCompleted)
                break;

            /* Process pending events, then wait for new ones. Note, this
             * processes NULL events signalling event loop termination. */
            gEventQ->processEventQueue(0);
            if (!g_fTerminateFE)
                gEventQ->processEventQueue(500);
        }

        if (SUCCEEDED(progress->WaitForCompletion(-1)))
        {
            /* Figure out if the operation completed with a failed status
             * and print the error message. Terminate immediately, and let
             * the cleanup code take care of potentially pending events. */
            LONG progressRc;
            progress->COMGETTER(ResultCode)(&progressRc);
            rc = progressRc;
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                {
                    RTPrintf("Error: failed to start machine. Error message: %ls\n", info.getText().raw());
                }
                else
                {
                    RTPrintf("Error: failed to start machine. No error message available!\n");
                }
                break;
            }
        }

        /* VirtualBox events registration. */
        {
            ComPtr<IEventSource> es;
            CHECK_ERROR(virtualBox, COMGETTER(EventSource)(es.asOutParam()));
            ComObjPtr<VirtualBoxEventListenerImpl> listener;
            listener.createObject();
            listener->init(new VirtualBoxEventListener());
            vboxListener = listener;
            com::SafeArray<VBoxEventType_T> eventTypes;
            eventTypes.push_back(VBoxEventType_OnGuestPropertyChanged);
            CHECK_ERROR(es, RegisterListener(vboxListener, ComSafeArrayAsInParam(eventTypes), true));
        }

#ifdef VBOX_WITH_SAVESTATE_ON_SIGNAL
        signal(SIGINT, SaveState);
        signal(SIGTERM, SaveState);
#endif

        Log(("VBoxHeadless: Waiting for PowerDown...\n"));

        while (   !g_fTerminateFE
               && RT_SUCCESS(gEventQ->processEventQueue(RT_INDEFINITE_WAIT)))
            /* nothing */ ;

        Log(("VBoxHeadless: event loop has terminated...\n"));

#ifdef VBOX_WITH_VIDEO_REC
        if (pFramebuffer)
        {
            pFramebuffer->Release();
            Log(("Released framebuffer\n"));
            pFramebuffer = NULL;
        }
#endif /* defined(VBOX_WITH_VIDEO_REC) */

        /* we don't have to disable VRDE here because we don't save the settings of the VM */
    }
    while (0);

    /*
     * Get the machine state.
     */
    MachineState_T machineState = MachineState_Aborted;
    if (!machine.isNull())
        machine->COMGETTER(State)(&machineState);

    /*
     * Turn off the VM if it's running
     */
    if (   gConsole
        && (   machineState == MachineState_Running
            || machineState == MachineState_Teleporting
            || machineState == MachineState_LiveSnapshotting
            /** @todo power off paused VMs too? */
           )
       )
    do
    {
        consoleListener->getWrapped()->ignorePowerOffEvents(true);
        ComPtr<IProgress> pProgress;
        CHECK_ERROR_BREAK(gConsole, PowerDown(pProgress.asOutParam()));
        CHECK_ERROR_BREAK(pProgress, WaitForCompletion(-1));
        BOOL completed;
        CHECK_ERROR_BREAK(pProgress, COMGETTER(Completed)(&completed));
        ASSERT(completed);
        LONG hrc;
        CHECK_ERROR_BREAK(pProgress, COMGETTER(ResultCode)(&hrc));
        if (FAILED(hrc))
        {
            RTPrintf("VBoxHeadless: ERROR: Failed to power down VM!");
            com::ErrorInfo info;
            if (!info.isFullAvailable() && !info.isBasicAvailable())
                com::GluePrintRCMessage(hrc);
            else
                GluePrintErrorInfo(info);
            break;
        }
    } while (0);

    /* VirtualBox callback unregistration. */
    if (vboxListener)
    {
        ComPtr<IEventSource> es;
        CHECK_ERROR(virtualBox, COMGETTER(EventSource)(es.asOutParam()));
        if (!es.isNull())
            CHECK_ERROR(es, UnregisterListener(vboxListener));
        vboxListener.setNull();
    }

    /* Console callback unregistration. */
    if (consoleListener)
    {
        ComPtr<IEventSource> es;
        CHECK_ERROR(gConsole, COMGETTER(EventSource)(es.asOutParam()));
        if (!es.isNull())
            CHECK_ERROR(es, UnregisterListener(consoleListener));
        consoleListener.setNull();
    }

    /* VirtualBoxClient callback unregistration. */
    if (vboxClientListener)
    {
        ComPtr<IEventSource> pES;
        CHECK_ERROR(pVirtualBoxClient, COMGETTER(EventSource)(pES.asOutParam()));
        if (!pES.isNull())
            CHECK_ERROR(pES, UnregisterListener(vboxClientListener));
        vboxClientListener.setNull();
    }

    /* No more access to the 'console' object, which will be uninitialized by the next session->Close call. */
    gConsole = NULL;

    if (fSessionOpened)
    {
        /*
         * Close the session. This will also uninitialize the console and
         * unregister the callback we've registered before.
         */
        Log(("VBoxHeadless: Closing the session...\n"));
        session->UnlockMachine();
    }

    /* Must be before com::Shutdown */
    session.setNull();
    virtualBox.setNull();
    pVirtualBoxClient.setNull();
    machine.setNull();

    com::Shutdown();

    LogFlow(("VBoxHeadless FINISHED.\n"));

    return FAILED(rc) ? 1 : 0;
}


#ifndef VBOX_WITH_HARDENING
/**
 * Main entry point.
 */
int main(int argc, char **argv, char **envp)
{
    // initialize VBox Runtime
    int rc = RTR3InitExe(argc, &argv, RTR3INIT_FLAGS_SUPLIB);
    if (RT_FAILURE(rc))
    {
        RTPrintf("VBoxHeadless: Runtime Error:\n"
                 " %Rrc -- %Rrf\n", rc, rc);
        switch (rc)
        {
            case VERR_VM_DRIVER_NOT_INSTALLED:
                RTPrintf("Cannot access the kernel driver. Make sure the kernel module has been \n"
                        "loaded successfully. Aborting ...\n");
                break;
            default:
                break;
        }
        return 1;
    }

    return TrustedMain(argc, argv, envp);
}
#endif /* !VBOX_WITH_HARDENING */

#ifdef VBOX_WITH_XPCOM
NS_DECL_CLASSINFO(NullFB)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(NullFB, IFramebuffer)
#endif
