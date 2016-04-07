/* $Id$ */
/** @file
 * UsbTestServ - Remote USB test configuration and execution server, USB gadget class
 *               for the test device.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
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

#include <iprt/asm.h>
#include <iprt/cdefs.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/types.h>

#include <iprt/linux/sysfs.h>

#include "UsbTestServiceGadgetInternal.h"

/*********************************************************************************************************************************
*   Constants And Macros, Structures and Typedefs                                                                                *
*********************************************************************************************************************************/

/** Default configfs mount point. */
#define UTS_GADGET_CLASS_CONFIGFS_MNT_DEF "/sys/kernel/config/usb_gadget"
/** Gadget template name */
#define UTS_GADGET_TEMPLATE_NAME "gadget_test"

#define UTS_GADGET_TEST_VENDOR_ID_DEF    UINT16_C(0x0525)
#define UTS_GADGET_TEST_PRODUCT_ID_DEF   UINT16_C(0xa4a0)
#define UTS_GADGET_TEST_DEVICE_CLASS_DEF UINT8_C(0xff)


/**
 * Internal UTS gadget host instance data.
 */
typedef struct UTSGADGETCLASSINT
{
    /** Gadget template path. */
    char                      *pszGadgetPath;
    /** The UDC this gadget is connected to. */
    char                      *pszUdc;
    /** Bus identifier for the used UDC. */
    uint32_t                   uBusId;
    /** Device identifier. */
    uint32_t                   uDevId;
} UTSGADGETCLASSINT;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


/**
 * @interface_method_impl{UTSGADGETCLASS,pfnInit}
 */
static DECLCALLBACK(int) utsGadgetClassTestInit(PUTSGADGETCLASSINT pClass, PCUTSGADGETCFGITEM paCfg)
{
    int rc = VINF_SUCCESS;

    if (RTLinuxSysFsExists(UTS_GADGET_CLASS_CONFIGFS_MNT_DEF))
    {
        /* Create the gadget template */
        unsigned idx = 0;
        char aszPath[RTPATH_MAX];

        do
        {
            RTStrPrintf(&aszPath[0], RT_ELEMENTS(aszPath), "%s/%s%u",
                        UTS_GADGET_CLASS_CONFIGFS_MNT_DEF, UTS_GADGET_TEMPLATE_NAME,
                        idx);
            rc = RTDirCreateFullPath(aszPath, 0700);
            if (RT_SUCCESS(rc))
                break;
            idx++;
        } while (idx < 100);

        if (RT_SUCCESS(rc))
        {
            pClass->pszGadgetPath = RTStrDup(aszPath);

            uint16_t idVendor = 0;
            uint16_t idProduct = 0;
            uint8_t  bDeviceClass = 0;
            rc = utsGadgetCfgQueryU16Def(paCfg, "Gadget/idVendor", &idVendor, UTS_GADGET_TEST_VENDOR_ID_DEF);
            if (RT_SUCCESS(rc))
                rc = utsGadgetCfgQueryU16Def(paCfg, "Gadget/idProduct", &idProduct, UTS_GADGET_TEST_PRODUCT_ID_DEF);
            if (RT_SUCCESS(rc))
                rc = utsGadgetCfgQueryU8Def(paCfg, "Gadget/bDeviceClass", &bDeviceClass, UTS_GADGET_TEST_DEVICE_CLASS_DEF);
        }
    }
    else
        rc = VERR_NOT_FOUND;

    return rc;
}


/**
 * @interface_method_impl{UTSGADGETCLASS,pfnTerm}
 */
static DECLCALLBACK(void) utsGadgetClassTestTerm(PUTSGADGETCLASSINT pClass)
{

}



/**
 * The gadget host interface callback table.
 */
const UTSGADGETCLASSIF g_UtsGadgetClassTest =
{
    /** enmType */
    UTSGADGETCLASS_TEST,
    /** pszDesc */
    "UTS test device gadget class",
    /** cbIf */
    sizeof(UTSGADGETCLASSINT),
    /** pfnInit */
    utsGadgetClassTestInit,
    /** pfnTerm */
    utsGadgetClassTestTerm
};

