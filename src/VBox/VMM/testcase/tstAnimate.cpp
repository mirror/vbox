/** @file
 *
 * VBox Animation Testcase / Tool.
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
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/cpum.h>
#include <VBox/cfgm.h>
#include <VBox/pgm.h>
#include <VBox/rem.h>
#include <VBox/dbgf.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/runtime.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/file.h>
#include <iprt/thread.h>

#include <string.h>
#include <ctype.h>
#include <signal.h>

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static volatile bool g_fSignaled = false;


static void SigInterrupt(int iSignal)
{
    signal(SIGINT, SigInterrupt);
    g_fSignaled = true;
    RTPrintf("caught SIGINT\n");
}

typedef DECLCALLBACK(int) FNSETGUESTGPR(PVM, uint32_t);
typedef FNSETGUESTGPR *PFNSETGUESTGPR;
static int scriptGPReg(PVM pVM, char *pszVar, char *pszValue, void *pvUser)
{
    uint32_t u32;
    int rc = RTStrToUInt32Ex(pszValue, NULL, 16, &u32);
    if (VBOX_FAILURE(rc))
        return rc;
    return ((PFNSETGUESTGPR)(uintptr_t)pvUser)(pVM, u32);
}

typedef DECLCALLBACK(int) FNSETGUESTSEL(PVM, uint16_t);
typedef FNSETGUESTSEL *PFNSETGUESTSEL;
static int scriptSelReg(PVM pVM, char *pszVar, char *pszValue, void *pvUser)
{
    uint16_t u16;
    int rc = RTStrToUInt16Ex(pszValue, NULL, 16, &u16);
    if (VBOX_FAILURE(rc))
        return rc;
    return ((PFNSETGUESTSEL)(uintptr_t)pvUser)(pVM, u16);
}

typedef DECLCALLBACK(int) FNSETGUESTSYS(PVM, uint32_t);
typedef FNSETGUESTSYS *PFNSETGUESTSYS;
static int scriptSysReg(PVM pVM, char *pszVar, char *pszValue, void *pvUser)
{
    uint32_t u32;
    int rc = RTStrToUInt32Ex(pszValue, NULL, 16, &u32);
    if (VBOX_FAILURE(rc))
        return rc;
    return ((PFNSETGUESTSYS)(uintptr_t)pvUser)(pVM, u32);
}


typedef DECLCALLBACK(int) FNSETGUESTDTR(PVM, uint32_t, uint16_t);
typedef FNSETGUESTDTR *PFNSETGUESTDTR;
static int scriptDtrReg(PVM pVM, char *pszVar, char *pszValue, void *pvUser)
{
    char *pszPart2 = strchr(pszValue, ':');
    if (!pszPart2)
        return -1;
    *pszPart2++ = '\0';
    pszPart2 = RTStrStripL(pszPart2);
    pszValue = RTStrStripR(pszValue);

    uint32_t u32;
    int rc = RTStrToUInt32Ex(pszValue, NULL, 16, &u32);
    if (VBOX_FAILURE(rc))
        return rc;

    uint16_t u16;
    rc = RTStrToUInt16Ex(pszPart2, NULL, 16, &u16);
    if (VBOX_FAILURE(rc))
        return rc;

    return ((PFNSETGUESTDTR)(uintptr_t)pvUser)(pVM, u32, u16);
}







static int scriptCommand(PVM pVM, const char *pszIn, size_t cch)
{
    int rc = VINF_SUCCESS;
    char *psz = RTStrDup(pszIn);
    char *pszEqual = strchr(psz, '=');
    if (pszEqual)
    {
        /*
         * var = value
         */
        *pszEqual = '\0';
        RTStrStripR(psz);
        char *pszValue = RTStrStrip(pszEqual + 1);

        /* variables */
        static struct
        {
            const char *pszVar;
            int (*pfnHandler)(PVM pVM, char *pszVar, char *pszValue, void *pvUser);
            PFNRT pvUser;
        } aVars[] =
        {
            { "eax", scriptGPReg,  (PFNRT)CPUMSetGuestEAX },
            { "ebx", scriptGPReg,  (PFNRT)CPUMSetGuestEBX },
            { "ecx", scriptGPReg,  (PFNRT)CPUMSetGuestECX },
            { "edx", scriptGPReg,  (PFNRT)CPUMSetGuestEDX },
            { "esp", scriptGPReg,  (PFNRT)CPUMSetGuestESP },
            { "ebp", scriptGPReg,  (PFNRT)CPUMSetGuestEBP },
            { "esi", scriptGPReg,  (PFNRT)CPUMSetGuestESI },
            { "edi", scriptGPReg,  (PFNRT)CPUMSetGuestEDI },
            { "efl", scriptGPReg,  (PFNRT)CPUMSetGuestEFlags },
            { "eip", scriptGPReg,  (PFNRT)CPUMSetGuestEIP },
            { "ss",  scriptSelReg, (PFNRT)CPUMSetGuestSS },
            { "cs",  scriptSelReg, (PFNRT)CPUMSetGuestCS },
            { "ds",  scriptSelReg, (PFNRT)CPUMSetGuestDS },
            { "es",  scriptSelReg, (PFNRT)CPUMSetGuestES },
            { "fs",  scriptSelReg, (PFNRT)CPUMSetGuestFS },
            { "gs",  scriptSelReg, (PFNRT)CPUMSetGuestGS },
            { "cr0", scriptSysReg, (PFNRT)CPUMSetGuestCR0 },
            { "cr2", scriptSysReg, (PFNRT)CPUMSetGuestCR2 },
            { "cr3", scriptSysReg, (PFNRT)CPUMSetGuestCR3 },
            { "cr4", scriptSysReg, (PFNRT)CPUMSetGuestCR4 },
            { "ldtr",scriptSelReg, (PFNRT)CPUMSetGuestLDTR },
            { "tr",  scriptSelReg, (PFNRT)CPUMSetGuestTR },
            { "idtr",scriptDtrReg, (PFNRT)CPUMSetGuestIDTR },
            { "gdtr",scriptDtrReg, (PFNRT)CPUMSetGuestGDTR }
        };

        rc = -1;
        for (unsigned i = 0; i < ELEMENTS(aVars); i++)
        {
            if (!strcmp(psz, aVars[i].pszVar))
            {
                rc = aVars[i].pfnHandler(pVM, psz, pszValue, (void*)(uintptr_t)aVars[i].pvUser);
                break;
            }
        }
    }

    RTStrFree(psz);
    return rc;
}

static DECLCALLBACK(int) scriptRun(PVM pVM, RTFILE File)
{
    RTPrintf("info: running script...\n");
    uint64_t cb;
    int rc = RTFileGetSize(File, &cb);
    if (VBOX_SUCCESS(rc))
    {
        if (cb == 0)
            return VINF_SUCCESS;
        if (cb < _1M)
        {
            char *pszBuf = (char *)RTMemAllocZ(cb + 1);
            if (pszBuf)
            {
                rc = RTFileRead(File, pszBuf, cb, NULL);
                if (VBOX_SUCCESS(rc))
                {
                    pszBuf[cb] = '\0';

                    /*
                     * Now process what's in the buffer.
                     */
                    char *psz = pszBuf;
                    while (psz && *psz)
                    {
                        /* skip blanks. */
                        while (isspace(*psz))
                            psz++;
                        if (!*psz)
                            break;

                        /* end of line */
                        char *pszNext;
                        char *pszEnd = strchr(psz, '\n');
                        if (!pszEnd)
                            pszEnd = strchr(psz, '\r');
                        if (!pszEnd)
                            pszNext = pszEnd = strchr(psz, '\0');
                        else
                            pszNext = pszEnd + 1;

                        if (*psz != ';' && *psz != '#' && *psz != '/')
                        {
                            /* strip end */
                            *pszEnd = '\0';
                            while (pszEnd > psz && isspace(pszEnd[-1]))
                                *--pszEnd = '\0';

                            /* process the line */
                            RTPrintf("debug: executing script line '%s'\n",  psz);
                            rc = scriptCommand(pVM, psz, pszEnd - psz);
                            if (VBOX_FAILURE(rc))
                            {
                                RTPrintf("error: '%s' failed: %Vrc\n", psz, rc);
                                break;
                            }
                        }
                        /* else comment line */

                        /* next */
                        psz = pszNext;
                    }

                }
                else
                    RTPrintf("error: failed to read script file: %Vrc\n", rc);
                RTMemFree(pszBuf);
            }
            else
            {
                RTPrintf("error: Out of memory. (%d bytes)\n", cb + 1);
                rc = VERR_NO_MEMORY;
            }
        }
        else
            RTPrintf("error: script file is too large (0x%llx bytes)\n", cb);
    }
    else
        RTPrintf("error: couldn't get size of script file: %Vrc\n", rc);

    return rc;
}


static DECLCALLBACK(int) loadMem(PVM pVM, RTFILE File, uint64_t *poff)
{
    uint64_t off = *poff;
    RTPrintf("info: loading memory...\n");

    int rc = RTFileSeek(File, off, RTFILE_SEEK_BEGIN, NULL);
    if (VBOX_SUCCESS(rc))
    {
        RTGCPHYS GCPhys = 0;
        for (;;)
        {
            if (!(GCPhys % (PAGE_SIZE * 0x1000)))
                RTPrintf("info: %VGp...\n", GCPhys);

            /* read a page from the file */
            unsigned cbRead = 0;
            uint8_t au8Page[PAGE_SIZE * 16];
            rc = RTFileRead(File, &au8Page, sizeof(au8Page), &cbRead);
            if (VBOX_SUCCESS(rc) && !cbRead)
                rc = RTFileRead(File, &au8Page, sizeof(au8Page), &cbRead);
            if (VBOX_SUCCESS(rc) && !cbRead)
                rc = VERR_EOF;
            if (VBOX_FAILURE(rc) || rc == VINF_EOF)
            {
                if (rc == VERR_EOF)
                    rc = VINF_SUCCESS;
                else
                    RTPrintf("error: Read error %Vrc while reading the raw memory file.\n", rc);
                break;
            }

            /* Write that page to the guest - skip known rom areas for now. */
            if (GCPhys < 0xa0000 || GCPhys >= 0x10000) /* ASSUME size of a8Page is a power of 2. */
                PGMPhysWrite(pVM, GCPhys, &au8Page, cbRead);
            GCPhys += cbRead;
        }
    }
    else
        RTPrintf("error: Failed to seek to 0x%llx in the raw memory file. rc=%Vrc\n", off, rc);

    return rc;
}


/**
 * Creates the default configuration.
 * This assumes an empty tree.
 *
 * @returns VBox status code.
 * @param   pVM     VM handle.
 */
static DECLCALLBACK(int) cfgmR3CreateDefault(PVM pVM, void *pvUser)
{
    uint64_t cbMem = *(uint64_t *)pvUser;
    int rc;
    int rcAll = VINF_SUCCESS;
#define UPDATERC() do { if (VBOX_FAILURE(rc) && VBOX_SUCCESS(rcAll)) rcAll = rc; } while (0)

    /*
     * Create VM default values.
     */
    PCFGMNODE pRoot = CFGMR3GetRoot(pVM);
    rc = CFGMR3InsertString(pRoot,  "Name",                 "Default VM");
    UPDATERC();
    rc = CFGMR3InsertInteger(pRoot, "RamSize",              cbMem);
    UPDATERC();
    rc = CFGMR3InsertInteger(pRoot, "TimerMillies",         10);
    UPDATERC();
    rc = CFGMR3InsertInteger(pRoot, "RawR3Enabled",         0);
    UPDATERC();
    /** @todo CFGM Defaults: RawR0, PATMEnabled and CASMEnabled needs attention later. */
    rc = CFGMR3InsertInteger(pRoot, "RawR0Enabled",         0);
    UPDATERC();
    rc = CFGMR3InsertInteger(pRoot, "PATMEnabled",          0);
    UPDATERC();
    rc = CFGMR3InsertInteger(pRoot, "CSAMEnabled",          0);
    UPDATERC();

    /*
     * PDM.
     */
    PCFGMNODE pPdm;
    rc = CFGMR3InsertNode(pRoot, "PDM", &pPdm);
    UPDATERC();
    PCFGMNODE pDevices = NULL;
    rc = CFGMR3InsertNode(pPdm, "Devices", &pDevices);
    UPDATERC();
    rc = CFGMR3InsertInteger(pDevices, "LoadBuiltin",       1); /* boolean */
    UPDATERC();
    PCFGMNODE pDrivers = NULL;
    rc = CFGMR3InsertNode(pPdm, "Drivers", &pDrivers);
    UPDATERC();
    rc = CFGMR3InsertInteger(pDrivers, "LoadBuiltin",       1); /* boolean */
    UPDATERC();


    /*
     * Devices
     */
    pDevices = NULL;
    rc = CFGMR3InsertNode(pRoot, "Devices", &pDevices);
    UPDATERC();
    /* device */
    PCFGMNODE pDev = NULL;
    PCFGMNODE pInst = NULL;
    PCFGMNODE pCfg = NULL;
#if 0
    PCFGMNODE pLunL0 = NULL;
    PCFGMNODE pLunL1 = NULL;
#endif

    /*
     * PC Arch.
     */
    rc = CFGMR3InsertNode(pDevices, "pcarch", &pDev);
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);         /* boolean */
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();

    /*
     * PC Bios.
     */
    rc = CFGMR3InsertNode(pDevices, "pcbios", &pDev);
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);         /* boolean */
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();
    rc = CFGMR3InsertInteger(pCfg,  "RamSize",              cbMem);
    UPDATERC();
    rc = CFGMR3InsertString(pCfg,   "BootDevice0",          "IDE");
    UPDATERC();
    rc = CFGMR3InsertString(pCfg,   "BootDevice1",          "NONE");
    UPDATERC();
    rc = CFGMR3InsertString(pCfg,   "BootDevice2",          "NONE");
    UPDATERC();
    rc = CFGMR3InsertString(pCfg,   "BootDevice3",          "NONE");
    UPDATERC();
    rc = CFGMR3InsertString(pCfg,   "HardDiskDevice",       "piix3ide");
    UPDATERC();
    rc = CFGMR3InsertString(pCfg,   "FloppyDevice",         "");
    UPDATERC();
    /* Bios logo. */
    rc = CFGMR3InsertInteger(pCfg,  "FadeIn",               0);
    UPDATERC();
    rc = CFGMR3InsertInteger(pCfg,  "FadeOut",              0);
    UPDATERC();
    rc = CFGMR3InsertInteger(pCfg,  "LogoTime",             0);
    UPDATERC();
    rc = CFGMR3InsertString(pCfg,   "LogoFile",             "");
    UPDATERC();

    /*
     * PCI bus.
     */
    rc = CFGMR3InsertNode(pDevices, "pci", &pDev); /* piix3 */
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);         /* boolean */
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();

    /*
     * PS/2 keyboard & mouse
     */
    rc = CFGMR3InsertNode(pDevices, "pckbd", &pDev);
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();

    /*
     * i8254 Programmable Interval Timer And Dummy Speaker
     */
    rc = CFGMR3InsertNode(pDevices, "i8254", &pDev);
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();

    /*
     * i8259 Programmable Interrupt Controller.
     */
    rc = CFGMR3InsertNode(pDevices, "i8259", &pDev);
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);         /* boolean */
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();

    /*
     * RTC MC146818.
     */
    rc = CFGMR3InsertNode(pDevices, "mc146818", &pDev);
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();

    /*
     * VGA.
     */
    rc = CFGMR3InsertNode(pDevices, "vga", &pDev);
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);         /* boolean */
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();
    rc = CFGMR3InsertInteger(pCfg,  "VRamSize",             4 * _1M);
    UPDATERC();

    /*
     * IDE controller.
     */
    rc = CFGMR3InsertNode(pDevices, "piix3ide", &pDev); /* piix3 */
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);         /* boolean */
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();



    /*
     * ...
     */

#undef UPDATERC
    return rcAll;
}

static void syntax(void)
{
    RTPrintf("Syntax: tstAnimate <-r <raw-mem-file>> [-o <rawmem offset>] [-s <script file>] [-m <bytes>]\n"
             "\n"
             "The script is on the form:\n"
             "<reg>=<value>\n");
}


int main(int argc, char **argv)
{
    int rcRet = 1;
    RTR3Init();

    /*
     * Parse input.
     */
    if (argc <= 1)
    {
        syntax();
        return 1;
    }

    uint64_t    cbMem = ~0ULL;
    const char *pszRawMem = NULL;
    uint64_t    offRawMem = 0;
    const char *pszScript = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            /* check that it's on short form */
            if (argv[i][2])
            {
                if (    strcmp(argv[i], "--help")
                    &&  strcmp(argv[i], "-help"))
                    RTPrintf("tstAnimate: Syntax error: Unknown argument '%s'.\n", argv[i]);
                else
                    syntax();
                return 1;
            }

            /* check for 2nd argument */
            switch (argv[i][1])
            {
                case 'r':
                case 'o':
                case 'c':
                case 'm':
                    if (i + 1 < argc)
                        break;
                    RTPrintf("tstAnimate: Syntax error: '%s' takes a 2nd argument.\n", argv[i]);
                    return 1;
            }

            /* process argument */
            switch (argv[i][1])
            {
                case 'r':
                    pszRawMem = argv[++i];
                    break;

                case 'o':
                {
                    int rc = RTStrToUInt64Ex(argv[++i], NULL, 0, &offRawMem);
                    if (VBOX_FAILURE(rc))
                    {
                        RTPrintf("tstAnimate: Syntax error: Invalid offset given to -o.\n");
                        return 1;
                    }
                    break;
                }

                case 'm':
                {
                    int rc = RTStrToUInt64Ex(argv[++i], NULL, 0, &cbMem);
                    if (VBOX_FAILURE(rc))
                    {
                        RTPrintf("tstAnimate: Syntax error: Invalid offset given to -m.\n");
                        return 1;
                    }
                    break;
                }

                case 's':
                    pszScript = argv[++i];
                    break;

                case 'h':
                case 'H':
                case '?':
                    syntax();
                    return 1;

                default:
                    RTPrintf("tstAnimate: Syntax error: Unknown argument '%s'.\n", argv[i]);
                    return 1;
            }
        }
        else
        {
            RTPrintf("tstAnimate: Syntax error at arg no. %d '%s'.\n", i, argv[i]);
            syntax();
            return 1;
        }
    }

    /*
     * Check that the basic requirements are met.
     */
    if (!pszRawMem)
    {
        RTPrintf("tstAnimate: Syntax error: The -r argument is compulsory.\n");
        return 1;
    }

    /*
     * Open the files.
     */
    RTFILE FileRawMem;
    int rc = RTFileOpen(&FileRawMem, pszRawMem, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstAnimate: error: Failed to open '%s': %Vrc\n", pszRawMem, rc);
        return 1;
    }
    RTFILE FileScript = NIL_RTFILE;
    if (pszScript)
    {
        rc = RTFileOpen(&FileScript, pszScript, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
        if (VBOX_FAILURE(rc))
        {
            RTPrintf("tstAnimate: error: Failed to open '%s': %Vrc\n", pszScript, rc);
            return 1;
        }
    }

    /*
     * Figure the memsize if not specified.
     */
    if (cbMem == ~0ULL)
    {
        rc = RTFileGetSize(FileRawMem, &cbMem);
        AssertReleaseRC(rc);
        cbMem -= offRawMem;
        cbMem &= ~(PAGE_SIZE - 1);
    }
    RTPrintf("tstAnimate: info: cbMem=0x%llx bytes\n", cbMem);

    /*
     * Create empty VM.
     */
    PVM pVM;
    rc = VMR3Create(NULL, NULL, cfgmR3CreateDefault, &cbMem, &pVM);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Load memory.
         */
        PVMREQ pReq1 = NULL;
        rc = VMR3ReqCall(pVM, &pReq1, RT_INDEFINITE_WAIT, (PFNRT)loadMem, 3, pVM, FileRawMem, &offRawMem);
        AssertReleaseRC(rc);
        rc = pReq1->iStatus;
        VMR3ReqFree(pReq1);
        if (VBOX_SUCCESS(rc))
        {
            /*
             * Load register script.
             */
            if (FileScript != NIL_RTFILE)
            {
                rc = VMR3ReqCall(pVM, &pReq1, RT_INDEFINITE_WAIT, (PFNRT)scriptRun, 2, pVM, FileScript);
                AssertReleaseRC(rc);
                rc = pReq1->iStatus;
                VMR3ReqFree(pReq1);
            }
            if (VBOX_SUCCESS(rc))
            {
                /*
                 * Start the thing.
                 */
                RTPrintf("info: powering on the VM...\n");
                RTLogGroupSettings(NULL, "+REM_DISAS.e.l.f");
                rc = REMR3DisasEnableStepping(pVM, true);
                if (VBOX_SUCCESS(rc))
                {
                    DBGFR3InfoLog(pVM, "cpumguest", "verbose");
                    rc = VMR3PowerOn(pVM);
                    if (VBOX_SUCCESS(rc))
                    {
                        RTPrintf("info: VM is running\n");
                        signal(SIGINT, SigInterrupt);
                        while (!g_fSignaled)
                            RTThreadSleep(1000);
                    }
                    else
                        RTPrintf("error: Failed to power on the VM: %Vrc\n", rc);
                }
                else
                    RTPrintf("error: Failed to enabled singlestepping: %Vrc\n", rc);
                RTPrintf("info: shutting down the VM...\n");
            }
            /* execScript complains */
        }
        /* loadMem complains */
        rcRet = VBOX_SUCCESS(rc) ? 0 : 1;

        /*
         * Cleanup.
         */
        rc = VMR3Destroy(pVM);
        if (!VBOX_SUCCESS(rc))
        {
            RTPrintf("tstAnimate: error: failed to destroy vm! rc=%d\n", rc);
            rcRet++;
        }
    }
    else
    {
        RTPrintf("tstAnimate: fatal error: failed to create vm! rc=%d\n", rc);
        rcRet++;
    }

    return rcRet;
}

