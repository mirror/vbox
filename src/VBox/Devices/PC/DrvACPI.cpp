/* $Id$ */
/** @file
 * DrvACPI - ACPI Host Driver.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_ACPI

#ifdef RT_OS_WINDOWS
# include <windows.h>
#endif

#include <VBox/pdmdrv.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#ifdef RT_OS_LINUX
# include <iprt/string.h>
# include <sys/types.h>
# include <dirent.h>
# include <stdio.h>
#endif

#ifdef RT_OS_DARWIN
# include <Carbon/Carbon.h>
# include <IOKit/ps/IOPowerSources.h>
# include <IOKit/ps/IOPSKeys.h>
#endif

#ifdef RT_OS_FREEBSD
# include <sys/ioctl.h>
# include <dev/acpica/acpiio.h>
# include <sys/types.h>
# include <sys/sysctl.h>
# include <stdio.h>
# include <errno.h>
# include <fcntl.h>
# include <unistd.h>
#endif

#include "Builtins.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * ACPI driver instance data.
 */
typedef struct DRVACPI
{
    /** The ACPI interface. */
    PDMIACPICONNECTOR   IACPIConnector;
    /** The ACPI port interface. */
    PPDMIACPIPORT       pPort;
    /** Pointer to the driver instance. */
    PPDMDRVINS          pDrvIns;
} DRVACPI, *PDRVACPI;


/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  Any thread.
 */
static DECLCALLBACK(void *) drvACPIQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVACPI pThis = PDMINS_2_DATA(pDrvIns, PDRVACPI);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_ACPI_CONNECTOR:
            return &pThis->IACPIConnector;
        default:
            return NULL;
    }
}

/**
 * Get the current power source of the host system.
 *
 * @returns status code
 * @param   pInterface   Pointer to the interface structure containing the called function pointer.
 * @param   pPowerSource Pointer to the power source result variable.
 */
static DECLCALLBACK(int) drvACPIQueryPowerSource(PPDMIACPICONNECTOR pInterface,
                                                 PDMACPIPOWERSOURCE *pPowerSource)
{
#if defined(RT_OS_WINDOWS)
    SYSTEM_POWER_STATUS powerStatus;
    if (GetSystemPowerStatus(&powerStatus))
    {
        /* running on battery? */
        if (    (powerStatus.ACLineStatus == 0)
             || (powerStatus.ACLineStatus == 255)
             && (powerStatus.BatteryFlag & 15))
        {
            *pPowerSource = PDM_ACPI_POWER_SOURCE_BATTERY;
        }
        /* running on AC link? */
        else if (powerStatus.ACLineStatus == 1)
        {
            *pPowerSource = PDM_ACPI_POWER_SOURCE_OUTLET;
        }
        else
        /* what the hell we're running on? */
        {
            *pPowerSource = PDM_ACPI_POWER_SOURCE_UNKNOWN;
        }
    }
    else
    {
        AssertMsgFailed(("Could not determine system power status, error: 0x%x\n",
                         GetLastError()));
        *pPowerSource = PDM_ACPI_POWER_SOURCE_UNKNOWN;
    }
#elif defined (RT_OS_LINUX) /* !RT_OS_WINDOWS */
    DIR *dfd;
    struct dirent *dp;
    FILE *statusFile = NULL;
    char buff[NAME_MAX+50];

    /* start with no result */
    *pPowerSource = PDM_ACPI_POWER_SOURCE_UNKNOWN;

    dfd = opendir("/proc/acpi/ac_adapter/");
    if (dfd)
    {
        for (;;)
        {
            dp = readdir(dfd);
            if (dp == 0)
                break;
            if (strcmp(dp->d_name, ".") == 0 ||
                strcmp(dp->d_name, "..") == 0)
                continue;
            strcpy(buff, "/proc/acpi/ac_adapter/");
            strcat(buff, dp->d_name);
            strcat(buff, "/status");
            statusFile = fopen(buff, "r");
            /* there's another possible name for this file */
            if (!statusFile)
            {
                strcpy(buff, "/proc/acpi/ac_adapter/");
                strcat(buff, dp->d_name);
                strcat(buff, "/state");
                statusFile = fopen(buff, "r");
            }
            if (statusFile)
                break;
        }
        closedir(dfd);
    }

    if (statusFile)
    {
        for (;;)
        {
            char buff2[1024];
            if (fgets(buff2, sizeof(buff), statusFile) == NULL)
                break;
            if (strstr(buff2, "Status:") != NULL ||
                strstr(buff2, "state:") != NULL)
            {
                if (strstr(buff2, "on-line") != NULL)
                    *pPowerSource = PDM_ACPI_POWER_SOURCE_OUTLET;
                else
                    *pPowerSource = PDM_ACPI_POWER_SOURCE_BATTERY;
            }
        }
        fclose(statusFile);
    }
#elif defined (RT_OS_DARWIN) /* !RT_OS_LINUX */
    *pPowerSource = PDM_ACPI_POWER_SOURCE_UNKNOWN;

    CFTypeRef pBlob = IOPSCopyPowerSourcesInfo();
    CFArrayRef pSources = IOPSCopyPowerSourcesList(pBlob);

    CFDictionaryRef pSource = NULL;
    const void *psValue;
    bool result;

    if (CFArrayGetCount(pSources) > 0)
    {
        for(int i = 0; i < CFArrayGetCount(pSources); ++i)
        {
            pSource = IOPSGetPowerSourceDescription(pBlob, CFArrayGetValueAtIndex(pSources, i));
            /* If the source is empty skip over to the next one. */
            if(!pSource)
                continue;
            /* Skip all power sources which are currently not present like a
             * second battery. */
            if (CFDictionaryGetValue(pSource, CFSTR(kIOPSIsPresentKey)) == kCFBooleanFalse)
                continue;
            /* Only internal power types are of interest. */
            result = CFDictionaryGetValueIfPresent(pSource, CFSTR(kIOPSTransportTypeKey), &psValue);
            if (result &&
                CFStringCompare((CFStringRef)psValue, CFSTR(kIOPSInternalType), 0) == kCFCompareEqualTo)
            {
                /* Check which power source we are connect on. */
                result = CFDictionaryGetValueIfPresent(pSource, CFSTR(kIOPSPowerSourceStateKey), &psValue);
                if (result &&
                    CFStringCompare((CFStringRef)psValue, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo)
                    *pPowerSource = PDM_ACPI_POWER_SOURCE_OUTLET;
                else if (result &&
                         CFStringCompare((CFStringRef)psValue, CFSTR(kIOPSBatteryPowerValue), 0) == kCFCompareEqualTo)
                    *pPowerSource = PDM_ACPI_POWER_SOURCE_BATTERY;
            }
        }
    }
    CFRelease(pBlob);
    CFRelease(pSources);
#elif defined(RT_OS_FREEBSD) /* !RT_OS_DARWIN */
    int fAcLine = 0;
    size_t cbParameter = sizeof(fAcLine);

    int rc = sysctlbyname("hw.acpi.acline", &fAcLine, &cbParameter, NULL, NULL);

    if (!rc)
    {
        if (fAcLine == 1)
            *pPowerSource = PDM_ACPI_POWER_SOURCE_OUTLET;
        else if (fAcLine == 0)
            *pPowerSource = PDM_ACPI_POWER_SOURCE_BATTERY;
        else
            *pPowerSource = PDM_ACPI_POWER_SOURCE_UNKNOWN;
    }
    else
    {
        AssertMsg(errno == ENOENT, ("rc=%d (%s)\n", rc, strerror(errno)));
        *pPowerSource = PDM_ACPI_POWER_SOURCE_OUTLET;
    }
#else /* !RT_OS_FREEBSD either - what could this be? */
    *pPowerSource = PDM_ACPI_POWER_SOURCE_OUTLET;
#endif /* !RT_OS_WINDOWS */
    return VINF_SUCCESS;
}

/**
 * @copydoc PDMIACPICONNECTOR::pfnQueryBatteryStatus
 */
static DECLCALLBACK(int) drvACPIQueryBatteryStatus(PPDMIACPICONNECTOR pInterface, bool *pfPresent,
                                                   PPDMACPIBATCAPACITY penmRemainingCapacity,
                                                   PPDMACPIBATSTATE penmBatteryState,
                                                   uint32_t *pu32PresentRate)
{
    /* default return values for all architectures */
    *pfPresent              = false;   /* no battery present */
    *penmBatteryState       = PDM_ACPI_BAT_STATE_CHARGED;
    *penmRemainingCapacity  = PDM_ACPI_BAT_CAPACITY_UNKNOWN;
    *pu32PresentRate        = ~0;      /* present rate is unknown */

#if defined(RT_OS_WINDOWS)
    SYSTEM_POWER_STATUS powerStatus;
    if (GetSystemPowerStatus(&powerStatus))
    {
        /* 128 means no battery present */
        *pfPresent = !(powerStatus.BatteryFlag & 128);
        /* just forward the value directly */
        *penmRemainingCapacity = (PDMACPIBATCAPACITY)powerStatus.BatteryLifePercent;
        /* we assume that we are discharging the battery if we are not on-line and
         * not charge the battery */
        uint32_t uBs = PDM_ACPI_BAT_STATE_CHARGED;
        if (powerStatus.BatteryFlag & 8)
            uBs = PDM_ACPI_BAT_STATE_CHARGING;
        else if (powerStatus.ACLineStatus == 0 || powerStatus.ACLineStatus == 255)
            uBs = PDM_ACPI_BAT_STATE_DISCHARGING;
        if (powerStatus.BatteryFlag & 4)
            uBs |= PDM_ACPI_BAT_STATE_CRITICAL;
        *penmBatteryState = (PDMACPIBATSTATE)uBs;
        /* on Windows it is difficult to request the present charging/discharging rate */
    }
    else
    {
        AssertMsgFailed(("Could not determine system power status, error: 0x%x\n",
                        GetLastError()));
    }
#elif defined(RT_OS_LINUX)
    DIR *dfd;
    struct dirent *dp;
    FILE *statusFile = NULL;
    FILE *infoFile = NULL;
    char buff[NAME_MAX+50];
    /* the summed up maximum capacity */
    int  maxCapacityTotal = ~0;
    /* the summed up total capacity */
    int  currentCapacityTotal = ~0;
    int  presentRate = 0;
    int  presentRateTotal = 0;
    bool fBatteryPresent = false, fCharging=false, fDischarging=false, fCritical=false;

    dfd = opendir("/proc/acpi/battery/");
    if (dfd)
    {
        for (;;)
        {
            dp = readdir(dfd);
            if (dp == 0)
                break;
            if (strcmp(dp->d_name, ".") == 0 ||
                strcmp(dp->d_name, "..") == 0)
                continue;
            strcpy(buff, "/proc/acpi/battery/");
            strcat(buff, dp->d_name);
            strcat(buff, "/status");
            statusFile = fopen(buff, "r");
            /* there is a 2nd variant of that file */
            if (!statusFile)
            {
                strcpy(buff, "/proc/acpi/battery/");
                strcat(buff, dp->d_name);
                strcat(buff, "/state");
                statusFile = fopen(buff, "r");
            }
            strcpy(buff, "/proc/acpi/battery/");
            strcat(buff, dp->d_name);
            strcat(buff, "/info");
            infoFile = fopen(buff, "r");
            /* we need both files */
            if (!statusFile || !infoFile)
            {
                if (statusFile)
                    fclose(statusFile);
                if (infoFile)
                    fclose(infoFile);
                break;
            }

            /* get 'present' status from the info file */
            for (;;)
            {
                char buff2[1024];
                if (fgets(buff2, sizeof(buff), infoFile) == NULL)
                    break;

                if (strstr(buff2, "present:") != NULL)
                {
                    if (strstr(buff2, "yes") != NULL)
                        fBatteryPresent = true;
                }
            }

            /* move file pointer back to start of file */
            fseek(infoFile, 0, SEEK_SET);

            if (fBatteryPresent)
            {
                /* get the maximum capacity from the info file */
                for (;;)
                {
                    char buff2[1024];
                    int maxCapacity = ~0;
                    if (fgets(buff2, sizeof(buff), infoFile) == NULL)
                        break;
                    if (strstr(buff2, "last full capacity:") != NULL)
                    {
                        if (sscanf(buff2 + 19, "%d", &maxCapacity) <= 0)
                            maxCapacity = ~0;

                        /* did we get a valid capacity and it's the first value we got? */
                        if (maxCapacityTotal < 0 && maxCapacity > 0)
                        {
                            /* take this as the maximum capacity */
                            maxCapacityTotal = maxCapacity;
                        }
                        else
                        {
                            /* sum up the maximum capacity */
                            if (maxCapacityTotal > 0 && maxCapacity > 0)
                                maxCapacityTotal += maxCapacity;
                        }
                        /* we got all we need */
                        break;
                    }
                }

                /* get the current capacity/state from the status file */
                bool gotRemainingCapacity=false, gotBatteryState=false,
                     gotCapacityState=false,     gotPresentRate=false;
                while (!gotRemainingCapacity || !gotBatteryState ||
                       !gotCapacityState     || !gotPresentRate)
                {
                    char buff2[1024];
                    int currentCapacity = ~0;
                    if (fgets(buff2, sizeof(buff), statusFile) == NULL)
                        break;
                    if (strstr(buff2, "remaining capacity:") != NULL)
                    {
                        if (sscanf(buff2 + 19, "%d", &currentCapacity) <= 0)
                            currentCapacity = ~0;

                        /* is this the first valid value we see? If so, take it! */
                        if (currentCapacityTotal < 0 && currentCapacity >= 0)
                        {
                            currentCapacityTotal = currentCapacity;
                        }
                        else
                        {
                            /* just sum up the current value */
                            if (currentCapacityTotal > 0 && currentCapacity > 0)
                                currentCapacityTotal += currentCapacity;
                        }
                        gotRemainingCapacity = true;
                    }
                    if (strstr(buff2, "charging state:") != NULL)
                    {
                        if (strstr(buff2 + 15, "discharging") != NULL)
                            fDischarging = true;
                        else if (strstr(buff2 + 15, "charging") != NULL)
                            fCharging = true;
                        gotBatteryState = true;
                    }
                    if (strstr(buff2, "capacity state:") != NULL)
                    {
                        if (strstr(buff2 + 15, "critical") != NULL)
                            fCritical = true;
                        gotCapacityState = true;
                    }
                    if (strstr(buff2, "present rate:") != NULL)
                    {
                        if (sscanf(buff2 + 13, "%d", &presentRate) <= 0)
                            presentRate = 0;
                        gotPresentRate = true;
                    }
                }
            }

            if (presentRate)
            {
                if (fDischarging)
                    presentRateTotal -= presentRate;
                else
                    presentRateTotal += presentRate;
            }

            if (statusFile)
                fclose(statusFile);
            if (infoFile)
                fclose(infoFile);

        }
        closedir(dfd);
    }

    *pfPresent = fBatteryPresent;

    /* charging/discharging bits are mutual exclusive */
    uint32_t uBs = PDM_ACPI_BAT_STATE_CHARGED;
    if (fDischarging)
        uBs = PDM_ACPI_BAT_STATE_DISCHARGING;
    else if (fCharging)
        uBs = PDM_ACPI_BAT_STATE_CHARGING;
    if (fCritical)
        uBs |= PDM_ACPI_BAT_STATE_CRITICAL;
    *penmBatteryState = (PDMACPIBATSTATE)uBs;

    if (presentRateTotal < 0)
        presentRateTotal = -presentRateTotal;

    if (maxCapacityTotal > 0 && currentCapacityTotal > 0)
    {
        /* calculate the percentage */
        *penmRemainingCapacity = (PDMACPIBATCAPACITY)(((float)currentCapacityTotal / (float)maxCapacityTotal)
                                                      * PDM_ACPI_BAT_CAPACITY_MAX);
        *pu32PresentRate = (uint32_t)(((float)presentRateTotal / (float)maxCapacityTotal) * 1000);
    }
#elif defined(RT_OS_DARWIN)
    CFTypeRef pBlob = IOPSCopyPowerSourcesInfo();
    CFArrayRef pSources = IOPSCopyPowerSourcesList(pBlob);

    CFDictionaryRef pSource = NULL;
    const void *psValue;
    bool result;

    if (CFArrayGetCount(pSources) > 0)
    {
        for(int i = 0; i < CFArrayGetCount(pSources); ++i)
        {
            pSource = IOPSGetPowerSourceDescription(pBlob, CFArrayGetValueAtIndex(pSources, i));
            /* If the source is empty skip over to the next one. */
            if(!pSource)
                continue;
            /* Skip all power sources which are currently not present like a
             * second battery. */
            if (CFDictionaryGetValue(pSource, CFSTR(kIOPSIsPresentKey)) == kCFBooleanFalse)
                continue;
            /* Only internal power types are of interest. */
            result = CFDictionaryGetValueIfPresent(pSource, CFSTR(kIOPSTransportTypeKey), &psValue);
            if (result &&
                CFStringCompare((CFStringRef)psValue, CFSTR(kIOPSInternalType), 0) == kCFCompareEqualTo)
            {
                PDMACPIPOWERSOURCE powerSource = PDM_ACPI_POWER_SOURCE_UNKNOWN;
                /* First check which power source we are connect on. */
                result = CFDictionaryGetValueIfPresent(pSource, CFSTR(kIOPSPowerSourceStateKey), &psValue);
                if (result &&
                    CFStringCompare((CFStringRef)psValue, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo)
                    powerSource = PDM_ACPI_POWER_SOURCE_OUTLET;
                else if (result &&
                         CFStringCompare((CFStringRef)psValue, CFSTR(kIOPSBatteryPowerValue), 0) == kCFCompareEqualTo)
                    powerSource = PDM_ACPI_POWER_SOURCE_BATTERY;

                /* At this point the power source is present. */
                *pfPresent = true;
                *penmBatteryState = PDM_ACPI_BAT_STATE_CHARGED;

                int curCapacity = 0;
                int maxCapacity = 1;
                float remCapacity = 0.0f;

                /* Fetch the current capacity value of the power source */
                result = CFDictionaryGetValueIfPresent(pSource, CFSTR(kIOPSCurrentCapacityKey), &psValue);
                if (result)
                    CFNumberGetValue((CFNumberRef)psValue, kCFNumberSInt32Type, &curCapacity);
                /* Fetch the maximum capacity value of the power source */
                result = CFDictionaryGetValueIfPresent(pSource, CFSTR(kIOPSMaxCapacityKey), &psValue);
                if (result)
                    CFNumberGetValue((CFNumberRef)psValue, kCFNumberSInt32Type, &maxCapacity);

                /* Calculate the remaining capacity in percent */
                remCapacity = ((float)curCapacity/(float)maxCapacity * PDM_ACPI_BAT_CAPACITY_MAX);
                *penmRemainingCapacity = (PDMACPIBATCAPACITY)remCapacity;

                if (powerSource == PDM_ACPI_POWER_SOURCE_BATTERY)
                {
                    /* If we are on battery power we are discharging in every
                     * case */
                    *penmBatteryState = PDM_ACPI_BAT_STATE_DISCHARGING;
                    int timeToEmpty = -1;
                    /* Get the time till the battery source will be empty */
                    result = CFDictionaryGetValueIfPresent(pSource, CFSTR(kIOPSTimeToEmptyKey), &psValue);
                    if (result)
                        CFNumberGetValue((CFNumberRef)psValue, kCFNumberSInt32Type, &timeToEmpty);
                    if (timeToEmpty != -1)
                        /* 0...1000 */
                        *pu32PresentRate = (uint32_t)roundf((remCapacity / ((float)timeToEmpty/60.0)) * 10.0);
                }

                if (powerSource == PDM_ACPI_POWER_SOURCE_OUTLET &&
                    CFDictionaryGetValueIfPresent(pSource, CFSTR(kIOPSIsChargingKey), &psValue))
                {
                    /* We are running on an AC power source, but we also have a
                     * battery power source present. */
                    if (CFBooleanGetValue((CFBooleanRef)psValue) > 0)
                    {
                        /* This means charging. */
                        *penmBatteryState = PDM_ACPI_BAT_STATE_CHARGING;
                        int timeToFull = -1;
                        /* Get the time till the battery source will be charged */
                        result = CFDictionaryGetValueIfPresent(pSource, CFSTR(kIOPSTimeToFullChargeKey), &psValue);
                        if (result)
                            CFNumberGetValue((CFNumberRef)psValue, kCFNumberSInt32Type, &timeToFull);
                        if (timeToFull != -1)
                            /* 0...1000 */
                            *pu32PresentRate = (uint32_t)roundf((100.0-(float)remCapacity) / ((float)timeToFull/60.0)) * 10.0;
                    }
                }

                /* Check for critical */
                int criticalValue = 20;
                result = CFDictionaryGetValueIfPresent(pSource, CFSTR(kIOPSDeadWarnLevelKey), &psValue);
                if (result)
                    CFNumberGetValue((CFNumberRef)psValue, kCFNumberSInt32Type, &criticalValue);
                if (remCapacity < criticalValue)
                    *penmBatteryState = (PDMACPIBATSTATE)(*penmBatteryState | PDM_ACPI_BAT_STATE_CRITICAL);
            }
        }
    }
    CFRelease(pBlob);
    CFRelease(pSources);
#elif defined(RT_OS_FREEBSD)
    /* We try to use /dev/acpi first and if that fails use the sysctls. */
    bool fSuccess = true;
    int FileAcpi = 0;
    int rc = 0;

    FileAcpi = open("/dev/acpi", O_RDONLY);
    if (FileAcpi != -1)
    {
        bool fMilliWatt;
        union acpi_battery_ioctl_arg BatteryIo;

        memset(&BatteryIo, 0, sizeof(BatteryIo));
        BatteryIo.unit = 0; /* Always use the first battery. */

        /* Determine the power units first. */
        if (ioctl(FileAcpi, ACPIIO_BATT_GET_BIF, &BatteryIo) == -1)
            fSuccess = false;
        else
        {
            if (BatteryIo.bif.units == ACPI_BIF_UNITS_MW)
                fMilliWatt = true;
            else
                fMilliWatt = false; /* mA */

            BatteryIo.unit = 0;
            if (ioctl(FileAcpi, ACPIIO_BATT_GET_BATTINFO, &BatteryIo) == -1)
                fSuccess = false;
            else
            {
                if ((BatteryIo.battinfo.state & ACPI_BATT_STAT_NOT_PRESENT) == ACPI_BATT_STAT_NOT_PRESENT)
                    *pfPresent = false;
                else
                {
                    *pfPresent = true;

                    if (BatteryIo.battinfo.state & ACPI_BATT_STAT_DISCHARG)
                        *penmBatteryState = PDM_ACPI_BAT_STATE_DISCHARGING;
                    else if (BatteryIo.battinfo.state & ACPI_BATT_STAT_CHARGING)
                        *penmBatteryState = PDM_ACPI_BAT_STATE_CHARGING;
                    else
                        *penmBatteryState = PDM_ACPI_BAT_STATE_CHARGED;

                    if (BatteryIo.battinfo.state & ACPI_BATT_STAT_CRITICAL)
                        *penmBatteryState = (PDMACPIBATSTATE)(*penmBatteryState | PDM_ACPI_BAT_STATE_CRITICAL);
                }

                if (BatteryIo.battinfo.cap != -1)
                    *penmRemainingCapacity = (PDMACPIBATCAPACITY)BatteryIo.battinfo.cap;

                BatteryIo.unit = 0;
                if (ioctl(FileAcpi, ACPIIO_BATT_GET_BST, &BatteryIo) == 0)
                {
                    /* The rate can be either mW or mA but the ACPI device wants mW. */
                    if (BatteryIo.bst.rate != 0xffffffff)
                    {
                        if (fMilliWatt)
                            *pu32PresentRate = BatteryIo.bst.rate;
                        else if (BatteryIo.bst.volt != 0xffffffff)
                        {
                            /*
                             * The rate is in mA so we have to convert it.
                             * The current power rate can be calculated with P = U * I
                             */
                            *pu32PresentRate = (uint32_t)((((float)BatteryIo.bst.volt/1000.0) * ((float)BatteryIo.bst.rate/1000.0)) * 1000.0);
                        }
                    }
                }
            }
        }

        close(FileAcpi);
    }
    else
        fSuccess = false;

    if (!fSuccess)
    {
        int fBatteryState = 0;
        size_t cbParameter = sizeof(fBatteryState);

        rc = sysctlbyname("hw.acpi.battery.state", &fBatteryState, &cbParameter, NULL, NULL);
        if (!rc)
        {
            if ((fBatteryState & ACPI_BATT_STAT_NOT_PRESENT) == ACPI_BATT_STAT_NOT_PRESENT)
                *pfPresent = false;
            else
            {
                *pfPresent = true;

                if (fBatteryState & ACPI_BATT_STAT_DISCHARG)
                    *penmBatteryState = PDM_ACPI_BAT_STATE_DISCHARGING;
                else if (fBatteryState & ACPI_BATT_STAT_CHARGING)
                    *penmBatteryState = PDM_ACPI_BAT_STATE_CHARGING;
                else
                    *penmBatteryState = PDM_ACPI_BAT_STATE_CHARGED;

                if (fBatteryState & ACPI_BATT_STAT_CRITICAL)
                    *penmBatteryState = (PDMACPIBATSTATE)(*penmBatteryState | PDM_ACPI_BAT_STATE_CRITICAL);

                /* Get battery level. */
                int curCapacity = 0;
                cbParameter = sizeof(curCapacity);
                rc = sysctlbyname("hw.acpi.battery.life", &curCapacity, &cbParameter, NULL, NULL);
                if ((!rc) && (curCapacity >= 0))
                    *penmRemainingCapacity = (PDMACPIBATCAPACITY)curCapacity;

                /* The rate can't be determined with sysctls. */
            }
        }
    }
#endif /* RT_OS_FREEBSD */
    return VINF_SUCCESS;
}

/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvACPIDestruct(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvACPIDestruct\n"));
}

/**
 * Construct an ACPI driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvACPIConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVACPI pThis = PDMINS_2_DATA(pDrvIns, PDRVACPI);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                              = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface            = drvACPIQueryInterface;
    /* IACPIConnector */
    pThis->IACPIConnector.pfnQueryPowerSource   = drvACPIQueryPowerSource;
    pThis->IACPIConnector.pfnQueryBatteryStatus = drvACPIQueryBatteryStatus;

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    /*
     * Check that no-one is attached to us.
     */
    int rc = pDrvIns->pDrvHlp->pfnAttach(pDrvIns, NULL);
    if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Configuration error: Cannot attach drivers to the ACPI driver!\n"));
        return VERR_PDM_DRVINS_NO_ATTACH;
    }

    /*
     * Query the ACPI port interface.
     */
    pThis->pPort = (PPDMIACPIPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase,
                                                                      PDMINTERFACE_ACPI_PORT);
    if (!pThis->pPort)
    {
        AssertMsgFailed(("Configuration error: "
                         "the above device/driver didn't export the ACPI port interface!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    return VINF_SUCCESS;
}


/**
 * ACPI driver registration record.
 */
const PDMDRVREG g_DrvACPI =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "ACPIHost",
    /* pszDescription */
    "ACPI Host Driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_ACPI,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVACPI),
    /* pfnConstruct */
    drvACPIConstruct,
    /* pfnDestruct */
    drvACPIDestruct,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL
};

