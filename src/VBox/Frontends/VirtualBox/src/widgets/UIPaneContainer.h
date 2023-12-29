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

#ifndef FEQT_INCLUDED_SRC_widgets_UIPaneContainer_h
#define FEQT_INCLUDED_SRC_widgets_UIPaneContainer_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>
#include <QKeySequence>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QHBoxLayout;
class QIDialogButtonBox;

/** QWidget extension acting as the base class for all the dialog panels like file manager, logviewer etc. */
class SHARED_LIBRARY_STUFF UIPaneContainer : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigCurrentTabChanged(int iIndex);
    void sigHidden();
    void sigDetach();

public:

    UIPaneContainer(QWidget *pParent, EmbedTo enmEmbedTo = EmbedTo_Stack, bool fDetachAllowed = false);
    void setCurrentIndex(int iIndex);
    int currentIndex() const;

protected:

    virtual void prepare();
    void insertTab(int iIndex, QWidget *pPage, const QString &strLabel = QString());
    void setTabText(int iIndex, const QString &strText);
    void retranslateUi() override;

private slots:

    void sltHide();

private:

    EmbedTo  m_enmEmbedTo;
    bool     m_fDetachAllowed;

    QTabWidget *m_pTabWidget;

    QIDialogButtonBox *m_pButtonBox;
};

#endif /* !FEQT_INCLUDED_SRC_widgets_UIPaneContainer_h */
