/** @file
 *
 * VBoxVideo D3D
 *
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
#include <windows.h>
#include <d3dtypes.h>
#include <D3dumddi.h>

#include <iprt/initterm.h>
#include <iprt/log.h>
#include <VBox/Log.h>

#include <VBox/VBoxGuestLib.h>

/**
 * DLL entry point.
 */
BOOL WINAPI DllMain(HINSTANCE hInstance,
                    DWORD     dwReason,
                    LPVOID    lpReserved)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            RTR3Init();
            VbglR3Init();

            AssertBreakpoint();
            LogRel(("VBoxDispD3D: DLL loaded.\n"));

//            DisableThreadLibraryCalls(hInstance);
//            hDllInstance = hInstance;
            break;
        }

        case DLL_PROCESS_DETACH:
        {
            LogRel(("VBoxDispD3D: DLL unloaded.\n"));
            VbglR3Term();
            /// @todo RTR3Term();
            break;
        }

        default:
            break;
    }
    return TRUE;
}

__checkReturn HRESULT APIENTRY OpenAdapter(__inout D3DDDIARG_OPENADAPTER*  pOpenData)
{
    AssertBreakpoint();
    return S_OK;
}
