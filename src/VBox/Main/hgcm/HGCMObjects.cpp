/** @file
 *
 * HGCM (Host-Guest Communication Manager):
 * HGCMObjects - Host-Guest Communication Manager objects
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#define LOG_GROUP_MAIN_OVERRIDE LOG_GROUP_HGCM
#include "Logging.h"

#include "hgcm/HGCMObjects.h"

#include <string.h>

#include <VBox/err.h>


static RTCRITSECT g_critsect;

static uint32_t volatile g_u32HandleCount;

static PAVLULNODECORE g_pTree;


DECLINLINE(int) hgcmObjEnter (void)
{
    return RTCritSectEnter (&g_critsect);
}

DECLINLINE(void) hgcmObjLeave (void)
{
    RTCritSectLeave (&g_critsect);
}

int hgcmObjInit (void)
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::hgcmObjInit\n"));

    g_u32HandleCount = 0;
    g_pTree = NULL;

    rc = RTCritSectInit (&g_critsect);

    LogFlow(("MAIN::hgcmObjInit: rc = %Vrc\n", rc));

    return rc;
}

void hgcmObjUninit (void)
{
    if (RTCritSectIsInitialized (&g_critsect))
    {
        RTCritSectDelete (&g_critsect);
    }
}

uint32_t hgcmObjGenerateHandle (HGCMObject *pObject)
{
    int handle = 0;

    LogFlow(("MAIN::hgcmObjGenerateHandle: pObject %p\n", pObject));

    int rc = hgcmObjEnter ();

    if (VBOX_SUCCESS(rc))
    {
        ObjectAVLCore *pCore = &pObject->Core;

        /* Generate a new handle value. */

        uint32_t u32Start = g_u32HandleCount;

        for (;;)
        {
            uint32_t Key = ASMAtomicIncU32 (&g_u32HandleCount);

            if (Key == u32Start)
            {
                /* Rollover. Something is wrong. */
                break;
            }

            /* 0 is not a valid handle. */
            if (Key == 0)
            {
                continue;
            }

            /* Insert object to AVL tree. */
            pCore->AvlCore.Key = Key;

            bool bRC = RTAvlULInsert(&g_pTree, &pCore->AvlCore);

            /* Could not insert a handle. */
            if (!bRC)
            {
                continue;
            }

            /* Initialize backlink. */
            pCore->pSelf = pObject;

            /* Reference the object for time while it resides in the tree. */
            pObject->Reference ();

            /* Store returned handle. */
            handle = Key;

            break;
        }

        hgcmObjLeave ();
    }
    else
    {
        AssertReleaseMsgFailed (("MAIN::hgcmObjGenerateHandle: Failed to acquire object pool semaphore"));
    }

    LogFlow(("MAIN::hgcmObjGenerateHandle: handle = %d, rc = %Vrc, return void\n", handle, rc));

    return handle;
}

void hgcmObjDeleteHandle (uint32_t handle)
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::hgcmObjDeleteHandle: handle %d\n", handle));

    if (handle)
    {
        rc = hgcmObjEnter ();

        if (VBOX_SUCCESS(rc))
        {
            ObjectAVLCore *pCore = (ObjectAVLCore *)RTAvlULRemove (&g_pTree, handle);

            if (pCore)
            {
                AssertRelease(pCore->pSelf);

                pCore->pSelf->Dereference ();
            }

            hgcmObjLeave ();
        }
        else
        {
            AssertReleaseMsgFailed (("Failed to acquire object pool semaphore, rc = %Vrc", rc));
        }
    }

    LogFlow(("MAIN::hgcmObjDeleteHandle: rc = %Vrc, return void\n", rc));

    return;
}

HGCMObject *hgcmObjReference (uint32_t handle, HGCMOBJ_TYPE enmObjType)
{
    LogFlow(("MAIN::hgcmObjReference: handle %d\n", handle));

    HGCMObject *pObject = NULL;

    int rc = hgcmObjEnter ();

    if (VBOX_SUCCESS(rc))
    {
        ObjectAVLCore *pCore = (ObjectAVLCore *)RTAvlULGet (&g_pTree, handle);

        Assert(!pCore || (pCore->pSelf && pCore->pSelf->Type() == enmObjType));
        if (    pCore 
            &&  pCore->pSelf
            &&  pCore->pSelf->Type() == enmObjType)
        {
            pObject = pCore->pSelf;

            AssertRelease(pObject);

            pObject->Reference ();
        }

        hgcmObjLeave ();
    }
    else
    {
        AssertReleaseMsgFailed (("Failed to acquire object pool semaphore, rc = %Vrc", rc));
    }

    LogFlow(("MAIN::hgcmObjReference: return pObject %p\n", pObject));

    return pObject;
}

void hgcmObjDereference (HGCMObject *pObject)
{
    LogFlow(("MAIN::hgcmObjDereference: pObject %p\n", pObject));

    AssertRelease(pObject);

    pObject->Dereference ();

    LogFlow(("MAIN::hgcmObjDereference: return\n"));
}

uint32_t hgcmObjQueryHandleCount ()
{
    return g_u32HandleCount;
}

void hgcmObjSetHandleCount (uint32_t u32HandleCount)
{
    Assert(g_u32HandleCount <= u32HandleCount);

    int rc = hgcmObjEnter ();

    if (VBOX_SUCCESS(rc))
    {
        if (g_u32HandleCount <= u32HandleCount)
            g_u32HandleCount = u32HandleCount;
        hgcmObjLeave ();
   }
}
