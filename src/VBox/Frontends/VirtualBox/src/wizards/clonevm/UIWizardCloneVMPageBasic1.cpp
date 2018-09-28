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
# include <QCheckBox>
# include <QLabel>
# include <QVBoxLayout>

/* GUI includes: */
# include "QIRichTextLabel.h"
# include "QILineEdit.h"
# include "UIFilePathSelector.h"
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
    , m_pReinitMACsCheckBox(0)
    , m_pNameLineEdit(0)
    , m_pPathSelector(0)
    , m_pNameLabel(0)
    , m_pPathLabel(0)
{
}

QString UIWizardCloneVMPage1::cloneName() const
{
    if (!m_pNameLineEdit)
        return QString();
    return m_pNameLineEdit->text();
}

void UIWizardCloneVMPage1::setCloneName(const QString &strName)
{
    if (!m_pNameLineEdit)
        return;
    m_pNameLineEdit->setText(strName);
}

QString UIWizardCloneVMPage1::clonePath() const
{
    if (!m_pPathSelector)
        return QString();
    return m_pPathSelector->path();
}

void UIWizardCloneVMPage1::setClonePath(const QString &strPath)
{
    if (!m_pPathSelector)
        m_pPathSelector->setPath(strPath);
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
    if (!m_pReinitMACsCheckBox)
        return false;
    return m_pReinitMACsCheckBox->isChecked();
}

void UIWizardCloneVMPage1::composeCloneFilePath()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    setCloneFilePath(vbox.ComposeMachineFilename(m_pNameLineEdit ? m_pNameLineEdit->text() : QString(),
                                                 m_strGroup,
                                                 QString::null,
                                                 m_pPathSelector ? m_pPathSelector->path() : QString()));
    const QFileInfo fileInfo(m_strCloneFilePath);
    m_strCloneFolder = fileInfo.absolutePath();
}

UIWizardCloneVMPageBasic1::UIWizardCloneVMPageBasic1(const QString &strOriginalName, const QString &strDefaultPath, const QString &strGroup)
    : UIWizardCloneVMPage1(strOriginalName, strDefaultPath, strGroup)
    , m_pMainLabel(0)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (!pMainLayout)
        return;

    m_pMainLabel = new QIRichTextLabel(this);
    if (m_pMainLabel)
    {
        pMainLayout->addWidget(m_pMainLabel);
    }

    QWidget *pContainerWidget = new QWidget(this);
    if (pContainerWidget)
    {
        pMainLayout->addWidget(pContainerWidget);
        QGridLayout *pContainerLayout = new QGridLayout(pContainerWidget);
        pContainerLayout->setContentsMargins(0, 0, 0, 0);

        m_pNameLabel = new QLabel;
        if (m_pNameLabel)
        {
            m_pNameLabel->setAlignment(Qt::AlignRight);
            m_pNameLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            pContainerLayout->addWidget(m_pNameLabel, 0, 0, 1, 1);
        }

        m_pNameLineEdit = new QILineEdit();
        if (m_pNameLineEdit)
        {
            pContainerLayout->addWidget(m_pNameLineEdit, 0, 1, 1, 1);
            m_pNameLineEdit->setText(UIWizardCloneVM::tr("%1 Clone").arg(m_strOriginalName));
        }

        m_pPathLabel = new QLabel(this);
        if (m_pPathLabel)
        {
            m_pPathLabel->setAlignment(Qt::AlignRight);
            m_pPathLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            pContainerLayout->addWidget(m_pPathLabel, 1, 0, 1, 1);
        }

        m_pPathSelector = new UIFilePathSelector(this);
        if (m_pPathSelector)
        {
            pContainerLayout->addWidget(m_pPathSelector, 1, 1, 1, 1);
            m_pPathSelector->setPath(m_strDefaultPath);

        }

         m_pReinitMACsCheckBox = new QCheckBox(this);
         if (m_pReinitMACsCheckBox)
         {
             pContainerLayout->addWidget(m_pReinitMACsCheckBox, 2, 0, 1, 2);
             m_pReinitMACsCheckBox->setChecked(true);
         }
    }
    pMainLayout->addStretch();

    /* Setup connections: */
    connect(m_pNameLineEdit, &QILineEdit::textChanged, this, &UIWizardCloneVMPageBasic1::completeChanged);
    connect(m_pPathSelector, &UIFilePathSelector::pathChanged, this, &UIWizardCloneVMPageBasic1::completeChanged);

    connect(m_pNameLineEdit, &QILineEdit::textChanged, this, &UIWizardCloneVMPageBasic1::sltNameChanged);
    connect(m_pPathSelector, &UIFilePathSelector::pathChanged, this, &UIWizardCloneVMPageBasic1::sltPathChanged);

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
    if (m_pMainLabel)
        m_pMainLabel->setText(UIWizardCloneVM::tr("<p>Please choose a name and optionally a folder for the new virtual machine. "
                                                  "The new machine will be a clone of the machine <b>%1</b>.</p>")
                              .arg(m_strOriginalName));

    if (m_pNameLabel)
        m_pNameLabel->setText(UIWizardCloneVM::tr("Name:"));

    if (m_pPathLabel)
        m_pPathLabel->setText(UIWizardCloneVM::tr("Path:"));
    if (m_pReinitMACsCheckBox)
    {
        m_pReinitMACsCheckBox->setToolTip(UIWizardCloneVM::tr("When checked a new unique MAC address will be assigned to all configured network cards."));
        m_pReinitMACsCheckBox->setText(UIWizardCloneVM::tr("&Reinitialize the MAC address of all network cards"));
    }
}

void UIWizardCloneVMPageBasic1::initializePage()
{
    /* Translate page: */
    retranslateUi();
    if (m_pNameLineEdit)
        m_pNameLineEdit->setFocus();
}

bool UIWizardCloneVMPageBasic1::isComplete() const
{
    if (!m_pPathSelector)
        return false;

    QString path = m_pPathSelector->path();
    if (path.isEmpty())
        return false;
    /* Make sure VM name feat the rules: */
    QString strName = m_pNameLineEdit->text().trimmed();
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
