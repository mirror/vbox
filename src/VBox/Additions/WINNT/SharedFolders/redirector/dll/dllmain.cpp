/*++

Copyright (c) 1989-1999  Microsoft Corporation

Module Name:

    dllmain.c

Abstract:

    This module implements the initialization routines for network
    provider interface

Notes:

    This module has been built and tested only in UNICODE environment

--*/

#include <windows.h>
#include <process.h>

#include "vboxmrxp.h"

// NOTE:
//
// Function:    DllMain
//
// Return:  TRUE  => Success
//      FALSE => Failure

BOOL WINAPI DllMain(HINSTANCE hDLLInst, DWORD fdwReason, LPVOID lpvReserved)
{
    BOOL    bStatus = TRUE;

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:

        RTR3InitDll(0);
        VbglR3Init();
        LogRel(("VBOXNP: DLL loaded.\n"));
        break;

    case DLL_PROCESS_DETACH:

        LogRel(("VBOXNP: DLL unloaded.\n"));
        VbglR3Term();
        /// @todo RTR3Term();
        break;

    case DLL_THREAD_ATTACH:
        break;

    case DLL_THREAD_DETACH:
        break;

    default:
        break;
    }

    return(bStatus);
}

