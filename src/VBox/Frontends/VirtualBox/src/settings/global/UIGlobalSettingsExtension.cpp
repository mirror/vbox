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
#include "UIMessageCenter.h"
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

        /* Version, Revision, Edition: */
        QString strVersion(m_data.m_strVersion.section(QRegExp("[-_]"), 0, 0));
        QString strAppend;
        /* workaround for http://qt.gitorious.org/qt/qt/commit/7fc63dd0ff368a637dcd17e692b9d6b26278b538this */
        if (m_data.m_strVersion.contains(QRegExp("[-_]")))
            strAppend = m_data.m_strVersion.section(QRegExp("[-_]"), 1, -1, QString::SectionIncludeLeadingSep);
        setText(2, QString("%1r%2%3").arg(strVersion).arg(m_data.m_strRevision).arg(strAppend));

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
 * This code is shared by UIGlobalSettingsExtension::sltInstallPackage and UISelectorWindow::sltOpenUrls.
 *
 * @param   strFilePath     The path to the tarball.
 * @param   pParent         The parent widget.
 * @param   pstrExtPackName Where to return the extension pack name. Optional.
 */
/*static*/ void UIGlobalSettingsExtension::doInstallation(QString const &strFilePath, QWidget *pParent, QString *pstrExtPackName)
{
    /*
     * Open the extpack tarball via IExtPackManager.
     */
    CExtPackManager manager = vboxGlobal().virtualBox().GetExtensionPackManager();
    CExtPackFile extPackFile = manager.OpenExtPackFile(strFilePath);
    if (!manager.isOk())
    {
        msgCenter().cannotOpenExtPack(strFilePath, manager, pParent);
        return;
    }

    if (!extPackFile.GetUsable())
    {
        msgCenter().badExtPackFile(strFilePath, extPackFile, pParent);
        return;
    }

    QString strPackName = extPackFile.GetName();
    QString strPackDescription = extPackFile.GetDescription();
    QString strVersion(extPackFile.GetVersion().section(QRegExp("[-_]"), 0, 0));
    QString strAppend;
    /* workaround for http://qt.gitorious.org/qt/qt/commit/7fc63dd0ff368a637dcd17e692b9d6b26278b538this */
    if (extPackFile.GetVersion().contains(QRegExp("[-_]")))
        strAppend = extPackFile.GetVersion().section(QRegExp("[-_]"), 1, -1, QString::SectionIncludeLeadingSep);
    QString strPackVersion = QString("%1r%2%3").arg(strVersion).arg(extPackFile.GetRevision()).arg(strAppend);

    /*
     * Check if there is a version of the extension pack already
     * installed on the system and let the user decide what to do about
     * it.
     */
    CExtPack extPackCur = manager.Find(strPackName);
    bool fReplaceIt = extPackCur.isOk();
    if (fReplaceIt)
    {
        QString strVersionCur(extPackCur.GetVersion().section(QRegExp("[-_]"), 0, 0));
        QString strAppendCur;
        /* workaround for http://qt.gitorious.org/qt/qt/commit/7fc63dd0ff368a637dcd17e692b9d6b26278b538this */
        if (extPackCur.GetVersion().contains(QRegExp("[-_]")))
            strAppendCur = extPackCur.GetVersion().section(QRegExp("[-_]"), 1, -1, QString::SectionIncludeLeadingSep);
        QString strPackVersionCur = QString("%1r%2%3").arg(strVersionCur).arg(extPackCur.GetRevision()).arg(strAppendCur);
        if (!msgCenter().confirmReplacePackage(strPackName, strPackVersion, strPackVersionCur, strPackDescription, pParent))
            return;
    }
    /*
     * If it's a new package just ask for general confirmation.
     */
    else
    {
        if (!msgCenter().confirmInstallingPackage(strPackName, strPackVersion, strPackDescription, pParent))
            return;
    }

    /*
     * Display the license dialog if required by the extension pack.
     */
    if (extPackFile.GetShowLicense())
    {
        QString strLicense = extPackFile.GetLicense();
        VBoxLicenseViewer licenseViewer(pParent);
        if (licenseViewer.showLicenseFromString(strLicense) != QDialog::Accepted)
            return;
    }

    /*
     * Install the selected package.
     *
     * Set the package name return value before doing this as the caller should
     * do a refresh even on failure.
     */
    QString displayInfo;
#ifdef RT_OS_WINDOWS
    if (pParent)
        displayInfo.sprintf("hwnd=%#llx", (uint64_t)(uintptr_t)pParent->winId());
#endif
    CProgress progress = extPackFile.Install(fReplaceIt, displayInfo);
    if (extPackFile.isOk())
    {
        if (progress.isNull())
            msgCenter().notifyAboutExtPackInstalled(strPackName, pParent);
        else
        {
            msgCenter().showModalProgressDialog(progress, tr("Extensions"));
            if (!progress.GetCanceled())
            {
                if (progress.isOk() && progress.GetResultCode() == 0)
                    msgCenter().notifyAboutExtPackInstalled(strPackName, pParent);
                else
                    msgCenter().cannotInstallExtPack(strFilePath, extPackFile, progress, pParent);
            }
        }
    }
    else
        msgCenter().cannotInstallExtPack(strFilePath, extPackFile, progress, pParent);

    if (pstrExtPackName)
        *pstrExtPackName = strPackName;
}

/* Load data to cache from corresponding external object(s),
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
    /*
     * Open file-open window to let user to choose package file.
     *
     * The default location is the user's Download or Downloads directory with
     * the user's home directory as a fallback.  ExtPacks are downloaded.
     */
    QString strBaseFolder = QDir::homePath() + "/Downloads";
    if (!QDir(strBaseFolder).exists())
    {
        strBaseFolder = QDir::homePath() + "/Download";
        if (!QDir(strBaseFolder).exists())
            strBaseFolder = QDir::homePath();
    }
    QString strTitle = tr("Select an extension package file");
    QStringList extensions;
    for (int i = 0; i < VBoxDefs::VBoxExtPackFileExts.size(); ++i)
        extensions << QString("*.%1").arg(VBoxDefs::VBoxExtPackFileExts[i]);
    QString strFilter = tr("Extension package files (%1)").arg(extensions.join(" "));

    QStringList fileNames = QIFileDialog::getOpenFileNames(strBaseFolder, strFilter, this, strTitle, 0, true, true);

    QString strFilePath;
    if (!fileNames.isEmpty())
        strFilePath = fileNames.at(0);

    /*
     * Install the chosen package.
     */
    if (!strFilePath.isEmpty())
    {
        QString strExtPackName;
        doInstallation(strFilePath, this, &strExtPackName);

        /*
         * Since we might be reinstalling an existing package, we have to
         * do a little refreshing regardless of what the user chose.
         */
        if (!strExtPackName.isNull())
        {
            /* Remove it from the cache. */
            for (int i = 0; i < m_cache.m_items.size(); i++)
                if (!strExtPackName.compare(m_cache.m_items[i].m_strName, Qt::CaseInsensitive))
                {
                    m_cache.m_items.removeAt(i);
                    break;
                }

            /* Remove it from the tree. */
            const int cItems = m_pPackagesTree->topLevelItemCount();
            for (int i = 0; i < cItems; i++)
            {
                UIExtensionPackageItem *pItem = static_cast<UIExtensionPackageItem*>(m_pPackagesTree->topLevelItem(i));
                if (!strExtPackName.compare(pItem->name(), Qt::CaseInsensitive))
                {
                    delete pItem;
                    break;
                }
            }

            /* Reinsert it into the cache and tree. */
            CExtPackManager manager = vboxGlobal().virtualBox().GetExtensionPackManager();
            const CExtPack &package = manager.Find(strExtPackName);
            if (package.isOk())
            {
                m_cache.m_items << fetchData(package);

                UIExtensionPackageItem *pItem = new UIExtensionPackageItem(m_pPackagesTree, m_cache.m_items.last());
                m_pPackagesTree->setCurrentItem(pItem);
                m_pPackagesTree->sortByColumn(1, Qt::AscendingOrder);
            }
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
        if (msgCenter().confirmRemovingPackage(strSelectedPackageName, this))
        {
            /*
             * Uninstall the package.
             */
            CExtPackManager manager = vboxGlobal().virtualBox().GetExtensionPackManager();
            /** @todo Refuse this if any VMs are running. */
            QString displayInfo;
#ifdef RT_OS_WINDOWS
            displayInfo.sprintf("hwnd=%#llx", (uint64_t)(uintptr_t)this->winId());
#endif
            CProgress progress = manager.Uninstall(strSelectedPackageName, false /* forced removal? */, displayInfo);
            if (manager.isOk())
            {
                bool fOk = true;
                if (!progress.isNull())
                {
                    msgCenter().showModalProgressDialog(progress, tr("Extensions"));
                    fOk = progress.isOk() && progress.GetResultCode() == 0;
                }
                if (fOk)
                {
                    /* Remove selected package from cache: */
                    for (int i = 0; i < m_cache.m_items.size(); ++i)
                    {
                        if (!strSelectedPackageName.compare(m_cache.m_items[i].m_strName, Qt::CaseInsensitive))
                        {
                            m_cache.m_items.removeAt(i);
                            break;
                        }
                    }

                    /* Remove selected package from tree: */
                    delete pItem;
                }
                else
                    msgCenter().cannotUninstallExtPack(strSelectedPackageName, manager, progress, this);
            }
            else
                msgCenter().cannotUninstallExtPack(strSelectedPackageName, manager, progress, this);
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

