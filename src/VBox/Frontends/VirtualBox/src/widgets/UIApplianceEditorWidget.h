/* $Id$ */
/** @file
 * VBox Qt GUI - UIApplianceEditorWidget class declaration.
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

#ifndef __UIApplianceEditorWidget_h__
#define __UIApplianceEditorWidget_h__

/* Qt includes: */
#include <QSortFilterProxyModel>
#include <QItemDelegate>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "COMEnums.h"
#include "CVirtualSystemDescription.h"

/* Forward declarations: */
class ModelItem;
class QWidget;
class QTreeView;
class QCheckBox;
class QLabel;
class QTextEdit;


/** Appliance tree-view section types. */
enum ApplianceViewSection
{
    ApplianceViewSection_Description = 0,
    ApplianceViewSection_OriginalValue,
    ApplianceViewSection_ConfigValue
};

/** Appliance model item types. */
enum ApplianceModelItemType
{
    ApplianceModelItemType_Root,
    ApplianceModelItemType_VirtualSystem,
    ApplianceModelItemType_Hardware
};


/* This & the following derived classes represent the data items of a Virtual
   System. All access/manipulation is done with the help of virtual functions
   to keep the interface clean. ModelItem is able to handle tree structures
   with a parent & several children's. */
class ModelItem
{
public:
    ModelItem(int number, ApplianceModelItemType type, ModelItem *pParent = NULL);

    virtual ~ModelItem();

    ModelItem *parent() const { return m_pParentItem; }

    void appendChild(ModelItem *pChild);
    ModelItem *child(int row) const;

    int row() const;

    int childCount() const;
    int columnCount() const { return 3; }

    virtual Qt::ItemFlags itemFlags(int /* column */) const { return 0; }
    virtual bool setData(int /* column */, const QVariant & /* value */, int /* role */) { return false; }
    virtual QVariant data(int /* column */, int /* role */) const { return QVariant(); }
    virtual QWidget *createEditor(QWidget * /* pParent */, const QStyleOptionViewItem & /* styleOption */, const QModelIndex & /* idx */) const { return NULL; }
    virtual bool setEditorData(QWidget * /* pEditor */, const QModelIndex & /* idx */) const { return false; }
    virtual bool setModelData(QWidget * /* pEditor */, QAbstractItemModel * /* pModel */, const QModelIndex & /* idx */) { return false; }

    virtual void restoreDefaults() {}
    virtual void putBack(QVector<BOOL>& finalStates, QVector<QString>& finalValues, QVector<QString>& finalExtraValues);

    ApplianceModelItemType type() const { return m_type; }

protected:
    /* Protected member vars */
    int                     m_number;
    ApplianceModelItemType  m_type;

    ModelItem              *m_pParentItem;
    QList<ModelItem*>       m_childItems;
};

////////////////////////////////////////////////////////////////////////////////
// VirtualSystemItem

/* This class represent a Virtual System with an index. */
class VirtualSystemItem: public ModelItem
{
public:
    VirtualSystemItem(int number, CVirtualSystemDescription aDesc, ModelItem *pParent);

    virtual QVariant data(int column, int role) const;

    virtual void putBack(QVector<BOOL>& finalStates, QVector<QString>& finalValues, QVector<QString>& finalExtraValues);

private:
    CVirtualSystemDescription m_desc;
};

////////////////////////////////////////////////////////////////////////////////
// HardwareItem

/* This class represent an hardware item of a Virtual System. All values of
   KVirtualSystemDescriptionType are supported & handled differently. */
class HardwareItem: public ModelItem
{
    friend class VirtualSystemSortProxyModel;
public:

    enum
    {
        TypeRole = Qt::UserRole,
        ModifiedRole
    };

    HardwareItem(int number,
                 KVirtualSystemDescriptionType type,
                 const QString &strRef,
                 const QString &strOrigValue,
                 const QString &strConfigValue,
                 const QString &strExtraConfigValue,
                 ModelItem *pParent);

    virtual void putBack(QVector<BOOL>& finalStates, QVector<QString>& finalValues, QVector<QString>& finalExtraValues);

    virtual bool setData(int column, const QVariant &value, int role);
    virtual QVariant data(int column, int role) const;

    virtual Qt::ItemFlags itemFlags(int column) const;

    virtual QWidget *createEditor(QWidget *pParent, const QStyleOptionViewItem &styleOption, const QModelIndex &idx) const;
    virtual bool setEditorData(QWidget *pEditor, const QModelIndex &idx) const;

    virtual bool setModelData(QWidget *pEditor, QAbstractItemModel *pModel, const QModelIndex &idx);

    virtual void restoreDefaults()
    {
        m_strConfigValue = m_strConfigDefaultValue;
        m_checkState = Qt::Checked;
    }

private:

    /* Private member vars */
    KVirtualSystemDescriptionType m_type;
    QString                       m_strRef;
    QString                       m_strOrigValue;
    QString                       m_strConfigValue;
    QString                       m_strConfigDefaultValue;
    QString                       m_strExtraConfigValue;
    Qt::CheckState                m_checkState;
    bool                          m_fModified;
};

////////////////////////////////////////////////////////////////////////////////
// VirtualSystemModel

class VirtualSystemModel: public QAbstractItemModel
{

public:
    VirtualSystemModel(QVector<CVirtualSystemDescription>& aVSDs, QObject *pParent = NULL);
    ~VirtualSystemModel();

    QModelIndex index(int row, int column, const QModelIndex &parentIdx = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &idx) const;
    int rowCount(const QModelIndex &parentIdx = QModelIndex()) const;
    int columnCount(const QModelIndex &parentIdx = QModelIndex()) const;
    bool setData(const QModelIndex &idx, const QVariant &value, int role);
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex &idx) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    QModelIndex buddy(const QModelIndex &idx) const;

    void restoreDefaults(const QModelIndex &parentIdx = QModelIndex());
    void putBack();

private:
    /* Private member vars */
    ModelItem *m_pRootItem;
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

    VirtualSystemDelegate(QAbstractProxyModel *pProxy, QObject *pParent = NULL);

    QWidget *createEditor(QWidget *pParent, const QStyleOptionViewItem &styleOption, const QModelIndex &idx) const;
    void setEditorData(QWidget *pEditor, const QModelIndex &idx) const;
    void setModelData(QWidget *pEditor, QAbstractItemModel *pModel, const QModelIndex &idx) const;
    void updateEditorGeometry(QWidget *pEditor, const QStyleOptionViewItem &styleOption, const QModelIndex &idx) const;

    QSize sizeHint(const QStyleOptionViewItem &styleOption, const QModelIndex &idx) const
    {
        QSize size = QItemDelegate::sizeHint(styleOption, idx);
#ifdef VBOX_WS_MAC
        int h = 28;
#else /* VBOX_WS_MAC */
        int h = 24;
#endif /* VBOX_WS_MAC */
        size.setHeight(RT_MAX(h, size.height()));
        return size;
    }

protected:

#ifdef VBOX_WS_MAC
    bool eventFilter(QObject *pObject, QEvent *pEvent);
#endif /* VBOX_WS_MAC */

private:

    /* Private member vars */
    QAbstractProxyModel *mProxy;
};

////////////////////////////////////////////////////////////////////////////////
// VirtualSystemSortProxyModel

class VirtualSystemSortProxyModel: public QSortFilterProxyModel
{
public:
    VirtualSystemSortProxyModel(QObject *pParent = NULL);

protected:
    bool filterAcceptsRow(int srcRow, const QModelIndex & srcParenIdx) const;
    bool lessThan(const QModelIndex &leftIdx, const QModelIndex &rightIdx) const;

    static KVirtualSystemDescriptionType m_sortList[];

    QList<KVirtualSystemDescriptionType> m_filterList;
};

////////////////////////////////////////////////////////////////////////////////
// UIApplianceEditorWidget

class UIApplianceEditorWidget: public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:
    UIApplianceEditorWidget(QWidget *pParent = NULL);

    bool isValid() const          { return m_pAppliance != NULL; }
    CAppliance* appliance() const { return m_pAppliance; }

    static int minGuestRAM()      { return m_minGuestRAM; }
    static int maxGuestRAM()      { return m_maxGuestRAM; }
    static int minGuestCPUCount() { return m_minGuestCPUCount; }
    static int maxGuestCPUCount() { return m_maxGuestCPUCount; }

public slots:
    void restoreDefaults();

protected:
    virtual void retranslateUi();

    /* Protected member vars */
    CAppliance         *m_pAppliance;
    VirtualSystemModel *m_pModel;

    /** Holds the information pane instance. */
    QWidget *m_pPaneInformation;
    /** Holds the settings tree-view instance. */
    QTreeView *m_pTreeViewSettings;
    /** Holds the 'reinit MACs' check-box instance. */
    QCheckBox *m_pCheckBoxReinitMACs;

    /** Holds the warning pane instance. */
    QWidget *m_pPaneWarning;
    /** Holds the warning label instance. */
    QLabel *m_pLabelWarning;
    /** Holds the warning browser instance. */
    QTextEdit *m_pTextEditWarning;

private:
    static void initSystemSettings();

    /* Private member vars */
    static int m_minGuestRAM;
    static int m_maxGuestRAM;
    static int m_minGuestCPUCount;
    static int m_maxGuestCPUCount;
};

#endif /* __UIApplianceEditorWidget_h__ */

