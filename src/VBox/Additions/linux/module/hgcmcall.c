/** @file
 *
 * vboxadd -- VirtualBox Guest Additions for Linux
 * hgcmcall.c -- wrapper for hgcm calls from user space
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "the-linux-kernel.h"
#include "version-generated.h"

/* #define IRQ_DEBUG */

#include "vboxmod.h"
#include "waitcompat.h"

#include <VBox/log.h>
#include <iprt/assert.h>


/**
 * Get the IOCTL structure from user space
 *
 * @returns          0 on success, Linux error code on failure
 * @param  cParms    The number of parameters to the HGCM call
 * @param  pUser     User space pointer to the data to be copied
 * @retval hgcmR3Ret Where to store the structure allocated on success
 */
static int vbox_hgcm_get_r3_struct(int cParms, void *pUser, VBoxGuestHGCMCallInfo **hgcmR3Ret)
{
    VBoxGuestHGCMCallInfo *hgcmR3;

    /* Allocate space for those call parameters. */
    hgcmR3 = kmalloc(sizeof(*hgcmR3) + cParms * sizeof(HGCMFunctionParameter), GFP_KERNEL);
    if (!hgcmR3)
    {
        LogRel(("IOCTL_VBOXGUEST_HGCM_CALL: cannot allocate memory!\n"));
        return -ENOMEM;
    }
    /* Get the call parameters from user space. */
    if (copy_from_user(hgcmR3, pUser, sizeof(*hgcmR3) + cParms * sizeof(HGCMFunctionParameter)))
    {
        LogRel(("IOCTL_VBOXGUEST_HGCM_CALL: copy_from_user failed!\n"));
        kfree(hgcmR3);
        return -EFAULT;
    }
    *hgcmR3Ret = hgcmR3;
    return 0;
}

/**
 * Read the IOCTL parameters from the calling user space application and repack them into
 * a structure in kernel space.  This repacking is needed because the structure contains pointers
 * which will no longer be valid after it is copied from user to kernel space.
 *
 * @returns                   0 on success, or a Linux error code on failure
 * @param  hgcmR3             The user space ioctl structure, copied into kernel space
 * @retval hgcmR0Ret          The kernel space structure set up in this function
 * @retval ppu8PointerDataRet A buffer storing the parameters copied from user space
 */
static int vbox_hgcm_get_r3_params(VBoxGuestHGCMCallInfo *hgcmR3,
                                   VBoxGuestHGCMCallInfo **hgcmR0Ret,
                                   uint8_t **ppu8PointerDataRet)
{
    VBoxGuestHGCMCallInfo *hgcmR0;
    uint8_t *pu8PointerData;
    size_t cbPointerData = 0, offPointerData = 0;
    int i;

    /* Allocate the structure which we will pass to the kernel space HGCM call. */
    hgcmR0 = kmalloc(sizeof(*hgcmR0) + hgcmR3->cParms * sizeof(HGCMFunctionParameter),
                        GFP_KERNEL);
    if (!hgcmR0)
    {
        LogRel(("IOCTL_VBOXGUEST_HGCM_CALL: cannot allocate memory!\n"));
        return -ENOMEM;
    }
    /* Set up the structure header */
    hgcmR0->u32ClientID = hgcmR3->u32ClientID;
    hgcmR0->u32Function = hgcmR3->u32Function;
    hgcmR0->cParms = hgcmR3->cParms;
    /* Calculate the total size of pointer space.  Will normally be for a single pointer */
    for (i = 0; i < hgcmR3->cParms; ++i)
    {
        switch (VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].type)
        {
        case VMMDevHGCMParmType_32bit:
        case VMMDevHGCMParmType_64bit:
            break;
        case VMMDevHGCMParmType_LinAddr:
        case VMMDevHGCMParmType_LinAddr_In:
        case VMMDevHGCMParmType_LinAddr_Out:
            cbPointerData += VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].u.Pointer.size;
            break;
        default:
            LogRel(("IOCTL_VBOXGUEST_HGCM_CALL: unsupported or unknown parameter type\n"));
            kfree(hgcmR0);
            return -EINVAL;
        }
    }
    pu8PointerData = kmalloc (cbPointerData, GFP_KERNEL);
    /* Reconstruct the pointer parameter data in kernel space */
    if (pu8PointerData == NULL)
    {
        LogRel(("IOCTL_VBOXGUEST_HGCM_CALL: out of memory allocating %d bytes for pointer data\n",
                cbPointerData));
        kfree(hgcmR0);
        return -ENOMEM;
    }
    /* Copy and translate the parameters from the user space structure to the kernel space
        structure. */
    for (i = 0; i < hgcmR3->cParms; ++i)
    {
        VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].type
            = VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].type;
        if (   (VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].type == VMMDevHGCMParmType_LinAddr)
            || (VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].type
                    == VMMDevHGCMParmType_LinAddr_In))
        {
            /* This pointer type means that we are sending data to the host or both
                sending and reading data. */
            void *pvR3LinAddr
                = (void *)VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].u.Pointer.u.linearAddr;
            if (copy_from_user(&pu8PointerData[offPointerData],
                                pvR3LinAddr,
                                VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].u.Pointer.size))
            {
                LogRel(("IOCTL_VBOXGUEST_HGCM_CALL: copy_from_user failed!\n"));
                kfree(hgcmR0);
                kfree(pu8PointerData);
                return -EFAULT;
            }
            VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.u.linearAddr
                = (vmmDevHypPtr)&pu8PointerData[offPointerData];
            VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.size
                = VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].u.Pointer.size;
            offPointerData += VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].u.Pointer.size;
        }
        else if (VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].type
                      == VMMDevHGCMParmType_LinAddr_Out)
        {
            /* This type of pointer means that we are reading data from the host. */
            VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.u.linearAddr
                = (vmmDevHypPtr)&pu8PointerData[offPointerData];
            VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.size
                = VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].u.Pointer.size;
            offPointerData += VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].u.Pointer.size;
        }
        else
        {
            /* If it is not a pointer, then it is a 32bit or 64bit value */
            VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.value64
                = VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].u.value64;
        }
    }
    *hgcmR0Ret = hgcmR0;
    *ppu8PointerDataRet = pu8PointerData;
    return 0;
}

/**
 * Dump the contents of an hgcm call info structure to the back door logger
 *
 * @param hgcmR0 The structure to dump.
 */
static void vbox_hgcm_dump_params(VBoxGuestHGCMCallInfo *hgcmR0)
{
#ifdef DEBUG_Michael
    int i;

    for (i = 0; i < hgcmR0->cParms; ++i)
    {
        switch(VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].type)
        {
        case VMMDevHGCMParmType_32bit:
            Log(("IOCTL_VBOXGUEST_HGCM_CALL: parameter %d is of type 32bit: %u\n",
                  i, VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.value32));
            break;
        case VMMDevHGCMParmType_64bit:
            Log(("IOCTL_VBOXGUEST_HGCM_CALL: parameter %d is of type 64bit: %lu\n",
                  i, VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.value64));
            break;
        case VMMDevHGCMParmType_LinAddr:
            Log(("IOCTL_VBOXGUEST_HGCM_CALL: parameter %d is of type LinAddr, size %u: %.*s...\n",
                  i,
                  VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.size > 10 ?
                      VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.size : 10,
                  VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.u.linearAddr,
                  VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.size));
            break;
        case VMMDevHGCMParmType_LinAddr_In:
            Log(("IOCTL_VBOXGUEST_HGCM_CALL: parameter %d is of type LinAddr_In, size %u: %.*s...\n",
                  i,
                  VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.size > 10 ?
                      VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.size : 10,
                  VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.u.linearAddr,
                  VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.size));
            break;
        case VMMDevHGCMParmType_LinAddr_Out:
            Log(("IOCTL_VBOXGUEST_HGCM_CALL: parameter %d is of type LinAddr_Out, size %u: %.*s...\n",
                  i,
                  VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.size > 10 ?
                      VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.size : 10,
                  VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.u.linearAddr,
                  VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.size));
            break;
        default:
            Log(("IOCTL_VBOXGUEST_HGCM_CALL: parameter %d is of unknown type!", i));
        }
    }
#endif /* defined DEBUG_Michael */
}

/**
 * Copy the return parameters from the IOCTL call from kernel to user space and update the
 * user space ioctl structure with the new parameter information.
 *
 * @returns      0 on success, a Linux error code on failure
 * @param hgcmR3 The user space structure to be updated and copied back to user space, including
 *               the user space addresses of the buffers for the parameters
 * @param hgcmR0 The kernel space structure pointing to the data to be returned
 */
static int vbox_hgcm_return_r0_struct(VBoxGuestHGCMCallInfo *hgcmR3, void *pUser,
                                      VBoxGuestHGCMCallInfo *hgcmR0)
{
   int i;

    for (i = 0; i < hgcmR3->cParms; ++i)
    {
        if (   (VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].type == VMMDevHGCMParmType_LinAddr)
            || (VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].type
                    == VMMDevHGCMParmType_LinAddr_Out))
        {
            /* We are sending data to the host or sending and reading. */
            void *pvR3LinAddr
                = (void *)VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].u.Pointer.u.linearAddr;
            void *pvR0LinAddr
                = (void *)VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.u.linearAddr;
            if (copy_to_user(pvR3LinAddr, pvR0LinAddr,
                              VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.size))
            {
                LogRel(("IOCTL_VBOXGUEST_HGCM_CALL: copy_to_user failed!\n"));
                return -EFAULT;
            }
            VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].u.Pointer.size
                = VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.Pointer.size;
        }
        else if (VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].type
                      != VMMDevHGCMParmType_LinAddr_Out)
        {
            /* If it is not a pointer, then it is a 32bit or 64bit value */
            VBOXGUEST_HGCM_CALL_PARMS(hgcmR3)[i].u.value64
                = VBOXGUEST_HGCM_CALL_PARMS(hgcmR0)[i].u.value64;
        }
    }
    hgcmR3->result = hgcmR0->result;
    if (copy_to_user(pUser, hgcmR3,
                     sizeof(*hgcmR3) + hgcmR3->cParms * sizeof(HGCMFunctionParameter)))
    {
        LogRel(("IOCTL_VBOXGUEST_HGCM_CALL: copy_to_user failed!\n"));
        return -EFAULT;
    }
    return 0;
}

/**
 * This IOCTL wrapper allows the guest to make an HGCM call from user space.  The
 * OS-independant part of the Guest Additions already contain code for making an
 * HGCM call from the guest, but this code assumes that the call is made from the
 * kernel's address space.  So before calling it, we have to copy all parameters
 * to the HGCM call from user space to kernel space and reconstruct the structures
 * passed to the call (which include pointers to other memory) inside the kernel's
 * address space.
 *
 * @returns   0 on success or Linux error code on failure
 * @param arg User space pointer to the call data structure
 */
int vbox_ioctl_hgcm_call(unsigned long arg, VBoxDevice *vboxDev)
{
    VBoxGuestHGCMCallInfo callHeader, *hgcmR3, *hgcmR0;
    uint8_t *pu8PointerData;
    int rc;

    AssertCompiler((_IOC_SIZE(IOCTL_VBOXGUEST_HGCM_CALL) == sizeof(VBoxGuestHGCMCallInfo)));
    /* Get the call header from user space to see how many call parameters there are. */
    if (copy_from_user(&callHeader, (void*)arg, sizeof(callHeader)))
    {
        LogRel(("IOCTL_VBOXGUEST_HGCM_CALL: copy_from_user failed!\n"));
        return -EFAULT;
    }
    /* Copy the ioctl structure from user to kernel space. */
    rc = vbox_hgcm_get_r3_struct(callHeader.cParms, (void *)arg, &hgcmR3);
    if (rc != 0)
    {
        return rc;
    }
    /* Copy the ioctl parameters from user to kernel space and repack the kernel space
       structure to point to the copied parameters. */
    rc = vbox_hgcm_get_r3_params(hgcmR3, &hgcmR0, &pu8PointerData);
    if (rc != 0)
    {
        kfree(hgcmR3);
        return rc;
    }
    /* Just to make sure that all that was successful, we read through the contents
        of the structure we just copied and print them to the log. */
    vbox_hgcm_dump_params(hgcmR0);
    /* Call the internal VBoxGuest ioctl interface with the ioctl structure we have just copied. */
    rc = vboxadd_cmc_call(vboxDev, IOCTL_VBOXGUEST_HGCM_CALL, hgcmR0);
    if (VBOX_FAILURE(rc))
    {
        LogRel(("IOCTL_VBOXGUEST_HGCM_CALL: internal ioctl call failed, rc=%Vrc\n", rc));
        rc = -RTErrConvertToErrno(rc);
    }
    else
    {
        /* Copy the return parameters back to user space and update the user space structure. */
        rc = vbox_hgcm_return_r0_struct(hgcmR3, (void *)arg, hgcmR0);
    }
    kfree(hgcmR3);
    kfree(hgcmR0);
    kfree(pu8PointerData);
    return rc;
}
