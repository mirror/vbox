/* $Id$ */
/** @file
 * pam_vbox - PAM module for auto logons.
 */

/*
 * Copyright (C) 2008-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define PAM_SM_PASSWORD
#define PAM_SM_SESSION

#ifdef DEBUG
# define PAM_DEBUG
#endif

#ifdef RT_OS_SOLARIS
# include <security/pam_appl.h>
#endif
#include <security/pam_modules.h>
#include <security/pam_appl.h>
#ifdef RT_OS_LINUX
# include <security/_pam_macros.h>
#endif

#include <pwd.h>
#include <syslog.h>
#include <stdlib.h>

#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/stream.h>
#include <iprt/string.h>

#include <VBox/VBoxGuestLib.h>

#include <VBox/log.h>
#ifdef VBOX_WITH_GUEST_PROPS
# include <VBox/HostServices/GuestPropertySvc.h>
  using namespace guestProp;
#endif

#define VBOX_MODULE_NAME "pam_vbox"

/** For debugging. */
#ifdef DEBUG
static pam_handle_t *g_pam_handle;
static int g_verbosity = 99;
#else
static int g_verbosity = 0;
#endif

/**
 * Write to system log.
 *
 * @param  pszBuf      Buffer to write to the log (NULL-terminated)
 */
static void pam_vbox_writesyslog(char *pszBuf)
{
#ifdef RT_OS_LINUX
    openlog("pam_vbox", LOG_PID, LOG_AUTHPRIV);
    syslog(LOG_ERR, "%s", pszBuf);
    closelog();
#elif defined(RT_OS_SOLARIS)
    syslog(LOG_ERR, "pam_vbox: %s\n", pszBuf);
#endif
}


/**
 * Displays an error message.
 *
 * @param   pszFormat   The message text.
 * @param   ...         Format arguments.
 */
static void pam_vbox_error(pam_handle_t *hPAM, const char *pszFormat, ...)
{
    va_list va;
    char *buf;
    va_start(va, pszFormat);
    if (RT_SUCCESS(RTStrAPrintfV(&buf, pszFormat, va)))
    {
        LogRel(("%s: Error: %s", VBOX_MODULE_NAME, buf));
        pam_vbox_writesyslog(buf);
        RTStrFree(buf);
    }
    va_end(va);
}


/**
 * Displays a debug message.
 *
 * @param   pszFormat   The message text.
 * @param   ...         Format arguments.
 */
static void pam_vbox_log(pam_handle_t *hPAM, const char *pszFormat, ...)
{
    if (g_verbosity)
    {
        va_list va;
        char *buf;
        va_start(va, pszFormat);
        if (RT_SUCCESS(RTStrAPrintfV(&buf, pszFormat, va)))
        {
            /* Only do normal logging in debug mode; could contain
             * sensitive data! */
            LogRel(("%s: %s", VBOX_MODULE_NAME, buf));
            /* Log to syslog */
            pam_vbox_writesyslog(buf);
            RTStrFree(buf);
        }
        va_end(va);
    }
}


static int vbox_set_msg(pam_handle_t *hPAM, int iStyle, const char *pszText)
{
    AssertPtrReturn(hPAM, VERR_INVALID_POINTER);
    AssertPtrReturn(pszText, VERR_INVALID_POINTER);

    if (!iStyle)
        iStyle = PAM_TEXT_INFO;

    int rc = VINF_SUCCESS;

    pam_message msg;
    msg.msg_style = iStyle;
#ifdef RT_OS_SOLARIS
    msg.msg = (char*)pszText;
#else
    msg.msg = pszText;
#endif

#ifdef RT_OS_SOLARIS
    pam_conv *conv = NULL;
    int pamrc = pam_get_item(hPAM, PAM_CONV, (void **)&conv);
#else
    const pam_conv *conv = NULL;
    int pamrc = pam_get_item(hPAM, PAM_CONV, (const void **)&conv);
#endif
    if (   pamrc == PAM_SUCCESS
        && conv)
    {
        pam_response *resp = NULL;
#ifdef RT_OS_SOLARIS
        pam_message *msg_p = &msg;
#else
        const pam_message *msg_p = &msg;
#endif
        pam_vbox_log(hPAM, "Showing message \"%s\" (type %d)", pszText, iStyle);

        pamrc = conv->conv(1 /* One message only */, &msg_p, &resp, conv->appdata_ptr);
        if (resp != NULL) /* If we use PAM_TEXT_INFO we never will get something back! */
        {
            if (resp->resp)
            {
                pam_vbox_log(hPAM, "Response to message \"%s\" was \"%s\"",
                             pszText, resp->resp);
                /** @todo Save response!  */
                free(resp->resp);
            }
            free(resp);
        }
    }
    else
        rc = VERR_NOT_FOUND;
    return rc;
}


static int pam_vbox_init(pam_handle_t *hPAM)
{
#ifdef DEBUG
    g_pam_handle = hPAM; /* hack for getting assertion text */
#endif

    /* Don't make assertions panic because the PAM stack +
     * the current logon module won't work anymore (or just restart).
     * This could result in not able to log into the system anymore. */
    RTAssertSetMayPanic(false);

    int rc = RTR3Init();
    if (RT_FAILURE(rc))
    {
        pam_vbox_error(hPAM, "pam_vbox_init: could not init runtime! rc=%Rrc. Aborting\n", rc);
        return PAM_SUCCESS; /* Jump out as early as we can to not mess around. */
    }

    pam_vbox_log(hPAM, "pam_vbox_init: runtime initialized\n");
    if (RT_SUCCESS(rc))
    {
        rc = VbglR3InitUser();
        if (RT_FAILURE(rc))
        {
            switch(rc)
            {
                case VERR_ACCESS_DENIED:
                    pam_vbox_error(hPAM, "pam_vbox_init: access is denied to guest driver! Please make sure you run with sufficient rights. Aborting\n");
                    break;

                case VERR_FILE_NOT_FOUND:
                    pam_vbox_error(hPAM, "pam_vbox_init: guest driver not found! Guest Additions installed? Aborting\n");
                    break;

                default:
                    pam_vbox_error(hPAM, "pam_vbox_init: could not init VbglR3 library! rc=%Rrc. Aborting\n", rc);
                    break;
            }
        }
        pam_vbox_log(hPAM, "pam_vbox_init: guest lib initialized\n");
    }

    if (RT_SUCCESS(rc))
    {
        char *rhost = NULL;
        char *tty = NULL;
        char *prompt = NULL;
#ifdef RT_OS_SOLARIS
        pam_get_item(hPAM, PAM_RHOST, (void**) &rhost);
        pam_get_item(hPAM, PAM_TTY, (void**) &tty);
        pam_get_item(hPAM, PAM_USER_PROMPT, (void**) &prompt);
#else
        pam_get_item(hPAM, PAM_RHOST, (const void**) &rhost);
        pam_get_item(hPAM, PAM_TTY, (const void**) &tty);
        pam_get_item(hPAM, PAM_USER_PROMPT, (const void**) &prompt);
#endif
        pam_vbox_log(hPAM, "pam_vbox_init: rhost=%s, tty=%s, prompt=%s\n",
                     rhost ? rhost : "<none>", tty ? tty : "<none>", prompt ? prompt : "<none>");
    }

    return rc;
}


static void pam_vbox_shutdown(pam_handle_t *hPAM)
{
    VbglR3Term();
}


static int pam_vbox_check_creds(pam_handle_t *hPAM)
{
    int rc = VbglR3CredentialsQueryAvailability();
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_NOT_FOUND)
            pam_vbox_log(hPAM, "pam_vbox_check_creds: no credentials available\n");
        else
            pam_vbox_error(hPAM, "pam_vbox_check_creds: could not query for credentials! rc=%Rrc. Aborting\n", rc);
    }
    else
    {
        char *pszUsername;
        char *pszPassword;
        char *pszDomain;

        rc = VbglR3CredentialsRetrieve(&pszUsername, &pszPassword, &pszDomain);
        if (RT_FAILURE(rc))
        {
            pam_vbox_error(hPAM, "pam_vbox_check_creds: could not retrieve credentials! rc=%Rrc. Aborting\n", rc);
        }
        else
        {
#ifdef DEBUG
            pam_vbox_log(hPAM, "pam_vbox_check_creds: credentials retrieved: user=%s, password=%s, domain=%s\n",
                         pszUsername, pszPassword, pszDomain);
#else
            /* Don't log passwords in release mode! */
            pam_vbox_log(hPAM, "pam_vbox_check_creds: credentials retrieved: user=%s, password=XXX, domain=%s\n",
                         pszUsername, pszDomain);
#endif
            /* Fill credentials into PAM. */
            int pamrc = pam_set_item(hPAM, PAM_USER, pszUsername);
            if (pamrc != PAM_SUCCESS)
            {
                pam_vbox_error(hPAM, "pam_vbox_check_creds: could not set user name! pamrc=%d, msg=%s. Aborting\n",
                               pamrc, pam_strerror(hPAM, pamrc));
            }
            else
            {
                pamrc = pam_set_item(hPAM, PAM_AUTHTOK, pszPassword);
                if (pamrc != PAM_SUCCESS)
                    pam_vbox_error(hPAM, "pam_vbox_check_creds: could not set password! pamrc=%d, msg=%s. Aborting\n",
                                   pamrc, pam_strerror(hPAM, pamrc));
            }
            /** @todo Add handling domains as well. */

            VbglR3CredentialsDestroy(pszUsername, pszPassword, pszDomain,
                                     3 /* Three wipe passes */);
           pam_vbox_log(hPAM, "pam_vbox_check_creds: returned with pamrc=%d, msg=%s\n",
                        pamrc, pam_strerror(hPAM, pamrc));
        }
    }

    pam_vbox_log(hPAM, "pam_vbox_check_creds: returned with rc=%Rrc\n", rc);
    return rc;
}

#ifdef VBOX_WITH_GUEST_PROPS
static int pam_vbox_wait_for_creds(pam_handle_t *hPAM, uint32_t uClientID, uint32_t uTimeoutMS)
{
    int rc;

    /* The buffer for storing the data and its initial size.  We leave a bit
     * of space here in case the maximum values are raised. */
    void *pvBuf = NULL;
    uint32_t cbBuf = MAX_NAME_LEN + MAX_VALUE_LEN + MAX_FLAGS_LEN + _1K;

    pam_vbox_log(hPAM, "Waiting for credentials (%dms) ...\n", uTimeoutMS);

    int i;
    for (i = 0; i < 10; i++)
    {
        void *pvTmpBuf = RTMemRealloc(pvBuf, cbBuf);
        if (pvTmpBuf)
        {
            char *pszName = NULL;
            char *pszValue = NULL;
            uint64_t u64TimestampOut = 0;
            char *pszFlags = NULL;

            pvBuf = pvTmpBuf;
            rc = VbglR3GuestPropWait(uClientID, "/VirtualBox/GuestAdd/PAM/CredsChanged", pvBuf, cbBuf,
                                     0 /* Last timestamp; just wait for next event */, uTimeoutMS,
                                     &pszName, &pszValue, &u64TimestampOut,
                                     &pszFlags, &cbBuf);
        }
        else
            rc = VERR_NO_MEMORY;

        switch (rc)
        {
            case VINF_SUCCESS:
                pam_vbox_error(hPAM, "Got notification for supplied credentials\n");
                break;

            case VERR_BUFFER_OVERFLOW:
            {
                /* Buffer too small, try it with a bigger one next time. */
                cbBuf += _1K;
                continue; /* Try next round. */
            }

            case VERR_INTERRUPTED:
                pam_vbox_error(hPAM, "The credentials notification request timed out or was interrupted\n");
                break;

            case VERR_TIMEOUT:
                pam_vbox_error(hPAM, "Credentials did not arrive within time (%dms)\n", uTimeoutMS);
                break;

            case VERR_TOO_MUCH_DATA:
                pam_vbox_error(hPAM, "Temporarily unable to get credentials notification\n");
                break;

            default:
                pam_vbox_error(hPAM, "The credentials notification request failed with rc=%Rrc\n", rc);
                break;
        }

        /* Everything except VERR_BUFFER_OVERLOW makes us bail out ... */
        break;
    }

    pam_vbox_log(hPAM, "Waiting for credentials returned with rc=%Rrc\n", rc);
    return rc;
}

static int pam_vbox_read_prop(pam_handle_t *hPAM, uint32_t uClientID,
                              const char *pszKey, bool fReadOnly,
                              char *pszValue, size_t cbValue)
{
    AssertReturn(uClientID, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszKey, VERR_INVALID_POINTER);
    AssertPtrReturn(pszValue, VERR_INVALID_POINTER);

    int rc;

    uint64_t u64Timestamp = 0;
    char *pszValTemp;
    char *pszFlags = NULL;
    /* The buffer for storing the data and its initial size.  We leave a bit
     * of space here in case the maximum values are raised. */
    void *pvBuf = NULL;
    uint32_t cbBuf = MAX_VALUE_LEN + MAX_FLAGS_LEN + _1K;

    /* Because there is a race condition between our reading the size of a
     * property and the guest updating it, we loop a few times here and
     * hope.  Actually this should never go wrong, as we are generous
     * enough with buffer space. */
    for (unsigned i = 0; i < 10; i++)
    {
        void *pvTmpBuf = RTMemRealloc(pvBuf, cbBuf);
        if (pvTmpBuf)
        {
            pvBuf = pvTmpBuf;
            rc = VbglR3GuestPropRead(uClientID, pszKey, pvBuf, cbBuf,
                                     &pszValTemp, &u64Timestamp, &pszFlags,
                                     &cbBuf);
        }
        else
            rc = VERR_NO_MEMORY;

        switch (rc)
        {
            case VERR_BUFFER_OVERFLOW:
            {
                /* Buffer too small, try it with a bigger one next time. */
                cbBuf += _1K;
                continue; /* Try next round. */
            }

            default:
                break;
        }

        /* Everything except VERR_BUFFER_OVERLOW makes us bail out ... */
        break;
    }

    if (RT_SUCCESS(rc))
    {
        /* Check security bits. */
        if (pszFlags)
        {
            if (   fReadOnly
                && !RTStrStr(pszFlags, "RDONLYGUEST"))
            {
                /* If we want a property which is read-only on the guest
                 * and it is *not* marked as such, deny access! */
                pam_vbox_error(hPAM, "pam_vbox_read_prop: key \"%s\" should be read-only on guest but it is not\n",
                               pszKey);
                rc = VERR_ACCESS_DENIED;
            }
        }
        else /* No flags, no access! */
        {
            pam_vbox_error(hPAM, "pam_vbox_read_prop: key \"%s\" contains no/wrong flags (%s)\n",
                           pszKey, pszFlags);
            rc = VERR_ACCESS_DENIED;
        }

        if (RT_SUCCESS(rc))
        {
            /* If everything went well copy property value to our destination buffer. */
            if (!RTStrPrintf(pszValue, cbValue, "%s", pszValTemp))
            {
                pam_vbox_error(hPAM, "pam_vbox_read_prop: could not store value of key \"%s\"\n",
                               pszKey);
                rc = VERR_INVALID_PARAMETER;
            }
        }
    }

    pam_vbox_log(hPAM, "pam_vbox_read_prop: read key \"%s\"=\"%s\" with rc=%Rrc\n",
                 pszKey, pszValue, rc);
    return rc;
}
#endif


/**
 * Performs authentication within the PAM framework.
 *
 * @todo
 */
DECLEXPORT(int) pam_sm_authenticate(pam_handle_t *hPAM, int iFlags,
                                    int argc, const char **argv)
{
    /* Parse arguments. */
    int i;
    for (i = 0; i < argc; i++)
    {
        if (!RTStrICmp(argv[i], "debug"))
            g_verbosity = 1;
        else
            pam_vbox_error(hPAM, "pam_sm_authenticate: unknown command line argument \"%s\"\n", argv[i]);
    }
    pam_vbox_log(hPAM, "pam_vbox_authenticate called\n");

    int rc = pam_vbox_init(hPAM);
    if (RT_FAILURE(rc))
        return PAM_SUCCESS; /* Jump out as early as we can to not mess around. */

    bool fFallback = true;

#ifdef VBOX_WITH_GUEST_PROPS
    uint32_t uClientId;
    rc = VbglR3GuestPropConnect(&uClientId);
    if (RT_SUCCESS(rc))
    {
        char szVal[256];
        rc = pam_vbox_read_prop(hPAM, uClientId,
                                "/VirtualBox/GuestAdd/PAM/CredsWait",
                                true /* Read-only on guest */,
                                szVal, sizeof(szVal));
        if (RT_SUCCESS(rc))
        {
            /* All calls which are checked against rc2 are not critical, e.g. it does
             * not matter if they succeed or not. */
            uint32_t uTimeoutMS = RT_INDEFINITE_WAIT; /* Wait infinite by default. */
            int rc2 = pam_vbox_read_prop(hPAM, uClientId,
                                         "/VirtualBox/GuestAdd/PAM/CredsWaitTimeout",
                                         true /* Read-only on guest */,
                                         szVal, sizeof(szVal));
            if (RT_SUCCESS(rc2))
            {
                uTimeoutMS = RTStrToUInt32(szVal);
                if (!uTimeoutMS)
                {
                    pam_vbox_error(hPAM, "pam_sm_authenticate: invalid waiting timeout value specified, defaulting to infinite timeout\n");
                    uTimeoutMS = RT_INDEFINITE_WAIT;
                }
                else
                    uTimeoutMS = uTimeoutMS * 1000; /* Make ms out of s. */
            }

            rc2 = pam_vbox_read_prop(hPAM, uClientId,
                                     "/VirtualBox/GuestAdd/PAM/CredsMsgWaiting",
                                     true /* Read-only on guest */,
                                     szVal, sizeof(szVal));
            const char *pszWaitMsg = NULL;
            if (RT_SUCCESS(rc2))
                pszWaitMsg = szVal;

            rc2 = vbox_set_msg(hPAM, 0 /* Info message */,
                               pszWaitMsg ? pszWaitMsg : "Waiting for credentials ...");
            if (RT_FAILURE(rc2)) /* Not critical. */
                pam_vbox_error(hPAM, "pam_sm_authenticate: error setting waiting information message, rc=%Rrc\n", rc2);

            if (RT_SUCCESS(rc))
            {
                /* Before we actuall wait for credentials just make sure we didn't already get credentials
                 * set so that we can skip waiting for them ... */
                rc = pam_vbox_check_creds(hPAM);
                if (rc == VERR_NOT_FOUND)
                {
                    rc = pam_vbox_wait_for_creds(hPAM, uClientId, uTimeoutMS);
                    if (RT_SUCCESS(rc))
                    {
                        /* Waiting for credentials succeeded, try getting those ... */
                        rc = pam_vbox_check_creds(hPAM);
                        if (RT_FAILURE(rc))
                            pam_vbox_error(hPAM, "pam_sm_authenticate: no credentials given, even when waited for it, rc=%Rrc\n", rc);
                    }
                    else if (rc == VERR_TIMEOUT)
                    {
                        pam_vbox_log(hPAM, "pam_sm_authenticate: no credentials given within time\n", rc);

                        rc2 = pam_vbox_read_prop(hPAM, uClientId,
                                                 "/VirtualBox/GuestAdd/PAM/CredsMsgWaitTimeout",
                                                 true /* Read-only on guest */,
                                                 szVal, sizeof(szVal));
                        if (RT_SUCCESS(rc2))
                            rc2 = vbox_set_msg(hPAM, 0 /* Info message */, szVal);
                    }
                }

                /* If we got here we don't need the fallback, so just deactivate it. */
                fFallback = false;
            }
        }

        VbglR3GuestPropDisconnect(uClientId);
    }
#endif /* VBOX_WITH_GUEST_PROPS */

    if (fFallback)
    {
        pam_vbox_log(hPAM, "pam_vbox_authenticate: falling back to old method\n");

        /* If anything went wrong in the code above we just do a credentials
         * check like it was before: Try retrieving the stuff and authenticating. */
        int rc2 = pam_vbox_check_creds(hPAM);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    pam_vbox_shutdown(hPAM);

    pam_vbox_log(hPAM, "pam_vbox_authenticate: overall result rc=%Rrc\n", rc);

    /* Never report an error here because if no credentials from the host are available or something
     * went wrong we then let do the authentication by the next module in the stack. */

    /* We report success here because this is all we can do right now -- we passed the credentials
     * to the next PAM module in the block above which then might do a shadow (like pam_unix/pam_unix2)
     * password verification to "really" authenticate the user. */
    return PAM_SUCCESS;
}


/**
 * Modifies / deletes user credentials
 *
 * @todo
 */
DECLEXPORT(int) pam_sm_setcred(pam_handle_t *hPAM, int iFlags, int argc, const char **argv)
{
    pam_vbox_log(hPAM, "pam_vbox_setcred called\n");
    return PAM_SUCCESS;
}


DECLEXPORT(int) pam_sm_acct_mgmt(pam_handle_t *hPAM, int iFlags, int argc, const char **argv)
{
    pam_vbox_log(hPAM, "pam_vbox_acct_mgmt called\n");
    return PAM_SUCCESS;
}


DECLEXPORT(int) pam_sm_open_session(pam_handle_t *hPAM, int iFlags, int argc, const char **argv)
{
    pam_vbox_log(hPAM, "pam_vbox_open_session called\n");
    RTPrintf("This session was provided by VirtualBox Guest Additions. Have a lot of fun!\n");
    return PAM_SUCCESS;
}


DECLEXPORT(int) pam_sm_close_session(pam_handle_t *hPAM, int iFlags, int argc, const char **argv)
{
    pam_vbox_log(hPAM, "pam_vbox_close_session called\n");
    return PAM_SUCCESS;
}

DECLEXPORT(int) pam_sm_chauthtok(pam_handle_t *hPAM, int iFlags, int argc, const char **argv)
{
    pam_vbox_log(hPAM, "pam_vbox_sm_chauthtok called\n");
    return PAM_SUCCESS;
}

#ifdef DEBUG
DECLEXPORT(void) RTAssertMsg1Weak(const char *pszExpr, unsigned uLine, const char *pszFile, const char *pszFunction)
{
    pam_vbox_log(g_pam_handle,
                 "\n!!Assertion Failed!!\n"
                 "Expression: %s\n"
                 "Location  : %s(%d) %s\n",
                 pszExpr, pszFile, uLine, pszFunction);
    RTAssertMsg1(pszExpr, uLine, pszFile, pszFunction);
}
#endif
