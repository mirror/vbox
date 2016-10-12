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
# include <QAccessibleWidget>

/* GUI includes: */
# include "QITableView.h"
# include "QIStyledItemDelegate.h"

/* Other VBox includes: */
# include "iprt/assert.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** QAccessibleWidget extension used as an accessibility interface for QITableView. */
class QIAccessibilityInterfaceForQITableView : public QAccessibleWidget
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating QITableView accessibility interface: */
        if (pObject && strClassname == QLatin1String("QITableView"))
            return new QIAccessibilityInterfaceForQITableView(qobject_cast<QWidget*>(pObject));

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pWidget to the base-class. */
    QIAccessibilityInterfaceForQITableView(QWidget *pWidget)
        : QAccessibleWidget(pWidget, QAccessible::List)
    {}

    /** Returns the number of children. */
    virtual int childCount() const /* override */;
    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int iIndex) const /* override */;
    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface *pChild) const /* override */;

    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text enmTextRole) const /* override */;

private:

    /** Returns corresponding QITableView. */
    QITableView *table() const { return qobject_cast<QITableView*>(widget()); }
};


/*********************************************************************************************************************************
*   Class QIAccessibilityInterfaceForQITableView implementation.                                                                 *
*********************************************************************************************************************************/

int QIAccessibilityInterfaceForQITableView::childCount() const
{
    /* Make sure table still alive: */
    AssertPtrReturn(table(), 0);

    /* Return the number of children: */
    return table()->childCount();
}

QAccessibleInterface *QIAccessibilityInterfaceForQITableView::child(int iIndex) const
{
    /* Make sure table still alive: */
    AssertPtrReturn(table(), 0);
    /* Make sure index is valid: */
    AssertReturn(iIndex >= 0 && iIndex < childCount(), 0);

    /* Return the child with the passed iIndex: */
    return QAccessible::queryAccessibleInterface(table()->childItem(iIndex));
}

int QIAccessibilityInterfaceForQITableView::indexOfChild(const QAccessibleInterface *pChild) const
{
    /* Search for corresponding child: */
    for (int i = 0; i < childCount(); ++i)
        if (child(i) == pChild)
            return i;

    /* -1 by default: */
    return -1;
}

QString QIAccessibilityInterfaceForQITableView::text(QAccessible::Text /* enmTextRole */) const
{
    /* Make sure table still alive: */
    AssertPtrReturn(table(), QString());

    /* Return tree whats-this: */
    return table()->whatsThis();
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

void QITableView::sltEditorCreated(QWidget *pEditor, const QModelIndex &index)
{
    /* Connect created editor to the table and store it: */
    connect(pEditor, SIGNAL(destroyed(QObject *)), this, SLOT(sltEditorDestroyed(QObject *)));
    m_editors[index] = pEditor;
}

void QITableView::sltEditorDestroyed(QObject *pEditor)
{
    /* Clear destroyed editor from the table: */
    const QModelIndex index = m_editors.key(pEditor);
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

void QITableView::prepare()
{
    /* Install QITableView accessibility interface factory: */
    QAccessible::installFactory(QIAccessibilityInterfaceForQITableView::pFactory);

    /* Delete old delegate: */
    delete itemDelegate();
    /* Create new delegate: */
    QIStyledItemDelegate *pStyledItemDelegate = new QIStyledItemDelegate(this);
    AssertPtrReturnVoid(pStyledItemDelegate);
    {
        /* Assign newly created delegate to the table: */
        setItemDelegate(pStyledItemDelegate);
        /* Connect newly created delegate to the table: */
        connect(pStyledItemDelegate, SIGNAL(sigEditorCreated(QWidget *, const QModelIndex &)),
                this, SLOT(sltEditorCreated(QWidget *, const QModelIndex &)));
    }
}

