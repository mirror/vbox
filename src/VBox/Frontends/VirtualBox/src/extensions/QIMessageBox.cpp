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
#include <QLabel>
#include <QPushButton>
#include <QStyleOptionFocusRect>
#include <QStylePainter>
#include <QToolButton>
#include <QKeyEvent>
#include <QClipboard>

/* GUI includes: */
#include "VBoxGlobal.h"
#include "QIArrowSplitter.h"
#include "QIMessageBox.h"
#include "QILabel.h"
#include "QIDialogButtonBox.h"
#include "UIIconPool.h"
#ifdef Q_WS_MAC
# include "UIMachineWindowFullscreen.h"
# include "UIMachineWindowSeamless.h"
#endif /* Q_WS_MAC */

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

    /* Set focus to dialog initially: */
    setFocus();

    /* Prepare content: */
    prepareContent();
}

/**
 *  Returns the text of the given message box button.
 *  See QMessageBox::buttonText() for details.
 *
 *  @param aButton Button index (0, 1 or 2).
 */
QString QIMessageBox::buttonText (int aButton) const
{
    switch (aButton)
    {
        case 0: if (m_pButton1) return m_pButton1->text(); break;
        case 1: if (m_pButton2) return m_pButton2->text(); break;
        case 2: if (m_pButton3) return m_pButton3->text(); break;
        default: break;
    }

    return QString::null;
}

/**
 *  Sets the text of the given message box button.
 *  See QMessageBox::setButtonText() for details.
 *
 *  @param aButton  Button index (0, 1 or 2).
 *  @param aText    New button text.
 */
void QIMessageBox::setButtonText (int aButton, const QString &aText)
{
    switch (aButton)
    {
        case 0: if (m_pButton1) m_pButton1->setText (aText); break;
        case 1: if (m_pButton2) m_pButton2->setText (aText); break;
        case 2: if (m_pButton3) m_pButton3->setText (aText); break;
        default: break;
    }
}

/** @fn QIMessageBox::flagText() const
 *
 *  Returns the text of the optional message box flag. If the flag is hidden
 *  (by default) a null string is returned.
 *
 *  @see #setFlagText()
 */

/**
 *  Sets the text for the optional message box flag (check box) that is
 *  displayed under the message text. Passing the null string as the argument
 *  will hide the flag. By default, the flag is hidden.
 */
void QIMessageBox::setFlagText (const QString &aText)
{
    if (aText.isNull())
    {
        m_pFlagCheckBox->hide();
    }
    else
    {
        m_pFlagCheckBox->setText (aText);
        m_pFlagCheckBox->show();
        m_pFlagCheckBox->setFocus();
    }
}

void QIMessageBox::prepareContent()
{
    /* Create main-layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        /* Create top-layout: */
        QHBoxLayout *pTopLayout = new QHBoxLayout;
        {
            /* Create icon-label: */
            m_pIconLabel = new QLabel;
            {
                /* Configure label: */
                m_pIconLabel->setPixmap(standardPixmap(m_iconType));
                m_pIconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
                m_pIconLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
            }
            /* Create top-right layout: */
            QVBoxLayout *pTopRightLayout = new QVBoxLayout;
            {
                /* Create text-label: */
                m_pTextLabel = new QILabel(m_strMessage);
                {
                    /* Configure label: */
                    m_pTextLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
                    m_pTextLabel->setWordWrap(true);
                    QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
                    sizePolicy.setHeightForWidth(true);
                    m_pTextLabel->setSizePolicy(sizePolicy);
                }
                /* Create main check-box: */
                m_pFlagCheckBox_Main = new QCheckBox;
                {
                    /* Configure check-box: */
                    m_pFlagCheckBox_Main->hide();
                    m_pFlagCheckBox_Main->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                }
                /* Configure layout: */
                VBoxGlobal::setLayoutMargin(pTopRightLayout, 0);
                pTopRightLayout->setSpacing(10);
                pTopRightLayout->addWidget(m_pTextLabel);
                pTopRightLayout->addWidget(m_pFlagCheckBox_Main);
            }
            /* Configure layout: */
            VBoxGlobal::setLayoutMargin(pTopLayout, 0);
            pTopLayout->setSpacing(10);
            pTopLayout->addWidget(m_pIconLabel);
            pTopLayout->addLayout(pTopRightLayout);
        }
        /* Create details-widget: */
        m_pDetailsWidget = new QWidget;
        {
            /* Create details-widget layout: */
            QVBoxLayout* pDetailsWidgetLayout = new QVBoxLayout(m_pDetailsWidget);
            {
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
                /* Create details splitter: */
                m_pDetailsSplitter = new QIArrowSplitter(m_pDetailsTextView);
                {
                    /* Configure splitter: */
                    connect(m_pDetailsSplitter, SIGNAL(showBackDetails()), this, SLOT(detailsBack()));
                    connect(m_pDetailsSplitter, SIGNAL(showNextDetails()), this, SLOT(detailsNext()));
                    connect(m_pDetailsSplitter, SIGNAL(sigSizeChanged()), this, SLOT(sltUpdateSize()));
                }
                /* Create details check-box: */
                m_pFlagCheckBox_Details = new QCheckBox;
                {
                    /* Configure check-box: */
                    m_pFlagCheckBox_Details->hide();
                    m_pFlagCheckBox_Details->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                }
                /* Configure layout: */
                VBoxGlobal::setLayoutMargin(pDetailsWidgetLayout, 0);
                pDetailsWidgetLayout->setSpacing(10);
                pDetailsWidgetLayout->addWidget(m_pDetailsSplitter);
                pDetailsWidgetLayout->addWidget(m_pFlagCheckBox_Details);
            }
        }
        /* Create button-box: */
        m_pButtonBox = new QIDialogButtonBox;
        {
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
        /* Configure layout: */
#ifdef Q_WS_MAC
        pMainLayout->setContentsMargins(40, 11, 40, 11);
#else /* !Q_WS_MAC */
        VBoxGlobal::setLayoutMargin(pMainLayout, 11);
#endif /* !Q_WS_MAC */
        pMainLayout->setSpacing(10);
        pMainLayout->addLayout(pTopLayout);
        pMainLayout->addWidget(m_pDetailsWidget);
        pMainLayout->addWidget(m_pButtonBox);
    }

    /* Initialize m_pFlagCheckBox: */
    setDetailsShown(false);
}

QPushButton *QIMessageBox::createButton (int aButton)
{
    if (aButton == 0)
        return 0;

    QString text;
    QDialogButtonBox::ButtonRole role;
    switch (aButton & AlertButtonMask)
    {
        case AlertButton_Ok:     text = tr("OK");     role = QDialogButtonBox::AcceptRole; break;
        case AlertButton_Cancel: text = tr("Cancel"); role = QDialogButtonBox::RejectRole; break;
        case AlertButton_Yes:    text = tr("Yes");    role = QDialogButtonBox::YesRole; break;
        case AlertButton_No:     text = tr("No");     role = QDialogButtonBox::NoRole; break;
        case AlertButton_Ignore: text = tr("Ignore"); role = QDialogButtonBox::AcceptRole; break;
        case AlertButton_Copy:   text = tr("Copy");   role = QDialogButtonBox::ActionRole; break;
        default:
            AssertMsgFailed(("Type %d is not implemented", aButton));
            return NULL;
    }

    QPushButton *b = m_pButtonBox->addButton (text, role);

    if (aButton & AlertButtonOption_Default)
    {
        b->setDefault (true);
        b->setFocus();
    }

    if (aButton & AlertButtonOption_Escape)
        m_iButtonEsc = aButton & AlertButtonMask;

    return b;
}

/** @fn QIMessageBox::detailsText() const
 *
 *  Returns the text of the optional details box. The details box is empty
 *  by default, so QString::null will be returned.
 *
 *  @see #setDetailsText()
 */

/**
 *  Sets the text for the optional details box. Note that the details box
 *  is hidden by default, call #setDetailsShown(true) to make it visible.
 *
 *  @see #detailsText()
 *  @see #setDetailsShown()
 */
void QIMessageBox::setDetailsText (const QString &aText)
{
    AssertMsg (!aText.isEmpty(), ("Details text should NOT be empty."));

    QStringList paragraphs (aText.split ("<!--EOP-->", QString::SkipEmptyParts));
    AssertMsg (paragraphs.size() != 0, ("There should be at least one paragraph."));

    foreach (QString paragraph, paragraphs)
    {
        QStringList parts (paragraph.split ("<!--EOM-->", QString::KeepEmptyParts));
        AssertMsg (parts.size() == 2, ("Each paragraph should consist of 2 parts."));
        m_detailsList << QPair <QString, QString> (parts [0], parts [1]);
    }

    m_pDetailsSplitter->setMultiPaging (m_detailsList.size() > 1);
    m_iDetailsIndex = 0;
    refreshDetails();
}

QPixmap QIMessageBox::standardPixmap(AlertIconType aIcon)
{
    QIcon icon;
    switch (aIcon)
    {
        case AlertIconType_Information:
            icon = UIIconPool::defaultIcon(UIIconPool::MessageBoxInformationIcon, this);
            break;
        case QMessageBox::Warning:
            icon = UIIconPool::defaultIcon(UIIconPool::MessageBoxWarningIcon, this);
            break;
        case AlertIconType_Critical:
            icon = UIIconPool::defaultIcon(UIIconPool::MessageBoxCriticalIcon, this);
            break;
        case AlertIconType_Question:
            icon = UIIconPool::defaultIcon(UIIconPool::MessageBoxQuestionIcon, this);
            break;
        case AlertIconType_GuruMeditation:
            icon = QIcon(":/meditation_32px.png");
            break;
        default:
            break;
    }
    if (!icon.isNull())
    {
        int size = style()->pixelMetric (QStyle::PM_MessageBoxIconSize, 0, this);
        return icon.pixmap (size, size);
    }
    else
        return QPixmap();
}

void QIMessageBox::closeEvent (QCloseEvent *e)
{
    if (m_fDone)
        e->accept();
    else
        e->ignore();
}

void QIMessageBox::polishEvent(QShowEvent *pPolishEvent)
{
    /* Tune our size: */
    resize(minimumSizeHint());
    m_pTextLabel->useSizeHintForWidth(m_pTextLabel->width());
    m_pTextLabel->updateGeometry();

    /* Call to base-class: */
    QIDialog::polishEvent(pPolishEvent);

    /* Make the size fixed: */
    setFixedSize(size());

    /* Toggle details-widget: */
    m_pDetailsSplitter->toggleWidget();
}

void QIMessageBox::refreshDetails()
{
    /* Update message text iteself */
    m_pTextLabel->setText (m_strMessage + m_detailsList [m_iDetailsIndex].first);
    /* Update details table */
    m_pDetailsTextView->setText (m_detailsList [m_iDetailsIndex].second);
    setDetailsShown (!m_pDetailsTextView->toPlainText().isEmpty());

    /* Update multi-paging system */
    if (m_detailsList.size() > 1)
    {
        m_pDetailsSplitter->setButtonEnabled (true, m_iDetailsIndex < m_detailsList.size() - 1);
        m_pDetailsSplitter->setButtonEnabled (false, m_iDetailsIndex > 0);
    }

    /* Update details label */
    m_pDetailsSplitter->setName (m_detailsList.size() == 1 ? tr ("&Details") :
        tr ("&Details (%1 of %2)").arg (m_iDetailsIndex + 1).arg (m_detailsList.size()));
}

/**
 *  Sets the visibility state of the optional details box
 *  to a value of the argument.
 *
 *  @see #isDetailsShown()
 *  @see #setDetailsText()
 */
void QIMessageBox::setDetailsShown (bool aShown)
{
    if (aShown)
    {
        m_pFlagCheckBox_Details->setVisible (m_pFlagCheckBox_Main->isVisible());
        m_pFlagCheckBox_Details->setChecked (m_pFlagCheckBox_Main->isChecked());
        m_pFlagCheckBox_Details->setText (m_pFlagCheckBox_Main->text());
        if (m_pFlagCheckBox_Main->hasFocus())
            m_pFlagCheckBox_Details->setFocus();
        m_pFlagCheckBox_Main->setVisible (false);
        m_pFlagCheckBox = m_pFlagCheckBox_Details;
    }

    m_pDetailsWidget->setVisible (aShown);

    if (!aShown)
    {
        m_pFlagCheckBox_Main->setVisible (m_pFlagCheckBox_Details->isVisible());
        m_pFlagCheckBox_Main->setChecked (m_pFlagCheckBox_Details->isChecked());
        m_pFlagCheckBox_Main->setText (m_pFlagCheckBox_Details->text());
        if (m_pFlagCheckBox_Details->hasFocus())
            m_pFlagCheckBox_Main->setFocus();
        m_pFlagCheckBox = m_pFlagCheckBox_Main;
    }
}

void QIMessageBox::sltUpdateSize()
{
    /* Update/activate all the layouts of the message-box: */
    QList<QLayout*> layouts = findChildren<QLayout*>();
    for (int i = 0; i < layouts.size(); ++i)
    {
        QLayout *pItem = layouts.at(i);
        pItem->update();
        pItem->activate();
    }
    QCoreApplication::sendPostedEvents(0, QEvent::LayoutRequest);

    /* Now resize message-box to the minimum possible size: */
    setFixedSize(minimumSizeHint());
}

void QIMessageBox::detailsBack()
{
    if (m_iDetailsIndex > 0)
    {
        -- m_iDetailsIndex;
        refreshDetails();
    }
}

void QIMessageBox::detailsNext()
{
    if (m_iDetailsIndex < m_detailsList.size() - 1)
    {
        ++ m_iDetailsIndex;
        refreshDetails();
    }
}

void QIMessageBox::reject()
{
    if (m_iButtonEsc)
    {
        QDialog::reject();
        setResult (m_iButtonEsc & AlertButtonMask);
    }
}

void QIMessageBox::copy() const
{
    /* Create the error string with all errors. First the html version. */
    QString strError = "<html><body><p>" + m_strMessage + "</p>";
    for (int i = 0; i < m_detailsList.size(); ++i)
        strError += m_detailsList.at(i).first + m_detailsList.at(i).second + "<br>";
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

