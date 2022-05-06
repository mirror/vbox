/** @file
 * GCM - Internal header file.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VMM_INCLUDED_SRC_include_GCMInternal_h
#define VMM_INCLUDED_SRC_include_GCMInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/vmm/gcm.h>
#include <VBox/vmm/pgm.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_gcm_int       Internal
 * @ingroup grp_gcm
 * @internal
 * @{
 */

/** The saved state version. */
#define GCM_SAVED_STATE_VERSION         1

/**
 * GCM VM Instance data.
 */
typedef struct GCM
{
    /** The provider that is active for this VM. */
    int32_t                         enmFixerIds;
    /** The interface implementation version. */
    uint32_t                        u32Version;

} GCM;
/** Pointer to GCM VM instance data. */
typedef GCM *PGCM;

/** @} */

RT_C_DECLS_END

#endif /* !VMM_INCLUDED_SRC_include_GCMInternal_h */

