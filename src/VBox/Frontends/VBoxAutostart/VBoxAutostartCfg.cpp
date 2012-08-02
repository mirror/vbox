/* $Id$ */
/** @file
 * VBoxAutostart - VirtualBox Autostart service, configuration parser.
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

#include <iprt/stream.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/ctype.h>
#include <iprt/message.h>

#include "VBoxAutostart.h"

/*******************************************************************************
*   Constants And Macros, Structures and Typedefs                              *
*******************************************************************************/

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

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

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

DECLHIDDEN(int) autostartParseConfig(const char *pszFilename, bool *pfAllowed, uint32_t *puStartupDelay)
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

