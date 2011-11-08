/* $Id$ */
/** @file
 * VBoxPci - PCI Driver (Host), Linux Specific Code.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#include "the-linux-kernel.h"
#include "version-generated.h"
#include "product-generated.h"

#define LOG_GROUP LOG_GROUP_DEV_PCI_RAW
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/process.h>
#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/mem.h>

#include "VBoxPciInternal.h"

#ifdef VBOX_WITH_IOMMU
# include <linux/dmar.h>
# include <linux/intel-iommu.h>
# include <linux/pci.h>
# if LINUX_VERSION_CODE < KERNEL_VERSION(3, 1, 0)
#  include <asm/amd_iommu.h>
# else
#  include <linux/amd-iommu.h>
# endif
# if LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)
#  define IOMMU_PRESENT()      iommu_found()
#  define IOMMU_DOMAIN_ALLOC() iommu_domain_alloc()
# else
#  define IOMMU_PRESENT()      iommu_present(&pci_bus_type)
#  define IOMMU_DOMAIN_ALLOC() iommu_domain_alloc(&pci_bus_type)
# endif
#endif /* VBOX_WITH_IOMMU */


/*******************************************************************************
 *   Internal Functions                                                         *
 *******************************************************************************/
static int  VBoxPciLinuxInit(void);
static void VBoxPciLinuxUnload(void);

/*******************************************************************************
 *   Global Variables                                                           *
 *******************************************************************************/
static VBOXRAWPCIGLOBALS g_VBoxPciGlobals;

module_init(VBoxPciLinuxInit);
module_exit(VBoxPciLinuxUnload);

MODULE_AUTHOR(VBOX_VENDOR);
MODULE_DESCRIPTION(VBOX_PRODUCT " PCI access Driver");
MODULE_LICENSE("GPL");
#ifdef MODULE_VERSION
MODULE_VERSION(VBOX_VERSION_STRING);
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
# define PCI_DEV_GET(v,d,p)            pci_get_device(v,d,p)
# define PCI_DEV_PUT(x)                pci_dev_put(x)
# define PCI_DEV_GET_SLOT(bus, devfn)  pci_get_bus_and_slot(bus, devfn)
#else
# define PCI_DEV_GET(v,d,p)            pci_find_device(v,d,p)
# define PCI_DEV_PUT(x)                do {} while(0)
# define PCI_DEV_GET_SLOT(bus, devfn)  pci_find_slot(bus, devfn)
#endif

/**
 * Name of module used to attach to the host PCI device, when
 * PCI device passthrough is used.
 */
#define PCI_STUB_MODULE      "pci-stub"
/* For some reasons my kernel names module for find_module() this way,
 * while device name seems to be above one.
 */
#define PCI_STUB_MODULE_NAME "pci_stub"

/**
 * Our driver name.
 */
#define DRIVER_NAME      "vboxpci"

/**
 * Initialize module.
 *
 * @returns appropriate status code.
 */
static int __init VBoxPciLinuxInit(void)
{
    int rc;
    /*
     * Initialize IPRT.
     */
    rc = RTR0Init(0);

    if (RT_FAILURE(rc))
        goto error;


    LogRel(("VBoxPciLinuxInit\n"));

    RT_ZERO(g_VBoxPciGlobals);

    rc = vboxPciInit(&g_VBoxPciGlobals);
    if (RT_FAILURE(rc))
    {
        LogRel(("cannot do VBoxPciInit: %Rc\n", rc));
        goto error;
    }

#if defined(CONFIG_PCI_STUB)
    /* nothing to do, pci_stub module part of the kernel */
    g_VBoxPciGlobals.fPciStubModuleAvail = true;

#elif defined(CONFIG_PCI_STUB_MODULE)
    if (request_module(PCI_STUB_MODULE) == 0)
    {
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
        /* find_module() is static before Linux 2.6.30 */
        g_VBoxPciGlobals.pciStubModule = find_module(PCI_STUB_MODULE_NAME);
        if (g_VBoxPciGlobals.pciStubModule)
        {
            if (try_module_get(g_VBoxPciGlobals.pciStubModule))
                g_VBoxPciGlobals.fPciStubModuleAvail = true;
        }
        else
            printk(KERN_INFO "vboxpci: find_module %s failed\n", PCI_STUB_MODULE);
# endif
    }
    else
        printk(KERN_INFO "vboxpci: cannot load %s\n", PCI_STUB_MODULE);

#else
    printk(KERN_INFO "vboxpci: %s module not available, cannot detach PCI devices\n",
                      PCI_STUB_MODULE);
#endif

#ifdef VBOX_WITH_IOMMU
    if (IOMMU_PRESENT())
        printk(KERN_INFO "vboxpci: IOMMU found\n");
    else
        printk(KERN_INFO "vboxpci: IOMMU not found (not registered)\n");
#else
    printk(KERN_INFO "vboxpci: IOMMU not found (not compiled)\n");
#endif

    return 0;

  error:
    return -RTErrConvertToErrno(rc);
}

/**
 * Unload the module.
 */
static void __exit VBoxPciLinuxUnload(void)
{
    LogRel(("VBoxPciLinuxLinuxUnload\n"));

    /*
     * Undo the work done during start (in reverse order).
     */
    vboxPciShutdown(&g_VBoxPciGlobals);

    RTR0Term();

    if (g_VBoxPciGlobals.pciStubModule)
    {
        module_put(g_VBoxPciGlobals.pciStubModule);
        g_VBoxPciGlobals.pciStubModule = NULL;
    }

    Log(("VBoxPciLinuxUnload - done\n"));
}

int vboxPciOsDevRegisterWithIommu(PVBOXRAWPCIINS pIns)
{
#ifdef VBOX_WITH_IOMMU
    int rc;
    int status;
    PVBOXRAWPCIDRVVM pData = VBOX_DRV_VMDATA(pIns);

    if (!pData)
    {
        printk(KERN_DEBUG "vboxpci: VM data not initialized (attach)\n");
        return VERR_INVALID_PARAMETER;
    }

    if (!pData->pIommuDomain)
    {
        printk(KERN_DEBUG "vboxpci: No IOMMU domain (attach)\n");
        return VERR_NOT_FOUND;
    }

    status = iommu_attach_device(pData->pIommuDomain, &pIns->pPciDev->dev);
    if (status == 0)
    {
        printk(KERN_DEBUG "vboxpci: iommu_attach_device() success\n");
        pIns->fIommuUsed = true;
        rc = VINF_SUCCESS;;
    }
    else
    {
        printk(KERN_DEBUG "vboxpci: iommu_attach_device() failed\n");
        rc = VERR_INTERNAL_ERROR;
    }

    /* @todo: KVM checks IOMMU_CAP_CACHE_COHERENCY and sets
       flag IOMMU_CACHE later used when mapping physical
       addresses, which could improve performance. */

    return rc;
#else
    return VERR_NOT_SUPPORTED;
#endif
}

int vboxPciOsDevUnregisterWithIommu(PVBOXRAWPCIINS pIns)
{
#ifdef VBOX_WITH_IOMMU
    int rc = VINF_SUCCESS;
    PVBOXRAWPCIDRVVM pData = VBOX_DRV_VMDATA(pIns);

    if (!pData)
    {
        printk(KERN_DEBUG "vboxpci: VM data not inited (detach)\n");
        return VERR_INVALID_PARAMETER;
    }

    if (!pData->pIommuDomain)
    {
        printk(KERN_DEBUG "vboxpci: No IOMMU domain (detach)\n");
        return VERR_NOT_FOUND;
    }

    if (pIns->fIommuUsed)
    {
        iommu_detach_device(pData->pIommuDomain, &pIns->pPciDev->dev);
        printk(KERN_DEBUG "vboxpci: iommu_detach_device()\n");
        pIns->fIommuUsed = false;
    }

    return rc;
#else
    return VERR_NOT_SUPPORTED;
#endif
}

int vboxPciOsDevReset(PVBOXRAWPCIINS pIns)
{
    int rc = VINF_SUCCESS;

    if (pIns->pPciDev)
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28)
        if (pci_reset_function(pIns->pPciDev))
        {
            printk(KERN_DEBUG "vboxpci: pci_reset_function() failed\n");
            rc = VERR_INTERNAL_ERROR;
        }
#else
        rc = VERR_NOT_SUPPORTED;
#endif
    }

    return rc;
}

static struct file* vboxPciFileOpen(const char* path, int flags)
{
    struct file* filp = NULL;
    int err = 0;

    filp = filp_open(path, flags, 0);

    if (IS_ERR(filp))
    {
        err = PTR_ERR(filp);
        printk(KERN_DEBUG "vboxPciFileOpen: error %d\n", err);
        return NULL;
    }

    if (!filp->f_op || !filp->f_op->write)
    {
        printk(KERN_DEBUG "Not writable FS\n");
        filp_close(filp, NULL);
        return NULL;
    }

    return filp;
}

static void  vboxPciFileClose(struct file* file)
{
    filp_close(file, NULL);
}

static int vboxPciFileWrite(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size)
{
    int          ret;
    mm_segment_t fs_save;

    fs_save = get_fs();
    set_fs(get_ds());
    ret = vfs_write(file, data, size, &offset);
    set_fs(fs_save);
    if (ret < 0)
        printk(KERN_DEBUG "vboxPciFileWrite: error %d\n", ret);

    return ret;
}

#if 0
static int vboxPciFileRead(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size)
{
    int ret;
    mm_segment_t fs_save;

    fs_save = get_fs();
    set_fs(get_ds());
    ret = vfs_read(file, data, size, &offset);
    set_fs(fs_save);

    return ret;
}
#endif

int vboxPciOsDevDetachHostDriver(PVBOXRAWPCIINS pIns)
{
    struct pci_dev *pPciDev = NULL;
    uint8_t uBus =   (pIns->HostPciAddress) >> 8;
    uint8_t uDevFn = (pIns->HostPciAddress) & 0xff;
    const char* currentDriver;
    uint16_t uVendor, uDevice;
    int fDetach = 0;

    if (!g_VBoxPciGlobals.fPciStubModuleAvail)
    {
        printk(KERN_INFO "vboxpci: stub module %s not detected: cannot detach\n",
               PCI_STUB_MODULE);
        return VERR_ACCESS_DENIED;
    }

    pPciDev = PCI_DEV_GET_SLOT(uBus, uDevFn);

    if (!pPciDev)
    {
        printk(KERN_INFO "vboxpci: device at %02x:%02x.%d not found\n",
               uBus, uDevFn>>3, uDevFn&7);
        return VERR_NOT_FOUND;
    }

    uVendor = pPciDev->vendor;
    uDevice = pPciDev->device;

    currentDriver = pPciDev->driver ? pPciDev->driver->name : NULL;

    printk(KERN_DEBUG "vboxpci: detected device: %04x:%04x at %02x:%02x.%d, driver %s\n",
           uVendor, uDevice, uBus, uDevFn>>3, uDevFn&7,
           currentDriver ? currentDriver : "<none>");

    fDetach = (currentDriver == NULL  || (strcmp(currentDriver, PCI_STUB_MODULE) != 0)) ? 1 : 0;

    /* Init previous driver data. */
    pIns->szPrevDriver[0] = '\0';

    if (fDetach && currentDriver)
    {
        /* Dangerous: if device name for some reasons contains slashes - arbitrary file could be written to. */
        if (strchr(currentDriver, '/') != 0)
        {
            printk(KERN_DEBUG "vboxpci: ERROR: %s contains invalid symbols\n", currentDriver);
            return VERR_ACCESS_DENIED;
        }
        /** @todo: RTStrCopy not exported. */
        strncpy(pIns->szPrevDriver, currentDriver, sizeof(pIns->szPrevDriver));
    }

    PCI_DEV_PUT(pPciDev);
    pPciDev = NULL;

    if (fDetach)
    {
        char*              szCmdBuf;
        char*              szFileBuf;
        struct file*       pFile;
        int                iCmdLen;
        const int          cMaxBuf = 128;
        const struct cred *pOldCreds;
        struct cred       *pNewCreds;

        /*
         * Now perform kernel analog of:
         *
         * echo -n "10de 040a" > /sys/bus/pci/drivers/pci-stub/new_id
         * echo -n 0000:03:00.0 > /sys/bus/pci/drivers/nvidia/unbind
         * echo -n 0000:03:00.0 > /sys/bus/pci/drivers/pci-stub/bind
         *
         * We do this way, as this interface is presumingly more stable than
         * in-kernel ones.
         */
        szCmdBuf  = kmalloc(cMaxBuf, GFP_KERNEL);
        szFileBuf = kmalloc(cMaxBuf, GFP_KERNEL);
        if (!szCmdBuf || !szFileBuf)
            goto done;

        /* Somewhat ugly hack - override current credentials */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
        pNewCreds = prepare_creds();
        if (!pNewCreds)
                goto done;

        pNewCreds->fsuid = 0;
        pOldCreds = override_creds(pNewCreds);
#endif

        RTStrPrintf(szFileBuf, cMaxBuf,
                              "/sys/bus/pci/drivers/%s/new_id",
                              PCI_STUB_MODULE);
        pFile = vboxPciFileOpen(szFileBuf, O_WRONLY);
        if (pFile)
        {
            iCmdLen = RTStrPrintf(szCmdBuf, cMaxBuf,
                                  "%04x %04x",
                                  uVendor, uDevice);
            /* Don't write trailing \0 */
            vboxPciFileWrite(pFile, 0, szCmdBuf, iCmdLen);
            vboxPciFileClose(pFile);
        }
        else
            printk(KERN_DEBUG "vboxpci: cannot open %s\n", szFileBuf);

        iCmdLen = RTStrPrintf(szCmdBuf, cMaxBuf,
                              "0000:%02x:%02x.%d",
                              uBus, uDevFn>>3, uDevFn&7);

        /* Unbind if bound to smth */
        if (pIns->szPrevDriver[0])
        {
            RTStrPrintf(szFileBuf, cMaxBuf,
                        "/sys/bus/pci/drivers/%s/unbind",
                         pIns->szPrevDriver);
            pFile = vboxPciFileOpen(szFileBuf, O_WRONLY);
            if (pFile)
            {

                /* Don't write trailing \0 */
                vboxPciFileWrite(pFile, 0, szCmdBuf, iCmdLen);
                vboxPciFileClose(pFile);
            }
            else
                printk(KERN_DEBUG "vboxpci: cannot open %s\n", szFileBuf);
        }

        RTStrPrintf(szFileBuf, cMaxBuf,
                    "/sys/bus/pci/drivers/%s/bind",
                    PCI_STUB_MODULE);
        pFile = vboxPciFileOpen(szFileBuf, O_WRONLY);
        if (pFile)
        {
            /* Don't write trailing \0 */
            vboxPciFileWrite(pFile, 0, szCmdBuf, iCmdLen);
            vboxPciFileClose(pFile);
        }
        else
            printk(KERN_DEBUG "vboxpci: cannot open %s\n", szFileBuf);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
        revert_creds(pOldCreds);
        put_cred(pNewCreds);
#endif

      done:
        kfree(szCmdBuf);
        kfree(szFileBuf);
    }

    return 0;
}

int vboxPciOsDevReattachHostDriver(PVBOXRAWPCIINS pIns)
{
    struct pci_dev *pPciDev = pIns->pPciDev;

    if (!pPciDev)
        return VINF_SUCCESS;

    if (pIns->szPrevDriver[0])
    {
        char*              szCmdBuf;
        char*              szFileBuf;
        struct file*       pFile;
        int                iCmdLen;
        const int          cMaxBuf = 128;
        const struct cred *pOldCreds;
        struct cred       *pNewCreds;
        uint8_t            uBus =   (pIns->HostPciAddress) >> 8;
        uint8_t            uDevFn = (pIns->HostPciAddress) & 0xff;

        printk(KERN_DEBUG "vboxpci: reattaching old host driver %s\n", pIns->szPrevDriver);
        /*
         * Now perform kernel analog of:
         *
         * echo -n 0000:03:00.0 > /sys/bus/pci/drivers/pci-stub/unbind
         * echo -n 0000:03:00.0 > /sys/bus/pci/drivers/nvidia/bind
         */
        szCmdBuf  = kmalloc(cMaxBuf, GFP_KERNEL);
        szFileBuf = kmalloc(cMaxBuf, GFP_KERNEL);

        if (!szCmdBuf || !szFileBuf)
            goto done;

        iCmdLen = RTStrPrintf(szCmdBuf, cMaxBuf,
                              "0000:%02x:%02x.%d",
                              uBus, uDevFn>>3, uDevFn&7);

        /* Somewhat ugly hack - override current credentials */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
        pNewCreds = prepare_creds();
        if (!pNewCreds)
            goto done;

        pNewCreds->fsuid = 0;
        pOldCreds = override_creds(pNewCreds);
#endif
        RTStrPrintf(szFileBuf, cMaxBuf,
                    "/sys/bus/pci/drivers/%s/unbind",
                    PCI_STUB_MODULE);
        pFile = vboxPciFileOpen(szFileBuf, O_WRONLY);
        if (pFile)
        {

            /* Don't write trailing \0 */
            vboxPciFileWrite(pFile, 0, szCmdBuf, iCmdLen);
            vboxPciFileClose(pFile);
        }
        else
            printk(KERN_DEBUG "vboxpci: cannot open %s\n", szFileBuf);

        RTStrPrintf(szFileBuf, cMaxBuf,
                    "/sys/bus/pci/drivers/%s/bind",
                    pIns->szPrevDriver);
        pFile = vboxPciFileOpen(szFileBuf, O_WRONLY);
        if (pFile)
        {

            /* Don't write trailing \0 */
            vboxPciFileWrite(pFile, 0, szCmdBuf, iCmdLen);
            vboxPciFileClose(pFile);
            pIns->szPrevDriver[0] = '\0';
        }
        else
            printk(KERN_DEBUG "vboxpci: cannot open %s\n", szFileBuf);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
        revert_creds(pOldCreds);
        put_cred(pNewCreds);
#endif

      done:
        kfree(szCmdBuf);
        kfree(szFileBuf);
    }

    return VINF_SUCCESS;
}

int  vboxPciOsDevInit(PVBOXRAWPCIINS pIns, uint32_t fFlags)
{
    struct pci_dev *pPciDev = NULL;
    int rc;

    printk(KERN_DEBUG "vboxpci: vboxPciOsDevInit: dev=%x\n", pIns->HostPciAddress);

    if (fFlags & PCIRAWDRIVERRFLAG_DETACH_HOST_DRIVER)
    {
        rc = vboxPciOsDevDetachHostDriver(pIns);
        if (RT_FAILURE(rc))
        {
            printk(KERN_DEBUG "Cannot detach host driver for device %x: %d\n",
                   pIns->HostPciAddress, rc);
            return VERR_ACCESS_DENIED;
        }
    }


    pPciDev = PCI_DEV_GET_SLOT((pIns->HostPciAddress) >> 8,
                               (pIns->HostPciAddress) & 0xff);

    printk(KERN_DEBUG "vboxpci: vboxPciOsDevInit: dev=%x pdev=%p\n",
           pIns->HostPciAddress, pPciDev);

    if (!pPciDev)
        return 0;

    pIns->pPciDev = pPciDev;

    rc = pci_enable_device(pPciDev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 1)
    if (pci_enable_msi(pPciDev) == 0)
    {
        printk(KERN_DEBUG "vboxpci: enabled MSI\n");
        pIns->fMsiUsed = true;
    }
#endif

    // pci_enable_msix(pPciDev, entries, nvec)

    /* In fact, if device uses interrupts, and cannot be forced to use MSI or MSI-X
       we have to refuse using it, as we cannot work with shared PCI interrupts (unless we're lucky
       to grab unshared PCI interrupt). */

    return VINF_SUCCESS;
}

int  vboxPciOsDevDeinit(PVBOXRAWPCIINS pIns, uint32_t fFlags)
{
    struct pci_dev *pPciDev = NULL;

    printk(KERN_DEBUG "vboxpci: vboxPciOsDevDeinit: dev=%x\n", pIns->HostPciAddress);

    pPciDev = pIns->pPciDev;

    if (pPciDev)
    {
        vboxPciOsDevUnregisterWithIommu(pIns);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 1)
        if (pIns->fMsiUsed)
            pci_disable_msi(pPciDev);
#endif
        // pci_disable_msix(pPciDev);
        pci_disable_device(pPciDev);
        vboxPciOsDevReattachHostDriver(pIns);

        PCI_DEV_PUT(pPciDev);
        pIns->pPciDev = NULL;
    }

    return 0;
}

int  vboxPciOsDevDestroy(PVBOXRAWPCIINS pIns)
{
    return 0;
}

int  vboxPciOsDevGetRegionInfo(PVBOXRAWPCIINS pIns,
                               int32_t        iRegion,
                               RTHCPHYS       *pRegionStart,
                               uint64_t       *pu64RegionSize,
                               bool           *pfPresent,
                               uint32_t       *pfFlags)
{
    int flags;
    struct pci_dev *pPciDev = pIns->pPciDev;
    uint32_t fResFlags;

    if (!pPciDev)
    {
        *pfPresent = false;
        return 0;
    }

    printk(KERN_DEBUG "%x: linux vboxPciOsDevGetRegionInfo: reg=%d\n",
           pIns->HostPciAddress, iRegion);

    flags = pci_resource_flags(pPciDev, iRegion);
    if (((flags & (IORESOURCE_MEM | IORESOURCE_IO)) == 0)
        ||
        ((flags & IORESOURCE_DISABLED) != 0))
    {
        *pfPresent = false;
        return 0;
    }

    *pfPresent = true;
    fResFlags = 0;

    if (flags & IORESOURCE_MEM)
        fResFlags |= PCIRAW_ADDRESS_SPACE_MEM;

    if (flags & IORESOURCE_IO)
        fResFlags |= PCIRAW_ADDRESS_SPACE_IO;

#ifdef IORESOURCE_MEM_64
    if (flags & IORESOURCE_MEM_64)
        fResFlags |= PCIRAW_ADDRESS_SPACE_BAR64;
#endif

    if (flags & IORESOURCE_PREFETCH)
        fResFlags |=  PCIRAW_ADDRESS_SPACE_MEM_PREFETCH;

    *pfFlags        = fResFlags;
    *pRegionStart   = pci_resource_start(pPciDev, iRegion);
    *pu64RegionSize = pci_resource_len  (pPciDev, iRegion);

    printk(KERN_DEBUG "got %s region: %llx:%lld\n",
           (flags & IORESOURCE_MEM) ? "mmio" : "pio", *pRegionStart, *pu64RegionSize);

    return 0;
}

int  vboxPciOsDevMapRegion(PVBOXRAWPCIINS pIns,
                           int32_t        iRegion,
                           RTHCPHYS       RegionStart,
                           uint64_t       u64RegionSize,
                           uint32_t       fFlags,
                           RTR0PTR        *pRegionBase)
{
    struct pci_dev  *pPciDev = pIns->pPciDev;
    struct resource *pRegion;
    RTR0PTR          result = 0;

    printk(KERN_DEBUG "linux vboxPciOsDevMapRegion: reg=%d start=%llx size=%lld\n", iRegion, RegionStart, u64RegionSize);

    if (!pPciDev)
        return 0;

    if (iRegion < 0 || iRegion > 6)
    {
        printk(KERN_DEBUG "vboxPciOsDevMapRegion: invalid region: %d\n", iRegion);
        return VERR_INVALID_PARAMETER;
    }

    pRegion = request_mem_region(RegionStart, u64RegionSize, "vboxpci");
    if (!pRegion)
    {
        /** @todo: need to make sure if thise error indeed can be ignored. */
        printk(KERN_DEBUG "request_mem_region() failed, don't care\n");
    }

    /* For now no caching, try to optimize later. */
    result = ioremap_nocache(RegionStart, u64RegionSize);

    if (!result)
    {
        printk(KERN_DEBUG "cannot ioremap_nocache\n");
        if (pRegion)
            release_mem_region(RegionStart, u64RegionSize);
        return 0;
    }

    *pRegionBase = result;

    return 0;
}

int  vboxPciOsDevUnmapRegion(PVBOXRAWPCIINS pIns,
                             int32_t        iRegion,
                             RTHCPHYS       RegionStart,
                             uint64_t       u64RegionSize,
                             RTR0PTR        RegionBase)
{

    iounmap(RegionBase);
    release_mem_region(RegionStart, u64RegionSize);

    return VINF_SUCCESS;
}

int  vboxPciOsDevPciCfgWrite(PVBOXRAWPCIINS pIns, uint32_t Register, PCIRAWMEMLOC *pValue)
{
    struct pci_dev *pPciDev = pIns->pPciDev;

    if (!pPciDev)
        return VINF_SUCCESS;

    switch (pValue->cb)
    {
        case 1:
            pci_write_config_byte(pPciDev,  Register, pValue->u.u8);
            break;
        case 2:
            pci_write_config_word(pPciDev,  Register, pValue->u.u16);
            break;
        case 4:
            pci_write_config_dword(pPciDev, Register, pValue->u.u32);
            break;
    }

    return VINF_SUCCESS;
}

int  vboxPciOsDevPciCfgRead (PVBOXRAWPCIINS pIns, uint32_t Register, PCIRAWMEMLOC *pValue)
{
    struct pci_dev *pPciDev = pIns->pPciDev;

    if (!pPciDev)
        return VINF_SUCCESS;

    switch (pValue->cb)
    {
        case 1:
            pci_read_config_byte(pPciDev, Register, &pValue->u.u8);
            break;
        case 2:
            pci_read_config_word(pPciDev, Register, &pValue->u.u16);
            break;
        case 4:
            pci_read_config_dword(pPciDev, Register, &pValue->u.u32);
            break;
    }

    return VINF_SUCCESS;
}

/**
 * Interrupt service routine.
 *
 * @returns In 2.6 we indicate whether we've handled the IRQ or not.
 *
 * @param   iIrq            The IRQ number.
 * @param   pvDevId         The device ID, a pointer to PVBOXRAWPCIINS.
 * @param   pvRegs          Register set. Removed in 2.6.19.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
static irqreturn_t vboxPciOsIrqHandler(int iIrq, void *pvDevId)
#else
static irqreturn_t vboxPciOsIrqHandler(int iIrq, void *pvDevId, struct pt_regs *pRegs)
#endif
{
    PVBOXRAWPCIINS pIns = (PVBOXRAWPCIINS)pvDevId;
    bool fTaken = true;

    if (pIns && pIns->IrqHandler.pfnIrqHandler)
        fTaken = pIns->IrqHandler.pfnIrqHandler(pIns->IrqHandler.pIrqContext, iIrq);
#ifndef VBOX_WITH_SHARED_PCI_INTERRUPTS
    /* If we don't allow interrupts sharing, we consider all interrupts as non-shared, thus targetted to us. */
    fTaken = true;
#endif

    return fTaken;
}

int vboxPciOsDevRegisterIrqHandler(PVBOXRAWPCIINS pIns, PFNRAWPCIISR pfnHandler, void* pIrqContext, int32_t *piHostIrq)
{
    int rc;
    int32_t iIrq = pIns->pPciDev->irq;

    if (iIrq == 0)
    {
        printk(KERN_DEBUG "device not assigned host interrupt\n");
        return VERR_INVALID_PARAMETER;
    }

    rc = request_irq(iIrq,
                     vboxPciOsIrqHandler,
#ifdef VBOX_WITH_SHARED_PCI_INTERRUPTS
                     /* Allow interrupts sharing. */
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
                     IRQF_SHARED,
# else
                     SA_SHIRQ,
# endif

#else

                     /* We don't allow interrupts sharing */
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
                     IRQF_DISABLED, /* keep irqs disabled when calling the action handler */
# else
                     0,
# endif
#endif
                     DRIVER_NAME,
                     pIns);
    if (rc)
    {
        printk(KERN_DEBUG "could not request IRQ %d: err=%d\n", iIrq, rc);
        return VERR_RESOURCE_BUSY;
    }

    printk(KERN_DEBUG "got PCI IRQ: %d\n", iIrq);
    *piHostIrq = iIrq;
    return VINF_SUCCESS;
}

int vboxPciOsDevUnregisterIrqHandler(PVBOXRAWPCIINS pIns, int32_t iHostIrq)
{
    printk(KERN_DEBUG "free PCI IRQ: %d\n", iHostIrq);
    free_irq(iHostIrq, pIns);
    return VINF_SUCCESS;
}

int  vboxPciOsDevPowerStateChange(PVBOXRAWPCIINS pIns, PCIRAWPOWERSTATE  aState)
{
    int rc;

    printk(KERN_DEBUG "power state: %d\n", (int)aState);

    switch (aState)
    {
        case PCIRAW_POWER_ON:
            /* Reset device, just in case. */
            vboxPciOsDevReset(pIns);
            /* register us with IOMMU */
            rc = vboxPciOsDevRegisterWithIommu(pIns);
            break;
        case PCIRAW_POWER_RESET:
            rc = vboxPciOsDevReset(pIns);
            break;
        case PCIRAW_POWER_OFF:
            /* unregister us from IOMMU */
            rc = vboxPciOsDevUnregisterWithIommu(pIns);
            break;
        case PCIRAW_POWER_SUSPEND:
        case PCIRAW_POWER_RESUME:
            rc = VINF_SUCCESS;
            /// @todo: what do we do here?
            break;
        default:
            /* to make compiler happy */
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    return rc;
}


#ifdef VBOX_WITH_IOMMU
/** Callback for FNRAWPCICONTIGPHYSMEMINFO. */
static int vboxPciOsContigMemInfo(PRAWPCIPERVM pVmCtx, RTHCPHYS HostStart, RTGCPHYS GuestStart, uint64_t cMemSize, PCIRAWMEMINFOACTION Action)
{
    struct iommu_domain* domain = ((PVBOXRAWPCIDRVVM)(pVmCtx->pDriverData))->pIommuDomain;
    int rc = VINF_SUCCESS;

    switch (Action)
    {
        case PCIRAW_MEMINFO_MAP:
        {
            int flags, r;

            if (iommu_iova_to_phys(domain, GuestStart))
                break;

            flags = IOMMU_READ | IOMMU_WRITE;
            /* @todo: flags |= IOMMU_CACHE; */

            r = iommu_map(domain, GuestStart, HostStart, get_order(cMemSize), flags);
            if (r)
            {
                printk(KERN_ERR "vboxPciOsContigMemInfo:"
                       "iommu failed to map pfn=%llx\n", HostStart);
                rc = VERR_GENERAL_FAILURE;
                break;
            }
            rc =  VINF_SUCCESS;
            break;
        }
        case PCIRAW_MEMINFO_UNMAP:
        {
            int order;
            order = iommu_unmap(domain, GuestStart, get_order(cMemSize));
            NOREF(order);
            break;
        }

        default:
            printk(KERN_DEBUG "Unsupported action: %d\n", (int)Action);
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    return rc;
}
#endif

int  vboxPciOsInitVm(PVBOXRAWPCIDRVVM pThis, PVM pVM, PRAWPCIPERVM pVmData)
{
#ifdef DEBUG
    printk(KERN_DEBUG "vboxPciOsInitVm: %p\n", pThis);
#endif
#ifdef VBOX_WITH_IOMMU
    if (IOMMU_PRESENT())
    {
        pThis->pIommuDomain = IOMMU_DOMAIN_ALLOC();
        if (!pThis->pIommuDomain)
        {
            printk(KERN_DEBUG "cannot allocate IOMMU domain\n");
            return VERR_NO_MEMORY;
        }

        pVmData->pfnContigMemInfo = vboxPciOsContigMemInfo;

        printk(KERN_DEBUG "created IOMMU domain %p\n", pThis->pIommuDomain);
    }
#endif
    return VINF_SUCCESS;
}

void vboxPciOsDeinitVm(PVBOXRAWPCIDRVVM pThis, PVM pVM)
{
#ifdef DEBUG
    printk(KERN_DEBUG "vboxPciOsDeinitVm: %p\n", pThis);
#endif
#ifdef VBOX_WITH_IOMMU
    if (pThis->pIommuDomain)
    {
        iommu_domain_free(pThis->pIommuDomain);
        pThis->pIommuDomain = NULL;
    }
#endif
}
