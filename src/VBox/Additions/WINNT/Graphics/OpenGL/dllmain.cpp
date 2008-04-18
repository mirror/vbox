/** @file
 *
 * VBoxDisp -- Windows Guest OpenGL ICD
 *
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
#include "VBoxOGL.h"
#include <VBox/version.h>
#include <iprt/cdefs.h>
#include <iprt/assert.h>


/**
 * Dll entrypoint
 *
 * @returns type size or 0 if unknown type
 * @param   hDLLInst        Dll instance handle
 * @param   fdwReason       Callback reason
 * @param   lpvReserved     Reserved
 */
BOOL WINAPI DllMain(HINSTANCE hDLLInst, DWORD fdwReason, LPVOID lpvReserved)
{
    BOOL	bStatus = TRUE;

    switch (fdwReason) 
    {
    case DLL_PROCESS_ATTACH:
        return VBoxOGLInit(hDLLInst);

    case DLL_PROCESS_DETACH:
        VBoxOGLExit();
        return TRUE;

    case DLL_THREAD_ATTACH:
        break;

    case DLL_THREAD_DETACH:
        return VBoxOGLThreadDetach();

    default:
        break;
    }

    return bStatus;
}


