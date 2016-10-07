/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QITableView class declaration.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___QITableView_h___
#define ___QITableView_h___

/* Qt includes: */
#include <QTableView>


/** QTableView subclass extending standard functionality. */
class QITableView : public QTableView
{
    Q_OBJECT;

signals:

    /** Notifies listeners about index changed from @a previous to @a current. */
    void sigCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

public:

    /** Constructs table-view passing @a pParent to the base-class. */
    QITableView(QWidget *pParent = 0);

    /** Makes sure current editor data committed. */
    void makeSureEditorDataCommitted();

protected slots:

    /** Stores the created @a pEditor for passed @a index in the map. */
    virtual void sltEditorCreated(QWidget *pEditor, const QModelIndex &index);
    /** Clears the destoyed @a pEditor from the map. */
    virtual void sltEditorDestroyed(QObject *pEditor);

protected:

    /** Handles index change from @a previous to @a current. */
    virtual void currentChanged(const QModelIndex &current, const QModelIndex &previous) /* override */;

private:

    /** Prepares all. */
    void prepare();

    /** Holds the map of editors stored for passed indexes. */
    QMap<QModelIndex, QObject*> m_editors;
};

#endif /* !___QITableView_h___ */

