/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSFDetails class implementation.
 */

/*
 * Copyright (C) 2008-2022 Oracle Corporation
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
#include <QDir>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

/* GUI includes */
#include "QIDialogButtonBox.h"
#include "UIFilePathSelector.h"
#include "UIMachineSettingsSFDetails.h"
#include "UICommon.h"


UIMachineSettingsSFDetails::UIMachineSettingsSFDetails(SFDialogType type,
                                                       bool fUsePermanent,
                                                       const QStringList &usedNames,
                                                       QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI2<QIDialog>(pParent)
    , m_type(type)
    , m_fUsePermanent(fUsePermanent)
    , m_usedNames(usedNames)
    , m_pCache(0)
    , m_pLabelPath(0)
    , m_pSelectorPath(0)
    , m_pLabelName(0)
    , m_pEditorName(0)
    , m_pLabelAutoMountPoint(0)
    , m_pEditorAutoMountPoint(0)
    , m_pCheckBoxReadonly(0)
    , m_pCheckBoxAutoMount(0)
    , m_pCheckBoxPermanent(0)
    , m_pButtonBox(0)
{
    prepare();
}

void UIMachineSettingsSFDetails::setPath(const QString &strPath)
{
    m_pSelectorPath->setPath(strPath);
}

QString UIMachineSettingsSFDetails::path() const
{
    return m_pSelectorPath->path();
}

void UIMachineSettingsSFDetails::setName(const QString &strName)
{
    m_pEditorName->setText(strName);
}

QString UIMachineSettingsSFDetails::name() const
{
    return m_pEditorName->text();
}

void UIMachineSettingsSFDetails::setWriteable(bool fWritable)
{
    m_pCheckBoxReadonly->setChecked(!fWritable);
}

bool UIMachineSettingsSFDetails::isWriteable() const
{
    return !m_pCheckBoxReadonly->isChecked();
}

void UIMachineSettingsSFDetails::setAutoMount(bool fAutoMount)
{
    m_pCheckBoxAutoMount->setChecked(fAutoMount);
}

bool UIMachineSettingsSFDetails::isAutoMounted() const
{
    return m_pCheckBoxAutoMount->isChecked();
}

void UIMachineSettingsSFDetails::setAutoMountPoint(const QString &strAutoMountPoint)
{
    m_pEditorAutoMountPoint->setText(strAutoMountPoint);
}

QString UIMachineSettingsSFDetails::autoMountPoint() const
{
    return m_pEditorAutoMountPoint->text();
}

void UIMachineSettingsSFDetails::setPermanent(bool fPermanent)
{
    m_pCheckBoxPermanent->setChecked(fPermanent);
}

bool UIMachineSettingsSFDetails::isPermanent() const
{
    return m_fUsePermanent ? m_pCheckBoxPermanent->isChecked() : true;
}

void UIMachineSettingsSFDetails::retranslateUi()
{
    m_pLabelPath->setText(tr("Folder Path:"));
    m_pLabelName->setText(tr("Folder Name:"));
    m_pEditorName->setToolTip(tr("Holds the name of the shared folder (as it will be seen by the guest OS)."));
    m_pCheckBoxReadonly->setToolTip(tr("When checked, the guest OS will not be able to write to the specified shared folder."));
    m_pCheckBoxReadonly->setText(tr("&Read-only"));
    m_pCheckBoxAutoMount->setToolTip(tr("When checked, the guest OS will try to automatically mount the shared folder on startup."));
    m_pCheckBoxAutoMount->setText(tr("&Auto-mount"));
    m_pLabelAutoMountPoint->setText(tr("Mount point:"));
    m_pEditorAutoMountPoint->setToolTip(tr("Where to automatically mount the folder in the guest.  A drive letter (e.g. 'G:') "
                                           "for Windows and OS/2 guests, path for the others.  If left empty the guest will pick "
                                           "something fitting."));
    m_pCheckBoxPermanent->setToolTip(tr("When checked, this shared folder will be permanent."));
    m_pCheckBoxPermanent->setText(tr("&Make Permanent"));

    switch (m_type)
    {
        case AddType:
            setWindowTitle(tr("Add Share"));
            break;
        case EditType:
            setWindowTitle(tr("Edit Share"));
            break;
        default:
            AssertMsgFailed(("Incorrect shared-folders dialog type: %d\n", m_type));
    }
}

void UIMachineSettingsSFDetails::sltValidate()
{
    m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(!m_pSelectorPath->path().isEmpty() &&
                                                         QDir(m_pSelectorPath->path()).exists() &&
                                                         !m_pEditorName->text().trimmed().isEmpty() &&
                                                         !m_pEditorName->text().contains(" ") &&
                                                         !m_usedNames.contains(m_pEditorName->text()));
}

void UIMachineSettingsSFDetails::sltSelectPath()
{
    if (!m_pSelectorPath->isPathSelected())
        return;

    QString strFolderName(m_pSelectorPath->path());
#if defined (VBOX_WS_WIN) || defined (Q_OS_OS2)
    if (strFolderName[0].isLetter() && strFolderName[1] == ':' && strFolderName[2] == 0)
    {
        /* UIFilePathSelector returns root path as 'X:', which is invalid path.
         * Append the trailing backslash to get a valid root path 'X:\': */
        strFolderName += "\\";
        m_pSelectorPath->setPath(strFolderName);
    }
#endif /* VBOX_WS_WIN || Q_OS_OS2 */
    QDir folder(strFolderName);
    if (!folder.isRoot())
    {
        /* Processing non-root folder */
        m_pEditorName->setText(folder.dirName().replace(' ', '_'));
    }
    else
    {
        /* Processing root folder: */
#if defined (VBOX_WS_WIN) || defined (Q_OS_OS2)
        m_pEditorName->setText(strFolderName.toUpper()[0] + "_DRIVE");
#elif defined (VBOX_WS_X11)
        m_pEditorName->setText("ROOT");
#endif
    }
    /* Validate the field values: */
    sltValidate();
}

void UIMachineSettingsSFDetails::prepare()
{
    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Apply language settings: */
    retranslateUi();

    /* Validate the initial field values: */
    sltValidate();

    /* Adjust dialog size: */
    adjustSize();

#ifdef VBOX_WS_MAC
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(minimumSize());
#endif /* VBOX_WS_MAC */
}

void UIMachineSettingsSFDetails::prepareWidgets()
{
    /* Prepare main layout: */
    QGridLayout *pLayoutMain = new QGridLayout(this);
    if (pLayoutMain)
    {
        pLayoutMain->setRowStretch(6, 1);

        /* Prepare path label: */
        m_pLabelPath = new QLabel;
        if (m_pLabelPath)
        {
            m_pLabelPath->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pLabelPath, 0, 0);
        }
        /* Prepare path selector: */
        m_pSelectorPath = new UIFilePathSelector;
        if (m_pSelectorPath)
        {
            m_pSelectorPath->setResetEnabled(false);
            m_pSelectorPath->setInitialPath(QDir::homePath());

            pLayoutMain->addWidget(m_pSelectorPath, 0, 1);
        }

        /* Prepare name label: */
        m_pLabelName = new QLabel;
        if (m_pLabelName)
        {
            m_pLabelName->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pLabelName, 1, 0);
        }
        /* Prepare name editor: */
        m_pEditorName = new QLineEdit;
        if (m_pEditorName)
            pLayoutMain->addWidget(m_pEditorName, 1, 1);

        /* Prepare auto-mount point: */
        m_pLabelAutoMountPoint = new QLabel;
        if (m_pLabelAutoMountPoint)
        {
            m_pLabelAutoMountPoint->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pLabelAutoMountPoint, 2, 0);
        }
        /* Prepare auto-mount editor: */
        m_pEditorAutoMountPoint = new QLineEdit;
        if (m_pEditorAutoMountPoint)
            pLayoutMain->addWidget(m_pEditorAutoMountPoint, 2, 1);

        /* Prepare read-only check-box: */
        m_pCheckBoxReadonly = new QCheckBox;
        if (m_pCheckBoxReadonly)
            pLayoutMain->addWidget(m_pCheckBoxReadonly, 3, 1);
        /* Prepare auto-mount check-box: */
        m_pCheckBoxAutoMount = new QCheckBox;
        if (m_pCheckBoxAutoMount)
            pLayoutMain->addWidget(m_pCheckBoxAutoMount, 4, 1);
        /* Prepare permanent check-box: */
        m_pCheckBoxPermanent = new QCheckBox;
        if (m_pCheckBoxPermanent)
        {
            m_pCheckBoxPermanent->setHidden(!m_fUsePermanent);
            pLayoutMain->addWidget(m_pCheckBoxPermanent, 5, 1);
        }

        /* Prepare button-box: */
        m_pButtonBox = new QIDialogButtonBox;
        if (m_pButtonBox)
        {
            m_pButtonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
            pLayoutMain->addWidget(m_pButtonBox, 7, 0, 1, 2);
        }
    }
}

void UIMachineSettingsSFDetails::prepareConnections()
{
    connect(m_pSelectorPath, static_cast<void(UIFilePathSelector::*)(int)>(&UIFilePathSelector::currentIndexChanged),
            this, &UIMachineSettingsSFDetails::sltSelectPath);
    connect(m_pSelectorPath, &UIFilePathSelector::pathChanged, this, &UIMachineSettingsSFDetails::sltSelectPath);
    connect(m_pEditorName, &QLineEdit::textChanged, this, &UIMachineSettingsSFDetails::sltValidate);
    if (m_fUsePermanent)
        connect(m_pCheckBoxPermanent, &QCheckBox::toggled, this, &UIMachineSettingsSFDetails::sltValidate);
    connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UIMachineSettingsSFDetails::accept);
    connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UIMachineSettingsSFDetails::reject);
}
