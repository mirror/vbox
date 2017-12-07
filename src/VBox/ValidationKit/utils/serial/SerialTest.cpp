/* $Id$ */
/** @file
 * SerialTest - Serial port testing utility.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/err.h>
#include <iprt/getopt.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/process.h>
#include <iprt/serialport.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/test.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/



/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


/** Command line parameters */
static const RTGETOPTDEF g_aCmdOptions[] =
{
    {"--device",           'd', RTGETOPT_REQ_STRING },
    {"--baudrate",         'b', RTGETOPT_REQ_UINT32 },
    {"--parity",           'p', RTGETOPT_REQ_STRING },
    {"--databits",         'c', RTGETOPT_REQ_UINT32 },
    {"--stopbits",         's', RTGETOPT_REQ_STRING },
    {"--loopbackdevice",   'l', RTGETOPT_REQ_STRING },
    {"--help",             'h', RTGETOPT_REQ_NOTHING}
};

/** The test handle. */
static RTTEST          g_hTest;
/** The serial port handle. */
static RTSERIALPORT    g_hSerialPort = NIL_RTSERIALPORT;
/** The loopback serial port handle if configured. */
static RTSERIALPORT    g_hSerialPortLoopback = NIL_RTSERIALPORT;
/** The config used. */
static RTSERIALPORTCFG g_SerialPortCfg =
{
    /* uBaudRate */
    115200,
    /* enmParity */
    RTSERIALPORTPARITY_NONE,
    /* enmDataBitCount */
    RTSERIALPORTDATABITS_8BITS,
    /* enmStopBitCount */
    RTSERIALPORTSTOPBITS_ONE
};



/**
 * Runs the selected serial tests with the given configuration.
 *
 * @returns IPRT status code.
 */
static int serialTestRun(void)
{
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Shows tool usage text.
 */
static void serialTestUsage(PRTSTREAM pStrm)
{
    char szExec[RTPATH_MAX];
    RTStrmPrintf(pStrm, "usage: %s [options]\n",
                 RTPathFilename(RTProcGetExecutablePath(szExec, sizeof(szExec))));
    RTStrmPrintf(pStrm, "\n");
    RTStrmPrintf(pStrm, "options: \n");


    for (unsigned i = 0; i < RT_ELEMENTS(g_aCmdOptions); i++)
    {
        const char *pszHelp;
        switch (g_aCmdOptions[i].iShort)
        {
            case 'h':
                pszHelp = "Displays this help and exit";
                break;
            case 'd':
                pszHelp = "Use the specified serial port device";
                break;
            case 'b':
                pszHelp = "Use the given baudrate";
                break;
            case 'p':
                pszHelp = "Use the given parity, valid modes are: none, even, odd, mark, space";
                break;
            case 'c':
                pszHelp = "Use the given data bitcount, valid are: 5, 6, 7, 8";
                break;
            case 's':
                pszHelp = "Use the given stop bitcount, valid are: 1, 1.5, 2";
                break;
            case 'l':
                pszHelp = "Use the given serial port device as the loopback device";
                break;
            default:
                pszHelp = "Option undocumented";
                break;
        }
        char szOpt[256];
        RTStrPrintf(szOpt, sizeof(szOpt), "%s, -%c", g_aCmdOptions[i].pszLong, g_aCmdOptions[i].iShort);
        RTStrmPrintf(pStrm, "  %-30s%s\n", szOpt, pszHelp);
    }
}


int main(int argc, char *argv[])
{
    /*
     * Init IPRT and globals.
     */
    int rc = RTTestInitAndCreate("SerialTest", &g_hTest);
    if (rc)
        return rc;

    /*
     * Default values.
     */
    const char *pszDevice = NULL;
    const char *pszDeviceLoopback = NULL;

    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, g_aCmdOptions, RT_ELEMENTS(g_aCmdOptions), 1, 0 /* fFlags */);
    while ((rc = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (rc)
        {
            case 'h':
                serialTestUsage(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case 'd':
                pszDevice = ValueUnion.psz;
                break;
            case 'l':
                pszDeviceLoopback = ValueUnion.psz;
                break;
            case 'b':
                g_SerialPortCfg.uBaudRate = ValueUnion.u32;
                break;
            case 'p':
                if (!RTStrICmp(ValueUnion.psz, "none"))
                    g_SerialPortCfg.enmParity = RTSERIALPORTPARITY_NONE;
                else if (!RTStrICmp(ValueUnion.psz, "even"))
                    g_SerialPortCfg.enmParity = RTSERIALPORTPARITY_EVEN;
                else if (!RTStrICmp(ValueUnion.psz, "odd"))
                    g_SerialPortCfg.enmParity = RTSERIALPORTPARITY_ODD;
                else if (!RTStrICmp(ValueUnion.psz, "mark"))
                    g_SerialPortCfg.enmParity = RTSERIALPORTPARITY_MARK;
                else if (!RTStrICmp(ValueUnion.psz, "space"))
                    g_SerialPortCfg.enmParity = RTSERIALPORTPARITY_SPACE;
                else
                {
                    RTPrintf("Unknown parity \"%s\" given\n", ValueUnion.psz);
                    return RTEXITCODE_FAILURE;
                }
                break;
            case 'c':
                if (!RTStrICmp(ValueUnion.psz, "5"))
                    g_SerialPortCfg.enmDataBitCount = RTSERIALPORTDATABITS_5BITS;
                else if (!RTStrICmp(ValueUnion.psz, "6"))
                    g_SerialPortCfg.enmDataBitCount = RTSERIALPORTDATABITS_6BITS;
                else if (!RTStrICmp(ValueUnion.psz, "7"))
                    g_SerialPortCfg.enmDataBitCount = RTSERIALPORTDATABITS_7BITS;
                else if (!RTStrICmp(ValueUnion.psz, "8"))
                    g_SerialPortCfg.enmDataBitCount = RTSERIALPORTDATABITS_8BITS;
                else
                {
                    RTPrintf("Unknown data bitcount \"%s\" given\n", ValueUnion.psz);
                    return RTEXITCODE_FAILURE;
                }
                break;
            case 's':
                if (!RTStrICmp(ValueUnion.psz, "1"))
                    g_SerialPortCfg.enmStopBitCount = RTSERIALPORTSTOPBITS_ONE;
                else if (!RTStrICmp(ValueUnion.psz, "1.5"))
                    g_SerialPortCfg.enmStopBitCount = RTSERIALPORTSTOPBITS_ONEPOINTFIVE;
                else if (!RTStrICmp(ValueUnion.psz, "2"))
                    g_SerialPortCfg.enmStopBitCount = RTSERIALPORTSTOPBITS_TWO;
                else
                {
                    RTPrintf("Unknown stop bitcount \"%s\" given\n", ValueUnion.psz);
                    return RTEXITCODE_FAILURE;
                }
                break;
            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    /*
     * Start testing.
     */
    RTTestBanner(g_hTest);

    if (pszDevice)
    {
        uint32_t fFlags =   RTSERIALPORT_OPEN_F_READ
                          | RTSERIALPORT_OPEN_F_WRITE
                          | RTSERIALPORT_OPEN_F_SUPPORT_STATUS_LINE_MONITORING;

        RTTestSub(g_hTest, "Opening device");
        rc = RTSerialPortOpen(&g_hSerialPort, pszDevice, fFlags);
        if (RT_SUCCESS(rc))
        {
            if (pszDeviceLoopback)
            {
                RTTestSub(g_hTest, "Opening loopback device");
                rc = RTSerialPortOpen(&g_hSerialPortLoopback, pszDeviceLoopback, fFlags);
                if (RT_FAILURE(rc))
                    RTTestFailed(g_hTest, "Opening loopback device \"%s\" failed with %Rrc\n", pszDevice, rc);
            }

            if (RT_SUCCESS(rc))
            {
                RTTestSub(g_hTest, "Setting serial port configuration");

                rc = RTSerialPortCfgSet(g_hSerialPort, &g_SerialPortCfg ,NULL);
                if (RT_SUCCESS(rc))
                {
                    if (pszDeviceLoopback)
                    {
                        RTTestSub(g_hTest, "Setting serial port configuration for loopback device");
                        rc = RTSerialPortCfgSet(g_hSerialPortLoopback,&g_SerialPortCfg ,NULL);
                        if (RT_FAILURE(rc))
                            RTTestFailed(g_hTest, "Setting configuration of loopback device \"%s\" failed with %Rrc\n", pszDevice, rc);
                    }

                    if (RT_SUCCESS(rc))
                        rc = serialTestRun();
                }
                else
                    RTTestFailed(g_hTest, "Setting configuration of device \"%s\" failed with %Rrc\n", pszDevice, rc);

                RTSerialPortClose(g_hSerialPort);
            }
        }
        else
            RTTestFailed(g_hTest, "Opening device \"%s\" failed with %Rrc\n", pszDevice, rc);
    }
    else
        RTTestFailed(g_hTest, "No device given on command line\n");

    RTEXITCODE rcExit = RTTestSummaryAndDestroy(g_hTest);
    return rcExit;
}

