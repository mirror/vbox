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


        /* release the VirtualBox object */
        virtualBox->Release();

    } while (0);

    CoUninitialize();
    return 0;
}


