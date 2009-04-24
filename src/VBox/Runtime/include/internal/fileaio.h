/* $Id$ */
/** @file
 * IPRT - Internal RTFileAio header.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___internal_fileaio_h
#define ___internal_fileaio_h

#include <iprt/file.h>
#include "internal/magics.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/** Validates a context handle and returns VERR_INVALID_HANDLE if not valid. */
#define RTFILEAIOREQ_VALID_RETURN_RC(pReq, rc) \
    do { \
        AssertPtrReturn((pReq), (rc)); \
        AssertReturn((pReq)->u32Magic == RTFILEAIOREQ_MAGIC, (rc)); \
    } while (0)

/** Validates a context handle and returns VERR_INVALID_HANDLE if not valid. */
#define RTFILEAIOREQ_VALID_RETURN(pReq) RTFILEAIOREQ_VALID_RETURN_RC((pReq), VERR_INVALID_HANDLE)

/** Validates a context handle and returns (void) if not valid. */
#define RTFILEAIOREQ_VALID_RETURN_VOID(pReq) \
    do { \
        AssertPtrReturnVoid(pReq); \
        AssertReturnVoid((pReq)->u32Magic == RTFILEAIOREQ_MAGIC); \
    } while (0)

/** Validates a context handle and returns the specified rc if not valid. */
#define RTFILEAIOCTX_VALID_RETURN_RC(pCtx, rc) \
    do { \
        AssertPtrReturn((pCtx), (rc)); \
        AssertReturn((pCtx)->u32Magic == RTFILEAIOCTX_MAGIC, (rc)); \
    } while (0)

/** Validates a context handle and returns VERR_INVALID_HANDLE if not valid. */
#define RTFILEAIOCTX_VALID_RETURN(pCtx) RTFILEAIOCTX_VALID_RETURN_RC((pCtx), VERR_INVALID_HANDLE)

__BEGIN_DECLS

__END_DECLS

#endif

