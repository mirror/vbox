/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIMessageBox class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/* VBox includes */
#include "VBoxDefs.h"
#include "VBoxGlobal.h"
#include "QIMessageBox.h"
#include "QILabel.h"
#include "QIDialogButtonBox.h"
#ifdef Q_WS_MAC
# include "VBoxConsoleWnd.h"
#endif /* Q_WS_MAC */

/* Qt includes */
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyleOptionFocusRect>
#include <QStylePainter>
#include <QToolButton>

/** @class QIArrowButton
 *
 *  The QIArrowButton class is an arrow tool-botton with text-label.
 */
QIArrowButton::QIArrowButton (const QString &aName, QWidget *aParent)
    : QWidget (aParent)
    , mIsExpanded (false)
    , mButton (new QToolButton())
    , mLabel (new QLabel (aName))
{
    /* Setup itself */
    setFocusPolicy (Qt::StrongFocus);

    /* Setup tool-button */
    mButton->setAutoRaise (true);
    mButton->setFixedSize (17, 16);
    mButton->setFocusPolicy (Qt::NoFocus);
    mButton->setStyleSheet ("QToolButton {border: 0px none black;}");
    connect (mButton, SIGNAL (clicked (bool)), this, SLOT (buttonClicked()));
    updateIcon();

    /* Setup text-label */
    mLabel->setBuddy (mButton);
    mLabel->setStyleSheet ("QLabel {padding: 2px 0px 2px 0px;}");

    /* Setup main-layout */
    QHBoxLayout *mainLayout = new QHBoxLayout (this);
    VBoxGlobal::setLayoutMargin (mainLayout, 0);
    mainLayout->setSpacing (0);
    mainLayout->addWidget (mButton);
    mainLayout->addWidget (mLabel);
    mainLayout->addStretch();

    /* Install event-filter */
    qApp->installEventFilter (this);
}

bool QIArrowButton::isExpanded() const
{
    return mIsExpanded;
}

void QIArrowButton::animateClick()
{
    mButton->animateClick();
}

void QIArrowButton::buttonClicked()
{
    mIsExpanded = !mIsExpanded;
    updateIcon();
    emit clicked();
}

void QIArrowButton::updateIcon()
{
    mButton->setIcon (VBoxGlobal::iconSet (mIsExpanded ?
                      ":/arrow_down_10px.png" : ":/arrow_right_10px.png"));
}

bool QIArrowButton::eventFilter (QObject *aObject, QEvent *aEvent)
{
    /* Process only QIArrowButton or children */
    if (!(aObject == this || children().contains (aObject)))
        return QWidget::eventFilter (aObject, aEvent);

    /* Process some keyboard events */
    if (aEvent->type() == QEvent::KeyPress)
    {
        QKeyEvent *kEvent = static_cast <QKeyEvent*> (aEvent);
        switch (kEvent->key())
        {
            /* "+" as expand */
            case Qt::Key_Plus:
            {
                if (!mIsExpanded)
                    mButton->animateClick();
                break;
            }
            /* "-" as collapse */
            case Qt::Key_Minus:
            {
                if (mIsExpanded)
                    mButton->animateClick();
                break;
            }
            /* Space as toggle */
            case Qt::Key_Space:
            {
                mButton->animateClick();
                break;
            }
        }
    }

    /* Process some mouse events */
    if ((aEvent->type() == QEvent::MouseButtonPress ||
         aEvent->type() == QEvent::MouseButtonDblClick)
        && aObject == mLabel)
    {
        /* Label click as toggle */
        mButton->animateClick();
    }

    /* Default one handler */
    return QWidget::eventFilter (aObject, aEvent);
}

void QIArrowButton::paintEvent (QPaintEvent *aEvent)
{
    if (hasFocus())
    {
        QStylePainter painter (this);
        QStyleOptionFocusRect option;
        option.initFrom (this);
        option.rect = mLabel->frameGeometry();
        painter.drawPrimitive (QStyle::PE_FrameFocusRect, option);
    }
    QWidget::paintEvent (aEvent);
}

/** @class QIArrowSplitter
 *
 *  The QIArrowSplitter class is a folding widget placeholder.
 */
QIArrowSplitter::QIArrowSplitter (QWidget *aParent)
    : QWidget (aParent)
    , mMainLayout (new QVBoxLayout (this))
{
    /* Setup main-layout */
    VBoxGlobal::setLayoutMargin (mMainLayout, 0);

    /* Install event-filter */
    qApp->installEventFilter (this);
}

void QIArrowSplitter::addWidget (const QString &aName, QWidget *aWidget)
{
    /* Creating arrow button */
    QIArrowButton *button = new QIArrowButton (aName);
    connect (button, SIGNAL (clicked()), this, SLOT (toggleWidget()));

    /* Append internal lists */
    mButtonsList.append (button);
    mWidgetsList.append (aWidget);

    /* Append layout with children */
    mMainLayout->addWidget (button);
    mMainLayout->addWidget (aWidget);
}

void QIArrowSplitter::toggleWidget()
{
    QIArrowButton *clickedButton = qobject_cast <QIArrowButton*> (sender());

    foreach (QIArrowButton *button, mButtonsList)
    {
        if ((button == clickedButton) || !clickedButton)
        {
            QWidget *relatedWidget = mWidgetsList [mButtonsList.indexOf (button)];
            Assert (relatedWidget);
            relatedWidget->setVisible (button->isExpanded());
        }
    }

    /* Update full layout system of message window */
    QList <QLayout*> layouts = findChildren <QLayout*> ();
    foreach (QLayout *item, layouts)
    {
        item->update();
        item->activate();
    }

    /* Update main layout of message window at last */
    window()->layout()->update();
    window()->layout()->activate();
    qApp->processEvents();

    /* Now resize window to minimum possible size */
    window()->resize (window()->minimumSizeHint());
    qApp->processEvents();

    /* Check if we have to make dialog fixed in height */
    bool makeFixedHeight = true;
    foreach (QIArrowButton *button, mButtonsList)
    {
        if (button->isExpanded())
        {
            makeFixedHeight = false;
            break;
        }
    }
    if (makeFixedHeight)
        window()->setFixedHeight (window()->minimumSizeHint().height());
    else
        window()->setMaximumHeight (QWIDGETSIZE_MAX);
}

bool QIArrowSplitter::eventFilter (QObject *aObject, QEvent *aEvent)
{
    /* Process only parent window children */
    if (!(aObject == window() || window()->children().contains (aObject)))
        return QWidget::eventFilter (aObject, aEvent);

    /* Do not process QIArrowButton children */
    foreach (QIArrowButton *button, mButtonsList)
        if (button->children().contains (aObject))
            return QWidget::eventFilter (aObject, aEvent);

    /* Process some keyboard events */
    if (aEvent->type() == QEvent::KeyPress)
    {
        QKeyEvent *kEvent = static_cast <QKeyEvent*> (aEvent);
        switch (kEvent->key())
        {
            case Qt::Key_Plus:
            {
                foreach (QIArrowButton *button, mButtonsList)
                    if (!button->isExpanded())
                        button->animateClick();
                break;
            }
            case Qt::Key_Minus:
            {
                foreach (QIArrowButton *button, mButtonsList)
                    if (button->isExpanded())
                        button->animateClick();
                break;
            }
        }
    }

    /* Default one handler */
    return QWidget::eventFilter (aObject, aEvent);
}

/** @class QIMessageBox
 *
 *  The QIMessageBox class is a message box similar to QMessageBox.
 *  It partly implements the QMessageBox interface and adds some enhanced
 *  functionality.
 */

/**
 *  See QMessageBox for details.
 */
QIMessageBox::QIMessageBox (const QString &aCaption, const QString &aText,
                            Icon aIcon, int aButton0, int aButton1, int aButton2,
                            QWidget *aParent, const char *aName, bool aModal)
    : QIDialog (aParent)
    , mWasDone (false)
    , mWasPolished (false)
{
#ifdef Q_WS_MAC
    /* Sheets are broken if the window is in fullscreen mode. So make it a
     * normal window in that case. */
    VBoxConsoleWnd *cwnd = qobject_cast<VBoxConsoleWnd*> (aParent);
    if (cwnd == NULL ||
        (!cwnd->isTrueFullscreen() &&
         !cwnd->isTrueSeamless()))
        setWindowFlags (Qt::Sheet);
#endif /* Q_WS_MAC */


    setWindowTitle (aCaption);
    /* Necessary to later find some of the message boxes */
    setObjectName (aName);
    setModal (aModal);

    mButton0 = aButton0;
    mButton1 = aButton1;
    mButton2 = aButton2;

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
    mIconLabel->setPixmap (standardPixmap (aIcon));
    mIconLabel->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Minimum);
    mIconLabel->setAlignment (Qt::AlignHCenter | Qt::AlignTop);
    hLayout->addWidget (mIconLabel);

    QVBoxLayout* messageVBoxLayout = new QVBoxLayout();
    VBoxGlobal::setLayoutMargin (messageVBoxLayout, 0);
    messageVBoxLayout->setSpacing (10);
    hLayout->addLayout (messageVBoxLayout);

    mTextLabel = new QILabel (aText);
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

    mDetailsSplitter = new QIArrowSplitter();
    detailsVBoxLayout->addWidget (mDetailsSplitter);

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
    mDetailsSplitter->addWidget (tr ("&Details"), mDetailsText);

    mFlagCB_Details = new QCheckBox();
    mFlagCB_Details->hide();
    detailsVBoxLayout->addWidget (mFlagCB_Details);

    mSpacer = new QSpacerItem (0, 0);
    layout->addItem (mSpacer);

    mButtonBox = new QIDialogButtonBox;
    mButtonBox->setCenterButtons (true);
    layout->addWidget (mButtonBox);

    mButtonEsc = 0;

    mButton0PB = createButton (aButton0);
    if (mButton0PB)
        connect (mButton0PB, SIGNAL (clicked()), SLOT (done0()));
    mButton1PB = createButton (aButton1);
    if (mButton1PB)
        connect (mButton1PB, SIGNAL (clicked()), SLOT (done1()));
    mButton2PB = createButton (aButton2);
    if (mButton2PB)
        connect (mButton2PB, SIGNAL (clicked()), SLOT (done2()));

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
    switch (aButton & ButtonMask)
    {
        case Ok:        text = tr ("OK"); role = QDialogButtonBox::AcceptRole; break;
        case Yes:       text = tr ("Yes"); role = QDialogButtonBox::YesRole; break;
        case No:        text = tr ("No"); role = QDialogButtonBox::NoRole; break;
        case Cancel:    text = tr ("Cancel"); role = QDialogButtonBox::RejectRole; break;
        case Ignore:    text = tr ("Ignore"); role = QDialogButtonBox::AcceptRole; break;
        default:
            AssertMsgFailed (("Type %d is not implemented", aButton));
            return NULL;
    }

    QPushButton *b = mButtonBox->addButton (text, role);

    if (aButton & Default)
    {
        b->setDefault (true);
        b->setFocus();
    }

    if (aButton & Escape)
        mButtonEsc = aButton & ButtonMask;

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
    QStringList parts (aText.split ("<!--EOM-->", QString::SkipEmptyParts));
    mTextLabel->setText (mTextLabel->text() + parts [0]);
    if (parts.size() > 1) mDetailsText->setText (parts [1]);
}

/** @fn QIMessageBox::isDetailsShown() const
 *
 *  Returns true if the optional details box is shown and false otherwise.
 *  By default, the details box is not shown.
 *
 *  @see #setDetailsShown()
 *  @see #setDetailsText()
 */

/**
 *  Sets the visibility state of the optional details box
 *  to a value of the argument.
 *
 *  @see #isDetailsShown()
 *  @see #setDetailsText()
 */
void QIMessageBox::setDetailsShown (bool aShown)
{
    if (aShown && mDetailsText->toPlainText().isEmpty())
        return;

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

QPixmap QIMessageBox::standardPixmap (QIMessageBox::Icon aIcon)
{
    QIcon icon;
    switch (aIcon)
    {
        case QIMessageBox::Information:
            icon = vboxGlobal().standardIcon (QStyle::SP_MessageBoxInformation, this);
            break;
        case QMessageBox::Warning:
            icon = vboxGlobal().standardIcon (QStyle::SP_MessageBoxWarning, this);
            break;
        case QIMessageBox::Critical:
            icon = vboxGlobal().standardIcon (QStyle::SP_MessageBoxCritical, this);
            break;
        case QIMessageBox::Question:
            icon = vboxGlobal().standardIcon (QStyle::SP_MessageBoxQuestion, this);
            break;
        case QIMessageBox::GuruMeditation:
            icon = QIcon (":/meditation_32px.png");
            break;
        default:
            break;
    }
    if (!icon.isNull())
    {
        int size = style()->pixelMetric (QStyle::PM_MessageBoxIconSize, 0, this);
        return icon.pixmap (size, size);
    }else
        return QPixmap();
}

void QIMessageBox::closeEvent (QCloseEvent *e)
{
    if (mWasDone)
        e->accept();
    else
        e->ignore();
}

void QIMessageBox::showEvent (QShowEvent *e)
{
    if (!mWasPolished)
    {
        /* Polishing sub-widgets */
        resize (minimumSizeHint());
        qApp->processEvents();
        mTextLabel->setMinimumWidth (mTextLabel->width());
        mTextLabel->updateSizeHint();
        qApp->processEvents();
        setFixedWidth (width());
        mDetailsSplitter->toggleWidget();
        mWasPolished = true;
    }

    QIDialog::showEvent (e);
}

void QIMessageBox::reject()
{
    if (mButtonEsc)
    {
        QDialog::reject();
        setResult (mButtonEsc & ButtonMask);
    }
}

