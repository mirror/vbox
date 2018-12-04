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
# include <QGridLayout>
# include <QLabel>
# include <QMenu>
# include <QProgressBar>
# include <QScrollArea>
# include <QScrollBar>
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
    void sigFocusIn(QWidget *pWidget);
    void sigFocusOut(QWidget *pWidget);

protected:

    virtual void retranslateUi() /* override */;
    virtual void focusInEvent(QFocusEvent *pEvent) /* override */;
    virtual void focusOutEvent(QFocusEvent *pEvent) /* override */;

private slots:

    void sltHandleProgressPercentageChange(const QUuid &uProgressId, const int iPercent);
    void sltHandleProgressComplete(const QUuid &uProgressId);
    void sltCancelProgress();

private:
    enum OperationStatus
    {
        OperationStatus_NotStarted,
        OperationStatus_Working,
        OperationStatus_Paused,
        OperationStatus_Canceled,
        OperationStatus_Succeded,
        OperationStatus_Failed,
        OperationStatus_Invalid,
        OperationStatus_Max

    };
    void prepare();
    void prepareWidgets();
    void prepareEventHandler();
    void cleanupEventHandler();

    OperationStatus         m_eStatus;
    CProgress               m_comProgress;
    UIProgressEventHandler *m_pEventHandler;
    QGridLayout            *m_pMainLayout;
    QProgressBar           *m_pProgressBar;
    QIToolButton           *m_pCancelButton;
    QILabel                *m_pStatusLabel;
    QILabel                *m_pPercentageLabel;
};


/*********************************************************************************************************************************
*   UIFileOperationProgressWidget implementation.                                                                                *
*********************************************************************************************************************************/

UIFileOperationProgressWidget::UIFileOperationProgressWidget(const CProgress &comProgress, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QFrame>(pParent)
    , m_eStatus(OperationStatus_NotStarted)
    , m_comProgress(comProgress)
    , m_pEventHandler(0)
    , m_pMainLayout(0)
    , m_pProgressBar(0)
    , m_pCancelButton(0)
    , m_pStatusLabel(0)
    , m_pPercentageLabel(0)
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
        m_pCancelButton->setToolTip(UIGuestControlFileManager::tr("Cancel"));

    switch (m_eStatus)
    {
        case OperationStatus_NotStarted:
            m_pStatusLabel->setText(UIGuestControlFileManager::tr("Not yet started"));
            break;
        case OperationStatus_Working:
            m_pStatusLabel->setText(UIGuestControlFileManager::tr("Working"));
            break;
        case OperationStatus_Paused:
            m_pStatusLabel->setText(UIGuestControlFileManager::tr("Paused"));
            break;
        case OperationStatus_Canceled:
            m_pStatusLabel->setText(UIGuestControlFileManager::tr("Canceled"));
            break;
        case OperationStatus_Succeded:
            m_pStatusLabel->setText(UIGuestControlFileManager::tr("Succeded"));
            break;
        case OperationStatus_Failed:
            m_pStatusLabel->setText(UIGuestControlFileManager::tr("Failed"));
            break;
        case OperationStatus_Invalid:
        case OperationStatus_Max:
        default:
            m_pStatusLabel->setText(UIGuestControlFileManager::tr("Invalid"));
            break;
    }
}

void UIFileOperationProgressWidget::focusInEvent(QFocusEvent *pEvent)
{
    QFrame::focusInEvent(pEvent);
    emit sigFocusIn(this);
}

void UIFileOperationProgressWidget::focusOutEvent(QFocusEvent *pEvent)
{
    QFrame::focusOutEvent(pEvent);
    emit sigFocusOut(this);
}

void UIFileOperationProgressWidget::prepare()
{
    prepareWidgets();
    prepareEventHandler();
    retranslateUi();
}

void UIFileOperationProgressWidget::prepareWidgets()
{
    m_pMainLayout = new QGridLayout;
    if (!m_pMainLayout)
        return;
    //m_pMainLayout->setSpacing(0);

    m_pProgressBar = new QProgressBar;
    if (m_pProgressBar)
    {
        m_pProgressBar->setMinimum(0);
        m_pProgressBar->setMaximum(100);
        /* Hide the QProgressBar's text since in MacOS it never shows: */
        m_pProgressBar->setTextVisible(false);
        m_pMainLayout->addWidget(m_pProgressBar, 0, 0, 1, 2);
    }

    m_pCancelButton = new QIToolButton;
    if (m_pCancelButton)
    {
        m_pCancelButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DockWidgetCloseButton));
        connect(m_pCancelButton, &QIToolButton::clicked, this, &UIFileOperationProgressWidget::sltCancelProgress);
        if (!m_comProgress.isNull() && !m_comProgress.GetCancelable())
            m_pCancelButton->setEnabled(false);
        m_pMainLayout->addWidget(m_pCancelButton, 0, 2, 1, 1);
    }

    m_pStatusLabel = new QILabel;
    if (m_pStatusLabel)
    {
        m_pStatusLabel->setContextMenuPolicy(Qt::NoContextMenu);
        m_pMainLayout->addWidget(m_pStatusLabel, 0, 3, 1, 1);
    }

    m_pPercentageLabel = new QILabel;
    if (m_pPercentageLabel)
    {
        m_pPercentageLabel->setContextMenuPolicy(Qt::NoContextMenu);
        m_pMainLayout->addWidget(m_pPercentageLabel, 1, 0, 1, 4);
    }

    setLayout(m_pMainLayout);
    retranslateUi();
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
    m_eStatus = OperationStatus_Working;
    retranslateUi();
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
    if (m_pPercentageLabel)
        m_pPercentageLabel->setText(QString("%1%").arg(QString::number(iPercent)));
}

void UIFileOperationProgressWidget::sltHandleProgressComplete(const QUuid &uProgressId)
{
    Q_UNUSED(uProgressId);
    if (m_pCancelButton)
        m_pCancelButton->setEnabled(false);

    if (!m_comProgress.isOk() || m_comProgress.GetResultCode() != 0)
    {
        emit sigProgressFail(UIErrorString::formatErrorInfo(m_comProgress), FileManagerLogType_Error);
        m_eStatus = OperationStatus_Failed;
    }
    else
    {
        emit sigProgressComplete(m_comProgress.GetId());
        m_eStatus = OperationStatus_Succeded;
    }
    if (m_pProgressBar)
        m_pProgressBar->setEnabled(false);
    retranslateUi();
}

void UIFileOperationProgressWidget::sltCancelProgress()
{
    m_comProgress.Cancel();
    /* Since we dont have a "progress canceled" event we have to do this here: */
    if (m_pCancelButton)
        m_pCancelButton->setEnabled(false);
    if (m_pProgressBar)
        m_pProgressBar->setEnabled(false);
    m_eStatus = OperationStatus_Canceled;
    retranslateUi();
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
    , m_pWidgetInFocus(0)
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
    m_widgetSet.insert(pOperationsWidget);
    m_pContainerLayout->insertWidget(m_pContainerLayout->count() - 1, pOperationsWidget);

    connect(pOperationsWidget, &UIFileOperationProgressWidget::sigProgressComplete,
            this, &UIGuestControlFileManagerOperationsPanel::sigFileOperationComplete);
    connect(pOperationsWidget, &UIFileOperationProgressWidget::sigProgressFail,
            this, &UIGuestControlFileManagerOperationsPanel::sigFileOperationFail);

    connect(pOperationsWidget, &UIFileOperationProgressWidget::sigFocusIn,
            this, &UIGuestControlFileManagerOperationsPanel::sltHandleWidgetFocusIn);
    connect(pOperationsWidget, &UIFileOperationProgressWidget::sigFocusOut,
            this, &UIGuestControlFileManagerOperationsPanel::sltHandleWidgetFocusOut);
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

    QScrollBar *pVerticalScrollBar = m_pScrollArea->verticalScrollBar();
    if (pVerticalScrollBar)
        QObject::connect(pVerticalScrollBar, &QScrollBar::rangeChanged, this, &UIGuestControlFileManagerOperationsPanel::sltScrollToBottom);

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

    if (m_pWidgetInFocus)
    {
        QAction *pRemoveSelected = menu->addAction(UIGuestControlFileManager::tr("Remove Selected"));
        connect(pRemoveSelected, &QAction::triggered,
                this, &UIGuestControlFileManagerOperationsPanel::sltRemoveSelected);
    }

    QAction *pRemoveFinished = menu->addAction(UIGuestControlFileManager::tr("Remove Finished"));
    QAction *pRemoveAll = menu->addAction(UIGuestControlFileManager::tr("Remove All"));

    connect(pRemoveFinished, &QAction::triggered,
            this, &UIGuestControlFileManagerOperationsPanel::sltRemoveFinished);
    connect(pRemoveAll, &QAction::triggered,
            this, &UIGuestControlFileManagerOperationsPanel::sltRemoveAll);

    menu->exec(pEvent->globalPos());
    delete menu;
}

void UIGuestControlFileManagerOperationsPanel::sltRemoveFinished()
{
    QList<UIFileOperationProgressWidget*> widgetsToRemove;
    foreach (QWidget *pWidget, m_widgetSet)
    {
        UIFileOperationProgressWidget *pProgressWidget = qobject_cast<UIFileOperationProgressWidget*>(pWidget);
        if (pProgressWidget && pProgressWidget->isCompleted())
        {
            delete pProgressWidget;
            widgetsToRemove << pProgressWidget;
        }
    }
    foreach (UIFileOperationProgressWidget *pWidget, widgetsToRemove)
        m_widgetSet.remove(pWidget);
}

void UIGuestControlFileManagerOperationsPanel::sltRemoveAll()
{
    foreach (QWidget *pWidget, m_widgetSet)
    {
        if (pWidget)
        {
            delete pWidget;
        }
    }
    m_widgetSet.clear();
}

void UIGuestControlFileManagerOperationsPanel::sltRemoveSelected()
{
    if (!m_pWidgetInFocus)
        return;
    delete m_pWidgetInFocus;
    m_widgetSet.remove(m_pWidgetInFocus);
}

void UIGuestControlFileManagerOperationsPanel::sltHandleWidgetFocusIn(QWidget *pWidget)
{
    if (!pWidget)
        return;
    m_pWidgetInFocus = pWidget;
}

void UIGuestControlFileManagerOperationsPanel::sltHandleWidgetFocusOut(QWidget *pWidget)
{
    if (!pWidget)
        return;
    m_pWidgetInFocus = 0;
}

void UIGuestControlFileManagerOperationsPanel::sltScrollToBottom(int iMin, int iMax)
{
    Q_UNUSED(iMin);
    if (m_pScrollArea)
    m_pScrollArea->verticalScrollBar()->setValue(iMax);
}

#include "UIGuestControlFileManagerOperationsPanel.moc"
