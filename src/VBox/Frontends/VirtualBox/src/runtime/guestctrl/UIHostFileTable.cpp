/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlFileTable class implementation.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
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
# include "UIHostFileTable.h"
# include "UIVMInformationDialog.h"
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

    if (!m_bContinueRunning)
        return;
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
        if (entryInfo.baseName().isEmpty() || entryInfo.baseName() == "." || entryInfo.baseName() == "..")
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

UIHostFileTable::UIHostFileTable(QWidget *pParent /* = 0 */)
    :UIGuestControlFileTable(pParent)
{
    initializeFileTree();
    retranslateUi();
    prepareActions();
}

void UIHostFileTable::retranslateUi()
{
    if (m_pLocationLabel)
        m_pLocationLabel->setText(UIVMInformationDialog::tr("Host System"));
    UIGuestControlFileTable::retranslateUi();
}

void UIHostFileTable::prepareActions()
{
    /* Hide cut, copy, and paste for now. We will implement those
       when we have an API for host file operations: */
    if (m_pCopy)
        m_pCopy->setVisible(false);
    if (m_pCut)
        m_pCut->setVisible(false);
    if (m_pPaste)
        m_pPaste->setVisible(false);
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

        QList<QVariant> data;
        data << fileInfo.fileName() << fileInfo.size()
             << fileInfo.lastModified() << fileInfo.owner();
        UIFileTableItem *item = new UIFileTableItem(data, parent, fileType(fileInfo));
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
        QDir itemToDelete;//(item->path());
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
         emit sigLogOutput(QString(item->path()).append(" could not be deleted"));
}

void UIHostFileTable::goToHomeDirectory()
{
    if (!m_pRootItem || m_pRootItem->childCount() <= 0)
        return;
    UIFileTableItem *startDirItem = m_pRootItem->child(0);
    if (!startDirItem)
        return;

    QString userHome = UIPathOperations::sanitize(QDir::homePath());
    QList<QString> pathList = userHome.split(UIPathOperations::delimiter, QString::SkipEmptyParts);
    goIntoDirectory(pathList);
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
        emit sigLogOutput(UIPathOperations::mergePaths(path, directoryName).append(" could not be created"));
        return false;
    }

    return true;
}

/* static */ FileObjectType UIHostFileTable::fileType(const QFileInfo &fsInfo)
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

    delete m_pPropertiesDialog;

    m_pPropertiesDialog = new UIPropertiesDialog();
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
        {
            directoryThread->stopRecursion();
            //directoryThread->wait();
        }
        disconnect(directoryThread, &UIHostDirectoryDiskUsageComputer::sigResultUpdated,
                this, &UIHostFileTable::sltReceiveDirectoryStatistics/*, Qt::DirectConnection*/);
        //delete directoryThread;
    }
    delete m_pPropertiesDialog;
    m_pPropertiesDialog = 0;
}

#include "UIHostFileTable.moc"
