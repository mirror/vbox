/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoContentBrowser class declaration.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_medium_viso_UIVisoContentBrowser_h
#define FEQT_INCLUDED_SRC_medium_viso_UIVisoContentBrowser_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* Qt includes: */
#include <QModelIndex>
#include <QFileInfo>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIVisoBrowserBase.h"

/* COM includes: */
#include "COMEnums.h"


/* Forward declarations: */
class UICustomFileSystemModel;
class UICustomFileSystemItem;
class UICustomFileSystemProxyModel;
class UIVisoContentTreeProxyModel;

class SHARED_LIBRARY_STUFF UIVisoContentBrowser : public QIWithRetranslateUI<UIVisoBrowserBase>
{
    Q_OBJECT;

public:

    UIVisoContentBrowser(QWidget *pParent = 0);
    ~UIVisoContentBrowser();
    /** Adds file objests from the host file system. @p pathList consists of list of paths to there objects. */
    void addObjectsToViso(QStringList pathList);
    QStringList pathList();
    virtual void showHideHiddenObjects(bool bShow) /* override */;

    void setVisoName(const QString &strName);

protected:

    void retranslateUi();
    virtual void tableViewItemDoubleClick(const QModelIndex &index) /* override */;
    /** @name Functions to set view root indices explicitly. They block the related signals. @p is converted
        to the correct index before setting.
      * @{ */
        virtual void setTableRootIndex(QModelIndex index = QModelIndex()) /* override */;
        virtual void setTreeCurrentIndex(QModelIndex index = QModelIndex()) /* override */;
    /** @} */

    virtual void treeSelectionChanged(const QModelIndex &selectedTreeIndex) /* override */;

private slots:

    void sltHandleCreateNewDirectory();
    void sltHandleItemRenameAttempt(UICustomFileSystemItem *pItem, QString strOldName, QString strNewName);


private:

    void                    prepareObjects();
    void                    prepareConnections();
    void                    initializeModel();
    UICustomFileSystemItem* rootItem();
    QModelIndex             convertIndexToTableIndex(const QModelIndex &index);
    QModelIndex             convertIndexToTreeIndex(const QModelIndex &index);
    void                    scanHostDirectory(UICustomFileSystemItem *directory);
    KFsObjType              fileType(const QFileInfo &fsInfo);
    void                    updateStartItemName();
    void                    renameFileObject(UICustomFileSystemItem *pItem);

    UICustomFileSystemModel      *m_pModel;
    UICustomFileSystemProxyModel *m_pTableProxyModel;
    UIVisoContentTreeProxyModel  *m_pTreeProxyModel;
    QIToolButton                 *m_pNewDirectoryButton;
    QString                       m_strVisoName;
};

#endif /* !FEQT_INCLUDED_SRC_medium_viso_UIVisoContentBrowser_h */
