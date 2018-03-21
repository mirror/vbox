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
// # include <QAction>
# include <QDateTime>
# include <QFileInfo>

/* GUI includes: */
# include "QILabel.h"
# include "UIGuestFileTable.h"
# include "UIVMInformationDialog.h"

/* COM includes: */
# include "CFsObjInfo.h"
# include "CGuestFsObjInfo.h"
# include "CGuestDirectory.h"
# include "CProgress.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIGuestFileTable::UIGuestFileTable(QWidget *pParent /*= 0*/)
    :UIGuestControlFileTable(pParent)
{
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
        m_pLocationLabel->setText(UIVMInformationDialog::tr("Guest System"));
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

    directory = m_comGuestSession.DirectoryOpen(strPath, /*aFilter*/ "", flag);
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
        updateCurrentLocationEdit(strPath);
    }
    directory.Close();
}

void UIGuestFileTable::deleteByItem(UIFileTableItem *item)
{
    if (!item)
        return;
    if (!m_comGuestSession.isOk())
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
        emit sigLogOutput(QString(item->path()).append(" could not be deleted"));

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
    //printf("user home %s\n", m_comGuestSession.GetUserHome().toStdString().c_str());
    QString userHome = UIPathOperations::sanitize(m_comGuestSession.GetUserHome()).remove(0,1);

    QList<QString> pathTrail = userHome.split(UIPathOperations::delimiter);

    goIntoDirectory(pathTrail);
}

bool UIGuestFileTable::renameItem(UIFileTableItem *item, QString newBaseName)
{

    if (!item || item->isUpDirectory() || newBaseName.isEmpty() || !m_comGuestSession.isOk())
        return false;
    QString newPath = UIPathOperations::constructNewItemPath(item->path(), newBaseName);
    QVector<KFsObjRenameFlag> aFlags(KFsObjRenameFlag_Replace);

    m_comGuestSession.FsObjRename(item->path(), newPath, aFlags);
    if (!m_comGuestSession.isOk())
        return false;
    item->setPath(newPath);
    return true;
}

bool UIGuestFileTable::createDirectory(const QString &path, const QString &directoryName)
{
    if (!m_comGuestSession.isOk())
        return false;

    QString newDirectoryPath = UIPathOperations::mergePaths(path, directoryName);
    QVector<KDirectoryCreateFlag> flags(KDirectoryCreateFlag_None);

    m_comGuestSession.DirectoryCreate(newDirectoryPath, 777/*aMode*/, flags);
    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(newDirectoryPath.append(" could not be created"));
        return false;
    }
    emit sigLogOutput(newDirectoryPath.append(" has been created"));
    return true;
}

void UIGuestFileTable::copyGuestToHost(const QString& hostDestinationPath)
{
    QStringList selectedPathList = selectedItemPathList();
    for (int i = 0; i < selectedPathList.size(); ++i)
        copyGuestToHost(selectedPathList.at(i), hostDestinationPath);
}

void UIGuestFileTable::copyHostToGuest(const QStringList &hostSourcePathList)
{
    for (int i = 0; i < hostSourcePathList.size(); ++i)
        copyHostToGuest(hostSourcePathList.at(i), currentDirectoryPath());
}

bool UIGuestFileTable::copyGuestToHost(const QString &guestSourcePath, const QString& hostDestinationPath)
{
    if (m_comGuestSession.isNull())
        return false;

    /* Currently API expects a path including a file name for file copy*/
    CGuestFsObjInfo fileInfo = guestFsObjectInfo(guestSourcePath, m_comGuestSession);
    KFsObjType objectType = fileInfo.GetType();
    if (objectType == KFsObjType_File)
    {
        QVector<KFileCopyFlag> flags(KFileCopyFlag_FollowLinks);
        /* API expects a full file path as destionation: */
        QString destinatioFilePath =  UIPathOperations::mergePaths(hostDestinationPath, UIPathOperations::getObjectName(guestSourcePath));
        /** @todo listen to CProgress object to monitor copy operation: */
        /*CProgress comProgress =*/ m_comGuestSession.FileCopyFromGuest(guestSourcePath, destinatioFilePath, flags);

    }
    else if (objectType == KFsObjType_Directory)
    {
        QVector<KDirectoryCopyFlag> aFlags(KDirectoryCopyFlag_CopyIntoExisting);
        /** @todo listen to CProgress object to monitor copy operation: */
        /*CProgress comProgress = */ m_comGuestSession.DirectoryCopyFromGuest(guestSourcePath, hostDestinationPath, aFlags);
    }
    if (!m_comGuestSession.isOk())
        return false;
    return true;
}

bool UIGuestFileTable::copyHostToGuest(const QString &hostSourcePath, const QString &guestDestinationPath)
{
    if (m_comGuestSession.isNull())
        return false;
    QFileInfo hostFileInfo(hostSourcePath);
    if (!hostFileInfo.exists())
        return false;

    /* Currently API expects a path including a file name for file copy*/
    if (hostFileInfo.isFile() || hostFileInfo.isSymLink())
    {
        QVector<KFileCopyFlag> flags(KFileCopyFlag_FollowLinks);
        /* API expects a full file path as destionation: */
        QString destinationFilePath =  UIPathOperations::mergePaths(guestDestinationPath, UIPathOperations::getObjectName(hostSourcePath));
        /** @todo listen to CProgress object to monitor copy operation: */
        /*CProgress comProgress =*/ m_comGuestSession.FileCopyFromGuest(hostSourcePath, destinationFilePath, flags);
    }
    else if(hostFileInfo.isDir())
    {
        QVector<KDirectoryCopyFlag> aFlags(KDirectoryCopyFlag_CopyIntoExisting);
        /** @todo listen to CProgress object to monitor copy operation: */
        /*CProgress comProgress = */ m_comGuestSession.DirectoryCopyToGuest(hostSourcePath, guestDestinationPath, aFlags);
    }
    if (!m_comGuestSession.isOk())
        return false;
    return true;
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
    if (m_comGuestSession.isNull())
        return QString();

    QStringList selectedObjects = selectedItemPathList();
    if (selectedObjects.isEmpty())
        return QString();
    if (selectedObjects.size() == 1)
    {
        if (selectedObjects.at(0).isNull())
            return QString();
        CGuestFsObjInfo fileInfo = m_comGuestSession.FsObjQueryInfo(selectedObjects.at(0), true);
        if (!m_comGuestSession.isOk())
            return QString();

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
    return QString();
}
