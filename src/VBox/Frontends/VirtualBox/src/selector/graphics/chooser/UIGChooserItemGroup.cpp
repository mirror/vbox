/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGChooserItemGroup class implementation
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneDragDropEvent>
#include <QLineEdit>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QMenu>

/* GUI includes: */
#include "UIGChooserItemGroup.h"
#include "UIGChooserItemMachine.h"
#include "UIGChooserModel.h"
#include "UIIconPool.h"
#include "UIGraphicsRotatorButton.h"
#include "UIGChooserView.h"

/* static */
QString UIGChooserItemGroup::className() { return "UIGChooserItemGroup"; }

UIGChooserItemGroup::UIGChooserItemGroup(QGraphicsScene *pScene)
    : UIGChooserItem(0, false /* temporary? */)
    , m_fClosed(false)
    , m_fMainRoot(true)
{
    /* Prepare: */
    prepare();

    /* Add item to the scene: */
    AssertMsg(pScene, ("Incorrect scene passed!"));
    pScene->addItem(this);

    /* Translate finally: */
    retranslateUi();

    /* Calculate minimum header size: */
    updateHeaderSize();
}

UIGChooserItemGroup::UIGChooserItemGroup(QGraphicsScene *pScene,
                                         UIGChooserItemGroup *pCopyFrom,
                                         bool fMainRoot)
    : UIGChooserItem(0, true /* temporary? */)
    , m_strName(pCopyFrom->name())
    , m_fClosed(pCopyFrom->isClosed())
    , m_fMainRoot(fMainRoot)
{
    /* Prepare: */
    prepare();

    /* Add item to the scene: */
    AssertMsg(pScene, ("Incorrect scene passed!"));
    pScene->addItem(this);

    /* Copy content to 'this': */
    copyContent(pCopyFrom, this);

    /* Translate finally: */
    retranslateUi();

    /* Calculate minimum header size: */
    updateHeaderSize();
}

UIGChooserItemGroup::UIGChooserItemGroup(UIGChooserItem *pParent,
                                         const QString &strName,
                                         bool fOpened /* = false */,
                                         int iPosition /* = -1 */)
    : UIGChooserItem(pParent, pParent->isTemporary())
    , m_strName(strName)
    , m_fClosed(!fOpened)
    , m_fMainRoot(false)
{
    /* Prepare: */
    prepare();

    /* Add item to the parent: */
    AssertMsg(parentItem(), ("Incorrect parent passed!"));
    parentItem()->addItem(this, iPosition);
    setZValue(parentItem()->zValue() + 1);
    connect(this, SIGNAL(sigToggleStarted()), model(), SIGNAL(sigToggleStarted()));
    connect(this, SIGNAL(sigToggleFinished()), model(), SIGNAL(sigToggleFinished()), Qt::QueuedConnection);

    /* Translate finally: */
    retranslateUi();

    /* Calculate minimum header size: */
    updateHeaderSize();
}

UIGChooserItemGroup::UIGChooserItemGroup(UIGChooserItem *pParent,
                                         UIGChooserItemGroup *pCopyFrom,
                                         int iPosition /* = -1 */)
    : UIGChooserItem(pParent, pParent->isTemporary())
    , m_strName(pCopyFrom->name())
    , m_fClosed(pCopyFrom->isClosed())
    , m_fMainRoot(false)
{
    /* Prepare: */
    prepare();

    /* Add item to the parent: */
    AssertMsg(parentItem(), ("Incorrect parent passed!"));
    parentItem()->addItem(this, iPosition);
    setZValue(parentItem()->zValue() + 1);
    connect(this, SIGNAL(sigToggleStarted()), model(), SIGNAL(sigToggleStarted()));
    connect(this, SIGNAL(sigToggleFinished()), model(), SIGNAL(sigToggleFinished()));

    /* Copy content to 'this': */
    copyContent(pCopyFrom, this);

    /* Translate finally: */
    retranslateUi();

    /* Calculate minimum header size: */
    updateHeaderSize();
}

UIGChooserItemGroup::~UIGChooserItemGroup()
{
    /* Delete all the items: */
    clearItems();

    /* If that item is focused: */
    if (model()->focusItem() == this)
    {
        /* Unset the focus: */
        model()->setFocusItem(0);
    }
    /* If that item is in selection list: */
    if (model()->currentItems().contains(this))
    {
        /* Remove item from the selection list: */
        model()->removeFromCurrentItems(this);
    }
    /* If that item is in navigation list: */
    if (model()->navigationList().contains(this))
    {
        /* Remove item from the navigation list: */
        model()->removeFromNavigationList(this);
    }

    /* Remove item from the parent: */
    if (parentItem())
        parentItem()->removeItem(this);
}

QString UIGChooserItemGroup::name() const
{
    return m_strName;
}

QString UIGChooserItemGroup::fullName() const
{
    /* Return "/" for main root-item: */
    if (isMainRoot())
        return "/";
    /* Get full parent name, append with '/' if not yet appended: */
    AssertMsg(parentItem(), ("Incorrect parent set!"));
    QString strFullParentName = parentItem()->fullName();
    if (!strFullParentName.endsWith('/'))
        strFullParentName.append('/');
    /* Return full item name based on parent prefix: */
    return strFullParentName + name();
}

QString UIGChooserItemGroup::definition() const
{
    return QString("g=%1").arg(name());
}

void UIGChooserItemGroup::setName(const QString &strName)
{
    /* Something changed? */
    if (m_strName == strName)
        return;

    /* Remember new name: */
    m_strName = strName;

    /* Update visible name: */
    updateVisibleName();
    /* Update minimum header size: */
    updateHeaderSize();
}

bool UIGChooserItemGroup::isClosed() const
{
    return m_fClosed && !isRoot();
}

bool UIGChooserItemGroup::isOpened() const
{
    return !m_fClosed || isRoot();
}

void UIGChooserItemGroup::close(bool fAnimated /* = true */)
{
    AssertMsg(!isRoot(), ("Can't close root-item!"));
    m_pToggleButton->setToggled(false, fAnimated);
}

void UIGChooserItemGroup::open(bool fAnimated /* = true */)
{
    AssertMsg(!isRoot(), ("Can't open root-item!"));
    m_pToggleButton->setToggled(true, fAnimated);
}

bool UIGChooserItemGroup::isContainsMachine(const QString &strId) const
{
    /* Check each machine-item: */
    foreach (UIGChooserItem *pItem, m_machineItems)
        if (pItem->toMachineItem()->id() == strId)
            return true;
    /* Found nothing? */
    return false;
}

bool UIGChooserItemGroup::isContainsLockedMachine()
{
    /* Check each machine-item: */
    foreach (UIGChooserItem *pItem, items(UIGChooserItemType_Machine))
        if (pItem->toMachineItem()->isLockedMachine())
            return true;
    /* Check each group-item: */
    foreach (UIGChooserItem *pItem, items(UIGChooserItemType_Group))
        if (pItem->toGroupItem()->isContainsLockedMachine())
            return true;
    /* Found nothing? */
    return false;
}

void UIGChooserItemGroup::sltHandleGeometryChange()
{
    /* What is the new geometry? */
    QRectF newGeometry = geometry();

    /* Should we update visible name? */
    if (m_previousGeometry.width() != newGeometry.width())
        updateVisibleName();

    /* Remember the new geometry: */
    m_previousGeometry = newGeometry;
}

void UIGChooserItemGroup::sltNameEditingFinished()
{
    /* Not for root: */
    if (isRoot())
        return;

    /* Lock name-editor: */
    m_pNameEditor->hide();

    /* Enumerate all the group names: */
    QStringList groupNames;
    foreach (UIGChooserItem *pItem, parentItem()->items(UIGChooserItemType_Group))
        groupNames << pItem->name();
    /* If proposed name is empty or not unique, reject it: */
    QString strNewName = m_pNameEditorWidget->text().trimmed();
    if (strNewName.isEmpty() || groupNames.contains(strNewName))
        return;

    /* We should replace forbidden symbols
     * with ... well, probably underscores: */
    strNewName.replace(QRegExp("[\\\\/:*?\"<>]"), "_");

    /* Set new name, save settings: */
    setName(strNewName);
    model()->saveGroupSettings();
}

void UIGChooserItemGroup::sltGroupToggleStart()
{
    /* Not for root: */
    if (isRoot())
        return;

    /* Toggle started: */
    if (!isTemporary())
        emit sigToggleStarted();

    /* Setup animation: */
    updateAnimationParameters();

    /* Group closed, we are opening it: */
    if (m_fClosed)
    {
        /* Toggle-state and navigation will be
         * updated on toggle-finish signal! */
    }
    /* Group opened, we are closing it: */
    else
    {
        /* Update toggle-state: */
        m_fClosed = true;
        /* Update navigation: */
        model()->updateNavigation();
        /* Relayout model: */
        model()->updateLayout();
    }
}

void UIGChooserItemGroup::sltGroupToggleFinish(bool fToggled)
{
    /* Not for root: */
    if (isRoot())
        return;

    /* Update toggle-state: */
    m_fClosed = !fToggled;
    /* Update navigation: */
    model()->updateNavigation();
    /* Relayout model: */
    model()->updateLayout();
    /* Update toggle-button tool-tip: */
    updateToggleButtonToolTip();

    /* Toggle finished: */
    if (!isTemporary())
        emit sigToggleFinished();
}

void UIGChooserItemGroup::sltIndentRoot()
{
    /* Unhover before indenting: */
    setHovered(false);

    /* Indent to this root: */
    model()->indentRoot(this);
}

void UIGChooserItemGroup::sltUnindentRoot()
{
    /* Unhover before unindenting: */
    setHovered(false);

    /* Unindent to previous root: */
    model()->unindentRoot();
}

QVariant UIGChooserItemGroup::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case GroupItemData_HorizonalMargin: return 5;
        case GroupItemData_VerticalMargin: return 5;
        case GroupItemData_MajorSpacing: return 10;
        case GroupItemData_MinorSpacing: return 3;
        case GroupItemData_RootIndent: return 2;
        /* Texts: */
        case GroupItemData_Name: return m_strVisibleName;
        case GroupItemData_GroupCountText: return m_groupItems.isEmpty() ? QString() : QString::number(m_groupItems.size());
        case GroupItemData_MachineCountText: return m_machineItems.isEmpty() ? QString() : QString::number(m_machineItems.size());
        /* Sizes: */
        case GroupItemData_ToggleButtonSize: return m_pToggleButton ? m_pToggleButton->minimumSizeHint().toSize() : QSize(0, 0);
        case GroupItemData_EnterButtonSize: return m_pEnterButton ? m_pEnterButton->minimumSizeHint().toSize() : QSize(0, 0);
        case GroupItemData_ExitButtonSize: return m_pExitButton ? m_pExitButton->minimumSizeHint().toSize() : QSize(0, 0);
        case GroupItemData_NameSize: return isMainRoot() ? QSize(0, 0) : textSize(m_nameFont, model()->paintDevice(),
                                                                                  data(GroupItemData_Name).toString());
        case GroupItemData_GroupPixmapSize: return isMainRoot() ? QSize(0, 0) : m_groupsPixmap.size();
        case GroupItemData_MachinePixmapSize: return isMainRoot() ? QSize(0, 0) : m_machinesPixmap.size();
        case GroupItemData_GroupCountTextSize: return isMainRoot() ? QSize(0, 0) : textSize(m_infoFont, model()->paintDevice(),
                                                                                            data(GroupItemData_GroupCountText).toString());
        case GroupItemData_MachineCountTextSize: return isMainRoot() ? QSize(0, 0) : textSize(m_infoFont, model()->paintDevice(),
                                                                                              data(GroupItemData_MachineCountText).toString());
        case GroupItemData_FullHeaderSize: return m_headerSize;
        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIGChooserItemGroup::prepare()
{
    /* Buttons: */
    m_pToggleButton = 0;
    m_pEnterButton = 0;
    m_pExitButton = 0;
    /* Name editor: */
    m_pNameEditorWidget = 0;
    m_pNameEditor = 0;
    /* Painting stuff: */
    m_iAdditionalHeight = 0;
    m_iCornerRadius = 10;
    m_iBlackoutDarkness = 110;
    m_nameFont = font();
    m_nameFont.setWeight(QFont::Bold);
    m_infoFont = font();
    m_groupsPixmap = QPixmap(":/nw_16px.png");
    m_machinesPixmap = QPixmap(":/machine_16px.png");

    /* Geometry-change handler: */
    connect(this, SIGNAL(geometryChanged()), this, SLOT(sltHandleGeometryChange()));

    /* Items except roots: */
    if (!isRoot())
    {
        /* Setup toggle-button: */
        m_pToggleButton = new UIGraphicsRotatorButton(this, "additionalHeight", isOpened());
        connect(m_pToggleButton, SIGNAL(sigRotationStart()), this, SLOT(sltGroupToggleStart()));
        connect(m_pToggleButton, SIGNAL(sigRotationFinish(bool)), this, SLOT(sltGroupToggleFinish(bool)));
        m_pToggleButton->hide();

        /* Setup enter-button: */
        m_pEnterButton = new UIGraphicsButton(this, UIGraphicsButtonType_DirectArrow);
        connect(m_pEnterButton, SIGNAL(sigButtonClicked()), this, SLOT(sltIndentRoot()));
        m_pEnterButton->hide();

        /* Setup name-editor: */
        m_pNameEditorWidget = new UIGroupRenameEditor(name(), this);
        m_pNameEditorWidget->setFont(m_nameFont);
        connect(m_pNameEditorWidget, SIGNAL(sigEditingFinished()), this, SLOT(sltNameEditingFinished()));
        m_pNameEditor = new QGraphicsProxyWidget(this);
        m_pNameEditor->setWidget(m_pNameEditorWidget);
        m_pNameEditor->hide();
    }
    /* Items except main root: */
    if (!isMainRoot())
    {
        /* Setup exit-button: */
        m_pExitButton = new UIGraphicsButton(this, UIGraphicsButtonType_DirectArrow);
        connect(m_pExitButton, SIGNAL(sigButtonClicked()), this, SLOT(sltUnindentRoot()));
        QSizeF sh = m_pExitButton->minimumSizeHint();
        m_pExitButton->setTransformOriginPoint(sh.width() / 2, sh.height() / 2);
        m_pExitButton->setRotation(180);
        m_pExitButton->hide();
    }
}

/* static */
void UIGChooserItemGroup::copyContent(UIGChooserItemGroup *pFrom, UIGChooserItemGroup *pTo)
{
    /* Copy group-items: */
    foreach (UIGChooserItem *pGroupItem, pFrom->items(UIGChooserItemType_Group))
        new UIGChooserItemGroup(pTo, pGroupItem->toGroupItem());
    /* Copy machine-items: */
    foreach (UIGChooserItem *pMachineItem, pFrom->items(UIGChooserItemType_Machine))
        new UIGChooserItemMachine(pTo, pMachineItem->toMachineItem());
}

void UIGChooserItemGroup::handleRootStatusChange()
{
    /* Update visible name: */
    updateVisibleName();
    /* Update minimum header size: */
    updateHeaderSize();
}

void UIGChooserItemGroup::updateVisibleName()
{
    /* Not for main root: */
    if (isMainRoot())
        return;

    /* Prepare variables: */
    int iHorizontalMargin = data(GroupItemData_HorizonalMargin).toInt();
    int iMajorSpacing = data(GroupItemData_MajorSpacing).toInt();
    int iMinorSpacing = data(GroupItemData_MinorSpacing).toInt();
    int iRootIndent = data(GroupItemData_RootIndent).toInt();
    int iToggleButtonWidth = data(GroupItemData_ToggleButtonSize).toSize().width();
    int iEnterButtonWidth = data(GroupItemData_EnterButtonSize).toSize().width();
    int iExitButtonWidth = data(GroupItemData_ExitButtonSize).toSize().width();
    int iGroupPixmapWidth = data(GroupItemData_GroupPixmapSize).toSize().width();
    int iMachinePixmapWidth = data(GroupItemData_MachinePixmapSize).toSize().width();
    int iGroupCountTextWidth = data(GroupItemData_GroupCountTextSize).toSize().width();
    int iMachineCountTextWidth = data(GroupItemData_MachineCountTextSize).toSize().width();
    int iMaximumWidth = geometry().width();

    /* Left margin: */
    if (isRoot())
        iMaximumWidth -= iRootIndent;
    iMaximumWidth -= iHorizontalMargin;
    /* Button width: */
    if (isRoot())
        iMaximumWidth -= iExitButtonWidth;
    else
        iMaximumWidth -= iToggleButtonWidth;
    /* Spacing between button and name: */
    iMaximumWidth -= iMajorSpacing;
    if (isHovered())
    {
        /* Spacing between name and info: */
        iMaximumWidth -= iMajorSpacing;
        /* Group info width: */
        if (!m_groupItems.isEmpty())
            iMaximumWidth -= (iGroupPixmapWidth + iGroupCountTextWidth);
        /* Machine info width: */
        if (!m_machineItems.isEmpty())
            iMaximumWidth -= (iMachinePixmapWidth + iMachineCountTextWidth);
        /* Spacing + button width: */
        if (!isRoot())
            iMaximumWidth -= (iMinorSpacing + iEnterButtonWidth);
    }
    /* Right margin: */
    iMaximumWidth -= iHorizontalMargin;
    if (isRoot())
        iMaximumWidth -= iRootIndent;

    /* Recache visible name: */
    m_strVisibleName = compressText(m_nameFont, model()->paintDevice(), name(), iMaximumWidth);

    /* Repaint item: */
    update();
}

void UIGChooserItemGroup::updateHeaderSize()
{
    /* Not for main root: */
    if (isMainRoot())
        return;

    /* Prepare variables: */
    int iMajorSpacing = data(GroupItemData_MajorSpacing).toInt();
    int iMinorSpacing = data(GroupItemData_MinorSpacing).toInt();
    QSize exitButtonSize = data(GroupItemData_ExitButtonSize).toSize();
    QSize toggleButtonSize = data(GroupItemData_ToggleButtonSize).toSize();
    QSize groupPixmapSize = data(GroupItemData_GroupPixmapSize).toSize();
    QSize groupCountTextSize = data(GroupItemData_GroupCountTextSize).toSize();
    QSize machinePixmapSize = data(GroupItemData_MachinePixmapSize).toSize();
    QSize machineCountTextSize = data(GroupItemData_MachineCountTextSize).toSize();
    QSize enterButtonSize = data(GroupItemData_EnterButtonSize).toSize();

    /* Calculate minimum visible name size: */
    QPaintDevice *pPaintDevice = model()->paintDevice();
    QFontMetrics fm(m_nameFont, pPaintDevice);
    int iMaximumNameWidth = textWidth(m_nameFont, pPaintDevice, 20);
    QString strCompressedName = compressText(m_nameFont, pPaintDevice, name(), iMaximumNameWidth);
    int iMinimumNameWidth = fm.width(strCompressedName);
    int iMinimumNameHeight = fm.height();

    /* Calculate minimum width: */
    int iHeaderWidth = 0;
    /* Button width: */
    if (isRoot())
        iHeaderWidth += exitButtonSize.width();
    else
        iHeaderWidth += toggleButtonSize.width();
    iHeaderWidth += /* Spacing between button and name: */
                    iMajorSpacing +
                    /* Minimum name width: */
                    iMinimumNameWidth +
                    /* Spacing between name and info: */
                    iMajorSpacing;
    /* Group info width: */
    if (!m_groupItems.isEmpty())
        iHeaderWidth += (groupPixmapSize.width() + groupCountTextSize.width());
    /* Machine info width: */
    if (!m_machineItems.isEmpty())
        iHeaderWidth += (machinePixmapSize.width() + machineCountTextSize.width());
    /* Spacing + button width: */
    if (!isRoot())
        iHeaderWidth += (iMinorSpacing + enterButtonSize.width());

    /* Search for maximum height: */
    QList<int> heights;
    /* Button height: */
    if (isRoot())
        heights << exitButtonSize.height();
    else
        heights << toggleButtonSize.height();
    heights /* Minimum name height: */
            << iMinimumNameHeight
            /* Group info heights: */
            << groupPixmapSize.height() << groupCountTextSize.height()
            /* Machine info heights: */
            << machinePixmapSize.height() << machineCountTextSize.height();
    /* Button height: */
    if (!isRoot())
        heights << enterButtonSize.height();
    int iHeaderHeight = 0;
    foreach (int iHeight, heights)
        iHeaderHeight = qMax(iHeaderHeight, iHeight);

    /* Return result: */
    m_headerSize = QSize(iHeaderWidth, iHeaderHeight);
}

void UIGChooserItemGroup::updateToolTip()
{
    /* Not for main root: */
    if (isMainRoot())
        return;

    /* Prepare variables: */
    QStringList toolTipInfo;

    /* Should we add name? */
    if (!name().isEmpty())
    {
        /* Template: */
        QString strTemplateForName = tr("<b>%1</b>", "Group item tool-tip / Group name");

        /* Append value: */
        toolTipInfo << strTemplateForName.arg(name());
    }

    /* Should we add group info? */
    if (!items(UIGChooserItemType_Group).isEmpty())
    {
        /* Template: */
        QString strGroupCount = tr("%n group(s)", "Group item tool-tip / Group info", items(UIGChooserItemType_Group).size());

        /* Append value: */
        QString strValue = tr("<nobr>%1</nobr>", "Group item tool-tip / Group info wrapper").arg(strGroupCount);
        toolTipInfo << strValue;
    }

    /* Should we add machine info? */
    if (!items(UIGChooserItemType_Machine).isEmpty())
    {
        /* Check if 'this' group contains started VMs: */
        int iCountOfStartedMachineItems = 0;
        foreach (UIGChooserItem *pItem, items(UIGChooserItemType_Machine))
            if (UIVMItem::isItemStarted(pItem->toMachineItem()))
                ++iCountOfStartedMachineItems;
        /* Template: */
        QString strMachineCount = tr("%n machine(s)", "Group item tool-tip / Machine info", items(UIGChooserItemType_Machine).size());
        QString strStartedMachineCount = tr("(%n running)", "Group item tool-tip / Running machine info", iCountOfStartedMachineItems);

        /* Append value: */
        QString strValue = !iCountOfStartedMachineItems ?
                           tr("<nobr>%1</nobr>", "Group item tool-tip / Machine info wrapper").arg(strMachineCount) :
                           tr("<nobr>%1 %2</nobr>", "Group item tool-tip / Machine info wrapper, including running").arg(strMachineCount).arg(strStartedMachineCount);
        toolTipInfo << strValue;
    }

    /* Set tool-tip: */
    setToolTip(toolTipInfo.join("<br>"));
}

void UIGChooserItemGroup::updateToggleButtonToolTip()
{
    /* Is toggle-button created? */
    if (!m_pToggleButton)
        return;

    /* Update toggle-button tool-tip: */
    m_pToggleButton->setToolTip(isOpened() ? tr("Collapse group") : tr("Expand group"));
}

void UIGChooserItemGroup::retranslateUi()
{
    /* Update group tool-tip: */
    updateToolTip();

    /* Update button tool-tips: */
    if (m_pEnterButton)
        m_pEnterButton->setToolTip(tr("Enter group"));
    if (m_pExitButton)
        m_pExitButton->setToolTip(tr("Exit group"));
    updateToggleButtonToolTip();
}

void UIGChooserItemGroup::show()
{
    /* Call to base-class: */
    UIGChooserItem::show();
    /* Show children: */
    if (!isClosed())
        foreach (UIGChooserItem *pItem, items())
            pItem->show();
}

void UIGChooserItemGroup::hide()
{
    /* Call to base-class: */
    UIGChooserItem::hide();
    /* Hide children: */
    foreach (UIGChooserItem *pItem, items())
        pItem->hide();
}

void UIGChooserItemGroup::startEditing()
{
    /* Not for root: */
    if (isRoot())
        return;

    /* Not while saving groups: */
    if (model()->isGroupSavingInProgress())
        return;

    /* Unlock name-editor: */
    m_pNameEditor->show();
    m_pNameEditorWidget->setText(name());
    m_pNameEditorWidget->setFocus();
}

void UIGChooserItemGroup::addItem(UIGChooserItem *pItem, int iPosition)
{
    /* Check item type: */
    switch (pItem->type())
    {
        case UIGChooserItemType_Group:
        {
            AssertMsg(!m_groupItems.contains(pItem), ("Group-item already added!"));
            if (iPosition < 0 || iPosition >= m_groupItems.size())
                m_groupItems.append(pItem);
            else
                m_groupItems.insert(iPosition, pItem);
            scene()->addItem(pItem);
            break;
        }
        case UIGChooserItemType_Machine:
        {
            AssertMsg(!m_machineItems.contains(pItem), ("Machine-item already added!"));
            if (iPosition < 0 || iPosition >= m_machineItems.size())
                m_machineItems.append(pItem);
            else
                m_machineItems.insert(iPosition, pItem);
            scene()->addItem(pItem);
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }

    /* Update: */
    updateVisibleName();
    updateHeaderSize();
    updateToolTip();
}

void UIGChooserItemGroup::removeItem(UIGChooserItem *pItem)
{
    /* Check item type: */
    switch (pItem->type())
    {
        case UIGChooserItemType_Group:
        {
            AssertMsg(m_groupItems.contains(pItem), ("Group-item was not found!"));
            scene()->removeItem(pItem);
            m_groupItems.removeAt(m_groupItems.indexOf(pItem));
            break;
        }
        case UIGChooserItemType_Machine:
        {
            AssertMsg(m_machineItems.contains(pItem), ("Machine-item was not found!"));
            scene()->removeItem(pItem);
            m_machineItems.removeAt(m_machineItems.indexOf(pItem));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }

    /* Update: */
    updateVisibleName();
    updateHeaderSize();
    updateToolTip();
}

void UIGChooserItemGroup::setItems(const QList<UIGChooserItem*> &items, UIGChooserItemType type)
{
    /* Check item type: */
    switch (type)
    {
        case UIGChooserItemType_Group: m_groupItems = items; break;
        case UIGChooserItemType_Machine: m_machineItems = items; break;
        default: AssertMsgFailed(("Invalid item type!")); break;
    }

    /* Update: */
    updateVisibleName();
    updateHeaderSize();
    updateToolTip();
}

QList<UIGChooserItem*> UIGChooserItemGroup::items(UIGChooserItemType type /* = UIGChooserItemType_Any */) const
{
    switch (type)
    {
        case UIGChooserItemType_Any: return items(UIGChooserItemType_Group) + items(UIGChooserItemType_Machine);
        case UIGChooserItemType_Group: return m_groupItems;
        case UIGChooserItemType_Machine: return m_machineItems;
        default: break;
    }
    return QList<UIGChooserItem*>();
}

bool UIGChooserItemGroup::hasItems(UIGChooserItemType type /* = UIGChooserItemType_Any */) const
{
    switch (type)
    {
        case UIGChooserItemType_Any:
            return hasItems(UIGChooserItemType_Group) || hasItems(UIGChooserItemType_Machine);
        case UIGChooserItemType_Group:
            return !m_groupItems.isEmpty();
        case UIGChooserItemType_Machine:
            return !m_machineItems.isEmpty();
    }
    return false;
}

void UIGChooserItemGroup::clearItems(UIGChooserItemType type /* = UIGChooserItemType_Any */)
{
    switch (type)
    {
        case UIGChooserItemType_Any:
        {
            clearItems(UIGChooserItemType_Group);
            clearItems(UIGChooserItemType_Machine);
            break;
        }
        case UIGChooserItemType_Group:
        {
            while (!m_groupItems.isEmpty()) { delete m_groupItems.last(); }
            AssertMsg(m_groupItems.isEmpty(), ("Group items cleanup failed!"));
            break;
        }
        case UIGChooserItemType_Machine:
        {
            while (!m_machineItems.isEmpty()) { delete m_machineItems.last(); }
            AssertMsg(m_machineItems.isEmpty(), ("Machine items cleanup failed!"));
            break;
        }
    }

    /* Update: */
    updateVisibleName();
    updateHeaderSize();
    updateToolTip();
}

void UIGChooserItemGroup::updateAll(const QString &strId)
{
    /* Update all the required items recursively: */
    foreach (UIGChooserItem *pItem, items())
        pItem->updateAll(strId);
}

void UIGChooserItemGroup::removeAll(const QString &strId)
{
    /* Remove all the required items recursively: */
    foreach (UIGChooserItem *pItem, items())
        pItem->removeAll(strId);
}

UIGChooserItem* UIGChooserItemGroup::searchForItem(const QString &strSearchTag, int iItemSearchFlags)
{
    /* Are we searching among group-items? */
    if (iItemSearchFlags & UIGChooserItemSearchFlag_Group)
    {
        /* Are we searching by the exact name? */
        if (iItemSearchFlags & UIGChooserItemSearchFlag_ExactName)
        {
            /* Exact name matches? */
            if (name() == strSearchTag)
                return this;
        }
        /* Are we searching by the few first symbols? */
        else
        {
            /* Name starts with passed symbols? */
            if (name().startsWith(strSearchTag, Qt::CaseInsensitive))
                return this;
        }
    }

    /* Search among all the children, but machines first: */
    foreach (UIGChooserItem *pItem, items(UIGChooserItemType_Machine))
        if (UIGChooserItem *pFoundItem = pItem->searchForItem(strSearchTag, iItemSearchFlags))
            return pFoundItem;
    foreach (UIGChooserItem *pItem, items(UIGChooserItemType_Group))
        if (UIGChooserItem *pFoundItem = pItem->searchForItem(strSearchTag, iItemSearchFlags))
            return pFoundItem;

    /* Found nothing? */
    return 0;
}

UIGChooserItemMachine* UIGChooserItemGroup::firstMachineItem()
{
    /* If this group-item have at least one machine-item: */
    if (hasItems(UIGChooserItemType_Machine))
        /* Return the first machine-item: */
        return items(UIGChooserItemType_Machine).first()->firstMachineItem();
    /* If this group-item have at least one group-item: */
    else if (hasItems(UIGChooserItemType_Group))
        /* Return the first machine-item of the first group-item: */
        return items(UIGChooserItemType_Group).first()->firstMachineItem();
    /* Found nothing? */
    return 0;
}

void UIGChooserItemGroup::sortItems()
{
    /* Sort group-items: */
    QMap<QString, UIGChooserItem*> sorter;
    foreach (UIGChooserItem *pItem, items(UIGChooserItemType_Group))
        sorter.insert(pItem->name().toLower(), pItem);
    setItems(sorter.values(), UIGChooserItemType_Group);

    /* Sort machine-items: */
    sorter.clear();
    foreach (UIGChooserItem *pItem, items(UIGChooserItemType_Machine))
        sorter.insert(pItem->name().toLower(), pItem);
    setItems(sorter.values(), UIGChooserItemType_Machine);

    /* Update model: */
    model()->updateNavigation();
    model()->updateLayout();
}

void UIGChooserItemGroup::updateLayout()
{
    /* Update size-hints for all the children: */
    foreach (UIGChooserItem *pItem, items())
        pItem->updateGeometry();
    /* Update size-hint for this item: */
    updateGeometry();

    /* Prepare variables: */
    int iHorizontalMargin = data(GroupItemData_HorizonalMargin).toInt();
    int iVerticalMargin = data(GroupItemData_VerticalMargin).toInt();
    int iMinorSpacing = data(GroupItemData_MinorSpacing).toInt();
    int iFullHeaderHeight = data(GroupItemData_FullHeaderSize).toSize().height();
    int iRootIndent = data(GroupItemData_RootIndent).toInt();
    int iPreviousVerticalIndent = 0;

    /* Header (root-item): */
    if (isRoot())
    {
        /* Header (main root-item): */
        if (isMainRoot())
        {
            /* Prepare body indent: */
            iPreviousVerticalIndent = iRootIndent;
        }
        /* Header (non-main root-item): */
        else
        {
            /* Hide unnecessary buttons: */
            if (m_pToggleButton)
                m_pToggleButton->hide();
            if (m_pEnterButton)
                m_pEnterButton->hide();

            /* Exit-button: */
            if (m_pExitButton)
            {
                /* Prepare variables: */
                int iExitButtonHeight = data(GroupItemData_ExitButtonSize).toSize().height();
                /* Layout exit-button: */
                int iExitButtonX = iHorizontalMargin + iRootIndent;
                int iExitButtonY = iExitButtonHeight == iFullHeaderHeight ? iVerticalMargin :
                                   iVerticalMargin + (iFullHeaderHeight - iExitButtonHeight) / 2;
                m_pExitButton->setPos(iExitButtonX, iExitButtonY);
                /* Show exit-button: */
                m_pExitButton->show();
            }

            /* Prepare body indent: */
            iPreviousVerticalIndent = iVerticalMargin + iFullHeaderHeight + iVerticalMargin + iMinorSpacing;
        }
    }
    /* Header (non-root-item): */
    else
    {
        /* Prepare variables: */
        QSize toggleButtonSize = data(GroupItemData_ToggleButtonSize).toSize();

        /* Hide unnecessary button: */
        if (m_pExitButton)
            m_pExitButton->hide();

        /* Toggle-button: */
        if (m_pToggleButton)
        {
            /* Prepare variables: */
            int iToggleButtonHeight = toggleButtonSize.height();
            /* Layout toggle-button: */
            int iToggleButtonX = iHorizontalMargin;
            int iToggleButtonY = iToggleButtonHeight == iFullHeaderHeight ? iVerticalMargin :
                                 iVerticalMargin + (iFullHeaderHeight - iToggleButtonHeight) / 2;
            m_pToggleButton->setPos(iToggleButtonX, iToggleButtonY);
            /* Show toggle-button: */
            m_pToggleButton->show();
        }

        /* Enter-button: */
        if (m_pEnterButton)
        {
            /* Prepare variables: */
            int iFullWidth = geometry().width();
            QSizeF enterButtonSize = data(GroupItemData_EnterButtonSize).toSize();
            int iEnterButtonWidth = enterButtonSize.width();
            int iEnterButtonHeight = enterButtonSize.height();
            /* Layout enter-button: */
            int iEnterButtonX = iFullWidth - iHorizontalMargin - iEnterButtonWidth;
            int iEnterButtonY = iEnterButtonHeight == iFullHeaderHeight ? iVerticalMargin :
                                iVerticalMargin + (iFullHeaderHeight - iEnterButtonHeight) / 2;
            m_pEnterButton->setPos(iEnterButtonX, iEnterButtonY);
        }

        /* Name-editor: */
        if (m_pNameEditor && m_pNameEditorWidget)
        {
            /* Prepare variables: */
            int iMajorSpacing = data(GroupItemData_MajorSpacing).toInt();
            int iToggleButtonWidth = toggleButtonSize.width();
            /* Layout name-editor: */
            int iNameEditorX = iHorizontalMargin + iToggleButtonWidth + iMajorSpacing;
            int iNameEditorY = 1;
            m_pNameEditor->setPos(iNameEditorX, iNameEditorY);
            m_pNameEditorWidget->resize(geometry().width() - iNameEditorX - iHorizontalMargin, m_pNameEditorWidget->height());
        }

        /* Prepare body indent: */
        iPreviousVerticalIndent = 3 * iVerticalMargin + iFullHeaderHeight;
    }

    /* No body for closed group: */
    if (isClosed())
    {
        /* Hide all the items: */
        foreach (UIGChooserItem *pItem, items())
            pItem->hide();
    }
    /* Body for opened group: */
    else
    {
        /* Prepare variables: */
        int iHorizontalIndent = isRoot() ? iRootIndent : iHorizontalMargin;
        QRect geo = geometry().toRect();
        int iX = geo.x();
        int iY = geo.y();
        int iWidth = geo.width();

        /* Layout all the items: */
        foreach (UIGChooserItem *pItem, items())
        {
            /* Show if hidden: */
            pItem->show();
            /* Get item height-hint: */
            int iMinimumHeight = pItem->minimumHeightHint();
            /* Set item position: */
            pItem->setPos(iX + iHorizontalIndent, iY + iPreviousVerticalIndent);
            /* Set item size: */
            pItem->resize(iWidth - 2 * iHorizontalIndent, iMinimumHeight);
            /* Relayout group: */
            pItem->updateLayout();
            /* Update indent for next items: */
            iPreviousVerticalIndent += (iMinimumHeight + iMinorSpacing);
        }
    }
}

int UIGChooserItemGroup::minimumWidthHint(bool fOpenedGroup) const
{
    /* Prepare variables: */
    int iHorizontalMargin = data(GroupItemData_HorizonalMargin).toInt();
    int iRootIndent = data(GroupItemData_RootIndent).toInt();
    int iFullHeaderWidth = data(GroupItemData_FullHeaderSize).toSize().width();

    /* Calculating proposed width: */
    int iProposedWidth = 0;

    /* Simple group-item have 2 margins - left and right: */
    iProposedWidth += 2 * iHorizontalMargin;
    /* And full header width to take into account: */
    iProposedWidth += iFullHeaderWidth;
    /* But if group is opened: */
    if (fOpenedGroup)
    {
        /* Prepare variables: */
        int iHorizontalIndent = isRoot() ? iRootIndent : iHorizontalMargin;
        /* We have to make sure that we had taken into account: */
        foreach (UIGChooserItem *pItem, items())
        {
            int iItemWidth = 2 * iHorizontalIndent + pItem->minimumWidthHint();
            iProposedWidth = qMax(iProposedWidth, iItemWidth);
        }
    }

    /* Return result: */
    return iProposedWidth;
}

int UIGChooserItemGroup::minimumHeightHint(bool fOpenedGroup) const
{
    /* Prepare variables: */
    int iHorizontalMargin = data(GroupItemData_HorizonalMargin).toInt();
    int iVerticalMargin = data(GroupItemData_VerticalMargin).toInt();
    int iMinorSpacing = data(GroupItemData_MinorSpacing).toInt();
    int iFullHeaderHeight = data(GroupItemData_FullHeaderSize).toSize().height();

    /* Calculating proposed height: */
    int iProposedHeight = 0;

    /* Simple group-item have 2 margins - top and bottom: */
    iProposedHeight += 2 * iVerticalMargin;
    /* And full header height to take into account: */
    iProposedHeight += iFullHeaderHeight;
    /* But if group is opened: */
    if (fOpenedGroup)
    {
        /* We should take into account vertical indent: */
        iProposedHeight += iVerticalMargin;
        /* And every item height: */
        QList<UIGChooserItem*> allItems = items();
        for (int i = 0; i < allItems.size(); ++i)
        {
            UIGChooserItem *pItem = allItems[i];
            iProposedHeight += (pItem->minimumHeightHint() + iMinorSpacing);
        }
        /* Minus last spacing: */
        iProposedHeight -= iMinorSpacing;
        /* Bottom margin: */
        iProposedHeight += iHorizontalMargin;
    }
    /* Finally, additional height during animation: */
    if (!fOpenedGroup && m_pToggleButton && m_pToggleButton->isAnimationRunning())
        iProposedHeight += m_iAdditionalHeight;

    /* Return result: */
    return iProposedHeight;
}

int UIGChooserItemGroup::minimumWidthHint() const
{
    return minimumWidthHint(isOpened());
}

int UIGChooserItemGroup::minimumHeightHint() const
{
    return minimumHeightHint(isOpened());
}

QSizeF UIGChooserItemGroup::minimumSizeHint(bool fOpenedGroup) const
{
    return QSizeF(minimumWidthHint(fOpenedGroup), minimumHeightHint(fOpenedGroup));
}

QSizeF UIGChooserItemGroup::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* If Qt::MinimumSize requested: */
    if (which == Qt::MinimumSize)
        return minimumSizeHint(isOpened());
    /* Else call to base-class: */
    return UIGChooserItem::sizeHint(which, constraint);
}

QPixmap UIGChooserItemGroup::toPixmap()
{
    QSize minimumSize = minimumSizeHint(false).toSize();
    QPixmap pixmap(minimumSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QStyleOptionGraphicsItem options;
    options.rect = QRect(QPoint(0, 0), minimumSize);
    paint(&painter, &options);
    return pixmap;
}

bool UIGChooserItemGroup::isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where) const
{
    /* No drops while saving groups: */
    if (model()->isGroupSavingInProgress())
        return false;
    /* Get mime: */
    const QMimeData *pMimeData = pEvent->mimeData();
    /* If drag token is shown, its up to parent to decide: */
    if (where != DragToken_Off)
        return parentItem()->isDropAllowed(pEvent);
    /* Else we should check mime format: */
    if (pMimeData->hasFormat(UIGChooserItemGroup::className()))
    {
        /* Get passed group-item: */
        const UIGChooserItemMimeData *pCastedMimeData = qobject_cast<const UIGChooserItemMimeData*>(pMimeData);
        AssertMsg(pCastedMimeData, ("Can't cast passed mime-data to UIGChooserItemMimeData!"));
        UIGChooserItem *pItem = pCastedMimeData->item();
        /* Make sure passed group is mutable within this group: */
        if (pItem->toGroupItem()->isContainsLockedMachine() &&
            !items(UIGChooserItemType_Group).contains(pItem))
            return false;
        /* Make sure passed group is not 'this': */
        if (pItem == this)
            return false;
        /* Make sure passed group is not among our parents: */
        const UIGChooserItem *pTestedWidget = this;
        while (UIGChooserItem *pParentOfTestedWidget = pTestedWidget->parentItem())
        {
            if (pItem == pParentOfTestedWidget)
                return false;
            pTestedWidget = pParentOfTestedWidget;
        }
        return true;
    }
    else if (pMimeData->hasFormat(UIGChooserItemMachine::className()))
    {
        /* Get passed machine-item: */
        const UIGChooserItemMimeData *pCastedMimeData = qobject_cast<const UIGChooserItemMimeData*>(pMimeData);
        AssertMsg(pCastedMimeData, ("Can't cast passed mime-data to UIGChooserItemMimeData!"));
        UIGChooserItem *pItem = pCastedMimeData->item();
        /* Make sure passed machine is mutable within this group: */
        if (pItem->toMachineItem()->isLockedMachine() &&
            !items(UIGChooserItemType_Machine).contains(pItem))
            return false;
        switch (pEvent->proposedAction())
        {
            case Qt::MoveAction:
            {
                /* Make sure passed item is ours or there is no other item with such id: */
                return m_machineItems.contains(pItem) || !isContainsMachine(pItem->toMachineItem()->id());
            }
            case Qt::CopyAction:
            {
                /* Make sure there is no other item with such id: */
                return !isContainsMachine(pItem->toMachineItem()->id());
            }
        }
    }
    /* That was invalid mime: */
    return false;
}

void UIGChooserItemGroup::processDrop(QGraphicsSceneDragDropEvent *pEvent, UIGChooserItem *pFromWho, DragToken where)
{
    /* Get mime: */
    const QMimeData *pMime = pEvent->mimeData();
    /* Check mime format: */
    if (pMime->hasFormat(UIGChooserItemGroup::className()))
    {
        switch (pEvent->proposedAction())
        {
            case Qt::MoveAction:
            case Qt::CopyAction:
            {
                /* Remember scene: */
                UIGChooserModel *pModel = model();

                /* Get passed group-item: */
                const UIGChooserItemMimeData *pCastedMime = qobject_cast<const UIGChooserItemMimeData*>(pMime);
                AssertMsg(pCastedMime, ("Can't cast passed mime-data to UIGChooserItemMimeData!"));
                UIGChooserItem *pItem = pCastedMime->item();

                /* Check if we have position information: */
                int iPosition = m_groupItems.size();
                if (pFromWho && where != DragToken_Off)
                {
                    /* Make sure sender item if our child: */
                    AssertMsg(m_groupItems.contains(pFromWho), ("Sender item is NOT our child!"));
                    if (m_groupItems.contains(pFromWho))
                    {
                        iPosition = m_groupItems.indexOf(pFromWho);
                        if (where == DragToken_Down)
                            ++iPosition;
                    }
                }

                /* Copy passed item into this group: */
                UIGChooserItem *pNewGroupItem = new UIGChooserItemGroup(this, pItem->toGroupItem(), iPosition);
                if (isClosed())
                    open(false);

                /* If proposed action is 'move': */
                if (pEvent->proposedAction() == Qt::MoveAction)
                {
                    /* Delete passed item: */
                    delete pItem;
                }

                /* Update model: */
                pModel->cleanupGroupTree();
                pModel->updateNavigation();
                pModel->updateLayout();
                pModel->setCurrentItem(pNewGroupItem);
                pModel->saveGroupSettings();
                break;
            }
            default:
                break;
        }
    }
    else if (pMime->hasFormat(UIGChooserItemMachine::className()))
    {
        switch (pEvent->proposedAction())
        {
            case Qt::MoveAction:
            case Qt::CopyAction:
            {
                /* Remember scene: */
                UIGChooserModel *pModel = model();

                /* Get passed item: */
                const UIGChooserItemMimeData *pCastedMime = qobject_cast<const UIGChooserItemMimeData*>(pMime);
                AssertMsg(pCastedMime, ("Can't cast passed mime-data to UIGChooserItemMimeData!"));
                UIGChooserItem *pItem = pCastedMime->item();

                /* Check if we have position information: */
                int iPosition = m_machineItems.size();
                if (pFromWho && where != DragToken_Off)
                {
                    /* Make sure sender item if our child: */
                    AssertMsg(m_machineItems.contains(pFromWho), ("Sender item is NOT our child!"));
                    if (m_machineItems.contains(pFromWho))
                    {
                        iPosition = m_machineItems.indexOf(pFromWho);
                        if (where == DragToken_Down)
                            ++iPosition;
                    }
                }

                /* Copy passed machine-item into this group: */
                UIGChooserItem *pNewMachineItem = new UIGChooserItemMachine(this, pItem->toMachineItem(), iPosition);
                if (isClosed())
                    open(false);

                /* If proposed action is 'move': */
                if (pEvent->proposedAction() == Qt::MoveAction)
                {
                    /* Delete passed item: */
                    delete pItem;
                }

                /* Update model: */
                pModel->cleanupGroupTree();
                pModel->updateNavigation();
                pModel->updateLayout();
                pModel->setCurrentItem(pNewMachineItem);
                pModel->saveGroupSettings();
                break;
            }
            default:
                break;
        }
    }
}

void UIGChooserItemGroup::resetDragToken()
{
    /* Reset drag token for this item: */
    if (dragTokenPlace() != DragToken_Off)
    {
        setDragTokenPlace(DragToken_Off);
        update();
    }
    /* Reset drag tokens for all the items: */
    foreach (UIGChooserItem *pItem, items())
        pItem->resetDragToken();
}

QMimeData* UIGChooserItemGroup::createMimeData()
{
    return new UIGChooserItemMimeData(this);
}

void UIGChooserItemGroup::hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent)
{
    /* Skip if hovered: */
    if (isHovered())
        return;

    /* Prepare variables: */
    QPoint pos = pEvent->pos().toPoint();
    int iMargin = data(GroupItemData_VerticalMargin).toInt();
    int iHeaderHeight = data(GroupItemData_FullHeaderSize).toSize().height();
    int iFullHeaderHeight = 2 * iMargin + iHeaderHeight;
    /* Skip if hovered part out of the header: */
    if (pos.y() >= iFullHeaderHeight)
        return;

    /* Call to base-class: */
    UIGChooserItem::hoverMoveEvent(pEvent);

    /* Update visible name: */
    updateVisibleName();
}

void UIGChooserItemGroup::hoverLeaveEvent(QGraphicsSceneHoverEvent *pEvent)
{
    /* Skip if not hovered: */
    if (!isHovered())
        return;

    /* Call to base-class: */
    UIGChooserItem::hoverLeaveEvent(pEvent);

    /* Update visible name: */
    updateVisibleName();
}

void UIGChooserItemGroup::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget* /* pWidget = 0 */)
{
    /* Setup: */
    pPainter->setRenderHint(QPainter::Antialiasing);

    /* Paint background: */
    paintBackground(pPainter, pOption->rect);

    /* Paint header: */
    paintHeader(pPainter, pOption->rect);
}

void UIGChooserItemGroup::paintBackground(QPainter *pPainter, const QRect &rect)
{
    /* Save painter: */
    pPainter->save();

    /* Prepare color: */
    QPalette pal = palette();
    QColor windowColor = pal.color(QPalette::Active,
                                   model()->currentItems().contains(this) ?
                                   QPalette::Highlight : QPalette::Window);

    /* Root-item: */
    if (isRoot())
    {
        /* Main root-item: */
        if (isMainRoot())
        {
            /* Simple and clear: */
            pPainter->fillRect(rect, QColor(240, 240, 240));
        }
        /* Non-main root-item: */
        else
        {
            /* Prepare variables: */
            int iMargin = data(GroupItemData_VerticalMargin).toInt();
            int iRootIndent = data(GroupItemData_RootIndent).toInt();
            int iHeaderHeight = data(GroupItemData_FullHeaderSize).toSize().height();
            int iFullHeaderHeight = 2 * iMargin + iHeaderHeight;
            QRect backgroundRect = QRect(0, 0, rect.width(), iFullHeaderHeight);

            /* Add clipping: */
            QPainterPath path;
            path.moveTo(iRootIndent, 0);
            path.lineTo(path.currentPosition().x(), iFullHeaderHeight - 10);
            path.arcTo(QRectF(path.currentPosition(), QSizeF(20, 20)).translated(0, -10), 180, 90);
            path.lineTo(rect.width() - 10 - iRootIndent, path.currentPosition().y());
            path.arcTo(QRectF(path.currentPosition(), QSizeF(20, 20)).translated(-10, -20), 270, 90);
            path.lineTo(path.currentPosition().x(), 0);
            path.closeSubpath();
            pPainter->setClipPath(path);

            /* Fill background: */
            QLinearGradient headerGradient(backgroundRect.bottomLeft(), backgroundRect.topLeft());
            headerGradient.setColorAt(1, windowColor.darker(blackoutDarkness()));
            headerGradient.setColorAt(0, windowColor.darker(animationDarkness()));
            pPainter->fillRect(backgroundRect, headerGradient);

            /* Stroke path: */
            pPainter->setClipping(false);
            pPainter->strokePath(path, windowColor.darker(strokeDarkness()));
        }
    }
    /* Non-root-item: */
    else
    {
        /* Prepare variables: */
        int iMargin = data(GroupItemData_VerticalMargin).toInt();
        int iHeaderHeight = data(GroupItemData_FullHeaderSize).toSize().height();
        int iFullHeaderHeight = 2 * iMargin + iHeaderHeight;
        int iFullHeight = rect.height();

        /* Add clipping: */
        QPainterPath path;
        path.moveTo(10, 0);
        path.arcTo(QRectF(path.currentPosition(), QSizeF(20, 20)).translated(-10, 0), 90, 90);
        path.lineTo(path.currentPosition().x(), iFullHeight - 10);
        path.arcTo(QRectF(path.currentPosition(), QSizeF(20, 20)).translated(0, -10), 180, 90);
        path.lineTo(rect.width() - 10, path.currentPosition().y());
        path.arcTo(QRectF(path.currentPosition(), QSizeF(20, 20)).translated(-10, -20), 270, 90);
        path.lineTo(path.currentPosition().x(), 10);
        path.arcTo(QRectF(path.currentPosition(), QSizeF(20, 20)).translated(-20, -10), 0, 90);
        path.closeSubpath();
        pPainter->setClipPath(path);

        /* Calculate top rectangle: */
        QRect tRect = rect;
        tRect.setBottom(tRect.top() + iFullHeaderHeight);
        /* Prepare top gradient: */
        QLinearGradient tGradient(tRect.bottomLeft(), tRect.topLeft());
        tGradient.setColorAt(1, windowColor.darker(animationDarkness()));
        tGradient.setColorAt(0, windowColor.darker(blackoutDarkness()));
        /* Fill top rectangle: */
        pPainter->fillRect(tRect, tGradient);

        if (rect.height() > tRect.height())
        {
            /* Calculate middle rectangle: */
            QRect midRect = QRect(tRect.bottomLeft(), rect.bottomRight());
            /* Paint all the stuff: */
            pPainter->fillRect(midRect, QColor(245, 245, 245));
        }

         /* Stroke path: */
        pPainter->setClipping(false);
        pPainter->strokePath(path, windowColor.darker(strokeDarkness()));
        pPainter->setClipPath(path);

        /* Paint drag token UP? */
        if (dragTokenPlace() != DragToken_Off)
        {
            QLinearGradient dragTokenGradient;
            QRect dragTokenRect = rect;
            if (dragTokenPlace() == DragToken_Up)
            {
                dragTokenRect.setHeight(5);
                dragTokenGradient.setStart(dragTokenRect.bottomLeft());
                dragTokenGradient.setFinalStop(dragTokenRect.topLeft());
            }
            else if (dragTokenPlace() == DragToken_Down)
            {
                dragTokenRect.setTopLeft(dragTokenRect.bottomLeft() - QPoint(0, 5));
                dragTokenGradient.setStart(dragTokenRect.topLeft());
                dragTokenGradient.setFinalStop(dragTokenRect.bottomLeft());
            }
            dragTokenGradient.setColorAt(0, windowColor.darker(dragTokenDarkness()));
            dragTokenGradient.setColorAt(1, windowColor.darker(dragTokenDarkness() + 40));
            pPainter->fillRect(dragTokenRect, dragTokenGradient);
        }
    }

    /* Restore painter: */
    pPainter->restore();
}

void UIGChooserItemGroup::paintHeader(QPainter *pPainter, const QRect &rect)
{
    /* Not for main root: */
    if (isMainRoot())
        return;

    /* Prepare variables: */
    int iHorizontalMargin = data(GroupItemData_HorizonalMargin).toInt();
    int iVerticalMargin = data(GroupItemData_VerticalMargin).toInt();
    int iMajorSpacing = data(GroupItemData_MajorSpacing).toInt();
    int iRootIndent = data(GroupItemData_RootIndent).toInt();
    QSize toggleButtonSize = data(GroupItemData_ToggleButtonSize).toSize();
    QSize exitButtonSize = data(GroupItemData_ExitButtonSize).toSize();
    QSize nameSize = data(GroupItemData_NameSize).toSize();
    int iFullHeaderHeight = data(GroupItemData_FullHeaderSize).toSize().height();

    /* Update palette: */
    if (model()->currentItems().contains(this))
    {
        QPalette pal = palette();
        pPainter->setPen(pal.color(QPalette::HighlightedText));
    }

    /* Update buttons: */
    if (m_pToggleButton)
        m_pToggleButton->setParentSelected(model()->currentItems().contains(this));
    if (m_pEnterButton)
        m_pEnterButton->setParentSelected(model()->currentItems().contains(this));
    if (m_pExitButton)
        m_pExitButton->setParentSelected(model()->currentItems().contains(this));

    /* Paint name: */
    int iNameX = iHorizontalMargin;
    if (isRoot())
        iNameX += iRootIndent + exitButtonSize.width();
    else
        iNameX += toggleButtonSize.width();
    iNameX += iMajorSpacing;
    int iNameY = nameSize.height() == iFullHeaderHeight ? iVerticalMargin :
                 iVerticalMargin + (iFullHeaderHeight - nameSize.height()) / 2;
    paintText(/* Painter: */
              pPainter,
              /* Point to paint in: */
              QPoint(iNameX, iNameY),
              /* Font to paint text: */
              m_nameFont,
              /* Paint device: */
              model()->paintDevice(),
              /* Text to paint: */
              data(GroupItemData_Name).toString());

    /* Should we add more info? */
    if (isHovered())
    {
        /* Show enter-button: */
        if (!isRoot() && m_pEnterButton)
            m_pEnterButton->show();

        /* Prepare variables: */
        int iMinorSpacing = data(GroupItemData_MinorSpacing).toInt();
        int iEnterButtonWidth = data(GroupItemData_EnterButtonSize).toSize().width();
        QSize groupPixmapSize = data(GroupItemData_GroupPixmapSize).toSize();
        QSize machinePixmapSize = data(GroupItemData_MachinePixmapSize).toSize();
        QSize groupCountTextSize = data(GroupItemData_GroupCountTextSize).toSize();
        QSize machineCountTextSize = data(GroupItemData_MachineCountTextSize).toSize();
        QString strGroupCountText = data(GroupItemData_GroupCountText).toString();
        QString strMachineCountText = data(GroupItemData_MachineCountText).toString();

        /* Indent: */
        int iHorizontalIndent = rect.right() - iHorizontalMargin;
        if (!isRoot())
            iHorizontalIndent -= (iEnterButtonWidth + iMinorSpacing);

        /* Should we draw machine count info? */
        if (!strMachineCountText.isEmpty())
        {
            iHorizontalIndent -= machineCountTextSize.width();
            int iMachineCountTextX = iHorizontalIndent;
            int iMachineCountTextY = machineCountTextSize.height() == iFullHeaderHeight ?
                                     iVerticalMargin : iVerticalMargin + (iFullHeaderHeight - machineCountTextSize.height()) / 2;
            paintText(/* Painter: */
                      pPainter,
                      /* Point to paint in: */
                      QPoint(iMachineCountTextX, iMachineCountTextY),
                      /* Font to paint text: */
                      m_infoFont,
                      /* Paint device: */
                      model()->paintDevice(),
                      /* Text to paint: */
                      strMachineCountText);

            iHorizontalIndent -= machinePixmapSize.width();
            int iMachinePixmapX = iHorizontalIndent;
            int iMachinePixmapY = machinePixmapSize.height() == iFullHeaderHeight ?
                                  iVerticalMargin : iVerticalMargin + (iFullHeaderHeight - machinePixmapSize.height()) / 2;
            paintPixmap(/* Painter: */
                        pPainter,
                        /* Rectangle to paint in: */
                        QRect(QPoint(iMachinePixmapX, iMachinePixmapY), machinePixmapSize),
                        /* Pixmap to paint: */
                        m_machinesPixmap);
        }

        /* Should we draw group count info? */
        if (!strGroupCountText.isEmpty())
        {
            iHorizontalIndent -= groupCountTextSize.width();
            int iGroupCountTextX = iHorizontalIndent;
            int iGroupCountTextY = groupCountTextSize.height() == iFullHeaderHeight ?
                                   iVerticalMargin : iVerticalMargin + (iFullHeaderHeight - groupCountTextSize.height()) / 2;
            paintText(/* Painter: */
                      pPainter,
                      /* Point to paint in: */
                      QPoint(iGroupCountTextX, iGroupCountTextY),
                      /* Font to paint text: */
                      m_infoFont,
                      /* Paint device: */
                      model()->paintDevice(),
                      /* Text to paint: */
                      strGroupCountText);

            iHorizontalIndent -= groupPixmapSize.width();
            int iGroupPixmapX = iHorizontalIndent;
            int iGroupPixmapY = groupPixmapSize.height() == iFullHeaderHeight ?
                                iVerticalMargin : iVerticalMargin + (iFullHeaderHeight - groupPixmapSize.height()) / 2;
            paintPixmap(/* Painter: */
                        pPainter,
                        /* Rectangle to paint in: */
                        QRect(QPoint(iGroupPixmapX, iGroupPixmapY), groupPixmapSize),
                        /* Pixmap to paint: */
                        m_groupsPixmap);
        }
    }
    else
    {
        /* Hide enter-button: */
        if (m_pEnterButton)
            m_pEnterButton->hide();
    }
}

void UIGChooserItemGroup::updateAnimationParameters()
{
    /* Only for item with button: */
    if (!m_pToggleButton)
        return;

    /* Recalculate animation parameters: */
    QSizeF openedSize = minimumSizeHint(true);
    QSizeF closedSize = minimumSizeHint(false);
    int iAdditionalHeight = openedSize.height() - closedSize.height();
    m_pToggleButton->setAnimationRange(0, iAdditionalHeight);
}

void UIGChooserItemGroup::setAdditionalHeight(int iAdditionalHeight)
{
    m_iAdditionalHeight = iAdditionalHeight;
    model()->updateLayout();
}

int UIGChooserItemGroup::additionalHeight() const
{
    return m_iAdditionalHeight;
}

UIGroupRenameEditor::UIGroupRenameEditor(const QString &strName, UIGChooserItem *pParent)
    : m_pParent(pParent)
    , m_pLineEdit(0)
    , m_pTemporaryMenu(0)
{
    /* Create line-edit: */
    m_pLineEdit = new QLineEdit(strName, this);
    m_pLineEdit->setTextMargins(0, 0, 0, 0);
    connect(m_pLineEdit, SIGNAL(returnPressed()), this, SIGNAL(sigEditingFinished()));
    /* Create main-layout: */
    QHBoxLayout *pLayout = new QHBoxLayout(this);
    pLayout->setContentsMargins(0, 0, 0, 0);
    /* Add line-edit into main-layout: */
    pLayout->addWidget(m_pLineEdit);
    /* Install event-filter: */
    m_pLineEdit->installEventFilter(this);
}

QString UIGroupRenameEditor::text() const
{
    return m_pLineEdit->text();
}

void UIGroupRenameEditor::setText(const QString &strText)
{
    m_pLineEdit->setText(strText);
}

void UIGroupRenameEditor::setFont(const QFont &font)
{
    QWidget::setFont(font);
    m_pLineEdit->setFont(font);
}

void UIGroupRenameEditor::setFocus()
{
    m_pLineEdit->setFocus();
}

bool UIGroupRenameEditor::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Process only events sent to line-edit: */
    if (pWatched != m_pLineEdit)
        return QWidget::eventFilter(pWatched, pEvent);

    /* Handle events: */
    switch (pEvent->type())
    {
        case QEvent::ContextMenu:
        {
            /* Handle context-menu event: */
            handleContextMenuEvent(static_cast<QContextMenuEvent*>(pEvent));
            /* Filter out this event: */
            return true;
        }
        case QEvent::FocusOut:
        {
            if (!m_pTemporaryMenu)
                emit sigEditingFinished();
            break;
        }
        default:
            break;
    }

    /* Call to base-class: */
    return QWidget::eventFilter(pWatched, pEvent);
}

void UIGroupRenameEditor::handleContextMenuEvent(QContextMenuEvent *pContextMenuEvent)
{
    /* Prepare variables: */
    QGraphicsView *pView = m_pParent->model()->scene()->views().first();

    /* Create context-menu: */
    m_pTemporaryMenu = new QMenu(pView);
    QMenu *pMenu = m_pLineEdit->createStandardContextMenu();
    const QList<QAction*> &actions = pMenu->actions();
    foreach (QAction *pAction, actions)
        m_pTemporaryMenu->addAction(pAction);

    /* Determine global position: */
    QPoint subItemPos = pContextMenuEvent->pos();
    QPoint itemPos = mapToParent(subItemPos);
    QPointF scenePos = m_pParent->mapToScene(itemPos);
    QPoint viewPos = pView->mapFromScene(scenePos);
    QPoint globalPos = pView->mapToGlobal(viewPos);

    /* Show context menu: */
    m_pTemporaryMenu->exec(globalPos);

    /* Delete context menu: */
    delete m_pTemporaryMenu;
    m_pTemporaryMenu = 0;
    delete pMenu;
}

