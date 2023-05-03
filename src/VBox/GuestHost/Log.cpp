/* $Id$ */
/** @file
 * Guest / Host common code - Logging stubs. Might be overriden by a component to fit its needs.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/string.h>

#include <VBox/log.h>
#include <VBox/GuestHost/Log.h>


/*********************************************************************************************************************************
*   Externals                                                                                                                    *
*********************************************************************************************************************************/
extern unsigned g_cVerbosity; /* Current verbosity level; must be provided by the implementation using this code. */


/*********************************************************************************************************************************
*   Implementation                                                                                                               *
*********************************************************************************************************************************/

/**
 * Logs a message with a given prefix, format string and a va_list.
 *
 * @param   pszPrefix           Log prefix to use.
 * @param   pszFormat           Format string to use.
 * @param   va                  va_list to use.
 */
static void vbghLogV(const char *pszPrefix, const char *pszFormat, va_list va)
{
    char *psz = NULL;
    RTStrAPrintfV(&psz, pszFormat, va);
    AssertPtrReturnVoid(psz);
    LogRel(("%s%s", pszPrefix ? pszPrefix : "", psz));
    RTStrFree(psz);
}

/** Logs a message with a prefix + a format string and va_list,
 *  and executes a given statement afterwards. */
#define VBGH_LOG_VALIST_STMT(a_Prefix, a_Fmt, a_VaList, a_Stmt) \
    { \
        vbghLogV(a_Prefix, a_Fmt, a_VaList); \
        a_Stmt; \
    }

/** Logs a message with a prefix + a format string and va_list . */
#define VBGH_LOG_VALIST(a_Prefix, a_Fmt, a_VaList) \
    VBGH_LOG_VALIST_STMT(a_Prefix, a_Fmt, a_VaList, do { } while (0))

/** Logs a message with a prefix, a format string (plus variable arguments),
 *  and executes a given statement afterwards. */
#define VBGH_LOG_VA_STMT(a_Prefix, a_Fmt, a_Stmt) \
    { \
        va_list va; \
        va_start(va, a_Fmt); \
        VBGH_LOG_VALIST_STMT(a_Prefix, a_Fmt, va, a_Stmt); \
        va_end(va); \
    }

/** Logs a message with a prefix + a format string (plus variable arguments). */
#define VBGH_LOG_VA(a_Prefix, a_Fmt) \
    VBGH_LOG_VA_STMT(a_Prefix, a_Fmt, do { } while (0))

/**
 * Shows a notification on the desktop (if available).
 *
 * @returns VBox status code.
 * @returns VERR_NOT_SUPPORTED if the current desktop environment is not supported.
 * @param   pszHeader           Header text to show.
 * @param   pszBody             Body text to show.
 *
 * @note    Most notification implementations have length limits on their header / body texts, so keep
 *          the text(s) short.
 */
static int vbghShowNotifyV(const char *pszHeader, const char *pszFormat, va_list va)
{
    vbghLogV(pszHeader, pszFormat, va);

    return VINF_SUCCESS;
}

/**
 * Shows a notification on the desktop (if available).
 *
 * @returns VBox status code.
 * @returns VERR_NOT_SUPPORTED if the current desktop environment is not supported.
 * @param   pszHeader           Header text to show.
 * @param   pszFormat           Format string of body text to show.
 *
 * @note    Most notification implementations have length limits on their header / body texts, so keep
 *          the text(s) short.
 */
int VBGHShowNotify(const char *pszHeader, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int rc = vbghShowNotifyV(pszHeader, pszFormat, va);
    va_end(va);

    return rc;
}

/**
 * Logs an error message with a va_list.
 *
 * @param   pszFormat   Format string..
 * @param   va          Format arguments.
 */
void VBGHLogErrorV(const char *pszFormat, va_list va)
{
    VBGH_LOG_VALIST("Error: ", pszFormat, va);
}

/**
 * Logs an error message to the (release) logging instance.
 *
 * @param   pszFormat               Format string to log.
 */
void VBGHLogError(const char *pszFormat, ...)
{
    VBGH_LOG_VA("Error: ", pszFormat);
}

/**
 * Logs a info message with a va_list.
 *
 * @param   pszFormat   Format string..
 * @param   va          Format arguments.
 */
void VBGHLogInfoV(const char *pszFormat, va_list va)
{
    VBGH_LOG_VALIST("", pszFormat, va);
}

/**
 * Logs an info message to the (release) logging instance.
 *
 * @param   pszFormat           Format string.
 */
void  VBGHLogInfo(const char *pszFormat, ...)
{
    VBGH_LOG_VA("", pszFormat);
}

/**
 * Logs a fatal error, notifies the desktop environment via a message (if available).
 *
 * @param   pszFormat   Format string..
 * @param   va          Format arguments.
 */
void VBGHLogFatalErrorV(const char *pszFormat, va_list va)
{
    VBGH_LOG_VALIST_STMT("Fatal Error: ", pszFormat, va,
                         (vbghShowNotifyV("Fatal Error: ", pszFormat, va)));
}

/**
 * Logs a fatal error, notifies the desktop environment via a message (if available).
 *
 * @param   pszFormat           Format string.
 * @param   ...                 Variable arguments for format string. Optional.
 */
void VBGHLogFatalError(const char *pszFormat, ...)
{
    VBGH_LOG_VA_STMT("Fatal Error: ", pszFormat,
                     vbghShowNotifyV("Fatal Error: ", pszFormat, va));
}

/**
 * Displays a verbose message based on the currently
 * set global verbosity level.
 *
 * @param   iLevel      Minimum log level required to display this message.
 * @param   pszFormat   Format string.
 * @param   ...         Format arguments.
 */
void VBGHLogVerbose(unsigned iLevel, const char *pszFormat, ...)
{
    if (iLevel <= g_cVerbosity)
        VBGH_LOG_VA("", pszFormat);
}

/**
 * Displays a verbose message based on the currently
 * set global verbosity level with a va_list.
 *
 * @param   iLevel      Minimum log level required to display this message.
 * @param   pszFormat   Format string.
 * @param   va          Format arguments.
 */
void VBGHLogVerboseV(unsigned iLevel, const char *pszFormat, va_list va)
{
    if (iLevel <= g_cVerbosity)
        VBGH_LOG_VALIST("", pszFormat, va);
}

