/* $Id */
/** @file
 * FTM - Internal header file.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___FTMInternal_h
#define ___FTMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/ftm.h>


/** @defgroup grp_ftm_int Internals.
 * @ingroup grp_ftm
 * @{
 */


/**
 * FTM VM Instance data.
 * Changes to this must checked against the padding of the ftm union in VM!
 */
typedef struct FTM
{
    char       *pszAddress;
    char       *pszPassword;
    unsigned    uPort;
    unsigned    uInterval;

} FTM;

/** @} */

#endif
