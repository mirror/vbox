/* $Id$ */
/** @file
 * VBox Qt GUI - UIAdvancedSettingsDialog class implementation.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QGridLayout>
#include <QPainter>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "QILineEdit.h"
#include "UIAdvancedSettingsDialog.h"
#include "UIAnimationFramework.h"
#include "UICommon.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIImageTools.h"
#include "UIMessageCenter.h"
#include "UIModalWindowManager.h"
#include "UIPopupCenter.h"
#include "UISettingsPage.h"
#include "UISettingsPageValidator.h"
#include "UISettingsSelector.h"
#include "UISettingsSerializer.h"
#include "UISettingsWarningPane.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils.h"
#endif

#ifdef VBOX_WS_MAC
//# define VBOX_GUI_WITH_TOOLBAR_SETTINGS
#endif

#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
# include "QIToolBar.h"
#endif


/** QCheckBox subclass used as mode checkbox. */
class UIModeCheckBox : public QCheckBox
{
    Q_OBJECT;

public:

    /** Constructs checkbox passing @a pParent to the base-class. */
    UIModeCheckBox(QWidget *pParent);

    /** Returns text 1. */
    QString text1() const { return m_strText1; }
    /** Defines @a strText1. */
    void setText1(const QString &strText1) { m_strText1 = strText1; }
    /** Returns text 2. */
    QString text2() const { return m_strText2; }
    /** Defines @a strText2. */
    void setText2(const QString &strText2) { m_strText2 = strText2; }

protected:

    /** Handles any @a pEvent. */
    virtual bool event(QEvent *pEvent) RT_OVERRIDE;

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) RT_OVERRIDE;

private:

    /** Returns text 1. */
    QString  m_strText1;
    /** Returns text 2. */
    QString  m_strText2;
};


/** QWidget reimplementation
  * wrapping custom QILineEdit and
  * representing filter editor for advanced settings dialog. */
class UIFilterEditor : public QWidget
{
    Q_OBJECT;
    Q_PROPERTY(int editorWidth READ editorWidth WRITE setEditorWidth);
    Q_PROPERTY(int unfocusedEditorWidth READ unfocusedEditorWidth);
    Q_PROPERTY(int focusedEditorWidth READ focusedEditorWidth);

signals:

    /** Notifies listeners about @a strText changed. */
    void sigTextChanged(const QString &strText);

    /** Notifies listeners about editor focused. */
    void sigFocused();
    /** Notifies listeners about editor unfocused. */
    void sigUnfocused();

public:

    /** Constructs filter editor passing @a pParent to the base-class. */
    UIFilterEditor(QWidget *pParent);
    /** Destructs filter editor. */
    virtual ~UIFilterEditor() RT_OVERRIDE;

    /** Defines placeholder @a strText. */
    void setPlaceholderText(const QString &strText);

    /** Returns filter editor text. */
    QString text() const;

protected:

    /** Returns the minimum widget size. */
    virtual QSize minimumSizeHint() const RT_OVERRIDE;

    /** Handles resize @a pEvent. */
    virtual void resizeEvent(QResizeEvent *pEvent) RT_OVERRIDE;

    /** Preprocesses Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) RT_OVERRIDE;

private slots:

    /** Handles editor @a strText change. */
    void sltHandleEditorTextChanged(const QString &strText);
    /** Handles button click. */
    void sltHandleButtonClicked();

private:

    /** Prepares all. */
    void prepare();
    /** Cleanups all. */
    void cleanup();

    /** Adjusts editor geometry. */
    void adjustEditorGeometry();

    /** Defines internal widget @a iWidth. */
    void setEditorWidth(int iWidth);
    /** Returns internal widget width. */
    int editorWidth() const;
    /** Returns internal widget width when it's unfocused. */
    int unfocusedEditorWidth() const { return m_iUnfocusedEditorWidth; }
    /** Returns internal widget width when it's focused. */
    int focusedEditorWidth() const { return m_iFocusedEditorWidth; }

    /** Holds the filter editor instance. */
    QILineEdit  *m_pLineEdit;
    /** Holds the filter reset button instance. */
    QToolButton *m_pToolButton;

    /** Holds whether filter editor focused. */
    bool         m_fFocused;
    /** Holds unfocused filter editor width. */
    int          m_iUnfocusedEditorWidth;
    /** Holds focused filter editor width. */
    int          m_iFocusedEditorWidth;
    /** Holds the animation framework object. */
    UIAnimation *m_pAnimation;
};


/** QScrollArea extension to be used for
  * advanced settings dialog. The idea is to make
  * vertical scroll-bar always visible, keeping
  * horizontal scroll-bar always hidden. */
class UIVerticalScrollArea : public QScrollArea
{
    Q_OBJECT;
    Q_PROPERTY(int verticalScrollBarPosition READ verticalScrollBarPosition WRITE setVerticalScrollBarPosition);

signals:

    /** Notifies listeners about wheel-event. */
    void sigWheelEvent();

public:

    /** Constructs vertical scroll-area passing @a pParent to the base-class. */
    UIVerticalScrollArea(QWidget *pParent);

    /** Returns vertical scrollbar position. */
    int verticalScrollBarPosition() const;
    /** Defines vertical scrollbar @a iPosition. */
    void setVerticalScrollBarPosition(int iPosition) const;

    /** Requests vertical scrollbar @a iPosition. */
    void requestVerticalScrollBarPosition(int iPosition);

protected:

    /** Returns the minimum widget size. */
    virtual QSize minimumSizeHint() const RT_OVERRIDE;

    /** Handles wheel @a pEvent. */
    virtual void wheelEvent(QWheelEvent *pEvent) RT_OVERRIDE;

private:

    /** Prepares all. */
    void prepare();

    /** Holds the vertical scrollbar animation instance. */
    QPropertyAnimation *m_pAnimation;
};


/*********************************************************************************************************************************
*   Class UIModeCheckBox implementation.                                                                                         *
*********************************************************************************************************************************/

UIModeCheckBox::UIModeCheckBox(QWidget *pParent)
    : QCheckBox(pParent)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

bool UIModeCheckBox::event(QEvent *pEvent)
{
    /* Handle desired events: */
    switch (pEvent->type())
    {
        /* Handles mouse button press/release: */
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        {
            /* Handle release, ignore press: */
            if (pEvent->type() == QEvent::MouseButtonRelease)
            {
                QMouseEvent *pMouseEvent = static_cast<QMouseEvent*>(pEvent);
                setCheckState(pMouseEvent->pos().x() < width() / 2 ? Qt::Unchecked : Qt::Checked);
            }
            /* Prevent from handling somewhere else: */
            pEvent->accept();
            return true;
        }

        default:
            break;
    }

    return QCheckBox::event(pEvent);
}

void UIModeCheckBox::paintEvent(QPaintEvent *pEvent)
{
    /* Acquire useful properties: */
    const QPalette pal = QGuiApplication::palette();
    QRect contentRect = pEvent->rect();
#ifdef VBOX_WS_MAC
    contentRect.setLeft(contentRect.left() + 2); /// @todo justify!
    contentRect.setWidth(contentRect.width() - 10); /// @todo justify!
#endif

    /* Prepare painter: */
    QPainter painter(this);

    /* Prepare left painter paths: */
    QPainterPath painterPath1;
    painterPath1.lineTo(contentRect.width() / 2 - 1,                    0);
    painterPath1.lineTo(contentRect.width() / 2 - contentRect.height(), contentRect.height());
    painterPath1.lineTo(0,                                              contentRect.height());
    painterPath1.closeSubpath();

    /* Prepare right painter paths: */
    QPainterPath painterPath2;
    painterPath2.moveTo(contentRect.width() / 2 + 1,                        0);
    painterPath2.lineTo(contentRect.width(),                                0);
    painterPath2.lineTo(contentRect.width() - contentRect.height(),         contentRect.height());
    painterPath2.lineTo(contentRect.width() / 2 + 1 - contentRect.height(), contentRect.height());
    painterPath2.closeSubpath();

    /* Prepare left painting gradient: */
    const QColor backColor1 = pal.color(QPalette::Active, isChecked() ? QPalette::Window : QPalette::Highlight);
    const QColor bcTone11 = backColor1.lighter(isChecked() ? 120 : 100);
    const QColor bcTone12 = backColor1.lighter(isChecked() ? 140 : 120);
    QLinearGradient grad1(painterPath1.boundingRect().topLeft(), painterPath1.boundingRect().bottomRight());
    grad1.setColorAt(0, bcTone11);
    grad1.setColorAt(1, bcTone12);

    /* Prepare right painting gradient: */
    const QColor backColor2 = pal.color(QPalette::Active, isChecked() ? QPalette::Highlight : QPalette::Window);
    const QColor bcTone21 = backColor2.lighter(isChecked() ? 100 : 120);
    const QColor bcTone22 = backColor2.lighter(isChecked() ? 120 : 140);
    QLinearGradient grad2(painterPath2.boundingRect().topLeft(), painterPath2.boundingRect().bottomRight());
    grad2.setColorAt(0, bcTone21);
    grad2.setColorAt(1, bcTone22);

    /* Paint fancy shape: */
    painter.save();
    painter.setClipPath(painterPath1);
    painter.fillRect(contentRect, grad1);
    painter.setClipPath(painterPath2);
    painter.fillRect(contentRect, grad2);
    painter.restore();

    /* Prepare text1/text2: */
    const QFont fnt = font();
    const QFontMetrics fm(fnt);
    const QColor foreground1 = suitableForegroundColor(pal, backColor1);
    const QString strName1 = text1();
    const QPoint point1 = QPoint(contentRect.left() + 5 /** @todo justify! */, contentRect.height() / 2 + fm.ascent() / 2 - 1 /* base line */);
    const QColor foreground2 = suitableForegroundColor(pal, backColor2);
    const QString strName2 = text2();
    const QPoint point2 = QPoint(contentRect.width() / 2 + 1 + 5 /** @todo justify! */,
                                 contentRect.height() / 2 + fm.ascent() / 2 - 1 /* base line */);

    /* Paint text: */
    painter.save();
    painter.setFont(fnt);
    painter.setPen(foreground1);
    painter.drawText(point1, strName1);
    painter.setPen(foreground2);
    painter.drawText(point2, strName2);
    painter.restore();
}


/*********************************************************************************************************************************
*   Class UIFilterEditor implementation.                                                                                         *
*********************************************************************************************************************************/

UIFilterEditor::UIFilterEditor(QWidget *pParent)
    : QWidget(pParent)
    , m_pLineEdit(0)
    , m_pToolButton(0)
    , m_fFocused(false)
    , m_iUnfocusedEditorWidth(0)
    , m_iFocusedEditorWidth(0)
    , m_pAnimation(0)
{
    prepare();
}

UIFilterEditor::~UIFilterEditor()
{
    cleanup();
}

void UIFilterEditor::setPlaceholderText(const QString &strText)
{
    if (m_pLineEdit)
        m_pLineEdit->setPlaceholderText(strText);
}

QString UIFilterEditor::text() const
{
    return m_pLineEdit ? m_pLineEdit->text() : QString();
}

QSize UIFilterEditor::minimumSizeHint() const
{
    return m_pLineEdit ? m_pLineEdit->minimumSizeHint() : QWidget::minimumSizeHint();
}

void UIFilterEditor::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QWidget::resizeEvent(pEvent);

    /* Adjust filter editor geometry on each parent resize: */
    adjustEditorGeometry();
}

bool UIFilterEditor::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Preprocess events for m_pLineEdit only: */
    if (pObject != m_pLineEdit)
        return QWidget::eventFilter(pObject, pEvent);

    /* Handles various event types: */
    switch (pEvent->type())
    {
        /* Foreard animation on focus-in: */
        case QEvent::FocusIn:
            m_fFocused = true;
            emit sigFocused();
            break;
        /* Backward animation on focus-out: */
        case QEvent::FocusOut:
            m_fFocused = false;
            emit sigUnfocused();
            break;
        default:
            break;
    }

    /* Call to base-class: */
    return QWidget::eventFilter(pObject, pEvent);
}

void UIFilterEditor::sltHandleEditorTextChanged(const QString &strText)
{
    m_pToolButton->setHidden(m_pLineEdit->text().isEmpty());
    emit sigTextChanged(strText);
}

void UIFilterEditor::sltHandleButtonClicked()
{
    m_pLineEdit->clear();
}

void UIFilterEditor::prepare()
{
    /* Prepare filter editor: */
    m_pLineEdit = new QILineEdit(this);
    if (m_pLineEdit)
    {
        m_pLineEdit->installEventFilter(this);
        connect(m_pLineEdit, &QILineEdit::textChanged,
                this, &UIFilterEditor::sltHandleEditorTextChanged);
    }

    /* Prepare filter reset button: */
    m_pToolButton = new QToolButton(this);
    if (m_pToolButton)
    {
#ifdef VBOX_WS_MAC
        setStyleSheet("QToolButton { border: 0px none black; margin: 0px 0px 0px 0px; } QToolButton::menu-indicator {image: none;}");
#endif
        m_pToolButton->hide();
        m_pToolButton->setIconSize(QSize(10, 10));
        m_pToolButton->setIcon(UIIconPool::iconSet(":/close_16px.png"));
        connect(m_pToolButton, &QToolButton::clicked,
                this, &UIFilterEditor::sltHandleButtonClicked);
    }

    /* Install 'unfocus/focus' animation to 'editorWidth' property: */
    m_pAnimation = UIAnimation::installPropertyAnimation(this,
                                                         "editorWidth",
                                                         "unfocusedEditorWidth", "focusedEditorWidth",
                                                         SIGNAL(sigFocused()), SIGNAL(sigUnfocused()));

    /* Adjust filter editor geometry initially: */
    adjustEditorGeometry();
}

void UIFilterEditor::cleanup()
{
    /* Cleanup 'unfocus/focus' animation: */
    delete m_pAnimation;
    m_pAnimation = 0;
}

void UIFilterEditor::adjustEditorGeometry()
{
    /* Acquire maximum widget width: */
    const int iWidth = width();

    /* Update filter editor geometry: */
    const QSize esh = m_pLineEdit->minimumSizeHint();
    const int iMinimumEditorWidth = esh.width();
    const int iMinimumEditorHeight = esh.height();
    /* Update minimum/maximum filter editor width: */
    m_iUnfocusedEditorWidth = qMax(iWidth / 2, iMinimumEditorWidth);
    m_iFocusedEditorWidth = qMax(iWidth, iMinimumEditorWidth);
    m_pAnimation->update();
    setEditorWidth(m_fFocused ? m_iFocusedEditorWidth : m_iUnfocusedEditorWidth);

    /* Update filter button geometry: */
    const QSize bsh = m_pToolButton->minimumSizeHint();
    const int iMinimumButtonWidth = bsh.width();
    const int iMinimumButtonHeight = bsh.height();
    const int iButtonY = iMinimumEditorHeight > iMinimumButtonHeight
                       ? (iMinimumEditorHeight - iMinimumButtonHeight) / 2
                       : 0;
    m_pToolButton->setGeometry(iWidth - iMinimumButtonWidth - 1, iButtonY, iMinimumButtonWidth, iMinimumButtonHeight);
}

void UIFilterEditor::setEditorWidth(int iWidth)
{
    /* Align filter editor right: */
    const int iX = width() - iWidth;
    const int iY = 0;
    const int iHeight = m_pLineEdit->minimumSizeHint().height();
    m_pLineEdit->setGeometry(iX, iY, iWidth, iHeight);
}

int UIFilterEditor::editorWidth() const
{
    return m_pLineEdit->width();
}


/*********************************************************************************************************************************
*   Class UIVerticalScrollArea implementation.                                                                                   *
*********************************************************************************************************************************/

UIVerticalScrollArea::UIVerticalScrollArea(QWidget *pParent)
    : QScrollArea(pParent)
    , m_pAnimation(0)
{
    prepare();
}

int UIVerticalScrollArea::verticalScrollBarPosition() const
{
    return verticalScrollBar()->value();
}

void UIVerticalScrollArea::setVerticalScrollBarPosition(int iPosition) const
{
    verticalScrollBar()->setValue(iPosition);
}

void UIVerticalScrollArea::requestVerticalScrollBarPosition(int iPosition)
{
    /* Acquire scroll-bar minumum, maximum and length: */
    const int iScrollBarMinimum = verticalScrollBar()->minimum();
    const int iScrollBarMaximum = verticalScrollBar()->maximum();
    const int iScrollBarLength = qAbs(iScrollBarMaximum - iScrollBarMinimum);

    /* Acquire start, final position and total shift:: */
    const int iStartPosition = verticalScrollBarPosition();
    const int iFinalPosition = iPosition;
    int iShift = qAbs(iFinalPosition - iStartPosition);
    /* Make sure iShift is no more than iScrollBarLength: */
    iShift = qMin(iShift, iScrollBarLength);

    /* Calculate walking ratio: */
    const float dRatio = (double)iShift / iScrollBarLength;
    m_pAnimation->setDuration(dRatio * 500 /* 500ms is the max */);
    m_pAnimation->setStartValue(iStartPosition);
    m_pAnimation->setEndValue(iFinalPosition);
    m_pAnimation->start();
}

QSize UIVerticalScrollArea::minimumSizeHint() const
{
    /* To make horizontal scroll-bar always hidden we'll
     * have to make sure minimum size-hint updated accordingly. */
    const int iMinWidth = viewportSizeHint().width()
                        + verticalScrollBar()->sizeHint().width()
                        + frameWidth() * 2;
    const int iMinHeight = qMax(QScrollArea::minimumSizeHint().height(),
                                (int)(iMinWidth / 1.6));
    return QSize(iMinWidth, iMinHeight);
}

void UIVerticalScrollArea::wheelEvent(QWheelEvent *pEvent)
{
    /* Call to base-class: */
    QScrollArea::wheelEvent(pEvent);

    /* Notify listeners: */
    emit sigWheelEvent();
}

void UIVerticalScrollArea::prepare()
{
    /* Make vertical scroll-bar always hidden: */
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /* Prepare vertical scrollbar animation: */
    m_pAnimation = new QPropertyAnimation(this, "verticalScrollBarPosition", this);
}


/*********************************************************************************************************************************
*   Class UIAdvancedSettingsDialog implementation.                                                                               *
*********************************************************************************************************************************/

UIAdvancedSettingsDialog::UIAdvancedSettingsDialog(QWidget *pParent,
                                                   const QString &strCategory,
                                                   const QString &strControl)
    : QIWithRetranslateUI<QMainWindow>(pParent)
    , m_strCategory(strCategory)
    , m_strControl(strControl)
    , m_pSelector(0)
    , m_enmConfigurationAccessLevel(ConfigurationAccessLevel_Null)
    , m_pSerializeProcess(0)
    , m_fPolished(false)
    , m_fSerializationIsInProgress(false)
    , m_fSerializationClean(false)
    , m_fClosed(false)
    , m_iPageId(MachineSettingsPageType_Invalid)
    , m_pStatusBar(0)
    , m_pProcessBar(0)
    , m_pWarningPane(0)
    , m_fValid(true)
    , m_fSilent(true)
    , m_pScrollingTimer(0)
    , m_pLayoutMain(0)
    , m_pCheckBoxMode(0)
    , m_pEditorFilter(0)
    , m_pScrollArea(0)
    , m_pScrollViewport(0)
    , m_pButtonBox(0)
{
    prepare();
}

UIAdvancedSettingsDialog::~UIAdvancedSettingsDialog()
{
    cleanup();
}

void UIAdvancedSettingsDialog::accept()
{
    /* Save data: */
    save();

    /* Close if there is no ongoing serialization: */
    if (!isSerializationInProgress())
        sltClose();
}

void UIAdvancedSettingsDialog::reject()
{
    /* Close if there is no ongoing serialization: */
    if (!isSerializationInProgress())
        sltClose();
}

void UIAdvancedSettingsDialog::sltCategoryChanged(int cId)
{
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    setWindowTitle(title());
#endif

    /* Cache current page ID for reusing: */
    m_iPageId = cId;

    /* Let's calculate required scroll-bar position: */
    int iPosition = 0;
    /* We'll have to take upper content's margin into account: */
    int iL, iT, iR, iB;
    m_pScrollViewport->layout()->getContentsMargins(&iL, &iT, &iR, &iB);
    iPosition -= iT;
    /* And actual page position according to parent: */
    UISettingsPageFrame *pFrame = m_frames.value(m_iPageId, 0);
    AssertPtr(pFrame);
    if (pFrame)
    {
        const QPoint pnt = pFrame->pos();
        iPosition += pnt.y();
    }
    /* Make sure corresponding page is visible: */
    m_pScrollArea->requestVerticalScrollBarPosition(iPosition);

#ifndef VBOX_WS_MAC
    uiCommon().setHelpKeyword(m_pButtonBox->button(QDialogButtonBox::Help), m_pageHelpKeywords.value(cId));
#endif
}

void UIAdvancedSettingsDialog::sltHandleSerializationStarted()
{
    m_pProcessBar->setValue(0);
    m_pStatusBar->setCurrentWidget(m_pProcessBar);
}

void UIAdvancedSettingsDialog::sltHandleSerializationProgressChange(int iValue)
{
    m_pProcessBar->setValue(iValue);
    if (m_pProcessBar->value() == m_pProcessBar->maximum())
    {
        if (!m_fValid || !m_fSilent)
            m_pStatusBar->setCurrentWidget(m_pWarningPane);
        else
            m_pStatusBar->setCurrentIndex(0);
    }
}

void UIAdvancedSettingsDialog::sltHandleSerializationFinished()
{
    /* Delete serializer if exists: */
    delete m_pSerializeProcess;
    m_pSerializeProcess = 0;

    /* Mark serialization finished: */
    m_fSerializationIsInProgress = false;
}

bool UIAdvancedSettingsDialog::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Ignore other than wheel events in this handler: */
    if (pEvent->type() != QEvent::Wheel)
        return QIWithRetranslateUI<QMainWindow>::eventFilter(pObject, pEvent);

    /* Do not touch wheel events for m_pScrollArea or it's children: */
    if (   pObject == m_pScrollArea
        || pObject->parent() == m_pScrollArea)
    {
        /* Moreover restart 'sticky scrolling timer' during which
         * all the scrolling will be redirected to m_pScrollViewport: */
        m_pScrollingTimer->start();
        return QIWithRetranslateUI<QMainWindow>::eventFilter(pObject, pEvent);
    }

    /* Unconditionally and for good
     * redirect wheel event for widgets of following types to m_pScrollViewport: */
    if (   qobject_cast<QAbstractButton*>(pObject)
        || qobject_cast<QAbstractSpinBox*>(pObject)
        || qobject_cast<QAbstractSpinBox*>(pObject->parent())
        || qobject_cast<QComboBox*>(pObject)
        || qobject_cast<QSlider*>(pObject)
        || qobject_cast<QTabWidget*>(pObject)
        || qobject_cast<QTabWidget*>(pObject->parent()))
    {
        /* Check if redirected event was really handled, otherwise give it back: */
        if (QCoreApplication::sendEvent(m_pScrollViewport, pEvent))
            return true;
    }

    /* While 'sticky scrolling timer' is active
     * redirect wheel event for widgets of following types to m_pScrollViewport: */
    if (   m_pScrollingTimer->isActive()
        && (   qobject_cast<QAbstractScrollArea*>(pObject)
            || qobject_cast<QAbstractScrollArea*>(pObject->parent())))
    {
        /* Check if redirected event was really handled, otherwise give it back: */
        if (QCoreApplication::sendEvent(m_pScrollViewport, pEvent))
            return true;
    }

    /* Call to base-class: */
    return QIWithRetranslateUI<QMainWindow>::eventFilter(pObject, pEvent);
}

void UIAdvancedSettingsDialog::retranslateUi()
{
    /* Translate mode checkbox: */
    m_pCheckBoxMode->setText1(tr("Basic"));
    m_pCheckBoxMode->setText2(tr("Expert"));

    /* Translate filter editor placeholder: */
    if (m_pEditorFilter)
        m_pEditorFilter->setPlaceholderText(tr("Search settings"));

    /* Translate warning-pane stuff: */
    m_pWarningPane->setWarningLabelText(tr("Invalid settings detected"));

    /* Translate page-frames: */
    foreach (int cId, m_frames.keys())
        m_frames.value(cId)->setName(m_pSelector->itemText(cId));

    /* Retranslate all validators: */
    foreach (UISettingsPageValidator *pValidator, findChildren<UISettingsPageValidator*>())
        pValidator->setTitlePrefix(m_pSelector->itemTextByPage(pValidator->page()));
    revalidate();
}

void UIAdvancedSettingsDialog::showEvent(QShowEvent *pEvent)
{
    /* Polish stuff: */
    if (!m_fPolished)
    {
        m_fPolished = true;
        polishEvent();
    }

    /* Call to base-class: */
    QIWithRetranslateUI<QMainWindow>::showEvent(pEvent);
}

void UIAdvancedSettingsDialog::polishEvent()
{
    /* Resize to minimum size: */
    resize(minimumSizeHint());

    /* Choose page/tab finally: */
    choosePageAndTab();

    /* Apply actual experience mode: */
    sltHandleExperienceModeChanged();

    /* Explicit centering according to our parent: */
    gpDesktop->centerWidget(this, parentWidget(), false);
}

void UIAdvancedSettingsDialog::closeEvent(QCloseEvent *pEvent)
{
    /* Ignore event initially: */
    pEvent->ignore();

    /* Use pure QWidget close functionality,
     * QWindow stuff is kind of overkill here.. */
    sltClose();
}

void UIAdvancedSettingsDialog::choosePageAndTab(bool fKeepPreviousByDefault /* = false */)
{
    /* Setup settings window: */
    if (!m_strCategory.isNull())
    {
        m_pSelector->selectByLink(m_strCategory);
        /* Search for a widget with the given name: */
        if (!m_strControl.isNull())
        {
            if (QWidget *pWidget = m_pScrollViewport->findChild<QWidget*>(m_strControl))
            {
                QList<QWidget*> parents;
                QWidget *pParentWidget = pWidget;
                while ((pParentWidget = pParentWidget->parentWidget()) != 0)
                {
                    if (QTabWidget *pTabWidget = qobject_cast<QTabWidget*>(pParentWidget))
                    {
                        // WORKAROUND:
                        // The tab contents widget is two steps down
                        // (QTabWidget -> QStackedWidget -> QWidget).
                        QWidget *pTabPage = parents[parents.count() - 1];
                        if (pTabPage)
                            pTabPage = parents[parents.count() - 2];
                        if (pTabPage)
                            pTabWidget->setCurrentWidget(pTabPage);
                    }
                    parents.append(pParentWidget);
                }
                pWidget->setFocus();
            }
        }
    }
    /* First item as default (if previous is not guarded): */
    else if (!fKeepPreviousByDefault)
        m_pSelector->selectById(1);
}

void UIAdvancedSettingsDialog::loadData(QVariant &data)
{
    /* Mark serialization started: */
    m_fSerializationIsInProgress = true;

    /* Create settings loader: */
    m_pSerializeProcess = new UISettingsSerializer(this, UISettingsSerializer::Load,
                                                   data, m_pSelector->settingPages());
    if (m_pSerializeProcess)
    {
        /* Configure settings loader: */
        connect(m_pSerializeProcess, &UISettingsSerializer::sigNotifyAboutProcessStarted,
                this, &UIAdvancedSettingsDialog::sltHandleSerializationStarted);
        connect(m_pSerializeProcess, &UISettingsSerializer::sigNotifyAboutProcessProgressChanged,
                this, &UIAdvancedSettingsDialog::sltHandleSerializationProgressChange);
        connect(m_pSerializeProcess, &UISettingsSerializer::sigNotifyAboutProcessFinished,
                this, &UIAdvancedSettingsDialog::sltHandleSerializationFinished);

        /* Raise current page priority: */
        m_pSerializeProcess->raisePriorityOfPage(m_pSelector->currentId());

        /* Start settings loader: */
        m_pSerializeProcess->start();

        /* Upload data finally: */
        data = m_pSerializeProcess->data();
    }
}

void UIAdvancedSettingsDialog::saveData(QVariant &data)
{
    /* Mark serialization started: */
    m_fSerializationIsInProgress = true;

    /* Create the 'settings saver': */
    QPointer<UISettingsSerializerProgress> pDlgSerializeProgress =
        new UISettingsSerializerProgress(this, UISettingsSerializer::Save,
                                         data, m_pSelector->settingPages());
    if (pDlgSerializeProgress)
    {
        /* Make the 'settings saver' temporary parent for all sub-dialogs: */
        windowManager().registerNewParent(pDlgSerializeProgress, windowManager().realParentWindow(this));

        /* Execute the 'settings saver': */
        pDlgSerializeProgress->exec();

        /* Any modal dialog can be destroyed in own event-loop
         * as a part of application termination procedure..
         * We have to check if the dialog still valid. */
        if (pDlgSerializeProgress)
        {
            /* Remember whether the serialization was clean: */
            m_fSerializationClean = pDlgSerializeProgress->isClean();

            /* Upload 'settings saver' data: */
            data = pDlgSerializeProgress->data();

            /* Delete the 'settings saver': */
            delete pDlgSerializeProgress;
        }
    }
}

void UIAdvancedSettingsDialog::setConfigurationAccessLevel(ConfigurationAccessLevel enmConfigurationAccessLevel)
{
    /* Make sure something changed: */
    if (m_enmConfigurationAccessLevel == enmConfigurationAccessLevel)
        return;

    /* Apply new configuration access level: */
    m_enmConfigurationAccessLevel = enmConfigurationAccessLevel;

    /* And propagate it to settings-page(s): */
    foreach (UISettingsPage *pPage, m_pSelector->settingPages())
        pPage->setConfigurationAccessLevel(configurationAccessLevel());
}

void UIAdvancedSettingsDialog::setOptionalFlags(const QMap<QString, QVariant> &flags)
{
    /* Avoid excessive calls: */
    if (m_flags == flags)
        return;

    /* Save new flags: */
    m_flags = flags;

    /* Reapply optional flags: */
    sltApplyFilteringRules();
}

void UIAdvancedSettingsDialog::addItem(const QString &strBigIcon,
                                       const QString &strMediumIcon,
                                       const QString &strSmallIcon,
                                       int cId,
                                       const QString &strLink,
                                       UISettingsPage *pSettingsPage /* = 0 */,
                                       int iParentId /* = -1 */)
{
    /* Init m_iPageId if we haven't yet: */
    if (m_iPageId == MachineSettingsPageType_Invalid)
        m_iPageId = cId;

    /* Add new selector item: */
    if (m_pSelector->addItem(strBigIcon, strMediumIcon, strSmallIcon,
                             cId, strLink, pSettingsPage, iParentId))
    {
        /* Create frame with page inside: */
        UISettingsPageFrame *pFrame = new UISettingsPageFrame(pSettingsPage, m_pScrollViewport);
        if (pFrame)
        {
            /* Add frame to scroll-viewport: */
            m_pScrollViewport->layout()->addWidget(pFrame);

            /* Remember page-frame for referencing: */
            m_frames[cId] = pFrame;

            /* Notify about frame visibility changes: */
            connect(pFrame, &UISettingsPageFrame::sigVisibilityChange,
                    this, &UIAdvancedSettingsDialog::sltHandleFrameVisibilityChange);
        }
    }

    /* Assign validator if necessary: */
    if (pSettingsPage)
    {
        pSettingsPage->setId(cId);

        /* Create validator: */
        UISettingsPageValidator *pValidator = new UISettingsPageValidator(this, pSettingsPage);
        connect(pValidator, &UISettingsPageValidator::sigValidityChanged,
                this, &UIAdvancedSettingsDialog::sltHandleValidityChange);
        pSettingsPage->setValidator(pValidator);
        m_pWarningPane->registerValidator(pValidator);

        /* Update navigation (tab-order): */
        pSettingsPage->setOrderAfter(m_pSelector->widget());
    }
}

void UIAdvancedSettingsDialog::addPageHelpKeyword(int iPageType, const QString &strHelpKeyword)
{
    m_pageHelpKeywords[iPageType] = strHelpKeyword;
}

void UIAdvancedSettingsDialog::revalidate()
{
    /* Perform dialog revalidation: */
    m_fValid = true;
    m_fSilent = true;

    /* Enumerating all the validators we have: */
    foreach (UISettingsPageValidator *pValidator, findChildren<UISettingsPageValidator*>())
    {
        /* Is current validator have something to say? */
        if (!pValidator->lastMessage().isEmpty())
        {
            /* What page is it related to? */
            UISettingsPage *pFailedSettingsPage = pValidator->page();
            LogRelFlow(("Settings Dialog:  Dialog validation FAILED: Page *%s*\n",
                        pFailedSettingsPage->internalName().toUtf8().constData()));

            /* Show error first: */
            if (!pValidator->isValid())
                m_fValid = false;
            /* Show warning if message is not an error: */
            else
                m_fSilent = false;

            /* Stop dialog revalidation on first error/warning: */
            break;
        }
    }

    /* Update warning-pane visibility: */
    m_pWarningPane->setWarningLabelVisible(!m_fValid || !m_fSilent);

    /* Make sure warning-pane visible if necessary: */
    if ((!m_fValid || !m_fSilent) && m_pStatusBar->currentIndex() == 0)
        m_pStatusBar->setCurrentWidget(m_pWarningPane);
    /* Make sure empty-pane visible otherwise: */
    else if (m_fValid && m_fSilent && m_pStatusBar->currentWidget() == m_pWarningPane)
        m_pStatusBar->setCurrentIndex(0);

    /* Lock/unlock settings-page OK button according global validity status: */
    m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(m_fValid);
}

bool UIAdvancedSettingsDialog::isSettingsChanged()
{
    bool fIsSettingsChanged = false;
    foreach (UISettingsPage *pPage, m_pSelector->settingPages())
    {
        pPage->putToCache();
        if (!fIsSettingsChanged && pPage->changed())
            fIsSettingsChanged = true;
    }
    return fIsSettingsChanged;
}

void UIAdvancedSettingsDialog::sltClose()
{
    /* Check whether serialization was clean (save)
     * or there are no unsaved settings to be lost (cancel): */
    if (   m_fSerializationClean
        || !isSettingsChanged()
        || msgCenter().confirmSettingsDiscarding(this))
    {
        /* Tell the listener to close us (once): */
        if (!m_fClosed)
        {
            m_fClosed = true;
            emit sigClose();
            return;
        }
    }
}

void UIAdvancedSettingsDialog::sltHandleValidityChange(UISettingsPageValidator *pValidator)
{
    /* Determine which settings-page had called for revalidation: */
    if (UISettingsPage *pSettingsPage = pValidator->page())
    {
        /* Determine settings-page name: */
        const QString strPageName(pSettingsPage->internalName());

        LogRelFlow(("Settings Dialog: %s Page: Revalidation in progress..\n",
                    strPageName.toUtf8().constData()));

        /* Perform page revalidation: */
        pValidator->revalidate();
        /* Perform inter-page recorrelation: */
        recorrelate(pSettingsPage);
        /* Perform dialog revalidation: */
        revalidate();

        LogRelFlow(("Settings Dialog: %s Page: Revalidation complete.\n",
                    strPageName.toUtf8().constData()));
    }
}

void UIAdvancedSettingsDialog::sltHandleWarningPaneHovered(UISettingsPageValidator *pValidator)
{
    LogRelFlow(("Settings Dialog: Warning-icon hovered: %s.\n", pValidator->internalName().toUtf8().constData()));

    /* Show corresponding popup: */
    if (!m_fValid || !m_fSilent)
        popupCenter().popup(m_pScrollArea, "SettingsDialogWarning",
                            pValidator->lastMessage());
}

void UIAdvancedSettingsDialog::sltHandleWarningPaneUnhovered(UISettingsPageValidator *pValidator)
{
    LogRelFlow(("Settings Dialog: Warning-icon unhovered: %s.\n", pValidator->internalName().toUtf8().constData()));

    /* Recall corresponding popup: */
    popupCenter().recall(m_pScrollArea, "SettingsDialogWarning");
}

void UIAdvancedSettingsDialog::sltHandleExperienceModeCheckBoxChanged()
{
    /* Save new value: */
    gEDataManager->setSettingsInExpertMode(m_pCheckBoxMode->isChecked());
}

void UIAdvancedSettingsDialog::sltHandleExperienceModeChanged()
{
    /* Acquire actual value: */
    const bool fExpertMode = gEDataManager->isSettingsInExpertMode();

    /* Update check-box state: */
    m_pCheckBoxMode->blockSignals(true);
    m_pCheckBoxMode->setChecked(fExpertMode);
    m_pCheckBoxMode->blockSignals(false);

    /* Reapply mode: */
    sltApplyFilteringRules();
}

void UIAdvancedSettingsDialog::sltApplyFilteringRules()
{
    /* Filter-out page contents: */
    foreach (UISettingsPageFrame *pFrame, m_frames.values())
        pFrame->filterOut(m_pCheckBoxMode->isChecked(),
                          m_pEditorFilter->text(),
                          m_flags);

    /* Make sure current page chosen again: */
    /// @todo fix this WORKAROUND properly!
    // Why the heck simple call to
    // QCoreApplication::sendPostedEvents(0, QEvent::LayoutRequest);
    // isn't enough and we still need some time to let system
    // process the layouts and vertical scroll-bar position?
    QTimer::singleShot(50, this, SLOT(sltCategoryChangedRepeat()));
}

void UIAdvancedSettingsDialog::sltHandleFrameVisibilityChange(bool fVisible)
{
    /* Acquire frame: */
    UISettingsPageFrame *pFrame = qobject_cast<UISettingsPageFrame*>(sender());
    AssertPtrReturnVoid(pFrame);

    /* Update selector item visibility: */
    const int iId = m_frames.key(pFrame);
    m_pSelector->setItemVisible(iId, fVisible);
}

void UIAdvancedSettingsDialog::sltHandleVerticalScrollAreaWheelEvent()
{
    /* Acquire layout info: */
    int iL = 0, iT = 0, iR = 0, iB = 0;
    if (   m_pScrollViewport
        && m_pScrollViewport->layout())
        m_pScrollViewport->layout()->getContentsMargins(&iL, &iT, &iR, &iB);

    /* Search through all the frame keys we have: */
    int iActualKey = -1;
    foreach (int iKey, m_frames.keys())
    {
        /* Let's calculate scroll-bar position for enumerated frame: */
        int iPosition = 0;
        /* We'll have to take upper content's margin into account: */
        iPosition -= iT;
        /* And actual page position according to parent: */
        const QPoint pnt = m_frames.value(iKey)->pos();
        iPosition += pnt.y();

        /* Check if scroll-bar haven't passed this position yet: */
        if (m_pScrollArea->verticalScrollBarPosition() < iPosition)
            break;

        /* Remember last suitable frame key: */
        iActualKey = iKey;
    }

    /* Silently update the selector with frame number we found: */
    if (iActualKey != -1)
        m_pSelector->selectById(iActualKey, true /* silently */);
}

void UIAdvancedSettingsDialog::prepare()
{
    /* Prepare 'sticky scrolling timer': */
    m_pScrollingTimer = new QTimer(this);
    if (m_pScrollingTimer)
    {
        m_pScrollingTimer->setInterval(500);
        m_pScrollingTimer->setSingleShot(true);
    }

    /* Prepare central-widget: */
    setCentralWidget(new QWidget);
    if (centralWidget())
    {
        /* Prepare main layout: */
        m_pLayoutMain = new QGridLayout(centralWidget());
        if (m_pLayoutMain)
        {
            /* Prepare widgets: */
            prepareSelector();
            prepareScrollArea();
            prepareButtonBox();
        }
    }

    /* Apply language settings: */
    retranslateUi();
}

void UIAdvancedSettingsDialog::prepareSelector()
{
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    /* Prepare modern tool-bar selector: */
    m_pSelector = new UISettingsSelectorToolBar(this);
    if (m_pSelector)
    {
        static_cast<QIToolBar*>(m_pSelector->widget())->enableMacToolbar();
        addToolBar(qobject_cast<QToolBar*>(m_pSelector->widget()));
    }

    /* No title in this mode, we change the title of the window: */
    m_pLayoutMain->setColumnMinimumWidth(0, 0);
    m_pLayoutMain->setHorizontalSpacing(0);

#else /* !VBOX_GUI_WITH_TOOLBAR_SETTINGS */

    /* Make sure there is a serious spacing between selector and pages: */
    m_pLayoutMain->setColumnMinimumWidth(1, 20);
    m_pLayoutMain->setRowStretch(1, 1);
    m_pLayoutMain->setColumnStretch(2, 1);

    /* Prepare mode checkbox: */
    m_pCheckBoxMode = new UIModeCheckBox(centralWidget());
    if (m_pCheckBoxMode)
    {
        connect(m_pCheckBoxMode, &UIModeCheckBox::stateChanged,
                this, &UIAdvancedSettingsDialog::sltHandleExperienceModeCheckBoxChanged);
        connect(gEDataManager, &UIExtraDataManager::sigSettingsExpertModeChange,
                this, &UIAdvancedSettingsDialog::sltHandleExperienceModeChanged);
        m_pLayoutMain->addWidget(m_pCheckBoxMode, 0, 0);
    }

    /* Prepare classical tree-view selector: */
    m_pSelector = new UISettingsSelectorTreeView(centralWidget());
    if (m_pSelector)
    {
        m_pLayoutMain->addWidget(m_pSelector->widget(), 1, 0);
        m_pSelector->widget()->setFocus();
    }

    /* Prepare filter editor: */
    m_pEditorFilter = new UIFilterEditor(centralWidget());
    if (m_pEditorFilter)
    {
        connect(m_pEditorFilter, &UIFilterEditor::sigTextChanged,
                this, &UIAdvancedSettingsDialog::sltApplyFilteringRules);
        m_pLayoutMain->addWidget(m_pEditorFilter, 0, 2);
    }
#endif /* !VBOX_GUI_WITH_TOOLBAR_SETTINGS */

    /* Configure selector created above: */
    if (m_pSelector)
        connect(m_pSelector, &UISettingsSelectorTreeWidget::sigCategoryChanged,
                this, &UIAdvancedSettingsDialog::sltCategoryChanged);
}

void UIAdvancedSettingsDialog::prepareScrollArea()
{
    /* Prepare scroll-area: */
    m_pScrollArea = new UIVerticalScrollArea(centralWidget());
    if (m_pScrollArea)
    {
        /* Configure popup-stack: */
        popupCenter().setPopupStackOrientation(m_pScrollArea, UIPopupStackOrientation_Bottom);

        m_pScrollArea->setWidgetResizable(true);
        m_pScrollArea->setFrameShape(QFrame::NoFrame);
        connect(m_pScrollArea, &UIVerticalScrollArea::sigWheelEvent,
                this, &UIAdvancedSettingsDialog::sltHandleVerticalScrollAreaWheelEvent);

        /* Prepare scroll-viewport: */
        m_pScrollViewport = new QWidget(m_pScrollArea);
        if (m_pScrollViewport)
        {
            QVBoxLayout *pLayout = new QVBoxLayout(m_pScrollViewport);
            if (pLayout)
            {
                pLayout->setAlignment(Qt::AlignTop);
                pLayout->setContentsMargins(0, 0, 0, 0);
                int iSpacing = pLayout->spacing();
                pLayout->setSpacing(2 * iSpacing);
            }
            m_pScrollArea->setWidget(m_pScrollViewport);
        }

        /* Add scroll-area into main layout: */
        m_pLayoutMain->addWidget(m_pScrollArea, 1, 2);
    }
}

void UIAdvancedSettingsDialog::prepareButtonBox()
{
    /* Prepare button-box: */
    m_pButtonBox = new QIDialogButtonBox(centralWidget());
    if (m_pButtonBox)
    {
#ifndef VBOX_WS_MAC
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                         QDialogButtonBox::NoButton | QDialogButtonBox::Help);
        m_pButtonBox->button(QDialogButtonBox::Help)->setShortcut(QKeySequence::HelpContents);
#else
        // WORKAROUND:
        // No Help button on macOS for now, conflict with old Qt.
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                         QDialogButtonBox::NoButton);
#endif
        m_pButtonBox->button(QDialogButtonBox::Ok)->setShortcut(Qt::Key_Return);
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(Qt::Key_Escape);
        connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UIAdvancedSettingsDialog::sltClose);
        connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UIAdvancedSettingsDialog::accept);
#ifndef VBOX_WS_MAC
        connect(m_pButtonBox->button(QDialogButtonBox::Help), &QAbstractButton::pressed,
                m_pButtonBox, &QIDialogButtonBox::sltHandleHelpRequest);
#endif

        /* Prepare status-bar: */
        m_pStatusBar = new QStackedWidget(m_pButtonBox);
        if (m_pStatusBar)
        {
            /* Add empty widget: */
            m_pStatusBar->addWidget(new QWidget);

            /* Prepare process-bar: */
            m_pProcessBar = new QProgressBar(m_pStatusBar);
            if (m_pProcessBar)
            {
                m_pProcessBar->setMinimum(0);
                m_pProcessBar->setMaximum(100);
                m_pStatusBar->addWidget(m_pProcessBar);
            }

            /* Prepare warning-pane: */
            m_pWarningPane = new UISettingsWarningPane(m_pStatusBar);
            if (m_pWarningPane)
            {
                connect(m_pWarningPane, &UISettingsWarningPane::sigHoverEnter,
                        this, &UIAdvancedSettingsDialog::sltHandleWarningPaneHovered);
                connect(m_pWarningPane, &UISettingsWarningPane::sigHoverLeave,
                        this, &UIAdvancedSettingsDialog::sltHandleWarningPaneUnhovered);
                m_pStatusBar->addWidget(m_pWarningPane);
            }

            /* Add status-bar to button-box: */
            m_pButtonBox->addExtraWidget(m_pStatusBar);
        }

        /* Add button-box into main layout: */
        m_pLayoutMain->addWidget(m_pButtonBox, 2, 0, 1, 3);
    }
}

void UIAdvancedSettingsDialog::cleanup()
{
    /* Delete serializer if exists: */
    delete m_pSerializeProcess;
    m_pSerializeProcess = 0;

    /* Recall popup-pane if any: */
    popupCenter().recall(m_pScrollArea, "SettingsDialogWarning");

    /* Delete selector early! */
    delete m_pSelector;
}

#include "UIAdvancedSettingsDialog.moc"
