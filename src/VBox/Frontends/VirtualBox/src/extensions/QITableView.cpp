/* $Id$ */
/** @file
 * VBox Qt GUI - VirtualBox Qt extensions: QITableView class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QStyledItemDelegate>

/* GUI includes: */
# include "QITableView.h"

/* Other VBox includes: */
# include "iprt/assert.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** QStyledItemDelegate extension used with QITableView. */
class QITableViewStyledItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT;

signals:

    /** Notifies listeners about @a pEditor created for particular model @a index. */
    void sigEditorCreated(QWidget *pEditor, const QModelIndex &index) const;

public:

    /** Constructs table-view styled-item-delegate on the basis of passed @a pParent. */
    QITableViewStyledItemDelegate(QObject *pParent);

private:

    /** Returns the widget used to edit the item specified by @a index for editing.
      * The @a pParent widget and style @a option are used to control how the editor widget appears.
      * Besides that, we are notifying listener about editor was created for particular model @a index. */
    virtual QWidget* createEditor(QWidget *pParent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};


/*********************************************************************************************************************************
*   Class QITableViewStyledItemDelegate implementation.                                                                          *
*********************************************************************************************************************************/

QITableViewStyledItemDelegate::QITableViewStyledItemDelegate(QObject *pParent)
    : QStyledItemDelegate(pParent)
{
}

QWidget* QITableViewStyledItemDelegate::createEditor(QWidget *pParent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    /* Call to base-class: */
    QWidget *pEditor = QStyledItemDelegate::createEditor(pParent, option, index);
    /* Notify listeners about editor created: */
    emit sigEditorCreated(pEditor, index);
    /* Return editor: */
    return pEditor;
}


/*********************************************************************************************************************************
*   Class QITableView implementation.                                                                                            *
*********************************************************************************************************************************/

QITableView::QITableView(QWidget *pParent)
    : QTableView(pParent)
{
    /* Prepare all: */
    prepare();
}

void QITableView::makeSureEditorDataCommitted()
{
    /* Do we have current editor at all? */
    QObject *pEditorObject = m_editors.value(currentIndex());
    if (pEditorObject && pEditorObject->isWidgetType())
    {
        /* Cast the editor to widget type: */
        QWidget *pEditor = qobject_cast<QWidget*>(pEditorObject);
        AssertPtrReturnVoid(pEditor);
        {
            /* Commit the editor data and closes it: */
            commitData(pEditor);
            closeEditor(pEditor, QAbstractItemDelegate::SubmitModelCache);
        }
    }
}

void QITableView::prepare()
{
    /* Delete old delegate: */
    delete itemDelegate();
    /* Create new delegate: */
    QITableViewStyledItemDelegate *pStyledItemDelegate = new QITableViewStyledItemDelegate(this);
    AssertPtrReturnVoid(pStyledItemDelegate);
    {
        /* Assign newly created delegate to the table: */
        setItemDelegate(pStyledItemDelegate);
        /* Connect newly created delegate to the table: */
        connect(pStyledItemDelegate, SIGNAL(sigEditorCreated(QWidget *, const QModelIndex &)),
                this, SLOT(sltEditorCreated(QWidget *, const QModelIndex &)));
    }
}

void QITableView::sltEditorCreated(QWidget *pEditor, const QModelIndex &index)
{
    /* Connect created editor to the table and store it: */
    connect(pEditor, SIGNAL(destroyed(QObject *)), this, SLOT(sltEditorDestroyed(QObject *)));
    m_editors[index] = pEditor;
}

void QITableView::sltEditorDestroyed(QObject *pEditor)
{
    /* Clear destroyed editor from the table: */
    QModelIndex index = m_editors.key(pEditor);
    AssertReturnVoid(index.isValid());
    m_editors.remove(index);
}

void QITableView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    /* Notify listeners about index changed: */
    emit sigCurrentChanged(current, previous);
    /* Call to base-class: */
    QTableView::currentChanged(current, previous);
}

#include "QITableView.moc"

