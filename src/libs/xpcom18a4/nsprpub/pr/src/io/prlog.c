/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Contributors:
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per
 * MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 04/10/2000   IBM Corp.       Added DebugBreak() definitions for OS/2
 */

#if defined(VBOX) && defined(DEBUG)
# include <iprt/initterm.h> /* for RTR3InitDll */
# include <iprt/log.h>
#endif
#ifdef VBOX_USE_IPRT_IN_NSPR
# include <iprt/env.h>
# include <iprt/string.h>
#endif

#include "primpl.h"
#include "prprf.h"
#include <string.h>

/*
 * Lock used to lock the log.
 *
 * We can't define _PR_LOCK_LOG simply as PR_Lock because PR_Lock may
 * contain assertions.  We have to avoid assertions in _PR_LOCK_LOG
 * because PR_ASSERT calls PR_LogPrint, which in turn calls _PR_LOCK_LOG.
 * This can lead to infinite recursion.
 */
static PRLock *_pr_logLock;
#define _PR_LOCK_LOG() PR_Lock(_pr_logLock);
#define _PR_UNLOCK_LOG() PR_Unlock(_pr_logLock);

/*
** Use the IPRT logging facility when
** NSPR_LOG_FILE is set to "WinDebug". The default IPRT log instance
** and the "default" log group will be used for logging.
*/
#if defined(VBOX) && defined(DEBUG)
#if defined(_PR_USE_STDIO_FOR_LOGGING)
#define IPRT_DEBUG_FILE (FILE*)-3
#else
#define IPRT_DEBUG_FILE (PRFileDesc*)-3
#endif
#endif

/* Macros used to reduce #ifdef pollution */

#if defined(_PR_USE_STDIO_FOR_LOGGING) && defined(XP_PC)
#define __PUT_LOG(fd, buf, nb) \
    PR_BEGIN_MACRO \
    if (fd == WIN32_DEBUG_FILE) { \
        char savebyte = buf[nb]; \
        buf[nb] = '\0'; \
        OutputDebugString(buf); \
        buf[nb] = savebyte; \
    } else { \
        fwrite(buf, 1, nb, fd); \
        fflush(fd); \
    } \
    PR_END_MACRO
#elif defined(_PR_USE_STDIO_FOR_LOGGING)
#define __PUT_LOG(fd, buf, nb) {fwrite(buf, 1, nb, fd); fflush(fd);}
#elif defined(_PR_PTHREADS)
#define __PUT_LOG(fd, buf, nb) PR_Write(fd, buf, nb)
#else
#define __PUT_LOG(fd, buf, nb) _PR_MD_WRITE(fd, buf, nb)
#endif

#if defined(VBOX) && defined(DEBUG)
#define _PUT_LOG(fd, buf, nb) \
    PR_BEGIN_MACRO \
    if (fd == IPRT_DEBUG_FILE) { \
        Log(("%*.*S", nb, nb, buf)); \
    } else { \
        __PUT_LOG(fd, buf, nb); \
    } \
    PR_END_MACRO
#else
#define _PUT_LOG(fd, buf, nb) __PUT_LOG(fd, buf, nb)
#endif

/************************************************************************/

static PRLogModuleInfo *logModules;

static char *logBuf = NULL;
static char *logp;
static char *logEndp;
#ifdef _PR_USE_STDIO_FOR_LOGGING
static FILE *logFile = NULL;
#else
static PRFileDesc *logFile = 0;
#endif

#define LINE_BUF_SIZE           512
#define DEFAULT_BUF_SIZE        16384

void _PR_InitLog(void)
{
    const char *ev;

    _pr_logLock = PR_NewLock();

    ev = RTEnvGet("NSPR_LOG_MODULES");
    if (ev && ev[0]) {
        char module[64];  /* Security-Critical: If you change this
                           * size, you must also change the sscanf
                           * format string to be size-1.
                           */
        PRBool isSync = PR_FALSE;
        PRIntn evlen = strlen(ev), pos = 0;
        PRInt32 bufSize = DEFAULT_BUF_SIZE;
        while (pos < evlen) {
            PRIntn level = 1, count = 0, delta = 0;
            count = sscanf(&ev[pos], "%63[ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789]%n:%d%n",
                           module, &delta, &level, &delta);
            pos += delta;
            if (count == 0) break;

            /*
            ** If count == 2, then we got module and level. If count
            ** == 1, then level defaults to 1 (module enabled).
            */
            if (strcasecmp(module, "sync") == 0) {
                isSync = PR_TRUE;
            } else if (strcasecmp(module, "bufsize") == 0) {
                if (level >= LINE_BUF_SIZE) {
                    bufSize = level;
                }
            } else {
                PRLogModuleInfo *lm = logModules;
                PRBool skip_modcheck =
                    (0 == strcasecmp (module, "all")) ? PR_TRUE : PR_FALSE;

                while (lm != NULL) {
                    if (skip_modcheck) lm -> level = (PRLogModuleLevel)level;
                    else if (strcasecmp(module, lm->name) == 0) {
                        lm->level = (PRLogModuleLevel)level;
                        break;
                    }
                    lm = lm->next;
                }
            }
            /*found:*/
            count = sscanf(&ev[pos], " , %n", &delta);
            pos += delta;
            if (count == EOF) break;
        }
        PR_SetLogBuffering(isSync ? bufSize : 0);

        ev = RTEnvGet("NSPR_LOG_FILE");
        if (ev && ev[0]) {
            if (!PR_SetLogFile(ev)) {
                fprintf(stderr, "Unable to create nspr log file '%s'\n", ev);
            }
        } else {
#ifdef _PR_USE_STDIO_FOR_LOGGING
            logFile = stderr;
#else
            logFile = _pr_stderr;
#endif
        }
    }
}

void _PR_LogCleanup(void)
{
    PRLogModuleInfo *lm = logModules;

    PR_LogFlush();

#ifdef _PR_USE_STDIO_FOR_LOGGING
    if (logFile
        && logFile != stdout
        && logFile != stderr
#if defined(VBOX) && defined(DEBUG)
        && logFile != IPRT_DEBUG_FILE
#endif
#ifdef XP_PC
        && logFile != WIN32_DEBUG_FILE
#endif
        ) {
        fclose(logFile);
    }
#else
    if (logFile && logFile != _pr_stdout && logFile != _pr_stderr) {
        PR_Close(logFile);
    }
#endif

    while (lm != NULL) {
        PRLogModuleInfo *next = lm->next;
#ifdef VBOX_USE_IPRT_IN_NSPR
        RTStrFree((/*const*/ char *)lm->name);
#else
        free((/*const*/ char *)lm->name);
#endif
        PR_Free(lm);
        lm = next;
    }
    logModules = NULL;

    if (_pr_logLock) {
        PR_DestroyLock(_pr_logLock);
        _pr_logLock = NULL;
    }
}

static void _PR_SetLogModuleLevel( PRLogModuleInfo *lm )
{
    const char *ev;

    ev = RTEnvGet("NSPR_LOG_MODULES");
    if (ev && ev[0]) {
        char module[64];  /* Security-Critical: If you change this
                           * size, you must also change the sscanf
                           * format string to be size-1.
                           */
        PRIntn evlen = strlen(ev), pos = 0;
        while (pos < evlen) {
            PRIntn level = 1, count = 0, delta = 0;

            count = sscanf(&ev[pos], "%63[ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789]%n:%d%n",
                           module, &delta, &level, &delta);
            pos += delta;
            if (count == 0) break;

            /*
            ** If count == 2, then we got module and level. If count
            ** == 1, then level defaults to 1 (module enabled).
            */
            if (lm != NULL)
            {
                if ((strcasecmp(module, "all") == 0)
                    || (strcasecmp(module, lm->name) == 0))
                {
                    lm->level = (PRLogModuleLevel)level;
                }
            }
            count = sscanf(&ev[pos], " , %n", &delta);
            pos += delta;
            if (count == EOF) break;
        }
    }
} /* end _PR_SetLogModuleLevel() */

PR_IMPLEMENT(PRLogModuleInfo*) PR_NewLogModule(const char *name)
{
    PRLogModuleInfo *lm;

        if (!_pr_initialized) _PR_ImplicitInitialization();

    lm = PR_NEWZAP(PRLogModuleInfo);
    if (lm) {
#ifdef VBOX_USE_IPRT_IN_NSPR
        lm->name = RTStrDup(name);
#else
        lm->name = strdup(name);
#endif
        lm->level = PR_LOG_NONE;
        lm->next = logModules;
        logModules = lm;
        _PR_SetLogModuleLevel(lm);
    }
    return lm;
}

PR_IMPLEMENT(PRBool) PR_SetLogFile(const char *file)
{
#ifdef _PR_USE_STDIO_FOR_LOGGING

    FILE *newLogFile;

#if defined(VBOX) && defined(DEBUG)
    if (strcmp(file, "IPRT") == 0) {
        /* initialize VBox Runtime */
        RTR3InitDll(RTR3INIT_FLAGS_UNOBTRUSIVE);
        newLogFile = IPRT_DEBUG_FILE;
    }
    else
#endif
#ifdef XP_PC
    if (strcmp(file, "WinDebug") == 0) {
        newLogFile = WIN32_DEBUG_FILE;
    }
    else
#endif
    {
        newLogFile = fopen(file, "w");
        if (!newLogFile)
            return PR_FALSE;

        /* We do buffering ourselves. */
        setvbuf(newLogFile, NULL, _IONBF, 0);
    }
    if (logFile
        && logFile != stdout
        && logFile != stderr
#if defined(VBOX) && defined(DEBUG)
        && logFile != IPRT_DEBUG_FILE
#endif
#ifdef XP_PC
        && logFile != WIN32_DEBUG_FILE
#endif
        ) {
        fclose(logFile);
    }
    logFile = newLogFile;
    return PR_TRUE;

#else /* _PR_USE_STDIO_FOR_LOGGING */

    PRFileDesc *newLogFile;

#if defined(VBOX) && defined(DEBUG)
    if (strcmp(file, "IPRT") == 0) {
        /* initialize VBox Runtime */
        RTR3InitDll(RTR3INIT_FLAGS_UNOBTRUSIVE);
        logFile = IPRT_DEBUG_FILE;
        return PR_TRUE;
    }
#endif

    newLogFile = PR_Open(file, PR_WRONLY|PR_CREATE_FILE|PR_TRUNCATE, 0666);
    if (newLogFile) {
        if (logFile && logFile != _pr_stdout && logFile != _pr_stderr) {
            PR_Close(logFile);
        }
        logFile = newLogFile;
    }
    return (PRBool) (newLogFile != 0);

#endif /* _PR_USE_STDIO_FOR_LOGGING */
}

PR_IMPLEMENT(void) PR_SetLogBuffering(PRIntn buffer_size)
{
    PR_LogFlush();

    if (logBuf)
        PR_DELETE(logBuf);

    if (buffer_size >= LINE_BUF_SIZE) {
        logp = logBuf = (char*) PR_MALLOC(buffer_size);
        logEndp = logp + buffer_size;
    }
}

PR_IMPLEMENT(void) PR_LogPrint(const char *fmt, ...)
{
    va_list ap;
    char line[LINE_BUF_SIZE];
    PRUint32 nb;
    PRThread *me;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (!logFile) {
        return;
    }

    va_start(ap, fmt);
    me = PR_GetCurrentThread();
    nb = PR_snprintf(line, sizeof(line)-1, "%ld[%p]: ",
                     me ? me->id : 0L, me);

    nb += PR_vsnprintf(line+nb, sizeof(line)-nb-1, fmt, ap);
    if (nb > 0) {
        if (line[nb-1] != '\n') {
            line[nb++] = '\015';
            line[nb] = '\0';
        }
    }
    va_end(ap);

    _PR_LOCK_LOG();
    if (logBuf == 0) {
        _PUT_LOG(logFile, line, nb);
    } else {
        if (logp + nb > logEndp) {
            _PUT_LOG(logFile, logBuf, logp - logBuf);
            logp = logBuf;
        }
        memcpy(logp, line, nb);
        logp += nb;
    }
    _PR_UNLOCK_LOG();
    PR_LogFlush();
}

PR_IMPLEMENT(void) PR_LogFlush(void)
{
    if (logBuf && logFile) {
        _PR_LOCK_LOG();
            if (logp > logBuf) {
                _PUT_LOG(logFile, logBuf, logp - logBuf);
                logp = logBuf;
            }
        _PR_UNLOCK_LOG();
    }
}

PR_IMPLEMENT(void) PR_Abort(void)
{
    PR_LogPrint("Aborting");
    abort();
}

PR_IMPLEMENT(void) PR_Assert(const char *s, const char *file, PRIntn ln)
{
    PR_LogPrint("Assertion failure: %s, at %s:%d\n", s, file, ln);
#if defined(XP_UNIX)
    fprintf(stderr, "Assertion failure: %s, at %s:%d\n", s, file, ln);
#endif
    abort();
}

