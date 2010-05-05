/* $Id$ */
/** @file
 * pam_vbox - PAM module for auto logons.
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
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

#ifdef _DEBUG
 #define PAM_DEBUG
#endif

#ifdef RT_OS_SOLARIS
# include <security/pam_appl.h>
#endif
#include <security/pam_modules.h>
#include <security/pam_appl.h>
#ifdef RT_OS_LINUX
#include <security/_pam_macros.h>
#endif

#include <pwd.h>
#include <syslog.h>

#include <iprt/env.h>
#include <iprt/stream.h>
#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <VBox/log.h>

#include <VBox/VBoxGuestLib.h>

#define VBOX_MODULE_NAME "pam_vbox"

/** For debugging. */
#ifdef _DEBUG
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
        syslog(LOG_ERR, pszBuf);
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
static void pam_vbox_error(pam_handle_t *h, const char *pszFormat, ...)
{
    va_list va;
    char *buf;
    va_start(va, pszFormat);
    /** @todo is this NULL terminated? */
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
static void pam_vbox_log(pam_handle_t *h, const char *pszFormat, ...)
{
    va_list va;
    char *buf;
    va_start(va, pszFormat);
    /** @todo is this NULL terminated? */
    if (RT_SUCCESS(RTStrAPrintfV(&buf, pszFormat, va)))
    {
        if (g_verbosity)
        {
            /* Only do normal logging in debug mode; could contain
             * sensitive data! */
            LogRel(("%s: %s", VBOX_MODULE_NAME, buf));
            /* Log to syslog */
            pam_vbox_writesyslog(buf);
        }
        RTStrFree(buf);
    }
    va_end(va);
}


static int pam_vbox_do_check(pam_handle_t *h)
{
    int rc;
    int pamrc;

#ifdef _DEBUG
    g_pam_handle = h; /* hack for getting assertion text */
#endif

    /* Don't make assertions panic because the PAM stack +
     * the current logon module won't work anymore (or just restart).
     * This could result in not able to log into the system anymore. */
    RTAssertSetMayPanic(false);

    rc = RTR3Init();
    if (RT_FAILURE(rc))
    {
        pam_vbox_error(h, "pam_vbox_do_check: could not init runtime! rc=%Rrc. Aborting.\n", rc);
        return PAM_SUCCESS; /* Jump out as early as we can to not mess around. */
    }

    pam_vbox_log(h, "pam_vbox_do_check: runtime initialized.\n");
    if (RT_SUCCESS(rc))
    {
        rc = VbglR3InitUser();
        if (RT_FAILURE(rc))
        {
            switch(rc)
            {
                case VERR_ACCESS_DENIED:
                    pam_vbox_error(h, "pam_vbox_do_check: access is denied to guest driver! Please make sure you run with sufficient rights. Aborting.\n");
                    break;

                case VERR_FILE_NOT_FOUND:
                    pam_vbox_error(h, "pam_vbox_do_check: guest driver not found! Guest Additions installed? Aborting.\n");
                    break;

                default:
                    pam_vbox_error(h, "pam_vbox_do_check: could not init VbglR3 library! rc=%Rrc. Aborting.\n", rc);
                    break;
            }
            return PAM_SUCCESS; /* Jump out as early as we can to not mess around. */
        }
        pam_vbox_log(h, "pam_vbox_do_check: guest lib initialized.\n");
    }

    pamrc = PAM_OPEN_ERR; /* The PAM return code; intentionally not used as an exit value below. */
    if (RT_SUCCESS(rc))
    {
        char *rhost = NULL;
        char *tty = NULL;
        char *prompt = NULL;
#ifdef RT_OS_SOLARIS
        pam_get_item(h, PAM_RHOST, (void**) &rhost);
        pam_get_item(h, PAM_TTY, (void**) &tty);
        pam_get_item(h, PAM_USER_PROMPT, (void**) &prompt);
#else
        pam_get_item(h, PAM_RHOST, (const void**) &rhost);
        pam_get_item(h, PAM_TTY, (const void**) &tty);
        pam_get_item(h, PAM_USER_PROMPT, (const void**) &prompt);
#endif
        pam_vbox_log(h, "pam_vbox_do_check: rhost=%s, tty=%s, prompt=%s\n",
                     rhost ? rhost : "<none>", tty ? tty : "<none>", prompt ? prompt : "<none>");

        rc = VbglR3CredentialsQueryAvailability();
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_NOT_FOUND)
                pam_vbox_log(h, "pam_vbox_do_check: no credentials available.\n");
            else
                pam_vbox_error(h, "pam_vbox_do_check: could not query for credentials! rc=%Rrc. Aborting.\n", rc);
            pamrc = PAM_SUCCESS;
        }
        else
        {
            char *pszUsername;
            char *pszPassword;
            char *pszDomain;

            rc = VbglR3CredentialsRetrieve(&pszUsername, &pszPassword, &pszDomain);
            if (RT_FAILURE(rc))
            {
                pam_vbox_error(h, "pam_vbox_do_check: could not retrieve credentials! rc=%Rrc. Aborting.\n", rc);
            }
            else
            {
                pam_vbox_log(h, "pam_vbox_do_check: credentials retrieved: user=%s, password=%s, domain=%s\n",
                             pszUsername, pszPassword, pszDomain);
                /* Fill credentials into PAM. */
                pamrc = pam_set_item(h, PAM_USER, pszUsername);
                if (pamrc != PAM_SUCCESS)
                {
                    pam_vbox_error(h, "pam_vbox_do_check: could not set user name! pamrc=%d. Aborting.\n", pamrc);
                }
                else
                {
                    pamrc = pam_set_item(h, PAM_AUTHTOK, pszPassword);
                    if (pamrc != PAM_SUCCESS)
                        pam_vbox_error(h, "pam_vbox_do_check: could not set password! pamrc=%d. Aborting.\n", pamrc);
                }
                /** @todo Add handling domains as well. */

                VbglR3CredentialsDestroy(pszUsername, pszPassword, pszDomain,
                                         3 /* Three wipe passes */);
            }
        }
        VbglR3Term();
    } /* VbglR3 init okay */

#if 0 /** @todo implement RTR3Term, or create some hack to force anything containing RTR3Init to stay loaded. */
    RTR3Term();
#endif

    pam_vbox_log(h, "pam_vbox_do_check: returned with pamrc=%d, msg=%s\n",
                 pamrc, pam_strerror(h, pamrc));

    /* Never report an error here because if no credentials from the host are available or something
     * went wrong we then let do the authentication by the next module in the stack. */

    /* We report success here because this is all we can do right now -- we passed the credentials
     * to the next PAM module in the block above which then might do a shadow (like pam_unix/pam_unix2)
     * password verification to "really" authenticate the user. */
    return PAM_SUCCESS;
}


/**
 * Performs authentication within the PAM framework.
 *
 * @todo
 */
DECLEXPORT(int) pam_sm_authenticate(pam_handle_t *h, int flags,
                                    int argc, const char **argv)
{
    /* Parse arguments. */
    int i;
    for (i = 0; i < argc; i++)
    {
        if (!RTStrICmp(argv[i], "debug"))
            g_verbosity=1;
        else
            pam_vbox_error(h, "pam_sm_authenticate: unknown command line argument \"%s\"\n", argv[i]);
    }
    pam_vbox_log(h, "pam_vbox_authenticate called.\n");

    /* Do the actual check. */
    return pam_vbox_do_check(h);
}


/**
 * Modifies / deletes user credentials
 *
 * @todo
 */
DECLEXPORT(int) pam_sm_setcred(pam_handle_t *h, int flags, int argc, const char **argv)
{
    pam_vbox_log(h, "pam_vbox_setcred called.\n");
    return PAM_SUCCESS;
}


DECLEXPORT(int) pam_sm_acct_mgmt(pam_handle_t *h, int flags, int argc, const char **argv)
{
    pam_vbox_log(h, "pam_vbox_acct_mgmt called.\n");
    return PAM_SUCCESS;
}


DECLEXPORT(int) pam_sm_open_session(pam_handle_t *h, int flags, int argc, const char **argv)
{
    pam_vbox_log(h, "pam_vbox_open_session called.\n");
    RTPrintf("This session was provided by VirtualBox Guest Additions. Have a lot of fun!\n");
    return PAM_SUCCESS;
}


DECLEXPORT(int) pam_sm_close_session(pam_handle_t *h, int flags, int argc, const char **argv)
{
    pam_vbox_log(h, "pam_vbox_close_session called.\n");
    return PAM_SUCCESS;
}

DECLEXPORT(int) pam_sm_chauthtok(pam_handle_t *h, int flags, int argc, const char **argv)
{
    pam_vbox_log(h, "pam_vbox_sm_chauthtok called.\n");
    return PAM_SUCCESS;
}

#ifdef _DEBUG
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
