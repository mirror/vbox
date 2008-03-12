/** @file
 * Shared folders: Mappings header.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___MAPPINGS_H
#define ___MAPPINGS_H

#include "shfl.h"
#include <VBox/shflsvc.h>

typedef struct
{
    PSHFLSTRING pFolderName;
    PSHFLSTRING pMapName;
    uint32_t    cMappings;
    bool        fValid;
    bool        fHostCaseSensitive;
    bool        fGuestCaseSensitive;
    bool        fWritable;
} MAPPING, *PMAPPING;

extern MAPPING FolderMapping[SHFL_MAX_MAPPINGS];

bool vbsfMappingQuery(uint32_t iMapping, PMAPPING *pMapping);

int vbsfMappingsAdd (PSHFLSTRING pFolderName, PSHFLSTRING pMapName, uint32_t fWritable);
int vbsfMappingsRemove (PSHFLSTRING pMapName);

int vbsfMappingsQuery (SHFLCLIENTDATA *pClient, SHFLMAPPING *pMappings, uint32_t *pcMappings);
int vbsfMappingsQueryName (SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLSTRING *pString);
int vbsfMappingsQueryWritable (SHFLCLIENTDATA *pClient, SHFLROOT root, bool *fWritable);

int vbsfMapFolder (SHFLCLIENTDATA *pClient, PSHFLSTRING pszMapName, RTUTF16 delimiter, bool fCaseSensitive, SHFLROOT *pRoot);
int vbsfUnmapFolder (SHFLCLIENTDATA *pClient, SHFLROOT root);

PCRTUTF16     vbsfMappingsQueryHostRoot (SHFLROOT root, uint32_t *pcbRoot);
bool          vbsfIsGuestMappingCaseSensitive (SHFLROOT root);
bool          vbsfIsHostMappingCaseSensitive (SHFLROOT root);

#endif /* !___MAPPINGS_H */

