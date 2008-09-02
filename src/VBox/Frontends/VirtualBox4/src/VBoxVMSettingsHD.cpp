/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsHD class implementation
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

#include "VBoxVMSettingsHD.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "QIWidgetValidator.h"
#include "VBoxToolBar.h"
#include "VBoxDiskImageManagerDlg.h"
#include "VBoxNewHDWzd.h"

#include <QHeaderView>
#include <QItemEditorFactory>
#include <QMetaProperty>
#include <QScrollBar>

/** SATA Ports count */
static const ULONG SATAPortsCount = 30;

Qt::ItemFlags HDItemsModel::flags (const QModelIndex &aIndex) const
{
    return aIndex.row() == rowCount() - 1 ?
        QAbstractItemModel::flags (aIndex) ^ Qt::ItemIsSelectable :
        QAbstractItemModel::flags (aIndex) | Qt::ItemIsEditable;
}

QVariant HDItemsModel::data (const QModelIndex &aIndex, int aRole) const
{
    if (!aIndex.isValid())
        return QVariant();

    if (aIndex.row() < 0 || aIndex.row() >= rowCount())
        return QVariant();

    switch (aRole)
    {
        case Qt::DisplayRole:
        {
            if (aIndex.row() == rowCount() - 1)
            {
                return QVariant();
            } else
            if (aIndex.column() == 0)
            {
                return QVariant (mSltList [aIndex.row()].name);
            } else
            if (aIndex.column() == 1)
            {
                return QVariant (mVdiList [aIndex.row()].name);
            } else
            {
                Assert (0);
                return QVariant();
            }
            break;
        }
        case Qt::EditRole:
        {
            if (aIndex.column() == 0)
            {
                QVariant val (mSltId, &mSltList [aIndex.row()]);
                return val;
            } else
            if (aIndex.column() == 1)
            {
                QVariant val (mVdiId, &mVdiList [aIndex.row()]);
                return val;
            } else
            {
                Assert (0);
                return QVariant();
            }
            break;
        }
        case Qt::ToolTipRole:
        {
            if (aIndex.row() == rowCount() - 1)
                return QVariant (tr ("Double-click to add a new attachment"));

            CHardDisk hd = vboxGlobal().virtualBox()
                           .GetHardDisk (mVdiList [aIndex.row()].id);
            return QVariant (VBoxDiskImageManagerDlg::composeHdToolTip (
                             hd, VBoxMedia::Ok));
        }
        default:
        {
            return QVariant();
            break;
        }
    }
}

bool HDItemsModel::setData (const QModelIndex &aIndex,
                            const QVariant &aValue,
                            int /* aRole = Qt::EditRole */)
{
    if (!aIndex.isValid())
        return false;

    if (aIndex.row() < 0 || aIndex.row() >= rowCount())
        return false;

    if (aIndex.column() == 0)
    {
        HDSltValue newSlt = aValue.isValid() ?
            aValue.value<HDSltValue>() : HDSltValue();
        if (mSltList [aIndex.row()] != newSlt)
        {
            mSltList [aIndex.row()] = newSlt;
            emit dataChanged (aIndex, aIndex);
            return true;
        }
        else
            return false;
    } else
    if (aIndex.column() == 1)
    {
        HDVdiValue newVdi = aValue.isValid() ?
            aValue.value<HDVdiValue>() : HDVdiValue();
        if (mVdiList [aIndex.row()] != newVdi)
        {
            mVdiList [aIndex.row()] = newVdi;
            emit dataChanged (aIndex, aIndex);
            return true;
        }
        else
            return false;
    } else
    {
        Assert (0);
        return false;
    }
}

QVariant HDItemsModel::headerData (int aSection,
                                   Qt::Orientation aOrientation,
                                   int aRole) const
{
    if (aRole != Qt::DisplayRole)
        return QVariant();

    if (aOrientation == Qt::Horizontal)
        return aSection ? tr ("Hard Disk") : tr ("Slot");
    else
        return QVariant();
}

void HDItemsModel::addItem (const HDSltValue &aSlt, const HDVdiValue &aVdi)
{
    beginInsertRows (QModelIndex(), rowCount() - 1, rowCount() - 1);
    mSltList.append (aSlt);
    mVdiList.append (aVdi);
    endInsertRows();
}

void HDItemsModel::delItem (int aIndex)
{
    beginRemoveRows (QModelIndex(), aIndex, aIndex);
    mSltList.removeAt (aIndex);
    mVdiList.removeAt (aIndex);
    endRemoveRows();
}

QList<HDValue> HDItemsModel::fullList (bool aSorted /* = true */)
{
    QList<HDValue> list;
    QList<HDSltValue> slts = slotsList();
    QList<HDVdiValue> vdis = vdiList();
    for (int i = 0; i < slts.size(); ++ i)
        list << HDValue (slts [i], vdis [i]);
    if (aSorted)
        qSort (list.begin(), list.end());
    return list;
}

void HDItemsModel::removeSata()
{
    QList<HDSltValue>::iterator sltIt = mSltList.begin();
    QList<HDVdiValue>::iterator vdiIt = mVdiList.begin();
    while (sltIt != mSltList.end())
    {
        if ((*sltIt).bus == KStorageBus_SATA)
        {
            sltIt = mSltList.erase (sltIt);
            vdiIt = mVdiList.erase (vdiIt);
        }
        else
        {
            ++ sltIt;
            ++ vdiIt;
        }
    }
}

/** QComboBox class reimplementation used as editor for hd slot */
HDSltEditor::HDSltEditor (QWidget *aParent)
    : QComboBox (aParent)
{
    connect (this, SIGNAL (currentIndexChanged (int)), this, SLOT (onActivate()));
    connect (this, SIGNAL (readyToCommit (QWidget *)),
             parent()->parent(), SLOT (commitData (QWidget *)));
}

QVariant HDSltEditor::slot() const
{
    int current = currentIndex();
    QVariant result;
    if (current >= 0 && current < mList.size())
        result.setValue (mList [currentIndex()]);
    return result;
}

void HDSltEditor::setSlot (QVariant aSlot)
{
    HDSltValue val (aSlot.value<HDSltValue>());
    populate (val);
    int cur = findText (val.name);
    setCurrentIndex (cur == -1 ? 0 : cur);
}

void HDSltEditor::onActivate()
{
    emit readyToCommit (this);
}

void HDSltEditor::keyPressEvent (QKeyEvent *aEvent)
{
    /* Make F2 key to show the popup. */
    if (aEvent->key() == Qt::Key_F2)
    {
        aEvent->accept();
        showPopup();
    }
    else
        aEvent->ignore();
    QComboBox::keyPressEvent (aEvent);
}

void HDSltEditor::populate (const HDSltValue &aIncluding)
{
    clear();
    mList.clear();
    QList<HDSltValue> list = HDSlotUniquizer::instance()->list (aIncluding);
    for (int i = 0; i < list.size() ; ++ i)
    {
        insertItem (i, list [i].name);
        mList << list [i];
    }
}

/** QComboBox class reimplementation used as editor for hd vdi */
HDVdiEditor* HDVdiEditor::mInstance = 0;
HDVdiEditor::HDVdiEditor (QWidget *aParent)
    : VBoxMediaComboBox (aParent, VBoxDefs::HD)
{
    mInstance = this;
    setBelongsTo (HDSlotUniquizer::instance()->machine().GetId());
    connect (this, SIGNAL (currentIndexChanged (int)), this, SLOT (onActivate()));
    connect (this, SIGNAL (readyToCommit (QWidget *)),
             parent()->parent(), SLOT (commitData (QWidget *)));
    refresh();
}
HDVdiEditor::~HDVdiEditor()
{
    if (mInstance == this)
        mInstance = 0;
}

QVariant HDVdiEditor::vdi() const
{
    int current = currentIndex();
    QVariant result;
    if (current >= 0 && current < count())
    {
        HDVdiValue val (currentText().isEmpty() ? QString::null : currentText(),
                        getId (current));
        result.setValue (val);
    }
    return result;
}

void HDVdiEditor::setVdi (QVariant aVdi)
{
    HDVdiValue val (aVdi.value<HDVdiValue>());
    setCurrentItem (val.id);
}

void HDVdiEditor::tryToChooseUniqueVdi (QList<HDVdiValue> &aList)
{
    for (int i = 0; i < count(); ++ i)
    {
        HDVdiValue val (itemText (i), getId (i));
        if (!aList.contains (val))
        {
            setCurrentItem (getId (i));
            break;
        }
    }
}

HDVdiEditor* HDVdiEditor::activeEditor()
{
    return mInstance;
}

void HDVdiEditor::onActivate()
{
    emit readyToCommit (this);
}

void HDVdiEditor::keyPressEvent (QKeyEvent *aEvent)
{
    /* Make F2 key to show the popup. */
    if (aEvent->key() == Qt::Key_F2)
    {
        aEvent->accept();
        showPopup();
    }
    else
        aEvent->ignore();
    VBoxMediaComboBox::keyPressEvent (aEvent);
}

/** Singleton QObject class reimplementation to use for making selected IDE &
 * SATA slots unique */
HDSlotUniquizer* HDSlotUniquizer::mInstance = 0;
HDSlotUniquizer* HDSlotUniquizer::instance (QWidget *aParent,
                                            HDItemsModel *aWatched,
                                            const CMachine &aMachine)
{
    if (!mInstance)
    {
        Assert (aParent && aWatched && !aMachine.isNull());
        mInstance = new HDSlotUniquizer (aParent, aWatched, aMachine);
    }
    return mInstance;
}

HDSlotUniquizer::HDSlotUniquizer (QWidget *aParent, HDItemsModel *aWatched,
                                  const CMachine &aMachine)
    : QObject (aParent)
    , mSataCount (SATAPortsCount)
    , mModel (aWatched)
    , mMachine (aMachine)
{
    makeIDEList();
    makeSATAList();
}

HDSlotUniquizer::~HDSlotUniquizer()
{
    mInstance = 0;
}

QList<HDSltValue> HDSlotUniquizer::list (const HDSltValue &aIncluding,
                                         bool aFilter /* = true */)
{
    QList<HDSltValue> list = mIDEList + mSATAList;
    if (!aFilter)
        return list;

    /* Current used list */
    QList<HDSltValue> current (mModel->slotsList());

    /* Filter the list */
    QList<HDSltValue>::iterator it = current.begin();
    while (it != current.end())
    {
        if (*it != aIncluding)
            list.removeAll (*it);
        ++ it;
    }

    return list;
}

void HDSlotUniquizer::makeIDEList()
{
    mIDEList.clear();

    /* IDE Primary Master */
    mIDEList << HDSltValue (vboxGlobal().toFullString (KStorageBus_IDE, 0, 0),
                            KStorageBus_IDE, 0, 0);
    /* IDE Primary Slave */
    mIDEList << HDSltValue (vboxGlobal().toFullString (KStorageBus_IDE, 0, 1),
                            KStorageBus_IDE, 0, 1);
    /* IDE Secondary Slave */
    mIDEList << HDSltValue (vboxGlobal().toFullString (KStorageBus_IDE, 1, 1),
                            KStorageBus_IDE, 1, 1);
}

void HDSlotUniquizer::makeSATAList()
{
    mSATAList.clear();

    for (int i = 0; i < mSataCount; ++ i)
        mSATAList << HDSltValue (vboxGlobal().toFullString (KStorageBus_SATA, i, 0),
                                 KStorageBus_SATA, i, 0);
}


VBoxVMSettingsHD::VBoxVMSettingsHD()
    : mValidator (0)
    , mWasTableSelected (false)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsHD::setupUi (this);

    /* Setup model/view factory */
    int idHDSlt = qRegisterMetaType<HDSltValue>();
    int idHDVdi = qRegisterMetaType<HDVdiValue>();
    QItemEditorFactory *factory = new QItemEditorFactory;
    QItemEditorCreatorBase *sltCreator =
        new QStandardItemEditorCreator<HDSltEditor>();
    QItemEditorCreatorBase *vdiCreator =
        new QStandardItemEditorCreator<HDVdiEditor>();
    factory->registerEditor ((QVariant::Type)idHDSlt, sltCreator);
    factory->registerEditor ((QVariant::Type)idHDVdi, vdiCreator);
    QItemEditorFactory::setDefaultFactory (factory);

    /* Setup view-model */
    mModel = new HDItemsModel (this, idHDSlt, idHDVdi);
    connect (mModel, SIGNAL (dataChanged (const QModelIndex&, const QModelIndex&)),
             this, SIGNAL (hdChanged()));

    /* Setup table-view */
    mTwAts->verticalHeader()->setDefaultSectionSize (
        (int) (mTwAts->fontMetrics().height() * 1.30 /* 130% of font height */));
    mTwAts->verticalHeader()->hide();
    mTwAts->horizontalHeader()->setStretchLastSection (true);
    mTwAts->setModel (mModel);
    mTwAts->setToolTip (mModel->data (mModel->index (mModel->rowCount() - 1, 0),
                                      Qt::ToolTipRole).toString());

    /* Prepare actions */
    mNewAction = new QAction (mTwAts);
    mDelAction = new QAction (mTwAts);
    mVdmAction = new QAction (mTwAts);

    mTwAts->addAction (mNewAction);
    mTwAts->addAction (mDelAction);
    mTwAts->addAction (mVdmAction);

    mNewAction->setShortcut (QKeySequence ("Ins"));
    mDelAction->setShortcut (QKeySequence ("Del"));
    mVdmAction->setShortcut (QKeySequence ("Ctrl+Space"));

    mNewAction->setIcon (VBoxGlobal::iconSet (":/vdm_add_16px.png",
                                              ":/vdm_add_disabled_16px.png"));
    mDelAction->setIcon (VBoxGlobal::iconSet (":/vdm_remove_16px.png",
                                              ":/vdm_remove_disabled_16px.png"));
    mVdmAction->setIcon (VBoxGlobal::iconSet (":/select_file_16px.png",
                                              ":/select_file_dis_16px.png"));

    /* Prepare toolbar */
    VBoxToolBar *toolBar = new VBoxToolBar (mGbAts);
    toolBar->setUsesTextLabel (false);
    toolBar->setIconSize (QSize (16, 16));
    toolBar->setOrientation (Qt::Vertical);
    toolBar->addAction (mNewAction);
    toolBar->addAction (mDelAction);
    toolBar->addAction (mVdmAction);
    mGbAts->layout()->addWidget (toolBar);

    /* Setup connections */
    connect (mNewAction, SIGNAL (triggered (bool)),
             this, SLOT (newClicked()));
    connect (mDelAction, SIGNAL (triggered (bool)),
             this, SLOT (delClicked()));
    connect (mVdmAction, SIGNAL (triggered (bool)),
             this, SLOT (vdmClicked()));
    connect (mCbSATA, SIGNAL (stateChanged (int)),
             this, SLOT (cbSATAToggled (int)));
    connect (mTwAts, SIGNAL (currentChanged (const QModelIndex &)),
             this, SLOT (onCurrentChanged (const QModelIndex &)));
    connect (&vboxGlobal(),
             SIGNAL (mediaRemoved (VBoxDefs::DiskType, const QUuid &)),
             this, SLOT (onMediaRemoved (VBoxDefs::DiskType, const QUuid &)));
    connect (this, SIGNAL (signalToCloseEditor (QWidget*, QAbstractItemDelegate::EndEditHint)),
             mTwAts, SLOT (closeEditor (QWidget*, QAbstractItemDelegate::EndEditHint)));

    /* Install global event filter */
    qApp->installEventFilter (this);

    /* Applying language settings */
    retranslateUi();
}

void VBoxVMSettingsHD::getFrom (const CMachine &aMachine)
{
    mMachine = aMachine;

    /* Setup slot uniquizer */
    HDSlotUniquizer::instance (mTwAts, mModel, mMachine);

    CSATAController ctl = mMachine.GetSATAController();
    /* Hide the SATA check box if the SATA controller is not available
     * (i.e. in VirtualBox OSE) */
    if (ctl.isNull())
        mCbSATA->setHidden (true);
    else
        mCbSATA->setChecked (ctl.GetEnabled());
    cbSATAToggled (mCbSATA->checkState());

    CHardDiskAttachmentEnumerator en =
        mMachine.GetHardDiskAttachments().Enumerate();
    while (en.HasMore())
    {
        CHardDiskAttachment hda = en.GetNext();
        HDSltValue slt (vboxGlobal().toFullString (hda.GetBus(),
                                                   hda.GetChannel(),
                                                   hda.GetDevice()),
                        hda.GetBus(), hda.GetChannel(), hda.GetDevice());
        HDVdiValue vdi (VBoxMediaComboBox::fullItemName (hda.GetHardDisk()
                                                         .GetLocation()),
                        hda.GetHardDisk().GetRoot().GetId());
        mModel->addItem (slt, vdi);
    }

    mTwAts->setCurrentIndex (mModel->index (0, 1));
    onCurrentChanged (mTwAts->currentIndex());

    if (mValidator)
        mValidator->revalidate();
}

void VBoxVMSettingsHD::putBackTo()
{
    CSATAController ctl = mMachine.GetSATAController();
    if (!ctl.isNull())
    {
        ctl.SetEnabled (mCbSATA->isChecked());
    }

    /* Detach all attached Hard Disks */
    CHardDiskAttachmentEnumerator en =
        mMachine.GetHardDiskAttachments().Enumerate();
    while (en.HasMore())
    {
        CHardDiskAttachment hda = en.GetNext();
        mMachine.DetachHardDisk (hda.GetBus(), hda.GetChannel(), hda.GetDevice());
        if (!mMachine.isOk())
            vboxProblem().cannotDetachHardDisk (this, mMachine,
                hda.GetBus(), hda.GetChannel(), hda.GetDevice());
    }

    /* Attach all listed Hard Disks */
    LONG maxSATAPort = 1;
    QList<HDValue> list (mModel->fullList());
    for (int i = 0; i < list.size(); ++ i)
    {
        if (list [i].slt.bus == KStorageBus_SATA)
            maxSATAPort = maxSATAPort < (list [i].slt.channel + 1) ?
                          (list [i].slt.channel + 1) : maxSATAPort;
        mMachine.AttachHardDisk (list [i].vdi.id,
            list [i].slt.bus, list [i].slt.channel, list [i].slt.device);
        if (!mMachine.isOk())
            vboxProblem().cannotAttachHardDisk (this, mMachine, list [i].vdi.id,
                list [i].slt.bus, list [i].slt.channel, list [i].slt.device);
    }

    if (!ctl.isNull())
    {
        mMachine.GetSATAController().SetPortCount (maxSATAPort);
    }
}

void VBoxVMSettingsHD::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
    connect (mModel, SIGNAL (dataChanged (const QModelIndex&, const QModelIndex&)),
             mValidator, SLOT (revalidate()));
}

bool VBoxVMSettingsHD::revalidate (QString &aWarning, QString &)
{
    QList<HDSltValue> sltList (mModel->slotsList());
    QList<HDVdiValue> vdiList (mModel->vdiList());
    for (int i = 0; i < vdiList.size(); ++ i)
    {
        /* Check for emptiness */
        if (vdiList [i].name.isNull())
        {
            aWarning = tr ("No hard disk is selected for <i>%1</i>")
                           .arg (sltList [i].name);
            break;
        }

        /* Check for coincidence */
        if (vdiList.count (vdiList [i]) > 1)
        {
            int first = vdiList.indexOf (vdiList [i]);
            int second = vdiList.indexOf (vdiList [i], first + 1);
            Assert (first != -1 && second != -1);
            aWarning = tr ("<i>%1</i> uses the hard disk that is "
                           "already attached to <i>%2</i>")
                           .arg (sltList [second].name,
                                 sltList [first].name);
            break;
        }
    }

    return aWarning.isNull();
}

void VBoxVMSettingsHD::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mCbSATA);
    setTabOrder (mCbSATA, mTwAts);
}

void VBoxVMSettingsHD::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsHD::retranslateUi (this);

    mNewAction->setText (tr ("&Add Attachment"));
    mDelAction->setText (tr ("&Remove Attachment"));
    mVdmAction->setText (tr ("&Select Hard Disk"));

    mNewAction->setToolTip (mNewAction->text().remove ('&') +
        QString (" (%1)").arg (mNewAction->shortcut().toString()));
    mDelAction->setToolTip (mDelAction->text().remove ('&') +
        QString (" (%1)").arg (mDelAction->shortcut().toString()));
    mVdmAction->setToolTip (mVdmAction->text().remove ('&') +
        QString (" (%1)").arg (mVdmAction->shortcut().toString()));

    mNewAction->setWhatsThis (tr ("Adds a new hard disk attachment."));
    mDelAction->setWhatsThis (tr ("Removes the highlighted hard disk attachment."));
    mVdmAction->setWhatsThis (tr ("Invokes the Virtual Disk Manager to select "
                                  "a hard disk to attach to the currently "
                                  "highlighted slot."));
}

void VBoxVMSettingsHD::newClicked()
{
    /* Remember the current vdis list */
    QList<HDVdiValue> vdis (mModel->vdiList());

    /* Add new index */
    mModel->addItem();

    /* Set the default data into the new index for column #1 */
    mTwAts->setCurrentIndex (mModel->index (mModel->rowCount() - 2, 1));
    /* Set the default data into the new index for column #0 */
    mTwAts->setCurrentIndex (mModel->index (mModel->rowCount() - 2, 0));

    /* Set column #1 of new index to be the current */
    mTwAts->setCurrentIndex (mModel->index (mModel->rowCount() - 2, 1));

    HDVdiEditor *editor = HDVdiEditor::activeEditor();
    if (editor)
    {
        /* Try to select unique vdi */
        editor->tryToChooseUniqueVdi (vdis);

        /* Ask the user for method to add new vdi */
        int result = mModel->rowCount() - 1 > editor->count() ?
            vboxProblem().confirmRunNewHDWzdOrVDM (this) :
            QIMessageBox::Cancel;
        if (result == QIMessageBox::Yes)
        {
            /* Close the editor to avoid it's infliction to data model */
            emit signalToCloseEditor (HDVdiEditor::activeEditor(),
                                      QAbstractItemDelegate::NoHint);

            /* Run new HD wizard */
            VBoxNewHDWzd dlg (this);
            if (dlg.exec() == QDialog::Accepted)
            {
                CHardDisk hd = dlg.hardDisk();
                QVariant result;
                HDVdiValue val (VBoxMediaComboBox::fullItemName (hd.GetLocation()),
                                hd.GetId());
                result.setValue (val);
                mModel->setData (mTwAts->currentIndex(), result);
                vboxGlobal().startEnumeratingMedia();
            }
        }
        else if (result == QIMessageBox::No)
            vdmClicked();
    }
}

void VBoxVMSettingsHD::delClicked()
{
    QModelIndex current = mTwAts->currentIndex();
    if (current.isValid())
    {
        /* Storing current attributes */
        int row = current.row();
        int col = current.column();

        /* Erase current index */
        mTwAts->setCurrentIndex (QModelIndex());

        /* Calculate new current index */
        int newRow = row < mModel->rowCount() - 2 ? row :
                     row > 0 ? row - 1 : -1;
        QModelIndex next = newRow == -1 ? mModel->index (0, col) :
                                          mModel->index (newRow, col);

        /* Delete current index */
        mModel->delItem (current.row());

        /* Set the new index to be the current */
        mTwAts->setCurrentIndex (next);
        onCurrentChanged (next);

        if (mValidator)
            mValidator->revalidate();
        emit hdChanged();
    }
}

void VBoxVMSettingsHD::vdmClicked()
{
    Assert (mTwAts->currentIndex().isValid());

    /* Close the editor to avoid it's infliction to data model */
    emit signalToCloseEditor (HDVdiEditor::activeEditor(),
                              QAbstractItemDelegate::NoHint);

    HDVdiValue oldVdi (mModel->data (mTwAts->currentIndex(), Qt::EditRole)
                       .value<HDVdiValue>());

    VBoxDiskImageManagerDlg dlg (this);
    dlg.setup (VBoxDefs::HD, true, mMachine.GetId(), true, mMachine, oldVdi.id);

    if (dlg.exec() == QDialog::Accepted)
    {
        /* Compose resulting vdi */
        QVariant result;
        HDVdiValue newVdi (VBoxMediaComboBox::fullItemName (dlg.selectedPath()),
                           dlg.selectedUuid());
        result.setValue (newVdi);

        /* Set the model's data */
        mModel->setData (mTwAts->currentIndex(), result);
    }

    vboxGlobal().startEnumeratingMedia();
}

void VBoxVMSettingsHD::onCurrentChanged (const QModelIndex& /* aIndex */)
{
    mNewAction->setEnabled (mModel->rowCount() - 1 <
        HDSlotUniquizer::instance()->list (HDSltValue(), false).count());
    mDelAction->setEnabled (mTwAts->currentIndex().row() != mModel->rowCount() - 1);
    mVdmAction->setEnabled (mTwAts->currentIndex().row() != mModel->rowCount() - 1 &&
                            mTwAts->currentIndex().column() == 1);
}

void VBoxVMSettingsHD::cbSATAToggled (int aState)
{
    if (aState == Qt::Unchecked)
    {
        /* Search the list for at least one SATA port in */
        QList<HDSltValue> list (mModel->slotsList());
        int firstSataPort = 0;
        for (; firstSataPort < list.size(); ++ firstSataPort)
            if (list [firstSataPort].bus == KStorageBus_SATA)
                break;

        /* If list contains at least one SATA port */
        if (firstSataPort < list.size())
        {
            if (vboxProblem().confirmDetachSATASlots (this) != QIMessageBox::Ok)
            {
                /* Switch check-box back to "Qt::Checked" */
                mCbSATA->blockSignals (true);
                mCbSATA->setCheckState (Qt::Checked);
                mCbSATA->blockSignals (false);
                return;
            }
            else
            {
                /* Delete SATA items */
                mModel->removeSata();

                /* Set column #1 of first index to be the current */
                mTwAts->setCurrentIndex (mModel->index (0, 1));

                if (mValidator)
                    mValidator->revalidate();
            }
        }
    }

    int newSataCount = aState == Qt::Checked ? SATAPortsCount : 0;
    if (HDSlotUniquizer::instance()->sataCount() != newSataCount)
        HDSlotUniquizer::instance()->setSataCount (newSataCount);
    onCurrentChanged (mTwAts->currentIndex());
}

void VBoxVMSettingsHD::onMediaRemoved (VBoxDefs::DiskType aType, const QUuid &aId)
{
    /* Check if it is necessary to update data-model if
     * some media was removed */
    if (aType == VBoxDefs::HD)
    {
        QList<HDVdiValue> vdis (mModel->vdiList());
        for (int i = 0; i < vdis.size(); ++ i)
        {
            if (vdis [i].id == aId)
            {
                QVariant emptyVal;
                emptyVal.setValue (HDVdiValue());
                mModel->setData (mModel->index (i, 1), emptyVal);
            }
        }
    }
}

bool VBoxVMSettingsHD::eventFilter (QObject *aObject, QEvent *aEvent)
{
    if (!aObject->isWidgetType())
        return QWidget::eventFilter (aObject, aEvent);

    QWidget *widget = static_cast<QWidget*> (aObject);
    if (widget->inherits ("HDSltEditor") ||
        widget->inherits ("HDVdiEditor"))
    {
        if (aEvent->type() == QEvent::KeyPress)
        {
            QKeyEvent *e = static_cast<QKeyEvent*> (aEvent);
            QModelIndex cur = mTwAts->currentIndex();
            switch (e->key())
            {
                case Qt::Key_Up:
                {
                    if (cur.row() > 0)
                        mTwAts->setCurrentIndex (mModel->index (cur.row() - 1,
                                                                cur.column()));
                    return true;
                }
                case Qt::Key_Down:
                {
                    if (cur.row() < mModel->rowCount() - 1)
                        mTwAts->setCurrentIndex (mModel->index (cur.row() + 1,
                                                                cur.column()));
                    return true;
                }
                case Qt::Key_Right:
                {
                    if (cur.column() == 0)
                        mTwAts->setCurrentIndex (mModel->index (cur.row(), 1));
                    return true;
                }
                case Qt::Key_Left:
                {
                    if (cur.column() == 1)
                        mTwAts->setCurrentIndex (mModel->index (cur.row(), 0));
                    return true;
                }
                case Qt::Key_Tab:
                {
                    focusNextPrevChild (true);
                    return true;
                }
                case Qt::Key_Backtab:
                {
                    focusNextPrevChild (false);
                    return true;
                }
                default:
                    break;
            }
        } else
        if (aEvent->type() == QEvent::WindowDeactivate)
        {
            /* Store focus state if it is on temporary editor. */
            if (widget->hasFocus())
                mWasTableSelected = true;
        }
    } else
    if (widget == mTwAts->viewport() &&
        aEvent->type() == QEvent::MouseButtonDblClick)
    {
        QMouseEvent *e = static_cast<QMouseEvent*> (aEvent);
        QModelIndex index = mTwAts->indexAt (e->pos());
        if (mNewAction->isEnabled() &&
            (index.row() == mModel->rowCount() - 1 || !index.isValid()))
            newClicked();
    } else
    if (aEvent->type() == QEvent::WindowActivate)
    {
        if (mWasTableSelected)
        {
            /* Restore focus state if it was on temporary editor. */
            mWasTableSelected = false;
            mTwAts->setFocus();
        }
    }

    return QWidget::eventFilter (aObject, aEvent);
}

int VBoxVMSettingsHD::maxNameLength() const
{
    QList<HDSltValue> fullList (HDSlotUniquizer::instance()
                                ->list (HDSltValue(), false));
    int maxLength = 0;
    for (int i = 0; i < fullList.size(); ++ i)
    {
        int length = mTwAts->fontMetrics().width (fullList [i].name);
        maxLength = length > maxLength ? length : maxLength;
    }
    return maxLength;
}

void VBoxVMSettingsHD::showEvent (QShowEvent *aEvent)
{
    /* Activate edit triggers now to restrict them during data loading */
    mTwAts->setEditTriggers (QAbstractItemView::CurrentChanged |
                             QAbstractItemView::SelectedClicked |
                             QAbstractItemView::EditKeyPressed |
                             QAbstractItemView::DoubleClicked);

    /* Temporary activating vertical scrollbar to calculate it's width */
    mTwAts->setVerticalScrollBarPolicy (Qt::ScrollBarAlwaysOn);
    int width = mTwAts->verticalScrollBar()->width();
    mTwAts->setVerticalScrollBarPolicy (Qt::ScrollBarAsNeeded);
    QWidget::showEvent (aEvent);
    mTwAts->horizontalHeader()->resizeSection (0,
        width + maxNameLength() + 9 * 2 /* 2 margins */);

    /* That little hack allows avoid one of qt4 children focusing bug */
    QWidget *current = QApplication::focusWidget();
    mTwAts->setFocus (Qt::TabFocusReason);
    if (current)
        current->setFocus (Qt::TabFocusReason);
}

