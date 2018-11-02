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
# include <QFontDatabase>
# include <QFontDialog>
# include <QCheckBox>
# include <QLabel>
# include <QSpinBox>

/* GUI includes: */
# include "QIToolButton.h"
# include "UIIconPool.h"
# include "UIGuestControlFileManager.h"
# include "UIGuestControlFileManagerSettingsPanel.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIGuestControlFileManagerSettingsPanel::UIGuestControlFileManagerSettingsPanel(UIGuestControlFileManager *pManagerWidget,
                                                                               QWidget *pParent, UIGuestControlFileManagerSettings *pFileManagerSettings)
    : UIGuestControlFileManagerPanel(pManagerWidget, pParent)
    , m_pListDirectoriesOnTopCheckBox(0)
    , m_pDeleteConfirmationCheckBox(0)
    , m_pFileManagerSettings(pFileManagerSettings)
{
    prepare();
}

QString UIGuestControlFileManagerSettingsPanel::panelName() const
{
    return "SettingsPanel";
}

void UIGuestControlFileManagerSettingsPanel::prepareWidgets()
{
    if (!mainLayout())
        return;

    m_pListDirectoriesOnTopCheckBox = new QCheckBox;
    if (m_pListDirectoriesOnTopCheckBox)
    {
        m_pListDirectoriesOnTopCheckBox->setChecked(true);
        mainLayout()->addWidget(m_pListDirectoriesOnTopCheckBox, 0, Qt::AlignLeft);
    }

    m_pDeleteConfirmationCheckBox = new QCheckBox;
    if (m_pDeleteConfirmationCheckBox)
    {
        m_pDeleteConfirmationCheckBox->setChecked(false);
        mainLayout()->addWidget(m_pDeleteConfirmationCheckBox, 0, Qt::AlignLeft);
    }


    retranslateUi();
    mainLayout()->addStretch(2);
}

void UIGuestControlFileManagerSettingsPanel::sltListDirectoryCheckBoxToogled(bool fChecked)
{
    if (!m_pFileManagerSettings)
        return;
    m_pFileManagerSettings->bListDirectoriesOnTop = fChecked;
    emit sigListDirectoriesFirstChanged();
}

void UIGuestControlFileManagerSettingsPanel::prepareConnections()
{
    if (m_pListDirectoriesOnTopCheckBox)
        connect(m_pListDirectoriesOnTopCheckBox, &QCheckBox::toggled,
                this, &UIGuestControlFileManagerSettingsPanel::sltListDirectoryCheckBoxToogled);
}

void UIGuestControlFileManagerSettingsPanel::retranslateUi()
{
    UIGuestControlFileManagerPanel::retranslateUi();

    m_pListDirectoriesOnTopCheckBox->setText(UIGuestControlFileManager::tr("List directories on top"));
    m_pListDirectoriesOnTopCheckBox->setToolTip(UIGuestControlFileManager::tr("List directories before files"));

    m_pDeleteConfirmationCheckBox->setText(UIGuestControlFileManager::tr("Ask before delete"));
    m_pDeleteConfirmationCheckBox->setToolTip(UIGuestControlFileManager::tr("List directories before files"));
}
