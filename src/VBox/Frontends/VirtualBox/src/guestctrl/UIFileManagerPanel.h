/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
 */

/*
 * Copyright (C) 2010-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef FEQT_INCLUDED_SRC_guestctrl_UIFileManagerPanel_h
#define FEQT_INCLUDED_SRC_guestctrl_UIFileManagerPanel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes */
#include <QWidget>
#include <QSet>

/* GUI includes: */
#include "UIGuestControlDefs.h"
#include "QIWithRetranslateUI.h"

#include "CProgress.h"

/* Forward declarations: */

class QTabWidget;
class QCheckBox;
class UIFileManagerLogViewer;
class UIFileManagerOptions;
class QScrollArea;
class QSpacerItem;
class QVBoxLayout;

class UIFileManagerPanel : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /* Signal(s) related to preferences tab. */
    void sigOptionsChanged();
    /* Signal(s) related to operations tab. */
    void sigFileOperationComplete(QUuid progressId);
    void sigFileOperationFail(QString strErrorString, QString strSourceTableName, FileManagerLogType eLogType);

public:

    UIFileManagerPanel(QWidget *pParent, UIFileManagerOptions *pFileManagerOptions);
    void updatePreferences();
    void appendLog(const QString &strLog, const QString &strMachineName, FileManagerLogType eLogType);
    void addNewProgress(const CProgress &comProgress, const QString &strSourceTableName);

protected:

    virtual void retranslateUi() final override;
    virtual void contextMenuEvent(QContextMenuEvent *pEvent) final override;

private slots:

    /** @name Preferences tab slots
     * @{ */
        void sltListDirectoryCheckBoxToogled(bool bChecked);
        void sltDeleteConfirmationCheckBoxToogled(bool bChecked);
        void sltHumanReabableSizesCheckBoxToogled(bool bChecked);
        void sltShowHiddenObjectsCheckBoxToggled(bool bChecked);
    /** @} */

    /** @name Operations tab slots
     * @{ */
        void sltRemoveFinished();
        void sltRemoveAll();
        void sltRemoveSelected();
        void sltHandleWidgetFocusIn(QWidget *pWidget);
        void sltHandleWidgetFocusOut(QWidget *pWidget);
        void sltScrollToBottom(int iMin, int iMax);
    /** @} */

private:

    void prepare();
    void preparePreferencesTab();
    void prepareLogTab();
    void prepareOperationsTab();

    QTabWidget   *m_pTabWidget;

    /** @name Preferences tab
     * @{ */
        QCheckBox  *m_pListDirectoriesOnTopCheckBox;
        QCheckBox  *m_pDeleteConfirmationCheckBox;
        QCheckBox  *m_pHumanReabableSizesCheckBox;
        QCheckBox  *m_pShowHiddenObjectsCheckBox;
        UIFileManagerOptions *m_pFileManagerOptions;
    /** @} */

    /** @name Log tab
     * @{ */
        UIFileManagerLogViewer *m_pLogTextEdit;
    /** @} */

    /** @name Operations tab
     * @{ */
        QScrollArea    *m_pScrollArea;
        QVBoxLayout    *m_pOperationsTabLayout;
        QSpacerItem    *m_pContainerSpaceItem;
        QWidget        *m_pWidgetInFocus;
        QSet<QWidget*>  m_widgetSet;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_guestctrl_UIFileManagerPanel_h */
