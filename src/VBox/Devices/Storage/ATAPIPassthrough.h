/* $Id$ */
/** @file
 * VBox storage devices: ATAPI passthrough helpers (common code for DevATA and DevAHCI).
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Storage_ATAPIPassthrough_h
#define VBOX_INCLUDED_SRC_Storage_ATAPIPassthrough_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/cdefs.h>
#include <VBox/vmm/pdmifs.h>
#include <VBox/vmm/pdmstorageifs.h>

RT_C_DECLS_BEGIN

/**
 * Opaque media track list.
 */
typedef struct TRACKLIST *PTRACKLIST;

DECLHIDDEN(int) ATAPIPassthroughTrackListCreateEmpty(PTRACKLIST *ppTrackList);
DECLHIDDEN(void) ATAPIPassthroughTrackListDestroy(PTRACKLIST pTrackList);
DECLHIDDEN(void) ATAPIPassthroughTrackListClear(PTRACKLIST pTrackList);
DECLHIDDEN(int)  ATAPIPassthroughTrackListUpdate(PTRACKLIST pTrackList, const uint8_t *pbCDB, const void *pvBuf, size_t cbBuf);
DECLHIDDEN(uint32_t) ATAPIPassthroughTrackListGetSectorSizeFromLba(PTRACKLIST pTrackList, uint32_t iAtapiLba);
DECLHIDDEN(bool) ATAPIPassthroughParseCdb(const uint8_t *pbCdb, size_t cbCdb, size_t cbBuf,
                                          PTRACKLIST pTrackList, uint8_t *pbSense, size_t cbSense,
                                          PDMMEDIATXDIR *penmTxDir, size_t *pcbXfer,
                                          size_t *pcbSector, uint8_t *pu8ScsiSts);

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_SRC_Storage_ATAPIPassthrough_h */
