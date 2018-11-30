/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
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
# include <QHBoxLayout>
# include <QLabel>
# include <QMenu>
# include <QProgressBar>
# include <QScrollArea>
# include <QStyle>


/* GUI includes: */
# include "QIToolButton.h"
# include "QILabel.h"
# include "UIErrorString.h"
# include "UIIconPool.h"
# include "UIGuestControlFileManager.h"
# include "UIGuestControlFileManagerOperationsPanel.h"
# include "UIProgressEventHandler.h"

/* COM includes: */
# include "CProgress.h"


#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   UIFileOperationProgressWidget definition.                                                                                    *
*********************************************************************************************************************************/

class UIFileOperationProgressWidget : public QIWithRetranslateUI<QFrame>
{

    Q_OBJECT;

public:

    UIFileOperationProgressWidget(const CProgress &comProgress, QWidget *pParent = 0);
    ~UIFileOperationProgressWidget();
    bool isCompleted() const;
    bool isCanceled() const;

signals:

    void sigProgressComplete(QUuid progressId);
    void sigProgressFail(QString strErrorString, FileManagerLogType eLogType);

protected:

    virtual void retranslateUi() /* override */;
    // virtual void focusInEvent(QFocusEvent *pEvent) /* override */;
    // virtual void focusOutEvent(QFocusEvent *pEvent) /* override */;

private slots:

    void sltHandleProgressPercentageChange(const QUuid &uProgressId, const int iPercent);
    void sltHandleProgressComplete(const QUuid &uProgressId);
    void sltCancelProgress();

private:

    void prepare();
    void prepareWidgets();
    void prepareEventHandler();
    void cleanupEventHandler();

    CProgress               m_comProgress;
    UIProgressEventHandler *m_pEventHandler;
    QHBoxLayout            *m_pMainLayout;
    QProgressBar           *m_pProgressBar;
    QIToolButton           *m_pCancelButton;
    QILabel                *m_pStatusLabel;
};


/*********************************************************************************************************************************
*   UIFileOperationProgressWidget implementation.                                                                                *
*********************************************************************************************************************************/

UIFileOperationProgressWidget::UIFileOperationProgressWidget(const CProgress &comProgress, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QFrame>(pParent)
    , m_comProgress(comProgress)
    , m_pEventHandler(0)
    , m_pMainLayout(0)
    , m_pProgressBar(0)
    , m_pCancelButton(0)
    , m_pStatusLabel(0)
{
    prepare();
    setFocusPolicy(Qt::ClickFocus);
    setStyleSheet("QFrame:focus {  border-width: 1px; border-style: dashed; border-color: black; }");
}

UIFileOperationProgressWidget::~UIFileOperationProgressWidget()
{
    cleanupEventHandler();
}

bool UIFileOperationProgressWidget::isCompleted() const
{
    if (m_comProgress.isNull())
        return true;
    return m_comProgress.GetCompleted();
}

bool UIFileOperationProgressWidget::isCanceled() const
{
    if (m_comProgress.isNull())
        return true;
    return m_comProgress.GetCanceled();
}

void UIFileOperationProgressWidget::retranslateUi()
{
    if (m_pCancelButton)
        m_pCancelButton->setToolTip(UIGuestControlFileManager::tr("Close the pane"));
}

void UIFileOperationProgressWidget::prepare()
{
    prepareWidgets();
    prepareEventHandler();
    retranslateUi();
}

void UIFileOperationProgressWidget::prepareWidgets()
{
    m_pMainLayout = new QHBoxLayout;
    if (!m_pMainLayout)
        return;
    //m_pMainLayout->setSpacing(0);

    m_pCancelButton = new QIToolButton;
    if (m_pCancelButton)
    {
        m_pCancelButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DockWidgetCloseButton));
        connect(m_pCancelButton, &QIToolButton::clicked, this, &UIFileOperationProgressWidget::sltCancelProgress);
        if (!m_comProgress.isNull() && !m_comProgress.GetCancelable())
            m_pCancelButton->setEnabled(false);
        m_pMainLayout->addWidget(m_pCancelButton);
    }

    m_pProgressBar = new QProgressBar;
    if (m_pProgressBar)
    {
        m_pProgressBar->setMinimum(0);
        m_pProgressBar->setMaximum(100);
        m_pProgressBar->setTextVisible(true);
        m_pMainLayout->addWidget(m_pProgressBar);
    }
    m_pStatusLabel = new QILabel;
    if (m_pStatusLabel)
    {
        m_pMainLayout->addWidget(m_pStatusLabel);
        m_pStatusLabel->setText(UIGuestControlFileManager::tr("Idle"));
    }
    m_pMainLayout->addStretch();

    setLayout(m_pMainLayout);
}

void UIFileOperationProgressWidget::prepareEventHandler()
{
    if (m_comProgress.isNull())
        return;
    m_pEventHandler = new UIProgressEventHandler(this, m_comProgress);
    connect(m_pEventHandler, &UIProgressEventHandler::sigProgressPercentageChange,
            this, &UIFileOperationProgressWidget::sltHandleProgressPercentageChange);
    connect(m_pEventHandler, &UIProgressEventHandler::sigProgressTaskComplete,
            this, &UIFileOperationProgressWidget::sltHandleProgressComplete);
    m_pStatusLabel->setText(UIGuestControlFileManager::tr("Working"));
}

void UIFileOperationProgressWidget::cleanupEventHandler()
{
    delete m_pEventHandler;
    m_pEventHandler = 0;
}

void UIFileOperationProgressWidget::sltHandleProgressPercentageChange(const QUuid &uProgressId, const int iPercent)
{
    Q_UNUSED(uProgressId);
    Q_UNUSED(iPercent);
    m_pProgressBar->setValue(iPercent);

}

void UIFileOperationProgressWidget::sltHandleProgressComplete(const QUuid &uProgressId)
{
    Q_UNUSED(uProgressId);
    if (m_pCancelButton)
        m_pCancelButton->setEnabled(false);

    if (!m_comProgress.isOk() || m_comProgress.GetResultCode() != 0)
    {
        emit sigProgressFail(UIErrorString::formatErrorInfo(m_comProgress), FileManagerLogType_Error);
        if (m_pStatusLabel)
            m_pStatusLabel->setText(UIGuestControlFileManager::tr("Failed"));
    }
    else
    {
        emit sigProgressComplete(m_comProgress.GetId());
        if (m_pStatusLabel)
            m_pStatusLabel->setText(UIGuestControlFileManager::tr("Succeeded"));
    }
}

void UIFileOperationProgressWidget::sltCancelProgress()
{
    m_comProgress.Cancel();
    /* Since we dont have a "progress canceled" event we have to do this here: */
    if (m_pCancelButton)
        m_pCancelButton->setEnabled(false);
    if (m_pStatusLabel)
        m_pStatusLabel->setText(UIGuestControlFileManager::tr("Canceled"));
    if (m_pProgressBar)
        m_pProgressBar->setEnabled(false);
}


/*********************************************************************************************************************************
*   UIGuestControlFileManagerOperationsPanel implementation.                                                                     *
*********************************************************************************************************************************/

UIGuestControlFileManagerOperationsPanel::UIGuestControlFileManagerOperationsPanel(UIGuestControlFileManager *pManagerWidget,
                                                                                   QWidget *pParent)
    : UIGuestControlFileManagerPanel(pManagerWidget, pParent)
    , m_pScrollArea(0)
    , m_pContainerWidget(0)
    , m_pContainerLayout(0)
    , m_pContainerSpaceItem(0)
{
    prepare();
}

void UIGuestControlFileManagerOperationsPanel::addNewProgress(const CProgress &comProgress)
{
    if (!m_pContainerLayout)
        return;

    UIFileOperationProgressWidget *pOperationsWidget = new UIFileOperationProgressWidget(comProgress);
    if (!pOperationsWidget)
        return;
    m_pContainerLayout->insertWidget(m_pContainerLayout->count() - 1, pOperationsWidget);

    connect(pOperationsWidget, &UIFileOperationProgressWidget::sigProgressComplete,
            this, &UIGuestControlFileManagerOperationsPanel::sigFileOperationComplete);
    connect(pOperationsWidget, &UIFileOperationProgressWidget::sigProgressFail,
            this, &UIGuestControlFileManagerOperationsPanel::sigFileOperationFail);
}

QString UIGuestControlFileManagerOperationsPanel::panelName() const
{
    return "OperationsPanel";
}

void UIGuestControlFileManagerOperationsPanel::prepareWidgets()
{
    if (!mainLayout())
        return;


    m_pScrollArea = new QScrollArea;
    m_pContainerWidget = new QWidget;
    m_pContainerLayout = new QVBoxLayout;
    if (!m_pScrollArea || !m_pContainerWidget || !m_pContainerLayout)
        return;

    m_pScrollArea->setBackgroundRole(QPalette::Window);
    m_pScrollArea->setWidgetResizable(true);

    mainLayout()->addWidget(m_pScrollArea);

    m_pScrollArea->setWidget(m_pContainerWidget);
    m_pContainerWidget->setLayout(m_pContainerLayout);
    m_pContainerLayout->addStretch(4);
}

void UIGuestControlFileManagerOperationsPanel::prepareConnections()
{

}

void UIGuestControlFileManagerOperationsPanel::retranslateUi()
{
    UIGuestControlFileManagerPanel::retranslateUi();
}

void UIGuestControlFileManagerOperationsPanel::contextMenuEvent(QContextMenuEvent *pEvent)
{
    QMenu *menu = new QMenu(this);

    QAction *pCleanFinished = menu->addAction(UIGuestControlFileManager::tr("Clean Finished"));
    QAction *pCleanAll = menu->addAction(UIGuestControlFileManager::tr("Clean All"));

    connect(pCleanFinished, &QAction::triggered,
            this, &UIGuestControlFileManagerOperationsPanel::sltCleanFinished);
    connect(pCleanAll, &QAction::triggered,
            this, &UIGuestControlFileManagerOperationsPanel::sltCleanAll);

    menu->exec(pEvent->globalPos());
    delete menu;
}

void UIGuestControlFileManagerOperationsPanel::sltCleanFinished()
{
    // QList<int> listOfRowsToRemove;
    // for (int i = 0; i < m_pListView->count(); ++i)
    // {
    //     UIFileOperationProgressWidget* pProgressWidget =
    //         qobject_cast<UIFileOperationProgressWidget*>(m_pListView->itemWidget(m_pListView->item(i)));
    //     if (pProgressWidget)
    //     {
    //         if (pProgressWidget->isCanceled() || pProgressWidget->isCompleted())
    //             listOfRowsToRemove << i;
    //     }
    // }
    // foreach (int row, listOfRowsToRemove)
    // {
    //     /* This will delete the progress widget as well: */
    //     QListWidgetItem *pItem = m_pListView->takeItem(row);
    //     delete m_pListView->itemWidget(pItem);
    //     delete pItem;
    // }
}

void UIGuestControlFileManagerOperationsPanel::sltCleanAll()
{
    //m_pListView->clear();
    //m_pListView->setRowCount(0);
}

#include "UIGuestControlFileManagerOperationsPanel.moc"
