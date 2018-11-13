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
# include <QTableWidget>

/* GUI includes: */
# include "UIGuestControlFileManager.h"
# include "UIGuestControlFileManagerOperationsPanel.h"
# include "UIProgressEventHandler.h"

/* COM includes: */
# include "CProgress.h"


#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   UIFileOperationProgressWidget definition.                                                                                    *
*********************************************************************************************************************************/

class UIFileOperationProgressWidget : public QWidget
{

    Q_OBJECT;

public:

    UIFileOperationProgressWidget(const CProgress &comProgress, QWidget *pParent = 0);
    ~UIFileOperationProgressWidget();

private slots:

    void sltHandleProgressPercentageChange(const QUuid &uProgressId, const int iPercent);
    void sltHandleProgressTaskComplete(const QUuid &uProgressId);

private:

    void prepare();
    void prepareWidgets();
    void prepareEventHandler();
    void cleanupEventHandler();

    CProgress               m_comProgress;
    UIProgressEventHandler *m_pEventHandler;
    QHBoxLayout            *m_pMainLayout;
    QLabel                 *m_pPercentageLabel;
};


/*********************************************************************************************************************************
*   UIFileOperationProgressWidget implementation.                                                                                *
*********************************************************************************************************************************/

UIFileOperationProgressWidget::UIFileOperationProgressWidget(const CProgress &comProgress, QWidget *pParent /* = 0 */)
    :QWidget(pParent)
    , m_comProgress(comProgress)
    , m_pEventHandler(0)
    , m_pMainLayout(0)
    , m_pPercentageLabel(0)
{
    prepare();
}

UIFileOperationProgressWidget::~UIFileOperationProgressWidget()
{
    cleanupEventHandler();
}

void UIFileOperationProgressWidget::prepare()
{
    prepareWidgets();
    prepareEventHandler();
}

void UIFileOperationProgressWidget::prepareWidgets()
{
    m_pMainLayout = new QHBoxLayout;
    if (m_pMainLayout)
    {
        m_pPercentageLabel = new QLabel;
        m_pPercentageLabel->setText("0asdsad");
        m_pMainLayout->addWidget(m_pPercentageLabel);
    }
    setLayout(m_pMainLayout);
}

void UIFileOperationProgressWidget::prepareEventHandler()
{
    m_pEventHandler = new UIProgressEventHandler(this, m_comProgress);
    connect(m_pEventHandler, &UIProgressEventHandler::sigProgressPercentageChange,
            this, &UIFileOperationProgressWidget::sltHandleProgressPercentageChange);
    connect(m_pEventHandler, &UIProgressEventHandler::sigProgressTaskComplete,
            this, &UIFileOperationProgressWidget::sltHandleProgressTaskComplete);
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
}

void UIFileOperationProgressWidget::sltHandleProgressTaskComplete(const QUuid &uProgressId)
{
    Q_UNUSED(uProgressId);
}


/*********************************************************************************************************************************
*   UIGuestControlFileManagerOperationsPanel implementation.                                                                     *
*********************************************************************************************************************************/

UIGuestControlFileManagerOperationsPanel::UIGuestControlFileManagerOperationsPanel(UIGuestControlFileManager *pManagerWidget,
                                                                                   QWidget *pParent)
    : UIGuestControlFileManagerPanel(pManagerWidget, pParent)
    , m_pTableWidget(0)
    , m_pOperationsWidget(0)
{
    prepare();
}

void UIGuestControlFileManagerOperationsPanel::addNewProgress(const CProgress &comProgress)
{
    if (!m_pTableWidget)
        return;

    m_pTableWidget->setRowCount(m_pTableWidget->rowCount() + 1);
    m_pTableWidget->setCellWidget(m_pTableWidget->rowCount() - 1, 0, new UIFileOperationProgressWidget(comProgress));
}

QString UIGuestControlFileManagerOperationsPanel::panelName() const
{
    return "OperationsPanel";
}

void UIGuestControlFileManagerOperationsPanel::prepareWidgets()
{
    if (!mainLayout())
        return;
    m_pTableWidget = new QTableWidget();

    if (m_pTableWidget)
    {
        m_pTableWidget->setColumnCount(1);
        mainLayout()->addWidget(m_pTableWidget);
    }
}

void UIGuestControlFileManagerOperationsPanel::prepareConnections()
{
}

void UIGuestControlFileManagerOperationsPanel::retranslateUi()
{
    UIGuestControlFileManagerPanel::retranslateUi();

}

#include "UIGuestControlFileManagerOperationsPanel.moc"
