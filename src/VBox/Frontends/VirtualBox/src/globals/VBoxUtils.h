/* $Id$ */
/** @file
 * VBox Qt GUI - Declarations of utility classes and functions.
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

/** QObject reimplementation,
  * providing passed QObject with property assignation routine. */
class QObjectPropertySetter : public QObject
{
    Q_OBJECT;

public:

    /** Constructor. */
    QObjectPropertySetter(QObject *pParent, const QString &strName)
        : QObject(pParent), m_strName(strName) {}

private slots:

    /** Assigns property value. */
    void sltAssignProperty(const QString &strValue)
        { parent()->setProperty(m_strName.toAscii().constData(), strValue); }

private:

    /** Holds property name. */
    const QString m_strName;
};

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

    QRichTextEdit (QWidget *aParent = 0) : QTextEdit (aParent) {}

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

/** Object containing functionality
  * to (de)serialize proxy settings. */
class UIProxyManager
{
public:

    /** Constructs object which parses passed @a strProxySettings. */
    UIProxyManager(const QString &strProxySettings = QString())
        : m_fProxyEnabled(false)
        , m_fAuthEnabled(false)
    {
        /* Parse proxy settings: */
        if (strProxySettings.isEmpty())
            return;
        QStringList proxySettings = strProxySettings.split(",");

        /* Parse proxy state, host and port: */
        if (proxySettings.size() > 0)
            m_fProxyEnabled = proxySettings[0] == "proxyEnabled";
        if (proxySettings.size() > 1)
            m_strProxyHost = proxySettings[1];
        if (proxySettings.size() > 2)
            m_strProxyPort = proxySettings[2];

        /* Parse whether proxy auth enabled and has login/password: */
        if (proxySettings.size() > 3)
            m_fAuthEnabled = proxySettings[3] == "authEnabled";
        if (proxySettings.size() > 4)
            m_strAuthLogin = proxySettings[4];
        if (proxySettings.size() > 5)
            m_strAuthPassword = proxySettings[5];
    }

    /** Serializes proxy settings. */
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

    /** Returns whether the proxy is enabled. */
    bool proxyEnabled() const { return m_fProxyEnabled; }
    /** Returns the proxy host. */
    const QString& proxyHost() const { return m_strProxyHost; }
    /** Returns the proxy port. */
    const QString& proxyPort() const { return m_strProxyPort; }

    /** Returns whether the proxy auth is enabled. */
    bool authEnabled() const { return m_fAuthEnabled; }
    /** Returns the proxy auth login. */
    const QString& authLogin() const { return m_strAuthLogin; }
    /** Returns the proxy auth password. */
    const QString& authPassword() const { return m_strAuthPassword; }

    /** Defines whether the proxy is @a fEnabled. */
    void setProxyEnabled(bool fEnabled) { m_fProxyEnabled = fEnabled; }
    /** Defines the proxy @a strHost. */
    void setProxyHost(const QString &strHost) { m_strProxyHost = strHost; }
    /** Defines the proxy @a strPort. */
    void setProxyPort(const QString &strPort) { m_strProxyPort = strPort; }

    /** Defines whether the proxy auth is @a fEnabled. */
    void setAuthEnabled(bool fEnabled) { m_fAuthEnabled = fEnabled; }
    /** Defines the proxy auth @a strLogin. */
    void setAuthLogin(const QString &strLogin) { m_strAuthLogin = strLogin; }
    /** Defines the proxy auth @a strPassword. */
    void setAuthPassword(const QString &strPassword) { m_strAuthPassword = strPassword; }

private:

    /** Holds whether the proxy is enabled. */
    bool m_fProxyEnabled;
    /** Holds the proxy host. */
    QString m_strProxyHost;
    /** Holds the proxy port. */
    QString m_strProxyPort;

    /** Holds whether the proxy auth is enabled. */
    bool m_fAuthEnabled;
    /** Holds the proxy auth login. */
    QString m_strAuthLogin;
    /** Holds the proxy auth password. */
    QString m_strAuthPassword;
};

#ifdef Q_WS_MAC
# include "VBoxUtils-darwin.h"
#endif /* Q_WS_MAC */

#endif // !___VBoxUtils_h___

