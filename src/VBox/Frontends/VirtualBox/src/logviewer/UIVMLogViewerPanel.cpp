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
# include <QLabel>
# include <QPlainTextEdit>
# include <QTextCursor>
# include <QToolButton>

/* GUI includes: */
# include "QIToolButton.h"
# include "UIIconPool.h"
# include "UIVMLogPage.h"
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

void UIVMLogViewerPanel::setCloseButtonShortCut(QKeySequence shortCut)
{
    if (!m_pCloseButton)
        return;
    m_pCloseButton->setShortcut(shortCut);
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
    if (m_pMainLayout)
    {
#ifdef VBOX_WS_MAC
        m_pMainLayout->setContentsMargins(5, 0, 0, 5);
        m_pMainLayout->setSpacing(5);
#else
        m_pMainLayout->setContentsMargins(qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 2, 0,
                                             qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin) / 2, 0);
        m_pMainLayout->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) / 2);
#endif
    }
    m_pCloseButton = new QIToolButton;
    if (m_pCloseButton)
    {
        m_pCloseButton->setIcon(UIIconPool::iconSet(":/close_16px.png"));
        m_pMainLayout->addWidget(m_pCloseButton, 0, Qt::AlignLeft);
    }
}

void UIVMLogViewerPanel::prepareConnections()
{
    if (m_pCloseButton)
        connect(m_pCloseButton, &QIToolButton::clicked, this, &UIVMLogViewerPanel::hide);
}

void UIVMLogViewerPanel::retranslateUi()
{
    if (m_pCloseButton)
        m_pCloseButton->setToolTip(UIVMLogViewerWidget::tr("Close the search panel."));
}

bool UIVMLogViewerPanel::eventFilter(QObject *pObject, QEvent *pEvent)
{
    Q_UNUSED(pObject);
    Q_UNUSED(pEvent);
    /* Dont consume this event. Pass it back to Qt's event system: */
    return false;
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
    if (m_pViewer)
        m_pViewer->hidePanel(this);

    QWidget::hideEvent(pEvent);
}

QTextDocument  *UIVMLogViewerPanel::textDocument()
{
    QPlainTextEdit *pEdit = textEdit();
    if (!pEdit)
        return 0;
    return textEdit()->document();
}

QPlainTextEdit *UIVMLogViewerPanel::textEdit()
{
    if (!viewer())
        return 0;
    UIVMLogPage *logPage = viewer()->currentLogPage();
    if (!logPage)
        return 0;
    return logPage->textEdit();
}

const QString* UIVMLogViewerPanel::logString() const
{
    if (!viewer())
        return 0;
    const UIVMLogPage* const page = qobject_cast<const UIVMLogPage* const>(viewer()->currentLogPage());
    if (!page)
        return 0;
    return &(page->logString());
}

