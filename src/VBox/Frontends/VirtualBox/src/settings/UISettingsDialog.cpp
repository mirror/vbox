/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISettingsDialog class implementation
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

/* Global includes */
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QCloseEvent>

/* Local includes */
#include "UISettingsDialog.h"
#include "VBoxWarningPane.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "QIWidgetValidator.h"
#include "VBoxSettingsSelector.h"
#include "UISettingsPage.h"
#include "UIToolBar.h"
#include "UIIconPool.h"
#include "UIConverter.h"
#ifdef Q_WS_MAC
# include "VBoxUtils.h"
# if MAC_LEOPARD_STYLE
#  define VBOX_GUI_WITH_TOOLBAR_SETTINGS
# endif /* MAC_LEOPARD_STYLE */
#endif /* Q_WS_MAC */

/* Settings Dialog Constructor: */
UISettingsDialog::UISettingsDialog(QWidget *pParent)
    /* Parent class: */
    : QIWithRetranslateUI<QIMainDialog>(pParent)
    /* Protected variables: */
    , m_pSelector(0)
    , m_pStack(0)
    /* Common variables: */
    , m_dialogType(SettingsDialogType_Wrong)
    , m_fPolished(false)
    /* Loading/saving stuff: */
    , m_fLoaded(false)
    , m_fSaved(false)
    /* Status bar stuff: */
    , m_pStatusBar(new QStackedWidget(this))
    /* Process bar stuff: */
    , m_pProcessBar(new QProgressBar(this))
    /* Error/Warning stuff: */
    , m_fValid(true)
    , m_fSilent(true)
    , m_pWarningPane(new VBoxWarningPane(this))
    /* Whats-this stuff: */
    , m_pWhatsThisTimer(new QTimer(this))
    , m_pWhatsThisCandidate(0)
{
    /* Apply UI decorations: */
    Ui::UISettingsDialog::setupUi(this);

#ifdef Q_WS_MAC
    /* No status bar on the mac: */
    setSizeGripEnabled(false);
    setStatusBar(0);
#endif /* Q_WS_MAC */

    /* Page-title font is derived from the system font: */
    QFont pageTitleFont = font();
    pageTitleFont.setBold(true);
    pageTitleFont.setPointSize(pageTitleFont.pointSize() + 2);
    m_pLbTitle->setFont(pageTitleFont);

    /* Get main grid layout: */
    QGridLayout *pMainLayout = static_cast<QGridLayout*>(centralWidget()->layout());
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    /* No page-title with tool-bar: */
    m_pLbTitle->hide();
    /* No whats-this with tool-bar: */
    m_pLbWhatsThis->hide();
    /* Create modern tool-bar selector: */
    m_pSelector = new VBoxSettingsToolBarSelector(this);
    static_cast<UIToolBar*>(m_pSelector->widget())->setMacToolbar();
    addToolBar(qobject_cast<QToolBar*>(m_pSelector->widget()));
    /* No title in this mode, we change the title of the window: */
    pMainLayout->setColumnMinimumWidth(0, 0);
    pMainLayout->setHorizontalSpacing(0);
#else
    /* Create classical tree-view selector: */
    m_pSelector = new VBoxSettingsTreeViewSelector(this);
    pMainLayout->addWidget(m_pSelector->widget(), 0, 0, 3, 1);
    m_pSelector->widget()->setFocus();
    pMainLayout->setSpacing(10);
#endif /* VBOX_GUI_WITH_TOOLBAR_SETTINGS */

    /* Creating stack of pages: */
    m_pStack = new QStackedWidget(m_pWtStackHandler);
    QVBoxLayout *pStackLayout = new QVBoxLayout(m_pWtStackHandler);
    pStackLayout->setContentsMargins(0, 0, 0, 0);
    pStackLayout->addWidget(m_pStack);

    /* Status bar: */
    m_pStatusBar->addWidget(new QWidget);
    m_pButtonBox->addExtraWidget(m_pStatusBar);

    /* Setup process bar stuff: */
    m_pStatusBar->addWidget(m_pProcessBar);

    /* Setup error & warning stuff: */
    m_pStatusBar->addWidget(m_pWarningPane);
    m_errorIcon = UIIconPool::defaultIcon(UIIconPool::MessageBoxCriticalIcon, this).pixmap(16, 16);
    m_warningIcon = UIIconPool::defaultIcon(UIIconPool::MessageBoxWarningIcon, this).pixmap(16, 16);

    /* Setup whatsthis stuff: */
    qApp->installEventFilter(this);
    m_pWhatsThisTimer->setSingleShot(true);
    connect(m_pWhatsThisTimer, SIGNAL(timeout()), this, SLOT(sltUpdateWhatsThis()));
    m_pLbWhatsThis->setAutoFillBackground(true);
    QPalette whatsThisPalette = m_pLbWhatsThis->palette();
    whatsThisPalette.setBrush(QPalette::Window, whatsThisPalette.brush(QPalette::Midlight));
    m_pLbWhatsThis->setPalette(whatsThisPalette);
    m_pLbWhatsThis->setFixedHeight(m_pLbWhatsThis->frameWidth() * 2 +
                                   m_pLbWhatsThis->margin() * 2 +
                                   m_pLbWhatsThis->fontMetrics().lineSpacing() * 4);

    /* Set the default button: */
    m_pButtonBox->button(QDialogButtonBox::Ok)->setDefault(true);

    /* Setup connections: */
    connect(m_pSelector, SIGNAL(categoryChanged(int)), this, SLOT(sltCategoryChanged(int)));
    connect(m_pButtonBox, SIGNAL(helpRequested()), &msgCenter(), SLOT(sltShowHelpHelpDialog()));

    /* Translate UI: */
    retranslateUi();
}

UISettingsDialog::~UISettingsDialog()
{
    /* Delete selector early! */
    delete m_pSelector;
}

void UISettingsDialog::execute()
{
    /* Load data: */
    loadData();

    /* Execute dialog and wait for completion: */
    if (exec() != QDialog::Accepted)
        return;

    /* Save data: */
    saveData();
}

void UISettingsDialog::sltCategoryChanged(int cId)
{
    int index = m_pages[cId];
#ifdef Q_WS_MAC
    QSize cs = size();
    if (index < m_sizeList.count())
    {
        QSize ss = m_sizeList.at(index);
        /* Switch to the new page first if we are shrinking: */
        if (cs.height() > ss.height())
            m_pStack->setCurrentIndex(index);
        /* Do the animation: */
        ::darwinWindowAnimateResize(this, QRect (x(), y(), ss.width(), ss.height()));
        /* Switch to the new page last if we are zooming: */
        if (cs.height() <= ss.height())
            m_pStack->setCurrentIndex(index);
    }
    ::darwinSetShowsResizeIndicator(this, false);
#else
    m_pLbTitle->setText(m_pSelector->itemText(cId));
    m_pStack->setCurrentIndex(index);
#endif
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    setWindowTitle(title());
#endif /* VBOX_GUI_WITH_TOOLBAR_SETTINGS */
}

void UISettingsDialog::sltMarkLoaded()
{
    m_fLoaded = true;
}

void UISettingsDialog::sltMarkSaved()
{
    m_fSaved = true;
}

void UISettingsDialog::sltHandleProcessStarted()
{
    m_pProcessBar->setValue(0);
    m_pStatusBar->setCurrentWidget(m_pProcessBar);
}

void UISettingsDialog::sltHandlePageProcessed()
{
    m_pProcessBar->setValue(m_pProcessBar->value() + 1);
    if (m_pProcessBar->value() == m_pProcessBar->maximum())
    {
        if (!m_fValid || !m_fSilent)
            m_pStatusBar->setCurrentWidget(m_pWarningPane);
        else
            m_pStatusBar->setCurrentIndex(0);
    }
}

void UISettingsDialog::loadData()
{
    m_fLoaded = false;
}

void UISettingsDialog::saveData()
{
    m_fSaved = false;
}

void UISettingsDialog::retranslateUi()
{
    /* Translate generated stuff: */
    Ui::UISettingsDialog::retranslateUi(this);

    /* Translate error/warning stuff: */
    m_strErrorHint = tr("Invalid settings detected");
    m_strWarningHint = tr("Non-optimal settings detected");
    if (!m_fValid)
        m_pWarningPane->setWarningText(m_strErrorHint);
    else if (!m_fSilent)
        m_pWarningPane->setWarningText(m_strWarningHint);

#ifndef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    /* Retranslate current page headline: */
    m_pLbTitle->setText(m_pSelector->itemText(m_pSelector->currentId()));
#endif /* VBOX_GUI_WITH_TOOLBAR_SETTINGS */

    /* Retranslate all validators: */
    foreach (UIPageValidator *pValidator, findChildren<UIPageValidator*>())
        if (!pValidator->lastMessage().isEmpty())
            revalidate(pValidator);
    revalidate();
}

void UISettingsDialog::setDialogType(SettingsDialogType settingsDialogType)
{
    /* Remember new dialog-type: */
    m_dialogType = settingsDialogType;

    /* Propagate it to settings-page(s): */
    foreach (UISettingsPage *pPage, m_pSelector->settingPages())
        pPage->setDialogType(dialogType());
}

QString UISettingsDialog::titleExtension() const
{
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    return m_pSelector->itemText(m_pSelector->currentId());
#else
    return tr("Settings");
#endif
}

void UISettingsDialog::setError(const QString &strError)
{
    m_strErrorString = strError.isEmpty() ? QString() :
                       QString("<font color=red>%1</font>").arg(strError);

    /* Do not touching QILabel until dialog is polished
     * otherwise it can change its size to undefined: */
    if (m_fPolished)
    {
        if (!m_strErrorString.isEmpty() && m_pStatusBar->currentWidget() == m_pWarningPane)
            m_pLbWhatsThis->setText(m_strErrorString);
        else
            sltUpdateWhatsThis(true /* got focus? */);
    }
}

void UISettingsDialog::setWarning(const QString &strWarning)
{
    m_strWarningString = strWarning.isEmpty() ? QString() :
                         QString("<font color=#ff5400>%1</font>").arg(strWarning);

    /* Do not touching QILabel until dialog is polished
     * otherwise it can change its size to undefined: */
    if (m_fPolished)
    {
        if (!m_strWarningString.isEmpty() && m_pStatusBar->currentWidget() == m_pWarningPane)
            m_pLbWhatsThis->setText(m_strWarningString);
        else
            sltUpdateWhatsThis(true /* got focus? */);
    }
}

void UISettingsDialog::addItem(const QString &strBigIcon,
                               const QString &strBigIconDisabled,
                               const QString &strSmallIcon,
                               const QString &strSmallIconDisabled,
                               int cId,
                               const QString &strLink,
                               UISettingsPage *pSettingsPage /* = 0 */,
                               int iParentId /* = -1 */)
{
    /* Add new selector item: */
    if (QWidget *pPage = m_pSelector->addItem(strBigIcon, strBigIconDisabled,
                                              strSmallIcon, strSmallIconDisabled,
                                              cId, strLink, pSettingsPage, iParentId))
    {
        /* Add stack-widget page if created: */
        m_pages[cId] = m_pStack->addWidget(pPage);
        /* Update process-bar: */
        m_pProcessBar->setMinimum(0);
        m_pProcessBar->setMaximum(m_pStack->count());
    }
    /* Assign validator if necessary: */
    if (pSettingsPage)
        assignValidator(pSettingsPage);
}

void UISettingsDialog::revalidate(UIPageValidator *pValidator)
{
    /* Perform page revalidation: */
    UISettingsPage *pSettingsPage = pValidator->page();
    QString strPageTitle = m_pSelector->itemTextByPage(pSettingsPage);
    QString strMessageText;
    bool fIsValid = pSettingsPage->validate(strMessageText, strPageTitle);

    /* Remember revalidation result: */
    pValidator->setValid(fIsValid);

    /* Remember warning/error message: */
    if (strMessageText.isEmpty())
        pValidator->setLastMessage(QString());
    else
    {
        pValidator->setLastMessage(tr("On the <b>%1</b> page, %2").arg(strPageTitle, strMessageText));
        LogRel(("Settings Dialog:  Page validation FAILED: {%s}\n",
                pValidator->lastMessage().toUtf8().constData()));
    }
}

void UISettingsDialog::revalidate()
{
    /* Perform dialog revalidation: */
    m_fValid = true;
    m_fSilent = true;
    setError(QString());
    setWarning(QString());

    /* Enumerating all the validators we have: */
    QList<UIPageValidator*> validators(findChildren<UIPageValidator*>());
    foreach (UIPageValidator *pValidator, validators)
    {
        /* Is current validator have something to say? */
        if (!pValidator->lastMessage().isEmpty())
        {
            /* What page is it related to? */
            UISettingsPage *pFailedSettingsPage = pValidator->page();
            LogRel(("Settings Dialog:  Dialog validation FAILED: Page *%s*\n",
                    gpConverter->toInternalString((MachineSettingsPageType)pFailedSettingsPage->id()).toUtf8().constData()));

            /* Show error first: */
            if (!pValidator->isValid())
            {
                m_fValid = false;
                setError(pValidator->lastMessage());
                m_pWarningPane->setWarningPixmap(m_errorIcon);
                m_pWarningPane->setWarningText(m_strErrorHint);
#ifdef Q_WS_MAC
                m_pWarningPane->setToolTip(m_strErrorString);
#endif /* Q_WS_MAC */
            }
            /* Show warning if message is not an error: */
            else
            {
                m_fSilent = false;
                setWarning(pValidator->lastMessage());
                m_pWarningPane->setWarningPixmap(m_warningIcon);
                m_pWarningPane->setWarningText(m_strWarningHint);
#ifdef Q_WS_MAC
                m_pWarningPane->setToolTip(m_strWarningString);
#endif /* Q_WS_MAC */
            }

            /* Stop dialog revalidation on first error/warning: */
            if (!m_fValid || !m_fSilent)
                break;
        }
    }

    /* Make sure message-pane visible if necessary: */
    if ((!m_fValid || !m_fSilent) && m_pStatusBar->currentIndex() == 0)
        m_pStatusBar->setCurrentWidget(m_pWarningPane);
    /* Make sure whats-this-pane visible otherwise: */
    else if (m_fValid && m_fSilent && m_pStatusBar->currentWidget() == m_pWarningPane)
        m_pStatusBar->setCurrentIndex(0);

    /* Lock/unlock settings-page OK button according global validity status: */
    m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(m_fValid);
}

void UISettingsDialog::sltHandleValidityChange(UIPageValidator *pValidator)
{
    /* Determine which settings-page had called for revalidation: */
    if (UISettingsPage *pSettingsPage = pValidator->page())
    {
        /* Determine settings-page name: */
        const QString strPageName(gpConverter->toInternalString((MachineSettingsPageType)pSettingsPage->id()));

        LogRel(("Settings Dialog: %s Page: Revalidation in progress..\n",
                strPageName.toUtf8().constData()));

        /* Perform page revalidation: */
        revalidate(pValidator);
        /* Perform inter-page recorrelation: */
        recorrelate(pSettingsPage);
        /* Perform dialog revalidation: */
        revalidate();

        LogRel(("Settings Dialog: %s Page: Revalidation complete.\n",
                strPageName.toUtf8().constData()));
    }
}

void UISettingsDialog::sltUpdateWhatsThis(bool fGotFocus /* = false */)
{
    QString strWhatsThisText;
    QWidget *pWhatsThisWidget = 0;

    /* If focus had NOT changed: */
    if (!fGotFocus)
    {
        /* We will use the recommended candidate: */
        if (m_pWhatsThisCandidate && m_pWhatsThisCandidate != this)
            pWhatsThisWidget = m_pWhatsThisCandidate;
    }
    /* If focus had changed: */
    else
    {
        /* We will use the focused widget instead: */
        pWhatsThisWidget = QApplication::focusWidget();
    }

    /* If the given widget lacks the whats-this text, look at its parent: */
    while (pWhatsThisWidget && pWhatsThisWidget != this)
    {
        strWhatsThisText = pWhatsThisWidget->whatsThis();
        if (!strWhatsThisText.isEmpty())
            break;
        pWhatsThisWidget = pWhatsThisWidget->parentWidget();
    }

#ifndef Q_WS_MAC
    if (m_pStatusBar->currentWidget() == m_pWarningPane)
    {
        if (strWhatsThisText.isEmpty() && !m_strErrorString.isEmpty())
            strWhatsThisText = m_strErrorString;
        else if (strWhatsThisText.isEmpty() && !m_strWarningString.isEmpty())
            strWhatsThisText = m_strWarningString;
    }
    if (strWhatsThisText.isEmpty())
        strWhatsThisText = whatsThis();
    m_pLbWhatsThis->setText(strWhatsThisText);
#else
    if (pWhatsThisWidget && !strWhatsThisText.isEmpty())
        pWhatsThisWidget->setToolTip(QString("<qt>%1</qt>").arg(strWhatsThisText));
#endif
}

void UISettingsDialog::reject()
{
    if (m_fLoaded)
        QIMainDialog::reject();
}

bool UISettingsDialog::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Ignore objects which are NOT widgets: */
    if (!pObject->isWidgetType())
        return QIMainDialog::eventFilter(pObject, pEvent);

    /* Ignore widgets which window is NOT settings dialog: */
    QWidget *pWidget = static_cast<QWidget*>(pObject);
    if (pWidget->window() != this)
        return QIMainDialog::eventFilter(pObject, pEvent);

    /* Process different event-types: */
    switch (pEvent->type())
    {
        /* Process enter/leave events to remember whats-this candidates: */
        case QEvent::Enter:
        case QEvent::Leave:
        {
            if (pEvent->type() == QEvent::Enter)
                m_pWhatsThisCandidate = pWidget;
            else
                m_pWhatsThisCandidate = 0;

            m_pWhatsThisTimer->start(100);
            break;
        }
        /* Process focus-in event to update whats-this pane: */
        case QEvent::FocusIn:
        {
            sltUpdateWhatsThis(true /* got focus? */);
            break;
        }
        default:
            break;
    }

    /* Base-class processing: */
    return QIMainDialog::eventFilter(pObject, pEvent);
}

void UISettingsDialog::showEvent(QShowEvent *pEvent)
{
    /* Base-class processing: */
    QIMainDialog::showEvent(pEvent);

    /* One may think that QWidget::polish() is the right place to do things
     * below, but apparently, by the time when QWidget::polish() is called,
     * the widget style & layout are not fully done, at least the minimum
     * size hint is not properly calculated. Since this is sometimes necessary,
     * we provide our own "polish" implementation. */
    if (m_fPolished)
        return;

    m_fPolished = true;

    int iMinWidth = m_pSelector->minWidth();
#ifdef Q_WS_MAC
    /* Remove all title bar buttons (Buggy Qt): */
    ::darwinSetHidesAllTitleButtons(this);

    /* Set all size policies to ignored: */
    for (int i = 0; i < m_pStack->count(); ++i)
        m_pStack->widget(i)->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    /* Activate every single page to get the optimal size: */
    for (int i = m_pStack->count() - 1; i >= 0; --i)
    {
        m_pStack->widget(i)->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        /* Prevent this widgets to go in the Small/Mini size state which is
         * available on Mac OS X. Not sure why this happens but this seems to help
         * against. */
        QList <QWidget*> list = m_pStack->widget(i)->findChildren<QWidget*>();
        for (int a = 0; a < list.size(); ++a)
        {
            QWidget *w = list.at(a);
            if (w->parent() == m_pStack->widget(i))
                w->setFixedHeight(w->sizeHint().height());
        }
        m_pStack->setCurrentIndex(i);
        /* Now make sure the layout is freshly calculated. */
        layout()->activate();
        QSize s = minimumSize();
        if (iMinWidth > s.width())
            s.setWidth(iMinWidth);
        m_sizeList.insert(0, s);
        m_pStack->widget(i)->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    }

    sltCategoryChanged(m_pSelector->currentId());
#else /* Q_WS_MAC */
    /* Resize to the minimum possible size: */
    QSize s = minimumSize();
    if (iMinWidth > s.width())
        s.setWidth(iMinWidth);
    resize(s);
#endif /* Q_WS_MAC */
}

void UISettingsDialog::assignValidator(UISettingsPage *pPage)
{
    /* Assign validator: */
    UIPageValidator *pValidator = new UIPageValidator(this, pPage);
    connect(pValidator, SIGNAL(sigValidityChanged(UIPageValidator*)), this, SLOT(sltHandleValidityChange(UIPageValidator*)));
    pPage->setValidator(pValidator);

    // TODO: Why here?
    /* Configure navigation (tab-order): */
    pPage->setOrderAfter(m_pSelector->widget());
}

