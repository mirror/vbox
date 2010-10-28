/* $Id$ */
/** @file
 * VirtualBox Linux host USB device enumeration.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/

#include "USBGetDevices.h"

#include <VBox/usb.h>
#include <VBox/usblib.h>

#include <iprt/linux/sysfs.h>
#include <iprt/cdefs.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/fs.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include "vector.h"

#ifdef VBOX_WITH_LINUX_COMPILER_H
# include <linux/compiler.h>
#endif
#include <linux/usbdevice_fs.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

/** Structure describing a host USB device */
typedef struct USBDeviceInfo
{
    /** The device node of the device. */
    char *mDevice;
    /** The system identifier of the device.  Specific to the probing
     * method. */
    char *mSysfsPath;
    /** List of interfaces as sysfs paths */
    VECTOR_PTR(char *) mvecpszInterfaces;
} USBDeviceInfo;

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


int USBProxyLinuxCheckForUsbfs(const char *pcszDevices)
{
    int fd, rc = VINF_SUCCESS;

    fd = open(pcszDevices, O_RDONLY, 00600);
    if (fd >= 0)
    {
        /*
         * Check that we're actually on the usbfs.
         */
        struct statfs StFS;
        if (!fstatfs(fd, &StFS))
        {
            if (StFS.f_type != USBDEVICE_SUPER_MAGIC)
                rc = VERR_NOT_FOUND;
        }
        else
            rc = RTErrConvertFromErrno(errno);
        close(fd);
    }
    else
        rc = RTErrConvertFromErrno(errno);
    return rc;
}


/**
 * "reads" the number suffix. It's more like validating it and
 * skipping the necessary number of chars.
 */
static int usbReadSkipSuffix(char **ppszNext)
{
    char *pszNext = *ppszNext;
    if (!RT_C_IS_SPACE(*pszNext) && *pszNext)
    {
        /* skip unit */
        if (pszNext[0] == 'm' && pszNext[1] == 's')
            pszNext += 2;
        else if (pszNext[0] == 'm' && pszNext[1] == 'A')
            pszNext += 2;

        /* skip parenthesis */
        if (*pszNext == '(')
        {
            pszNext = strchr(pszNext, ')');
            if (!pszNext++)
            {
                AssertMsgFailed(("*ppszNext=%s\n", *ppszNext));
                return VERR_PARSE_ERROR;
            }
        }

        /* blank or end of the line. */
        if (!RT_C_IS_SPACE(*pszNext) && *pszNext)
        {
            AssertMsgFailed(("pszNext=%s\n", pszNext));
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
static int usbReadNum(const char *pszValue, unsigned uBase, uint32_t u32Mask, PCUSBSUFF paSuffs, void *pvNum, char **ppszNext)
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
    pszValue = RTStrStripL(pszValue);
    if (*pszValue)
    {
        /*
         * Try convert the number.
         */
        char *pszNext;
        uint32_t u32 = 0;
        RTStrToUInt32Ex(pszValue, &pszNext, uBase, &u32);
        if (pszNext == pszValue)
        {
            AssertMsgFailed(("pszValue=%d\n", pszValue));
            return VERR_NO_DATA;
        }

        /*
         * Check the range.
         */
        if (u32 & ~u32Mask)
        {
            AssertMsgFailed(("pszValue=%d u32=%#x lMask=%#x\n", pszValue, u32, u32Mask));
            return VERR_OUT_OF_RANGE;
        }

        /*
         * Validate and skip stuff following the number.
         */
        if (paSuffs)
        {
            if (!RT_C_IS_SPACE(*pszNext) && *pszNext)
            {
                for (PCUSBSUFF pSuff = paSuffs; pSuff->szSuff[0]; pSuff++)
                {
                    if (    !strncmp(pSuff->szSuff, pszNext, pSuff->cchSuff)
                        &&  (!pszNext[pSuff->cchSuff] || RT_C_IS_SPACE(pszNext[pSuff->cchSuff])))
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
            int rc = usbReadSkipSuffix(&pszNext);
            if (RT_FAILURE(rc))
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


static int usbRead8(const char *pszValue, unsigned uBase, uint8_t *pu8, char **ppszNext)
{
    return usbReadNum(pszValue, uBase, 0xff, NULL, pu8, ppszNext);
}


static int usbRead16(const char *pszValue, unsigned uBase, uint16_t *pu16, char **ppszNext)
{
    return usbReadNum(pszValue, uBase, 0xffff, NULL, pu16, ppszNext);
}


#if 0
static int usbRead16Suff(const char *pszValue, unsigned uBase, PCUSBSUFF paSuffs, uint16_t *pu16,  char **ppszNext)
{
    return usbReadNum(pszValue, uBase, 0xffff, paSuffs, pu16, ppszNext);
}
#endif


/**
 * Reads a USB BCD number returning the number and the position of the next character to parse.
 * The returned number contains the integer part in the high byte and the decimal part in the low byte.
 */
static int usbReadBCD(const char *pszValue, unsigned uBase, uint16_t *pu16, char **ppszNext)
{
    /*
     * Initialize return value to zero and strip leading spaces.
     */
    *pu16 = 0;
    pszValue = RTStrStripL(pszValue);
    if (*pszValue)
    {
        /*
         * Try convert the number.
         */
        /* integer part */
        char *pszNext;
        uint32_t u32Int = 0;
        RTStrToUInt32Ex(pszValue, &pszNext, uBase, &u32Int);
        if (pszNext == pszValue)
        {
            AssertMsgFailed(("pszValue=%s\n", pszValue));
            return VERR_NO_DATA;
        }
        if (u32Int & ~0xff)
        {
            AssertMsgFailed(("pszValue=%s u32Int=%#x (int)\n", pszValue, u32Int));
            return VERR_OUT_OF_RANGE;
        }

        /* skip dot and read decimal part */
        if (*pszNext != '.')
        {
            AssertMsgFailed(("pszValue=%s pszNext=%s (int)\n", pszValue, pszNext));
            return VERR_PARSE_ERROR;
        }
        char *pszValue2 = RTStrStripL(pszNext + 1);
        uint32_t u32Dec = 0;
        RTStrToUInt32Ex(pszValue2, &pszNext, uBase, &u32Dec);
        if (pszNext == pszValue)
        {
            AssertMsgFailed(("pszValue=%s\n", pszValue));
            return VERR_NO_DATA;
        }
        if (u32Dec & ~0xff)
        {
            AssertMsgFailed(("pszValue=%s u32Dec=%#x\n", pszValue, u32Dec));
            return VERR_OUT_OF_RANGE;
        }

        /*
         * Validate and skip stuff following the number.
         */
        int rc = usbReadSkipSuffix(&pszNext);
        if (RT_FAILURE(rc))
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
static int usbReadStr(const char *pszValue, const char **ppsz)
{
    if (*ppsz)
        RTStrFree((char *)*ppsz);
    *ppsz = RTStrDup(pszValue);
    if (*ppsz)
        return VINF_SUCCESS;
    return VERR_NO_MEMORY;
}


/**
 * Skips the current property.
 */
static char *usbReadSkip(char *pszValue)
{
    char *psz = strchr(pszValue, '=');
    if (psz)
        psz = strchr(psz + 1, '=');
    if (!psz)
        return strchr(pszValue,  '\0');
    while (psz > pszValue && !RT_C_IS_SPACE(psz[-1]))
        psz--;
    Assert(psz > pszValue);
    return psz;
}


/**
 * Determine the USB speed.
 */
static int usbReadSpeed(const char *pszValue, USBDEVICESPEED *pSpd, char **ppszNext)
{
    pszValue = RTStrStripL(pszValue);
    /* verified with Linux 2.4.0 ... Linux 2.6.25 */
    if (!strncmp(pszValue, "1.5", 3))
        *pSpd = USBDEVICESPEED_LOW;
    else if (!strncmp(pszValue, "12 ", 3))
        *pSpd = USBDEVICESPEED_FULL;
    else if (!strncmp(pszValue, "480", 3))
        *pSpd = USBDEVICESPEED_HIGH;
    else
        *pSpd = USBDEVICESPEED_UNKNOWN;
    while (pszValue[0] != '\0' && !RT_C_IS_SPACE(pszValue[0]))
        pszValue++;
    *ppszNext = (char *)pszValue;
    return VINF_SUCCESS;
}


/**
 * Compare a prefix and returns pointer to the char following it if it matches.
 */
static char *usbPrefix(char *psz, const char *pszPref, size_t cchPref)
{
    if (strncmp(psz, pszPref, cchPref))
        return NULL;
    return psz + cchPref;
}


/**
 * Does some extra checks to improve the detected device state.
 *
 * We cannot distinguish between USED_BY_HOST_CAPTURABLE and
 * USED_BY_GUEST, HELD_BY_PROXY all that well and it shouldn't be
 * necessary either.
 *
 * We will however, distinguish between the device we have permissions
 * to open and those we don't. This is necessary for two reasons.
 *
 * Firstly, because it's futile to even attempt opening a device which we
 * don't have access to, it only serves to confuse the user. (That said,
 * it might also be a bit confusing for the user to see that a USB device
 * is grayed out with no further explanation, and no way of generating an
 * error hinting at why this is the case.)
 *
 * Secondly and more importantly, we're racing against udevd with respect
 * to permissions and group settings on newly plugged devices. When we
 * detect a new device that we cannot access we will poll on it for a few
 * seconds to give udevd time to fix it. The polling is actually triggered
 * in the 'new device' case in the compare loop.
 *
 * The USBDEVICESTATE_USED_BY_HOST state is only used for this no-access
 * case, while USBDEVICESTATE_UNSUPPORTED is only used in the 'hub' case.
 * When it's neither of these, we set USBDEVICESTATE_UNUSED or
 * USBDEVICESTATE_USED_BY_HOST_CAPTURABLE depending on whether there is
 * a driver associated with any of the interfaces.
 *
 * All except the access check and a special idVendor == 0 precaution
 * is handled at parse time.
 *
 * @returns The adjusted state.
 * @param   pDevice     The device.
 */
static USBDEVICESTATE usbDeterminState(PCUSBDEVICE pDevice)
{
    /*
     * If it's already flagged as unsupported, there is nothing to do.
     */
    USBDEVICESTATE enmState = pDevice->enmState;
    if (enmState == USBDEVICESTATE_UNSUPPORTED)
        return USBDEVICESTATE_UNSUPPORTED;

    /*
     * Root hubs and similar doesn't have any vendor id, just
     * refuse these device.
     */
    if (!pDevice->idVendor)
        return USBDEVICESTATE_UNSUPPORTED;

    /*
     * Check if we've got access to the device, if we haven't flag
     * it as used-by-host.
     */
#ifndef VBOX_USB_WITH_SYSFS
    const char *pszAddress = pDevice->pszAddress;
#else
    if (pDevice->pszAddress == NULL)
        /* We can't do much with the device without an address. */
        return USBDEVICESTATE_UNSUPPORTED;
    const char *pszAddress = strstr(pDevice->pszAddress, "//device:");
    pszAddress = pszAddress != NULL
               ? pszAddress + sizeof("//device:") - 1
               : pDevice->pszAddress;
#endif
    if (    access(pszAddress, R_OK | W_OK) != 0
        &&  errno == EACCES)
        return USBDEVICESTATE_USED_BY_HOST;

#ifdef VBOX_USB_WITH_SYSFS
    /**
     * @todo Check that any other essential fields are present and mark as
     * invalid if not.  Particularly to catch the case where the device was
     * unplugged while we were reading in its properties.
     */
#endif

    return enmState;
}


/** Just a worker for USBProxyServiceLinux::getDevices that avoids some code duplication. */
static int addDeviceToChain(PUSBDEVICE pDev, PUSBDEVICE *ppFirst, PUSBDEVICE **pppNext, const char *pcszUsbfsRoot, int rc)
{
    /* usbDeterminState requires the address. */
    PUSBDEVICE pDevNew = (PUSBDEVICE)RTMemDup(pDev, sizeof(*pDev));
    if (pDevNew)
    {
        RTStrAPrintf((char **)&pDevNew->pszAddress, "%s/%03d/%03d", pcszUsbfsRoot, pDevNew->bBus, pDevNew->bDevNum);
        if (pDevNew->pszAddress)
        {
            pDevNew->enmState = usbDeterminState(pDevNew);
            if (pDevNew->enmState != USBDEVICESTATE_UNSUPPORTED)
            {
                if (*pppNext)
                    **pppNext = pDevNew;
                else
                    *ppFirst = pDevNew;
                *pppNext = &pDevNew->pNext;
            }
            else
                deviceFree(pDevNew);
        }
        else
        {
            deviceFree(pDevNew);
            rc = VERR_NO_MEMORY;
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
        deviceFreeMembers(pDev);
    }

    return rc;
}


static int openDevicesFile(const char *pcszUsbfsRoot, FILE **ppFile)
{
    char *pszPath;
    FILE *pFile;
    RTStrAPrintf(&pszPath, "%s/devices", pcszUsbfsRoot);
    if (!pszPath)
        return VERR_NO_MEMORY;
    pFile = fopen(pszPath, "r");
    RTStrFree(pszPath);
    if (!pFile)
        return RTErrConvertFromErrno(errno);
    *ppFile = pFile;
    return VINF_SUCCESS;
}

/**
 * USBProxyService::getDevices() implementation for usbfs.
 */
static PUSBDEVICE getDevicesFromUsbfs(const char *pcszUsbfsRoot)
{
    PUSBDEVICE pFirst = NULL;
    FILE *pFile = NULL;
    int rc;
    rc = openDevicesFile(pcszUsbfsRoot, &pFile);
    if (RT_SUCCESS(rc))
    {
        PUSBDEVICE     *ppNext = NULL;
        int             cHits = 0;
        char            szLine[1024];
        USBDEVICE       Dev;
        RT_ZERO(Dev);
        Dev.enmState = USBDEVICESTATE_UNUSED;

        /* Set close on exit and hope no one is racing us. */
        rc =   fcntl(fileno(pFile), F_SETFD, FD_CLOEXEC) >= 0
             ? VINF_SUCCESS
             : RTErrConvertFromErrno(errno);
        while (     RT_SUCCESS(rc)
               &&   fgets(szLine, sizeof(szLine), pFile))
        {
            char   *psz;
            char   *pszValue;

            /* validate and remove the trailing newline. */
            psz = strchr(szLine, '\0');
            if (psz[-1] != '\n' && !feof(pFile))
            {
                AssertMsgFailed(("Line too long. (cch=%d)\n", strlen(szLine)));
                continue;
            }

            /* strip */
            psz = RTStrStrip(szLine);
            if (!*psz)
                continue;

            /*
             * Interpret the line.
             * (Ordered by normal occurrence.)
             */
            char ch = psz[0];
            if (psz[1] != ':')
                continue;
            psz = RTStrStripL(psz + 3);
#define PREFIX(str) ( (pszValue = usbPrefix(psz, str, sizeof(str) - 1)) != NULL )
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
                    AssertMsg(cHits >= 3 || cHits == 0, ("cHits=%d\n", cHits));
                    if (cHits >= 3)
                        rc = addDeviceToChain(&Dev, &pFirst, &ppNext, pcszUsbfsRoot, rc);
                    else
                        deviceFreeMembers(&Dev);

                    /* Reset device state */
                    memset(&Dev, 0, sizeof (Dev));
                    Dev.enmState = USBDEVICESTATE_UNUSED;
                    cHits = 1;

                    /* parse the line. */
                    while (*psz && RT_SUCCESS(rc))
                    {
                        if (PREFIX("Bus="))
                            rc = usbRead8(pszValue, 10, &Dev.bBus, &psz);
                        else if (PREFIX("Port="))
                            rc = usbRead8(pszValue, 10, &Dev.bPort, &psz);
                        else if (PREFIX("Spd="))
                            rc = usbReadSpeed(pszValue, &Dev.enmSpeed, &psz);
                        else if (PREFIX("Dev#="))
                            rc = usbRead8(pszValue, 10, &Dev.bDevNum, &psz);
                        else
                            psz = usbReadSkip(psz);
                        psz = RTStrStripL(psz);
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
                    while (*psz && RT_SUCCESS(rc))
                    {
                        if (PREFIX("Ver="))
                            rc = usbReadBCD(pszValue, 16, &Dev.bcdUSB, &psz);
                        else if (PREFIX("Cls="))
                        {
                            rc = usbRead8(pszValue, 16, &Dev.bDeviceClass, &psz);
                            if (RT_SUCCESS(rc) && Dev.bDeviceClass == 9 /* HUB */)
                                Dev.enmState = USBDEVICESTATE_UNSUPPORTED;
                        }
                        else if (PREFIX("Sub="))
                            rc = usbRead8(pszValue, 16, &Dev.bDeviceSubClass, &psz);
                        else if (PREFIX("Prot="))
                            rc = usbRead8(pszValue, 16, &Dev.bDeviceProtocol, &psz);
                        //else if (PREFIX("MxPS="))
                        //    rc = usbRead16(pszValue, 10, &Dev.wMaxPacketSize, &psz);
                        else if (PREFIX("#Cfgs="))
                            rc = usbRead8(pszValue, 10, &Dev.bNumConfigurations, &psz);
                        else
                            psz = usbReadSkip(psz);
                        psz = RTStrStripL(psz);
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
                    while (*psz && RT_SUCCESS(rc))
                    {
                        if (PREFIX("Vendor="))
                            rc = usbRead16(pszValue, 16, &Dev.idVendor, &psz);
                        else if (PREFIX("ProdID="))
                            rc = usbRead16(pszValue, 16, &Dev.idProduct, &psz);
                        else if (PREFIX("Rev="))
                            rc = usbReadBCD(pszValue, 16, &Dev.bcdDevice, &psz);
                        else
                            psz = usbReadSkip(psz);
                        psz = RTStrStripL(psz);
                    }
                    cHits++;
                    break;

                /*
                 * String.
                 */
                case 'S':
                    if (PREFIX("Manufacturer="))
                        rc = usbReadStr(pszValue, &Dev.pszManufacturer);
                    else if (PREFIX("Product="))
                        rc = usbReadStr(pszValue, &Dev.pszProduct);
                    else if (PREFIX("SerialNumber="))
                    {
                        rc = usbReadStr(pszValue, &Dev.pszSerialNumber);
                        if (RT_SUCCESS(rc))
                            Dev.u64SerialHash = USBLibHashSerial(pszValue);
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
                    break;

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
                    /* Check for thing we don't support.  */
                    while (*psz && RT_SUCCESS(rc))
                    {
                        if (PREFIX("Driver="))
                        {
                            const char *pszDriver = NULL;
                            rc = usbReadStr(pszValue, &pszDriver);
                            if (   !pszDriver
                                || !*pszDriver
                                || !strcmp(pszDriver, "(none)")
                                || !strcmp(pszDriver, "(no driver)"))
                                /* no driver */;
                            else if (!strcmp(pszDriver, "hub"))
                                Dev.enmState = USBDEVICESTATE_UNSUPPORTED;
                            else if (Dev.enmState == USBDEVICESTATE_UNUSED)
                                Dev.enmState = USBDEVICESTATE_USED_BY_HOST_CAPTURABLE;
                            RTStrFree((char *)pszDriver);
                            break; /* last attrib */
                        }
                        else if (PREFIX("Cls="))
                        {
                            uint8_t bInterfaceClass;
                            rc = usbRead8(pszValue, 16, &bInterfaceClass, &psz);
                            if (RT_SUCCESS(rc) && bInterfaceClass == 9 /* HUB */)
                                Dev.enmState = USBDEVICESTATE_UNSUPPORTED;
                        }
                        else
                            psz = usbReadSkip(psz);
                        psz = RTStrStripL(psz);
                    }
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
                    break;

            }
#undef PREFIX
        } /* parse loop */
        fclose(pFile);

        /*
         * Add the current entry.
         */
        AssertMsg(cHits >= 3 || cHits == 0, ("cHits=%d\n", cHits));
        if (cHits >= 3)
            rc = addDeviceToChain(&Dev, &pFirst, &ppNext, pcszUsbfsRoot, rc);

        /*
         * Success?
         */
        if (RT_FAILURE(rc))
        {
            while (pFirst)
            {
                PUSBDEVICE pFree = pFirst;
                pFirst = pFirst->pNext;
                deviceFree(pFree);
            }
        }
    }
    if (RT_FAILURE(rc))
        LogFlow(("USBProxyServiceLinux::getDevices: rc=%Rrc\n", rc));
    return pFirst;
}

#ifdef VBOX_USB_WITH_SYSFS

static void USBDevInfoCleanup(USBDeviceInfo *pSelf)
{
    RTStrFree(pSelf->mDevice);
    RTStrFree(pSelf->mSysfsPath);
    pSelf->mDevice = pSelf->mSysfsPath = NULL;
    VEC_CLEANUP_PTR(&pSelf->mvecpszInterfaces);
}

static int USBDevInfoInit(USBDeviceInfo *pSelf, const char *aDevice,
                   const char *aSystemID)
{
    pSelf->mDevice = aDevice ? RTStrDup(aDevice) : NULL;
    pSelf->mSysfsPath = aSystemID ? RTStrDup(aSystemID) : NULL;
    VEC_INIT_PTR(&pSelf->mvecpszInterfaces, char *, RTStrFree);
    if ((aDevice && !pSelf->mDevice) || (aSystemID && ! pSelf->mSysfsPath))
    {
        USBDevInfoCleanup(pSelf);
        return 0;
    }
    return 1;
}

#define USBDEVICE_MAJOR 189

/** Deduce the bus that a USB device is plugged into from the device node
 * number.  See drivers/usb/core/hub.c:usb_new_device as of Linux 2.6.20. */
static unsigned usbBusFromDevNum(dev_t devNum)
{
    AssertReturn(devNum, 0);
    AssertReturn(major(devNum) == USBDEVICE_MAJOR, 0);
    return (minor(devNum) >> 7) + 1;
}


/** Deduce the device number of a USB device on the bus from the device node
 * number.  See drivers/usb/core/hub.c:usb_new_device as of Linux 2.6.20. */
static unsigned usbDeviceFromDevNum(dev_t devNum)
{
    AssertReturn(devNum, 0);
    AssertReturn(major(devNum) == USBDEVICE_MAJOR, 0);
    return (minor(devNum) & 127) + 1;
}


/**
 * If a file @a pcszNode from /sys/bus/usb/devices is a device rather than an
 * interface add an element for the device to @a pvecDevInfo.
 */
static int addIfDevice(const char *pcszNode,
                       VECTOR_OBJ(USBDeviceInfo) *pvecDevInfo)
{
    const char *pcszFile = strrchr(pcszNode, '/');
    if (strchr(pcszFile, ':'))
        return VINF_SUCCESS;
    dev_t devnum = RTLinuxSysFsReadDevNumFile("%s/dev", pcszNode);
    /* Sanity test of our static helpers */
    Assert(usbBusFromDevNum(makedev(USBDEVICE_MAJOR, 517)) == 5);
    Assert(usbDeviceFromDevNum(makedev(USBDEVICE_MAJOR, 517)) == 6);
    if (!devnum)
        return VINF_SUCCESS;
    char szDevPath[RTPATH_MAX];
    ssize_t cchDevPath;
    cchDevPath = RTLinuxFindDevicePath(devnum, RTFS_TYPE_DEV_CHAR,
                                       szDevPath, sizeof(szDevPath),
                                       "/dev/bus/usb/%.3d/%.3d",
                                       usbBusFromDevNum(devnum),
                                       usbDeviceFromDevNum(devnum));
    if (cchDevPath < 0)
        return VINF_SUCCESS;

    USBDeviceInfo info;
    if (USBDevInfoInit(&info, szDevPath, pcszNode))
        if (RT_SUCCESS(VEC_PUSH_BACK_OBJ(pvecDevInfo, USBDeviceInfo,
                                         &info)))
            return VINF_SUCCESS;
    USBDevInfoCleanup(&info);
    return VERR_NO_MEMORY;
}

/** The logic for testing whether a sysfs address corresponds to an
 * interface of a device.  Both must be referenced by their canonical
 * sysfs paths.  This is not tested, as the test requires file-system
 * interaction. */
static bool muiIsAnInterfaceOf(const char *pcszIface, const char *pcszDev)
{
    size_t cchDev = strlen(pcszDev);

    AssertPtr(pcszIface);
    AssertPtr(pcszDev);
    Assert(pcszIface[0] == '/');
    Assert(pcszDev[0] == '/');
    Assert(pcszDev[cchDev - 1] != '/');
    /* If this passes, pcszIface is at least cchDev long */
    if (strncmp(pcszIface, pcszDev, cchDev))
        return false;
    /* If this passes, pcszIface is longer than cchDev */
    if (pcszIface[cchDev] != '/')
        return false;
    /* In sysfs an interface is an immediate subdirectory of the device */
    if (strchr(pcszIface + cchDev + 1, '/'))
        return false;
    /* And it always has a colon in its name */
    if (!strchr(pcszIface + cchDev + 1, ':'))
        return false;
    /* And hopefully we have now elimitated everything else */
    return true;
}

#ifdef DEBUG
# ifdef __cplusplus
/** Unit test the logic in muiIsAnInterfaceOf in debug builds. */
class testIsAnInterfaceOf
{
public:
    testIsAnInterfaceOf()
    {
        Assert(muiIsAnInterfaceOf("/sys/devices/pci0000:00/0000:00:1a.0/usb3/3-0:1.0",
               "/sys/devices/pci0000:00/0000:00:1a.0/usb3"));
        Assert(!muiIsAnInterfaceOf("/sys/devices/pci0000:00/0000:00:1a.0/usb3/3-1",
               "/sys/devices/pci0000:00/0000:00:1a.0/usb3"));
        Assert(!muiIsAnInterfaceOf("/sys/devices/pci0000:00/0000:00:1a.0/usb3/3-0:1.0/driver",
               "/sys/devices/pci0000:00/0000:00:1a.0/usb3"));
    }
};
static testIsAnInterfaceOf testIsAnInterfaceOfInst;
# endif /* __cplusplus */
#endif /* DEBUG */

/**
 * Tell whether a file in /sys/bus/usb/devices is an interface rather than a
 * device.  To be used with getDeviceInfoFromSysfs().
 */
static int addIfInterfaceOf(const char *pcszNode, USBDeviceInfo *pInfo)
{
    if (!muiIsAnInterfaceOf(pcszNode, pInfo->mSysfsPath))
        return VINF_SUCCESS;
    char *pszDup = (char *)RTStrDup(pcszNode);
    if (pszDup)
        if (RT_SUCCESS(VEC_PUSH_BACK_PTR(&pInfo->mvecpszInterfaces,
                                         char *, pszDup)))
            return VINF_SUCCESS;
    RTStrFree(pszDup);
    return VERR_NO_MEMORY;
}

/** Helper for readFilePaths().  Adds the entries from the open directory
 * @a pDir to the vector @a pvecpchDevs using either the full path or the
 * realpath() and skipping hidden files and files on which realpath() fails. */
static int readFilePathsFromDir(const char *pcszPath, DIR *pDir,
                                VECTOR_PTR(char *) *pvecpchDevs)
{
    struct dirent entry, *pResult;
    int err, rc;

    for (err = readdir_r(pDir, &entry, &pResult); pResult;
         err = readdir_r(pDir, &entry, &pResult))
    {
        char szPath[RTPATH_MAX + 1], szRealPath[RTPATH_MAX + 1], *pszPath;
        if (entry.d_name[0] == '.')
            continue;
        if (snprintf(szPath, sizeof(szPath), "%s/%s", pcszPath,
                     entry.d_name) < 0)
            return RTErrConvertFromErrno(errno);
        pszPath = RTStrDup(realpath(szPath, szRealPath));
        if (!pszPath)
            return VERR_NO_MEMORY;
        if (RT_FAILURE(rc = VEC_PUSH_BACK_PTR(pvecpchDevs, char *, pszPath)))
            return rc;
    }
    return RTErrConvertFromErrno(err);
}

/**
 * Dump the names of a directory's entries into a vector of char pointers.
 *
 * @returns zero on success or (positive) posix error value.
 * @param   pcszPath      the path to dump.
 * @param   pvecpchDevs   an empty vector of char pointers - must be cleaned up
 *                        by the caller even on failure.
 * @param   withRealPath  whether to canonicalise the filename with realpath
 */
static int readFilePaths(const char *pcszPath, VECTOR_PTR(char *) *pvecpchDevs)
{
    DIR *pDir;
    int rc;

    AssertPtrReturn(pvecpchDevs, EINVAL);
    AssertReturn(VEC_SIZE_PTR(pvecpchDevs) == 0, EINVAL);
    AssertPtrReturn(pcszPath, EINVAL);

    pDir = opendir(pcszPath);
    if (!pDir)
        return RTErrConvertFromErrno(errno);
    rc = readFilePathsFromDir(pcszPath, pDir, pvecpchDevs);
    if (closedir(pDir) < 0 && RT_SUCCESS(rc))
        rc = RTErrConvertFromErrno(errno);
    return rc;
}

/**
 * Logic for USBSysfsEnumerateHostDevices.
 * @param pvecDevInfo  vector of device information structures to add device
 *                     information to
 * @param pvecpchDevs  empty scratch vector which will be freed by the caller
 */
static int doSysfsEnumerateHostDevices(VECTOR_OBJ(USBDeviceInfo) *pvecDevInfo,
                              VECTOR_PTR(char *) *pvecpchDevs)
{
    char **ppszEntry;
    USBDeviceInfo *pInfo;
    int rc;

    AssertPtrReturn(pvecDevInfo, VERR_INVALID_POINTER);
    LogFlowFunc (("pvecDevInfo=%p\n", pvecDevInfo));

    rc = readFilePaths("/sys/bus/usb/devices", pvecpchDevs);
    if (RT_FAILURE(rc))
        return rc;
    VEC_FOR_EACH(pvecpchDevs, char *, ppszEntry)
        if (RT_FAILURE(rc = addIfDevice(*ppszEntry, pvecDevInfo)))
            return rc;
    VEC_FOR_EACH(pvecDevInfo, USBDeviceInfo, pInfo)
        VEC_FOR_EACH(pvecpchDevs, char *, ppszEntry)
            if (RT_FAILURE(rc = addIfInterfaceOf(*ppszEntry, pInfo)))
                return rc;
    return VINF_SUCCESS;
}

static int USBSysfsEnumerateHostDevices(VECTOR_OBJ(USBDeviceInfo) *pvecDevInfo)
{
    VECTOR_PTR(char *) vecpchDevs;
    int rc = VERR_NOT_IMPLEMENTED;

    AssertReturn(VEC_SIZE_OBJ(pvecDevInfo) == 0, VERR_INVALID_PARAMETER);
    LogFlowFunc(("entered\n"));
    VEC_INIT_PTR(&vecpchDevs, char *, RTStrFree);
    rc = doSysfsEnumerateHostDevices(pvecDevInfo, &vecpchDevs);
    VEC_CLEANUP_PTR(&vecpchDevs);
    LogFlowFunc(("rc=%Rrc\n", rc));
    return rc;
}

/**
 * Helper function for extracting the port number on the parent device from
 * the sysfs path value.
 *
 * The sysfs path is a chain of elements separated by forward slashes, and for
 * USB devices, the last element in the chain takes the form
 *   <port>-<port>.[...].<port>[:<config>.<interface>]
 * where the first <port> is the port number on the root hub, and the following
 * (optional) ones are the port numbers on any other hubs between the device
 * and the root hub.  The last part (:<config.interface>) is only present for
 * interfaces, not for devices.  This API should only be called for devices.
 * For compatibility with usbfs, which enumerates from zero up, we subtract one
 * from the port number.
 *
 * For root hubs, the last element in the chain takes the form
 *   usb<hub number>
 * and usbfs always returns port number zero.
 *
 * @returns VBox status. pu8Port is set on success.
 * @param   pszPath     The sysfs path to parse.
 * @param   pu8Port     Where to store the port number.
 */
static int usbGetPortFromSysfsPath(const char *pszPath, uint8_t *pu8Port)
{
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertPtrReturn(pu8Port, VERR_INVALID_POINTER);

    /*
     * This should not be possible until we get PCs with USB as their primary bus.
     * Note: We don't assert this, as we don't expect the caller to validate the
     *       sysfs path.
     */
    const char *pszLastComp = strrchr(pszPath, '/');
    if (!pszLastComp)
    {
        Log(("usbGetPortFromSysfsPath(%s): failed [1]\n", pszPath));
        return VERR_INVALID_PARAMETER;
    }
    pszLastComp++; /* skip the slash */

    /*
     * This API should not be called for interfaces, so the last component
     * of the path should not contain a colon.  We *do* assert this, as it
     * might indicate a caller bug.
     */
    AssertMsgReturn(strchr(pszLastComp, ':') == NULL, ("%s\n", pszPath), VERR_INVALID_PARAMETER);

    /*
     * Look for the start of the last number.
     */
    const char *pchDash = strrchr(pszLastComp, '-');
    const char *pchDot  = strrchr(pszLastComp, '.');
    if (!pchDash && !pchDot)
    {
        /* No -/. so it must be a root hub. Check that it's usb<something>. */
        if (strncmp(pszLastComp, "usb", sizeof("usb") - 1) != 0)
        {
            Log(("usbGetPortFromSysfsPath(%s): failed [2]\n", pszPath));
            return VERR_INVALID_PARAMETER;
        }
        return VERR_NOT_SUPPORTED;
    }
    else
    {
        const char *pszLastPort = pchDot != NULL
                                ? pchDot  + 1
                                : pchDash + 1;
        int rc = RTStrToUInt8Full(pszLastPort, 10, pu8Port);
        if (rc != VINF_SUCCESS)
        {
            Log(("usbGetPortFromSysfsPath(%s): failed [3], rc=%Rrc\n", pszPath, rc));
            return VERR_INVALID_PARAMETER;
        }
        if (*pu8Port == 0)
        {
            Log(("usbGetPortFromSysfsPath(%s): failed [4]\n", pszPath));
            return VERR_INVALID_PARAMETER;
        }

        /* usbfs compatibility, 0-based port number. */
        *pu8Port -= 1;
    }
    return VINF_SUCCESS;
}


/**
 * Dumps a USBDEVICE structure to the log using LogLevel 3.
 * @param   pDev        The structure to log.
 * @todo    This is really common code.
 */
DECLINLINE(void) usbLogDevice(PUSBDEVICE pDev)
{
    NOREF(pDev);

    Log3(("USB device:\n"));
    Log3(("Product: %s (%x)\n", pDev->pszProduct, pDev->idProduct));
    Log3(("Manufacturer: %s (Vendor ID %x)\n", pDev->pszManufacturer, pDev->idVendor));
    Log3(("Serial number: %s (%llx)\n", pDev->pszSerialNumber, pDev->u64SerialHash));
    Log3(("Device revision: %d\n", pDev->bcdDevice));
    Log3(("Device class: %x\n", pDev->bDeviceClass));
    Log3(("Device subclass: %x\n", pDev->bDeviceSubClass));
    Log3(("Device protocol: %x\n", pDev->bDeviceProtocol));
    Log3(("USB version number: %d\n", pDev->bcdUSB));
    Log3(("Device speed: %s\n",
            pDev->enmSpeed == USBDEVICESPEED_UNKNOWN  ? "unknown"
          : pDev->enmSpeed == USBDEVICESPEED_LOW      ? "1.5 MBit/s"
          : pDev->enmSpeed == USBDEVICESPEED_FULL     ? "12 MBit/s"
          : pDev->enmSpeed == USBDEVICESPEED_HIGH     ? "480 MBit/s"
          : pDev->enmSpeed == USBDEVICESPEED_VARIABLE ? "variable"
          :                                             "invalid"));
    Log3(("Number of configurations: %d\n", pDev->bNumConfigurations));
    Log3(("Bus number: %d\n", pDev->bBus));
    Log3(("Port number: %d\n", pDev->bPort));
    Log3(("Device number: %d\n", pDev->bDevNum));
    Log3(("Device state: %s\n",
            pDev->enmState == USBDEVICESTATE_UNSUPPORTED   ? "unsupported"
          : pDev->enmState == USBDEVICESTATE_USED_BY_HOST  ? "in use by host"
          : pDev->enmState == USBDEVICESTATE_USED_BY_HOST_CAPTURABLE ? "in use by host, possibly capturable"
          : pDev->enmState == USBDEVICESTATE_UNUSED        ? "not in use"
          : pDev->enmState == USBDEVICESTATE_HELD_BY_PROXY ? "held by proxy"
          : pDev->enmState == USBDEVICESTATE_USED_BY_GUEST ? "used by guest"
          :                                                  "invalid"));
    Log3(("OS device address: %s\n", pDev->pszAddress));
}

/**
 * In contrast to usbReadBCD() this function can handle BCD values without
 * a decimal separator. This is necessary for parsing bcdDevice.
 * @param   pszBuf      Pointer to the string buffer.
 * @param   pu15        Pointer to the return value.
 * @returns IPRT status code.
 */
static int convertSysfsStrToBCD(const char *pszBuf, uint16_t *pu16)
{
    char *pszNext;
    int32_t i32;

    pszBuf = RTStrStripL(pszBuf);
    int rc = RTStrToInt32Ex(pszBuf, &pszNext, 16, &i32);
    if (   RT_FAILURE(rc)
        || rc == VWRN_NUMBER_TOO_BIG
        || i32 < 0)
        return VERR_NUMBER_TOO_BIG;
    if (*pszNext == '.')
    {
        if (i32 > 255)
            return VERR_NUMBER_TOO_BIG;
        int32_t i32Lo;
        rc = RTStrToInt32Ex(pszNext+1, &pszNext, 16, &i32Lo);
        if (   RT_FAILURE(rc)
            || rc == VWRN_NUMBER_TOO_BIG
            || i32Lo > 255
            || i32Lo < 0)
            return VERR_NUMBER_TOO_BIG;
        i32 = (i32 << 8) | i32Lo;
    }
    if (   i32 > 65535
        || (*pszNext != '\0' && *pszNext != ' '))
        return VERR_NUMBER_TOO_BIG;

    *pu16 = (uint16_t)i32;
    return VINF_SUCCESS;
}

#endif  /* VBOX_USB_WITH_SYSFS */

static void fillInDeviceFromSysfs(USBDEVICE *Dev, USBDeviceInfo *pInfo)
{
    int rc;
    const char *pszSysfsPath = pInfo->mSysfsPath;

    /* Fill in the simple fields */
    Dev->enmState           = USBDEVICESTATE_UNUSED;
    Dev->bBus               = RTLinuxSysFsReadIntFile(10, "%s/busnum", pszSysfsPath);
    Dev->bDeviceClass       = RTLinuxSysFsReadIntFile(16, "%s/bDeviceClass", pszSysfsPath);
    Dev->bDeviceSubClass    = RTLinuxSysFsReadIntFile(16, "%s/bDeviceSubClass", pszSysfsPath);
    Dev->bDeviceProtocol    = RTLinuxSysFsReadIntFile(16, "%s/bDeviceProtocol", pszSysfsPath);
    Dev->bNumConfigurations = RTLinuxSysFsReadIntFile(10, "%s/bNumConfigurations", pszSysfsPath);
    Dev->idVendor           = RTLinuxSysFsReadIntFile(16, "%s/idVendor", pszSysfsPath);
    Dev->idProduct          = RTLinuxSysFsReadIntFile(16, "%s/idProduct", pszSysfsPath);
    Dev->bDevNum            = RTLinuxSysFsReadIntFile(10, "%s/devnum", pszSysfsPath);

    /* Now deal with the non-numeric bits. */
    char szBuf[1024];  /* Should be larger than anything a sane device
                        * will need, and insane devices can be unsupported
                        * until further notice. */
    ssize_t cchRead;

    /* For simplicity, we just do strcmps on the next one. */
    cchRead = RTLinuxSysFsReadStrFile(szBuf, sizeof(szBuf), "%s/speed",
                                      pszSysfsPath);
    if (cchRead <= 0 || (size_t) cchRead == sizeof(szBuf))
        Dev->enmState = USBDEVICESTATE_UNSUPPORTED;
    else
        Dev->enmSpeed =   !strcmp(szBuf, "1.5") ? USBDEVICESPEED_LOW
                        : !strcmp(szBuf, "12")  ? USBDEVICESPEED_FULL
                        : !strcmp(szBuf, "480") ? USBDEVICESPEED_HIGH
                        : USBDEVICESPEED_UNKNOWN;

    cchRead = RTLinuxSysFsReadStrFile(szBuf, sizeof(szBuf), "%s/version",
                                      pszSysfsPath);
    if (cchRead <= 0 || (size_t) cchRead == sizeof(szBuf))
        Dev->enmState = USBDEVICESTATE_UNSUPPORTED;
    else
    {
        rc = convertSysfsStrToBCD(szBuf, &Dev->bcdUSB);
        if (RT_FAILURE(rc))
        {
            Dev->enmState = USBDEVICESTATE_UNSUPPORTED;
            Dev->bcdUSB = (uint16_t)-1;
        }
    }

    cchRead = RTLinuxSysFsReadStrFile(szBuf, sizeof(szBuf), "%s/bcdDevice",
                                      pszSysfsPath);
    if (cchRead <= 0 || (size_t) cchRead == sizeof(szBuf))
        Dev->bcdDevice = (uint16_t)-1;
    else
    {
        rc = convertSysfsStrToBCD(szBuf, &Dev->bcdDevice);
        if (RT_FAILURE(rc))
            Dev->bcdDevice = (uint16_t)-1;
    }

    /* Now do things that need string duplication */
    cchRead = RTLinuxSysFsReadStrFile(szBuf, sizeof(szBuf), "%s/product",
                                      pszSysfsPath);
    if (cchRead > 0 && (size_t) cchRead < sizeof(szBuf))
    {
        RTStrPurgeEncoding(szBuf);
        Dev->pszProduct = RTStrDup(szBuf);
    }

    cchRead = RTLinuxSysFsReadStrFile(szBuf, sizeof(szBuf), "%s/serial",
                                      pszSysfsPath);
    if (cchRead > 0 && (size_t) cchRead < sizeof(szBuf))
    {
        RTStrPurgeEncoding(szBuf);
        Dev->pszSerialNumber = RTStrDup(szBuf);
        Dev->u64SerialHash = USBLibHashSerial(szBuf);
    }

    cchRead = RTLinuxSysFsReadStrFile(szBuf, sizeof(szBuf), "%s/manufacturer",
                                      pszSysfsPath);
    if (cchRead > 0 && (size_t) cchRead < sizeof(szBuf))
    {
        RTStrPurgeEncoding(szBuf);
        Dev->pszManufacturer = RTStrDup(szBuf);
    }

    /* Work out the port number */
    if (RT_FAILURE(usbGetPortFromSysfsPath(pszSysfsPath, &Dev->bPort)))
        Dev->enmState = USBDEVICESTATE_UNSUPPORTED;

    /* Check the interfaces to see if we can support the device. */
    char **ppszIf;
    VEC_FOR_EACH(&pInfo->mvecpszInterfaces, char *, ppszIf)
    {
        ssize_t cb = RTLinuxSysFsGetLinkDest(szBuf, sizeof(szBuf), "%s/driver",
                                             *ppszIf);
        if (cb > 0 && Dev->enmState != USBDEVICESTATE_UNSUPPORTED)
            Dev->enmState = (strcmp(szBuf, "hub") == 0)
                          ? USBDEVICESTATE_UNSUPPORTED
                          : USBDEVICESTATE_USED_BY_HOST_CAPTURABLE;
        if (RTLinuxSysFsReadIntFile(16, "%s/bInterfaceClass",
                                    *ppszIf) == 9 /* hub */)
            Dev->enmState = USBDEVICESTATE_UNSUPPORTED;
    }

    /* We use a double slash as a separator in the pszAddress field.  This is
     * alright as the two paths can't contain a slash due to the way we build
     * them. */
    char *pszAddress = NULL;
    RTStrAPrintf(&pszAddress, "sysfs:%s//device:%s", pszSysfsPath,
                 pInfo->mDevice);
    Dev->pszAddress = pszAddress;

    /* Work out from the data collected whether we can support this device. */
    Dev->enmState = usbDeterminState(Dev);
    usbLogDevice(Dev);
}

/**
 * USBProxyService::getDevices() implementation for sysfs.
 */
static PUSBDEVICE getDevicesFromSysfs(void)
{
#ifdef VBOX_USB_WITH_SYSFS
    /* Add each of the devices found to the chain. */
    PUSBDEVICE pFirst = NULL;
    PUSBDEVICE pLast  = NULL;
    VECTOR_OBJ(USBDeviceInfo) vecDevInfo;
    USBDeviceInfo *pInfo;
    int rc;

    VEC_INIT_OBJ(&vecDevInfo, USBDeviceInfo, USBDevInfoCleanup);
    rc = USBSysfsEnumerateHostDevices(&vecDevInfo);
    if (RT_FAILURE(rc))
        return NULL;
    VEC_FOR_EACH(&vecDevInfo, USBDeviceInfo, pInfo)
    {
        USBDEVICE *Dev = (USBDEVICE *)RTMemAllocZ(sizeof(USBDEVICE));
        if (!Dev)
            rc = VERR_NO_MEMORY;
        if (RT_SUCCESS(rc))
        {
            fillInDeviceFromSysfs(Dev, pInfo);
        }
        if (   RT_SUCCESS(rc)
            && Dev->enmState != USBDEVICESTATE_UNSUPPORTED
            && Dev->pszAddress != NULL
           )
        {
            if (pLast != NULL)
            {
                pLast->pNext = Dev;
                pLast = pLast->pNext;
            }
            else
                pFirst = pLast = Dev;
        }
        else
            deviceFree(Dev);
        if (RT_FAILURE(rc))
            break;
    }
    if (RT_FAILURE(rc))
        deviceListFree(&pFirst);

    VEC_CLEANUP_OBJ(&vecDevInfo);
    return pFirst;
#else  /* !VBOX_USB_WITH_SYSFS */
    return NULL;
#endif  /* !VBOX_USB_WITH_SYSFS */
}

PUSBDEVICE USBProxyLinuxGetDevices(const char *pcszUsbfsRoot)
{
    if (pcszUsbfsRoot)
        return getDevicesFromUsbfs(pcszUsbfsRoot);
    else
        return getDevicesFromSysfs();
}
