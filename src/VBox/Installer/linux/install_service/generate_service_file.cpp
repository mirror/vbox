/* $Id$ */
/** @file
 * Read a service file template from standard input and output a service file
 * to standard output generated from the template based on arguments passed to
 * the utility.  See the usage text for more information.
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

#include <VBox/version.h>

#include <iprt/ctype.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>

#ifndef READ_SIZE
/** How much of the input we read at a time.  Override to something small for
 *  testing. */
# define READ_SIZE _1M
#endif

/* Macros for the template substitution sequences to guard against mis-types. */
#define COMMAND "%COMMAND%"
#define ARGUMENTS "%ARGUMENTS%"
#define DESCRIPTION "%DESCRIPTION%"
#define SERVICE_NAME "%SERVICE_NAME%"

void showLogo(void)
{
    static bool s_fShown; /* show only once */

    RTPrintf(VBOX_PRODUCT " Service File Generator Version "
             VBOX_VERSION_STRING "\n"
             "(C) 2012" /* "-" VBOX_C_YEAR */ " " VBOX_VENDOR "\n"
             "All rights reserved.\n"
             "\n");
}

void showUsage(const char *pcszArgv0)
{
    const char *pcszName = strrchr(pcszArgv0, '/');
    if (!pcszName)
        pcszName = pcszArgv0;
    RTPrintf(
"Usage:\n"
"\n"
"  %s --help|-h|-?|--version|-V|<options>\n"
"\n"
"Read a service file template from standard input and output a service file to\n"
"standard output which was generated from the template based on parameters\n"
"passed on the utility's command line.  Generation is done by replacing well-\n"
"known text sequences in the template with strings based on the parameters.  The\n",
             pcszArgv0);
    RTPrintf(
"exact strings substituted will depend on the format of the template, for\n"
"example shell script or systemd unit file.  The sequence \"%%%%\" in the template\n"
"will be replaced by a single \"%%\" character.  The description of the options\n"
"also describes the sequences which will be replaced in the template.  All\n"
"arguments should be in Utf-8 encoding.\n"
"\n"
"\n");
    RTPrintf(
"  --help|-h|-?\n"
"      Print this help text and exit.\n"
"\n"
"  --version|-V\n"
"      Print version information and exit.\n"
"\n");
    RTPrintf(
"Required options:\n"
"\n"
"  --format <shell>\n"
"      The format of the template.  Currently only \"shell\" for shell script\n"
"      is supported.  This affects escaping of strings substituted.\n"
"\n"
"  --command\n"
"      The absolute path of the executable file to be started by the service.\n"
"      No form of quoting should be used here.  Substituted for the sequence\n"
"      \"%%COMMAND%%\" in the template.\n");
    RTPrintf(
"\n"
"  --arguments <arguments>\n"
"      The arguments to pass to the executable file when it is started.  ASCII\n"
"      characters 0 to 31, \"\'\" and 127 should be escaped in C string-style and\n"
"      spaces inside words should be preceeded by a back slash.  Some systemd-\n"
"      style \"%%\" sequences may be added at a future time.  Substituted for the\n"
"      sequence \"%%ARGUMENTS%%\" in the template.\n");
    RTPrintf(
"\n"
"  --description <description>\n"
"      A short description of the service which can also be used in sentences\n"
"      like \"<description> failed to start.\".  ASCII characters 0 to 31 and 127\n"
"      should not be used.  Substituted for the sequence \"%%DESCRIPTION%%\" in the\n"
"      template.\n"
"\n"
"Other options:\n"
"\n");
    RTPrintf(
"  --service-name <name>\n"
"      Specify the name of the service.  By default the base name without the\n"
"      extension of the command binary is used.  Only ASCII characters 33 to 126\n"
"      should be used.  Substituted for the sequence \"%%SERVICE_NAME%%\" in the\n"
"      template.\n"
"\n");
}

/** @name Template format.
 * @{
 */
enum ENMFORMAT
{
    /** No format selected. */
    FORMAT_NONE = 0,
    /** Shell script format. */
    FORMAT_SHELL
};
/** @} */

static bool errorIfSet(const char *pcszName, bool isSet);
static bool errorIfUnset(const char *pcszName, bool isSet);
static enum ENMFORMAT getFormat(const char *pcszName, const char *pcszValue);
static bool checkAbsoluteFilePath(const char *pcszName, const char *pcszValue);
static bool checkArguments(const char *pcszName, const char *pcszValue);
static bool checkPrintable(const char *pcszName, const char *pcszValue);
static bool checkGraphic(const char *pcszName, const char *pcszValue);
static bool createServiceFile(enum ENMFORMAT enmFormat,
                              const char *pcszCommand,
                              const char *pcszArguments,
                              const char *pcszDescription,
                              const char *pcszServiceName);

int main(int cArgs, char **apszArgs)
{
     int rc = RTR3InitExe(cArgs, &apszArgs, 0);
     if (RT_FAILURE(rc))
         return RTMsgInitFailure(rc);

     enum
     {
         OPTION_FORMAT = 1,
         OPTION_COMMAND,
         OPTION_ARGUMENTS,
         OPTION_DESCRIPTION,
         OPTION_SERVICE_NAME
     };

     static const RTGETOPTDEF s_aOptions[] =
     {
         { "--format",             OPTION_FORMAT,
           RTGETOPT_REQ_STRING },
         { "--command",            OPTION_COMMAND,
           RTGETOPT_REQ_STRING },
         { "--arguments",          OPTION_ARGUMENTS,
           RTGETOPT_REQ_STRING },
         { "--description",        OPTION_DESCRIPTION,
           RTGETOPT_REQ_STRING },
         { "--service-name",       OPTION_SERVICE_NAME,
           RTGETOPT_REQ_STRING }
     };

     int ch;
     enum ENMFORMAT enmFormat = FORMAT_NONE;
     const char *pcszCommand = NULL;
     const char *pcszArguments = NULL;
     const char *pcszDescription = NULL;
     const char *pcszServiceName = NULL;
     RTGETOPTUNION ValueUnion;
     RTGETOPTSTATE GetState;
     RTGetOptInit(&GetState, cArgs, apszArgs, s_aOptions,
                  RT_ELEMENTS(s_aOptions), 1, 0);
     while ((ch = RTGetOpt(&GetState, &ValueUnion)))
     {
         switch (ch)
         {
             case 'h':
                 showUsage(apszArgs[0]);
                 return RTEXITCODE_SUCCESS;
                 break;

             case 'V':
                 showLogo();
                 return RTEXITCODE_SUCCESS;
                 break;

             case OPTION_FORMAT:
                 if (errorIfSet("--format", enmFormat != FORMAT_NONE))
                     return(RTEXITCODE_SYNTAX);
                 enmFormat = getFormat("--format", ValueUnion.psz);
                 if (enmFormat == FORMAT_NONE)
                     return(RTEXITCODE_SYNTAX);
                 break;

             case OPTION_COMMAND:
                 if (errorIfSet("--command", pcszCommand))
                     return(RTEXITCODE_SYNTAX);
                 pcszCommand = ValueUnion.psz;
                 if (!checkAbsoluteFilePath("--command", pcszCommand))
                     return(RTEXITCODE_SYNTAX);
                 break;

             case OPTION_ARGUMENTS:
                 if (errorIfSet("--arguments", pcszArguments))
                     return(RTEXITCODE_SYNTAX);
                 pcszArguments = ValueUnion.psz;
                 if (!checkArguments("--arguments", pcszArguments))
                     return(RTEXITCODE_SYNTAX);
                 break;

             case OPTION_DESCRIPTION:
                 if (errorIfSet("--description", pcszDescription))
                     return(RTEXITCODE_SYNTAX);
                 pcszDescription = ValueUnion.psz;
                 if (!checkPrintable("--description", pcszDescription))
                     return(RTEXITCODE_SYNTAX);
                 break;

             case OPTION_SERVICE_NAME:
                 if (errorIfSet("--service-name", pcszServiceName))
                     return(RTEXITCODE_SYNTAX);
                 pcszServiceName = ValueUnion.psz;
                 if (!checkGraphic("--service-name", pcszServiceName))
                     return(RTEXITCODE_SYNTAX);
                 break;

             default:
                 return RTGetOptPrintError(ch, &ValueUnion);
         }
     }
     if (   errorIfUnset("--format", enmFormat != FORMAT_NONE)
         || errorIfUnset("--command", pcszCommand)
         || errorIfUnset("--description", pcszDescription))
         return(RTEXITCODE_SYNTAX);
     return   createServiceFile(enmFormat, pcszCommand, pcszArguments,
                                pcszDescription, pcszServiceName)
            ? RTEXITCODE_SUCCESS
            : RTEXITCODE_FAILURE;
}

/** Print an error and return true if an option is already set. */
bool errorIfSet(const char *pcszName, bool isSet)
{
    if (isSet)
        RTStrmPrintf(g_pStdErr, "%s may only be specified once.\n", pcszName);
    return isSet;
}

/** Print an error and return true if an option is not set. */
bool errorIfUnset(const char *pcszName, bool isSet)
{
    if (!isSet)
        RTStrmPrintf(g_pStdErr, "%s must be specified.\n", pcszName);
    return !isSet;
}

/** Match the string to a known format and return that (or "none" and print an
 * error). */
enum ENMFORMAT getFormat(const char *pcszName, const char *pcszValue)
{
    if (!strcmp(pcszValue, "shell"))
        return FORMAT_SHELL;
    RTStrmPrintf(g_pStdErr, "%s: unknown format %s.\n", pcszName, pcszValue);
    return FORMAT_NONE;
}

/** Check that the string is an absolute path to a file or print an error. */
bool checkAbsoluteFilePath(const char *pcszName, const char *pcszValue)
{
    if (RTPathFilename(pcszValue) && RTPathStartsWithRoot(pcszValue))
        return true;
    RTStrmPrintf(g_pStdErr, "%s: %s must be an absolute path of a file.\n", pcszName, pcszValue);
    return false;
}

/** Check that the string does not contain any non-printable characters and
 * that the quoting and escaping are balanced. */
bool checkArguments(const char *pcszName, const char *pcszValue)
{
    size_t cQuotes = 0;
    bool fEscaped = false;
    const char *pcch = pcszValue;
    for (; *pcch; ++pcch)
    {
        if (!RT_C_IS_PRINT(*pcch))
        {
            RTStrmPrintf(g_pStdErr, "%s: invalid character after \"%.*s\".\n",
                         pcszName, pcch - pcszValue, pcszValue);
            return false;
        }
        if (fEscaped)
            fEscaped = false;
        else
        {
            if (*pcch == '\\')
                fEscaped == true;
            if (*pcch == '\'')
                ++cQuotes;
        }
    }
    if (cQuotes % 2)
        RTStrmPrintf(g_pStdErr, "%s: quote (\') mismatch.\n", pcszName);
    else if (fEscaped)
        RTStrmPrintf(g_pStdErr, "%s: stray backslash at end of value.\n",
                     pcszName);
    else
        return true;
    return false;
}

/** Check that the string does not contain any non-printable characters. */
bool checkPrintable(const char *pcszName, const char *pcszValue)
{
    const char *pcch = pcszValue;
    for (; *pcch; ++pcch)
    {
        if (!RT_C_IS_PRINT(*pcch))
        {
            RTStrmPrintf(g_pStdErr, "%s: invalid character after \"%.*s\".\n",
                         pcszName, pcch - pcszValue, pcszValue);
            return false;
        }
    }
    return true;
}

/** Check that the string does not contain any non-graphic characters. */
static bool checkGraphic(const char *pcszName, const char *pcszValue)
{
    const char *pcch = pcszValue;
    for (; *pcch; ++pcch)
    {
        if (!RT_C_IS_GRAPH(*pcch))
        {
            RTStrmPrintf(g_pStdErr, "%s: invalid character after \"%.*s\".\n",
                         pcszName, pcch - pcszValue, pcszValue);
            return false;
        }
    }
    return true;
}

static bool createServiceFileCore(char **ppachTemplate,
                                  enum ENMFORMAT enmFormat,
                                  const char *pcszCommand,
                                  const char *pcszArguments,
                                  const char *pcszDescription,
                                  const char *pcszServiceName);

/**
 * Read standard input and write it to standard output, doing all substitutions
 * as per the usage documentation.
 * @note This is a wrapper around the actual function to simplify resource
 *       allocation without requiring a single point of exit.
 */
bool createServiceFile(enum ENMFORMAT enmFormat,
                       const char *pcszCommand,
                       const char *pcszArguments,
                       const char *pcszDescription,
                       const char *pcszServiceName)
{
    char *pachTemplate = NULL;
    bool rc = createServiceFileCore(&pachTemplate, enmFormat, pcszCommand,
                                    pcszArguments, pcszDescription,
                                    pcszServiceName);
    RTMemFree(pachTemplate);
    return rc;
}

static bool writeCommand(enum ENMFORMAT enmFormat, const char *pcszCommand);
static bool writeArguments(enum ENMFORMAT enmFormat, const char *pcszArguments);
static bool writePrintableString(enum ENMFORMAT enmFormat,
                                 const char *pcszString);

/** The actual implemenation code for @a createServiceFile. */
bool createServiceFileCore(char **ppachTemplate,
                           enum ENMFORMAT enmFormat,
                           const char *pcszCommand,
                           const char *pcszArguments,
                           const char *pcszDescription,
                           const char *pcszServiceName)
{
    /* The size of the template data we have read. */
    size_t cchTemplate = 0;
    /* The size of the buffer we have allocated. */
    size_t cbBuffer = 0;
    /* How much of the template data we have written out. */
    size_t cchWritten = 0;
    int rc = VINF_SUCCESS;
    /* First of all read in the file. */
    while (rc != VINF_EOF)
    {
        size_t cchRead;

        if (cchTemplate == cbBuffer)
        {
            cbBuffer += READ_SIZE;
            *ppachTemplate = (char *)RTMemRealloc((void *)*ppachTemplate,
                                                  cbBuffer);
        }
        if (!*ppachTemplate)
        {
            RTStrmPrintf(g_pStdErr, "Out of memory.\n");
            return false;
        }
        rc = RTStrmReadEx(g_pStdIn, *ppachTemplate + cchTemplate,
                          cbBuffer - cchTemplate, &cchRead);
        if (RT_FAILURE(rc))
        {
            RTStrmPrintf(g_pStdErr, "Error reading input: %Rrc\n", rc);
            return false;
        }
        if (!cchRead)
            rc = VINF_EOF;
        cchTemplate += cchRead;
    }
    while (true)
    {
        /* Find the next '%' character if any and write out up to there (or the
         * end if there is no '%'). */
        char *pchNext = (char *) memchr((void *)(*ppachTemplate + cchWritten),
                                        '%', cchTemplate - cchWritten);
        size_t cchToWrite =   pchNext
                            ? pchNext - *ppachTemplate - cchWritten
                            : cchTemplate - cchWritten;
        rc = RTStrmWrite(g_pStdOut, *ppachTemplate + cchWritten, cchToWrite);
        if (RT_FAILURE(rc))
        {
            RTStrmPrintf(g_pStdErr, "Error writing output: %Rrc\n", rc);
            return false;
        }
        cchWritten += cchToWrite;
        if (!pchNext)
            break;
        /* And substitute any of our well-known strings.  We favour code
         * readability over efficiency here. */
        if (   cchTemplate - cchWritten >= sizeof(COMMAND) - 1
            && !RTStrNCmp(*ppachTemplate + cchWritten, COMMAND,
                          sizeof(COMMAND) - 1))
        {
            if (!writeCommand(enmFormat, pcszCommand))
                return false;
            cchWritten += sizeof(COMMAND) - 1;
        }
        else if (   cchTemplate - cchWritten >= sizeof(ARGUMENTS) - 1
                 && !RTStrNCmp(*ppachTemplate + cchWritten, ARGUMENTS,
                               sizeof(ARGUMENTS) - 1))
        {
            if (pcszArguments && !writeArguments(enmFormat, pcszArguments))
                return false;
            cchWritten += sizeof(ARGUMENTS) - 1;
        }
        else if (   cchTemplate - cchWritten >= sizeof(DESCRIPTION) - 1
                 && !RTStrNCmp(*ppachTemplate + cchWritten, DESCRIPTION,
                               sizeof(DESCRIPTION) - 1))
        {
            if (!writePrintableString(enmFormat, pcszDescription))
                return false;
            cchWritten += sizeof(DESCRIPTION) - 1;
        }
        else if (   cchTemplate - cchWritten >= sizeof(SERVICE_NAME) - 1
                 && !RTStrNCmp(*ppachTemplate + cchWritten, SERVICE_NAME,
                               sizeof(SERVICE_NAME) - 1))
        {
            if (pcszServiceName)
            {
                if (!writePrintableString(enmFormat, pcszServiceName))
                    return false;
            }
            else
            {
                const char *pcszFileName = RTPathFilename(pcszCommand);
                const char *pcszExtension = RTPathExt(pcszCommand);
                char *pszName = RTStrDupN(pcszFileName,
                                            pcszExtension
                                          ? pcszExtension - pcszFileName
                                          : RTPATH_MAX);
                bool fRc;
                if (!pszName)
                {
                    RTStrmPrintf(g_pStdErr, "Out of memory.\n");
                    return false;
                }
                fRc = writePrintableString(enmFormat, pszName);
                RTStrFree(pszName);
                if (!fRc)
                    return false;
            }
            cchWritten += sizeof(SERVICE_NAME) - 1;
        }
        else if (   cchTemplate - cchWritten > 1
                 && *(*ppachTemplate + cchWritten + 1) == '%')
        {
            rc = RTStrmPutCh(g_pStdOut, '%');
            if (RT_FAILURE(rc))
            {
                RTStrmPrintf(g_pStdErr, "Error writing output: %Rrc\n", rc);
                return false;
            }
            cchWritten += 2;
        }
        else
        {
            RTStrmPrintf(g_pStdErr, "Unknown substitution sequence in input at \"%.*s\"\n",
                         RT_MIN(16, cchTemplate - cchWritten),
                         *ppachTemplate + cchWritten);
            return false;
        }
   }
    return true;
}

/** Write a character to standard output and print an error and return false on
 * failure. */
bool outputCharacter(char ch)
{
    int rc = RTStrmWrite(g_pStdOut, &ch, 1);
    if (RT_FAILURE(rc))
    {
        RTStrmPrintf(g_pStdErr, "Error writing output: %Rrc\n", rc);
        return false;
    }
    return true;
}

/** Write a string to standard output and print an error and return false on
 * failure. */
bool outputString(const char *pcsz)
{
    int rc = RTStrmPutStr(g_pStdOut, pcsz);
    if (RT_FAILURE(rc))
    {
        RTStrmPrintf(g_pStdErr, "Error writing output: %Rrc\n", rc);
        return false;
    }
    return true;
}

/** Write a character to standard output, adding any escaping needed for the
 * format being written. */
static bool escapeAndOutputCharacter(enum ENMFORMAT enmFormat, char ch)
{
    if (enmFormat == FORMAT_SHELL)
    {
        if (ch == '\'')
            return outputString("\'\\\'\'");
        return outputCharacter(ch);
    }
    RTStrmPrintf(g_pStdErr, "Error: unknown template format.\n");
    return false;
}

/** Write a character to standard output, adding any escaping needed for the
 * format being written. */
static bool outputArgumentSeparator(enum ENMFORMAT enmFormat)
{
    if (enmFormat == FORMAT_SHELL)
        return outputString("\' \'");
    RTStrmPrintf(g_pStdErr, "Error: unknown template format.\n");
    return false;
}

bool writeCommand(enum ENMFORMAT enmFormat, const char *pcszCommand)
{
    if (enmFormat == FORMAT_SHELL)
        if (!outputCharacter('\''))
            return false;
    for (; *pcszCommand; ++pcszCommand)
        if (enmFormat == FORMAT_SHELL)
        {
            if (*pcszCommand == '\'')
            {
                if (!outputString("\'\\\'\'"))
                    return false;
            }
            else if (!outputCharacter(*pcszCommand))
                return false;
        }
    if (enmFormat == FORMAT_SHELL)
        if (!outputCharacter('\''))
            return false;
    return true;
}

const char aachEscapes[][2] =
{
    { 'a', '\a' }, { 'b', '\b' }, { 'f', '\f' }, { 'n', '\n' }, { 'r', '\r' },
    { 't', '\t' }, { 'v', '\v' }, { '\"', '\"' }, { '\'', '\'' },
    { '\\', '\\' }, { ' ', ' ' }, { 0, 0 }
};

bool writeArguments(enum ENMFORMAT enmFormat, const char *pcszArguments)
{
    /* Was the last character seen a back slash? */
    bool fEscaped = false;
    /* Was the last character seen an argument separator (an unescaped space)?
     */
    bool fNextArgument = false;

    if (enmFormat == FORMAT_SHELL)
        if (!outputCharacter('\''))
            return false;
    for (; *pcszArguments; ++pcszArguments)
    {
        if (fEscaped)
        {
            bool fRc = true;
            const char (*pachEscapes)[2];
            fEscaped = false;
            /* One-letter escapes. */
            for (pachEscapes = aachEscapes; (*pachEscapes)[0]; ++pachEscapes)
                if (*pcszArguments == (*pachEscapes)[0])
                {
                    if (!escapeAndOutputCharacter(enmFormat, (*pachEscapes)[1]))
                        return false;
                    break;
                }
            if ((*pachEscapes)[0])
                continue;
            /* Octal. */
            if (*pcszArguments >= '0' && *pcszArguments <= '7')
            {
                uint8_t cNum;
                char *pchNext;
                char achDigits[4];
                int rc;
                RTStrCopy(achDigits, sizeof(achDigits), pcszArguments);
                rc = RTStrToUInt8Ex(achDigits, &pchNext, 8, &cNum);
                if (rc == VWRN_NUMBER_TOO_BIG)
                {
                    RTStrmPrintf(g_pStdErr, "Invalid octal sequence at \"%.16s\"\n",
                                 pcszArguments - 1);
                    return false;
                }
                if (!escapeAndOutputCharacter(enmFormat, cNum))
                    return false;
                pcszArguments += pchNext - achDigits - 1;
                continue;
            }
            /* Hexadecimal. */
            if (*pcszArguments == 'x')
            {
                uint8_t cNum;
                char *pchNext;
                char achDigits[3];
                int rc;
                RTStrCopy(achDigits, sizeof(achDigits), pcszArguments + 1);
                rc = RTStrToUInt8Ex(achDigits, &pchNext, 16, &cNum);
                if (   rc == VWRN_NUMBER_TOO_BIG
                    || rc == VWRN_NEGATIVE_UNSIGNED
                    || RT_FAILURE(rc))
                {
                    RTStrmPrintf(g_pStdErr, "Invalid hexadecimal sequence at \"%.16s\"\n",
                                 pcszArguments - 1);
                    return false;
                }
                if (!escapeAndOutputCharacter(enmFormat, cNum))
                    return false;
                pcszArguments += pchNext - achDigits;
                continue;
            }
            /* Reject anything else. */
            RTStrmPrintf(g_pStdErr, "Invalid escape sequence or trailing back slash in argument.\n");
            return false;
        }
        /* Argument separator. */
        if (*pcszArguments == ' ')
        {
            if (!fNextArgument && !outputArgumentSeparator(enmFormat))
                return false;
            fNextArgument = true;
            continue;
        }
        else
            fNextArgument = false;
        /* Start of escape sequence. */
        if (*pcszArguments == '\\')
        {
            fEscaped = true;
            continue;
        }
        /* Anything else. */
        if (!outputCharacter(*pcszArguments))
            return false;
    }
    if (enmFormat == FORMAT_SHELL)
        if (!outputCharacter('\''))
            return false;
    return true;
}

bool writePrintableString(enum ENMFORMAT enmFormat, const char *pcszString)
{
    if (enmFormat == FORMAT_SHELL)
        return outputString(pcszString);
    RTStrmPrintf(g_pStdErr, "Error: unknown template format.\n");
    return false;
}
