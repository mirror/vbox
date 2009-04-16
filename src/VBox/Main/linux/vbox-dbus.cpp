/** @file
 *
 * Module to dynamically load libdbus and load all symbols
 * which are needed by VirtualBox.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "vbox-dbus.h"

#define LOG_GROUP LOG_GROUP_MAIN
#include <VBox/log.h>
#include <VBox/err.h>

#include <iprt/ldr.h>
#include <iprt/assert.h>
#include <iprt/once.h>

/** The following are the symbols which we need from libdbus. */
#define VBOX_PROXY_STUB(function, rettype, signature, shortsig) \
void (*function ## _fn)(void); \
rettype function signature \
{ return ( (rettype (*) signature) function ## _fn ) shortsig; }

#include "vbox-dbus-internal.h"

static int32_t loadDBusLibOnce(void *, void *);

#undef VBOX_PROXY_STUB

/* Now comes a table of functions to be loaded from libdbus-1 */
typedef struct
{
    const char *name;
    void (**fn)(void);
} SHARED_FUNC;

#define VBOX_PROXY_STUB(s, dummy1, dummy2, dummy3 ) { #s , & s ## _fn } ,
static SHARED_FUNC SharedFuncs[] =
{
#include "vbox-dbus-internal.h"
    { NULL, NULL }
};
#undef VBOX_PROXY_STUB

/* extern */
int VBoxLoadDBusLib(void)
{
    static RTONCE sOnce = RTONCE_INITIALIZER;

    LogFlowFunc(("\n"));
    int rc = RTOnce (&sOnce, loadDBusLibOnce, NULL, NULL);
    LogFlowFunc(("rc = %Rrc\n", rc));
    return rc;
}

/* The function which does the actual work for VBoxLoadDBusLib, serialised for
 * thread safety. */
static int32_t loadDBusLibOnce(void *, void *)
{
    int rc = VINF_SUCCESS;
    RTLDRMOD hLib;

    LogFlowFunc(("\n"));
    rc = RTLdrLoad(VBOX_DBUS_1_3_LIB, &hLib);
    if (RT_FAILURE(rc))
        LogRelFunc(("Failed to load library %s\n", VBOX_DBUS_1_3_LIB));
    for (unsigned i = 0; RT_SUCCESS(rc) && SharedFuncs[i].name != NULL; ++i)
        rc = RTLdrGetSymbol(hLib, SharedFuncs[i].name, (void**)SharedFuncs[i].fn);
    LogFlowFunc(("rc = %Rrc\n", rc));
    return rc;
}

