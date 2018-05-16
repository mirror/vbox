/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMNamePathSelector class implementation.
 */

/*
 * Copyright (C) 2008-2018 Oracle Corporation
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
# include <QDir>
# include <QHBoxLayout>
# include <QRegExpValidator>
# include <QStyle>

/* GUI includes: */
# include "QIFileDialog.h"
# include "QILineEdit.h"
# include "QILabel.h"
# include "QIToolButton.h"
# include "UIIconPool.h"
# include "UIVMNamePathSelector.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CSystemProperties.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIVMNamePathSelector::UIVMNamePathSelector(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pMainLayout(0)
    , m_pPath(0)
    , m_pName(0)
    , m_pSeparator(0)
    ,m_pFileDialogButton(0)
{
    prepareWidgets();
    retranslateUi();
}

QString UIVMNamePathSelector::defaultMachineFolder() const
{
    /* Get VBox: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    /* Get default machine folder: */
    return vbox.GetSystemProperties().GetDefaultMachineFolder();

}

void UIVMNamePathSelector::prepareWidgets()
{
    m_pMainLayout = new QHBoxLayout;
    if (!m_pMainLayout)
        return;
            /* Configure layout: */
#ifdef VBOX_WS_MAC
            m_pMainLayout->setContentsMargins(0, 0, 0, 0);
            m_pMainLayout->setSpacing(0);
#else
            m_pMainLayout->setContentsMargins(qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 2, 0,
                                                 qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin) / 2, 0);
            m_pMainLayout->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) / 2);
#endif
    setLayout(m_pMainLayout);

    m_pFileDialogButton = new QIToolButton(this);
    if (m_pFileDialogButton)
    {
        m_pMainLayout->addWidget(m_pFileDialogButton);
        m_pFileDialogButton->setIcon(UIIconPool::iconSet(QString(":/select_file_16px.png")));
        connect(m_pFileDialogButton, &QIToolButton::clicked, this, &UIVMNamePathSelector::sltOpenPathSelector);
    }

    m_pPath = new QILineEdit;
    if (m_pPath)
    {
        m_pMainLayout->addWidget(m_pPath);
        m_pPath->setReadOnly(true);
        setPath(defaultMachineFolder());
    }
    m_pSeparator = new QILabel;
    if (m_pSeparator)
    {
        m_pSeparator->setText(QDir::separator());
        m_pMainLayout->addWidget(m_pSeparator);
    }

    m_pName = new QILineEdit;
    if (m_pName)
    {
        m_pMainLayout->addWidget(m_pName);
        connect(m_pName, &QILineEdit::textChanged,
                this, &UIVMNamePathSelector::sigNameChanged);
        setFocusProxy(m_pName);
        if (m_pFileDialogButton)
            m_pFileDialogButton->setFocusProxy(m_pName);
    }
}

QString UIVMNamePathSelector::path() const
{
    return m_strNonNativePath;
}

void UIVMNamePathSelector::setPath(const QString &path)
{
    if (m_strNonNativePath == path)
        return;
    m_strNonNativePath = path;
    if (m_pPath)
    {
        QString nativePath(QDir::toNativeSeparators(path));
        m_pPath->setText(nativePath);
        m_pPath->setFixedWidthByText(nativePath);
    }
    emit sigPathChanged(m_strNonNativePath);
}

QString UIVMNamePathSelector::name() const
{
    if (!m_pName)
        return QString();
    return m_pName->text();
}

void UIVMNamePathSelector::setName(const QString &name)
{
    if (!m_pName && m_pName->text() == name)
        return;
    m_pName->setText(name);
}

void UIVMNamePathSelector::retranslateUi()
{
    if (m_strToolTipText.isEmpty())
    {
        setToolTip(tr("You have to enter a name for the virtual machine"));
        return;
    }
    QString strToolTip = "The Virtual Machine files will be saved under " + m_strToolTipText;
    setToolTip(tr(qPrintable(strToolTip)));
}

void UIVMNamePathSelector::sltOpenPathSelector()
{
    QString strSelectedPath = QIFileDialog::getExistingDirectory(m_strNonNativePath, this,
                                                                 QString("Select a parent folder for new Virtual Machine"));
    if (!strSelectedPath.isEmpty())
    {
        setPath(strSelectedPath);
    }
}

void UIVMNamePathSelector::setNameFieldValidator(const QString &strValidatorString)
{
    if (!m_pName)
        return;
    m_pName->setValidator(new QRegExpValidator(QRegExp(strValidatorString), this));
}

void UIVMNamePathSelector::setToolTipText(const QString &strToolTipText)
{
    if (m_strToolTipText == strToolTipText)
        return;
    m_strToolTipText = strToolTipText;
    retranslateUi();
}

const QString& UIVMNamePathSelector::toolTipText() const
{
    return m_strToolTipText;
}
