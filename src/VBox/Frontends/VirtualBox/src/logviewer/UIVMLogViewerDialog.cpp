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
# include "UIVMLogViewerDialog.h"
# include "UIVMLogViewerWidget.h"

/* COM includes: */
# include "COMEnums.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/** Holds the VM Log-Viewer array. */
VMLogViewerMap UIVMLogViewerDialog::m_viewers = VMLogViewerMap();


UIVMLogViewerDialog::UIVMLogViewerDialog(QWidget *pParent, const CMachine &machine)
    : QIWithRetranslateUI<QIDialog>(pParent)
    , m_strMachineUUID(machine.GetHardwareUUID())
{
    prepare(machine);
}

UIVMLogViewerDialog::~UIVMLogViewerDialog()
{
    cleanup();
}

void UIVMLogViewerDialog::showLogViewerFor(QWidget* parent, const CMachine &machine)
{
    if (m_viewers.contains(machine.GetHardwareUUID()))
    {
        showLogViewerDialog(m_viewers[machine.GetHardwareUUID()]);
        return;
    }
    UIVMLogViewerDialog dialog(parent, machine);
    dialog.exec();
}

void UIVMLogViewerDialog::retranslateUi()
{
    m_pCloseButton->setText(UIVMLogViewerWidget::tr("Close"));
}

void UIVMLogViewerDialog::prepare(const CMachine& machine)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    /* According to QDoc passing widget to layout's ctor also handles setLayout(..) call: */
    m_pMainLayout = new QVBoxLayout(this);

    UIVMLogViewerWidget *viewerWidget = new UIVMLogViewerWidget(this, machine);
    m_pMainLayout->addWidget(viewerWidget);

    m_pButtonBox = new QDialogButtonBox(this);
    m_pCloseButton = m_pButtonBox->addButton("Close", QDialogButtonBox::RejectRole);
    m_pMainLayout->addWidget(m_pButtonBox);
    connect(m_pCloseButton, &QPushButton::clicked, this, &UIVMLogViewerDialog::close);

    m_viewers[m_strMachineUUID] = this;

    /* Load settings: */
    loadSettings();

    showLogViewerDialog(this);
}

void UIVMLogViewerDialog::showLogViewerDialog(UIVMLogViewerDialog *logViewerDialog)
{
    logViewerDialog->show();
    logViewerDialog->raise();
    logViewerDialog->setWindowState(logViewerDialog->windowState() & ~Qt::WindowMinimized);
    logViewerDialog->activateWindow();
}

void UIVMLogViewerDialog::cleanup()
{
    saveSettings();
    /* Remove log-viewer: */
    if (!m_strMachineUUID.isNull())
        m_viewers.remove(m_strMachineUUID);
}

void UIVMLogViewerDialog::loadSettings()
{
}

void UIVMLogViewerDialog::saveSettings()
{
}
