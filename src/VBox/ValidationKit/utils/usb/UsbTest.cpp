/* $Id$ */
/** @file
 * UsbTest - User frontend for the Linux usbtest USB test and benchmarking module.
 *           Integrates with our test framework for nice outputs.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/err.h>
#include <iprt/getopt.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/test.h>
#include <iprt/file.h>

#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/** Number of tests implemented at the moment. */
#define USBTEST_TEST_CASES      25

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * USB test request data.
 * There is no public header with this information so we define it ourself here.
 */
typedef struct USBTESTPARMS
{
    /** Specifies the test to run. */
    uint32_t        idxTest;
    /** How many iterations the test should be executed. */
    uint32_t        cIterations;
    /** Size of the data packets. */
    uint32_t        cbData;
    /** Size of  */
    uint32_t        cbVariation;
    /** Length of the S/G list for the test. */
    uint32_t        cSgLength;
    /** Returned time data after completing the test. */
    struct timeval  TimeTest;
} USBTESTPARAMS;
/** Pointer to a test parameter structure. */
typedef USBTESTPARAMS *PUSBTESTPARAMS;

/**
 * USB device descriptor. Used to search for the test device based
 * on the vendor and product id.
 */
#pragma pack(1)
typedef struct USBDEVDESC
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} USBDEVDESC;
#pragma pack()

#define USBTEST_REQUEST _IOWR('U', 100, USBTESTPARMS)

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/

/** Command line parameters */
static const RTGETOPTDEF g_aCmdOptions[] =
{
    {"--device",           'd', RTGETOPT_REQ_STRING },
    {"--help",             'h', RTGETOPT_REQ_NOTHING}
};

/** (Sort of) Descriptive test descriptions. */
static const char *g_apszTests[] =
{
    "NOP",
    "Non-queued Bulk write",
    "Non-queued Bulk read",
    "Non-queued Bulk write variabe size",
    "Non-queued Bulk read variabe size",
    "Queued Bulk write",
    "Queued Bulk read",
    "Queued Bulk write variabe size",
    "Queued Bulk read variabe size",
    "Chapter 9 Control Test",
    "Queued control messaging",
    "Unlink reads",
    "Unlink writes",
    "Set/Clear halts",
    "Control writes",
    "Isochronous write",
    "Isochronous read",
    "Bulk write unaligned (DMA)",
    "Bulk read unaligned (DMA)",
    "Bulk write unaligned (no DMA)",
    "Bulk read unaligned (no DMA)",
    "Control writes unaligned",
    "Isochronous write unaligned",
    "Isochronous read unaligned",
    "Unlink queued Bulk"
};
AssertCompile(RT_ELEMENTS(g_apszTests) == USBTEST_TEST_CASES);

/** The test handle. */
static RTTEST g_hTest;


static void usbTestUsage(PRTSTREAM pStrm)
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
                pszHelp = "Use the specified test device";
                break;
            default:
                pszHelp = "Option undocumented";
                break;
        }
        char szOpt[256];
        RTStrPrintf(szOpt, sizeof(szOpt), "%s, -%c", g_aCmdOptions[i].pszLong, g_aCmdOptions[i].iShort);
        RTStrmPrintf(pStrm, "  %-20s%s\n", szOpt, pszHelp);
    }
}

/**
 * Search for a USB test device and return the device path.
 *
 * @returns Path to the USB test device or NULL if none was found.
 */
static char *usbTestFindDevice(void)
{
    /*
     * Very crude and quick way to search for the correct test device.
     * Assumption is that the path looks like /dev/bus/usb/%3d/%3d.
     */
    uint8_t uBus = 1;
    bool fBusExists = false;
    char aszDevPath[64];

    RT_ZERO(aszDevPath);

    do
    {
        RTStrPrintf(aszDevPath, sizeof(aszDevPath), "/dev/bus/usb/%03d", uBus);

        fBusExists = RTPathExists(aszDevPath);

        if (fBusExists)
        {
            /* Check every device. */
            bool fDevExists = false;
            uint8_t uDev = 1;

            do
            {
                RTStrPrintf(aszDevPath, sizeof(aszDevPath), "/dev/bus/usb/%03d/%03d", uBus, uDev);

                fDevExists = RTPathExists(aszDevPath);

                if (fDevExists)
                {
                    RTFILE hFileDev;
                    int rc = RTFileOpen(&hFileDev, aszDevPath, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE);
                    if (RT_SUCCESS(rc))
                    {
                        USBDEVDESC DevDesc;

                        rc = RTFileRead(hFileDev, &DevDesc, sizeof(DevDesc), NULL);
                        RTFileClose(hFileDev);

                        if (   RT_SUCCESS(rc)
                            && DevDesc.idVendor == 0x0525
                            && DevDesc.idProduct == 0xa4a0)
                            return RTStrDup(aszDevPath);
                    }
                }

                uDev++;
            } while (fDevExists);
        }

        uBus++;
    } while (fBusExists);

    return NULL;
}

static int usbTestIoctl(int iDevFd, int iInterface, PUSBTESTPARAMS pParams)
{
    struct usbdevfs_ioctl IoCtlData;

    IoCtlData.ifno = iInterface;
    IoCtlData.ioctl_code = (int)USBTEST_REQUEST;
    IoCtlData.data = pParams;
    return ioctl(iDevFd, USBDEVFS_IOCTL, &IoCtlData);
}

/**
 * Test execution worker.
 *
 * @returns nothing.
 * @param   pszDevice    The device to use for testing.
 */
static void usbTestExec(const char *pszDevice)
{
    int iDevFd;

    RTTestSub(g_hTest, "Opening device");
    iDevFd = open(pszDevice, O_RDWR);
    if (iDevFd != -1)
    {
        USBTESTPARAMS Params;

        RTTestPassed(g_hTest, "Opening device successful\n");

        /*
         * Fill params with some defaults.
         * @todo: Make them configurable.
         */
        Params.cIterations = 1000;
        Params.cbData = 512;
        Params.cbVariation = 512;
        Params.cSgLength = 32;

        for (unsigned i = 0; i < USBTEST_TEST_CASES; i++)
        {
            RTTestSub(g_hTest, g_apszTests[i]);

            Params.idxTest = i;

            /* Assume the test interface has the number 0 for now. */
            int rcPosix = usbTestIoctl(iDevFd, 0, &Params);
            if (rcPosix < 0 && errno == EOPNOTSUPP)
            {
                RTTestSkipped(g_hTest, "Not supported");
                continue;
            }

            if (rcPosix < 0)
                RTTestFailed(g_hTest, "Test failed with %Rrc\n", RTErrConvertFromErrno(errno));
            else
            {
                uint64_t u64Ns = Params.TimeTest.tv_sec * RT_NS_1SEC + Params.TimeTest.tv_usec * RT_NS_1US;
                RTTestValue(g_hTest, "Runtime", u64Ns, RTTESTUNIT_NS);
            }
            RTTestSubDone(g_hTest);
        }

        close(iDevFd);
    }
    else
        RTTestFailed(g_hTest, "Opening device failed with %Rrc\n", RTErrConvertFromErrno(errno));

}

int main(int argc, char *argv[])
{
    /*
     * Init IPRT and globals.
     */
    int rc = RTTestInitAndCreate("UsbTest", &g_hTest);
    if (rc)
        return rc;

    /*
     * Default values.
     */
    const char *pszDevice = NULL;

    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, g_aCmdOptions, RT_ELEMENTS(g_aCmdOptions), 1, 0 /* fFlags */);
    while ((rc = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (rc)
        {
            case 'h':
                usbTestUsage(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case 'd':
                pszDevice = ValueUnion.psz;
                break;
            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    /*
     * Start testing.
     */
    RTTestBanner(g_hTest);

    /* Find the first test device if none was given. */
    if (!pszDevice)
        pszDevice = usbTestFindDevice();

    if (pszDevice)
        usbTestExec(pszDevice);
    else
    {
        RTTestPrintf(g_hTest, RTTESTLVL_FAILURE, "Failed to find a test device\n");
        RTTestErrorInc(g_hTest);
    }

    RTEXITCODE rcExit = RTTestSummaryAndDestroy(g_hTest);
    return rcExit;
}

