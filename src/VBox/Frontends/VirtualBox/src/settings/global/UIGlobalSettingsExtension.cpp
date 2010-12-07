/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsExtension class implementation
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#include <QHeaderView>

/* Local includes */
#include "UIGlobalSettingsExtension.h"
#include "UIIconPool.h"
#include "QIFileDialog.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxLicenseViewer.h"

/* Extension package item: */
class UIExtensionPackageItem : public QTreeWidgetItem
{
public:

    /* Extension package item type: */
    enum { UIItemType = QTreeWidgetItem::UserType + 1 };

    /* Extension package item constructor: */
    UIExtensionPackageItem(QTreeWidget *pParent, const UISettingsCacheGlobalExtensionItem &data)
        : QTreeWidgetItem(pParent, UIItemType)
        , m_data(data)
    {
        /* Icon: */
        setIcon(0, UIIconPool::iconSet(m_data.m_fIsUsable ?
                                       ":/status_check_16px.png" :
                                       ":/status_error_16px.png"));

        /* Name: */
        setText(1, m_data.m_strName);

        /* Version: */
        setText(2, QString("%1.%2").arg(m_data.m_strVersion).arg(m_data.m_strRevision));

        /* Tool-tip: */
        QString strTip = m_data.m_strDescription;
        if (!m_data.m_fIsUsable)
        {
            strTip += QString("<hr>");
            strTip += m_data.m_strWhyUnusable;
        }
        setToolTip(0, strTip);
        setToolTip(1, strTip);
        setToolTip(2, strTip);
    }

    QString name() const { return m_data.m_strName; }

private:

    UISettingsCacheGlobalExtensionItem m_data;
};

/* Extension page constructor: */
UIGlobalSettingsExtension::UIGlobalSettingsExtension()
    : m_pActionAdd(0), m_pActionRemove(0)
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsExtension::setupUi(this);

    /* Setup tree-widget: */
    //m_pPackagesTree->header()->hide();
    m_pPackagesTree->header()->setStretchLastSection(false);
    m_pPackagesTree->header()->setResizeMode(0, QHeaderView::ResizeToContents);
    m_pPackagesTree->header()->setResizeMode(1, QHeaderView::Stretch);
    m_pPackagesTree->header()->setResizeMode(2, QHeaderView::ResizeToContents);
    m_pPackagesTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_pPackagesTree, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
            this, SLOT(sltHandleCurrentItemChange(QTreeWidgetItem*)));
    connect(m_pPackagesTree, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(sltShowContextMenu(const QPoint&)));

    /* Setup tool-bar: */
    m_pPackagesToolbar->setUsesTextLabel(false);
    m_pPackagesToolbar->setIconSize(QSize(16, 16));
    m_pActionAdd = m_pPackagesToolbar->addAction(UIIconPool::iconSet(":/extension_pack_install_16px.png",
                                                                     ":/extension_pack_install_disabled_16px.png"),
                                                 QString(), this, SLOT(sltInstallPackage()));
    m_pActionRemove = m_pPackagesToolbar->addAction(UIIconPool::iconSet(":/extension_pack_uninstall_16px.png",
                                                                        ":/extension_pack_uninstall_disabled_16px.png"),
                                                    QString(), this, SLOT(sltRemovePackage()));

    /* Apply language settings: */
    retranslateUi();
}

/**
 * Attempt the actual installation.
 *
 * This code is shared by UIGlobalSettingsExtension::sltInstallPackage and
 * VBoxSelectorWnd::sltOpenUrls.
 * @todo    Is there perhaps a better home for this method?
 *
 * @returns true if successfully installed, false if not.
 * @param   strFilePath     The path to the tarball.
 * @param   pParent         The parent widget.
 * @param   pstrExtPackName Where to return the extension pack name. Optional.
 */
/*static*/ bool UIGlobalSettingsExtension::doInstallation(QString const &strFilePath, QWidget *pParent, QString *pstrExtPackName)
{
    bool fInstalled = false;

    /* Open the extpack tarball via IExtPackManager: */
    CExtPackManager manager = vboxGlobal().virtualBox().GetExtensionPackManager();
    CExtPackFile extPackFile = manager.OpenExtPackFile(strFilePath);
    if (manager.isOk())
    {
        if (pstrExtPackName)
            *pstrExtPackName = extPackFile.GetName();
        if (extPackFile.GetUsable())
        {
            /* Ask user to confirm installation: */
            QString strPackName = extPackFile.GetName();
            QString strPackVersion = QString("%1.%2").arg(extPackFile.GetVersion()).arg(extPackFile.GetRevision());
            QString strPackDescription = extPackFile.GetDescription();
            if (vboxProblem().confirmInstallingPackage(strPackName, strPackVersion, strPackDescription))
            {
                /* Display license if necessary: */
                bool fShouldBeLicenseShown = extPackFile.GetShowLicense();
                QString strLicense = extPackFile.GetLicense();
                VBoxLicenseViewer licenseViewer;
                if (!fShouldBeLicenseShown || licenseViewer.showLicenseFromString(strLicense) == QDialog::Accepted)
                {
                    /* Install package: */
                    extPackFile.Install();
                    if (extPackFile.isOk())
                        fInstalled = true;
                    else
                        vboxProblem().cannotInstallExtPack(strFilePath, extPackFile, pParent);
                }
            }
        }
        else
            vboxProblem().badExtPackFile(strFilePath, extPackFile, pParent);
    }
    else
        vboxProblem().cannotOpenExtPack(strFilePath, manager, pParent);

    return fInstalled;
}

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsExtension::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Load to cache: */
    const CExtPackManager &manager = vboxGlobal().virtualBox().GetExtensionPackManager();
    const CExtPackVector &packages = manager.GetInstalledExtPacks();
    for (int i = 0; i < packages.size(); ++i)
        m_cache.m_items << fetchData(packages[i]);

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsExtension::getFromCache()
{
    /* Fetch from cache: */
    for (int i = 0; i < m_cache.m_items.size(); ++i)
        new UIExtensionPackageItem(m_pPackagesTree, m_cache.m_items[i]);
    /* If at least one item present: */
    if (m_pPackagesTree->topLevelItemCount())
        m_pPackagesTree->setCurrentItem(m_pPackagesTree->topLevelItem(0));
    /* Update action's availability: */
    sltHandleCurrentItemChange(m_pPackagesTree->currentItem());
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsExtension::putToCache()
{
    /* Nothing to put to cache... */
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsExtension::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Nothing to save from cache... */

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Navigation stuff: */
void UIGlobalSettingsExtension::setOrderAfter(QWidget *pWidget)
{
    /* Setup tab-order: */
    setTabOrder(pWidget, m_pPackagesTree);
}

/* Translation stuff: */
void UIGlobalSettingsExtension::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsExtension::retranslateUi(this);

    /* Translate actions: */
    m_pActionAdd->setText(tr("Add package"));
    m_pActionRemove->setText(tr("Remove package"));
}

/* Handle current-item change fact: */
void UIGlobalSettingsExtension::sltHandleCurrentItemChange(QTreeWidgetItem *pCurrentItem)
{
    /* Check action's availability: */
    //m_pActionAdd->setEnabled(true);
    m_pActionRemove->setEnabled(pCurrentItem);
}

/* Invoke context menu: */
void UIGlobalSettingsExtension::sltShowContextMenu(const QPoint &position)
{
    QMenu menu;
    if (m_pPackagesTree->itemAt(position))
    {
        menu.addAction(m_pActionAdd);
        menu.addAction(m_pActionRemove);
    }
    else
    {
        menu.addAction(m_pActionAdd);
    }
    menu.exec(m_pPackagesTree->viewport()->mapToGlobal(position));
}

/* Package add procedure: */
void UIGlobalSettingsExtension::sltInstallPackage()
{
    /* Open file-open window to let user to choose package file: */
    QString strFilePath;
    QString strBaseFolder = vboxGlobal().virtualBox().GetHomeFolder();
    QString strTitle = tr("Select an extension package file");
    QStringList extensions;
    for (int i = 0; i < VBoxDefs::VBoxExtPackFileExts.size(); ++i)
        extensions << QString("*.%1").arg(VBoxDefs::VBoxExtPackFileExts[i]);
    QString strFilter = tr("Extension package files (%1)").arg(extensions.join(" "));

    /* Create open file dialog: */
    QStringList fileNames = QIFileDialog::getOpenFileNames(strBaseFolder, strFilter, this, strTitle, 0, true, true);
    if (!fileNames.isEmpty())
        strFilePath = fileNames.at(0);

    /*
     * Install chosen package.
     */
    if (!strFilePath.isEmpty())
    {
        QString strExtPackName;
        if (doInstallation(strFilePath, this, &strExtPackName))
        {
            /* Insert the fresly installed extension pack, mark it as
             * current in the tree and sort by name (col 1): */
            CExtPackManager manager = vboxGlobal().virtualBox().GetExtensionPackManager();
            const CExtPack &package = manager.Find(strExtPackName);
            m_cache.m_items << fetchData(package);
            UIExtensionPackageItem *pItem = new UIExtensionPackageItem(m_pPackagesTree, m_cache.m_items.last());
            m_pPackagesTree->setCurrentItem(pItem);
            m_pPackagesTree->sortByColumn(1, Qt::AscendingOrder);
        }
    }
}

/* Package remove procedure: */
void UIGlobalSettingsExtension::sltRemovePackage()
{
    /* Get current item: */
    UIExtensionPackageItem *pItem = m_pPackagesTree &&
                                    m_pPackagesTree->currentItem() &&
                                    m_pPackagesTree->currentItem()->type() == UIExtensionPackageItem::UIItemType ?
                                    static_cast<UIExtensionPackageItem*>(m_pPackagesTree->currentItem()) : 0;

    /* Uninstall chosen package: */
    if (pItem)
    {
        /* Get name of current package: */
        QString strSelectedPackageName = pItem->name();
        /* Ask the user about package removing: */
        if (vboxProblem().confirmRemovingPackage(strSelectedPackageName, this))
        {
            /* Get package manager: */
            CExtPackManager manager = vboxGlobal().virtualBox().GetExtensionPackManager();
            /* Uninstall package: */
            manager.Uninstall(strSelectedPackageName, false /* forced removal? */);
            if (manager.isOk())
            {
                /* Remove selected package from cache: */
                for (int i = 0; i < m_cache.m_items.size(); ++i)
                {
                    if (m_cache.m_items[i].m_strName == strSelectedPackageName)
                    {
                        m_cache.m_items.removeAt(i);
                        break;
                    }
                }
                /* Remove selected package from tree: */
                delete pItem;
            }
            else vboxProblem().cannotUninstallExtPack(strSelectedPackageName, manager, this);
        }
    }
}

UISettingsCacheGlobalExtensionItem UIGlobalSettingsExtension::fetchData(const CExtPack &package) const
{
    UISettingsCacheGlobalExtensionItem item;
    item.m_strName = package.GetName();
    item.m_strDescription = package.GetDescription();
    item.m_strVersion = package.GetVersion();
    item.m_strRevision = package.GetRevision();
    item.m_fIsUsable = package.GetUsable();
    if (!item.m_fIsUsable)
        item.m_strWhyUnusable = package.GetWhyUnusable();
    return item;
}

