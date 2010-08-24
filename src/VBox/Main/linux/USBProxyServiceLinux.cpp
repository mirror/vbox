/* $Id$ */
/** @file
 * VirtualBox USB Proxy Service, Linux Specialization.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "USBProxyService.h"
#include "Logging.h"

#include <VBox/usb.h>
#include <VBox/usblib.h>
#include <VBox/err.h>

#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/linux/sysfs.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/statfs.h>
#include <sys/poll.h>
#ifdef VBOX_WITH_LINUX_COMPILER_H
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
USBProxyServiceLinux::USBProxyServiceLinux(Host *aHost, const char *aUsbfsRoot /* = "/proc/bus/usb" */)
    : USBProxyService(aHost), mFile(NIL_RTFILE), mStream(NULL), mWakeupPipeR(NIL_RTFILE),
      mWakeupPipeW(NIL_RTFILE), mUsbfsRoot(aUsbfsRoot), mUsingUsbfsDevices(true /* see init */), mUdevPolls(0)
{
    LogFlowThisFunc(("aHost=%p aUsbfsRoot=%p:{%s}\n", aHost, aUsbfsRoot, aUsbfsRoot));
}


/**
 * Initializes the object (called right after construction).
 *
 * @returns S_OK on success and non-fatal failures, some COM error otherwise.
 */
HRESULT USBProxyServiceLinux::init(void)
{
    /*
     * Call the superclass method first.
     */
    HRESULT hrc = USBProxyService::init();
    AssertComRCReturn(hrc, hrc);

    /*
     * We have two methods available for getting host USB device data - using
     * USBFS and using sysfs/hal.  The default choice depends on build-time
     * settings and an environment variable; if the default is not available
     * we fall back to the second.
     * In the event of both failing, the error from the second method tried
     * will be presented to the user.
     */
#ifdef VBOX_WITH_SYSFS_BY_DEFAULT
    mUsingUsbfsDevices = false;
#else
    mUsingUsbfsDevices = true;
#endif
    const char *pszUsbFromEnv = RTEnvGet("VBOX_USB");
    if (pszUsbFromEnv)
    {
        if (!RTStrICmp(pszUsbFromEnv, "USBFS"))
        {
            LogRel(("Default USB access method set to \"usbfs\" from environment\n"));
            mUsingUsbfsDevices = true;
        }
        else if (!RTStrICmp(pszUsbFromEnv, "SYSFS"))
        {
            LogRel(("Default USB method set to \"sysfs\" from environment\n"));
            mUsingUsbfsDevices = false;
        }
        else
            LogRel(("Invalid VBOX_USB environment variable setting \"%s\"\n",
                    pszUsbFromEnv));
    }
    int rc = mUsingUsbfsDevices ? initUsbfs() : initSysfs();
    if (RT_FAILURE(rc))
    {
        /* For the day when we have VBoxSVC release logging... */
        LogRel(("Failed to initialise host USB using %s\n",
                mUsingUsbfsDevices ? "USBFS" : "sysfs/hal"));
        mUsingUsbfsDevices = !mUsingUsbfsDevices;
        rc = mUsingUsbfsDevices ? initUsbfs() : initSysfs();
    }
    LogRel((RT_SUCCESS(rc) ? "Successfully initialised host USB using %s\n"
                           : "Failed to initialise host USB using %s\n",
            mUsingUsbfsDevices ? "USBFS" : "sysfs/hal"));
    mLastError = rc;
    return S_OK;
}


/**
 * Initializiation routine for the usbfs based operation.
 *
 * @returns iprt status code.
 */
int USBProxyServiceLinux::initUsbfs(void)
{
    Assert(mUsingUsbfsDevices);

    /*
     * Open the devices file.
     */
    int rc;
    char *pszDevices;
    RTStrAPrintf(&pszDevices, "%s/devices", mUsbfsRoot.c_str());
    if (pszDevices)
    {
        rc = RTFileOpen(&mFile, pszDevices, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc))
        {
            /*
             * Check that we're actually on the usbfs.
             */
            struct statfs StFS;
            if (!fstatfs(mFile, &StFS))
            {
                if (StFS.f_type == USBDEVICE_SUPER_MAGIC)
                {
                    int pipes[2];
                    if (!pipe(pipes))
                    {
                        mWakeupPipeR = pipes[0];
                        mWakeupPipeW = pipes[1];
                        mStream = fdopen(mFile, "r");
                        if (mStream)
                        {
                            /*
                             * Start the poller thread.
                             */
                            rc = start();
                            if (RT_SUCCESS(rc))
                            {
                                RTStrFree(pszDevices);
                                LogFlowThisFunc(("returns successfully - mFile=%d mStream=%p mWakeupPipeR/W=%d/%d\n",
                                               mFile, mStream, mWakeupPipeR, mWakeupPipeW));
                                /*
                                 * Turn buffering off to work around rewind() problems, see getDevices().
                                 */
                                setvbuf(mStream, NULL, _IONBF, 0);
                                return VINF_SUCCESS;
                            }

                            fclose(mStream);
                            mStream = NULL;
                            mFile = NIL_RTFILE;
                        }
                        else
                        {
                            rc = RTErrConvertFromErrno(errno);
                            Log(("USBProxyServiceLinux::USBProxyServiceLinux: fdopen failed, errno=%d\n", errno));
                        }

                        RTFileClose(mWakeupPipeR);
                        RTFileClose(mWakeupPipeW);
                        mWakeupPipeW = mWakeupPipeR = NIL_RTFILE;
                    }
                    else
                    {
                        rc = RTErrConvertFromErrno(errno);
                        Log(("USBProxyServiceLinux::USBProxyServiceLinux: pipe failed, errno=%d\n", errno));
                    }
                }
                else
                {
                    Log(("USBProxyServiceLinux::USBProxyServiceLinux: StFS.f_type=%d expected=%d\n", StFS.f_type, USBDEVICE_SUPER_MAGIC));
                    rc = VERR_INVALID_PARAMETER;
                }
            }
            else
            {
                rc = RTErrConvertFromErrno(errno);
                Log(("USBProxyServiceLinux::USBProxyServiceLinux: fstatfs failed, errno=%d\n", errno));
            }
            RTFileClose(mFile);
            mFile = NIL_RTFILE;
        }
        RTStrFree(pszDevices);
    }
    else
    {
        rc = VERR_NO_MEMORY;
        Log(("USBProxyServiceLinux::USBProxyServiceLinux: out of memory!\n"));
    }

    LogFlowThisFunc(("returns failure!!! (rc=%Rrc)\n", rc));
    return rc;
}


/**
 * Initializiation routine for the sysfs based operation.
 *
 * @returns iprt status code
 */
int USBProxyServiceLinux::initSysfs(void)
{
    Assert(!mUsingUsbfsDevices);

#ifdef VBOX_USB_WITH_SYSFS
    if (!VBoxMainUSBDevInfoInit(&mDeviceList))
        return VERR_NO_MEMORY;
    int rc = mWaiter.getStatus();
    if (RT_SUCCESS(rc) || rc == VERR_TIMEOUT || rc == VERR_TRY_AGAIN)
        rc = start();
    else if (rc == VERR_NOT_SUPPORTED)
        /* This can legitimately happen if hal or DBus are not running, but of
         * course we can't start in this case. */
        rc = VINF_SUCCESS;
    return rc;

#else  /* !VBOX_USB_WITH_SYSFS */
    return VERR_NOT_IMPLEMENTED;
#endif /* !VBOX_USB_WITH_SYSFS */
}


/**
 * Stop all service threads and free the device chain.
 */
USBProxyServiceLinux::~USBProxyServiceLinux()
{
    LogFlowThisFunc(("\n"));

    /*
     * Stop the service.
     */
    if (isActive())
        stop();

    /*
     * Free resources.
     */
    doUsbfsCleanupAsNeeded();

    /* (No extra work for !mUsingUsbfsDevices.) */
}


/**
 * If any Usbfs-releated resources are currently allocated, then free them
 * and mark them as freed.
 */
void USBProxyServiceLinux::doUsbfsCleanupAsNeeded()
{
    /*
     * Free resources.
     */
    if (mStream)
    {
        fclose(mStream);
        mStream = NULL;
        mFile = NIL_RTFILE;
    }
    else if (mFile != NIL_RTFILE)
    {
        RTFileClose(mFile);
        mFile = NIL_RTFILE;
    }

    if (mWakeupPipeR != NIL_RTFILE)
        RTFileClose(mWakeupPipeR);
    if (mWakeupPipeW != NIL_RTFILE)
        RTFileClose(mWakeupPipeW);
    mWakeupPipeW = mWakeupPipeR = NIL_RTFILE;
}


int USBProxyServiceLinux::captureDevice(HostUSBDevice *aDevice)
{
    Log(("USBProxyServiceLinux::captureDevice: %p {%s}\n", aDevice, aDevice->getName().c_str()));
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    AssertReturn(aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);

    /*
     * Don't think we need to do anything when the device is held... fake it.
     */
    Assert(aDevice->getUnistate() == kHostUSBDeviceState_Capturing);
    interruptWait();

    return VINF_SUCCESS;
}


int USBProxyServiceLinux::releaseDevice(HostUSBDevice *aDevice)
{
    Log(("USBProxyServiceLinux::releaseDevice: %p\n", aDevice));
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    AssertReturn(aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);

    /*
     * We're not really holding it atm., just fake it.
     */
    Assert(aDevice->getUnistate() == kHostUSBDeviceState_ReleasingToHost);
    interruptWait();

    return VINF_SUCCESS;
}


bool USBProxyServiceLinux::updateDeviceState(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters, SessionMachine **aIgnoreMachine)
{
    if (    aUSBDevice->enmState == USBDEVICESTATE_USED_BY_HOST_CAPTURABLE
        &&  aDevice->mUsb->enmState == USBDEVICESTATE_USED_BY_HOST)
        LogRel(("USBProxy: Device %04x:%04x (%s) has become accessible.\n",
                aUSBDevice->idVendor, aUSBDevice->idProduct, aUSBDevice->pszAddress));
    return updateDeviceStateFake(aDevice, aUSBDevice, aRunFilters, aIgnoreMachine);
}


/**
 * A device was added, we need to adjust mUdevPolls.
 *
 * See USBProxyService::deviceAdded for details.
 */
void USBProxyServiceLinux::deviceAdded(ComObjPtr<HostUSBDevice> &aDevice, SessionMachinesList &llOpenedMachines, PUSBDEVICE aUSBDevice)
{
    if (aUSBDevice->enmState == USBDEVICESTATE_USED_BY_HOST)
    {
        LogRel(("USBProxy: Device %04x:%04x (%s) isn't accessible. giving udev a few seconds to fix this...\n",
                aUSBDevice->idVendor, aUSBDevice->idProduct, aUSBDevice->pszAddress));
        mUdevPolls = 10; /* (10 * 500ms = 5s) */
    }

    USBProxyService::deviceAdded(aDevice, llOpenedMachines, aUSBDevice);
}


int USBProxyServiceLinux::wait(RTMSINTERVAL aMillies)
{
    int rc;
    if (mUsingUsbfsDevices)
        rc = waitUsbfs(aMillies);
    else
        rc = waitSysfs(aMillies);
    return rc;
}


/** String written to the wakeup pipe. */
#define WAKE_UP_STRING      "WakeUp!"
/** Length of the string written. */
#define WAKE_UP_STRING_LEN  ( sizeof(WAKE_UP_STRING) - 1 )

int USBProxyServiceLinux::waitUsbfs(RTMSINTERVAL aMillies)
{
    struct pollfd PollFds[2];

    /* Cap the wait interval if we're polling for udevd changing device permissions. */
    if (aMillies > 500 && mUdevPolls > 0)
    {
        mUdevPolls--;
        aMillies = 500;
    }

    memset(&PollFds, 0, sizeof(PollFds));
    PollFds[0].fd        = mFile;
    PollFds[0].events    = POLLIN;
    PollFds[1].fd        = mWakeupPipeR;
    PollFds[1].events    = POLLIN | POLLERR | POLLHUP;

    int rc = poll(&PollFds[0], 2, aMillies);
    if (rc == 0)
        return VERR_TIMEOUT;
    if (rc > 0)
    {
        /* drain the pipe */
        if (PollFds[1].revents & POLLIN)
        {
            char szBuf[WAKE_UP_STRING_LEN];
            rc = RTFileRead(mWakeupPipeR, szBuf, sizeof(szBuf), NULL);
            AssertRC(rc);
        }
        return VINF_SUCCESS;
    }
    return RTErrConvertFromErrno(errno);
}


int USBProxyServiceLinux::waitSysfs(RTMSINTERVAL aMillies)
{
#ifdef VBOX_USB_WITH_SYSFS
    int rc = mWaiter.Wait(aMillies);
    if (rc == VERR_TRY_AGAIN)
    {
        RTThreadYield();
        rc = VINF_SUCCESS;
    }
    return rc;
#else  /* !VBOX_USB_WITH_SYSFS */
    return USBProxyService::wait(aMillies);
#endif /* !VBOX_USB_WITH_SYSFS */
}


int USBProxyServiceLinux::interruptWait(void)
{
#ifdef VBOX_USB_WITH_SYSFS
    LogFlowFunc(("mUsingUsbfsDevices=%d\n", mUsingUsbfsDevices));
    if (!mUsingUsbfsDevices)
    {
        mWaiter.Interrupt();
        LogFlowFunc(("Returning VINF_SUCCESS\n"));
        return VINF_SUCCESS;
    }
#endif /* VBOX_USB_WITH_SYSFS */
    int rc = RTFileWrite(mWakeupPipeW, WAKE_UP_STRING, WAKE_UP_STRING_LEN, NULL);
    if (RT_SUCCESS(rc))
        RTFileFlush(mWakeupPipeW);
    LogFlowFunc(("returning %Rrc\n", rc));
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
int USBProxyServiceLinux::addDeviceToChain(PUSBDEVICE pDev, PUSBDEVICE *ppFirst, PUSBDEVICE **pppNext, int rc)
{
    /* usbDeterminState requires the address. */
    PUSBDEVICE pDevNew = (PUSBDEVICE)RTMemDup(pDev, sizeof(*pDev));
    if (pDevNew)
    {
        RTStrAPrintf((char **)&pDevNew->pszAddress, "%s/%03d/%03d", mUsbfsRoot.c_str(), pDevNew->bBus, pDevNew->bDevNum);
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
                freeDevice(pDevNew);
        }
        else
        {
            freeDevice(pDevNew);
            rc = VERR_NO_MEMORY;
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
        freeDeviceMembers(pDev);
    }

    return rc;
}


/**
 * USBProxyService::getDevices() implementation for usbfs.
 */
PUSBDEVICE USBProxyServiceLinux::getDevicesFromUsbfs(void)
{
    PUSBDEVICE pFirst = NULL;
    if (mStream)
    {
        PUSBDEVICE     *ppNext = NULL;
        int             cHits = 0;
        char            szLine[1024];
        USBDEVICE       Dev;
        RT_ZERO(Dev);
        Dev.enmState = USBDEVICESTATE_UNUSED;

        /*
         * Rewind the stream and make 100% sure we flush the buffer.
         *
         * We've had trouble with rewind() messing up on buffered streams when attaching
         * device clusters such as the Bloomberg keyboard. Therefor the stream is now
         * without a permanent buffer (see the constructor) and we'll employ a temporary
         * stack buffer while parsing the file (speed).
         */
        rewind(mStream);
        char szBuf[1024];
        setvbuf(mStream, szBuf, _IOFBF, sizeof(szBuf));

        int rc = VINF_SUCCESS;
        while (     RT_SUCCESS(rc)
               &&   fgets(szLine, sizeof(szLine), mStream))
        {
            char   *psz;
            char   *pszValue;

            /* validate and remove the trailing newline. */
            psz = strchr(szLine, '\0');
            if (psz[-1] != '\n' && !feof(mStream))
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
             * (Ordered by normal occurence.)
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
                        rc = addDeviceToChain(&Dev, &pFirst, &ppNext, rc);
                    else
                        freeDeviceMembers(&Dev);

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

        /*
         * Add the current entry.
         */
        AssertMsg(cHits >= 3 || cHits == 0, ("cHits=%d\n", cHits));
        if (cHits >= 3)
            rc = addDeviceToChain(&Dev, &pFirst, &ppNext, rc);

        /*
         * Success?
         */
        if (RT_FAILURE(rc))
        {
            LogFlow(("USBProxyServiceLinux::getDevices: rc=%Rrc\n", rc));
            while (pFirst)
            {
                PUSBDEVICE pFree = pFirst;
                pFirst = pFirst->pNext;
                freeDevice(pFree);
            }
        }

        /*
         * Turn buffering off to detach it from the local buffer and to
         * make subsequent rewind() calls work correctly.
         */
        setvbuf(mStream, NULL, _IONBF, 0);
    }
    return pFirst;
}

#ifdef VBOX_USB_WITH_SYSFS

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

/**
 * USBProxyService::getDevices() implementation for sysfs.
 */
PUSBDEVICE USBProxyServiceLinux::getDevicesFromSysfs(void)
{
#ifdef VBOX_USB_WITH_SYSFS
    USBDevInfoUpdateDevices(&mDeviceList);
    /* Add each of the devices found to the chain. */
    PUSBDEVICE pFirst = NULL;
    PUSBDEVICE pLast  = NULL;
    int        rc     = VINF_SUCCESS;
    USBDeviceInfoList_iterator it;
    USBDeviceInfoList_iter_init(&it, USBDevInfoBegin(&mDeviceList));
    for (;    RT_SUCCESS(rc)
           && !USBDeviceInfoList_iter_eq(&it, USBDevInfoEnd(&mDeviceList));
           USBDeviceInfoList_iter_incr(&it))
    {
        USBDEVICE *Dev = (USBDEVICE *)RTMemAllocZ(sizeof(USBDEVICE));
        if (!Dev)
            rc = VERR_NO_MEMORY;
        if (RT_SUCCESS(rc))
        {
            const char *pszSysfsPath = USBDeviceInfoList_iter_target(&it)->mSysfsPath;

            /* Fill in the simple fields */
            Dev->enmState = USBDEVICESTATE_UNUSED;
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
            USBInterfaceList_iterator it2;
            USBDeviceInfo *udi = USBDeviceInfoList_iter_target(&it);
            USBInterfaceList_iter_init(&it2, USBInterfaceList_begin(&udi->mInterfaces));
            for (; !USBInterfaceList_iter_eq(&it2, USBInterfaceList_end(&udi->mInterfaces));
                   USBInterfaceList_iter_incr(&it2))
            {
                ssize_t cb = RTLinuxSysFsGetLinkDest(szBuf, sizeof(szBuf), "%s/driver",
                                                     USBInterfaceList_iter_target(&it2));
                if (cb > 0 && Dev->enmState != USBDEVICESTATE_UNSUPPORTED)
                    Dev->enmState = (strcmp(szBuf, "hub") == 0)
                                  ? USBDEVICESTATE_UNSUPPORTED
                                  : USBDEVICESTATE_USED_BY_HOST_CAPTURABLE;
                if (RTLinuxSysFsReadIntFile(16, "%s/bInterfaceClass",
                                            USBInterfaceList_iter_target(&it2)) == 9 /* hub */)
                    Dev->enmState = USBDEVICESTATE_UNSUPPORTED;
            }

            /* We want a copy of the device node and sysfs paths guaranteed not to
             * contain double slashes, since we use a double slash as a separator in
             * the pszAddress field. */
            char szDeviceClean[RTPATH_MAX];
            char szSysfsClean[RTPATH_MAX];
            char *pszAddress = NULL;
            if (   RT_SUCCESS(RTPathReal(USBDeviceInfoList_iter_target(&it)->mDevice, szDeviceClean,
                                         sizeof(szDeviceClean)))
                && RT_SUCCESS(RTPathReal(pszSysfsPath, szSysfsClean,
                                         sizeof(szSysfsClean)))
               )
                RTStrAPrintf(&pszAddress, "sysfs:%s//device:%s", szSysfsClean,
                             szDeviceClean);
            Dev->pszAddress = pszAddress;

            /* Work out from the data collected whether we can support this device. */
            Dev->enmState = usbDeterminState(Dev);
            usbLogDevice(Dev);
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
            freeDevice(Dev);
    }
    if (RT_FAILURE(rc))
        while (pFirst)
        {
            PUSBDEVICE pNext = pFirst->pNext;
            freeDevice(pFirst);
            pFirst = pNext;
        }

    /* Eliminate any duplicates.  This was originally a sanity check, but it
     * turned out that hal can get confused and return devices twice. */
    for (PUSBDEVICE pDev = pFirst; pDev != NULL; pDev = pDev->pNext)
        for (PUSBDEVICE pDev2 = pDev; pDev2 != NULL && pDev2->pNext != NULL;
             pDev2 = pDev2->pNext)
            while (   pDev2->pNext != NULL
                   && RTStrCmp(pDev->pszAddress, pDev2->pNext->pszAddress) == 0)
            {
                PUSBDEVICE pDup = pDev2->pNext;
                pDev2->pNext = pDup->pNext;
                freeDevice(pDup);
            }
    return pFirst;
#else  /* !VBOX_USB_WITH_SYSFS */
    return NULL;
#endif  /* !VBOX_USB_WITH_SYSFS */
}


PUSBDEVICE USBProxyServiceLinux::getDevices(void)
{
    PUSBDEVICE pDevices;
    if (mUsingUsbfsDevices)
        pDevices = getDevicesFromUsbfs();
    else
        pDevices = getDevicesFromSysfs();
    return pDevices;
}

