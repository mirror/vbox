/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIPopupCenter class implementation
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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
#include <QVBoxLayout>
#include <QPushButton>
#include <QEvent>
#include <QMainWindow>
#include <QStatusBar>
#include <QPainter>
#include <QStateMachine>
#include <QPropertyAnimation>
#include <QSignalTransition>

/* GUI includes: */
#include "UIPopupCenter.h"
#include "UIModalWindowManager.h"
#include "QIRichTextLabel.h"
#include "QIDialogButtonBox.h"

/* Other VBox includes: */
#include <VBox/sup.h>

/* static */
UIPopupCenter* UIPopupCenter::m_spInstance = 0;
UIPopupCenter* UIPopupCenter::instance() { return m_spInstance; }

/* static */
void UIPopupCenter::create()
{
    /* Make sure instance is NOT created yet: */
    if (m_spInstance)
        return;

    /* Create instance: */
    new UIPopupCenter;
}

/* static */
void UIPopupCenter::destroy()
{
    /* Make sure instance is NOT destroyed yet: */
    if (!m_spInstance)
        return;

    /* Destroy instance: */
    delete m_spInstance;
}

UIPopupCenter::UIPopupCenter()
{
    /* Assign instance: */
    m_spInstance = this;
}

UIPopupCenter::~UIPopupCenter()
{
    /* Unassign instance: */
    m_spInstance = 0;
}

void UIPopupCenter::message(QWidget *pParent, const QString &strId,
                            const QString &strMessage, const QString &strDetails,
                            int iButton1 /*= 0*/, int iButton2 /*= 0*/, int iButton3 /*= 0*/,
                            const QString &strButtonText1 /* = QString() */,
                            const QString &strButtonText2 /* = QString() */,
                            const QString &strButtonText3 /* = QString() */) const
{
    showPopupBox(pParent, strId,
                 strMessage, strDetails,
                 iButton1, iButton2, iButton3,
                 strButtonText1, strButtonText2, strButtonText3);
}

void UIPopupCenter::error(QWidget *pParent, const QString &strId,
                          const QString &strMessage, const QString &strDetails) const
{
    message(pParent, strId,
            strMessage, strDetails,
            AlertButton_Ok | AlertButtonOption_Default | AlertButtonOption_Escape);
}

void UIPopupCenter::alert(QWidget *pParent, const QString &strId,
                          const QString &strMessage) const
{
    error(pParent, strId,
          strMessage, QString());
}

void UIPopupCenter::question(QWidget *pParent, const QString &strId,
                             const QString &strMessage,
                             int iButton1 /*= 0*/, int iButton2 /*= 0*/, int iButton3 /*= 0*/,
                             const QString &strButtonText1 /*= QString()*/,
                             const QString &strButtonText2 /*= QString()*/,
                             const QString &strButtonText3 /*= QString()*/) const
{
    message(pParent, strId,
            strMessage, QString(),
            iButton1, iButton2, iButton3,
            strButtonText1, strButtonText2, strButtonText3);
}

void UIPopupCenter::questionBinary(QWidget *pParent, const QString &strId,
                                   const QString &strMessage,
                                   const QString &strOkButtonText /*= QString()*/,
                                   const QString &strCancelButtonText /*= QString()*/) const
{
    question(pParent, strId,
             strMessage,
             AlertButton_Ok | AlertButtonOption_Default,
             AlertButton_Cancel | AlertButtonOption_Escape,
             0 /* third button */,
             strOkButtonText,
             strCancelButtonText,
             QString() /* third button */);
}

void UIPopupCenter::questionTrinary(QWidget *pParent, const QString &strId,
                                    const QString &strMessage,
                                    const QString &strChoice1ButtonText /*= QString()*/,
                                    const QString &strChoice2ButtonText /*= QString()*/,
                                    const QString &strCancelButtonText /*= QString()*/) const
{
    question(pParent, strId,
             strMessage,
             AlertButton_Choice1,
             AlertButton_Choice2 | AlertButtonOption_Default,
             AlertButton_Cancel | AlertButtonOption_Escape,
             strChoice1ButtonText,
             strChoice2ButtonText,
             strCancelButtonText);
}

void UIPopupCenter::showPopupBox(QWidget *pParent, const QString &strId,
                                 const QString &strMessage, const QString &strDetails,
                                 int iButton1, int iButton2, int iButton3,
                                 const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3) const
{
    /* Choose at least one button by 'default': */
    if (iButton1 == 0 && iButton2 == 0 && iButton3 == 0)
        iButton1 = AlertButton_Ok | AlertButtonOption_Default | AlertButtonOption_Escape;

    /* Create popup-box window: */
    QWidget *pPopupBoxParent = windowManager().realParentWindow(pParent ? pParent : windowManager().mainWindowShown());
    UIPopupPane *pPopupBox = new UIPopupPane(pPopupBoxParent, strId,
                                             strMessage, strDetails,
                                             iButton1, iButton2, iButton3,
                                             strButtonText1, strButtonText2, strButtonText3);
    m_popups.insert(strId, pPopupBox);
    connect(pPopupBox, SIGNAL(sigDone(int)), this, SLOT(sltPopupDone(int)));
    pPopupBox->show();
}

void UIPopupCenter::sltPopupDone(int iButtonCode) const
{
    /* Make sure the sender is popup: */
    const UIPopupPane *pPopup = qobject_cast<UIPopupPane*>(sender());
    if (!pPopup)
        return;

    /* Make sure the popup is still exists: */
    const QString strPopupID(pPopup->id());
    const bool fIsPopupStillHere = m_popups.contains(strPopupID);
    AssertMsg(fIsPopupStillHere, ("Popup already destroyed!"));
    if (!fIsPopupStillHere)
        return;

    /* Notify listeners: */
    emit sigPopupDone(strPopupID, iButtonCode);

    /* Cleanup the popup: */
    m_popups.remove(strPopupID);
    delete pPopup;
}

void UIPopupCenter::remindAboutMouseIntegration(bool fSupportsAbsolute) const
{
    if (fSupportsAbsolute)
    {
        alert(0, QString("remindAboutMouseIntegrationOn"),
              tr("<p>The Virtual Machine reports that the guest OS supports <b>mouse pointer integration</b>. "
                 "This means that you do not need to <i>capture</i> the mouse pointer to be able to use it in your guest OS -- "
                 "all mouse actions you perform when the mouse pointer is over the Virtual Machine's display "
                 "are directly sent to the guest OS. If the mouse is currently captured, it will be automatically uncaptured.</p>"
                 "<p>The mouse icon on the status bar will look like&nbsp;<img src=:/mouse_seamless_16px.png/>&nbsp;to inform you "
                 "that mouse pointer integration is supported by the guest OS and is currently turned on.</p>"
                 "<p><b>Note</b>: Some applications may behave incorrectly in mouse pointer integration mode. "
                 "You can always disable it for the current session (and enable it again) "
                 "by selecting the corresponding action from the menu bar.</p>"));
    }
    else
    {
        alert(0, QString("remindAboutMouseIntegrationOff"),
              tr("<p>The Virtual Machine reports that the guest OS does not support <b>mouse pointer integration</b> "
                 "in the current video mode. You need to capture the mouse (by clicking over the VM display "
                 "or pressing the host key) in order to use the mouse inside the guest OS.</p>"));
    }
}

void UIAnimationFramework::installPropertyAnimation(QWidget *pParent, const QByteArray &strPropertyName,
                                                    int iStartValue, int iFinalValue, int iAnimationDuration,
                                                    const char *pSignalForward, const char *pSignalBackward)
{
    /* State-machine: */
    QStateMachine *pStateMachine = new QStateMachine(pParent);
    /* State-machine 'start' state: */
    QState *pStateStart = new QState(pStateMachine);
    /* State-machine 'final' state: */
    QState *pStateFinal = new QState(pStateMachine);

    /* State-machine 'forward' animation: */
    QPropertyAnimation *pForwardAnimation = new QPropertyAnimation(pParent, strPropertyName, pParent);
    pForwardAnimation->setDuration(iAnimationDuration);
    pForwardAnimation->setStartValue(iStartValue);
    pForwardAnimation->setEndValue(iFinalValue);
    /* State-machine 'backward' animation: */
    QPropertyAnimation *pBackwardAnimation = new QPropertyAnimation(pParent, strPropertyName, pParent);
    pBackwardAnimation->setDuration(iAnimationDuration);
    pBackwardAnimation->setStartValue(iFinalValue);
    pBackwardAnimation->setEndValue(iStartValue);

    /* State-machine state transitions: */
    QSignalTransition *pDefaultToHovered = pStateStart->addTransition(pParent, pSignalForward, pStateFinal);
    pDefaultToHovered->addAnimation(pForwardAnimation);
    QSignalTransition *pHoveredToDefault = pStateFinal->addTransition(pParent, pSignalBackward, pStateStart);
    pHoveredToDefault->addAnimation(pBackwardAnimation);

    /* Initial state is 'start': */
    pStateMachine->setInitialState(pStateStart);
    /* Start hover-machine: */
    pStateMachine->start();
}

UIPopupPane::UIPopupPane(QWidget *pParent, const QString &strId,
                         const QString &strMessage, const QString &strDetails,
                         int iButton1, int iButton2, int iButton3,
                         const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3)
    : QWidget(pParent)
    , m_fPolished(false)
    , m_strId(strId)
    , m_strMessage(strMessage), m_strDetails(strDetails)
    , m_iButton1(iButton1), m_iButton2(iButton2), m_iButton3(iButton3)
    , m_strButtonText1(strButtonText1), m_strButtonText2(strButtonText2), m_strButtonText3(strButtonText3)
    , m_iButtonEsc(0)
    , m_iParentStatusBarHeight(parentStatusBarHeight(pParent))
    , m_pMainLayout(0), m_pFrameLayout(0)
    , m_pTextPane(0), m_pButtonBox(0)
    , m_pButton1(0), m_pButton2(0), m_pButton3(0)
{
    /* Prepare: */
    prepare();
}

UIPopupPane::~UIPopupPane()
{
    /* Cleanup: */
    cleanup();
}

void UIPopupPane::prepare()
{
    /* Install event-filter to parent: */
    parent()->installEventFilter(this);
    /* Prepare content: */
    prepareContent();
}

void UIPopupPane::cleanup()
{
}

bool UIPopupPane::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Make sure its parent event came: */
    if (pWatched != parent())
        return false;

    /* Make sure its resize event came: */
    if (pEvent->type() != QEvent::Resize)
        return false;

    /* Adjust geometry: */
    adjustAccordingParent();

    /* Do not filter anything: */
    return false;
}

void UIPopupPane::showEvent(QShowEvent *pEvent)
{
    /* Make sure we should polish dialog: */
    if (m_fPolished)
        return;

    /* Call to polish-event: */
    polishEvent(pEvent);

    /* Mark dialog as polished: */
    m_fPolished = true;
}

void UIPopupPane::polishEvent(QShowEvent*)
{
    /* Adjust geometry: */
    adjustAccordingParent();
}

void UIPopupPane::keyPressEvent(QKeyEvent *pEvent)
{
    /* Preprocess Escape key: */
    if (pEvent->key() == Qt::Key_Escape && m_iButtonEsc)
    {
        done(m_iButtonEsc);
        return;
    }
    /* Handle all the other keys: */
    QWidget::keyPressEvent(pEvent);
}

void UIPopupPane::adjustAccordingParent()
{
    /* Get some variables: */
    const int iWidth = parentWidget()->width();
    const int iHeight = parentWidget()->height();

    /* Resize popup according parent: */
    resize(iWidth, minimumSizeHint().height());

    /* Move popup according parent: */
    move(0, iHeight - height() - m_iParentStatusBarHeight);

    /* Raise popup according parent: */
    raise();

    /* Resize text-pane according new size: */
    if (m_pTextPane)
    {
        int iMinimumTextWidth = iWidth;
        int iLeft, iTop, iRight, iBottom;
        m_pMainLayout->getContentsMargins(&iLeft, &iTop, &iRight, &iBottom);
        iMinimumTextWidth -= iLeft;
        iMinimumTextWidth -= iRight;
        m_pFrameLayout->getContentsMargins(&iLeft, &iTop, &iRight, &iBottom);
        iMinimumTextWidth -= iLeft;
        iMinimumTextWidth -= iRight;
        m_pTextPane->setMinimumTextWidth(iMinimumTextWidth);
    }
}

void UIPopupPane::prepareContent()
{
    /* Configure this: */
    setFocusPolicy(Qt::StrongFocus);
    /* Create main-layout: */
    m_pMainLayout = new QVBoxLayout(this);
    {
        /* Configure layout: */
        m_pMainLayout->setContentsMargins(2, 2, 2, 2);
        m_pMainLayout->setSpacing(0);
        /* Create main frame: */
        UIPopupPaneFrame *pMainFrame = new UIPopupPaneFrame;
        {
            /* Add into layout: */
            m_pMainLayout->addWidget(pMainFrame);
            /* Create frame-layout: */
            m_pFrameLayout = new QVBoxLayout(pMainFrame);
            {
                /* Configure layout: */
                m_pFrameLayout->setContentsMargins(15, 0, 15, 0);
                m_pFrameLayout->setSpacing(0);
                /* Create message-label: */
                m_pTextPane = new QIRichTextLabel;
                {
                    /* Add into layout: */
                    m_pFrameLayout->addWidget(m_pTextPane);
                    /* Configure label: */
                    m_pTextPane->setFocusPolicy(Qt::StrongFocus);
                    m_pTextPane->installEventFilter(pMainFrame);
                    m_pTextPane->setText(m_strMessage);
                    m_pTextPane->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
                }
                /* Create button-box: */
                m_pButtonBox = new QIDialogButtonBox;
                {
                    /* Add into layout: */
                    m_pFrameLayout->addWidget(m_pButtonBox);
                    /* Configure button-box: */
                    m_pButtonBox->installEventFilter(pMainFrame);
                    prepareButtons();
                }
            }
        }
    }
}

void UIPopupPane::prepareButtons()
{
    /* Prepare descriptions: */
    QList<int> descriptions;
    descriptions << m_iButton1 << m_iButton2 << m_iButton3;

    /* Choose 'escape' button: */
    foreach (int iButton, descriptions)
        if (iButton & AlertButtonOption_Escape)
        {
            m_iButtonEsc = iButton & AlertButtonMask;
            break;
        }

    /* Create buttons: */
    QList<QPushButton*> buttons = createButtons(m_pButtonBox, descriptions);

    /* Install focus-proxy into the 'default' button: */
    foreach (QPushButton *pButton, buttons)
        if (pButton && pButton->isDefault())
        {
            setFocusProxy(pButton);
            m_pTextPane->setFocusProxy(pButton);
            break;
        }

    /* Prepare button 1: */
    m_pButton1 = buttons[0];
    if (m_pButton1)
    {
        connect(m_pButton1, SIGNAL(clicked()), SLOT(done1()));
        if (!m_strButtonText1.isEmpty())
            m_pButton1->setText(m_strButtonText1);
    }
    /* Prepare button 2: */
    m_pButton2 = buttons[1];
    if (m_pButton2)
    {
        connect(m_pButton2, SIGNAL(clicked()), SLOT(done2()));
        if (!m_strButtonText2.isEmpty())
            m_pButton1->setText(m_strButtonText2);
    }
    /* Prepare button 3: */
    m_pButton3 = buttons[2];
    if (m_pButton3)
    {
        connect(m_pButton3, SIGNAL(clicked()), SLOT(done3()));
        if (!m_strButtonText3.isEmpty())
            m_pButton1->setText(m_strButtonText3);
    }
}

void UIPopupPane::done(int iButtonCode)
{
    /* Close the window: */
    close();

    /* Notify listeners: */
    emit sigDone(iButtonCode);
}

/* static */
int UIPopupPane::parentStatusBarHeight(QWidget *pParent)
{
    /* Check if passed parent is QMainWindow and contains status-bar: */
    if (QMainWindow *pParentWindow = qobject_cast<QMainWindow*>(pParent))
        if (pParentWindow->statusBar())
            return pParentWindow->statusBar()->height();
    /* Zero by default: */
    return 0;
}

/* static */
QList<QPushButton*> UIPopupPane::createButtons(QIDialogButtonBox *pButtonBox, const QList<int> descriptions)
{
    /* Create button according descriptions: */
    QList<QPushButton*> buttons;
    foreach (int iButton, descriptions)
        buttons << createButton(pButtonBox, iButton);
    /* Return buttons: */
    return buttons;
}

/* static */
QPushButton* UIPopupPane::createButton(QIDialogButtonBox *pButtonBox, int iButton)
{
    /* Null for AlertButton_NoButton: */
    if (iButton == 0)
        return 0;

    /* Prepare button text & role: */
    QString strText;
    QDialogButtonBox::ButtonRole role;
    switch (iButton & AlertButtonMask)
    {
        case AlertButton_Ok:      strText = QIMessageBox::tr("OK");     role = QDialogButtonBox::AcceptRole; break;
        case AlertButton_Cancel:  strText = QIMessageBox::tr("Cancel"); role = QDialogButtonBox::RejectRole; break;
        case AlertButton_Choice1: strText = QIMessageBox::tr("Yes");    role = QDialogButtonBox::YesRole; break;
        case AlertButton_Choice2: strText = QIMessageBox::tr("No");     role = QDialogButtonBox::NoRole; break;
        default: return 0;
    }

    /* Create push-button: */
    QPushButton *pButton = pButtonBox->addButton(strText, role);

    /* Configure 'default' button: */
    if (iButton & AlertButtonOption_Default)
    {
        pButton->setDefault(true);
        pButton->setFocusPolicy(Qt::StrongFocus);
        pButton->setFocus();
    }

    /* Return button: */
    return pButton;
}

UIPopupPaneFrame::UIPopupPaneFrame(QWidget *pParent /*= 0*/)
    : QWidget(pParent)
    , m_fHovered(false)
    , m_iDefaultOpacity(128)
    , m_iHoveredOpacity(230)
    , m_iOpacity(m_iDefaultOpacity)
    , m_iHoverAnimationDuration(300)
{
    /* Prepare: */
    prepare();
}

UIPopupPaneFrame::~UIPopupPaneFrame()
{
    /* Cleanup: */
    cleanup();
}

void UIPopupPaneFrame::prepare()
{
    /* Install event-filter: */
    installEventFilter(this);
    /* Install 'hover' animation for 'opacity' property: */
    UIAnimationFramework::installPropertyAnimation(this, QByteArray("opacity"),
                                                   m_iDefaultOpacity, m_iHoveredOpacity, m_iHoverAnimationDuration,
                                                   SIGNAL(sigHoverEnter()), SIGNAL(sigHoverLeave()));
}

void UIPopupPaneFrame::cleanup()
{
}

bool UIPopupPaneFrame::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Depending on event-type: */
    switch (pEvent->type())
    {
        /* Something is hovered: */
        case QEvent::HoverEnter:
        case QEvent::Enter:
        {
            if (!m_fHovered)
            {
                m_fHovered = true;
                emit sigHoverEnter();
            }
            break;
        }
        /* Nothing is hovered: */
        case QEvent::Leave:
        {
            if (pObject == this && m_fHovered)
            {
                m_fHovered = false;
                emit sigHoverLeave();
            }
            break;
        }
        /* Default case: */
        default: break;
    }
    /* Do not filter anything: */
    return false;
}

void UIPopupPaneFrame::paintEvent(QPaintEvent*)
{
    /* Compose painting rectangle: */
    const QRect rect(0, 0, width(), height());

    /* Create painter: */
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    /* Configure painter clipping: */
    QPainterPath path;
    int iDiameter = 5;
    QSizeF arcSize(2 * iDiameter, 2 * iDiameter);
    path.moveTo(iDiameter, 0);
    path.arcTo(QRectF(path.currentPosition(), arcSize).translated(-iDiameter, 0), 90, 90);
    path.lineTo(path.currentPosition().x(), rect.height() - iDiameter);
    path.arcTo(QRectF(path.currentPosition(), arcSize).translated(0, -iDiameter), 180, 90);
    path.lineTo(rect.width() - iDiameter, path.currentPosition().y());
    path.arcTo(QRectF(path.currentPosition(), arcSize).translated(-iDiameter, -2 * iDiameter), 270, 90);
    path.lineTo(path.currentPosition().x(), iDiameter);
    path.arcTo(QRectF(path.currentPosition(), arcSize).translated(-2 * iDiameter, -iDiameter), 0, 90);
    path.closeSubpath();
    painter.setClipPath(path);

    /* Fill with background: */
    QColor currentColor(palette().color(QPalette::Window));
    QColor newColor(currentColor.red(), currentColor.green(), currentColor.blue(), opacity());
    painter.fillRect(rect, newColor);
}

