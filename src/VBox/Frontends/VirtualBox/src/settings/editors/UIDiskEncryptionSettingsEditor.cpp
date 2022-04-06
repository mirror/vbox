/* $Id$ */
/** @file
 * VBox Qt GUI - UIDiskEncryptionSettingsEditor class implementation.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>

/* GUI includes: */
#include "UIConverter.h"
#include "UIDiskEncryptionSettingsEditor.h"


UIDiskEncryptionSettingsEditor::UIDiskEncryptionSettingsEditor(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fFeatureEnabled(false)
    , m_enmCipherType(UIDiskEncryptionCipherType_Max)
    , m_pCheckboxFeature(0)
    , m_pWidgetSettings(0)
    , m_pLabelCipher(0)
    , m_pComboCipher(0)
    , m_pLabelEncryptionPassword(0)
    , m_pEditorEncryptionPassword(0)
    , m_pLabelEncryptionPasswordConfirm(0)
    , m_pEditorEncryptionPasswordConfirm(0)
{
    prepare();
}

void UIDiskEncryptionSettingsEditor::setFeatureEnabled(bool fEnabled)
{
    /* Update cached value and
     * check-box if value has changed: */
    if (m_fFeatureEnabled != fEnabled)
    {
        m_fFeatureEnabled = fEnabled;
        if (m_pCheckboxFeature)
        {
            m_pCheckboxFeature->setChecked(m_fFeatureEnabled);
            sltHandleFeatureToggled(m_pCheckboxFeature->isChecked());
        }
    }
}

bool UIDiskEncryptionSettingsEditor::isFeatureEnabled() const
{
    return m_pCheckboxFeature ? m_pCheckboxFeature->isChecked() : m_fFeatureEnabled;
}

void UIDiskEncryptionSettingsEditor::setCipherType(const UIDiskEncryptionCipherType &enmType)
{
    /* Update cached value and
     * combo if value has changed: */
    if (m_enmCipherType != enmType)
    {
        m_enmCipherType = enmType;
        repopulateCombo();
    }
}

UIDiskEncryptionCipherType UIDiskEncryptionSettingsEditor::cipherType() const
{
    return m_pComboCipher ? m_pComboCipher->currentData().value<UIDiskEncryptionCipherType>() : m_enmCipherType;
}

QString UIDiskEncryptionSettingsEditor::password1() const
{
    return m_pEditorEncryptionPassword ? m_pEditorEncryptionPassword->text() : m_strPassword1;
}

QString UIDiskEncryptionSettingsEditor::password2() const
{
    return m_pEditorEncryptionPasswordConfirm ? m_pEditorEncryptionPasswordConfirm->text() : m_strPassword2;
}

void UIDiskEncryptionSettingsEditor::retranslateUi()
{
    if (m_pCheckboxFeature)
    {
        m_pCheckboxFeature->setText(tr("En&able Disk Encryption"));
        m_pCheckboxFeature->setToolTip(tr("When checked, disks attached to this virtual machine will be encrypted."));
    }

    if (m_pLabelCipher)
        m_pLabelCipher->setText(tr("Disk Encryption C&ipher:"));
    if (m_pComboCipher)
    {
        for (int iIndex = 0; iIndex < m_pComboCipher->count(); ++iIndex)
        {
            const UIDiskEncryptionCipherType enmType = m_pComboCipher->itemData(iIndex).value<UIDiskEncryptionCipherType>();
            m_pComboCipher->setItemText(iIndex, gpConverter->toString(enmType));
        }
        m_pComboCipher->setToolTip(tr("Holds the cipher to be used for encrypting the virtual machine disks."));
    }

    if (m_pLabelEncryptionPassword)
        m_pLabelEncryptionPassword->setText(tr("E&nter New Password:"));
    if (m_pEditorEncryptionPassword)
        m_pEditorEncryptionPassword->setToolTip(tr("Holds the encryption password for disks attached to this virtual machine."));
    if (m_pLabelEncryptionPasswordConfirm)
        m_pLabelEncryptionPasswordConfirm->setText(tr("C&onfirm New Password:"));
    if (m_pEditorEncryptionPasswordConfirm)
        m_pEditorEncryptionPasswordConfirm->setToolTip(tr("Confirms the disk encryption password."));

    /* Translate Cipher type combo: */
    m_pComboCipher->setItemText(0, tr("Leave Unchanged", "cipher type"));
}

void UIDiskEncryptionSettingsEditor::sltHandleFeatureToggled(bool fEnabled)
{
    /* Update widget availability: */
    if (m_pWidgetSettings)
        m_pWidgetSettings->setEnabled(fEnabled);

    /* Notify listeners: */
    emit sigStatusChanged();
}

void UIDiskEncryptionSettingsEditor::prepare()
{
    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Apply language settings: */
    retranslateUi();
}

void UIDiskEncryptionSettingsEditor::prepareWidgets()
{
    /* Prepare main layout: */
    QGridLayout *pLayout = new QGridLayout(this);
    if (pLayout)
    {
        pLayout->setContentsMargins(0, 0, 0, 0);
        pLayout->setColumnStretch(1, 1);

        /* Prepare 'feature' check-box: */
        m_pCheckboxFeature = new QCheckBox(this);
        if (m_pCheckboxFeature)
            pLayout->addWidget(m_pCheckboxFeature, 0, 0, 1, 2);

        /* Prepare 20-px shifting spacer: */
        QSpacerItem *pSpacerItem = new QSpacerItem(20, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        if (pSpacerItem)
            pLayout->addItem(pSpacerItem, 1, 0);

        /* Prepare 'settings' widget: */
        m_pWidgetSettings = new QWidget(this);
        if (m_pWidgetSettings)
        {
            /* Prepare encryption settings widget layout: */
            QGridLayout *m_pLayoutSettings = new QGridLayout(m_pWidgetSettings);
            if (m_pLayoutSettings)
            {
                m_pLayoutSettings->setContentsMargins(0, 0, 0, 0);

                /* Prepare encryption cipher label: */
                m_pLabelCipher = new QLabel(m_pWidgetSettings);
                if (m_pLabelCipher)
                {
                    m_pLabelCipher->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    m_pLayoutSettings->addWidget(m_pLabelCipher, 0, 0);
                }
                /* Prepare encryption cipher combo: */
                m_pComboCipher = new QComboBox(m_pWidgetSettings);
                if (m_pComboCipher)
                {
                    if (m_pLabelCipher)
                        m_pLabelCipher->setBuddy(m_pComboCipher);
                    m_pLayoutSettings->addWidget(m_pComboCipher, 0, 1);
                }

                /* Prepare encryption password label: */
                m_pLabelEncryptionPassword = new QLabel(m_pWidgetSettings);
                if (m_pLabelEncryptionPassword)
                {
                    m_pLabelEncryptionPassword->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    m_pLayoutSettings->addWidget(m_pLabelEncryptionPassword, 1, 0);
                }
                /* Prepare encryption password editor: */
                m_pEditorEncryptionPassword = new QLineEdit(m_pWidgetSettings);
                if (m_pEditorEncryptionPassword)
                {
                    if (m_pLabelEncryptionPassword)
                        m_pLabelEncryptionPassword->setBuddy(m_pEditorEncryptionPassword);
                    m_pEditorEncryptionPassword->setEchoMode(QLineEdit::Password);

                    m_pLayoutSettings->addWidget(m_pEditorEncryptionPassword, 1, 1);
                }

                /* Prepare encryption confirm password label: */
                m_pLabelEncryptionPasswordConfirm = new QLabel(m_pWidgetSettings);
                if (m_pLabelEncryptionPasswordConfirm)
                {
                    m_pLabelEncryptionPasswordConfirm->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    m_pLayoutSettings->addWidget(m_pLabelEncryptionPasswordConfirm, 2, 0);
                }
                /* Prepare encryption confirm password editor: */
                m_pEditorEncryptionPasswordConfirm = new QLineEdit(m_pWidgetSettings);
                if (m_pEditorEncryptionPasswordConfirm)
                {
                    if (m_pLabelEncryptionPasswordConfirm)
                        m_pLabelEncryptionPasswordConfirm->setBuddy(m_pEditorEncryptionPasswordConfirm);
                    m_pEditorEncryptionPasswordConfirm->setEchoMode(QLineEdit::Password);

                    m_pLayoutSettings->addWidget(m_pEditorEncryptionPasswordConfirm, 2, 1);
                }
            }

            pLayout->addWidget(m_pWidgetSettings, 1, 1, 1, 2);
        }
    }

    /* Update widget availability: */
    if (m_pCheckboxFeature)
        sltHandleFeatureToggled(m_pCheckboxFeature->isChecked());
}

void UIDiskEncryptionSettingsEditor::prepareConnections()
{
    if (m_pCheckboxFeature)
        connect(m_pCheckboxFeature, &QCheckBox::toggled,
                this, &UIDiskEncryptionSettingsEditor::sltHandleFeatureToggled);
    if (m_pComboCipher)
        connect(m_pComboCipher, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, &UIDiskEncryptionSettingsEditor::sigCipherChanged);
    if (m_pEditorEncryptionPassword)
        connect(m_pEditorEncryptionPassword, &QLineEdit::textEdited,
                this, &UIDiskEncryptionSettingsEditor::sigPasswordChanged);
    if (m_pEditorEncryptionPasswordConfirm)
        connect(m_pEditorEncryptionPasswordConfirm, &QLineEdit::textEdited,
                this, &UIDiskEncryptionSettingsEditor::sigPasswordChanged);
}

void UIDiskEncryptionSettingsEditor::repopulateCombo()
{
    if (m_pComboCipher)
    {
        /* Clear combo first of all: */
        m_pComboCipher->clear();

        /// @todo get supported auth types (API not implemented), not hardcoded!
        QVector<UIDiskEncryptionCipherType> cipherTypes =
            QVector<UIDiskEncryptionCipherType>() << UIDiskEncryptionCipherType_Unchanged
                                                  << UIDiskEncryptionCipherType_XTS256
                                                  << UIDiskEncryptionCipherType_XTS128;

        /* Take into account currently cached value: */
        if (!cipherTypes.contains(m_enmCipherType))
            cipherTypes.prepend(m_enmCipherType);

        /* Populate combo finally: */
        foreach (const UIDiskEncryptionCipherType &enmType, cipherTypes)
            m_pComboCipher->addItem(gpConverter->toString(enmType), QVariant::fromValue(enmType));

        /* Look for proper index to choose: */
        const int iIndex = m_pComboCipher->findData(QVariant::fromValue(m_enmCipherType));
        if (iIndex != -1)
            m_pComboCipher->setCurrentIndex(iIndex);
    }
}
