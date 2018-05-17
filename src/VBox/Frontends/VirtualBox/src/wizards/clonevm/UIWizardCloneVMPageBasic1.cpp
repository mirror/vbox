/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVMPageBasic1 class implementation.
 */

/*
 * Copyright (C) 2011-2017 Oracle Corporation
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
# include <QVBoxLayout>
# include <QLineEdit>
# include <QCheckBox>

/* GUI includes: */
# include "QIRichTextLabel.h"
# include "UIVMNamePathSelector.h"
# include "UIWizardCloneVM.h"
# include "UIWizardCloneVMPageBasic1.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CVirtualBox.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIWizardCloneVMPage1::UIWizardCloneVMPage1(const QString &strOriginalName, const QString &strDefaultPath, const QString &strGroup)
    : m_strOriginalName(strOriginalName)
    , m_strDefaultPath(strDefaultPath)
    , m_strGroup(strGroup)
{
}

QString UIWizardCloneVMPage1::cloneName() const
{
    return m_pNamePathSelector->name();
}

void UIWizardCloneVMPage1::setCloneName(const QString &strName)
{
    m_pNamePathSelector->setName(strName);
}

QString UIWizardCloneVMPage1::clonePath() const
{
    if (!m_pNamePathSelector)
        return QString();
    return m_pNamePathSelector->path();
}

void UIWizardCloneVMPage1::setClonePath(const QString &strPath)
{
    m_pNamePathSelector->setPath(strPath);
}

QString UIWizardCloneVMPage1::cloneFilePath() const
{
    return m_strCloneFilePath;
}

void UIWizardCloneVMPage1::setCloneFilePath(const QString &path)
{
    if (m_strCloneFilePath == path)
        return;
    m_strCloneFilePath = path;
}


bool UIWizardCloneVMPage1::isReinitMACsChecked() const
{
    return m_pReinitMACsCheckBox->isChecked();
}

void UIWizardCloneVMPage1::composeCloneFilePath()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    setCloneFilePath(vbox.ComposeMachineFilename(m_pNamePathSelector->name(),
                                                 m_strGroup,
                                                 QString::null,
                                                 m_pNamePathSelector->path()));
    const QFileInfo fileInfo(m_strCloneFilePath);
    m_strCloneFolder = fileInfo.absolutePath();
    if (m_pNamePathSelector)
        m_pNamePathSelector->setToolTipText(m_strCloneFolder);
}

UIWizardCloneVMPageBasic1::UIWizardCloneVMPageBasic1(const QString &strOriginalName, const QString &strDefaultPath, const QString &strGroup)
    : UIWizardCloneVMPage1(strOriginalName, strDefaultPath, strGroup)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel = new QIRichTextLabel(this);
        if (m_pLabel)
        {
            pMainLayout->addWidget(m_pLabel);
        }

        m_pNamePathSelector = new UIVMNamePathSelector(this);
        if (m_pNamePathSelector)
        {
            pMainLayout->addWidget(m_pNamePathSelector);
            m_pNamePathSelector->setName(UIWizardCloneVM::tr("%1 Clone").arg(m_strOriginalName));
            m_pNamePathSelector->setPath(m_strDefaultPath);
        }

        m_pReinitMACsCheckBox = new QCheckBox(this);

        if (m_pReinitMACsCheckBox)
        {
            pMainLayout->addWidget(m_pReinitMACsCheckBox);
        }
        pMainLayout->addStretch();
    }

    /* Setup connections: */
    connect(m_pNamePathSelector, &UIVMNamePathSelector::sigNameChanged, this, &UIWizardCloneVMPageBasic1::completeChanged);
    connect(m_pNamePathSelector, &UIVMNamePathSelector::sigPathChanged, this, &UIWizardCloneVMPageBasic1::completeChanged);

    connect(m_pNamePathSelector, &UIVMNamePathSelector::sigNameChanged, this, &UIWizardCloneVMPageBasic1::sltNameChanged);
    connect(m_pNamePathSelector, &UIVMNamePathSelector::sigPathChanged, this, &UIWizardCloneVMPageBasic1::sltPathChanged);

    /* Register fields: */
    registerField("cloneName", this, "cloneName");
    registerField("cloneFilePath", this, "cloneFilePath");
    registerField("reinitMACs", this, "reinitMACs");
    composeCloneFilePath();
}

void UIWizardCloneVMPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardCloneVM::tr("New machine name"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardCloneVM::tr("<p>Please choose a folder and a name for the new virtual machine. "
                                          "The new machine will be a clone of the machine <b>%1</b>.</p>")
                                          .arg(m_strOriginalName));
    m_pReinitMACsCheckBox->setToolTip(UIWizardCloneVM::tr("When checked a new unique MAC address will be assigned to all configured network cards."));
    m_pReinitMACsCheckBox->setText(UIWizardCloneVM::tr("&Reinitialize the MAC address of all network cards"));
}

void UIWizardCloneVMPageBasic1::initializePage()
{
    /* Translate page: */
    retranslateUi();
    if (m_pNamePathSelector)
        m_pNamePathSelector->setFocus();
}

bool UIWizardCloneVMPageBasic1::isComplete() const
{
    if (!m_pNamePathSelector)
        return false;

    QString path = m_pNamePathSelector->path();
    if (path.isEmpty())
        return false;
    /* Make sure VM name feat the rules: */
    QString strName = m_pNamePathSelector->name().trimmed();
    return !strName.isEmpty() && strName != m_strOriginalName;
}

void UIWizardCloneVMPageBasic1::sltNameChanged()
{
    composeCloneFilePath();
}

void UIWizardCloneVMPageBasic1::sltPathChanged()
{
    composeCloneFilePath();
}
