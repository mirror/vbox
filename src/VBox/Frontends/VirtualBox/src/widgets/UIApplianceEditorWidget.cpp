/* $Id$ */
/** @file
 * VBox Qt GUI - UIApplianceEditorWidget class implementation.
 */

/*
 * Copyright (C) 2009-2016 Oracle Corporation
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
# include <QItemDelegate>
# include <QSortFilterProxyModel>
# include <QHeaderView>
# include <QLineEdit>
# include <QSpinBox>
# include <QComboBox>
# include <QDir>
# include <QTreeView>
# include <QCheckBox>
# include <QLabel>
# include <QTextEdit>

/* GUI includes: */
# include "UIApplianceEditorWidget.h"
# include "VBoxGlobal.h"
# include "UIMessageCenter.h"
# include "VBoxOSTypeSelectorButton.h"
# include "UILineTextEdit.h"
# include "UIConverter.h"
# include "UIIconPool.h"

/* COM includes: */
# include "CSystemProperties.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


////////////////////////////////////////////////////////////////////////////////
// ModelItem

/* This & the following derived classes represent the data items of a Virtual
   System. All access/manipulation is done with the help of virtual functions
   to keep the interface clean. ModelItem is able to handle tree structures
   with a parent & several children's. */
ModelItem::ModelItem(int number, ApplianceModelItemType type, ModelItem *pParent /* = NULL */)
  : m_number(number)
  , m_type(type)
  , m_pParentItem(pParent)
{}

ModelItem::~ModelItem()
{
    qDeleteAll(m_childItems);
}

void ModelItem::appendChild(ModelItem *pChild)
{
    AssertPtr(pChild);
    m_childItems << pChild;
}

ModelItem *ModelItem::child(int row) const
{
    return m_childItems.value(row);
}

int ModelItem::row() const
{
    if (m_pParentItem)
        return m_pParentItem->m_childItems.indexOf(const_cast<ModelItem*>(this));

    return 0;
}

int ModelItem::childCount() const
{
    return m_childItems.count();
}

void ModelItem::putBack(QVector<BOOL>& finalStates, QVector<QString>& finalValues, QVector<QString>& finalExtraValues)
{
    for (int i = 0; i < childCount(); ++i)
        child(i)->putBack(finalStates, finalValues, finalExtraValues);
}

////////////////////////////////////////////////////////////////////////////////
// VirtualSystemItem

VirtualSystemItem::VirtualSystemItem(int number, CVirtualSystemDescription aDesc, ModelItem *pParent)
  : ModelItem(number, ApplianceModelItemType_VirtualSystem, pParent)
  , m_desc(aDesc)
{}

QVariant VirtualSystemItem::data(int column, int role) const
{
    QVariant v;
    if (column == ApplianceViewSection_Description &&
        role == Qt::DisplayRole)
        v = UIApplianceEditorWidget::tr("Virtual System %1").arg(m_number + 1);
    return v;
}

void VirtualSystemItem::putBack(QVector<BOOL>& finalStates, QVector<QString>& finalValues, QVector<QString>& finalExtraValues)
{
    /* Resize the vectors */
    unsigned long count = m_desc.GetCount();
    finalStates.resize(count);
    finalValues.resize(count);
    finalExtraValues.resize(count);
    /* Recursively fill the vectors */
    ModelItem::putBack(finalStates, finalValues, finalExtraValues);
    /* Set all final values at once */
    m_desc.SetFinalValues(finalStates, finalValues, finalExtraValues);
}

////////////////////////////////////////////////////////////////////////////////
// HardwareItem

HardwareItem::HardwareItem(int number,
                           KVirtualSystemDescriptionType type,
                           const QString &strRef,
                           const QString &aOrigValue,
                           const QString &strConfigValue,
                           const QString &strExtraConfigValue,
                           ModelItem *pParent)
  : ModelItem(number, ApplianceModelItemType_Hardware, pParent)
  , m_type(type)
  , m_strRef(strRef)
  , m_strOrigValue(aOrigValue)
  , m_strConfigValue(strConfigValue)
  , m_strConfigDefaultValue(strConfigValue)
  , m_strExtraConfigValue(strExtraConfigValue)
  , m_checkState(Qt::Checked)
  , m_fModified(false)
{}

void HardwareItem::putBack(QVector<BOOL>& finalStates, QVector<QString>& finalValues, QVector<QString>& finalExtraValues)
{
    finalStates[m_number]      = m_checkState == Qt::Checked;
    finalValues[m_number]      = m_strConfigValue;
    finalExtraValues[m_number] = m_strExtraConfigValue;
    ModelItem::putBack(finalStates, finalValues, finalExtraValues);
}

bool HardwareItem::setData(int column, const QVariant &value, int role)
{
    bool fDone = false;
    switch (role)
    {
        case Qt::CheckStateRole:
        {
            if (column == ApplianceViewSection_ConfigValue &&
                (m_type == KVirtualSystemDescriptionType_Floppy ||
                 m_type == KVirtualSystemDescriptionType_CDROM ||
                 m_type == KVirtualSystemDescriptionType_USBController ||
                 m_type == KVirtualSystemDescriptionType_SoundCard ||
                 m_type == KVirtualSystemDescriptionType_NetworkAdapter))
            {
                m_checkState = static_cast<Qt::CheckState>(value.toInt());
                fDone = true;
            }
            break;
        }
        case Qt::EditRole:
        {
            if (column == ApplianceViewSection_OriginalValue)
                m_strOrigValue = value.toString();
            else if (column == ApplianceViewSection_ConfigValue)
                m_strConfigValue = value.toString();
            break;
        }
        default: break;
    }
    return fDone;
}

QVariant HardwareItem::data(int column, int role) const
{
    QVariant v;
    switch (role)
    {
        case Qt::EditRole:
        {
            if (column == ApplianceViewSection_OriginalValue)
                v = m_strOrigValue;
            else if (column == ApplianceViewSection_ConfigValue)
                v = m_strConfigValue;
            break;
        }
        case Qt::DisplayRole:
        {
            if (column == ApplianceViewSection_Description)
            {
                switch (m_type)
                {
                    case KVirtualSystemDescriptionType_Name:                   v = UIApplianceEditorWidget::tr("Name"); break;
                    case KVirtualSystemDescriptionType_Product:                v = UIApplianceEditorWidget::tr("Product"); break;
                    case KVirtualSystemDescriptionType_ProductUrl:             v = UIApplianceEditorWidget::tr("Product-URL"); break;
                    case KVirtualSystemDescriptionType_Vendor:                 v = UIApplianceEditorWidget::tr("Vendor"); break;
                    case KVirtualSystemDescriptionType_VendorUrl:              v = UIApplianceEditorWidget::tr("Vendor-URL"); break;
                    case KVirtualSystemDescriptionType_Version:                v = UIApplianceEditorWidget::tr("Version"); break;
                    case KVirtualSystemDescriptionType_Description:            v = UIApplianceEditorWidget::tr("Description"); break;
                    case KVirtualSystemDescriptionType_License:                v = UIApplianceEditorWidget::tr("License"); break;
                    case KVirtualSystemDescriptionType_OS:                     v = UIApplianceEditorWidget::tr("Guest OS Type"); break;
                    case KVirtualSystemDescriptionType_CPU:                    v = UIApplianceEditorWidget::tr("CPU"); break;
                    case KVirtualSystemDescriptionType_Memory:                 v = UIApplianceEditorWidget::tr("RAM"); break;
                    case KVirtualSystemDescriptionType_HardDiskControllerIDE:  v = UIApplianceEditorWidget::tr("Storage Controller (IDE)"); break;
                    case KVirtualSystemDescriptionType_HardDiskControllerSATA: v = UIApplianceEditorWidget::tr("Storage Controller (SATA)"); break;
                    case KVirtualSystemDescriptionType_HardDiskControllerSCSI: v = UIApplianceEditorWidget::tr("Storage Controller (SCSI)"); break;
                    case KVirtualSystemDescriptionType_HardDiskControllerSAS:  v = UIApplianceEditorWidget::tr("Storage Controller (SAS)"); break;
                    case KVirtualSystemDescriptionType_CDROM:                  v = UIApplianceEditorWidget::tr("DVD"); break;
                    case KVirtualSystemDescriptionType_Floppy:                 v = UIApplianceEditorWidget::tr("Floppy"); break;
                    case KVirtualSystemDescriptionType_NetworkAdapter:         v = UIApplianceEditorWidget::tr("Network Adapter"); break;
                    case KVirtualSystemDescriptionType_USBController:          v = UIApplianceEditorWidget::tr("USB Controller"); break;
                    case KVirtualSystemDescriptionType_SoundCard:              v = UIApplianceEditorWidget::tr("Sound Card"); break;
                    case KVirtualSystemDescriptionType_HardDiskImage:          v = UIApplianceEditorWidget::tr("Virtual Disk Image"); break;
                    default:                                                   v = UIApplianceEditorWidget::tr("Unknown Hardware Item"); break;
                }
            }
            else if (column == ApplianceViewSection_OriginalValue)
                v = m_strOrigValue;
            else if (column == ApplianceViewSection_ConfigValue)
            {
                switch (m_type)
                {
                    case KVirtualSystemDescriptionType_Description:
                    case KVirtualSystemDescriptionType_License:
                    {
                        /* Shorten the big text if there is more than
                         * one line */
                        QString tmp(m_strConfigValue);
                        int i = tmp.indexOf('\n');
                        if (i > -1)
                            tmp.replace(i, tmp.length(), "...");
                        v = tmp; break;
                    }
                    case KVirtualSystemDescriptionType_OS:             v = vboxGlobal().vmGuestOSTypeDescription(m_strConfigValue); break;
                    case KVirtualSystemDescriptionType_Memory:         v = m_strConfigValue + " " + VBoxGlobal::tr("MB", "size suffix MBytes=1024 KBytes"); break;
                    case KVirtualSystemDescriptionType_SoundCard:      v = gpConverter->toString(static_cast<KAudioControllerType>(m_strConfigValue.toInt())); break;
                    case KVirtualSystemDescriptionType_NetworkAdapter: v = gpConverter->toString(static_cast<KNetworkAdapterType>(m_strConfigValue.toInt())); break;
                    default:                                           v = m_strConfigValue; break;
                }
            }
            break;
        }
        case Qt::ToolTipRole:
        {
            if (column == ApplianceViewSection_ConfigValue)
            {
                if (!m_strOrigValue.isEmpty())
                    v = UIApplianceEditorWidget::tr("<b>Original Value:</b> %1").arg(m_strOrigValue);
            }
            break;
        }
        case Qt::DecorationRole:
        {
            if (column == ApplianceViewSection_Description)
            {
                switch (m_type)
                {
                    case KVirtualSystemDescriptionType_Name:                   v = UIIconPool::iconSet(":/name_16px.png"); break;
                    case KVirtualSystemDescriptionType_Product:
                    case KVirtualSystemDescriptionType_ProductUrl:
                    case KVirtualSystemDescriptionType_Vendor:
                    case KVirtualSystemDescriptionType_VendorUrl:
                    case KVirtualSystemDescriptionType_Version:
                    case KVirtualSystemDescriptionType_Description:
                    case KVirtualSystemDescriptionType_License:                v = UIIconPool::iconSet(":/description_16px.png"); break;
                    case KVirtualSystemDescriptionType_OS:                     v = UIIconPool::iconSet(":/os_type_16px.png"); break;
                    case KVirtualSystemDescriptionType_CPU:                    v = UIIconPool::iconSet(":/cpu_16px.png"); break;
                    case KVirtualSystemDescriptionType_Memory:                 v = UIIconPool::iconSet(":/ram_16px.png"); break;
                    case KVirtualSystemDescriptionType_HardDiskControllerIDE:  v = UIIconPool::iconSet(":/ide_16px.png"); break;
                    case KVirtualSystemDescriptionType_HardDiskControllerSATA: v = UIIconPool::iconSet(":/sata_16px.png"); break;
                    case KVirtualSystemDescriptionType_HardDiskControllerSCSI: v = UIIconPool::iconSet(":/scsi_16px.png"); break;
                    case KVirtualSystemDescriptionType_HardDiskControllerSAS:  v = UIIconPool::iconSet(":/scsi_16px.png"); break;
                    case KVirtualSystemDescriptionType_HardDiskImage:          v = UIIconPool::iconSet(":/hd_16px.png"); break;
                    case KVirtualSystemDescriptionType_CDROM:                  v = UIIconPool::iconSet(":/cd_16px.png"); break;
                    case KVirtualSystemDescriptionType_Floppy:                 v = UIIconPool::iconSet(":/fd_16px.png"); break;
                    case KVirtualSystemDescriptionType_NetworkAdapter:         v = UIIconPool::iconSet(":/nw_16px.png"); break;
                    case KVirtualSystemDescriptionType_USBController:          v = UIIconPool::iconSet(":/usb_16px.png"); break;
                    case KVirtualSystemDescriptionType_SoundCard:              v = UIIconPool::iconSet(":/sound_16px.png"); break;
                    default: break;
                }
            }
            else if (column == ApplianceViewSection_ConfigValue &&
                     m_type == KVirtualSystemDescriptionType_OS)
            {
                const QStyle *pStyle = QApplication::style();
                const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);
                v = vboxGlobal().vmGuestOSTypeIcon(m_strConfigValue).scaledToHeight(iIconMetric, Qt::SmoothTransformation);
            }
            break;
        }
        case Qt::FontRole:
        {
            /* If the item is unchecked mark it with italic text. */
            if (column == ApplianceViewSection_ConfigValue &&
                m_checkState == Qt::Unchecked)
            {
                QFont font = qApp->font();
                font.setItalic(true);
                v = font;
            }
            break;
        }
        case Qt::ForegroundRole:
        {
            /* If the item is unchecked mark it with gray text. */
            if (column == ApplianceViewSection_ConfigValue &&
                m_checkState == Qt::Unchecked)
            {
                QPalette pal = qApp->palette();
                v = pal.brush(QPalette::Disabled, QPalette::WindowText);
            }
            break;
        }
        case Qt::CheckStateRole:
        {
            if (column == ApplianceViewSection_ConfigValue &&
                (m_type == KVirtualSystemDescriptionType_Floppy ||
                 m_type == KVirtualSystemDescriptionType_CDROM ||
                 m_type == KVirtualSystemDescriptionType_USBController ||
                 m_type == KVirtualSystemDescriptionType_SoundCard ||
                 m_type == KVirtualSystemDescriptionType_NetworkAdapter))
                v = m_checkState;
            break;
        }
        case HardwareItem::TypeRole:
        {
            v = m_type;
            break;
        }
        case HardwareItem::ModifiedRole:
        {
            if (column == ApplianceViewSection_ConfigValue)
                v = m_fModified;
            break;
        }
    }
    return v;
}

Qt::ItemFlags HardwareItem::itemFlags(int column) const
{
    Qt::ItemFlags flags = 0;
    if (column == ApplianceViewSection_ConfigValue)
    {
        /* Some items are checkable */
        if (m_type == KVirtualSystemDescriptionType_Floppy ||
            m_type == KVirtualSystemDescriptionType_CDROM ||
            m_type == KVirtualSystemDescriptionType_USBController ||
            m_type == KVirtualSystemDescriptionType_SoundCard ||
            m_type == KVirtualSystemDescriptionType_NetworkAdapter)
            flags |= Qt::ItemIsUserCheckable;
        /* Some items are editable */
        if ((m_type == KVirtualSystemDescriptionType_Name ||
             m_type == KVirtualSystemDescriptionType_Product ||
             m_type == KVirtualSystemDescriptionType_ProductUrl ||
             m_type == KVirtualSystemDescriptionType_Vendor ||
             m_type == KVirtualSystemDescriptionType_VendorUrl ||
             m_type == KVirtualSystemDescriptionType_Version ||
             m_type == KVirtualSystemDescriptionType_Description ||
             m_type == KVirtualSystemDescriptionType_License ||
             m_type == KVirtualSystemDescriptionType_OS ||
             m_type == KVirtualSystemDescriptionType_CPU ||
             m_type == KVirtualSystemDescriptionType_Memory ||
             m_type == KVirtualSystemDescriptionType_SoundCard ||
             m_type == KVirtualSystemDescriptionType_NetworkAdapter ||
             m_type == KVirtualSystemDescriptionType_HardDiskControllerIDE ||
             m_type == KVirtualSystemDescriptionType_HardDiskImage) &&
            m_checkState == Qt::Checked) /* Item has to be enabled */
            flags |= Qt::ItemIsEditable;
    }
    return flags;
}

QWidget *HardwareItem::createEditor(QWidget *pParent, const QStyleOptionViewItem & /* styleOption */, const QModelIndex &idx) const
{
    QWidget *editor = NULL;
    if (idx.column() == ApplianceViewSection_ConfigValue)
    {
        switch (m_type)
        {
            case KVirtualSystemDescriptionType_OS:
            {
                VBoxOSTypeSelectorButton *e = new VBoxOSTypeSelectorButton(pParent);
                /* Fill the background with the highlight color in the case
                 * the button hasn't a rectangle shape. This prevents the
                 * display of parts from the current text on the Mac. */
#ifdef VBOX_WS_MAC
                /* Use the palette from the tree view, not the one from the
                 * editor. */
                QPalette p = e->palette();
                p.setBrush(QPalette::Highlight, pParent->palette().brush(QPalette::Highlight));
                e->setPalette(p);
#endif /* VBOX_WS_MAC */
                e->setAutoFillBackground(true);
                e->setBackgroundRole(QPalette::Highlight);
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
                QLineEdit *e = new QLineEdit(pParent);
                editor = e;
                break;
            }
            case KVirtualSystemDescriptionType_Description:
            case KVirtualSystemDescriptionType_License:
            {
                UILineTextEdit *e = new UILineTextEdit(pParent);
                editor = e;
                break;
            }
            case KVirtualSystemDescriptionType_CPU:
            {
                QSpinBox *e = new QSpinBox(pParent);
                e->setRange(UIApplianceEditorWidget::minGuestCPUCount(), UIApplianceEditorWidget::maxGuestCPUCount());
                editor = e;
                break;
            }
            case KVirtualSystemDescriptionType_Memory:
            {
                QSpinBox *e = new QSpinBox(pParent);
                e->setRange(UIApplianceEditorWidget::minGuestRAM(), UIApplianceEditorWidget::maxGuestRAM());
                e->setSuffix(" " + VBoxGlobal::tr("MB", "size suffix MBytes=1024 KBytes"));
                editor = e;
                break;
            }
            case KVirtualSystemDescriptionType_SoundCard:
            {
                QComboBox *e = new QComboBox(pParent);
                e->addItem(gpConverter->toString(KAudioControllerType_AC97), KAudioControllerType_AC97);
                e->addItem(gpConverter->toString(KAudioControllerType_SB16), KAudioControllerType_SB16);
                e->addItem(gpConverter->toString(KAudioControllerType_HDA),  KAudioControllerType_HDA);
                editor = e;
                break;
            }
            case KVirtualSystemDescriptionType_NetworkAdapter:
            {
                QComboBox *e = new QComboBox(pParent);
                e->addItem(gpConverter->toString(KNetworkAdapterType_Am79C970A), KNetworkAdapterType_Am79C970A);
                e->addItem(gpConverter->toString(KNetworkAdapterType_Am79C973), KNetworkAdapterType_Am79C973);
#ifdef VBOX_WITH_E1000
                e->addItem(gpConverter->toString(KNetworkAdapterType_I82540EM), KNetworkAdapterType_I82540EM);
                e->addItem(gpConverter->toString(KNetworkAdapterType_I82543GC), KNetworkAdapterType_I82543GC);
                e->addItem(gpConverter->toString(KNetworkAdapterType_I82545EM), KNetworkAdapterType_I82545EM);
#endif /* VBOX_WITH_E1000 */
#ifdef VBOX_WITH_VIRTIO
                e->addItem(gpConverter->toString(KNetworkAdapterType_Virtio), KNetworkAdapterType_Virtio);
#endif /* VBOX_WITH_VIRTIO */
                editor = e;
                break;
            }
            case KVirtualSystemDescriptionType_HardDiskControllerIDE:
            {
                QComboBox *e = new QComboBox(pParent);
                e->addItem(gpConverter->toString(KStorageControllerType_PIIX3), "PIIX3");
                e->addItem(gpConverter->toString(KStorageControllerType_PIIX4), "PIIX4");
                e->addItem(gpConverter->toString(KStorageControllerType_ICH6),  "ICH6");
                editor = e;
                break;
            }
            case KVirtualSystemDescriptionType_HardDiskImage:
            {
                /* disabled for now
                   UIFilePathSelector *e = new UIFilePathSelector(pParent);
                   e->setMode(UIFilePathSelector::Mode_File);
                   e->setResetEnabled(false);
                   */
                QLineEdit *e = new QLineEdit(pParent);
                editor = e;
                break;
            }
            default: break;
        }
    }
    return editor;
}

bool HardwareItem::setEditorData(QWidget *pEditor, const QModelIndex & /* idx */) const
{
    bool fDone = false;
    switch (m_type)
    {
        case KVirtualSystemDescriptionType_OS:
        {
            if (VBoxOSTypeSelectorButton *e = qobject_cast<VBoxOSTypeSelectorButton*>(pEditor))
            {
                e->setOSTypeId(m_strConfigValue);
                fDone = true;
            }
            break;
        }
        case KVirtualSystemDescriptionType_HardDiskControllerIDE:
        {
            if (QComboBox *e = qobject_cast<QComboBox*>(pEditor))
            {
                int i = e->findData(m_strConfigValue);
                if (i != -1)
                    e->setCurrentIndex(i);
                fDone = true;
            }
            break;
        }
        case KVirtualSystemDescriptionType_CPU:
        case KVirtualSystemDescriptionType_Memory:
        {
            if (QSpinBox *e = qobject_cast<QSpinBox*>(pEditor))
            {
                e->setValue(m_strConfigValue.toInt());
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
            if (QLineEdit *e = qobject_cast<QLineEdit*>(pEditor))
            {
                e->setText(m_strConfigValue);
                fDone = true;
            }
            break;
        }
        case KVirtualSystemDescriptionType_Description:
        case KVirtualSystemDescriptionType_License:
        {
            if (UILineTextEdit *e = qobject_cast<UILineTextEdit*>(pEditor))
            {
                e->setText(m_strConfigValue);
                fDone = true;
            }
            break;
        }
        case KVirtualSystemDescriptionType_SoundCard:
        case KVirtualSystemDescriptionType_NetworkAdapter:
        {
            if (QComboBox *e = qobject_cast<QComboBox*>(pEditor))
            {
                int i = e->findData(m_strConfigValue.toInt());
                if (i != -1)
                    e->setCurrentIndex(i);
                fDone = true;
            }
            break;
        }
        case KVirtualSystemDescriptionType_HardDiskImage:
        {
            /* disabled for now
               if (UIFilePathSelector *e = qobject_cast<UIFilePathSelector*>(pEditor))
               {
               e->setPath(m_strConfigValue);
               }
               */
            if (QLineEdit *e = qobject_cast<QLineEdit*>(pEditor))
            {
                e->setText(m_strConfigValue);
                fDone = true;
            }
            break;
        }
        default: break;
    }
    return fDone;
}

bool HardwareItem::setModelData(QWidget *pEditor, QAbstractItemModel *pModel, const QModelIndex & idx)
{
    bool fDone = false;
    switch (m_type)
    {
        case KVirtualSystemDescriptionType_OS:
        {
            if (VBoxOSTypeSelectorButton *e = qobject_cast<VBoxOSTypeSelectorButton*>(pEditor))
            {
                m_strConfigValue = e->osTypeId();
                fDone = true;
            }
            break;
        }
        case KVirtualSystemDescriptionType_HardDiskControllerIDE:
        {
            if (QComboBox *e = qobject_cast<QComboBox*>(pEditor))
            {
                m_strConfigValue = e->itemData(e->currentIndex()).toString();
                fDone = true;
            }
            break;
        }
        case KVirtualSystemDescriptionType_CPU:
        case KVirtualSystemDescriptionType_Memory:
        {
            if (QSpinBox *e = qobject_cast<QSpinBox*>(pEditor))
            {
                m_strConfigValue = QString::number(e->value());
                fDone = true;
            }
            break;
        }
        case KVirtualSystemDescriptionType_Name:
        {
            if (QLineEdit *e = qobject_cast<QLineEdit*>(pEditor))
            {
                /* When the VM name is changed the path of the disk images
                 * should be also changed. So first of all find all disk
                 * images corresponding to this appliance. Next check if
                 * they are modified by the user already. If not change the
                 * path to the new path. */
                /* Create an index of this position, but in column 0. */
                QModelIndex c0Index = pModel->index(idx.row(), 0, idx.parent());
                /* Query all items with the type HardDiskImage and which
                 * are child's of this item. */
                QModelIndexList list = pModel->match(c0Index,
                                                     HardwareItem::TypeRole,
                                                     KVirtualSystemDescriptionType_HardDiskImage,
                                                     -1,
                                                     Qt::MatchExactly | Qt::MatchWrap | Qt::MatchRecursive);
                for (int i = 0; i < list.count(); ++i)
                {
                    /* Get the index for the config value column. */
                    QModelIndex hdIndex = pModel->index(list.at(i).row(), ApplianceViewSection_ConfigValue, list.at(i).parent());
                    /* Ignore it if was already modified by the user. */
                    if (!hdIndex.data(ModifiedRole).toBool())
                        /* Replace any occurrence of the old VM name with
                         * the new VM name. */
                    {
                        QStringList splittedOriginalPath = hdIndex.data(Qt::EditRole).toString().split(QDir::separator());
                        QStringList splittedNewPath;

                        foreach (QString a, splittedOriginalPath)
                        {
                            (a.compare(m_strConfigValue) == 0) ? splittedNewPath << e->text() : splittedNewPath << a;
                        }

                        QString newPath = splittedNewPath.join(QDir::separator());

                        pModel->setData(hdIndex,
                                        newPath,
                                        Qt::EditRole);
                    }
                }
                m_strConfigValue = e->text();
                fDone = true;
            }
            break;
        }
        case KVirtualSystemDescriptionType_Product:
        case KVirtualSystemDescriptionType_ProductUrl:
        case KVirtualSystemDescriptionType_Vendor:
        case KVirtualSystemDescriptionType_VendorUrl:
        case KVirtualSystemDescriptionType_Version:
        {
            if (QLineEdit *e = qobject_cast<QLineEdit*>(pEditor))
            {
                m_strConfigValue = e->text();
                fDone = true;
            }
            break;
        }
        case KVirtualSystemDescriptionType_Description:
        case KVirtualSystemDescriptionType_License:
        {
            if (UILineTextEdit *e = qobject_cast<UILineTextEdit*>(pEditor))
            {
                m_strConfigValue = e->text();
                fDone = true;
            }
            break;
        }
        case KVirtualSystemDescriptionType_SoundCard:
        case KVirtualSystemDescriptionType_NetworkAdapter:
        {
            if (QComboBox *e = qobject_cast<QComboBox*>(pEditor))
            {
                m_strConfigValue = e->itemData(e->currentIndex()).toString();
                fDone = true;
            }
            break;
        }
        case KVirtualSystemDescriptionType_HardDiskImage:
        {
            /* disabled for now
               if (UIFilePathSelector *e = qobject_cast<UIFilePathSelector*>(pEditor))
               {
               m_strConfigValue = e->path();
               }
               */
            if (QLineEdit *e = qobject_cast<QLineEdit*>(pEditor))
            {
                m_strConfigValue = e->text();
                fDone = true;
            }
            break;
        }
        default: break;
    }
    if (fDone)
        m_fModified = true;

    return fDone;
}

////////////////////////////////////////////////////////////////////////////////
// VirtualSystemModel

/* This class is a wrapper model for our ModelItem. It could be used with any
   TreeView & forward mostly all calls to the methods of ModelItem. The
   ModelItems itself are stored as internal pointers in the QModelIndex class. */
VirtualSystemModel::VirtualSystemModel(QVector<CVirtualSystemDescription>& aVSDs, QObject *pParent /* = NULL */)
   : QAbstractItemModel(pParent)
{
    m_pRootItem = new ModelItem(0, ApplianceModelItemType_Root);
    for (int a = 0; a < aVSDs.size(); ++a)
    {
        CVirtualSystemDescription vs = aVSDs[a];

        VirtualSystemItem *vi = new VirtualSystemItem(a, vs, m_pRootItem);
        m_pRootItem->appendChild(vi);

        /** @todo ask Dmitry about include/COMDefs.h:232 */
        QVector<KVirtualSystemDescriptionType> types;
        QVector<QString> refs;
        QVector<QString> origValues;
        QVector<QString> configValues;
        QVector<QString> extraConfigValues;

        QList<int> hdIndizies;
        QMap<int, HardwareItem*> controllerMap;
        vs.GetDescription(types, refs, origValues, configValues, extraConfigValues);
        for (int i = 0; i < types.size(); ++i)
        {
            /* We add the hard disk images in an second step, so save a
               reference to them. */
            if (types[i] == KVirtualSystemDescriptionType_HardDiskImage)
                hdIndizies << i;
            else
            {
                HardwareItem *hi = new HardwareItem(i, types[i], refs[i], origValues[i], configValues[i], extraConfigValues[i], vi);
                vi->appendChild(hi);
                /* Save the hard disk controller types in an extra map */
                if (types[i] == KVirtualSystemDescriptionType_HardDiskControllerIDE ||
                    types[i] == KVirtualSystemDescriptionType_HardDiskControllerSATA ||
                    types[i] == KVirtualSystemDescriptionType_HardDiskControllerSCSI ||
                    types[i] == KVirtualSystemDescriptionType_HardDiskControllerSAS)
                    controllerMap[i] = hi;
            }
        }
        QRegExp rx("controller=(\\d+);?");
        /* Now process the hard disk images */
        for (int a = 0; a < hdIndizies.size(); ++a)
        {
            int i = hdIndizies[a];
            QString ecnf = extraConfigValues[i];
            if (rx.indexIn(ecnf) != -1)
            {
                /* Get the controller */
                HardwareItem *ci = controllerMap[rx.cap(1).toInt()];
                if (ci)
                {
                    /* New hardware item as child of the controller */
                    HardwareItem *hi = new HardwareItem(i, types[i], refs[i], origValues[i], configValues[i], extraConfigValues[i], ci);
                    ci->appendChild(hi);
                }
            }
        }
    }
}

VirtualSystemModel::~VirtualSystemModel()
{
    if (m_pRootItem)
        delete m_pRootItem;
}

QModelIndex VirtualSystemModel::index(int row, int column, const QModelIndex &parentIdx /* = QModelIndex() */) const
{
    if (!hasIndex(row, column, parentIdx))
        return QModelIndex();

    ModelItem *parentItem;

    if (!parentIdx.isValid())
        parentItem = m_pRootItem;
    else
        parentItem = static_cast<ModelItem*>(parentIdx.internalPointer());

    ModelItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex VirtualSystemModel::parent(const QModelIndex &idx) const
{
    if (!idx.isValid())
        return QModelIndex();

    ModelItem *childItem = static_cast<ModelItem*>(idx.internalPointer());
    ModelItem *parentItem = childItem->parent();

    if (parentItem == m_pRootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int VirtualSystemModel::rowCount(const QModelIndex &parentIdx /* = QModelIndex() */) const
{
    ModelItem *parentItem;
    if (parentIdx.column() > 0)
        return 0;

    if (!parentIdx.isValid())
        parentItem = m_pRootItem;
    else
        parentItem = static_cast<ModelItem*>(parentIdx.internalPointer());

    return parentItem->childCount();
}

int VirtualSystemModel::columnCount(const QModelIndex &parentIdx /* = QModelIndex() */) const
{
    if (parentIdx.isValid())
        return static_cast<ModelItem*>(parentIdx.internalPointer())->columnCount();
    else
        return m_pRootItem->columnCount();
}

bool VirtualSystemModel::setData(const QModelIndex &idx, const QVariant &value, int role)
{
    if (!idx.isValid())
        return false;

    ModelItem *item = static_cast<ModelItem*>(idx.internalPointer());

    return item->setData(idx.column(), value, role);
}

QVariant VirtualSystemModel::data(const QModelIndex &idx, int role /* = Qt::DisplayRole */) const
{
    if (!idx.isValid())
        return QVariant();

    ModelItem *item = static_cast<ModelItem*>(idx.internalPointer());

    return item->data(idx.column(), role);
}

Qt::ItemFlags VirtualSystemModel::flags(const QModelIndex &idx) const
{
    if (!idx.isValid())
        return 0;

    ModelItem *item = static_cast<ModelItem*>(idx.internalPointer());

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | item->itemFlags(idx.column());
}

QVariant VirtualSystemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole ||
        orientation != Qt::Horizontal)
        return QVariant();

    QString title;
    switch (section)
    {
        case ApplianceViewSection_Description: title = UIApplianceEditorWidget::tr("Description"); break;
        case ApplianceViewSection_ConfigValue: title = UIApplianceEditorWidget::tr("Configuration"); break;
    }
    return title;
}

QModelIndex VirtualSystemModel::buddy(const QModelIndex &idx) const
{
    if (!idx.isValid())
        return QModelIndex();

    if (idx.column() == ApplianceViewSection_ConfigValue)
        return idx;
    else
        return index(idx.row(), ApplianceViewSection_ConfigValue, idx.parent());
}

void VirtualSystemModel::restoreDefaults(const QModelIndex &parentIdx /* = QModelIndex() */)
{
    ModelItem *parentItem;

    if (!parentIdx.isValid())
        parentItem = m_pRootItem;
    else
        parentItem = static_cast<ModelItem*>(parentIdx.internalPointer());

    for (int i = 0; i < parentItem->childCount(); ++i)
    {
        parentItem->child(i)->restoreDefaults();
        restoreDefaults(index(i, 0, parentIdx));
    }
    emit dataChanged(index(0, 0, parentIdx), index(parentItem->childCount()-1, 0, parentIdx));
}

void VirtualSystemModel::putBack()
{
    QVector<BOOL> v1;
    QVector<QString> v2;
    QVector<QString> v3;
    m_pRootItem->putBack(v1, v2, v3);
}

////////////////////////////////////////////////////////////////////////////////
// VirtualSystemDelegate

/* The delegate is used for creating/handling the different editors for the
   various types we support. This class forward the requests to the virtual
   methods of our different ModelItems. If this is not possible the default
   methods of QItemDelegate are used to get some standard behavior. Note: We
   have to handle the proxy model ourself. I really don't understand why Qt is
   not doing this for us. */
VirtualSystemDelegate::VirtualSystemDelegate(QAbstractProxyModel *pProxy, QObject *pParent /* = NULL */)
  : QItemDelegate(pParent)
  , mProxy(pProxy)
{}

QWidget *VirtualSystemDelegate::createEditor(QWidget *pParent, const QStyleOptionViewItem &styleOption, const QModelIndex &idx) const
{
    if (!idx.isValid())
        return QItemDelegate::createEditor(pParent, styleOption, idx);

    QModelIndex index(idx);
    if (mProxy)
        index = mProxy->mapToSource(idx);

    ModelItem *item = static_cast<ModelItem*>(index.internalPointer());
    QWidget *editor = item->createEditor(pParent, styleOption, index);

    /* Allow UILineTextEdit to commit data early: */
    if (editor && qobject_cast<UILineTextEdit*>(editor))
        connect(editor, SIGNAL(sigFinished(QWidget*)), this, SIGNAL(commitData(QWidget*)));

    if (editor == NULL)
        return QItemDelegate::createEditor(pParent, styleOption, index);
    else
        return editor;
}

void VirtualSystemDelegate::setEditorData(QWidget *pEditor, const QModelIndex &idx) const
{
    if (!idx.isValid())
        return QItemDelegate::setEditorData(pEditor, idx);

    QModelIndex index(idx);
    if (mProxy)
        index = mProxy->mapToSource(idx);

    ModelItem *item = static_cast<ModelItem*>(index.internalPointer());

    if (!item->setEditorData(pEditor, index))
        QItemDelegate::setEditorData(pEditor, index);
}

void VirtualSystemDelegate::setModelData(QWidget *pEditor, QAbstractItemModel *pModel, const QModelIndex &idx) const
{
    if (!idx.isValid())
        return QItemDelegate::setModelData(pEditor, pModel, idx);

    QModelIndex index = pModel->index(idx.row(), idx.column());
    if (mProxy)
        index = mProxy->mapToSource(idx);

    ModelItem *item = static_cast<ModelItem*>(index.internalPointer());
    if (!item->setModelData(pEditor, pModel, idx))
        QItemDelegate::setModelData(pEditor, pModel, idx);
}

void VirtualSystemDelegate::updateEditorGeometry(QWidget *pEditor, const QStyleOptionViewItem &styleOption, const QModelIndex & /* idx */) const
{
    if (pEditor)
        pEditor->setGeometry(styleOption.rect);
}

#ifdef VBOX_WS_MAC
bool VirtualSystemDelegate::eventFilter(QObject *pObject, QEvent *pEvent)
{
    if (pEvent->type() == QEvent::FocusOut)
    {
        /* On Mac OS X Cocoa the OS type selector widget loses it focus when
         * the popup menu is shown. Prevent this here, cause otherwise the new
         * selected OS will not be updated. */
        VBoxOSTypeSelectorButton *pButton = qobject_cast<VBoxOSTypeSelectorButton*>(pObject);
        if (pButton && pButton->isMenuShown())
            return false;
        /* The same counts for the text edit buttons of the license or
         * description fields. */
        else if (qobject_cast<UILineTextEdit*>(pObject))
            return false;
    }

    return QItemDelegate::eventFilter(pObject, pEvent);
}
#endif /* VBOX_WS_MAC */

////////////////////////////////////////////////////////////////////////////////
// VirtualSystemSortProxyModel

/* How to sort the items in the tree view */
KVirtualSystemDescriptionType VirtualSystemSortProxyModel::m_sortList[] =
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
    KVirtualSystemDescriptionType_HardDiskControllerSCSI,
    KVirtualSystemDescriptionType_HardDiskControllerSAS
};

VirtualSystemSortProxyModel::VirtualSystemSortProxyModel(QObject *pParent)
    : QSortFilterProxyModel(pParent)
{}

bool VirtualSystemSortProxyModel::filterAcceptsRow(int srcRow, const QModelIndex & srcParenIdx) const
{
    /* By default enable all, we will explicitly filter out below */
    if (srcParenIdx.isValid())
    {
        QModelIndex i = srcParenIdx.child(srcRow, 0);
        if (i.isValid())
        {
            ModelItem *item = static_cast<ModelItem*>(i.internalPointer());
            /* We filter hardware types only */
            if (item->type() == ApplianceModelItemType_Hardware)
            {
                HardwareItem *hwItem = static_cast<HardwareItem*>(item);
                /* The license type shouldn't be displayed */
                if (m_filterList.contains(hwItem->m_type))
                    return false;
            }
        }
    }
    return true;
}

bool VirtualSystemSortProxyModel::lessThan(const QModelIndex &leftIdx, const QModelIndex &rightIdx) const
{
    if (!leftIdx.isValid() ||
        !rightIdx.isValid())
        return false;

    ModelItem *pLeftItem = static_cast<ModelItem*>(leftIdx.internalPointer());
    ModelItem *pRightItem = static_cast<ModelItem*>(rightIdx.internalPointer());

    /* We sort hardware types only */
    if (!(pLeftItem->type() == ApplianceModelItemType_Hardware &&
          pRightItem->type() == ApplianceModelItemType_Hardware))
        return false;

    HardwareItem *pHwLeft = static_cast<HardwareItem*>(pLeftItem);
    HardwareItem *pHwRight = static_cast<HardwareItem*>(pRightItem);

    for (unsigned int i = 0; i < RT_ELEMENTS(m_sortList); ++i)
        if (pHwLeft->m_type == m_sortList[i])
        {
            for (unsigned int a = 0; a <= i; ++a)
                if (pHwRight->m_type == m_sortList[a])
                    return true;
            return false;
        }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// UIApplianceEditorWidget

int UIApplianceEditorWidget::m_minGuestRAM      = -1;
int UIApplianceEditorWidget::m_maxGuestRAM      = -1;
int UIApplianceEditorWidget::m_minGuestCPUCount = -1;
int UIApplianceEditorWidget::m_maxGuestCPUCount = -1;

UIApplianceEditorWidget::UIApplianceEditorWidget(QWidget *pParent /* = NULL */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pAppliance(NULL)
    , m_pModel(NULL)
{
    /* Make sure all static content is properly initialized */
    initSystemSettings();

    /* Create layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    {
        /* Configure information layout: */
        pLayout->setContentsMargins(0, 0, 0, 0);

        /* Create information pane: */
        m_pPaneInformation = new QWidget;
        {
            /* Create information layout: */
            QVBoxLayout *pLayoutInformation = new QVBoxLayout(m_pPaneInformation);
            {
                /* Configure information layout: */
                pLayoutInformation->setContentsMargins(0, 0, 0, 0);

                /* Create tree-view: */
                m_pTreeViewSettings = new QTreeView;
                {
                    /* Configure tree-view: */
                    m_pTreeViewSettings->setRootIsDecorated(false);
                    m_pTreeViewSettings->setAlternatingRowColors(true);
                    m_pTreeViewSettings->setAllColumnsShowFocus(true);
                    m_pTreeViewSettings->header()->setStretchLastSection(true);
                    m_pTreeViewSettings->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
#if QT_VERSION < 0x050000
                    m_pTreeViewSettings->header()->setResizeMode(QHeaderView::ResizeToContents);
#else
                    m_pTreeViewSettings->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
#endif

                    /* Add tree-view into information layout: */
                    pLayoutInformation->addWidget(m_pTreeViewSettings);
                }

                /* Create check-box: */
                m_pCheckBoxReinitMACs = new QCheckBox;
                {
                    /* Configure check-box: */
                    m_pCheckBoxReinitMACs->setHidden(true);

                    /* Add tree-view into information layout: */
                    pLayoutInformation->addWidget(m_pCheckBoxReinitMACs);
                }
            }

            /* Add information pane into layout: */
            pLayout->addWidget(m_pPaneInformation);
        }

        /* Create warning pane: */
        m_pPaneWarning  = new QWidget;
        {
            /* Configure warning pane: */
            m_pPaneWarning->hide();
            m_pPaneWarning->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

            /* Create warning layout: */
            QVBoxLayout *pLayoutWarning = new QVBoxLayout(m_pPaneWarning);
            {
                /* Configure warning layout: */
                pLayoutWarning->setContentsMargins(0, 0, 0, 0);

                /* Create label: */
                m_pLabelWarning = new QLabel;
                {
                    /* Add label into warning layout: */
                    pLayoutWarning->addWidget(m_pLabelWarning);
                }

                /* Create text-edit: */
                m_pTextEditWarning = new QTextEdit;
                {
                    /* Configure text-edit: */
                    m_pTextEditWarning->setReadOnly(true);
                    m_pTextEditWarning->setMaximumHeight(50);
                    m_pTextEditWarning->setAutoFormatting(QTextEdit::AutoBulletList);

                    /* Add text-edit into warning layout: */
                    pLayoutWarning->addWidget(m_pTextEditWarning);
                }
            }

            /* Add warning pane into layout: */
            pLayout->addWidget(m_pPaneWarning);
        }
    }

    /* Translate finally: */
    retranslateUi();
}

void UIApplianceEditorWidget::restoreDefaults()
{
    m_pModel->restoreDefaults();
}

void UIApplianceEditorWidget::retranslateUi()
{
    /* Translate information pane check-box: */
    m_pCheckBoxReinitMACs->setText(tr("&Reinitialize the MAC address of all network cards"));
    m_pCheckBoxReinitMACs->setToolTip(tr("When checked a new unique MAC address will assigned to all configured network cards."));

    /* Translate warning pane label: */
    m_pLabelWarning->setText(tr("Warnings:"));
}

/* static */
void UIApplianceEditorWidget::initSystemSettings()
{
    if (m_minGuestRAM == -1)
    {
        /* We need some global defaults from the current VirtualBox
           installation */
        CSystemProperties sp = vboxGlobal().virtualBox().GetSystemProperties();
        m_minGuestRAM        = sp.GetMinGuestRAM();
        m_maxGuestRAM        = sp.GetMaxGuestRAM();
        m_minGuestCPUCount   = sp.GetMinGuestCPUCount();
        m_maxGuestCPUCount   = sp.GetMaxGuestCPUCount();
    }
}

