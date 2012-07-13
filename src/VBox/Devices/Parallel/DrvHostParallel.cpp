/* $Id$ */
/** @file
 * VirtualBox Host Parallel Port Driver.
 *
 * Initial Linux-only code contributed by: Alexander Eichner
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_HOST_PARALLEL
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmthread.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/pipe.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/uuid.h>
#include <iprt/cdefs.h>
#include <iprt/ctype.h>

#ifdef RT_OS_LINUX
# include <sys/ioctl.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/poll.h>
# include <fcntl.h>
# include <unistd.h>
# include <linux/ppdev.h>
# include <linux/parport.h>
# include <errno.h>
#endif


/** @def VBOX_WITH_WIN_PARPORT_SUP *
 * Indicates whether to use the generic direct hardware access or host specific
 * code to access the parallel port.
 */
#if defined(RT_OS_LINUX)
# undef VBOX_WITH_WIN_PARPORT_SUP
#elif defined(RT_OS_WINDOWS)
//# define VBOX_WITH_WIN_PARPORT_SUP
#else
# error "Not ported"
#endif

#if defined(VBOX_WITH_WIN_PARPORT_SUP) && defined(IN_RING0)
# include <iprt/asm-amd64-x86.h>
#endif

#if defined(VBOX_WITH_WIN_PARPORT_SUP) && defined(IN_RING3)
# include <Windows.h>
# include <setupapi.h>
# include <cfgmgr32.h>
# include <iprt/mem.h>
# include <iprt/string.h>
#endif

#include "VBoxDD.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Host parallel port driver instance data.
 * @implements PDMIHOSTPARALLELCONNECTOR
 */
typedef struct DRVHOSTPARALLEL
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                    pDrvIns;
    /** Pointer to the driver instance. */
    PPDMDRVINSR3                  pDrvInsR3;
    PPDMDRVINSR0                  pDrvInsR0;
    /** Pointer to the char port interface of the driver/device above us. */
    PPDMIHOSTPARALLELPORT         pDrvHostParallelPort;
    /** Our host device interface. */
    PDMIHOSTPARALLELCONNECTOR     IHostParallelConnector;
    /** Our host device interface. */
    PDMIHOSTPARALLELCONNECTOR     IHostParallelConnectorR3;
    /** Device Path */
    char                         *pszDevicePath;
    /** Device Handle */
    RTFILE                        hFileDevice;
#ifndef VBOX_WITH_WIN_PARPORT_SUP
    /** Thread waiting for interrupts. */
    PPDMTHREAD                    pMonitorThread;
    /** Wakeup pipe read end. */
    RTPIPE                        hWakeupPipeR;
    /** Wakeup pipe write end. */
    RTPIPE                        hWakeupPipeW;
    /** Current mode the parallel port is in. */
    PDMPARALLELPORTMODE           enmModeCur;
#endif

#ifdef VBOX_WITH_WIN_PARPORT_SUP
    /** Data register. */
    uint32_t                      u32LptAddr;
    /** Status register. */
    uint32_t                      u32LptAddrStatus;
    /** Control register.  */
    uint32_t                      u32LptAddrControl;
    /** Data read buffer. */
    uint8_t                       u8ReadIn;
    /** Control read buffer. */
    uint8_t                       u8ReadInControl;
    /** Status read buffer. */
    uint8_t                       u8ReadInStatus;
	/** Parallel port name */
    /** @todo r=bird: This is an array, they start with 'a'. However, this seems
     * to be a string, and they are not uint8_t but char and are prefixed with 'sz'
     * in char array form.
     *
     * Also, the use of tabs as indentation in anything but Makefiles is
     * forbidden.  That is not a guideline, it's the law. :-) (See above line.) */
    uint8_t                       u8ParportName[6];
    /** Whether the parallel port is available or not. */
    bool                          fParportAvail;
#endif /* VBOX_WITH_WIN_PARPORT_SUP */
} DRVHOSTPARALLEL, *PDRVHOSTPARALLEL;


/**
 * Ring-0 operations.
 */
typedef enum DRVHOSTPARALLELR0OP
{
    /** Invalid zero value. */
    DRVHOSTPARALLELR0OP_INVALID = 0,
    /** Perform R0 initialization. */
    DRVHOSTPARALLELR0OP_INITR0STUFF,
    /** Read data. */
    DRVHOSTPARALLELR0OP_READ,
    /** Read status register. */
    DRVHOSTPARALLELR0OP_READSTATUS,
    /** Read control register. */
    DRVHOSTPARALLELR0OP_READCONTROL,
    /** Write data. */
    DRVHOSTPARALLELR0OP_WRITE,
    /** Write control register. */
    DRVHOSTPARALLELR0OP_WRITECONTROL,
    /** Set port direction. */
    DRVHOSTPARALLELR0OP_SETPORTDIRECTION
} DRVHOSTPARALLELR0OP;

/** Converts a pointer to DRVHOSTPARALLEL::IHostDeviceConnector to a PDRHOSTPARALLEL. */
#define PDMIHOSTPARALLELCONNECTOR_2_DRVHOSTPARALLEL(pInterface) ( (PDRVHOSTPARALLEL)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTPARALLEL, CTX_SUFF(IHostParallelConnector))) )


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define CTRL_REG_OFFSET                 2
#define STATUS_REG_OFFSET               1
#define LPT_CONTROL_ENABLE_BIDIRECT     0x20



#ifdef VBOX_WITH_WIN_PARPORT_SUP
# ifdef IN_RING0

/**
 * R0 mode function to write byte value to data port.
 * @returns VBox status code.
 * @param   pDrvIns    Driver instance.
 * @param   u64Arg     Data to be written to data register.
 *
 */
static int drvR0HostParallelReqWrite(PPDMDRVINS pDrvIns, uint64_t u64Arg)
{
    PDRVHOSTPARALLEL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);
    LogFlowFunc(("write to data port=%#x val=%#x\n", pThis->u32LptAddr, u64Arg));
    ASMOutU8(pThis->u32LptAddr, (uint8_t)(u64Arg));
    return VINF_SUCCESS;
}

/**
 * R0 mode function to write byte value to parallel port control
 * register.
 * @returns VBox status code.
 * @param   pDrvIns     Driver instance.
 * @param   u64Arg      Data to be written to control register.
 */
static int drvR0HostParallelReqWriteControl(PPDMDRVINS pDrvIns, uint64_t u64Arg)
{
    PDRVHOSTPARALLEL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);
    LogFlowFunc(("write to ctrl port=%#x val=%#x\n", pThis->u32LptAddrControl, u64Arg));
    ASMOutU8(pThis->u32LptAddrControl, (uint8_t)(u64Arg));
    return VINF_SUCCESS;
}

/**
 * R0 mode function to ready byte value from the parallel port
 * data register
 * @returns VBox status code.
 * @param   pDrvIns    Driver instance.
 * @param   u64Arg     Not used.
 */
static int drvR0HostParallelReqRead(PPDMDRVINS pDrvIns, uint64_t u64Arg)
{
    uint8_t u8Data;
    PDRVHOSTPARALLEL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);
    u8Data = ASMInU8(pThis->u32LptAddr);
    LogFlowFunc(("read from data port=%#x val=%#x\n", pThis->u32LptAddr, u8Data));
    pThis->u8ReadIn = u8Data;
    return VINF_SUCCESS;
}

/**
 * R0 mode function to ready byte value from the parallel port
 * control register.
 * @returns VBox status code.
 * @param   pDrvIns    Driver instance.
 * @param   u64Arg     Not used.
 */
static int drvR0HostParallelReqReadControl(PPDMDRVINS pDrvIns, uint64_t u64Arg)
{
    uint8_t u8Data;
    PDRVHOSTPARALLEL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);
    u8Data = ASMInU8(pThis->u32LptAddrControl);
    LogFlowFunc(("read from ctrl port=%#x val=%#x\n", pThis->u32LptAddr, u8Data));
    pThis->u8ReadInControl = u8Data;
    return VINF_SUCCESS;
}

/**
 * R0 mode function to ready byte value from the parallel port
 * status register.
 * @returns VBox status code.
 * @param   pDrvIns    Driver instance.
 * @param   u64Arg     Not used.
 */
static int drvR0HostParallelReqReadStatus(PPDMDRVINS pDrvIns, uint64_t u64Arg)
{
    uint8_t u8Data;
    PDRVHOSTPARALLEL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);
    u8Data = ASMInU8(pThis->u32LptAddrStatus);
    LogFlowFunc(("read from status port=%#x val=%#x\n", pThis->u32LptAddr, u8Data));
    pThis->u8ReadInStatus = u8Data;
    return VINF_SUCCESS;
}

/**
 * R0 mode function to set the direction of parallel port -
 * operate in bidirectional mode or single direction.
 * @returns VBox status code.
 * @param   pDrvIns    Driver instance.
 * @param   u64Arg     Mode.
 */
static int drvR0HostParallelReqSetPortDir(PPDMDRVINS pDrvIns, uint64_t u64Arg)
{
    PDRVHOSTPARALLEL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);
    uint8_t u8ReadControlVal;
    uint8_t u8WriteControlVal;

    if (u64Arg)
    {
       u8ReadControlVal = ASMInU8(pThis->u32LptAddrControl);
       u8WriteControlVal = u8ReadControlVal | LPT_CONTROL_ENABLE_BIDIRECT; /* enable input direction */
       ASMOutU8(pThis->u32LptAddrControl, u8WriteControlVal);
    }
    else
    {
        u8ReadControlVal = ASMInU8(pThis->u32LptAddrControl);
        u8WriteControlVal = u8ReadControlVal & ~LPT_CONTROL_ENABLE_BIDIRECT; /* disable input direction */
        ASMOutU8(pThis->u32LptAddrControl, u8WriteControlVal);
    }
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{FNPDMDRVREQHANDLERR0}
 */
PDMBOTHCBDECL(int) drvR0HostParallelReqHandler(PPDMDRVINS pDrvIns, uint32_t uOperation, uint64_t u64Arg)
{
    int rc;

    LogFlowFuncEnter();
    /* I have included break after each case. Need to work on this. */
    switch ((DRVHOSTPARALLELR0OP)uOperation)
    {
        case DRVHOSTPARALLELR0OP_READ:
            rc = drvR0HostParallelReqRead(pDrvIns, u64Arg);
            break;
        case DRVHOSTPARALLELR0OP_READSTATUS:
            rc = drvR0HostParallelReqReadStatus(pDrvIns, u64Arg);
            break;
        case DRVHOSTPARALLELR0OP_READCONTROL:
            rc = drvR0HostParallelReqReadControl(pDrvIns, u64Arg);
            break;
        case DRVHOSTPARALLELR0OP_WRITE:
            rc = drvR0HostParallelReqWrite(pDrvIns, u64Arg);
            break;
        case DRVHOSTPARALLELR0OP_WRITECONTROL:
            rc = drvR0HostParallelReqWriteControl(pDrvIns, u64Arg);
            break;
        case DRVHOSTPARALLELR0OP_SETPORTDIRECTION:
            rc = drvR0HostParallelReqSetPortDir(pDrvIns, u64Arg);
            break;
        default:        /* not supported */
            rc = VERR_NOT_SUPPORTED;
    }
    LogFlowFuncLeave();
    return rc;
}

# endif /* IN_RING0 */
#endif /* VBOX_WITH_WIN_PARPORT_SUP */

#ifdef IN_RING3
# ifdef VBOX_WITH_WIN_PARPORT_SUP

/**
 * Find IO port range for the parallel port and return the lower address.
 *
 * @returns parallel port IO address.
 * @param   DevInst    Device Instance for parallel port.
 */
static uint32_t drvHostWinFindIORangeResource(const DEVINST DevInst)
{
    uint8_t  *pBuf  = NULL;
    short     wHeaderSize;
    uint32_t  u32Size;
    CONFIGRET cmRet;
    LOG_CONF  firstLogConf;
    LOG_CONF  nextLogConf;
    RES_DES   rdPrevResDes;
    uint32_t  u32ParportAddr;

    wHeaderSize = sizeof(IO_DES);
    cmRet = CM_Get_First_Log_Conf(&firstLogConf, DevInst, ALLOC_LOG_CONF);
    if (cmRet != CR_SUCCESS)
    {
        cmRet = CM_Get_First_Log_Conf(&firstLogConf, DevInst, BOOT_LOG_CONF);
        if (cmRet != CR_SUCCESS)
            return 0;
    }
    cmRet = CM_Get_Next_Res_Des(&nextLogConf, firstLogConf, 2, 0L, 0L);
    if (cmRet != CR_SUCCESS)
    {
        CM_Free_Res_Des_Handle(firstLogConf);
        return 0;
    }

    for (;;)
    {
        u32Size = 0;
        cmRet = CM_Get_Res_Des_Data_Size((PULONG)(&u32Size), nextLogConf, 0L);
        if (cmRet != CR_SUCCESS)
        {
            CM_Free_Res_Des_Handle(nextLogConf); /** @todo r=bird: Why are you doing this twice in this code path? */
            break;
        }
        pBuf = (uint8_t *)RTMemAlloc(u32Size + 1);
        if (!pBuf)
        {
            CM_Free_Res_Des_Handle(nextLogConf); /** @todo r=bird: Ditto above. */
            break;
        }
        cmRet = CM_Get_Res_Des_Data(nextLogConf, pBuf, u32Size, 0L);
        if (cmRet != CR_SUCCESS)
        {
            CM_Free_Res_Des_Handle(nextLogConf);
            RTMemFree(pBuf);
            break;
        }
        LogFlowFunc(("call GetIOResource\n"));
        u32ParportAddr = ((IO_DES *)pBuf)->IOD_Alloc_Base;
        LogFlowFunc(("called GetIOResource, ret=%#x\n", u32ParportAddr));
        rdPrevResDes = 0;
        cmRet = CM_Get_Next_Res_Des(&rdPrevResDes,
                                    nextLogConf,
                                    2,
                                    0L,
                                    0L);
        RTMemFree(pBuf);
        if (cmRet != CR_SUCCESS)
           break;

        CM_Free_Res_Des_Handle(nextLogConf);
        nextLogConf = rdPrevResDes;
    }
    CM_Free_Res_Des_Handle(nextLogConf);
    LogFlowFunc(("return u32ParportAddr=%#x", u32ParportAddr));
    return u32ParportAddr;
}

/**
 * Get Parallel port address and update the shared data
 * structure.
 * @returns VBox status code.
 * @param   pThis    The host parallel port instance data.
 */
static int drvWinHostGetparportAddr(PDRVHOSTPARALLEL pThis)
{
    HDEVINFO hDevInfo;
    SP_DEVINFO_DATA DeviceInfoData;
    uint32_t u32Idx;
    uint32_t u32ParportAddr;
    int rc = VINF_SUCCESS;

    hDevInfo = SetupDiGetClassDevs(NULL, 0, 0, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (hDevInfo == INVALID_HANDLE_VALUE)
        return VERR_INVALID_HANDLE;

    /* Enumerate through all devices in Set. */
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    for (u32Idx = 0; SetupDiEnumDeviceInfo(hDevInfo, u32Idx, &DeviceInfoData); u32Idx++)
    {
        uint32_t u32DataType; /** @todo r=bird: why do you use uint32_t here when the function wants a DWORD? */
        uint8_t *pBuf = NULL;
        uint32_t u32BufSize = 0; /** @todo r=bird: ditto */

        while (!SetupDiGetDeviceRegistryProperty(hDevInfo, &DeviceInfoData, SPDRP_FRIENDLYNAME,
                                                 (PDWORD)&u32DataType, (uint8_t *)pBuf,
                                                 u32BufSize, (PDWORD)&u32BufSize)
               && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            if (pBuf)
                 RTMemFree(pBuf);
            /* Max size will never be more than 2048 bytes */
            pBuf = (uint8_t *)RTMemAlloc(u32BufSize * 2);
        }

        if (pBuf) /** @todo r=bird: You're not checking errors here. */
        {
            /** @todo r=bird: The indent of the block is wrong... More importantely, the
             * scope and purpose of the two variables would be clearer if you moved them
             * to the RTStrStr calls further down. */
             char *pCh = NULL;
             char* pTmpCh = NULL;
             if (RTStrStr((char*)pBuf, "LPT"))
             {
                 u32ParportAddr = drvHostWinFindIORangeResource(DeviceInfoData.DevInst);
                 if (u32ParportAddr)
                 {
                     /* Find parallel port name and update the shared data struncture */
                     pCh = RTStrStr((char*)pBuf, "(");
                     pTmpCh = RTStrStr((char *)pBuf, ")");
                     /* check for the confirmation for the availability of parallel port */
                     if (!(pCh && pTmpCh))
                     {
                         LogFlowFunc(("Parallel port Not Found. \n"));
                         return VERR_NOT_FOUND;

                     }
                     if (((pTmpCh - (char *)pBuf) - (pCh - (char *)pBuf)) < 0) {
                         LogFlowFunc(("Parallel port string not properly formatted.\n"));
                         return VERR_NOT_FOUND;
                     }
                     /* check for the confirmation for the availability of parallel port */
                     if (RTStrCopyEx((char *)(pThis->u8ParportName), sizeof(pThis->u8ParportName),
                                      pCh+1, ((pTmpCh - (char *)pBuf) - (pCh - (char *)pBuf)) - 1))
                     {
                         LogFlowFunc(("Parallel Port Not Found.\n"));
                         return VERR_NOT_FOUND;
                     }
                     *((char *)pThis->u8ParportName + (pTmpCh - (char *)pBuf) - (pCh - (char *)pBuf) + 1 ) = '\0';

                     /* checking again to make sure that we have got a valid name and in valid format too. */
                     if (RTStrNCmp((char *)pThis->u8ParportName, "LPT", 3)) {
                         LogFlowFunc(("Parallel Port name \"LPT\" Not Found.\n"));
                         return VERR_NOT_FOUND;
                     }

                     /** @todo r=bird: Multiline expressions starts the next line with the
                      * operator, (instead of the previous line ending with an operator like you do
                      * here).
                      *
                      * Also, note that the opening curly brackets have it's own line in this
                      * file, so you do the same as the original author(s) of the file. */
                     if (!RTStrStr((char *)pThis->u8ParportName, "LPT") ||
                          !(pThis->u8ParportName[3] >= '0' && pThis->u8ParportName[3] <= '9')) {
                         RT_BZERO(pThis->u8ParportName, sizeof(pThis->u8ParportName));
                         LogFlowFunc(("Printer Port Name Not Found.\n"));
                         return VERR_NOT_FOUND;
                     }
                     pThis->fParportAvail     = true;
                     pThis->u32LptAddr        = u32ParportAddr;
                     pThis->u32LptAddrControl = pThis->u32LptAddr + CTRL_REG_OFFSET;
                     pThis->u32LptAddrStatus  = pThis->u32LptAddr + STATUS_REG_OFFSET;
                 }
                 if (pThis->fParportAvail)
                     break;
             }
        }
        if (pBuf)
            RTMemFree(pBuf);
        if (pThis->fParportAvail)
        {
            /* Parallel port address has been found. No need to iterate further. */
            break;
        }
    }

    if (GetLastError() != NO_ERROR && GetLastError() != ERROR_NO_MORE_ITEMS)
        rc =  VERR_GENERAL_FAILURE;

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return rc;

}
# endif /* VBOX_WITH_WIN_PARPORT_SUP */

/**
 * Changes the current mode of the host parallel port.
 *
 * @returns VBox status code.
 * @param   pThis    The host parallel port instance data.
 * @param   enmMode  The mode to change the port to.
 */
static int drvHostParallelSetMode(PDRVHOSTPARALLEL pThis, PDMPARALLELPORTMODE enmMode)
{
    int iMode = 0;
    int rc = VINF_SUCCESS;
    LogFlowFunc(("mode=%d\n", enmMode));

# ifndef VBOX_WITH_WIN_PARPORT_SUP
    int rcLnx;
    if (pThis->enmModeCur != enmMode)
    {
        switch (enmMode)
        {
            case PDM_PARALLEL_PORT_MODE_SPP:
                iMode = IEEE1284_MODE_COMPAT;
                break;
            case PDM_PARALLEL_PORT_MODE_EPP_DATA:
                iMode = IEEE1284_MODE_EPP | IEEE1284_DATA;
                break;
            case PDM_PARALLEL_PORT_MODE_EPP_ADDR:
                iMode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
                break;
            case PDM_PARALLEL_PORT_MODE_ECP:
            case PDM_PARALLEL_PORT_MODE_INVALID:
            default:
                return VERR_NOT_SUPPORTED;
        }

        rcLnx = ioctl(RTFileToNative(pThis->hFileDevice), PPSETMODE, &iMode);
        if (RT_UNLIKELY(rcLnx < 0))
            rc = RTErrConvertFromErrno(errno);
        else
            pThis->enmModeCur = enmMode;
    }

    return rc;
# else  /* VBOX_WITH_WIN_PARPORT_SUP */
    return VINF_SUCCESS;
# endif /* VBOX_WITH_WIN_PARPORT_SUP */
}

/* -=-=-=-=- IBase -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostParallelQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS          pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTPARALLEL    pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTPARALLELCONNECTOR, &pThis->CTX_SUFF(IHostParallelConnector));
    return NULL;
}


/* -=-=-=-=- IHostDeviceConnector -=-=-=-=- */

/** @copydoc PDMICHARCONNECTOR::pfnWrite */
static DECLCALLBACK(int) drvHostParallelWrite(PPDMIHOSTPARALLELCONNECTOR pInterface, const void *pvBuf, size_t cbWrite, PDMPARALLELPORTMODE enmMode)
{
    PPDMDRVINS          pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    //PDRVHOSTPARALLEL    pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);
    PDRVHOSTPARALLEL    pThis   = RT_FROM_MEMBER(pInterface, DRVHOSTPARALLEL, CTX_SUFF(IHostParallelConnector));
    int rc = VINF_SUCCESS;
    int rcLnx = 0;

    LogFlowFunc(("pvBuf=%#p cbWrite=%d\n", pvBuf, cbWrite));

    rc = drvHostParallelSetMode(pThis, enmMode);
    if (RT_FAILURE(rc))
        return rc;
# ifndef VBOX_WITH_WIN_PARPORT_SUP
    if (enmMode == PDM_PARALLEL_PORT_MODE_SPP)
    {
        /* Set the data lines directly. */
        rcLnx = ioctl(RTFileToNative(pThis->hFileDevice), PPWDATA, pvBuf);
    }
    else
    {
        /* Use write interface. */
        rcLnx = write(RTFileToNative(pThis->hFileDevice), pvBuf, cbWrite);
    }
    if (RT_UNLIKELY(rcLnx < 0))
        rc = RTErrConvertFromErrno(errno);
# else /* VBOX_WITH_WIN_PARPORT_SUP */
    /** @todo r=klaus this code assumes cbWrite==1, which may not be guaranteed forever */
    uint64_t u64Data;
    u64Data = (uint8_t) *((uint8_t *)(pvBuf));
    LogFlowFunc(("calling R0 to write to parallel port, data=%#x\n", u64Data));
    if (pThis->fParportAvail)
    {
        rc = PDMDrvHlpCallR0(pThis->CTX_SUFF(pDrvIns), DRVHOSTPARALLELR0OP_WRITE, u64Data);
        AssertRC(rc);
    }
# endif /* VBOX_WITH_WIN_PARPORT_SUP */
    return rc;
}

/**
 * @interface_method_impl{PDMIBASE,pfnRead}
 */
static DECLCALLBACK(int) drvHostParallelRead(PPDMIHOSTPARALLELCONNECTOR pInterface, void *pvBuf, size_t cbRead, PDMPARALLELPORTMODE enmMode)
{
    PDRVHOSTPARALLEL    pThis   = RT_FROM_MEMBER(pInterface, DRVHOSTPARALLEL, CTX_SUFF(IHostParallelConnector));
    int rc = VINF_SUCCESS;

# ifndef VBOX_WITH_WIN_PARPORT_SUP
    int rcLnx = 0;
    LogFlowFunc(("pvBuf=%#p cbRead=%d\n", pvBuf, cbRead));

    rc = drvHostParallelSetMode(pThis, enmMode);
    if (RT_FAILURE(rc))
        return rc;

    if (enmMode == PDM_PARALLEL_PORT_MODE_SPP)
    {
        /* Set the data lines directly. */
        rcLnx = ioctl(RTFileToNative(pThis->hFileDevice), PPWDATA, pvBuf);
    }
    else
    {
        /* Use write interface. */
        rcLnx = read(RTFileToNative(pThis->hFileDevice), pvBuf, cbRead);
    }
    if (RT_UNLIKELY(rcLnx < 0))
        rc = RTErrConvertFromErrno(errno);
# else  /* VBOX_WITH_WIN_PARPORT_SUP */
    /** @todo r=klaus this code assumes cbRead==1, which may not be guaranteed forever */
    *((uint8_t*)(pvBuf)) = 0; /* Initialize the buffer. */
    LogFlowFunc(("calling R0 to read from parallel port\n"));
    if (pThis->fParportAvail)
    {
        int rc = PDMDrvHlpCallR0(pThis->CTX_SUFF(pDrvIns), DRVHOSTPARALLELR0OP_READ, 0);
        AssertRC(rc);
        *(uint8_t *)pvBuf = (uint8_t)pThis->u8ReadIn;
    }
# endif /* VBOX_WITH_WIN_PARPORT_SUP */
    return rc;
}

static DECLCALLBACK(int) drvHostParallelSetPortDirection(PPDMIHOSTPARALLELCONNECTOR pInterface, bool fForward)
{
    PDRVHOSTPARALLEL    pThis   = RT_FROM_MEMBER(pInterface, DRVHOSTPARALLEL, CTX_SUFF(IHostParallelConnector));
    int rc = VINF_SUCCESS;
    int iMode = 0;
    if (!fForward)
        iMode = 1;
# ifndef VBOX_WITH_WIN_PARPORT_SUP
    int rcLnx = ioctl(RTFileToNative(pThis->hFileDevice), PPDATADIR, &iMode);
    if (RT_UNLIKELY(rcLnx < 0))
        rc = RTErrConvertFromErrno(errno);

# else /* VBOX_WITH_WIN_PARPORT_SUP */
    uint64_t u64Data;
    u64Data = (uint8_t)iMode;
    LogFlowFunc(("calling R0 to write CTRL, data=%#x\n", u64Data));
    if (pThis->fParportAvail)
    {
        rc = PDMDrvHlpCallR0(pThis->CTX_SUFF(pDrvIns), DRVHOSTPARALLELR0OP_SETPORTDIRECTION, u64Data);
        AssertRC(rc);
    }
# endif /* VBOX_WITH_WIN_PARPORT_SUP */
    return rc;
}

/**
 * @interface_method_impl{PDMIBASE,pfnWriteControl}
 */
static DECLCALLBACK(int) drvHostParallelWriteControl(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t fReg)
{
    PDRVHOSTPARALLEL    pThis   = RT_FROM_MEMBER(pInterface, DRVHOSTPARALLEL, CTX_SUFF(IHostParallelConnector));
    int rc = VINF_SUCCESS;
    int rcLnx = 0;

    LogFlowFunc(("fReg=%#x\n", fReg));
# ifndef VBOX_WITH_WIN_PARPORT_SUP
    rcLnx = ioctl(RTFileToNative(pThis->hFileDevice), PPWCONTROL, &fReg);
    if (RT_UNLIKELY(rcLnx < 0))
        rc = RTErrConvertFromErrno(errno);
# else /* VBOX_WITH_WIN_PARPORT_SUP */
    uint64_t u64Data;
    u64Data = (uint8_t)fReg;
    LogFlowFunc(("calling R0 to write CTRL, data=%#x\n", u64Data));
    if (pThis->fParportAvail)
    {
        rc = PDMDrvHlpCallR0(pThis->CTX_SUFF(pDrvIns), DRVHOSTPARALLELR0OP_WRITECONTROL, u64Data);
        AssertRC(rc);
    }
# endif /* VBOX_WITH_WIN_PARPORT_SUP */
    return rc;
}


/**
 * @interface_method_impl{PDMIBASE,pfnReadControl}
 */
static DECLCALLBACK(int) drvHostParallelReadControl(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t *pfReg)
{
    PDRVHOSTPARALLEL    pThis   = RT_FROM_MEMBER(pInterface, DRVHOSTPARALLEL, CTX_SUFF(IHostParallelConnector));
    int rc = VINF_SUCCESS;
    int rcLnx = 0;
    uint8_t fReg = 0;

# ifndef VBOX_WITH_WIN_PARPORT_SUP
    rcLnx = ioctl(RTFileToNative(pThis->hFileDevice), PPRCONTROL, &fReg);
    if (RT_UNLIKELY(rcLnx < 0))
        rc = RTErrConvertFromErrno(errno);
    else
    {
        LogFlowFunc(("fReg=%#x\n", fReg));
        *pfReg = fReg;
    }
# else /* VBOX_WITH_WIN_PARPORT_SUP */
    *pfReg = 0; /* Initialize the buffer*/
    if (pThis->fParportAvail)
    {
        LogFlowFunc(("calling R0 to read control from parallel port\n"));
        rc = PDMDrvHlpCallR0(pThis-> CTX_SUFF(pDrvIns), DRVHOSTPARALLELR0OP_READCONTROL, 0);
        AssertRC(rc);
        *pfReg = pThis->u8ReadInControl;
    }
# endif /* VBOX_WITH_WIN_PARPORT_SUP */
    return rc;
}

/**
 * @interface_method_impl{PDMIBASE,pfnReadStatus}
 */
static DECLCALLBACK(int) drvHostParallelReadStatus(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t *pfReg)
{
    PDRVHOSTPARALLEL    pThis   = RT_FROM_MEMBER(pInterface, DRVHOSTPARALLEL, CTX_SUFF(IHostParallelConnector));
    int rc = VINF_SUCCESS;
    int rcLnx = 0;
    uint8_t fReg = 0;
# ifndef  VBOX_WITH_WIN_PARPORT_SUP
    rcLnx = ioctl(RTFileToNative(pThis->hFileDevice), PPRSTATUS, &fReg);
    if (RT_UNLIKELY(rcLnx < 0))
        rc = RTErrConvertFromErrno(errno);
    else
    {
        LogFlowFunc(("fReg=%#x\n", fReg));
        *pfReg = fReg;
    }
# else /* VBOX_WITH_WIN_PARPORT_SUP */
    *pfReg = 0; /* Intialize the buffer. */
    if (pThis->fParportAvail)
    {
        LogFlowFunc(("calling R0 to read status from parallel port\n"));
        rc = PDMDrvHlpCallR0(pThis->CTX_SUFF(pDrvIns), DRVHOSTPARALLELR0OP_READSTATUS, 0);
        AssertRC(rc);
        *pfReg = pThis->u8ReadInStatus;
    }
# endif /* VBOX_WITH_WIN_PARPORT_SUP */
    return rc;
}

# ifndef VBOX_WITH_WIN_PARPORT_SUP

static DECLCALLBACK(int) drvHostParallelMonitorThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVHOSTPARALLEL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);
    struct pollfd aFDs[2];

    /*
     * We can wait for interrupts using poll on linux hosts.
     */
    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        int rc;

        aFDs[0].fd      = RTFileToNative(pThis->hFileDevice);
        aFDs[0].events  = POLLIN;
        aFDs[0].revents = 0;
        aFDs[1].fd      = RTPipeToNative(pThis->hWakeupPipeR);
        aFDs[1].events  = POLLIN | POLLERR | POLLHUP;
        aFDs[1].revents = 0;
        rc = poll(aFDs, RT_ELEMENTS(aFDs), -1);
        if (rc < 0)
        {
            AssertMsgFailed(("poll failed with rc=%d\n", RTErrConvertFromErrno(errno)));
            return RTErrConvertFromErrno(errno);
        }

        if (pThread->enmState != PDMTHREADSTATE_RUNNING)
            break;
        if (rc > 0 && aFDs[1].revents)
        {
            if (aFDs[1].revents & (POLLHUP | POLLERR | POLLNVAL))
                break;
            /* notification to terminate -- drain the pipe */
            char ch;
            size_t cbRead;
            RTPipeRead(pThis->hWakeupPipeR, &ch, 1, &cbRead);
            continue;
        }

        /* Interrupt occurred. */
        rc = pThis->pDrvHostParallelPort->pfnNotifyInterrupt(pThis->pDrvHostParallelPort);
        AssertRC(rc);
    }

    return VINF_SUCCESS;
}

/**
 * Unblock the monitor thread so it can respond to a state change.
 *
 * @returns a VBox status code.
 * @param     pDrvIns     The driver instance.
 * @param     pThread     The send thread.
 */
static DECLCALLBACK(int) drvHostParallelWakeupMonitorThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVHOSTPARALLEL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);
    size_t cbIgnored;
    return RTPipeWrite(pThis->hWakeupPipeW, "", 1, &cbIgnored);
}

# endif /* VBOX_WITH_WIN_PARPORT_SUP */

/**
 * Destruct a host parallel driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that
 * any non-VM resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvHostParallelDestruct(PPDMDRVINS pDrvIns)
{
    PDRVHOSTPARALLEL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);
    LogFlowFunc(("iInstance=%d\n", pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

#ifndef VBOX_WITH_WIN_PARPORT_SUP

    int rc;

    if (pThis->hFileDevice != NIL_RTFILE)
        ioctl(RTFileToNative(pThis->hFileDevice), PPRELEASE);

    rc = RTPipeClose(pThis->hWakeupPipeW); AssertRC(rc);
    pThis->hWakeupPipeW = NIL_RTPIPE;

    rc = RTPipeClose(pThis->hWakeupPipeR); AssertRC(rc);
    pThis->hWakeupPipeR = NIL_RTPIPE;

    rc = RTFileClose(pThis->hFileDevice); AssertRC(rc); /** @todo r=bird: Why aren't this closed on Windows? */
    pThis->hFileDevice = NIL_RTFILE;

    if (pThis->pszDevicePath)
    {
        MMR3HeapFree(pThis->pszDevicePath);
        pThis->pszDevicePath = NULL;
    }
#endif /* VBOX_WITH_WIN_PARPORT_SUP */
}

/**
 * Construct a host parallel driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostParallelConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVHOSTPARALLEL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);
    LogFlowFunc(("iInstance=%d\n", pDrvIns->iInstance));

    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Init basic data members and interfaces.
     *
     * Must be done before returning any failure because we've got a destructor.
     */
    pThis->hFileDevice  = NIL_RTFILE;
#ifndef VBOX_WITH_WIN_PARPORT_SUP
    pThis->hWakeupPipeR = NIL_RTPIPE;
    pThis->hWakeupPipeW = NIL_RTPIPE;
#endif

    pThis->pDrvInsR3                                = pDrvIns;
#ifdef VBOX_WITH_DRVINTNET_IN_R0
    pThis->pDrvInsR0                                = PDMDRVINS_2_R0PTR(pDrvIns);
#endif

    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface                  = drvHostParallelQueryInterface;
    /* IHostParallelConnector. */
    pThis->IHostParallelConnectorR3.pfnWrite            = drvHostParallelWrite;
    pThis->IHostParallelConnectorR3.pfnRead             = drvHostParallelRead;
    pThis->IHostParallelConnectorR3.pfnSetPortDirection = drvHostParallelSetPortDirection;
    pThis->IHostParallelConnectorR3.pfnWriteControl     = drvHostParallelWriteControl;
    pThis->IHostParallelConnectorR3.pfnReadControl      = drvHostParallelReadControl;
    pThis->IHostParallelConnectorR3.pfnReadStatus       = drvHostParallelReadStatus;

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfg, "DevicePath\0"))
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES,
                                N_("Unknown host parallel configuration option, only supports DevicePath"));

    /*
     * Query configuration.
     */
    /* Device */
    int rc = CFGMR3QueryStringAlloc(pCfg, "DevicePath", &pThis->pszDevicePath);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: query for \"DevicePath\" string returned %Rra.\n", rc));
        return rc;
    }

    /*
     * Open the device
     */
    rc = RTFileOpen(&pThis->hFileDevice, pThis->pszDevicePath, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("Parallel#%d could not open '%s'"),
                                   pDrvIns->iInstance, pThis->pszDevicePath);

#ifndef VBOX_WITH_WIN_PARPORT_SUP
    /*
     * Try to get exclusive access to parallel port
     */
    rc = ioctl(RTFileToNative(pThis->hFileDevice), PPEXCL);
    if (rc < 0)
        return PDMDrvHlpVMSetError(pDrvIns, RTErrConvertFromErrno(errno), RT_SRC_POS,
                                   N_("Parallel#%d could not get exclusive access for parallel port '%s'"
                                      "Be sure that no other process or driver accesses this port"),
                                   pDrvIns->iInstance, pThis->pszDevicePath);

    /*
     * Claim the parallel port
     */
    rc = ioctl(RTFileToNative(pThis->hFileDevice), PPCLAIM);
    if (rc < 0)
        return PDMDrvHlpVMSetError(pDrvIns, RTErrConvertFromErrno(errno), RT_SRC_POS,
                                   N_("Parallel#%d could not claim parallel port '%s'"
                                      "Be sure that no other process or driver accesses this port"),
                                   pDrvIns->iInstance, pThis->pszDevicePath);

    /*
     * Get the IHostParallelPort interface of the above driver/device.
     */
    pThis->pDrvHostParallelPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIHOSTPARALLELPORT);
    if (!pThis->pDrvHostParallelPort)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE, RT_SRC_POS, N_("Parallel#%d has no parallel port interface above"),
                                   pDrvIns->iInstance);

    /*
     * Create wakeup pipe.
     */
    rc = RTPipeCreate(&pThis->hWakeupPipeR, &pThis->hWakeupPipeW, 0 /*fFlags*/);
    AssertRCReturn(rc, rc);

    /*
     * Start in SPP mode.
     */
    pThis->enmModeCur = PDM_PARALLEL_PORT_MODE_INVALID;
    rc = drvHostParallelSetMode(pThis, PDM_PARALLEL_PORT_MODE_SPP);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("HostParallel#%d cannot change mode of parallel mode to SPP"), pDrvIns->iInstance);

    /*
     * Start waiting for interrupts.
     */
    rc = PDMDrvHlpThreadCreate(pDrvIns, &pThis->pMonitorThread, pThis, drvHostParallelMonitorThread, drvHostParallelWakeupMonitorThread, 0,
                               RTTHREADTYPE_IO, "ParMon");
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("HostParallel#%d cannot create monitor thread"), pDrvIns->iInstance);

#else /* VBOX_WITH_WIN_PARPORT_SUP */
    HANDLE hPort;
    pThis->fParportAvail     = false;
    pThis->u32LptAddr        = 0;
    pThis->u32LptAddrControl = 0;
    pThis->u32LptAddrStatus  = 0;
    rc = drvWinHostGetparportAddr(pThis);

    /* If we have the char port availabe use it , else I am not getting exclusive access to parallel port.
       Read and write will be done only if addresses are available
    */
    if (pThis->u8ParportName)
    {
        LogFlowFunc(("Get the Handle to Printer Port =%s\n", (char *)pThis->u8ParportName));
        /** @todo r=klaus convert to IPRT */
        hPort = CreateFile((char *)pThis->u8ParportName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    /** @todo amakkar: handle the case if hPort is NULL */
# if 0
    if (hPort == INVALID_HANDLE_VALUE)
    {
        LogFlow(("Failed to get exclusive access to parallel port\n"));
        rc = VERR_INVALID_HANDLE;
    }*/
# endif /* VBOX_WITH_WIN_PARPORT_SUP  */
#endif
    return VINF_SUCCESS;
}


/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvHostParallel =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "HostParallel",
    /* szRCMod */
    "",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Parallel host driver.",
    /* fFlags */
# if defined(VBOX_WITH_WIN_PARPORT_SUP)
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DRVREG_FLAGS_R0,
# else
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
# endif
    /* fClass. */
    PDM_DRVREG_CLASS_CHAR,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTPARALLEL),
    /* pfnConstruct */
    drvHostParallelConstruct,
    /* pfnDestruct */
    drvHostParallelDestruct,
    /* pfnRelocate */
    NULL,
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
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};
#endif /*IN_RING3*/


