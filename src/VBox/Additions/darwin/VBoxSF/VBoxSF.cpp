/* $Id$ */
/** @file
 * VBoxSF - Darwin Shared Folders, KEXT entry points.
 */

/*
 * Copyright (C) 2013-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEFAULT
#include "VBoxSFInternal.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <VBox/version.h>
#include <VBox/log.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * The service class for this driver.
 *
 * This has one purpose: Use waitForMatchingService() to find VBoxGuest.
 */
class org_virtualbox_VBoxSF : public IOService
{
    OSDeclareDefaultStructors(org_virtualbox_VBoxSF);

private:
    IOService *waitForCoreService(void);

    IOService *m_pCoreService;

public:
    virtual bool start(IOService *pProvider);
    virtual void stop(IOService *pProvider);
};

OSDefineMetaClassAndStructors(org_virtualbox_VBoxSF, IOService);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * Declare the module stuff.
 */
RT_C_DECLS_BEGIN
static kern_return_t VBoxSfModuleLoad(struct kmod_info *pKModInfo, void *pvData);
static kern_return_t VBoxSfModuleUnload(struct kmod_info *pKModInfo, void *pvData);
extern kern_return_t _start(struct kmod_info *pKModInfo, void *pvData);
extern kern_return_t _stop(struct kmod_info *pKModInfo, void *pvData);
KMOD_EXPLICIT_DECL(VBoxVFS, VBOX_VERSION_STRING, _start, _stop)
DECLHIDDEN(kmod_start_func_t *) _realmain      = VBoxSfModuleLoad;
DECLHIDDEN(kmod_stop_func_t *)  _antimain      = VBoxSfModuleUnload;
DECLHIDDEN(int)                 _kext_apple_cc = __APPLE_CC__;
RT_C_DECLS_END

/** The org_virtualbox_VBoxSF instance.
 * Used for preventing multiple instantiations. */
static org_virtualbox_VBoxSF   *g_pService = NULL;

/** The shared folder service client structure. */
VBGLSFCLIENT                    g_SfClient;
/* VBoxVFS filesystem handle. Needed for FS unregistering. */
static vfstable_t               g_pVBoxSfVfsTableEntry;

/** For vfs_fsentry. */
static struct vnodeopv_desc    *g_apVBoxSfVnodeOpDescList[] =
{
    &g_VBoxSfVnodeOpvDesc,
};


/** VFS registration structure. */
static struct vfs_fsentry g_VBoxSfFsEntry =
{
    .vfe_vfsops     = &g_VBoxSfVfsOps,
    .vfe_vopcnt     = RT_ELEMENTS(g_apVBoxSfVnodeOpDescList),
    .vfe_opvdescs   = g_apVBoxSfVnodeOpDescList,
    .vfe_fstypenum  = -1,
    .vfe_fsname     = VBOXSF_DARWIN_FS_NAME,
    .vfe_flags      = VFS_TBLTHREADSAFE     /* Required. */
                    | VFS_TBLFSNODELOCK     /* Required. */
                    | VFS_TBLNOTYPENUM      /* No historic file system number. */
                    | VFS_TBL64BITREADY,    /* Can handle 64-bit processes */
    /** @todo add VFS_TBLREADDIR_EXTENDED */
    .vfe_reserv     = { NULL, NULL },
};


/**
 * KEXT Module BSD entry point
 */
static kern_return_t VBoxSfModuleLoad(struct kmod_info *pKModInfo, void *pvData)
{
    /* Initialize the R0 guest library. */
#if 0
    rc = VbglR0SfInit();
    if (RT_FAILURE(rc))
        return KERN_FAILURE;
#endif

    PINFO("VirtualBox " VBOX_VERSION_STRING " shared folders "
          "driver is loaded");

    return KERN_SUCCESS;
}


/**
 * KEXT Module BSD exit point
 */
static kern_return_t VBoxSfModuleUnload(struct kmod_info *pKModInfo, void *pvData)
{
#if 0
   VbglR0SfTerm();
#endif

    PINFO("VirtualBox " VBOX_VERSION_STRING " shared folders driver is unloaded");

    return KERN_SUCCESS;
}

/**
 * Wait for VBoxGuest.kext to be started
 */
IOService *org_virtualbox_VBoxSF::waitForCoreService(void)
{
    OSDictionary *pServiceToMatach = serviceMatching("org_virtualbox_VBoxGuest");
    if (pServiceToMatach)
    {
        /* Wait 15 seconds for VBoxGuest to be started */
        IOService *pService = waitForMatchingService(pServiceToMatach, 15 * RT_NS_1SEC_64);
        pServiceToMatach->release();
        return pService;
    }
    PINFO("unable to create matching dictionary");
    return NULL;
}


/**
 * Start this service.
 */
bool org_virtualbox_VBoxSF::start(IOService *pProvider)
{
    if (g_pService == NULL)
        g_pService = this;
    else
    {
        printf("org_virtualbox_VBoxSF::start: g_pService=%p this=%p -> false\n", g_pService, this);
        return false;
    }

    if (IOService::start(pProvider))
    {
        /*
         * Get hold of VBoxGuest.
         */
        m_pCoreService = waitForCoreService();
        if (m_pCoreService)
        {
            int rc = VbglR0SfInit();
            if (RT_SUCCESS(rc))
            {
                /*
                 * Connect to the host service and set UTF-8 as the string encoding to use.
                 */
                rc = VbglR0SfConnect(&g_SfClient);
                if (RT_SUCCESS(rc))
                {
                    rc = VbglR0SfSetUtf8(&g_SfClient);
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Register the file system.
                         */
                        rc = vfs_fsadd(&g_VBoxSfFsEntry, &g_pVBoxSfVfsTableEntry);
                        if (rc == 0)
                        {
                            registerService();

                            LogRel(("VBoxSF: ready\n"));
                            return true;
                        }

                        LogRel(("VBoxSF: vfs_fsadd failed: %d\n", rc));
                    }
                    else
                        LogRel(("VBoxSF: VbglR0SfSetUtf8 failed: %Rrc\n", rc));
                    VbglR0SfDisconnect(&g_SfClient);
                }
                else
                    LogRel(("VBoxSF: VbglR0SfConnect failed: %Rrc\n", rc));
                VbglR0SfTerm();
            }
            else
                LogRel(("VBoxSF: VbglR0SfInit failed: %Rrc\n", rc));
            m_pCoreService->release();
        }
        else
            LogRel(("VBoxSF: Failed to find VBoxGuest!\n"));

        IOService::stop(pProvider);
    }
    g_pService = NULL;
    return false;
}


/**
 * Stop this service.
 */
void org_virtualbox_VBoxSF::stop(IOService *pProvider)
{
    if (m_pCoreService == this)
    {
        /*
         * Unregister the filesystem.
         */
        if (g_pVBoxSfVfsTableEntry != NULL)
        {
            int rc = vfs_fsremove(g_pVBoxSfVfsTableEntry);
            if (rc == 0)
            {
                g_pVBoxSfVfsTableEntry = NULL;
                PINFO("VBoxVFS filesystem successfully unregistered");
            }
            else
            {
                PINFO("Unable to unregister the VBoxSF filesystem (%d)", rc);
                /** @todo how on earth do we deal with this...  Gues we shouldn't be using
                 *        IOService at all here. sigh.  */
            }
        }
        VbglR0SfDisconnect(&g_SfClient);
        VbglR0SfTerm();
        if (m_pCoreService)
            m_pCoreService->release();

    }
    IOService::stop(pProvider);
}

