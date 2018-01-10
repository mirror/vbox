/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QComboBox>
# include <QHBoxLayout>
# if defined(RT_OS_SOLARIS)
#  include <QFontDatabase>
# endif
# include <QLabel>
# include <QLineEdit>
# include <QPlainTextEdit>
# include <QPushButton>
# include <QTextCursor>
# include <QToolButton>
# include <QScrollArea>

/* GUI includes: */
# include "UIIconPool.h"
# include "UISpecialControls.h"
# include "UIVMLogViewerPanel.h"
# include "UIVMLogViewerWidget.h"
# ifdef VBOX_WS_MAC
#  include "VBoxUtils-darwin.h"
# endif
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIVMLogViewerPanel::UIVMLogViewerPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pViewer(pViewer)
    , m_pMainLayout(0)
    , m_pCloseButton(0)
{
    prepare();
}

UIVMLogViewerWidget* UIVMLogViewerPanel::viewer()
{
    return m_pViewer;
}

const UIVMLogViewerWidget* UIVMLogViewerPanel::viewer() const
{
    return m_pViewer;
}

QHBoxLayout* UIVMLogViewerPanel::mainLayout()
{
    return m_pMainLayout;
}

void UIVMLogViewerPanel::prepare()
{
    prepareWidgets();
    prepareConnections();
    retranslateUi();
}

void UIVMLogViewerPanel::prepareWidgets()
{
    m_pMainLayout = new QHBoxLayout(this);
    AssertPtrReturnVoid(m_pMainLayout);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
    m_pMainLayout->setSpacing(2);

    m_pCloseButton = new UIMiniCancelButton(this);
    AssertPtrReturnVoid(m_pCloseButton);
    m_pMainLayout->addWidget(m_pCloseButton, 0, Qt::AlignLeft);
}

void UIVMLogViewerPanel::prepareConnections()
{
    if (m_pCloseButton)
        connect(m_pCloseButton, &UIMiniCancelButton::clicked, this, &UIVMLogViewerPanel::hide);

}


void UIVMLogViewerPanel::retranslateUi()
{
    if (m_pCloseButton)
        m_pCloseButton->setToolTip(UIVMLogViewerWidget::tr("Close the search panel."));
}

bool UIVMLogViewerPanel::eventFilter(QObject *pObject, QEvent *pEvent)
{
    return QWidget::eventFilter(pObject, pEvent);
}

void UIVMLogViewerPanel::showEvent(QShowEvent *pEvent)
{
    QWidget::showEvent(pEvent);
}

void UIVMLogViewerPanel::hideEvent(QHideEvent *pEvent)
{
    /* Get focused widget: */
    QWidget *pFocus = QApplication::focusWidget();
    /* If focus-widget is valid and child-widget of search-panel,
     * focus next child-widget in line: */
    if (pFocus && pFocus->parent() == this)
        focusNextPrevChild(true);
    if(m_pViewer)
        m_pViewer->hidePanel(this);

    QWidget::hideEvent(pEvent);
}
