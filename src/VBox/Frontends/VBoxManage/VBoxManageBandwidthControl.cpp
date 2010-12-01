/* $Id$ */
/** @file
 * VBoxManage - The bandwidth control related commands.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_ONLY_DOCS

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/VirtualBox.h>

#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/ctype.h>
#include <iprt/stream.h>
#include <iprt/getopt.h>
#include <VBox/log.h>

#include "VBoxManage.h"
using namespace com;


// funcs
///////////////////////////////////////////////////////////////////////////////


static const RTGETOPTDEF g_aBandwidthControlOptions[] =
{
    { "--name",   'n', RTGETOPT_REQ_STRING },
    { "--add",    'a', RTGETOPT_REQ_STRING },
    { "--limit",  'l', RTGETOPT_REQ_UINT32 },
    { "--delete", 'd', RTGETOPT_REQ_NOTHING },
};

int handleBandwidthControl(HandlerArg *a)
{
    int c = VERR_INTERNAL_ERROR;        /* initialized to shut up gcc */
    HRESULT rc = S_OK;
    const char *pszName  = NULL;
    const char *pszType  = NULL;
    ULONG cMaxMbPerSec   = UINT32_MAX;
    bool fDelete = false;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    ComPtr<IMachine> machine;
    ComPtr<IBandwidthControl> bwCtrl;
    ComPtr<IBandwidthGroup> bwGroup;

    if (a->argc < 4)
        return errorSyntax(USAGE_BANDWIDTHCONTROL, "Too few parameters");
    else if (a->argc > 7)
        return errorSyntax(USAGE_BANDWIDTHCONTROL, "Too many parameters");

    RTGetOptInit(&GetState, a->argc, a->argv, g_aBandwidthControlOptions,
                 RT_ELEMENTS(g_aBandwidthControlOptions), 1, RTGETOPTINIT_FLAGS_NO_STD_OPTS);

    while (   SUCCEEDED(rc)
           && (c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'n':   // bandwidth group name
            {
                if (ValueUnion.psz)
                    pszName = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'a':   // add
            {
                if (ValueUnion.psz)
                    pszType = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'l': // limit
            {
                cMaxMbPerSec = ValueUnion.u32;
                break;
            }

            case 'd':   // delete
            {
                fDelete = true;
                break;
            }

            default:
            {
                errorGetOpt(USAGE_BANDWIDTHCONTROL, c, &ValueUnion);
                rc = E_FAIL;
                break;
            }
        }
    }

    if (FAILED(rc))
        return 1;

    if (!pszName)
        return errorSyntax(USAGE_BANDWIDTHCONTROL, "Bandwidth group name not specified");

    /* try to find the given machine */
    CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                               machine.asOutParam()), 1);

    /* open a session for the VM (new or shared) */
    CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), 1);
    SessionType_T st;
    CHECK_ERROR_RET(a->session, COMGETTER(Type)(&st), 1);
    bool fRunTime = (st == SessionType_Shared);

    if (fRunTime && pszType)
    {
        errorArgument("Bandwidth groups cannot be created while the VM is running\n");
        goto leave;
    }

    if (fRunTime && fDelete)
    {
        errorArgument("Bandwidth groups cannot be deleted while the VM is running\n");
        goto leave;
    }

    if (fDelete && pszType)
    {
        errorArgument("Bandwidth groups cannot be created and deleted at the same time\n");
        goto leave;
    }

    /* get the mutable session machine */
    a->session->COMGETTER(Machine)(machine.asOutParam());
    rc = machine->COMGETTER(BandwidthControl)(bwCtrl.asOutParam());
    if (FAILED(rc)) goto leave;

    /* Add a new group if requested. */
    if (pszType)
    {
        BandwidthGroupType_T enmType;

        if (!RTStrICmp(pszType, "disk"))
            enmType = BandwidthGroupType_Disk;
        else if (!RTStrICmp(pszType, "network"))
            enmType = BandwidthGroupType_Network;
        else
        {
            errorArgument("Invalid bandwidth group type\n");
            goto leave;
        }

        CHECK_ERROR(bwCtrl, CreateBandwidthGroup(Bstr(pszName).raw(), enmType, cMaxMbPerSec));
    }
    else if (fDelete)
    {
        CHECK_ERROR(bwCtrl, DeleteBandwidthGroup(Bstr(pszName).raw()));
    }
    else if (cMaxMbPerSec != UINT32_MAX)
    {
        CHECK_ERROR(bwCtrl, GetBandwidthGroup(Bstr(pszName).raw(), bwGroup.asOutParam()));
        if (SUCCEEDED(rc))
            CHECK_ERROR(bwGroup, COMSETTER(MaxMbPerSec)(cMaxMbPerSec));
    }

    /* commit changes */
    if (SUCCEEDED(rc))
        CHECK_ERROR(machine, SaveSettings());

leave:
    /* it's important to always close sessions */
    a->session->UnlockMachine();

    return SUCCEEDED(rc) ? 0 : 1;
}

#endif /* !VBOX_ONLY_DOCS */

