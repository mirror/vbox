/* $Id$ */
/** @file
 * IPRT JSON parser API (JSON).
 */

/*
 * Copyright (C) 2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/cdefs.h>
#include <iprt/ctype.h>
#include <iprt/json.h>
#include <iprt/mem.h>
#include <iprt/string.h>

/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * JSON parser position information.
 */
typedef struct RTJSONPOS
{
    /** Line in the source. */
    size_t         iLine;
    /** Current start character .*/
    size_t         iChStart;
    /** Current end character. */
    size_t         iChEnd;
} RTJSONPOS;
/** Pointer to a position. */
typedef RTJSONPOS *PRTJSONPOS;

/**
 * JSON token class.
 */
typedef enum RTJSONTOKENCLASS
{
    /** Invalid. */
    RTJSONTOKENCLASS_INVALID = 0,
    /** Array begin. */
    RTJSONTOKENCLASS_BEGIN_ARRAY,
    /** Object begin. */
    RTJSONTOKENCLASS_BEGIN_OBJECT,
    /** Array end. */
    RTJSONTOKENCLASS_END_ARRAY,
    /** Object end. */
    RTJSONTOKENCLASS_END_OBJECT,
    /** Separator for name/value pairs. */
    RTJSONTOKENCLASS_NAME_SEPARATOR,
    /** Value separator. */
    RTJSONTOKENCLASS_VALUE_SEPARATOR,
    /** String */
    RTJSONTOKENCLASS_STRING,
    /** Number. */
    RTJSONTOKENCLASS_NUMBER,
    /** null keyword. */
    RTJSONTOKENCLASS_NULL,
    /** false keyword. */
    RTJSONTOKENCLASS_FALSE,
    /** true keyword. */
    RTJSONTOKENCLASS_TRUE,
    /** End of stream */
    RTJSONTOKENCLASS_EOS,
    /** 32bit hack. */
    RTJSONTOKENCLASS_32BIT_HACK = 0x7fffffff
} RTJSONTOKENCLASS;
/** Pointer to a token class. */
typedef RTJSONTOKENCLASS *PRTJSONTOKENCLASS;

/**
 * JSON token.
 */
typedef struct RTJSONTOKEN
{
    /** Token class. */
    RTJSONTOKENCLASS        enmClass;
    /** Token position in the source buffer. */
    RTJSONPOS               Pos;
    /** Data based on the token class. */
    union
    {
        /** String. */
        struct
        {
            /** Pointer to the start of the string. */
            char            *pszStr;
        } String;
        /** Number. */
        struct
        {
            int64_t         i64Num;
        } Number;
    } Class;
} RTJSONTOKEN;
/** Pointer to a JSON token. */
typedef RTJSONTOKEN *PRTJSONTOKEN;
/** Pointer to a const script token. */
typedef const RTJSONTOKEN *PCRTJSONTOKEN;

/**
 * Tokenizer state.
 */
typedef struct RTJSONTOKENIZER
{
    /** Char buffer to read from. */
    const char              *pszInput;
    /** Current position into the input stream. */
    RTJSONPOS               Pos;
    /** Token 1. */
    RTJSONTOKEN             Token1;
    /** Token 2. */
    RTJSONTOKEN             Token2;
    /** Pointer to the current active token. */
    PRTJSONTOKEN            pTokenCurr;
    /** The next token in the input stream (used for peeking). */
    PRTJSONTOKEN            pTokenNext;
} RTJSONTOKENIZER;
/** Pointer to a JSON tokenizer. */
typedef RTJSONTOKENIZER *PRTJSONTOKENIZER;

/** Pointer to the internal JSON value instance. */
typedef struct RTJSONVALINT *PRTJSONVALINT;

/**
 * A JSON value.
 */
typedef struct RTJSONVALINT
{
    /** Type of the JSON value. */
    RTJSONVALTYPE           enmType;
    /** Reference count for this JSON value. */
    volatile uint32_t       cRefs;
    /** Type dependent data. */
    union
    {
        /** String type*/
        struct
        {
            /** Pointer to the string. */
            char            *pszStr;
        } String;
        /** Number type. */
        struct
        {
            /** Signed 64-bit integer. */
            int64_t         i64Num;
        } Number;
        /** Array type. */
        struct
        {
            /** Number of elements in the array. */
            uint32_t        cItems;
            /** Pointer to the array of items. */
            PRTJSONVALINT   *papItems;
        } Array;
        /** Object type. */
        struct
        {
            /** Number of members. */
            uint32_t        cMembers;
            /** Pointer to the array holding the member names. */
            char            **papszNames;
            /** Pointer to the array holding the values. */
            PRTJSONVALINT   *papValues;
        } Object;
    } Type;
} RTJSONVALINT;

/**
 * A JSON iterator.
 */
typedef struct RTJSONITINT
{
    /** Referenced JSON value. */
    PRTJSONVALINT           pJsonVal;
    /** Current index. */
    unsigned                idxCur;
} RTJSONITINT;
/** Pointer to the internal JSON iterator instance. */
typedef RTJSONITINT *PRTJSONITINT;

/*********************************************************************************************************************************
*   Global variables                                                                                                             *
*********************************************************************************************************************************/

static int rtJsonParseValue(PRTJSONTOKENIZER pTokenizer, PRTJSONTOKEN pToken,
                            PRTJSONVALINT *ppJsonVal, PRTERRINFO pErrInfo);

/**
 * Returns whether the tokenizer reached the end of the stream.
 *
 * @returns true if the tokenizer reached the end of stream marker
 *          false otherwise.
 * @param   pTokenizer      The tokenizer state.
 */
DECLINLINE(bool) rtJsonTokenizerIsEos(PRTJSONTOKENIZER pTokenizer)
{
    return *pTokenizer->pszInput == '\0';
}

/**
 * Skip one character in the input stream.
 *
 * @returns nothing.
 * @param   pTokenizer      The tokenizer state.
 */
DECLINLINE(void) rtJsonTokenizerSkipCh(PRTJSONTOKENIZER pTokenizer)
{
    pTokenizer->pszInput++;
    pTokenizer->Pos.iChStart++;
    pTokenizer->Pos.iChEnd++;
}

/**
 * Returns the next char in the input buffer without advancing it.
 *
 * @returns Next character in the input buffer.
 * @param   pTokenizer      The tokenizer state.
 */
DECLINLINE(char) rtJsonTokenizerPeekCh(PRTJSONTOKENIZER pTokenizer)
{
    return   rtJsonTokenizerIsEos(pTokenizer)
           ? '\0'
           : *(pTokenizer->pszInput + 1);
}

/**
 * Returns the next character in the input buffer advancing the internal
 * position.
 *
 * @returns Next character in the stream.
 * @param   pTokenizer      The tokenizer state.
 */
DECLINLINE(char) rtJsonTokenizerGetCh(PRTJSONTOKENIZER pTokenizer)
{
    char ch;

    if (rtJsonTokenizerIsEos(pTokenizer))
        ch = '\0';
    else
        ch = *pTokenizer->pszInput;

    return ch;
}

/**
 * Sets a new line for the tokenizer.
 *
 * @returns nothing.
 * @param   pTokenizer      The tokenizer state.
 */
DECLINLINE(void) rtJsonTokenizerNewLine(PRTJSONTOKENIZER pTokenizer, unsigned cSkip)
{
    pTokenizer->pszInput += cSkip;
    pTokenizer->Pos.iLine++;
    pTokenizer->Pos.iChStart = 1;
    pTokenizer->Pos.iChEnd   = 1;
}

/**
 * Checks whether the current position in the input stream is a new line
 * and skips it.
 *
 * @returns Flag whether there was a new line at the current position
 *          in the input buffer.
 * @param   pTokenizer      The tokenizer state.
 */
DECLINLINE(bool) rtJsonTokenizerIsSkipNewLine(PRTJSONTOKENIZER pTokenizer)
{
    bool fNewline = true;

    if (   rtJsonTokenizerGetCh(pTokenizer) == '\r'
        && rtJsonTokenizerPeekCh(pTokenizer) == '\n')
        rtJsonTokenizerNewLine(pTokenizer, 2);
    else if (rtJsonTokenizerGetCh(pTokenizer) == '\n')
        rtJsonTokenizerNewLine(pTokenizer, 1);
    else
        fNewline = false;

    return fNewline;
}

/**
 * Skip all whitespace starting from the current input buffer position.
 * Skips all present comments too.
 *
 * @returns nothing.
 * @param   pTokenizer      The tokenizer state.
 */
DECLINLINE(void) rtJsonTokenizerSkipWhitespace(PRTJSONTOKENIZER pTokenizer)
{
    while (!rtJsonTokenizerIsEos(pTokenizer))
    {
        while (   rtJsonTokenizerGetCh(pTokenizer) == ' '
               || rtJsonTokenizerGetCh(pTokenizer) == '\t')
            rtJsonTokenizerSkipCh(pTokenizer);

        if (   !rtJsonTokenizerIsEos(pTokenizer)
            && !rtJsonTokenizerIsSkipNewLine(pTokenizer))
        {
            break; /* Skipped everything, next is some real content. */
        }
    }
}

/**
 * Get an literal token from the tokenizer.
 *
 * @returns IPRT status code.
 * @param   pTokenizer    The tokenizer state.
 * @param   pToken        The uninitialized token.
 */
static int rtJsonTokenizerGetLiteral(PRTJSONTOKENIZER pTokenizer, PRTJSONTOKEN pToken)
{
    int rc = VINF_SUCCESS;
    char ch = rtJsonTokenizerGetCh(pTokenizer);
    size_t cchLiteral = 0;
    char aszLiteral[6]; /* false + 0 terminator as the lingest possible literal. */
    RT_ZERO(aszLiteral);

    pToken->Pos = pTokenizer->Pos;

    Assert(RT_C_IS_ALPHA(ch));

    while (   RT_C_IS_ALPHA(ch)
           && cchLiteral < RT_ELEMENTS(aszLiteral) - 1)
    {
        aszLiteral[cchLiteral] = ch;
        cchLiteral++;
        rtJsonTokenizerSkipCh(pTokenizer);
        ch = rtJsonTokenizerGetCh(pTokenizer);
    }

    if (!RTStrNCmp(&aszLiteral[0], "false", RT_ELEMENTS(aszLiteral)))
        pToken->enmClass = RTJSONTOKENCLASS_FALSE;
    else if (!RTStrNCmp(&aszLiteral[0], "true", RT_ELEMENTS(aszLiteral)))
        pToken->enmClass = RTJSONTOKENCLASS_TRUE;
    else if (!RTStrNCmp(&aszLiteral[0], "null", RT_ELEMENTS(aszLiteral)))
        pToken->enmClass = RTJSONTOKENCLASS_NULL;
    else
    {
        pToken->enmClass = RTJSONTOKENCLASS_INVALID;
        rc = VERR_JSON_MALFORMED;
    }

    pToken->Pos.iChEnd += cchLiteral;
    return rc;
}

/**
 * Get a numerical constant from the tokenizer.
 *
 * @returns IPRT status code.
 * @param   pTokenizer    The tokenizer state.
 * @param   pToken        The uninitialized token.
 */
static int rtJsonTokenizerGetNumber(PRTJSONTOKENIZER pTokenizer, PRTJSONTOKEN pToken)
{
    unsigned uBase = 10;
    char *pszNext = NULL;

    Assert(RT_C_IS_DIGIT(rtJsonTokenizerGetCh(pTokenizer)));

    /* Let RTStrToInt64Ex() do all the work, looks compliant. */
    pToken->enmClass = RTJSONTOKENCLASS_NUMBER;
    int rc = RTStrToInt64Ex(pTokenizer->pszInput, &pszNext, 0, &pToken->Class.Number.i64Num);
    Assert(RT_SUCCESS(rc) || rc == VWRN_TRAILING_CHARS || rc == VWRN_TRAILING_SPACES);
    /** @todo: Handle number to big, throw a warning */

    unsigned cchNumber = pszNext - pTokenizer->pszInput;
    for (unsigned i = 0; i < cchNumber; i++)
        rtJsonTokenizerSkipCh(pTokenizer);

    return VINF_SUCCESS;
}

/**
 * Parses a string constant.
 *
 * @returns IPRT status code.
 * @param   pTokenizer    The tokenizer state.
 * @param   pToken        The uninitialized token.
 */
static int rtJsonTokenizerGetString(PRTJSONTOKENIZER pTokenizer, PRTJSONTOKEN pToken)
{
    int rc = VINF_SUCCESS;
    size_t cchStr = 0;
    char aszTmp[_4K];
    RT_ZERO(aszTmp);

    Assert(rtJsonTokenizerGetCh(pTokenizer) == '\"');
    rtJsonTokenizerSkipCh(pTokenizer); /* Skip " */

    pToken->enmClass = RTJSONTOKENCLASS_STRING;
    pToken->Pos      = pTokenizer->Pos;

    char ch = rtJsonTokenizerGetCh(pTokenizer);
    while (   ch != '\"'
           && ch != '\0')
    {
        if (ch == '\\')
        {
            /* Escape sequence, check the next character  */
            rtJsonTokenizerSkipCh(pTokenizer);
            char chNext = rtJsonTokenizerGetCh(pTokenizer);
            switch (chNext)
            {
                case '\"':
                    aszTmp[cchStr] = '\"';
                    break;
                case '\\':
                    aszTmp[cchStr] = '\\';
                    break;
                case '/':
                    aszTmp[cchStr] = '/';
                    break;
                case '\b':
                    aszTmp[cchStr] = '\b';
                    break;
                case '\n':
                    aszTmp[cchStr] = '\n';
                    break;
                case '\f':
                    aszTmp[cchStr] = '\f';
                    break;
                case '\r':
                    aszTmp[cchStr] = '\r';
                    break;
                case '\t':
                    aszTmp[cchStr] = '\t';
                    break;
                case 'u':
                    rc = VERR_NOT_SUPPORTED;
                    break;
                default:
                    rc = VERR_JSON_MALFORMED;
            }
        }
        else
            aszTmp[cchStr] = ch;
        cchStr++;
        rtJsonTokenizerSkipCh(pTokenizer);
        ch = rtJsonTokenizerGetCh(pTokenizer);
    }

    if (rtJsonTokenizerGetCh(pTokenizer) == '\"')
        rtJsonTokenizerSkipCh(pTokenizer); /* Skip closing " */

    pToken->Class.String.pszStr = RTStrDupN(&aszTmp[0], cchStr);
    if (pToken->Class.String.pszStr)
        pToken->Pos.iChEnd += cchStr;
    else
        rc = VERR_NO_STR_MEMORY;
    return rc;
}

/**
 * Get the end of stream token.
 *
 * @returns IPRT status code.
 * @param   pTokenizer    The tokenizer state.
 * @param   pToken        The uninitialized token.
 */
static int rtJsonTokenizerGetEos(PRTJSONTOKENIZER pTokenizer, PRTJSONTOKEN pToken)
{
    Assert(rtJsonTokenizerGetCh(pTokenizer) == '\0');

    pToken->enmClass = RTJSONTOKENCLASS_EOS;
    pToken->Pos      = pTokenizer->Pos;
    return VINF_SUCCESS;
}

/**
 * Read the next token from the tokenizer stream.
 *
 * @returns IPRT status code.
 * @param   pTokenizer    The tokenizer to read from.
 * @param   pToken        Uninitialized token to fill the token data into.
 */
static int rtJsonTokenizerReadNextToken(PRTJSONTOKENIZER pTokenizer, PRTJSONTOKEN pToken)
{
    int rc = VINF_SUCCESS;

    /* Skip all eventually existing whitespace and newlines first. */
    rtJsonTokenizerSkipWhitespace(pTokenizer);

    char ch = rtJsonTokenizerGetCh(pTokenizer);
    if (RT_C_IS_ALPHA(ch))
        rc = rtJsonTokenizerGetLiteral(pTokenizer, pToken);
    else if (RT_C_IS_DIGIT(ch))
        rc = rtJsonTokenizerGetNumber(pTokenizer, pToken);
    else if (ch == '\"')
        rc = rtJsonTokenizerGetString(pTokenizer, pToken);
    else if (ch == '\0')
        rc = rtJsonTokenizerGetEos(pTokenizer, pToken);
    else if (ch == '{')
        rc = pToken->enmClass == RTJSONTOKENCLASS_BEGIN_OBJECT;
    else if (ch == '}')
        rc = pToken->enmClass == RTJSONTOKENCLASS_END_OBJECT;
    else if (ch == '[')
        rc = pToken->enmClass == RTJSONTOKENCLASS_BEGIN_ARRAY;
    else if (ch == ']')
        rc = pToken->enmClass == RTJSONTOKENCLASS_END_ARRAY;
    else if (ch == ':')
        rc = pToken->enmClass == RTJSONTOKENCLASS_NAME_SEPARATOR;
    else if (ch == ',')
        rc = pToken->enmClass == RTJSONTOKENCLASS_VALUE_SEPARATOR;
    else
    {
        pToken->enmClass = RTJSONTOKENCLASS_INVALID;
        rc = VERR_JSON_MALFORMED;
    }

    return rc;
}

/**
 * Create a new tokenizer.
 *
 * @returns IPRT status code.
 * @param   pTokenizer      The tokenizer state to initialize.
 * @param   pszInput        The input to create the tokenizer for.
 */
static int rtJsonTokenizerInit(PRTJSONTOKENIZER pTokenizer, const char *pszInput)
{
    pTokenizer->pszInput     = pszInput;
    pTokenizer->Pos.iLine    = 1;
    pTokenizer->Pos.iChStart = 1;
    pTokenizer->Pos.iChEnd   = 1;
    pTokenizer->pTokenCurr   = &pTokenizer->Token1;
    pTokenizer->pTokenNext   = &pTokenizer->Token2;
    /* Fill the tokenizer with two first tokens. */
    int rc = rtJsonTokenizerReadNextToken(pTokenizer, pTokenizer->pTokenCurr);
    if (RT_SUCCESS(rc))
        rc = rtJsonTokenizerReadNextToken(pTokenizer, pTokenizer->pTokenNext);

    return rc;
}

/**
 * Destroys a given tokenizer state.
 *
 * @returns nothing.
 * @param   pTokenizer      The tokenizer to destroy.
 */
static void rtJsonTokenizerDestroy(PRTJSONTOKENIZER pTokenizer)
{

}

/**
 * Get the current token in the input stream.
 *
 * @returns Pointer to the next token in the stream.
 * @param   pTokenizer      The tokenizer state.
 * @param   ppToken         Where to store the pointer to the current token on success.
 */
DECLINLINE(int) rtJsonTokenizerGetToken(PRTJSONTOKENIZER pTokenizer, PRTJSONTOKEN *ppToken)
{
    *ppToken = pTokenizer->pTokenCurr;
    return VINF_SUCCESS;
}

/**
 * Consume the current token advancing to the next in the stream.
 *
 * @returns nothing.
 * @param   pTokenizer    The tokenizer state.
 */
static void rtJsonTokenizerConsume(PRTJSONTOKENIZER pTokenizer)
{
    PRTJSONTOKEN  pTokenTmp = pTokenizer->pTokenCurr;

    /* Switch next token to current token and read in the next token. */
    pTokenizer->pTokenCurr = pTokenizer->pTokenNext;
    pTokenizer->pTokenNext = pTokenTmp;
    rtJsonTokenizerReadNextToken(pTokenizer, pTokenizer->pTokenNext);
}

/**
 * Consumes the current token if it matches the given class returning an indicator.
 *
 * @returns true if the class matched and the token was consumed.
 * @param   false otherwise.
 * @param   pTokenizer      The tokenizer state.
 * @param   enmClass        The token class to match against.
 */
static bool rtJsonTokenizerConsumeIfMatched(PRTJSONTOKENIZER pTokenizer, RTJSONTOKENCLASS enmClass)
{
    PRTJSONTOKEN pToken = NULL;
    int rc = rtJsonTokenizerGetToken(pTokenizer, &pToken);
    AssertRC(rc);

    if (pToken->enmClass == enmClass)
    {
        rtJsonTokenizerConsume(pTokenizer);
        return true;
    }

    return false;
}

/**
 * Destroys a given JSON value releasing the reference to all child values.
 *
 * @returns nothing.
 * @param   pThis           The JSON value to destroy.
 */
static void rtJsonValDestroy(PRTJSONVALINT pThis)
{
    switch (pThis->enmType)
    {
        case RTJSONVALTYPE_OBJECT:
            for (unsigned i = 0; i < pThis->Type.Object.cMembers; i++)
            {
                RTStrFree(pThis->Type.Object.papszNames[i]);
                RTJsonValueRelease(pThis->Type.Object.papValues[i]);
            }
            break;
        case RTJSONVALTYPE_ARRAY:
            for (unsigned i = 0; i < pThis->Type.Array.cItems; i++)
                RTJsonValueRelease(pThis->Type.Array.papItems[i]);
            break;
        case RTJSONVALTYPE_STRING:
            RTStrFree(pThis->Type.String.pszStr);
            break;
        case RTJSONVALTYPE_NUMBER:
        case RTJSONVALTYPE_NULL:
        case RTJSONVALTYPE_TRUE:
        case RTJSONVALTYPE_FALSE:
            /* nothing to do. */
            break;
        default:
            AssertMsgFailed(("Invalid JSON value type: %u\n", pThis->enmType));
    }
    RTMemFree(pThis);
}

/**
 * Creates a new JSON value with the given type.
 *
 * @returns Pointer to JSON value on success, NULL if out of memory.
 * @param   enmType         The JSON value type.
 */
static PRTJSONVALINT rtJsonValueCreate(RTJSONVALTYPE enmType)
{
    PRTJSONVALINT pThis = (PRTJSONVALINT)RTMemAllocZ(sizeof(RTJSONVALINT));
    if (RT_LIKELY(pThis))
    {
        pThis->enmType = enmType;
        pThis->cRefs   = 1;
    }

    return pThis;
}

/**
 * Parses an JSON array.
 *
 * @returns IPRT status code.
 * @param   pTokenizer      The tokenizer to use.
 * @param   pVal            The JSON array value to fill in.
 * @param   pErrInfo        Where to store extended error info. Optional.
 */
static int rtJsonParseArray(PRTJSONTOKENIZER pTokenizer, PRTJSONVALINT pJsonVal, PRTERRINFO pErrInfo)
{
    int rc = VINF_SUCCESS;
    PRTJSONTOKEN pToken = NULL;
    uint32_t cItems = 0;

    rc = rtJsonTokenizerGetToken(pTokenizer, &pToken);
    while (   RT_SUCCESS(rc)
           && pToken->enmClass != RTJSONTOKENCLASS_END_ARRAY)
    {
        PRTJSONVALINT pVal = NULL;
        rc = rtJsonParseValue(pTokenizer, pToken, &pVal, pErrInfo);
        if (RT_SUCCESS(rc))
        {
            cItems++;
            /** @todo: Add value to array. */
        }

        /* Next token. */
        rtJsonTokenizerConsume(pTokenizer);
        rc = rtJsonTokenizerGetToken(pTokenizer, &pToken);
    }

    if (RT_SUCCESS(rc))
    {
        Assert(pToken->enmClass == RTJSONTOKENCLASS_END_ARRAY);
        rtJsonTokenizerConsume(pTokenizer);
        pJsonVal->Type.Array.cItems = cItems;
    }

    return rc;
}

/**
 * Parses an JSON object.
 *
 * @returns IPRT status code.
 * @param   pTokenizer      The tokenizer to use.
 * @param   pVal            The JSON object value to fill in.
 * @param   pErrInfo        Where to store extended error info. Optional.
 */
static int rtJsonParseObject(PRTJSONTOKENIZER pTokenizer, PRTJSONVALINT pJsonVal, PRTERRINFO pErrInfo)
{
    int rc = VINF_SUCCESS;
    PRTJSONTOKEN pToken = NULL;
    uint32_t cMembers = 0;

    rc = rtJsonTokenizerGetToken(pTokenizer, &pToken);
    while (   RT_SUCCESS(rc)
           && pToken->enmClass == RTJSONTOKENCLASS_STRING)
    {
        char *pszName = pToken->Class.String.pszStr;

        rtJsonTokenizerConsume(pTokenizer);
        if (rtJsonTokenizerConsumeIfMatched(pTokenizer, RTJSONTOKENCLASS_NAME_SEPARATOR))
        {
            PRTJSONVALINT pVal = NULL;
            rc = rtJsonTokenizerGetToken(pTokenizer, &pToken);
            if (RT_SUCCESS(rc))
                rc = rtJsonParseValue(pTokenizer, pToken, &pVal, pErrInfo);
            if (RT_SUCCESS(rc))
            {
                cMembers++;
                /** @todo: Add name/value pair to object. */

                /* Next token. */
                rtJsonTokenizerConsume(pTokenizer);
                rc = rtJsonTokenizerGetToken(pTokenizer, &pToken);
            }
        }
        else
            rc = VERR_JSON_MALFORMED;
    }

    if (RT_SUCCESS(rc))
    {
        if (pToken->enmClass == RTJSONTOKENCLASS_END_OBJECT)
        {
            rtJsonTokenizerConsume(pTokenizer);
            pJsonVal->Type.Object.cMembers = cMembers;
        }
        else
            rc = VERR_JSON_MALFORMED;
    }

    return rc;
}

/**
 * Parses a single JSON value and returns it on success.
 *
 * @returns IPRT status code.
 * @param   pTokenizer      The tokenizer to use.
 * @param   pToken          The token to parse.
 * @param   ppJsonVal       Where to store the pointer to the JSON value on success.
 * @param   pErrInfo        Where to store extended error info. Optional.
 */
static int rtJsonParseValue(PRTJSONTOKENIZER pTokenizer, PRTJSONTOKEN pToken,
                            PRTJSONVALINT *ppJsonVal, PRTERRINFO pErrInfo)
{
    int rc = VINF_SUCCESS;
    PRTJSONVALINT pVal = NULL;

    switch (pToken->enmClass)
    {
        case RTJSONTOKENCLASS_BEGIN_ARRAY:
            pVal = rtJsonValueCreate(RTJSONVALTYPE_ARRAY);
            if (RT_LIKELY(pVal))
                rc = rtJsonParseArray(pTokenizer, pVal, pErrInfo);
            break;
        case RTJSONTOKENCLASS_BEGIN_OBJECT:
            pVal = rtJsonValueCreate(RTJSONVALTYPE_OBJECT);
            if (RT_LIKELY(pVal))
                rc = rtJsonParseObject(pTokenizer, pVal, pErrInfo);
            break;
        case RTJSONTOKENCLASS_STRING:
            pVal = rtJsonValueCreate(RTJSONVALTYPE_STRING);
            if (RT_LIKELY(pVal))
                pVal->Type.String.pszStr = pToken->Class.String.pszStr;
            break;
        case RTJSONTOKENCLASS_NUMBER:
            pVal = rtJsonValueCreate(RTJSONVALTYPE_NUMBER);
            if (RT_LIKELY(pVal))
                pVal->Type.Number.i64Num = pToken->Class.Number.i64Num;
            break;
        case RTJSONTOKENCLASS_NULL:
            pVal = rtJsonValueCreate(RTJSONVALTYPE_NULL);
            break;
        case RTJSONTOKENCLASS_FALSE:
            pVal = rtJsonValueCreate(RTJSONVALTYPE_FALSE);
            break;
        case RTJSONTOKENCLASS_TRUE:
            pVal = rtJsonValueCreate(RTJSONVALTYPE_TRUE);
            break;
        case RTJSONTOKENCLASS_END_ARRAY:
        case RTJSONTOKENCLASS_END_OBJECT:
        case RTJSONTOKENCLASS_NAME_SEPARATOR:
        case RTJSONTOKENCLASS_VALUE_SEPARATOR:
        case RTJSONTOKENCLASS_EOS:
        default:
            /** @todo: Error info */
            rc = VERR_JSON_MALFORMED;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        if (pVal)
            *ppJsonVal = pVal;
        else
            rc = VERR_NO_MEMORY;
    }
    else if (pVal)
        rtJsonValDestroy(pVal);

    return rc;
}


RTDECL(int) RTJsonParseFromBuf(PRTJSONVAL phJsonVal, const uint8_t *pbBuf, size_t cbBuf,
                               PRTERRINFO pErrInfo)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int) RTJsonParseFromString(PRTJSONVAL phJsonVal, const char *pszStr, PRTERRINFO pErrInfo)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int) RTJsonParseFromFile(PRTJSONVAL phJsonVal, const char *pszFilename, PRTERRINFO pErrInfo)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(uint32_t) RTJsonValueRetain(RTJSONVAL hJsonVal)
{
    PRTJSONVALINT pThis = hJsonVal;
    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    return cRefs;
}

RTDECL(uint32_t) RTJsonValueRelease(RTJSONVAL hJsonVal)
{
    PRTJSONVALINT pThis = hJsonVal;
    if (pThis == NIL_RTJSONVAL)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 0)
        rtJsonValDestroy(pThis);
    return cRefs;
}

RTDECL(RTJSONVALTYPE) RTJsonValueGetType(RTJSONVAL hJsonVal)
{
    PRTJSONVALINT pThis = hJsonVal;
    if (pThis == NIL_RTJSONVAL)
        return RTJSONVALTYPE_INVALID;
    AssertPtrReturn(pThis, RTJSONVALTYPE_INVALID);

    return pThis->enmType;
}

RTDECL(const char *) RTJsonValueGetString(RTJSONVAL hJsonVal)
{
    PRTJSONVALINT pThis = hJsonVal;
    AssertReturn(pThis != NIL_RTJSONVAL, NULL);
    AssertReturn(pThis->enmType == RTJSONVALTYPE_STRING, NULL);

    return pThis->Type.String.pszStr;
}

RTDECL(int) RTJsonValueGetStringEx(RTJSONVAL hJsonVal, const char **ppszStr)
{
    PRTJSONVALINT pThis = hJsonVal;
    AssertPtrReturn(ppszStr, VERR_INVALID_POINTER);
    AssertReturn(pThis != NIL_RTJSONVAL, VERR_INVALID_HANDLE);
    AssertReturn(pThis->enmType == RTJSONVALTYPE_STRING, VERR_JSON_VALUE_INVALID_TYPE);

    *ppszStr = pThis->Type.String.pszStr;
    return VINF_SUCCESS;
}

RTDECL(int) RTJsonValueGetNumber(RTJSONVAL hJsonVal, int64_t *pi64Num)
{
    PRTJSONVALINT pThis = hJsonVal;
    AssertPtrReturn(pi64Num, VERR_INVALID_POINTER);
    AssertReturn(pThis != NIL_RTJSONVAL, VERR_INVALID_HANDLE);
    AssertReturn(pThis->enmType == RTJSONVALTYPE_NUMBER, VERR_JSON_VALUE_INVALID_TYPE);

    *pi64Num = pThis->Type.Number.i64Num;
    return VINF_SUCCESS;
}

RTDECL(int) RTJsonValueGetByName(RTJSONVAL hJsonVal, const char *pszName, PRTJSONVAL phJsonVal)
{
    PRTJSONVALINT pThis = hJsonVal;
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrReturn(phJsonVal, VERR_INVALID_POINTER);
    AssertReturn(pThis != NIL_RTJSONVAL, VERR_INVALID_HANDLE);
    AssertReturn(pThis->enmType == RTJSONVALTYPE_OBJECT, VERR_JSON_VALUE_INVALID_TYPE);

    int rc = VERR_NOT_FOUND;
    for (unsigned i = 0; i < pThis->Type.Object.cMembers; i++)
    {
        if (!RTStrCmp(pThis->Type.Object.papszNames[i], pszName))
        {
            RTJsonValueRetain(pThis->Type.Object.papValues[i]);
            *phJsonVal = pThis->Type.Object.papValues[i];
            rc = VINF_SUCCESS;
            break;
        }
    }

    return rc;
}

RTDECL(uint32_t) RTJsonValueGetArraySize(RTJSONVAL hJsonVal)
{
    PRTJSONVALINT pThis = hJsonVal;
    AssertReturn(pThis != NIL_RTJSONVAL, 0);
    AssertReturn(pThis->enmType == RTJSONVALTYPE_ARRAY, 0);

    return pThis->Type.Array.cItems;
}

RTDECL(int) RTJsonValueGetArraySizeEx(RTJSONVAL hJsonVal, uint32_t *pcItems)
{
    PRTJSONVALINT pThis = hJsonVal;
    AssertPtrReturn(pcItems, VERR_INVALID_POINTER);
    AssertReturn(pThis != NIL_RTJSONVAL, VERR_INVALID_HANDLE);
    AssertReturn(pThis->enmType == RTJSONVALTYPE_ARRAY, VERR_JSON_VALUE_INVALID_TYPE);

    *pcItems = pThis->Type.Array.cItems;
    return VINF_SUCCESS;
}

RTDECL(int) RTJsonValueGetByIndex(RTJSONVAL hJsonVal, uint32_t idx, PRTJSONVAL phJsonVal)
{
    PRTJSONVALINT pThis = hJsonVal;
    AssertPtrReturn(phJsonVal, VERR_INVALID_POINTER);
    AssertReturn(pThis != NIL_RTJSONVAL, VERR_INVALID_HANDLE);
    AssertReturn(pThis->enmType == RTJSONVALTYPE_ARRAY, VERR_JSON_VALUE_INVALID_TYPE);
    AssertReturn(idx < pThis->Type.Array.cItems, VERR_OUT_OF_RANGE);

    RTJsonValueRetain(pThis->Type.Array.papItems[idx]);
    *phJsonVal = pThis->Type.Array.papItems[idx];
    return VINF_SUCCESS;
}

RTDECL(int) RTJsonIteratorBegin(RTJSONVAL hJsonVal, PRTJSONIT phJsonIt)
{
    PRTJSONVALINT pThis = hJsonVal;
    AssertPtrReturn(phJsonIt, VERR_INVALID_POINTER);
    AssertReturn(pThis != NIL_RTJSONVAL, VERR_INVALID_HANDLE);
    AssertReturn(pThis->enmType == RTJSONVALTYPE_ARRAY || pThis->enmType == RTJSONVALTYPE_OBJECT,
                 VERR_JSON_VALUE_INVALID_TYPE);

    PRTJSONITINT pIt = (PRTJSONITINT)RTMemTmpAllocZ(sizeof(RTJSONITINT));
    if (RT_UNLIKELY(!pIt))
        return VERR_NO_MEMORY;

    RTJsonValueRetain(hJsonVal);
    pIt->pJsonVal = pThis;
    pIt->idxCur = 0;
    *phJsonIt = pIt;

    return VINF_SUCCESS;
}

RTDECL(int) RTJsonIteratorGetValue(RTJSONIT hJsonIt, PRTJSONVAL phJsonVal, const char **ppszName)
{
    PRTJSONITINT pIt = hJsonIt;
    AssertReturn(pIt != NIL_RTJSONIT, VERR_INVALID_HANDLE);
    AssertPtrReturn(phJsonVal, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    PRTJSONVALINT pThis = pIt->pJsonVal;
    if (pThis->enmType == RTJSONVALTYPE_ARRAY)
    {
        if (pIt->idxCur < pThis->Type.Array.cItems)
        {
            if (ppszName)
                *ppszName = NULL;

            RTJsonValueRetain(pThis->Type.Array.papItems[pIt->idxCur]);
            *phJsonVal = pThis->Type.Array.papItems[pIt->idxCur];
        }
        else
            rc = VERR_JSON_ITERATOR_END;
    }
    else
    {
        Assert(pThis->enmType == RTJSONVALTYPE_OBJECT);

        if (pIt->idxCur < pThis->Type.Object.cMembers)
        {
            if (ppszName)
                *ppszName = pThis->Type.Object.papszNames[pIt->idxCur];

            RTJsonValueRetain(pThis->Type.Object.papValues[pIt->idxCur]);
            *phJsonVal = pThis->Type.Object.papValues[pIt->idxCur];
        }
        else
            rc = VERR_JSON_ITERATOR_END;
    }

    return rc;
}

RTDECL(int) RTJsonIteratorNext(RTJSONIT hJsonIt)
{
    PRTJSONITINT pIt = hJsonIt;
    AssertReturn(pIt != NIL_RTJSONIT, VERR_INVALID_HANDLE);

    int rc = VINF_SUCCESS;
    PRTJSONVALINT pThis = pIt->pJsonVal;
    if (pThis->enmType == RTJSONVALTYPE_ARRAY)
    {
        if (pIt->idxCur < pThis->Type.Array.cItems)
            pIt->idxCur++;
        else
            rc = VERR_JSON_ITERATOR_END;
    }
    else
    {
        Assert(pThis->enmType == RTJSONVALTYPE_OBJECT);

        if (pIt->idxCur < pThis->Type.Object.cMembers)
            pIt->idxCur++;
        else
            rc = VERR_JSON_ITERATOR_END;
    }

    return rc;
}

RTDECL(void) RTJsonIteratorFree(RTJSONIT hJsonIt)
{
    PRTJSONITINT pThis = hJsonIt;
    if (pThis == NIL_RTJSONIT)
        return;
    AssertPtrReturnVoid(pThis);

    RTJsonValueRelease(pThis->pJsonVal);
    RTMemTmpFree(pThis);
}

