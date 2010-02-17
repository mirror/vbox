/* $Id$ */
/** @file
 * VBoxService - Guest Additions CPU Hot Plugging Service.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"

#ifdef RT_OS_LINUX
# include <iprt/linux/sysfs.h>
# include <errno.h> /* For the sysfs API */
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#ifdef RT_OS_LINUX
/** @name Paths to access the CPU device
 * @{
 */
# define SYSFS_ACPI_CPU_PATH    "/sys/devices/LNXSYSTM:00/device:00"
# define SYSFS_CPU_PATH         "/sys/devices/system/cpu"
/** @} */
#endif


#ifdef RT_OS_LINUX
/**
 * Returns the path of the ACPI CPU device with the given core and package ID.
 *
 * @returns VBox status code.
 * @param   ppszPath     Where to store the path.
 * @param   idCpuCore    The core ID of the CPU.
 * @param   idCpuPackage The package ID of the CPU.
 */
static int VBoxServiceCpuHotPlugGetACPIDevicePath(char **ppszPath, uint32_t idCpuCore, uint32_t idCpuPackage)
{
    AssertPtr(ppszPath);

    PRTDIR pDirDevices = NULL;
    int rc = RTDirOpen(&pDirDevices, SYSFS_ACPI_CPU_PATH);  /* could use RTDirOpenFiltered */
    if (RT_SUCCESS(rc))
    {
        /* Search every ACPI0004 container device for LNXCPU devices. */
        RTDIRENTRY DirFolderAcpiContainer;
        bool fFound = false;

        while (   RT_SUCCESS(RTDirRead(pDirDevices, &DirFolderAcpiContainer, NULL))
               && !fFound) /* Assumption that szName has always enough space */
        {
            if (!strncmp(DirFolderAcpiContainer.szName, "ACPI0004", 8))
            {
                char *pszAcpiContainerPath = NULL;
                PRTDIR pDirAcpiContainer = NULL;

                rc = RTStrAPrintf(&pszAcpiContainerPath, "%s/%s", SYSFS_ACPI_CPU_PATH, DirFolderAcpiContainer.szName);
                if (RT_FAILURE(rc))
                    break;

                rc = RTDirOpen(&pDirAcpiContainer, pszAcpiContainerPath);
                if (RT_SUCCESS(rc))
                {
                    RTDIRENTRY DirFolderContent;
                    while (RT_SUCCESS(RTDirRead(pDirAcpiContainer, &DirFolderContent, NULL))) /* Assumption that szName has always enough space */
                    {
                        if (!strncmp(DirFolderContent.szName, "LNXCPU", 6))
                        {
                            /* Get the sysdev */
                            uint32_t idCore    = RTLinuxSysFsReadIntFile(10, "%s/%s/sysdev/topology/core_id",
                                                                         pszAcpiContainerPath, DirFolderContent.szName);
                            uint32_t idPackage = RTLinuxSysFsReadIntFile(10, "%s/%s/sysdev/topology/physical_package_id",
                                                                         pszAcpiContainerPath, DirFolderContent.szName);
                            if (   idCore    == idCpuCore
                                && idPackage == idCpuPackage)
                            {
                                /* Return the path */
                                rc = RTStrAPrintf(ppszPath, "%s/%s", pszAcpiContainerPath, DirFolderContent.szName);
                                fFound = true;
                                break;
                            }
                        }
                    }

                    RTDirClose(pDirAcpiContainer);
                }

                RTStrFree(pszAcpiContainerPath);
            }
        }

        RTDirClose(pDirDevices);
    }

    return rc;
}
#endif /* RT_OS_LINUX */


/** @copydoc VBOXSERVICE::pfnPreInit */
static DECLCALLBACK(int) VBoxServiceCpuHotPlugPreInit(void)
{
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnOption */
static DECLCALLBACK(int) VBoxServiceCpuHotPlugOption(const char **ppszShort, int argc, char **argv, int *pi)
{
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnInit */
static DECLCALLBACK(int) VBoxServiceCpuHotPlugInit(void)
{
    return VINF_SUCCESS;
}


/**
 * Handles VMMDevCpuEventType_Plug.
 *
 * @param   idCpuCore       The CPU core ID.
 * @param   idCpuPackage    The CPU package ID.
 */
static void VBoxServiceCpuHotPlugHandlePlugEvent(uint32_t idCpuCore, uint32_t idCpuPackage)
{
#ifdef RT_OS_LINUX
    /*
     * The topology directory (containing the physical and core id properties)
     * is not available until the CPU is online. So we just iterate over all directories
     * and enable every CPU which is not online already.
     * Because the directory might not be available immediately we try a few times.
     *
     * @todo: Maybe use udev to monitor hot-add events from the kernel
     */
    bool fCpuOnline = false;
    unsigned cTries = 5;

    do
    {
        PRTDIR pDirDevices = NULL;
        int rc = RTDirOpen(&pDirDevices, SYSFS_CPU_PATH);
        if (RT_SUCCESS(rc))
        {
            RTDIRENTRY DirFolderContent;
            while (RT_SUCCESS(RTDirRead(pDirDevices, &DirFolderContent, NULL))) /* Assumption that szName has always enough space */
            {
                /** @todo r-bird: This code is bringing all CPUs online; the idCpuCore and
                 *        idCpuPackage parameters are unused!   
                 *        aeichner: These files are not available at this point unfortunately. (see comment above) 
                 */
                /*
                 * Check if this is a CPU object.
                 * cpu0 is excluded because it is not possible to change the state
                 * of the first CPU on Linux (it doesn't even have an online file)
                 * and cpuidle is no CPU device. Prevents error messages later.
                 */
                if(   !strncmp(DirFolderContent.szName, "cpu", 3)
                    && strncmp(DirFolderContent.szName, "cpu0", 4)
                    && strncmp(DirFolderContent.szName, "cpuidle", 7))
                {
                    /* Get the sysdev */
                    RTFILE hFileCpuOnline = NIL_RTFILE;

                    rc = RTFileOpenF(&hFileCpuOnline, RTFILE_O_WRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE,
                                     "%s/%s/online", SYSFS_CPU_PATH, DirFolderContent.szName);
                    if (RT_SUCCESS(rc))
                    {
                        /* Write a 1 to online the CPU */
                        rc = RTFileWrite(hFileCpuOnline, "1", 1, NULL);
                        RTFileClose(hFileCpuOnline);
                        if (RT_SUCCESS(rc))
                        {
                            VBoxServiceVerbose(1, "CpuHotPlug: CPU %u/%u was brought online\n", idCpuPackage, idCpuCore);
                            fCpuOnline = true;
                            break;
                        }
                        /* Error means CPU not present or online already  */
                    }
                    else
                        VBoxServiceError("CpuHotPlug: Failed to open \"%s/%s/online\" rc=%Rrc\n",
                                         SYSFS_CPU_PATH, DirFolderContent.szName, rc);
                }
            }
        }
        else
            VBoxServiceError("CpuHotPlug: Failed to open path %s rc=%Rrc\n", SYSFS_CPU_PATH, rc);

        /* Sleep a bit */
        if (!fCpuOnline)
            RTThreadSleep(10);

    } while (   !fCpuOnline
             && cTries-- > 0);
#else
# error "Port me"
#endif
}


/**
 * Handles VMMDevCpuEventType_Unplug.
 *
 * @param   idCpuCore       The CPU core ID.
 * @param   idCpuPackage    The CPU package ID.
 */
static void VBoxServiceCpuHotPlugHandleUnplugEvent(uint32_t idCpuCore, uint32_t idCpuPackage)
{
#ifdef RT_OS_LINUX
    char *pszCpuDevicePath = NULL;
    int rc = VBoxServiceCpuHotPlugGetACPIDevicePath(&pszCpuDevicePath, idCpuCore, idCpuPackage);
    if (RT_SUCCESS(rc))
    {
        RTFILE hFileCpuEject;
        rc = RTFileOpenF(&hFileCpuEject, RTFILE_O_WRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE,
                         "%s/eject", pszCpuDevicePath);
        if (RT_SUCCESS(rc))
        {
            /* Write a 1 to eject the CPU */
            rc = RTFileWrite(hFileCpuEject, "1", 1, NULL);
            if (RT_SUCCESS(rc))
                VBoxServiceVerbose(1, "CpuHotPlug: CPU %u/%u was ejected\n", idCpuPackage, idCpuCore);
            else
                VBoxServiceError("CpuHotPlug: Failed to eject CPU %u/%u rc=%Rrc\n", idCpuPackage, idCpuCore, rc);

            RTFileClose(hFileCpuEject);
        }
        else
            VBoxServiceError("CpuHotPlug: Failed to open \"%s/eject\" rc=%Rrc\n", pszCpuDevicePath, rc);
        RTStrFree(pszCpuDevicePath);
    }
    else
        VBoxServiceError("CpuHotPlug: Failed to get CPU device path rc=%Rrc\n", rc);
#else
# error "Port me"
#endif
}


/** @copydoc VBOXSERVICE::pfnWorker */
DECLCALLBACK(int) VBoxServiceCpuHotPlugWorker(bool volatile *pfShutdown)
{
    /*
     * Tell the control thread that it can continue spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

    /*
     * Enable the CPU hotplug notifier.
     */
    int rc = VbglR3CpuHotPlugInit();
    if (RT_FAILURE(rc))
        return rc;

    /*
     * The Work Loop.
     */
    for (;;)
    {
        /* Wait for CPU hot plugging event. */
        uint32_t            idCpuCore;
        uint32_t            idCpuPackage;
        VMMDevCpuEventType  enmEventType;
        rc = VbglR3CpuHotPlugWaitForEvent(&enmEventType, &idCpuCore, &idCpuPackage);
        if (RT_SUCCESS(rc))
        {
            VBoxServiceVerbose(3, "CpuHotPlug: Event happened idCpuCore=%u idCpuPackage=%u enmEventType=%d\n",
                               idCpuCore, idCpuPackage, enmEventType);
            switch (enmEventType)
            {
                case VMMDevCpuEventType_Plug:
                    VBoxServiceCpuHotPlugHandlePlugEvent(idCpuCore, idCpuPackage);
                    break;

                case VMMDevCpuEventType_Unplug:
                    VBoxServiceCpuHotPlugHandleUnplugEvent(idCpuCore, idCpuPackage);
                    break;

                default:
                {
                    static uint32_t s_iErrors = 0;
                    if (s_iErrors++ < 10)
                        VBoxServiceError("CpuHotPlug: Unknown event: idCpuCore=%u idCpuPackage=%u enmEventType=%d\n",
                                         idCpuCore, idCpuPackage, enmEventType);
                    break;
                }
            }
        }
        else if (rc != VERR_INTERRUPTED && rc != VERR_TRY_AGAIN)
        {
            VBoxServiceError("CpuHotPlug: VbglR3CpuHotPlugWaitForEvent returned %Rrc\n", rc);
            break;
        }

        if (*pfShutdown)
            break;
    }

    VbglR3CpuHotPlugTerm();
    return rc;
}


/** @copydoc VBOXSERVICE::pfnStop */
static DECLCALLBACK(void) VBoxServiceCpuHotPlugStop(void)
{
    VbglR3InterruptEventWaits();
    return;
}


/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServiceCpuHotPlugTerm(void)
{
    return;
}


/**
 * The 'timesync' service description.
 */
VBOXSERVICE g_CpuHotPlug =
{
    /* pszName. */
    "cpuhotplug",
    /* pszDescription. */
    "CPU hot plugging monitor",
    /* pszUsage. */
    NULL,
    /* pszOptions. */
    NULL,
    /* methods */
    VBoxServiceCpuHotPlugPreInit,
    VBoxServiceCpuHotPlugOption,
    VBoxServiceCpuHotPlugInit,
    VBoxServiceCpuHotPlugWorker,
    VBoxServiceCpuHotPlugStop,
    VBoxServiceCpuHotPlugTerm
};

