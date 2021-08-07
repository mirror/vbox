/* $Id$ */
/** @file
 * VBox Qt GUI - UIFDCreationDialog class implementation.
 */

/*
 * Copyright (C) 2008-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes */
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

/* GUI includes */
#include "UICommon.h"
#include "UIFDCreationDialog.h"
#include "UIFilePathSelector.h"
#include "UIMedium.h"
#include "UIMessageCenter.h"
#include "UINotificationCenter.h"

/* COM includes: */
#include "CSystemProperties.h"
#include "CMedium.h"
#include "CMediumFormat.h"


UIFDCreationDialog::UIFDCreationDialog(QWidget *pParent,
                                       const QString &strDefaultFolder,
                                       const QString &strMachineName /* = QString() */)
    : QIWithRetranslateUI<QDialog>(pParent)
    , m_strDefaultFolder(strDefaultFolder)
    , m_strMachineName(strMachineName)
    , m_pLabelPath(0)
    , m_pFilePathSelector(0)
    , m_pSizeLabel(0)
    , m_pComboSize(0)
    , m_pCheckBoxFormat(0)
    , m_pButtonBox(0)
{
    prepare();
}

QUuid UIFDCreationDialog::mediumID() const
{
    return m_uMediumID;
}

void UIFDCreationDialog::accept()
{
    /* Make Ok button disabled first of all: */
    m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    /* Acquire medium path & formats: */
    const QString strMediumLocation = m_pFilePathSelector->path();
    const QVector<CMediumFormat> mediumFormats = UIMediumDefs::getFormatsForDeviceType(KDeviceType_Floppy);
    /* Make sure we have both path and formats selected: */
    if (strMediumLocation.isEmpty() || mediumFormats.isEmpty())
        return;

    /* Get VBox for further activities: */
    CVirtualBox comVBox = uiCommon().virtualBox();

    /* Create medium: */
    CMedium comMedium = comVBox.CreateMedium(mediumFormats[0].GetName(), strMediumLocation,
                                             KAccessMode_ReadWrite, KDeviceType_Floppy);
    if (!comVBox.isOk())
    {
        msgCenter().cannotCreateMediumStorage(comVBox, strMediumLocation, this);
        return;
    }

    /* Compose medium storage variants: */
    QVector<KMediumVariant> variants(1, KMediumVariant_Fixed);
    /* Decide if disk formatting is required: */
    if (m_pCheckBoxFormat && m_pCheckBoxFormat->checkState() == Qt::Checked)
        variants.push_back(KMediumVariant_Formatted);

    /* Create medium storage, asynchronously: */
    UINotificationProgressMediumCreate *pNotification =
        new UINotificationProgressMediumCreate(comMedium, m_pComboSize->currentData().toLongLong(), variants);
    connect(pNotification, &UINotificationProgressMediumCreate::sigMediumCreated,
            &uiCommon(), &UICommon::sltHandleMediumCreated);
    connect(pNotification, &UINotificationProgressMediumCreate::sigMediumCreated,
            this, &UIFDCreationDialog::sltHandleMediumCreated);
    gpNotificationCenter->append(pNotification);
}

void UIFDCreationDialog::retranslateUi()
{
    if (m_strMachineName.isEmpty())
        setWindowTitle(QString("%1").arg(tr("Floppy Disk Creator")));
    else
        setWindowTitle(QString("%1 - %2").arg(m_strMachineName).arg(tr("Floppy Disk Creator")));
    if (m_pLabelPath)
        m_pLabelPath->setText(tr("File Path:"));
    if (m_pSizeLabel)
        m_pSizeLabel->setText(tr("Size:"));
    if (m_pButtonBox)
        m_pButtonBox->button(QDialogButtonBox::Ok)->setText("Create");
    if (m_pCheckBoxFormat)
        m_pCheckBoxFormat->setText(tr("Format disk as FAT12"));
    if (m_pComboSize)
    {
        //m_pComboSize->setItemText(FDSize_2_88M, tr("2.88M"));
        m_pComboSize->setItemText(FDSize_1_44M, tr("1.44M"));
        m_pComboSize->setItemText(FDSize_1_2M, tr("1.2M"));
        m_pComboSize->setItemText(FDSize_720K, tr("720K"));
        m_pComboSize->setItemText(FDSize_360K, tr("360K"));
    }
}

void UIFDCreationDialog::sltHandleMediumCreated(const CMedium &comMedium)
{
    /* Store the ID of the newly created medium: */
    m_uMediumID = comMedium.GetId();

    /* Close the dialog now: */
    QDialog::accept();
}

void UIFDCreationDialog::prepare()
{
#ifndef VBOX_WS_MAC
    setWindowIcon(QIcon(":/fd_add_32px.png"));
#endif

    setWindowModality(Qt::WindowModal);
    setSizeGripEnabled(false);

    /* Prepare main layout: */
    QGridLayout *pLayoutMain = new QGridLayout(this);
    if (pLayoutMain)
    {
        /* Prepare path label: */
        m_pLabelPath = new QLabel(this);
        if (m_pLabelPath)
        {
            m_pLabelPath->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pLabelPath, 0, 0);
        }
        /* Prepare file path selector: */
        m_pFilePathSelector = new UIFilePathSelector(this);
        if (m_pFilePathSelector)
        {
            m_pFilePathSelector->setMode(UIFilePathSelector::Mode_File_Save);
            const QString strFilePath = getDefaultFilePath();
            m_pFilePathSelector->setDefaultPath(strFilePath);
            m_pFilePathSelector->setPath(strFilePath);

            pLayoutMain->addWidget(m_pFilePathSelector, 0, 1, 1, 3);
        }

        /* Prepare size label: */
        m_pSizeLabel = new QLabel(this);
        if (m_pSizeLabel)
        {
            m_pSizeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pSizeLabel, 1, 0);
        }
        /* Prepare size combo: */
        m_pComboSize = new QComboBox(this);
        if (m_pComboSize)
        {
            //m_pComboSize->insertItem(FDSize_2_88M, "2.88M", 2949120);
            m_pComboSize->insertItem(FDSize_1_44M, "1.44M", 1474560);
            m_pComboSize->insertItem(FDSize_1_2M, "1.2M", 1228800);
            m_pComboSize->insertItem(FDSize_720K, "720K", 737280);
            m_pComboSize->insertItem(FDSize_360K, "360K", 368640);
            m_pComboSize->setCurrentIndex(FDSize_1_44M);

            pLayoutMain->addWidget(m_pComboSize, 1, 1);
        }

        /* Prepare format check-box: */
        m_pCheckBoxFormat = new QCheckBox;
        if (m_pCheckBoxFormat)
        {
            m_pCheckBoxFormat->setCheckState(Qt::Checked);
            pLayoutMain->addWidget(m_pCheckBoxFormat, 2, 1, 1, 2);
        }

        /* Prepare button-box: */
        m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
        if (m_pButtonBox)
        {
            connect(m_pButtonBox, &QDialogButtonBox::accepted, this, &UIFDCreationDialog::accept);
            connect(m_pButtonBox, &QDialogButtonBox::rejected, this, &UIFDCreationDialog::reject);

            pLayoutMain->addWidget(m_pButtonBox, 3, 0, 1, 3);
        }
    }

    /* Apply language settings: */
    retranslateUi();

#ifdef VBOX_WS_MAC
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(minimumSize());
#endif /* VBOX_WS_MAC */

    /* Adjust dialog size: */
    adjustSize();
}

QString UIFDCreationDialog::getDefaultFilePath() const
{
    /* Prepare default file-path on the basis of passerd default folder: */
    QString strDefaultFilePath = m_strDefaultFolder;

    /* Make sure it's not empty if possible: */
    if (strDefaultFilePath.isEmpty())
        strDefaultFilePath = uiCommon().virtualBox().GetSystemProperties().GetDefaultMachineFolder();
    if (strDefaultFilePath.isEmpty())
        return strDefaultFilePath;

    /* Append file-path with disc name, generate unique file-name if necessary: */
    QString strDiskname = !m_strMachineName.isEmpty() ? m_strMachineName : "NewFloppyDisk";
    strDiskname = UICommon::findUniqueFileName(m_strDefaultFolder, strDiskname);

    /* Append file-path with preferred extension finally: */
    const QString strPreferredExtension = UIMediumDefs::getPreferredExtensionForMedium(KDeviceType_Floppy);
    strDefaultFilePath = QDir(strDefaultFilePath).absoluteFilePath(strDiskname + "." + strPreferredExtension);

    /* Return default file-path: */
    return strDefaultFilePath;
}
