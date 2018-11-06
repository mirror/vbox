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
# include <QHBoxLayout>
# include <QTableWidget>

/* GUI includes: */
# include "UIGuestControlFileManager.h"
# include "UIGuestControlFileManagerOperationsPanel.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   UIFileOperationProgressWidget definition.                                                                                    *
*********************************************************************************************************************************/

class UIFileOperationProgressWidget : public QWidget
{

    Q_OBJECT;

public:

    UIFileOperationProgressWidget(QWidget *pParent = 0);

};


/*********************************************************************************************************************************
*   UIFileOperationProgressWidget implementation.                                                                                *
*********************************************************************************************************************************/

UIFileOperationProgressWidget::UIFileOperationProgressWidget(QWidget *pParent /* = 0 */)
    :QWidget(pParent)
{
}


/*********************************************************************************************************************************
*   UIGuestControlFileManagerOperationsPanel implementation.                                                                     *
*********************************************************************************************************************************/

UIGuestControlFileManagerOperationsPanel::UIGuestControlFileManagerOperationsPanel(UIGuestControlFileManager *pManagerWidget, QWidget *pParent)
    : UIGuestControlFileManagerPanel(pManagerWidget, pParent)
    , m_pTableWidget(0)
{
    prepare();
}


QString UIGuestControlFileManagerOperationsPanel::panelName() const
{
    return "OperationsPanel";
}

void UIGuestControlFileManagerOperationsPanel::prepareWidgets()
{
    if (!mainLayout())
        return;
    m_pTableWidget = new QTableWidget;
    if (m_pTableWidget)
    {
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
