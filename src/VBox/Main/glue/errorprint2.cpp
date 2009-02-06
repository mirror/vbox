/* $Id$ */

/** @file
 * MS COM / XPCOM Abstraction Layer:
 * Error info print helpers. This implements the shared code from the macros from errorprint2.h.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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


#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint2.h>
#include <VBox/log.h>

#include <iprt/stream.h>
#include <iprt/path.h>

namespace com
{


void GluePrintErrorInfo(com::ErrorInfo &info)
{
    Utf8Str str = Utf8StrFmt("ERROR: %ls\n"
                             "Details: code %Rhrc (0x%RX32), component %ls, interface %ls, callee %ls\n"
                             ,
                             info.getText().raw(),
                             info.getResultCode(),
                             info.getResultCode(),
                             info.getComponent().raw(),
                             info.getInterfaceName().raw(),
                             info.getCalleeName().raw());
    // print and log
    RTPrintf("%s", str.c_str());
    Log(("%s", str.c_str()));
}

void GluePrintRCMessage(HRESULT rc)
{
    Utf8Str str = Utf8StrFmt("ERROR: code %Rhra (extended info not available)\n", rc);
    // print and log
    RTPrintf("%s", str.c_str());
    Log(("%s", str.c_str()));
}

void GlueHandleComError(ComPtr<IUnknown> iface,
                        const char *pcszContext,
                        HRESULT rc,
                        const char *pcszSourceFile,
                        uint32_t ulLine)
{
    // if we have full error info, print something nice, and start with the actual error message
    com::ErrorInfo info(iface);
    if (info.isFullAvailable() || info.isBasicAvailable())
    {
        GluePrintErrorInfo(info);

        // pcszSourceFile comes from __FILE__ macro, which always contains the full path,
        // which we don't want to see printed:
        Utf8Str strFilename(RTPathFilename(pcszSourceFile));
        Utf8Str str = Utf8StrFmt("Context: \"%s\" at line %d of file %s\n",
                                 pcszContext,
                                 ulLine,
                                 strFilename.c_str());
        // print and log
        RTPrintf("%s", str.c_str());
        Log(("%s", str.c_str()));
    }
}


} /* namespace com */

