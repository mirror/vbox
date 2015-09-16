/** @file
 * VBox Shared Folders header.
 * Guest/host path convertion and verification.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBSFPATH__H
#define __VBSFPATH__H

#include "shfl.h"
#include <VBox/shflsvc.h>

#define VBSF_O_PATH_WILDCARD                UINT32_C(0x00000001)
#define VBSF_O_PATH_PRESERVE_LAST_COMPONENT UINT32_C(0x00000002)
#define VBSF_O_PATH_CHECK_ROOT_ESCAPE       UINT32_C(0x00000004)

#define VBSF_F_PATH_HAS_WILDCARD_IN_PREFIX UINT32_C(0x00000001) /* A component before the last one contains a wildcard. */
#define VBSF_F_PATH_HAS_WILDCARD_IN_LAST   UINT32_C(0x00000002) /* The last component contains a wildcard. */

/**
 *
 * @param pClient                Shared folder client.
 * @param hRoot                  Root handle.
 * @param pGuestString           Guest want to access the path.
 * @param cbGuestString          Size of pGuestString memory buffer.
 * @param ppszHostPath           Returned full host path: root prefix + guest path.
 * @param pcbHostPathRoot        Length of the root prefix in bytes. Optional, can be NULL.
 * @param fWildCard              Whether the last component may contain
 *                               wildcards and thus might require exclusion
 *                               from the case correction.
 * @param fPreserveLastComponent Always exclude the last component from case
 *                               correction if set.
 * @param pfu32PathFlags         VBSF_F_PATH_* flags. Optional, can be NULL.
 */
int vbsfPathGuestToHost(SHFLCLIENTDATA *pClient, SHFLROOT hRoot,
                        PSHFLSTRING pGuestString, uint32_t cbGuestString,
                        char **ppszHostPath, uint32_t *pcbHostPathRoot,
                        uint32_t fu32Options,
                        uint32_t *pfu32PathFlags);

/** Free the host path returned by vbsfPathGuestToHost.
 *
 * @param pszHostPath Host path string.
 */
void vbsfFreeHostPath(char *pszHostPath);

#endif /* __VBSFPATH__H */
