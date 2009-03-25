/* $Id$ */
/** @file
 * Basic Frontend (BFE): VBoxBFE main routines.
 *
 * VBoxBFE is a limited frontend that sits directly on the Virtual Machine
 * Manager (VMM) and does _not_ use COM to communicate.
 * On Linux and Windows, VBoxBFE is based on SDL; on L4 it's based on the
 * L4 console. Much of the code has been copied over from the other frontends
 * in VBox/Main/ and src/Frontends/VBoxSDL/.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_GUI

#ifndef VBOXBFE_WITHOUT_COM
# include <VBox/com/Guid.h>
# include <VBox/com/string.h>
using namespace com;
#endif

#include <VBox/types.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <VBox/pdm.h>
#include <VBox/version.h>
#ifdef VBOXBFE_WITH_USB
# include <VBox/vusb.h>
#endif
#ifdef VBOX_WITH_HGCM
# include <VBox/shflsvc.h>
#endif
#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/initterm.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/uuid.h>

#include "VBoxBFE.h"

#include <stdio.h>
#include <stdlib.h> /* putenv */
#include <errno.h>

#if defined(RT_OS_LINUX) || defined(RT_OS_L4)
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>
#endif

#include "ConsoleImpl.h"
#include "DisplayImpl.h"
#include "MouseImpl.h"
#include "KeyboardImpl.h"
#include "VMMDevInterface.h"
#include "StatusImpl.h"
#include "Framebuffer.h"
#include "MachineDebuggerImpl.h"
#ifdef VBOXBFE_WITH_USB
# include "HostUSBImpl.h"
#endif

#if defined(USE_SDL) && ! defined(RT_OS_L4)
#include "SDLConsole.h"
#include "SDLFramebuffer.h"
#endif

#ifdef RT_OS_L4
#include "L4Console.h"
#include "L4Framebuffer.h"
#include "L4IDLInterface.h"
#endif

#ifdef RT_OS_L4
# include <l4/sys/ktrace.h>
# include <l4/vboxserver/file.h>
#endif

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

#define VBOXSDL_ADVANCED_OPTIONS
#define MAC_STRING_LEN 12


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) vboxbfeConfigConstructor(PVM pVM, void *pvUser);
static DECLCALLBACK(void) vmstateChangeCallback(PVM pVM, VMSTATE enmState, VMSTATE enmOldState, void *pvUser);
static DECLCALLBACK(void) setVMErrorCallback(PVM pVM, void *pvUser, int rc, RT_SRC_POS_DECL,
                                             const char *pszFormat, va_list args);
static DECLCALLBACK(int) VMPowerUpThread(RTTHREAD Thread, void *pvUser);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/

PVM                pVM              = NULL;
Mouse             *gMouse           = NULL;
VMDisplay         *gDisplay         = NULL;
Keyboard          *gKeyboard        = NULL;
VMMDev            *gVMMDev          = NULL;
Framebuffer       *gFramebuffer     = NULL;
MachineDebugger   *gMachineDebugger = NULL;
VMStatus          *gStatus          = NULL;
Console           *gConsole         = NULL;
#ifdef VBOXBFE_WITH_USB
HostUSB           *gHostUSB         = NULL;
#endif

VMSTATE machineState = VMSTATE_CREATING;

static PPDMLED     mapFDLeds[2]   = {0};

/** flag whether keyboard/mouse events are grabbed */
#ifdef RT_OS_L4
/** see <l4/input/macros.h> for key definitions */
int                gHostKey; /* not used */
int                gHostKeySym = KEY_RIGHTCTRL;
#elif defined (DEBUG_dmik)
// my mini kbd doesn't have RCTRL...
int                gHostKey    = KMOD_RSHIFT;
int                gHostKeySym = SDLK_RSHIFT;
#else
int                gHostKey    = KMOD_RCTRL;
int                gHostKeySym = SDLK_RCTRL;
#endif
bool gfAllowFullscreenToggle = true;

static bool        g_fIOAPIC = false;
static bool        g_fACPI   = true;
static bool        g_fAudio  = false;
#ifdef VBOXBFE_WITH_USB
static bool        g_fUSB    = false;
#endif
static char       *g_pszHdaFile   = NULL;
static bool        g_fHdaSpf = false;
static char       *g_pszHdbFile   = NULL;
static bool        g_fHdbSpf = false;
static char       *g_pszCdromFile = NULL;
static char       *g_pszFdaFile   = NULL;
       const char *g_pszStateFile = NULL;
static const char *g_pszBootDevice = "IDE";
static uint32_t    g_u32MemorySizeMB = 128;
static uint32_t    g_u32VRamSize = 4 * _1M;
#ifdef VBOXSDL_ADVANCED_OPTIONS
static bool        g_fRawR0 = true;
static bool        g_fRawR3 = true;
static bool        g_fPATM  = true;
static bool        g_fCSAM  = true;
#endif
static bool        g_fRestoreState = false;
static const char *g_pszShareDir[MaxSharedFolders];
static const char *g_pszShareName[MaxSharedFolders];
static bool        g_fShareReadOnly[MaxSharedFolders];
static unsigned    g_uNumShares;
static bool        g_fPreAllocRam = false;
static int         g_iBootMenu = 2;
static bool        g_fReleaseLog = true; /**< Set if we should open the release. */
       const char *g_pszProgressString;
       unsigned    g_uProgressPercent = ~0U;


/**
 * Network device config info.
 */
typedef struct BFENetworkDevice
{
    enum
    {
        NOT_CONFIGURED = 0,
        NONE,
        NAT,
        HIF,
        INTNET
    }           enmType;    /**< The type of network driver. */
    bool        fSniff;     /**< Set if the network sniffer should be installed. */
    const char *pszSniff;   /**< Output file for the network sniffer. */
    RTMAC       Mac;        /**< The mac address for the device. */
    const char *pszName;    /**< The device name of a HIF device. The name of the internal network. */
#ifdef RT_OS_OS2
    bool        fHaveConnectTo; /**< Whether fConnectTo is set. */
    int32_t     iConnectTo; /**< The lanX to connect to (bridge with). */
#elif 1//defined(RT_OS_LINUX)
    bool        fHaveFd;    /**< Set if fd is valid. */
    int32_t     fd;         /**< The file descriptor of a HIF device.*/
#endif
} BFENETDEV, *PBFENETDEV;

/** Array of network device configurations. */
static BFENETDEV g_aNetDevs[NetworkAdapterCount];


/** @todo currently this is only set but never read. */
static char szError[512];


/**
 */
bool fActivateHGCM()
{
    return !!(g_uNumShares > 0);
}

/**
 * Converts the passed in network option
 *
 * @returns Index into g_aNetDevs on success. (positive)
 * @returns VERR_INVALID_PARAMETER on failure. (negative)
 * @param   pszArg          The argument.
 * @param   cchRoot         The length of the argument root.
 */
static int networkArg2Index(const char *pszArg, int cchRoot)
{
    uint32_t n;
    int rc = RTStrToUInt32Ex(&pszArg[cchRoot], NULL, 10, &n);
    if (RT_FAILURE(rc))
    {
        RTPrintf("Error: invalid network device option (rc=%Rrc): %s\n", rc, pszArg);
        return -1;
    }
    if (n < 1 || n > NetworkAdapterCount)
    {
        RTPrintf("Error: The network device number is out of range: %RU32 (1 <= 0 <= %u) (%s)\n",
                 n, NetworkAdapterCount, pszArg);
        return -1;
    }
    return n;
}

/**
 *  Generates a new unique MAC address based on our vendor ID and
 *  parts of a GUID.
 *
 * @returns iprt status code
 * @param pAddress An array into which to store the newly generated address
 */
int GenerateMACAddress(char pszAddress[MAC_STRING_LEN + 1])
{
    /*
     * Our strategy is as follows: the first three bytes are our fixed
     * vendor ID (080027). The remaining 3 bytes will be taken from the
     * start of a GUID. This is a fairly safe algorithm.
     */
    LogFlowFunc(("called\n"));
    RTUUID uuid;
    int rc = RTUuidCreate(&uuid);
    if (RT_FAILURE(rc))
    {
        LogFlowFunc(("RTUuidCreate failed, returning %Rrc\n", rc));
        return rc;
    }
    RTStrPrintf(pszAddress, MAC_STRING_LEN + 1, "080027%02X%02X%02X",
                uuid.au8[0], uuid.au8[1], uuid.au8[2]);
    LogFlowFunc(("generated MAC: '%s'\n", pszAddress));
    return VINF_SUCCESS;
}

/**
 * Print a syntax error.
 *
 * @returns return value for main().
 * @param   pszMsg  The message format string.
 * @param   ...     Format arguments.
 */
static int SyntaxError(const char *pszMsg, ...)
{
    va_list va;
    RTPrintf("error: ");
    va_start(va, pszMsg);
    RTPrintfV(pszMsg, va);
    va_end(va);
    return 1;
}


/**
 * Print a fatal error.
 *
 * @returns return value for main().
 * @param   pszMsg  The message format string.
 * @param   ...     Format arguments.
 */
static int FatalError(const char *pszMsg, ...)
{
    va_list va;
    RTPrintf("fatal error: ");
    va_start(va, pszMsg);
    RTPrintfV(pszMsg, va);
    va_end(va);
    return 1;
}

/**
 * Start progress display.
 */
void startProgressInfo(const char *pszStr)
{
    g_pszProgressString = pszStr;
    g_uProgressPercent = 0;
}

/**
 * Update progress display.
 */
void callProgressInfo(PVM pVM, unsigned uPercent, void *pvUser)
{
    if (gConsole)
        gConsole->progressInfo(pVM, uPercent, pvUser);
}

/**
 * End progress display.
 */
void endProgressInfo(void)
{
    g_uProgressPercent = ~0U;
}


/**
 * Print program usage.
 */
static void show_usage()
{
    RTPrintf("Usage:\n"
             "  -hda <file>        Set first hard disk to file\n"
             "  -hdb <file>        Set second hard disk to file\n"
             "  -fda <file>        Set first floppy disk to file\n"
             "  -cdrom <file>      Set CDROM to file/device ('none' to unmount)\n"
             "  -boot <a|c|d>      Set boot device (a = floppy, c = first hard disk, d = DVD)\n"
             "  -boot menu <0|1|2> Boot menu (0 = disable, 1 = menu only, 2 = message + menu)\n"
             "  -m <size>          Set memory size in megabytes (default 128MB)\n"
             "  -vram <size>       Set size of video memory in megabytes\n"
             "  -prealloc          Force RAM pre-allocation\n"
             "  -fullscreen        Start VM in fullscreen mode\n"
             "  -statefile <file>  Define the file name for VM save/restore\n"
             "  -restore           Restore the VM if the statefile exists, normal start otherwise\n"
             "  -nofstoggle        Forbid switching to/from fullscreen mode\n"
             "  -share <dir> <name> [readonly]\n"
             "                     Share directory <dir> as name <name>. Optionally read-only.\n"
             "  -nohostkey         Disable hostkey\n"
             "  -[no]acpi          Enable or disable ACPI (default: enabled)\n"
             "  -[no]ioapic        Enable or disable the IO-APIC (default: disabled)\n"
             "  -audio             Enable audio\n"
#ifndef RT_OS_L4
             "  -natdev<1-N> [mac] Use NAT networking on network adapter <N>.  Use hardware\n"
             "                     address <mac> if specified.\n"
#endif
             "  -hifdev<1-N>       Use Host Interface Networking with host interface <int>\n"
             "      <int> [mac]    on network adapter <N>.  Use hardware address <mac> if\n"
             "                     specified.\n"
#ifndef RT_OS_L4
             "  -intnet<1-N>       Attach network adapter <N> to internal network <net>.  Use\n"
             "      <net> [mac]    hardware address <mac> if specified.\n"
#endif
#if 0
             "  -netsniff<1-N>     Enable packet sniffer\n"
#endif
#ifdef RT_OS_OS2
             "  -brdev<1-N> lan<X> Bridge network adaptor <N> with the 'lanX' device.\n"
#endif
#ifdef RT_OS_LINUX
             "  -tapfd<1-N> <fd>   Use existing TAP device, don't allocate\n"
#endif
#ifdef VBOX_WITH_VRDP
             "  -vrdp [port]       Listen for VRDP connections on port (default if not specified)\n"
#endif
#ifdef VBOX_SECURELABEL
             "  -securelabel       Display a secure VM label at the top of the screen\n"
             "  -seclabelfnt       TrueType (.ttf) font file for secure session label\n"
             "  -seclabelsiz       Font point size for secure session label (default 12)\n"
#endif
             "  -[no]rellog        Enable or disable the release log './VBoxBFE.log' (default: enabled)\n"
#ifdef VBOXSDL_ADVANCED_OPTIONS
             "  -[no]rawr0         Enable or disable raw ring 3\n"
             "  -[no]rawr3         Enable or disable raw ring 0\n"
             "  -[no]patm          Enable or disable PATM\n"
             "  -[no]csam          Enable or disable CSAM\n"
#endif
#ifdef RT_OS_L4
             "  -env <var=value>   Set the given environment variable to \"value\"\n"
#endif
             "\n");
}


/** entry point */
extern "C" DECLEXPORT(int) TrustedMain (int argc, char **argv, char **envp)
{
    bool fFullscreen = false;
#ifdef VBOX_WITH_VRDP
    int32_t portVRDP = -1;
#endif
#ifdef VBOX_SECURELABEL
    bool fSecureLabel = false;
    uint32_t secureLabelPointSize = 12;
    char *secureLabelFontFile = NULL;
#endif
#ifdef RT_OS_L4
    uint32_t u32MaxVRAM;
#endif
    int rc = VINF_SUCCESS;

    RTPrintf("Sun VirtualBox Simple SDL GUI built %s %s\n", __DATE__, __TIME__);

    // less than one parameter is not possible
    if (argc < 2)
    {
        show_usage();
        return 1;
    }

    /*
     * Parse the command line arguments.
     */
    for (int curArg = 1; curArg < argc; curArg++)
    {
        const char * const pszArg = argv[curArg];
        if (strcmp(pszArg, "-boot") == 0)
        {
            if (++curArg >= argc)
                return SyntaxError("missing argument for boot drive!\n");
            if (strlen(argv[curArg]) != 1)
                return SyntaxError("invalid argument for boot drive! (%s)\n", argv[curArg]);
            rc = VINF_SUCCESS;
            switch (argv[curArg][0])
            {
                case 'a':
                {
                    g_pszBootDevice = "FLOPPY";
                    break;
                }

                case 'c':
                {
                    g_pszBootDevice = "IDE";
                    break;
                }

                case 'd':
                {
                    g_pszBootDevice = "DVD";
                    break;
                }

                default:
                    return SyntaxError("wrong argument for boot drive! (%s)\n", argv[curArg]);
            }
        }
        else if (strcmp(pszArg, "-bootmenu") == 0)
        {
            if (++curArg >= argc)
                return SyntaxError("missing argument for boot menu!\n");
            if (strlen(argv[curArg]) != 1 || *argv[curArg] < '0' || *argv[curArg] > '2')
                return SyntaxError("invalid argument for boot menu! (%s)\n", argv[curArg]);
            rc = VINF_SUCCESS;
            g_iBootMenu = *argv[curArg] - 0;
        }
        else if (strcmp(pszArg, "-m") == 0)
        {
            if (++curArg >= argc)
                return SyntaxError("missing argument for memory size!\n");
            rc = RTStrToUInt32Ex(argv[curArg], NULL, 0, &g_u32MemorySizeMB);
            if (RT_FAILURE(rc))
                return SyntaxError("bad memory size: %s (error %Rrc)\n",
                                   argv[curArg], rc);
        }
        else if (strcmp(pszArg, "-vram") == 0)
        {
            if (++curArg >= argc)
                return SyntaxError("missing argument for vram size!\n");
            uint32_t uVRAMMB;
            rc = RTStrToUInt32Ex(argv[curArg], NULL, 0, &uVRAMMB);
            g_u32VRamSize = uVRAMMB * _1M;
            if (RT_FAILURE(rc))
                return SyntaxError("bad video ram size: %s (error %Rrc)\n",
                                   argv[curArg], rc);
        }
        else if (strcmp(pszArg, "-statefile") == 0)
        {
            if (++curArg >= argc)
                return SyntaxError("missing argument for restore!\n");
            g_pszStateFile = argv[curArg];
        }
        else if (strcmp(pszArg, "-restore") == 0)
            g_fRestoreState = true;
        else if (strcmp(pszArg, "-share") == 0)
        {
            if (g_uNumShares >= MaxSharedFolders)
                return SyntaxError("too many shared folders specified!\n");
            if (++curArg >= argc)
                return SyntaxError("missing 1s argument for share!\n");
            g_pszShareDir[g_uNumShares] = argv[curArg];
            if (++curArg >= argc)
                return SyntaxError("missing 2nd argument for share!\n");
            g_pszShareName[g_uNumShares] = argv[curArg];
            if (curArg < argc-1 && strcmp(argv[curArg+1], "readonly") == 0)
            {
                g_fShareReadOnly[g_uNumShares] = true;
                curArg++;
            }
            g_uNumShares++;
        }
        else if (strcmp(pszArg, "-fullscreen") == 0)
            fFullscreen = true;
        else if (strcmp(pszArg, "-nofstoggle") == 0)
            gfAllowFullscreenToggle = false;
        else if (strcmp(pszArg, "-nohostkey") == 0)
        {
            gHostKey = 0;
            gHostKeySym = 0;
        }
        else if (strcmp(pszArg, "-acpi") == 0)
            g_fACPI = true;
        else if (strcmp(pszArg, "-noacpi") == 0)
            g_fACPI = false;
        else if (strcmp(pszArg, "-ioapic") == 0)
            g_fIOAPIC = true;
        else if (strcmp(pszArg, "-noioapic") == 0)
            g_fIOAPIC = false;
        else if (strcmp(pszArg, "-audio") == 0)
            g_fAudio = true;
#ifdef VBOXBFE_WITH_USB
        else if (strcmp(pszArg, "-usb") == 0)
            g_fUSB = true;
#endif
        else if (strcmp(pszArg, "-hda") == 0)
        {
            if (++curArg >= argc)
                return SyntaxError("missing file name for first hard disk!\n");

            /* resolve it. */
            if (RTPathExists(argv[curArg]))
                g_pszHdaFile = RTPathRealDup(argv[curArg]);
            if (!g_pszHdaFile)
                return SyntaxError("The path to the specified harddisk, '%s', could not be resolved.\n", argv[curArg]);
        }
        else if (strcmp(pszArg, "-hdaspf") == 0)
        {
            g_fHdaSpf = true;
        }
        else if (strcmp(pszArg, "-hdb") == 0)
        {
            if (++curArg >= argc)
                return SyntaxError("missing file name for second hard disk!\n");

            /* resolve it. */
            if (RTPathExists(argv[curArg]))
                g_pszHdbFile = RTPathRealDup(argv[curArg]);
            if (!g_pszHdbFile)
                return SyntaxError("The path to the specified harddisk, '%s', could not be resolved.\n", argv[curArg]);
        }
        else if (strcmp(pszArg, "-hdbspf") == 0)
        {
            g_fHdbSpf = true;
        }
        else if (strcmp(pszArg, "-fda") == 0)
        {
            if (++curArg >= argc)
                return SyntaxError("missing file/device name for first floppy disk!\n");

            /* resolve it. */
            if (RTPathExists(argv[curArg]))
                g_pszFdaFile = RTPathRealDup(argv[curArg]);
            if (!g_pszFdaFile)
                return SyntaxError("The path to the specified floppy disk, '%s', could not be resolved.\n", argv[curArg]);
        }
        else if (strcmp(pszArg, "-cdrom") == 0)
        {
            if (++curArg >= argc)
                return SyntaxError("missing file/device name for first hard disk!\n");

            /* resolve it. */
            if (RTPathExists(argv[curArg]))
                g_pszCdromFile = RTPathRealDup(argv[curArg]);
            if (!g_pszCdromFile)
                return SyntaxError("The path to the specified cdrom, '%s', could not be resolved.\n", argv[curArg]);
        }
#ifdef RT_OS_L4
        /* This is leaving a lot of dead code in the L4 version of course,
           but I don't think that that is a major problem.  We may even
           activate it sometime... */
        else if (   strncmp(pszArg, "-hifdev", 7) == 0
                 || strncmp(pszArg, "-nonetd", 7) == 0)
#else
        else if (   strncmp(pszArg, "-natdev", 7) == 0
                 || strncmp(pszArg, "-hifdev", 7) == 0
                 || strncmp(pszArg, "-nonetd", 7) == 0
                 || strncmp(pszArg, "-intnet", 7) == 0)
#endif
        {
            int i = networkArg2Index(pszArg, 7);
            if (i < 0)
                return 1;
            g_aNetDevs[i].enmType = !strncmp(pszArg, "-natdev", 7)
                                  ? BFENETDEV::NAT
                                  : !strncmp(pszArg, "-hifdev", 7)
                                  ? BFENETDEV::HIF
                                  : !strncmp(pszArg, "-intnet", 7)
                                  ? BFENETDEV::INTNET
                                  : BFENETDEV::NONE;

            /* The HIF device name / The Internal Network name. */
            g_aNetDevs[i].pszName = NULL;
            if (    g_aNetDevs[i].enmType == BFENETDEV::HIF
                ||  g_aNetDevs[i].enmType == BFENETDEV::INTNET)
            {
                if (curArg + 1 >= argc)
                    return SyntaxError(g_aNetDevs[i].enmType == BFENETDEV::HIF
                                       ? "The TAP network device name is missing! (%s)\n"
                                       : "The internal network name is missing! (%s)\n"
                                       , pszArg);
                g_aNetDevs[i].pszName = argv[++curArg];
            }

            /* The MAC address. */
            const char *pszMac;
            char szMacGen[MAC_STRING_LEN + 1];
            if ((curArg + 1 < argc) && (argv[curArg + 1][0] != '-'))
                pszMac = argv[++curArg];
            else
            {
                rc = GenerateMACAddress(szMacGen);
                if (RT_FAILURE(rc))
                    return SyntaxError("failed to generate a hardware address for network device %d (error %Rrc)\n",
                                       i, rc);
                pszMac = szMacGen;
            }
            if (strlen(pszMac) != MAC_STRING_LEN)
                return SyntaxError("The network MAC address has an invalid length: %s (%s)\n", pszMac, pszArg);
            for (unsigned j = 0; j < RT_ELEMENTS(g_aNetDevs[i].Mac.au8); j++)
            {
                char c1 = toupper(*pszMac++) - '0';
                if (c1 > 9)
                    c1 -= 7;
                char c2 = toupper(*pszMac++) - '0';
                if (c2 > 9)
                    c2 -= 7;
                if (c2 > 16 || c1 > 16)
                    return SyntaxError("Invalid MAC address: %s\n", argv[curArg]);
                g_aNetDevs[i].Mac.au8[j] = ((c1 & 0x0f) << 4) | (c2 & 0x0f);
            }
        }
        else if (strncmp(pszArg, "-netsniff", 9) == 0)
        {
            int i = networkArg2Index(pszArg, 9);
            if (i < 0)
                return 1;
            g_aNetDevs[i].fSniff = true;
            /** @todo filename */
        }
#ifdef RT_OS_OS2
        else if (strncmp(pszArg, "-brdev", 6) == 0)
        {
            int i = networkArg2Index(pszArg, 6);
            if (i < 0)
                return 1;
            if (g_aNetDevs[i].enmType != BFENETDEV::HIF)
                return SyntaxError("%d is not a hif device! Make sure you put the -hifdev argument first.\n", i);
            if (++curArg >= argc)
                return SyntaxError("missing argument for %s!\n", pszArg);
            if (    strncmp(argv[curArg], "lan", 3)
                ||  argv[curArg][3] < '0'
                ||  argv[curArg][3] >= '8'
                ||  argv[curArg][4])
                return SyntaxError("bad interface name '%s' specified with '%s'. Expected 'lan0', 'lan1' and similar.\n",
                                   argv[curArg], pszArg);
            g_aNetDevs[i].iConnectTo = argv[curArg][3] - '0';
            g_aNetDevs[i].fHaveConnectTo = true;
        }
#endif
#ifdef RT_OS_LINUX
        else if (strncmp(pszArg, "-tapfd", 6) == 0)
        {
            int i = networkArg2Index(pszArg, 6);
            if (i < 0)
                return 1;
            if (++curArg >= argc)
                return SyntaxError("missing argument for %s!\n", pszArg);
            rc = RTStrToInt32Ex(argv[curArg], NULL, 0, &g_aNetDevs[i].fd);
            if (RT_FAILURE(rc))
                return SyntaxError("bad tap file descriptor: %s (error %Rrc)\n", argv[curArg], rc);
            g_aNetDevs[i].fHaveFd = true;
        }
#endif /* RT_OS_LINUX */
#ifdef VBOX_WITH_VRDP
        else if (strcmp(pszArg, "-vrdp") == 0)
        {
            // -vrdp might take a port number (positive).
            portVRDP = 0;       // indicate that it was encountered.
            if (curArg + 1 < argc && argv[curArg + 1][0] != '-')
            {
                rc = RTStrToInt32Ex(argv[curArg], NULL, 0, &portVRDP);
                if (RT_FAILURE(rc))
                    return SyntaxError("cannot vrpd port: %s (%Rrc)\n", argv[curArg], rc);
                if (portVRDP < 0 || portVRDP >= 0x10000)
                    return SyntaxError("vrdp port number is out of range: %RI32\n", portVRDP);
            }
        }
#endif /* VBOX_WITH_VRDP */
#ifdef VBOX_SECURELABEL
        else if (strcmp(pszArg, "-securelabel") == 0)
        {
            fSecureLabel = true;
            LogFlow(("Secure labelling turned on\n"));
        }
        else if (strcmp(pszArg, "-seclabelfnt") == 0)
        {
            if (++curArg >= argc)
                return SyntaxError("missing font file name for secure label!\n");
            secureLabelFontFile = argv[curArg];
        }
        else if (strcmp(pszArg, "-seclabelsiz") == 0)
        {
            if (++curArg >= argc)
                return SyntaxError("missing font point size for secure label!\n");
            secureLabelPointSize = atoi(argv[curArg]);
        }
#endif
        else if (strcmp(pszArg, "-rellog") == 0)
            g_fReleaseLog = true;
        else if (strcmp(pszArg, "-norellog") == 0)
            g_fReleaseLog = false;
        else if (strcmp(pszArg, "-prealloc") == 0)
            g_fPreAllocRam = true;
#ifdef VBOXSDL_ADVANCED_OPTIONS
        else if (strcmp(pszArg, "-rawr0") == 0)
            g_fRawR0 = true;
        else if (strcmp(pszArg, "-norawr0") == 0)
            g_fRawR0 = false;
        else if (strcmp(pszArg, "-rawr3") == 0)
            g_fRawR3 = true;
        else if (strcmp(pszArg, "-norawr3") == 0)
            g_fRawR3 = false;
        else if (strcmp(pszArg, "-patm") == 0)
            g_fPATM = true;
        else if (strcmp(pszArg, "-nopatm") == 0)
            g_fPATM = false;
        else if (strcmp(pszArg, "-csam") == 0)
            g_fCSAM = true;
        else if (strcmp(pszArg, "-nocsam") == 0)
            g_fCSAM = false;
#endif /* VBOXSDL_ADVANCED_OPTIONS */
#ifdef RT_OS_L4
        else if (strcmp(pszArg, "-env") == 0)
            ++curArg;
#endif /* RT_OS_L4 */
        /* just show the help screen */
        else
        {
            SyntaxError("unrecognized argument '%s'\n", pszArg);
            show_usage();
            return 1;
        }
    }

    gMachineDebugger = new MachineDebugger();
    gStatus = new VMStatus();
    gKeyboard = new Keyboard();
    gMouse = new Mouse();
    gVMMDev = new VMMDev();
    gDisplay = new VMDisplay();
#if defined(USE_SDL)
    /* First console, then framebuffer!! */
    gConsole = new SDLConsole();
    gFramebuffer = new SDLFramebuffer();
#elif defined(RT_OS_L4)
    gConsole = new L4Console();
    gFramebuffer = new L4Framebuffer();
#else
#error "todo"
#endif
    if (!gConsole->initialized())
        goto leave;
    gDisplay->RegisterExternalFramebuffer(gFramebuffer);

    /* start with something in the titlebar */
    gConsole->updateTitlebar();

    /*
     * Start the VM execution thread. This has to be done
     * asynchronously as powering up can take some time
     * (accessing devices such as the host DVD drive). In
     * the meantime, we have to service the SDL event loop.
     */

    RTTHREAD thread;
    rc = RTThreadCreate(&thread, VMPowerUpThread, 0, 0, RTTHREADTYPE_MAIN_WORKER, 0, "PowerUp");
    if (RT_FAILURE(rc))
    {
        RTPrintf("Error: Thread creation failed with %d\n", rc);
        return -1;
    }

#ifdef RT_OS_L4
    /* Start the external IDL interface */
    L4CtrlInit();
#endif

    /* loop until the powerup processing is done */
    do
    {
#if defined(VBOXBFE_WITH_X11) && defined(USE_SDL)
        if (   machineState == VMSTATE_CREATING
            || machineState == VMSTATE_LOADING)
        {
            int event = gConsole->eventWait();

            switch (event)
            {
            case CONEVENT_USR_SCREENRESIZE:
                LogFlow(("CONEVENT_USR_SCREENRESIZE\n"));
                gFramebuffer->resize();
                /* notify the display that the resize has been completed */
                gDisplay->ResizeCompleted();
                break;

            case CONEVENT_USR_TITLEBARUPDATE:
                gConsole->updateTitlebar();
                break;

            case CONEVENT_USR_QUIT:
                RTPrintf("Error: failed to power up VM! No error text available.\n");
                goto leave;
            }
        }
        else
#endif
            RTThreadSleep(1000);
    }
    while (   machineState == VMSTATE_CREATING
           || machineState == VMSTATE_LOADING);

    if (machineState == VMSTATE_TERMINATED)
        goto leave;

    /* did the power up succeed? */
    if (machineState != VMSTATE_RUNNING)
    {
        RTPrintf("Error: failed to power up VM! No error text available (rc = 0x%x state = %d)\n", rc, machineState);
        goto leave;
    }

    gConsole->updateTitlebar();

#ifdef RT_OS_L4
    /* The L4 console provides (currently) a fixed resolution. */
    if (g_u32VRamSize >= gFramebuffer->getHostXres()
                       * gFramebuffer->getHostYres()
                       * (gDisplay->getBitsPerPixel() / 8))
        gDisplay->SetVideoModeHint(gFramebuffer->getHostXres(), gFramebuffer->getHostYres(), 0, 0);

    /* Limit the VRAM of the guest to the amount of memory we got actually
     * mapped from the L4 console. */
    u32MaxVRAM = (gFramebuffer->getHostYres() + 18) /* don't omit the status bar */
               * gFramebuffer->getHostXres()
               * (gFramebuffer->getHostBitsPerPixel() / 8);
    if (g_u32VRamSize > u32MaxVRAM)
    {
        RTPrintf("Limiting the video memory to %u bytes\n", u32MaxVRAM);
        g_u32VRamSize = u32MaxVRAM;
    }
#endif

    /*
     * Main event loop
     */
    LogFlow(("VBoxSDL: Entering big event loop\n"));

    while (1)
    {
        int event = gConsole->eventWait();

        switch (event)
        {
        case CONEVENT_NONE:
            /* Handled internally */
            break;

        case CONEVENT_QUIT:
        case CONEVENT_USR_QUIT:
            goto leave;

        case CONEVENT_SCREENUPDATE:
            /// @todo that somehow doesn't seem to work!
            gFramebuffer->repaint();
            break;

        case CONEVENT_USR_TITLEBARUPDATE:
            gConsole->updateTitlebar();
            break;

        case CONEVENT_USR_SCREENRESIZE:
        {
            LogFlow(("CONEVENT_USR_SCREENRESIZE\n"));
            gFramebuffer->resize();
            /* notify the display that the resize has been completed */
            gDisplay->ResizeCompleted();
            break;
        }

#ifdef VBOX_SECURELABEL
        case CONEVENT_USR_SECURELABELUPDATE:
        {
           /*
             * Query the new label text
             */
            Bstr key = VBOXSDL_SECURELABEL_EXTRADATA;
            Bstr label;
            gMachine->COMGETTER(ExtraData)(key, label.asOutParam());
            Utf8Str labelUtf8 = label;
            /*
             * Now update the label
             */
            gFramebuffer->setSecureLabelText(labelUtf8.raw());
            break;
        }
#endif /* VBOX_SECURELABEL */

        }

    }

leave:
    LogFlow(("Returning from main()!\n"));

    if (pVM)
    {
        /*
         * If get here because the guest terminated using ACPI off we don't have to
         * switch off the VM because we were notified via vmstateChangeCallback()
         * that this already happened. In any other case stop the VM before killing her.
         */
        if (machineState != VMSTATE_OFF)
        {
            /* Power off VM */
            PVMREQ pReq;
            rc = VMR3ReqCall(pVM, VMREQDEST_ANY, &pReq, RT_INDEFINITE_WAIT, (PFNRT)VMR3PowerOff, 1, pVM);
        }

        /* And destroy it */
        rc = VMR3Destroy(pVM);
        AssertRC(rc);
    }

    delete gFramebuffer;
    delete gConsole;
    delete gDisplay;
    delete gKeyboard;
    delete gMouse;
    delete gStatus;
    delete gMachineDebugger;

    RTLogFlush(NULL);
    return RT_FAILURE (rc) ? 1 : 0;
}


#ifndef VBOX_WITH_HARDENING
/**
 * Main entry point.
 */
int main(int argc, char **argv)
{
# ifdef RT_OS_L4
# ifndef L4API_l4v2onv4
    /* clear Fiasco kernel trace buffer */
    fiasco_tbuf_clear();
# endif
    /* set the environment.  Must be done before the runtime is
       initialised.  Yes, it really must. */
    for (int i = 0; i < argc; i++)
        if (strcmp(argv[i], "-env") == 0)
        {
            if (++i >= argc)
                return SyntaxError("missing argument to -env (format: var=value)!\n");
            /* add it to the environment */
            if (putenv(argv[i]) != 0)
                return SyntaxError("Error setting environment string %s.\n", argv[i]);
        }
# endif /* RT_OS_L4 */

    /*
     * Before we do *anything*, we initialize the runtime.
     */
    int rc = RTR3Init();
    if (RT_FAILURE(rc))
        return FatalError("RTR3Init failed rc=%Rrc\n", rc);

    return TrustedMain(argc, argv, NULL);
}
#endif /* !VBOX_WITH_HARDENING */


/**
 * VM state callback function. Called by the VMM
 * using its state machine states.
 *
 * Primarily used to handle VM initiated power off, suspend and state saving,
 * but also for doing termination completed work (VMSTATE_TERMINATE).
 *
 * In general this function is called in the context of the EMT.
 *
 * @todo machineState is set to VMSTATE_RUNNING before all devices have received power on events
 *       this can prematurely allow the main thread to enter the event loop
 *
 * @param   pVM         The VM handle.
 * @param   enmState    The new state.
 * @param   enmOldState The old state.
 * @param   pvUser      The user argument.
 */
static DECLCALLBACK(void) vmstateChangeCallback(PVM pVM, VMSTATE enmState, VMSTATE enmOldState, void *pvUser)
{
    LogFlow(("vmstateChangeCallback: changing state from %d to %d\n", enmOldState, enmState));
    machineState = enmState;

    switch (enmState)
    {
        /*
         * The VM has terminated
         */
        case VMSTATE_OFF:
        {
            gConsole->eventQuit();
            break;
        }

        /*
         * The VM has been completely destroyed.
         *
         * Note: This state change can happen at two points:
         *       1) At the end of VMR3Destroy() if it was not called from EMT.
         *       2) At the end of vmR3EmulationThread if VMR3Destroy() was called by EMT.
         */
        case VMSTATE_TERMINATED:
        {
            break;
        }

        default: /* shut up gcc */
            break;
    }
}


/**
 * VM error callback function. Called by the various VM components.
 *
 * @param   pVM         The VM handle.
 * @param   pvUser      The user argument.
 * @param   rc          VBox status code.
 * @param   pszError    Error message format string.
 * @param   args        Error message arguments.
 * @thread EMT.
 */
DECLCALLBACK(void) setVMErrorCallback(PVM pVM, void *pvUser, int rc, RT_SRC_POS_DECL,
                                      const char *pszFormat, va_list args)
{
    /** @todo accessing shared resource without any kind of synchronization */
    if (RT_SUCCESS(rc))
        szError[0] = '\0';
    else
    {
        va_list va2;
        va_copy(va2, args); /* Have to make a copy here or GCC will break. */
        RTStrPrintf(szError, sizeof(szError),
                    "%N!\nVBox status code: %d (%Rrc)", pszFormat, &va2, rc, rc);
        RTPrintf("%s\n", szError);
        va_end(va2);
    }
}


/**
 * VM Runtime error callback function. Called by the various VM components.
 *
 * @param   pVM         The VM handle.
 * @param   pvUser      The user argument.
 * @param   fFata       Wheather it is a fatal error or not.
 * @param   pszErrorId  Error ID string.
 * @param   pszError    Error message format string.
 * @param   args        Error message arguments.
 * @thread EMT.
 */
DECLCALLBACK(void) setVMRuntimeErrorCallback(PVM pVM, void *pvUser, bool fFatal,
                                             const char *pszErrorId,
                                             const char *pszFormat, va_list args)
{
    va_list va2;
    va_copy(va2, args); /* Have to make a copy here or GCC will break. */
    RTPrintf("%s: %s!\n%N!\n", fFatal ? "Error" : "Warning", pszErrorId, pszFormat, &va2);
    va_end(va2);
}


/** VM asynchronous operations thread */
DECLCALLBACK(int) VMPowerUpThread(RTTHREAD Thread, void *pvUser)
{
    int rc = VINF_SUCCESS;
    int rc2;

    /*
     * Setup the release log instance in current directory.
     */
    if (g_fReleaseLog)
    {
        static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
        static char szError[RTPATH_MAX + 128] = "";
        PRTLOGGER pLogger;
        rc2 = RTLogCreateEx(&pLogger, RTLOGFLAGS_PREFIX_TIME_PROG, "all",
                            "VBOX_RELEASE_LOG", RT_ELEMENTS(s_apszGroups), s_apszGroups,
                            RTLOGDEST_FILE, szError, sizeof(szError), "./VBoxBFE.log");
        if (RT_SUCCESS(rc2))
        {
            /* some introductory information */
            RTTIMESPEC TimeSpec;
            char szNowUct[64];
            RTTimeSpecToString(RTTimeNow(&TimeSpec), szNowUct, sizeof(szNowUct));
            RTLogRelLogger(pLogger, 0, ~0U,
                           "VBoxBFE %s (%s %s) release log\n"
                           "Log opened %s\n",
                           VBOX_VERSION_STRING, __DATE__, __TIME__,
                           szNowUct);

            /* register this logger as the release logger */
            RTLogRelSetDefaultInstance(pLogger);
        }
        else
            RTPrintf("Could not open release log (%s)\n", szError);
    }

    /*
     * Start VM (also from saved state) and track progress
     */
    LogFlow(("VMPowerUp\n"));

    /*
     * Create empty VM.
     */
    rc = VMR3Create(1, setVMErrorCallback, NULL, vboxbfeConfigConstructor, NULL, &pVM);
    if (RT_FAILURE(rc))
    {
        RTPrintf("Error: VM creation failed with %Rrc.\n", rc);
        goto failure;
    }


    /*
     * Register VM state change handler
     */
    rc = VMR3AtStateRegister(pVM, vmstateChangeCallback, NULL);
    if (RT_FAILURE(rc))
    {
        RTPrintf("Error: VMR3AtStateRegister failed with %Rrc.\n", rc);
        goto failure;
    }

#ifdef VBOX_WITH_HGCM
    /*
     * Add shared folders to the VM
     */
    if (fActivateHGCM() && gVMMDev->isShFlActive())
    {
        for (unsigned i=0; i<g_uNumShares; i++)
        {
            VBOXHGCMSVCPARM  parms[SHFL_CPARMS_ADD_MAPPING];
            SHFLSTRING      *pFolderName, *pMapName;
            int              cbString;
            PRTUTF16         aHostPath, aMapName;
            int              rc;

            rc = RTStrToUtf16(g_pszShareDir[i], &aHostPath);
            AssertRC(rc);
            rc = RTStrToUtf16(g_pszShareName[i], &aMapName);
            AssertRC(rc);

            cbString = (RTUtf16Len (aHostPath) + 1) * sizeof (RTUTF16);
            pFolderName = (SHFLSTRING *) RTMemAllocZ (sizeof (SHFLSTRING) + cbString);
            Assert (pFolderName);
            memcpy (pFolderName->String.ucs2, aHostPath, cbString);

            pFolderName->u16Size   = cbString;
            pFolderName->u16Length = cbString - sizeof(RTUTF16);

            parms[0].type = VBOX_HGCM_SVC_PARM_PTR;
            parms[0].u.pointer.addr = pFolderName;
            parms[0].u.pointer.size = sizeof (SHFLSTRING) + cbString;

            cbString = (RTUtf16Len (aMapName) + 1) * sizeof (RTUTF16);
            pMapName = (SHFLSTRING *) RTMemAllocZ (sizeof(SHFLSTRING) + cbString);
            Assert (pMapName);
            memcpy (pMapName->String.ucs2, aMapName, cbString);

            pMapName->u16Size   = cbString;
            pMapName->u16Length = cbString - sizeof (RTUTF16);

            parms[1].type = VBOX_HGCM_SVC_PARM_PTR;
            parms[1].u.pointer.addr = pMapName;
            parms[1].u.pointer.size = sizeof (SHFLSTRING) + cbString;

            parms[2].type = VBOX_HGCM_SVC_PARM_32BIT;
            parms[2].u.uint32 = !g_fShareReadOnly[i];

            rc = gVMMDev->hgcmHostCall ("VBoxSharedFolders",
                                        SHFL_FN_ADD_MAPPING, SHFL_CPARMS_ADD_MAPPING, &parms[0]);
            AssertRC(rc);
            LogRel(("Added share %s: (%s)\n", g_pszShareName[i], g_pszShareDir[i]));
            RTMemFree (pFolderName);
            RTMemFree (pMapName);
            RTUtf16Free (aHostPath);
            RTUtf16Free (aMapName);
        }
    }
#endif

#ifdef VBOXBFE_WITH_USB
    /*
     * Capture USB devices.
     */
    if (g_fUSB)
    {
        gHostUSB = new HostUSB();
        gHostUSB->init(pVM);
    }
#endif /* VBOXBFE_WITH_USB */

#ifdef RT_OS_L4
    /* L4 console cannot draw a host cursor */
    gMouse->setHostCursor(false);
#else
    gMouse->setHostCursor(true);
#endif

    /*
     * Power on the VM (i.e. start executing).
     */
    if (RT_SUCCESS(rc))
    {
        PVMREQ pReq;

        if (   g_fRestoreState
            && g_pszStateFile
            && *g_pszStateFile
            && RTPathExists(g_pszStateFile))
        {
            startProgressInfo("Restoring");
            rc = VMR3ReqCall(pVM, VMREQDEST_ANY, &pReq, RT_INDEFINITE_WAIT,
                             (PFNRT)VMR3Load, 4, pVM, g_pszStateFile, &callProgressInfo, (uintptr_t)NULL);
            endProgressInfo();
            if (RT_SUCCESS(rc))
            {
                VMR3ReqFree(pReq);
                rc = VMR3ReqCall(pVM, VMREQDEST_ANY, &pReq, RT_INDEFINITE_WAIT,
                                 (PFNRT)VMR3Resume, 1, pVM);
                if (RT_SUCCESS(rc))
                {
                    rc = pReq->iStatus;
                    VMR3ReqFree(pReq);
                }
                gDisplay->setRunning();
            }
            else
                AssertMsgFailed(("VMR3Load failed, rc=%Rrc\n", rc));
        }
        else
        {
            rc = VMR3ReqCall(pVM, VMREQDEST_ANY, &pReq, RT_INDEFINITE_WAIT, (PFNRT)VMR3PowerOn, 1, pVM);
            if (RT_SUCCESS(rc))
            {
                rc = pReq->iStatus;
                AssertRC(rc);
                VMR3ReqFree(pReq);
            }
            else
                AssertMsgFailed(("VMR3PowerOn failed, rc=%Rrc\n", rc));
        }
    }

    /*
     * On failure destroy the VM.
     */
    if (RT_FAILURE(rc))
        goto failure;

    return 0;

failure:
    if (pVM)
    {
        rc2 = VMR3Destroy(pVM);
        AssertRC(rc2);
        pVM = NULL;
    }
    machineState = VMSTATE_TERMINATED;

    return 0;
}

/**
 * Register the main drivers.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VBox version number.
 */
DECLCALLBACK(int) VBoxDriversRegister(PCPDMDRVREGCB pCallbacks, uint32_t u32Version)
{
    int rc;

    LogFlow(("VBoxDriversRegister: u32Version=%#x\n", u32Version));
    AssertReleaseMsg(u32Version == VBOX_VERSION, ("u32Version=%#x VBOX_VERSION=%#x\n", u32Version, VBOX_VERSION));

    rc = pCallbacks->pfnRegister(pCallbacks, &Mouse::DrvReg);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &Keyboard::DrvReg);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return rc;

    rc = pCallbacks->pfnRegister(pCallbacks, &VMDisplay::DrvReg);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &VMMDev::DrvReg);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return rc;

    rc = pCallbacks->pfnRegister(pCallbacks, &VMStatus::DrvReg);
    if (RT_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}


/**
 * Constructs the VMM configuration tree.
 *
 * @returns VBox status code.
 * @param   pVM     VM handle.
 */
static DECLCALLBACK(int) vboxbfeConfigConstructor(PVM pVM, void *pvUser)
{
    int rcAll = VINF_SUCCESS;
    int rc;

#define UPDATE_RC() do { if (RT_FAILURE(rc) && RT_SUCCESS(rcAll)) rcAll = rc; } while (0)

    /*
     * Root values.
     */
    PCFGMNODE pRoot = CFGMR3GetRoot(pVM);
    rc = CFGMR3InsertString(pRoot,  "Name",           "Default VM");                UPDATE_RC();
    rc = CFGMR3InsertInteger(pRoot, "RamSize",        g_u32MemorySizeMB * _1M);     UPDATE_RC();
    rc = CFGMR3InsertInteger(pRoot, "RamHoleSize",    512U * _1M);                  UPDATE_RC();
    if (g_fPreAllocRam)
    {
        rc = CFGMR3InsertInteger(pRoot, "RamPreAlloc",    1);                       UPDATE_RC();
    }
    rc = CFGMR3InsertInteger(pRoot, "TimerMillies",   10);                          UPDATE_RC();
#ifdef VBOXSDL_ADVANCED_OPTIONS
    rc = CFGMR3InsertInteger(pRoot, "RawR3Enabled",   g_fRawR3);                    UPDATE_RC();
    rc = CFGMR3InsertInteger(pRoot, "RawR0Enabled",   g_fRawR0);                    UPDATE_RC();
    rc = CFGMR3InsertInteger(pRoot, "PATMEnabled",    g_fPATM);                     UPDATE_RC();
    rc = CFGMR3InsertInteger(pRoot, "CSAMEnabled",    g_fCSAM);                     UPDATE_RC();
#else
    rc = CFGMR3InsertInteger(pRoot, "RawR3Enabled",   1);                           UPDATE_RC();
    rc = CFGMR3InsertInteger(pRoot, "RawR0Enabled",   1);                           UPDATE_RC();
    rc = CFGMR3InsertInteger(pRoot, "PATMEnabled",    1);                           UPDATE_RC();
    rc = CFGMR3InsertInteger(pRoot, "CSAMEnabled",    1);                           UPDATE_RC();
#endif

    /*
     * PDM.
     */
    rc = PDMR3RegisterDrivers(pVM, VBoxDriversRegister);                            UPDATE_RC();

    /*
     * Devices
     */
    PCFGMNODE pDevices = NULL;
    rc = CFGMR3InsertNode(pRoot, "Devices", &pDevices);                             UPDATE_RC();
    /* device */
    PCFGMNODE pDev = NULL;
    PCFGMNODE pInst = NULL;
    PCFGMNODE pCfg = NULL;
    PCFGMNODE pLunL0 = NULL;
    PCFGMNODE pLunL1 = NULL;
    PCFGMNODE pDrv = NULL;

    /*
     * PC Arch.
     */
    rc = CFGMR3InsertNode(pDevices, "pcarch",         &pDev);                       UPDATE_RC();
    rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",        1);           /* boolean */   UPDATE_RC();
    rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                       UPDATE_RC();

    /*
     * PC Bios.
     */
    rc = CFGMR3InsertNode(pDevices, "pcbios",         &pDev);                       UPDATE_RC();
    rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   UPDATE_RC();
    rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                       UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "RamSize",        g_u32MemorySizeMB * _1M);     UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "RamHoleSize",    512U * _1M);                  UPDATE_RC();
    rc = CFGMR3InsertString(pCfg,   "BootDevice0",    g_pszBootDevice);             UPDATE_RC();
    rc = CFGMR3InsertString(pCfg,   "BootDevice1",    "NONE");                      UPDATE_RC();
    rc = CFGMR3InsertString(pCfg,   "BootDevice2",    "NONE");                      UPDATE_RC();
    rc = CFGMR3InsertString(pCfg,   "BootDevice3",    "NONE");                      UPDATE_RC();
    rc = CFGMR3InsertString(pCfg,   "HardDiskDevice", "piix3ide");                  UPDATE_RC();
    rc = CFGMR3InsertString(pCfg,   "FloppyDevice",   "i82078");                    UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "IOAPIC",         g_fIOAPIC);                   UPDATE_RC();
    RTUUID Uuid;
    RTUuidClear(&Uuid);
    rc = CFGMR3InsertBytes(pCfg,    "UUID", &Uuid, sizeof(Uuid));                   UPDATE_RC();

    /*
     * ACPI
     */
    if (g_fACPI)
    {
        rc = CFGMR3InsertNode(pDevices, "acpi",           &pDev);                   UPDATE_RC();
        rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                  UPDATE_RC();
        rc = CFGMR3InsertInteger(pInst, "Trusted",        1);       /* boolean */   UPDATE_RC();
        rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                   UPDATE_RC();
        rc = CFGMR3InsertInteger(pCfg,  "RamSize",        g_u32MemorySizeMB * _1M); UPDATE_RC();
        rc = CFGMR3InsertInteger(pCfg,  "RamHoleSize",    512U * _1M);              UPDATE_RC();
        rc = CFGMR3InsertInteger(pCfg,  "IOAPIC",         g_fIOAPIC);               UPDATE_RC();
        rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",    7);                       UPDATE_RC();
        rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",  0);                       UPDATE_RC();

        rc = CFGMR3InsertNode(pInst,    "LUN#0",          &pLunL0);                 UPDATE_RC();
        rc = CFGMR3InsertString(pLunL0, "Driver",         "ACPIHost");              UPDATE_RC();
        rc = CFGMR3InsertNode(pLunL0,   "Config",         &pCfg);                   UPDATE_RC();
    }

    /*
     * PCI bus.
     */
    rc = CFGMR3InsertNode(pDevices, "pci",            &pDev);       /* piix3 */     UPDATE_RC();
    rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",        1);           /* boolean */   UPDATE_RC();
    rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                       UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "IOAPIC",         g_fIOAPIC);                   UPDATE_RC();

    /*
     * DMA
     */
    rc = CFGMR3InsertNode(pDevices, "8237A",          &pDev);                       UPDATE_RC();
    rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",        1);           /* boolean */   UPDATE_RC();

    /*
     * PCI bus.
     */
    rc = CFGMR3InsertNode(pDevices, "pci",            &pDev);       /* piix3 */     UPDATE_RC();
    rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",        1);           /* boolean */   UPDATE_RC();
    rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                       UPDATE_RC();

    /*
     * PS/2 keyboard & mouse.
     */
    rc = CFGMR3InsertNode(pDevices, "pckbd",          &pDev);                       UPDATE_RC();
    rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",        1);           /* boolean */   UPDATE_RC();
    rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                       UPDATE_RC();

    rc = CFGMR3InsertNode(pInst,    "LUN#0",          &pLunL0);                     UPDATE_RC();
    rc = CFGMR3InsertString(pLunL0, "Driver",         "KeyboardQueue");             UPDATE_RC();
    rc = CFGMR3InsertNode(pLunL0,   "Config",         &pCfg);                       UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "QueueSize",      64);                          UPDATE_RC();

    rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pDrv);                       UPDATE_RC();
    rc = CFGMR3InsertString(pDrv,   "Driver",         "MainKeyboard");              UPDATE_RC();
    rc = CFGMR3InsertNode(pDrv,     "Config",         &pCfg);                       UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "Object",         (uintptr_t)gKeyboard);        UPDATE_RC();

    rc = CFGMR3InsertNode(pInst,    "LUN#1",          &pLunL0);                     UPDATE_RC();
    rc = CFGMR3InsertString(pLunL0, "Driver",         "MouseQueue");                UPDATE_RC();
    rc = CFGMR3InsertNode(pLunL0,   "Config",         &pCfg);                       UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "QueueSize",      128);                         UPDATE_RC();

    rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pDrv);                       UPDATE_RC();
    rc = CFGMR3InsertString(pDrv,   "Driver",         "MainMouse");                 UPDATE_RC();
    rc = CFGMR3InsertNode(pDrv,     "Config",         &pCfg);                       UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "Object",         (uintptr_t)gMouse);           UPDATE_RC();


    /*
     * i82078 Floppy drive controller
     */
    rc = CFGMR3InsertNode(pDevices, "i82078",         &pDev);                       UPDATE_RC();
    rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",        1);                           UPDATE_RC();
    rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                       UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "IRQ",            6);                           UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "DMA",            2);                           UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "MemMapped",      0 );                          UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "IOBase",         0x3f0);                       UPDATE_RC();

    /* Attach the status driver */
    rc = CFGMR3InsertNode(pInst,    "LUN#999",        &pLunL0);                     UPDATE_RC();
    rc = CFGMR3InsertString(pLunL0, "Driver",         "MainStatus");                UPDATE_RC();
    rc = CFGMR3InsertNode(pLunL0,   "Config",         &pCfg);                       UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "papLeds",        (uintptr_t)&mapFDLeds[0]);    UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "First",          0);                           UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "Last",           0);                           UPDATE_RC();

    if (g_pszFdaFile)
    {
        rc = CFGMR3InsertNode(pInst,    "LUN#0",          &pLunL0);                 UPDATE_RC();
        rc = CFGMR3InsertString(pLunL0, "Driver",         "Block");                 UPDATE_RC();
        rc = CFGMR3InsertNode(pLunL0,   "Config",         &pCfg);                   UPDATE_RC();
        rc = CFGMR3InsertString(pCfg,   "Type",           "Floppy 1.44");           UPDATE_RC();
        rc = CFGMR3InsertInteger(pCfg,  "Mountable", 1);                            UPDATE_RC();

        rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pDrv);                   UPDATE_RC();
        rc = CFGMR3InsertString(pDrv,   "Driver",         "RawImage");              UPDATE_RC();
        rc = CFGMR3InsertNode(pDrv,     "Config",         &pCfg);                   UPDATE_RC();
        rc = CFGMR3InsertString(pCfg,   "Path",           g_pszFdaFile);            UPDATE_RC();
    }

    /*
     * i8254 Programmable Interval Timer And Dummy Speaker
     */
    rc = CFGMR3InsertNode(pDevices, "i8254",          &pDev);                       UPDATE_RC();
    rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                       UPDATE_RC();
#ifdef DEBUG
    rc = CFGMR3InsertInteger(pInst, "Trusted",        1);           /* boolean */   UPDATE_RC();
#endif

    /*
     * i8259 Programmable Interrupt Controller.
     */
    rc = CFGMR3InsertNode(pDevices, "i8259",          &pDev);                       UPDATE_RC();
    rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",        1);           /* boolean */   UPDATE_RC();
    rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                       UPDATE_RC();

    /*
     * Advanced Programmable Interrupt Controller.
     */
    rc = CFGMR3InsertNode(pDevices, "apic",           &pDev);                       UPDATE_RC();
    rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",        1);           /* boolean */   UPDATE_RC();
    rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                       UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "IOAPIC",         g_fIOAPIC);                   UPDATE_RC();

    /*
     * I/O Advanced Programmable Interrupt Controller.
     */
    if (g_fIOAPIC)
    {
        rc = CFGMR3InsertNode(pDevices, "ioapic",         &pDev);                   UPDATE_RC();
        rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                  UPDATE_RC();
        rc = CFGMR3InsertInteger(pInst, "Trusted",        1);       /* boolean */   UPDATE_RC();
        rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                   UPDATE_RC();
    }

    /*
     * RTC MC146818.
     */
    rc = CFGMR3InsertNode(pDevices, "mc146818",       &pDev);                       UPDATE_RC();
    rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                       UPDATE_RC();

    /*
     * Serial ports
     */
    rc = CFGMR3InsertNode(pDevices, "serial",         &pDev);                       UPDATE_RC();
    rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                       UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "IRQ",            4);                           UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "IOBase",         0x3f8);                       UPDATE_RC();

    rc = CFGMR3InsertNode(pDev,     "1",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                       UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "IRQ",            3);                           UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "IOBase",         0x2f8);                       UPDATE_RC();

    /*
     * VGA.
     */
    rc = CFGMR3InsertNode(pDevices, "vga",            &pDev);                       UPDATE_RC();
    rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",        1);           /* boolean */   UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",    2);                           UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",  0);                           UPDATE_RC();
    rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                       UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "VRamSize",       g_u32VRamSize);               UPDATE_RC();

    /* Default: no bios logo. */
    rc = CFGMR3InsertInteger(pCfg,  "FadeIn",         1);                           UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "FadeOut",        0);                           UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "LogoTime",       0);                           UPDATE_RC();
    rc = CFGMR3InsertString(pCfg,   "LogoFile",       "");                          UPDATE_RC();

    /* Boot menu */
    rc = CFGMR3InsertInteger(pCfg,  "ShowBootMenu",   g_iBootMenu);                 UPDATE_RC();

#ifdef RT_OS_L4
    /* XXX hard-coded */
    rc = CFGMR3InsertInteger(pCfg,  "HeightReduction", 18);                         UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "CustomVideoModes", 1);                         UPDATE_RC();
    char szBuf[64];
    /* Tell the guest which is the ideal video mode to use */
    RTStrPrintf(szBuf, sizeof(szBuf), "%dx%dx%d",
                gFramebuffer->getHostXres(),
                gFramebuffer->getHostYres(),
                gFramebuffer->getHostBitsPerPixel());
    rc = CFGMR3InsertString(pCfg,   "CustomVideoMode1", szBuf);                     UPDATE_RC();
#endif

    rc = CFGMR3InsertNode(pInst,    "LUN#0",          &pLunL0);                     UPDATE_RC();
    rc = CFGMR3InsertString(pLunL0, "Driver",         "MainDisplay");               UPDATE_RC();
    rc = CFGMR3InsertNode(pLunL0,   "Config",         &pCfg);                       UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "Object",         (uintptr_t)gDisplay);         UPDATE_RC();

    /*
     * IDE (update this when the main interface changes)
     */
    rc = CFGMR3InsertNode(pDevices, "piix3ide",       &pDev); /* piix3 */           UPDATE_RC();
    rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",        1);     /* boolean */         UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",    1);                           UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",  1);                           UPDATE_RC();
    rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                       UPDATE_RC();

    if (g_pszHdaFile)
    {
        rc = CFGMR3InsertNode(pInst,    "LUN#0",          &pLunL0);                 UPDATE_RC();
        rc = CFGMR3InsertString(pLunL0, "Driver",         "Block");                 UPDATE_RC();
        rc = CFGMR3InsertNode(pLunL0,   "Config",         &pCfg);                   UPDATE_RC();
        rc = CFGMR3InsertString(pCfg,   "Type",           "HardDisk");              UPDATE_RC();
        rc = CFGMR3InsertInteger(pCfg,  "Mountable",      0);                       UPDATE_RC();

        rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pDrv);                   UPDATE_RC();
        rc = CFGMR3InsertString(pDrv,   "Driver",         "VD");                    UPDATE_RC();
        rc = CFGMR3InsertNode(pDrv,     "Config",         &pCfg);                   UPDATE_RC();
        rc = CFGMR3InsertString(pCfg,   "Path",           g_pszHdaFile);            UPDATE_RC();

        if (g_fHdaSpf)
        {
            rc = CFGMR3InsertString(pCfg, "Format",       "SPF");                   UPDATE_RC();
        }
        else
        {
            char *pcExt = RTPathExt(g_pszHdaFile);
            if ((pcExt) && (!strcmp(pcExt, ".vdi")))
            {
                rc = CFGMR3InsertString(pCfg, "Format",       "VDI");               UPDATE_RC();
            }
            else
            {
                rc = CFGMR3InsertString(pCfg, "Format",       "VMDK");              UPDATE_RC();
            }
        }
    }

    if (g_pszHdbFile)
    {
        rc = CFGMR3InsertNode(pInst,    "LUN#1",          &pLunL1);                 UPDATE_RC();
        rc = CFGMR3InsertString(pLunL1, "Driver",         "Block");                 UPDATE_RC();
        rc = CFGMR3InsertNode(pLunL1,   "Config",         &pCfg);                   UPDATE_RC();
        rc = CFGMR3InsertString(pCfg,   "Type",           "HardDisk");              UPDATE_RC();
        rc = CFGMR3InsertInteger(pCfg,  "Mountable",      0);                       UPDATE_RC();

        rc = CFGMR3InsertNode(pLunL1,   "AttachedDriver", &pDrv);                   UPDATE_RC();
        rc = CFGMR3InsertString(pDrv,   "Driver",         "VD");                    UPDATE_RC();
        rc = CFGMR3InsertNode(pDrv,     "Config",         &pCfg);                   UPDATE_RC();
        rc = CFGMR3InsertString(pCfg,   "Path",           g_pszHdbFile);            UPDATE_RC();

        if (g_fHdbSpf)
        {
            rc = CFGMR3InsertString(pCfg, "Format",       "SPF");                   UPDATE_RC();
        }
        else
        {
            char *pcExt = RTPathExt(g_pszHdbFile);
            if ((pcExt) && (!strcmp(pcExt, ".vdi")))
            {
                rc = CFGMR3InsertString(pCfg, "Format",       "VDI");               UPDATE_RC();
            }
            else
            {
                rc = CFGMR3InsertString(pCfg, "Format",       "VMDK");              UPDATE_RC();
            }
        }
    }

    if (g_pszCdromFile)
    {
        // ASSUME: DVD drive is always attached to LUN#2 (i.e. secondary IDE master)
        rc = CFGMR3InsertNode(pInst,    "LUN#2",          &pLunL0);                 UPDATE_RC();
        rc = CFGMR3InsertString(pLunL0, "Driver",         "Block");                 UPDATE_RC();
        rc = CFGMR3InsertNode(pLunL0,   "Config",         &pCfg);                   UPDATE_RC();
        rc = CFGMR3InsertString(pCfg,   "Type",           "DVD");                   UPDATE_RC();
        rc = CFGMR3InsertInteger(pCfg,  "Mountable",      1);                       UPDATE_RC();

        rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pDrv);                   UPDATE_RC();
        rc = CFGMR3InsertString(pDrv,   "Driver",         "MediaISO");              UPDATE_RC();
        rc = CFGMR3InsertNode(pDrv,     "Config",         &pCfg);                   UPDATE_RC();
        rc = CFGMR3InsertString(pCfg,   "Path",           g_pszCdromFile);          UPDATE_RC();
    }

    /*
     * Network adapters
     */
    rc = CFGMR3InsertNode(pDevices, "pcnet",          &pDev);                       UPDATE_RC();
    for (ULONG ulInstance = 0; ulInstance < NetworkAdapterCount; ulInstance++)
    {
        if (g_aNetDevs[ulInstance].enmType != BFENETDEV::NOT_CONFIGURED)
        {
            char szInstance[4];
            RTStrPrintf(szInstance, sizeof(szInstance), "%lu", ulInstance);
            rc = CFGMR3InsertNode(pDev, szInstance, &pInst);                        UPDATE_RC();
            rc = CFGMR3InsertInteger(pInst, "Trusted", 1);                          UPDATE_RC();
            rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",
                                             !ulInstance ? 3 : ulInstance - 1 + 8); UPDATE_RC();
            rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",  0);                   UPDATE_RC();
            rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);               UPDATE_RC();
            rc = CFGMR3InsertBytes(pCfg,    "MAC",            &g_aNetDevs[ulInstance].Mac, sizeof(RTMAC));
                                                                                    UPDATE_RC();

            /*
             * Enable the packet sniffer if requested.
             */
            if (g_aNetDevs[ulInstance].fSniff)
            {
                /* insert the sniffer filter driver. */
                rc = CFGMR3InsertNode(pInst,    "LUN#0",          &pLunL0);         UPDATE_RC();
                rc = CFGMR3InsertString(pLunL0, "Driver",         "NetSniffer");    UPDATE_RC();
                rc = CFGMR3InsertNode(pLunL0,   "Config",         &pCfg);           UPDATE_RC();
                if (g_aNetDevs[ulInstance].pszSniff)
                {
                    rc = CFGMR3InsertString(pCfg,   "File", g_aNetDevs[ulInstance].pszSniff);
                                                                                    UPDATE_RC();
                }
            }

            /*
             * Create the driver config (if any).
             */
            if (g_aNetDevs[ulInstance].enmType != BFENETDEV::NONE)
            {
                if (g_aNetDevs[ulInstance].fSniff)
                {
                    rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);       UPDATE_RC();
                }
                else
                {
                    rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                 UPDATE_RC();
                }
                rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                     UPDATE_RC();
            }

            /*
             * Configure the driver.
             */
            if (g_aNetDevs[ulInstance].enmType == BFENETDEV::NAT)
            {
                rc = CFGMR3InsertString(pLunL0, "Driver", "NAT");                   UPDATE_RC();
                rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                     UPDATE_RC();
                /* (Port forwarding goes here.) */
            }
            else if (g_aNetDevs[ulInstance].enmType == BFENETDEV::HIF)
            {
                rc = CFGMR3InsertString(pLunL0, "Driver", "HostInterface");         UPDATE_RC();
                rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                     UPDATE_RC();

#if defined(RT_OS_LINUX)
                if (g_aNetDevs[ulInstance].fHaveFd)
                {
                    rc = CFGMR3InsertString(pCfg, "Device", g_aNetDevs[ulInstance].pszName); UPDATE_RC();
                    rc = CFGMR3InsertInteger(pCfg, "FileHandle", g_aNetDevs[ulInstance].fd); UPDATE_RC();
                }
                else
#endif
                {
#if defined(RT_OS_LINUX) || defined(RT_OS_L4)
                    /*
                     * Create/Open the TAP the device.
                     */
                    RTFILE tapFD;
                    rc = RTFileOpen(&tapFD, "/dev/net/tun",
                                    RTFILE_O_READWRITE | RTFILE_O_OPEN |
                                    RTFILE_O_DENY_NONE | RTFILE_O_INHERIT);
                    if (RT_FAILURE(rc))
                    {
                        FatalError("Failed to open /dev/net/tun: %Rrc\n", rc);
                        return rc;
                    }

                    struct ifreq IfReq;
                    memset(&IfReq, 0, sizeof(IfReq));
                    if (g_aNetDevs[ulInstance].pszName && g_aNetDevs[ulInstance].pszName[0])
                    {
                        size_t cch = strlen(g_aNetDevs[ulInstance].pszName);
                        if (cch >= sizeof(IfReq.ifr_name))
                        {
                            FatalError("HIF name too long for device #%d: %s\n",
                                       ulInstance + 1, g_aNetDevs[ulInstance].pszName);
                            return VERR_BUFFER_OVERFLOW;
                        }
                        memcpy(IfReq.ifr_name, g_aNetDevs[ulInstance].pszName, cch + 1);
                    }
                    else
                        strcpy(IfReq.ifr_name, "tun%d");
                    IfReq.ifr_flags = IFF_TAP | IFF_NO_PI;
                    rc = ioctl(tapFD, TUNSETIFF, &IfReq);
                    if (rc)
                    {
                        int rc2 = RTErrConvertFromErrno(errno);
                        FatalError("ioctl TUNSETIFF '%s' failed: errno=%d rc=%d (%Rrc)\n",
                                   IfReq.ifr_name, errno, rc, rc2);
                        return rc2;
                    }

                    rc = fcntl(tapFD, F_SETFL, O_NONBLOCK);
                    if (rc)
                    {
                        int rc2 = RTErrConvertFromErrno(errno);
                        FatalError("fcntl F_SETFL/O_NONBLOCK '%s' failed: errno=%d rc=%d (%Rrc)\n",
                                   IfReq.ifr_name, errno, rc, rc2);
                        return rc2;
                    }

                    rc = CFGMR3InsertString(pCfg, "Device", g_aNetDevs[ulInstance].pszName);        UPDATE_RC();
                    rc = CFGMR3InsertInteger(pCfg, "FileHandle", (RTFILE)tapFD);                    UPDATE_RC();

#elif defined(RT_OS_SOLARIS)
                    rc = CFGMR3InsertString(pCfg, "Device", g_aNetDevs[ulInstance].pszName); UPDATE_RC();
# ifdef VBOX_WITH_CROSSBOW
                    rc = CFGMR3InsertBytes(pCfg, "MAC", &g_aNetDevs[ulInstance].Mac, sizeof(g_aNetDevs[ulInstance].Mac));
                    UPDATE_RC();
# endif

#elif defined(RT_OS_OS2)
                    /*
                     * The TAP driver does all the opening and setting up,
                     * as it was originally was ment to be (stupid fork() problems).
                     */
                    rc = CFGMR3InsertString(pCfg, "Device", g_aNetDevs[ulInstance].pszName); UPDATE_RC();
                    if (g_aNetDevs[ulInstance].fHaveConnectTo)
                    {
                        rc = CFGMR3InsertInteger(pCfg, "ConnectTo", g_aNetDevs[ulInstance].iConnectTo);
                        UPDATE_RC();
                    }
#elif defined(RT_OS_WINDOWS)
                    /*
                     * We need the GUID too here...
                     */
                    rc = CFGMR3InsertString(pCfg, "Device", g_aNetDevs[ulInstance].pszName);            UPDATE_RC();
                    rc = CFGMR3InsertString(pCfg, "HostInterfaceName", g_aNetDevs[ulInstance].pszName); UPDATE_RC();
                    rc = CFGMR3InsertString(pCfg, "GUID", g_aNetDevs[ulInstance].pszName /*pszGUID*/);  UPDATE_RC();


#else
                    FatalError("Name based HIF devices not implemented yet for this host platform\n");
                    return VERR_NOT_IMPLEMENTED;
#endif
                }
            }
            else if (g_aNetDevs[ulInstance].enmType == BFENETDEV::INTNET)
            {
                /*
                 * Internal networking.
                 */
                rc = CFGMR3InsertString(pCfg, "Network", g_aNetDevs[ulInstance].pszName); UPDATE_RC();
            }
        }
    }

    /*
     * VMM Device
     */
    rc = CFGMR3InsertNode(pDevices, "VMMDev",         &pDev);                       UPDATE_RC();
    rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                      UPDATE_RC();
    rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                       UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",        1);           /* boolean */   UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",    4);                           UPDATE_RC();
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",  0);                           UPDATE_RC();

    /* the VMM device's Main driver */
    rc = CFGMR3InsertNode(pInst,    "LUN#0",          &pLunL0);                     UPDATE_RC();
    rc = CFGMR3InsertString(pLunL0, "Driver",        "MainVMMDev");                 UPDATE_RC();
    rc = CFGMR3InsertNode(pLunL0,   "Config",        &pCfg);                        UPDATE_RC();
    rc = CFGMR3InsertInteger(pCfg,  "Object",        (uintptr_t)gVMMDev);           UPDATE_RC();

    /*
     * AC'97 ICH audio
     */
    if (g_fAudio)
    {
        rc = CFGMR3InsertNode(pDevices, "ichac97",        &pDev);                   UPDATE_RC();
        rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                  UPDATE_RC();
        rc = CFGMR3InsertInteger(pInst, "Trusted",        1);       /* boolean */   UPDATE_RC();
        rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",    5);                       UPDATE_RC();
        rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",  0);                       UPDATE_RC();
        rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);

        /* the Audio driver */
        rc = CFGMR3InsertNode(pInst,    "LUN#0",          &pLunL0);                 UPDATE_RC();
        rc = CFGMR3InsertString(pLunL0, "Driver",         "AUDIO");                 UPDATE_RC();
        rc = CFGMR3InsertNode(pLunL0,   "Config",         &pCfg);                   UPDATE_RC();
#ifdef RT_OS_WINDOWS
        rc = CFGMR3InsertString(pCfg, "AudioDriver",      "winmm");                 UPDATE_RC();
#elif defined(RT_OS_DARWIN)
        rc = CFGMR3InsertString(pCfg, "AudioDriver",      "coreaudio");             UPDATE_RC();
#elif defined(RT_OS_LINUX)
        rc = CFGMR3InsertString(pCfg, "AudioDriver",      "oss");                   UPDATE_RC();
#elif defined(RT_OS_L4)
        rc = CFGMR3InsertString(pCfg, "AudioDriver",      "oss");                   UPDATE_RC();
#else /* portme */
        rc = CFGMR3InsertString(pCfg, "AudioDriver",      "none");                  UPDATE_RC();
#endif /* !RT_OS_WINDOWS */
    }

#ifdef VBOXBFE_WITH_USB
    /*
     * The USB Controller.
     */
    if (g_fUSB)
    {
        rc = CFGMR3InsertNode(pDevices, "usb-ohci",       &pDev);                   UPDATE_RC();
        rc = CFGMR3InsertNode(pDev,     "0",              &pInst);                  UPDATE_RC();
        rc = CFGMR3InsertNode(pInst,    "Config",         &pCfg);                   UPDATE_RC();
        rc = CFGMR3InsertInteger(pInst, "Trusted",        1);       /* boolean */   UPDATE_RC();
        rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",    6);                       UPDATE_RC();
        rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",  0);                       UPDATE_RC();

        rc = CFGMR3InsertNode(pInst,    "LUN#0",          &pLunL0);                 UPDATE_RC();
        rc = CFGMR3InsertString(pLunL0, "Driver",         "VUSBRootHub");           UPDATE_RC();
        rc = CFGMR3InsertNode(pLunL0,   "Config",         &pCfg);                   UPDATE_RC();
    }
#endif /* VBOXBFE_WITH_USB */

#undef UPDATE_RC
#undef UPDATE_RC

    VMR3AtRuntimeErrorRegister (pVM, setVMRuntimeErrorCallback, NULL);

    return rc;
}

