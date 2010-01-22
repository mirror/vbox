/* $Id$ */
/** @file
 * VBoxService - Guest Additions Cpu Hotplug Service.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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
#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/file.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/dir.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"

#ifdef RT_OS_LINUX
# include <iprt/linux/sysfs.h>
# include <errno.h> /* For the sysfs API */
#endif

/**
 * Paths to access the CPU device
 */
#ifdef RT_OS_LINUX
# define SYSFS_ACPI_CPU_PATH "/sys/devices/LNXSYSTM:00/device:00"
# define SYSFS_CPU_PATH "/sys/devices/system/cpu"
#endif

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/

#ifdef RT_OS_LINUX
/**
 * Returns the path of the ACPI CPU device with the given core and package ID.
 *
 * @returns VBox status code.
 * @param   ppszPath     Where to store the path.
 * @param   idCpuCore    The core ID of the CPU.
 * @param   idCpuPackage The package ID of the CPU.
 */
static int cpuHotplugGetACPIDevicePath(char **ppszPath, uint32_t idCpuCore, uint32_t idCpuPackage)
{
    int rc = VINF_SUCCESS;
    PRTDIR pDirDevices = NULL;

    AssertPtrReturn(ppszPath, VERR_INVALID_PARAMETER);

    rc = RTDirOpen(&pDirDevices, SYSFS_ACPI_CPU_PATH);  /*could use RTDirOpenFiltered*/

    if (RT_SUCCESS(rc))
    {
        RTDIRENTRY DirFolderContent;

        while (RT_SUCCESS(RTDirRead(pDirDevices, &DirFolderContent, NULL))) /* Assumption that szName has always enough space */
        {
            if (!RTStrNCmp(DirFolderContent.szName, "LNXCPU", 6))
            {
                char *pszSysDevPath = NULL;

                /* Get the sysdev */
                rc = RTStrAPrintf(&pszSysDevPath, "%s/%s", SYSFS_ACPI_CPU_PATH, DirFolderContent.szName);
                if (RT_SUCCESS(rc))
                {
                    uint32_t idCore    = RTLinuxSysFsReadIntFile(10, "%s/sysdev/topology/core_id", pszSysDevPath);
                    uint32_t idPackage = RTLinuxSysFsReadIntFile(10, "%s/sysdev/topology/physical_package_id", pszSysDevPath);

                    if (   (idCore == idCpuCore)
                        && (idPackage == idCpuPackage))
                    {
                        /* Return the path */
                        *ppszPath = pszSysDevPath;
                        break;
                    }
                    RTStrFree(pszSysDevPath);
                }
                else
                    break;
            }
        }

        RTDirClose(pDirDevices);
    }

    return rc;
}
#endif

/** @copydoc VBOXSERVICE::pfnPreInit */
static DECLCALLBACK(int) VBoxServiceCpuHotplugPreInit(void)
{
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnOption */
static DECLCALLBACK(int) VBoxServiceCpuHotplugOption(const char **ppszShort, int argc, char **argv, int *pi)
{
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnInit */
static DECLCALLBACK(int) VBoxServiceCpuHotplugInit(void)
{
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnWorker */
DECLCALLBACK(int) VBoxServiceCpuHotplugWorker(bool volatile *pfShutdown)
{
    int rc = VINF_SUCCESS;

    /*
     * Tell the control thread that it can continue spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

    /*
     * Enable the CPU hotplug notifier.
     */
    rc = VbglR3CpuHotplugInit();
    if (RT_FAILURE(rc))
        return rc;

    /*
     * The Work Loop.
     */
    for (;;)
    {
        uint32_t idCpuCore    = UINT32_MAX;
        uint32_t idCpuPackage = UINT32_MAX;
        VMMDevCpuEventType enmEventType = VMMDevCpuEventType_None;

        /* Wait for CPU hotplug event. */
        rc = VbglR3CpuHotplugWaitForEvent(&enmEventType, &idCpuCore, &idCpuPackage);
        if (RT_FAILURE(rc) && (rc != VERR_INTERRUPTED))
            break;

        VBoxServiceVerbose(3, "CPUHotplug: Event happened idCpuCore=%u idCpuPackage=%u enmEventType=%d\n", idCpuCore, idCpuPackage, enmEventType);

        if (enmEventType == VMMDevCpuEventType_Plug)
        {
#ifdef RT_OS_LINUX
            /*
             * The topology directory is not available until the CPU is online. So we just iterate over all directories
             * and enable every CPU which is not online already.
             */
            /** @todo Maybe use udev to monitor events from the kernel */
            PRTDIR pDirDevices = NULL;

            rc = RTDirOpen(&pDirDevices, SYSFS_CPU_PATH);  /*could use RTDirOpenFiltered*/

            if(RT_SUCCESS(rc))
            {
                RTDIRENTRY DirFolderContent;

                while(RT_SUCCESS(RTDirRead(pDirDevices, &DirFolderContent, NULL))) /* Assumption that szName has always enough space */
                {
                    /** Check if this is a CPU object.
                     *  cpu0 is excluded because it is not possbile
                     *  to change the state of the first CPU
                     *  (it doesn't even have an online file)
                     *  and cpuidle is no CPU device.
                     *  Prevents error messages later.
                     */
                    if(   !RTStrNCmp(DirFolderContent.szName, "cpu", 3)
                        && RTStrNCmp(DirFolderContent.szName, "cpu0", 4)
                        && RTStrNCmp(DirFolderContent.szName, "cpuidle", 7))
                    {
                        char *pszSysDevPath = NULL;

                        /* Get the sysdev */
                        rc = RTStrAPrintf(&pszSysDevPath, "%s/%s/online", SYSFS_CPU_PATH, DirFolderContent.szName);
                        if (RT_SUCCESS(rc))
                        {
                            RTFILE FileCpuOnline = NIL_RTFILE;

                            rc = RTFileOpen(&FileCpuOnline, pszSysDevPath, RTFILE_O_WRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
                            if (RT_SUCCESS(rc))
                            {
                                /* Write a 1 to online the CPU */
                                rc = RTFileWrite(FileCpuOnline, "1", 1, NULL);
                                if (RT_SUCCESS(rc))
                                {
                                    VBoxServiceVerbose(1, "CPUHotplug: CPU %u/%u was brought online\n", idCpuPackage, idCpuCore);
                                    RTFileClose(FileCpuOnline);
                                    break;
                                }
                                /* Error means CPU not present or online already  */

                                RTFileClose(FileCpuOnline);
                            }
                            else
                                VBoxServiceError("CPUHotplug: Failed to open online path %s rc=%Rrc\n", pszSysDevPath, rc);

                            RTStrFree(pszSysDevPath);
                        }
                    }
                }
            }
            else
                VBoxServiceError("CPUHotplug: Failed to open path %s rc=%Rrc\n", SYSFS_CPU_PATH, rc);
#else
# error "Port me"
#endif
        }
        else if (enmEventType == VMMDevCpuEventType_Unplug)
        {
#ifdef RT_OS_LINUX
            char *pszCpuDevicePath = NULL;

            rc = cpuHotplugGetACPIDevicePath(&pszCpuDevicePath, idCpuCore, idCpuPackage);
            if (RT_SUCCESS(rc))
            {
                char *pszPathEject = NULL;

                rc = RTStrAPrintf(&pszPathEject, "%s/eject", pszCpuDevicePath);
                if (RT_SUCCESS(rc))
                {
                    RTFILE FileCpuEject = NIL_RTFILE;

                    rc = RTFileOpen(&FileCpuEject, pszPathEject, RTFILE_O_WRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
                    if (RT_SUCCESS(rc))
                    {
                        /* Write a 1 to eject the CPU */
                        rc = RTFileWrite(FileCpuEject, "1", 1, NULL);
                        if (RT_SUCCESS(rc))
                            VBoxServiceVerbose(1, "CPUHotplug: CPU %u/%u was ejected\n", idCpuPackage, idCpuCore);
                        else
                            VBoxServiceError("CPUHotplug: Failed to eject CPU %u/%u rc=%Rrc\n", idCpuPackage, idCpuCore, rc);

                        RTFileClose(FileCpuEject);
                    }
                    else
                        VBoxServiceError("CPUHotplug: Failed to open eject path %s rc=%Rrc\n", pszPathEject, rc);
                }
                else
                    VBoxServiceError("CPUHotplug: Failed to allocate eject path rc=%Rrc\n", rc);

                RTStrFree(pszCpuDevicePath);
            }
            else
                VBoxServiceError("CPUHotplug: Failed to get CPU device path rc=%Rrc\n", rc);
#else
# error "Port me"
#endif
        }
        /* Ignore invalid values. */

        if (*pfShutdown)
            break;
    }

    VbglR3CpuHotplugTerm();
    return rc;
}


/** @copydoc VBOXSERVICE::pfnStop */
static DECLCALLBACK(void) VBoxServiceCpuHotplugStop(void)
{
    VbglR3InterruptEventWaits();
    return;
}


/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServiceCpuHotplugTerm(void)
{
    return;
}


/**
 * The 'timesync' service description.
 */
VBOXSERVICE g_CpuHotplug =
{
    /* pszName. */
    "cpuhotplug",
    /* pszDescription. */
    "CPU hotplug monitor",
    /* pszUsage. */
    NULL,
    /* pszOptions. */
    NULL,
    /* methods */
    VBoxServiceCpuHotplugPreInit,
    VBoxServiceCpuHotplugOption,
    VBoxServiceCpuHotplugInit,
    VBoxServiceCpuHotplugWorker,
    VBoxServiceCpuHotplugStop,
    VBoxServiceCpuHotplugTerm
};

