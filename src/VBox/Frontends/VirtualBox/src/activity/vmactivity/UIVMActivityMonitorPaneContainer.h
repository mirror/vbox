/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMActivityMonitorPanel class declaration.
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

#ifndef FEQT_INCLUDED_SRC_activity_vmactivity_UIVMActivityMonitorPaneContainer_h
#define FEQT_INCLUDED_SRC_activity_vmactivity_UIVMActivityMonitorPaneContainer_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UILibraryDefs.h"
#include "UIPaneContainer.h"

class QLabel;
class QPushButton;

class SHARED_LIBRARY_STUFF UIVMActivityMonitorPaneContainer : public UIPaneContainer
{

    Q_OBJECT;

public:

    UIVMActivityMonitorPaneContainer(QWidget *pParent, EmbedTo enmEmbedTo = EmbedTo_Stack);

private slots:

    void sltRetranslateUI();

private:

    void prepare();
    QString m_strTabText;


    QLabel *m_pColorLabel0;
    QLabel *m_pColorLabel1;

    QPushButton *m_pColorChangeButton0;
    QPushButton *m_pColorChangeButton1;


};




#endif /* !FEQT_INCLUDED_SRC_activity_vmactivity_UIVMActivityMonitorPaneContainer_h */
