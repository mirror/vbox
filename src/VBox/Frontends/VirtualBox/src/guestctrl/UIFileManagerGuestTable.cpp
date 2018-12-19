/* $Id$ */
/** @file
 * VBox Qt GUI - UIFileManagerGuestTable class implementation.
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
# include <QDateTime>
# include <QFileInfo>
# include <QUuid>

/* GUI includes: */
# include "QILabel.h"
# include "UIActionPool.h"
# include "UIErrorString.h"
# include "UIFileManager.h"
# include "UIFileManagerGuestTable.h"
# include "UIMessageCenter.h"
# include "UIToolBar.h"

/* COM includes: */
# include "CFsObjInfo.h"
# include "CGuestFsObjInfo.h"
# include "CGuestDirectory.h"
# include "CProgress.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   UIGuestDirectoryDiskUsageComputer definition.                                                                                *
*********************************************************************************************************************************/

/** Open directories recursively and sum the disk usage. Don't block the GUI thread while doing this */
class UIGuestDirectoryDiskUsageComputer : public UIDirectoryDiskUsageComputer
{
    Q_OBJECT;

public:

    UIGuestDirectoryDiskUsageComputer(QObject *parent, QStringList strStartPath, const CGuestSession &session);

protected:

    virtual void run() /* override */;
    virtual void directoryStatisticsRecursive(const QString &path, UIDirectoryStatistics &statistics) /* override */;

private:

    CGuestSession m_comGuestSession;
};


/*********************************************************************************************************************************
*   UIGuestDirectoryDiskUsageComputer implementation.                                                                            *
*********************************************************************************************************************************/

UIGuestDirectoryDiskUsageComputer::UIGuestDirectoryDiskUsageComputer(QObject *parent, QStringList pathList, const CGuestSession &session)
    :UIDirectoryDiskUsageComputer(parent, pathList)
    , m_comGuestSession(session)
{
}

void UIGuestDirectoryDiskUsageComputer::run()
{
    /* Initialize COM: */
    COMBase::InitializeCOM(false);
    UIDirectoryDiskUsageComputer::run();
    /* Cleanup COM: */
    COMBase::CleanupCOM();
}

void UIGuestDirectoryDiskUsageComputer::directoryStatisticsRecursive(const QString &path, UIDirectoryStatistics &statistics)
{
    if (m_comGuestSession.isNull())
        return;
    /* Prevent modification of the continue flag while reading: */
    m_mutex.lock();
    /* Check if m_fOkToContinue is set to false. if so just end recursion: */
    if (!isOkToContinue())
    {
        m_mutex.unlock();
        return;
    }
    m_mutex.unlock();

    CGuestFsObjInfo fileInfo = m_comGuestSession.FsObjQueryInfo(path, true);

    if (!m_comGuestSession.isOk())
        return;
    /* if the object is a file or a symlink then read the size and return: */
    if (fileInfo.GetType() == KFsObjType_File)
    {
        statistics.m_totalSize += fileInfo.GetObjectSize();
        ++statistics.m_uFileCount;
        sigResultUpdated(statistics);
        return;
    }
    else if (fileInfo.GetType() == KFsObjType_Symlink)
    {
        statistics.m_totalSize += fileInfo.GetObjectSize();
        ++statistics.m_uSymlinkCount;
        sigResultUpdated(statistics);
        return;
    }

    if (fileInfo.GetType() != KFsObjType_Directory)
        return;
    /* Open the directory to start reading its content: */
    QVector<KDirectoryOpenFlag> flag(KDirectoryOpenFlag_None);
    CGuestDirectory directory = m_comGuestSession.DirectoryOpen(path, /*aFilter*/ "", flag);
    if (!m_comGuestSession.isOk())
        return;

    if (directory.isOk())
    {
        CFsObjInfo fsInfo = directory.Read();
        while (fsInfo.isOk())
        {
            if (fsInfo.GetType() == KFsObjType_File)
                statistics.m_uFileCount++;
            else if (fsInfo.GetType() == KFsObjType_Symlink)
                statistics.m_uSymlinkCount++;
            else if(fsInfo.GetType() == KFsObjType_Directory)
            {
                QString dirPath = UIPathOperations::mergePaths(path, fsInfo.GetName());
                directoryStatisticsRecursive(dirPath, statistics);
            }
        }
    }
    sigResultUpdated(statistics);
}

UIFileManagerGuestTable::UIFileManagerGuestTable(UIActionPool *pActionPool, QWidget *pParent /*= 0*/)
    :UIFileManagerTable(pActionPool, pParent)
{
    prepareToolbar();
    prepareActionConnections();
    retranslateUi();
}

void UIFileManagerGuestTable::initGuestFileTable(const CGuestSession &session)
{
    if (!session.isOk())
        return;
    if (session.GetStatus() != KGuestSessionStatus_Started)
        return;
    m_comGuestSession = session;
    initializeFileTree();
}

void UIFileManagerGuestTable::retranslateUi()
{
    if (m_pLocationLabel)
        m_pLocationLabel->setText(UIFileManager::tr("Guest System"));
    UIFileManagerTable::retranslateUi();
}

void UIFileManagerGuestTable::readDirectory(const QString& strPath,
                                     UIFileTableItem *parent, bool isStartDir /*= false*/)

{
    if (!parent)
        return;

    CGuestDirectory directory;
    QVector<KDirectoryOpenFlag> flag;
    flag.push_back(KDirectoryOpenFlag_None);

    directory = m_comGuestSession.DirectoryOpen(UIPathOperations::sanitize(strPath), /*aFilter*/ "", flag);
    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), FileManagerLogType_Error);
        return;
    }

    parent->setIsOpened(true);
    if (directory.isOk())
    {
        CFsObjInfo fsInfo = directory.Read();
        QMap<QString, UIFileTableItem*> directories;
        QMap<QString, UIFileTableItem*> files;

        while (fsInfo.isOk())
        {
            QVector<QVariant> data;
            QDateTime changeTime = QDateTime::fromMSecsSinceEpoch(fsInfo.GetChangeTime()/RT_NS_1MS);

            data << fsInfo.GetName() << static_cast<qulonglong>(fsInfo.GetObjectSize())
                 << changeTime << fsInfo.GetUserName() << permissionString(fsInfo);

            KFsObjType fsObjectType = fileType(fsInfo);
            UIFileTableItem *item = new UIFileTableItem(data, parent, fsObjectType);

            if (!item)
                continue;
            item->setPath(UIPathOperations::mergePaths(strPath, fsInfo.GetName()));
            if (fsObjectType == KFsObjType_Directory)
            {
                directories.insert(fsInfo.GetName(), item);
                item->setIsOpened(false);
            }
            else if(fsObjectType == KFsObjType_File)
            {
                files.insert(fsInfo.GetName(), item);
                item->setIsOpened(false);
            }
            else if(fsObjectType == KFsObjType_Symlink)
            {
                files.insert(fsInfo.GetName(), item);
                item->setIsOpened(false);
                /* @todo. We will need to wait a fully implemented SymlinkRead function
                 * to be able to handle sym links properly: */
                // QString path = UIPathOperations::mergePaths(strPath, fsInfo.GetName());
                // QVector<KSymlinkReadFlag> aFlags;
                // printf("%s %s %s\n", qPrintable(fsInfo.GetName()), qPrintable(path),
                //        qPrintable(m_comGuestSession.SymlinkRead(path, aFlags)));
            }

            fsInfo = directory.Read();
        }
        insertItemsToTree(directories, parent, true, isStartDir);
        insertItemsToTree(files, parent, false, isStartDir);
    }
    directory.Close();
}

void UIFileManagerGuestTable::deleteByItem(UIFileTableItem *item)
{
    if (!item)
        return;
    if (item->isUpDirectory())
        return;
    QVector<KDirectoryRemoveRecFlag> flags(KDirectoryRemoveRecFlag_ContentAndDir);

    if (item->isDirectory())
    {
        m_comGuestSession.DirectoryRemoveRecursive(item->path(), flags);
    }
    else
        m_comGuestSession.FsObjRemove(item->path());
    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(QString(item->path()).append(" could not be deleted"), FileManagerLogType_Error);
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), FileManagerLogType_Error);
    }
}

void UIFileManagerGuestTable::deleteByPath(const QStringList &pathList)
{
    foreach (const QString &strPath, pathList)
    {
        CGuestFsObjInfo fileInfo = m_comGuestSession.FsObjQueryInfo(strPath, true);
        KFsObjType eType = fileType(fileInfo);
        if (eType == KFsObjType_File || eType == KFsObjType_Symlink)
        {
              m_comGuestSession.FsObjRemove(strPath);
        }
        else if (eType == KFsObjType_Directory)
        {
            QVector<KDirectoryRemoveRecFlag> flags(KDirectoryRemoveRecFlag_ContentAndDir);
            m_comGuestSession.DirectoryRemoveRecursive(strPath, flags);
        }

    }
}

void UIFileManagerGuestTable::goToHomeDirectory()
{
    if (m_comGuestSession.isNull())
        return;
    if (!m_pRootItem || m_pRootItem->childCount() <= 0)
        return;
    UIFileTableItem *startDirItem = m_pRootItem->child(0);
    if (!startDirItem)
        return;

    QString userHome = UIPathOperations::sanitize(m_comGuestSession.GetUserHome());
    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), FileManagerLogType_Error);
        return;
    }
    QStringList pathList = userHome.split(UIPathOperations::delimiter, QString::SkipEmptyParts);
    goIntoDirectory(UIPathOperations::pathTrail(userHome));
}

bool UIFileManagerGuestTable::renameItem(UIFileTableItem *item, QString newBaseName)
{

    if (!item || item->isUpDirectory() || newBaseName.isEmpty())
        return false;
    QString newPath = UIPathOperations::constructNewItemPath(item->path(), newBaseName);
    QVector<KFsObjRenameFlag> aFlags(KFsObjRenameFlag_Replace);

    m_comGuestSession.FsObjRename(item->path(), newPath, aFlags);
    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), FileManagerLogType_Error);
        return false;
    }

    item->setPath(newPath);
    return true;
}

bool UIFileManagerGuestTable::createDirectory(const QString &path, const QString &directoryName)
{
    QString newDirectoryPath = UIPathOperations::mergePaths(path, directoryName);
    QVector<KDirectoryCreateFlag> flags(KDirectoryCreateFlag_None);

    m_comGuestSession.DirectoryCreate(newDirectoryPath, 0/*aMode*/, flags);

    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(newDirectoryPath.append(" could not be created"), FileManagerLogType_Error);
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), FileManagerLogType_Error);
        return false;
    }
    emit sigLogOutput(newDirectoryPath.append(" has been created"), FileManagerLogType_Info);
    return true;
}

void UIFileManagerGuestTable::copyHostToGuest(const QStringList &hostSourcePathList,
                                        const QString &strDestination /* = QString() */)
{
    if (!checkGuestSession())
        return;
    QVector<QString> sourcePaths = hostSourcePathList.toVector();
    QVector<QString>  aFilters;
    QVector<QString>  aFlags;
    QString strDestinationPath = strDestination;
    if (strDestinationPath.isEmpty())
        strDestinationPath = currentDirectoryPath();

    if (strDestinationPath.isEmpty())
    {
        emit sigLogOutput("No destination for copy operation", FileManagerLogType_Error);
        return;
    }
    if (hostSourcePathList.empty())
    {
        emit sigLogOutput("No source for copy operation", FileManagerLogType_Error);
        return;
    }

    CProgress progress = m_comGuestSession.CopyToGuest(sourcePaths, aFilters, aFlags, strDestinationPath);
    if (!checkGuestSession())
        return;
    emit sigNewFileOperation(progress);
}

void UIFileManagerGuestTable::copyGuestToHost(const QString& hostDestinationPath)
{
    if (!checkGuestSession())
        return;
    QVector<QString> sourcePaths = selectedItemPathList().toVector();
    QVector<QString>  aFilters;
    QVector<QString>  aFlags;

    if (hostDestinationPath.isEmpty())
    {
        emit sigLogOutput("No destination for copy operation", FileManagerLogType_Error);
        return;
    }
    if (sourcePaths.empty())
    {
        emit sigLogOutput("No source for copy operation", FileManagerLogType_Error);
        return;
    }

    CProgress progress = m_comGuestSession.CopyFromGuest(sourcePaths, aFilters, aFlags, hostDestinationPath);
    if (!checkGuestSession())
        return;
    emit sigNewFileOperation(progress);
}

KFsObjType UIFileManagerGuestTable::fileType(const CFsObjInfo &fsInfo)
{
    if (fsInfo.isNull() || !fsInfo.isOk())
        return KFsObjType_Unknown;
    if (fsInfo.GetType() == KFsObjType_Directory)
         return KFsObjType_Directory;
    else if (fsInfo.GetType() == KFsObjType_File)
        return KFsObjType_File;
    else if (fsInfo.GetType() == KFsObjType_Symlink)
        return KFsObjType_Symlink;

    return KFsObjType_Unknown;
}

KFsObjType UIFileManagerGuestTable::fileType(const CGuestFsObjInfo &fsInfo)
{
    if (fsInfo.isNull() || !fsInfo.isOk())
        return KFsObjType_Unknown;
    if (fsInfo.GetType() == KFsObjType_Directory)
         return KFsObjType_Directory;
    else if (fsInfo.GetType() == KFsObjType_File)
        return KFsObjType_File;
    else if (fsInfo.GetType() == KFsObjType_Symlink)
        return KFsObjType_Symlink;

    return KFsObjType_Unknown;
}


QString UIFileManagerGuestTable::fsObjectPropertyString()
{
    QStringList selectedObjects = selectedItemPathList();
    if (selectedObjects.isEmpty())
        return QString();
    if (selectedObjects.size() == 1)
    {
        if (selectedObjects.at(0).isNull())
            return QString();

        CGuestFsObjInfo fileInfo = m_comGuestSession.FsObjQueryInfo(selectedObjects.at(0), false /*aFollowSymlinks*/);
        if (!m_comGuestSession.isOk())
        {
            emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), FileManagerLogType_Error);
            return QString();
        }

        QStringList propertyStringList;

        /* Name: */
        propertyStringList << QString("<b>Name:</b> %1<br/>").arg(UIPathOperations::getObjectName(fileInfo.GetName()));

        /* Size: */
        LONG64 size = fileInfo.GetObjectSize();
        propertyStringList << UIFileManager::tr("<b>Size:</b> %1 bytes").arg(QString::number(size));
        if (size >= UIFileManagerTable::m_iKiloByte)
            propertyStringList << QString(" (%1)<br/>").arg(humanReadableSize(size));
        else
            propertyStringList << QString("<br/>");

        /* Allocated size: */
        size = fileInfo.GetAllocatedSize();
        propertyStringList << UIFileManager::tr("<b>Allocated:</b> %1 bytes").arg(QString::number(size));
        if (size >= UIFileManagerTable::m_iKiloByte)
            propertyStringList << QString(" (%1)<br/>").arg(humanReadableSize(size));
        else
            propertyStringList << QString("<br/>");

        /* Type: */
        QString str;
        KFsObjType const enmType = fileInfo.GetType();
        switch (enmType)
        {
            case KFsObjType_Directory:  str = UIFileManager::tr("directory"); break;
            case KFsObjType_File:       str = UIFileManager::tr("file"); break;
            case KFsObjType_Symlink:    str = UIFileManager::tr("symbolic link"); break;
            case KFsObjType_DevChar:    str = UIFileManager::tr("character device"); break;
            case KFsObjType_DevBlock:   str = UIFileManager::tr("block device"); break;
            case KFsObjType_Fifo:       str = UIFileManager::tr("fifo"); break;
            case KFsObjType_Socket:     str = UIFileManager::tr("socket"); break;
            case KFsObjType_WhiteOut:   str = UIFileManager::tr("whiteout"); break;
            case KFsObjType_Unknown:    str = UIFileManager::tr("unknown"); break;
            default:                    str = UIFileManager::tr("illegal-value"); break;
        }
        propertyStringList <<  UIFileManager::tr("<b>Type:</b> %1<br/>").arg(str);

        /* INode number, device, link count: */
        propertyStringList << UIFileManager::tr("<b>INode:</b> %1<br/>").arg(fileInfo.GetNodeId());
        propertyStringList << UIFileManager::tr("<b>Device:</b> %1<br/>").arg(fileInfo.GetNodeIdDevice());  /** @todo hex */
        propertyStringList << UIFileManager::tr("<b>Hardlinks:</b> %1<br/>").arg(fileInfo.GetHardLinks());

        /* Attributes: */
        str = fileInfo.GetFileAttributes();
        if (!str.isEmpty())
        {
            int offSpace = str.indexOf(' ');
            if (offSpace < 0)
                offSpace = str.length();
            propertyStringList << UIFileManager::tr("<b>Mode:</b> %1<br/>").arg(str.left(offSpace));
            propertyStringList << UIFileManager::tr("<b>Attributes:</b> %1<br/>").arg(str.mid(offSpace + 1).trimmed());
        }

        /* Character/block device ID: */
        ULONG uDeviceNo = fileInfo.GetDeviceNumber();
        if (uDeviceNo != 0 || enmType == KFsObjType_DevChar || enmType == KFsObjType_DevBlock)
            propertyStringList << UIFileManager::tr("<b>Device ID:</b> %1<br/>").arg(uDeviceNo); /** @todo hex */

        /* Owner: */
        propertyStringList << UIFileManager::tr("<b>Owner:</b> %1 (%2)<br/>").
            arg(fileInfo.GetUserName()).arg(fileInfo.GetUID());
        propertyStringList << UIFileManager::tr("<b>Group:</b> %1 (%2)<br/>").
            arg(fileInfo.GetGroupName()).arg(fileInfo.GetGID());

        /* Timestamps: */
        propertyStringList << UIFileManager::tr("<b>Birth:</b> %1<br/>").
            arg(QDateTime::fromMSecsSinceEpoch(fileInfo.GetBirthTime() / RT_NS_1MS).toString());
        propertyStringList << UIFileManager::tr("<b>Change:</b> %1<br/>").
            arg(QDateTime::fromMSecsSinceEpoch(fileInfo.GetChangeTime() / RT_NS_1MS).toString());
        propertyStringList << UIFileManager::tr("<b>Modified:</b> %1<br/>").
            arg(QDateTime::fromMSecsSinceEpoch(fileInfo.GetModificationTime() / RT_NS_1MS).toString());
        propertyStringList << UIFileManager::tr("<b>Access:</b> %1<br/>").
            arg(QDateTime::fromMSecsSinceEpoch(fileInfo.GetAccessTime() / RT_NS_1MS).toString());

        /* Join the list elements into a single string seperated by empty string: */
        return propertyStringList.join(QString());
    }

    int fileCount = 0;
    int directoryCount = 0;
    ULONG64 totalSize = 0;

    for(int i = 0; i < selectedObjects.size(); ++i)
    {
        CGuestFsObjInfo fileInfo = m_comGuestSession.FsObjQueryInfo(selectedObjects.at(0), true);
        if (!m_comGuestSession.isOk())
        {
            emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), FileManagerLogType_Error);
            continue;
        }

        KFsObjType type = fileType(fileInfo);

        if (type == KFsObjType_File)
            ++fileCount;
        if (type == KFsObjType_Directory)
            ++directoryCount;
        totalSize += fileInfo.GetObjectSize();
    }
    QStringList propertyStringList;
    propertyStringList << UIFileManager::tr("<b>Selected:</b> %1 files and %2 directories<br/>").
        arg(QString::number(fileCount)).arg(QString::number(directoryCount));
    propertyStringList << UIFileManager::tr("<b>Size (non-recursive):</b> %1 bytes").arg(QString::number(totalSize));
    if (totalSize >= m_iKiloByte)
        propertyStringList << QString(" (%1)").arg(humanReadableSize(totalSize));

    return propertyStringList.join(QString());;
}

void UIFileManagerGuestTable::showProperties()
{
    if (m_comGuestSession.isNull())
        return;
    QString fsPropertyString = fsObjectPropertyString();
    if (fsPropertyString.isEmpty())
        return;

    m_pPropertiesDialog = new UIPropertiesDialog();
    if (!m_pPropertiesDialog)
        return;

    QStringList selectedObjects = selectedItemPathList();
    if (selectedObjects.size() == 0)
        return;
    //UIGuestDirectoryDiskUsageComputer *directoryThread = 0;

    /* if the selection include a directory or it is a multiple selection the create a worker thread
       to compute total size of the selection (recusively) */
    // bool createWorkerThread = (selectedObjects.size() > 1);
    // if (!createWorkerThread &&
    //     fileType(m_comGuestSession.FsObjQueryInfo(selectedObjects[0], true)) == KFsObjType_Directory)
    //     createWorkerThread = true;
    // if (createWorkerThread)
    // {
    //     directoryThread = new UIGuestDirectoryDiskUsageComputer(this, selectedObjects, m_comGuestSession);
    //     if (directoryThread)
    //     {
    //         connect(directoryThread, &UIGuestDirectoryDiskUsageComputer::sigResultUpdated,
    //                 this, &UIFileManagerGuestTable::sltReceiveDirectoryStatistics/*, Qt::DirectConnection*/);
    //         directoryThread->start();
    //     }
    // }

    m_pPropertiesDialog->setWindowTitle(UIFileManager::tr("Properties"));
    m_pPropertiesDialog->setPropertyText(fsPropertyString);
    m_pPropertiesDialog->execute();

    // if (directoryThread)
    // {
    //     if (directoryThread->isRunning())
    //         directoryThread->stopRecursion();
    //     disconnect(directoryThread, &UIGuestDirectoryDiskUsageComputer::sigResultUpdated,
    //                this, &UIFileManagerGuestTable::sltReceiveDirectoryStatistics/*, Qt::DirectConnection*/);
    // }


    delete m_pPropertiesDialog;
    m_pPropertiesDialog = 0;

}

void UIFileManagerGuestTable::determineDriveLetters()
{
    if (m_comGuestSession.isNull())
        return;
    KPathStyle pathStyle = m_comGuestSession.GetPathStyle();
    if (pathStyle != KPathStyle_DOS)
        return;

    /** @todo Currently API lacks a way to query windows drive letters.
     *  so we enumarate them by using CGuestSession::DirectoryExists() */
    m_driveLetterList.clear();
    for (int i = 'A'; i <= 'Z'; ++i)
    {
        QString path((char)i);
        path += ":/";
        bool exists = m_comGuestSession.DirectoryExists(path, false /* aFollowSymlinks */);
        if (exists)
            m_driveLetterList.push_back(path);
    }
}

void UIFileManagerGuestTable::prepareToolbar()
{
    if (m_pToolBar && m_pActionPool)
    {
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_GoUp));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_GoHome));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Refresh));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Delete));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Rename));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_CreateNewDirectory));

        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Copy));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Cut));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Paste));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_SelectAll));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_InvertSelection));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_ShowProperties));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Delete));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Rename));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Copy));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Cut));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_ShowProperties));

        /* Hide these actions for now until we have a suitable guest-to-guest copy function: */
        m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Copy)->setVisible(false);
        m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Cut)->setVisible(false);
        m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Paste)->setVisible(false);
    }
    setSelectionDependentActionsEnabled(false);
    setPasteActionEnabled(false);
}

void UIFileManagerGuestTable::createFileViewContextMenu(const QWidget *pWidget, const QPoint &point)
{
    if (!pWidget)
        return;

    QMenu menu;
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_GoUp));

    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_GoHome));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Refresh));
    menu.addSeparator();
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Delete));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Rename));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_CreateNewDirectory));
    menu.addSeparator();
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Copy));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Cut));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Paste));
    menu.addSeparator();
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_SelectAll));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_InvertSelection));
    menu.addSeparator();
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_ShowProperties));
    menu.exec(pWidget->mapToGlobal(point));
}

void UIFileManagerGuestTable::setPasteActionEnabled(bool fEnabled)
{
    m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Paste)->setEnabled(fEnabled);
}

void UIFileManagerGuestTable::pasteCutCopiedObjects()
{
    /** Wait until we have a API function that would take multiple source objects
     *  and return a single IProgress instance: */
    // QVector<KFileCopyFlag> fileCopyFlags;
    // QVector<KDirectoryCopyFlag> directoryCopyFlags;

    // foreach (const QString &strPath, m_copyCutBuffer)
    // {

    // }
}


void UIFileManagerGuestTable::prepareActionConnections()
{
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_GoUp), &QAction::triggered,
            this, &UIFileManagerTable::sltGoUp);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_GoHome), &QAction::triggered,
            this, &UIFileManagerTable::sltGoHome);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Refresh), &QAction::triggered,
            this, &UIFileManagerTable::sltRefresh);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Delete), &QAction::triggered,
            this, &UIFileManagerTable::sltDelete);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Rename), &QAction::triggered,
            this, &UIFileManagerTable::sltRename);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Copy), &QAction::triggered,
            this, &UIFileManagerTable::sltCopy);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Cut), &QAction::triggered,
            this, &UIFileManagerTable::sltCut);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Paste), &QAction::triggered,
            this, &UIFileManagerTable::sltPaste);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_SelectAll), &QAction::triggered,
            this, &UIFileManagerTable::sltSelectAll);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_InvertSelection), &QAction::triggered,
            this, &UIFileManagerTable::sltInvertSelection);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_ShowProperties), &QAction::triggered,
            this, &UIFileManagerTable::sltShowProperties);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_CreateNewDirectory), &QAction::triggered,
            this, &UIFileManagerTable::sltCreateNewDirectory);
}

bool UIFileManagerGuestTable::checkGuestSession()
{
    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), FileManagerLogType_Error);
        return false;
    }
    return true;
}

QString UIFileManagerGuestTable::permissionString(const CFsObjInfo &fsInfo)
{
    /* Attributes: */
    QString strAttributes = fsInfo.GetFileAttributes();

    if (strAttributes.isEmpty())
        return strAttributes;

    int offSpace = strAttributes.indexOf(' ');
    if (offSpace < 0)
        offSpace = strAttributes.length();
    return strAttributes.left(offSpace);
}

#include "UIFileManagerGuestTable.moc"
