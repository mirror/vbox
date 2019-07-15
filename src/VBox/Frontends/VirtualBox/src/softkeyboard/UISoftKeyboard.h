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
class QToolButton;
class UILayoutEditor;
class UILayoutSelector;
class UISession;
class UISoftKeyboardKey;
class UISoftKeyboardLayout;
class UISoftKeyboardSettingsWidget;
class UISoftKeyboardStatusBarWidget;
class UISoftKeyboardWidget;
class QSplitter;
class QStackedWidget;

class UISoftKeyboard : public QIWithRetranslateUI<QMainWindow>
{
    Q_OBJECT;

public:

    UISoftKeyboard(QWidget *pParent, UISession *pSession, QWidget *pCenterWidget,
                   QString strMachineName = QString());
    ~UISoftKeyboard();

protected:

    virtual void retranslateUi() /* override */;

private slots:

    void sltKeyboardLedsChange();
    void sltPutKeyboardSequence(QVector<LONG> sequence);
    void sltPutUsageCodesPress(QVector<QPair<LONG, LONG> > sequence);
    void sltPutUsageCodesRelease(QVector<QPair<LONG, LONG> > sequence);

    /** Handles the signal we get from the layout selector widget.
      * Selection changed is forwarded to the keyboard widget. */
    void sltLayoutSelectionChanged(const QUuid &layoutUid);
    /** Handles the signal we get from the keyboard widget. */
    void sltCurentLayoutChanged();
    void sltShowLayoutSelector();
    void sltShowLayoutEditor();
    void sltKeyToEditChanged(UISoftKeyboardKey* pKey);
    void sltLayoutEdited();
    /** Make th necessary changes to data structures when th key captions updated. */
    void sltKeyCaptionsEdited(UISoftKeyboardKey* pKey);
    void sltShowHideSidePanel();
    void sltShowHideSettingsWidget();
    void sltCopyLayout();
    void sltSaveLayout();
    void sltDeleteLayout();
    void sltStatusBarMessage(const QString &strMessage);
    void sltShowHideOSMenuKeys(bool fShow);
    void sltShowHideNumPad(bool fShow);
    void sltHandleColorCellClick(int iColorRow);
    void sltResetKeyboard();

private:

    void prepareObjects();
    void prepareConnections();
    void saveSettings();
    void loadSettings();
    void configure();
    void updateStatusBarMessage(const QString &strLayoutName);
    void updateLayoutSelectorList();
    void setDialogGeometry(const QRect &geometry);
    CKeyboard& keyboard() const;

    UISession     *m_pSession;
    QWidget       *m_pCenterWidget;
    QHBoxLayout   *m_pMainLayout;
    UISoftKeyboardWidget       *m_pKeyboardWidget;
    QString           m_strMachineName;
    QSplitter        *m_pSplitter;
    QStackedWidget   *m_pSidePanelWidget;
    UILayoutEditor   *m_pLayoutEditor;
    UILayoutSelector *m_pLayoutSelector;

    UISoftKeyboardSettingsWidget  *m_pSettingsWidget;
    UISoftKeyboardStatusBarWidget *m_pStatusBarWidget;

};

#endif /* !FEQT_INCLUDED_SRC_softkeyboard_UISoftKeyboard_h */
