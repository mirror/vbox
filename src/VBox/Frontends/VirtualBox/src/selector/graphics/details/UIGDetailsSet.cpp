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
    , m_fFullSet(true)
    , m_pStep(0)
    , m_iStep(-1)
    , m_iLastStep(-1)
{
    /* Setup size-policy: */
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    /* Add item to the parent: */
    parentItem()->addItem(this);

    /* Prepare connections: */
    connect(this, SIGNAL(sigStartFirstStep(QString)), this, SLOT(sltFirstStep(QString)), Qt::QueuedConnection);
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
    m_fFullSet = fFullSet;
    m_machine = pItem->machine();
    m_settings = settings;

    /* Create elements step-by-step: */
    prepareElements();
}

const CMachine& UIGDetailsSet::machine() const
{
    return m_machine;
}

void UIGDetailsSet::sltFirstStep(QString strSetId)
{
    /* Clear step: */
    delete m_pStep;
    m_pStep = 0;

    /* Was that a requested set? */
    if (strSetId != m_strSetId)
        return;

    /* Prepare first element: */
    m_iStep = DetailsElementType_General;
    prepareElement(strSetId);
}

void UIGDetailsSet::sltNextStep(QString strSetId)
{
    /* Clear step: */
    delete m_pStep;
    m_pStep = 0;

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
    prepareElements();
}

void UIGDetailsSet::sltMachineAttributesChange(QString strId)
{
    /* Is this our VM changed? */
    if (machine().GetId() != strId)
        return;

    /* Update appearance: */
    prepareElements();
}

void UIGDetailsSet::sltUpdateAppearance()
{
    /* Update appearance: */
    prepareElements();
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

int UIGDetailsSet::minimumWidthHint() const
{
    /* Prepare variables: */
    int iMargin = data(SetData_Margin).toInt();
    int iSpacing = data(SetData_Spacing).toInt();
    int iMinimumWidthHint = 0;

    /* Take into account all the elements: */
    foreach (UIGDetailsItem *pItem, items())
    {
        /* Skip hidden: */
        if (!pItem->isVisible())
            continue;

        /* For each particular element: */
        UIGDetailsElement *pElement = pItem->toElement();
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
                iMinimumWidthHint = qMax(iMinimumWidthHint, pItem->minimumWidthHint());
                break;
            }
            case DetailsElementType_Preview:
            {
                UIGDetailsItem *pGeneralItem = element(DetailsElementType_General);
                UIGDetailsItem *pSystemItem = element(DetailsElementType_System);
                int iGeneralElementWidth = pGeneralItem ? pGeneralItem->minimumWidthHint() : 0;
                int iSystemElementWidth = pSystemItem ? pSystemItem->minimumWidthHint() : 0;
                int iFirstColumnWidth = qMax(iGeneralElementWidth, iSystemElementWidth);
                iMinimumWidthHint = qMax(iMinimumWidthHint, iFirstColumnWidth + iSpacing + pItem->minimumWidthHint());
                break;
            }
        }
    }

    /* And two margins finally: */
    iMinimumWidthHint += 2 * iMargin;

    /* Return result: */
    return iMinimumWidthHint;
}

int UIGDetailsSet::minimumHeightHint() const
{
    /* Prepare variables: */
    int iMargin = data(SetData_Margin).toInt();
    int iSpacing = data(SetData_Spacing).toInt();
    int iMinimumHeightHint = 0;

    /* Take into account all the elements: */
    foreach (UIGDetailsItem *pItem, items())
    {
        /* Skip hidden: */
        if (!pItem->isVisible())
            continue;

        /* For each particular element: */
        UIGDetailsElement *pElement = pItem->toElement();
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
                iMinimumHeightHint += (pItem->minimumHeightHint() + iSpacing);
                break;
            }
            case DetailsElementType_Preview:
            {
                iMinimumHeightHint = qMax(iMinimumHeightHint, pItem->minimumHeightHint() + iSpacing);
                break;
            }
        }
    }

    /* Minus last spacing: */
    iMinimumHeightHint -= iSpacing;

    /* And two margins finally: */
    iMinimumHeightHint += 2 * iMargin;

    /* Return result: */
    return iMinimumHeightHint;
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
                /* Move element: */
                pElement->setPos(iMargin, iVerticalIndent);
                /* Resize element to required width: */
                int iWidth = iMaximumWidth - 2 * iMargin;
                if (pElement->elementType() == DetailsElementType_General ||
                    pElement->elementType() == DetailsElementType_System)
                    if (UIGDetailsElement *pPreviewElement = element(DetailsElementType_Preview))
                        if (pPreviewElement->isVisible())
                            iWidth -= (iSpacing + pPreviewElement->minimumWidthHint());
                int iHeight = pElement->minimumHeightHint();
                pElement->resize(iWidth, iHeight);
                /* Update minimum-height-hint: */
                pElement->updateMinimumTextHeight();
                pItem->updateSizeHint();
                /* Resize element to required height: */
                iHeight = pElement->minimumHeightHint();
                pElement->resize(iWidth, iHeight);
                /* Layout element content: */
                pItem->updateLayout();
                /* Advance indent: */
                iVerticalIndent += (iHeight + iSpacing);
                break;
            }
            case DetailsElementType_Preview:
            {
                /* Prepare variables: */
                int iWidth = pElement->minimumWidthHint();
                int iHeight = pElement->minimumHeightHint();
                /* Move element: */
                pElement->setPos(iMaximumWidth - iMargin - iWidth, iMargin);
                /* Resize element: */
                pElement->resize(iWidth, iHeight);
                /* Layout element content: */
                pItem->updateLayout();
                /* Advance indent: */
                iVerticalIndent = qMax(iVerticalIndent, iHeight + iSpacing);
                break;
            }
        }
    }
}

void UIGDetailsSet::prepareElements()
{
    /* Which will be the last step? */
    m_iLastStep = m_fFullSet ? DetailsElementType_Description : DetailsElementType_Preview;
    /* Cleanup superfluous elements: */
    if (!m_fFullSet)
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

    /* Clear step: */
    delete m_pStep;
    m_pStep = 0;

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
        UIGDetailsElement *pElement = element(elementType);
        if (pElement && fOpen)
            pElement->open(false);
        /* Create element if necessary: */
        bool fJustCreated = false;
        if (!pElement)
        {
            fJustCreated = true;
            pElement = createElement(elementType, fOpen);
        }

        /* Show element if necessary: */
        if (fVisible && !pElement->isVisible())
        {
            /* Show the element: */
            pElement->show();
            /* Update layout: */
            model()->updateLayout();
        }
        /* Hide element if necessary: */
        else if (!fVisible && pElement->isVisible())
        {
            /* Hide the element: */
            pElement->hide();
            /* Update layout: */
            model()->updateLayout();
        }
        /* Update model if necessary: */
        else if (fJustCreated)
            model()->updateLayout();

        /* For visible element: */
        if (pElement->isVisible())
        {
            /* Create prepare step: */
            m_pStep = new UIPrepareStep(this, strSetId);
            connect(pElement, SIGNAL(sigElementUpdateDone()), m_pStep, SLOT(sltStepDone()), Qt::QueuedConnection);
            connect(m_pStep, SIGNAL(sigStepDone(const QString&)), this, SLOT(sltNextStep(const QString&)), Qt::QueuedConnection);

            /* Update element: */
            pElement->updateAppearance();
        }
        /* For invisible element: */
        else
        {
            /* Just go to the next step: */
            sltNextStep(strSetId);
        }
    }
    /* Step number out of bounds: */
    else
    {
        /* Mark whole set prepared: */
        model()->updateLayout();
        foreach (UIGDetailsItem *pElement, items())
            pElement->update();
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

