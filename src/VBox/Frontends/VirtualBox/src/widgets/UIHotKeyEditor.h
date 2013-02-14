/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: UIHotKeyEditor class declaration
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIHotKeyEditor_h___
#define ___UIHotKeyEditor_h___

/* Qt includes: */
#include <QMetaType>
#include <QWidget>
#include <QSet>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QHBoxLayout;
class QIToolButton;
class UIHotKeyLineEdit;

/* A string pair wrapper for hot-key sequence: */
class UIHotKey
{
public:

    /* Constructors: */
    UIHotKey() {}
    UIHotKey(const QString &strSequence,
             const QString &strDefaultSequence)
        : m_strSequence(strSequence)
        , m_strDefaultSequence(strDefaultSequence)
    {}
    UIHotKey(const UIHotKey &other)
        : m_strSequence(other.sequence())
        , m_strDefaultSequence(other.defaultSequence())
    {}

    /* API: Operators stuff: */
    UIHotKey& operator=(const UIHotKey &other)
    {
        m_strSequence = other.sequence();
        m_strDefaultSequence = other.defaultSequence();
        return *this;
    }

    /* API: Access stuff: */
    const QString& sequence() const { return m_strSequence; }
    const QString& defaultSequence() const { return m_strDefaultSequence; }
    void setSequence(const QString &strSequence) { m_strSequence = strSequence; }

private:

    /* Variables: */
    QString m_strSequence;
    QString m_strDefaultSequence;
};
Q_DECLARE_METATYPE(UIHotKey);

/* A widget wrapper for real hot-key editor: */
class UIHotKeyEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;
    Q_PROPERTY(UIHotKey hotKey READ hotKey WRITE setHotKey USER true);

public:

    /* Constructor: */
    UIHotKeyEditor(QWidget *pParent);

private slots:

    /* Handlers: Tool-button stuff: */
    void sltReset();
    void sltClear();

private:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Handlers: Line-edit key event pre-processing stuff: */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);
    bool shouldWeSkipKeyEventToLineEdit(QKeyEvent *pEvent);

    /* Handlers: Key event processing stuff: */
    void keyPressEvent(QKeyEvent *pEvent);
    void keyReleaseEvent(QKeyEvent *pEvent);
    bool isKeyEventIgnored(QKeyEvent *pEvent);

    /* Helper: Modifier stuff: */
    void fetchModifiersState();

    /* Handlers: Sequence stuff: */
    bool approvedKeyPressed(QKeyEvent *pKeyEvent);
    void handleKeyPress(QKeyEvent *pKeyEvent);
    void handleKeyRelease(QKeyEvent *pKeyEvent);
    void reflectSequence();

    /* API: Editor stuff: */
    UIHotKey hotKey() const;
    void setHotKey(const UIHotKey &hotKey);

    /* Variables: */
    mutable UIHotKey m_hotKey;
    QHBoxLayout *m_pMainLayout;
    UIHotKeyLineEdit *m_pLineEdit;
    QIToolButton *m_pResetButton;
    QIToolButton *m_pClearButton;
    QSet<int> m_takenModifiers;
    int m_iTakenKey;
    bool m_fSequenceTaken;
    bool m_fTakenKeyReleased;
};

#endif /* !___UIHotKeyEditor_h___ */

