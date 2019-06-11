/* $Id$ */
/** @file
 * VBox Qt GUI - UISoftKeyboard class declaration.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_softkeyboard_UISoftKeyboard_h
#define FEQT_INCLUDED_SRC_softkeyboard_UISoftKeyboard_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CGuest.h"
#include "CEventListener.h"

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QHBoxLayout;
class QVBoxLayout;
class UISoftKeyboardWidget;
class QToolButton;
class UISession;
class UISoftKeyboardKey;

class UISoftKeyboard : public QIWithRetranslateUI<QMainWindow>
{
    Q_OBJECT;

public:

    UISoftKeyboard(QWidget *pParent, UISession *pSession,
                   QString strMachineName = QString());
    ~UISoftKeyboard();

protected:

    virtual void retranslateUi() /* override */;
    bool eventFilter(QObject *pWatched, QEvent *pEvent)/* override */;

private slots:

    void sltHandleKeyboardLedsChange();
    void sltHandlePutKeyboardSequence(QVector<LONG> sequence);
    void sltHandleLayoutChange(const QString &strLayoutName);
    void sltHandleKeyCapFileChange(const QString &strKeyCapFileName);
    void sltHandleStatusBarContextMenuRequest(const QPoint &point);
private:

    void prepareObjects();
    void prepareConnections();
    void saveSettings();
    void loadSettings();
    void updateStatusBarMessage();
    CKeyboard& keyboard() const;

    UISession     *m_pSession;
    QHBoxLayout   *m_pMainLayout;
    UISoftKeyboardWidget       *m_pContainerWidget;
    QString       m_strMachineName;
    QString       m_strLayoutName;
    QString       m_strKeyCapFileName;
    QToolButton   *m_pSettingsButton;
};

#endif /* !FEQT_INCLUDED_SRC_softkeyboard_UISoftKeyboard_h */
