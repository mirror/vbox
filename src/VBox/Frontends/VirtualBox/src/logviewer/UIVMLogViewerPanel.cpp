/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
 */

/*
 * Copyright (C) 2010-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QToolButton>

/* GUI includes: */
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UIVMLogPage.h"
#include "UIVMLogViewerPanel.h"
#include "UIVMLogViewerWidget.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif


UIVMLogViewerPanel::UIVMLogViewerPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer)
    : UIDialogPanel(pParent)
    , m_pViewer(pViewer)
{
}

void UIVMLogViewerPanel::retranslateUi()
{
}

UIVMLogViewerWidget* UIVMLogViewerPanel::viewer()
{
    return m_pViewer;
}

const UIVMLogViewerWidget* UIVMLogViewerPanel::viewer() const
{
    return m_pViewer;
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
