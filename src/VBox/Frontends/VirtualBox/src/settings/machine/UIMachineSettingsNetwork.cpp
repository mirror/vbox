/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsNetwork class implementation.
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

/* GUI includes: */
# include "QIWidgetValidator.h"
# include "QIArrowButtonSwitch.h"
# include "UIMachineSettingsNetwork.h"
# include "QITabWidget.h"
# include "VBoxGlobal.h"
# include "UIConverter.h"
# include "UIIconPool.h"

/* COM includes: */
# include "CNetworkAdapter.h"
# include "CHostNetworkInterface.h"
# include "CNATNetwork.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#include "CNATEngine.h"

/* Other VBox includes: */
#ifdef VBOX_WITH_VDE
# include <iprt/ldr.h>
# include <VBox/VDEPlug.h>
#endif /* VBOX_WITH_VDE */


/* Empty item extra-code: */
const char *pEmptyItemCode = "#empty#";

QString wipedOutString(const QString &strInputString)
{
    return strInputString.isEmpty() ? QString() : strInputString;
}

UIMachineSettingsNetwork::UIMachineSettingsNetwork(UIMachineSettingsNetworkPage *pParent)
    : QIWithRetranslateUI<QWidget>(0)
    , m_pParent(pParent)
    , m_iSlot(-1)
{
    /* Apply UI decorations: */
    Ui::UIMachineSettingsNetwork::setupUi(this);

    /* Determine icon metric: */
    const QStyle *pStyle = QApplication::style();
    const int iIconMetric = (int)(pStyle->pixelMetric(QStyle::PM_SmallIconSize) * .625);

    /* Setup widgets: */
    m_pAdapterNameCombo->setInsertPolicy(QComboBox::NoInsert);
    m_pMACEditor->setValidator(new QRegExpValidator(QRegExp("[0-9A-Fa-f]{12}"), this));
    m_pMACEditor->setMinimumWidthByText(QString().fill('0', 12));
    m_pMACButton->setIcon(UIIconPool::iconSet(":/refresh_16px.png"));
    m_pAdvancedArrow->setIconSize(QSize(iIconMetric, iIconMetric));
    m_pAdvancedArrow->setIcons(UIIconPool::iconSet(":/arrow_right_10px.png"),
                               UIIconPool::iconSet(":/arrow_down_10px.png"));

    /* Setup connections: */
    connect(m_pEnableAdapterCheckBox, SIGNAL(toggled(bool)), this, SLOT(sltHandleAdapterActivityChange()));
    connect(m_pAttachmentTypeComboBox, SIGNAL(activated(int)), this, SLOT(sltHandleAttachmentTypeChange()));
    connect(m_pAdapterNameCombo, SIGNAL(activated(int)), this, SLOT(sltHandleAlternativeNameChange()));
    connect(m_pAdapterNameCombo, SIGNAL(editTextChanged(const QString&)), this, SLOT(sltHandleAlternativeNameChange()));
    connect(m_pAdvancedArrow, SIGNAL(sigClicked()), this, SLOT(sltHandleAdvancedButtonStateChange()));
    connect(m_pMACButton, SIGNAL(clicked()), this, SLOT(sltGenerateMac()));
    connect(m_pPortForwardingButton, SIGNAL(clicked()), this, SLOT(sltOpenPortForwardingDlg()));
    connect(this, SIGNAL(sigTabUpdated()), m_pParent, SLOT(sltHandleUpdatedTab()));

    /* Prepare validation: */
    prepareValidation();

    /* Applying language settings: */
    retranslateUi();
}

void UIMachineSettingsNetwork::fetchAdapterCache(const UISettingsCacheMachineNetworkAdapter &adapterCache)
{
    /* Get adapter data: */
    const UIDataSettingsMachineNetworkAdapter &adapterData = adapterCache.base();

    /* Load slot number: */
    m_iSlot = adapterData.m_iSlot;

    /* Load adapter activity state: */
    m_pEnableAdapterCheckBox->setChecked(adapterData.m_fAdapterEnabled);
    /* Handle adapter activity change: */
    sltHandleAdapterActivityChange();

    /* Load attachment type: */
    m_pAttachmentTypeComboBox->setCurrentIndex(position(m_pAttachmentTypeComboBox, adapterData.m_attachmentType));
    /* Load alternative name: */
    m_strBridgedAdapterName = wipedOutString(adapterData.m_strBridgedAdapterName);
    m_strInternalNetworkName = wipedOutString(adapterData.m_strInternalNetworkName);
    m_strHostInterfaceName = wipedOutString(adapterData.m_strHostInterfaceName);
    m_strGenericDriverName = wipedOutString(adapterData.m_strGenericDriverName);
    m_strNATNetworkName = wipedOutString(adapterData.m_strNATNetworkName);
    /* Handle attachment type change: */
    sltHandleAttachmentTypeChange();

    /* Load adapter type: */
    m_pAdapterTypeCombo->setCurrentIndex(position(m_pAdapterTypeCombo, adapterData.m_adapterType));

    /* Load promiscuous mode type: */
    m_pPromiscuousModeCombo->setCurrentIndex(position(m_pPromiscuousModeCombo, adapterData.m_promiscuousMode));

    /* Other options: */
    m_pMACEditor->setText(adapterData.m_strMACAddress);
    m_pGenericPropertiesTextEdit->setText(adapterData.m_strGenericProperties);
    m_pCableConnectedCheckBox->setChecked(adapterData.m_fCableConnected);

    /* Load port forwarding rules: */
    m_portForwardingRules = adapterData.m_redirects;
}

void UIMachineSettingsNetwork::uploadAdapterCache(UISettingsCacheMachineNetworkAdapter &adapterCache)
{
    /* Prepare adapter data: */
    UIDataSettingsMachineNetworkAdapter adapterData = adapterCache.base();

    /* Save adapter activity state: */
    adapterData.m_fAdapterEnabled = m_pEnableAdapterCheckBox->isChecked();

    /* Save attachment type & alternative name: */
    adapterData.m_attachmentType = attachmentType();
    switch (adapterData.m_attachmentType)
    {
        case KNetworkAttachmentType_Null:
            break;
        case KNetworkAttachmentType_NAT:
            break;
        case KNetworkAttachmentType_Bridged:
            adapterData.m_strBridgedAdapterName = alternativeName();
            break;
        case KNetworkAttachmentType_Internal:
            adapterData.m_strInternalNetworkName = alternativeName();
            break;
        case KNetworkAttachmentType_HostOnly:
            adapterData.m_strHostInterfaceName = alternativeName();
            break;
        case KNetworkAttachmentType_Generic:
            adapterData.m_strGenericDriverName = alternativeName();
            adapterData.m_strGenericProperties = m_pGenericPropertiesTextEdit->toPlainText();
            break;
        case KNetworkAttachmentType_NATNetwork:
            adapterData.m_strNATNetworkName = alternativeName();
            break;
        default:
            break;
    }

    /* Save adapter type: */
    adapterData.m_adapterType = (KNetworkAdapterType)m_pAdapterTypeCombo->itemData(m_pAdapterTypeCombo->currentIndex()).toInt();

    /* Save promiscuous mode type: */
    adapterData.m_promiscuousMode = (KNetworkAdapterPromiscModePolicy)m_pPromiscuousModeCombo->itemData(m_pPromiscuousModeCombo->currentIndex()).toInt();

    /* Other options: */
    adapterData.m_strMACAddress = m_pMACEditor->text().isEmpty() ? QString() : m_pMACEditor->text();
    adapterData.m_fCableConnected = m_pCableConnectedCheckBox->isChecked();

    /* Save port forwarding rules: */
    adapterData.m_redirects = m_portForwardingRules;

    /* Cache adapter data: */
    adapterCache.cacheCurrentData(adapterData);
}

bool UIMachineSettingsNetwork::validate(QList<UIValidationMessage> &messages)
{
    /* Pass if adapter is disabled: */
    if (!m_pEnableAdapterCheckBox->isChecked())
        return true;

    /* Pass by default: */
    bool fPass = true;

    /* Prepare message: */
    UIValidationMessage message;
    message.first = vboxGlobal().removeAccelMark(tabTitle());

    /* Validate alternatives: */
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
        {
            if (alternativeName().isNull())
            {
                message.second << tr("No bridged network adapter is currently selected.");
                fPass = false;
            }
            break;
        }
        case KNetworkAttachmentType_Internal:
        {
            if (alternativeName().isNull())
            {
                message.second << tr("No internal network name is currently specified.");
                fPass = false;
            }
            break;
        }
        case KNetworkAttachmentType_HostOnly:
        {
            if (alternativeName().isNull())
            {
                message.second << tr("No host-only network adapter is currently selected.");
                fPass = false;
            }
            break;
        }
        case KNetworkAttachmentType_Generic:
        {
            if (alternativeName().isNull())
            {
                message.second << tr("No generic driver is currently selected.");
                fPass = false;
            }
            break;
        }
        case KNetworkAttachmentType_NATNetwork:
        {
            if (alternativeName().isNull())
            {
                message.second << tr("No NAT network name is currently specified.");
                fPass = false;
            }
            break;
        }
        default:
            break;
    }

    /* Validate MAC-address length: */
    if (m_pMACEditor->text().size() < 12)
    {
        message.second << tr("The MAC address must be 12 hexadecimal digits long.");
        fPass = false;
    }

    /* Make sure MAC-address is unicast: */
    if (m_pMACEditor->text().size() >= 2)
    {
        QRegExp validator("^[0-9A-Fa-f][02468ACEace]");
        if (validator.indexIn(m_pMACEditor->text()) != 0)
        {
            message.second << tr("The second digit in the MAC address may not be odd as only unicast addresses are allowed.");
            fPass = false;
        }
    }

    /* Serialize message: */
    if (!message.second.isEmpty())
        messages << message;

    /* Return result: */
    return fPass;
}

QWidget* UIMachineSettingsNetwork::setOrderAfter(QWidget *pAfter)
{
    setTabOrder(pAfter, m_pEnableAdapterCheckBox);
    setTabOrder(m_pEnableAdapterCheckBox, m_pAttachmentTypeComboBox);
    setTabOrder(m_pAttachmentTypeComboBox, m_pAdapterNameCombo);
    setTabOrder(m_pAdapterNameCombo, m_pAdvancedArrow);
    setTabOrder(m_pAdvancedArrow, m_pAdapterTypeCombo);
    setTabOrder(m_pAdapterTypeCombo, m_pPromiscuousModeCombo);
    setTabOrder(m_pPromiscuousModeCombo, m_pMACEditor);
    setTabOrder(m_pMACEditor, m_pMACButton);
    setTabOrder(m_pMACButton, m_pGenericPropertiesTextEdit);
    setTabOrder(m_pGenericPropertiesTextEdit, m_pCableConnectedCheckBox);
    setTabOrder(m_pCableConnectedCheckBox, m_pPortForwardingButton);
    return m_pPortForwardingButton;
}

QString UIMachineSettingsNetwork::tabTitle() const
{
    return VBoxGlobal::tr("Adapter %1").arg(QString("&%1").arg(m_iSlot + 1));
}

KNetworkAttachmentType UIMachineSettingsNetwork::attachmentType() const
{
    return (KNetworkAttachmentType)m_pAttachmentTypeComboBox->itemData(m_pAttachmentTypeComboBox->currentIndex()).toInt();
}

QString UIMachineSettingsNetwork::alternativeName(int iType) const
{
    if (iType == -1)
        iType = attachmentType();
    QString strResult;
    switch (iType)
    {
        case KNetworkAttachmentType_Bridged:
            strResult = m_strBridgedAdapterName;
            break;
        case KNetworkAttachmentType_Internal:
            strResult = m_strInternalNetworkName;
            break;
        case KNetworkAttachmentType_HostOnly:
            strResult = m_strHostInterfaceName;
            break;
        case KNetworkAttachmentType_Generic:
            strResult = m_strGenericDriverName;
            break;
        case KNetworkAttachmentType_NATNetwork:
            strResult = m_strNATNetworkName;
            break;
        default:
            break;
    }
    Assert(strResult.isNull() || !strResult.isEmpty());
    return strResult;
}

void UIMachineSettingsNetwork::polishTab()
{
    /* Basic attributes: */
    m_pEnableAdapterCheckBox->setEnabled(m_pParent->isMachineOffline());
    m_pAttachmentTypeLabel->setEnabled(m_pParent->isMachineInValidMode());
    m_pAttachmentTypeComboBox->setEnabled(m_pParent->isMachineInValidMode());
    m_pAdapterNameLabel->setEnabled(m_pParent->isMachineInValidMode() &&
                                    attachmentType() != KNetworkAttachmentType_Null &&
                                    attachmentType() != KNetworkAttachmentType_NAT);
    m_pAdapterNameCombo->setEnabled(m_pParent->isMachineInValidMode() &&
                                    attachmentType() != KNetworkAttachmentType_Null &&
                                    attachmentType() != KNetworkAttachmentType_NAT);
    m_pAdvancedArrow->setEnabled(m_pParent->isMachineInValidMode());

    /* Advanced attributes: */
    m_pAdapterTypeLabel->setEnabled(m_pParent->isMachineOffline());
    m_pAdapterTypeCombo->setEnabled(m_pParent->isMachineOffline());
    m_pPromiscuousModeLabel->setEnabled(m_pParent->isMachineInValidMode() &&
                                        attachmentType() != KNetworkAttachmentType_Null &&
                                        attachmentType() != KNetworkAttachmentType_Generic &&
                                        attachmentType() != KNetworkAttachmentType_NAT);
    m_pPromiscuousModeCombo->setEnabled(m_pParent->isMachineInValidMode() &&
                                        attachmentType() != KNetworkAttachmentType_Null &&
                                        attachmentType() != KNetworkAttachmentType_Generic &&
                                        attachmentType() != KNetworkAttachmentType_NAT);
    m_pMACLabel->setEnabled(m_pParent->isMachineOffline());
    m_pMACEditor->setEnabled(m_pParent->isMachineOffline());
    m_pMACButton->setEnabled(m_pParent->isMachineOffline());
    m_pGenericPropertiesLabel->setEnabled(m_pParent->isMachineInValidMode());
    m_pGenericPropertiesTextEdit->setEnabled(m_pParent->isMachineInValidMode());
    m_pPortForwardingButton->setEnabled(m_pParent->isMachineInValidMode() &&
                                        attachmentType() == KNetworkAttachmentType_NAT);

    /* Postprocessing: */
    handleAdvancedButtonStateChange();
}

void UIMachineSettingsNetwork::reloadAlternative()
{
    /* Repopulate alternative-name combo-box content: */
    updateAlternativeList();
    /* Select previous or default alternative-name combo-box item: */
    updateAlternativeName();
}

void UIMachineSettingsNetwork::setAdvancedButtonState(bool fExpanded)
{
    /* Check whether the button state really changed: */
    if (m_pAdvancedArrow->isExpanded() == fExpanded)
        return;

    /* Push the state to button and handle the state change: */
    m_pAdvancedArrow->setExpanded(fExpanded);
    handleAdvancedButtonStateChange();
}

void UIMachineSettingsNetwork::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIMachineSettingsNetwork::retranslateUi(this);

    /* Translate combo-boxes content: */
    populateComboboxes();

    /* Translate attachment info: */
    sltHandleAttachmentTypeChange();
}

void UIMachineSettingsNetwork::sltHandleAdapterActivityChange()
{
    /* Update availability: */
    m_pAdapterOptionsContainer->setEnabled(m_pEnableAdapterCheckBox->isChecked());

    /* Generate a new MAC address in case this adapter was never enabled before: */
    if (   m_pEnableAdapterCheckBox->isChecked()
        && m_pMACEditor->text().isEmpty())
        sltGenerateMac();

    /* Revalidate: */
    m_pParent->revalidate();
}

void UIMachineSettingsNetwork::sltHandleAttachmentTypeChange()
{
    /* Update alternative-name combo-box availability: */
    m_pAdapterNameLabel->setEnabled(attachmentType() != KNetworkAttachmentType_Null &&
                                    attachmentType() != KNetworkAttachmentType_NAT);
    m_pAdapterNameCombo->setEnabled(attachmentType() != KNetworkAttachmentType_Null &&
                                    attachmentType() != KNetworkAttachmentType_NAT);
    /* Update promiscuous-mode combo-box availability: */
    m_pPromiscuousModeLabel->setEnabled(attachmentType() != KNetworkAttachmentType_Null &&
                                        attachmentType() != KNetworkAttachmentType_Generic &&
                                        attachmentType() != KNetworkAttachmentType_NAT);
    m_pPromiscuousModeCombo->setEnabled(attachmentType() != KNetworkAttachmentType_Null &&
                                        attachmentType() != KNetworkAttachmentType_Generic &&
                                        attachmentType() != KNetworkAttachmentType_NAT);
    /* Update generic-properties editor visibility: */
    m_pGenericPropertiesLabel->setVisible(attachmentType() == KNetworkAttachmentType_Generic &&
                                          m_pAdvancedArrow->isExpanded());
    m_pGenericPropertiesTextEdit->setVisible(attachmentType() == KNetworkAttachmentType_Generic &&
                                             m_pAdvancedArrow->isExpanded());
    /* Update forwarding-rules button availability: */
    m_pPortForwardingButton->setEnabled(attachmentType() == KNetworkAttachmentType_NAT);
    /* Update alternative-name combo-box whats-this and editable state: */
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
        {
            m_pAdapterNameCombo->setWhatsThis(tr("Selects the network adapter on the host system that traffic "
                                                 "to and from this network card will go through."));
            m_pAdapterNameCombo->setEditable(false);
            break;
        }
        case KNetworkAttachmentType_Internal:
        {
            m_pAdapterNameCombo->setWhatsThis(tr("Holds the name of the internal network that this network card "
                                                 "will be connected to. You can create a new internal network by "
                                                 "choosing a name which is not used by any other network cards "
                                                 "in this virtual machine or others."));
            m_pAdapterNameCombo->setEditable(true);
            break;
        }
        case KNetworkAttachmentType_HostOnly:
        {
            m_pAdapterNameCombo->setWhatsThis(tr("Selects the virtual network adapter on the host system that traffic "
                                                 "to and from this network card will go through. "
                                                 "You can create and remove adapters using the global network "
                                                 "settings in the virtual machine manager window."));
            m_pAdapterNameCombo->setEditable(false);
            break;
        }
        case KNetworkAttachmentType_Generic:
        {
            m_pAdapterNameCombo->setWhatsThis(tr("Selects the driver to be used with this network card."));
            m_pAdapterNameCombo->setEditable(true);
            break;
        }
        case KNetworkAttachmentType_NATNetwork:
        {
            m_pAdapterNameCombo->setWhatsThis(tr("Holds the name of the NAT network that this network card "
                                                 "will be connected to. You can create and remove networks "
                                                 "using the global network settings in the virtual machine "
                                                 "manager window."));
            m_pAdapterNameCombo->setEditable(false);
            break;
        }
        default:
        {
            m_pAdapterNameCombo->setWhatsThis(QString());
            break;
        }
    }

    /* Update alternative combo: */
    reloadAlternative();

    /* Handle alternative-name cange: */
    sltHandleAlternativeNameChange();
}

void UIMachineSettingsNetwork::sltHandleAlternativeNameChange()
{
    /* Remember new name if its changed,
     * Call for other pages update, if necessary: */
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
        {
            QString newName(m_pAdapterNameCombo->itemData(m_pAdapterNameCombo->currentIndex()).toString() == QString(pEmptyItemCode) ||
                            m_pAdapterNameCombo->currentText().isEmpty() ? QString() : m_pAdapterNameCombo->currentText());
            if (m_strBridgedAdapterName != newName)
                m_strBridgedAdapterName = newName;
            break;
        }
        case KNetworkAttachmentType_Internal:
        {
            QString newName((m_pAdapterNameCombo->itemData(m_pAdapterNameCombo->currentIndex()).toString() == QString(pEmptyItemCode) &&
                             m_pAdapterNameCombo->currentText() == m_pAdapterNameCombo->itemText(m_pAdapterNameCombo->currentIndex())) ||
                             m_pAdapterNameCombo->currentText().isEmpty() ? QString() : m_pAdapterNameCombo->currentText());
            if (m_strInternalNetworkName != newName)
            {
                m_strInternalNetworkName = newName;
                if (!m_strInternalNetworkName.isNull())
                    emit sigTabUpdated();
            }
            break;
        }
        case KNetworkAttachmentType_HostOnly:
        {
            QString newName(m_pAdapterNameCombo->itemData(m_pAdapterNameCombo->currentIndex()).toString() == QString(pEmptyItemCode) ||
                            m_pAdapterNameCombo->currentText().isEmpty() ? QString() : m_pAdapterNameCombo->currentText());
            if (m_strHostInterfaceName != newName)
                m_strHostInterfaceName = newName;
            break;
        }
        case KNetworkAttachmentType_Generic:
        {
            QString newName((m_pAdapterNameCombo->itemData(m_pAdapterNameCombo->currentIndex()).toString() == QString(pEmptyItemCode) &&
                             m_pAdapterNameCombo->currentText() == m_pAdapterNameCombo->itemText(m_pAdapterNameCombo->currentIndex())) ||
                             m_pAdapterNameCombo->currentText().isEmpty() ? QString() : m_pAdapterNameCombo->currentText());
            if (m_strGenericDriverName != newName)
            {
                m_strGenericDriverName = newName;
                if (!m_strGenericDriverName.isNull())
                    emit sigTabUpdated();
            }
            break;
        }
        case KNetworkAttachmentType_NATNetwork:
        {
            QString newName(m_pAdapterNameCombo->itemData(m_pAdapterNameCombo->currentIndex()).toString() == QString(pEmptyItemCode) ||
                            m_pAdapterNameCombo->currentText().isEmpty() ? QString() : m_pAdapterNameCombo->currentText());
            if (m_strNATNetworkName != newName)
                m_strNATNetworkName = newName;
            break;
        }
        default:
            break;
    }

    /* Revalidate: */
    m_pParent->revalidate();
}

void UIMachineSettingsNetwork::sltHandleAdvancedButtonStateChange()
{
    /* Handle the button state change: */
    handleAdvancedButtonStateChange();

    /* Notify listeners about the button state change: */
    emit sigNotifyAdvancedButtonStateChange(m_pAdvancedArrow->isExpanded());
}

void UIMachineSettingsNetwork::sltGenerateMac()
{
    m_pMACEditor->setText(vboxGlobal().host().GenerateMACAddress());
}

void UIMachineSettingsNetwork::sltOpenPortForwardingDlg()
{
    UIMachineSettingsPortForwardingDlg dlg(this, m_portForwardingRules);
    if (dlg.exec() == QDialog::Accepted)
        m_portForwardingRules = dlg.rules();
}

void UIMachineSettingsNetwork::prepareValidation()
{
    /* Configure validation: */
    connect(m_pMACEditor, SIGNAL(textChanged(const QString &)), m_pParent, SLOT(revalidate()));
}

void UIMachineSettingsNetwork::populateComboboxes()
{
    /* Attachment type: */
    {
        /* Remember the currently selected attachment type: */
        int iCurrentAttachment = m_pAttachmentTypeComboBox->currentIndex();

        /* Clear the attachments combo-box: */
        m_pAttachmentTypeComboBox->clear();

        /* Populate attachments: */
        int iAttachmentTypeIndex = 0;
        m_pAttachmentTypeComboBox->insertItem(iAttachmentTypeIndex, gpConverter->toString(KNetworkAttachmentType_Null));
        m_pAttachmentTypeComboBox->setItemData(iAttachmentTypeIndex, KNetworkAttachmentType_Null);
        m_pAttachmentTypeComboBox->setItemData(iAttachmentTypeIndex, m_pAttachmentTypeComboBox->itemText(iAttachmentTypeIndex), Qt::ToolTipRole);
        ++iAttachmentTypeIndex;
        m_pAttachmentTypeComboBox->insertItem(iAttachmentTypeIndex, gpConverter->toString(KNetworkAttachmentType_NAT));
        m_pAttachmentTypeComboBox->setItemData(iAttachmentTypeIndex, KNetworkAttachmentType_NAT);
        m_pAttachmentTypeComboBox->setItemData(iAttachmentTypeIndex, m_pAttachmentTypeComboBox->itemText(iAttachmentTypeIndex), Qt::ToolTipRole);
        ++iAttachmentTypeIndex;
        m_pAttachmentTypeComboBox->insertItem(iAttachmentTypeIndex, gpConverter->toString(KNetworkAttachmentType_NATNetwork));
        m_pAttachmentTypeComboBox->setItemData(iAttachmentTypeIndex, KNetworkAttachmentType_NATNetwork);
        m_pAttachmentTypeComboBox->setItemData(iAttachmentTypeIndex, m_pAttachmentTypeComboBox->itemText(iAttachmentTypeIndex), Qt::ToolTipRole);
        ++iAttachmentTypeIndex;
        m_pAttachmentTypeComboBox->insertItem(iAttachmentTypeIndex, gpConverter->toString(KNetworkAttachmentType_Bridged));
        m_pAttachmentTypeComboBox->setItemData(iAttachmentTypeIndex, KNetworkAttachmentType_Bridged);
        m_pAttachmentTypeComboBox->setItemData(iAttachmentTypeIndex, m_pAttachmentTypeComboBox->itemText(iAttachmentTypeIndex), Qt::ToolTipRole);
        ++iAttachmentTypeIndex;
        m_pAttachmentTypeComboBox->insertItem(iAttachmentTypeIndex, gpConverter->toString(KNetworkAttachmentType_Internal));
        m_pAttachmentTypeComboBox->setItemData(iAttachmentTypeIndex, KNetworkAttachmentType_Internal);
        m_pAttachmentTypeComboBox->setItemData(iAttachmentTypeIndex, m_pAttachmentTypeComboBox->itemText(iAttachmentTypeIndex), Qt::ToolTipRole);
        ++iAttachmentTypeIndex;
        m_pAttachmentTypeComboBox->insertItem(iAttachmentTypeIndex, gpConverter->toString(KNetworkAttachmentType_HostOnly));
        m_pAttachmentTypeComboBox->setItemData(iAttachmentTypeIndex, KNetworkAttachmentType_HostOnly);
        m_pAttachmentTypeComboBox->setItemData(iAttachmentTypeIndex, m_pAttachmentTypeComboBox->itemText(iAttachmentTypeIndex), Qt::ToolTipRole);
        ++iAttachmentTypeIndex;
        m_pAttachmentTypeComboBox->insertItem(iAttachmentTypeIndex, gpConverter->toString(KNetworkAttachmentType_Generic));
        m_pAttachmentTypeComboBox->setItemData(iAttachmentTypeIndex, KNetworkAttachmentType_Generic);
        m_pAttachmentTypeComboBox->setItemData(iAttachmentTypeIndex, m_pAttachmentTypeComboBox->itemText(iAttachmentTypeIndex), Qt::ToolTipRole);
        ++iAttachmentTypeIndex;

        /* Restore the previously selected attachment type: */
        m_pAttachmentTypeComboBox->setCurrentIndex(iCurrentAttachment == -1 ? 0 : iCurrentAttachment);
    }

    /* Adapter type: */
    {
        /* Remember the currently selected adapter type: */
        int iCurrentAdapter = m_pAdapterTypeCombo->currentIndex();

        /* Clear the adapter type combo-box: */
        m_pAdapterTypeCombo->clear();

        /* Populate adapter types: */
        int iAdapterTypeIndex = 0;
        m_pAdapterTypeCombo->insertItem(iAdapterTypeIndex, gpConverter->toString(KNetworkAdapterType_Am79C970A));
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, KNetworkAdapterType_Am79C970A);
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, m_pAdapterTypeCombo->itemText(iAdapterTypeIndex), Qt::ToolTipRole);
        ++iAdapterTypeIndex;
        m_pAdapterTypeCombo->insertItem(iAdapterTypeIndex, gpConverter->toString(KNetworkAdapterType_Am79C973));
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, KNetworkAdapterType_Am79C973);
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, m_pAdapterTypeCombo->itemText(iAdapterTypeIndex), Qt::ToolTipRole);
        ++iAdapterTypeIndex;
#ifdef VBOX_WITH_E1000
        m_pAdapterTypeCombo->insertItem(iAdapterTypeIndex, gpConverter->toString(KNetworkAdapterType_I82540EM));
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, KNetworkAdapterType_I82540EM);
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, m_pAdapterTypeCombo->itemText(iAdapterTypeIndex), Qt::ToolTipRole);
        ++iAdapterTypeIndex;
        m_pAdapterTypeCombo->insertItem(iAdapterTypeIndex, gpConverter->toString(KNetworkAdapterType_I82543GC));
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, KNetworkAdapterType_I82543GC);
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, m_pAdapterTypeCombo->itemText(iAdapterTypeIndex), Qt::ToolTipRole);
        ++iAdapterTypeIndex;
        m_pAdapterTypeCombo->insertItem(iAdapterTypeIndex, gpConverter->toString(KNetworkAdapterType_I82545EM));
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, KNetworkAdapterType_I82545EM);
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, m_pAdapterTypeCombo->itemText(iAdapterTypeIndex), Qt::ToolTipRole);
        ++iAdapterTypeIndex;
#endif /* VBOX_WITH_E1000 */
#ifdef VBOX_WITH_VIRTIO
        m_pAdapterTypeCombo->insertItem(iAdapterTypeIndex, gpConverter->toString(KNetworkAdapterType_Virtio));
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, KNetworkAdapterType_Virtio);
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, m_pAdapterTypeCombo->itemText(iAdapterTypeIndex), Qt::ToolTipRole);
        ++iAdapterTypeIndex;
#endif /* VBOX_WITH_VIRTIO */

        /* Restore the previously selected adapter type: */
        m_pAdapterTypeCombo->setCurrentIndex(iCurrentAdapter == -1 ? 0 : iCurrentAdapter);
    }

    /* Promiscuous Mode type: */
    {
        /* Remember the currently selected promiscuous mode type: */
        int iCurrentPromiscuousMode = m_pPromiscuousModeCombo->currentIndex();

        /* Clear the promiscuous mode combo-box: */
        m_pPromiscuousModeCombo->clear();

        /* Populate promiscuous modes: */
        int iPromiscuousModeIndex = 0;
        m_pPromiscuousModeCombo->insertItem(iPromiscuousModeIndex, gpConverter->toString(KNetworkAdapterPromiscModePolicy_Deny));
        m_pPromiscuousModeCombo->setItemData(iPromiscuousModeIndex, KNetworkAdapterPromiscModePolicy_Deny);
        m_pPromiscuousModeCombo->setItemData(iPromiscuousModeIndex, m_pPromiscuousModeCombo->itemText(iPromiscuousModeIndex), Qt::ToolTipRole);
        ++iPromiscuousModeIndex;
        m_pPromiscuousModeCombo->insertItem(iPromiscuousModeIndex, gpConverter->toString(KNetworkAdapterPromiscModePolicy_AllowNetwork));
        m_pPromiscuousModeCombo->setItemData(iPromiscuousModeIndex, KNetworkAdapterPromiscModePolicy_AllowNetwork);
        m_pPromiscuousModeCombo->setItemData(iPromiscuousModeIndex, m_pPromiscuousModeCombo->itemText(iPromiscuousModeIndex), Qt::ToolTipRole);
        ++iPromiscuousModeIndex;
        m_pPromiscuousModeCombo->insertItem(iPromiscuousModeIndex, gpConverter->toString(KNetworkAdapterPromiscModePolicy_AllowAll));
        m_pPromiscuousModeCombo->setItemData(iPromiscuousModeIndex, KNetworkAdapterPromiscModePolicy_AllowAll);
        m_pPromiscuousModeCombo->setItemData(iPromiscuousModeIndex, m_pPromiscuousModeCombo->itemText(iPromiscuousModeIndex), Qt::ToolTipRole);
        ++iPromiscuousModeIndex;

        /* Restore the previously selected promiscuous mode type: */
        m_pPromiscuousModeCombo->setCurrentIndex(iCurrentPromiscuousMode == -1 ? 0 : iCurrentPromiscuousMode);
    }
}

void UIMachineSettingsNetwork::updateAlternativeList()
{
    /* Block signals initially: */
    m_pAdapterNameCombo->blockSignals(true);

    /* Repopulate alternative-name combo: */
    m_pAdapterNameCombo->clear();
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
            m_pAdapterNameCombo->insertItems(0, m_pParent->bridgedAdapterList());
            break;
        case KNetworkAttachmentType_Internal:
            m_pAdapterNameCombo->insertItems(0, m_pParent->internalNetworkList());
            break;
        case KNetworkAttachmentType_HostOnly:
            m_pAdapterNameCombo->insertItems(0, m_pParent->hostInterfaceList());
            break;
        case KNetworkAttachmentType_Generic:
            m_pAdapterNameCombo->insertItems(0, m_pParent->genericDriverList());
            break;
        case KNetworkAttachmentType_NATNetwork:
            m_pAdapterNameCombo->insertItems(0, m_pParent->natNetworkList());
            break;
        default:
            break;
    }

    /* Prepend 'empty' or 'default' item to alternative-name combo: */
    if (m_pAdapterNameCombo->count() == 0)
    {
        switch (attachmentType())
        {
            case KNetworkAttachmentType_Bridged:
            case KNetworkAttachmentType_HostOnly:
            case KNetworkAttachmentType_NATNetwork:
            {
                /* If adapter list is empty => add 'Not selected' item: */
                int pos = m_pAdapterNameCombo->findData(pEmptyItemCode);
                if (pos == -1)
                    m_pAdapterNameCombo->insertItem(0, tr("Not selected", "network adapter name"), pEmptyItemCode);
                else
                    m_pAdapterNameCombo->setItemText(pos, tr("Not selected", "network adapter name"));
                break;
            }
            case KNetworkAttachmentType_Internal:
            {
                /* Internal network list should have a default item: */
                if (m_pAdapterNameCombo->findText("intnet") == -1)
                    m_pAdapterNameCombo->insertItem(0, "intnet");
                break;
            }
            default:
                break;
        }
    }

    /* Unblock signals finally: */
    m_pAdapterNameCombo->blockSignals(false);
}

void UIMachineSettingsNetwork::updateAlternativeName()
{
    /* Block signals initially: */
    m_pAdapterNameCombo->blockSignals(true);

    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
        case KNetworkAttachmentType_Internal:
        case KNetworkAttachmentType_HostOnly:
        case KNetworkAttachmentType_Generic:
        case KNetworkAttachmentType_NATNetwork:
        {
            m_pAdapterNameCombo->setCurrentIndex(position(m_pAdapterNameCombo, alternativeName()));
            break;
        }
        default:
            break;
    }

    /* Unblock signals finally: */
    m_pAdapterNameCombo->blockSignals(false);
}

void UIMachineSettingsNetwork::handleAdvancedButtonStateChange()
{
    /* Update visibility of advanced options: */
    m_pAdapterTypeLabel->setVisible(m_pAdvancedArrow->isExpanded());
    m_pAdapterTypeCombo->setVisible(m_pAdvancedArrow->isExpanded());
    m_pPromiscuousModeLabel->setVisible(m_pAdvancedArrow->isExpanded());
    m_pPromiscuousModeCombo->setVisible(m_pAdvancedArrow->isExpanded());
    m_pGenericPropertiesLabel->setVisible(attachmentType() == KNetworkAttachmentType_Generic &&
                                          m_pAdvancedArrow->isExpanded());
    m_pGenericPropertiesTextEdit->setVisible(attachmentType() == KNetworkAttachmentType_Generic &&
                                             m_pAdvancedArrow->isExpanded());
    m_pMACLabel->setVisible(m_pAdvancedArrow->isExpanded());
    m_pMACEditor->setVisible(m_pAdvancedArrow->isExpanded());
    m_pMACButton->setVisible(m_pAdvancedArrow->isExpanded());
    m_pCableConnectedCheckBox->setVisible(m_pAdvancedArrow->isExpanded());
    m_pPortForwardingButton->setVisible(m_pAdvancedArrow->isExpanded());
}

/* static */
int UIMachineSettingsNetwork::position(QComboBox *pComboBox, int iData)
{
    int iPosition = pComboBox->findData(iData);
    return iPosition == -1 ? 0 : iPosition;
}

/* static */
int UIMachineSettingsNetwork::position(QComboBox *pComboBox, const QString &strText)
{
    int iPosition = pComboBox->findText(strText);
    return iPosition == -1 ? 0 : iPosition;
}

/* UIMachineSettingsNetworkPage Stuff: */
UIMachineSettingsNetworkPage::UIMachineSettingsNetworkPage()
    : m_pTwAdapters(0)
{
    /* Setup main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setContentsMargins(0, 5, 0, 5);

    /* Creating tab-widget: */
    m_pTwAdapters = new QITabWidget(this);
    pMainLayout->addWidget(m_pTwAdapters);

    /* How many adapters to display: */
    /** @todo r=klaus this needs to be done based on the actual chipset type of the VM,
     * but in this place the m_machine field isn't set yet. My observation (on Linux)
     * is that the limitation to 4 isn't necessary any more, but this needs to be checked
     * on all platforms to be certain that it's usable everywhere. */
    ulong uCount = qMin((ULONG)4, vboxGlobal().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(KChipsetType_PIIX3));
    /* Add corresponding tab pages to parent tab widget: */
    for (ulong uSlot = 0; uSlot < uCount; ++uSlot)
    {
        /* Creating adapter tab: */
        UIMachineSettingsNetwork *pTab = new UIMachineSettingsNetwork(this);
        connect(pTab, SIGNAL(sigNotifyAdvancedButtonStateChange(bool)),
                this, SLOT(sltHandleAdvancedButtonStateChange(bool)));
        m_pTwAdapters->addTab(pTab, pTab->tabTitle());
    }
}

void UIMachineSettingsNetworkPage::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_cache.clear();

    /* Cache name lists: */
    refreshBridgedAdapterList();
    refreshInternalNetworkList(true);
    refreshHostInterfaceList();
    refreshGenericDriverList(true);
    refreshNATNetworkList();

    /* For each network adapter: */
    for (int iSlot = 0; iSlot < m_pTwAdapters->count(); ++iSlot)
    {
        /* Prepare adapter data: */
        UIDataSettingsMachineNetworkAdapter adapterData;

        /* Check if adapter is valid: */
        const CNetworkAdapter &adapter = m_machine.GetNetworkAdapter(iSlot);
        if (!adapter.isNull())
        {
            /* Gather main options: */
            adapterData.m_iSlot = iSlot;
            adapterData.m_fAdapterEnabled = adapter.GetEnabled();
            adapterData.m_attachmentType = adapter.GetAttachmentType();
            adapterData.m_strBridgedAdapterName = wipedOutString(adapter.GetBridgedInterface());
            adapterData.m_strInternalNetworkName = wipedOutString(adapter.GetInternalNetwork());
            adapterData.m_strHostInterfaceName = wipedOutString(adapter.GetHostOnlyInterface());
            adapterData.m_strGenericDriverName = wipedOutString(adapter.GetGenericDriver());
            adapterData.m_strNATNetworkName = wipedOutString(adapter.GetNATNetwork());

            /* Gather advanced options: */
            adapterData.m_adapterType = adapter.GetAdapterType();
            adapterData.m_promiscuousMode = adapter.GetPromiscModePolicy();
            adapterData.m_strMACAddress = adapter.GetMACAddress();
            adapterData.m_strGenericProperties = summarizeGenericProperties(adapter);
            adapterData.m_fCableConnected = adapter.GetCableConnected();

            /* Gather redirect options: */
            QVector<QString> redirects = adapter.GetNATEngine().GetRedirects();
            for (int i = 0; i < redirects.size(); ++i)
            {
                QStringList redirectData = redirects[i].split(',');
                AssertMsg(redirectData.size() == 6, ("Redirect rule should be composed of 6 parts!\n"));
                adapterData.m_redirects << UIPortForwardingData(redirectData[0],
                                                                (KNATProtocol)redirectData[1].toUInt(),
                                                                redirectData[2],
                                                                redirectData[3].toUInt(),
                                                                redirectData[4],
                                                                redirectData[5].toUInt());
            }
        }

        /* Cache adapter data: */
        m_cache.child(iSlot).cacheInitialData(adapterData);
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsNetworkPage::getFromCache()
{
    /* Setup tab order: */
    Assert(firstWidget());
    setTabOrder(firstWidget(), m_pTwAdapters->focusProxy());
    QWidget *pLastFocusWidget = m_pTwAdapters->focusProxy();

    /* For each network adapter: */
    for (int iSlot = 0; iSlot < m_pTwAdapters->count(); ++iSlot)
    {
        /* Get adapter page: */
        UIMachineSettingsNetwork *pTab = qobject_cast<UIMachineSettingsNetwork*>(m_pTwAdapters->widget(iSlot));

        /* Load adapter data to page: */
        pTab->fetchAdapterCache(m_cache.child(iSlot));

        /* Setup tab order: */
        pLastFocusWidget = pTab->setOrderAfter(pLastFocusWidget);
    }

    /* Applying language settings: */
    retranslateUi();

    /* Polish page finally: */
    polishPage();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsNetworkPage::putToCache()
{
    /* For each network adapter: */
    for (int iSlot = 0; iSlot < m_pTwAdapters->count(); ++iSlot)
    {
        /* Get adapter page: */
        UIMachineSettingsNetwork *pTab = qobject_cast<UIMachineSettingsNetwork*>(m_pTwAdapters->widget(iSlot));

        /* Gather & cache adapter data: */
        pTab->uploadAdapterCache(m_cache.child(iSlot));
    }
}

void UIMachineSettingsNetworkPage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Check if network data was changed: */
    if (m_cache.wasChanged())
    {
        /* For each network adapter: */
        for (int iSlot = 0; iSlot < m_pTwAdapters->count(); ++iSlot)
        {
            /* Check if adapter data was changed: */
            const UISettingsCacheMachineNetworkAdapter &adapterCache = m_cache.child(iSlot);
            if (adapterCache.wasChanged())
            {
                /* Check if adapter still valid: */
                CNetworkAdapter adapter = m_machine.GetNetworkAdapter(iSlot);
                if (!adapter.isNull())
                {
                    /* Get adapter data from cache: */
                    const UIDataSettingsMachineNetworkAdapter &adapterData = adapterCache.data();

                    /* Store adapter data: */
                    if (isMachineOffline())
                    {
                        /* Basic attributes: */
                        adapter.SetEnabled(adapterData.m_fAdapterEnabled);
                        adapter.SetAdapterType(adapterData.m_adapterType);
                        adapter.SetMACAddress(adapterData.m_strMACAddress);
                    }
                    if (isMachineInValidMode())
                    {
                        /* Attachment type: */
                        switch (adapterData.m_attachmentType)
                        {
                            case KNetworkAttachmentType_Bridged:
                                adapter.SetBridgedInterface(adapterData.m_strBridgedAdapterName);
                                break;
                            case KNetworkAttachmentType_Internal:
                                adapter.SetInternalNetwork(adapterData.m_strInternalNetworkName);
                                break;
                            case KNetworkAttachmentType_HostOnly:
                                adapter.SetHostOnlyInterface(adapterData.m_strHostInterfaceName);
                                break;
                            case KNetworkAttachmentType_Generic:
                                adapter.SetGenericDriver(adapterData.m_strGenericDriverName);
                                updateGenericProperties(adapter, adapterData.m_strGenericProperties);
                                break;
                            case KNetworkAttachmentType_NATNetwork:
                                adapter.SetNATNetwork(adapterData.m_strNATNetworkName);
                                break;
                            default:
                                break;
                        }
                        adapter.SetAttachmentType(adapterData.m_attachmentType);
                        /* Advanced attributes: */
                        adapter.SetPromiscModePolicy(adapterData.m_promiscuousMode);
                        /* Cable connected flag: */
                        adapter.SetCableConnected(adapterData.m_fCableConnected);
                        /* Redirect options: */
                        if (adapterCache.base().m_attachmentType == KNetworkAttachmentType_NAT ||
                            adapterCache.data().m_attachmentType == KNetworkAttachmentType_NAT)
                        {
                            QVector<QString> oldRedirects = adapter.GetNATEngine().GetRedirects();
                            for (int i = 0; i < oldRedirects.size(); ++i)
                                adapter.GetNATEngine().RemoveRedirect(oldRedirects[i].section(',', 0, 0));
                            UIPortForwardingDataList newRedirects = adapterData.m_redirects;
                            for (int i = 0; i < newRedirects.size(); ++i)
                            {
                                UIPortForwardingData newRedirect = newRedirects[i];
                                adapter.GetNATEngine().AddRedirect(newRedirect.name, newRedirect.protocol,
                                                                   newRedirect.hostIp, newRedirect.hostPort.value(),
                                                                   newRedirect.guestIp, newRedirect.guestPort.value());
                            }
                        }
                    }
                }
            }
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

bool UIMachineSettingsNetworkPage::validate(QList<UIValidationMessage> &messages)
{
    /* Pass by default: */
    bool fValid = true;

    /* Delegate validation to adapter tabs: */
    for (int i = 0; i < m_pTwAdapters->count(); ++i)
    {
        UIMachineSettingsNetwork *pTab = qobject_cast<UIMachineSettingsNetwork*>(m_pTwAdapters->widget(i));
        AssertMsg(pTab, ("Can't get adapter tab!\n"));
        if (!pTab->validate(messages))
            fValid = false;
    }

    /* Return result: */
    return fValid;
}

void UIMachineSettingsNetworkPage::retranslateUi()
{
    for (int i = 0; i < m_pTwAdapters->count(); ++ i)
    {
        UIMachineSettingsNetwork *pTab = qobject_cast<UIMachineSettingsNetwork*>(m_pTwAdapters->widget(i));
        Assert(pTab);
        m_pTwAdapters->setTabText(i, pTab->tabTitle());
    }
}

void UIMachineSettingsNetworkPage::sltHandleUpdatedTab()
{
    /* Determine the sender: */
    UIMachineSettingsNetwork *pSender = qobject_cast<UIMachineSettingsNetwork*>(sender());
    AssertMsg(pSender, ("This slot should be called only through signal<->slot mechanism from one of UIMachineSettingsNetwork tabs!\n"));

    /* Determine sender's attachment type: */
    KNetworkAttachmentType senderAttachmentType = pSender->attachmentType();
    switch (senderAttachmentType)
    {
        case KNetworkAttachmentType_Internal:
        {
            refreshInternalNetworkList();
            break;
        }
        case KNetworkAttachmentType_Generic:
        {
            refreshGenericDriverList();
            break;
        }
        default:
            break;
    }

    /* Update all the tabs except the sender: */
    for (int iSlot = 0; iSlot < m_pTwAdapters->count(); ++iSlot)
    {
        /* Get the iterated tab: */
        UIMachineSettingsNetwork *pTab = qobject_cast<UIMachineSettingsNetwork*>(m_pTwAdapters->widget(iSlot));
        AssertMsg(pTab, ("All the tabs of m_pTwAdapters should be of the UIMachineSettingsNetwork type!\n"));

        /* Update all the tabs (except sender) with the same attachment type as sender have: */
        if (pTab != pSender && pTab->attachmentType() == senderAttachmentType)
            pTab->reloadAlternative();
    }
}

void UIMachineSettingsNetworkPage::sltHandleAdvancedButtonStateChange(bool fExpanded)
{
    /* Update the advanced button states for all the pages: */
    for (int iSlot = 0; iSlot < m_pTwAdapters->count(); ++iSlot)
    {
        UIMachineSettingsNetwork *pTab = qobject_cast<UIMachineSettingsNetwork*>(m_pTwAdapters->widget(iSlot));
        pTab->setAdvancedButtonState(fExpanded);
    }
}

void UIMachineSettingsNetworkPage::polishPage()
{
    /* Get the count of network adapter tabs: */
    for (int iSlot = 0; iSlot < m_pTwAdapters->count(); ++iSlot)
    {
        m_pTwAdapters->setTabEnabled(iSlot,
                                     isMachineOffline() ||
                                     (isMachineInValidMode() && m_cache.child(iSlot).base().m_fAdapterEnabled));
        UIMachineSettingsNetwork *pTab = qobject_cast<UIMachineSettingsNetwork*>(m_pTwAdapters->widget(iSlot));
        pTab->polishTab();
    }
}

void UIMachineSettingsNetworkPage::refreshBridgedAdapterList()
{
    /* Reload bridged interface list: */
    m_bridgedAdapterList.clear();
    const CHostNetworkInterfaceVector &ifaces = vboxGlobal().host().GetNetworkInterfaces();
    for (int i = 0; i < ifaces.size(); ++i)
    {
        const CHostNetworkInterface &iface = ifaces[i];
        if (iface.GetInterfaceType() == KHostNetworkInterfaceType_Bridged && !m_bridgedAdapterList.contains(iface.GetName()))
            m_bridgedAdapterList << iface.GetName();
    }
}

void UIMachineSettingsNetworkPage::refreshInternalNetworkList(bool fFullRefresh /* = false */)
{
    /* Reload internal network list: */
    m_internalNetworkList.clear();
    /* Get internal network names from other VMs: */
    if (fFullRefresh)
        m_internalNetworkList << otherInternalNetworkList();
    /* Append internal network list with names from all the tabs: */
    for (int iTab = 0; iTab < m_pTwAdapters->count(); ++iTab)
    {
        UIMachineSettingsNetwork *pTab = qobject_cast<UIMachineSettingsNetwork*>(m_pTwAdapters->widget(iTab));
        if (pTab)
        {
            QString strName = pTab->alternativeName(KNetworkAttachmentType_Internal);
            if (!strName.isEmpty() && !m_internalNetworkList.contains(strName))
                m_internalNetworkList << strName;
        }
    }
}

void UIMachineSettingsNetworkPage::refreshHostInterfaceList()
{
    /* Reload host-only interface list: */
    m_hostInterfaceList.clear();
    const CHostNetworkInterfaceVector &ifaces = vboxGlobal().host().GetNetworkInterfaces();
    for (int i = 0; i < ifaces.size(); ++i)
    {
        const CHostNetworkInterface &iface = ifaces[i];
        if (iface.GetInterfaceType() == KHostNetworkInterfaceType_HostOnly && !m_hostInterfaceList.contains(iface.GetName()))
            m_hostInterfaceList << iface.GetName();
    }
}

void UIMachineSettingsNetworkPage::refreshGenericDriverList(bool fFullRefresh /* = false */)
{
    /* Load generic driver list: */
    m_genericDriverList.clear();
    /* Get generic driver names from other VMs: */
    if (fFullRefresh)
        m_genericDriverList << otherGenericDriverList();
    /* Append generic driver list with names from all the tabs: */
    for (int iTab = 0; iTab < m_pTwAdapters->count(); ++iTab)
    {
        UIMachineSettingsNetwork *pTab = qobject_cast<UIMachineSettingsNetwork*>(m_pTwAdapters->widget(iTab));
        if (pTab)
        {
            QString strName = pTab->alternativeName(KNetworkAttachmentType_Generic);
            if (!strName.isEmpty() && !m_genericDriverList.contains(strName))
                m_genericDriverList << strName;
        }
    }
}

void UIMachineSettingsNetworkPage::refreshNATNetworkList()
{
    /* Reload NAT network list: */
    m_natNetworkList.clear();
    const CNATNetworkVector &nws = vboxGlobal().virtualBox().GetNATNetworks();
    for (int i = 0; i < nws.size(); ++i)
    {
        const CNATNetwork &nw = nws[i];
        m_natNetworkList << nw.GetNetworkName();
    }
}

/* static */
QStringList UIMachineSettingsNetworkPage::otherInternalNetworkList()
{
    /* Load total internal network list of all VMs: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QStringList otherInternalNetworks(QList<QString>::fromVector(vbox.GetInternalNetworks()));
    return otherInternalNetworks;
}

/* static */
QStringList UIMachineSettingsNetworkPage::otherGenericDriverList()
{
    /* Load total generic driver list of all VMs: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QStringList otherGenericDrivers(QList<QString>::fromVector(vbox.GetGenericNetworkDrivers()));
    return otherGenericDrivers;
}

/* static */
QString UIMachineSettingsNetworkPage::summarizeGenericProperties(const CNetworkAdapter &adapter)
{
    /* Prepare formatted string: */
    QVector<QString> names;
    QVector<QString> props;
    props = adapter.GetProperties(QString(), names);
    QString strResult;
    /* Load generic properties: */
    for (int i = 0; i < names.size(); ++i)
    {
        strResult += names[i] + "=" + props[i];
        if (i < names.size() - 1)
          strResult += "\n";
    }
    /* Return formatted string: */
    return strResult;
}

/* static */
void UIMachineSettingsNetworkPage::updateGenericProperties(CNetworkAdapter &adapter, const QString &strPropText)
{
    /* Parse new properties: */
    QStringList newProps = strPropText.split("\n");
    QHash<QString, QString> hash;

    /* Save new properties: */
    for (int i = 0; i < newProps.size(); ++i)
    {
        QString strLine = newProps[i];
        int iSplitPos = strLine.indexOf("=");
        if (iSplitPos)
        {
            QString strKey = strLine.left(iSplitPos);
            QString strVal = strLine.mid(iSplitPos+1);
            adapter.SetProperty(strKey, strVal);
            hash[strKey] = strVal;
        }
    }

    /* Removing deleted properties: */
    QVector<QString> names;
    QVector<QString> props;
    props = adapter.GetProperties(QString(), names);
    for (int i = 0; i < names.size(); ++i)
    {
        QString strName = names[i];
        QString strValue = props[i];
        if (strValue != hash[strName])
            adapter.SetProperty(strName, hash[strName]);
    }
}

