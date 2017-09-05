/* $Id$ */
/** @file
 * VBoxGuestLib - Ring-0 Support Library for VBoxGuest, IDC, UNIX-like OSes.
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "VBoxGuestR0LibInternal.h"
#include <VBox/VBoxGuest.h>
#include <VBox/err.h>


int VBOXCALL vbglR0IdcNativeOpen(PVBGLIDCHANDLE pHandle, PVBGLIOCIDCCONNECT pReq)
{
    ldi_handle_t hDev   = NULL;
    ldi_ident_t  hIdent = ldi_ident_from_anon();
    int rc = ldi_open_by_name(VBOXGUEST_DEVICE_NAME, FREAD, kcred, &hDev, hIdent);
    ldi_ident_release(hIdent);
    if (rc == 0)
    {
        rc = VBoxGuestIDC(NULL, VBGL_IOCTL_IDC_DISCONNECT, &pReq->Hdr, sizeof(*pReq));
        if (RT_SUCCESS(rc) && RT_SUCCESS(pReq->Hdr.rc))
        {
            pHandle->s.hDev = hDev;
            return VINF_SUCCESS;
        }
        ldi_close(hDev, FREAD, kcred);
    }
    else
        rc = VERR_OPEN_FAILED;
    pHandle->s.hDev = NULL;
    return rc;
}


int VBOXCALL vbglR0IdcNativeClose(PVBGLIDCHANDLE pHandle, PVBGLIOCIDCDISCONNECT pReq)
{
    int rc = VBoxGuestIDC(pHandle->s.pvSession, VBGL_IOCTL_IDC_DISCONNECT, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(rc) && RT_SUCCESS(pReq->Hdr.rc))
    {
        ldi_close(pHandle->s.hDev, FREAD, kcred);
        pHandle->s.hDev = NULL;
    }
    return rc;
}


/**
 * Makes an IDC call, returning only the I/O control status code.
 *
 * @returns VBox status code (the I/O control failure status).
 * @param   pHandle             The IDC handle.
 * @param   uReq                The request number.
 * @param   pReqHdr             The request header.
 * @param   cbReq               The request size.
 */
DECLR0VBGL(int) VbglR0IdcCallRaw(PVBGLIDCHANDLE pHandle, uintptr_t uReq, PVBGLREQHDR pReqHdr, uint32_t cbReq)
{
    return VBoxGuestIDC(pHandle->s.pvSession, uReq, pReqHdr, cbReq);
}

