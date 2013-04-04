/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIMessageBox class implementation
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QClipboard>
#include <QLabel>
#include <QTextEdit>
#include <QCheckBox>
#include <QPushButton>

/* GUI includes: */
#include "QIMessageBox.h"
#include "UIIconPool.h"
#include "QILabel.h"
#include "QIArrowSplitter.h"
#include "QIDialogButtonBox.h"
#include "VBoxGlobal.h"

QIMessageBox::QIMessageBox(const QString &strCaption, const QString &strMessage, AlertIconType iconType,
                           int iButton1 /*= 0*/, int iButton2 /*= 0*/, int iButton3 /*= 0*/, QWidget *pParent /*= 0*/)
    : QIDialog(pParent)
    , m_iButton1(iButton1)
    , m_iButton2(iButton2)
    , m_iButton3(iButton3)
    , m_iButtonEsc(0)
    , m_iconType(iconType)
    , m_strMessage(strMessage)
    , m_iDetailsIndex(-1)
    , m_fDone(false)
{
    /* Set caption: */
    setWindowTitle(strCaption);

    /* Prepare content: */
    prepareContent();
}

QString QIMessageBox::detailsText() const
{
    /* Return in html format: */
    return m_pDetailsTextView->toHtml();
}

void QIMessageBox::setDetailsText(const QString &strText)
{
    /* Split details into paragraphs: */
    AssertMsg(!strText.isEmpty(), ("Details text should NOT be empty!"));
    QStringList paragraphs(strText.split("<!--EOP-->", QString::SkipEmptyParts));
    AssertMsg(paragraphs.size() != 0, ("There should be at least one paragraph."));
    /* Populate details list: */
    foreach (const QString &strParagraph, paragraphs)
    {
        QStringList parts(strParagraph.split("<!--EOM-->", QString::KeepEmptyParts));
        AssertMsg(parts.size() == 2, ("Each paragraph should consist of 2 parts."));
        m_details << QPair<QString, QString>(parts[0], parts[1]);
    }
    /* Update details container: */
    updateDetailsContainer();
}

bool QIMessageBox::flagChecked() const
{
    return m_pFlagCheckBox->isChecked();
}

void QIMessageBox::setFlagChecked(bool fChecked)
{
    m_pFlagCheckBox->setChecked(fChecked);
}

QString QIMessageBox::flagText() const
{
    return m_pFlagCheckBox->text();
}

void QIMessageBox::setFlagText(const QString &strText)
{
    /* Set check-box text: */
    m_pFlagCheckBox->setText(strText);
    /* And update check-box finally: */
    updateCheckBox();
}

QString QIMessageBox::buttonText(int iButton) const
{
    switch (iButton)
    {
        case 0: if (m_pButton1) return m_pButton1->text(); break;
        case 1: if (m_pButton2) return m_pButton2->text(); break;
        case 2: if (m_pButton3) return m_pButton3->text(); break;
        default: break;
    }
    return QString();
}

void QIMessageBox::setButtonText(int iButton, const QString &strText)
{
    switch (iButton)
    {
        case 0: if (m_pButton1) m_pButton1->setText(strText); break;
        case 1: if (m_pButton2) m_pButton2->setText(strText); break;
        case 2: if (m_pButton3) m_pButton3->setText(strText); break;
        default: break;
    }
}

void QIMessageBox::reject()
{
    if (m_iButtonEsc)
    {
        QDialog::reject();
        setResult(m_iButtonEsc & AlertButtonMask);
    }
}

void QIMessageBox::copy() const
{
    /* Create the error string with all errors. First the html version. */
    QString strError = "<html><body><p>" + m_strMessage + "</p>";
    for (int i = 0; i < m_details.size(); ++i)
        strError += m_details.at(i).first + m_details.at(i).second + "<br>";
    strError += "</body></html>";
    strError.remove(QRegExp("</+qt>"));
    strError = strError.replace(QRegExp("&nbsp;"), " ");
    /* Create a new mime data object holding both the html and the plain text version. */
    QMimeData *pMd = new QMimeData();
    pMd->setHtml(strError);
    /* Replace all the html entities. */
    strError = strError.replace(QRegExp("<br>|</tr>"), "\n");
    strError = strError.replace(QRegExp("</p>"), "\n\n");
    strError = strError.remove(QRegExp("<[^>]*>"));
    pMd->setText(strError);
    /* Add the mime data to the global clipboard. */
    QClipboard *pClipboard = QApplication::clipboard();
    pClipboard->setMimeData(pMd);
}

void QIMessageBox::detailsBack()
{
    /* Make sure details-page index feats the bounds: */
    if (m_iDetailsIndex <= 0)
        return;

    /* Advance the page index: */
    --m_iDetailsIndex;
    /* Update details-page: */
    updateDetailsPage();
}

void QIMessageBox::detailsNext()
{
    /* Make sure details-page index feats the bounds: */
    if (m_iDetailsIndex >= m_details.size() - 1)
        return;

    /* Advance the page index: */
    ++m_iDetailsIndex;
    /* Update details-page: */
    updateDetailsPage();
}

void QIMessageBox::sltUpdateSize()
{
    /* Reactivate all the layouts: */
    QList<QLayout*> layouts = findChildren<QLayout*>();
    foreach (QLayout *pLayout, layouts)
    {
        pLayout->update();
        pLayout->activate();
    }
    QCoreApplication::sendPostedEvents(0, QEvent::LayoutRequest);
    /* And fix the size to the minimum possible: */
    setFixedSize(minimumSizeHint());
}

void QIMessageBox::prepareContent()
{
    /* Create main-layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        /* Configure layout: */
#ifdef Q_WS_MAC
        pMainLayout->setContentsMargins(40, 11, 40, 11);
        pMainLayout->setSpacing(15);
#else /* !Q_WS_MAC */
        VBoxGlobal::setLayoutMargin(pMainLayout, 11);
        pMainLayout->setSpacing(10);
#endif /* !Q_WS_MAC */
        /* Create top-layout: */
        QHBoxLayout *pTopLayout = new QHBoxLayout;
        {
            /* Insert into parent layout: */
            pMainLayout->addLayout(pTopLayout);
            /* Configure layout: */
            VBoxGlobal::setLayoutMargin(pTopLayout, 0);
            pTopLayout->setSpacing(10);
            /* Create icon-label: */
            m_pIconLabel = new QLabel;
            {
                /* Insert into parent layout: */
                pTopLayout->addWidget(m_pIconLabel);
                /* Configure label: */
                m_pIconLabel->setPixmap(standardPixmap(m_iconType, this));
                m_pIconLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
                m_pIconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
            }
            /* Create text-label: */
            m_pTextLabel = new QILabel(m_strMessage);
            {
                /* Insert into parent layout: */
                pTopLayout->addWidget(m_pTextLabel);
                /* Configure label: */
                m_pTextLabel->setWordWrap(true);
                m_pTextLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
                QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
                sizePolicy.setHeightForWidth(true);
                m_pTextLabel->setSizePolicy(sizePolicy);
            }
        }
        /* Create details text-view: */
        m_pDetailsTextView = new QTextEdit;
        {
            /* Configure text-view: */
            m_pDetailsTextView->setReadOnly(true);
            m_pDetailsTextView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
            /* Calculate the minimum size dynamically, approx. for 40 chars, 4 lines & 2 <table> margins: */
            QFontMetrics fm = m_pDetailsTextView->fontMetrics();
            m_pDetailsTextView->setMinimumSize(fm.width ('m') * 40, fm.lineSpacing() * 4 + 4 * 2);
        }
        /* Create details-container: */
        m_pDetailsContainer = new QIArrowSplitter(m_pDetailsTextView);
        {
            /* Insert into parent layout: */
            pMainLayout->addWidget(m_pDetailsContainer);
            /* Configure container: */
            connect(m_pDetailsContainer, SIGNAL(showBackDetails()), this, SLOT(detailsBack()));
            connect(m_pDetailsContainer, SIGNAL(showNextDetails()), this, SLOT(detailsNext()));
            connect(m_pDetailsContainer, SIGNAL(sigSizeChanged()), this, SLOT(sltUpdateSize()));
            /* And update container finally: */
            updateDetailsContainer();
        }
        /* Create details check-box: */
        m_pFlagCheckBox = new QCheckBox;
        {
            /* Insert into parent layout: */
            pMainLayout->addWidget(m_pFlagCheckBox, 0, Qt::AlignHCenter | Qt::AlignVCenter);
            /* Configure check-box: */
            m_pFlagCheckBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            /* And update check-box finally: */
            updateCheckBox();
        }
        /* Create button-box: */
        m_pButtonBox = new QIDialogButtonBox;
        {
            /* Insert into parent layout: */
            pMainLayout->addWidget(m_pButtonBox);
            /* Configure button-box: */
            m_pButtonBox->setCenterButtons(true);
            m_pButton1 = createButton(m_iButton1);
            if (m_pButton1)
                connect(m_pButton1, SIGNAL(clicked()), SLOT(done1()));
            m_pButton2 = createButton(m_iButton2);
            if (m_pButton2)
                connect(m_pButton2, SIGNAL(clicked()), SLOT(done2()));
            m_pButton3 = createButton(m_iButton3);
            if (m_pButton3)
                connect(m_pButton3, SIGNAL(clicked()), SLOT(done3()));
            /* If this is a critical message add a "Copy to clipboard" button: */
            if (m_iconType == AlertIconType_Critical)
            {
                QPushButton *pCopyButton = createButton(AlertButton_Copy);
                pCopyButton->setToolTip(tr("Copy all errors to the clipboard"));
                connect(pCopyButton, SIGNAL(clicked()), SLOT(copy()));
            }
        }
    }
}

QPushButton* QIMessageBox::createButton(int iButton)
{
    /* Not for AlertButton_NoButton: */
    if (iButton == 0)
        return 0;

    /* Prepare button text & role: */
    QString strText;
    QDialogButtonBox::ButtonRole role;
    switch (iButton & AlertButtonMask)
    {
        case AlertButton_Ok:     strText = tr("OK");     role = QDialogButtonBox::AcceptRole; break;
        case AlertButton_Cancel: strText = tr("Cancel"); role = QDialogButtonBox::RejectRole; break;
        case AlertButton_Yes:    strText = tr("Yes");    role = QDialogButtonBox::YesRole; break;
        case AlertButton_No:     strText = tr("No");     role = QDialogButtonBox::NoRole; break;
        case AlertButton_Ignore: strText = tr("Ignore"); role = QDialogButtonBox::AcceptRole; break;
        case AlertButton_Copy:   strText = tr("Copy");   role = QDialogButtonBox::ActionRole; break;
        default:
            AssertMsgFailed(("Type %d is not supported!", iButton));
            return 0;
    }

    /* Create push-button: */
    QPushButton *pButton = m_pButtonBox->addButton(strText, role);

    /* Configure <default> button: */
    if (iButton & AlertButtonOption_Default)
    {
        pButton->setDefault(true);
        pButton->setFocus();
    }
    /* Configure <escape> button: */
    if (iButton & AlertButtonOption_Escape)
        m_iButtonEsc = iButton & AlertButtonMask;

    /* Return button: */
    return pButton;
}

void QIMessageBox::polishEvent(QShowEvent *pPolishEvent)
{
    /* Tune our size: */
    m_pTextLabel->useSizeHintForWidth(m_pTextLabel->width());
    m_pTextLabel->updateGeometry();

    /* Call to base-class: */
    QIDialog::polishEvent(pPolishEvent);

    /* Make the size fixed: */
    setFixedSize(size());
}

void QIMessageBox::closeEvent(QCloseEvent *pCloseEvent)
{
    if (m_fDone)
        pCloseEvent->accept();
    else
        pCloseEvent->ignore();
}

void QIMessageBox::updateDetailsContainer()
{
    /* Do we have details to show? */
    m_pDetailsContainer->setVisible(!m_details.isEmpty());

    /* Reset the details page index: */
    m_iDetailsIndex = m_details.isEmpty() ? -1 : 0;

    /* Do we have any details? */
    if (m_details.isEmpty())
        m_pDetailsContainer->setName(QString());
    else if (m_details.size() == 1)
        m_pDetailsContainer->setName(tr("&Details"));
    else
        m_pDetailsContainer->setMultiPaging(true);

    /* Do we have any details? */
    if (!m_details.isEmpty())
        updateDetailsPage();
}

void QIMessageBox::updateDetailsPage()
{
    /* Make sure details-page index feats the bounds: */
    if (m_iDetailsIndex < 0 || m_iDetailsIndex >= m_details.size())
        return;

    /* Update message text-label: */
    m_pTextLabel->setText(m_strMessage + m_details[m_iDetailsIndex].first);

    /* Update details text-view: */
    m_pDetailsTextView->setText(m_details[m_iDetailsIndex].second);

    /* Update details-container: */
    if (m_details.size() > 1)
    {
        m_pDetailsContainer->setName(tr("&Details (%1 of %2)").arg(m_iDetailsIndex + 1).arg(m_details.size()));
        m_pDetailsContainer->setButtonEnabled(true, m_iDetailsIndex < m_details.size() - 1);
        m_pDetailsContainer->setButtonEnabled(false, m_iDetailsIndex > 0);
    }
}

void QIMessageBox::updateCheckBox()
{
    /* Flag check-box with text is always visible: */
    m_pFlagCheckBox->setVisible(!m_pFlagCheckBox->text().isEmpty());
}

/* static */
QPixmap QIMessageBox::standardPixmap(AlertIconType iconType, QWidget *pWidget /*= 0*/)
{
    /* Prepare standard icon: */
    QIcon icon;
    switch (iconType)
    {
        case AlertIconType_Information:    icon = UIIconPool::defaultIcon(UIIconPool::MessageBoxInformationIcon, pWidget); break;
        case AlertIconType_Warning:        icon = UIIconPool::defaultIcon(UIIconPool::MessageBoxWarningIcon, pWidget); break;
        case AlertIconType_Critical:       icon = UIIconPool::defaultIcon(UIIconPool::MessageBoxCriticalIcon, pWidget); break;
        case AlertIconType_Question:       icon = UIIconPool::defaultIcon(UIIconPool::MessageBoxQuestionIcon, pWidget); break;
        case AlertIconType_GuruMeditation: icon = UIIconPool::iconSet(":/meditation_32px.png"); break;
        default: break;
    }
    /* Return empty pixmap if nothing found: */
    if (icon.isNull())
        return QPixmap();
    /* Return pixmap of standard size if possible: */
    QStyle *pStyle = pWidget ? pWidget->style() : QApplication::style();
    int iSize = pStyle->pixelMetric(QStyle::PM_MessageBoxIconSize, 0, pWidget);
    return icon.pixmap(iSize, iSize);
}

