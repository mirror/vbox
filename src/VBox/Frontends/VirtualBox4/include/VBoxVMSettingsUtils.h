/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsUtils class declaration
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

#ifndef __VBoxVMSettingsUtils_h__
#define __VBoxVMSettingsUtils_h__

#include <VBoxGlobal.h>

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
    /* mTwSelector column numbers */
    listView_Category = 0,
    listView_Id = 1,
    listView_Link = 2,
    /* mTwUSBFilters column numbers */
    lvUSBFilters_Name = 0,
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
        if (aEvent->QInputEvent::modifiers () == Qt::ControlButton)
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

#ifdef Q_WS_WIN
/**
 *  QDialog class reimplementation to use for adding network interface.
 *  It has one line-edit field for entering network interface's name and
 *  common dialog's ok/cancel buttons.
 */
class VBoxAddNIDialog : public QDialog
{
    Q_OBJECT

public:

    VBoxAddNIDialog (QWidget *aParent, const QString &aIfaceName) :
        QDialog (aParent),
        mLeName (0)
    {
        setWindowTitle (tr ("Add Host Interface"));
        QVBoxLayout *mainLayout = new QVBoxLayout (this);

        /* Setup Input layout */
        QHBoxLayout *inputLayout = new QHBoxLayout();
        QLabel *lbName = new QLabel (tr ("Interface Name"), this);
        mLeName = new QLineEdit (aIfaceName, this);
        mLeName->setWhatsThis (tr ("Descriptive name of the new "
                                   "network interface"));
        inputLayout->addWidget (lbName);
        inputLayout->addWidget (mLeName);
        connect (mLeName, SIGNAL (textChanged (const QString &)),
                 this, SLOT (validate()));
        mainLayout->addLayout (inputLayout);

        /* Setup Button layout */
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        mBtOk = new QPushButton (tr ("&OK"), this);
        QPushButton *btCancel = new QPushButton (tr ("Cancel"), this);
        connect (mBtOk, SIGNAL (clicked()), this, SLOT (accept()));
        connect (btCancel, SIGNAL (clicked()), this, SLOT (reject()));
        buttonLayout->addWidget (mBtOk);
        buttonLayout->addStretch();
        buttonLayout->addWidget (btCancel);
        mainLayout->addLayout (buttonLayout);

        /* Resize to fit the aIfaceName in one string */
        int requiredWidth = mLeName->fontMetrics().width (aIfaceName) +
                            inputLayout->spacing() +
                            lbName->fontMetrics().width (lbName->text()) +
                            lbName->frameWidth() * 2 +
                            lbName->lineWidth() * 2 +
                            mainLayout->margin() * 2;
        resize (requiredWidth, minimumHeight());

        /* Validate interface name field */
        validate();
    }

    ~VBoxAddNIDialog() {}

    QString getName() { return mLeName->text(); }

private slots:

    void validate()
    {
        mBtOk->setEnabled (!mLeName->text().isEmpty());
    }

private:

    void showEvent (QShowEvent *aEvent)
    {
        setFixedHeight (height());
        QDialog::showEvent (aEvent);
    }

    QPushButton *mBtOk;
    QLineEdit   *mLeName;
};
#endif

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
    }

    void setWarningPixmap (const QPixmap& aPixmap) { mIcon.setPixmap (aPixmap); }
    void setWarningText (const QString& aText) { mLabel.setText (aText); }

private:

    QLabel mIcon;
    QLabel mLabel;
};

#endif // __VBoxVMSettingsUtils_h__

