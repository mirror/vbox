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
# include <QFontMetrics>
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

const float UIVMNamePathSelector::s_fPathLabelWidthWeight = 0.65f;

class UIPathLabel : public QLabel
{

    Q_OBJECT;

public:

    UIPathLabel(QWidget *parent = 0);

    int maxWidth() const;
    void setMaxWidth(int width);
    void setText(const QString &text);

private:

    /** Compact the text this is not wider than maxWidth */
    void compactText();

    int m_maxWidth;
    QString m_strOriginalText;
};

UIPathLabel::UIPathLabel(QWidget *parent /* = 0*/)
    :QLabel(parent)
    , m_maxWidth(0)
{
}

void UIPathLabel::compactText()
{
    QFontMetrics fontMetrics = this->fontMetrics();
    QString strCompactedText = fontMetrics.elidedText(m_strOriginalText, Qt::ElideMiddle, m_maxWidth);
    QLabel::setText(strCompactedText);
}

int UIPathLabel::maxWidth() const
{
    return m_maxWidth;
}

void UIPathLabel::setMaxWidth(int width)
{
    if (m_maxWidth == width)
        return;
    m_maxWidth = width;
    compactText();
}

void UIPathLabel::setText(const QString &text)
{
    if (m_strOriginalText == text)
        return;

    m_strOriginalText = text;
    compactText();
}


UIVMNamePathSelector::UIVMNamePathSelector(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pMainLayout(0)
    , m_pPath(0)
    , m_pName(0)
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

    m_pPath = new UIPathLabel;
    if (m_pPath)
    {
        m_pMainLayout->addWidget(m_pPath);
        setPath(defaultMachineFolder());
        m_pPath->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
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
        /** make sure we have a trailing seperator */
        if (!nativePath.isEmpty() && !nativePath.endsWith(QDir::toNativeSeparators("/")))
            nativePath += QDir::toNativeSeparators("/");
        m_pPath->setText(nativePath);
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

void UIVMNamePathSelector::resizeEvent(QResizeEvent *pEvent)
{
    QWidget::resizeEvent(pEvent);

    if (m_pPath)
        m_pPath->setMaxWidth(s_fPathLabelWidthWeight * width());
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

#include "UIVMNamePathSelector.moc"
