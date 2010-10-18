/** @file
 *
 * VBox Remote Desktop Protocol:
 * External Authentication Library:
 * Simple Authentication.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <iprt/cdefs.h>
#include <iprt/uuid.h>

#include <VBox/VRDPAuth.h>

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/VirtualBox.h>

using namespace com;

/* If defined, debug messages will be written to the specified file. */
//#define VRDPAUTH_DEBUG_FILE_NAME "/tmp/VRDPAuth.log"


static void dprintf(const char *fmt, ...)
{
#ifdef VRDPAUTH_DEBUG_FILE_NAME
    va_list va;

    va_start(va, fmt);

    char buffer[1024];

    vsnprintf(buffer, sizeof(buffer), fmt, va);

    FILE *f = fopen(VRDPAUTH_DEBUG_FILE_NAME, "ab");
    fprintf(f, "%s", buffer);
    fclose(f);

    va_end (va);
#endif
}

RT_C_DECLS_BEGIN
DECLEXPORT(VRDPAuthResult) VRDPAUTHCALL VRDPAuth2(PVRDPAUTHUUID pUuid,
                                                  VRDPAuthGuestJudgement guestJudgement,
                                                  const char *szUser,
                                                  const char *szPassword,
                                                  const char *szDomain,
                                                  int fLogon,
                                                  unsigned clientId)
{
    /* default is failed */
    VRDPAuthResult result = VRDPAuthAccessDenied;

    /* only interested in logon */
    if (!fLogon)
        /* return value ignored */
        return result;

    char uuid[RTUUID_STR_LENGTH] = {0};
    if (pUuid)
        RTUuidToStr((PCRTUUID)pUuid, (char*)uuid, RTUUID_STR_LENGTH);

    /* the user might contain a domain name, split it */
    char *user = strchr((char*)szUser, '\\');
    if (user)
        user++;
    else
        user = (char*)szUser;

    dprintf("VRDPAuth: uuid: %s, user: %s, szPassword: %s\n", uuid, user, szPassword);

#if 0
    /* this is crude stuff, but let's keep it there as a sample */
    if (getenv("VBOX_VRDP_AUTH_USER") && getenv("VBOX_VRDP_AUTH_PASSWORD"))
    {

        if (   !strcmp(getenv("VBOX_VRDP_AUTH_USER"), user)
            && !strcmp(getenv("VBOX_VRDP_AUTH_PASSWORD"), szPassword))
        {
            result = VRDPAuthAccessGranted;
        }
    }
#endif

    ComPtr<IVirtualBox> virtualBox;
    HRESULT rc;

    rc = virtualBox.createLocalObject(CLSID_VirtualBox);
    if (SUCCEEDED(rc))
    {
        Bstr key = BstrFmt("VRDPAuthSimple/users/%s", user);
        Bstr password;

        /* lookup in VM's extra data? */
        if (pUuid)
        {
            ComPtr<IMachine> machine;
            virtualBox->GetMachine(Bstr(uuid).raw(), machine.asOutParam());
            if (machine)
                machine->GetExtraData(key.raw(), password.asOutParam());
        } else
            /* lookup global extra data */
            virtualBox->GetExtraData(key.raw(), password.asOutParam());

        /* we compare the password or check for special NULL marker */
        if (   (!password.isEmpty() && (password == szPassword))
            || ((password == "[NULL]") && (!szPassword || (*szPassword == '\0'))))
        {
            result = VRDPAuthAccessGranted;
        }
    }

    return result;
}
RT_C_DECLS_END

/* Verify the function prototype. */
static PVRDPAUTHENTRY2 gpfnAuthEntry = VRDPAuth2;
