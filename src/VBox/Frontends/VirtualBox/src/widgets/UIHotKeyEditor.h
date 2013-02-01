/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: UIHotKeyEditor class declaration
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
#include <QLineEdit>
#include <QMap>
#include <QSet>

/* Hot-key namespace to unify
 * all the related hot-key processing stuff: */
namespace UIHotKey
{
    QString toString(int iKeyCode);
    bool isValidKey(int iKeyCode);
#ifdef Q_WS_WIN
    int distinguishModifierVKey(int wParam, int lParam);
#endif /* Q_WS_WIN */
#ifdef Q_WS_X11
    void retranslateKeyNames();
#endif /* Q_WS_X11 */
}

/* Hot-combo namespace to unify
 * all the related hot-combo processing stuff: */
namespace UIHotKeyCombination
{
    QString toReadableString(const QString &strKeyCombo);
    QList<int> toKeyCodeList(const QString &strKeyCombo);
    bool isValidKeyCombo(const QString &strKeyCombo);
}

class UIHotKeyEditor : public QLineEdit
{
    Q_OBJECT;

public:

    UIHotKeyEditor(QWidget *pParent);
    ~UIHotKeyEditor();

    void setCombo(const QString &strKeyCombo);
    QString combo() const;

public slots:

    void sltDeselect();
    void sltClear();

protected:

#ifdef Q_WS_WIN
    bool winEvent(MSG *pMsg, long *pResult);
#endif /* Q_WS_WIN */
#ifdef Q_WS_X11
    bool x11Event(XEvent *pEvent);
#endif /* Q_WS_X11 */
#ifdef Q_WS_MAC
    static bool darwinEventHandlerProc(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser);
    bool darwinKeyboardEvent(const void *pvCocoaEvent, EventRef inEvent);
#endif /* Q_WS_MAC */

    void keyPressEvent(QKeyEvent *pEvent);
    void keyReleaseEvent(QKeyEvent *pEvent);
    void mousePressEvent(QMouseEvent *pEvent);
    void mouseReleaseEvent(QMouseEvent *pEvent);

private slots:

    void sltReleasePendingKeys();

private:

    bool processKeyEvent(int iKeyCode, bool fKeyPress);
    void updateText();

    QSet<int> m_pressedKeys;
    QSet<int> m_releasedKeys;
    QMap<int, QString> m_shownKeys;

    QTimer* m_pReleaseTimer;
    bool m_fStartNewSequence;

#ifdef Q_WS_MAC
    /* The current modifier key mask. Used to figure out which modifier
     * key was pressed when we get a kEventRawKeyModifiersChanged event. */
    uint32_t m_uDarwinKeyModifiers;
#endif /* Q_WS_MAC */
};

#endif // !___UIHotKeyEditor_h___

