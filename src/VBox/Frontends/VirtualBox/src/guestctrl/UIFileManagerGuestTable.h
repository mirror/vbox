/* $Id$ */
/** @file
 * VBox Qt GUI - UIFileManagerGuestTable class declaration.
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

#ifndef ___UIGuestTable_h___
#define ___UIGuestTable_h___

/* Qt includes: */
# include <QUuid>

/* COM includes: */
#include "COMEnums.h"
#include "CGuestSession.h"

/* GUI includes: */
#include "UIFileManagerTable.h"

/* Forward declarations: */
class UIActionPool;

/** This class scans the guest file system by using the VBox Guest Control API
 *  and populates the UIGuestControlFileModel*/
class UIFileManagerGuestTable : public UIFileManagerTable
{
    Q_OBJECT;

public:

    UIFileManagerGuestTable(UIActionPool *pActionPool, QWidget *pParent = 0);
    void initGuestFileTable(const CGuestSession &session);
    void copyGuestToHost(const QString& hostDestinationPath);
    void copyHostToGuest(const QStringList &hostSourcePathList,
                         const QString &strDestination = QString());

signals:

    void sigNewFileOperation(const CProgress &comProgress);

protected:

    void            retranslateUi() /* override */;
    virtual void    readDirectory(const QString& strPath, UIFileTableItem *parent, bool isStartDir = false) /* override */;
    virtual void    deleteByItem(UIFileTableItem *item) /* override */;
    virtual void    deleteByPath(const QStringList &pathList) /* override */;
    virtual void    goToHomeDirectory() /* override */;
    virtual bool    renameItem(UIFileTableItem *item, QString newBaseName);
    virtual bool    createDirectory(const QString &path, const QString &directoryName);
    virtual QString fsObjectPropertyString() /* override */;
    virtual void    showProperties() /* override */;
    virtual void    determineDriveLetters() /* override */;
    virtual void    prepareToolbar() /* override */;
    virtual void    createFileViewContextMenu(const QWidget *pWidget, const QPoint &point) /* override */;
    /** @name Copy/Cut guest-to-guest stuff.
     * @{ */
        /** Disable/enable paste action depending on the m_eFileOperationType. */
        virtual void  setPasteActionEnabled(bool fEnabled) /* override */;
        virtual void  pasteCutCopiedObjects() /* override */;
    /** @} */

private:

    FileObjectType  fileType(const CFsObjInfo &fsInfo);
    FileObjectType  fileType(const CGuestFsObjInfo &fsInfo);

    void prepareActionConnections();
    bool checkGuestSession();
    QString permissionString(const CFsObjInfo &fsInfo);
    mutable CGuestSession     m_comGuestSession;
};

#endif /* !___UIFileManagerGuestTable_h___ */
