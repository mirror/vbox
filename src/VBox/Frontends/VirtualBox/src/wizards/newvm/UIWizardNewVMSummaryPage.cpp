/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMSummaryPage class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QHeaderView>
#include <QList>
#include <QFileInfo>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "QITreeView.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIWizardNewVMSummaryPage.h"
#include "UIWizardDiskEditors.h"
#include "UIWizardNewVM.h"

/*********************************************************************************************************************************
*   UIWizardNewVMSummaryItem definition.                                                                                  *
*********************************************************************************************************************************/

class UIWizardNewVMSummaryItem : public QITreeViewItem
{
    Q_OBJECT;

public:

    UIWizardNewVMSummaryItem(QITreeView *pParentTree, const QString &strText,
                             const QVariant &data = QVariant(), const QIcon &icon = QIcon());

    virtual UIWizardNewVMSummaryItem *childItem(int iIndex) const /* override final */;
    virtual int childCount() const /* override final */;
    virtual QString text() const /* override final */;
    const QVariant &data() const;
    const QIcon &icon() const;

    /** Returns the index of this item within its parent's children list. */
    int row() const;
    int childIndex(const UIWizardNewVMSummaryItem *pChild) const;
    int columnCount() const;


    UIWizardNewVMSummaryItem *addChild(const QString &strText,
                                       const QVariant &data = QVariant(), const QIcon &icon = QIcon());


    bool isSectionTitle() const;
    void setIsSectionTitle(bool fIsSectionTitle);

private:

    UIWizardNewVMSummaryItem(UIWizardNewVMSummaryItem *pParentItem, const QString &strText,
                             const QVariant &data = QVariant(), const QIcon &icon = QIcon());

    UIWizardNewVMSummaryItem *m_pParentItem;
    QString m_strText;
    QVariant m_data;
    QIcon m_icon;
    QList<UIWizardNewVMSummaryItem*> m_childList;
    bool m_fIsSectionTitle;
};

/*********************************************************************************************************************************
*   UIWizardNewVMSummaryModel definition.                                                                                  *
*********************************************************************************************************************************/

class UIWizardNewVMSummaryModel : public QAbstractItemModel
{
    Q_OBJECT;

public:

    UIWizardNewVMSummaryModel(QITreeView *pParentTree);

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const /* override final */;

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const  /* override final */;
    QModelIndex parent(const QModelIndex &index) const  /* override final */;
    int rowCount(const QModelIndex &parent = QModelIndex()) const  /* override final */;
    int columnCount(const QModelIndex &parent = QModelIndex()) const  /* override final */;

    void populateData(UIWizardNewVM *pWizard);

private:

    UIWizardNewVMSummaryItem *m_pRootItem;

};

/*********************************************************************************************************************************
*   UIWizardNewVMSummaryItem implementation.                                                                                  *
*********************************************************************************************************************************/


UIWizardNewVMSummaryItem::UIWizardNewVMSummaryItem(QITreeView *pParentTree, const QString &strText,
                                                   const QVariant &data /* = QVariant() */, const QIcon &icon /* = QIcon() */)
    : QITreeViewItem(pParentTree)
    , m_strText(strText)
    , m_data(data)
    , m_icon(icon)
    , m_fIsSectionTitle(false)
{
}

UIWizardNewVMSummaryItem::UIWizardNewVMSummaryItem(UIWizardNewVMSummaryItem *pParentItem, const QString &strText,
                                                   const QVariant &data /* = QVariant() */, const QIcon &icon /* = QIcon() */)
    : QITreeViewItem(pParentItem)
    , m_strText(strText)
    , m_data(data)
    , m_icon(icon)
    , m_fIsSectionTitle(false)
{
}

UIWizardNewVMSummaryItem *UIWizardNewVMSummaryItem::childItem(int iIndex) const
{
    if (iIndex >= m_childList.size())
        return 0;
    return m_childList[iIndex];
}

int UIWizardNewVMSummaryItem::childIndex(const UIWizardNewVMSummaryItem *pChild) const
{
    if (!pChild)
        return 0;
    return m_childList.indexOf(const_cast<UIWizardNewVMSummaryItem*>(pChild));
}

int UIWizardNewVMSummaryItem::row() const
{
    UIWizardNewVMSummaryItem *pParent = qobject_cast<UIWizardNewVMSummaryItem*>(parentItem());
    if (!pParent)
        return 0;
    return pParent->childIndex(this);
}


int UIWizardNewVMSummaryItem::childCount() const
{
    return m_childList.size();
}


QString UIWizardNewVMSummaryItem::text() const
{
    return m_strText;
}

const QVariant &UIWizardNewVMSummaryItem::data() const
{
    return m_data;
}

const QIcon &UIWizardNewVMSummaryItem::icon() const
{
    return m_icon;
}

int UIWizardNewVMSummaryItem::columnCount() const
{
    if (m_data.isValid())
        return 2;
    return 1;
}

UIWizardNewVMSummaryItem *UIWizardNewVMSummaryItem::addChild(const QString &strText, const QVariant &data /* = QVariant() */, const QIcon &icon /* = QIcon() */)
{
    UIWizardNewVMSummaryItem *pNewItem = new UIWizardNewVMSummaryItem(this, strText, data, icon);
    m_childList << pNewItem;
    return pNewItem;
}

void UIWizardNewVMSummaryItem::setIsSectionTitle(bool fIsSectionTitle)
{
    m_fIsSectionTitle = fIsSectionTitle;
}

bool UIWizardNewVMSummaryItem::isSectionTitle() const
{
    return m_fIsSectionTitle;
}


/*********************************************************************************************************************************
*   UIWizardNewVMSummaryModel implementation.                                                                                  *
*********************************************************************************************************************************/

UIWizardNewVMSummaryModel::UIWizardNewVMSummaryModel(QITreeView *pParentTree)
    : QAbstractItemModel(pParentTree)
    , m_pRootItem(0)
{
}

QVariant UIWizardNewVMSummaryModel::data(const QModelIndex &index, int role /* = Qt::DisplayRole */) const
{
    if (!index.isValid())
        return QVariant();
    UIWizardNewVMSummaryItem *pItem = static_cast<UIWizardNewVMSummaryItem*>(index.internalPointer());
    if (!pItem)
        return QVariant();
    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
            case 0:
                return pItem->text();
                break;
            case 1:
                return pItem->data();
                break;
            default:
                break;
        }
    }
    else if (role == Qt::DecorationRole)
    {
        if (index.column() == 0 && !pItem->icon().isNull())
            return pItem->icon();
    }
    else if (role == Qt::FontRole)
    {
        UIWizardNewVMSummaryItem *pItem = static_cast<UIWizardNewVMSummaryItem*>(index.internalPointer());
        if (pItem && pItem->isSectionTitle())
        {
            QFont font = qApp->font();
            font.setBold(true);
            return font;
        }
    }
    return QVariant();
}

QModelIndex UIWizardNewVMSummaryModel::index(int row, int column, const QModelIndex &parent /* = QModelIndex() */) const
{
     if (!hasIndex(row, column, parent))
          return QModelIndex();

     UIWizardNewVMSummaryItem *pParentItem;

     if (!parent.isValid())
         pParentItem = m_pRootItem;
     else
         pParentItem = static_cast<UIWizardNewVMSummaryItem*>(parent.internalPointer());

     UIWizardNewVMSummaryItem *pChildItem = pParentItem->childItem(row);
     if (pChildItem)
         return createIndex(row, column, pChildItem);
     else
         return QModelIndex();
}

QModelIndex UIWizardNewVMSummaryModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    UIWizardNewVMSummaryItem *pChildItem = static_cast<UIWizardNewVMSummaryItem*>(index.internalPointer());
    if (!pChildItem)
        return QModelIndex();

    UIWizardNewVMSummaryItem *pParentItem = static_cast<UIWizardNewVMSummaryItem*>(pChildItem->parentItem());

    if (pParentItem == m_pRootItem)
        return QModelIndex();

    return createIndex(pParentItem->row(), 0, pParentItem);
}

int UIWizardNewVMSummaryModel::rowCount(const QModelIndex &parent /* = QModelIndex() */) const
{
    if (parent.column() > 0)
        return 0;
    UIWizardNewVMSummaryItem *pItem = 0;
    if (!parent.isValid())
        pItem = m_pRootItem;
    else
        pItem = static_cast<UIWizardNewVMSummaryItem*>(parent.internalPointer());

    if (pItem)
        return pItem->childCount();
    return 0;
}

int UIWizardNewVMSummaryModel::columnCount(const QModelIndex &parentIndex /* = QModelIndex() */) const
{
    return 2;
    AssertReturn(m_pRootItem, 0);
    if (parentIndex.isValid())
    {
        UIWizardNewVMSummaryItem *pParent = static_cast<UIWizardNewVMSummaryItem*>(parentIndex.internalPointer());
        // if (pParent && pParent->childItem(0))
        // {
        //     printf("sasadasd %d\n", pParent->childItem(0)->columnCount());
        //     return pParent->childItem(0)->columnCount();
        // }
        if (pParent)
            return pParent->columnCount();

    }
    return m_pRootItem->columnCount();
}

void UIWizardNewVMSummaryModel::populateData(UIWizardNewVM *pWizard)
{
    QITreeView *pParentTree = qobject_cast<QITreeView*>(QObject::parent());

    AssertReturnVoid(pWizard && pParentTree);
    if (m_pRootItem)
        delete m_pRootItem;
    m_pRootItem = new UIWizardNewVMSummaryItem(pParentTree, "root");

    UIWizardNewVMSummaryItem *pNameRoot = m_pRootItem->addChild(UIWizardNewVM::tr("Machine Name and OS Type"));
    pNameRoot->setIsSectionTitle(true);

    /* Name and OS Type page stuff: */
    pNameRoot->addChild(UIWizardNewVM::tr("Machine Name"), pWizard->machineBaseName(), UIIconPool::iconSet(":/name_16px.png"));
    pNameRoot->addChild(UIWizardNewVM::tr("Machine Folder"), pWizard->machineFolder(), UIIconPool::iconSet(":/name_16px.png"));
    pNameRoot->addChild(UIWizardNewVM::tr("ISO Image"), pWizard->ISOFilePath(), UIIconPool::iconSet(":/cd_16px.png"));
    pNameRoot->addChild(UIWizardNewVM::tr("Guest OS Type"), pWizard->guestOSType().GetDescription(), UIIconPool::iconSet(":/system_type_16px.png"));

    const QString &ISOPath = pWizard->ISOFilePath();
    if (!ISOPath.isNull() && !ISOPath.isEmpty())
        pNameRoot->addChild(UIWizardNewVM::tr("Skip Unattended Install"), pWizard->skipUnattendedInstall(),
                            UIIconPool::iconSet(":/extension_pack_install_16px.png"));


    /* Unattended install related info: */
    if (pWizard->isUnattendedEnabled())
    {
        UIWizardNewVMSummaryItem *pUnattendedRoot = m_pRootItem->addChild(UIWizardNewVM::tr("Unattended Install"));
        pUnattendedRoot->setIsSectionTitle(true);
        pUnattendedRoot->addChild(UIWizardNewVM::tr("Username"), pWizard->userName(),
                                  UIIconPool::iconSet(":/extension_pack_install_16px.png"));
        pUnattendedRoot->addChild(UIWizardNewVM::tr("Product Key"), pWizard->installGuestAdditions(),
                                  UIIconPool::iconSet(":/extension_pack_install_16px.png"));
        pUnattendedRoot->addChild(UIWizardNewVM::tr("Hostname/Domain Name"), pWizard->hostnameDomainName(),
                                  UIIconPool::iconSet(":/extension_pack_install_16px.png"));
        pUnattendedRoot->addChild(UIWizardNewVM::tr("Install in Background"), pWizard->startHeadless(),
                                  UIIconPool::iconSet(":/extension_pack_install_16px.png"));
        pUnattendedRoot->addChild(UIWizardNewVM::tr("Install Guest Additions"), pWizard->installGuestAdditions(),
                                  UIIconPool::iconSet(":/extension_pack_install_16px.png"));
        if (pWizard->installGuestAdditions())
            pUnattendedRoot->addChild(UIWizardNewVM::tr("Guest Additions ISO"), pWizard->guestAdditionsISOPath(),
                                      UIIconPool::iconSet(":/extension_pack_install_16px.png"));
    }


    // UIWizardNewVMSummaryItem *pHardwareRoot = new UIWizardNewVMSummaryItem(m_pRootItem, UIWizardNewVM::tr("Hardware"));
    // UIWizardNewVMSummaryItem *pDiskRoot = new UIWizardNewVMSummaryItem(m_pRootItem, UIWizardNewVM::tr("Disk"));
    // Q_UNUSED(pHardwareRoot);
    // Q_UNUSED(pDiskRoot);

}

/*********************************************************************************************************************************
*   UIWizardNewVMSummaryPage implementation.                                                                                     *
*********************************************************************************************************************************/

UIWizardNewVMSummaryPage::UIWizardNewVMSummaryPage()
    : m_pLabel(0)
    , m_pTree(0)
{
    prepare();
}


void UIWizardNewVMSummaryPage::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);

    m_pLabel = new QIRichTextLabel(this);
    pMainLayout->addWidget(m_pLabel);

    m_pTree = new QITreeView;
    QString sty("QTreeView::branch {"
                "background: palette(base);"
                "}");

    if (m_pTree)
    {
        //m_pTree->setStyleSheet(sty);
        m_pTree->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
        m_pTree->setAlternatingRowColors(true);
        m_pModel = new UIWizardNewVMSummaryModel(m_pTree);
        m_pTree->setModel(m_pModel);
        pMainLayout->addWidget(m_pTree);
    }

    //pMainLayout->addStretch();

    createConnections();
}

void UIWizardNewVMSummaryPage::createConnections()
{
}

void UIWizardNewVMSummaryPage::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Summary"));
}

void UIWizardNewVMSummaryPage::initializePage()
{
    retranslateUi();
    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertReturnVoid(pWizard && m_pModel);
    if (m_pModel)
        m_pModel->populateData(pWizard);
    if (m_pTree)
    {
        m_pTree->expandToDepth(4);
        m_pTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    }
}

bool UIWizardNewVMSummaryPage::isComplete() const
{
    return true;
}

bool UIWizardNewVMSummaryPage::validatePage()
{
    bool fResult = true;
    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertReturn(pWizard, false);

    /* Make sure user really intents to creae a vm with no hard drive: */
    if (pWizard->diskSource() == SelectedDiskSource_Empty)
    {
        /* Ask user about disk-less machine unless that's the recommendation: */
        if (!pWizard->emptyDiskRecommended())
        {
            if (!msgCenter().confirmHardDisklessMachine(this))
                return false;
        }
    }
    else if (pWizard->diskSource() == SelectedDiskSource_New)
    {
        /* Check if the path we will be using for hard drive creation exists: */
        const QString &strMediumPath = pWizard->mediumPath();
        fResult = !QFileInfo(strMediumPath).exists();
        if (!fResult)
        {
            msgCenter().cannotOverwriteHardDiskStorage(strMediumPath, this);
            return fResult;
        }
        /* Check FAT size limitation of the host hard drive: */
        fResult = UIDiskEditorGroupBox::checkFATSizeLimitation(pWizard->mediumVariant(),
                                                                strMediumPath,
                                                                pWizard->mediumSize());
        if (!fResult)
        {
            msgCenter().cannotCreateHardDiskStorageInFAT(strMediumPath, this);
            return fResult;
        }

        /* Try to create the hard drive:*/
        fResult = pWizard->createVirtualDisk();
        /*Don't show any error message here since UIWizardNewVM::createVirtualDisk already does so: */
        if (!fResult)
            return fResult;
    }

    fResult = pWizard->createVM();
    /* Try to delete the hard disk: */
    if (!fResult)
        pWizard->deleteVirtualDisk();

    return fResult;
}


#include "UIWizardNewVMSummaryPage.moc"
