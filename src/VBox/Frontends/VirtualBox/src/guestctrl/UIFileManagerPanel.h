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

/* GUI includes: */
#include "UIGuestControlDefs.h"
#include "QIWithRetranslateUI.h"
/* Forward declarations: */

class QTabWidget;
class QCheckBox;
class UIFileManagerOptions;

class UIFileManagerPanel : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigOptionsChanged();

public:

    UIFileManagerPanel(QWidget *pParent, UIFileManagerOptions *pFileManagerOptions);
    void update();

protected:

    virtual void retranslateUi() final override;

private slots:

    void sltListDirectoryCheckBoxToogled(bool bChecked);
    void sltDeleteConfirmationCheckBoxToogled(bool bChecked);
    void sltHumanReabableSizesCheckBoxToogled(bool bChecked);
    void sltShowHiddenObjectsCheckBoxToggled(bool bChecked);

private:

    void prepare();
    void preparePreferencesTab();
    QTabWidget   *m_pTabWidget;

    /** @name Preferences tab
     * @{ */
        QWidget    *m_pPreferencesTab;
        QCheckBox  *m_pListDirectoriesOnTopCheckBox;
        QCheckBox  *m_pDeleteConfirmationCheckBox;
        QCheckBox  *m_pHumanReabableSizesCheckBox;
        QCheckBox  *m_pShowHiddenObjectsCheckBox;
        UIFileManagerOptions *m_pFileManagerOptions;
    /** @} */


};

#endif /* !FEQT_INCLUDED_SRC_guestctrl_UIFileManagerPanel_h */
