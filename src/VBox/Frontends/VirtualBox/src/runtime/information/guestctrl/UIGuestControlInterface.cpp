/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlInterface class implementation.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include "UIErrorString.h"
# include "UIGuestControlInterface.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CGuestProcess.h"
# include "CGuestSession.h"

#include <iprt/getopt.h>
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


#define GCTLCMD_COMMON_OPT_USER             999 /**< The --username option number. */
#define GCTLCMD_COMMON_OPT_PASSWORD         998 /**< The --password option number. */
#define GCTLCMD_COMMON_OPT_PASSWORD_FILE    997 /**< The --password-file option number. */
#define GCTLCMD_COMMON_OPT_DOMAIN           996 /**< The --domain option number. */
#define GCTLCMD_COMMON_OPT_SESSION_NAME     995 /**< The --sessionname option number. */
#define GCTLCMD_COMMON_OPT_SESSION_ID       994 /**< The --sessionid option number. */

#define RETURN_ERROR(strError)     \
    {                              \
    m_strStatus.append(strError);  \
    return false;                  \
    }

#define GCTLCMD_COMMON_OPTION_DEFS() \
        { "--username",             GCTLCMD_COMMON_OPT_USER,            RTGETOPT_REQ_STRING  }, \
        { "--passwordfile",         GCTLCMD_COMMON_OPT_PASSWORD_FILE,   RTGETOPT_REQ_STRING  }, \
        { "--password",             GCTLCMD_COMMON_OPT_PASSWORD,        RTGETOPT_REQ_STRING  }, \
        { "--domain",               GCTLCMD_COMMON_OPT_DOMAIN,          RTGETOPT_REQ_STRING  }, \
        { "--quiet",                'q',                                RTGETOPT_REQ_NOTHING }, \
        { "--verbose",              'v',                                RTGETOPT_REQ_NOTHING },

#define HANDLE_COMMON_OPTION_DEFS()                   \
    case GCTLCMD_COMMON_OPT_USER:                     \
        commandData.m_strUserName = ValueUnion.psz;   \
        break;                                        \
    case GCTLCMD_COMMON_OPT_PASSWORD:                 \
        commandData.m_strPassword = ValueUnion.psz;   \
        break;

QString generateErrorString(int getOptErrorCode, const RTGETOPTUNION &/*valueUnion*/)
{
    QString errorString;
    // if (valueUnion.pDef)
    // {
    //     if (valueUnion.pDef->pszLong)
    //     {
    //         errorString = QString(valueUnion.pDef->pszLong);
    //     }
    // }

    switch (getOptErrorCode)
    {
        case (VERR_GETOPT_UNKNOWN_OPTION):
            errorString = errorString.append("RTGetOpt: Command line option not recognized.");
            break;
        case (VERR_GETOPT_REQUIRED_ARGUMENT_MISSING):
            errorString = errorString.append("RTGetOpt: Command line option needs argument.");
            break;
        case (VERR_GETOPT_INVALID_ARGUMENT_FORMAT):
            errorString = errorString.append("RTGetOpt: Command line option has argument with bad format.");
            break;
        case (VINF_GETOPT_NOT_OPTION):
            errorString = errorString.append("RTGetOpt: Not an option.");
            break;
        case (VERR_GETOPT_INDEX_MISSING):
            errorString = errorString.append("RTGetOpt: Command line option needs an index.");
            break;
        default:
            break;
    }
    return errorString;
}

/** Common option definitions: */
class CommandData
{
public:
    CommandData()
        : m_bSessionIdGiven(false)
        , m_bSessionNameGiven(false)
        , m_bCreateParentDirectories(false){}
    QString m_strUserName;
    QString m_strPassword;
    QString m_strExePath;
    QString m_strSessionName;
    QString m_strDirectoryPath;
    ULONG   m_uSessionId;
    QString m_strDomain;
    bool    m_bSessionIdGiven;
    bool    m_bSessionNameGiven;
    /* Create the whole path during mkdir */
    bool    m_bCreateParentDirectories;
    QVector<QString> m_arguments;
    QVector<QString> m_environmentChanges;
};

UIGuestControlInterface::UIGuestControlInterface(QObject* parent, const CGuest &comGuest)
    :QObject(parent)
    , m_comGuest(comGuest)
    , m_strHelp("[common-options]               [--username <name>] [--domain <domain>]\n"
                "                                   [--passwordfile <file> | --password <password>]\n"
                "start                           [common-options]\n"
                "                                   [--exe <path to executable>] [--timeout <msec>]\n"
                "                                   [--sessionid <id> |  [sessionname <name>]]\n"
                "                                   [-E|--putenv <NAME>[=<VALUE>]] [--unquoted-args]\n"
                "                                   [--ignore-operhaned-processes] [--profile]\n"
                "                                   -- <program/arg0> [argument1] ... [argumentN]]\n"
                "createsession                  [common-options]  [--sessionname <name>]\n"
                "mkdir                            [common-options]\n"
                "                                   [-P|--parents] [<guest directory>\n"
                "                                   [--sessionid <id> |  [sessionname <name>]]\n"
                )
{
    prepareSubCommandHandlers();
}

bool UIGuestControlInterface::handleMkdir(int argc , char** argv)
{

    CommandData commandData;

    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
        { "--sessionname",                  GCTLCMD_COMMON_OPT_SESSION_NAME,          RTGETOPT_REQ_STRING  },
        { "--sessionid",                    GCTLCMD_COMMON_OPT_SESSION_ID,            RTGETOPT_REQ_UINT32  },
        { "--parents",                      'P',                                      RTGETOPT_REQ_NOTHING  }
    };

    int ch;
    bool pathFound = false;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1 /* ignore 0th element (command) */, 0);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            HANDLE_COMMON_OPTION_DEFS()
            case GCTLCMD_COMMON_OPT_SESSION_NAME:
                commandData.m_bSessionNameGiven = true;
                commandData.m_strSessionName  = ValueUnion.psz;
                break;
            case GCTLCMD_COMMON_OPT_SESSION_ID:
                commandData.m_bSessionIdGiven = true;
                commandData.m_uSessionId  = ValueUnion.i32;
                break;
            case 'P':
                commandData.m_bCreateParentDirectories  = true;
                break;
            case VINF_GETOPT_NOT_OPTION:
                if (!pathFound)
                {
                    commandData.m_strDirectoryPath = ValueUnion.psz;
                    pathFound = true;
                }
                /* Allow only a single NOT_OPTION */
                else
                    RETURN_ERROR(generateErrorString(ch, ValueUnion))

                break;
            default:
                RETURN_ERROR(generateErrorString(ch, ValueUnion))
        }
    }
    if (commandData.m_strDirectoryPath.isEmpty())
        RETURN_ERROR(QString(m_strHelp).append("Syntax error! No path is given\n"));

    CGuestSession guestSession;
    if (!findOrCreateSession(commandData, guestSession) || !guestSession.isOk())
        return false;


    //const QString &strErr = comProgressInstall.GetErrorInfo().GetText();
    QVector<KDirectoryCreateFlag> creationFlags;
    if (commandData.m_bCreateParentDirectories)
        creationFlags.push_back(KDirectoryCreateFlag_None);
    else
        creationFlags.push_back(KDirectoryCreateFlag_Parents);

    guestSession.DirectoryCreate(commandData.m_strDirectoryPath, 0 /*ULONG aMode*/, creationFlags);

    //startProcess(commandData, guestSession);
    return true;
}

bool UIGuestControlInterface::handleStart(int argc, char** argv)
{
    enum kGstCtrlRunOpt
    {
        kGstCtrlRunOpt_IgnoreOrphanedProcesses = 1000,
        kGstCtrlRunOpt_NoProfile, /** @todo Deprecated and will be removed soon; use kGstCtrlRunOpt_Profile instead, if needed. */
        kGstCtrlRunOpt_Profile,
        kGstCtrlRunOpt_Dos2Unix,
        kGstCtrlRunOpt_Unix2Dos,
        kGstCtrlRunOpt_WaitForStdOut,
        kGstCtrlRunOpt_NoWaitForStdOut,
        kGstCtrlRunOpt_WaitForStdErr,
        kGstCtrlRunOpt_NoWaitForStdErr
    };

    CommandData commandData;

    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
        { "--sessionname",                   GCTLCMD_COMMON_OPT_SESSION_NAME,         RTGETOPT_REQ_STRING  },
        { "--sessionid",                     GCTLCMD_COMMON_OPT_SESSION_ID,           RTGETOPT_REQ_UINT32  },
        { "--putenv",                       'E',                                      RTGETOPT_REQ_STRING  },
        { "--exe",                          'e',                                      RTGETOPT_REQ_STRING  },
        { "--timeout",                      't',                                      RTGETOPT_REQ_UINT32  },
        { "--unquoted-args",                'u',                                      RTGETOPT_REQ_NOTHING },
        { "--ignore-operhaned-processes",   kGstCtrlRunOpt_IgnoreOrphanedProcesses,   RTGETOPT_REQ_NOTHING },
        { "--no-profile",                   kGstCtrlRunOpt_NoProfile,                 RTGETOPT_REQ_NOTHING }, /** @todo Deprecated. */
        { "--profile",                      kGstCtrlRunOpt_Profile,                   RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1 /* ignore 0th element (command) */, 0);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            HANDLE_COMMON_OPTION_DEFS()
            case GCTLCMD_COMMON_OPT_SESSION_NAME:
                commandData.m_bSessionNameGiven = true;
                commandData.m_strSessionName  = ValueUnion.psz;
                break;
            case GCTLCMD_COMMON_OPT_SESSION_ID:
                commandData.m_bSessionIdGiven = true;
                commandData.m_uSessionId  = ValueUnion.i32;
                break;
            case 'e':
                commandData.m_strExePath  = ValueUnion.psz;
                break;
            default:
                RETURN_ERROR(generateErrorString(ch, ValueUnion))
        }
    }
    if (commandData.m_strExePath.isEmpty())
        RETURN_ERROR(QString(m_strHelp).append("Syntax error! No executable is given\n"))

    CGuestSession guestSession;
    if (!findOrCreateSession(commandData, guestSession) || !guestSession.isOk())
        return false;
    startProcess(commandData, guestSession);
    return true;
}

bool UIGuestControlInterface::findOrCreateSession(const CommandData &commandData, CGuestSession &outGuestSession)
{
    if (commandData.m_bSessionNameGiven && commandData.m_strSessionName.isEmpty())
        RETURN_ERROR(QString(m_strHelp).append("'Session Name' is not name valid\n"))

    /* Check if sessionname and sessionid are both supplied */
    if (commandData.m_bSessionIdGiven && commandData.m_bSessionNameGiven)
        RETURN_ERROR(QString(m_strHelp).append("Both 'Session Name' and 'Session Id' are supplied\n"))

    /* If sessionid is given then look for the session. if not found return without starting the process: */
    else if (commandData.m_bSessionIdGiven && !commandData.m_bSessionNameGiven)
    {
        if (!findSession(commandData.m_uSessionId, outGuestSession))
        {
            RETURN_ERROR(QString(m_strHelp).append("No session with id %1 found.\n").arg(commandData.m_uSessionId))
        }
    }
    /* If sessionname is given then look for the session. if not try to create a session with the provided name: */
    else if (!commandData.m_bSessionIdGiven && commandData.m_bSessionNameGiven)
    {
        if (!findSession(commandData.m_strSessionName, outGuestSession))
        {
            if (!createSession(commandData, outGuestSession))
                return false;
        }
    }
    /* if neither sessionname and session id is given then create a new session */
    else
    {
        if (!createSession(commandData, outGuestSession))
            return false;
    }
    return true;
}

bool UIGuestControlInterface::handleHelp(int, char**)
{
    emit  sigOutputString(m_strHelp);
    return true;
}

bool UIGuestControlInterface::handleCreateSession(int argc, char** argv)
{
    CommandData commandData;

    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
        { "--sessionname",      GCTLCMD_COMMON_OPT_SESSION_NAME,  RTGETOPT_REQ_STRING  }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            HANDLE_COMMON_OPTION_DEFS()
            case GCTLCMD_COMMON_OPT_SESSION_NAME:
                commandData.m_strSessionName  = ValueUnion.psz;
                if (commandData.m_strSessionName.isEmpty())
                {
                    RETURN_ERROR(QString("'Session Name' is not name valid\n").append(m_strHelp))
                }
                break;
            default:
                break;
        }
    }
    CGuestSession guestSession;
    if (!createSession(commandData, guestSession))
        return false;
    return true;
}

bool UIGuestControlInterface::startProcess(const CommandData &commandData, CGuestSession &guestSession)
{
    QVector<KProcessCreateFlag>  createFlags;
    createFlags.push_back(KProcessCreateFlag_WaitForProcessStartOnly);

    CGuestProcess process = guestSession.ProcessCreate(commandData.m_strExePath,
                                                       commandData.m_arguments,
                                                       commandData.m_environmentChanges,
                                                       createFlags,
                                                       0);
    if (!process.isOk())
        return false;
    return true;
}

UIGuestControlInterface::~UIGuestControlInterface()
{
}

void UIGuestControlInterface::prepareSubCommandHandlers()
{
    m_subCommandHandlers.insert("createsession" , &UIGuestControlInterface::handleCreateSession);
    m_subCommandHandlers.insert("start", &UIGuestControlInterface::handleStart);
    m_subCommandHandlers.insert("help" , &UIGuestControlInterface::handleHelp);
    m_subCommandHandlers.insert("mkdir" , &UIGuestControlInterface::handleMkdir);
}

void UIGuestControlInterface::putCommand(const QString &strCommand)
{

    char **argv;
    int argc;
    QByteArray array = strCommand.toLocal8Bit();
    RTGetOptArgvFromString(&argv, &argc, array.data(), RTGETOPTARGV_CNV_QUOTE_BOURNE_SH, 0);
    m_strStatus.clear();
    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
             case VINF_GETOPT_NOT_OPTION:
             {
                 /* Try to map ValueUnion.psz to a sub command handler: */
                 QString strNoOption(ValueUnion.psz);
                 if (!strNoOption.isNull())
                 {
                     QMap<QString, HandleFuncPtr>::iterator iterator =
                         m_subCommandHandlers.find(strNoOption);
                     if (iterator != m_subCommandHandlers.end())
                     {
                         (this->*(iterator.value()))(argc, argv);
                         RTGetOptArgvFree(argv);
                         if (!m_strStatus.isEmpty())
                             emit sigOutputString(m_strStatus);
                         return;
                     }
                     else
                     {
                         emit sigOutputString(QString(m_strHelp).append("\nSyntax Error. Unknown Command '%1'").arg(ValueUnion.psz));
                         RTGetOptArgvFree(argv);
                         return;
                     }
                 }
                 break;
             }
             default:
                 break;
         }
    }
    if (!m_strStatus.isEmpty())
        emit sigOutputString(m_strStatus);

    RTGetOptArgvFree(argv);
}

bool UIGuestControlInterface::findSession(ULONG sessionId, CGuestSession& outSession)
{
    if (!m_comGuest.isOk())
        return false;
    QVector<CGuestSession> sessionVector = m_comGuest.GetSessions();
    if (sessionVector.isEmpty())
        return false;
    for (int  i = 0; i < sessionVector.size(); ++i)
    {
        if (sessionVector.at(i).isOk() && sessionId == sessionVector.at(i).GetId())
        {
            outSession = sessionVector.at(i);
            return true;
        }
    }
    return false;
}

bool UIGuestControlInterface::findSession(const QString& strSessionName, CGuestSession& outSession)
{
    if (!m_comGuest.isOk())
        return false;
    QVector<CGuestSession> sessionVector = m_comGuest.FindSession(strSessionName);
    if (sessionVector.isEmpty())
        return false;
    /* Return the first session with @a sessionName */
    outSession = sessionVector.at(0);
    return false;
}

bool UIGuestControlInterface::createSession(const CommandData &commandData, CGuestSession& outSession)
{
    CGuestSession guestSession = m_comGuest.CreateSession(commandData.m_strUserName,
                                                          commandData.m_strPassword,
                                                          commandData.m_strDomain,
                                                          commandData.m_strSessionName);

    if (!guestSession.isOk())
        return false;

    /* Wait session to start: */
    const ULONG waitTimeout = 2000;
    KGuestSessionWaitResult waitResult = guestSession.WaitFor(KGuestSessionWaitForFlag_Start, waitTimeout);
    if (waitResult != KGuestSessionWaitResult_Start)
        return false;

    outSession = guestSession;
    return true;
}
