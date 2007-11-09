/** @file
 * PDM - Pluggable Device Manager, Async Task.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBox_pdmasynctask_h
#define ___VBox_pdmasynctask_h

#include <VBox/types.h>

__BEGIN_DECLS


/** @defgroup grp_pdm_async_task    Async Task
 * @ingroup grp_pdm
 * @{
 */

/** Pointer to a PDM async task template handle. */
typedef struct PDMASYNCTASKTEMPLATE *PPDMASYNCTASKTEMPLATE;
/** Pointer to a PDM async task template handle pointer. */
typedef PPDMASYNCTASKTEMPLATE *PPPDMASYNCTASKTEMPLATE;

/** Pointer to a PDM async task handle. */
typedef struct PDMASYNCTASK *PPDMASYNCTASK;
/** Pointer to a PDM async task handle pointer. */
typedef PPDMASYNCTASK *PPPDMASYNCTASK;

/* This should be similar to VMReq, only difference there will be a pool
   of worker threads instead of EMT. The actual implementation should be
   made in IPRT so we can reuse it for other stuff later. The reason why
   it should be put in PDM is because we need to manage it wrt to VM
   state changes (need exception - add a flag for this). */

/** @} */


__END_DECLS

#endif

