/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsNetwork class implementation
 */

/*
 * Copyright (C) 2008-2011 Oracle Corporation
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
#include <QTimer>
#include <QCompleter>

/* Local includes */
#include "QIWidgetValidator.h"
#include "QIArrowButtonSwitch.h"
#include "VBoxGlobal.h"
#include "UIMachineSettingsNetwork.h"
#include "QITabWidget.h"

#ifdef VBOX_WITH_VDE
# include <iprt/ldr.h>
# include <VBox/VDEPlug.h>
#endif /* VBOX_WITH_VDE */

/* Empty item extra-code: */
const char *emptyItemCode = "#empty#";

UIMachineSettingsNetwork::UIMachineSettingsNetwork(UIMachineSettingsNetworkPage *pParent, bool fDisableStaticControls)
    : QIWithRetranslateUI<QWidget>(0)
    , m_pParent(pParent)
    , m_pValidator(0)
    , m_iSlot(-1)
    , m_fPolished(false)
    , m_fDisableStaticControls(fDisableStaticControls)
{
    /* Apply UI decorations: */
    Ui::UIMachineSettingsNetwork::setupUi(this);

    /* Setup widgets: */
    m_pAdapterNameCombo->setInsertPolicy(QComboBox::NoInsert);
    m_pMACEditor->setValidator(new QRegExpValidator(QRegExp("[0-9A-Fa-f][02468ACEace][0-9A-Fa-f]{10}"), this));
    m_pMACEditor->setMinimumWidthByText(QString().fill('0', 12));

    /* Setup connections: */
    connect(m_pAdvancedArrow, SIGNAL(clicked()), this, SLOT(sltToggleAdvanced()));
    connect(m_pMACButton, SIGNAL(clicked()), this, SLOT(sltGenerateMac()));
    connect(m_pPortForwardingButton, SIGNAL(clicked()), this, SLOT(sltOpenPortForwardingDlg()));

    /* Applying language settings: */
    retranslateUi();
}

void UIMachineSettingsNetwork::fetchAdapterData(const UINetworkAdapterData &data)
{
    /* Load adapter & slot number: */
    m_iSlot = data.m_iSlot;
    m_adapter = data.m_adapter;

    /* Load adapter activity state: */
    mCbEnableAdapter->setChecked(data.m_fAdapterEnabled);

    /* Load attachment type: */
    int iAttachmentPos = m_pAttachmentTypeCombo->findData(data.m_attachmentType);
    m_pAttachmentTypeCombo->setCurrentIndex(iAttachmentPos == -1 ? 0 : iAttachmentPos);

    /* Load alternative name: */
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
            m_strBrgName = data.m_strBridgedAdapterName;
            if (m_strBrgName.isEmpty()) m_strBrgName = QString();
            break;
        case KNetworkAttachmentType_Internal:
            m_strIntName = data.m_strInternalNetworkName;
            if (m_strIntName.isEmpty()) m_strIntName = QString();
            break;
        case KNetworkAttachmentType_HostOnly:
            m_strHoiName = data.m_strHostInterfaceName;
            if (m_strHoiName.isEmpty()) m_strHoiName = QString();
            break;
#ifdef VBOX_WITH_VDE
        case KNetworkAttachmentType_VDE:
            mVDEName = data.m_strVDENetworkName;
            if (mVDEName.isEmpty()) mVDEName = QString();
            break;
#endif
        default:
            break;
    }
    sltUpdateAttachmentAlternative();

    /* Load adapter type: */
    int iAdapterPos = m_pAdapterTypeCombo->findData(data.m_adapterType);
    m_pAdapterTypeCombo->setCurrentIndex(iAdapterPos == -1 ? 0 : iAdapterPos);

    /* Load promiscuous mode type: */
    int iPromiscuousModePos = m_pPromiscuousModeCombo->findData(data.m_promiscuousMode);
    m_pPromiscuousModeCombo->setCurrentIndex(iPromiscuousModePos == -1 ? 0 : iPromiscuousModePos);

    /* Other options: */
    m_pMACEditor->setText(data.m_strMACAddress);
    m_pCableConnectedCheckBox->setChecked(data.m_fCableConnected);

    /* Load port forwarding rules: */
    m_portForwardingRules = data.m_redirects;
}

void UIMachineSettingsNetwork::uploadAdapterData(UINetworkAdapterData &data)
{
    /* Save adapter activity state: */
    data.m_fAdapterEnabled = mCbEnableAdapter->isChecked();

    /* Save attachment type & alternative name: */
    data.m_attachmentType = attachmentType();
    switch (data.m_attachmentType)
    {
        case KNetworkAttachmentType_Null:
            break;
        case KNetworkAttachmentType_NAT:
            break;
        case KNetworkAttachmentType_Bridged:
            data.m_strBridgedAdapterName = alternativeName();
            break;
        case KNetworkAttachmentType_Internal:
            data.m_strInternalNetworkName = alternativeName();
            break;
        case KNetworkAttachmentType_HostOnly:
            data.m_strHostInterfaceName = alternativeName();
            break;
#ifdef VBOX_WITH_VDE
        case KNetworkAttachmentType_VDE:
            data.m_strVDENetworkName = alternativeName();
            break;
#endif
        default:
            break;
    }

    /* Save adapter type: */
    data.m_adapterType = (KNetworkAdapterType)m_pAdapterTypeCombo->itemData(m_pAdapterTypeCombo->currentIndex()).toInt();

    /* Save promiscuous mode type: */
    data.m_promiscuousMode = (KNetworkAdapterPromiscModePolicy)m_pPromiscuousModeCombo->itemData(m_pPromiscuousModeCombo->currentIndex()).toInt();

    /* Other options: */
    data.m_strMACAddress = m_pMACEditor->text().isEmpty() ? QString() : m_pMACEditor->text();
    data.m_fCableConnected = m_pCableConnectedCheckBox->isChecked();

    /* Save port forwarding rules: */
    data.m_redirects = m_portForwardingRules;
}

void UIMachineSettingsNetwork::setValidator(QIWidgetValidator *pValidator)
{
    m_pValidator = pValidator;

    if (!m_fDisableStaticControls)
        connect(mCbEnableAdapter, SIGNAL(toggled(bool)), m_pValidator, SLOT(revalidate()));
    connect(m_pAttachmentTypeCombo, SIGNAL(activated(const QString&)), this, SLOT(sltUpdateAttachmentAlternative()));
    connect(m_pAdapterNameCombo, SIGNAL(activated(const QString&)), this, SLOT(sltUpdateAlternativeName()));
    connect(m_pAdapterNameCombo, SIGNAL(editTextChanged(const QString&)), this, SLOT(sltUpdateAlternativeName()));

    if (!m_fDisableStaticControls)
        m_pValidator->revalidate();
}

bool UIMachineSettingsNetwork::revalidate(QString &strWarning, QString &strTitle)
{
    /* 'True' for disabled adapter: */
    if (!mCbEnableAdapter->isChecked())
        return true;

    /* Validate alternatives: */
    bool fValid = true;
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
            if (alternativeName().isNull())
            {
                strWarning = tr("no bridged network adapter is selected");
                fValid = false;
            }
            break;
        case KNetworkAttachmentType_Internal:
            if (alternativeName().isNull())
            {
                strWarning = tr("no internal network name is specified");
                fValid = false;
            }
            break;
        case KNetworkAttachmentType_HostOnly:
            if (alternativeName().isNull())
            {
                strWarning = tr("no host-only network adapter is selected");
                fValid = false;
            }
            break;
        default:
            break;
    }
    if (!fValid)
        strTitle += ": " + vboxGlobal().removeAccelMark(pageTitle());

    return fValid;
}

QWidget* UIMachineSettingsNetwork::setOrderAfter(QWidget *pAfter)
{
    setTabOrder(pAfter, mCbEnableAdapter);
    setTabOrder(mCbEnableAdapter, m_pAttachmentTypeCombo);
    setTabOrder(m_pAttachmentTypeCombo, m_pAdapterNameCombo);
    setTabOrder(m_pAdapterNameCombo, m_pAdvancedArrow);
    setTabOrder(m_pAdvancedArrow, m_pAdapterTypeCombo);
    setTabOrder(m_pAdapterTypeCombo, m_pPromiscuousModeCombo);
    setTabOrder(m_pPromiscuousModeCombo, m_pMACEditor);
    setTabOrder(m_pMACEditor, m_pMACButton);
    setTabOrder(m_pMACButton, m_pCableConnectedCheckBox);
    setTabOrder(m_pCableConnectedCheckBox, m_pPortForwardingButton);
    return m_pPortForwardingButton;
}

QString UIMachineSettingsNetwork::pageTitle() const
{
    return VBoxGlobal::tr("Adapter %1", "network").arg(QString("&%1").arg(m_iSlot + 1));;
}

KNetworkAttachmentType UIMachineSettingsNetwork::attachmentType() const
{
    return (KNetworkAttachmentType)m_pAttachmentTypeCombo->itemData(m_pAttachmentTypeCombo->currentIndex()).toInt();
}

QString UIMachineSettingsNetwork::alternativeName(int type) const
{
    if (type == -1)
        type = attachmentType();
    QString strResult;
    switch (type)
    {
        case KNetworkAttachmentType_Bridged:
            strResult = m_strBrgName;
            break;
        case KNetworkAttachmentType_Internal:
            strResult = m_strIntName;
            break;
        case KNetworkAttachmentType_HostOnly:
            strResult = m_strHoiName;
            break;
#ifdef VBOX_WITH_VDE
        case KNetworkAttachmentType_VDE:
            strResult = mVDEName;
            break;
#endif
        default:
            break;
    }
    Assert(strResult.isNull() || !strResult.isEmpty());
    return strResult;
}

void UIMachineSettingsNetwork::showEvent(QShowEvent *pEvent)
{
    if (!m_fPolished)
    {
        m_fPolished = true;

        /* Give the minimum size hint to the first layout column: */
        m_pNetworkChildGridLayout->setColumnMinimumWidth (0, m_pAttachmentTypeLabel->width());

        if (m_fDisableStaticControls)
        {
            /* Disable controls for dynamically displayed page: */
            mCbEnableAdapter->setEnabled(false);
            m_pAdapterTypeCombo->setEnabled(false);
            m_pPromiscuousModeCombo->setEnabled(false);
            m_pMACEditor->setEnabled(false);
            m_pMACButton->setEnabled(false);
            m_pAdapterTypeLabel->setEnabled(false);
            m_pPromiscuousModeLabel->setEnabled(false);
            m_pMACLabel->setEnabled(false);
            m_pAdvancedArrow->animateClick();
        }
        else
        {
            /* Hide advanced items initially: */
            sltToggleAdvanced();
        }
    }
    QWidget::showEvent(pEvent);
}

void UIMachineSettingsNetwork::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIMachineSettingsNetwork::retranslateUi(this);

    /* Translate combo-boxes content: */
    populateComboboxes();

    /* Translate attachment info: */
    sltUpdateAttachmentAlternative();
}

void UIMachineSettingsNetwork::sltUpdateAttachmentAlternative()
{
    /* Blocking signals to change content manually: */
    m_pAdapterNameCombo->blockSignals(true);

    /* Update alternative-name combo-box availability: */
    m_pAdapterNameCombo->setEnabled(attachmentType() != KNetworkAttachmentType_Null &&
                                    attachmentType() != KNetworkAttachmentType_NAT);
    m_pPromiscuousModeCombo->setEnabled(attachmentType() != KNetworkAttachmentType_Null &&
                                        attachmentType() != KNetworkAttachmentType_NAT);

    /* Refresh list: */
    m_pAdapterNameCombo->clear();
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
            m_pAdapterNameCombo->insertItems(0, m_pParent->brgList());
            m_pAdapterNameCombo->setEditable(false);
            break;
        case KNetworkAttachmentType_Internal:
            m_pAdapterNameCombo->insertItems(0, m_pParent->fullIntList());
            m_pAdapterNameCombo->setEditable(true);
            m_pAdapterNameCombo->setCompleter(0);
            break;
        case KNetworkAttachmentType_HostOnly:
            m_pAdapterNameCombo->insertItems(0, m_pParent->hoiList());
            m_pAdapterNameCombo->setEditable(false);
            break;
#ifdef VBOX_WITH_VDE
        case KNetworkAttachmentType_VDE:
            m_pAdapterNameCombo->insertItem(0, alternativeName());
            m_pAdapterNameCombo->setEditable(true);
            m_pAdapterNameCombo->setCompleter(0);
            break;
#endif
        default:
            break;
    }

    /* Prepend 'empty' or 'default' item: */
    if (m_pAdapterNameCombo->count() == 0)
    {
        switch (attachmentType())
        {
            case KNetworkAttachmentType_Bridged:
            case KNetworkAttachmentType_HostOnly:
            {
                /* Adapters list 'empty': */
                int pos = m_pAdapterNameCombo->findData(emptyItemCode);
                if (pos == -1)
                    m_pAdapterNameCombo->insertItem(0, tr("Not selected", "network adapter name"), emptyItemCode);
                else
                    m_pAdapterNameCombo->setItemText(pos, tr("Not selected", "network adapter name"));
                break;
            }
            case KNetworkAttachmentType_Internal:
            {
                /* Internal network 'default' name: */
                if (m_pAdapterNameCombo->findText("intnet") == -1)
                    m_pAdapterNameCombo->insertItem(0, "intnet");
                break;
            }
            default:
                break;
        }
    }

    /* Select previous or default item: */
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
        case KNetworkAttachmentType_HostOnly:
        {
            int pos = m_pAdapterNameCombo->findText(alternativeName());
            m_pAdapterNameCombo->setCurrentIndex(pos == -1 ? 0 : pos);
            break;
        }
        case KNetworkAttachmentType_Internal:
        {
            int pos = m_pAdapterNameCombo->findText(alternativeName());
            m_pAdapterNameCombo->setCurrentIndex(pos == -1 ? 0 : pos);
            break;
        }
        default:
            break;
    }

    /* Remember selected item: */
    sltUpdateAlternativeName();

    /* Update Forwarding rules button availability: */
    m_pPortForwardingButton->setEnabled(attachmentType() == KNetworkAttachmentType_NAT);

    /* Unblocking signals as content is changed already: */
    m_pAdapterNameCombo->blockSignals(false);
}

void UIMachineSettingsNetwork::sltUpdateAlternativeName()
{
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
        {
            QString newName(m_pAdapterNameCombo->itemData(m_pAdapterNameCombo->currentIndex()).toString() ==
                            QString(emptyItemCode) ||
                            m_pAdapterNameCombo->currentText().isEmpty() ?
                            QString::null : m_pAdapterNameCombo->currentText());
            if (m_strBrgName != newName)
                m_strBrgName = newName;
            break;
        }
        case KNetworkAttachmentType_Internal:
        {
            QString newName((m_pAdapterNameCombo->itemData(m_pAdapterNameCombo->currentIndex()).toString() ==
                             QString(emptyItemCode) &&
                             m_pAdapterNameCombo->currentText() ==
                             m_pAdapterNameCombo->itemText(m_pAdapterNameCombo->currentIndex())) ||
                             m_pAdapterNameCombo->currentText().isEmpty() ?
                             QString::null : m_pAdapterNameCombo->currentText());
            if (m_strIntName != newName)
            {
                m_strIntName = newName;
                if (!m_strIntName.isNull())
                    QTimer::singleShot(0, m_pParent, SLOT (updatePages()));
            }
            break;
        }
        case KNetworkAttachmentType_HostOnly:
        {
            QString newName(m_pAdapterNameCombo->itemData(m_pAdapterNameCombo->currentIndex()).toString() ==
                            QString(emptyItemCode) ||
                            m_pAdapterNameCombo->currentText().isEmpty() ?
                            QString::null : m_pAdapterNameCombo->currentText());
            if (m_strHoiName != newName)
                m_strHoiName = newName;
            break;
        }
#ifdef VBOX_WITH_VDE
        case KNetworkAttachmentType_VDE:
        {
            QString newName((m_pAdapterNameCombo->itemData(m_pAdapterNameCombo->currentIndex()).toString() ==
                             QString(emptyItemCode) &&
                             m_pAdapterNameCombo->currentText() ==
                             m_pAdapterNameCombo->itemText(m_pAdapterNameCombo->currentIndex())) ||
                             m_pAdapterNameCombo->currentText().isEmpty() ?
                             QString::null : m_pAdapterNameCombo->currentText());
            if (mVDEName != newName)
                mVDEName = newName;
            break;
        }
#endif
        default:
            break;
    }

    if (m_pValidator)
        m_pValidator->revalidate();
}

void UIMachineSettingsNetwork::sltToggleAdvanced()
{
    m_pAdapterTypeLabel->setVisible(m_pAdvancedArrow->isExpanded());
    m_pAdapterTypeCombo->setVisible(m_pAdvancedArrow->isExpanded());
    m_pPromiscuousModeLabel->setVisible(m_pAdvancedArrow->isExpanded());
    m_pPromiscuousModeCombo->setVisible(m_pAdvancedArrow->isExpanded());
    m_pMACLabel->setVisible(m_pAdvancedArrow->isExpanded());
    m_pMACEditor->setVisible(m_pAdvancedArrow->isExpanded());
    m_pMACButton->setVisible(m_pAdvancedArrow->isExpanded());
    m_pCableConnectedCheckBox->setVisible(m_pAdvancedArrow->isExpanded());
    m_pPortForwardingButton->setVisible(m_pAdvancedArrow->isExpanded());
}

void UIMachineSettingsNetwork::sltGenerateMac()
{
    m_adapter.SetMACAddress(QString::null);
    m_pMACEditor->setText(m_adapter.GetMACAddress());
}

void UIMachineSettingsNetwork::sltOpenPortForwardingDlg()
{
    UIMachineSettingsPortForwardingDlg dlg(this, m_portForwardingRules);
    if (dlg.exec() == QDialog::Accepted)
        m_portForwardingRules = dlg.rules();
}

void UIMachineSettingsNetwork::populateComboboxes()
{
    /* Attachment type: */
    {
        /* Remember the currently selected attachment type: */
        int iCurrentAttachment = m_pAttachmentTypeCombo->currentIndex();

        /* Clear the attachments combo-box: */
        m_pAttachmentTypeCombo->clear();

        /* Populate attachments: */
        int iAttachmentTypeIndex = 0;
        m_pAttachmentTypeCombo->insertItem(iAttachmentTypeIndex, vboxGlobal().toString(KNetworkAttachmentType_Null));
        m_pAttachmentTypeCombo->setItemData(iAttachmentTypeIndex, KNetworkAttachmentType_Null);
        m_pAttachmentTypeCombo->setItemData(iAttachmentTypeIndex, m_pAttachmentTypeCombo->itemText(iAttachmentTypeIndex), Qt::ToolTipRole);
        ++iAttachmentTypeIndex;
        m_pAttachmentTypeCombo->insertItem(iAttachmentTypeIndex, vboxGlobal().toString(KNetworkAttachmentType_NAT));
        m_pAttachmentTypeCombo->setItemData(iAttachmentTypeIndex, KNetworkAttachmentType_NAT);
        m_pAttachmentTypeCombo->setItemData(iAttachmentTypeIndex, m_pAttachmentTypeCombo->itemText(iAttachmentTypeIndex), Qt::ToolTipRole);
        ++iAttachmentTypeIndex;
        m_pAttachmentTypeCombo->insertItem(iAttachmentTypeIndex, vboxGlobal().toString(KNetworkAttachmentType_Bridged));
        m_pAttachmentTypeCombo->setItemData(iAttachmentTypeIndex, KNetworkAttachmentType_Bridged);
        m_pAttachmentTypeCombo->setItemData(iAttachmentTypeIndex, m_pAttachmentTypeCombo->itemText(iAttachmentTypeIndex), Qt::ToolTipRole);
        ++iAttachmentTypeIndex;
        m_pAttachmentTypeCombo->insertItem(iAttachmentTypeIndex, vboxGlobal().toString(KNetworkAttachmentType_Internal));
        m_pAttachmentTypeCombo->setItemData(iAttachmentTypeIndex, KNetworkAttachmentType_Internal);
        m_pAttachmentTypeCombo->setItemData(iAttachmentTypeIndex, m_pAttachmentTypeCombo->itemText(iAttachmentTypeIndex), Qt::ToolTipRole);
        ++iAttachmentTypeIndex;
        m_pAttachmentTypeCombo->insertItem(iAttachmentTypeIndex, vboxGlobal().toString(KNetworkAttachmentType_HostOnly));
        m_pAttachmentTypeCombo->setItemData(iAttachmentTypeIndex, KNetworkAttachmentType_HostOnly);
        m_pAttachmentTypeCombo->setItemData(iAttachmentTypeIndex, m_pAttachmentTypeCombo->itemText(iAttachmentTypeIndex), Qt::ToolTipRole);
        ++iAttachmentTypeIndex;
#ifdef VBOX_WITH_VDE
        RTLDRMOD hLdrDummy;
        if (RT_SUCCESS(RTLdrLoad(VBOX_LIB_VDE_PLUG_NAME, &hLdrDummy)))
        {
            m_pAttachmentTypeCombo->insertItem(iAttachmentTypeIndex, vboxGlobal().toString(KNetworkAttachmentType_VDE));
            m_pAttachmentTypeCombo->setItemData(iAttachmentTypeIndex, KNetworkAttachmentType_VDE);
            m_pAttachmentTypeCombo->setItemData(iAttachmentTypeIndex, m_pAttachmentTypeCombo->itemText(iAttachmentTypeIndex), Qt::ToolTipRole);
            ++iAttachmentTypeIndex;
        }
#endif

        /* Restore the previously selected attachment type: */
        m_pAttachmentTypeCombo->setCurrentIndex(iCurrentAttachment);
    }

    /* Adapter type: */
    {
        /* Remember the currently selected adapter type: */
        int iCurrentAdapter = m_pAdapterTypeCombo->currentIndex();

        /* Clear the adapter type combo-box: */
        m_pAdapterTypeCombo->clear();

        /* Populate adapter types: */
        int iAdapterTypeIndex = 0;
        m_pAdapterTypeCombo->insertItem(iAdapterTypeIndex, vboxGlobal().toString(KNetworkAdapterType_Am79C970A));
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, KNetworkAdapterType_Am79C970A);
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, m_pAdapterTypeCombo->itemText(iAdapterTypeIndex), Qt::ToolTipRole);
        ++iAdapterTypeIndex;
        m_pAdapterTypeCombo->insertItem(iAdapterTypeIndex, vboxGlobal().toString(KNetworkAdapterType_Am79C973));
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, KNetworkAdapterType_Am79C973);
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, m_pAdapterTypeCombo->itemText(iAdapterTypeIndex), Qt::ToolTipRole);
        ++iAdapterTypeIndex;
#ifdef VBOX_WITH_E1000
        m_pAdapterTypeCombo->insertItem(iAdapterTypeIndex, vboxGlobal().toString(KNetworkAdapterType_I82540EM));
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, KNetworkAdapterType_I82540EM);
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, m_pAdapterTypeCombo->itemText(iAdapterTypeIndex), Qt::ToolTipRole);
        ++iAdapterTypeIndex;
        m_pAdapterTypeCombo->insertItem(iAdapterTypeIndex, vboxGlobal().toString(KNetworkAdapterType_I82543GC));
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, KNetworkAdapterType_I82543GC);
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, m_pAdapterTypeCombo->itemText(iAdapterTypeIndex), Qt::ToolTipRole);
        ++iAdapterTypeIndex;
        m_pAdapterTypeCombo->insertItem(iAdapterTypeIndex, vboxGlobal().toString(KNetworkAdapterType_I82545EM));
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, KNetworkAdapterType_I82545EM);
        m_pAdapterTypeCombo->setItemData(iAdapterTypeIndex, m_pAdapterTypeCombo->itemText(iAdapterTypeIndex), Qt::ToolTipRole);
        ++iAdapterTypeIndex;
#endif /* VBOX_WITH_E1000 */
#ifdef VBOX_WITH_VIRTIO
        m_pAdapterTypeCombo->insertItem(iAdapterTypeIndex, vboxGlobal().toString(KNetworkAdapterType_Virtio));
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
        m_pPromiscuousModeCombo->insertItem(iPromiscuousModeIndex, vboxGlobal().toString(KNetworkAdapterPromiscModePolicy_Deny));
        m_pPromiscuousModeCombo->setItemData(iPromiscuousModeIndex, KNetworkAdapterPromiscModePolicy_Deny);
        m_pPromiscuousModeCombo->setItemData(iPromiscuousModeIndex, m_pPromiscuousModeCombo->itemText(iPromiscuousModeIndex), Qt::ToolTipRole);
        ++iPromiscuousModeIndex;
        m_pPromiscuousModeCombo->insertItem(iPromiscuousModeIndex, vboxGlobal().toString(KNetworkAdapterPromiscModePolicy_AllowNetwork));
        m_pPromiscuousModeCombo->setItemData(iPromiscuousModeIndex, KNetworkAdapterPromiscModePolicy_AllowNetwork);
        m_pPromiscuousModeCombo->setItemData(iPromiscuousModeIndex, m_pPromiscuousModeCombo->itemText(iPromiscuousModeIndex), Qt::ToolTipRole);
        ++iPromiscuousModeIndex;
        m_pPromiscuousModeCombo->insertItem(iPromiscuousModeIndex, vboxGlobal().toString(KNetworkAdapterPromiscModePolicy_AllowAll));
        m_pPromiscuousModeCombo->setItemData(iPromiscuousModeIndex, KNetworkAdapterPromiscModePolicy_AllowAll);
        m_pPromiscuousModeCombo->setItemData(iPromiscuousModeIndex, m_pPromiscuousModeCombo->itemText(iPromiscuousModeIndex), Qt::ToolTipRole);
        ++iPromiscuousModeIndex;

        /* Restore the previously selected promiscuous mode type: */
        m_pPromiscuousModeCombo->setCurrentIndex(iCurrentPromiscuousMode);
    }
}

/* UIMachineSettingsNetworkPage Stuff */
UIMachineSettingsNetworkPage::UIMachineSettingsNetworkPage(bool fDisableStaticControls)
    : m_pValidator(0)
    , m_pTwAdapters(0)
    , m_fDisableStaticControls(fDisableStaticControls)
{
    /* Setup main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setContentsMargins(0, 5, 0, 5);

    /* Creating tab-widget: */
    m_pTwAdapters = new QITabWidget(this);
    pMainLayout->addWidget(m_pTwAdapters);

    /* How many adapters to display: */
    ulong uCount = qMin((ULONG)4, vboxGlobal().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(KChipsetType_PIIX3));
    /* Add the tab pages to parent tab widget. Needed for space calculations: */
    for (ulong iSlot = 0; iSlot < uCount; ++iSlot)
    {
        /* Creating adapter's page: */
        UIMachineSettingsNetwork *pPage = new UIMachineSettingsNetwork(this, m_fDisableStaticControls);
        /* Attach adapter's page to Tab Widget: */
        m_pTwAdapters->addTab(pPage, pPage->pageTitle());
    }
}

void UIMachineSettingsNetworkPage::loadDirectlyFrom(const CMachine &machine)
{
    qRegisterMetaType<UISettingsDataMachine>();
    UISettingsDataMachine data(machine);
    QVariant wrapper = QVariant::fromValue(data);
    loadToCacheFrom(wrapper);
    getFromCache();
}

void UIMachineSettingsNetworkPage::saveDirectlyTo(CMachine &machine)
{
    qRegisterMetaType<UISettingsDataMachine>();
    UISettingsDataMachine data(machine);
    QVariant wrapper = QVariant::fromValue(data);
    putToCache();
    saveFromCacheTo(wrapper);
}

QStringList UIMachineSettingsNetworkPage::brgList(bool fRefresh)
{
    if (fRefresh)
    {
        /* Load & filter interface list: */
        m_brgList.clear();
        CHostNetworkInterfaceVector interfaces = vboxGlobal().virtualBox().GetHost().GetNetworkInterfaces();
        for (CHostNetworkInterfaceVector::ConstIterator it = interfaces.begin();
             it != interfaces.end(); ++it)
        {
            if (it->GetInterfaceType() == KHostNetworkInterfaceType_Bridged)
                m_brgList << it->GetName();
        }
    }

    return m_brgList;
}

QStringList UIMachineSettingsNetworkPage::intList(bool fRefresh)
{
    if (fRefresh)
    {
        /* Load total network list of all VMs: */
        m_intList.clear();
        CVirtualBox vbox = vboxGlobal().virtualBox();
        ulong count = qMin ((ULONG) 4, vbox.GetSystemProperties().GetMaxNetworkAdapters(KChipsetType_PIIX3));
        CMachineVector vec = vbox.GetMachines();
        for (CMachineVector::ConstIterator m = vec.begin(); m != vec.end(); ++m)
        {
            if (m->GetAccessible())
            {
                for (ulong slot = 0; slot < count; ++slot)
                {
                    QString strName = m->GetNetworkAdapter(slot).GetInternalNetwork();
                    if (!strName.isEmpty() && !m_intList.contains(strName))
                        m_intList << strName;
                }
            }
        }
    }

    return m_intList;
}

QStringList UIMachineSettingsNetworkPage::fullIntList(bool fRefresh)
{
    QStringList list(intList(fRefresh));
    /* Append network list with names from all the pages: */
    for (int index = 0; index < m_pTwAdapters->count(); ++index)
    {
        UIMachineSettingsNetwork *pPage =
            qobject_cast <UIMachineSettingsNetwork*>(m_pTwAdapters->widget(index));
        if (pPage)
        {
            QString strName = pPage->alternativeName(KNetworkAttachmentType_Internal);
            if (!strName.isEmpty() && !list.contains(strName))
                list << strName;
        }
    }
    return list;
}

QStringList UIMachineSettingsNetworkPage::hoiList(bool fRefresh)
{
    if (fRefresh)
    {
        /* Load & filter interface list: */
        m_hoiList.clear();
        CHostNetworkInterfaceVector interfaces = vboxGlobal().virtualBox().GetHost().GetNetworkInterfaces();
        for (CHostNetworkInterfaceVector::ConstIterator it = interfaces.begin();
             it != interfaces.end(); ++it)
        {
            if (it->GetInterfaceType() == KHostNetworkInterfaceType_HostOnly)
                m_hoiList << it->GetName();
        }
    }

    return m_hoiList;
}

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsNetworkPage::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Cache names lists: */
    brgList(true);
    intList(true);
    hoiList(true);

    /* Load adapters data: */
    ulong uCount = qMin((ULONG)4, vboxGlobal().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(KChipsetType_PIIX3));
    for (ulong uSlot = 0; uSlot < uCount; ++uSlot)
    {
        /* Get adapter: */
        const CNetworkAdapter &adapter = m_machine.GetNetworkAdapter(uSlot);

        /* Prepare adapter's data container: */
        UINetworkAdapterData data;

        /* Load main options: */
        data.m_iSlot = uSlot;
        data.m_adapter = adapter;
        data.m_fAdapterEnabled = adapter.GetEnabled();
        data.m_attachmentType = adapter.GetAttachmentType();
        switch (data.m_attachmentType)
        {
            case KNetworkAttachmentType_Bridged:
                data.m_strBridgedAdapterName = adapter.GetHostInterface();
                if (data.m_strBridgedAdapterName.isEmpty()) data.m_strBridgedAdapterName = QString();
                break;
            case KNetworkAttachmentType_Internal:
                data.m_strInternalNetworkName = adapter.GetInternalNetwork();
                if (data.m_strInternalNetworkName.isEmpty()) data.m_strInternalNetworkName = QString();
                break;
            case KNetworkAttachmentType_HostOnly:
                data.m_strHostInterfaceName = adapter.GetHostInterface();
                if (data.m_strHostInterfaceName.isEmpty()) data.m_strHostInterfaceName = QString();
                break;
#ifdef VBOX_WITH_VDE
            case KNetworkAttachmentType_VDE:
                data.m_strVDENetworkName = adapter.GetVDENetwork();
                if (data.m_strVDENetworkName.isEmpty()) data.m_strVDENetworkName = QString();
                break;
#endif
            default:
                break;
        }

        /* Load advanced options: */
        data.m_adapterType = adapter.GetAdapterType();
        data.m_promiscuousMode = adapter.GetPromiscModePolicy();
        data.m_strMACAddress = adapter.GetMACAddress();
        data.m_fCableConnected = adapter.GetCableConnected();

        /* Load redirect options: */
        QVector<QString> redirects = adapter.GetNatDriver().GetRedirects();
        for (int i = 0; i < redirects.size(); ++i)
        {
            QStringList redirectData = redirects[i].split(',');
            AssertMsg(redirectData.size() == 6, ("Redirect rule should be composed of 6 parts!\n"));
            data.m_redirects << UIPortForwardingData(redirectData[0],
                                                     (KNATProtocol)redirectData[1].toUInt(),
                                                     redirectData[2],
                                                     redirectData[3].toUInt(),
                                                     redirectData[4],
                                                     redirectData[5].toUInt());
        }

        /* Append adapter's data container: */
        m_cache.m_items << data;
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsNetworkPage::getFromCache()
{
    /* Setup tab order: */
    Assert(firstWidget());
    setTabOrder(firstWidget(), m_pTwAdapters->focusProxy());
    QWidget *pLastFocusWidget = m_pTwAdapters->focusProxy();

    int uCount = qMin(m_pTwAdapters->count(), m_cache.m_items.size());
    for (int iSlot = 0; iSlot < uCount; ++iSlot)
    {
        UIMachineSettingsNetwork *pPage = qobject_cast<UIMachineSettingsNetwork*>(m_pTwAdapters->widget(iSlot));
        Assert(pPage);

        /* Loading adapter's data into page: */
        pPage->fetchAdapterData(m_cache.m_items[iSlot]);

        /* Disable tab page of disabled adapter if it is being configured dynamically: */
        if (m_fDisableStaticControls && !m_cache.m_items[iSlot].m_fAdapterEnabled)
            m_pTwAdapters->setTabEnabled(iSlot, false);

        /* Setup page validation: */
        pPage->setValidator(m_pValidator);

        /* Setup tab order: */
        pLastFocusWidget = pPage->setOrderAfter(pLastFocusWidget);
    }

    /* Applying language settings: */
    retranslateUi();

    /* Revalidate if possible: */
    if (m_pValidator) m_pValidator->revalidate();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsNetworkPage::putToCache()
{
    /* Gather internal variables data from QWidget(s): */
    for (int iSlot = 0; iSlot < m_cache.m_items.size(); ++iSlot)
    {
        /* Getting adapter's page: */
        UIMachineSettingsNetwork *pPage = qobject_cast<UIMachineSettingsNetwork*>(m_pTwAdapters->widget(iSlot));

        /* Loading Adapter's data from page: */
        pPage->uploadAdapterData(m_cache.m_items[iSlot]);
    }
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsNetworkPage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Gather corresponding values from internal variables: */
    for (int iSlot = 0; iSlot < m_cache.m_items.size(); ++iSlot)
    {
        /* Get adapter: */
        CNetworkAdapter adapter = m_machine.GetNetworkAdapter(iSlot);

        /* Get cached data for this adapter: */
        const UINetworkAdapterData &data = m_cache.m_items[iSlot];

        /* Save main options: */
        adapter.SetEnabled(data.m_fAdapterEnabled);
        switch (data.m_attachmentType)
        {
            case KNetworkAttachmentType_Null:
                adapter.Detach();
                break;
            case KNetworkAttachmentType_NAT:
                adapter.AttachToNAT();
                break;
            case KNetworkAttachmentType_Bridged:
                adapter.SetHostInterface(data.m_strBridgedAdapterName);
                adapter.AttachToBridgedInterface();
                break;
            case KNetworkAttachmentType_Internal:
                adapter.SetInternalNetwork(data.m_strInternalNetworkName);
                adapter.AttachToInternalNetwork();
                break;
            case KNetworkAttachmentType_HostOnly:
                adapter.SetHostInterface(data.m_strHostInterfaceName);
                adapter.AttachToHostOnlyInterface();
                break;
#ifdef VBOX_WITH_VDE
            case KNetworkAttachmentType_VDE:
                adapter.SetVDENetwork(data.m_strVDENetworkName);
                adapter.AttachToVDE();
                break;
#endif /* VBOX_WITH_VDE */
            default:
                break;
        }

        /* Save advanced options: */
        adapter.SetAdapterType(data.m_adapterType);
        adapter.SetPromiscModePolicy(data.m_promiscuousMode);
        adapter.SetMACAddress(data.m_strMACAddress);
        adapter.SetCableConnected(data.m_fCableConnected);

        /* Save redirect options: */
        QVector<QString> oldRedirects = adapter.GetNatDriver().GetRedirects();
        for (int i = 0; i < oldRedirects.size(); ++i)
            adapter.GetNatDriver().RemoveRedirect(oldRedirects[i].section(',', 0, 0));
        UIPortForwardingDataList newRedirects = data.m_redirects;
        for (int i = 0; i < newRedirects.size(); ++i)
        {
            UIPortForwardingData newRedirect = newRedirects[i];
            adapter.GetNatDriver().AddRedirect(newRedirect.name, newRedirect.protocol,
                                               newRedirect.hostIp, newRedirect.hostPort.value(),
                                               newRedirect.guestIp, newRedirect.guestPort.value());
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsNetworkPage::setValidator(QIWidgetValidator *pValidator)
{
    m_pValidator = pValidator;
}

bool UIMachineSettingsNetworkPage::revalidate(QString &strWarning, QString &strTitle)
{
    bool fValid = true;

    for (int i = 0; i < m_pTwAdapters->count(); ++i)
    {
        UIMachineSettingsNetwork *pPage = qobject_cast<UIMachineSettingsNetwork*>(m_pTwAdapters->widget(i));
        Assert(pPage);
        fValid = pPage->revalidate(strWarning, strTitle);
        if (!fValid)
            break;
    }

    return fValid;
}

void UIMachineSettingsNetworkPage::retranslateUi()
{
    for (int i = 0; i < m_pTwAdapters->count(); ++ i)
    {
        UIMachineSettingsNetwork *pPage = qobject_cast<UIMachineSettingsNetwork*>(m_pTwAdapters->widget(i));
        Assert(pPage);
        m_pTwAdapters->setTabText(i, pPage->pageTitle());
    }
}

void UIMachineSettingsNetworkPage::updatePages()
{
    for (int i = 0; i < m_pTwAdapters->count(); ++ i)
    {
        /* Get the iterated page: */
        UIMachineSettingsNetwork *pPage = qobject_cast<UIMachineSettingsNetwork*>(m_pTwAdapters->widget(i));
        Assert(pPage);

        /* Update the page if the attachment type is 'internal network' */
        if (pPage->attachmentType() == KNetworkAttachmentType_Internal)
            QTimer::singleShot(0, pPage, SLOT(sltUpdateAttachmentAlternative()));
    }
}
