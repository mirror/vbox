/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlFileTree class declaration.
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

#ifndef ___UIGuestControlFileTree_h___
#define ___UIGuestControlFileTree_h___

/* Qt includes: */
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CEventListener.h"
#include "CEventSource.h"
#include "CGuest.h"
#include "CGuestSession.h"

/* GUI includes: */
#include "QITreeWidget.h"

/* Forward declarations: */



class UIGuestControlFileTree : public QITreeWidget
{
    Q_OBJECT;

public:


private:


};

class UIGuestControlFileTreeItem : public QITreeWidgetItem
{
    Q_OBJECT;

public:
    UIGuestControlFileTreeItem(UIGuestControlFileTree *pTreeWidget,
                               int depth, const QString &path,
                               const QStringList &strings = QStringList());
    UIGuestControlFileTreeItem(UIGuestControlFileTreeItem *pTreeWidgetItem,
                               int depth, const QString &path,
                               const QStringList &strings = QStringList());
    virtual ~UIGuestControlFileTreeItem();

    int depth() const;
    const QString& path() const;

    bool    isOpened() const;
    void    setIsOpened(bool flag);

    bool    isDirectory() const;
    void    setIsDirectory(bool flag);

    QList<UIGuestControlFileTreeItem*> children();

private:

    void    prepare();

    int     m_iDepth;
    QString m_strPath;
    /* Makes sense for directories. Used to not to descend unnecessarily: */
    bool    m_bIsOpened;
    bool    m_bIsDirectory;
};

#endif /* !___UIGuestControlFileTree_h___ */
