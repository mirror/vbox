/* $Id$ */
/** @file
 * VBoxAutostart - VirtualBox Autostart service.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>

#include <VBox/com/EventQueue.h>
#include <VBox/com/listeners.h>
#include <VBox/com/VirtualBox.h>

#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/version.h>

#include <package-generated.h>

#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/critsect.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/time.h>
#include <iprt/ctype.h>


#include <algorithm>
#include <list>
#include <string>
#include <signal.h>

using namespace com;

/**
 * Tokenizer instance data for the config data.
 */
typedef struct CFGTOKENIZER
{
    /** Config file handle. */
    PRTSTREAM hStrmConfig;
    /** String buffer for the current line we are operating in. */
    char      *pszLine;
    /** Size of the string buffer. */
    size_t     cbLine;
    /** Current position in the line. */
    char      *pszLineCurr;
    /** Current line in the config file. */
    unsigned   iLine;
} CFGTOKENIZER, *PCFGTOKENIZER;

/**
 * VM list entry.
 */
typedef struct AUTOSTARTVM
{
    /** ID of the VM to start. */
    Bstr  strId;
    /** Startup delay of the VM. */
    ULONG uStartupDelay;
} AUTOSTARTVM;

static ComPtr<IVirtualBoxClient> g_pVirtualBoxClient = NULL;
static bool                      g_fVerbose    = false;
static ComPtr<IVirtualBox>       g_pVirtualBox = NULL;
static ComPtr<ISession>          g_pSession    = NULL;

/** Logging parameters. */
static uint32_t      g_cHistory = 10;                   /* Enable log rotation, 10 files. */
static uint32_t      g_uHistoryFileTime = RT_SEC_1DAY;  /* Max 1 day per file. */
static uint64_t      g_uHistoryFileSize = 100 * _1M;    /* Max 100MB per file. */

/** Run in background. */
static bool          g_fDaemonize = false;

/**
 * Command line arguments.
 */
static const RTGETOPTDEF g_aOptions[] = {
#if defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD) || defined(RT_OS_DARWIN)
    { "--background",           'b',                                       RTGETOPT_REQ_NOTHING },
#endif
    /** For displayHelp(). */
    { "--help",                 'h',                                       RTGETOPT_REQ_NOTHING },
    { "--verbose",              'v',                                       RTGETOPT_REQ_NOTHING },
    { "--start",                's',                                       RTGETOPT_REQ_NOTHING },
    { "--stop",                 'd',                                       RTGETOPT_REQ_NOTHING },
    { "--config",               'c',                                       RTGETOPT_REQ_STRING },
    { "--logfile",              'F',                                       RTGETOPT_REQ_STRING },
    { "--logrotate",            'R',                                       RTGETOPT_REQ_UINT32 },
    { "--logsize",              'S',                                       RTGETOPT_REQ_UINT64 },
    { "--loginterval",          'I',                                       RTGETOPT_REQ_UINT32 },
    { "--quiet",                'Q',                                       RTGETOPT_REQ_NOTHING }
};


static void serviceLog(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    char *psz = NULL;
    RTStrAPrintfV(&psz, pszFormat, args);
    va_end(args);

    LogRel(("%s", psz));

    RTStrFree(psz);
}

#define serviceLogVerbose(a) if (g_fVerbose) { serviceLog a; }

/**
 * Reads the next line from the config stream.
 *
 * @returns VBox status code.
 * @param   pCfgTokenizer    The config tokenizer.
 */
static int autostartConfigTokenizerReadNextLine(PCFGTOKENIZER pCfgTokenizer)
{
    int rc = VINF_SUCCESS;

    do
    {
        rc = RTStrmGetLine(pCfgTokenizer->hStrmConfig, pCfgTokenizer->pszLine,
                           pCfgTokenizer->cbLine);
        if (rc == VERR_BUFFER_OVERFLOW)
        {
            char *pszTmp;

            pCfgTokenizer->cbLine += 128;
            pszTmp = (char *)RTMemRealloc(pCfgTokenizer->pszLine, pCfgTokenizer->cbLine);
            if (pszTmp)
                pCfgTokenizer->pszLine = pszTmp;
            else
                rc = VERR_NO_MEMORY;
        }
    } while (rc == VERR_BUFFER_OVERFLOW);

    if (RT_SUCCESS(rc))
    {
        pCfgTokenizer->iLine++;
        pCfgTokenizer->pszLineCurr = pCfgTokenizer->pszLine;
    }

    return rc;
}

/**
 * Creates the config tokenizer from the given filename.
 *
 * @returns VBox status code.
 * @param   pszFilename    Config filename.
 * @param   ppCfgTokenizer Where to store the pointer to the config tokenizer on
 *                         success.
 */
static int autostartConfigTokenizerCreate(const char *pszFilename, PCFGTOKENIZER *ppCfgTokenizer)
{
    int rc = VINF_SUCCESS;
    PCFGTOKENIZER pCfgTokenizer = (PCFGTOKENIZER)RTMemAllocZ(sizeof(CFGTOKENIZER));

    if (pCfgTokenizer)
    {
        pCfgTokenizer->iLine = 1;
        pCfgTokenizer->cbLine = 128;
        pCfgTokenizer->pszLine = (char *)RTMemAllocZ(pCfgTokenizer->cbLine);
        if (pCfgTokenizer->pszLine)
        {
            rc = RTStrmOpen(pszFilename, "r", &pCfgTokenizer->hStrmConfig);
            if (RT_SUCCESS(rc))
                rc = autostartConfigTokenizerReadNextLine(pCfgTokenizer);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_SUCCESS(rc))
        *ppCfgTokenizer = pCfgTokenizer;
    else if (   RT_FAILURE(rc)
             && pCfgTokenizer)
    {
        if (pCfgTokenizer->pszLine)
            RTMemFree(pCfgTokenizer->pszLine);
        if (pCfgTokenizer->hStrmConfig)
            RTStrmClose(pCfgTokenizer->hStrmConfig);
        RTMemFree(pCfgTokenizer);
    }

    return rc;
}

/**
 * Destroys the given config tokenizer.
 *
 * @returns nothing.
 * @param   pCfgTokenizer    The config tokenizer to destroy.
 */
static void autostartConfigTokenizerDestroy(PCFGTOKENIZER pCfgTokenizer)
{
    if (pCfgTokenizer->pszLine)
        RTMemFree(pCfgTokenizer->pszLine);
    if (pCfgTokenizer->hStrmConfig)
        RTStrmClose(pCfgTokenizer->hStrmConfig);
    RTMemFree(pCfgTokenizer);
}

/**
 * Read the next token from the config file.
 *
 * @returns VBox status code.
 * @param   pCfgTokenizer    The config tokenizer data.
 * @param   ppszToken        Where to store the start to the next token on success.
 * @param   pcchToken        Where to store the number of characters of the next token
 *                           excluding the \0 terminator on success.
 */
static int autostartConfigTokenizerReadNext(PCFGTOKENIZER pCfgTokenizer, const char **ppszToken,
                                            size_t *pcchToken)
{
    if (!pCfgTokenizer->pszLineCurr)
        return VERR_EOF;

    int rc = VINF_SUCCESS;

    for (;;)
    {
        char *pszTok = pCfgTokenizer->pszLineCurr;

        /* Skip all spaces. */
        while (RT_C_IS_BLANK(*pszTok))
            pszTok++;

        /* Check if we have to read a new line. */
        if (   *pszTok == '\0'
            || *pszTok == '#')
        {
            rc = autostartConfigTokenizerReadNextLine(pCfgTokenizer);
            if (RT_FAILURE(rc))
                break;
            /* start from the beginning. */
        }
        else if (   *pszTok == '='
                 || *pszTok == ',')
        {
            *ppszToken = pszTok;
            *pcchToken = 1;
            pCfgTokenizer->pszLineCurr = pszTok + 1;
            break;
        }
        else
        {
            /* Get the complete token. */
            size_t cchToken = 1;
            char *pszTmp = pszTok + 1;

            while (   RT_C_IS_ALNUM(*pszTmp)
                   || *pszTmp == '_')
            {
                pszTmp++;
                cchToken++;
            }

            *ppszToken = pszTok;
            *pcchToken = cchToken;
            pCfgTokenizer->pszLineCurr = pszTmp;
            break;
        }
    }

    return rc;
}

static int autostartConfigTokenizerCheckAndConsume(PCFGTOKENIZER pCfgTokenizer, const char *pszTokCheck)
{
    int rc = VINF_SUCCESS;
    const char *pszToken = NULL;
    size_t cchToken = 0;

    rc = autostartConfigTokenizerReadNext(pCfgTokenizer, &pszToken, &cchToken);
    if (RT_SUCCESS(rc))
    {
        if (RTStrNCmp(pszToken, pszTokCheck, cchToken))
        {
            RTMsgError("Unexpected token at line %d, expected '%s'",
                       pCfgTokenizer->iLine, pszTokCheck);
            rc = VERR_INVALID_PARAMETER;
        }
    }
    return rc;
}

/**
 * Returns the start of the next token without consuming it.
 *
 * @returns VBox status code.
 * @param   pCfgTokenizer    Tokenizer instance data.
 * @param   ppszTok          Where to store the start of the next token on success.
 */
static int autostartConfigTokenizerPeek(PCFGTOKENIZER pCfgTokenizer, const char **ppszTok)
{
    int rc = VINF_SUCCESS;

    for (;;)
    {
        char *pszTok = pCfgTokenizer->pszLineCurr;

        /* Skip all spaces. */
        while (RT_C_IS_BLANK(*pszTok))
            pszTok++;

        /* Check if we have to read a new line. */
        if (   *pszTok == '\0'
            || *pszTok == '#')
        {
            rc = autostartConfigTokenizerReadNextLine(pCfgTokenizer);
            if (RT_FAILURE(rc))
                break;
            /* start from the beginning. */
        }
        else
        {
            *ppszTok = pszTok;
            break;
        }
    }

    return rc;
}

/**
 * Check whether the given token is a reserved token.
 *
 * @returns true if the token is reserved or false otherwise.
 * @param   pszToken    The token to check.
 * @param   cchToken    Size of the token in characters.
 */
static bool autostartConfigTokenizerIsReservedToken(const char *pszToken, size_t cchToken)
{
    if (   cchToken == 1
        && (   *pszToken == ','
            || *pszToken == '='))
        return true;
    else if (   cchToken > 1
             && (   !RTStrNCmp(pszToken, "default_policy", cchToken)
                 || !RTStrNCmp(pszToken, "exception_list", cchToken)))
        return true;

    return false;
}

/**
 * Parse the given configuration file and return the interesting config parameters.
 *
 * @returns VBox status code.
 * @param   pszFilename    The config file to parse.
 * @param   pfAllowed      Where to store the flag whether the user of this process
 *                         is allowed to start VMs automatically during system startup.
 * @param   puStartupDelay Where to store the startup delay for the user.
 */
static int autostartParseConfig(const char *pszFilename, bool *pfAllowed, uint32_t *puStartupDelay)
{
    int rc = VINF_SUCCESS;
    char *pszUserProcess = NULL;
    bool fDefaultAllow = false;
    bool fInExceptionList = false;

    AssertPtrReturn(pfAllowed, VERR_INVALID_POINTER);
    AssertPtrReturn(puStartupDelay, VERR_INVALID_POINTER);

    *pfAllowed = false;
    *puStartupDelay = 0;

    rc = RTProcQueryUsernameA(RTProcSelf(), &pszUserProcess);
    if (RT_SUCCESS(rc))
    {
        PCFGTOKENIZER pCfgTokenizer = NULL;

        rc = autostartConfigTokenizerCreate(pszFilename, &pCfgTokenizer);
        if (RT_SUCCESS(rc))
        {
            do
            {
                size_t cchToken = 0;
                const char *pszToken = NULL;

                rc = autostartConfigTokenizerReadNext(pCfgTokenizer, &pszToken,
                                                      &cchToken);
                if (RT_SUCCESS(rc))
                {
                    if (!RTStrNCmp(pszToken, "default_policy", strlen("default_policy")))
                    {
                        rc = autostartConfigTokenizerCheckAndConsume(pCfgTokenizer, "=");
                        if (RT_SUCCESS(rc))
                        {
                            rc = autostartConfigTokenizerReadNext(pCfgTokenizer, &pszToken,
                                                                  &cchToken);
                            if (RT_SUCCESS(rc))
                            {
                                if (!RTStrNCmp(pszToken, "allow", strlen("allow")))
                                    fDefaultAllow = true;
                                else if (!RTStrNCmp(pszToken, "deny", strlen("deny")))
                                    fDefaultAllow = false;
                                else
                                {
                                    RTMsgError("Unexpected token at line %d, expected either 'allow' or 'deny'",
                                               pCfgTokenizer->iLine);
                                    rc = VERR_INVALID_PARAMETER;
                                    break;
                                }
                            }
                        }
                    }
                    else if (!RTStrNCmp(pszToken, "exception_list", strlen("exception_list")))
                    {
                        rc = autostartConfigTokenizerCheckAndConsume(pCfgTokenizer, "=");
                        if (RT_SUCCESS(rc))
                        {
                            rc = autostartConfigTokenizerReadNext(pCfgTokenizer, &pszToken,
                                                                  &cchToken);

                            while (RT_SUCCESS(rc))
                            {
                                if (autostartConfigTokenizerIsReservedToken(pszToken, cchToken))
                                {
                                    RTMsgError("Unexpected token at line %d, expected a username",
                                               pCfgTokenizer->iLine);
                                    rc = VERR_INVALID_PARAMETER;
                                    break;
                                }
                                else if (!RTStrNCmp(pszUserProcess, pszToken, strlen(pszUserProcess)))
                                    fInExceptionList = true;

                                /* Skip , */
                                rc = autostartConfigTokenizerPeek(pCfgTokenizer, &pszToken);
                                if (   RT_SUCCESS(rc)
                                    && *pszToken == ',')
                                {
                                    rc = autostartConfigTokenizerCheckAndConsume(pCfgTokenizer, ",");
                                    AssertRC(rc);
                                }
                                else if (RT_SUCCESS(rc))
                                    break;

                                rc = autostartConfigTokenizerReadNext(pCfgTokenizer, &pszToken,
                                                                      &cchToken);
                            }

                            if (rc == VERR_EOF)
                                rc = VINF_SUCCESS;
                        }
                    }
                    else if (!autostartConfigTokenizerIsReservedToken(pszToken, cchToken))
                    {
                        /* Treat as 'username = <base delay in seconds>. */
                        rc = autostartConfigTokenizerCheckAndConsume(pCfgTokenizer, "=");
                        if (RT_SUCCESS(rc))
                        {
                            size_t cchDelay = 0;
                            const char *pszDelay = NULL;

                            rc = autostartConfigTokenizerReadNext(pCfgTokenizer, &pszDelay,
                                                                  &cchDelay);
                            if (RT_SUCCESS(rc))
                            {
                                uint32_t uDelay = 0;

                                rc = RTStrToUInt32Ex(pszDelay, NULL, 10, &uDelay);
                                if (rc == VWRN_TRAILING_SPACES)
                                    rc = VINF_SUCCESS;

                                if (   RT_SUCCESS(rc)
                                    && !RTStrNCmp(pszUserProcess, pszToken, strlen(pszUserProcess)))
                                        *puStartupDelay = uDelay;

                                if (RT_FAILURE(rc))
                                    RTMsgError("Unexpected token at line %d, expected a number",
                                               pCfgTokenizer->iLine);
                            }
                        }
                    }
                    else
                    {
                        RTMsgError("Unexpected token at line %d, expected a username",
                                   pCfgTokenizer->iLine);
                        rc = VERR_INVALID_PARAMETER;
                    }
                }
                else if (rc == VERR_EOF)
                {
                    rc = VINF_SUCCESS;
                    break;
                }
            } while (RT_SUCCESS(rc));

            if (   RT_SUCCESS(rc)
                && (   (fDefaultAllow && !fInExceptionList)
                    || (!fDefaultAllow && fInExceptionList)))
                *pfAllowed= true;

            autostartConfigTokenizerDestroy(pCfgTokenizer);
        }

        RTStrFree(pszUserProcess);
    }

    return rc;
}

static DECLCALLBACK(bool) autostartVMCmp(const AUTOSTARTVM &vm1, const AUTOSTARTVM &vm2)
{
    return vm1.uStartupDelay <= vm2.uStartupDelay;
}

/**
 * Main routine for the autostart daemon.
 *
 * @returns exit status code.
 */
static RTEXITCODE autostartMain(uint32_t uStartupDelay)
{
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    int vrc = VINF_SUCCESS;
    std::list<AUTOSTARTVM> listVM;

    if (uStartupDelay)
    {
        serviceLogVerbose(("Delay starting for %d seconds ...\n", uStartupDelay));
        vrc = RTThreadSleep(uStartupDelay * 1000);
    }

    if (vrc == VERR_INTERRUPTED)
        return RTEXITCODE_SUCCESS;

    /*
     * Build a list of all VMs we need to autostart first, apply the overrides
     * from the configuration and start the VMs afterwards.
     */
    com::SafeIfaceArray<IMachine> machines;
    HRESULT rc = g_pVirtualBox->COMGETTER(Machines)(ComSafeArrayAsOutParam(machines));
    if (SUCCEEDED(rc))
    {
        /*
         * Iterate through the collection
         */
        for (size_t i = 0; i < machines.size(); ++i)
        {
            if (machines[i])
            {
                BOOL fAccessible;
                CHECK_ERROR_BREAK(machines[i], COMGETTER(Accessible)(&fAccessible));
                if (!fAccessible)
                    continue;

                BOOL fAutostart;
                CHECK_ERROR_BREAK(machines[i], COMGETTER(AutostartEnabled)(&fAutostart));
                if (fAutostart)
                {
                    AUTOSTARTVM autostartVM;

                    CHECK_ERROR_BREAK(machines[i], COMGETTER(Id)(autostartVM.strId.asOutParam()));
                    CHECK_ERROR_BREAK(machines[i], COMGETTER(AutostartDelay)(&autostartVM.uStartupDelay));

                    listVM.push_back(autostartVM);
                }
            }
        }

        if (   SUCCEEDED(rc)
            && listVM.size())
        {
            ULONG uDelayCurr = 0;

            /* Sort by startup delay and apply base override. */
            listVM.sort(autostartVMCmp);

            std::list<AUTOSTARTVM>::iterator it;
            for (it = listVM.begin(); it != listVM.end(); it++)
            {
                ComPtr<IMachine> machine;
                ComPtr<IProgress> progress;

                if ((*it).uStartupDelay > uDelayCurr)
                {
                    serviceLogVerbose(("Delay starting of the next VMs for %d seconds ...\n",
                                       (*it).uStartupDelay - uDelayCurr));
                    RTThreadSleep(((*it).uStartupDelay - uDelayCurr) * 1000);
                    uDelayCurr = (*it).uStartupDelay;
                }

                CHECK_ERROR_BREAK(g_pVirtualBox, FindMachine((*it).strId.raw(),
                                                             machine.asOutParam()));

                CHECK_ERROR_BREAK(machine, LaunchVMProcess(g_pSession, Bstr("headless").raw(),
                                                           Bstr("").raw(), progress.asOutParam()));
                if (SUCCEEDED(rc) && !progress.isNull())
                {
                    serviceLogVerbose(("Waiting for VM \"%ls\" to power on...\n", (*it).strId.raw()));
                    CHECK_ERROR(progress, WaitForCompletion(-1));
                    if (SUCCEEDED(rc))
                    {
                        BOOL completed = true;
                        CHECK_ERROR(progress, COMGETTER(Completed)(&completed));
                        if (SUCCEEDED(rc))
                        {
                            ASSERT(completed);

                            LONG iRc;
                            CHECK_ERROR(progress, COMGETTER(ResultCode)(&iRc));
                            if (SUCCEEDED(rc))
                            {
                                if (FAILED(iRc))
                                {
                                    ProgressErrorInfo info(progress);
                                    com::GluePrintErrorInfo(info);
                                }
                                else
                                    serviceLogVerbose(("VM \"%ls\" has been successfully started.\n", (*it).strId.raw()));
                            }
                        }
                    }
                }
                g_pSession->UnlockMachine();
            }
        }
    }

    return rcExit;
}

static void displayHeader()
{
    RTStrmPrintf(g_pStdErr, VBOX_PRODUCT " Autostart " VBOX_VERSION_STRING "\n"
                 "(C) " VBOX_C_YEAR " " VBOX_VENDOR "\n"
                 "All rights reserved.\n\n");
}

/**
 * Displays the help.
 *
 * @param   pszImage                Name of program name (image).
 */
static void displayHelp(const char *pszImage)
{
    AssertPtrReturnVoid(pszImage);

    displayHeader();

    RTStrmPrintf(g_pStdErr,
                 "Usage:\n"
                 " %s [-v|--verbose] [-h|-?|--help]\n"
                 " [-F|--logfile=<file>] [-R|--logrotate=<num>] [-S|--logsize=<bytes>]\n"
                 " [-I|--loginterval=<seconds>]\n"
                 " [-c|--config=<config file>]\n", pszImage);

    RTStrmPrintf(g_pStdErr, "\n"
                 "Options:\n");

    for (unsigned i = 0;
         i < RT_ELEMENTS(g_aOptions);
         ++i)
    {
        std::string str(g_aOptions[i].pszLong);
        if (g_aOptions[i].iShort < 1000) /* Don't show short options which are defined by an ID! */
        {
            str += ", -";
            str += g_aOptions[i].iShort;
        }
        str += ":";

        const char *pcszDescr = "";

        switch (g_aOptions[i].iShort)
        {
            case 'h':
                pcszDescr = "Print this help message and exit.";
                break;

#if defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD) || defined(RT_OS_DARWIN)
            case 'b':
                pcszDescr = "Run in background (daemon mode).";
                break;
#endif

            case 'F':
                pcszDescr = "Name of file to write log to (no file).";
                break;

            case 'R':
                pcszDescr = "Number of log files (0 disables log rotation).";
                break;

            case 'S':
                pcszDescr = "Maximum size of a log file to trigger rotation (bytes).";
                break;

            case 'I':
                pcszDescr = "Maximum time interval to trigger log rotation (seconds).";
                break;

            case 'c':
                pcszDescr = "Name of the configuration file for the global overrides.";
                break;
        }

        RTStrmPrintf(g_pStdErr, "%-23s%s\n", str.c_str(), pcszDescr);
    }

    RTStrmPrintf(g_pStdErr, "\nUse environment variable VBOXAUTOSTART_RELEASE_LOG for logging options.\n");
}

/**
 * Creates all global COM objects.
 *
 * @return  HRESULT
 */
static int autostartSetup()
{
    serviceLogVerbose(("Setting up ...\n"));

    /*
     * Setup VirtualBox + session interfaces.
     */
    HRESULT rc = g_pVirtualBoxClient->COMGETTER(VirtualBox)(g_pVirtualBox.asOutParam());
    if (SUCCEEDED(rc))
    {
        rc = g_pSession.createInprocObject(CLSID_Session);
        if (FAILED(rc))
            RTMsgError("Failed to create a session object (rc=%Rhrc)!", rc);
    }
    else
        RTMsgError("Failed to get VirtualBox object (rc=%Rhrc)!", rc);

    if (FAILED(rc))
        return VERR_COM_OBJECT_NOT_FOUND;

    return VINF_SUCCESS;
}

static void autostartShutdown()
{
    serviceLogVerbose(("Shutting down ...\n"));

    g_pSession.setNull();
    g_pVirtualBox.setNull();
}

int main(int argc, char *argv[])
{
    /*
     * Before we do anything, init the runtime without loading
     * the support driver.
     */
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Parse the global options
    */
    int c;
    const char *pszLogFile = NULL;
    const char *pszConfigFile = NULL;
    bool fQuiet = false;
    bool fStart = false;
    bool fStop  = false;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv,
                 g_aOptions, RT_ELEMENTS(g_aOptions), 1 /* First */, 0 /*fFlags*/);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'h':
                displayHelp(argv[0]);
                return 0;

            case 'v':
                g_fVerbose = true;
                break;

#if defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD) || defined(RT_OS_DARWIN)
            case 'b':
                g_fDaemonize = true;
                break;
#endif
            case 'V':
                RTPrintf("%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());
                return 0;

            case 'F':
                pszLogFile = ValueUnion.psz;
                break;

            case 'R':
                g_cHistory = ValueUnion.u32;
                break;

            case 'S':
                g_uHistoryFileSize = ValueUnion.u64;
                break;

            case 'I':
                g_uHistoryFileTime = ValueUnion.u32;
                break;

            case 'Q':
                fQuiet = true;
                break;

            case 'c':
                pszConfigFile = ValueUnion.psz;
                break;

            case 's':
                fStart = true;
                break;

            case 'd':
                fStop = true;
                break;

            default:
                return RTGetOptPrintError(c, &ValueUnion);
        }
    }

    if (!fStart && !fStop)
    {
        displayHelp(argv[0]);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Either --start or --stop must be present");
    }
    else if (fStart && fStop)
    {
        displayHelp(argv[0]);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "--start or --stop are mutually exclusive");
    }

    if (!pszConfigFile)
    {
        displayHelp(argv[0]);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "--config <config file> is missing");
    }

    if (!fQuiet)
        displayHeader();

    bool fAllowed = false;
    uint32_t uStartupDelay = 0;
    rc = autostartParseConfig(pszConfigFile, &fAllowed, &uStartupDelay);
    if (RT_FAILURE(rc))
        return RTEXITCODE_FAILURE;

    if (!fAllowed)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "User is not allowed to autostart VMs");

    /* create release logger, to stdout */
    char szError[RTPATH_MAX + 128];
    rc = com::VBoxLogRelCreate("Autostart", g_fDaemonize ? NULL : pszLogFile,
                               RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME_PROG,
                               "all", "VBOXAUTOSTART_RELEASE_LOG",
                               RTLOGDEST_STDOUT, UINT32_MAX /* cMaxEntriesPerGroup */,
                               g_cHistory, g_uHistoryFileTime, g_uHistoryFileSize,
                               szError, sizeof(szError));
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to open release log (%s, %Rrc)", szError, rc);

#if defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD) || defined(RT_OS_DARWIN)
    if (g_fDaemonize)
    {
        /* prepare release logging */
        char szLogFile[RTPATH_MAX];

        if (!pszLogFile || !*pszLogFile)
        {
            rc = com::GetVBoxUserHomeDirectory(szLogFile, sizeof(szLogFile));
            if (RT_FAILURE(rc))
                 return RTMsgErrorExit(RTEXITCODE_FAILURE, "could not get base directory for logging: %Rrc", rc);
            rc = RTPathAppend(szLogFile, sizeof(szLogFile), "vboxautostart.log");
            if (RT_FAILURE(rc))
                 return RTMsgErrorExit(RTEXITCODE_FAILURE, "could not construct logging path: %Rrc", rc);
            pszLogFile = szLogFile;
        }

        rc = RTProcDaemonizeUsingFork(false /* fNoChDir */, false /* fNoClose */, NULL);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to daemonize, rc=%Rrc. exiting.", rc);
        /* create release logger, to file */
        rc = com::VBoxLogRelCreate("Autostart", pszLogFile,
                                   RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME_PROG,
                                   "all", "VBOXAUTOSTART_RELEASE_LOG",
                                   RTLOGDEST_FILE, UINT32_MAX /* cMaxEntriesPerGroup */,
                                   g_cHistory, g_uHistoryFileTime, g_uHistoryFileSize,
                                   szError, sizeof(szError));
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to open release log (%s, %Rrc)", szError, rc);
    }
#endif

    /*
     * Initialize COM.
     */
    using namespace com;
    HRESULT hrc = com::Initialize();
# ifdef VBOX_WITH_XPCOM
    if (hrc == NS_ERROR_FILE_ACCESS_DENIED)
    {
        char szHome[RTPATH_MAX] = "";
        com::GetVBoxUserHomeDirectory(szHome, sizeof(szHome));
        return RTMsgErrorExit(RTEXITCODE_FAILURE,
               "Failed to initialize COM because the global settings directory '%s' is not accessible!", szHome);
    }
# endif
    if (FAILED(hrc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to initialize COM (%Rhrc)!", hrc);

    hrc = g_pVirtualBoxClient.createInprocObject(CLSID_VirtualBoxClient);
    if (FAILED(hrc))
    {
        RTMsgError("Failed to create the VirtualBoxClient object (%Rhrc)!", hrc);
        com::ErrorInfo info;
        if (!info.isFullAvailable() && !info.isBasicAvailable())
        {
            com::GluePrintRCMessage(hrc);
            RTMsgError("Most likely, the VirtualBox COM server is not running or failed to start.");
        }
        else
            com::GluePrintErrorInfo(info);
        return RTEXITCODE_FAILURE;
    }

    rc = autostartSetup();
    if (RT_FAILURE(rc))
        return RTEXITCODE_FAILURE;

    RTEXITCODE rcExit = autostartMain(uStartupDelay);

    EventQueue::getMainEventQueue()->processEventQueue(0);

    autostartShutdown();

    g_pVirtualBoxClient.setNull();

    com::Shutdown();

    return rcExit;
}

