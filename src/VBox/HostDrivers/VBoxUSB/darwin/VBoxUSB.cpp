/* $Id$ */
/** @file
 * VirtualBox USB driver for Darwin.
 *
 * This driver is responsible for hijacking USB devices when any of the
 * VBoxSVC daemons requests it. It is also responsible for arbitrating
 * access to hijacked USB devices.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
#define LOG_GROUP  LOG_GROUP_USB_DRV
/* Deal with conflicts first.
 * (This is mess inherited from BSD. The *BSDs has clean this up long ago.) */
#include <sys/param.h>
#undef PVM
#include <IOKit/IOLib.h> /* Assert as function */

#include "VBoxUSBInterface.h"
#include "VBoxUSBFilterMgr.h"
#include <VBox/version.h>
#include <VBox/usblib-darwin.h>
#include <VBox/log.h>
#include <iprt/types.h>
#include <iprt/initterm.h>
#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/process.h>
#include <iprt/alloc.h>
#include <iprt/err.h>
#include <iprt/asm.h>

#include <mach/kmod.h>
#include <miscfs/devfs/devfs.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <kern/task.h>
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBUserClient.h>

/* private: */
RT_C_DECLS_BEGIN
extern void     *get_bsdtask_info(task_t);
RT_C_DECLS_END

/* Temporary: Extra logging */
#ifdef Log
# undef Log
#endif
#define Log LogRel
#ifdef Log2
# undef Log2
#endif
#define Log2 LogRel
/* Temporary: needed for extra debug info */
#define VBOX_PROC_SELFNAME_LEN  (20)
#define VBOX_RETRIEVE_CUR_PROC_NAME(_name)    char _name[VBOX_PROC_SELFNAME_LEN]; \
                                              proc_selfname(pszProcName, VBOX_PROC_SELFNAME_LEN)


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Locks the lists. */
#define VBOXUSB_LOCK()      do { int rc = RTSemFastMutexRequest(g_Mtx); AssertRC(rc); } while (0)
/** Unlocks the lists. */
#define VBOXUSB_UNLOCK()    do { int rc = RTSemFastMutexRelease(g_Mtx); AssertRC(rc); } while (0)


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
RT_C_DECLS_BEGIN
static kern_return_t    VBoxUSBStart(struct kmod_info *pKModInfo, void *pvData);
static kern_return_t    VBoxUSBStop(struct kmod_info *pKModInfo, void *pvData);
RT_C_DECLS_END


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The service class.
 *
 * This is the management service that VBoxSVC and the VMs speak to.
 *
 * @remark  The method prototypes are ordered somewhat after their order of
 *          invocation, while the implementation is ordered by pair.
 */
class org_virtualbox_VBoxUSB : public IOService
{
    OSDeclareDefaultStructors(org_virtualbox_VBoxUSB);

public:
    /** @name IOService
     * @{ */
    virtual bool init(OSDictionary *pDictionary = 0);
    virtual bool start(IOService *pProvider);
    virtual bool open(IOService *pForClient, IOOptionBits fOptions = 0, void *pvArg = 0);
    virtual bool terminate(IOOptionBits fOptions);
    virtual void close(IOService *pForClient, IOOptionBits fOptions = 0);
    virtual void stop(IOService *pProvider);
    virtual void free();

    virtual bool terminateClient(IOService *pClient, IOOptionBits fOptions);
    virtual bool finalize(IOOptionBits fOptions);
    virtual void retain() const;
    virtual void release(int freeWhen) const;
    virtual void taggedRetain(const void *pTag=0) const;
    virtual void taggedRelease(const void *pTag, const int freeWhen) const;
    /** @} */
};
OSDefineMetaClassAndStructors(org_virtualbox_VBoxUSB, IOService);




/**
 * The user client class that pairs up with org_virtualbox_VBoxUSB.
 */
class org_virtualbox_VBoxUSBClient : public IOUserClient
{
    OSDeclareDefaultStructors(org_virtualbox_VBoxUSBClient);

public:
    /** @name IOService & IOUserClient
     * @{ */
    virtual bool initWithTask(task_t OwningTask, void *pvSecurityId, UInt32 u32Type);
    virtual bool start(IOService *pProvider);
    virtual IOReturn clientClose(void);
    virtual IOReturn clientDied(void);
    virtual bool terminate(IOOptionBits fOptions = 0);
    virtual bool finalize(IOOptionBits fOptions);
    virtual void stop(IOService *pProvider);
    virtual void free();
    virtual void retain() const;
    virtual void release(int freeWhen) const;
    virtual void taggedRetain(const void *pTag=0) const;
    virtual void taggedRelease(const void *pTag, const int freeWhen) const;
    virtual IOExternalMethod *getTargetAndMethodForIndex(IOService **ppService, UInt32 iMethod);
    /** @} */

    /** @name User client methods
     * @{ */
    IOReturn addFilter(PUSBFILTER pFilter, PVBOXUSBADDFILTEROUT pOut, IOByteCount cbFilter, IOByteCount *pcbOut);
    IOReturn removeFilter(uintptr_t *puId, int *prc, IOByteCount cbIn, IOByteCount *pcbOut);
    /** @} */

    static bool isClientTask(task_t ClientTask);

private:
    /** The service provider. */
    org_virtualbox_VBoxUSB *m_pProvider;
    /** The client task. */
    task_t m_Task;
    /** The client process. */
    RTPROCESS m_Process;
    /** Pointer to the next user client. */
    org_virtualbox_VBoxUSBClient * volatile m_pNext;
    /** List of user clients. Protected by g_Mtx. */
    static org_virtualbox_VBoxUSBClient * volatile s_pHead;
};
OSDefineMetaClassAndStructors(org_virtualbox_VBoxUSBClient, IOUserClient);


/**
 * The IOUSBDevice driver class.
 *
 * The main purpose of this is hijack devices matching current filters.
 *
 * @remarks This is derived from IOUSBUserClientInit instead of IOService because we must make
 *          sure IOUSBUserClientInit::start() gets invoked for this provider. The problem is that
 *          there is some kind of magic that prevents this from happening if we boost the probe
 *          score to high. With the result that we don't have the required plugin entry for
 *          user land and consequently cannot open it.
 *
 *          So, to avoid having to write a lot of code we just inherit from IOUSBUserClientInit
 *          and make some possibly bold assumptions about it not changing. This just means
 *          we'll have to keep an eye on the source apple releases or only call
 *          IOUSBUserClientInit::start() and hand the rest of the super calls to IOService. For
 *          now we're doing it by the C++ book.
 */
class org_virtualbox_VBoxUSBDevice : public IOUSBUserClientInit
{
    OSDeclareDefaultStructors(org_virtualbox_VBoxUSBDevice);

public:
    /** @name IOService
     * @{ */
    virtual bool init(OSDictionary *pDictionary = 0);
    virtual IOService *probe(IOService *pProvider, SInt32 *pi32Score);
    virtual bool start(IOService *pProvider);
    virtual bool terminate(IOOptionBits fOptions = 0);
    virtual void stop(IOService *pProvider);
    virtual void free();
    virtual IOReturn message(UInt32 enmMsg, IOService *pProvider, void *pvArg = 0);

    virtual bool open(IOService *pForClient, IOOptionBits fOptions = 0, void *pvArg = 0);
    virtual void close(IOService *pForClient, IOOptionBits fOptions = 0);
    virtual bool terminateClient(IOService *pClient, IOOptionBits fOptions);
    virtual bool finalize(IOOptionBits fOptions);

    virtual void retain() const;
    virtual void release(int freeWhen) const;
    virtual void taggedRetain(const void *pTag=0) const;
    virtual void taggedRelease(const void *pTag, const int freeWhen) const;

    /** @} */

    static void  scheduleReleaseByOwner(RTPROCESS Owner);
private:
    /** The interface we're driving (aka. the provider). */
    IOUSBDevice *m_pDevice;
    /** The owner process, meaning the VBoxSVC process. */
    RTPROCESS volatile m_Owner;
    /** The client process, meaning the VM process. */
    RTPROCESS volatile m_Client;
    /** The ID of the matching filter. */
    uintptr_t m_uId;
    /** Have we opened the device or not? */
    bool volatile m_fOpen;
    /** Should be open the device on the next close notification message? */
    bool volatile m_fOpenOnWasClosed;
    /** Whether to re-enumerate this device when the client closes it.
     * This is something we'll do when the filter owner dies. */
    bool volatile m_fReleaseOnClose;
    /** Whether we're being unloaded or not.
     * Only valid in stop(). */
    bool m_fBeingUnloaded;
    /** Pointer to the next device in the list. */
    org_virtualbox_VBoxUSBDevice * volatile m_pNext;
    /** Pointer to the list head. Protected by g_Mtx. */
    static org_virtualbox_VBoxUSBDevice * volatile s_pHead;

#ifdef DEBUG
    /** The interest notifier. */
    IONotifier *m_pNotifier;

    static IOReturn MyInterestHandler(void *pvTarget, void *pvRefCon, UInt32 enmMsgType,
                                      IOService *pProvider, void * pvMsgArg, vm_size_t cbMsgArg);
#endif
};
OSDefineMetaClassAndStructors(org_virtualbox_VBoxUSBDevice, IOUSBUserClientInit);


/**
 * The IOUSBInterface driver class.
 *
 * The main purpose of this is hijack interfaces which device is driven
 * by org_virtualbox_VBoxUSBDevice.
 *
 * @remarks See org_virtualbox_VBoxUSBDevice for why we use IOUSBUserClientInit.
 */
class org_virtualbox_VBoxUSBInterface : public IOUSBUserClientInit
{
    OSDeclareDefaultStructors(org_virtualbox_VBoxUSBInterface);

public:
    /** @name IOService
     * @{ */
    virtual bool init(OSDictionary *pDictionary = 0);
    virtual IOService *probe(IOService *pProvider, SInt32 *pi32Score);
    virtual bool start(IOService *pProvider);
    virtual bool terminate(IOOptionBits fOptions = 0);
    virtual void stop(IOService *pProvider);
    virtual void free();
    virtual IOReturn message(UInt32 enmMsg, IOService *pProvider, void *pvArg = 0);

    virtual bool open(IOService *pForClient, IOOptionBits fOptions = 0, void *pvArg = 0);
    virtual void close(IOService *pForClient, IOOptionBits fOptions = 0);
    virtual bool terminateClient(IOService *pClient, IOOptionBits fOptions);
    virtual bool finalize(IOOptionBits fOptions);


    virtual void retain() const;
    virtual void release(int freeWhen) const;
    virtual void taggedRetain(const void *pTag=0) const;
    virtual void taggedRelease(const void *pTag, const int freeWhen) const;

    /** @} */

private:
    /** The interface we're driving (aka. the provider). */
    IOUSBInterface *m_pInterface;
    /** Have we opened the device or not? */
    bool volatile m_fOpen;
    /** Should be open the device on the next close notification message? */
    bool volatile m_fOpenOnWasClosed;
};
OSDefineMetaClassAndStructors(org_virtualbox_VBoxUSBInterface, IOUSBUserClientInit);





/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/*
 * Declare the module stuff.
 */
RT_C_DECLS_BEGIN
extern kern_return_t _start(struct kmod_info *pKModInfo, void *pvData);
extern kern_return_t _stop(struct kmod_info *pKModInfo, void *pvData);

KMOD_EXPLICIT_DECL(VBoxDrv, VBOX_VERSION_STRING, _start, _stop)
DECLHIDDEN(kmod_start_func_t *) _realmain = VBoxUSBStart;
DECLHIDDEN(kmod_stop_func_t  *) _antimain = VBoxUSBStop;
DECLHIDDEN(int)                 _kext_apple_cc = __APPLE_CC__;
RT_C_DECLS_END

/** Mutex protecting the lists. */
static RTSEMFASTMUTEX g_Mtx = NIL_RTSEMFASTMUTEX;
org_virtualbox_VBoxUSBClient * volatile org_virtualbox_VBoxUSBClient::s_pHead = NULL;
org_virtualbox_VBoxUSBDevice * volatile org_virtualbox_VBoxUSBDevice::s_pHead = NULL;

/** Global instance count - just for checking proving that everything is destroyed correctly. */
static volatile uint32_t g_cInstances = 0;


/**
 * Start the kernel module.
 */
static kern_return_t VBoxUSBStart(struct kmod_info *pKModInfo, void *pvData)
{
    int rc;
    Log(("VBoxUSBStart\n"));

    /*
     * Initialize IPRT.
     */
    rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        /*
         * Create the spinlock.
         */
        rc = RTSemFastMutexCreate(&g_Mtx);
        if (RT_SUCCESS(rc))
        {
            rc = VBoxUSBFilterInit();
            if (RT_SUCCESS(rc))
            {
#if 0 /* testing */
                USBFILTER Flt;
                USBFilterInit(&Flt, USBFILTERTYPE_CAPTURE);
                USBFilterSetNumExact(&Flt, USBFILTERIDX_VENDOR_ID, 0x096e, true);
                uintptr_t uId;
                rc = VBoxUSBFilterAdd(&Flt, 1, &uId);
                printf("VBoxUSB: VBoxUSBFilterAdd #1 -> %d + %p\n", rc, uId);

                USBFilterInit(&Flt, USBFILTERTYPE_CAPTURE);
                USBFilterSetStringPattern(&Flt, USBFILTERIDX_PRODUCT_STR, "*DISK*", true);
                rc = VBoxUSBFilterAdd(&Flt, 2, &uId);
                printf("VBoxUSB: VBoxUSBFilterAdd #2 -> %d + %p\n", rc, uId);
#endif
                return KMOD_RETURN_SUCCESS;
            }
            printf("VBoxUSB: VBoxUSBFilterInit failed (rc=%d)\n", rc);
            RTSemFastMutexDestroy(g_Mtx);
            g_Mtx = NIL_RTSEMFASTMUTEX;
        }
        else
            printf("VBoxUSB: RTSemFastMutexCreate failed (rc=%d)\n", rc);
        RTR0Term();
    }
    else
        printf("VBoxUSB: failed to initialize IPRT (rc=%d)\n", rc);

    return KMOD_RETURN_FAILURE;
}


/**
 * Stop the kernel module.
 */
static kern_return_t VBoxUSBStop(struct kmod_info *pKModInfo, void *pvData)
{
    int rc;
    Log(("VBoxUSBStop: g_cInstances=%d\n", g_cInstances));

    /** @todo Fix problem with crashing when unloading a driver that's in use. */

    /*
     * Undo the work done during start (in reverse order).
     */
    VBoxUSBFilterTerm();

    rc = RTSemFastMutexDestroy(g_Mtx);
    AssertRC(rc);
    g_Mtx = NIL_RTSEMFASTMUTEX;

    RTR0Term();

    Log(("VBoxUSBStop - done\n"));
    return KMOD_RETURN_SUCCESS;
}






/**
 * Gets the name of a IOKit message.
 *
 * @returns Message name (read only).
 * @param   enmMsg      The message.
 */
DECLINLINE(const char *) DbgGetIOKitMessageName(UInt32 enmMsg)
{
#ifdef DEBUG
    switch (enmMsg)
    {
#define MY_CASE(enm) case enm: return #enm; break
        MY_CASE(kIOMessageServiceIsTerminated);
        MY_CASE(kIOMessageServiceIsSuspended);
        MY_CASE(kIOMessageServiceIsResumed);
        MY_CASE(kIOMessageServiceIsRequestingClose);
        MY_CASE(kIOMessageServiceIsAttemptingOpen);
        MY_CASE(kIOMessageServiceWasClosed);
        MY_CASE(kIOMessageServiceBusyStateChange);
        MY_CASE(kIOMessageServicePropertyChange);
        MY_CASE(kIOMessageCanDevicePowerOff);
        MY_CASE(kIOMessageDeviceWillPowerOff);
        MY_CASE(kIOMessageDeviceWillNotPowerOff);
        MY_CASE(kIOMessageDeviceHasPoweredOn);
        MY_CASE(kIOMessageCanSystemPowerOff);
        MY_CASE(kIOMessageSystemWillPowerOff);
        MY_CASE(kIOMessageSystemWillNotPowerOff);
        MY_CASE(kIOMessageCanSystemSleep);
        MY_CASE(kIOMessageSystemWillSleep);
        MY_CASE(kIOMessageSystemWillNotSleep);
        MY_CASE(kIOMessageSystemHasPoweredOn);
        MY_CASE(kIOMessageSystemWillRestart);
        MY_CASE(kIOMessageSystemWillPowerOn);
        MY_CASE(kIOUSBMessageHubResetPort);
        MY_CASE(kIOUSBMessageHubSuspendPort);
        MY_CASE(kIOUSBMessageHubResumePort);
        MY_CASE(kIOUSBMessageHubIsDeviceConnected);
        MY_CASE(kIOUSBMessageHubIsPortEnabled);
        MY_CASE(kIOUSBMessageHubReEnumeratePort);
        MY_CASE(kIOUSBMessagePortHasBeenReset);
        MY_CASE(kIOUSBMessagePortHasBeenResumed);
        MY_CASE(kIOUSBMessageHubPortClearTT);
        MY_CASE(kIOUSBMessagePortHasBeenSuspended);
        MY_CASE(kIOUSBMessageFromThirdParty);
        MY_CASE(kIOUSBMessagePortWasNotSuspended);
        MY_CASE(kIOUSBMessageExpressCardCantWake);
//        MY_CASE(kIOUSBMessageCompositeDriverReconfigured);
#undef MY_CASE
    }
#endif /* DEBUG */
    return "unknown";
}





/*
 *
 * org_virtualbox_VBoxUSB
 *
 */


/**
 * Initialize the object.
 * @remark  Only for logging.
 */
bool
org_virtualbox_VBoxUSB::init(OSDictionary *pDictionary)
{
    uint32_t cInstances = ASMAtomicIncU32(&g_cInstances);
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSB::init([%p], %p) new g_cInstances=%d pszProcName=[%s] [retain count: %d]\n", this, pDictionary, cInstances, pszProcName, getRetainCount()));
    if (IOService::init(pDictionary))
    {
        /* init members. */
        Log(("VBoxUSB::init([%p], %p) returns true pszProcName=[%s] [retain count: %d]\n", this, pDictionary, pszProcName, getRetainCount()));
        return true;
    }
    ASMAtomicDecU32(&g_cInstances);
    Log(("VBoxUSB::init([%p], %p) returns false pszProcName=[%s] [retain count: %d]\n", this, pDictionary, pszProcName, getRetainCount()));
    return false;
}


/**
 * Free the object.
 * @remark  Only for logging.
 */
void
org_virtualbox_VBoxUSB::free()
{
    uint32_t cInstances = ASMAtomicDecU32(&g_cInstances); NOREF(cInstances);
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSB::free([%p]) new g_cInstances=%d pszProcName=[%s] [retain count: %d]\n", this, cInstances, pszProcName, getRetainCount()));
    IOService::free();
}


/**
 * Start this service.
 */
bool
org_virtualbox_VBoxUSB::start(IOService *pProvider)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSB::start([%p], %p {%s}) pszProcName=[%s] [retain count: %d]\n", this, pProvider, pProvider->getName(), pszProcName, getRetainCount()));

    if (IOService::start(pProvider))
    {
        /* register the service. */
        registerService();
        Log(("VBoxUSB::start([%p], %p {%s}) pszProcName=[%s] returns true [retain count: %d]\n", this, pProvider, pProvider->getName(), pszProcName, getRetainCount()));
        return true;
    }

    Log(("VBoxUSB::start([%p], %p {%s}) pszProcName=[%s] returns false [retain count: %d]\n", this, pProvider, pProvider->getName(), pszProcName, getRetainCount()));
    return false;
}


/**
 * Stop this service.
 * @remark  Only for logging.
 */
void
org_virtualbox_VBoxUSB::stop(IOService *pProvider)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSB::stop([%p], %p (%s)) (1) pszProcName=[%s] [retain count: %d]\n", this, pProvider, pProvider->getName(), pszProcName, getRetainCount()));
    IOService::stop(pProvider);
    Log(("VBoxUSB::stop([%p], %p (%s)) (2) pszProcName=[%s] [retain count: %d]\n", this, pProvider, pProvider->getName(), pszProcName, getRetainCount()));
}


/**
 * Stop this service.
 * @remark  Only for logging.
 */
bool
org_virtualbox_VBoxUSB::open(IOService *pForClient, IOOptionBits fOptions/* = 0*/, void *pvArg/* = 0*/)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSB::open([%p], %p, %#x, %p) pszProcName=[%s] [retain count: %d]\n", this, pForClient, fOptions, pvArg, pszProcName, getRetainCount()));
    bool fRc = IOService::open(pForClient, fOptions, pvArg);
    Log(("VBoxUSB::open([%p], %p, %#x, %p) -> %d pszProcName=[%s] [retain count: %d]\n", this, pForClient, fOptions, pvArg, fRc, pszProcName, getRetainCount()));
    return fRc;
}


/**
 * Stop this service.
 * @remark  Only for logging.
 */
void
org_virtualbox_VBoxUSB::close(IOService *pForClient, IOOptionBits fOptions/* = 0*/)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    IOService::close(pForClient, fOptions);
    Log(("VBoxUSB::close([%p], %p, %#x) pszProcName=[%s] [retain count: %d]\n", this, pForClient, fOptions, pszProcName, getRetainCount()));
}


/**
 * Terminate request.
 * @remark  Only for logging.
 */
bool
org_virtualbox_VBoxUSB::terminate(IOOptionBits fOptions)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSB::terminate([%p], %#x): pszProcName=[%s] g_cInstances=%d [retain count: %d]\n", this, fOptions, pszProcName, g_cInstances, getRetainCount()));
    bool fRc = IOService::terminate(fOptions);
    Log(("VBoxUSB::terminate([%p], %#x): pszProcName=[%s] returns %d [retain count: %d]\n", this, fOptions, pszProcName, fRc, getRetainCount()));
    return fRc;
}


bool
org_virtualbox_VBoxUSB::terminateClient(IOService *pClient, IOOptionBits fOptions)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSB::terminateClient([%p], pClient=%p, fOptions=%#x) pszProcName=[%s] [retain count: %d]\n", this, pClient, fOptions, pszProcName, getRetainCount()));
    bool fRc = IOService::terminateClient(pClient, fOptions);
    Log(("VBoxUSB::terminateClient([%p], pClient=%p, fOptions=%#x) returns %d pszProcName=[%s] [retain count: %d]\n", this, pClient, fOptions, fRc, pszProcName, getRetainCount()));
    return fRc;
}


bool
org_virtualbox_VBoxUSB::finalize(IOOptionBits fOptions)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSB::finalize([%p], fOptions=%#x) pszProcName=[%s] [retain count: %d]\n", this, fOptions, pszProcName, getRetainCount()));
    bool fRc = IOService::finalize(fOptions);
    Log(("VBoxUSB::finalize([%p], fOptions=%#x) returns %d pszProcName=[%s] [retain count: %d]\n", this, fOptions, fRc, pszProcName, getRetainCount()));
    return fRc;
}


void
org_virtualbox_VBoxUSB::retain() const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSB::retain([%p]) (1) pszProcName=[%s] [retain count: %d]\n", this, pszProcName, getRetainCount()));
    IOService::retain();
    Log(("VBoxUSB::retain([%p]) (2) pszProcName=[%s] [retain count: %d]\n", this, pszProcName, getRetainCount()));
}


void
org_virtualbox_VBoxUSB::release(int freeWhen) const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSB::release([%p], freeWhen=[%d]) pszProcName=[%s] [retain count: %d]\n", this, freeWhen, pszProcName, getRetainCount()));
    IOService::release(freeWhen);
}


void
org_virtualbox_VBoxUSB::taggedRetain(const void *pTag) const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSB::taggedRetain([%p], pTag=[%p]) (1) pszProcName=[%s] [retain count: %d]\n", this, pTag, pszProcName, getRetainCount()));
    IOService::taggedRetain(pTag);
    Log(("VBoxUSB::taggedRetain([%p], pTag=[%p]) (2) pszProcName=[%s] [retain count: %d]\n", this, pTag, pszProcName, getRetainCount()));
}


void
org_virtualbox_VBoxUSB::taggedRelease(const void *pTag, const int freeWhen) const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSB::taggedRelease([%p], pTag=[%p], freeWhen=[%d]) pszProcName=[%s] [retain count: %d]\n", this, pTag, freeWhen, pszProcName, getRetainCount()));
    IOService::taggedRelease(pTag, freeWhen);
}










/*
 *
 * org_virtualbox_VBoxUSBClient
 *
 */


/**
 * Initializer called when the client opens the service.
 */
bool
org_virtualbox_VBoxUSBClient::initWithTask(task_t OwningTask, void *pvSecurityId, UInt32 u32Type)
{
    if (!OwningTask)
    {
        Log(("VBoxUSBClient::initWithTask([%p], %p, %p, %#x) -> false (no task) [retain count: %d]\n", this, OwningTask, pvSecurityId, u32Type, getRetainCount()));
        return false;
    }

    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);

    proc_t pProc = (proc_t)get_bsdtask_info(OwningTask); /* we need the pid */
    Log(("VBoxUSBClient::initWithTask([%p], %p(->%p:{.pid=%d (%s)}, %p, %#x) [retain count: %d]\n",
             this, OwningTask, pProc, pProc ? proc_pid(pProc) : -1, pszProcName, pvSecurityId, u32Type, getRetainCount()));

    if (IOUserClient::initWithTask(OwningTask, pvSecurityId , u32Type))
    {
        m_pProvider = NULL;
        m_Task = OwningTask;
        m_Process = pProc ? proc_pid(pProc) : NIL_RTPROCESS;
        m_pNext = NULL;

        uint32_t cInstances = ASMAtomicIncU32(&g_cInstances);
        Log(("VBoxUSBClient::initWithTask([%p], %p(->%p:{.pid=%d (%s)}, %p, %#x) -> true; new g_cInstances=%d [retain count: %d]\n",
                 this, OwningTask, pProc, pProc ? proc_pid(pProc) : -1, pszProcName, pvSecurityId, u32Type, cInstances, getRetainCount()));
        return true;
    }

    Log(("VBoxUSBClient::initWithTask([%p], %p(->%p:{.pid=%d (%s)}, %p, %#x) -> false [retain count: %d]\n",
             this, OwningTask, pProc, pProc ? proc_pid(pProc) : -1, pszProcName, pvSecurityId, u32Type, getRetainCount()));
    return false;
}


/**
 * Free the object.
 * @remark  Only for logging.
 */
void
org_virtualbox_VBoxUSBClient::free()
{
    uint32_t cInstances = ASMAtomicDecU32(&g_cInstances); NOREF(cInstances);
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBClient::free([%p]) new g_cInstances=%d pszProcName=[%s] [retain count: %d]\n", this, cInstances, pszProcName, getRetainCount()));
    IOUserClient::free();
}


/**
 * Retain the object.
 * @remark  Only for logging.
 */
void
org_virtualbox_VBoxUSBClient::retain() const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);

    Log(("VBoxUSBClient::retain([%p]) (1) pszProcName=[%s] [retain count: %d]\n", this, pszProcName, getRetainCount()));
    IOUserClient::retain();
    Log(("VBoxUSBClient::retain([%p]) (2) pszProcName=[%s] [retain count: %d]\n", this, pszProcName, getRetainCount()));
}


/**
 * Release the object.
 * @remark  Only for logging.
 */
void
org_virtualbox_VBoxUSBClient::release(int freeWhen) const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);

    Log(("VBoxUSBClient::release([%p], freeWhen=%d) pszProcName=[%s] [retain count: %d]\n", this, freeWhen, pszProcName, getRetainCount()));
    IOUserClient::release(freeWhen);
}


/**
 * Tagged retain the object.
 * @remark  Only for logging.
 */
void
org_virtualbox_VBoxUSBClient::taggedRetain(const void *pTag) const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);

    Log(("VBoxUSBClient::taggedRetain([%p], [pTag=%p]) (1) pszProcName=[%s] [retain count: %d]\n", this, pTag, pszProcName, getRetainCount()));
    IOUserClient::taggedRetain(pTag);
    Log(("VBoxUSBClient::taggedRetain([%p], [pTag=%p]) (2) pszProcName=[%s] [retain count: %d]\n", this, pTag, pszProcName, getRetainCount()));
}


/**
 * Tagged release the object.
 * @remark  Only for logging.
 */
void
org_virtualbox_VBoxUSBClient::taggedRelease(const void *pTag, const int freeWhen) const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);

    Log(("VBoxUSBClient::taggedRelease([%p], [pTag=%p], [freeWhen=%d]) pszProcName=[%s] [retain count: %d]\n", this, pTag, freeWhen, pszProcName, getRetainCount()));
    IOUserClient::taggedRelease(pTag, freeWhen);
}


/**
 * Start the client service.
 */
bool
org_virtualbox_VBoxUSBClient::start(IOService *pProvider)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);

    Log(("VBoxUSBClient::start([%p], %p) pszProcName=[%s] [retain count: %d]\n", this, pProvider, pszProcName, getRetainCount()));
    if (IOUserClient::start(pProvider))
    {
        m_pProvider = OSDynamicCast(org_virtualbox_VBoxUSB, pProvider);
        if (m_pProvider)
        {
            /*
             * Add ourselves to the list of user clients.
             */
            VBOXUSB_LOCK();

            m_pNext = s_pHead;
            s_pHead = this;

            VBOXUSB_UNLOCK();

            Log(("VBoxUSBClient::start([%p]): returns true pszProcName=[%s] [retain count: %d]\n", this, pszProcName, getRetainCount()));
            return true;
        }
        Log(("VBoxUSBClient::start: %p isn't org_virtualbox_VBoxUSB pszProcName=[%s] [retain count: %d]\n", pProvider, pszProcName, getRetainCount()));
    }

    Log(("VBoxUSBClient::start([%p]): returns false pszProcName=[%s] [retain count: %d]\n", this, pszProcName, getRetainCount()));
    return false;
}


/**
 * Client exits normally.
 */
IOReturn
org_virtualbox_VBoxUSBClient::clientClose(void)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);

    Log(("VBoxUSBClient::clientClose([%p:{.m_Process=%d}]) pszProcName=[%s] [retain count: %d]\n", this, (int)m_Process, pszProcName, getRetainCount()));

    /*
     * Remove this process from the client list.
     */
    VBOXUSB_LOCK();

    org_virtualbox_VBoxUSBClient *pPrev = NULL;
    for (org_virtualbox_VBoxUSBClient *pCur = s_pHead; pCur; pCur = pCur->m_pNext)
    {
        if (pCur == this)
        {
            if (pPrev)
                pPrev->m_pNext = m_pNext;
            else
                s_pHead = m_pNext;
            m_pNext = NULL;
            break;
        }
        pPrev = pCur;
    }

    VBOXUSB_UNLOCK();

    /*
     * Drop all filters owned by this client.
     */
    if (m_Process != NIL_RTPROCESS)
        VBoxUSBFilterRemoveOwner(m_Process);

    /*
     * Schedule all devices owned (filtered) by this process for
     * immediate release or release upon close.
     */
    if (m_Process != NIL_RTPROCESS)
        org_virtualbox_VBoxUSBDevice::scheduleReleaseByOwner(m_Process);

    /*
     * Initiate termination.
     */
    m_pProvider = NULL;
    terminate();

    Log(("VBoxUSBClient::clientClose([%p:{.m_Process=%d}]) return kIOReturnSuccess pszProcName=[%s] [retain count: %d]\n", this, (int)m_Process, pszProcName, getRetainCount()));

    return kIOReturnSuccess;
}


/**
 * The client exits abnormally / forgets to do cleanups.
 * @remark  Only for logging.
 */
IOReturn
org_virtualbox_VBoxUSBClient::clientDied(void)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);

    Log(("VBoxUSBClient::clientDied([%p]) m_Task=%p R0Process=%p Process=%d pszProcName=[%s] [retain count: %d]\n",
             this, m_Task, RTR0ProcHandleSelf(), RTProcSelf(), pszProcName, getRetainCount()));
    /* IOUserClient::clientDied() calls clientClose... */
    bool fRc = IOUserClient::clientDied();
    Log(("VBoxUSBClient::clientDied([%p]): returns %d pszProcName=[%s] [retain count: %d]\n", this, fRc, pszProcName, getRetainCount()));
    return fRc;
}


/**
 * Terminate the service (initiate the destruction).
 * @remark  Only for logging.
 */
bool
org_virtualbox_VBoxUSBClient::terminate(IOOptionBits fOptions)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    /* kIOServiceRecursing, kIOServiceRequired, kIOServiceTerminate, kIOServiceSynchronous - interesting option bits */
    Log(("VBoxUSBClient::terminate([%p], %#x) pszProcName=[%s] [retain count: %d]\n", this, fOptions, pszProcName, getRetainCount()));
    bool fRc = IOUserClient::terminate(fOptions);
    Log(("VBoxUSBClient::terminate([%p]): returns %d pszProcName=[%s] [retain count: %d]\n", this, fRc, pszProcName, getRetainCount()));
    return fRc;
}


/**
 * The final stage of the client service destruction.
 * @remark  Only for logging.
 */
bool
org_virtualbox_VBoxUSBClient::finalize(IOOptionBits fOptions)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBClient::finalize([%p], %#x) pszProcName=[%s] [retain count: %d]\n", this, fOptions, pszProcName, getRetainCount()));
    bool fRc = IOUserClient::finalize(fOptions);
    Log(("VBoxUSBClient::finalize([%p]): returns %d pszProcName=[%s] [retain count: %d]\n", this, fRc, pszProcName, getRetainCount()));
    return fRc;
}


/**
 * Stop the client service.
 */
void
org_virtualbox_VBoxUSBClient::stop(IOService *pProvider)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBClient::stop([%p]) (1) pszProcName=[%s] [retain count: %d]\n", this, pszProcName, getRetainCount()));
    IOUserClient::stop(pProvider);
    Log(("VBoxUSBClient::stop([%p]) (2) pszProcName=[%s] [retain count: %d]\n", this, pszProcName, getRetainCount()));

    /*
     * Paranoia.
     */
    VBOXUSB_LOCK();

    org_virtualbox_VBoxUSBClient *pPrev = NULL;
    for (org_virtualbox_VBoxUSBClient *pCur = s_pHead; pCur; pCur = pCur->m_pNext)
    {
        if (pCur == this)
        {
            if (pPrev)
                pPrev->m_pNext = m_pNext;
            else
                s_pHead = m_pNext;
            m_pNext = NULL;
            break;
        }
        pPrev = pCur;
    }

    VBOXUSB_UNLOCK();
}


/**
 * Translate a user method index into a service object and an external method structure.
 *
 * @returns Pointer to external method structure descripting the method.
 *          NULL if the index isn't valid.
 * @param   ppService       Where to store the service object on success.
 * @param   iMethod         The method index.
 */
IOExternalMethod *
org_virtualbox_VBoxUSBClient::getTargetAndMethodForIndex(IOService **ppService, UInt32 iMethod)
{
    static IOExternalMethod s_aMethods[VBOXUSBMETHOD_END] =
    {
        /*[VBOXUSBMETHOD_ADD_FILTER] = */
        {
            (IOService *)0,                                         /* object */
            (IOMethod)&org_virtualbox_VBoxUSBClient::addFilter,     /* func */
            kIOUCStructIStructO,                                    /* flags - struct input (count0) and struct output (count1) */
            sizeof(USBFILTER),                                      /* count0 - size of the input struct. */
            sizeof(VBOXUSBADDFILTEROUT)                             /* count1 - size of the return struct. */
        },
        /* [VBOXUSBMETHOD_FILTER_REMOVE] = */
        {
            (IOService *)0,                                         /* object */
            (IOMethod)&org_virtualbox_VBoxUSBClient::removeFilter,  /* func */
            kIOUCStructIStructO,                                    /* flags - struct input (count0) and struct output (count1) */
            sizeof(uintptr_t),                                      /* count0 - size of the input (id) */
            sizeof(int)                                             /* count1 - size of the output (rc) */
        },
    };

    if (RT_UNLIKELY(iMethod >= RT_ELEMENTS(s_aMethods)))
        return NULL;

    *ppService = this;
    return &s_aMethods[iMethod];
}


/**
 * Add filter user request.
 *
 * @returns IOKit status code.
 * @param   pFilter     The filter to add.
 * @param   pOut        Pointer to the output structure.
 * @param   cbFilter    Size of the filter structure.
 * @param   pcbOut      In/Out - sizeof(*pOut).
 */
IOReturn
org_virtualbox_VBoxUSBClient::addFilter(PUSBFILTER pFilter, PVBOXUSBADDFILTEROUT pOut, IOByteCount cbFilter, IOByteCount *pcbOut)
{
    Log(("VBoxUSBClient::addFilter: [%p:{.m_Process=%d}] pFilter=%p pOut=%p\n", this, (int)m_Process, pFilter, pOut));

    /*
     * Validate input.
     */
    if (RT_UNLIKELY(    cbFilter != sizeof(*pFilter)
                    ||  *pcbOut != sizeof(*pOut)))
    {
        printf("VBoxUSBClient::addFilter: cbFilter=%#x expected %#x; *pcbOut=%#x expected %#x\n",
               (int)cbFilter, (int)sizeof(*pFilter), (int)*pcbOut, (int)sizeof(*pOut));
        return kIOReturnBadArgument;
    }

    /*
     * Log the filter details.
     */
#ifdef DEBUG
    Log2(("VBoxUSBClient::addFilter: idVendor=%#x idProduct=%#x bcdDevice=%#x bDeviceClass=%#x bDeviceSubClass=%#x bDeviceProtocol=%#x bBus=%#x bPort=%#x\n",
              USBFilterGetNum(pFilter, USBFILTERIDX_VENDOR_ID),
              USBFilterGetNum(pFilter, USBFILTERIDX_PRODUCT_ID),
              USBFilterGetNum(pFilter, USBFILTERIDX_DEVICE_REV),
              USBFilterGetNum(pFilter, USBFILTERIDX_DEVICE_CLASS),
              USBFilterGetNum(pFilter, USBFILTERIDX_DEVICE_SUB_CLASS),
              USBFilterGetNum(pFilter, USBFILTERIDX_DEVICE_PROTOCOL),
              USBFilterGetNum(pFilter, USBFILTERIDX_BUS),
              USBFilterGetNum(pFilter, USBFILTERIDX_PORT)));
    Log2(("VBoxUSBClient::addFilter: Manufacturer=%s Product=%s Serial=%s\n",
              USBFilterGetString(pFilter, USBFILTERIDX_MANUFACTURER_STR)  ? USBFilterGetString(pFilter, USBFILTERIDX_MANUFACTURER_STR)  : "<null>",
              USBFilterGetString(pFilter, USBFILTERIDX_PRODUCT_STR)       ? USBFilterGetString(pFilter, USBFILTERIDX_PRODUCT_STR)       : "<null>",
              USBFilterGetString(pFilter, USBFILTERIDX_SERIAL_NUMBER_STR) ? USBFilterGetString(pFilter, USBFILTERIDX_SERIAL_NUMBER_STR) : "<null>"));
#endif

    /*
     * Since we cannot query the bus number, make sure the filter
     * isn't requiring that field to be present.
     */
    int rc = USBFilterSetMustBePresent(pFilter, USBFILTERIDX_BUS, false /* fMustBePresent */); AssertRC(rc);

    /*
     * Add the filter.
     */
    pOut->uId = 0;
    pOut->rc = VBoxUSBFilterAdd(pFilter, m_Process, &pOut->uId);

    Log(("VBoxUSBClient::addFilter: returns *pOut={.rc=%d, .uId=%p}\n", pOut->rc, (void *)pOut->uId));
    return kIOReturnSuccess;
}


/**
 * Removes filter user request.
 *
 * @returns IOKit status code.
 * @param   puId    Where to get the filter ID.
 * @param   prc     Where to store the return code.
 * @param   cbIn    sizeof(*puId).
 * @param   pcbOut  In/Out - sizeof(*prc).
 */
IOReturn
org_virtualbox_VBoxUSBClient::removeFilter(uintptr_t *puId, int *prc, IOByteCount cbIn, IOByteCount *pcbOut)
{
    Log(("VBoxUSBClient::removeFilter: [%p:{.m_Process=%d}] *puId=%p m_Proc\n", this, (int)m_Process, *puId));

    /*
     * Validate input.
     */
    if (RT_UNLIKELY(    cbIn != sizeof(*puId)
                    ||  *pcbOut != sizeof(*prc)))
    {
        printf("VBoxUSBClient::removeFilter: cbIn=%#x expected %#x; *pcbOut=%#x expected %#x\n",
               (int)cbIn, (int)sizeof(*puId), (int)*pcbOut, (int)sizeof(*prc));
        return kIOReturnBadArgument;
    }

    /*
     * Remove the filter.
     */
    *prc = VBoxUSBFilterRemove(m_Process, *puId);

    Log(("VBoxUSBClient::removeFilter: returns *prc=%d\n", *prc));
    return kIOReturnSuccess;
}


/**
 * Checks whether the specified task is a VBoxUSB client task or not.
 *
 * This is used to validate clients trying to open any of the device
 * or interfaces that we've hijacked.
 *
 * @returns true / false.
 * @param   ClientTask      The task.
 *
 * @remark  This protecting against other user clients is not currently implemented
 *          as it turned out to be more bothersome than first imagined.
 */
/* static*/ bool
org_virtualbox_VBoxUSBClient::isClientTask(task_t ClientTask)
{
    VBOXUSB_LOCK();

    for (org_virtualbox_VBoxUSBClient *pCur = s_pHead; pCur; pCur = pCur->m_pNext)
        if (pCur->m_Task == ClientTask)
        {
            VBOXUSB_UNLOCK();
            return true;
        }

    VBOXUSB_UNLOCK();
    return false;
}














/*
 *
 * org_virtualbox_VBoxUSBDevice
 *
 */

/**
 * Initialize instance data.
 *
 * @returns Success indicator.
 * @param   pDictionary     The dictionary that will become the registry entry's
 *                          property table, or NULL. Hand it up to our parents.
 */
bool
org_virtualbox_VBoxUSBDevice::init(OSDictionary *pDictionary)
{
    uint32_t cInstances = ASMAtomicIncU32(&g_cInstances);

    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBDevice::init([%p], %p) new g_cInstances=%d pszProcName=[%s] [retain count: %d]\n", this, pDictionary, cInstances, pszProcName, getRetainCount()));

    m_pDevice = NULL;
    m_Owner = NIL_RTPROCESS;
    m_Client = NIL_RTPROCESS;
    m_uId = ~(uintptr_t)0;
    m_fOpen = false;
    m_fOpenOnWasClosed = false;
    m_fReleaseOnClose = false;
    m_fBeingUnloaded = false;
    m_pNext = NULL;
#ifdef DEBUG
    m_pNotifier = NULL;
#endif

    return IOUSBUserClientInit::init(pDictionary);
}

/**
 * Free the object.
 * @remark  Only for logging.
 */
void
org_virtualbox_VBoxUSBDevice::free()
{
    uint32_t cInstances = ASMAtomicDecU32(&g_cInstances); NOREF(cInstances);
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);

    Log(("VBoxUSBDevice::free([%p]) new g_cInstances=%d pszProcName=[%s] [retain count: %d]\n", this, cInstances, pszProcName, getRetainCount()));
    IOUSBUserClientInit::free();
}


/**
 * The device/driver probing.
 *
 * I/O Kit will iterate all device drivers suitable for this kind of device
 * (this is something it figures out from the property file) and call their
 * probe() method in order to try determine which is the best match for the
 * device. We will match the device against the registered filters and set
 * a ridiculously high score if we find it, thus making it extremely likely
 * that we'll be the first driver to be started. We'll also set a couple of
 * attributes so that it's not necessary to do a rematch in init to find
 * the appropriate filter (might not be necessary..., see todo).
 *
 * @returns Service instance to be started and *pi32Score if matching.
 *          NULL if not a device suitable for this driver.
 *
 * @param   pProvider       The provider instance.
 * @param   pi32Score       Where to store the probe score.
 */
IOService *
org_virtualbox_VBoxUSBDevice::probe(IOService *pProvider, SInt32 *pi32Score)
{
    Log(("VBoxUSBDevice::probe([%p], %p {%s}, %p={%d})\n", this,
             pProvider, pProvider->getName(), pi32Score, pi32Score ? *pi32Score : 0));

    /*
     * Check against filters.
     */
    USBFILTER Device;
    USBFilterInit(&Device, USBFILTERTYPE_CAPTURE);

    static const struct
    {
        const char     *pszName;
        USBFILTERIDX    enmIdx;
        bool            fNumeric;
    } s_aProps[] =
    {
        { kUSBVendorID,             USBFILTERIDX_VENDOR_ID,         true },
        { kUSBProductID,            USBFILTERIDX_PRODUCT_ID,        true },
        { kUSBDeviceReleaseNumber,  USBFILTERIDX_DEVICE_REV,        true },
        { kUSBDeviceClass,          USBFILTERIDX_DEVICE_CLASS,      true },
        { kUSBDeviceSubClass,       USBFILTERIDX_DEVICE_SUB_CLASS,  true },
        { kUSBDeviceProtocol,       USBFILTERIDX_DEVICE_PROTOCOL,   true },
        { "PortNum",                USBFILTERIDX_PORT,              true },
        /// @todo {          ,                USBFILTERIDX_BUS,              true }, - must be derived :-/
        /// Seems to be the upper byte of locationID and our "grand parent" has a USBBusNumber prop.
        { "USB Vendor Name",        USBFILTERIDX_MANUFACTURER_STR,  false },
        { "USB Product Name",       USBFILTERIDX_PRODUCT_STR,       false },
        { "USB Serial Number",      USBFILTERIDX_SERIAL_NUMBER_STR, false },
    };
    for (unsigned i = 0; i < RT_ELEMENTS(s_aProps); i++)
    {
        OSObject *pObj = pProvider->getProperty(s_aProps[i].pszName);
        if (!pObj)
            continue;
        if (s_aProps[i].fNumeric)
        {
            OSNumber *pNum = OSDynamicCast(OSNumber, pObj);
            if (pNum)
            {
                uint16_t u16 = pNum->unsigned16BitValue();
                Log2(("VBoxUSBDevice::probe: %d/%s - %#x (32bit=%#x)\n", i, s_aProps[i].pszName, u16, pNum->unsigned32BitValue()));
                int vrc = USBFilterSetNumExact(&Device, s_aProps[i].enmIdx, u16, true);
                if (RT_FAILURE(vrc))
                    Log(("VBoxUSBDevice::probe: pObj=%p pNum=%p - %d/%s - rc=%d!\n", pObj, pNum, i, s_aProps[i].pszName, vrc));
            }
            else
                Log(("VBoxUSBDevice::probe: pObj=%p pNum=%p - %d/%s!\n", pObj, pNum, i, s_aProps[i].pszName));
        }
        else
        {
            OSString *pStr = OSDynamicCast(OSString, pObj);
            if (pStr)
            {
                Log2(("VBoxUSBDevice::probe: %d/%s - %s\n", i, s_aProps[i].pszName, pStr->getCStringNoCopy()));
                int vrc = USBFilterSetStringExact(&Device, s_aProps[i].enmIdx, pStr->getCStringNoCopy(), true);
                if (RT_FAILURE(vrc))
                    Log(("VBoxUSBDevice::probe: pObj=%p pStr=%p - %d/%s - rc=%d!\n", pObj, pStr, i, s_aProps[i].pszName, vrc));
            }
            else
                Log(("VBoxUSBDevice::probe: pObj=%p pStr=%p - %d/%s\n", pObj, pStr, i, s_aProps[i].pszName));
        }
    }
    /** @todo try figure the blasted bus number */

    /*
     * Run filters on it.
     */
    uintptr_t uId = 0;
    RTPROCESS Owner = VBoxUSBFilterMatch(&Device, &uId);
    USBFilterDelete(&Device);
    if (Owner == NIL_RTPROCESS)
    {
        Log(("VBoxUSBDevice::probe: returns NULL uId=%d\n", uId));
        return NULL;
    }

    /*
     * It matched. Save the owner in the provider registry (hope that works).
     */
    IOService *pRet = IOUSBUserClientInit::probe(pProvider, pi32Score);
    Assert(pRet == this);
    m_Owner = Owner;
    m_uId = uId;
    Log(("%p: m_Owner=%d m_uId=%d\n", this, (int)m_Owner, (int)m_uId));
    *pi32Score = _1G;
    Log(("VBoxUSBDevice::probe: returns %p and *pi32Score=%d\n", pRet, *pi32Score));
    return pRet;
}


/**
 * Try start the device driver.
 *
 * We will do device linking, copy the filter and owner properties from the provider,
 * set the client property, retain the device, and try open (seize) the device.
 *
 * @returns Success indicator.
 * @param   pProvider       The provider instance.
 */
bool
org_virtualbox_VBoxUSBDevice::start(IOService *pProvider)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBDevice::start([%p:{.m_Owner=%d, .m_uId=%p}], %p {%s}) pszProcName=[%s] [retain count: %d]\n",
             this, m_Owner, m_uId, pProvider, pProvider->getName(), pszProcName, getRetainCount()));

    m_pDevice = OSDynamicCast(IOUSBDevice, pProvider);
    if (!m_pDevice)
    {
        printf("VBoxUSBDevice::start([%p], %p {%s}): failed!\n", this, pProvider, pProvider->getName());
        return false;
    }

#ifdef DEBUG
    /* for some extra log messages */
    m_pNotifier = pProvider->registerInterest(gIOGeneralInterest,
                                              &org_virtualbox_VBoxUSBDevice::MyInterestHandler,
                                              this,     /* pvTarget */
                                              NULL);    /* pvRefCon */
#endif

    /*
     * Exploit IOUSBUserClientInit to process IOProviderMergeProperties.
     */
    IOUSBUserClientInit::start(pProvider); /* returns false */

    /*
     * Link ourselves into the list of hijacked device.
     */
    VBOXUSB_LOCK();

    m_pNext = s_pHead;
    s_pHead = this;

    VBOXUSB_UNLOCK();

    /*
     * Set the VBoxUSB properties.
     */
    if (!setProperty(VBOXUSB_OWNER_KEY, (unsigned long long)m_Owner, sizeof(m_Owner) * 8 /* bits */))
        Log(("VBoxUSBDevice::start: failed to set the '" VBOXUSB_OWNER_KEY "' property!\n"));
    if (!setProperty(VBOXUSB_CLIENT_KEY, (unsigned long long)m_Client, sizeof(m_Client) * 8 /* bits */))
        Log(("VBoxUSBDevice::start: failed to set the '" VBOXUSB_CLIENT_KEY "' property!\n"));
    if (!setProperty(VBOXUSB_FILTER_KEY, (unsigned long long)m_uId, sizeof(m_uId) * 8 /* bits */))
        Log(("VBoxUSBDevice::start: failed to set the '" VBOXUSB_FILTER_KEY "' property!\n"));

    /*
     * Retain and open the device.
     */
    m_pDevice->retain();
    m_fOpen = m_pDevice->open(this, kIOServiceSeize, 0);
    if (!m_fOpen)
        Log(("VBoxUSBDevice::start: failed to open the device!\n"));
    m_fOpenOnWasClosed = !m_fOpen;

    Log(("VBoxUSBDevice::start: returns %d\n", true));
    return true;
}


/**
 * Stop the device driver.
 *
 * We'll unlink the device, start device re-enumeration and close it. And call
 * the parent stop method of course.
 *
 * @param   pProvider       The provider instance.
 */
void
org_virtualbox_VBoxUSBDevice::stop(IOService *pProvider)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBDevice::stop([%p], %p {%s}) pszProcName=[%s] [retain count: %d]\n", this, pProvider, pProvider->getName(), pszProcName, getRetainCount()));

    /*
     * Remove ourselves from the list of device.
     */
    VBOXUSB_LOCK();

    org_virtualbox_VBoxUSBDevice *pPrev = NULL;
    for (org_virtualbox_VBoxUSBDevice *pCur = s_pHead; pCur; pCur = pCur->m_pNext)
    {
        if (pCur == this)
        {
            if (pPrev)
                pPrev->m_pNext = m_pNext;
            else
                s_pHead = m_pNext;
            m_pNext = NULL;
            break;
        }
        pPrev = pCur;
    }

    VBOXUSB_UNLOCK();

    /*
     * Should we release the device?
     */
    if (m_fBeingUnloaded)
    {
        if (m_pDevice)
        {
            IOReturn irc = m_pDevice->ReEnumerateDevice(0); NOREF(irc);
            Log(("VBoxUSBDevice::stop([%p], %p {%s}): m_pDevice=%p unload & ReEnumerateDevice -> %#x\n",
                     this, pProvider, pProvider->getName(), m_pDevice, irc));
        }
        else
        {
            IOUSBDevice *pDevice = OSDynamicCast(IOUSBDevice, pProvider);
            if (pDevice)
            {
                IOReturn irc = pDevice->ReEnumerateDevice(0); NOREF(irc);
                Log(("VBoxUSBDevice::stop([%p], %p {%s}): pDevice=%p unload & ReEnumerateDevice -> %#x\n",
                         this, pProvider, pProvider->getName(), pDevice, irc));
            }
            else
                Log(("VBoxUSBDevice::stop([%p], %p {%s}): failed to cast provider to IOUSBDevice\n",
                         this, pProvider, pProvider->getName()));
        }
    }
    else if (m_fReleaseOnClose)
    {
        ASMAtomicWriteBool(&m_fReleaseOnClose, false);
        if (m_pDevice)
        {
            IOReturn irc = m_pDevice->ReEnumerateDevice(0); NOREF(irc);
            Log(("VBoxUSBDevice::stop([%p], %p {%s}): m_pDevice=%p close & ReEnumerateDevice -> %#x\n",
                     this, pProvider, pProvider->getName(), m_pDevice, irc));
        }
    }

    /*
     * Close and release the IOUSBDevice if didn't do that already in message().
     */
    if (m_pDevice)
    {
        /* close it */
        if (m_fOpen)
        {
            m_fOpenOnWasClosed = false;
            m_fOpen = false;
            m_pDevice->close(this, 0);
        }

        /* release it (see start()) */
        m_pDevice->release();
        m_pDevice = NULL;
    }

#ifdef DEBUG
    /* avoid crashing on unload. */
    if (m_pNotifier)
    {
        m_pNotifier->release();
        m_pNotifier = NULL;
    }
#endif

    IOUSBUserClientInit::stop(pProvider);
    Log(("VBoxUSBDevice::stop: returns void\n"));
}


/**
 * Terminate the service (initiate the destruction).
 * @remark  Only for logging.
 */
bool
org_virtualbox_VBoxUSBDevice::terminate(IOOptionBits fOptions)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);

    /* kIOServiceRecursing, kIOServiceRequired, kIOServiceTerminate, kIOServiceSynchronous - interesting option bits */
    Log(("VBoxUSBDevice::terminate([%p], %#x) pszProcName=[%s] [retain count: %d]\n", this, fOptions, pszProcName, getRetainCount()));

    /*
     * There aren't too many reasons why we gets terminated.
     * The most common one is that the device is being unplugged. Another is
     * that we've triggered reenumeration. In both cases we'll get a
     * kIOMessageServiceIsTerminated message before we're stopped.
     *
     * But, when we're unloaded the provider service isn't terminated, and
     * for some funny reason we're frequently causing kernel panics when the
     * device is detached (after we're unloaded). So, for now, let's try
     * re-enumerate it in stop.
     *
     * To avoid creating unnecessary trouble we'll try guess if we're being
     * unloaded from the option bit mask. (kIOServiceRecursing is private btw.)
     */
    /** @todo would be nice if there was a documented way of doing the unload detection this, or
     * figure out what exactly we're doing wrong in the unload scenario. */
    if ((fOptions & 0xffff) == (kIOServiceRequired | kIOServiceSynchronous))
        m_fBeingUnloaded = true;

    bool fRc = IOUSBUserClientInit::terminate(fOptions);
    Log(("VBoxUSBDevice::terminate([%p]): returns %d\n", this, fRc));
    return fRc;
}


/**
 * Intercept open requests and only let Mr. Right (the VM process) open the device.
 * This is where it all gets a bit complicated...
 *
 * @return  Status code.
 *
 * @param   enmMsg          The message number.
 * @param   pProvider       Pointer to the provider instance.
 * @param   pvArg           Message argument.
 */
IOReturn
org_virtualbox_VBoxUSBDevice::message(UInt32 enmMsg, IOService *pProvider, void *pvArg)
{
    Log(("VBoxUSBDevice::message([%p], %#x {%s}, %p {%s}, %p) - pid=%d\n",
             this, enmMsg, DbgGetIOKitMessageName(enmMsg), pProvider, pProvider->getName(), pvArg, RTProcSelf()));

    IOReturn irc;
    switch (enmMsg)
    {
        /*
         * This message is send to the current IOService client from IOService::handleOpen(),
         * expecting it to call pProvider->close() if it agrees to the other party seizing
         * the service. It is also called in IOService::didTerminate() and perhaps some other
         * odd places. The way to find out is to examin the pvArg, which would be including
         * kIOServiceSeize if it's the handleOpen case.
         *
         * How to validate that the other end is actually our VM process? Well, IOKit doesn't
         * provide any clue about the new client really. But fortunately, it seems like the
         * calling task/process context when the VM tries to open the device is the VM process.
         * We'll ASSUME this'll remain like this for now...
         */
        case kIOMessageServiceIsRequestingClose:
            irc = kIOReturnExclusiveAccess;
            /* If it's not a seize request, assume it's didTerminate and pray that it isn't a rouge driver.
               ... weird, doesn't seem to match for the post has-terminated messages. */
            if (!((uintptr_t)pvArg & kIOServiceSeize))
            {
                Log(("VBoxUSBDevice::message([%p],%p {%s}, %p) - pid=%d: not seize - closing...\n",
                         this, pProvider, pProvider->getName(), pvArg, RTProcSelf()));
                m_fOpen = false;
                m_fOpenOnWasClosed = false;
                if (m_pDevice)
                    m_pDevice->close(this, 0);
                m_Client = NIL_RTPROCESS;
                irc = kIOReturnSuccess;
            }
            else
            {
                if (org_virtualbox_VBoxUSBClient::isClientTask(current_task()))
                {
                    Log(("VBoxUSBDevice::message([%p],%p {%s}, %p) - pid=%d task=%p: client process, closing.\n",
                             this, pProvider, pProvider->getName(), pvArg, RTProcSelf(), current_task()));
                    m_fOpen = false;
                    m_fOpenOnWasClosed = false;
                    if (m_pDevice)
                        m_pDevice->close(this, 0);
                    m_fOpenOnWasClosed = true;
                    m_Client = RTProcSelf();
                    irc = kIOReturnSuccess;
                }
                else
                    Log(("VBoxUSBDevice::message([%p],%p {%s}, %p) - pid=%d task=%p: not client process!\n",
                             this, pProvider, pProvider->getName(), pvArg, RTProcSelf(), current_task()));
            }
            if (!setProperty(VBOXUSB_CLIENT_KEY, (unsigned long long)m_Client, sizeof(m_Client) * 8 /* bits */))
                Log(("VBoxUSBDevice::message: failed to set the '" VBOXUSB_CLIENT_KEY "' property!\n"));
            break;

        /*
         * The service was closed by the current client.
         * Update the client property, check for scheduled re-enumeration and re-open.
         *
         * Note that we will not be called if we're doing the closing. (Even if we was
         * called in that case, the code should be able to handle it.)
         */
        case kIOMessageServiceWasClosed:
            /*
             * Update the client property value.
             */
            if (m_Client != NIL_RTPROCESS)
            {
                m_Client = NIL_RTPROCESS;
                if (!setProperty(VBOXUSB_CLIENT_KEY, (unsigned long long)m_Client, sizeof(m_Client) * 8 /* bits */))
                    Log(("VBoxUSBDevice::message: failed to set the '" VBOXUSB_CLIENT_KEY "' property!\n"));
            }

            if (m_pDevice)
            {
                /*
                 * Should we release the device?
                 */
                if (ASMAtomicXchgBool(&m_fReleaseOnClose, false))
                {
                    m_fOpenOnWasClosed = false;
                    irc = m_pDevice->ReEnumerateDevice(0);
                    Log(("VBoxUSBDevice::message([%p], %p {%s}) - ReEnumerateDevice() -> %#x\n",
                             this, pProvider, pProvider->getName(), irc));
                }
                /*
                 * Should we attempt to re-open the device?
                 */
                else if (m_fOpenOnWasClosed)
                {
                    Log(("VBoxUSBDevice::message: attempting to re-open the device...\n"));
                    m_fOpenOnWasClosed = false;
                    m_fOpen = m_pDevice->open(this, kIOServiceSeize, 0);
                    if (!m_fOpen)
                        Log(("VBoxUSBDevice::message: failed to open the device!\n"));
                    m_fOpenOnWasClosed = !m_fOpen;
                }
            }

            irc = IOUSBUserClientInit::message(enmMsg, pProvider, pvArg);
            break;

        /*
         * The IOUSBDevice is shutting down, so close it if we've opened it.
         */
        case kIOMessageServiceIsTerminated:
            m_fBeingUnloaded = false;
            ASMAtomicWriteBool(&m_fReleaseOnClose, false);
            if (m_pDevice)
            {
                /* close it */
                if (m_fOpen)
                {
                    m_fOpen = false;
                    m_fOpenOnWasClosed = false;
                    Log(("VBoxUSBDevice::message: closing the device (%p)...\n", m_pDevice));
                    m_pDevice->close(this, 0);
                }

                /* release it (see start()) */
                Log(("VBoxUSBDevice::message: releasing the device (%p)...\n", m_pDevice));
                m_pDevice->release();
                m_pDevice = NULL;
            }

            irc = IOUSBUserClientInit::message(enmMsg, pProvider, pvArg);
            break;

        default:
            irc = IOUSBUserClientInit::message(enmMsg, pProvider, pvArg);
            break;
    }

    Log(("VBoxUSBDevice::message([%p], %#x {%s}, %p {%s}, %p) -> %#x\n",
             this, enmMsg, DbgGetIOKitMessageName(enmMsg), pProvider, pProvider->getName(), pvArg, irc));
    return irc;
}


/**
 * Schedule all devices belonging to the specified process for release.
 *
 * Devices that aren't currently in use will be released immediately.
 *
 * @param   Owner       The owner process.
 */
/* static */ void
org_virtualbox_VBoxUSBDevice::scheduleReleaseByOwner(RTPROCESS Owner)
{
    Log2(("VBoxUSBDevice::scheduleReleaseByOwner: Owner=%d\n", Owner));
    AssertReturnVoid(Owner && Owner != NIL_RTPROCESS);

    /*
     * Walk the list of devices looking for device belonging to this process.
     *
     * If we release a device, we have to lave the spinlock and will therefore
     * have to restart the search.
     */
    VBOXUSB_LOCK();

    org_virtualbox_VBoxUSBDevice *pCur;
    do
    {
        for (pCur = s_pHead; pCur; pCur = pCur->m_pNext)
        {
            Log2(("VBoxUSBDevice::scheduleReleaseByOwner: pCur=%p m_Owner=%d (%s) m_fReleaseOnClose=%d\n",
                      pCur, pCur->m_Owner, pCur->m_Owner == Owner ? "match" : "mismatch", pCur->m_fReleaseOnClose));
            if (pCur->m_Owner == Owner)
            {
                /* make sure we won't hit it again. */
                pCur->m_Owner = NIL_RTPROCESS;
                IOUSBDevice *pDevice = pCur->m_pDevice;
                if (    pDevice
                    &&  !pCur->m_fReleaseOnClose)
                {
                    pCur->m_fOpenOnWasClosed = false;
                    if (pCur->m_Client != NIL_RTPROCESS)
                    {
                        /* It's currently open, so just schedule it for re-enumeration on close. */
                        ASMAtomicWriteBool(&pCur->m_fReleaseOnClose, true);
                        Log(("VBoxUSBDevice::scheduleReleaseByOwner: %p {%s} - used by %d\n",
                                 pDevice, pDevice->getName(), pCur->m_Client));
                    }
                    else
                    {
                        /*
                         * Get the USBDevice object and do the re-enumeration now.
                         * Retain the device so we don't run into any trouble.
                         */
                        pDevice->retain();
                        VBOXUSB_UNLOCK();

                        IOReturn irc = pDevice->ReEnumerateDevice(0); NOREF(irc);
                        Log(("VBoxUSBDevice::scheduleReleaseByOwner: %p {%s} - ReEnumerateDevice -> %#x\n",
                                 pDevice, pDevice->getName(), irc));

                        pDevice->release();
                        VBOXUSB_LOCK();
                        break;
                    }
                }
            }
        }
    } while (pCur);

    VBOXUSB_UNLOCK();
}


#ifdef DEBUG
/*static*/ IOReturn
org_virtualbox_VBoxUSBDevice::MyInterestHandler(void *pvTarget, void *pvRefCon, UInt32 enmMsgType,
                                                IOService *pProvider, void * pvMsgArg, vm_size_t cbMsgArg)
{
    org_virtualbox_VBoxUSBDevice *pThis = (org_virtualbox_VBoxUSBDevice *)pvTarget;
    if (!pThis)
        return kIOReturnError;

    switch (enmMsgType)
    {
        case kIOMessageServiceIsAttemptingOpen:
            /* pvMsgArg == the open() fOptions, so we could check for kIOServiceSeize if we care.
               We'll also get a kIIOServiceRequestingClose message() for that...  */
            Log(("VBoxUSBDevice::MyInterestHandler: kIOMessageServiceIsAttemptingOpen - pvRefCon=%p pProvider=%p pvMsgArg=%p cbMsgArg=%d\n",
                     pvRefCon, pProvider, pvMsgArg, cbMsgArg));
            break;

        case kIOMessageServiceWasClosed:
            Log(("VBoxUSBDevice::MyInterestHandler: kIOMessageServiceWasClosed - pvRefCon=%p pProvider=%p pvMsgArg=%p cbMsgArg=%d\n",
                     pvRefCon, pProvider, pvMsgArg, cbMsgArg));
            break;

        case kIOMessageServiceIsTerminated:
            Log(("VBoxUSBDevice::MyInterestHandler: kIOMessageServiceIsTerminated - pvRefCon=%p pProvider=%p pvMsgArg=%p cbMsgArg=%d\n",
                     pvRefCon, pProvider, pvMsgArg, cbMsgArg));
            break;

        case kIOUSBMessagePortHasBeenReset:
            Log(("VBoxUSBDevice::MyInterestHandler: kIOUSBMessagePortHasBeenReset - pvRefCon=%p pProvider=%p pvMsgArg=%p cbMsgArg=%d\n",
                     pvRefCon, pProvider, pvMsgArg, cbMsgArg));
            break;

        default:
            Log(("VBoxUSBDevice::MyInterestHandler: %#x (%s) - pvRefCon=%p pProvider=%p pvMsgArg=%p cbMsgArg=%d\n",
                     enmMsgType, DbgGetIOKitMessageName(enmMsgType), pvRefCon, pProvider, pvMsgArg, cbMsgArg));
            break;
    }

    return kIOReturnSuccess;
}
#endif /* DEBUG */


bool
org_virtualbox_VBoxUSBDevice::open(IOService *pForClient, IOOptionBits fOptions, void *pvArg)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBDevice::open([%p], pForClient=%p, fOptions=%#x, pvArg=%p) (1) pszProcName=[%s] [retain count: %d]\n", this, pForClient, fOptions, pvArg, pszProcName, getRetainCount()));
    bool fRc = IOUSBUserClientInit::open(pForClient, fOptions, pvArg);
    Log(("VBoxUSBDevice::open([%p], pForClient=%p, fOptions=%#x, pvArg=%p) (2) returns %d pszProcName=[%s] [retain count: %d]\n", this, pForClient, fOptions, pvArg, fRc, pszProcName, getRetainCount()));
    return fRc;
}

void
org_virtualbox_VBoxUSBDevice::close(IOService *pForClient, IOOptionBits fOptions)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBDevice::close([%p], pForClient=%p, fOptions=%#x) (1) pszProcName=[%s] [retain count: %d]\n", this, pForClient, fOptions, pszProcName, getRetainCount()));
    IOUSBUserClientInit::close(pForClient, fOptions);
    Log(("VBoxUSBDevice::close([%p], pForClient=%p, fOptions=%#x) (2) pszProcName=[%s] [retain count: %d]\n", this, pForClient, fOptions, pszProcName, getRetainCount()));
}

bool
org_virtualbox_VBoxUSBDevice::terminateClient(IOService *pClient, IOOptionBits fOptions)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBDevice::terminateClient([%p], pClient=%p, fOptions=%#x) (1) pszProcName=[%s] [retain count: %d]\n", this, pClient, fOptions, pszProcName, getRetainCount()));
    bool fRc = IOUSBUserClientInit::terminateClient(pClient, fOptions);
    Log(("VBoxUSBDevice::terminateClient([%p], pClient=%p, fOptions=%#x) (2) returns %d pszProcName=[%s] [retain count: %d]\n", this, pClient, fOptions, fRc, pszProcName, getRetainCount()));
    return fRc;
}


bool
org_virtualbox_VBoxUSBDevice::finalize(IOOptionBits fOptions)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBDevice::finalize([%p], fOptions=%#x) (1) pszProcName=[%s] [retain count: %d]\n", this, fOptions, pszProcName, getRetainCount()));
    bool fRc = IOUSBUserClientInit::finalize(fOptions);
    Log(("VBoxUSBDevice::finalize([%p], fOptions=%#x) (2) returns %d pszProcName=[%s] [retain count: %d]\n", this, fOptions, fRc, pszProcName, getRetainCount()));
    return fRc;
}


void
org_virtualbox_VBoxUSBDevice::retain() const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBDevice::retain([%p]) (1) pszProcName=[%s] [retain count: %d]\n", this, pszProcName, getRetainCount()));
    IOUSBUserClientInit::retain();
    Log(("VBoxUSBDevice::retain([%p]) (2) pszProcName=[%s] [retain count: %d]\n", this, pszProcName, getRetainCount()));
}


void
org_virtualbox_VBoxUSBDevice::release(int freeWhen) const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBDevice::release([%p], freeWhen=[%d]) pszProcName=[%s] [retain count: %d]\n", this, freeWhen, pszProcName, getRetainCount()));
    IOUSBUserClientInit::release(freeWhen);
}


void
org_virtualbox_VBoxUSBDevice::taggedRetain(const void *pTag) const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBDevice::taggedRetain([%p], pTag=[%p]) (1) pszProcName=[%s] [retain count: %d]\n", this, pTag, pszProcName, getRetainCount()));
    IOUSBUserClientInit::taggedRetain(pTag);
    Log(("VBoxUSBDevice::taggedRetain([%p], pTag=[%p]) (2) pszProcName=[%s] [retain count: %d]\n", this, pTag, pszProcName, getRetainCount()));
}


void
org_virtualbox_VBoxUSBDevice::taggedRelease(const void *pTag, const int freeWhen) const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBDevice::taggedRelease([%p], pTag=[%p], freeWhen=[%d]) pszProcName=[%s] [retain count: %d]\n", this, pTag, freeWhen, pszProcName, getRetainCount()));
    IOUSBUserClientInit::taggedRelease(pTag, freeWhen);
}












/*
 *
 * org_virtualbox_VBoxUSBInterface
 *
 */

/**
 * Initialize our data members.
 */
bool
org_virtualbox_VBoxUSBInterface::init(OSDictionary *pDictionary)
{
    uint32_t cInstances = ASMAtomicIncU32(&g_cInstances);
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);

    Log(("VBoxUSBInterface::init([%p], %p) new g_cInstances=%d pszProcName=[%s] [retain count: %d]\n", this, pDictionary, cInstances, pszProcName, getRetainCount()));

    m_pInterface = NULL;
    m_fOpen = false;
    m_fOpenOnWasClosed = false;

    return IOUSBUserClientInit::init(pDictionary);
}


/**
 * Free the object.
 * @remark  Only for logging.
 */
void
org_virtualbox_VBoxUSBInterface::free()
{
    uint32_t cInstances = ASMAtomicDecU32(&g_cInstances); NOREF(cInstances);
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBInterfaces::free([%p]) new g_cInstances=%d pszProcName=[%s] [retain count: %d]\n", this, cInstances, pszProcName, getRetainCount()));
    IOUSBUserClientInit::free();
}


/**
 * Probe the interface to see if we're the right driver for it.
 *
 * We implement this similarly to org_virtualbox_VBoxUSBDevice, except that
 * we don't bother matching filters but instead just check if the parent is
 * handled by org_virtualbox_VBoxUSBDevice or not.
 */
IOService *
org_virtualbox_VBoxUSBInterface::probe(IOService *pProvider, SInt32 *pi32Score)
{
    Log(("VBoxUSBInterface::probe([%p], %p {%s}, %p={%d})\n", this,
             pProvider, pProvider->getName(), pi32Score, pi32Score ? *pi32Score : 0));

    /*
     * Check if VBoxUSBDevice is the parent's driver.
     */
    bool fHijackIt = false;
    const IORegistryPlane *pServicePlane = getPlane(kIOServicePlane);
    IORegistryEntry *pParent = pProvider->getParentEntry(pServicePlane);
    if (pParent)
    {
        Log(("VBoxUSBInterface::probe: pParent=%p {%s}\n", pParent, pParent->getName()));

        OSIterator *pSiblings = pParent->getChildIterator(pServicePlane);
        if (pSiblings)
        {
            IORegistryEntry *pSibling;
            while ( (pSibling = OSDynamicCast(IORegistryEntry, pSiblings->getNextObject())) )
            {
                const OSMetaClass *pMetaClass = pSibling->getMetaClass();
                Log2(("sibling: %p - %s - %s\n", pMetaClass, pSibling->getName(), pMetaClass->getClassName()));
                if (pMetaClass == &org_virtualbox_VBoxUSBDevice::gMetaClass)
                {
                    fHijackIt = true;
                    break;
                }
            }
            pSiblings->release();
        }
    }
    if (!fHijackIt)
    {
        Log(("VBoxUSBInterface::probe: returns NULL\n"));
        return NULL;
    }

    IOService *pRet = IOUSBUserClientInit::probe(pProvider, pi32Score);
    *pi32Score = _1G;
    Log(("VBoxUSBInterface::probe: returns %p and *pi32Score=%d - hijack it.\n", pRet, *pi32Score));
    return pRet;
}


/**
 * Start the driver (this), retain and open the USB interface object (pProvider).
 */
bool
org_virtualbox_VBoxUSBInterface::start(IOService *pProvider)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBInterface::start([%p], %p {%s}) pszProcName=[%s] [retain count: %d]\n", this, pProvider, pProvider->getName(), pszProcName, getRetainCount()));

    /*
     * Exploit IOUSBUserClientInit to process IOProviderMergeProperties.
     */
    IOUSBUserClientInit::start(pProvider); /* returns false */

    /*
     * Retain the and open the interface (stop() or message() cleans up).
     */
    bool fRc = true;
    m_pInterface = OSDynamicCast(IOUSBInterface, pProvider);
    if (m_pInterface)
    {
        m_pInterface->retain();
        m_fOpen = m_pInterface->open(this, kIOServiceSeize, 0);
        if (!m_fOpen)
            Log(("VBoxUSBInterface::start: failed to open the interface!\n"));
        m_fOpenOnWasClosed = !m_fOpen;
    }
    else
    {
        printf("VBoxUSBInterface::start([%p], %p {%s}): failed!\n", this, pProvider, pProvider->getName());
        fRc = false;
    }

    Log(("VBoxUSBInterface::start: returns %d\n", fRc));
    return fRc;
}


/**
 * Close and release the USB interface object (pProvider) and stop the driver (this).
 */
void
org_virtualbox_VBoxUSBInterface::stop(IOService *pProvider)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("org_virtualbox_VBoxUSBInterface::stop([%p], %p {%s}) pszProcName=[%s] [retain count: %d]\\n", this, pProvider, pProvider->getName(), pszProcName, getRetainCount()));

    /*
     * Close and release the IOUSBInterface if didn't do that already in message().
     */
    if (m_pInterface)
    {
        /* close it */
        if (m_fOpen)
        {
            m_fOpenOnWasClosed = false;
            m_fOpen = false;
            m_pInterface->close(this, 0);
        }

        /* release it (see start()) */
        m_pInterface->release();
        m_pInterface = NULL;
    }

    IOUSBUserClientInit::stop(pProvider);
    Log(("VBoxUSBInterface::stop: returns void\n"));
}


/**
 * Terminate the service (initiate the destruction).
 * @remark  Only for logging.
 */
bool
org_virtualbox_VBoxUSBInterface::terminate(IOOptionBits fOptions)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    /* kIOServiceRecursing, kIOServiceRequired, kIOServiceTerminate, kIOServiceSynchronous - interesting option bits */
    Log(("VBoxUSBInterface::terminate([%p], %#x) pszProcName=[%s] [retain count: %d]\n", this, fOptions, pszProcName, getRetainCount()));
    bool fRc = IOUSBUserClientInit::terminate(fOptions);
    Log(("VBoxUSBInterface::terminate([%p]): returns %d\n", this, fRc));
    return fRc;
}


/**
 * @copydoc org_virtualbox_VBoxUSBDevice::message
 */
IOReturn
org_virtualbox_VBoxUSBInterface::message(UInt32 enmMsg, IOService *pProvider, void *pvArg)
{
    Log(("VBoxUSBInterface::message([%p], %#x {%s}, %p {%s}, %p)\n",
             this, enmMsg, DbgGetIOKitMessageName(enmMsg), pProvider, pProvider->getName(), pvArg));

    IOReturn irc;
    switch (enmMsg)
    {
        /*
         * See explanation in org_virtualbox_VBoxUSBDevice::message.
         */
        case kIOMessageServiceIsRequestingClose:
            irc = kIOReturnExclusiveAccess;
            if (!((uintptr_t)pvArg & kIOServiceSeize))
            {
                Log(("VBoxUSBInterface::message([%p],%p {%s}, %p) - pid=%d: not seize - closing...\n",
                         this, pProvider, pProvider->getName(), pvArg, RTProcSelf()));
                m_fOpen = false;
                m_fOpenOnWasClosed = false;
                if (m_pInterface)
                    m_pInterface->close(this, 0);
                irc = kIOReturnSuccess;
            }
            else
            {
                if (org_virtualbox_VBoxUSBClient::isClientTask(current_task()))
                {
                    Log(("VBoxUSBInterface::message([%p],%p {%s}, %p) - pid=%d task=%p: client process, closing.\n",
                             this, pProvider, pProvider->getName(), pvArg, RTProcSelf(), current_task()));
                    m_fOpen = false;
                    m_fOpenOnWasClosed = false;
                    if (m_pInterface)
                        m_pInterface->close(this, 0);
                    m_fOpenOnWasClosed = true;
                    irc = kIOReturnSuccess;
                }
                else
                    Log(("VBoxUSBInterface::message([%p],%p {%s}, %p) - pid=%d task=%p: not client process!\n",
                             this, pProvider, pProvider->getName(), pvArg, RTProcSelf(), current_task()));
            }
            break;

        /*
         * The service was closed by the current client, check for re-open.
         */
        case kIOMessageServiceWasClosed:
            if (m_pInterface && m_fOpenOnWasClosed)
            {
                Log(("VBoxUSBInterface::message: attempting to re-open the interface...\n"));
                m_fOpenOnWasClosed = false;
                m_fOpen = m_pInterface->open(this, kIOServiceSeize, 0);
                if (!m_fOpen)
                    Log(("VBoxUSBInterface::message: failed to open the interface!\n"));
                m_fOpenOnWasClosed = !m_fOpen;
            }

            irc = IOUSBUserClientInit::message(enmMsg, pProvider, pvArg);
            break;

        /*
         * The IOUSBInterface/Device is shutting down, so close and release.
         */
        case kIOMessageServiceIsTerminated:
            if (m_pInterface)
            {
                /* close it */
                if (m_fOpen)
                {
                    m_fOpen = false;
                    m_fOpenOnWasClosed = false;
                    m_pInterface->close(this, 0);
                }

                /* release it (see start()) */
                m_pInterface->release();
                m_pInterface = NULL;
            }

            irc = IOUSBUserClientInit::message(enmMsg, pProvider, pvArg);
            break;

        default:
            irc = IOUSBUserClientInit::message(enmMsg, pProvider, pvArg);
            break;
    }

    Log(("VBoxUSBInterface::message([%p], %#x {%s}, %p {%s}, %p) -> %#x\n",
             this, enmMsg, DbgGetIOKitMessageName(enmMsg), pProvider, pProvider->getName(), pvArg, irc));
    return irc;
}

bool
org_virtualbox_VBoxUSBInterface::open(IOService *pForClient, IOOptionBits fOptions, void *pvArg)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBInterface::open([%p], pForClient=%p, fOptions=%#x, pvArg=%p) (1) pszProcName=[%s] [retain count: %d]\n", this, pForClient, fOptions, pvArg, pszProcName, getRetainCount()));
    bool fRc = IOUSBUserClientInit::open(pForClient, fOptions, pvArg);
    Log(("VBoxUSBInterface::open([%p], pForClient=%p, fOptions=%#x, pvArg=%p) (2) returns %d pszProcName=[%s] [retain count: %d]\n", this, pForClient, fOptions, pvArg, fRc, pszProcName, getRetainCount()));
    return fRc;
}

void
org_virtualbox_VBoxUSBInterface::close(IOService *pForClient, IOOptionBits fOptions)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBInterface::close([%p], pForClient=%p, fOptions=%#x) (1) pszProcName=[%s] [retain count: %d]\n", this, pForClient, fOptions, pszProcName, getRetainCount()));
    IOUSBUserClientInit::close(pForClient, fOptions);
    Log(("VBoxUSBInterface::close([%p], pForClient=%p, fOptions=%#x) (2) pszProcName=[%s] [retain count: %d]\n", this, pForClient, fOptions, pszProcName, getRetainCount()));
}

bool
org_virtualbox_VBoxUSBInterface::terminateClient(IOService *pClient, IOOptionBits fOptions)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBInterface::terminateClient([%p], pClient=%p, fOptions=%#x) (1) pszProcName=[%s] [retain count: %d]\n", this, pClient, fOptions, pszProcName, getRetainCount()));
    bool fRc = IOUSBUserClientInit::terminateClient(pClient, fOptions);
    Log(("VBoxUSBInterface::terminateClient([%p], pClient=%p, fOptions=%#x) (2) returns %d pszProcName=[%s] [retain count: %d]\n", this, pClient, fOptions, fRc, pszProcName, getRetainCount()));
    return fRc;
}


bool
org_virtualbox_VBoxUSBInterface::finalize(IOOptionBits fOptions)
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBInterface::finalize([%p], fOptions=%#x) (1) pszProcName=[%s] [retain count: %d]\n", this, fOptions, pszProcName, getRetainCount()));
    bool fRc = IOUSBUserClientInit::finalize(fOptions);
    Log(("VBoxUSBInterface::finalize([%p], fOptions=%#x) (2) returns %d pszProcName=[%s] [retain count: %d]\n", this, fOptions, fRc, pszProcName, getRetainCount()));
    return fRc;
}

void
org_virtualbox_VBoxUSBInterface::retain() const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBInterface::retain([%p]) (1) pszProcName=[%s] [retain count: %d]\n", this, pszProcName, getRetainCount()));
    IOUSBUserClientInit::retain();
    Log(("VBoxUSBInterface::retain([%p]) (2) pszProcName=[%s] [retain count: %d]\n", this, pszProcName, getRetainCount()));
}


void
org_virtualbox_VBoxUSBInterface::release(int freeWhen) const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBInterface::release([%p], freeWhen=[%d]) pszProcName=[%s] [retain count: %d]\n", this, freeWhen, pszProcName, getRetainCount()));
    IOUSBUserClientInit::release(freeWhen);
}


void
org_virtualbox_VBoxUSBInterface::taggedRetain(const void *pTag) const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBInterface::taggedRetain([%p], pTag=[%p]) (1) pszProcName=[%s] [retain count: %d]\n", this, pTag, pszProcName, getRetainCount()));
    IOUSBUserClientInit::taggedRetain(pTag);
    Log(("VBoxUSBInterface::taggedRetain([%p], pTag=[%p]) (2) pszProcName=[%s] [retain count: %d]\n", this, pTag, pszProcName, getRetainCount()));
}


void
org_virtualbox_VBoxUSBInterface::taggedRelease(const void *pTag, const int freeWhen) const
{
    VBOX_RETRIEVE_CUR_PROC_NAME(pszProcName);
    Log(("VBoxUSBInterface::taggedRelease([%p], pTag=[%p], freeWhen=[%d]) pszProcName=[%s] [retain count: %d]\n", this, pTag, freeWhen, pszProcName, getRetainCount()));
    IOUSBUserClientInit::taggedRelease(pTag, freeWhen);
}

