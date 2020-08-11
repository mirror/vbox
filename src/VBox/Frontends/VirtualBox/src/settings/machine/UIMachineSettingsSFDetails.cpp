/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSFDetails class implementation.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
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
                                                       bool fEnableSelector, /* for "permanent" checkbox */
                                                       const QStringList &usedNames,
                                                       QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI2<QIDialog>(pParent)
    , m_type(type)
    , m_fUsePermanent(fEnableSelector)
    , m_usedNames(usedNames)
    , m_pPathSelector(0)
    , m_pPermanentCheckBox(0)
    , m_pNameLineEdit(0)
    , m_pReadonlyCheckBox(0)
    , m_pAutoMountCheckBox(0)
    , m_pAutoMountPointLineEdit(0)
    , m_pPathLabel(0)
    , m_pNameLabel(0)
    , m_pAutoMountPointLabel(0)
    , m_pButtonBox(0)
{
    prepareWidgets();

    /* Setup widgets: */
    m_pPathSelector->setResetEnabled(false);
    m_pPathSelector->setHomeDir(QDir::homePath());
    m_pPermanentCheckBox->setHidden(!fEnableSelector);

    /* Setup connections: */
    connect(m_pPathSelector, static_cast<void(UIFilePathSelector::*)(int)>(&UIFilePathSelector::currentIndexChanged),
            this, &UIMachineSettingsSFDetails::sltSelectPath);
    connect(m_pPathSelector, &UIFilePathSelector::pathChanged, this, &UIMachineSettingsSFDetails::sltSelectPath);
    connect(m_pNameLineEdit, &QLineEdit::textChanged, this, &UIMachineSettingsSFDetails::sltValidate);
    if (fEnableSelector)
        connect(m_pPermanentCheckBox, &QCheckBox::toggled, this, &UIMachineSettingsSFDetails::sltValidate);

     /* Applying language settings: */
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
    if (objectName().isEmpty())
        setObjectName(QStringLiteral("UIMachineSettingsSFDetails"));
    resize(218, 196);
    QGridLayout *gridLayout = new QGridLayout(this);
    gridLayout->setObjectName(QStringLiteral("gridLayout"));
    m_pPathLabel = new QLabel;
    m_pPathLabel->setObjectName(QStringLiteral("m_pPathLabel"));
    m_pPathLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

    gridLayout->addWidget(m_pPathLabel, 0, 0, 1, 1);

    m_pPathSelector = new UIFilePathSelector;
    m_pPathSelector->setObjectName(QStringLiteral("m_pPathSelector"));
    gridLayout->addWidget(m_pPathSelector, 0, 1, 1, 1);

    m_pNameLabel = new QLabel;
    m_pNameLabel->setObjectName(QStringLiteral("m_pNameLabel"));
    m_pNameLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    gridLayout->addWidget(m_pNameLabel, 1, 0, 1, 1);

    m_pNameLineEdit = new QLineEdit;
    m_pNameLineEdit->setObjectName(QStringLiteral("m_pNameLineEdit"));
    gridLayout->addWidget(m_pNameLineEdit, 1, 1, 1, 1);

    m_pReadonlyCheckBox = new QCheckBox;
    m_pReadonlyCheckBox->setObjectName(QStringLiteral("m_pReadonlyCheckBox"));
    gridLayout->addWidget(m_pReadonlyCheckBox, 2, 1, 1, 1);

    m_pAutoMountCheckBox = new QCheckBox;
    m_pAutoMountCheckBox->setObjectName(QStringLiteral("m_pAutoMountCheckBox"));
    gridLayout->addWidget(m_pAutoMountCheckBox, 3, 1, 1, 1);

    m_pAutoMountPointLabel = new QLabel;
    m_pAutoMountPointLabel->setObjectName(QStringLiteral("m_pAutoMountPointLabel"));
    m_pAutoMountPointLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    gridLayout->addWidget(m_pAutoMountPointLabel, 4, 0, 1, 1);

    m_pAutoMountPointLineEdit = new QLineEdit;
    m_pAutoMountPointLineEdit->setObjectName(QStringLiteral("m_pAutoMountPointLineEdit"));
    gridLayout->addWidget(m_pAutoMountPointLineEdit, 4, 1, 1, 1);

    m_pPermanentCheckBox = new QCheckBox;
    m_pPermanentCheckBox->setObjectName(QStringLiteral("m_pPermanentCheckBox"));
    gridLayout->addWidget(m_pPermanentCheckBox, 5, 1, 1, 1);

    QSpacerItem *spacerItem = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    gridLayout->addItem(spacerItem, 6, 1, 1, 1);

    m_pButtonBox = new QIDialogButtonBox;
    m_pButtonBox->setObjectName(QStringLiteral("m_pButtonBox"));
    m_pButtonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::NoButton|QDialogButtonBox::Ok);
    gridLayout->addWidget(m_pButtonBox, 7, 0, 1, 2);


    QObject::connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UIMachineSettingsSFDetails::accept);
    QObject::connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UIMachineSettingsSFDetails::reject);
}


void UIMachineSettingsSFDetails::setPath(const QString &strPath)
{
    m_pPathSelector->setPath(strPath);
}

QString UIMachineSettingsSFDetails::path() const
{
    return m_pPathSelector->path();
}

void UIMachineSettingsSFDetails::setName(const QString &strName)
{
    m_pNameLineEdit->setText(strName);
}

QString UIMachineSettingsSFDetails::name() const
{
    return m_pNameLineEdit->text();
}

void UIMachineSettingsSFDetails::setWriteable(bool fWritable)
{
    m_pReadonlyCheckBox->setChecked(!fWritable);
}

bool UIMachineSettingsSFDetails::isWriteable() const
{
    return !m_pReadonlyCheckBox->isChecked();
}

void UIMachineSettingsSFDetails::setAutoMount(bool fAutoMount)
{
    m_pAutoMountCheckBox->setChecked(fAutoMount);
}

bool UIMachineSettingsSFDetails::isAutoMounted() const
{
    return m_pAutoMountCheckBox->isChecked();
}

void UIMachineSettingsSFDetails::setAutoMountPoint(const QString &strAutoMountPoint)
{
    m_pAutoMountPointLineEdit->setText(strAutoMountPoint);
}

QString UIMachineSettingsSFDetails::autoMountPoint() const
{
    return m_pAutoMountPointLineEdit->text();
}

void UIMachineSettingsSFDetails::setPermanent(bool fPermanent)
{
    m_pPermanentCheckBox->setChecked(fPermanent);
}

bool UIMachineSettingsSFDetails::isPermanent() const
{
    return m_fUsePermanent ? m_pPermanentCheckBox->isChecked() : true;
}

void UIMachineSettingsSFDetails::retranslateUi()
{
    m_pPathLabel->setText(tr("Folder Path:"));
    m_pNameLabel->setText(tr("Folder Name:"));
    m_pNameLineEdit->setToolTip(tr("Holds the name of the shared folder (as it will be seen by the guest OS)."));
    m_pReadonlyCheckBox->setToolTip(tr("When checked, the guest OS will not be able to write to the specified shared folder."));
    m_pReadonlyCheckBox->setText(tr("&Read-only"));
    m_pAutoMountCheckBox->setToolTip(tr("When checked, the guest OS will try to automatically mount the shared folder on startup."));
    m_pAutoMountCheckBox->setText(tr("&Auto-mount"));
    m_pAutoMountPointLabel->setText(tr("Mount point:"));
    m_pAutoMountPointLineEdit->setToolTip(tr("Where to automatically mount the folder in the guest.  A drive letter (e.g. 'G:') for Windows and OS/2 guests, path for the others.  If left empty the guest will pick something fitting."));
    m_pPermanentCheckBox->setToolTip(tr("When checked, this shared folder will be permanent."));
    m_pPermanentCheckBox->setText(tr("&Make Permanent"));

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
    m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(!m_pPathSelector->path().isEmpty() &&
                                                         QDir(m_pPathSelector->path()).exists() &&
                                                         !m_pNameLineEdit->text().trimmed().isEmpty() &&
                                                         !m_pNameLineEdit->text().contains(" ") &&
                                                         !m_usedNames.contains(m_pNameLineEdit->text()));
}

void UIMachineSettingsSFDetails::sltSelectPath()
{
    if (!m_pPathSelector->isPathSelected())
        return;

    QString strFolderName(m_pPathSelector->path());
#if defined (VBOX_WS_WIN) || defined (Q_OS_OS2)
    if (strFolderName[0].isLetter() && strFolderName[1] == ':' && strFolderName[2] == 0)
    {
        /* UIFilePathSelector returns root path as 'X:', which is invalid path.
         * Append the trailing backslash to get a valid root path 'X:\': */
        strFolderName += "\\";
        m_pPathSelector->setPath(strFolderName);
    }
#endif /* VBOX_WS_WIN || Q_OS_OS2 */
    QDir folder(strFolderName);
    if (!folder.isRoot())
    {
        /* Processing non-root folder */
        m_pNameLineEdit->setText(folder.dirName().replace(' ', '_'));
    }
    else
    {
        /* Processing root folder: */
#if defined (VBOX_WS_WIN) || defined (Q_OS_OS2)
        m_pNameLineEdit->setText(strFolderName.toUpper()[0] + "_DRIVE");
#elif defined (VBOX_WS_X11)
        m_pNameLineEdit->setText("ROOT");
#endif
    }
    /* Validate the field values: */
    sltValidate();
}
