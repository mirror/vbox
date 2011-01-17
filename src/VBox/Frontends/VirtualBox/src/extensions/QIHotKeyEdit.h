/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIHotKeyEdit class declaration
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

#ifndef ___QIHotKeyEdit_h___
#define ___QIHotKeyEdit_h___

/* Global includes */
#include <QLabel>

#ifdef Q_WS_X11
# include <QMap>
#endif /* Q_WS_X11 */

class QIHotKeyEdit : public QLabel
{
    Q_OBJECT

public:

    QIHotKeyEdit(QWidget *pParent);
    virtual ~QIHotKeyEdit();

    void setKey(int iKeyVal);
    int key() const { return m_iKeyVal; }

    QString symbolicName() const { return m_strSymbName; }

    QSize sizeHint() const;
    QSize minimumSizeHint() const;

    static QString keyName(int iKeyVal);
    static bool isValidKey(int iKeyVal);
#ifdef Q_WS_X11
    static void retranslateUi();
#endif /* Q_WS_X11 */

public slots:

    void clear();

protected:

#if defined (Q_WS_WIN32)
    bool winEvent(MSG *pMsg, long *pResult);
#elif defined (Q_WS_X11)
    bool x11Event(XEvent *pEvent);
#elif defined (Q_WS_MAC)
    static bool darwinEventHandlerProc(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser);
    bool darwinKeyboardEvent(const void *pvCocoaEvent, EventRef inEvent);
#endif

    void focusInEvent(QFocusEvent *pEvent);
    void focusOutEvent(QFocusEvent *pEvent);
    void paintEvent(QPaintEvent *pEvent);

private:

    void updateText();

    int m_iKeyVal;
    QString m_strSymbName;

#ifdef Q_WS_X11
    static QMap<QString, QString> s_keyNames;
#endif /* Q_WS_X11 */

#ifdef Q_WS_MAC
    /* The current modifier key mask. Used to figure out which modifier
     * key was pressed when we get a kEventRawKeyModifiersChanged event. */
    uint32_t m_uDarwinKeyModifiers;
#endif /* Q_WS_MAC */

    static const char *m_spNoneSymbName;
};

#endif // !___QIHotKeyEdit_h___

