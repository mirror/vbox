/* $Id$ */
/** @file
 * VBox Qt GUI - UINameAndSystemEditor class implementation.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
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
# include <QGridLayout>
# include <QVBoxLayout>
# include <QLabel>
# include <QLineEdit>
# include <QComboBox>

/* GUI includes: */
# include "UINameAndSystemEditor.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/** Defines the VM OS type ID. */
enum
{
    TypeID = Qt::UserRole + 1
};


UINameAndSystemEditor::UINameAndSystemEditor(QWidget *pParent)
    : QIWithRetranslateUI<QWidget>(pParent)
{
    /* Register CGuestOSType type: */
    qRegisterMetaType<CGuestOSType>();

    /* Create widgets: */
    QGridLayout *pMainLayout = new QGridLayout(this);
    {
        pMainLayout->setContentsMargins(0, 0, 0, 0);
        m_pNameLabel = new QLabel(this);
        {
            m_pNameLabel->setAlignment(Qt::AlignRight);
            m_pNameLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        }
        m_pNameEditor = new QLineEdit(this);
        {
            m_pNameEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            m_pNameLabel->setBuddy(m_pNameEditor);
        }
        m_pFamilyLabel = new QLabel(this);
        {
            m_pFamilyLabel->setAlignment(Qt::AlignRight);
            m_pFamilyLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        }
        m_pFamilyCombo = new QComboBox(this);
        {
            m_pFamilyCombo->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            m_pFamilyLabel->setBuddy(m_pFamilyCombo);
        }
        m_pTypeLabel = new QLabel(this);
        {
            m_pTypeLabel->setAlignment(Qt::AlignRight);
            m_pTypeLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        }
        m_pTypeCombo = new QComboBox(this);
        {
            m_pTypeCombo->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            m_pTypeLabel->setBuddy(m_pTypeCombo);
        }
        QVBoxLayout *pIconLayout = new QVBoxLayout;
        {
            m_pTypeIcon = new QLabel(this);
            {
                m_pTypeIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            }
            pIconLayout->addWidget(m_pTypeIcon);
            pIconLayout->addStretch();
        }
        pMainLayout->addWidget(m_pNameLabel, 0, 0);
        pMainLayout->addWidget(m_pNameEditor, 0, 1, 1, 2);
        pMainLayout->addWidget(m_pFamilyLabel, 1, 0);
        pMainLayout->addWidget(m_pFamilyCombo, 1, 1);
        pMainLayout->addWidget(m_pTypeLabel, 2, 0);
        pMainLayout->addWidget(m_pTypeCombo, 2, 1);
        pMainLayout->addLayout(pIconLayout, 1, 2, 2, 1);
    }

    /* Check if host supports (AMD-V or VT-x) and long mode: */
    CHost host = vboxGlobal().host();
    m_fSupportsHWVirtEx = host.GetProcessorFeature(KProcessorFeature_HWVirtEx);
    m_fSupportsLongMode = host.GetProcessorFeature(KProcessorFeature_LongMode);

    /* Fill OS family selector: */
    QList<CGuestOSType> families(vboxGlobal().vmGuestOSFamilyList());
    for (int i = 0; i < families.size(); ++i)
    {
        QString familyName(families[i].GetFamilyDescription());
        m_pFamilyCombo->insertItem(i, familyName);
        m_pFamilyCombo->setItemData(i, families[i].GetFamilyId(), TypeID);
    }
    m_pFamilyCombo->setCurrentIndex(0);
    sltFamilyChanged(m_pFamilyCombo->currentIndex());

    /* Setup connections: */
    connect(m_pNameEditor, SIGNAL(textChanged(const QString &)), this, SIGNAL(sigNameChanged(const QString &)));
    connect(m_pFamilyCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(sltFamilyChanged(int)));
    connect(m_pTypeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(sltTypeChanged(int)));

    /* Retranslate: */
    retranslateUi();
}

QLineEdit* UINameAndSystemEditor::nameEditor() const
{
    return m_pNameEditor;
}

void UINameAndSystemEditor::setName(const QString &strName)
{
    m_pNameEditor->setText(strName);
}

QString UINameAndSystemEditor::name() const
{
    return m_pNameEditor->text();
}

void UINameAndSystemEditor::setType(const CGuestOSType &type)
{
    /* Initialize variables: */
    QString strFamilyId(type.GetFamilyId());
    QString strTypeId(type.GetId());

    /* Get/check family index: */
    int iFamilyIndex = m_pFamilyCombo->findData(strFamilyId, TypeID);
    AssertMsg(iFamilyIndex != -1, ("Invalid family ID: '%s'", strFamilyId.toLatin1().constData()));
    if (iFamilyIndex != -1)
        m_pFamilyCombo->setCurrentIndex(iFamilyIndex);

    /* Get/check type index: */
    int iTypeIndex = m_pTypeCombo->findData(strTypeId, TypeID);
    AssertMsg(iTypeIndex != -1, ("Invalid type ID: '%s'", strTypeId.toLatin1().constData()));
    if (iTypeIndex != -1)
        m_pTypeCombo->setCurrentIndex(iTypeIndex);
}

CGuestOSType UINameAndSystemEditor::type() const
{
    return m_type;
}

void UINameAndSystemEditor::retranslateUi()
{
    m_pNameLabel->setText(tr("N&ame:"));
    m_pNameEditor->setWhatsThis(tr("Holds the name of the virtual machine."));
    m_pFamilyLabel->setText(tr("&Type:"));
    m_pFamilyCombo->setWhatsThis(tr("Selects the operating system family that "
                                    "you plan to install into this virtual machine."));
    m_pTypeLabel->setText(tr("&Version:"));
    m_pTypeCombo->setWhatsThis(tr("Selects the operating system type that "
                                  "you plan to install into this virtual machine "
                                  "(called a guest operating system)."));
}

void UINameAndSystemEditor::sltFamilyChanged(int iIndex)
{
    /* Lock the signals of m_pTypeCombo to prevent it's reaction on clearing: */
    m_pTypeCombo->blockSignals(true);
    m_pTypeCombo->clear();

    /* Populate combo-box with OS types related to currently selected family id: */
    QString strFamilyId(m_pFamilyCombo->itemData(iIndex, TypeID).toString());
    QList<CGuestOSType> types(vboxGlobal().vmGuestOSTypeList(strFamilyId));
    for (int i = 0; i < types.size(); ++i)
    {
        if (types[i].GetIs64Bit() && (!m_fSupportsHWVirtEx || !m_fSupportsLongMode))
            continue;
        int iIndex = m_pTypeCombo->count();
        m_pTypeCombo->insertItem(iIndex, types[i].GetDescription());
        m_pTypeCombo->setItemData(iIndex, types[i].GetId(), TypeID);
    }

    /* Select the most recently chosen item: */
    if (m_currentIds.contains(strFamilyId))
    {
        QString strTypeId(m_currentIds[strFamilyId]);
        int iTypeIndex = m_pTypeCombo->findData(strTypeId, TypeID);
        if (iTypeIndex != -1)
            m_pTypeCombo->setCurrentIndex(iTypeIndex);
    }
    /* Or select Windows 7 item for Windows family as default: */
    else if (strFamilyId == "Windows")
    {
        QString strDefaultID = "Windows7";
        if (ARCH_BITS == 64 && m_fSupportsHWVirtEx && m_fSupportsLongMode)
            strDefaultID += "_64";
        int iIndexWin7 = m_pTypeCombo->findData(strDefaultID, TypeID);
        if (iIndexWin7 != -1)
            m_pTypeCombo->setCurrentIndex(iIndexWin7);
    }
    /* Or select Ubuntu item for Linux family as default: */
    else if (strFamilyId == "Linux")
    {
        QString strDefaultID = "Ubuntu";
        if (ARCH_BITS == 64 && m_fSupportsHWVirtEx && m_fSupportsLongMode)
            strDefaultID += "_64";
        int iIndexUbuntu = m_pTypeCombo->findData(strDefaultID, TypeID);
        if (iIndexUbuntu != -1)
            m_pTypeCombo->setCurrentIndex(iIndexUbuntu);
    }
    /* Else simply select the first one present: */
    else m_pTypeCombo->setCurrentIndex(0);

    /* Update all the stuff: */
    sltTypeChanged(m_pTypeCombo->currentIndex());

    /* Unlock the signals of m_pTypeCombo: */
    m_pTypeCombo->blockSignals(false);
}

void UINameAndSystemEditor::sltTypeChanged(int iIndex)
{
    /* Save the new selected OS Type: */
    m_type = vboxGlobal().vmGuestOSType(m_pTypeCombo->itemData(iIndex, TypeID).toString(),
                                        m_pFamilyCombo->itemData(m_pFamilyCombo->currentIndex(), TypeID).toString());
    m_pTypeIcon->setPixmap(vboxGlobal().vmGuestOSTypeIcon(m_type.GetId()));

    /* Save the most recently used item: */
    m_currentIds[m_type.GetFamilyId()] = m_type.GetId();

    /* Notifies listeners about OS type change: */
    emit sigOsTypeChanged();
}

