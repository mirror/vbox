/* $Id$ */
/** @file
 * VBox Qt GUI - VirtualBox Qt extensions: UIHostComboEditor class declaration.
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

#ifndef ___UIHostComboEditor_h___
#define ___UIHostComboEditor_h___

/* Qt includes: */
#include <QLineEdit>
#include <QMetaType>
#include <QMap>
#include <QSet>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QIToolButton;
class UIHostComboEditorPrivate;
#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
# if QT_VERSION >= 0x050000
class ComboEditorEventFilter;
# endif /* QT_VERSION >= 0x050000 */
#endif /* VBOX_WS_MAC || VBOX_WS_WIN */
#ifdef VBOX_WS_WIN
class WinAltGrMonitor;
#endif /* VBOX_WS_WIN */


/* Native hot-key namespace to unify
 * all the related hot-key processing stuff: */
namespace UINativeHotKey
{
    QString toString(int iKeyCode);
    bool isValidKey(int iKeyCode);
    /** Translates a modifier key in host platform
      * encoding to the corresponding set 1 PC scan code.
      * @note  Non-modifier keys will return zero. */
    unsigned modifierToSet1ScanCode(int iKeyCode);
#if defined(VBOX_WS_WIN)
    int distinguishModifierVKey(int wParam, int lParam);
#elif defined(VBOX_WS_X11)
    void retranslateKeyNames();
#endif /* VBOX_WS_X11 */
}

/* Host-combo namespace to unify
 * all the related hot-combo processing stuff: */
namespace UIHostCombo
{
    int hostComboModifierIndex();
    QString hostComboModifierName();
    QString hostComboCacheKey();
    QString toReadableString(const QString &strKeyCombo);
    QList<int> toKeyCodeList(const QString &strKeyCombo);
    /** Returns a sequence of the set 1 PC scan codes for all
      * modifiers contained in the (host platform format) sequence passed. */
    QList<unsigned> modifiersToScanCodes(const QString &strKeyCombo);
    bool isValidKeyCombo(const QString &strKeyCombo);
}

/* Host-combo wrapper: */
class UIHostComboWrapper
{
public:

    UIHostComboWrapper(const QString &strHostCombo = QString())
        : m_strHostCombo(strHostCombo)
    {}

    const QString& toString() const { return m_strHostCombo; }

private:

    QString m_strHostCombo;
};
Q_DECLARE_METATYPE(UIHostComboWrapper);

/** Host-combo editor widget. */
class UIHostComboEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;
    Q_PROPERTY(UIHostComboWrapper combo READ combo WRITE setCombo USER true);

signals:

    /** Notifies listener about data should be committed. */
    void sigCommitData(QWidget *pThis);

public:

    /** Constructs host-combo editor for passed @a pParent. */
    UIHostComboEditor(QWidget *pParent);

private slots:

    /** Notifies listener about data should be committed. */
    void sltCommitData();

private:

    /** Prepares widget content. */
    void prepare();

    /** Translates widget content. */
    void retranslateUi();

    /** Defines host-combo sequence. */
    void setCombo(const UIHostComboWrapper &strCombo);
    /** Returns host-combo sequence. */
    UIHostComboWrapper combo() const;

    /** UIHostComboEditorPrivate instance. */
    UIHostComboEditorPrivate *m_pEditor;
    /** <b>Clear</b> QIToolButton instance. */
    QIToolButton *m_pButtonClear;
};

/* Host-combo editor widget private stuff: */
class UIHostComboEditorPrivate : public QLineEdit
{
    Q_OBJECT;

signals:

    /** Notifies parent about data changed. */
    void sigDataChanged();

public:

    UIHostComboEditorPrivate();
    ~UIHostComboEditorPrivate();

    void setCombo(const UIHostComboWrapper &strCombo);
    UIHostComboWrapper combo() const;

public slots:

    void sltDeselect();
    void sltClear();

protected:

#if QT_VERSION >= 0x050000
    /** Qt5: Handles all native events. */
    bool nativeEvent(const QByteArray &eventType, void *pMessage, long *pResult);
#else /* QT_VERSION < 0x050000 */
# if defined(VBOX_WS_MAC)
    /** Mac: Qt4: Handles all native events (static callback). */
    static bool darwinEventHandlerProc(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser);
    /** Mac: Qt4: Handles all native events. */
    bool darwinKeyboardEvent(const void *pvCocoaEvent, EventRef inEvent);
# elif defined(VBOX_WS_WIN)
    /** Win: Qt4: Handles all native events. */
    bool winEvent(MSG *pMsg, long *pResult);
# elif defined(VBOX_WS_X11)
    /** X11: Qt4: Handles all native events. */
    bool x11Event(XEvent *pEvent);
# endif /* VBOX_WS_X11 */
#endif /* QT_VERSION < 0x050000 */

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

#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
# if QT_VERSION >= 0x050000
    /** Mac, Win: Holds the native event filter instance. */
    ComboEditorEventFilter *m_pPrivateEventFilter;
    /** Mac, Win: Allows the native event filter to
      * redirect events directly to nativeEvent handler. */
    friend class ComboEditorEventFilter;
# endif /* QT_VERSION >= 0x050000 */
#endif /* VBOX_WS_MAC || VBOX_WS_WIN */

#if defined(VBOX_WS_MAC)
    /** Mac: Holds the current modifier key mask. Used to figure out which modifier
      * key was pressed when we get a kEventRawKeyModifiersChanged event. */
    uint32_t m_uDarwinKeyModifiers;
#elif defined(VBOX_WS_WIN)
    /** Win: Holds the object monitoring key event
      * stream for problematic AltGr events. */
    WinAltGrMonitor *m_pAltGrMonitor;
#endif /* VBOX_WS_WIN */
};

#endif /* !___UIHostComboEditor_h___ */

