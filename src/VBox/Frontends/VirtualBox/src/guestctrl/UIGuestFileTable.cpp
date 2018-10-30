/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestFileTable class implementation.
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
# include <QDateTime>
# include <QFileInfo>

/* GUI includes: */
# include "QILabel.h"
# include "UIActionPool.h"
# include "UIErrorString.h"
# include "UIGuestFileTable.h"
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

UIGuestFileTable::UIGuestFileTable(UIActionPool *pActionPool, QWidget *pParent /*= 0*/)
    :UIGuestControlFileTable(pActionPool, pParent)
{
    prepareToolbar();
    prepareActionConnections();
    retranslateUi();
}

void UIGuestFileTable::initGuestFileTable(const CGuestSession &session)
{
    if (!session.isOk())
        return;
    if (session.GetStatus() != KGuestSessionStatus_Started)
        return;
    m_comGuestSession = session;
    initializeFileTree();
}

void UIGuestFileTable::retranslateUi()
{
    if (m_pLocationLabel)
        m_pLocationLabel->setText(QApplication::translate("UIGuestControlFileManager", "Guest System"));
    UIGuestControlFileTable::retranslateUi();
}

void UIGuestFileTable::readDirectory(const QString& strPath,
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
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession));
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
            QList<QVariant> data;
            QDateTime changeTime = QDateTime::fromMSecsSinceEpoch(fsInfo.GetChangeTime()/1000000);

            data << fsInfo.GetName() << static_cast<qulonglong>(fsInfo.GetObjectSize())
                 << changeTime << fsInfo.GetUserName();
            FileObjectType fsObjectType = fileType(fsInfo);
            UIFileTableItem *item = new UIFileTableItem(data, parent, fsObjectType);
            if (!item)
                continue;
            item->setPath(UIPathOperations::mergePaths(strPath, fsInfo.GetName()));
            if (fsObjectType == FileObjectType_Directory)
            {
                directories.insert(fsInfo.GetName(), item);
                item->setIsOpened(false);
            }
            else if(fsObjectType == FileObjectType_File)
            {
                files.insert(fsInfo.GetName(), item);
                item->setIsOpened(false);
            }
            /** @todo Seems like our API is not able to detect symlinks: */
            else if(fsObjectType == FileObjectType_SymLink)
            {
                files.insert(fsInfo.GetName(), item);
                item->setIsOpened(false);
            }

            fsInfo = directory.Read();
        }
        insertItemsToTree(directories, parent, true, isStartDir);
        insertItemsToTree(files, parent, false, isStartDir);
        //updateCurrentLocationEdit(strPath);
    }
    directory.Close();
}

void UIGuestFileTable::deleteByItem(UIFileTableItem *item)
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
        emit sigLogOutput(QString(item->path()).append(" could not be deleted"));
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession));
    }
}

void UIGuestFileTable::goToHomeDirectory()
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
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession));
        return;
    }
    QStringList pathList = userHome.split(UIPathOperations::delimiter, QString::SkipEmptyParts);
    goIntoDirectory(UIPathOperations::pathTrail(userHome));
}

bool UIGuestFileTable::renameItem(UIFileTableItem *item, QString newBaseName)
{

    if (!item || item->isUpDirectory() || newBaseName.isEmpty())
        return false;
    QString newPath = UIPathOperations::constructNewItemPath(item->path(), newBaseName);
    QVector<KFsObjRenameFlag> aFlags(KFsObjRenameFlag_Replace);

    m_comGuestSession.FsObjRename(item->path(), newPath, aFlags);
    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession));
        return false;
    }

    item->setPath(newPath);
    return true;
}

bool UIGuestFileTable::createDirectory(const QString &path, const QString &directoryName)
{
    QString newDirectoryPath = UIPathOperations::mergePaths(path, directoryName);
    QVector<KDirectoryCreateFlag> flags(KDirectoryCreateFlag_None);

    m_comGuestSession.DirectoryCreate(newDirectoryPath, 0/*aMode*/, flags);

    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(newDirectoryPath.append(" could not be created"));
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession));
        return false;
    }
    emit sigLogOutput(newDirectoryPath.append(" has been created"));
    return true;
}

void UIGuestFileTable::copyHostToGuest(const QStringList &hostSourcePathList)
{
    QVector<QString> sourcePaths = hostSourcePathList.toVector();
    QVector<QString>  aFilters;
    QVector<QString>  aFlags;
    CProgress progress = m_comGuestSession.CopyToGuest(sourcePaths, aFilters, aFlags, currentDirectoryPath());
    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession));
        //msgCenter().cannotRemoveMachine(machine);
        return;
    }

    msgCenter().showModalProgressDialog(progress, "copying", ":/progress_delete_90px.png");
    if (!progress.isOk() || progress.GetResultCode() != 0)
    {
        emit sigLogOutput(UIErrorString::formatErrorInfo(progress));
        return;
    }

    else
        refresh();
}

void UIGuestFileTable::copyGuestToHost(const QString& hostDestinationPath)
{
    QVector<QString> sourcePaths = selectedItemPathList().toVector();
    QVector<QString>  aFilters;
    QVector<QString>  aFlags;

    CProgress progress = m_comGuestSession.CopyFromGuest(sourcePaths, aFilters, aFlags, hostDestinationPath);
    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession));
        return;
    }

    msgCenter().showModalProgressDialog(progress, "copying", ":/progress_delete_90px.png");
    if (!progress.isOk() || progress.GetResultCode() != 0)
    {
        emit sigLogOutput(UIErrorString::formatErrorInfo(progress));
        return;
    }
    else
        refresh();
}

FileObjectType UIGuestFileTable::fileType(const CFsObjInfo &fsInfo)
{
    if (fsInfo.isNull() || !fsInfo.isOk())
        return FileObjectType_Unknown;
    if (fsInfo.GetType() == KFsObjType_Directory)
         return FileObjectType_Directory;
    else if (fsInfo.GetType() == KFsObjType_File)
        return FileObjectType_File;
    else if (fsInfo.GetType() == KFsObjType_Symlink)
        return FileObjectType_SymLink;

    return FileObjectType_Other;
}

FileObjectType UIGuestFileTable::fileType(const CGuestFsObjInfo &fsInfo)
{
    if (fsInfo.isNull() || !fsInfo.isOk())
        return FileObjectType_Unknown;
    if (fsInfo.GetType() == KFsObjType_Directory)
         return FileObjectType_Directory;
    else if (fsInfo.GetType() == KFsObjType_File)
        return FileObjectType_File;
    else if (fsInfo.GetType() == KFsObjType_Symlink)
        return FileObjectType_SymLink;

    return FileObjectType_Other;
}


QString UIGuestFileTable::fsObjectPropertyString()
{
    QStringList selectedObjects = selectedItemPathList();
    if (selectedObjects.isEmpty())
        return QString();
    if (selectedObjects.size() == 1)
    {
        if (selectedObjects.at(0).isNull())
            return QString();

        CGuestFsObjInfo fileInfo = m_comGuestSession.FsObjQueryInfo(selectedObjects.at(0), true);
        if (!m_comGuestSession.isOk())
        {
            emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession));
            return QString();
        }


        QString propertyString;

        /* Name: */
        propertyString += "<b>Name:</b> " + UIPathOperations::getObjectName(fileInfo.GetName()) + "\n";
        propertyString += "<br/>";
        /* Size: */
        LONG64 size = fileInfo.GetObjectSize();
        propertyString += "<b>Size:</b> " + QString::number(size) + QString(" bytes");
        if (size >= 1024)
            propertyString += " (" + humanReadableSize(size) + ")";
        propertyString += "<br/>";
        /* Type: */
        propertyString += "<b>Type:</b> " + fileTypeString(fileType(fileInfo));
        propertyString += "<br/>";
        /* Creation Date: */
        propertyString += "<b>Created:</b> " + QDateTime::fromMSecsSinceEpoch(fileInfo.GetChangeTime()/1000000).toString();
        propertyString += "<br/>";
        /* Last Modification Date: */
        propertyString += "<b>Modified:</b> " + QDateTime::fromMSecsSinceEpoch(fileInfo.GetModificationTime()/1000000).toString();
        propertyString += "<br/>";
        /* Owner: */
        propertyString += "<b>Owner:</b> " + fileInfo.GetUserName();
        return propertyString;
    }

    int fileCount = 0;
    int directoryCount = 0;
    ULONG64 totalSize = 0;

    for(int i = 0; i < selectedObjects.size(); ++i)
    {
        CGuestFsObjInfo fileInfo = m_comGuestSession.FsObjQueryInfo(selectedObjects.at(0), true);
        if (!m_comGuestSession.isOk())
        {
            emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession));
            continue;
        }

        FileObjectType type = fileType(fileInfo);

        if (type == FileObjectType_File)
            ++fileCount;
        if (type == FileObjectType_Directory)
            ++directoryCount;
        totalSize += fileInfo.GetObjectSize();
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

void UIGuestFileTable::showProperties()
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
    //     fileType(m_comGuestSession.FsObjQueryInfo(selectedObjects[0], true)) == FileObjectType_Directory)
    //     createWorkerThread = true;
    // if (createWorkerThread)
    // {
    //     directoryThread = new UIGuestDirectoryDiskUsageComputer(this, selectedObjects, m_comGuestSession);
    //     if (directoryThread)
    //     {
    //         connect(directoryThread, &UIGuestDirectoryDiskUsageComputer::sigResultUpdated,
    //                 this, &UIGuestFileTable::sltReceiveDirectoryStatistics/*, Qt::DirectConnection*/);
    //         directoryThread->start();
    //     }
    // }

    m_pPropertiesDialog->setWindowTitle("Properties");
    m_pPropertiesDialog->setPropertyText(fsPropertyString);
    m_pPropertiesDialog->execute();

    // if (directoryThread)
    // {
    //     if (directoryThread->isRunning())
    //         directoryThread->stopRecursion();
    //     disconnect(directoryThread, &UIGuestDirectoryDiskUsageComputer::sigResultUpdated,
    //                this, &UIGuestFileTable::sltReceiveDirectoryStatistics/*, Qt::DirectConnection*/);
    // }


    delete m_pPropertiesDialog;
    m_pPropertiesDialog = 0;

}

void UIGuestFileTable::determineDriveLetters()
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

void UIGuestFileTable::prepareToolbar()
{
    if (m_pToolBar && m_pActionPool)
    {
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_GoUp));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_GoHome));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Refresh));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Delete));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Rename));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_CreateNewDirectory));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Copy));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Cut));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Paste));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_SelectAll));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_InvertSelection));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_ShowProperties));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Delete));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Rename));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Copy));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Cut));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_ShowProperties));
    }
    setSelectionDependentActionsEnabled(false);
}

void UIGuestFileTable::prepareActionConnections()
{
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_GoUp), &QAction::triggered,
            this, &UIGuestControlFileTable::sltGoUp);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_GoHome), &QAction::triggered,
            this, &UIGuestControlFileTable::sltGoHome);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Refresh), &QAction::triggered,
            this, &UIGuestControlFileTable::sltRefresh);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Delete), &QAction::triggered,
            this, &UIGuestControlFileTable::sltDelete);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Rename), &QAction::triggered,
            this, &UIGuestControlFileTable::sltRename);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Copy), &QAction::triggered,
            this, &UIGuestControlFileTable::sltCopy);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Cut), &QAction::triggered,
            this, &UIGuestControlFileTable::sltCut);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_Paste), &QAction::triggered,
            this, &UIGuestControlFileTable::sltPaste);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_SelectAll), &QAction::triggered,
            this, &UIGuestControlFileTable::sltSelectAll);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_InvertSelection), &QAction::triggered,
            this, &UIGuestControlFileTable::sltInvertSelection);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_ShowProperties), &QAction::triggered,
            this, &UIGuestControlFileTable::sltShowProperties);
    connect(m_pActionPool->action(UIActionIndex_M_GuestControlFileManager_S_Guest_CreateNewDirectory), &QAction::triggered,
            this, &UIGuestControlFileTable::sltCreateNewDirectory);
}

#include "UIGuestFileTable.moc"
