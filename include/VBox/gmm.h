/** @file
 * GMM - Global Memory Manager.
 */

/*
 * Copyright (C) 2007 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 */

#ifndef ___VBox_gmm_h
#define ___VBox_gmm_h

#include <VBox/types.h>

#include "gvm.h"

/** @defgroup   grp_gmm     GMM - The Global Memory Manager
 * @{
 */

/** @def IN_GMM_R0
 * Used to indicate whether we're inside the same link module as the ring 0
 * part of the Global Memory Manager or not.
 */
/** @def GMMR0DECL
 * Ring 0 GMM export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_GVM_R0
# define GVMR0DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define GVMR0DECL(type)    DECLIMPORT(type) VBOXCALL
#endif


/** The chunk shift. (2^20 = 1 MB) */
#define GMM_CHUNK_SHIFT                 20
/** The allocation chunk size. */
#define GMM_CHUNK_SIZE                  (1U << GMM_CHUNK_SHIFT)
/** The shift factor for converting a page id into a chunk id. */
#define GMM_CHUNKID_SHIFT               (GMM_CHUNK_SHIFT - PAGE_SHIFT)
/** The NIL Chunk ID value. */
#define NIL_GMM_CHUNKID                 0
/** The NIL Page ID value. */
#define NIL_GMM_PAGEID                  0


/** @} */

#endif

