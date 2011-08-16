/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Declarations of utility classes and functions
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxUtils_h___
#define ___VBoxUtils_h___

#include <iprt/types.h>

/* Qt includes */
#include <QMouseEvent>
#include <QWidget>
#include <QTextBrowser>

/**
 *  Simple class that filters out all key presses and releases
 *  got while the Alt key is pressed. For some very strange reason,
 *  QLineEdit accepts those combinations that are not used as accelerators,
 *  and inserts the corresponding characters to the entry field.
 */
class QIAltKeyFilter : protected QObject
{
    Q_OBJECT;

public:

    QIAltKeyFilter (QObject *aParent) :QObject (aParent) {}

    void watchOn (QObject *aObject) { aObject->installEventFilter (this); }

protected:

    bool eventFilter (QObject * /* aObject */, QEvent *aEvent)
    {
        if (aEvent->type() == QEvent::KeyPress || aEvent->type() == QEvent::KeyRelease)
        {
            QKeyEvent *pEvent = static_cast<QKeyEvent *> (aEvent);
            if (pEvent->modifiers() & Qt::AltModifier)
                return true;
        }
        return false;
    }
};

/**
 *  Simple class which simulates focus-proxy rule redirecting widget
 *  assigned shortcut to desired widget.
 */
class QIFocusProxy : protected QObject
{
    Q_OBJECT;

public:

    QIFocusProxy (QWidget *aFrom, QWidget *aTo)
        : QObject (aFrom), mFrom (aFrom), mTo (aTo)
    {
        mFrom->installEventFilter (this);
    }

protected:

    bool eventFilter (QObject *aObject, QEvent *aEvent)
    {
        if (aObject == mFrom && aEvent->type() == QEvent::Shortcut)
        {
            mTo->setFocus();
            return true;
        }
        return QObject::eventFilter (aObject, aEvent);
    }

    QWidget *mFrom;
    QWidget *mTo;
};

/**
 *  QTextEdit reimplementation to feat some extended requirements.
 */
class QRichTextEdit : public QTextEdit
{
    Q_OBJECT;

public:

    QRichTextEdit (QWidget *aParent) : QTextEdit (aParent) {}

    void setViewportMargins (int aLeft, int aTop, int aRight, int aBottom)
    {
        QTextEdit::setViewportMargins (aLeft, aTop, aRight, aBottom);
    }
};

/**
 *  QTextBrowser reimplementation to feat some extended requirements.
 */
class QRichTextBrowser : public QTextBrowser
{
    Q_OBJECT;

public:

    QRichTextBrowser (QWidget *aParent) : QTextBrowser (aParent) {}

    void setViewportMargins (int aLeft, int aTop, int aRight, int aBottom)
    {
        QTextBrowser::setViewportMargins (aLeft, aTop, aRight, aBottom);
    }
};

class UIProxyManager
{
public:

    UIProxyManager(const QString &strProxySettings = QString())
        : m_fProxyEnabled(false), m_fAuthEnabled(false)
    {
        /* Parse settings: */
        if (!strProxySettings.isEmpty())
        {
            QStringList proxySettings = strProxySettings.split(",");
            if (proxySettings.size() > 0)
                m_fProxyEnabled = proxySettings[0] == "proxyEnabled";
            if (proxySettings.size() > 1)
                m_strProxyHost = proxySettings[1];
            if (proxySettings.size() > 2)
                m_strProxyPort = proxySettings[2];
            if (proxySettings.size() > 3)
                m_fAuthEnabled = proxySettings[3] == "authEnabled";
            if (proxySettings.size() > 4)
                m_strAuthLogin = proxySettings[4];
            if (proxySettings.size() > 5)
                m_strAuthPassword = proxySettings[5];
        }
    }

    QString toString() const
    {
        /* Serialize settings: */
        QString strResult;
        if (m_fProxyEnabled || !m_strProxyHost.isEmpty() || !m_strProxyPort.isEmpty() ||
            m_fAuthEnabled || !m_strAuthLogin.isEmpty() || !m_strAuthPassword.isEmpty())
        {
            QStringList proxySettings;
            proxySettings << QString(m_fProxyEnabled ? "proxyEnabled" : "proxyDisabled");
            proxySettings << m_strProxyHost;
            proxySettings << m_strProxyPort;
            proxySettings << QString(m_fAuthEnabled ? "authEnabled" : "authDisabled");
            proxySettings << m_strAuthLogin;
            proxySettings << m_strAuthPassword;
            strResult = proxySettings.join(",");
        }
        return strResult;
    }

    /* Proxy attribute getters: */
    bool proxyEnabled() const { return m_fProxyEnabled; }
    const QString& proxyHost() const { return m_strProxyHost; }
    const QString& proxyPort() const { return m_strProxyPort; }
    bool authEnabled() const { return m_fAuthEnabled; }
    const QString& authLogin() const { return m_strAuthLogin; }
    const QString& authPassword() const { return m_strAuthPassword; }

    /* Proxy attribute setters: */
    void setProxyEnabled(bool fProxyEnabled) { m_fProxyEnabled = fProxyEnabled; }
    void setProxyHost(const QString &strProxyHost) { m_strProxyHost = strProxyHost; }
    void setProxyPort(const QString &strProxyPort) { m_strProxyPort = strProxyPort; }
    void setAuthEnabled(bool fAuthEnabled) { m_fAuthEnabled = fAuthEnabled; }
    void setAuthLogin(const QString &strAuthLogin) { m_strAuthLogin = strAuthLogin; }
    void setAuthPassword(const QString &strAuthPassword) { m_strAuthPassword = strAuthPassword; }

private:

    /* Proxy attribute variables: */
    bool m_fProxyEnabled;
    QString m_strProxyHost;
    QString m_strProxyPort;
    bool m_fAuthEnabled;
    QString m_strAuthLogin;
    QString m_strAuthPassword;
};

/**
 *  VBoxVersion represents VirtualBox version parser
 */
class VBoxVersion
{
public:

    VBoxVersion(const QString &strVersion)
        : m_x(0), m_y(0), m_z(0)
    {
        QStringList versionStack = strVersion.split('.');
        if (versionStack.size() > 0)
            m_x = versionStack[0].toInt();
        if (versionStack.size() > 1)
            m_y = versionStack[1].toInt();
        if (versionStack.size() > 2)
            m_z = versionStack[2].toInt();
    }

    bool equal(const VBoxVersion &other) const { return (m_x == other.m_x) && (m_y == other.m_y) && (m_z == other.m_z); }
    bool operator==(const VBoxVersion &other) const { return equal(other); }
    bool operator!=(const VBoxVersion &other) const { return !equal(other); }

    bool operator<(const VBoxVersion &other) const
    {
        return (m_x <  other.m_x) ||
               (m_x == other.m_x && m_y <  other.m_y) ||
               (m_x == other.m_x && m_y == other.m_y && m_z <  other.m_z);
    }
    bool operator>(const VBoxVersion &other) const
    {
        return (m_x >  other.m_x) ||
               (m_x == other.m_x && m_y >  other.m_y) ||
               (m_x == other.m_x && m_y == other.m_y && m_z >  other.m_z);
    }

    QString toString() const
    {
        return QString("%1.%2.%3").arg(m_x).arg(m_y).arg(m_z);
    }

    int x() const { return m_x; }
    int y() const { return m_y; }
    int z() const { return m_z; }

private:

    int m_x;
    int m_y;
    int m_z;
};

#ifdef Q_WS_MAC
# include "VBoxUtils-darwin.h"
#endif /* Q_WS_MAC */

#endif // !___VBoxUtils_h___

