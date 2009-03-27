/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxApplianceEditorWgt class declaration
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

#ifndef __VBoxApplianceEditorWgt_h__
#define __VBoxApplianceEditorWgt_h__

/* VBox includes */
#include "COMDefs.h"
#include "VBoxApplianceEditorWgt.gen.h"
#include "QIWithRetranslateUI.h"

/* Qt includes */
#include <QSortFilterProxyModel>
#include <QItemDelegate>

/* VBox forward declarations */
class ModelItem;

////////////////////////////////////////////////////////////////////////////////
// Globals

enum TreeViewSection { DescriptionSection = 0, OriginalValueSection, ConfigValueSection };

////////////////////////////////////////////////////////////////////////////////
// ModelItem

enum ModelItemType { RootType, VirtualSystemType, HardwareType };

/* This & the following derived classes represent the data items of a Virtual
   System. All access/manipulation is done with the help of virtual functions
   to keep the interface clean. ModelItem is able to handle tree structures
   with a parent & several children's. */
class ModelItem
{
public:
    ModelItem (int aNumber, ModelItemType aType, ModelItem *aParent = NULL);

    ~ModelItem();

    ModelItem *parent() const { return mParentItem; }

    void appendChild (ModelItem *aChild);
    ModelItem * child (int aRow) const;

    int row() const;

    int childCount() const;
    int columnCount() const { return 3; }

    virtual Qt::ItemFlags itemFlags (int /* aColumn */) const { return 0; }
    virtual bool setData (int /* aColumn */, const QVariant & /* aValue */, int /* aRole */) { return false; }
    virtual QVariant data (int /* aColumn */, int /* aRole */) const { return QVariant(); }
    virtual QWidget * createEditor (QWidget * /* aParent */, const QStyleOptionViewItem & /* aOption */, const QModelIndex & /* aIndex */) const { return NULL; }
    virtual bool setEditorData (QWidget * /* aEditor */, const QModelIndex & /* aIndex */) const { return false; }
    virtual bool setModelData (QWidget * /* aEditor */, QAbstractItemModel * /* aModel */, const QModelIndex & /* aIndex */) { return false; }

    virtual void restoreDefaults() {}
    virtual void putBack (QVector<BOOL>& aFinalStates, QVector<QString>& aFinalValues, QVector<QString>& aFinalExtraValues);

    ModelItemType type() const { return mType; }

protected:
    /* Protected member vars */
    int mNumber;
    ModelItemType mType;

    ModelItem *mParentItem;
    QList<ModelItem*> mChildItems;
};

////////////////////////////////////////////////////////////////////////////////
// VirtualSystemItem

/* This class represent a Virtual System with an index. */
class VirtualSystemItem: public ModelItem
{
public:
    VirtualSystemItem (int aNumber, CVirtualSystemDescription aDesc, ModelItem *aParent);

    virtual QVariant data (int aColumn, int aRole) const;

    virtual void putBack (QVector<BOOL>& aFinalStates, QVector<QString>& aFinalValues, QVector<QString>& aFinalExtraValues);

private:
    CVirtualSystemDescription mDesc;
};

////////////////////////////////////////////////////////////////////////////////
// HardwareItem

/* This class represent an hardware item of a Virtual System. All values of
   KVirtualSystemDescriptionType are supported & handled differently. */
class HardwareItem: public ModelItem
{
    friend class VirtualSystemSortProxyModel;
public:

    HardwareItem (int aNumber,
                  KVirtualSystemDescriptionType aType,
                  const QString &aRef,
                  const QString &aOrigValue,
                  const QString &aConfigValue,
                  const QString &aExtraConfigValue,
                  ModelItem *aParent);

    virtual void putBack (QVector<BOOL>& aFinalStates, QVector<QString>& aFinalValues, QVector<QString>& aFinalExtraValues);

    virtual bool setData (int aColumn, const QVariant &aValue, int aRole);
    virtual QVariant data (int aColumn, int aRole) const;

    virtual Qt::ItemFlags itemFlags (int aColumn) const;

    virtual QWidget * createEditor (QWidget *aParent, const QStyleOptionViewItem &aOption, const QModelIndex &aIndex) const;
    virtual bool setEditorData (QWidget *aEditor, const QModelIndex &aIndex) const;

    virtual bool setModelData (QWidget *aEditor, QAbstractItemModel *aModel, const QModelIndex &aIndex);

    virtual void restoreDefaults()
    {
        mConfigValue = mConfigDefaultValue;
        mCheckState = Qt::Checked;
    }

private:

    /* Private member vars */
    KVirtualSystemDescriptionType mType;
    QString mRef;
    QString mOrigValue;
    QString mConfigValue;
    QString mConfigDefaultValue;
    QString mExtraConfigValue;
    Qt::CheckState mCheckState;
};

////////////////////////////////////////////////////////////////////////////////
// VirtualSystemModel

class VirtualSystemModel: public QAbstractItemModel
{

public:
    VirtualSystemModel (QVector<CVirtualSystemDescription>& aVSDs, QObject *aParent = NULL);

    QModelIndex index (int aRow, int aColumn, const QModelIndex &aParent = QModelIndex()) const;
    QModelIndex parent (const QModelIndex &aIndex) const;
    int rowCount (const QModelIndex &aParent = QModelIndex()) const;
    int columnCount (const QModelIndex &aParent = QModelIndex()) const;
    bool setData (const QModelIndex &aIndex, const QVariant &aValue, int aRole);
    QVariant data (const QModelIndex &aIndex, int aRole = Qt::DisplayRole) const;
    Qt::ItemFlags flags (const QModelIndex &aIndex) const;
    QVariant headerData (int aSection, Qt::Orientation aOrientation, int aRole) const;

    void restoreDefaults (const QModelIndex& aParent = QModelIndex());
    void putBack();

private:
    /* Private member vars */
    ModelItem *mRootItem;
};

////////////////////////////////////////////////////////////////////////////////
// VirtualSystemDelegate

/* The delegate is used for creating/handling the different editors for the
   various types we support. This class forward the requests to the virtual
   methods of our different ModelItems. If this is not possible the default
   methods of QItemDelegate are used to get some standard behavior. Note: We
   have to handle the proxy model ourself. I really don't understand why Qt is
   not doing this for us. */
class VirtualSystemDelegate: public QItemDelegate
{
public:
    VirtualSystemDelegate (QAbstractProxyModel *aProxy, QObject *aParent = NULL);

    QWidget * createEditor (QWidget *aParent, const QStyleOptionViewItem &aOption, const QModelIndex &aIndex) const;
    void setEditorData (QWidget *aEditor, const QModelIndex &aIndex) const;
    void setModelData (QWidget *aEditor, QAbstractItemModel *aModel, const QModelIndex &aIndex) const;
    void updateEditorGeometry (QWidget *aEditor, const QStyleOptionViewItem &aOption, const QModelIndex &aIndex) const;

    QSize sizeHint (const QStyleOptionViewItem &aOption, const QModelIndex &aIndex) const
    {
        QSize size = QItemDelegate::sizeHint (aOption, aIndex);
#ifdef Q_WS_MAC
        int h = 28;
#else /* Q_WS_MAC */
        int h = 24;
#endif /* Q_WS_MAC */
        size.setHeight (RT_MAX (h, size.height()));
        return size;
    }
private:
    /* Private member vars */
    QAbstractProxyModel *mProxy;
};

////////////////////////////////////////////////////////////////////////////////
// VirtualSystemSortProxyModel

class VirtualSystemSortProxyModel: public QSortFilterProxyModel
{
public:
    VirtualSystemSortProxyModel (QObject *aParent = NULL);

protected:
    bool filterAcceptsRow (int aSourceRow, const QModelIndex & aSourceParent) const;
    bool lessThan (const QModelIndex &aLeft, const QModelIndex &aRight) const;

    static KVirtualSystemDescriptionType mSortList[];

    QList<KVirtualSystemDescriptionType> mFilterList;
};

////////////////////////////////////////////////////////////////////////////////
// VBoxApplianceEditorWgt

class VBoxApplianceEditorWgt : public QIWithRetranslateUI<QWidget>,
                               public Ui::VBoxApplianceEditorWgt
{
    Q_OBJECT;

public:
    VBoxApplianceEditorWgt (QWidget *aParent = NULL);

    bool isValid() const { return mAppliance != NULL; }
    CAppliance* appliance() const { return mAppliance; }

    static int minGuestRAM() { return mMinGuestRAM; }
    static int maxGuestRAM() { return mMaxGuestRAM; }
    static int minGuestCPUCount() { return mMinGuestCPUCount; }
    static int maxGuestCPUCount() { return mMaxGuestCPUCount; }

public slots:
    void restoreDefaults();

protected:
    virtual void retranslateUi();

    /* Protected member vars */
    CAppliance *mAppliance;
    VirtualSystemModel *mModel;

private:
    static void initSystemSettings();

    /* Private member vars */
    static int mMinGuestRAM;
    static int mMaxGuestRAM;
    static int mMinGuestCPUCount;
    static int mMaxGuestCPUCount;
};

#endif /* __VBoxApplianceEditorWgt_h__ */

