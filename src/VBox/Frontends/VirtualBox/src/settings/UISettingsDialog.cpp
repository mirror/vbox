/* $Id$ */
/** @file
 * VBox Qt GUI - UISettingsDialog class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Global includes */
# include <QProgressBar>
# include <QPushButton>
# include <QStackedWidget>
# include <QTimer>
# include <QCloseEvent>

/* Local includes */
# include "UISettingsDialog.h"
# include "UIWarningPane.h"
# include "VBoxGlobal.h"
# include "UIMessageCenter.h"
# include "UIPopupCenter.h"
# include "QIWidgetValidator.h"
# include "VBoxSettingsSelector.h"
# include "UISettingsSerializer.h"
# include "UISettingsPage.h"
# include "UIToolBar.h"
# include "UIIconPool.h"
# include "UIConverter.h"
# ifdef Q_WS_MAC
#  include "VBoxUtils.h"
# endif

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


#ifdef Q_WS_MAC
# if MAC_LEOPARD_STYLE
#  define VBOX_GUI_WITH_TOOLBAR_SETTINGS
# endif
#endif


/* Settings Dialog Constructor: */
UISettingsDialog::UISettingsDialog(QWidget *pParent)
    /* Parent class: */
    : QIWithRetranslateUI<QIMainDialog>(pParent)
    /* Protected variables: */
    , m_pSelector(0)
    , m_pStack(0)
    /* Common variables: */
    , m_configurationAccessLevel(ConfigurationAccessLevel_Null)
    , m_fPolished(false)
    /* Loading/saving stuff: */
    , m_fLoaded(false)
    , m_fSaved(false)
    /* Status-bar stuff: */
    , m_pStatusBar(0)
    /* Process-bar stuff: */
    , m_pProcessBar(0)
    /* Warning-pane stuff: */
    , m_pWarningPane(0)
    , m_fValid(true)
    , m_fSilent(true)
    /* Whats-this stuff: */
    , m_pWhatsThisTimer(new QTimer(this))
    , m_pWhatsThisCandidate(0)
{
    /* Apply UI decorations: */
    Ui::UISettingsDialog::setupUi(this);

    /* Page-title font is derived from the system font: */
    QFont pageTitleFont = font();
    pageTitleFont.setBold(true);
    pageTitleFont.setPointSize(pageTitleFont.pointSize() + 2);
    m_pLbTitle->setFont(pageTitleFont);

    /* Prepare selector: */
    QGridLayout *pMainLayout = static_cast<QGridLayout*>(centralWidget()->layout());
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    /* No page-title with tool-bar: */
    m_pLbTitle->hide();
    /* Create modern tool-bar selector: */
    m_pSelector = new VBoxSettingsToolBarSelector(this);
    static_cast<UIToolBar*>(m_pSelector->widget())->enableMacToolbar();
    addToolBar(qobject_cast<QToolBar*>(m_pSelector->widget()));
    /* No title in this mode, we change the title of the window: */
    pMainLayout->setColumnMinimumWidth(0, 0);
    pMainLayout->setHorizontalSpacing(0);
#else
    /* Create classical tree-view selector: */
    m_pSelector = new VBoxSettingsTreeViewSelector(this);
    pMainLayout->addWidget(m_pSelector->widget(), 0, 0, 2, 1);
    m_pSelector->widget()->setFocus();
    pMainLayout->setSpacing(10);
#endif /* VBOX_GUI_WITH_TOOLBAR_SETTINGS */
    connect(m_pSelector, SIGNAL(categoryChanged(int)), this, SLOT(sltCategoryChanged(int)));

    /* Prepare page-stack: */
    m_pStack = new QStackedWidget(m_pWtStackHandler);
    popupCenter().setPopupStackOrientation(m_pStack, UIPopupStackOrientation_Bottom);
    QVBoxLayout *pStackLayout = new QVBoxLayout(m_pWtStackHandler);
    pStackLayout->setContentsMargins(0, 0, 0, 0);
    pStackLayout->addWidget(m_pStack);

    /* Prepare button-box: */
    m_pButtonBox->button(QDialogButtonBox::Ok)->setDefault(true);
    connect(m_pButtonBox, SIGNAL(helpRequested()), &msgCenter(), SLOT(sltShowHelpHelpDialog()));

    /* Prepare process-bar: */
    m_pProcessBar = new QProgressBar;

    /* Prepare warning-pane: */
    m_pWarningPane = new UIWarningPane;
    connect(m_pWarningPane, SIGNAL(sigHoverEnter(UIPageValidator*)), this, SLOT(sltHandleWarningPaneHovered(UIPageValidator*)));
    connect(m_pWarningPane, SIGNAL(sigHoverLeave(UIPageValidator*)), this, SLOT(sltHandleWarningPaneUnhovered(UIPageValidator*)));

    /* Prepare status-bar: */
    m_pStatusBar = new QStackedWidget;
    /* Add empty widget: */
    m_pStatusBar->addWidget(new QWidget);
    /* Add process-bar widget: */
    m_pStatusBar->addWidget(m_pProcessBar);
    /* Add warning-pane: */
    m_pStatusBar->addWidget(m_pWarningPane);
    /* Add status-bar to button-box: */
    m_pButtonBox->addExtraWidget(m_pStatusBar);

    /* Setup whatsthis stuff: */
    qApp->installEventFilter(this);
    m_pWhatsThisTimer->setSingleShot(true);
    connect(m_pWhatsThisTimer, SIGNAL(timeout()), this, SLOT(sltUpdateWhatsThis()));

    /* Translate UI: */
    retranslateUi();
}

UISettingsDialog::~UISettingsDialog()
{
    /* Recall popup-pane if any: */
    popupCenter().recall(m_pStack, "SettingsDialogWarning");
    /* Delete selector early! */
    delete m_pSelector;
}

void UISettingsDialog::execute()
{
    /* Load data: */
    loadOwnData();

    /* Execute dialog and wait for completion: */
    if (exec() != QDialog::Accepted)
        return;

    /* Save data: */
    saveOwnData();
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
    /* Mark as loaded: */
    m_fLoaded = true;
}

void UISettingsDialog::sltMarkSaved()
{
    /* Mark as saved: */
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

void UISettingsDialog::loadData(QVariant &data)
{
    /* Mark as not loaded: */
    m_fLoaded = false;

    /* Create settings loader: */
    UISettingsSerializer *pSettingsLoader = new UISettingsSerializer(this,
                                                                     UISettingsSerializer::Load,
                                                                     QVariant::fromValue(data),
                                                                     m_pSelector->settingPages());
    connect(pSettingsLoader, SIGNAL(destroyed(QObject*)), this, SLOT(sltMarkLoaded()));
    /* Ask to raise required page priority: */
    pSettingsLoader->raisePriorityOfPage(m_pSelector->currentId());
    /* Start loader: */
    pSettingsLoader->start();

    /* Upload data finally: */
    data = pSettingsLoader->data();
}

void UISettingsDialog::saveData(QVariant &data)
{
    /* Mark as not saved: */
    m_fSaved = false;

    /* Create settings saver: */
    UISettingsSerializer *pSettingsSaver = new UISettingsSerializer(this,
                                                                    UISettingsSerializer::Save,
                                                                    QVariant::fromValue(data),
                                                                    m_pSelector->settingPages());
    /* Start saver: */
    pSettingsSaver->start();

    /* Upload data finally: */
    data = pSettingsSaver->data();
}

void UISettingsDialog::retranslateUi()
{
    /* Translate generated stuff: */
    Ui::UISettingsDialog::retranslateUi(this);

    /* Translate warning stuff: */
    m_strWarningHint = tr("Invalid settings detected");
    if (!m_fValid || !m_fSilent)
        m_pWarningPane->setWarningLabel(m_strWarningHint);

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

void UISettingsDialog::setConfigurationAccessLevel(ConfigurationAccessLevel newConfigurationAccessLevel)
{
    /* Make sure something changed: */
    if (m_configurationAccessLevel == newConfigurationAccessLevel)
        return;

    /* Apply new configuration access level: */
    m_configurationAccessLevel = newConfigurationAccessLevel;

    /* And propagate it to settings-page(s): */
    foreach (UISettingsPage *pPage, m_pSelector->settingPages())
        pPage->setConfigurationAccessLevel(configurationAccessLevel());
}

QString UISettingsDialog::titleExtension() const
{
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    return m_pSelector->itemText(m_pSelector->currentId());
#else
    return tr("Settings");
#endif
}

void UISettingsDialog::addItem(const QString &strBigIcon,
                               const QString &strMediumIcon,
                               const QString &strSmallIcon,
                               int cId,
                               const QString &strLink,
                               UISettingsPage *pSettingsPage /* = 0 */,
                               int iParentId /* = -1 */)
{
    /* Add new selector item: */
    if (QWidget *pPage = m_pSelector->addItem(strBigIcon, strMediumIcon, strSmallIcon,
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
    {
        pSettingsPage->setId(cId);
        assignValidator(pSettingsPage);
    }
}

void UISettingsDialog::revalidate(UIPageValidator *pValidator)
{
    /* Perform page revalidation: */
    UISettingsPage *pSettingsPage = pValidator->page();
    QList<UIValidationMessage> messages;
    bool fIsValid = pSettingsPage->validate(messages);

    /* Remember revalidation result: */
    pValidator->setValid(fIsValid);

    /* Remember warning/error message: */
    if (messages.isEmpty())
        pValidator->setLastMessage(QString());
    else
    {
        /* Prepare title prefix: */
        // Its the only thing preventing us from moving this method to validator.
        const QString strTitlePrefix(m_pSelector->itemTextByPage(pSettingsPage));
        /* Prepare text: */
        QStringList text;
        foreach (const UIValidationMessage &message, messages)
        {
            /* Prepare title: */
            const QString strTitle(message.first.isNull() ? tr("<b>%1</b> page:").arg(strTitlePrefix) :
                                                            tr("<b>%1: %2</b> page:").arg(strTitlePrefix, message.first));
            /* Prepare paragraph: */
            QStringList paragraph(message.second);
            paragraph.prepend(strTitle);
            /* Format text for iterated message: */
            text << paragraph.join("<br>");
        }
        /* Remember text: */
        pValidator->setLastMessage(text.join("<br><br>"));
        LogRelFlow(("Settings Dialog:  Page validation FAILED: {%s}\n",
                    pValidator->lastMessage().toUtf8().constData()));
    }
}

void UISettingsDialog::revalidate()
{
    /* Perform dialog revalidation: */
    m_fValid = true;
    m_fSilent = true;
    m_pWarningPane->setWarningLabel(QString());

    /* Enumerating all the validators we have: */
    QList<UIPageValidator*> validators(findChildren<UIPageValidator*>());
    foreach (UIPageValidator *pValidator, validators)
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

            /* Configure warning-pane label: */
            m_pWarningPane->setWarningLabel(m_strWarningHint);

            /* Stop dialog revalidation on first error/warning: */
            break;
        }
    }

    /* Make sure warning-pane visible if necessary: */
    if ((!m_fValid || !m_fSilent) && m_pStatusBar->currentIndex() == 0)
        m_pStatusBar->setCurrentWidget(m_pWarningPane);
    /* Make sure empty-pane visible otherwise: */
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
        const QString strPageName(pSettingsPage->internalName());

        LogRelFlow(("Settings Dialog: %s Page: Revalidation in progress..\n",
                    strPageName.toUtf8().constData()));

        /* Perform page revalidation: */
        revalidate(pValidator);
        /* Perform inter-page recorrelation: */
        recorrelate(pSettingsPage);
        /* Perform dialog revalidation: */
        revalidate();

        LogRelFlow(("Settings Dialog: %s Page: Revalidation complete.\n",
                    strPageName.toUtf8().constData()));
    }
}

void UISettingsDialog::sltHandleWarningPaneHovered(UIPageValidator *pValidator)
{
    LogRelFlow(("Settings Dialog: Warning-icon hovered: %s.\n", pValidator->internalName().toUtf8().constData()));

    /* Show corresponding popup: */
    if (!m_fValid || !m_fSilent)
    {
        popupCenter().popup(m_pStack, "SettingsDialogWarning",
                            pValidator->lastMessage());
    }
}

void UISettingsDialog::sltHandleWarningPaneUnhovered(UIPageValidator *pValidator)
{
    LogRelFlow(("Settings Dialog: Warning-icon unhovered: %s.\n", pValidator->internalName().toUtf8().constData()));

    /* Recall corresponding popup: */
    popupCenter().recall(m_pStack, "SettingsDialogWarning");
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

    if (pWhatsThisWidget && !strWhatsThisText.isEmpty())
        pWhatsThisWidget->setToolTip(QString("<qt>%1</qt>").arg(strWhatsThisText));
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

    /* Ignore widgets which window is NOT settings window: */
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
    m_pWarningPane->registerValidator(pValidator);

    // TODO: Why here?
    /* Configure navigation (tab-order): */
    pPage->setOrderAfter(m_pSelector->widget());
}

