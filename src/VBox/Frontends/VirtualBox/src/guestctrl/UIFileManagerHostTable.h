/* $Id$ */
/** @file
 * VBox Qt GUI - UIFileManagerHostTable class declaration.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_guestctrl_UIFileManagerHostTable_h
#define FEQT_INCLUDED_SRC_guestctrl_UIFileManagerHostTable_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIFileManagerTable.h"

/* Forward declarations: */
class UIActionPool;
class UICustomFileSystemItem;

/** This class scans the host file system by using the Qt API
    and connects to the UICustomFileSystemModel*/
class UIFileManagerHostTable : public UIFileManagerTable
{
    Q_OBJECT;

public:

    UIFileManagerHostTable(UIActionPool *pActionPool, QWidget *pParent = 0);
    static KFsObjType  fileType(const QFileInfo &fsInfo);

protected:

    /** Scans the directory with the path @strPath and inserts items to the
     *  tree under the @p parent. */
    static void scanDirectory(const QString& strPath, UICustomFileSystemItem *parent,
                              QMap<QString, UICustomFileSystemItem*> &fileObjects);
    void            retranslateUi() /* override */;
    virtual void    readDirectory(const QString& strPath, UICustomFileSystemItem *parent, bool isStartDir = false) /* override */;
    virtual void    deleteByItem(UICustomFileSystemItem *item) /* override */;
    virtual void    deleteByPath(const QStringList &pathList) /* override */;
    virtual void    goToHomeDirectory() /* override */;
    virtual bool    renameItem(UICustomFileSystemItem *item, QString newBaseName);
    virtual bool    createDirectory(const QString &path, const QString &directoryName);
    virtual QString fsObjectPropertyString() /* override */;
    virtual void    showProperties() /* override */;
    virtual void    determineDriveLetters() /* override */;
    virtual void    determinePathSeparator() /* override */;
    virtual void    prepareToolbar() /* override */;
    virtual void    createFileViewContextMenu(const QWidget *pWidget, const QPoint &point) /* override */;
    /** @name Copy/Cut host-to-host stuff. Currently not implemented.
     * @{ */
        /** Disable/enable paste action depending on the m_eFileOperationType. */
        virtual void  setPasteActionEnabled(bool) /* override */{}
        virtual void  pasteCutCopiedObjects() /* override */{}
    /** @} */

private:

    static QString permissionString(QFileDevice::Permissions permissions);
    void    prepareActionConnections();
};

#endif /* !FEQT_INCLUDED_SRC_guestctrl_UIFileManagerHostTable_h */
