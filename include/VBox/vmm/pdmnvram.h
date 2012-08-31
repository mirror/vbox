/** @file
 * PDM - Pluggable Device Manager, EFI NVRAM storage back-end.
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

#ifndef ___VBox_vmm_pdmnvram_h_
#define ___VBox_vmm_pdmnvram_h_

#include <VBox/types.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_pdm_ifs_nvram       NVRAM Interface
 * @ingroup grp_pdm_interfaces
 * @{
 */

typedef struct PDMINVRAM *PPDMINVRAM;

typedef struct PDMINVRAM
{
    /**
     * This method flushes all values in the storage.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlushNvramStorage, (PPDMINVRAM pInterface));

    /**
     *  This method store NVRAM variable to storage
     */
    DECLR3CALLBACKMEMBER(int, pfnStoreNvramValue, (PPDMINVRAM pInterface, int idxVariable, RTUUID *pVendorUuid, const char *pcszVariableName, size_t cbVariableName, uint8_t *pu8Value, size_t cbValue));

    /**
     *  This method load NVRAM variable to storage
     */
    DECLR3CALLBACKMEMBER(int, pfnLoadNvramValue, (PPDMINVRAM pInterface, int idxVariable, RTUUID *pVendorUuid, char *pcszVariableName, size_t *pcbVariableName, uint8_t *pu8Value, size_t *pcbValue));

} PDMINVRAM;


#define PDMINVRAM_IID                       "11226408-CB4C-4369-9218-1EE0092FB9F8"

/** @} */

RT_C_DECLS_END


#endif


