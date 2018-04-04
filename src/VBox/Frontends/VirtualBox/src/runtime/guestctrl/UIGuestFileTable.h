/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlFileTable class declaration.
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

#ifndef ___UIGuestTable_h___
#define ___UIGuestTable_h___

/* COM includes: */
#include "COMEnums.h"
#include "CGuestSession.h"

/* GUI includes: */
#include "UIGuestControlFileTable.h"

/** This class scans the guest file system by using the VBox Guest Control API
 *  and populates the UIGuestControlFileModel*/
class UIGuestFileTable : public UIGuestControlFileTable
{
    Q_OBJECT;

public:

    UIGuestFileTable(QWidget *pParent = 0);
    void initGuestFileTable(const CGuestSession &session);
    void copyGuestToHost(const QString& hostDestinationPath);
    void copyHostToGuest(const QStringList &hostSourcePathList);

protected:

    void retranslateUi() /* override */;
    virtual void readDirectory(const QString& strPath, UIFileTableItem *parent, bool isStartDir = false) /* override */;
    virtual void deleteByItem(UIFileTableItem *item) /* override */;
    virtual void goToHomeDirectory() /* override */;
    virtual bool renameItem(UIFileTableItem *item, QString newBaseName);
    virtual bool createDirectory(const QString &path, const QString &directoryName);
    virtual QString fsObjectPropertyString() /* override */;
    virtual void  showProperties() /* override */;

private:

    static FileObjectType fileType(const CFsObjInfo &fsInfo);
    static FileObjectType fileType(const CGuestFsObjInfo &fsInfo);

    bool copyGuestToHost(const QString &guestSourcePath, const QString& hostDestinationPath);
    bool copyHostToGuest(const QString& hostSourcePath, const QString &guestDestinationPath);

    mutable CGuestSession m_comGuestSession;

};

#endif /* !___UIGuestControlFileTable_h___ */

