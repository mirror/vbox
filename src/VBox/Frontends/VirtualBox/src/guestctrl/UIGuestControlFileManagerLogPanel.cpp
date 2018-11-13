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
# include <QMenu>
# include <QSpinBox>
# include <QTextEdit>
# include <QTime>

/* GUI includes: */
# include "QIToolButton.h"
# include "UIIconPool.h"
# include "UIGuestControlFileManager.h"
# include "UIGuestControlFileManagerLogPanel.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   UIFileManagerLogViewer definition.                                                                                   *
*********************************************************************************************************************************/

class UIFileManagerLogViewer : public QTextEdit
{

    Q_OBJECT;

public:

    UIFileManagerLogViewer(QWidget *pParent = 0);

protected:

    virtual void contextMenuEvent(QContextMenuEvent * event) /* override */;

private slots:

    void sltClear();
};

/*********************************************************************************************************************************
*   UIFileManagerLogViewer implementation.                                                                                   *
*********************************************************************************************************************************/

UIFileManagerLogViewer::UIFileManagerLogViewer(QWidget *pParent /* = 0 */)
    :QTextEdit(pParent)
{
    setUndoRedoEnabled(false);
}

void UIFileManagerLogViewer::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();
    void sltClear();

    QAction *pClearAction = menu->addAction(tr("Clear"));
    connect(pClearAction, &QAction::triggered, this, &UIFileManagerLogViewer::sltClear);
    menu->exec(event->globalPos());
    delete menu;
}

void UIFileManagerLogViewer::sltClear()
{
    clear();
}


/*********************************************************************************************************************************
*   UIGuestControlFileManagerLogPanel implementation.                                                                            *
*********************************************************************************************************************************/

UIGuestControlFileManagerLogPanel::UIGuestControlFileManagerLogPanel(UIGuestControlFileManager *pManagerWidget, QWidget *pParent)
    : UIGuestControlFileManagerPanel(pManagerWidget, pParent)
    , m_pLogTextEdit(0)
{
    prepare();
}

void UIGuestControlFileManagerLogPanel::appendLog(const QString &strLog, FileManagerLogType eLogType)
{
    if (!m_pLogTextEdit)
        return;
    QString strColorTag("<font color=\"Black\">");
    if (eLogType == FileManagerLogType_Error)
    {
        strColorTag = "<font color=\"Red\">";
    }
    QString strColoredLog = QString("%1 %2: %3 %4").arg(strColorTag).arg(QTime::currentTime().toString("hh:mm:ss")).arg(strLog).arg("</font>");
    m_pLogTextEdit->append(strColoredLog);
}

QString UIGuestControlFileManagerLogPanel::panelName() const
{
    return "LogPanel";
}

void UIGuestControlFileManagerLogPanel::prepareWidgets()
{
    if (!mainLayout())
        return;
    m_pLogTextEdit = new UIFileManagerLogViewer;
    if (m_pLogTextEdit)
    {
        mainLayout()->addWidget(m_pLogTextEdit);
    }
}

void UIGuestControlFileManagerLogPanel::prepareConnections()
{
}

void UIGuestControlFileManagerLogPanel::retranslateUi()
{
    UIGuestControlFileManagerPanel::retranslateUi();

}


#include "UIGuestControlFileManagerLogPanel.moc"
