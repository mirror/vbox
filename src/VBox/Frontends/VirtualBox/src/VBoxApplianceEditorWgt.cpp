/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxApplianceEditorWgt class implementation
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

/* VBox includes */
#include "VBoxApplianceEditorWgt.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxOSTypeSelectorButton.h"
#include "VBoxLineTextEdit.h"

/* Qt includes */
#include <QItemDelegate>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QLineEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QComboBox>

////////////////////////////////////////////////////////////////////////////////
// ModelItem

/* This & the following derived classes represent the data items of a Virtual
   System. All access/manipulation is done with the help of virtual functions
   to keep the interface clean. ModelItem is able to handle tree structures
   with a parent & several children's. */
ModelItem::ModelItem (int aNumber, ModelItemType aType, ModelItem *aParent /* = NULL */)
  : mNumber (aNumber)
  , mType (aType)
  , mParentItem (aParent)
{}

ModelItem::~ModelItem()
{
    qDeleteAll (mChildItems);
}

void ModelItem::appendChild (ModelItem *aChild)
{
    AssertPtr (aChild);
    mChildItems << aChild;
}

ModelItem * ModelItem::child (int aRow) const
{
    return mChildItems.value (aRow);
}

int ModelItem::row() const
{
    if (mParentItem)
        return mParentItem->mChildItems.indexOf (const_cast<ModelItem*> (this));

    return 0;
}

int ModelItem::childCount() const
{
    return mChildItems.count();
}

void ModelItem::putBack (QVector<BOOL>& aFinalStates, QVector<QString>& aFinalValues, QVector<QString>& aFinalExtraValues)
{
    for (int i = 0; i < childCount(); ++i)
        child (i)->putBack (aFinalStates, aFinalValues, aFinalExtraValues);
}

////////////////////////////////////////////////////////////////////////////////
// VirtualSystemItem

VirtualSystemItem::VirtualSystemItem (int aNumber, CVirtualSystemDescription aDesc, ModelItem *aParent)
  : ModelItem (aNumber, VirtualSystemType, aParent)
  , mDesc (aDesc)
{}

QVariant VirtualSystemItem::data (int aColumn, int aRole) const
{
    QVariant v;
    if (aColumn == DescriptionSection &&
        aRole == Qt::DisplayRole)
        v = VBoxApplianceEditorWgt::tr ("Virtual System %1").arg (mNumber + 1);
    return v;
}

void VirtualSystemItem::putBack (QVector<BOOL>& aFinalStates, QVector<QString>& aFinalValues, QVector<QString>& aFinalExtraValues)
{
    /* Resize the vectors */
    unsigned long count = mDesc.GetCount();
    aFinalStates.resize (count);
    aFinalValues.resize (count);
    aFinalExtraValues.resize (count);
    /* Recursively fill the vectors */
    ModelItem::putBack (aFinalStates, aFinalValues, aFinalExtraValues);
    /* Set all final values at once */
    mDesc.SetFinalValues (aFinalStates, aFinalValues, aFinalExtraValues);
}

////////////////////////////////////////////////////////////////////////////////
// HardwareItem

HardwareItem::HardwareItem (int aNumber,
                            KVirtualSystemDescriptionType aType,
                            const QString &aRef,
                            const QString &aOrigValue,
                            const QString &aConfigValue,
                            const QString &aExtraConfigValue,
                            ModelItem *aParent)
  : ModelItem (aNumber, HardwareType, aParent)
  , mType (aType)
  , mRef (aRef)
  , mOrigValue (aOrigValue)
  , mConfigValue (aConfigValue)
  , mConfigDefaultValue (aConfigValue)
  , mExtraConfigValue (aExtraConfigValue)
  , mCheckState (Qt::Checked)
{}

void HardwareItem::putBack (QVector<BOOL>& aFinalStates, QVector<QString>& aFinalValues, QVector<QString>& aFinalExtraValues)
{
    aFinalStates[mNumber] = mCheckState == Qt::Checked;
    aFinalValues[mNumber] = mConfigValue;
    aFinalExtraValues[mNumber] = mExtraConfigValue;
    ModelItem::putBack (aFinalStates, aFinalValues, aFinalExtraValues);
}

bool HardwareItem::setData (int aColumn, const QVariant &aValue, int aRole)
{
    bool fDone = false;
    switch (aRole)
    {
        case Qt::CheckStateRole:
            {
                if (aColumn == ConfigValueSection &&
                    (mType == KVirtualSystemDescriptionType_Floppy ||
                     mType == KVirtualSystemDescriptionType_CDROM ||
                     mType == KVirtualSystemDescriptionType_USBController ||
                     mType == KVirtualSystemDescriptionType_SoundCard ||
                     mType == KVirtualSystemDescriptionType_NetworkAdapter))
                {
                    mCheckState = static_cast<Qt::CheckState> (aValue.toInt());
                    fDone = true;
                }
                break;
            }
        default: break;
    }
    return fDone;
}

QVariant HardwareItem::data (int aColumn, int aRole) const
{
    QVariant v;
    switch (aRole)
    {
        case Qt::DisplayRole:
            {
                if (aColumn == DescriptionSection)
                {
                    switch (mType)
                    {
                        case KVirtualSystemDescriptionType_Name: v = VBoxApplianceEditorWgt::tr ("Name"); break;
                        case KVirtualSystemDescriptionType_Product: v = VBoxApplianceEditorWgt::tr ("Product"); break;
                        case KVirtualSystemDescriptionType_ProductUrl: v = VBoxApplianceEditorWgt::tr ("Product-URL"); break;
                        case KVirtualSystemDescriptionType_Vendor: v = VBoxApplianceEditorWgt::tr ("Vendor"); break;
                        case KVirtualSystemDescriptionType_VendorUrl: v = VBoxApplianceEditorWgt::tr ("Vendor-URL"); break;
                        case KVirtualSystemDescriptionType_Version: v = VBoxApplianceEditorWgt::tr ("Version"); break;
                        case KVirtualSystemDescriptionType_Description: v = VBoxApplianceEditorWgt::tr ("Description"); break;
                        case KVirtualSystemDescriptionType_License: v = VBoxApplianceEditorWgt::tr ("License"); break;
                        case KVirtualSystemDescriptionType_OS: v = VBoxApplianceEditorWgt::tr ("Guest OS Type"); break;
                        case KVirtualSystemDescriptionType_CPU: v = VBoxApplianceEditorWgt::tr ("CPU"); break;
                        case KVirtualSystemDescriptionType_Memory: v = VBoxApplianceEditorWgt::tr ("RAM"); break;
                        case KVirtualSystemDescriptionType_HardDiskControllerIDE: v = VBoxApplianceEditorWgt::tr ("Hard Disk Controller IDE"); break;
                        case KVirtualSystemDescriptionType_HardDiskControllerSATA: v = VBoxApplianceEditorWgt::tr ("Hard Disk Controller SATA"); break;
                        case KVirtualSystemDescriptionType_HardDiskControllerSCSI: v = VBoxApplianceEditorWgt::tr ("Hard Disk Controller SCSI"); break;
                        case KVirtualSystemDescriptionType_CDROM: v = VBoxApplianceEditorWgt::tr ("DVD"); break;
                        case KVirtualSystemDescriptionType_Floppy: v = VBoxApplianceEditorWgt::tr ("Floppy"); break;
                        case KVirtualSystemDescriptionType_NetworkAdapter: v = VBoxApplianceEditorWgt::tr ("Network Adapter"); break;
                        case KVirtualSystemDescriptionType_USBController: v = VBoxApplianceEditorWgt::tr ("USB Controller"); break;
                        case KVirtualSystemDescriptionType_SoundCard: v = VBoxApplianceEditorWgt::tr ("Sound Card"); break;
                        case KVirtualSystemDescriptionType_HardDiskImage: v = VBoxApplianceEditorWgt::tr ("Virtual Disk Image"); break;
                        default: v = VBoxApplianceEditorWgt::tr ("Unknown Hardware Item"); break;
                    }
                }
                else if (aColumn == OriginalValueSection)
                    v = mOrigValue;
                else if (aColumn == ConfigValueSection)
                {
                    switch (mType)
                    {
                        case KVirtualSystemDescriptionType_Description:
                        case KVirtualSystemDescriptionType_License:
                            {
                                /* Shorten the big text if there is more than
                                 * one line */
                                QString tmp (mConfigValue);
                                tmp.replace (tmp.indexOf ('\n'), tmp.length(), "...");
                                v = tmp; break;
                            }
                        case KVirtualSystemDescriptionType_OS: v = vboxGlobal().vmGuestOSTypeDescription (mConfigValue); break;
                        case KVirtualSystemDescriptionType_Memory: v = mConfigValue + " " + VBoxApplianceEditorWgt::tr ("MB"); break;
                        case KVirtualSystemDescriptionType_SoundCard: v = vboxGlobal().toString (static_cast<KAudioControllerType> (mConfigValue.toInt())); break;
                        case KVirtualSystemDescriptionType_NetworkAdapter: v = vboxGlobal().toString (static_cast<KNetworkAdapterType> (mConfigValue.toInt())); break;
                        default: v = mConfigValue; break;
                    }
                }
                break;
            }
        case Qt::ToolTipRole:
            {
                if (aColumn == ConfigValueSection)
                {
                    if (!mOrigValue.isEmpty())
                        v = VBoxApplianceEditorWgt::tr ("<b>Original Value:</b> %1").arg (mOrigValue);
                }
                break;
            }
        case Qt::DecorationRole:
            {
                if (aColumn == DescriptionSection)
                {
                    switch (mType)
                    {
                        case KVirtualSystemDescriptionType_Name: v = QIcon (":/name_16px.png"); break;
                        case KVirtualSystemDescriptionType_Product:
                        case KVirtualSystemDescriptionType_ProductUrl:
                        case KVirtualSystemDescriptionType_Vendor:
                        case KVirtualSystemDescriptionType_VendorUrl:
                        case KVirtualSystemDescriptionType_Version:
                        case KVirtualSystemDescriptionType_Description:
                        case KVirtualSystemDescriptionType_License: v = QIcon (":/description_16px.png"); break;
                        case KVirtualSystemDescriptionType_OS: v = QIcon (":/os_type_16px.png"); break;
                        case KVirtualSystemDescriptionType_CPU: v = QIcon (":/cpu_16px.png"); break;
                        case KVirtualSystemDescriptionType_Memory: v = QIcon (":/ram_16px.png"); break;
                        case KVirtualSystemDescriptionType_HardDiskControllerIDE: v = QIcon (":/ide_16px.png"); break;
                        case KVirtualSystemDescriptionType_HardDiskControllerSATA: v = QIcon (":/sata_16px.png"); break;
                        case KVirtualSystemDescriptionType_HardDiskControllerSCSI: v = QIcon (":/scsi_16px.png"); break;
                        case KVirtualSystemDescriptionType_HardDiskImage: v = QIcon (":/hd_16px.png"); break;
                        case KVirtualSystemDescriptionType_CDROM: v = QIcon (":/cd_16px.png"); break;
                        case KVirtualSystemDescriptionType_Floppy: v = QIcon (":/fd_16px.png"); break;
                        case KVirtualSystemDescriptionType_NetworkAdapter: v = QIcon (":/nw_16px.png"); break;
                        case KVirtualSystemDescriptionType_USBController: v = QIcon (":/usb_16px.png"); break;
                        case KVirtualSystemDescriptionType_SoundCard: v = QIcon (":/sound_16px.png"); break;
                        default: break;
                    }
                }
                else if (aColumn == ConfigValueSection &&
                         mType == KVirtualSystemDescriptionType_OS)
                {
                    v = vboxGlobal().vmGuestOSTypeIcon (mConfigValue).scaledToHeight (16, Qt::SmoothTransformation);
                }
                break;
            }
        case Qt::FontRole:
            {
                /* If the item is unchecked mark it with italic text. */
                if (aColumn == ConfigValueSection &&
                    mCheckState == Qt::Unchecked)
                {
                    QFont font = qApp->font();
                    font.setItalic (true);
                    v = font;
                }
                break;
            }
        case Qt::ForegroundRole:
            {
                /* If the item is unchecked mark it with gray text. */
                if (aColumn == ConfigValueSection &&
                    mCheckState == Qt::Unchecked)
                {
                    QPalette pal = qApp->palette();
                    v = pal.brush (QPalette::Disabled, QPalette::WindowText);
                }
                break;
            }
        case Qt::CheckStateRole:
            {
                if (aColumn == ConfigValueSection &&
                    (mType == KVirtualSystemDescriptionType_Floppy ||
                     mType == KVirtualSystemDescriptionType_CDROM ||
                     mType == KVirtualSystemDescriptionType_USBController ||
                     mType == KVirtualSystemDescriptionType_SoundCard ||
                     mType == KVirtualSystemDescriptionType_NetworkAdapter))
                    v = mCheckState;
                break;
            }
    }
    return v;
}

Qt::ItemFlags HardwareItem::itemFlags (int aColumn) const
{
    Qt::ItemFlags flags = 0;
    if (aColumn == ConfigValueSection)
    {
        /* Some items are checkable */
        if (mType == KVirtualSystemDescriptionType_Floppy ||
            mType == KVirtualSystemDescriptionType_CDROM ||
            mType == KVirtualSystemDescriptionType_USBController ||
            mType == KVirtualSystemDescriptionType_SoundCard ||
            mType == KVirtualSystemDescriptionType_NetworkAdapter)
            flags |= Qt::ItemIsUserCheckable;
        /* Some items are editable */
        if ((mType == KVirtualSystemDescriptionType_Name ||
             mType == KVirtualSystemDescriptionType_Product ||
             mType == KVirtualSystemDescriptionType_ProductUrl ||
             mType == KVirtualSystemDescriptionType_Vendor ||
             mType == KVirtualSystemDescriptionType_VendorUrl ||
             mType == KVirtualSystemDescriptionType_Version ||
             mType == KVirtualSystemDescriptionType_Description ||
             mType == KVirtualSystemDescriptionType_License ||
             mType == KVirtualSystemDescriptionType_OS ||
             mType == KVirtualSystemDescriptionType_Memory ||
             mType == KVirtualSystemDescriptionType_SoundCard ||
             mType == KVirtualSystemDescriptionType_NetworkAdapter ||
             mType == KVirtualSystemDescriptionType_HardDiskControllerIDE ||
             mType == KVirtualSystemDescriptionType_HardDiskImage) &&
            mCheckState == Qt::Checked) /* Item has to be enabled */
            flags |= Qt::ItemIsEditable;
    }
    return flags;
}

QWidget * HardwareItem::createEditor (QWidget *aParent, const QStyleOptionViewItem & /* aOption */, const QModelIndex &aIndex) const
{
    QWidget *editor = NULL;
    if (aIndex.column() == ConfigValueSection)
    {
        switch (mType)
        {
            case KVirtualSystemDescriptionType_OS:
                {
                    VBoxOSTypeSelectorButton *e = new VBoxOSTypeSelectorButton (aParent);
                    /* Fill the background with the highlight color in the case
                     * the button hasn't a rectangle shape. This prevents the
                     * display of parts from the current text on the Mac. */
                    e->setAutoFillBackground (true);
                    e->setBackgroundRole (QPalette::Highlight);
                    editor = e;
                    break;
                }
            case KVirtualSystemDescriptionType_Name:
            case KVirtualSystemDescriptionType_Product:
            case KVirtualSystemDescriptionType_ProductUrl:
            case KVirtualSystemDescriptionType_Vendor:
            case KVirtualSystemDescriptionType_VendorUrl:
            case KVirtualSystemDescriptionType_Version:
                {
                    QLineEdit *e = new QLineEdit (aParent);
                    editor = e;
                    break;
                }
            case KVirtualSystemDescriptionType_Description:
            case KVirtualSystemDescriptionType_License:
                {
                    VBoxLineTextEdit *e = new VBoxLineTextEdit (aParent);
                    editor = e;
                    break;
                }
            case KVirtualSystemDescriptionType_CPU:
                {
                    QSpinBox *e = new QSpinBox (aParent);
                    e->setRange (VBoxApplianceEditorWgt::minGuestCPUCount(), VBoxApplianceEditorWgt::maxGuestCPUCount());
                    editor = e;
                    break;
                }
            case KVirtualSystemDescriptionType_Memory:
                {
                    QSpinBox *e = new QSpinBox (aParent);
                    e->setRange (VBoxApplianceEditorWgt::minGuestRAM(), VBoxApplianceEditorWgt::maxGuestRAM());
                    e->setSuffix (" " + VBoxApplianceEditorWgt::tr ("MB"));
                    editor = e;
                    break;
                }
            case KVirtualSystemDescriptionType_SoundCard:
                {
                    QComboBox *e = new QComboBox (aParent);
                    e->addItem (vboxGlobal().toString (KAudioControllerType_AC97), KAudioControllerType_AC97);
                    e->addItem (vboxGlobal().toString (KAudioControllerType_SB16), KAudioControllerType_SB16);
                    editor = e;
                    break;
                }
            case KVirtualSystemDescriptionType_NetworkAdapter:
                {
                    QComboBox *e = new QComboBox (aParent);
                    e->addItem (vboxGlobal().toString (KNetworkAdapterType_Am79C970A), KNetworkAdapterType_Am79C970A);
                    e->addItem (vboxGlobal().toString (KNetworkAdapterType_Am79C973), KNetworkAdapterType_Am79C973);
#ifdef VBOX_WITH_E1000
                    e->addItem (vboxGlobal().toString (KNetworkAdapterType_I82540EM), KNetworkAdapterType_I82540EM);
                    e->addItem (vboxGlobal().toString (KNetworkAdapterType_I82543GC), KNetworkAdapterType_I82543GC);
#endif /* VBOX_WITH_E1000 */
                    editor = e;
                    break;
                }
            case KVirtualSystemDescriptionType_HardDiskControllerIDE:
                {
                    QComboBox *e = new QComboBox (aParent);
                    e->addItem (vboxGlobal().toString (KStorageControllerType_PIIX3), "PIIX3");
                    e->addItem (vboxGlobal().toString (KStorageControllerType_PIIX4), "PIIX4");
                    e->addItem (vboxGlobal().toString (KStorageControllerType_ICH6),  "ICH6");
                    editor = e;
                    break;
                }
            case KVirtualSystemDescriptionType_HardDiskImage:
                {
                    /* disabled for now
                       VBoxFilePathSelectorWidget *e = new VBoxFilePathSelectorWidget (aParent);
                       e->setMode (VBoxFilePathSelectorWidget::Mode_File);
                       e->setResetEnabled (false);
                       */
                    QLineEdit *e = new QLineEdit (aParent);
                    editor = e;
                    break;
                }
            default: break;
        }
    }
    return editor;
}

bool HardwareItem::setEditorData (QWidget *aEditor, const QModelIndex & /* aIndex */) const
{
    bool fDone = false;
    switch (mType)
    {
        case KVirtualSystemDescriptionType_OS:
            {
                if (VBoxOSTypeSelectorButton *e = qobject_cast<VBoxOSTypeSelectorButton*> (aEditor))
                {
                    e->setOSTypeId (mConfigValue);
                    fDone = true;
                }
                break;
            }
        case KVirtualSystemDescriptionType_HardDiskControllerIDE:
            {
                if (QComboBox *e = qobject_cast<QComboBox*> (aEditor))
                {
                    int i = e->findData (mConfigValue);
                    if (i != -1)
                        e->setCurrentIndex (i);
                    fDone = true;
                }
                break;
            }
        case KVirtualSystemDescriptionType_CPU:
        case KVirtualSystemDescriptionType_Memory:
            {
                if (QSpinBox *e = qobject_cast<QSpinBox*> (aEditor))
                {
                    e->setValue (mConfigValue.toInt());
                    fDone = true;
                }
                break;
            }
        case KVirtualSystemDescriptionType_Name:
        case KVirtualSystemDescriptionType_Product:
        case KVirtualSystemDescriptionType_ProductUrl:
        case KVirtualSystemDescriptionType_Vendor:
        case KVirtualSystemDescriptionType_VendorUrl:
        case KVirtualSystemDescriptionType_Version:
            {
                if (QLineEdit *e = qobject_cast<QLineEdit*> (aEditor))
                {
                    e->setText (mConfigValue);
                    fDone = true;
                }
                break;
            }
        case KVirtualSystemDescriptionType_Description:
        case KVirtualSystemDescriptionType_License:
            {
                if (VBoxLineTextEdit *e = qobject_cast<VBoxLineTextEdit*> (aEditor))
                {
                    e->setText (mConfigValue);
                    fDone = true;
                }
                break;
            }
        case KVirtualSystemDescriptionType_SoundCard:
        case KVirtualSystemDescriptionType_NetworkAdapter:
            {
                if (QComboBox *e = qobject_cast<QComboBox*> (aEditor))
                {
                    int i = e->findData (mConfigValue.toInt());
                    if (i != -1)
                        e->setCurrentIndex (i);
                    fDone = true;
                }
                break;
            }
        case KVirtualSystemDescriptionType_HardDiskImage:
            {
                /* disabled for now
                   if (VBoxFilePathSelectorWidget *e = qobject_cast<VBoxFilePathSelectorWidget*> (aEditor))
                   {
                   e->setPath (mConfigValue);
                   }
                   */
                if (QLineEdit *e = qobject_cast<QLineEdit*> (aEditor))
                {
                    e->setText (mConfigValue);
                    fDone = true;
                }
                break;
            }
        default: break;
    }
    return fDone;
}

bool HardwareItem::setModelData (QWidget *aEditor, QAbstractItemModel * /* aModel */, const QModelIndex & /* aIndex */)
{
    bool fDone = false;
    switch (mType)
    {
        case KVirtualSystemDescriptionType_OS:
            {
                if (VBoxOSTypeSelectorButton *e = qobject_cast<VBoxOSTypeSelectorButton*> (aEditor))
                {
                    mConfigValue = e->osTypeId();
                    fDone = true;
                }
                break;
            }
        case KVirtualSystemDescriptionType_HardDiskControllerIDE:
            {
                if (QComboBox *e = qobject_cast<QComboBox*> (aEditor))
                {
                    mConfigValue = e->itemData (e->currentIndex()).toString();
                    fDone = true;
                }
                break;
            }
        case KVirtualSystemDescriptionType_CPU:
        case KVirtualSystemDescriptionType_Memory:
            {
                if (QSpinBox *e = qobject_cast<QSpinBox*> (aEditor))
                {
                    mConfigValue = QString::number (e->value());
                    fDone = true;
                }
                break;
            }
        case KVirtualSystemDescriptionType_Name:
        case KVirtualSystemDescriptionType_Product:
        case KVirtualSystemDescriptionType_ProductUrl:
        case KVirtualSystemDescriptionType_Vendor:
        case KVirtualSystemDescriptionType_VendorUrl:
        case KVirtualSystemDescriptionType_Version:
            {
                if (QLineEdit *e = qobject_cast<QLineEdit*> (aEditor))
                {
                    mConfigValue = e->text();
                    fDone = true;
                }
                break;
            }
        case KVirtualSystemDescriptionType_Description:
        case KVirtualSystemDescriptionType_License:
            {
                if (VBoxLineTextEdit *e = qobject_cast<VBoxLineTextEdit*> (aEditor))
                {
                    mConfigValue = e->text();
                    fDone = true;
                }
                break;
            }
        case KVirtualSystemDescriptionType_SoundCard:
        case KVirtualSystemDescriptionType_NetworkAdapter:
            {
                if (QComboBox *e = qobject_cast<QComboBox*> (aEditor))
                {
                    mConfigValue = e->itemData (e->currentIndex()).toString();
                    fDone = true;
                }
                break;
            }
        case KVirtualSystemDescriptionType_HardDiskImage:
            {
                /* disabled for now
                   if (VBoxFilePathSelectorWidget *e = qobject_cast<VBoxFilePathSelectorWidget*> (aEditor))
                   {
                   mConfigValue = e->path();
                   }
                   */
                if (QLineEdit *e = qobject_cast<QLineEdit*> (aEditor))
                {
                    mConfigValue = e->text();
                    fDone = true;
                }
                break;
            }
        default: break;
    }
    return fDone;
}

////////////////////////////////////////////////////////////////////////////////
// VirtualSystemModel

/* This class is a wrapper model for our ModelItem. It could be used with any
   TreeView & forward mostly all calls to the methods of ModelItem. The
   ModelItems itself are stored as internal pointers in the QModelIndex class. */
VirtualSystemModel::VirtualSystemModel (QVector<CVirtualSystemDescription>& aVSDs, QObject *aParent /* = NULL */)
   : QAbstractItemModel (aParent)
{
    mRootItem = new ModelItem (0, RootType);
    for (int a = 0; a < aVSDs.size(); ++a)
    {
        CVirtualSystemDescription vs = aVSDs[a];

        VirtualSystemItem *vi = new VirtualSystemItem (a, vs, mRootItem);
        mRootItem->appendChild (vi);

        /* @todo: ask Dmitry about include/COMDefs.h:232 */
        QVector<KVirtualSystemDescriptionType> types;
        QVector<QString> refs;
        QVector<QString> origValues;
        QVector<QString> configValues;
        QVector<QString> extraConfigValues;

        QList<int> hdIndizies;
        QMap<int, HardwareItem*> controllerMap;
        vs.GetDescription (types, refs, origValues, configValues, extraConfigValues);
        for (int i = 0; i < types.size(); ++i)
        {
            /* We add the hard disk images in an second step, so save a
               reference to them. */
            if (types[i] == KVirtualSystemDescriptionType_HardDiskImage)
                hdIndizies << i;
            else
            {
                HardwareItem *hi = new HardwareItem (i, types[i], refs[i], origValues[i], configValues[i], extraConfigValues[i], vi);
                vi->appendChild (hi);
                /* Save the hard disk controller types in an extra map */
                if (types[i] == KVirtualSystemDescriptionType_HardDiskControllerIDE ||
                    types[i] == KVirtualSystemDescriptionType_HardDiskControllerSATA ||
                    types[i] == KVirtualSystemDescriptionType_HardDiskControllerSCSI)
                    controllerMap[i] = hi;
            }
        }
        QRegExp rx ("controller=(\\d+);?");
        /* Now process the hard disk images */
        for (int a = 0; a < hdIndizies.size(); ++a)
        {
            int i = hdIndizies[a];
            QString ecnf = extraConfigValues[i];
            if (rx.indexIn (ecnf) != -1)
            {
                /* Get the controller */
                HardwareItem *ci = controllerMap[rx.cap (1).toInt()];
                if (ci)
                {
                    /* New hardware item as child of the controller */
                    HardwareItem *hi = new HardwareItem (i, types[i], refs[i], origValues[i], configValues[i], extraConfigValues[i], ci);
                    ci->appendChild (hi);
                }
            }
        }
    }
}

QModelIndex VirtualSystemModel::index (int aRow, int aColumn, const QModelIndex &aParent /* = QModelIndex() */) const
{
    if (!hasIndex (aRow, aColumn, aParent))
        return QModelIndex();

    ModelItem *parentItem;

    if (!aParent.isValid())
        parentItem = mRootItem;
    else
        parentItem = static_cast<ModelItem*> (aParent.internalPointer());

    ModelItem *childItem = parentItem->child (aRow);
    if (childItem)
        return createIndex (aRow, aColumn, childItem);
    else
        return QModelIndex();
}

QModelIndex VirtualSystemModel::parent (const QModelIndex &aIndex) const
{
    if (!aIndex.isValid())
        return QModelIndex();

    ModelItem *childItem = static_cast<ModelItem*> (aIndex.internalPointer());
    ModelItem *parentItem = childItem->parent();

    if (parentItem == mRootItem)
        return QModelIndex();

    return createIndex (parentItem->row(), 0, parentItem);
}

int VirtualSystemModel::rowCount (const QModelIndex &aParent /* = QModelIndex() */) const
{
    ModelItem *parentItem;
    if (aParent.column() > 0)
        return 0;

    if (!aParent.isValid())
        parentItem = mRootItem;
    else
        parentItem = static_cast<ModelItem*> (aParent.internalPointer());

    return parentItem->childCount();
}

int VirtualSystemModel::columnCount (const QModelIndex &aParent /* = QModelIndex() */) const
{
    if (aParent.isValid())
        return static_cast<ModelItem*> (aParent.internalPointer())->columnCount();
    else
        return mRootItem->columnCount();
}

bool VirtualSystemModel::setData (const QModelIndex &aIndex, const QVariant &aValue, int aRole)
{
    if (!aIndex.isValid())
        return false;

    ModelItem *item = static_cast<ModelItem*> (aIndex.internalPointer());

    return item->setData (aIndex.column(), aValue, aRole);
}

QVariant VirtualSystemModel::data (const QModelIndex &aIndex, int aRole /* = Qt::DisplayRole */) const
{
    if (!aIndex.isValid())
        return QVariant();

    ModelItem *item = static_cast<ModelItem*> (aIndex.internalPointer());

    return item->data (aIndex.column(), aRole);
}

Qt::ItemFlags VirtualSystemModel::flags (const QModelIndex &aIndex) const
{
    if (!aIndex.isValid())
        return 0;

    ModelItem *item = static_cast<ModelItem*> (aIndex.internalPointer());

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | item->itemFlags (aIndex.column());
}

QVariant VirtualSystemModel::headerData (int aSection, Qt::Orientation aOrientation, int aRole) const
{
    if (aRole != Qt::DisplayRole ||
        aOrientation != Qt::Horizontal)
        return QVariant();

    QString title;
    switch (aSection)
    {
        case DescriptionSection: title = VBoxApplianceEditorWgt::tr ("Description"); break;
        case ConfigValueSection: title = VBoxApplianceEditorWgt::tr ("Configuration"); break;
    }
    return title;
}

void VirtualSystemModel::restoreDefaults (const QModelIndex& aParent /* = QModelIndex() */)
{
    ModelItem *parentItem;

    if (!aParent.isValid())
        parentItem = mRootItem;
    else
        parentItem = static_cast<ModelItem*> (aParent.internalPointer());

    for (int i = 0; i < parentItem->childCount(); ++i)
    {
        parentItem->child (i)->restoreDefaults();
        restoreDefaults (index (i, 0, aParent));
    }
    emit dataChanged (index (0, 0, aParent), index (parentItem->childCount()-1, 0, aParent));
}

void VirtualSystemModel::putBack()
{
    QVector<BOOL> v1;
    QVector<QString> v2;
    QVector<QString> v3;
    mRootItem->putBack (v1, v2, v3);
}

////////////////////////////////////////////////////////////////////////////////
// VirtualSystemDelegate

/* The delegate is used for creating/handling the different editors for the
   various types we support. This class forward the requests to the virtual
   methods of our different ModelItems. If this is not possible the default
   methods of QItemDelegate are used to get some standard behavior. Note: We
   have to handle the proxy model ourself. I really don't understand why Qt is
   not doing this for us. */
VirtualSystemDelegate::VirtualSystemDelegate (QAbstractProxyModel *aProxy, QObject *aParent /* = NULL */)
  : QItemDelegate (aParent)
  , mProxy (aProxy)
{}

QWidget * VirtualSystemDelegate::createEditor (QWidget *aParent, const QStyleOptionViewItem &aOption, const QModelIndex &aIndex) const
{
    if (!aIndex.isValid())
        return QItemDelegate::createEditor (aParent, aOption, aIndex);

    QModelIndex index (aIndex);
    if (mProxy)
        index = mProxy->mapToSource (aIndex);

    ModelItem *item = static_cast<ModelItem*> (index.internalPointer());
    QWidget *editor = item->createEditor (aParent, aOption, index);

    if (editor == NULL)
        return QItemDelegate::createEditor (aParent, aOption, index);
    else
        return editor;
}

void VirtualSystemDelegate::setEditorData (QWidget *aEditor, const QModelIndex &aIndex) const
{
    if (!aIndex.isValid())
        return QItemDelegate::setEditorData (aEditor, aIndex);

    QModelIndex index (aIndex);
    if (mProxy)
        index = mProxy->mapToSource (aIndex);

    ModelItem *item = static_cast<ModelItem*> (index.internalPointer());

    if (!item->setEditorData (aEditor, index))
        QItemDelegate::setEditorData (aEditor, index);
}

void VirtualSystemDelegate::setModelData (QWidget *aEditor, QAbstractItemModel *aModel, const QModelIndex &aIndex) const
{
    if (!aIndex.isValid())
        return QItemDelegate::setModelData (aEditor, aModel, aIndex);

    QModelIndex index = aModel->index (aIndex.row(), aIndex.column());
    if (mProxy)
        index = mProxy->mapToSource (aIndex);

    ModelItem *item = static_cast<ModelItem*> (index.internalPointer());
    if (!item->setModelData (aEditor, aModel, index))
        QItemDelegate::setModelData (aEditor, aModel, aIndex);
}

void VirtualSystemDelegate::updateEditorGeometry (QWidget *aEditor, const QStyleOptionViewItem &aOption, const QModelIndex & /* aIndex */) const
{
    if (aEditor)
        aEditor->setGeometry (aOption.rect);
}

////////////////////////////////////////////////////////////////////////////////
// VirtualSystemSortProxyModel

/* How to sort the items in the tree view */
KVirtualSystemDescriptionType VirtualSystemSortProxyModel::mSortList[] =
{
    KVirtualSystemDescriptionType_Name,
    KVirtualSystemDescriptionType_Product,
    KVirtualSystemDescriptionType_ProductUrl,
    KVirtualSystemDescriptionType_Vendor,
    KVirtualSystemDescriptionType_VendorUrl,
    KVirtualSystemDescriptionType_Version,
    KVirtualSystemDescriptionType_Description,
    KVirtualSystemDescriptionType_License,
    KVirtualSystemDescriptionType_OS,
    KVirtualSystemDescriptionType_CPU,
    KVirtualSystemDescriptionType_Memory,
    KVirtualSystemDescriptionType_Floppy,
    KVirtualSystemDescriptionType_CDROM,
    KVirtualSystemDescriptionType_USBController,
    KVirtualSystemDescriptionType_SoundCard,
    KVirtualSystemDescriptionType_NetworkAdapter,
    KVirtualSystemDescriptionType_HardDiskControllerIDE,
    KVirtualSystemDescriptionType_HardDiskControllerSATA,
    KVirtualSystemDescriptionType_HardDiskControllerSCSI
};

VirtualSystemSortProxyModel::VirtualSystemSortProxyModel (QObject *aParent)
    : QSortFilterProxyModel (aParent)
{}

bool VirtualSystemSortProxyModel::filterAcceptsRow (int aSourceRow, const QModelIndex & aSourceParent) const
{
    /* By default enable all, we will explicitly filter out below */
    if (aSourceParent.isValid())
    {
        QModelIndex i = aSourceParent.child (aSourceRow, 0);
        if (i.isValid())
        {
            ModelItem *item = static_cast<ModelItem*> (i.internalPointer());
            /* We filter hardware types only */
            if (item->type() == HardwareType)
            {
                HardwareItem *hwItem = static_cast<HardwareItem*> (item);
                /* The license type shouldn't be displayed */
                if (mFilterList.contains (hwItem->mType))
                    return false;
            }
        }
    }
    return true;
}

bool VirtualSystemSortProxyModel::lessThan (const QModelIndex &aLeft, const QModelIndex &aRight) const
{
    if (!aLeft.isValid() ||
        !aRight.isValid())
        return false;

    ModelItem *leftItem = static_cast<ModelItem*> (aLeft.internalPointer());
    ModelItem *rightItem = static_cast<ModelItem*> (aRight.internalPointer());

    /* We sort hardware types only */
    if (!(leftItem->type() == HardwareType &&
          rightItem->type() == HardwareType))
        return false;

    HardwareItem *hwLeft = static_cast<HardwareItem*> (leftItem);
    HardwareItem *hwRight = static_cast<HardwareItem*> (rightItem);

    for (unsigned int i = 0; i < RT_ELEMENTS (mSortList); ++i)
        if (hwLeft->mType == mSortList[i])
        {
            for (unsigned int a = 0; a <= i; ++a)
                if (hwRight->mType == mSortList[a])
                    return true;
            return false;
        }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// VBoxApplianceEditorWgt

int VBoxApplianceEditorWgt::mMinGuestRAM = -1;
int VBoxApplianceEditorWgt::mMaxGuestRAM = -1;
int VBoxApplianceEditorWgt::mMinGuestCPUCount = -1;
int VBoxApplianceEditorWgt::mMaxGuestCPUCount = -1;

VBoxApplianceEditorWgt::VBoxApplianceEditorWgt (QWidget *aParent /* = NULL */)
    : QIWithRetranslateUI<QWidget> (aParent)
    , mAppliance (NULL)
    , mModel (NULL)
{
    /* Make sure all static content is properly initialized */
    initSystemSettings();

    /* Apply UI decorations */
    Ui::VBoxApplianceEditorWgt::setupUi (this);

    /* Make the tree looking nicer */
    mTvSettings->setRootIsDecorated (false);
    mTvSettings->setAlternatingRowColors (true);
    mTvSettings->header()->setStretchLastSection (true);
    mTvSettings->header()->setResizeMode (QHeaderView::ResizeToContents);

    /* Applying language settings */
    retranslateUi();
}

void VBoxApplianceEditorWgt::restoreDefaults()
{
    mModel->restoreDefaults();
}

void VBoxApplianceEditorWgt::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxApplianceEditorWgt::retranslateUi (this);
}

/* static */
void VBoxApplianceEditorWgt::initSystemSettings()
{
    if (mMinGuestRAM == -1)
    {
        /* We need some global defaults from the current VirtualBox
           installation */
        CSystemProperties sp = vboxGlobal().virtualBox().GetSystemProperties();
        mMinGuestRAM = sp.GetMinGuestRAM();
        mMaxGuestRAM = sp.GetMaxGuestRAM();
        mMinGuestCPUCount = sp.GetMinGuestCPUCount();
        mMaxGuestCPUCount = sp.GetMaxGuestCPUCount();
    }
}

