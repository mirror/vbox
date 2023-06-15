/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoCreator classes implementation.
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
#include <QCheckBox>
#include <QGridLayout>
#include <QGraphicsBlurEffect>
#include <QGroupBox>
#include <QMenuBar>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QStatusBar>
#include <QStackedLayout>
#include <QTextStream>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "QIFileDialog.h"
#include "QILabel.h"
#include "QILineEdit.h"
#include "QIToolBar.h"
#include "UIActionPool.h"
#include "UICommon.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIModalWindowManager.h"
#include "UIVisoHostBrowser.h"
#include "UIVisoCreator.h"
#include "UIVisoContentBrowser.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif

/* Other VBox includes: */
#include <iprt/assert.h>
#include <iprt/getopt.h>
#include <iprt/path.h>


/*********************************************************************************************************************************
*   UIVisoSettingWidget definition.                                                                                          *
*********************************************************************************************************************************/

class SHARED_LIBRARY_STUFF UIVisoSettingWidget : public QIWithRetranslateUI<QGroupBox>
{
    Q_OBJECT;

signals:

    void sigClosed(bool fWithAccept);

public:

    UIVisoSettingWidget(QWidget *pParent);
    virtual void retranslateUi();
    void setSettings(const UIVisoCreatorWidget::Settings &settings);
    UIVisoCreatorWidget::Settings settings() const;

private slots:

    void sltButtonBoxOk();
    void sltButtonBoxCancel();

private:

    void prepareObjects();
    void prepareConnections();

    QTabWidget   *m_pTabWidget;
    QILabel      *m_pVisoNameLabel;
    QILabel      *m_pCustomOptionsLabel;
    QILineEdit   *m_pVisoNameLineEdit;
    QILineEdit   *m_pCustomOptionsLineEdit;
    QCheckBox    *m_pShowHiddenObjectsCheckBox;
    QILabel      *m_pShowHiddenObjectsLabel;
    QIDialogButtonBox *m_pButtonBox;
};


/*********************************************************************************************************************************
*   UIVisoSettingWidget implementation.                                                                                          *
*********************************************************************************************************************************/

UIVisoSettingWidget::UIVisoSettingWidget(QWidget *pParent)
    :QIWithRetranslateUI<QGroupBox>(pParent)
    , m_pTabWidget(0)
    , m_pVisoNameLabel(0)
    , m_pCustomOptionsLabel(0)
    , m_pVisoNameLineEdit(0)
    , m_pCustomOptionsLineEdit(0)
    , m_pShowHiddenObjectsCheckBox(0)
    , m_pShowHiddenObjectsLabel(0)
    , m_pButtonBox(0)
{
    prepareObjects();
    prepareConnections();
}

void UIVisoSettingWidget::prepareObjects()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    AssertReturnVoid(pMainLayout);
    m_pTabWidget = new QTabWidget;
    AssertReturnVoid(m_pTabWidget);
    pMainLayout->addWidget(m_pTabWidget);

    QWidget *pVisoOptionsContainerWidget = new QWidget;
    AssertReturnVoid(pVisoOptionsContainerWidget);
    QGridLayout *pVisoOptionsGridLayout = new QGridLayout(pVisoOptionsContainerWidget);
    AssertReturnVoid(pVisoOptionsGridLayout);
    //pVisoOptionsGridLayout->setSpacing(0);
    //pVisoOptionsGridLayout->setContentsMargins(0, 0, 0, 0);

    m_pTabWidget->addTab(pVisoOptionsContainerWidget, QApplication::translate("UIVisoCreatorWidget", "VISO options"));

    /* Name edit and and label: */
    m_pVisoNameLabel = new QILabel(QApplication::translate("UIVisoCreatorWidget", "VISO Name:"));
    m_pVisoNameLineEdit = new QILineEdit;
    int iRow = 0;
    if (m_pVisoNameLabel && m_pVisoNameLineEdit)
    {
        m_pVisoNameLabel->setBuddy(m_pVisoNameLineEdit);
        pVisoOptionsGridLayout->addWidget(m_pVisoNameLabel,    iRow, 0, 1, 1, Qt::AlignTop);
        pVisoOptionsGridLayout->addWidget(m_pVisoNameLineEdit, iRow, 1, 1, 1, Qt::AlignTop);
    }

    /* Custom Viso options stuff: */
    m_pCustomOptionsLabel = new QILabel(QApplication::translate("UIVisoCreatorWidget", "Custom VISO options:"));
    m_pCustomOptionsLineEdit = new QILineEdit;
    ++iRow;
    AssertReturnVoid(m_pCustomOptionsLabel);
    AssertReturnVoid(m_pCustomOptionsLineEdit);
    m_pCustomOptionsLabel->setBuddy(m_pCustomOptionsLineEdit);
    pVisoOptionsGridLayout->addWidget(m_pCustomOptionsLabel,    iRow, 0, 1, 1, Qt::AlignTop);
    pVisoOptionsGridLayout->addWidget(m_pCustomOptionsLineEdit, iRow, 1, 1, 1, Qt::AlignTop);


    ++iRow;
    pVisoOptionsGridLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding), iRow, 0, 1, 2);

    QWidget *pDialogSettingsContainerWidget = new QWidget;
    AssertReturnVoid(pDialogSettingsContainerWidget);
    QGridLayout *pDialogSettingsContainerLayout = new QGridLayout(pDialogSettingsContainerWidget);
    AssertReturnVoid(pDialogSettingsContainerLayout);

    m_pTabWidget->addTab(pDialogSettingsContainerWidget, QApplication::translate("UIVisoCreatorWidget", "Dialog Settings"));

    iRow = 0;
    m_pShowHiddenObjectsCheckBox = new QCheckBox;
    m_pShowHiddenObjectsLabel = new QILabel(QApplication::translate("UIVisoCreatorWidget", "Show Hidden Objects"));
    m_pShowHiddenObjectsLabel->setBuddy(m_pShowHiddenObjectsCheckBox);
    pDialogSettingsContainerLayout->addWidget(m_pShowHiddenObjectsLabel,    iRow, 0, 1, 1, Qt::AlignTop);
    pDialogSettingsContainerLayout ->addWidget(m_pShowHiddenObjectsCheckBox, iRow, 1, 1, 1, Qt::AlignTop);

    ++iRow;
    pDialogSettingsContainerLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding), iRow, 0, 1, 2);

    m_pButtonBox = new QIDialogButtonBox;
    AssertReturnVoid(m_pButtonBox);
    m_pButtonBox->setDoNotPickDefaultButton(true);
    m_pButtonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    pMainLayout->addWidget(m_pButtonBox);

    retranslateUi();
}

void UIVisoSettingWidget::retranslateUi()
{
    if (m_pVisoNameLabel)
        m_pVisoNameLabel->setText(QApplication::translate("UIVisoCreatorWidget", "VISO Name:"));
    if (m_pCustomOptionsLabel)
        m_pCustomOptionsLabel->setText(QApplication::translate("UIVisoCreatorWidget", "Custom VISO options:"));

    if (m_pVisoNameLineEdit)
        m_pVisoNameLineEdit->setToolTip(QApplication::translate("UIVisoCreatorWidget", "Holds the name of the VISO medium."));
    if (m_pCustomOptionsLineEdit)
        m_pCustomOptionsLineEdit->setToolTip(QApplication::translate("UIVisoCreatorWidget", "The list of suctom options delimited with ';'."));
    if (m_pShowHiddenObjectsLabel)
        m_pShowHiddenObjectsLabel->setText(QApplication::translate("UIVisoCreatorWidget", "Show Hidden Objects"));
    if (m_pShowHiddenObjectsCheckBox)
        m_pShowHiddenObjectsCheckBox->setToolTip(QApplication::translate("UIVisoCreatorWidget", "When checked, "
                                                                         "multiple hidden objects are shown in the file browser"));
}

void UIVisoSettingWidget::prepareConnections()
{
    if (m_pButtonBox)
    {
        connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UIVisoSettingWidget::sltButtonBoxCancel);
        connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UIVisoSettingWidget::sltButtonBoxOk);
    }
}

void UIVisoSettingWidget::sltButtonBoxOk()
{
    emit sigClosed(true);
}

void UIVisoSettingWidget::sltButtonBoxCancel()
{
    emit sigClosed(false);
}


void UIVisoSettingWidget::setSettings(const UIVisoCreatorWidget::Settings &settings)
{
    if (m_pVisoNameLineEdit)
        m_pVisoNameLineEdit->setText(settings.m_strVisoName);
    if (m_pCustomOptionsLineEdit)
        m_pCustomOptionsLineEdit->setText(settings.m_customOptions.join(";"));
    if (m_pShowHiddenObjectsCheckBox)
        m_pShowHiddenObjectsCheckBox->setChecked(settings.m_fShowHiddenObjects);
}

UIVisoCreatorWidget::Settings UIVisoSettingWidget::settings() const
{
    UIVisoCreatorWidget::Settings settings;
    if (m_pVisoNameLineEdit)
        settings.m_strVisoName = m_pVisoNameLineEdit->text();
    if (m_pCustomOptionsLineEdit)
        settings.m_customOptions = m_pCustomOptionsLineEdit->text().split(";");
    if (m_pShowHiddenObjectsCheckBox)
        settings.m_fShowHiddenObjects = m_pShowHiddenObjectsCheckBox->isChecked();
    return settings;
}


/*********************************************************************************************************************************
*   UIVisoCreatorWidget implementation.                                                                                          *
*********************************************************************************************************************************/

UIVisoCreatorWidget::UIVisoCreatorWidget(UIActionPool *pActionPool, QWidget *pParent,
                                         bool fShowToolBar, const QString& strMachineName /* = QString() */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pActionSettings(0)
    , m_pAddAction(0)
    , m_pRemoveAction(0)
    , m_pCreateNewDirectoryAction(0)
    , m_pRenameAction(0)
    , m_pResetAction(0)
    , m_pOpenAction(0)
    , m_pImportISOAction(0)
    , m_pMainLayout(0)
    , m_pHostBrowser(0)
    , m_pVISOContentBrowser(0)
    , m_pToolBar(0)
    , m_pVerticalToolBar(0)
    , m_pMainMenu(0)
    , m_pActionPool(pActionPool)
    , m_fShowToolBar(fShowToolBar)
    , m_fShowSettingsDialog(false)
    , m_pSettingsWidget(0)
    , m_pBrowserContainerWidget(0)
    , m_pStackedLayout(0)
{
    m_visoOptions.m_strVisoName = !strMachineName.isEmpty() ? strMachineName : "ad-hoc";
    prepareWidgets();
    populateMenuMainToolbar();
    prepareConnections();
    retranslateUi();
}

QStringList UIVisoCreatorWidget::entryList() const
{
    if (!m_pVISOContentBrowser)
        return QStringList();
    return m_pVISOContentBrowser->entryList();
}

const QString &UIVisoCreatorWidget::visoName() const
{
    return m_visoOptions.m_strVisoName;
}

const QStringList &UIVisoCreatorWidget::customOptions() const
{
    return m_visoOptions.m_customOptions;
}

QString UIVisoCreatorWidget::currentPath() const
{
    if (!m_pHostBrowser)
        return QString();
    return m_pHostBrowser->currentPath();
}

void UIVisoCreatorWidget::setCurrentPath(const QString &strPath)
{
    if (!m_pHostBrowser)
        return;
    m_pHostBrowser->setCurrentPath(strPath);
}

QMenu *UIVisoCreatorWidget::menu() const
{
    return m_pMainMenu;
}

void UIVisoCreatorWidget::retranslateUi()
{
    if (m_pHostBrowser)
        m_pHostBrowser->setTitle(tr("Host File System"));
    if (m_pVISOContentBrowser)
        m_pVISOContentBrowser->setTitle(tr("VISO Content"));
    if (m_pSettingsWidget)
        m_pSettingsWidget->setTitle(tr("Settings"));
}

void UIVisoCreatorWidget::resizeEvent(QResizeEvent *pEvent)
{
    Q_UNUSED(pEvent);
    if (m_pOverlayWidget && m_fShowSettingsDialog)
    {
#ifndef VBOX_IS_QT6_OR_LATER
        const QPixmap *pLabelPixmap = m_pOverlayWidget->pixmap();
        if (pLabelPixmap)
        {
            QPixmap newPixmap = pLabelPixmap->scaled(m_pOverlayWidget->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            if (!newPixmap.isNull())
                m_pOverlayWidget->setPixmap(newPixmap);
        }
#else
        QPixmap labelPixmap = m_pOverlayWidget->pixmap();
        QPixmap newPixmap = labelPixmap.scaled(m_pOverlayWidget->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (!newPixmap.isNull())
            m_pOverlayWidget->setPixmap(newPixmap);
#endif
    }
}

void UIVisoCreatorWidget::sltAddObjectsToViso(QStringList pathList)
{
    if (m_pVISOContentBrowser)
        m_pVISOContentBrowser->addObjectsToViso(pathList);
}

void UIVisoCreatorWidget::sltSettingsActionToggled(bool fChecked)
{
    if (m_fShowSettingsDialog == fChecked)
        return;
    toggleSettingsWidget(fChecked);
}

void UIVisoCreatorWidget::sltSettingsDialogClosed(bool fAccepted)
{
    toggleSettingsWidget(false);
    if (m_pActionSettings)
        m_pActionSettings->setChecked(false);
    /* Update settings:*/
    if (fAccepted && m_pSettingsWidget)
    {
        Settings newSettings = m_pSettingsWidget->settings();
        if (m_visoOptions.m_strVisoName != newSettings.m_strVisoName)
        {
            m_visoOptions.m_strVisoName = newSettings.m_strVisoName;
            emit sigVisoNameChanged(m_visoOptions.m_strVisoName);
        }
        if (m_visoOptions.m_customOptions != newSettings.m_customOptions)
            m_visoOptions.m_customOptions = newSettings.m_customOptions;
        if (m_visoOptions.m_fShowHiddenObjects != newSettings.m_fShowHiddenObjects)
            m_visoOptions.m_fShowHiddenObjects = newSettings.m_fShowHiddenObjects;
    }
}

void UIVisoCreatorWidget::sltBrowserTreeViewVisibilityChanged(bool fVisible)
{
    Q_UNUSED(fVisible);
}

void UIVisoCreatorWidget::sltHostBrowserTableSelectionChanged(QStringList pathList)
{
    if (m_pAddAction)
        m_pAddAction->setEnabled(!pathList.isEmpty());
    if (m_pImportISOAction)
        m_pImportISOAction->setEnabled(!findISOFiles(pathList).isEmpty());
}

void UIVisoCreatorWidget::sltContentBrowserTableSelectionChanged(bool fIsSelectionEmpty)
{
    if (m_pRemoveAction)
        m_pRemoveAction->setEnabled(!fIsSelectionEmpty);
    if (m_pRenameAction)
        m_pRenameAction->setEnabled(!fIsSelectionEmpty);
}

void UIVisoCreatorWidget::sltShowContextMenu(const QWidget *pContextMenuRequester, const QPoint &point)
{
    if (!pContextMenuRequester)
        return;

    QMenu menu;

    if (sender() == m_pHostBrowser)
    {
        menu.addAction(m_pAddAction);
    }
    else if (sender() == m_pVISOContentBrowser)
    {
        menu.addAction(m_pRemoveAction);
        menu.addAction(m_pCreateNewDirectoryAction);
        menu.addAction(m_pResetAction);
    }

    menu.exec(pContextMenuRequester->mapToGlobal(point));
}

void UIVisoCreatorWidget::sltOpenAction()
{
    QString strFileName =  QIFileDialog::getOpenFileName(uiCommon().defaultFolderPathForType(UIMediumDeviceType_DVD),
                                                         "VISO files (*.viso)", this, UIVisoCreatorWidget::tr("Select a VISO file to load"));
    if (!strFileName.isEmpty() && m_pVISOContentBrowser)
        m_pVISOContentBrowser->parseVisoFileContent(strFileName);
}

void UIVisoCreatorWidget::sltISOImportAction()
{
    if (!m_pHostBrowser)
        return;
    // QStringList selectedObjectPaths = m_pHostBrowser->selectedPathList();
    // printf("dddd %d\n", findISOFiles(selectedObjectPaths).size());
}

void UIVisoCreatorWidget::prepareWidgets()
{
    m_pMainLayout = new QVBoxLayout(this);
    AssertPtrReturnVoid(m_pMainLayout);

    /* Configure layout: */
    const int iL = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 2;
    const int iT = qApp->style()->pixelMetric(QStyle::PM_LayoutTopMargin) / 2;
    const int iR = qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin) / 2;
    const int iB = qApp->style()->pixelMetric(QStyle::PM_LayoutBottomMargin) / 2;
    m_pMainLayout->setContentsMargins(iL, iT, iR, iB);
#ifdef VBOX_WS_MAC
    m_pMainLayout->setSpacing(10);
#else
    m_pMainLayout->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) / 2);
#endif

    if (m_pActionPool && m_pActionPool->action(UIActionIndex_M_VISOCreator))
        m_pMainMenu = m_pActionPool->action(UIActionIndex_M_VISOCreator)->menu();

    if (m_fShowToolBar)
    {
        m_pToolBar = new QIToolBar(parentWidget());
        AssertPtrReturnVoid(m_pToolBar);
        /* Configure toolbar: */
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        m_pMainLayout->addWidget(m_pToolBar);
    }

    m_pStackedLayout = new QStackedLayout;
    AssertPtrReturnVoid(m_pStackedLayout);
    m_pMainLayout->addLayout(m_pStackedLayout);

    m_pBrowserContainerWidget = new QWidget;
    AssertPtrReturnVoid(m_pBrowserContainerWidget);

    QGridLayout *pContainerLayout = new QGridLayout(m_pBrowserContainerWidget);
    AssertPtrReturnVoid(pContainerLayout);
    pContainerLayout->setContentsMargins(0, 0, 0, 0);

    m_pHostBrowser = new UIVisoHostBrowser;
    AssertPtrReturnVoid(m_pHostBrowser);
    pContainerLayout->addWidget(m_pHostBrowser, 0, 0, 1, 4);

    prepareVerticalToolBar();
    AssertPtrReturnVoid(m_pVerticalToolBar);
    pContainerLayout->addWidget(m_pVerticalToolBar, 0, 4, 1, 1);

    m_pVISOContentBrowser = new UIVisoContentBrowser;
    AssertPtrReturnVoid(m_pVISOContentBrowser);
    pContainerLayout->addWidget(m_pVISOContentBrowser, 0, 5, 1, 4);
    m_pVISOContentBrowser->setVisoName(m_visoOptions.m_strVisoName);

    m_pOverlayWidget = new QLabel(this);
    AssertPtrReturnVoid(m_pOverlayWidget);
    m_pOverlayBlurEffect = new QGraphicsBlurEffect(this);
    AssertPtrReturnVoid(m_pOverlayBlurEffect);
    m_pOverlayWidget->setGraphicsEffect(m_pOverlayBlurEffect);
    m_pOverlayBlurEffect->setEnabled(false);
    m_pOverlayBlurEffect->setBlurRadius(4);

    m_pSettingsWidget = new UIVisoSettingWidget(this);
    AssertPtrReturnVoid(m_pSettingsWidget);
    m_pSettingsWidget->setMinimumWidth(300);
    m_pSettingsWidget->setMinimumHeight(300);
    m_pStackedLayout->addWidget(m_pOverlayWidget);
    m_pStackedLayout->addWidget(m_pBrowserContainerWidget);

    m_pStackedLayout->setCurrentWidget(m_pBrowserContainerWidget);
    m_pSettingsWidget->hide();
}

void UIVisoCreatorWidget::prepareConnections()
{
    if (m_pHostBrowser)
    {
        connect(m_pHostBrowser, &UIVisoHostBrowser::sigAddObjectsToViso,
                this, &UIVisoCreatorWidget::sltAddObjectsToViso);
        connect(m_pHostBrowser, &UIVisoHostBrowser::sigTreeViewVisibilityChanged,
                this, &UIVisoCreatorWidget::sltBrowserTreeViewVisibilityChanged);
        connect(m_pHostBrowser, &UIVisoHostBrowser::sigTableSelectionChanged,
                this, &UIVisoCreatorWidget::sltHostBrowserTableSelectionChanged);
        connect(m_pHostBrowser, &UIVisoHostBrowser::sigCreateFileTableViewContextMenu,
                this, &UIVisoCreatorWidget::sltShowContextMenu);
    }

    if (m_pVISOContentBrowser)
    {
        connect(m_pVISOContentBrowser, &UIVisoContentBrowser::sigTableSelectionChanged,
                this, &UIVisoCreatorWidget::sltContentBrowserTableSelectionChanged);
        connect(m_pVISOContentBrowser, &UIVisoContentBrowser::sigCreateFileTableViewContextMenu,
                this, &UIVisoCreatorWidget::sltShowContextMenu);
    }

    if (m_pActionSettings)
        connect(m_pActionSettings, &QAction::triggered, this, &UIVisoCreatorWidget::sltSettingsActionToggled);

    if (m_pSettingsWidget)
    {
        connect(m_pSettingsWidget, &UIVisoSettingWidget::sigClosed,
                this, &UIVisoCreatorWidget::sltSettingsDialogClosed);
    }

    if (m_pAddAction)
        connect(m_pAddAction, &QAction::triggered,
                m_pHostBrowser, &UIVisoHostBrowser::sltAddAction);

    if (m_pCreateNewDirectoryAction)
        connect(m_pCreateNewDirectoryAction, &QAction::triggered,
                m_pVISOContentBrowser, &UIVisoContentBrowser::sltCreateNewDirectory);
    if (m_pRemoveAction)
        connect(m_pRemoveAction, &QAction::triggered,
                m_pVISOContentBrowser, &UIVisoContentBrowser::sltRemoveItems);
    if (m_pResetAction)
        connect(m_pResetAction, &QAction::triggered,
                m_pVISOContentBrowser, &UIVisoContentBrowser::sltResetAction);
    if (m_pRenameAction)
        connect(m_pRenameAction, &QAction::triggered,
                m_pVISOContentBrowser,&UIVisoContentBrowser::sltItemRenameAction);
    if (m_pOpenAction)
        connect(m_pOpenAction, &QAction::triggered,
                this, &UIVisoCreatorWidget::sltOpenAction);
    if (m_pImportISOAction)
        connect(m_pImportISOAction, &QAction::triggered,
                this, &UIVisoCreatorWidget::sltISOImportAction);
}

void UIVisoCreatorWidget::prepareActions()
{
    if (!m_pActionPool)
        return;

    m_pActionSettings = m_pActionPool->action(UIActionIndex_M_VISOCreator_ToggleSettingsDialog);

    m_pAddAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_Add);
    if (m_pAddAction && m_pHostBrowser)
        m_pAddAction->setEnabled(m_pHostBrowser->tableViewHasSelection());
    m_pRemoveAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_Remove);
    if (m_pRemoveAction && m_pVISOContentBrowser)
        m_pRemoveAction->setEnabled(m_pVISOContentBrowser->tableViewHasSelection());
    m_pCreateNewDirectoryAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_CreateNewDirectory);
    m_pRenameAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_Rename);
    if (m_pRenameAction && m_pVISOContentBrowser)
        m_pRenameAction->setEnabled(m_pVISOContentBrowser->tableViewHasSelection());
    m_pResetAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_Reset);
    m_pOpenAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_Open);
    m_pImportISOAction = m_pActionPool->action(UIActionIndex_M_VISOCreator_ImportISO);
    if (m_pImportISOAction)
        m_pImportISOAction->setEnabled(false);
}

void UIVisoCreatorWidget::populateMenuMainToolbar()
{
    prepareActions();
    if (m_pToolBar)
    {
        if (m_pActionSettings)
            m_pToolBar->addAction(m_pActionSettings);
    }
    if (m_pMainMenu)
    {
        m_pMainMenu->addAction(m_pActionSettings);
        m_pMainMenu->addSeparator();
        if (m_pOpenAction)
            m_pMainMenu->addAction(m_pOpenAction);
        if (m_pAddAction)
            m_pMainMenu->addAction(m_pAddAction);
        if (m_pImportISOAction)
            m_pMainMenu->addAction(m_pImportISOAction);
        if (m_pRemoveAction)
            m_pMainMenu->addAction(m_pRemoveAction);
        if (m_pRenameAction)
            m_pMainMenu->addAction(m_pRenameAction);
        if (m_pCreateNewDirectoryAction)
            m_pMainMenu->addAction(m_pCreateNewDirectoryAction);
        if (m_pResetAction)
            m_pMainMenu->addAction(m_pResetAction);
    }

    if (m_pVerticalToolBar)
    {
        /* Add to dummy QWidget to toolbar to center the action icons vertically: */
        QWidget *topSpacerWidget = new QWidget(this);
        AssertPtrReturnVoid(topSpacerWidget);
        topSpacerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        topSpacerWidget->setVisible(true);
        QWidget *bottomSpacerWidget = new QWidget(this);
        AssertPtrReturnVoid(bottomSpacerWidget);
        bottomSpacerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        bottomSpacerWidget->setVisible(true);

        m_pVerticalToolBar->addWidget(topSpacerWidget);
        if (m_pAddAction)
            m_pVerticalToolBar->addAction(m_pAddAction);
        if (m_pImportISOAction)
            m_pVerticalToolBar->addAction(m_pImportISOAction);
        if (m_pRenameAction)
            m_pVerticalToolBar->addAction(m_pRenameAction);
        if (m_pRemoveAction)
            m_pVerticalToolBar->addAction(m_pRemoveAction);
        if (m_pCreateNewDirectoryAction)
            m_pVerticalToolBar->addAction(m_pCreateNewDirectoryAction);
        if (m_pResetAction)
            m_pVerticalToolBar->addAction(m_pResetAction);

        m_pVerticalToolBar->addWidget(bottomSpacerWidget);
    }
}

void UIVisoCreatorWidget::toggleSettingsWidget(bool fShown)
{
    m_fShowSettingsDialog = fShown;
    emit sigSettingDialogToggle(fShown);

    if (m_pSettingsWidget && m_pSettingsWidget->isVisible() != m_fShowSettingsDialog /*&& m_pOverlayWidget*/ && m_pOverlayBlurEffect)
    {
        QPixmap shot = m_pBrowserContainerWidget->grab();
        m_pOverlayWidget->setPixmap(shot);

        if (m_fShowSettingsDialog)
            m_pStackedLayout->setCurrentWidget(m_pOverlayWidget);
        else
            m_pStackedLayout->setCurrentWidget(m_pBrowserContainerWidget);

        m_pSettingsWidget->setVisible(m_fShowSettingsDialog);

        m_pOverlayBlurEffect->setEnabled(m_fShowSettingsDialog);
        m_pSettingsWidget->raise();
        if (m_fShowSettingsDialog)
        {
            int x = 0.5 * (m_pBrowserContainerWidget->width() - m_pSettingsWidget->width());
            int y = 0.5 * (m_pBrowserContainerWidget->height() - m_pSettingsWidget->height());
            m_pSettingsWidget->move(m_pBrowserContainerWidget->x() + x, m_pBrowserContainerWidget->y() + y);
        }
        if (m_pMainMenu)
            m_pMainMenu->setEnabled(!m_fShowSettingsDialog);
        if (m_fShowSettingsDialog)
            m_pSettingsWidget->setSettings(m_visoOptions);
    }
}

QStringList UIVisoCreatorWidget::findISOFiles(const QStringList &pathList) const
{
    QStringList isoList;
    foreach (const QString &strPath, pathList)
    {
        if (QFileInfo(strPath).suffix().compare("iso", Qt::CaseInsensitive) == 0)
            isoList << strPath;
    }
    return isoList;
}

void UIVisoCreatorWidget::prepareVerticalToolBar()
{
    m_pVerticalToolBar = new QIToolBar;
    AssertPtrReturnVoid(m_pVerticalToolBar);

    m_pVerticalToolBar->setOrientation(Qt::Vertical);
}

/* static */
QUuid UIVisoCreatorDialog::createViso(UIActionPool *pActionPool, QWidget *pParent,
                                      const QString &strDefaultFolder /* = QString() */,
                                      const QString &strMachineName /* = QString() */)
{
    QString strVisoSaveFolder(strDefaultFolder);
    if (strVisoSaveFolder.isEmpty())
        strVisoSaveFolder = uiCommon().defaultFolderPathForType(UIMediumDeviceType_DVD);

    QWidget *pDialogParent = windowManager().realParentWindow(pParent);
    UIVisoCreatorDialog *pVisoCreator = new UIVisoCreatorDialog(pActionPool, pDialogParent,
                                                                strVisoSaveFolder, strMachineName);
    AssertPtrReturn(pVisoCreator, QUuid());

    windowManager().registerNewParent(pVisoCreator, pDialogParent);
    pVisoCreator->setCurrentPath(gEDataManager->visoCreatorRecentFolder());

    if (pVisoCreator->exec(false /* not application modal */))
    {
        QStringList VisoEntryList = pVisoCreator->entryList();

        if (VisoEntryList.empty() || VisoEntryList[0].isEmpty())
        {
            delete pVisoCreator;
            return QUuid();
        }

        gEDataManager->setVISOCreatorRecentFolder(pVisoCreator->currentPath());

        QFile file(pVisoCreator->visoFileFullPath());
        if (file.open(QFile::WriteOnly | QFile::Truncate))
        {
            QString strVisoName = pVisoCreator->visoName();
            if (strVisoName.isEmpty())
                strVisoName = strMachineName;

            QTextStream stream(&file);
            stream << QString("%1 %2").arg("--iprt-iso-maker-file-marker-bourne-sh").arg(QUuid::createUuid().toString()) << "\n";
            stream<< "--volume-id=" << strVisoName << "\n";
            stream << VisoEntryList.join("\n");
            if (!pVisoCreator->customOptions().isEmpty())
            {
                stream << "\n";
                stream << pVisoCreator->customOptions().join("\n");
            }
            file.close();
        }
    } // if (pVisoCreator->exec(false /* not application modal */))
    delete pVisoCreator;
    return QUuid();
}


/*********************************************************************************************************************************
*   UIVisoCreatorDialog implementation.                                                                                          *
*********************************************************************************************************************************/
UIVisoCreatorDialog::UIVisoCreatorDialog(UIActionPool *pActionPool, QWidget *pParent,
                                         const QString& strVisoSavePath, const QString& strMachineName /* = QString() */)
    : QIWithRetranslateUI<QIWithRestorableGeometry<QIMainDialog> >(pParent)
    , m_pVisoCreatorWidget(0)
    , m_pButtonBox(0)
    , m_pStatusBar(0)
    , m_pStatusLabel(0)
    , m_pActionPool(pActionPool)
    , m_iGeometrySaveTimerId(-1)
    , m_strVisoSavePath(strVisoSavePath)
{
    /* Make sure that the base class does not close this dialog upon pressing escape.
       we manage escape key here with special casing: */
    setRejectByEscape(false);
    prepareWidgets(strMachineName);
    prepareConnections();
    loadSettings();
}

QStringList  UIVisoCreatorDialog::entryList() const
{
    if (m_pVisoCreatorWidget)
        return m_pVisoCreatorWidget->entryList();
    return QStringList();
}

QString UIVisoCreatorDialog::visoName() const
{
    if (m_pVisoCreatorWidget)
        return m_pVisoCreatorWidget->visoName();
    return QString();
}

QStringList UIVisoCreatorDialog::customOptions() const
{
    if (m_pVisoCreatorWidget)
        return m_pVisoCreatorWidget->customOptions();
    return QStringList();
}

QString UIVisoCreatorDialog::currentPath() const
{
    if (m_pVisoCreatorWidget)
        return m_pVisoCreatorWidget->currentPath();
    return QString();
}

void    UIVisoCreatorDialog::setCurrentPath(const QString &strPath)
{
    if (m_pVisoCreatorWidget)
        m_pVisoCreatorWidget->setCurrentPath(strPath);
}

void UIVisoCreatorDialog::prepareWidgets(const QString &strMachineName)
{
    QWidget *pCentralWidget = new QWidget;
    AssertPtrReturnVoid(pCentralWidget);
    setCentralWidget(pCentralWidget);
    QVBoxLayout *pMainLayout = new QVBoxLayout;
    AssertPtrReturnVoid(pMainLayout);
    pCentralWidget->setLayout(pMainLayout);


    m_pVisoCreatorWidget = new UIVisoCreatorWidget(m_pActionPool, this, true /* show toolbar */, strMachineName);
    AssertPtrReturnVoid(m_pVisoCreatorWidget);
    if (m_pVisoCreatorWidget->menu())
    {
        menuBar()->addMenu(m_pVisoCreatorWidget->menu());
        pMainLayout->addWidget(m_pVisoCreatorWidget);
        connect(m_pVisoCreatorWidget, &UIVisoCreatorWidget::sigSetCancelButtonShortCut,
                this, &UIVisoCreatorDialog::sltSetCancelButtonShortCut);
        connect(m_pVisoCreatorWidget, &UIVisoCreatorWidget::sigVisoNameChanged,
                this, &UIVisoCreatorDialog::sltVisoNameChanged);
        connect(m_pVisoCreatorWidget, &UIVisoCreatorWidget::sigSettingDialogToggle,
                this, &UIVisoCreatorDialog::sltSettingDialogToggle);
    }

    m_pButtonBox = new QIDialogButtonBox;
    AssertPtrReturnVoid(m_pButtonBox);
    m_pButtonBox->setDoNotPickDefaultButton(true);
    m_pButtonBox->setStandardButtons(QDialogButtonBox::Help | QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(QKeySequence(Qt::Key_Escape));
    pMainLayout->addWidget(m_pButtonBox);

    connect(m_pButtonBox->button(QIDialogButtonBox::Help), &QPushButton::pressed,
            m_pButtonBox, &QIDialogButtonBox::sltHandleHelpRequest);

    m_pButtonBox->button(QDialogButtonBox::Help)->setShortcut(QKeySequence::HelpContents);

    uiCommon().setHelpKeyword(m_pButtonBox->button(QIDialogButtonBox::Help), "create-optical-disk-image");

    m_pStatusLabel = new QILabel;
    m_pStatusBar = new QStatusBar(this);
    AssertPtrReturnVoid(m_pButtonBox);
    AssertPtrReturnVoid(m_pStatusLabel);

    pMainLayout->addWidget(m_pStatusBar);
    m_pStatusBar->addPermanentWidget(m_pStatusLabel);

    retranslateUi();
}

void UIVisoCreatorDialog::prepareConnections()
{
    if (m_pButtonBox)
    {
        connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UIVisoCreatorDialog::close);
        connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UIVisoCreatorDialog::accept);
    }
}

void UIVisoCreatorDialog::retranslateUi()
{
    updateWindowTitle();
    if (m_pButtonBox && m_pButtonBox->button(QDialogButtonBox::Ok))
    {
        m_pButtonBox->button(QDialogButtonBox::Ok)->setText(UIVisoCreatorWidget::tr("C&reate"));
        m_pButtonBox->button(QDialogButtonBox::Ok)->setToolTip(UIVisoCreatorWidget::tr("Creates VISO file with the selected content"));
    }
    if (m_pButtonBox && m_pButtonBox->button(QDialogButtonBox::Help))
        m_pButtonBox->button(QDialogButtonBox::Help)->setToolTip(UIVisoCreatorWidget::tr("Opens the help browser and navigates to the related section"));
    updateWindowTitle();
    updateStatusLabel();
}

bool UIVisoCreatorDialog::event(QEvent *pEvent)
{
    if (pEvent->type() == QEvent::Resize || pEvent->type() == QEvent::Move)
    {
        if (m_iGeometrySaveTimerId != -1)
            killTimer(m_iGeometrySaveTimerId);
        m_iGeometrySaveTimerId = startTimer(300);
    }
    else if (pEvent->type() == QEvent::Timer)
    {
        QTimerEvent *pTimerEvent = static_cast<QTimerEvent*>(pEvent);
        if (pTimerEvent->timerId() == m_iGeometrySaveTimerId)
        {
            killTimer(m_iGeometrySaveTimerId);
            m_iGeometrySaveTimerId = -1;
            saveDialogGeometry();
        }
    }
    return QIWithRetranslateUI<QIWithRestorableGeometry<QIMainDialog> >::event(pEvent);
}

void UIVisoCreatorDialog::sltSetCancelButtonShortCut(QKeySequence keySequence)
{
    if (m_pButtonBox && m_pButtonBox->button(QDialogButtonBox::Cancel))
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(keySequence);
}

void UIVisoCreatorDialog::sltVisoNameChanged(const QString &strName)
{
    Q_UNUSED(strName);
    updateWindowTitle();
    updateStatusLabel();
}

void UIVisoCreatorDialog::sltSettingDialogToggle(bool fIsShown)
{
    if (m_pButtonBox)
        m_pButtonBox->setEnabled(!fIsShown);
}

void UIVisoCreatorDialog::loadSettings()
{
    const QRect availableGeo = gpDesktop->availableGeometry(this);
    int iDefaultWidth = availableGeo.width() / 2;
    int iDefaultHeight = availableGeo.height() * 3 / 4;
    QRect defaultGeo(0, 0, iDefaultWidth, iDefaultHeight);

    QWidget *pParent = windowManager().realParentWindow(parentWidget() ? parentWidget() : windowManager().mainWindowShown());
    /* Load geometry from extradata: */
    const QRect geo = gEDataManager->visoCreatorDialogGeometry(this, pParent, defaultGeo);
    LogRel2(("GUI: UISoftKeyboard: Restoring geometry to: Origin=%dx%d, Size=%dx%d\n",
             geo.x(), geo.y(), geo.width(), geo.height()));

    restoreGeometry(geo);
}

void UIVisoCreatorDialog::saveDialogGeometry()
{
    const QRect geo = currentGeometry();
    LogRel2(("GUI: UIMediumSelector: Saving geometry as: Origin=%dx%d, Size=%dx%d\n",
             geo.x(), geo.y(), geo.width(), geo.height()));
    gEDataManager->setVisoCreatorDialogGeometry(geo, isCurrentlyMaximized());
}

void UIVisoCreatorDialog::updateWindowTitle()
{
    setWindowTitle(QString("%1 - %2.%3").arg(UIVisoCreatorWidget::tr("VISO Creator")).arg(visoName()).arg("viso"));
}

void UIVisoCreatorDialog::updateStatusLabel()
{
    if (m_pStatusLabel)
        m_pStatusLabel->setText(QString("%1: %2").arg(UIVisoCreatorWidget::tr("VISO file")).arg(visoFileFullPath()));
}

QString UIVisoCreatorDialog::visoFileFullPath() const
{
    return QString("%1/%2%3").arg(m_strVisoSavePath).arg(visoName()).arg(".viso");
}

#include "UIVisoCreator.moc"
