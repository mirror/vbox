/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationConfiguration class declaration.
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

#ifndef ___UIGuestControlInterface_h___
#define ___UIGuestControlInterface_h___


/* Qt includes: */
#include <QObject>
#include <QMap>

/* COM includes: */
#include "COMEnums.h"
#include "CGuest.h"


class UIGuestControlInterface : public QObject
{

    Q_OBJECT;

signals:

    void sigOutputString(const QString &strOutput);

public:

    UIGuestControlInterface(QObject *parent, const CGuest &comGeust);
    /** Receives a command string */
    void putCommand(const QString &strCommand);

private slots:

private:

    bool parseCommand(const QString &strCommand);
    bool createArgumentMap(const QStringList &commandList);
    bool parseStartCommand();
    bool parseStartSessionCommand();
    bool parseSetCommand();

    /* Search username/password withing argument map if not
       check if username/password is set previously. Return false
       if username/string could not be found. */
    bool getUsername(QString &outStrUsername);
    bool getPassword(QString &outStrUsername);
    void reset();

    /** @name API call wrappers
     * @{ */
    void createSession(const QString &username, const QString &password,
                       const QString &sessionName);
    /** @} */


    CGuest        m_comGuest;
    const QString m_strHelp;
    QString       m_strStatus;
    QString       m_strUsername;
    QString       m_strPassword;
    bool          m_bUsernameIsSet;
    bool          m_bPasswordIsSet;

    /* Key is argument and value is value (possibly empty). */
    QMap<QString, QString> m_argumentMap;

};

#endif /* !___UIGuestControlInterface_h___ */
