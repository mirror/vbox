/* $Id$ */
/** @file
 * Shared folders service - Mappings header.
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
 */

#ifndef ___MAPPINGS_H
#define ___MAPPINGS_H

#include "shfl.h"
#include <VBox/shflsvc.h>

typedef struct
{
    char       *pszFolderName;          /**< Directory at the host to share with the guest. */
    PSHFLSTRING pMapName;               /**< Share name for the guest. */
    uint32_t    cMappings;              /**< Number of mappings. */
    bool        fValid;                 /**< Mapping entry is used/valid. */
    bool        fHostCaseSensitive;     /**< Host file name space is case-sensitive. */
    bool        fGuestCaseSensitive;    /**< Guest file name space is case-sensitive. */
    bool        fWritable;              /**< Folder is writable for the guest. */
    PSHFLSTRING pAutoMountPoint;        /**< Where the guest should try auto-mount the folder. */
    bool        fAutoMount;             /**< Folder will be auto-mounted by the guest. */
    bool        fSymlinksCreate;        /**< Guest is able to create symlinks. */
    bool        fMissing;               /**< Mapping not invalid but host path does not exist.
                                             Any guest operation on such a folder fails! */
    bool        fPlaceholder;           /**< Mapping does not exist in the VM settings but the guest
                                             still has. fMissing is always true for this mapping. */
} MAPPING;
/** Pointer to a MAPPING structure. */
typedef MAPPING *PMAPPING;

void vbsfMappingInit(void);

bool vbsfMappingQuery(uint32_t iMapping, PMAPPING *pMapping);

int vbsfMappingsAdd(const char *pszFolderName, PSHFLSTRING pMapName, bool fWritable,
                    bool fAutoMount, PSHFLSTRING pAutoMountPoint, bool fCreateSymlinks, bool fMissing, bool fPlaceholder);
int vbsfMappingsRemove(PSHFLSTRING pMapName);

int vbsfMappingsQuery(PSHFLCLIENTDATA pClient, PSHFLMAPPING pMappings, uint32_t *pcMappings);
int vbsfMappingsQueryName(PSHFLCLIENTDATA pClient, SHFLROOT root, SHFLSTRING *pString);
int vbsfMappingsQueryWritable(PSHFLCLIENTDATA pClient, SHFLROOT root, bool *fWritable);
int vbsfMappingsQueryAutoMount(PSHFLCLIENTDATA pClient, SHFLROOT root, bool *fAutoMount);
int vbsfMappingsQuerySymlinksCreate(PSHFLCLIENTDATA pClient, SHFLROOT root, bool *fSymlinksCreate);

int vbsfMapFolder(PSHFLCLIENTDATA pClient, PSHFLSTRING pszMapName, RTUTF16 delimiter,
                  bool fCaseSensitive, SHFLROOT *pRoot);
int vbsfUnmapFolder(PSHFLCLIENTDATA pClient, SHFLROOT root);

const char* vbsfMappingsQueryHostRoot(SHFLROOT root);
int vbsfMappingsQueryHostRootEx(SHFLROOT hRoot, const char **ppszRoot, uint32_t *pcbRootLen);
bool vbsfIsGuestMappingCaseSensitive(SHFLROOT root);
bool vbsfIsHostMappingCaseSensitive(SHFLROOT root);

int vbsfMappingLoaded(const PMAPPING pLoadedMapping, SHFLROOT root);
PMAPPING vbsfMappingGetByRoot(SHFLROOT root);

#endif /* !___MAPPINGS_H */

