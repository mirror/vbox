/* $Id$ */
/** @file
 * VBoxSharedFolders - Handling for shared folders
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

#include "VBoxSharedFolders.h"
#include "VBoxTray.h"
#include "helpers.h"

#include <iprt/mem.h>
#include <VBox/VBoxGuestLib.h>

int VBoxSharedFoldersAutoMount(void)
{
    uint32_t u32ClientId;
    int rc = VbglR3SharedFolderConnect(&u32ClientId);
    if (!RT_SUCCESS(rc))
        Log(("VBoxTray: Failed to connect to the shared folder service, error %Rrc\n", rc));
    else
    {
        uint32_t cMappings = 64; /* See shflsvc.h for define; should be used later. */
        uint32_t cbMappings = cMappings * sizeof(VBGLR3SHAREDFOLDERMAPPING);
        VBGLR3SHAREDFOLDERMAPPING *paMappings = (PVBGLR3SHAREDFOLDERMAPPING)RTMemAlloc(cbMappings);

        if (paMappings)
        {
            rc = VbglR3SharedFolderGetMappings(u32ClientId, true /* Only process auto-mounted folders */,
                                               paMappings, cbMappings,
                                               &cMappings);
            if (RT_SUCCESS(rc))
            {
                RT_CLAMP(cMappings, 0, 64); /* Maximum mappings, see shflsvc.h */
                for (uint32_t i = 0; i < cMappings; i++)
                {
                    char *pszName = NULL;
                    rc = VbglR3SharedFolderGetName(u32ClientId, paMappings[i].u32Root, &pszName);
                    if (   RT_SUCCESS(rc)
                        && *pszName)
                    {
                        char *pszShareName = NULL;
                        RTStrAPrintf(&pszShareName, "\\\\vboxsrv\\%s", pszName);
                        if (pszShareName)
                        {
                            NETRESOURCE resource;
                            RT_ZERO(resource);
                            resource.dwType = RESOURCETYPE_ANY;
                            resource.lpLocalName = TEXT("f:");
                            resource.lpRemoteName = TEXT(pszShareName);

                            /** @todo Figure out how to map the drives in a block (F,G,H, ...).
                                      Save the mapping for later use. */
                            DWORD dwErr = WNetAddConnection2A(&resource, NULL, NULL, 0);
                            if (dwErr == NO_ERROR)
                            {
                                LogRel(("VBoxTray: Shared folder \"%s\" was mounted to share \"%s\"\n", pszName, dwErr));
                            }
                            else
                            {
                                switch (dwErr)
                                {
                                    case ERROR_ALREADY_ASSIGNED:
                                        break;

                                    default:
                                        LogRel(("VBoxTray: Error while mounting shared folder \"%s\", error = %ld\n",
                                                pszName, dwErr));
                                }
                            }
                            RTStrFree(pszShareName);
                        }
                        RTStrFree(pszName);
                    }
                    else
                        Log(("VBoxTray: Error while getting the shared folder name for root node = %u, rc = %Rrc\n",
                             paMappings[i].u32Root, rc));
                }
            }
            else
                Log(("VBoxTray: Error while getting the shared folder mappings, rc = %Rrc\n", rc));
            RTMemFree(paMappings);
        }
        else
            rc = VERR_NO_MEMORY;
        VbglR3SharedFolderDisconnect(u32ClientId);
    }
    return rc;
}

int VBoxSharedFoldersAutoUnmount(void)
{
    //WNetCancelConnection2(name, 0, 1 /* Force disconnect */);
    return 0;
}
