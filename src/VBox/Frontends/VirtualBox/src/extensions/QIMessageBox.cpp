/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIMessageBox class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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

QIMessageBox::QIMessageBox(const QString &strCaption, const QString &strMessage, AlertIconType icon,
                           int iButton1 /*= 0*/, int iButton2 /*= 0*/, int iButton3 /*= 0*/, QWidget *pParent /*= 0*/)
    : QIDialog(pParent)
    , mButton1(iButton1)
    , mButton2(iButton2)
    , mButton3(iButton3)
    , m_strMessage(strMessage)
    , m_iDetailsIndex(-1)
    , m_fDone(false)
{
    /* Set alert caption: */
    setWindowTitle(strCaption);

    QVBoxLayout *layout = new QVBoxLayout (this);
#ifdef Q_WS_MAC
    layout->setContentsMargins (40, 11, 40, 11);
#else /* !Q_WS_MAC */
    VBoxGlobal::setLayoutMargin (layout, 11);
#endif /* !Q_WS_MAC */
    layout->setSpacing (10);
    layout->setSizeConstraint (QLayout::SetMinimumSize);

    QWidget *main = new QWidget();

    QHBoxLayout *hLayout = new QHBoxLayout (main);
    VBoxGlobal::setLayoutMargin (hLayout, 0);
    hLayout->setSpacing (10);
    layout->addWidget (main);

    mIconLabel = new QLabel();
    mIconLabel->setPixmap (standardPixmap (icon));
    mIconLabel->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Minimum);
    mIconLabel->setAlignment (Qt::AlignHCenter | Qt::AlignTop);
    hLayout->addWidget (mIconLabel);

    QVBoxLayout* messageVBoxLayout = new QVBoxLayout();
    VBoxGlobal::setLayoutMargin (messageVBoxLayout, 0);
    messageVBoxLayout->setSpacing (10);
    hLayout->addLayout (messageVBoxLayout);

    mTextLabel = new QILabel (m_strMessage);
    mTextLabel->setAlignment (Qt::AlignLeft | Qt::AlignTop);
    mTextLabel->setWordWrap (true);
    QSizePolicy sp (QSizePolicy::Minimum, QSizePolicy::Minimum);
    sp.setHeightForWidth (true);
    mTextLabel->setSizePolicy (sp);
    messageVBoxLayout->addWidget (mTextLabel);

    mFlagCB_Main = new QCheckBox();
    mFlagCB_Main->hide();
    messageVBoxLayout->addWidget (mFlagCB_Main);

    mDetailsVBox = new QWidget();
    layout->addWidget (mDetailsVBox);

    QVBoxLayout* detailsVBoxLayout = new QVBoxLayout (mDetailsVBox);
    VBoxGlobal::setLayoutMargin (detailsVBoxLayout, 0);
    detailsVBoxLayout->setSpacing (10);

    mDetailsText = new QTextEdit();
    {
        /* Calculate the minimum size dynamically, approx.
         * for 40 chars, 4 lines & 2 <table> margins */
        QFontMetrics fm = mDetailsText->fontMetrics();
        mDetailsText->setMinimumSize (fm.width ('m') * 40,
                                      fm.lineSpacing() * 4 + 4 * 2);
    }
    mDetailsText->setReadOnly (true);
    mDetailsText->setSizePolicy (QSizePolicy::Expanding,
                                 QSizePolicy::MinimumExpanding);
    mDetailsSplitter = new QIArrowSplitter (mDetailsText);
    connect (mDetailsSplitter, SIGNAL (showBackDetails()), this, SLOT (detailsBack()));
    connect (mDetailsSplitter, SIGNAL (showNextDetails()), this, SLOT (detailsNext()));
    connect (mDetailsSplitter, SIGNAL (sigSizeChanged()), this, SLOT (sltUpdateSize()));
    detailsVBoxLayout->addWidget (mDetailsSplitter);

    mFlagCB_Details = new QCheckBox();
    mFlagCB_Details->hide();
    detailsVBoxLayout->addWidget (mFlagCB_Details);

    mSpacer = new QSpacerItem (0, 0);
    layout->addItem (mSpacer);

    mButtonBox = new QIDialogButtonBox;
    mButtonBox->setCenterButtons (true);
    layout->addWidget (mButtonBox);

    mButtonEsc = 0;

    mButton0PB = createButton (iButton1);
    if (mButton0PB)
        connect (mButton0PB, SIGNAL (clicked()), SLOT (done0()));
    mButton1PB = createButton (iButton2);
    if (mButton1PB)
        connect (mButton1PB, SIGNAL (clicked()), SLOT (done1()));
    mButton2PB = createButton (iButton3);
    if (mButton2PB)
        connect (mButton2PB, SIGNAL (clicked()), SLOT (done2()));

    /* If this is an error message add an "Copy to clipboard" button for easier
     * bug reports. */
    if (icon == AlertIconType_Critical)
    {
        QPushButton *pCopyButton = createButton(AlertButton_Copy);
        pCopyButton->setToolTip(tr("Copy all errors to the clipboard"));
        connect(pCopyButton, SIGNAL(clicked()), SLOT(copy()));
    }

    /* this call is a must -- it initializes mFlagCB and mSpacer */
    setDetailsShown (false);
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
        case 0: if (mButton0PB) return mButton0PB->text(); break;
        case 1: if (mButton1PB) return mButton1PB->text(); break;
        case 2: if (mButton2PB) return mButton2PB->text(); break;
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
        case 0: if (mButton0PB) mButton0PB->setText (aText); break;
        case 1: if (mButton1PB) mButton1PB->setText (aText); break;
        case 2: if (mButton2PB) mButton2PB->setText (aText); break;
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
        mFlagCB->hide();
    }
    else
    {
        mFlagCB->setText (aText);
        mFlagCB->show();
        mFlagCB->setFocus();
    }
}

/** @fn QIMessageBox::isFlagChecked() const
 *
 *  Returns true if the optional message box flag is checked and false
 *  otherwise. By default, the flag is not checked.
 *
 *  @see #setFlagChecked()
 *  @see #setFlagText()
 */

/** @fn QIMessageBox::setFlagChecked (bool)
 *
 *  Sets the state of the optional message box flag to a value of the argument.
 *
 *  @see #isFlagChecked()
 *  @see #setFlagText()
 */

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

    QPushButton *b = mButtonBox->addButton (text, role);

    if (aButton & AlertButtonOption_Default)
    {
        b->setDefault (true);
        b->setFocus();
    }

    if (aButton & AlertButtonOption_Escape)
        mButtonEsc = aButton & AlertButtonMask;

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
        mDetailsList << QPair <QString, QString> (parts [0], parts [1]);
    }

    mDetailsSplitter->setMultiPaging (mDetailsList.size() > 1);
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
    mTextLabel->useSizeHintForWidth(mTextLabel->width());
    mTextLabel->updateGeometry();

    /* Call to base-class: */
    QIDialog::polishEvent(pPolishEvent);

    /* Make the size fixed: */
    setFixedSize(size());

    /* Toggle details-widget: */
    mDetailsSplitter->toggleWidget();
}

void QIMessageBox::refreshDetails()
{
    /* Update message text iteself */
    mTextLabel->setText (m_strMessage + mDetailsList [m_iDetailsIndex].first);
    /* Update details table */
    mDetailsText->setText (mDetailsList [m_iDetailsIndex].second);
    setDetailsShown (!mDetailsText->toPlainText().isEmpty());

    /* Update multi-paging system */
    if (mDetailsList.size() > 1)
    {
        mDetailsSplitter->setButtonEnabled (true, m_iDetailsIndex < mDetailsList.size() - 1);
        mDetailsSplitter->setButtonEnabled (false, m_iDetailsIndex > 0);
    }

    /* Update details label */
    mDetailsSplitter->setName (mDetailsList.size() == 1 ? tr ("&Details") :
        tr ("&Details (%1 of %2)").arg (m_iDetailsIndex + 1).arg (mDetailsList.size()));
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
        mFlagCB_Details->setVisible (mFlagCB_Main->isVisible());
        mFlagCB_Details->setChecked (mFlagCB_Main->isChecked());
        mFlagCB_Details->setText (mFlagCB_Main->text());
        if (mFlagCB_Main->hasFocus())
            mFlagCB_Details->setFocus();
        mFlagCB_Main->setVisible (false);
        mFlagCB = mFlagCB_Details;
        mSpacer->changeSize (0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
    }

    mDetailsVBox->setVisible (aShown);

    if (!aShown)
    {
        mFlagCB_Main->setVisible (mFlagCB_Details->isVisible());
        mFlagCB_Main->setChecked (mFlagCB_Details->isChecked());
        mFlagCB_Main->setText (mFlagCB_Details->text());
        if (mFlagCB_Details->hasFocus())
            mFlagCB_Main->setFocus();
        mFlagCB = mFlagCB_Main;
        mSpacer->changeSize (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
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
    resize(minimumSizeHint());
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
    if (m_iDetailsIndex < mDetailsList.size() - 1)
    {
        ++ m_iDetailsIndex;
        refreshDetails();
    }
}

void QIMessageBox::reject()
{
    if (mButtonEsc)
    {
        QDialog::reject();
        setResult (mButtonEsc & AlertButtonMask);
    }
}

void QIMessageBox::copy() const
{
    /* Create the error string with all errors. First the html version. */
    QString strError = "<html><body><p>" + m_strMessage + "</p>";
    for (int i = 0; i < mDetailsList.size(); ++i)
        strError += mDetailsList.at(i).first + mDetailsList.at(i).second + "<br>";
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

