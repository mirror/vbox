/* $Id$ */
/** @file
 * VBox Qt GUI - UIFDCreationDialog class implementation.
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
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

/* Qt includes */
# include<QCheckBox>
# include<QDialogButtonBox>
# include<QDir>
# include<QGridLayout>
# include<QLabel>
# include<QPushButton>

/* GUI includes */
# include "UIFDCreationDialog.h"
# include "UIFilePathSelector.h"
# include "UIMedium.h"
# include "UIMessageCenter.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CSystemProperties.h"
# include "CMedium.h"
# include "CMediumFormat.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIFDCreationDialog::UIFDCreationDialog(QWidget *pParent /* = 0 */,
                                       const QString &strMachineName /* = QString() */,
                                       const QString &strMachineFolder /* = QString() */)
   : QIWithRetranslateUI<QDialog>(pParent)
    , m_pFilePathselector(0)
    , m_pPathLabel(0)
    , m_pSizeLabel(0)
    , m_pSizeCombo(0)
    , m_pButtonBox(0)
    , m_pFormatCheckBox(0)
    , m_strMachineName(strMachineName)
    , m_strMachineFolder(strMachineFolder)
{

    prepare();
    /* Adjust dialog size: */
    adjustSize();

#ifdef VBOX_WS_MAC
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(minimumSize());
#endif /* VBOX_WS_MAC */
}

void UIFDCreationDialog::retranslateUi()
{
    setWindowTitle(QApplication::translate("UIMediumManager", "Create a Floppy Disk"));
    if (m_pPathLabel)
        m_pPathLabel->setText(QApplication::translate("UIMediumManager", "File Path:"));
    if (m_pSizeLabel)
        m_pSizeLabel->setText(QApplication::translate("UIMediumManager", "Size:"));
    if (m_pButtonBox)
        m_pButtonBox->button(QDialogButtonBox::Ok)->setText("Create");
    if (m_pFormatCheckBox)
        m_pFormatCheckBox->setText(QApplication::translate("UIMediumManager", "Format disk as FAT12"));
    if (m_pSizeCombo)
    {
        //m_pSizeCombo->setItemText(FDSize_2_88M, QApplication::translate("UIMediumManager", "2.88M"));
        m_pSizeCombo->setItemText(FDSize_1_44M, QApplication::translate("UIMediumManager", "1.44M"));
        m_pSizeCombo->setItemText(FDSize_1_2M, QApplication::translate("UIMediumManager", "1.2M"));
        m_pSizeCombo->setItemText(FDSize_720K, QApplication::translate("UIMediumManager", "720K"));
        m_pSizeCombo->setItemText(FDSize_360K, QApplication::translate("UIMediumManager", "360K"));
    }
}

void UIFDCreationDialog::prepare()
{

#ifndef VBOX_WS_MAC
    setWindowIcon(QIcon(":/fd_add_32px.png"));
#endif

    setWindowModality(Qt::WindowModal);
    setSizeGripEnabled(false);

    QGridLayout *pMainLayout = new QGridLayout;
    if (!pMainLayout)
        return;
    setLayout(pMainLayout);

    m_pPathLabel = new QLabel;
    if (m_pPathLabel)
    {
        pMainLayout->addWidget(m_pPathLabel, 0, 0, 1, 1);
        m_pPathLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    }

    m_pFilePathselector = new UIFilePathSelector;
    if (m_pFilePathselector)
    {
        pMainLayout->addWidget(m_pFilePathselector, 0, 1, 1, 2);
        m_pFilePathselector->setMode(UIFilePathSelector::Mode_File_Save);
        QString strFolder = getDefaultFolder();
        m_pFilePathselector->setDefaultPath(strFolder);
        m_pFilePathselector->setPath(strFolder);
    }

    m_pSizeLabel = new QLabel;
    if (m_pSizeLabel)
    {
        pMainLayout->addWidget(m_pSizeLabel, 1, 0, 1, 1);
        m_pSizeLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    }

    m_pSizeCombo = new QComboBox;
    if (m_pSizeCombo)
    {
        pMainLayout->addWidget(m_pSizeCombo, 1, 1, 1, 1);
        //m_pSizeCombo->insertItem(FDSize_2_88M, "2.88M", 2949120);
        m_pSizeCombo->insertItem(FDSize_1_44M, "1.44M", 1474560);
        m_pSizeCombo->insertItem(FDSize_1_2M, "1.2M", 1228800);
        m_pSizeCombo->insertItem(FDSize_720K, "720K", 737280);
        m_pSizeCombo->insertItem(FDSize_360K, "360K", 368640);
        m_pSizeCombo->setCurrentIndex(FDSize_1_44M);

    }

    m_pFormatCheckBox = new QCheckBox;
    if (m_pFormatCheckBox)
    {
        pMainLayout->addWidget(m_pFormatCheckBox, 2, 1, 1, 1);
        m_pFormatCheckBox->setCheckState(Qt::Checked);
    }

    m_pButtonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    if (m_pButtonBox)
    {
        pMainLayout->addWidget(m_pButtonBox, 3, 0, 1, 3);
        connect(m_pButtonBox, &QDialogButtonBox::accepted, this, &UIFDCreationDialog::accept);
        connect(m_pButtonBox, &QDialogButtonBox::rejected, this, &UIFDCreationDialog::reject);
    }
    retranslateUi();
}

QString UIFDCreationDialog::getDefaultFolder() const
{
    QString strPreferredExtension = UIMediumDefs::getPreferredExtensionForMedium(KDeviceType_Floppy);

    QString strInitialPath = m_strMachineFolder;
    if (strInitialPath.isEmpty())
        strInitialPath = vboxGlobal().virtualBox().GetSystemProperties().GetDefaultMachineFolder();

    if (strInitialPath.isEmpty())
        return strInitialPath;

    strInitialPath = QDir(strInitialPath).absoluteFilePath(m_strMachineName + "." + strPreferredExtension);
    return strInitialPath;
}

void UIFDCreationDialog::accept()
{
    QVector<CMediumFormat>  mediumFormats = UIMediumDefs::getFormatsForDeviceType(KDeviceType_Floppy);

    if (m_pFilePathselector->path().isEmpty() || mediumFormats.isEmpty())
        return;

    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString strMediumLocation = m_pFilePathselector->path();

    CMedium newMedium = vbox.CreateMedium(mediumFormats[0].GetName(), strMediumLocation,
                                          KAccessMode_ReadWrite, KDeviceType_Floppy);
    if (!vbox.isOk())
    {
        msgCenter().cannotCreateMediumStorage(vbox, strMediumLocation, this);
        return;
    }

    QVector<KMediumVariant> variants(1, KMediumVariant_Fixed);
    /* Decide if formatting the disk is required: */
    if (m_pFormatCheckBox && m_pFormatCheckBox->checkState() == Qt::Checked)
        variants.push_back(KMediumVariant_Formatted);
    CProgress progress = newMedium.CreateBaseStorage(m_pSizeCombo->currentData().toLongLong(), variants);

    if (!newMedium.isOk())
    {
        msgCenter().cannotCreateMediumStorage(newMedium, strMediumLocation, this);
        return;
    }
    /* Show creation progress: */
    msgCenter().showModalProgressDialog(progress, windowTitle(), ":/progress_media_create_90px.png", this);
    if (progress.GetCanceled())
        return;

    if (!progress.isOk() || progress.GetResultCode() != 0)
    {
        msgCenter().cannotCreateHardDiskStorage(progress, strMediumLocation, this);
        return;
    }
    /* Store the id of the newly create medium: */
    m_strMediumID = newMedium.GetId();

    /* Notify VBoxGlobal about the new medium: */
    vboxGlobal().createMedium(UIMedium(newMedium, UIMediumDeviceType_Floppy, KMediumState_Created));
    /* Notify VBoxGlobal about the new medium: */
    vboxGlobal().updateRecentlyUsedMediumListAndFolder(UIMediumDeviceType_Floppy, strMediumLocation);

    /* After a successful creation and initilization of the floppy disk we call base class accept
       effectively closing this dialog: */
    QDialog::accept();
}

QString UIFDCreationDialog::mediumID() const
{
    return m_strMediumID;
}
