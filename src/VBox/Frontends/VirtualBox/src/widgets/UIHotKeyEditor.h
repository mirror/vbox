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

/* Forward declarations: */
class QHBoxLayout;
class UIHotKeyLineEdit;

/* A string wrapper for hot-key sequence: */
class UIHotKey
{
public:

    /* Constructors: */
    UIHotKey(const QString &strSequence = QString()) : m_strSequence(strSequence) {}
    UIHotKey(const UIHotKey &other) : m_strSequence(other.toString()) {}

    /* API: Conversion stuff: */
    const QString& toString() const { return m_strSequence; }

private:

    /* Variables: */
    QString m_strSequence;
};
Q_DECLARE_METATYPE(UIHotKey);

/* A widget wrapper for real hot-key editor: */
class UIHotKeyEditor : public QWidget
{
    Q_OBJECT;
    Q_PROPERTY(UIHotKey hotKey READ hotKey WRITE setHotKey USER true);

public:

    /* Constructor: */
    UIHotKeyEditor(QWidget *pParent);

private:

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
    void handleKeyPress(QKeyEvent *pKeyEvent);
    void handleKeyRelease(QKeyEvent *pKeyEvent);
    void reflectSequence();

    /* API: Editor stuff: */
    UIHotKey hotKey() const;
    void setHotKey(const UIHotKey &hotKey);

    /* Variables: */
    QHBoxLayout *m_pMainLayout;
    UIHotKeyLineEdit *m_pLineEdit;
    QSet<int> m_takenModifiers;
    int m_iTakenKey;
    bool m_fSequenceTaken;
    bool m_fTakenKeyReleased;
};

#endif /* !___UIHotKeyEditor_h___ */

