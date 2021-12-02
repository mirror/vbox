/* $Id$ */
/** @file
 * VBox Qt GUI - UIFileManagerGuestTable class declaration.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_guestctrl_UIFileManagerGuestTable_h
#define FEQT_INCLUDED_SRC_guestctrl_UIFileManagerGuestTable_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
# include <QUuid>

/* COM includes: */
#include "COMEnums.h"
#include "CEventListener.h"
#include "CEventSource.h"
#include "CGuest.h"
#include "CGuestSession.h"
#include "CMachine.h"
#include "CSession.h"


/* GUI includes: */
#include "UIFileManagerTable.h"
#include "UIMainEventListener.h"

/* Forward declarations: */
class UIActionPool;
class UICustomFileSystemItem;
class UIFileManagerGuestSessionPanel;

/** This class scans the guest file system by using the VBox Guest Control API
 *  and populates the UIGuestControlFileModel*/
class UIFileManagerGuestTable : public UIFileManagerTable
{
    Q_OBJECT;

signals:

    void sigNewFileOperation(const CProgress &comProgress);

public:

    UIFileManagerGuestTable(UIActionPool *pActionPool, const CMachine &comMachine, QWidget *pParent = 0);
    void initGuestFileTable(const CGuestSession &session);
    void copyGuestToHost(const QString& hostDestinationPath);
    void copyHostToGuest(const QStringList &hostSourcePathList,
                         const QString &strDestination = QString());
    QUuid machineId();

protected:

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
    /** @name Copy/Cut guest-to-guest stuff.
     * @{ */
        /** Disable/enable paste action depending on the m_eFileOperationType. */
        virtual void  setPasteActionEnabled(bool fEnabled) /* override */;
        virtual void  pasteCutCopiedObjects() /* override */;
    /** @} */

private:

    KFsObjType  fileType(const CFsObjInfo &fsInfo);
    KFsObjType  fileType(const CGuestFsObjInfo &fsInfo);

    void prepareActionConnections();
    bool checkGuestSession();
    QString permissionString(const CFsObjInfo &fsInfo);
    bool isFileObjectHidden(const CFsObjInfo &fsInfo);

    void prepareListener(ComObjPtr<UIMainEventListenerImpl> &Qtistener,
                         CEventListener &comEventListener,
                         CEventSource comEventSource, QVector<KVBoxEventType>& eventTypes);

    void cleanupListener(ComObjPtr<UIMainEventListenerImpl> &QtListener,
                         CEventListener &comEventListener,
                         CEventSource comEventSource);
    void prepareGuestSessionPanel();

    CGuest                    m_comGuest;
    CGuestSession             m_comGuestSession;
    CSession                  m_comSession;
    CMachine                  m_comMachine;

    ComObjPtr<UIMainEventListenerImpl> m_pQtGuestListener;
    ComObjPtr<UIMainEventListenerImpl> m_pQtSessionListener;
    CEventListener m_comSessionListener;
    CEventListener m_comGuestListener;
    UIFileManagerGuestSessionPanel     *m_pGuestSessionPanel;
};

#endif /* !FEQT_INCLUDED_SRC_guestctrl_UIFileManagerGuestTable_h */
