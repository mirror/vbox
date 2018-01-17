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
# include <QScrollBar>
# include <QPlainTextEdit>
# include <QPushButton>
# include <QVBoxLayout>

/* GUI includes: */
# include "UIDesktopWidgetWatchdog.h"
# include "UIExtraDataManager.h"
# include "UIIconPool.h"
# include "UIVMLogViewerDialog.h"
# include "UIVMLogViewerWidget.h"
# include "VBoxGlobal.h"
# ifdef VBOX_WS_MAC
#  include "VBoxUtils-darwin.h"
# endif /* VBOX_WS_MAC */

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
    button(ButtonType_Close)->setShortcut(Qt::Key_Escape);
    /* Setup a dialog caption: */
    if (!m_comMachine.isNull())
        setWindowTitle(tr("%1 - Log Viewer").arg(m_comMachine.GetName()));
    else
        setWindowTitle(UIVMLogViewerWidget::tr("Log Viewer"));
}

void UIVMLogViewerDialog::configureCentralWidget()
{
    /* Create widget: */
    UIVMLogViewerWidget *pWidget = new UIVMLogViewerWidget(EmbedTo_Dialog, this, m_comMachine);
    AssertPtrReturnVoid(pWidget);
    {
        /* Configure widget: */
        setWidget(pWidget);
        setWidgetMenu(pWidget->menu());
#ifdef VBOX_WS_MAC
        setWidgetToolbar(pWidget->toolbar());
#endif
        /* Add into layout: */
        centralWidget()->layout()->addWidget(pWidget);
    }
}

void UIVMLogViewerDialog::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/vm_show_logs_32px.png", ":/vm_show_logs_16px.png"));
}

void UIVMLogViewerDialog::finalize()
{
    retranslateUi();
}

void UIVMLogViewerDialog::loadSettings()
{
    const UIVMLogViewerWidget *pWidget = qobject_cast<const UIVMLogViewerWidget *>(widget());

    /* Restore window geometry: */
    /* Getting available geometry to calculate default geometry: */
    const QRect desktopRect = gpDesktop->availableGeometry(this);
    int iDefaultWidth = desktopRect.width() / 2;
    int iDefaultHeight = desktopRect.height() * 3 / 4;

    /* Try obtain the default width of the current logviewer */
    if (pWidget)
    {
        int width =  pWidget->defaultLogPageWidth();
        if (width != 0)
            iDefaultWidth = width;
    }

    QRect defaultGeometry(0, 0, iDefaultWidth, iDefaultHeight);
    if (centerWidget())
        defaultGeometry.moveCenter(centerWidget()->geometry().center());

    /* Load geometry from extradata: */
    QRect geometry = gEDataManager->logWindowGeometry(this, defaultGeometry);

    /* Restore geometry: */
    LogRel2(("GUI: UIVMLogViewer: Restoring geometry to: Origin=%dx%d, Size=%dx%d\n",
             geometry.x(), geometry.y(), geometry.width(), geometry.height()));
    setDialogGeometry(geometry);
}

void UIVMLogViewerDialog::saveSettings() const
{
    /* Save window geometry to extradata: */
    const QRect saveGeometry = geometry();
#ifdef VBOX_WS_MAC
    /* darwinIsWindowMaximized expects a non-const QWidget*. thus const_cast: */
    QWidget *pw = const_cast<QWidget*>(qobject_cast<const QWidget*>(this));
    gEDataManager->setLogWindowGeometry(saveGeometry, ::darwinIsWindowMaximized(pw));
#else /* !VBOX_WS_MAC */
    gEDataManager->setLogWindowGeometry(saveGeometry, isMaximized());
#endif /* !VBOX_WS_MAC */
    LogRel2(("GUI: UIVMLogViewer: Geometry saved as: Origin=%dx%d, Size=%dx%d\n",
             saveGeometry.x(), saveGeometry.y(), saveGeometry.width(), saveGeometry.height()));
}

bool UIVMLogViewerDialog::shouldBeMaximized() const
{
    return gEDataManager->logWindowShouldBeMaximized();
}
