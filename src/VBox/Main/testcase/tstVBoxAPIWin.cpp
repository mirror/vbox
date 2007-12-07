/** @file
 *
 * tstVBoxAPIWin - sample program to illustrate the VirtualBox
 *                 COM API for machine management on Windows.
                   It only uses standard C/C++ and COM semantics,
 *                 no additional VBox classes/macros/helpers. To
 *                 make things even easier to follow, only the
 *                 standard Win32 API has been used. Typically,
 *                 C++ developers would make use of Microsoft's
 *                 ATL to ease development.
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

/*
 * PURPOSE OF THIS SAMPLE PROGRAM
 * ------------------------------
 *
 * This sample program is intended to demonstrate the minimal code necessary
 * to use VirtualBox COM API for learning puroses only. The program uses pure
 * Win32 API and doesn't have any extra dependencies to let you better
 * understand what is going on when a client talks to the VirtualBox core
 * using the COM framework.
 *
 * However, if you want to write a real application, it is highly recommended
 * to use our MS COM XPCOM Glue library and helper C++ classes. This way, you
 * will get at least the following benefits:
 *
 * a) better portability: both the MS COM (used on Windows) and XPCOM (used
 *    everywhere else) VirtualBox client application from the same source code
 *    (including common smart C++ templates for automatic interface pointer
 *    reference counter and string data management);
 * b) simpler XPCOM initialization and shutdown (only a signle method call
 *    that does everything right).
 *
 * Currently, there is no separate sample program that uses the VirtualBox MS
 * COM XPCOM Glue library. Please refer to the sources of stock VirtualBox
 * applications such as the VirtualBox GUI frontend or the VBoxManage command
 * line frontend.
 */


#include <stdio.h>
#include "VirtualBox.h"


int listVMs(IVirtualBox *virtualBox)
{
    HRESULT rc;

    /*
     * First we have to get a list of all registered VMs
     */
    IMachineCollection *collection = NULL;
    IMachineEnumerator *enumerator = NULL;

    do
    {
        rc = virtualBox->get_Machines(&collection);
        if (SUCCEEDED(rc))
            rc = collection->Enumerate(&enumerator);

        if (SUCCEEDED(rc))
        {
            BOOL hasMore;
            while (enumerator->HasMore(&hasMore), hasMore)
            {
                /*
                 * Get the machine object
                 */
                IMachine *machine = NULL;
                rc = enumerator->GetNext(&machine);
                if (SUCCEEDED(rc))
                {
                    BSTR str;

                    machine->get_Name(&str);
                    printf("Name: %S\n", str);

                    SysFreeString(str);

                    machine->Release();
                }
            }
        }
    } while (0);

    if (enumerator)
        enumerator->Release();
    if (collection)
        collection->Release();

    return 0;
}

int testErrorInfo(IVirtualBox *virtualBox)
{
    HRESULT rc;

    /* try to find a machine that doesn't exist */
    IMachine *machine = NULL;
    BSTR machineName = SysAllocString(L"Foobar");

    rc = virtualBox->FindMachine(machineName, &machine);

    if (FAILED(rc))
    {
        IErrorInfo *errorInfo;

        rc = GetErrorInfo(0, &errorInfo);

        if (FAILED(rc))
            printf("Error getting error info! rc = 0x%x\n", rc);
        else
        {
            BSTR errorDescription = NULL;

            rc = errorInfo->GetDescription(&errorDescription);

            if (FAILED(rc) || !errorDescription)
                printf("Error getting error description! rc = 0x%x\n", rc);
            else
            {
                printf("Successfully retrieved error description: %S\n", errorDescription);

                SysFreeString(errorDescription);
            }

            errorInfo->Release();
        }
    }

    if (machine)
        machine->Release();

    SysFreeString(machineName);

    return 0;
}


int main(int argc, char *argv[])
{
    HRESULT rc;
    IVirtualBox *virtualBox;

    do
    {
        /* initialize the COM subsystem */
        CoInitialize(NULL);

        /* instantiate the VirtualBox root object */
        rc = CoCreateInstance(CLSID_VirtualBox,       /* the VirtualBox base object */
                              NULL,                   /* no aggregation */
                              CLSCTX_LOCAL_SERVER,    /* the object lives in a server process on this machine */
                              IID_IVirtualBox,        /* IID of the interface */
                              (void**)&virtualBox);

        if (!SUCCEEDED(rc))
        {
            printf("Error creating VirtualBox instance! rc = 0x%x\n", rc);
            break;
        }

        listVMs(virtualBox);

        testErrorInfo(virtualBox);

        /* release the VirtualBox object */
        virtualBox->Release();

    } while (0);

    CoUninitialize();
    return 0;
}


