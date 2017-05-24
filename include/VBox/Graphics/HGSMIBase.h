/** @file
 * VBox Host Guest Shared Memory Interface (HGSMI) buffer management.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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


#ifndef ___VBox_Graphics_HGSMIBase_h___
#define ___VBox_Graphics_HGSMIBase_h___

#include <HGSMI.h>
#include <HGSMIContext.h>
#include <VBoxVideoIPRT.h>

RT_C_DECLS_BEGIN

/** @name Base HGSMI Buffer APIs
 * @{ */

/** Acknowlege an IRQ. */
DECLINLINE(void) VBoxHGSMIClearIrq(PHGSMIHOSTCOMMANDCONTEXT pCtx)
{
    VBVO_PORT_WRITE_U32(pCtx->port, HGSMIOFFSET_VOID);
}

DECLHIDDEN(void *)   VBoxHGSMIBufferAlloc(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                                          HGSMISIZE cbData,
                                          uint8_t u8Ch,
                                          uint16_t u16Op);
DECLHIDDEN(void)     VBoxHGSMIBufferFree(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                                         void *pvBuffer);
DECLHIDDEN(int)      VBoxHGSMIBufferSubmit(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                                           void *pvBuffer);
/** @}  */

RT_C_DECLS_END

#endif
