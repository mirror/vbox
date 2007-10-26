/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Implementation of USBProxyServiceLinux class
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "USBProxyService.h"
#include "Logging.h"

#include <VBox/usb.h>
#include <VBox/err.h>

#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/err.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/statfs.h>
#include <sys/poll.h>
#include <unistd.h>
#ifndef VBOX_WITHOUT_LINUX_COMPILER_H
# include <linux/compiler.h>
#endif
#include <linux/usbdevice_fs.h>



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Suffix translation. */
typedef struct USBSUFF
{
    char        szSuff[4];
    unsigned    cchSuff;
    unsigned    uMul;
    unsigned    uDiv;
} USBSUFF, *PUSBSUFF;
typedef const USBSUFF *PCUSBSUFF;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Suffixes for the endpoint polling interval.
 */
static const USBSUFF s_aIntervalSuff[] =
{
    { "ms", 2,    1,       0 },
    { "us", 2,    1,    1000 },
    { "ns", 2,    1, 1000000 },
    { "s",  1, 1000,       0 },
    { "",   0,    0,       0 }  /* term */
};


/**
 * Initialize data members.
 */
USBProxyServiceLinux::USBProxyServiceLinux (HostUSB *aHost, const char *aUsbfsRoot /* = "/proc/bus/usb" */)
    : USBProxyService (aHost), mFile (NIL_RTFILE), mStream (NULL), mWakeupPipeR (NIL_RTFILE),
      mWakeupPipeW (NIL_RTFILE), mUsbfsRoot (aUsbfsRoot)
{
    LogFlowMember (("USBProxyServiceLinux::USBProxyServiceLinux: aHost=%p aUsbfsRoot=%p:{%s}\n", aHost, aUsbfsRoot, aUsbfsRoot));

    /*
     * Open the devices file.
     */
    int rc = VERR_NO_MEMORY;
    char *pszDevices;
    RTStrAPrintf (&pszDevices, "%s/devices", aUsbfsRoot);
    if (pszDevices)
    {
        rc = RTFileOpen (&mFile, pszDevices, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
        if (VBOX_SUCCESS (rc))
        {
            /*
             * Check that we're actually on the usbfs.
             */
            struct statfs StFS;
            if (!fstatfs (mFile, &StFS))
            {
                if (StFS.f_type == USBDEVICE_SUPER_MAGIC)
                {
                    int pipes[2];
                    if (!pipe (pipes))
                    {
                        mWakeupPipeR = pipes[0];
                        mWakeupPipeW = pipes[1];
                        mStream = fdopen (mFile, "r");
                        if (mStream)
                        {
                            /*
                             * Start the poller thread.
                             */
                            rc = start();
                            if (VBOX_SUCCESS (rc))
                            {
                                RTStrFree (pszDevices);
                                LogFlowMember (("USBProxyServiceLinux::USBProxyServiceLinux: returns successfully - mFile=%d mStream=%p mWakeupPipeR/W=%d/%d\n",
                                                mFile, mStream, mWakeupPipeR, mWakeupPipeW));
                                return;
                            }

                            fclose (mStream);
                            mStream = NULL;
                            mFile = NIL_RTFILE;
                        }

                        RTFileClose (mWakeupPipeR);
                        RTFileClose (mWakeupPipeW);
                        mWakeupPipeW = mWakeupPipeR = NIL_RTFILE;
                    }
                }
                else
                {
                    Log (("USBProxyServiceLinux::USBProxyServiceLinux: StFS.f_type=%d expected=%d\n", StFS.f_type, USBDEVICE_SUPER_MAGIC));
                    rc = VERR_INVALID_PARAMETER;
                }
            }
            else
            {
                rc = RTErrConvertFromErrno (errno);
                Log (("USBProxyServiceLinux::USBProxyServiceLinux: fstatfs failed, errno=%d\n", errno));
            }
            RTFileClose (mFile);
            mFile = NIL_RTFILE;
        }
        else
        {
#ifndef DEBUG_fm3
            /* I'm currently using Linux with disabled USB support */
            AssertRC (rc);
#endif
            Log (("USBProxyServiceLinux::USBProxyServiceLinux: RTFileOpen(,%s,,) -> %Vrc\n", pszDevices, rc));
        }
        RTStrFree (pszDevices);
    }
    else
        Log (("USBProxyServiceLinux::USBProxyServiceLinux: out of memory!\n"));

    mLastError = rc;
    LogFlowMember (("USBProxyServiceLinux::USBProxyServiceLinux: returns failure!!! (rc=%Vrc)\n", rc));
}


/**
 * Stop all service threads and free the device chain.
 */
USBProxyServiceLinux::~USBProxyServiceLinux()
{
    LogFlowMember (("USBProxyServiceLinux::~USBProxyServiceLinux:\n"));

    /*
     * Stop the service.
     */
    if (isActive())
        stop();

    /*
     * Free resources.
     */
    if (mStream)
    {
        fclose (mStream);
        mStream = NULL;
        mFile = NIL_RTFILE;
    }
    else if (mFile != NIL_RTFILE)
    {
        RTFileClose (mFile);
        mFile = NIL_RTFILE;
    }

    RTFileClose (mWakeupPipeR);
    RTFileClose (mWakeupPipeW);
    mWakeupPipeW = mWakeupPipeR = NIL_RTFILE;
}


int USBProxyServiceLinux::captureDevice (HostUSBDevice *aDevice)
{
    /*
     * Don't think we need to do anything when the device is held...
     */
    return VINF_SUCCESS;
}


int USBProxyServiceLinux::holdDevice (HostUSBDevice *pDevice)
{
    /*
     * This isn't really implemented, we can usually wrestle
     * any user when we need it... Anyway, I don't have anywhere to store
     * any info per device atm.
     */
    return VINF_SUCCESS;
}


int USBProxyServiceLinux::releaseDevice (HostUSBDevice *aDevice)
{
    /*
     * We're not really holding it atm.
     */
    return VINF_SUCCESS;
}


int USBProxyServiceLinux::resetDevice (HostUSBDevice *aDevice)
{
    /*
     * We don't dare reset anything, but the USB Proxy Device
     * will reset upon detach, so this should be ok.
     */
    return VINF_SUCCESS;
}


int USBProxyServiceLinux::wait (unsigned aMillies)
{
    struct pollfd PollFds[2];

    memset(&PollFds, 0, sizeof(PollFds));
    PollFds[0].fd        = mFile;
    PollFds[0].events    = POLLIN;
    PollFds[1].fd        = mWakeupPipeR;
    PollFds[1].events    = POLLIN | POLLERR | POLLHUP;

    int rc = poll (&PollFds[0], 2, aMillies);
    if (rc == 0)
        return VERR_TIMEOUT;
    if (rc > 0)
        return VINF_SUCCESS;
    return RTErrConvertFromErrno (errno);
}


int USBProxyServiceLinux::interruptWait (void)
{
    int rc = RTFileWrite (mWakeupPipeW, "Wakeup!", sizeof("Wakeup!") - 1, NULL);
    if (VBOX_SUCCESS (rc))
        fsync (mWakeupPipeW);
    return rc;
}


/**
 * "reads" the number suffix. It's more like validating it and
 * skipping the necessary number of chars.
 */
static int usbReadSkipSuffix (char **ppszNext)
{
    char *pszNext = *ppszNext;
    if (!isspace (*pszNext) && *pszNext)
    {
        /* skip unit */
        if (pszNext[0] == 'm' && pszNext[1] == 's')
            pszNext += 2;
        else if (pszNext[0] == 'm' && pszNext[1] == 'A')
            pszNext += 2;

        /* skip parenthesis */
        if (*pszNext == '(')
        {
            pszNext = strchr (pszNext, ')');
            if (!pszNext++)
            {
                AssertMsgFailed (("*ppszNext=%s\n", *ppszNext));
                return VERR_PARSE_ERROR;
            }
        }

        /* blank or end of the line. */
        if (!isspace (*pszNext) && *pszNext)
        {
            AssertMsgFailed (("pszNext=%s\n", pszNext));
            return VERR_PARSE_ERROR;
        }

        /* it's ok. */
        *ppszNext = pszNext;
    }

    return VINF_SUCCESS;
}


/**
 * Reads a USB number returning the number and the position of the next character to parse.
 */
static int usbReadNum (const char *pszValue, unsigned uBase, uint32_t u32Mask, PCUSBSUFF paSuffs, void *pvNum, char **ppszNext)
{
    /*
     * Initialize return value to zero and strip leading spaces.
     */
    switch (u32Mask)
    {
        case 0xff: *(uint8_t *)pvNum = 0; break;
        case 0xffff: *(uint16_t *)pvNum = 0; break;
        case 0xffffffff: *(uint32_t *)pvNum = 0; break;
    }
    pszValue = RTStrStripL (pszValue);
    if (*pszValue)
    {
        /*
         * Try convert the number.
         */
        char *pszNext;
        uint32_t u32 = 0;
        RTStrToUInt32Ex (pszValue, &pszNext, uBase, &u32);
        if (pszNext == pszValue)
        {
            AssertMsgFailed (("pszValue=%d\n", pszValue));
            return VERR_NO_DATA;
        }

        /*
         * Check the range.
         */
        if (u32 & ~u32Mask)
        {
            AssertMsgFailed (("pszValue=%d u32=%#x lMask=%#x\n", pszValue, u32, u32Mask));
            return VERR_OUT_OF_RANGE;
        }

        /*
         * Validate and skip stuff following the number.
         */
        if (paSuffs)
        {
            if (!isspace (*pszNext) && *pszNext)
            {
                for (PCUSBSUFF pSuff = paSuffs; pSuff->szSuff[0]; pSuff++)
                {
                    if (    !strncmp (pSuff->szSuff, pszNext, pSuff->cchSuff)
                        &&  (!pszNext[pSuff->cchSuff] || isspace (pszNext[pSuff->cchSuff])))
                    {
                        if (pSuff->uDiv)
                            u32 /= pSuff->uDiv;
                        else
                            u32 *= pSuff->uMul;
                        break;
                    }
                }
            }
        }
        else
        {
            int rc = usbReadSkipSuffix (&pszNext);
            if (VBOX_FAILURE (rc))
                return rc;
        }

        *ppszNext = pszNext;

        /*
         * Set the value.
         */
        switch (u32Mask)
        {
            case 0xff: *(uint8_t *)pvNum = (uint8_t)u32; break;
            case 0xffff: *(uint16_t *)pvNum = (uint16_t)u32; break;
            case 0xffffffff: *(uint32_t *)pvNum = (uint32_t)u32; break;
        }
    }
    return VINF_SUCCESS;
}

static int usbRead8 (const char *pszValue, unsigned uBase, uint8_t *pu8, char **ppszNext)
{
    return usbReadNum (pszValue, uBase, 0xff, NULL, pu8, ppszNext);
}

static int usbRead16 (const char *pszValue, unsigned uBase, uint16_t *pu16, char **ppszNext)
{
    return usbReadNum (pszValue, uBase, 0xffff, NULL, pu16, ppszNext);
}

static int usbRead16Suff (const char *pszValue, unsigned uBase, PCUSBSUFF paSuffs, uint16_t *pu16,  char **ppszNext)
{
    return usbReadNum (pszValue, uBase, 0xffff, paSuffs, pu16, ppszNext);
}

/**
 * Reads a USB BCD number returning the number and the position of the next character to parse.
 * The returned number contains the integer part in the high byte and the decimal part in the low byte.
 */
static int usbReadBCD (const char *pszValue, unsigned uBase, uint16_t *pu16, char **ppszNext)
{
    /*
     * Initialize return value to zero and strip leading spaces.
     */
    *pu16 = 0;
    pszValue = RTStrStripL (pszValue);
    if (*pszValue)
    {
        /*
         * Try convert the number.
         */
        /* integer part */
        char *pszNext;
        uint32_t u32Int = 0;
        RTStrToUInt32Ex (pszValue, &pszNext, uBase, &u32Int);
        if (pszNext == pszValue)
        {
            AssertMsgFailed (("pszValue=%s\n", pszValue));
            return VERR_NO_DATA;
        }
        if (u32Int & ~0xff)
        {
            AssertMsgFailed (("pszValue=%s u32Int=%#x (int)\n", pszValue, u32Int));
            return VERR_OUT_OF_RANGE;
        }

        /* skip dot and read decimal part */
        if (*pszNext != '.')
        {
            AssertMsgFailed (("pszValue=%s pszNext=%s (int)\n", pszValue, pszNext));
            return VERR_PARSE_ERROR;
        }
        char *pszValue2 = RTStrStripL (pszNext + 1);
        uint32_t u32Dec = 0;
        RTStrToUInt32Ex (pszValue2, &pszNext, uBase, &u32Dec);
        if (pszNext == pszValue)
        {
            AssertMsgFailed (("pszValue=%s\n", pszValue));
            return VERR_NO_DATA;
        }
        if (u32Dec & ~0xff)
        {
            AssertMsgFailed (("pszValue=%s u32Dec=%#x\n", pszValue, u32Dec));
            return VERR_OUT_OF_RANGE;
        }

        /*
         * Validate and skip stuff following the number.
         */
        int rc = usbReadSkipSuffix (&pszNext);
        if (VBOX_FAILURE (rc))
            return rc;
        *ppszNext = pszNext;

        /*
         * Set the value.
         */
        *pu16 = (uint16_t)u32Int << 8 | (uint16_t)u32Dec;
    }
    return VINF_SUCCESS;
}


/**
 * Reads a string, i.e. allocates memory and copies it.
 *
 * We assume that a string is pure ASCII, if that's not the case
 * tell me how to figure out the codeset please.
 */
static int usbReadStr (const char *pszValue, const char **ppsz)
{
    if (*ppsz)
        RTStrFree ((char *)*ppsz);
    *ppsz = RTStrDup (pszValue);
    if (*ppsz)
        return VINF_SUCCESS;
    return VERR_NO_MEMORY;
}


/**
 * Skips the current property.
 */
static char * usbReadSkip (const char *pszValue)
{
    char *psz = strchr (pszValue, '=');
    if (psz)
        psz = strchr (psz + 1, '=');
    if (!psz)
        return strchr (pszValue,  '\0');
    while (psz > pszValue && !isspace (psz[-1]))
        psz--;
    Assert (psz > pszValue);
    return psz;
}


/**
 * Compare a prefix and returns pointer to the char following it if it matches.
 */
static char *usbPrefix (char *psz, const char *pszPref, size_t cchPref)
{
    if (strncmp (psz, pszPref, cchPref))
        return NULL;
    return psz + cchPref;
}


/**
 * Checks which state the device is in.
 */
static USBDEVICESTATE usbDeterminState (PCUSBDEVICE pDevice)
{
    if (!pDevice->idVendor)
        return USBDEVICESTATE_UNSUPPORTED;

    /*
     * We cannot distinguish between USED_BY_HOST_CAPTURABLE and
     * USED_BY_GUEST, HELD_BY_PROXY all that well and it shouldn't be
     * necessary either.
     */
    USBDEVICESTATE  enmState = USBDEVICESTATE_UNUSED;
    for (int iCfg = pDevice->bNumConfigurations - 1; iCfg >= 0; iCfg--)
        for (int iIf = pDevice->paConfigurations[iCfg].bConfigurationValue - 1; iIf >= 0; iIf--)
        {
            const char *pszDriver = pDevice->paConfigurations[iCfg].paInterfaces[iIf].pszDriver;
            if (pszDriver)
            {
                if (!strcmp (pszDriver, "hub"))
                {
                    enmState = USBDEVICESTATE_USED_BY_HOST;
                    break;
                }
                enmState = USBDEVICESTATE_USED_BY_HOST_CAPTURABLE;
            }
        }

    return enmState;
}


PUSBDEVICE USBProxyServiceLinux::getDevices (void)
{
    PUSBDEVICE      pFirst = NULL;
    if (mStream)
    {
        PUSBDEVICE     *ppNext = NULL;
        USBDEVICE       Dev = {0};
        int             cHits = 0;
        int             iCfg = 0;
        PUSBCONFIG      pCfg = NULL;
        PUSBINTERFACE   pIf = NULL;
        int             iEp = 0;
        PUSBENDPOINT    pEp = NULL;
        char            szLine[1024];

        rewind (mStream);
        int rc = VINF_SUCCESS;
        while (     VBOX_SUCCESS (rc)
               &&   fgets (szLine, sizeof (szLine), mStream))
        {
            char   *psz;
            char   *pszValue;

            /* validate and remove the trailing newline. */
            psz = strchr (szLine, '\0');
            if (psz[-1] != '\n' && !feof (mStream))
            {
                AssertMsgFailed (("Line too long. (cch=%d)\n", strlen (szLine)));
                continue;
            }

            /* strip */
            psz = RTStrStrip (szLine);
            if (!*psz)
                continue;

            /*
             * Interpret the line.
             * (Ordered by normal occurence.)
             */
            char ch = psz[0];
            if (psz[1] != ':')
                continue;
            psz = RTStrStripL (psz + 3);
            #define PREFIX(str) ( (pszValue = usbPrefix (psz, str, sizeof (str) - 1)) != NULL )
            switch (ch)
            {
                /*
                 * T:  Bus=dd Lev=dd Prnt=dd Port=dd Cnt=dd Dev#=ddd Spd=ddd MxCh=dd
                 * |   |      |      |       |       |      |        |       |__MaxChildren
                 * |   |      |      |       |       |      |        |__Device Speed in Mbps
                 * |   |      |      |       |       |      |__DeviceNumber
                 * |   |      |      |       |       |__Count of devices at this level
                 * |   |      |      |       |__Connector/Port on Parent for this device
                 * |   |      |      |__Parent DeviceNumber
                 * |   |      |__Level in topology for this bus
                 * |   |__Bus number
                 * |__Topology info tag
                 */
                case 'T':
                    /* add */
                    AssertMsg (cHits >= 3 || cHits == 0, ("cHits=%d\n", cHits));
                    if (cHits >= 3)
                    {
                        Dev.enmState = usbDeterminState (&Dev);
                        PUSBDEVICE pDev = (PUSBDEVICE) RTMemAlloc (sizeof(*pDev));
                        if (pDev)
                        {
                            *pDev = Dev;
                            if (Dev.enmState != USBDEVICESTATE_UNSUPPORTED)
                            {
                                RTStrAPrintf((char **)&pDev->pszAddress, "%s/%03d/%03d", mUsbfsRoot.c_str(), pDev->bBus, pDev->bDevNum);
                                if (pDev->pszAddress)
                                {
                                    if (ppNext)
                                        *ppNext = pDev;
                                    else
                                        pFirst = pDev;
                                    ppNext = &pDev->pNext;
                                }
                                else
                                {
                                    freeDevice (pDev);
                                    rc = VERR_NO_MEMORY;
                                }
                            }
                            else
                                freeDevice (pDev);
                            memset (&Dev, 0, sizeof (Dev));
                        }
                        else
                            rc = VERR_NO_MEMORY;
                    }

                    /* Reset device state */
                    cHits = 1;
                    iCfg = 0;
                    pCfg = NULL;
                    pIf = NULL;
                    iEp = 0;
                    pEp = NULL;


                    /* parse the line. */
                    while (*psz && VBOX_SUCCESS (rc))
                    {
                        if (PREFIX ("Bus="))
                            rc = usbRead8 (pszValue, 10, &Dev.bBus, &psz);
                        else if (PREFIX ("Lev="))
                            rc = usbRead8 (pszValue, 10, &Dev.bLevel, &psz);
                        else if (PREFIX ("Dev#="))
                            rc = usbRead8 (pszValue, 10, &Dev.bDevNum, &psz);
                        else if (PREFIX ("Prnt="))
                            rc = usbRead8 (pszValue, 10, &Dev.bDevNumParent, &psz);
                        else if (PREFIX ("Port="))
                            rc = usbRead8 (pszValue, 10, &Dev.bPort, &psz);
                        else if (PREFIX ("Cnt="))
                            rc = usbRead8 (pszValue, 10, &Dev.bNumDevices, &psz);
                        //else if (PREFIX ("Spd="))
                        //    rc = usbReadSpeed (pszValue, &Dev.cbSpeed, &psz);
                        else if (PREFIX ("MxCh="))
                            rc = usbRead8 (pszValue, 10, &Dev.bMaxChildren, &psz);
                        else
                            psz = usbReadSkip (psz);
                        psz = RTStrStripL (psz);
                    }
                    break;

                /*
                 * Bandwidth info:
                 * B:  Alloc=ddd/ddd us (xx%), #Int=ddd, #Iso=ddd
                 * |   |                       |         |__Number of isochronous requests
                 * |   |                       |__Number of interrupt requests
                 * |   |__Total Bandwidth allocated to this bus
                 * |__Bandwidth info tag
                 */
                case 'B':
                    break;

                /*
                 * D:  Ver=x.xx Cls=xx(sssss) Sub=xx Prot=xx MxPS=dd #Cfgs=dd
                 * |   |        |             |      |       |       |__NumberConfigurations
                 * |   |        |             |      |       |__MaxPacketSize of Default Endpoint
                 * |   |        |             |      |__DeviceProtocol
                 * |   |        |             |__DeviceSubClass
                 * |   |        |__DeviceClass
                 * |   |__Device USB version
                 * |__Device info tag #1
                 */
                case 'D':
                    while (*psz && VBOX_SUCCESS (rc))
                    {
                        if (PREFIX ("Ver="))
                            rc = usbReadBCD (pszValue, 16, &Dev.bcdUSB, &psz);
                        else if (PREFIX ("Cls="))
                            rc = usbRead8 (pszValue, 16, &Dev.bDeviceClass, &psz);
                        else if (PREFIX ("Sub="))
                            rc = usbRead8 (pszValue, 16, &Dev.bDeviceSubClass, &psz);
                        else if (PREFIX ("Prot="))
                            rc = usbRead8 (pszValue, 16, &Dev.bDeviceProtocol, &psz);
                        //else if (PREFIX ("MxPS="))
                        //    rc = usbRead16 (pszValue, 10, &Dev.wMaxPacketSize, &psz);
                        else if (PREFIX ("#Cfgs="))
                            rc = usbRead8 (pszValue, 10, &Dev.bNumConfigurations, &psz);
                        else
                            psz = usbReadSkip (psz);
                        psz = RTStrStripL (psz);
                    }
                    cHits++;
                    break;

                /*
                 * P:  Vendor=xxxx ProdID=xxxx Rev=xx.xx
                 * |   |           |           |__Product revision number
                 * |   |           |__Product ID code
                 * |   |__Vendor ID code
                 * |__Device info tag #2
                 */
                case 'P':
                    while (*psz && VBOX_SUCCESS (rc))
                    {
                        if (PREFIX ("Vendor="))
                            rc = usbRead16 (pszValue, 16, &Dev.idVendor, &psz);
                        else if (PREFIX ("ProdID="))
                            rc = usbRead16 (pszValue, 16, &Dev.idProduct, &psz);
                        else if (PREFIX ("Rev="))
                            rc = usbReadBCD (pszValue, 16, &Dev.bcdDevice, &psz);
                        else
                            psz = usbReadSkip (psz);
                        psz = RTStrStripL (psz);
                    }
                    cHits++;
                    break;

                /*
                 * String.
                 */
                case 'S':
                    if (PREFIX ("Manufacturer="))
                        rc = usbReadStr (pszValue, &Dev.pszManufacturer);
                    else if (PREFIX ("Product="))
                        rc = usbReadStr (pszValue, &Dev.pszProduct);
                    else if (PREFIX ("SerialNumber="))
                    {
                        rc = usbReadStr (pszValue, &Dev.pszSerialNumber);
                        if (VBOX_SUCCESS (rc))
                            Dev.u64SerialHash = calcSerialHash (pszValue);
                    }
                    break;

                /*
                 * C:* #Ifs=dd Cfg#=dd Atr=xx MPwr=dddmA
                 * | | |       |       |      |__MaxPower in mA
                 * | | |       |       |__Attributes
                 * | | |       |__ConfiguratioNumber
                 * | | |__NumberOfInterfaces
                 * | |__ "*" indicates the active configuration (others are " ")
                 * |__Config info tag
                 */
                case 'C':
                {
                    USBCONFIG Cfg = {0};
                    Cfg.fActive = psz[-2] == '*';
                    while (*psz && VBOX_SUCCESS (rc))
                    {
                        if (PREFIX ("#Ifs="))
                            rc = usbRead8 (pszValue, 10, &Cfg.bNumInterfaces, &psz);
                        else if (PREFIX ("Cfg#="))
                            rc = usbRead8 (pszValue, 10, &Cfg.bConfigurationValue, &psz);
                        else if (PREFIX ("Atr="))
                            rc = usbRead8 (pszValue, 16, &Cfg.bmAttributes, &psz);
                        else if (PREFIX ("MxPwr="))
                            rc = usbRead16 (pszValue, 10, &Cfg.u16MaxPower, &psz);
                        else
                            psz = usbReadSkip (psz);
                        psz = RTStrStripL (psz);
                    }
                    if (VBOX_SUCCESS (rc))
                    {
                        if (iCfg < Dev.bNumConfigurations)
                        {
                            /* Add the config. */
                            if (!Dev.paConfigurations)
                            {
                                Dev.paConfigurations = pCfg = (PUSBCONFIG) RTMemAllocZ (sizeof (Cfg) * Dev.bNumConfigurations);
                                if (pCfg)
                                {
                                    *pCfg = Cfg;
                                    iCfg = 1;
                                }
                                else
                                    rc = VERR_NO_MEMORY;
                            }
                            else
                            {
                                *++pCfg = Cfg;
                                iCfg++;
                            }
                        }
                        else
                        {
                            AssertMsgFailed (("iCfg=%d bNumConfigurations=%d\n", iCfg, Dev.bNumConfigurations));
                            rc = VERR_INTERNAL_ERROR;
                        }
                    }

                    /* new config, so, start anew with interfaces and endpoints. */
                    pIf = NULL;
                    iEp = 0;
                    pEp = NULL;
                    break;
                }

                /*
                 * I:  If#=dd Alt=dd #EPs=dd Cls=xx(sssss) Sub=xx Prot=xx Driver=ssss
                 * |   |      |      |       |             |      |       |__Driver name
                 * |   |      |      |       |             |      |          or "(none)"
                 * |   |      |      |       |             |      |__InterfaceProtocol
                 * |   |      |      |       |             |__InterfaceSubClass
                 * |   |      |      |       |__InterfaceClass
                 * |   |      |      |__NumberOfEndpoints
                 * |   |      |__AlternateSettingNumber
                 * |   |__InterfaceNumber
                 * |__Interface info tag
                 */
                case 'I':
                {
                    USBINTERFACE If = {0};
                    bool fIfAdopted = false;
                    while (*psz && VBOX_SUCCESS (rc))
                    {
                        if (PREFIX ("If#="))
                            rc = usbRead8 (pszValue, 10, &If.bInterfaceNumber, &psz);
                        else if (PREFIX ("Alt="))
                            rc = usbRead8 (pszValue, 10, &If.bAlternateSetting, &psz);
                        else if (PREFIX ("#EPs="))
                            rc = usbRead8 (pszValue, 10, &If.bNumEndpoints, &psz);
                        else if (PREFIX ("Cls="))
                            rc = usbRead8 (pszValue, 16, &If.bInterfaceClass, &psz);
                        else if (PREFIX ("Sub="))
                            rc = usbRead8 (pszValue, 16, &If.bInterfaceSubClass, &psz);
                        else if (PREFIX ("Prot="))
                            rc = usbRead8 (pszValue, 16, &If.bInterfaceProtocol, &psz);
                        else if (PREFIX ("Driver="))
                        {
                            rc = usbReadStr (pszValue, &If.pszDriver);
                            if (   If.pszDriver
                                && (    !strcmp (If.pszDriver, "(none)")
                                    ||  !strcmp (If.pszDriver, "(no driver)"))
                                    ||  !*If.pszDriver)
                            {
                                RTStrFree ((char *)If.pszDriver);
                                If.pszDriver = NULL;
                            }
                            break;
                        }
                        else
                            psz = usbReadSkip (psz);
                        psz = RTStrStripL (psz);
                    }
                    if (VBOX_SUCCESS (rc))
                    {
                        if (pCfg && If.bInterfaceNumber < pCfg->bNumInterfaces)
                        {
                            /* Add the config. */
                            if (!pCfg->paInterfaces)
                            {
                                pCfg->paInterfaces = pIf = (PUSBINTERFACE) RTMemAllocZ (sizeof (If) * pCfg->bNumInterfaces);
                                if (pIf)
                                {
                                    Assert (!If.bInterfaceNumber); Assert (!If.bAlternateSetting);
                                    *pIf = If;
                                    fIfAdopted = true;
                                }
                                else
                                    rc = VERR_NO_MEMORY;
                            }
                            else
                            {
                                /*
                                 * Alternate settings makes life *difficult*!
                                 * ASSUMES: ORDER ASC bInterfaceNumber, bAlternateSetting
                                 */
                                pIf = &pCfg->paInterfaces[If.bInterfaceNumber];
                                if (!If.bAlternateSetting)
                                {
                                    freeInterfaceMembers (pIf, 1);
                                    *pIf = If;
                                    fIfAdopted = true;
                                }
                                else
                                {
                                    PUSBINTERFACE paAlts = (PUSBINTERFACE) RTMemRealloc (pIf->paAlts, (pIf->cAlts + 1) * sizeof(*pIf));
                                    if (paAlts)
                                    {
                                        pIf->paAlts = paAlts;
                                        // don't do pIf = &paAlts[pIf->cAlts++]; as it will increment after the assignment
                                        unsigned cAlts = pIf->cAlts++;
                                        pIf = &paAlts[cAlts];
                                        *pIf = If;
                                        fIfAdopted = true;
                                    }
                                    else
                                        rc = VERR_NO_MEMORY;
                                }
                            }
                        }
                        else
                        {
                            AssertMsgFailed (("iCfg=%d bInterfaceNumber=%d bNumInterfaces=%d\n", iCfg, If.bInterfaceNumber, pCfg->bNumInterfaces));
                            rc = VERR_INTERNAL_ERROR;
                        }
                    }

                    if (!fIfAdopted)
                        freeInterfaceMembers (&If, 1);

                    /* start anew with endpoints. */
                    iEp = 0;
                    pEp = NULL;
                    break;
                }


                /*
                 * E:  Ad=xx(s) Atr=xx(ssss) MxPS=dddd Ivl=dddms
                 * |   |        |            |         |__Interval (max) between transfers
                 * |   |        |            |__EndpointMaxPacketSize
                 * |   |        |__Attributes(EndpointType)
                 * |   |__EndpointAddress(I=In,O=Out)
                 * |__Endpoint info tag
                 */
                case 'E':
                {
                    USBENDPOINT Ep = {0};
                    while (*psz && VBOX_SUCCESS (rc))
                    {
                        if (PREFIX ("Ad="))
                            rc = usbRead8 (pszValue, 16, &Ep.bEndpointAddress, &psz);
                        else if (PREFIX ("Atr="))
                            rc = usbRead8 (pszValue, 16, &Ep.bmAttributes, &psz);
                        else if (PREFIX ("MxPS="))
                            rc = usbRead16 (pszValue, 10, &Ep.wMaxPacketSize, &psz);
                        else if (PREFIX ("Ivl="))
                            rc = usbRead16Suff (pszValue, 10, &s_aIntervalSuff[0], &Ep.u16Interval, &psz);
                        else
                            psz = usbReadSkip (psz);
                        psz = RTStrStripL (psz);
                    }
                    if (VBOX_SUCCESS (rc))
                    {
                        if (pIf && iEp < pIf->bNumEndpoints)
                        {
                            /* Add the config. */
                            if (!pIf->paEndpoints)
                            {
                                pIf->paEndpoints = pEp = (PUSBENDPOINT) RTMemAllocZ (sizeof (Ep) * pIf->bNumEndpoints);
                                if (pEp)
                                {
                                    *pEp = Ep;
                                    iEp = 1;
                                }
                                else
                                    rc = VERR_NO_MEMORY;
                            }
                            else
                            {
                                *++pEp = Ep;
                                iEp++;
                            }
                        }
                        else
                        {
                            AssertMsgFailed (("iCfg=%d bInterfaceNumber=%d iEp=%d bNumInterfaces=%d\n", iCfg, pIf->bInterfaceNumber, iEp, pIf->bNumEndpoints));
                            rc = VERR_INTERNAL_ERROR;
                        }
                    }
                    break;
                }

            }
            #undef PREFIX
        } /* parse loop */

        /*
         * Add the current entry.
         */
        AssertMsg (cHits >= 3 || cHits == 0, ("cHits=%d\n", cHits));
        if (cHits >= 3)
        {
            Dev.enmState = usbDeterminState (&Dev);
            PUSBDEVICE pDev = (PUSBDEVICE) RTMemAlloc (sizeof(*pDev));
            if (pDev)
            {
                *pDev = Dev;
                if (Dev.enmState != USBDEVICESTATE_UNSUPPORTED)
                {
                    RTStrAPrintf((char **)&pDev->pszAddress, "%s/%03d/%03d", mUsbfsRoot.c_str(), pDev->bBus, pDev->bDevNum);
                    if (pDev->pszAddress)
                    {
                        if (ppNext)
                            *ppNext = pDev;
                        else
                            pFirst = pDev;
                        ppNext = &pDev->pNext;
                    }
                    else
                    {
                        rc = VERR_NO_MEMORY;
                        freeDevice (pDev);
                    }
                }
                else
                    freeDevice (pDev);
            }
            else
                rc = VERR_NO_MEMORY;
        }

        /*
         * Success?
         */
        if (VBOX_FAILURE (rc))
        {
            LogFlow (("USBProxyServiceLinux::getDevices: rc=%Vrc\n", rc));
            while (pFirst)
            {
                PUSBDEVICE pFree = pFirst;
                pFirst = pFirst->pNext;
                freeDevice (pFree);
            }
        }
    }
    return pFirst;
}

