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
# if defined(RT_OS_SOLARIS)
#  include <QFontDatabase>
# endif
# include <QDialogButtonBox>
# include <QKeyEvent>
# include <QLabel>
# include <QPushButton>
# include <QScrollBar>
# include <QTextEdit>
# include <QVBoxLayout>

/* GUI includes: */
# include "UIIconPool.h"
# include "UIVMLogViewerDialog.h"
# include "UIVMLogViewerWidget.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIVMLogViewerDialogFactory::UIVMLogViewerDialogFactory(const CMachine &machine)
    :m_comMachine(machine)
{
}

void UIVMLogViewerDialogFactory::create(QIManagerDialog *&pDialog, QWidget *pCenterWidget)
{
    pDialog = new UIVMLogViewerDialog(pCenterWidget, m_comMachine);
}

UIVMLogViewerDialog::UIVMLogViewerDialog(QWidget *pCenterWidget, const CMachine &machine)
    : QIWithRetranslateUI<QIManagerDialog>(pCenterWidget)
    , m_comMachine(machine)
{
}

void UIVMLogViewerDialog::retranslateUi()
{
    button(ButtonType_Close)->setText(UIVMLogViewerWidget::tr("Close"));
}

void UIVMLogViewerDialog::configureCentralWidget()
{
    /* Create widget: */
    UIVMLogViewerWidget *pWidget = new UIVMLogViewerWidget(this, m_comMachine);
    AssertPtrReturnVoid(pWidget);
    {
        /* Configure widget: */
        setWidget(pWidget);
        //setWidgetMenu(pWidget->menu());
#ifdef VBOX_WS_MAC
        //setWidgetToolbar(pWidget->toolbar());
#endif
        /* Add into layout: */
        centralWidget()->layout()->addWidget(pWidget);
    }
}

void UIVMLogViewerDialog::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/diskimage_32px.png", ":/diskimage_16px.png"));
}
