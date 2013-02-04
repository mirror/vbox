/* $Id$ */
/** @file
 * Read a service file template from standard input and output a service file
 * to standard output generated from the template based on arguments passed to
 * the utility.  See the usage text for more information.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/**
 * Description of the generation process.
 *
 * A template for the service file to be generated is fed into standard input
 * and the service file is sent to standard output.  The following
 * substitutions are performed based on the command line parameters supplied,
 * with all quoting appropriate to the format of the template as specified on
 * the command line.
 *
 * %COMMAND%          -> path to the service binary or script.
 * %ARGUMENTS%        -> the arguments to pass to the binary when starting the
 *                       service.
 * %SERVICE_NAME%     -> the name of the service.
 * %DESCRIPTION%      -> the short description of the service.
 * %STOP_COMMAND%     -> path to the command used to stop the service.
 * %STOP_ARGUMENTS%   -> the arguments for the stop command
 * %STATUS_COMMAND%   -> path to the command used to determine the service
 *                       status.
 * %STATUS_ARGUMENTS% -> the arguments for the status command

 * %NO_STOP_COMMAND%     -> if no stop command was specified, this and all text
 *                          following it on the line (including the end-of-
 *                          line) will be removed, otherwise only the marker
 *                          will be removed.
 * %HAVE_STOP_COMMAND%   -> like above, but text on the line will be removed
 *                          if a stop command was supplied.
 * %NO_STATUS_COMMAND%   -> Analogue to %NO_STOP_COMMAND% for the status
 *                          command.
 * %HAVE_STATUS_COMMAND% -> Analogue to %HAVE_STOP_COMMAND% for the status
 *                          command.
 * %HAVE_ONESHOT%        -> like above, text on the line will be removed unless
 *                          --one-shot was specified on the command line.
 * %HAVE_DAEMON%         -> the same if --one-shot was not specified.
 *
 * %% will be replaced with a single %.
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
#define HAVE_ONESHOT "%HAVE_ONESHOT%"
#define HAVE_DAEMON "%HAVE_DAEMON%"
#define STOP_COMMAND "%STOP_COMMAND%"
#define STOP_ARGUMENTS "%STOP_ARGUMENTS%"
#define HAVE_STOP_COMMAND "%HAVE_STOP_COMMAND%"
#define NO_STOP_COMMAND "%NO_STOP_COMMAND%"
#define STATUS_COMMAND "%STATUS_COMMAND%"
#define STATUS_ARGUMENTS "%STATUS_ARGUMENTS%"
#define HAVE_STATUS_COMMAND "%HAVE_STATUS_COMMAND%"
#define NO_STATUS_COMMAND "%NO_STATUS_COMMAND%"

void showLogo(void)
{
    static bool s_fShown; /* show only once */

    RTPrintf(VBOX_PRODUCT " Service File Generator Version "
             VBOX_VERSION_STRING "\n"
             "(C) 2012" /* "-" VBOX_C_YEAR */ " " VBOX_VENDOR "\n"
             "All rights reserved.\n"
             "\n");
}

static void showOptions(void);

void showUsage(const char *pcszArgv0)
{
    const char *pcszName = strrchr(pcszArgv0, '/');
    if (!pcszName)
        pcszName = pcszArgv0;
    RTPrintf(
"Usage:\n"
"\n"
"  %s --help|-h|-?|--version|-V|--format <format> <parameters...>\n\n",
             pcszArgv0);
    RTPrintf(
"Read a service file template from standard input and output a service file to\n"
"standard output which was generated from the template based on parameters\n"
"passed on the utility's command line.  Generation is done by replacing well-\n"
"known text sequences in the template with strings based on the parameters.\n"
"All strings should be in UTF-8 format.  Processing will stop if a sequence is\n"
"read which cannot be replace based on the parameters supplied.\n\n");

    RTPrintf(
"  --help|-h|-?\n"
"      Print this help text and exit.\n\n"
"  --version|-V\n"
"      Print version information and exit.\n\n"
"  --format <shell>\n"
"      The format of the template.  Currently only \"shell\" for shell script\n"
"      is supported.  This affects escaping of strings substituted.\n\n");
    RTPrintf(
"Parameters:\n"
"\n");
    RTPrintf(
"  --command <command>\n"
"      The absolute path of the executable file to be started by the service.\n"
"      No form of quoting should be used here.\n\n");
    RTPrintf(
"  --description <description>\n"
"      A short description of the service which can also be used in sentences\n"
"      like \"<description> failed to start.\", as a single parameter.  Characters\n"
"      0 to 31 and 127 should not be used.\n\n"
    );
    RTPrintf(
"  --arguments <arguments>\n"
"      The arguments to pass to the executable file when it is started, as a\n"
"      single parameter.  Characters \" \", \"\\\" and \"%%\" must be escaped with\n"
"      back-slashes and C string-style back-slash escapes are recognised.  Some\n"
"      systemd-style \"%%\" sequences may be added at a future time.\n\n");
    RTPrintf(
"  --service-name <name>\n"
"      Specify the name of the service.  By default the base name without the\n"
"      extension of the command binary is used.  Only ASCII characters 33 to 126\n"
"      should be used.\n\n");
    RTPrintf(
"  --one-shot\n"
"      The service command is expected to do some work and exit immediately with"
"      a status indicating success or failure.\n\n"
    );
    RTPrintf(
"  --stop-command <command>\n"
"      The command which should be used to stop the service before sending the\n"
"      termination signal to the main process.  No form of quoting should be\n"
"      used here.\n\n"
    );
    RTPrintf(
"  --stop-arguments <arguments>\n"
"      Arguments for the stop command.  This may only be used in combination\n"
"      with \"--stop-command\".  Quoting is the same as for \"--arguments\".\n\n"
    );
    RTPrintf(
"  --status-command <command>\n"
"      The command which should be used to determine the status of the service.\n"
"      This may not be respected by all service management systems.  The command\n"
"      should return an LSB status code.  No form of quoting should be used.\n\n"
    );
    RTPrintf(
"  --stop-arguments <arguments>\n"
"      Arguments for the status command.  This may only be used in combination\n"
"      with \"--status-command\".  Quoting is the same as for \"--arguments\".\n\n"
    );
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

struct SERVICEPARAMETERS
{
    enum ENMFORMAT enmFormat;
    const char *pcszCommand;
    const char *pcszArguments;
    const char *pcszDescription;
    const char *pcszServiceName;
    bool fOneShot;
    const char *pcszStopCommand;
    const char *pcszStopArguments;
    const char *pcszStatusCommand;
    const char *pcszStatusArguments;
};

static bool errorIfSet(const char *pcszName, bool isSet);
static enum ENMFORMAT getFormat(const char *pcszName, const char *pcszValue);
static bool checkAbsoluteFilePath(const char *pcszName, const char *pcszValue);
static bool checkPrintable(const char *pcszName, const char *pcszValue);
static bool checkGraphic(const char *pcszName, const char *pcszValue);
static bool createServiceFile(struct SERVICEPARAMETERS *pParameters);

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
         OPTION_SERVICE_NAME,
         OPTION_ONE_SHOT,
         OPTION_STOP_COMMAND,
         OPTION_STOP_ARGUMENTS,
         OPTION_STATUS_COMMAND,
         OPTION_STATUS_ARGUMENTS
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
           RTGETOPT_REQ_STRING },
         { "--one-shot",           OPTION_ONE_SHOT,
           RTGETOPT_REQ_NOTHING },
         { "--stop-command",       OPTION_STOP_COMMAND,
           RTGETOPT_REQ_STRING },
         { "--stop-arguments",     OPTION_STOP_ARGUMENTS,
           RTGETOPT_REQ_STRING },
         { "--status-command",     OPTION_STATUS_COMMAND,
           RTGETOPT_REQ_STRING },
         { "--status-arguments",   OPTION_STATUS_ARGUMENTS,
           RTGETOPT_REQ_STRING }
     };

     int ch;
     struct SERVICEPARAMETERS Parameters = { FORMAT_NONE };
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
                 if (errorIfSet("--format",
                                Parameters.enmFormat != FORMAT_NONE))
                     return(RTEXITCODE_SYNTAX);
                 Parameters.enmFormat
                     = getFormat("--format", ValueUnion.psz);
                 if (Parameters.enmFormat == FORMAT_NONE)
                     return(RTEXITCODE_SYNTAX);
                 break;

             case OPTION_COMMAND:
                 if (errorIfSet("--command", Parameters.pcszCommand))
                     return(RTEXITCODE_SYNTAX);
                 Parameters.pcszCommand = ValueUnion.psz;
                 if (!checkAbsoluteFilePath("--command",
                                            Parameters.pcszCommand))
                     return(RTEXITCODE_SYNTAX);
                 break;

             case OPTION_ARGUMENTS:
                 if (errorIfSet("--arguments",
                                Parameters.pcszArguments))
                     return(RTEXITCODE_SYNTAX);
                 /* Quoting will be checked while writing out the string. */
                 Parameters.pcszArguments = ValueUnion.psz;
                 break;

             case OPTION_DESCRIPTION:
                 if (errorIfSet("--description",
                                Parameters.pcszDescription))
                     return(RTEXITCODE_SYNTAX);
                 Parameters.pcszDescription = ValueUnion.psz;
                 if (!checkPrintable("--description",
                                     Parameters.pcszDescription))
                     return(RTEXITCODE_SYNTAX);
                 break;

             case OPTION_SERVICE_NAME:
                 if (errorIfSet("--service-name",
                                Parameters.pcszServiceName))
                     return(RTEXITCODE_SYNTAX);
                 Parameters.pcszServiceName = ValueUnion.psz;
                 if (!checkGraphic("--service-name",
                                   Parameters.pcszServiceName))
                     return(RTEXITCODE_SYNTAX);
                 break;

             case OPTION_ONE_SHOT:
                 Parameters.fOneShot = true;
                 break;

             case OPTION_STOP_COMMAND:
                 if (errorIfSet("--stop-command",
                                Parameters.pcszStopCommand))
                     return(RTEXITCODE_SYNTAX);
                 Parameters.pcszStopCommand = ValueUnion.psz;
                 if (!checkAbsoluteFilePath("--stop-command",
                         Parameters.pcszStopCommand))
                     return(RTEXITCODE_SYNTAX);
                 break;

             case OPTION_STOP_ARGUMENTS:
                 if (errorIfSet("--stop-arguments",
                                Parameters.pcszStopArguments))
                     return(RTEXITCODE_SYNTAX);
                 /* Quoting will be checked while writing out the string. */
                 Parameters.pcszStopArguments = ValueUnion.psz;
                 break;

             case OPTION_STATUS_COMMAND:
                 if (errorIfSet("--status-command",
                                Parameters.pcszStatusCommand))
                     return(RTEXITCODE_SYNTAX);
                 Parameters.pcszStatusCommand = ValueUnion.psz;
                 if (!checkAbsoluteFilePath("--status-command",
                         Parameters.pcszStatusCommand))
                     return(RTEXITCODE_SYNTAX);
                 break;

             case OPTION_STATUS_ARGUMENTS:
                 if (errorIfSet("--status-arguments",
                                Parameters.pcszStatusArguments))
                     return(RTEXITCODE_SYNTAX);
                 /* Quoting will be checked while writing out the string. */
                 Parameters.pcszStatusArguments = ValueUnion.psz;
                 break;

             default:
                 return RTGetOptPrintError(ch, &ValueUnion);
         }
     }
     if (Parameters.enmFormat == FORMAT_NONE)
     {
         RTStrmPrintf(g_pStdErr, "--format must be specified.\n");
         return(RTEXITCODE_SYNTAX);
     }
     if (Parameters.pcszArguments && !Parameters.pcszCommand)
     {
         RTStrmPrintf(g_pStdErr, "--arguments requires --command to be specified.\n");
         return(RTEXITCODE_SYNTAX);
     }
     if (Parameters.pcszStopArguments && !Parameters.pcszStopCommand)
     {
         RTStrmPrintf(g_pStdErr, "--stop-arguments requires --stop-command to be specified.\n");
         return(RTEXITCODE_SYNTAX);
     }
     if (Parameters.pcszStatusArguments && !Parameters.pcszStatusCommand)
     {
         RTStrmPrintf(g_pStdErr, "--status-arguments requires --status-command to be specified.\n");
         return(RTEXITCODE_SYNTAX);
     }
     return createServiceFile(&Parameters)
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
                                  struct SERVICEPARAMETERS
                                      *pParamters);

/**
 * Read standard input and write it to standard output, doing all substitutions
 * as per the usage documentation.
 * @note This is a wrapper around the actual function to simplify resource
 *       allocation without requiring a single point of exit.
 */
bool createServiceFile(struct SERVICEPARAMETERS *pParameters)
{
    char *pachTemplate = NULL;
    bool rc = createServiceFileCore(&pachTemplate, pParameters);
    RTMemFree(pachTemplate);
    return rc;
}

static bool getSequence(const char *pach, size_t cch, size_t *pcchRead,
                        const char *pcszSequence, size_t cchSequence);
static bool writeCommand(enum ENMFORMAT enmFormat, const char *pcszCommand);
static bool writeQuoted(enum ENMFORMAT enmFormat, const char *pcszQuoted);
static bool writePrintableString(enum ENMFORMAT enmFormat,
                                 const char *pcszString);
static void skipLine(const char *pach, size_t cch, size_t *pcchRead);

/** The actual implemenation code for @a createServiceFile. */
bool createServiceFileCore(char **ppachTemplate,
                           struct SERVICEPARAMETERS *pParameters)
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
        if (getSequence(*ppachTemplate, cchTemplate, &cchWritten,
                        COMMAND, sizeof(COMMAND) - 1))
        {
            if (!pParameters->pcszCommand)
            {
                RTStrmPrintf(g_pStdErr, "--command not specified.\n");
                return false;
            }
            if (!writeCommand(pParameters->enmFormat,
                              pParameters->pcszCommand))
                return false;
        }
        else if (getSequence(*ppachTemplate, cchTemplate, &cchWritten,
                             ARGUMENTS, sizeof(ARGUMENTS) - 1))
        {
            if (   pParameters->pcszArguments
                && !writeQuoted(pParameters->enmFormat,
                                pParameters->pcszArguments))
                return false;
        }
        else if (getSequence(*ppachTemplate, cchTemplate, &cchWritten,
                             DESCRIPTION, sizeof(DESCRIPTION) - 1))
        {
            if (!pParameters->pcszDescription)
            {
                RTStrmPrintf(g_pStdErr, "--description not specified.\n");
                return false;
            }
            if (!writePrintableString(pParameters->enmFormat,
                                      pParameters->pcszDescription))
                return false;
        }
        else if (getSequence(*ppachTemplate, cchTemplate, &cchWritten,
                             SERVICE_NAME, sizeof(SERVICE_NAME) - 1))
        {
            if (   !pParameters->pcszCommand
                && !pParameters->pcszServiceName)
            {
                RTStrmPrintf(g_pStdErr, "Neither --command nor --service-name specified.\n");
                return false;
            }
            if (pParameters->pcszServiceName)
            {
                if (!writePrintableString(pParameters->enmFormat,
                                          pParameters->pcszServiceName))
                    return false;
            }
            else
            {
                const char *pcszFileName =
                    RTPathFilename(pParameters->pcszCommand);
                const char *pcszExtension =
                    RTPathExt(pParameters->pcszCommand);
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
                fRc = writePrintableString(pParameters->enmFormat,
                                           pszName);
                RTStrFree(pszName);
                if (!fRc)
                    return false;
            }
        }
        else if (getSequence(*ppachTemplate, cchTemplate, &cchWritten,
                             HAVE_ONESHOT, sizeof(HAVE_ONESHOT) - 1))
        {
            if (!pParameters->fOneShot)
                skipLine(*ppachTemplate, cchTemplate, &cchWritten);
        }
        else if (getSequence(*ppachTemplate, cchTemplate, &cchWritten,
                             HAVE_DAEMON, sizeof(HAVE_DAEMON) - 1))
        {
            if (pParameters->fOneShot)
                skipLine(*ppachTemplate, cchTemplate, &cchWritten);
        }
        else if (getSequence(*ppachTemplate, cchTemplate, &cchWritten,
                             STOP_COMMAND, sizeof(STOP_COMMAND) - 1))
        {
            if (   pParameters->pcszStopCommand
                && !writeCommand(pParameters->enmFormat,
                                 pParameters->pcszStopCommand))
                return false;
        }
        else if (getSequence(*ppachTemplate, cchTemplate, &cchWritten,
                             STOP_ARGUMENTS, sizeof(STOP_ARGUMENTS) - 1))
        {
            if (   pParameters->pcszStopArguments
                && !writeQuoted(pParameters->enmFormat,
                                pParameters->pcszStopArguments))
                return false;
        }
        else if (getSequence(*ppachTemplate, cchTemplate, &cchWritten,
                             HAVE_STOP_COMMAND, sizeof(HAVE_STOP_COMMAND) - 1))
        {
            if (!pParameters->pcszStopCommand)
                skipLine(*ppachTemplate, cchTemplate, &cchWritten);
        }
        else if (getSequence(*ppachTemplate, cchTemplate, &cchWritten,
                             NO_STOP_COMMAND, sizeof(NO_STOP_COMMAND) - 1))
        {
            if (pParameters->pcszStopCommand)
                skipLine(*ppachTemplate, cchTemplate, &cchWritten);
        }
        else if (getSequence(*ppachTemplate, cchTemplate, &cchWritten,
                             STATUS_COMMAND, sizeof(STATUS_COMMAND) - 1))
        {
            if (   pParameters->pcszStatusCommand
                && !writeCommand(pParameters->enmFormat,
                                 pParameters->pcszStatusCommand))
                return false;
        }
        else if (getSequence(*ppachTemplate, cchTemplate, &cchWritten,
                             STATUS_ARGUMENTS, sizeof(STATUS_ARGUMENTS) - 1))
        {
            if (   pParameters->pcszStatusArguments
                && !writeQuoted(pParameters->enmFormat,
                                pParameters->pcszStatusArguments))
                return false;
        }
        else if (getSequence(*ppachTemplate, cchTemplate, &cchWritten,
                             HAVE_STATUS_COMMAND,
                             sizeof(HAVE_STATUS_COMMAND) - 1))
        {
            if (!pParameters->pcszStatusCommand)
                skipLine(*ppachTemplate, cchTemplate, &cchWritten);
        }
        else if (getSequence(*ppachTemplate, cchTemplate, &cchWritten,
                             NO_STATUS_COMMAND, sizeof(NO_STATUS_COMMAND) - 1))
        {
            if (pParameters->pcszStatusCommand)
                skipLine(*ppachTemplate, cchTemplate, &cchWritten);
        }
        else if (getSequence(*ppachTemplate, cchTemplate, &cchWritten,
                             "%%", 2))
        {
            rc = RTStrmPutCh(g_pStdOut, '%');
            if (RT_FAILURE(rc))
            {
                RTStrmPrintf(g_pStdErr, "Error writing output: %Rrc\n", rc);
                return false;
            }
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

bool getSequence(const char *pach, size_t cch, size_t *pcchRead,
                 const char *pcszSequence, size_t cchSequence)
{
    if (   cch - *pcchRead >= cchSequence
        && !RTStrNCmp(pach + *pcchRead, pcszSequence, cchSequence))
    {
        *pcchRead += cchSequence;
        return true;
    }
    return false;
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
    { 't', '\t' }, { 'v', '\v' }, { 0, 0 }
};

bool writeQuoted(enum ENMFORMAT enmFormat, const char *pcszQuoted)
{
    /* Was the last character seen a back slash? */
    bool fEscaped = false;
    /* Was the last character seen an argument separator (an unescaped space)?
     */
    bool fNextArgument = false;

    if (enmFormat == FORMAT_SHELL)
        if (!outputCharacter('\''))
            return false;
    for (; *pcszQuoted; ++pcszQuoted)
    {
        if (fEscaped)
        {
            bool fRc = true;
            const char (*pachEscapes)[2];
            fEscaped = false;
            /* One-letter escapes. */
            for (pachEscapes = aachEscapes; (*pachEscapes)[0]; ++pachEscapes)
                if (*pcszQuoted == (*pachEscapes)[0])
                {
                    if (!escapeAndOutputCharacter(enmFormat, (*pachEscapes)[1]))
                        return false;
                    break;
                }
            if ((*pachEscapes)[0])
                continue;
            /* Octal. */
            if (*pcszQuoted >= '0' && *pcszQuoted <= '7')
            {
                uint8_t cNum;
                char *pchNext;
                char achDigits[4];
                int rc;
                RTStrCopy(achDigits, sizeof(achDigits), pcszQuoted);
                rc = RTStrToUInt8Ex(achDigits, &pchNext, 8, &cNum);
                if (rc == VWRN_NUMBER_TOO_BIG)
                {
                    RTStrmPrintf(g_pStdErr, "Invalid octal sequence at \"%.16s\"\n",
                                 pcszQuoted - 1);
                    return false;
                }
                if (!escapeAndOutputCharacter(enmFormat, cNum))
                    return false;
                pcszQuoted += pchNext - achDigits - 1;
                continue;
            }
            /* Hexadecimal. */
            if (*pcszQuoted == 'x')
            {
                uint8_t cNum;
                char *pchNext;
                char achDigits[3];
                int rc;
                RTStrCopy(achDigits, sizeof(achDigits), pcszQuoted + 1);
                rc = RTStrToUInt8Ex(achDigits, &pchNext, 16, &cNum);
                if (   rc == VWRN_NUMBER_TOO_BIG
                    || rc == VWRN_NEGATIVE_UNSIGNED
                    || RT_FAILURE(rc))
                {
                    RTStrmPrintf(g_pStdErr, "Invalid hexadecimal sequence at \"%.16s\"\n",
                                 pcszQuoted - 1);
                    return false;
                }
                if (!escapeAndOutputCharacter(enmFormat, cNum))
                    return false;
                pcszQuoted += pchNext - achDigits;
                continue;
            }
            /* Output anything else non-zero as is. */
            if (*pcszQuoted)
            {
                if (!escapeAndOutputCharacter(enmFormat, *pcszQuoted))
                    return false;
                continue;
            }
            RTStrmPrintf(g_pStdErr, "Trailing back slash in argument.\n");
            return false;
        }
        /* Argument separator. */
        if (*pcszQuoted == ' ')
        {
            if (!fNextArgument && !outputArgumentSeparator(enmFormat))
                return false;
            fNextArgument = true;
            continue;
        }
        else
            fNextArgument = false;
        /* Start of escape sequence. */
        if (*pcszQuoted == '\\')
        {
            fEscaped = true;
            continue;
        }
        /* Anything else. */
        if (!outputCharacter(*pcszQuoted))
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

void skipLine(const char *pach, size_t cch, size_t *pcchRead)
{
    while (   *pcchRead < cch
           && (pach)[*pcchRead] != '\n'
           && (pach)[*pcchRead] != '\r')
        ++*pcchRead;
    while (   *pcchRead < cch
           && (   (pach)[*pcchRead] == '\n'
               || (pach)[*pcchRead] == '\r'))
        ++*pcchRead;
}
