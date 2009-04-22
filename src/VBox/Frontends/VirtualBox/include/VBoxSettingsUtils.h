/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxSettingsUtils class declaration
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBoxSettingsUtils_h__
#define __VBoxSettingsUtils_h__

#include <VBoxGlobal.h>

/* Qt includes */
#ifdef Q_WS_WIN
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#endif
#include <QHBoxLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QHeaderView>
#include <QKeyEvent>
#include <QTableView>

enum
{
    /* mTwUSBFilters column numbers */
    twUSBFilters_Name = 0,
};

/**
 *  QTreeWidget class reimplementation to use as boot items table.
 *  It has one unsorted column without header.
 *  Keymapping handlers for ctrl-up & ctrl-down are translated into
 *  boot-items up/down moving signals.
 *  Emits itemToggled() signal when the item changed.
 */
class BootItemsTable : public QTreeWidget
{
    Q_OBJECT;

public:

    BootItemsTable (QWidget *aParent) : QTreeWidget (aParent)
    {
        header()->hide();
        connect (this, SIGNAL (itemChanged (QTreeWidgetItem *, int)),
                 this, SLOT (onItemChanged()));
    }

   ~BootItemsTable() {}

signals:

    void moveItemUp();
    void moveItemDown();
    void itemToggled();

private slots:

    void onItemChanged()
    {
        emit itemToggled();
    }

    void keyPressEvent (QKeyEvent *aEvent)
    {
        if (aEvent->QInputEvent::modifiers () == Qt::ControlModifier)
        {
            switch (aEvent->key())
            {
                case Qt::Key_Up:
                    emit moveItemUp();
                    return;
                case Qt::Key_Down:
                    emit moveItemDown();
                    return;
                default:
                    break;
            }
        }
        QTreeWidget::keyPressEvent (aEvent);
    }
};

class USBListItem : public QTreeWidgetItem
{
public:

    enum { USBListItemType = 1002 };

    USBListItem (QTreeWidget *aParent)
        : QTreeWidgetItem (aParent, QStringList (QString::null), USBListItemType)
        , mId (-1) {}

    USBListItem (QTreeWidget *aParent, QTreeWidgetItem *aPreceding)
        : QTreeWidgetItem (aParent, aPreceding, USBListItemType)
        , mId (-1) {}

    int mId;
};

/**
 *  Table-View class reimplementation to extend standard QTableView.
 */
class QITableView : public QTableView
{
    Q_OBJECT;

public:

    QITableView (QWidget *aParent) : QTableView (aParent) {}

signals:

    void currentChanged (const QModelIndex &aCurrent);

protected:

    void currentChanged (const QModelIndex &aCurrent,
                         const QModelIndex &aPrevious)
    {
        QTableView::currentChanged (aCurrent, aPrevious);
        emit currentChanged (aCurrent);
    }

    void focusInEvent (QFocusEvent *aEvent)
    {
        /* Restore edit-mode on focus in. */
        QTableView::focusInEvent (aEvent);
        if (model()->flags (currentIndex()) & Qt::ItemIsEditable)
            edit (currentIndex());
    }
};

class VBoxWarnIconLabel: public QWidget
{
    Q_OBJECT;

public:

    VBoxWarnIconLabel (QWidget *aParent = NULL)
        : QWidget (aParent)
    {
        QHBoxLayout *layout = new QHBoxLayout (this);
        VBoxGlobal::setLayoutMargin (layout, 0);
        layout->addWidget (&mIcon);
        layout->addWidget (&mLabel);
        setVisible (false);
    }

    void setWarningPixmap (const QPixmap& aPixmap) { mIcon.setPixmap (aPixmap); }
    void setWarningText (const QString& aText) { mLabel.setText (aText); }

private:

    QLabel mIcon;
    QLabel mLabel;
};

#endif // __VBoxSettingsUtils_h__

