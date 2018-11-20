/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlFileTable class implementation.
 */

/*
 * Copyright (C) 2016-2018 Oracle Corporation
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

/* Qt includes: */
# include <QAction>
# include <QDateTime>
# include <QDir>

/* GUI includes: */
# include "QILabel.h"
# include "UIActionPool.h"
# include "UIGuestControlFileModel.h"
# include "UIHostFileTable.h"
# include "UIToolBar.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   UIHostDirectoryDiskUsageComputer definition.                                                                                 *
*********************************************************************************************************************************/

/** Open directories recursively and sum the disk usage. Don't block the GUI thread while doing this */
class UIHostDirectoryDiskUsageComputer : public UIDirectoryDiskUsageComputer
{
    Q_OBJECT;

public:

    UIHostDirectoryDiskUsageComputer(QObject *parent, QStringList strStartPath);

protected:

    virtual void directoryStatisticsRecursive(const QString &path, UIDirectoryStatistics &statistics) /* override */;
};


/*********************************************************************************************************************************
*   UIHostDirectoryDiskUsageComputer implementation.                                                                             *
*********************************************************************************************************************************/

UIHostDirectoryDiskUsageComputer::UIHostDirectoryDiskUsageComputer(QObject *parent, QStringList pathList)
    :UIDirectoryDiskUsageComputer(parent, pathList)
{
}

void UIHostDirectoryDiskUsageComputer::directoryStatisticsRecursive(const QString &path, UIDirectoryStatistics &statistics)
{
    /* Prevent modification of the continue flag while reading: */
    m_mutex.lock();
    /* Check if m_fOkToContinue is set to false. if so just end recursion: */
    if (!isOkToContinue())
    {
        m_mutex.unlock();
        return;
    }
    m_mutex.unlock();

    QFileInfo fileInfo(path);
    if (!fileInfo.exists())
        return;
    /* if the object is a file or a symlink then read the size and return: */
    if (fileInfo.isFile())
    {
        statistics.m_totalSize += fileInfo.size();
        ++statistics.m_uFileCount;
        sigResultUpdated(statistics);
        return;
    }
    else if (fileInfo.isSymLink())
    {
        statistics.m_totalSize += fileInfo.size();
        ++statistics.m_uSymlinkCount;
        sigResultUpdated(statistics);
        return;
    }

    /* if it is a directory then read the content: */
    QDir dir(path);
    if (!dir.exists())
        return;

    QFileInfoList entryList = dir.entryInfoList();
    for (int i = 0; i < entryList.size(); ++i)
    {
        const QFileInfo &entryInfo = entryList.at(i);
        if (entryInfo.baseName().isEmpty() || entryInfo.baseName() == "." ||
            entryInfo.baseName() == UIGuestControlFileModel::strUpDirectoryString)
            continue;
        statistics.m_totalSize += entryInfo.size();
        if (entryInfo.isSymLink())
            statistics.m_uSymlinkCount++;
        else if(entryInfo.isFile())
            statistics.m_uFileCount++;
        else if (entryInfo.isDir())
        {
            statistics.m_uDirectoryCount++;
            directoryStatisticsRecursive(entryInfo.absoluteFilePath(), statistics);
        }
    }
    sigResultUpdated(statistics);
}


/*********************************************************************************************************************************
*   UIHostFileTable implementation.                                                                                              *
*********************************************************************************************************************************/

UIHostFileTable::UIHostFileTable(UIActionPool *pActionPool, QWidget *pParent /* = 0 */)
    :UIGuestControlFileTable(pActionPool, pParent)
{
    initializeFileTree();
    prepareToolbar();
    prepareActionConnections();
    retranslateUi();
}

void UIHostFileTable::updateDeleteAfterCopyCache(const QUuid &progressId, const QStringList &sourceObjectsList)
{
    m_deleteAfterCopyCache[progressId] = sourceObjectsList;
}

void UIHostFileTable::retranslateUi()
{
    if (m_pLocationLabel)
        m_pLocationLabel->setText(QApplication::translate("UIGuestControlFileManager", "Host System"));
    UIGuestControlFileTable::retranslateUi();
}

void UIHostFileTable::prepareToolbar()
{
    if (m_pToolBar && m_pActionPool)
    {
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_GoUp));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_GoHome));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Refresh));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Delete));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Rename));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_CreateNewDirectory));
        /* Hide cut, copy, and paste for now. We will implement those
           when we have an API for host file operations: */
        // m_pToolBar->addSeparator();
        // m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Copy));
        // m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Cut));
        // m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Paste));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_SelectAll));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_InvertSelection));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_ShowProperties));

        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Delete));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Rename));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_ShowProperties));
    }

    setSelectionDependentActionsEnabled(false);
}

void UIHostFileTable::createFileViewContextMenu(const QWidget *pWidget, const QPoint &point)
{
    if (!pWidget)
        return;

    QMenu menu;
    menu.addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_GoUp));

    menu.addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_GoHome));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Refresh));
    menu.addSeparator();
    menu.addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Delete));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Rename));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_CreateNewDirectory));
    /* Hide cut, copy, and paste for now. We will implement those
       when we have an API for host file operations: */
    // menu.addSeparator();
    // menu.addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Copy));
    // menu.addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Cut));
    // menu.addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Paste));
    menu.addSeparator();
    menu.addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_SelectAll));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_InvertSelection));
    menu.addSeparator();
    menu.addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_ShowProperties));



    menu.exec(pWidget->mapToGlobal(point));
}

void UIHostFileTable::readDirectory(const QString& strPath, UIFileTableItem *parent, bool isStartDir /*= false*/)
{
    if (!parent)
        return;

    QDir directory(strPath);
    //directory.setFilter(QDir::NoDotAndDotDot);
    parent->setIsOpened(true);
    if (!directory.exists())
        return;
    QFileInfoList entries = directory.entryInfoList();
    QMap<QString, UIFileTableItem*> directories;
    QMap<QString, UIFileTableItem*> files;

    for (int i = 0; i < entries.size(); ++i)
    {

        const QFileInfo &fileInfo = entries.at(i);
        UIFileTableItem *item = new UIFileTableItem(createTreeItemData(fileInfo.fileName(), fileInfo.size(),
                                                                       fileInfo.lastModified(), fileInfo.owner(),
                                                                       permissionString(fileInfo.permissions())),
                                                    parent, fileType(fileInfo));
        if (!item)
            continue;
        item->setPath(fileInfo.absoluteFilePath());
        /* if the item is a symlink set the target path and
           check the target if it is a directory: */
        if (fileInfo.isSymLink())
        {
            item->setTargetPath(fileInfo.symLinkTarget());
            item->setIsTargetADirectory(QFileInfo(fileInfo.symLinkTarget()).isDir());
        }
        if (fileInfo.isDir())
        {
            directories.insert(fileInfo.fileName(), item);
            item->setIsOpened(false);
        }
        else
        {
            files.insert(fileInfo.fileName(), item);
            item->setIsOpened(false);
        }
    }
    insertItemsToTree(directories, parent, true, isStartDir);
    insertItemsToTree(files, parent, false, isStartDir);
    //updateCurrentLocationEdit(strPath);
}

void UIHostFileTable::deleteByItem(UIFileTableItem *item)
{
    if (item->isUpDirectory())
        return;
    if (!item->isDirectory())
    {
        QDir itemToDelete;
        itemToDelete.remove(item->path());
    }
    QDir itemToDelete(item->path());
    itemToDelete.setFilter(QDir::NoDotAndDotDot);
    /* Try to delete item recursively (in case of directories).
       note that this is no good way of deleting big directory
       trees. We need a better error reporting and a kind of progress
       indicator: */
    /** @todo replace this recursive delete by a better implementation: */
    bool deleteSuccess = itemToDelete.removeRecursively();

     if (!deleteSuccess)
         emit sigLogOutput(QString(item->path()).append(" could not be deleted"), FileManagerLogType_Error);
}

void UIHostFileTable::deleteByPath(const QStringList &pathList)
{
    foreach (const QString &strPath, pathList)
    {
        bool deleteSuccess = true;
        FileObjectType eType = fileType(QFileInfo(strPath));
        if (eType == FileObjectType_File || eType == FileObjectType_SymLink)
        {
            deleteSuccess = QDir().remove(strPath);
        }
        else if (eType == FileObjectType_Directory)
        {
            QDir itemToDelete(strPath);
            itemToDelete.setFilter(QDir::NoDotAndDotDot);
            deleteSuccess = itemToDelete.removeRecursively();
        }
        if (!deleteSuccess)
            emit sigLogOutput(QString(strPath).append(" could not be deleted"), FileManagerLogType_Error);
    }
}

void UIHostFileTable::goToHomeDirectory()
{
    if (!m_pRootItem || m_pRootItem->childCount() <= 0)
        return;
    UIFileTableItem *startDirItem = m_pRootItem->child(0);
    if (!startDirItem)
        return;

    QString userHome = UIPathOperations::sanitize(QDir::homePath());
    goIntoDirectory(UIPathOperations::pathTrail(userHome));
}

bool UIHostFileTable::renameItem(UIFileTableItem *item, QString newBaseName)
{
    if (!item || item->isUpDirectory() || newBaseName.isEmpty())
        return false;
    QString newPath = UIPathOperations::constructNewItemPath(item->path(), newBaseName);
    QDir tempDir;
    if (tempDir.rename(item->path(), newPath))
    {
        item->setPath(newPath);
        return true;
    }
    return false;
}

bool UIHostFileTable::createDirectory(const QString &path, const QString &directoryName)
{
    QDir parentDir(path);
    if (!parentDir.mkdir(directoryName))
    {
        emit sigLogOutput(UIPathOperations::mergePaths(path, directoryName).append(" could not be created"), FileManagerLogType_Error);
        return false;
    }

    return true;
}

FileObjectType UIHostFileTable::fileType(const QFileInfo &fsInfo)
{
    if (!fsInfo.exists())
        return FileObjectType_Unknown;
    /* first check if it is symlink becacuse for Qt
       being smylin and directory/file is not mutually exclusive: */
    if (fsInfo.isSymLink())
        return FileObjectType_SymLink;
    else if (fsInfo.isFile())
        return FileObjectType_File;
    else if (fsInfo.isDir())
        return FileObjectType_Directory;

    return FileObjectType_Other;
}

QString UIHostFileTable::fsObjectPropertyString()
{
    QStringList selectedObjects = selectedItemPathList();
    if (selectedObjects.isEmpty())
        return QString();
    if (selectedObjects.size() == 1)
    {
        if (selectedObjects.at(0).isNull())
            return QString();
        QFileInfo fileInfo(selectedObjects.at(0));
        if (!fileInfo.exists())
            return QString();
        QString propertyString;
        /* Name: */
        propertyString += "<b>Name:</b> " + fileInfo.fileName() + "\n";
        if (!fileInfo.suffix().isEmpty())
            propertyString += "." + fileInfo.suffix();
        propertyString += "<br/>";
        /* Size: */
        propertyString += "<b>Size:</b> " + QString::number(fileInfo.size()) + QString(" bytes");
        if (fileInfo.size() >= m_iKiloByte)
            propertyString += " (" + humanReadableSize(fileInfo.size()) + ")";
        propertyString += "<br/>";
        /* Type: */
        propertyString += "<b>Type:</b> " + fileTypeString(fileType(fileInfo));
        propertyString += "<br/>";
        /* Creation Date: */
        propertyString += "<b>Created:</b> " + fileInfo.created().toString();
        propertyString += "<br/>";
        /* Last Modification Date: */
        propertyString += "<b>Modified:</b> " + fileInfo.lastModified().toString();
        propertyString += "<br/>";
        /* Owner: */
        propertyString += "<b>Owner:</b> " + fileInfo.owner();

        return propertyString;
    }

    int fileCount = 0;
    int directoryCount = 0;
    ULONG64 totalSize = 0;

    for(int i = 0; i < selectedObjects.size(); ++i)
    {
        QFileInfo fileInfo(selectedObjects.at(i));
        if (!fileInfo.exists())
            continue;
        if (fileInfo.isFile())
            ++fileCount;
        if (fileInfo.isDir())
            ++directoryCount;
        totalSize += fileInfo.size();
    }
    QString propertyString;
    propertyString += "<b>Selected:</b> " + QString::number(fileCount) + " files ";
    propertyString += "and " + QString::number(directoryCount) + " directories";
    propertyString += "<br/>";
    propertyString += "<b>Size:</b> " + QString::number(totalSize) + QString(" bytes");
    if (totalSize >= m_iKiloByte)
        propertyString += " (" + humanReadableSize(totalSize) + ")";

    return propertyString;
}

void  UIHostFileTable::showProperties()
{
    qRegisterMetaType<UIDirectoryStatistics>();
    QString fsPropertyString = fsObjectPropertyString();
    if (fsPropertyString.isEmpty())
        return;
    if (!m_pPropertiesDialog)
        m_pPropertiesDialog = new UIPropertiesDialog(this);
    if (!m_pPropertiesDialog)
        return;

    UIHostDirectoryDiskUsageComputer *directoryThread = 0;

    QStringList selectedObjects = selectedItemPathList();
    if ((selectedObjects.size() == 1 && QFileInfo(selectedObjects.at(0)).isDir())
        || selectedObjects.size() > 1)
    {
        directoryThread = new UIHostDirectoryDiskUsageComputer(this, selectedObjects);
        if (directoryThread)
        {
            connect(directoryThread, &UIHostDirectoryDiskUsageComputer::sigResultUpdated,
                    this, &UIHostFileTable::sltReceiveDirectoryStatistics/*, Qt::DirectConnection*/);
            directoryThread->start();
        }
    }
    m_pPropertiesDialog->setWindowTitle("Properties");
    m_pPropertiesDialog->setPropertyText(fsPropertyString);
    m_pPropertiesDialog->execute();
    if (directoryThread)
    {
        if (directoryThread->isRunning())
            directoryThread->stopRecursion();
        disconnect(directoryThread, &UIHostDirectoryDiskUsageComputer::sigResultUpdated,
                this, &UIHostFileTable::sltReceiveDirectoryStatistics/*, Qt::DirectConnection*/);
        directoryThread->wait();
    }
}

void UIHostFileTable::determineDriveLetters()
{
    QFileInfoList drive = QDir::drives();
    m_driveLetterList.clear();
    for (int i = 0; i < drive.size(); ++i)
    {
        if (UIPathOperations::doesPathStartWithDriveLetter(drive[i].filePath()))
            m_driveLetterList.push_back(drive[i].filePath());

    }
}

QString UIHostFileTable::permissionString(QFileDevice::Permissions permissions)
{
    QString strPermissions;
    if (permissions & QFileDevice::ReadOwner)
        strPermissions += 'r';
    else
        strPermissions += '-';

    if (permissions & QFileDevice::WriteOwner)
        strPermissions += 'w';
    else
        strPermissions += '-';

    if (permissions & QFileDevice::ExeOwner)
        strPermissions += 'x';
    else
        strPermissions += '-';

    if (permissions & QFileDevice::ReadGroup)
        strPermissions += 'r';
    else
        strPermissions += '-';

    if (permissions & QFileDevice::WriteGroup)
        strPermissions += 'w';
    else
        strPermissions += '-';

    if (permissions & QFileDevice::ExeGroup)
        strPermissions += 'x';
    else
        strPermissions += '-';

    if (permissions & QFileDevice::ReadOther)
        strPermissions += 'r';
    else
        strPermissions += '-';

    if (permissions & QFileDevice::WriteOther)
        strPermissions += 'w';
    else
        strPermissions += '-';

    if (permissions & QFileDevice::ExeOther)
        strPermissions += 'x';
    else
        strPermissions += '-';
    return strPermissions;
}

void UIHostFileTable::prepareActionConnections()
{
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_GoUp), &QAction::triggered,
            this, &UIGuestControlFileTable::sltGoUp);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_GoHome), &QAction::triggered,
            this, &UIGuestControlFileTable::sltGoHome);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Refresh), &QAction::triggered,
            this, &UIGuestControlFileTable::sltRefresh);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Delete), &QAction::triggered,
            this, &UIGuestControlFileTable::sltDelete);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Rename), &QAction::triggered,
            this, &UIGuestControlFileTable::sltRename);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Copy), &QAction::triggered,
            this, &UIGuestControlFileTable::sltCopy);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Cut), &QAction::triggered,
            this, &UIGuestControlFileTable::sltCut);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_Paste), &QAction::triggered,
            this, &UIGuestControlFileTable::sltPaste);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_SelectAll), &QAction::triggered,
            this, &UIGuestControlFileTable::sltSelectAll);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_InvertSelection), &QAction::triggered,
            this, &UIGuestControlFileTable::sltInvertSelection);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_ShowProperties), &QAction::triggered,
            this, &UIGuestControlFileTable::sltShowProperties);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Host_CreateNewDirectory), &QAction::triggered,
            this, &UIGuestControlFileTable::sltCreateNewDirectory);
}

#include "UIHostFileTable.moc"
