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
    , m_pHumanReabableSizesCheckBox(0)
    , m_pFileManagerSettings(pFileManagerSettings)
{
    prepare();
}

QString UIGuestControlFileManagerSettingsPanel::panelName() const
{
    return "SettingsPanel";
}

void UIGuestControlFileManagerSettingsPanel::update()
{
    if (!m_pFileManagerSettings)
        return;

    if (m_pListDirectoriesOnTopCheckBox)
    {
        m_pListDirectoriesOnTopCheckBox->blockSignals(true);
        m_pListDirectoriesOnTopCheckBox->setChecked(m_pFileManagerSettings->bListDirectoriesOnTop);
        m_pListDirectoriesOnTopCheckBox->blockSignals(false);
    }

    if (m_pDeleteConfirmationCheckBox)
    {
        m_pDeleteConfirmationCheckBox->blockSignals(true);
        m_pDeleteConfirmationCheckBox->setChecked(m_pFileManagerSettings->bAskDeleteConfirmation);
        m_pDeleteConfirmationCheckBox->blockSignals(false);
    }

    if (m_pHumanReabableSizesCheckBox)
    {
        m_pHumanReabableSizesCheckBox->blockSignals(true);
        m_pHumanReabableSizesCheckBox->setChecked(m_pFileManagerSettings->bShowHumanReadableSizes);
        m_pHumanReabableSizesCheckBox->blockSignals(false);
    }
}

void UIGuestControlFileManagerSettingsPanel::prepareWidgets()
{
    if (!mainLayout())
        return;

    m_pListDirectoriesOnTopCheckBox = new QCheckBox;
    if (m_pListDirectoriesOnTopCheckBox)
    {
        mainLayout()->addWidget(m_pListDirectoriesOnTopCheckBox, 0, Qt::AlignLeft);
    }

    m_pDeleteConfirmationCheckBox = new QCheckBox;
    if (m_pDeleteConfirmationCheckBox)
    {
        mainLayout()->addWidget(m_pDeleteConfirmationCheckBox, 0, Qt::AlignLeft);
    }

    m_pHumanReabableSizesCheckBox = new QCheckBox;
    if (m_pHumanReabableSizesCheckBox)
    {
        mainLayout()->addWidget(m_pHumanReabableSizesCheckBox, 0, Qt::AlignLeft);
    }
    /* Set initial checkbox status wrt. settings: */
    if (m_pFileManagerSettings)
    {
        if (m_pListDirectoriesOnTopCheckBox)
            m_pListDirectoriesOnTopCheckBox->setChecked(m_pFileManagerSettings->bListDirectoriesOnTop);
        if (m_pDeleteConfirmationCheckBox)
            m_pDeleteConfirmationCheckBox->setChecked(m_pFileManagerSettings->bAskDeleteConfirmation);
        if (m_pHumanReabableSizesCheckBox)
            m_pHumanReabableSizesCheckBox->setChecked(m_pFileManagerSettings->bShowHumanReadableSizes);
    }
    retranslateUi();
    mainLayout()->addStretch(2);
}

void UIGuestControlFileManagerSettingsPanel::sltListDirectoryCheckBoxToogled(bool bChecked)
{
    if (!m_pFileManagerSettings)
        return;
    m_pFileManagerSettings->bListDirectoriesOnTop = bChecked;
    emit sigSettingsChanged();
}

void UIGuestControlFileManagerSettingsPanel::sltDeleteConfirmationCheckBoxToogled(bool bChecked)
{
    if (!m_pFileManagerSettings)
        return;
    m_pFileManagerSettings->bAskDeleteConfirmation = bChecked;
    emit sigSettingsChanged();
}

void UIGuestControlFileManagerSettingsPanel::sltHumanReabableSizesCheckBoxToogled(bool bChecked)
{
    if (!m_pFileManagerSettings)
        return;
    m_pFileManagerSettings->bShowHumanReadableSizes = bChecked;
    emit sigSettingsChanged();
}

void UIGuestControlFileManagerSettingsPanel::prepareConnections()
{
    if (m_pListDirectoriesOnTopCheckBox)
        connect(m_pListDirectoriesOnTopCheckBox, &QCheckBox::toggled,
                this, &UIGuestControlFileManagerSettingsPanel::sltListDirectoryCheckBoxToogled);
    if (m_pDeleteConfirmationCheckBox)
        connect(m_pDeleteConfirmationCheckBox, &QCheckBox::toggled,
                this, &UIGuestControlFileManagerSettingsPanel::sltDeleteConfirmationCheckBoxToogled);
    if (m_pHumanReabableSizesCheckBox)
        connect(m_pHumanReabableSizesCheckBox, &QCheckBox::toggled,
                this, &UIGuestControlFileManagerSettingsPanel::sltHumanReabableSizesCheckBoxToogled);
}

void UIGuestControlFileManagerSettingsPanel::retranslateUi()
{
    UIGuestControlFileManagerPanel::retranslateUi();
    if (m_pListDirectoriesOnTopCheckBox)
    {
        m_pListDirectoriesOnTopCheckBox->setText(UIGuestControlFileManager::tr("List directories on top"));
        m_pListDirectoriesOnTopCheckBox->setToolTip(UIGuestControlFileManager::tr("List directories before files"));
    }

    if (m_pDeleteConfirmationCheckBox)
    {
        m_pDeleteConfirmationCheckBox->setText(UIGuestControlFileManager::tr("Ask before delete"));
        m_pDeleteConfirmationCheckBox->setToolTip(UIGuestControlFileManager::tr("Show a confirmation dialog "
                                                                                "before deleting files and directories"));
    }

    if (m_pHumanReabableSizesCheckBox)
    {
        m_pHumanReabableSizesCheckBox->setText(UIGuestControlFileManager::tr("Human readable sizes"));
        m_pHumanReabableSizesCheckBox->setToolTip(UIGuestControlFileManager::tr("Show file/directory sizes in human "
                                                                                "readable format rather than in bytes"));
    }
}
