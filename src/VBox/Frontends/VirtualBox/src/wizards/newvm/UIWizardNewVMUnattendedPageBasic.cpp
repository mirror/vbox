/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMUnattendedPageBasic class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <QFileInfo>
#include <QGridLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UIWizardNewVMEditors.h"
#include "UIWizardNewVMUnattendedPageBasic.h"
#include "UIWizardNewVM.h"

bool UIWizardNewVMUnattendedPage::checkGAISOFile(const QString &strPath)
{
    if (strPath.isNull() || strPath.isEmpty())
        return false;
    QFileInfo fileInfo(strPath);
    if (!fileInfo.exists() || !fileInfo.isReadable())
        return false;
    return true;
}

UIWizardNewVMUnattendedPageBasic::UIWizardNewVMUnattendedPageBasic()
    : m_pLabel(0)
    , m_pAdditionalOptionsContainer(0)
    , m_pGAInstallationISOContainer(0)
    , m_pUserNamePasswordGroupBox(0)
{
    prepare();
}

void UIWizardNewVMUnattendedPageBasic::prepare()
{
    QGridLayout *pMainLayout = new QGridLayout(this);

    m_pLabel = new QIRichTextLabel(this);
    if (m_pLabel)
        pMainLayout->addWidget(m_pLabel, 0, 0, 1, 2);

    m_pUserNamePasswordGroupBox = new UIUserNamePasswordGroupBox;
    AssertReturnVoid(m_pUserNamePasswordGroupBox);
    pMainLayout->addWidget(m_pUserNamePasswordGroupBox, 1, 0, 1, 1);

    m_pAdditionalOptionsContainer = new UIAdditionalUnattendedOptions;
    AssertReturnVoid(m_pAdditionalOptionsContainer);
    pMainLayout->addWidget(m_pAdditionalOptionsContainer, 1, 1, 1, 1);

    m_pGAInstallationISOContainer = new UIGAInstallationGroupBox;
    AssertReturnVoid(m_pGAInstallationISOContainer);
    pMainLayout->addWidget(m_pGAInstallationISOContainer, 2, 0, 1, 2);

    pMainLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding), 3, 0, 1, 2);

    createConnections();
}

void UIWizardNewVMUnattendedPageBasic::createConnections()
{
    if (m_pUserNamePasswordGroupBox)
    {
        connect(m_pUserNamePasswordGroupBox, &UIUserNamePasswordGroupBox::sigPasswordChanged,
                this, &UIWizardNewVMUnattendedPageBasic::sltPasswordChanged);
        connect(m_pUserNamePasswordGroupBox, &UIUserNamePasswordGroupBox::sigUserNameChanged,
                this, &UIWizardNewVMUnattendedPageBasic::sltUserNameChanged);
    }
    if (m_pGAInstallationISOContainer)
    {
        connect(m_pGAInstallationISOContainer, &UIGAInstallationGroupBox::toggled,
                this, &UIWizardNewVMUnattendedPageBasic::sltInstallGACheckBoxToggle);
        connect(m_pGAInstallationISOContainer, &UIGAInstallationGroupBox::sigPathChanged,
                this, &UIWizardNewVMUnattendedPageBasic::sltGAISOPathChanged);
    }

    if (m_pAdditionalOptionsContainer)
    {
        connect(m_pAdditionalOptionsContainer, &UIAdditionalUnattendedOptions::sigHostnameDomainNameChanged,
                this, &UIWizardNewVMUnattendedPageBasic::sltHostnameDomainNameChanged);
        connect(m_pAdditionalOptionsContainer, &UIAdditionalUnattendedOptions::sigProductKeyChanged,
                this, &UIWizardNewVMUnattendedPageBasic::sltProductKeyChanged);
        connect(m_pAdditionalOptionsContainer, &UIAdditionalUnattendedOptions::sigStartHeadlessChanged,
                this, &UIWizardNewVMUnattendedPageBasic::sltStartHeadlessChanged);
    }
}


void UIWizardNewVMUnattendedPageBasic::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Unattended Guest OS Install Setup"));
    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("<p>You can configure the unattended guest OS install by modifying username, password, and "
                                            "hostname. Additionally you can enable guest additions install. "
                                            "For Microsoft Windows guests it is possible to provide a product key.</p>"));
    if (m_pUserNamePasswordGroupBox)
        m_pUserNamePasswordGroupBox->setTitle(UIWizardNewVM::tr("Username and Password"));
}


void UIWizardNewVMUnattendedPageBasic::initializePage()
{
    if (m_pAdditionalOptionsContainer)
        m_pAdditionalOptionsContainer->disableEnableProductKeyWidgets(isProductKeyWidgetEnabled());
    retranslateUi();

    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    AssertReturnVoid(pWizard);
    /* Initialize user/password if they are not modified by the user: */
    if (m_pUserNamePasswordGroupBox)
    {
        m_pUserNamePasswordGroupBox->blockSignals(true);
        if (!m_userModifiedParameters.contains("UserName"))
            m_pUserNamePasswordGroupBox->setUserName(pWizard->userName());
        if (!m_userModifiedParameters.contains("Password"))
            m_pUserNamePasswordGroupBox->setPassword(pWizard->password());
        m_pUserNamePasswordGroupBox->blockSignals(false);
    }
    if (m_pAdditionalOptionsContainer)
    {
        m_pAdditionalOptionsContainer->blockSignals(true);

        if (!m_userModifiedParameters.contains("HostnameDomainName"))
        {
            m_pAdditionalOptionsContainer->setHostname(pWizard->machineBaseName());
            m_pAdditionalOptionsContainer->setDomainName("myguest.virtualbox.org");
            /* Initialize unattended hostname here since we cannot get the efault value from CUnattended this early (unlike username etc): */
            newVMWizardPropertySet(HostnameDomainName, m_pAdditionalOptionsContainer->hostnameDomainName());
        }
        m_pAdditionalOptionsContainer->blockSignals(false);
    }
    if (m_pGAInstallationISOContainer && !m_userModifiedParameters.contains("InstallGuestAdditions"))
    {
        m_pGAInstallationISOContainer->blockSignals(true);
        m_pGAInstallationISOContainer->setChecked(pWizard->installGuestAdditions());
        m_pGAInstallationISOContainer->blockSignals(false);
    }

    if (m_pGAInstallationISOContainer && !m_userModifiedParameters.contains("GuestAdditionsISOPath"))
    {
        m_pGAInstallationISOContainer->blockSignals(true);
        m_pGAInstallationISOContainer->setPath(pWizard->guestAdditionsISOPath());
        m_pGAInstallationISOContainer->blockSignals(false);
    }
}

bool UIWizardNewVMUnattendedPageBasic::isComplete() const
{
    markWidgets();
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    if (pWizard && pWizard->installGuestAdditions() &&
        m_pGAInstallationISOContainer &&
        !UIWizardNewVMUnattendedPage::checkGAISOFile(m_pGAInstallationISOContainer->path()))
        return false;
    if (m_pUserNamePasswordGroupBox && !m_pUserNamePasswordGroupBox->isComplete())
        return false;
    if (m_pAdditionalOptionsContainer && !m_pAdditionalOptionsContainer->isComplete())
        return false;
    return true;
}

void UIWizardNewVMUnattendedPageBasic::cleanupPage()
{
}

void UIWizardNewVMUnattendedPageBasic::showEvent(QShowEvent *pEvent)
{
    UINativeWizardPage::showEvent(pEvent);
}

void UIWizardNewVMUnattendedPageBasic::sltInstallGACheckBoxToggle(bool fEnabled)
{
    newVMWizardPropertySet(InstallGuestAdditions, fEnabled);
    m_userModifiedParameters << "InstallGuestAdditions";
    emit completeChanged();
}

void UIWizardNewVMUnattendedPageBasic::sltGAISOPathChanged(const QString &strPath)
{
    newVMWizardPropertySet(GuestAdditionsISOPath, strPath);
    m_userModifiedParameters << "GuestAdditionsISOPath";
    emit completeChanged();
}

void UIWizardNewVMUnattendedPageBasic::sltPasswordChanged(const QString &strPassword)
{
    newVMWizardPropertySet(Password, strPassword);
    m_userModifiedParameters << "Password";
    emit completeChanged();
}

void UIWizardNewVMUnattendedPageBasic::sltUserNameChanged(const QString &strUserName)
{
    newVMWizardPropertySet(UserName, strUserName);
    m_userModifiedParameters << "UserName";
    emit completeChanged();
}

bool UIWizardNewVMUnattendedPageBasic::isProductKeyWidgetEnabled() const
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    if (!pWizard || !pWizard->isUnattendedEnabled() || !pWizard->isGuestOSTypeWindows())
        return false;
    return true;
}

void UIWizardNewVMUnattendedPageBasic::sltHostnameDomainNameChanged(const QString &strHostnameDomainName)
{
    newVMWizardPropertySet(HostnameDomainName, strHostnameDomainName);
    m_userModifiedParameters << "HostnameDomainName";
    emit completeChanged();
}

void UIWizardNewVMUnattendedPageBasic::sltProductKeyChanged(const QString &strProductKey)
{
    m_userModifiedParameters << "ProductKey";
    newVMWizardPropertySet(ProductKey, strProductKey);
}

void UIWizardNewVMUnattendedPageBasic::sltStartHeadlessChanged(bool fStartHeadless)
{
    m_userModifiedParameters << "StartHeadless";
    newVMWizardPropertySet(StartHeadless, fStartHeadless);
}

void UIWizardNewVMUnattendedPageBasic::markWidgets() const
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    if (pWizard && pWizard->installGuestAdditions() && m_pGAInstallationISOContainer)
        m_pGAInstallationISOContainer->mark();
}
