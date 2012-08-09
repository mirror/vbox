/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGDetailsSet class implementation
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

/* GUI includes: */
#include "UIGDetailsSet.h"
#include "UIGDetailsModel.h"
#include "UIGDetailsElements.h"
#include "UIVMItem.h"
#include "UIConverter.h"
#include "UIVirtualBoxEventHandler.h"
#include "VBoxGlobal.h"

/* COM includes: */
#include "CUSBController.h"

UIGDetailsSet::UIGDetailsSet(UIGDetailsItem *pParent)
    : UIGDetailsItem(pParent)
    , m_iStep(-1)
    , m_iLastStep(-1)
{
    /* Setup size-policy: */
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    /* Add item to the parent: */
    parentItem()->addItem(this);

    /* Prepare connections: */
    connect(this, SIGNAL(sigStartFirstStep(QString)), this, SLOT(sltFirstStep(QString)), Qt::QueuedConnection);
    connect(this, SIGNAL(sigElementPrepared(QString)), this, SLOT(sltNextStep(QString)), Qt::QueuedConnection);
    connect(this, SIGNAL(sigSetPrepared()), this, SLOT(sltSetPrepared()), Qt::QueuedConnection);
    connect(gVBoxEvents, SIGNAL(sigMachineStateChange(QString, KMachineState)), this, SLOT(sltMachineStateChange(QString)));
    connect(gVBoxEvents, SIGNAL(sigMachineDataChange(QString)), this, SLOT(sltMachineAttributesChange(QString)));
    connect(gVBoxEvents, SIGNAL(sigSessionStateChange(QString, KSessionState)), this, SLOT(sltMachineAttributesChange(QString)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotChange(QString, QString)), this, SLOT(sltMachineAttributesChange(QString)));
    connect(&vboxGlobal(), SIGNAL(mediumEnumStarted()), this, SLOT(sltUpdateAppearance()));
    connect(&vboxGlobal(), SIGNAL(mediumEnumFinished(const VBoxMediaList &)), this, SLOT(sltUpdateAppearance()));
}

UIGDetailsSet::~UIGDetailsSet()
{
    /* Delete all the items: */
    clearItems();

    /* Remove item from the parent: */
    parentItem()->removeItem(this);
}

void UIGDetailsSet::configure(UIVMItem *pItem, const QStringList &settings, bool fFullSet)
{
    /* Assign settings: */
    m_machine = pItem->machine();
    m_settings = settings;

    /* Create elements step-by-step: */
    prepareElements(fFullSet);
}

const CMachine& UIGDetailsSet::machine() const
{
    return m_machine;
}

void UIGDetailsSet::sltFirstStep(QString strSetId)
{
    /* Was that a requested set? */
    if (strSetId != m_strSetId)
        return;

    /* Prepare first element: */
    m_iStep = DetailsElementType_General;
    prepareElement(strSetId);
}

void UIGDetailsSet::sltNextStep(QString strSetId)
{
    /* Was that a requested set? */
    if (strSetId != m_strSetId)
        return;

    /* Prepare next element: */
    ++m_iStep;
    prepareElement(strSetId);
}

void UIGDetailsSet::sltSetPrepared()
{
    /* Reset step index: */
    m_iStep = -1;
    /* Notify parent group: */
    emit sigSetCreationDone();
}

void UIGDetailsSet::sltMachineStateChange(QString strId)
{
    /* Is this our VM changed? */
    if (machine().GetId() != strId)
        return;

    /* Update hover accessibility: */
    foreach (UIGDetailsItem *pItem, items())
        pItem->toElement()->updateHoverAccessibility();

    /* Update appearance: */
    foreach (UIGDetailsItem *pItem, items())
        pItem->toElement()->updateAppearance();
}

void UIGDetailsSet::sltMachineAttributesChange(QString strId)
{
    /* Is this our VM changed? */
    if (machine().GetId() != strId)
        return;

    /* Update appearance: */
    foreach (UIGDetailsItem *pItem, items())
        pItem->toElement()->updateAppearance();
}

void UIGDetailsSet::sltUpdateAppearance()
{
    /* Update appearance: */
    foreach (UIGDetailsItem *pItem, items())
        pItem->toElement()->updateAppearance();
}

QVariant UIGDetailsSet::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case SetData_Margin: return 0;
        case SetData_Spacing: return 3;
        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIGDetailsSet::addItem(UIGDetailsItem *pItem)
{
    switch (pItem->type())
    {
        case UIGDetailsItemType_Element:
        {
            UIGDetailsElement *pElement = pItem->toElement();
            DetailsElementType type = pElement->elementType();
            AssertMsg(!m_elements.contains(type), ("Element already added!"));
            m_elements.insert(type, pItem);
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }
}

void UIGDetailsSet::removeItem(UIGDetailsItem *pItem)
{
    switch (pItem->type())
    {
        case UIGDetailsItemType_Element:
        {
            UIGDetailsElement *pElement = pItem->toElement();
            DetailsElementType type = pElement->elementType();
            AssertMsg(m_elements.contains(type), ("Element do not present (type = %d)!", (int)type));
            m_elements.remove(type);
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }
}

QList<UIGDetailsItem*> UIGDetailsSet::items(UIGDetailsItemType type /* = UIGDetailsItemType_Element */) const
{
    switch (type)
    {
        case UIGDetailsItemType_Any: return items(UIGDetailsItemType_Element);
        case UIGDetailsItemType_Element: return m_elements.values();
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
    return QList<UIGDetailsItem*>();
}

bool UIGDetailsSet::hasItems(UIGDetailsItemType type /* = UIGDetailsItemType_Element */) const
{
    switch (type)
    {
        case UIGDetailsItemType_Any: return hasItems(UIGDetailsItemType_Element);
        case UIGDetailsItemType_Element: return !m_elements.isEmpty();
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
    return false;
}

void UIGDetailsSet::clearItems(UIGDetailsItemType type /* = UIGDetailsItemType_Element */)
{
    switch (type)
    {
        case UIGDetailsItemType_Any:
        {
            clearItems(UIGDetailsItemType_Element);
            break;
        }
        case UIGDetailsItemType_Element:
        {
            foreach (int iKey, m_elements.keys())
                delete m_elements[iKey];
            AssertMsg(m_elements.isEmpty(), ("Set items cleanup failed!"));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }
}

UIGDetailsElement* UIGDetailsSet::element(DetailsElementType elementType) const
{
    UIGDetailsItem *pItem = m_elements.value(elementType, 0);
    if (pItem)
        return pItem->toElement();
    return 0;
}

void UIGDetailsSet::updateLayout()
{
    /* Update size-hints for all the items: */
    foreach (UIGDetailsItem *pItem, items())
        pItem->updateSizeHint();
    /* Update size-hint for this item: */
    updateSizeHint();

    /* Prepare variables: */
    int iMargin = data(SetData_Margin).toInt();
    int iSpacing = data(SetData_Spacing).toInt();
    int iMaximumWidth = geometry().size().toSize().width();
    int iVerticalIndent = iMargin;

    /* Layout all the elements: */
    foreach (UIGDetailsItem *pItem, items())
    {
        /* Get particular element: */
        UIGDetailsElement *pElement = pItem->toElement();
        if (!pElement->isVisible())
            continue;

        /* For each particular element: */
        switch (pElement->elementType())
        {
            case DetailsElementType_General:
            case DetailsElementType_System:
            {
                UIGDetailsElement *pPreviewElement = element(DetailsElementType_Preview);
                int iPreviewWidth = pPreviewElement && pPreviewElement->isVisible() ? pPreviewElement->minimumWidthHint() : 0;
                int iWidth = iPreviewWidth == 0 ? iMaximumWidth - 2 * iMargin :
                                                  iMaximumWidth - 2 * iMargin - iSpacing - iPreviewWidth;
                pElement->setPos(iMargin, iVerticalIndent);
                /* Resize to required width: */
                int iHeight = pElement->minimumHeightHint();
                pElement->resize(iWidth, iHeight);
                /* Update minimum height hint: */
                pItem->updateSizeHint();
                /* Resize to required height: */
                iHeight = pElement->minimumHeightHint();
                pElement->resize(iWidth, iHeight);
                pItem->updateLayout();
                iVerticalIndent += (iHeight + iSpacing);
                break;
            }
            case DetailsElementType_Preview:
            {
                int iWidth = pElement->minimumWidthHint();
                int iHeight = pElement->minimumHeightHint();
                pElement->setPos(iMaximumWidth - iMargin - iWidth, iMargin);
                pElement->resize(iWidth, iHeight);
                pItem->updateLayout();
                iVerticalIndent = qMax(iVerticalIndent, iHeight + iSpacing);
                break;
            }
            case DetailsElementType_Display:
            case DetailsElementType_Storage:
            case DetailsElementType_Audio:
            case DetailsElementType_Network:
            case DetailsElementType_Serial:
#ifdef VBOX_WITH_PARALLEL_PORTS
            case DetailsElementType_Parallel:
#endif /* VBOX_WITH_PARALLEL_PORTS */
            case DetailsElementType_USB:
            case DetailsElementType_SF:
            case DetailsElementType_Description:
            {
                int iWidth = iMaximumWidth - 2 * iMargin;
                pElement->setPos(iMargin, iVerticalIndent);
                /* Resize to required width: */
                int iHeight = pElement->minimumHeightHint();
                pElement->resize(iWidth, iHeight);
                /* Update minimum height hint: */
                pItem->updateSizeHint();
                /* Resize to required height: */
                iHeight = pElement->minimumHeightHint();
                pElement->resize(iWidth, iHeight);
                pItem->updateLayout();
                iVerticalIndent += (iHeight + iSpacing);
                break;
            }
        }
    }
}

int UIGDetailsSet::minimumWidthHint() const
{
    /* Prepare variables: */
    int iMargin = data(SetData_Margin).toInt();
    int iSpacing = data(SetData_Spacing).toInt();
    int iProposedWidth = 0;

    /* Layout all the elements: */
    foreach (UIGDetailsItem *pItem, items())
    {
        /* Get particular element: */
        UIGDetailsElement *pElement = pItem->toElement();
        if (!pElement->isVisible())
            continue;

        /* For each particular element: */
        switch (pElement->elementType())
        {
            case DetailsElementType_General:
            case DetailsElementType_System:
            case DetailsElementType_Display:
            case DetailsElementType_Storage:
            case DetailsElementType_Audio:
            case DetailsElementType_Network:
            case DetailsElementType_Serial:
#ifdef VBOX_WITH_PARALLEL_PORTS
            case DetailsElementType_Parallel:
#endif /* VBOX_WITH_PARALLEL_PORTS */
            case DetailsElementType_USB:
            case DetailsElementType_SF:
            case DetailsElementType_Description:
            {
                iProposedWidth = qMax(iProposedWidth, pElement->minimumWidthHint());
                break;
            }
            case DetailsElementType_Preview:
            {
                UIGDetailsElement *pGeneralElement = element(DetailsElementType_General);
                UIGDetailsElement *pSystemElement = element(DetailsElementType_System);
                int iGeneralElementWidth = pGeneralElement ? pGeneralElement->minimumWidthHint() : 0;
                int iSystemElementWidth = pSystemElement ? pSystemElement->minimumWidthHint() : 0;
                int iFirstColumnWidth = qMax(iGeneralElementWidth, iSystemElementWidth);
                iProposedWidth = qMax(iProposedWidth, iFirstColumnWidth + iSpacing + pElement->minimumWidthHint());
                break;
            }
        }
    }

    /* Two margins finally: */
    iProposedWidth += 2 * iMargin;

    /* Return result: */
    return iProposedWidth;
}

int UIGDetailsSet::minimumHeightHint() const
{
    /* Prepare variables: */
    int iMargin = data(SetData_Margin).toInt();
    int iSpacing = data(SetData_Spacing).toInt();
    int iProposedHeight = 0;

    /* Layout all the elements: */
    foreach (UIGDetailsItem *pItem, items())
    {
        /* Get particular element: */
        UIGDetailsElement *pElement = pItem->toElement();
        if (!pElement->isVisible())
            continue;

        /* For each particular element: */
        switch (pElement->elementType())
        {
            case DetailsElementType_General:
            case DetailsElementType_System:
            case DetailsElementType_Display:
            case DetailsElementType_Storage:
            case DetailsElementType_Audio:
            case DetailsElementType_Network:
            case DetailsElementType_Serial:
#ifdef VBOX_WITH_PARALLEL_PORTS
            case DetailsElementType_Parallel:
#endif /* VBOX_WITH_PARALLEL_PORTS */
            case DetailsElementType_USB:
            case DetailsElementType_SF:
            case DetailsElementType_Description:
            {
                iProposedHeight += (pElement->minimumHeightHint() + iSpacing);
                break;
            }
            case DetailsElementType_Preview:
            {
                iProposedHeight = qMax(iProposedHeight, pElement->minimumHeightHint() + iSpacing);
                break;
            }
        }
    }

    /* Minus last spacing: */
    iProposedHeight -= iSpacing;

    /* Two margins finally: */
    iProposedHeight += 2 * iMargin;

    /* Return result: */
    return iProposedHeight;
}

QSizeF UIGDetailsSet::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* If Qt::MinimumSize requested: */
    if (which == Qt::MinimumSize || which == Qt::PreferredSize)
    {
        /* Return wrappers: */
        return QSizeF(minimumWidthHint(), minimumHeightHint());
    }

    /* Call to base-class: */
    return UIGDetailsItem::sizeHint(which, constraint);
}

void UIGDetailsSet::prepareElements(bool fFullSet)
{
    /* Which will be the last step? */
    m_iLastStep = fFullSet ? DetailsElementType_Description : DetailsElementType_Preview;
    /* Cleanup superfluous elements: */
    if (!fFullSet)
        for (int i = DetailsElementType_Display; i <= DetailsElementType_Description; ++i)
            if (m_elements.contains(i))
                delete m_elements[i];

    /* Per-set configuration: */
    {
        /* USB controller restrictions: */
        const CUSBController &ctl = machine().GetUSBController();
        if (ctl.isNull() || !ctl.GetProxyAvailable())
        {
            QString strElementTypeOpened = gpConverter->toInternalString(DetailsElementType_USB);
            QString strElementTypeClosed = strElementTypeOpened + "Closed";
            m_settings.removeAll(strElementTypeOpened);
            m_settings.removeAll(strElementTypeClosed);
        }
    }

    /* Prepare first element: */
    m_strSetId = QUuid::createUuid().toString();
    emit sigStartFirstStep(m_strSetId);
}

void UIGDetailsSet::prepareElement(QString strSetId)
{
    /* Step number feats the bounds: */
    if (m_iStep <= m_iLastStep)
    {
        /* Load details settings: */
        DetailsElementType elementType = (DetailsElementType)m_iStep;
        QString strElementTypeOpened = gpConverter->toInternalString(elementType);
        QString strElementTypeClosed = strElementTypeOpened + "Closed";
        /* Should be element visible? */
        bool fVisible = m_settings.contains(strElementTypeOpened) || m_settings.contains(strElementTypeClosed);
        /* Should be element opened? */
        bool fOpen = m_settings.contains(strElementTypeOpened);

        /* Check if element is present already: */
        bool fJustCreated = false;
        UIGDetailsElement *pElement = element(elementType);
        /* Create if necessary: */
        if (!pElement)
        {
            pElement = createElement(elementType, fOpen);
            fJustCreated = true;
        }
        /* Prepare element: */
        if (fVisible && !pElement->isVisible())
            pElement->show();
        else if (!fVisible && pElement->isVisible())
            pElement->hide();
        if (pElement->isVisible())
            pElement->updateAppearance();
        if (fJustCreated)
            model()->updateLayout();
        /* Mark element prepared: */
        emit sigElementPrepared(strSetId);
    }
    /* Step number out of bounds: */
    else
    {
        /* Mark whole set prepared: */
        emit sigSetPrepared();
    }
}

UIGDetailsElement* UIGDetailsSet::createElement(DetailsElementType elementType, bool fOpen)
{
    UIGDetailsElement *pElement = 0;
    switch (elementType)
    {
        case DetailsElementType_General:
        {
            /* Create 'general' element: */
            pElement = new UIGDetailsElementGeneral(this, fOpen);
            break;
        }
        case DetailsElementType_System:
        {
            /* Create 'system' element: */
            pElement = new UIGDetailsElementSystem(this, fOpen);
            break;
        }
        case DetailsElementType_Preview:
        {
            /* Create 'preview' element: */
            pElement = new UIGDetailsElementPreview(this, fOpen);
            break;
        }
        case DetailsElementType_Display:
        {
            /* Create 'display' element: */
            pElement = new UIGDetailsElementDisplay(this, fOpen);
            break;
        }
        case DetailsElementType_Storage:
        {
            /* Create 'storage' element: */
            pElement = new UIGDetailsElementStorage(this, fOpen);
            break;
        }
        case DetailsElementType_Audio:
        {
            /* Create 'audio' element: */
            pElement = new UIGDetailsElementAudio(this, fOpen);
            break;
        }
        case DetailsElementType_Network:
        {
            /* Create 'network' element: */
            pElement = new UIGDetailsElementNetwork(this, fOpen);
            break;
        }
        case DetailsElementType_Serial:
        {
            /* Create 'serial' element: */
            pElement = new UIGDetailsElementSerial(this, fOpen);
            break;
        }
#ifdef VBOX_WITH_PARALLEL_PORTS
        case DetailsElementType_Parallel:
        {
            /* Create 'parallel' element: */
            pElement = new UIGDetailsElementParallel(this, fOpen);
            break;
        }
#endif /* VBOX_WITH_PARALLEL_PORTS */
        case DetailsElementType_USB:
        {
            /* Create 'usb' element: */
            pElement = new UIGDetailsElementUSB(this, fOpen);
            break;
        }
        case DetailsElementType_SF:
        {
            /* Create 'sf' element: */
            pElement = new UIGDetailsElementSF(this, fOpen);
            break;
        }
        case DetailsElementType_Description:
        {
            /* Create 'description' element: */
            pElement = new UIGDetailsElementDescription(this, fOpen);
            break;
        }
    }
    return pElement;
}

