/** @file
 * VirtualBox Guest Additions Driver for Solaris - Solaris helper functions.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
 
/******************************************************************************
*   Header Files                                                              *
******************************************************************************/

#include "solaris.h"

/******************************************************************************
*   Helper functions                                                          *
******************************************************************************/

void miocack(queue_t *pWriteQueue, mblk_t *pMBlk, int cbData, int rc)
{
    struct iocblk *pIOCBlk = (struct iocblk *)pMBlk->b_rptr;

    pMBlk->b_datap->db_type = M_IOCACK;
    pIOCBlk->ioc_count = cbData;
    pIOCBlk->ioc_rval = rc;
    pIOCBlk->ioc_error = 0;
    qreply(pWriteQueue, pMBlk);
}

void miocnak(queue_t *pWriteQueue, mblk_t *pMBlk, int cbData, int iErr)
{
    struct iocblk *pIOCBlk = (struct iocblk *)pMBlk->b_rptr;

    pMBlk->b_datap->db_type = M_IOCNAK;
    pIOCBlk->ioc_count = cbData;
    pIOCBlk->ioc_error = iErr ? iErr : EINVAL;
    pIOCBlk->ioc_rval = 0;
    qreply(pWriteQueue, pMBlk);
}

/* This does not work like the real version, but does some sanity testing
 * and sets a flag. */
int miocpullup(mblk_t *pMBlk, size_t cbMsg)
{
    struct iocblk *pIOCBlk = (struct iocblk *)pMBlk->b_rptr;

    if (pIOCBlk->ioc_count == TRANSPARENT)
        return EINVAL;
    if (   !pMBlk->b_cont
        || pMBlk->b_cont->b_wptr < pMBlk->b_cont->b_rptr + cbMsg)
        return EINVAL;
    pMBlk->b_flag = 1;  /* Test for this to be sure miocpullup was called. */
    return 0;
}
