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
# include "UIGuestControlInterface.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CGuestSession.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#define SUCCESS_RETURN(strMessage){   \
    emit sigOutputString(strMessage); \
    return true;                      \
}

#define ERROR_RETURN(strMessage){      \
    emit sigOutputString(strMessage);  \
    return false;                      \
}

UIGuestControlInterface::UIGuestControlInterface(QObject* parent, const CGuest &comGuest)
    :QObject(parent)
    , m_comGuest(comGuest)
    , m_strHelp("start\n"
                "        --session: Starts a new session. It takes following arguments\n"
                "                        --username=username: Sets the username. Overrides the username set by 'set --username=username'\n"
                "                        --password=password: Sets the password. Overrides the username set by 'set --password=password'\n"
                "                        --session-name=name: Optional\n"
                "                        --domain=domain: Currently not implemented and siliently ignored\n"
                "        --process: Starts a new process. It takes following arguments\n"
                "                        --username=username: Sets the username. Overrides the username set by 'set --username=username'\n"
                "                        --password=password: Sets the password. Overrides the username set by 'set --password=password'\n"
                "                        --session-name=name: Session name to start the new process under. If not found a new session with this name is created\n"
                "                        --exe-path=path: Execuable path\n"
                "                        --argument1=argument ... --argumentN=argument: Optional. Arguments to be passed to the new process\n"
                "                        --environmentVar1=variable ... --environmentVarN=variable: Optional. Currently no avaible through this interface\n"
                "                        --timout=time: (in ms). Optional. Timeout (in ms) for limiting the guest process' running time. Give 0 for an infinite timeout.\n"

                "set\n"
                "                        --username=username: Sets user name which is used in subsequent calls (maybe overriden).\n"
                "                        --password=password: Sets user name which is used in subsequent calls (maybe overriden).\n"

                //"start --process --username=username --password=password --session-name=name --exepath=path --argument1=argument ... --argumentN=argument --environmentVar1=variable ... --environmentVarN=variable\n"
                )
    , m_bUsernameIsSet(false)
    , m_bPasswordIsSet(false)
{
}

void UIGuestControlInterface::putCommand(const QString &strCommand)
{
    parseCommand(strCommand);
}

bool UIGuestControlInterface::parseCommand(const QString &strCommand)
{
    reset();
    QStringList commandStrList = strCommand.split(" ", QString::SkipEmptyParts, Qt::CaseSensitive);

    if (commandStrList.isEmpty())
        ERROR_RETURN("Syntax Error! Type 'help' for usage")
    else if (commandStrList.at(0) == "help")
        SUCCESS_RETURN(m_strHelp)

    if (!createArgumentMap(commandStrList))
        return false;

    if (commandStrList.at(0) == "start")
        return parseStartCommand();
    else
        return parseSetCommand();
    ERROR_RETURN("Syntax Error! Type 'help' for usage")
}

bool UIGuestControlInterface::createArgumentMap(const QStringList &commandList)
{
    if (commandList.size() <= 1)
        ERROR_RETURN("Syntax Error! Type 'help' for usage")

    /* All arguments start with '--' and may have a value: */
    for (int i = 1; i < commandList.size(); ++i)
    {
        const QString &strArgument = commandList.at(i);
        if (strArgument.size() <= 2 || strArgument.left(2) != "--")
            ERROR_RETURN("Syntax Error! Type 'help' for usage")

        QString strKey;
        QString strValue;
        /* If argument includes a '=' then we have a two parts: */
        int indexOfEqual = strArgument.indexOf("=");
        strKey = strArgument.mid(2 /* -- prefix */, indexOfEqual - 2);
        if (indexOfEqual != -1)
            strValue = strArgument.mid(indexOfEqual + 1);
        if (indexOfEqual != -1 && strValue.isEmpty())
            ERROR_RETURN("Syntax Error! Type 'help' for usage")
        if (strKey.isEmpty())
            ERROR_RETURN("Syntax Error! Type 'help' for usage")

        m_argumentMap.insert(strKey, strValue);
    }
    return true;
}

bool UIGuestControlInterface::parseStartCommand()
{
    if (m_argumentMap.contains("session"))
        return parseStartSessionCommand();

    ERROR_RETURN("Syntax Error! Type 'help' for usage")
}

//CGuestProcess CGuestSession::ProcessCreate(const QString & aExecutable,
//const QVector<QString> & aArguments,
//const QVector<QString> & aEnvironmentChanges, const QVector<KProcessCreateFlag> & aFlags, ULONG aTimeoutMS)


bool UIGuestControlInterface::parseStartSessionCommand()
{
    /* Each "start" command requires username and password: */
    QString username;
    QString password;
    if (!getUsername(username))
        return false;
    if (!getPassword(password))
        return false;
    /* Check if session name is supplied: */
    QString sessionname = m_argumentMap.value("session-name", QString());
    if (!m_comGuest.isOk())
        ERROR_RETURN("No Valid guest object found")

    CGuestSession comGuestSession =
        m_comGuest.CreateSession(username, password, QString() /*domain name*/, sessionname);
    if (!comGuestSession.isOk())
        ERROR_RETURN("Could not create guest session")

    SUCCESS_RETURN("Guest Session started");
}

bool UIGuestControlInterface::parseSetCommand()
{
    /* After 'set' we should have a single argument with value: */
    if (m_argumentMap.size() != 1)
        ERROR_RETURN("Syntax Error! Type 'help' for usage")
    /* Look for username: */
    if (m_argumentMap.contains("username"))
    {
        QString strName = m_argumentMap.value("username", QString());
        if (strName.isEmpty())
            ERROR_RETURN("Syntax Error! Type 'help' for usage")
        m_strUsername = strName;
        return true;
    }
    else if (m_argumentMap.contains("password"))
    {
        QString strPassword = m_argumentMap.value("password", QString());
        if (strPassword.isEmpty())
            ERROR_RETURN("Syntax Error! Type 'help' for usage")
        m_strPassword = strPassword;
        return true;
    }
    /* Found neither 'usernanme' nor 'password' after set: */
    ERROR_RETURN("Syntax Error! Type 'help' for usage")
}

bool UIGuestControlInterface::getUsername(QString &outStrUsername)
{
    /* First check argument map for a valid username string: */
    QString mapValue = m_argumentMap.value("username", QString());
    if (!mapValue.isEmpty())
    {
        outStrUsername = mapValue;
        return true;
    }
    /* Check to see if username is set previously. */
    if (m_bUsernameIsSet)
    {
        outStrUsername = m_strUsername;
        return true;
    }
    ERROR_RETURN("No username is set. Type 'help' for usage");
}

bool UIGuestControlInterface::getPassword(QString &outStrPassword)
{
    /* First check argument map for a valid password string: */
    QString mapValue = m_argumentMap.value("password", QString());
    if (!mapValue.isEmpty())
    {
        outStrPassword = mapValue;
        return true;
    }

    /* Check to see if username is set previously. */
    if (m_bPasswordIsSet)
    {
        outStrPassword = m_strPassword;
        return true;
    }
    ERROR_RETURN("No password is set. Type 'help' for usage")
}

void UIGuestControlInterface::reset()
{
    m_argumentMap.clear();
    m_bUsernameIsSet = false;
    m_bPasswordIsSet = false;
}
