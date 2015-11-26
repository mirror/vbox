/* $Id$ */
/** @file
 * VBox Qt GUI - UIGInformationSet class implementation.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
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

/* GUI includes: */
# include "UIGInformationSet.h"
# include "UIGInformationModel.h"
# include "UIGInformationElements.h"
# include "UIVMItem.h"
# include "UIVirtualBoxEventHandler.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CUSBController.h"
# include "CUSBDeviceFilters.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIGInformationSet::UIGInformationSet(UIGInformationItem *pParent)
    : UIGInformationItem(pParent)
    , m_pMachineItem(0)
    , m_fHasDetails(false)
    , m_configurationAccessLevel(ConfigurationAccessLevel_Null)
    , m_fFullSet(true)
    , m_pBuildStep(0)
    , m_iLastStepNumber(-1)
{
    /* Add set to the parent group: */
    parentItem()->addItem(this);

    /* Prepare set: */
    prepareSet();

    /* Prepare connections: */
    prepareConnections();
}

UIGInformationSet::~UIGInformationSet()
{
    /* Cleanup items: */
    clearItems();

    /* Remove set from the parent group: */
    parentItem()->removeItem(this);
}

void UIGInformationSet::buildSet(UIVMItem *pMachineItem, bool fFullSet, const QMap<InformationElementType, bool> &settings)
{
    /* Remember passed arguments: */
    m_pMachineItem = pMachineItem;
    m_machine = m_pMachineItem->machine();
    m_fHasDetails = m_pMachineItem->hasDetails();
    m_fFullSet = fFullSet;
    m_settings = settings;

    /* Cleanup superfluous items: */
    if (!m_fFullSet || !m_fHasDetails)
    {
        int iFirstItem = m_fHasDetails ? InformationElementType_Display : InformationElementType_General;
        int iLastItem = InformationElementType_RuntimeAttributes;
        bool fCleanupPerformed = false;
        for (int i = iFirstItem; i <= iLastItem; ++i)
            if (m_elements.contains(i))
            {
                delete m_elements[i];
                fCleanupPerformed = true;
            }
        if (fCleanupPerformed)
            updateGeometry();
    }

    /* Make sure we have details: */
    if (!m_fHasDetails)
    {
        /* Reset last-step number: */
        m_iLastStepNumber = -1;
        /* Notify parent group we are built: */
        emit sigBuildDone();
        return;
    }

    /* Choose last-step number: */
    m_iLastStepNumber = m_fFullSet ? InformationElementType_Description : InformationElementType_Preview;

    /* Fetch USB controller restrictions: */
    const CUSBDeviceFilters &filters = m_machine.GetUSBDeviceFilters();
    if (filters.isNull() || !m_machine.GetUSBProxyAvailable())
        m_settings.remove(InformationElementType_USB);

    /* Start building set: */
    rebuildSet();
}

void UIGInformationSet::sltBuildStep(QString strStepId, int iStepNumber)
{
    /* Cleanup build-step: */
    delete m_pBuildStep;
    m_pBuildStep = 0;

    /* Is step id valid? */
    if (strStepId != m_strSetId)
        return;

    /* Step number feats the bounds: */
    if (iStepNumber >= 0 && iStepNumber <= m_iLastStepNumber)
    {
        /* Load details settings: */
        InformationElementType elementType = (InformationElementType)iStepNumber;
        /* Should the element be visible? */
        bool fVisible = m_settings.contains(elementType);
        /* Should the element be opened? */
        //bool fOpen = fVisible && m_settings[elementType];

        /* Check if element is present already: */
        UIGInformationElement *pElement = element(elementType);
        //if (pElement && fOpen)
            //pElement->open(false);
        /* Create element if necessary: */
        bool fJustCreated = false;
        if (!pElement)
        {
            fJustCreated = true;
            pElement = createElement(elementType, true);
        }

        /* Show element if necessary: */
        if (fVisible && !pElement->isVisible())
        {
            /* Show the element: */
            pElement->show();
            /* Recursively update size-hint: */
            pElement->updateGeometry();
            /* Update layout: */
            model()->updateLayout();
        }
        /* Hide element if necessary: */
        else if (!fVisible && pElement->isVisible())
        {
            /* Hide the element: */
            pElement->hide();
            /* Recursively update size-hint: */
            updateGeometry();
            /* Update layout: */
            model()->updateLayout();
        }
        /* Update model if necessary: */
        else if (fJustCreated)
            model()->updateLayout();

        /* For visible element: */
        if (pElement->isVisible())
        {
            /* Create next build-step: */
            m_pBuildStep = new UIInformationBuildStep(this, pElement, strStepId, iStepNumber + 1);

            /* Build element: */
            pElement->updateAppearance();
        }
        /* For invisible element: */
        else
        {
            /* Just build next step: */
            sltBuildStep(strStepId, iStepNumber + 1);
        }
    }
    /* Step number out of bounds: */
    else
    {
        /* Update model: */
        model()->updateLayout();
        /* Repaint all the items: */
        foreach (UIGInformationItem *pItem, items())
            pItem->update();
        /* Notify listener about build done: */
        emit sigBuildDone();
    }
}

void UIGInformationSet::sltMachineStateChange(QString strId)
{
    /* Is this our VM changed? */
    if (m_machine.GetId() != strId)
        return;

    /* Update appearance: */
    rebuildSet();
}

void UIGInformationSet::sltMachineAttributesChange(QString strId)
{
    /* Is this our VM changed? */
    if (m_machine.GetId() != strId)
        return;

    /* Update appearance: */
    rebuildSet();
}

void UIGInformationSet::sltUpdateAppearance()
{
    /* Update appearance: */
    rebuildSet();
}

QVariant UIGInformationSet::data(int iKey) const
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

void UIGInformationSet::addItem(UIGInformationItem *pItem)
{
    switch (pItem->type())
    {
        case UIGInformationItemType_Element:
        {
            UIGInformationElement *pElement = pItem->toElement();
            InformationElementType type = pElement->elementType();
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

void UIGInformationSet::removeItem(UIGInformationItem *pItem)
{
    switch (pItem->type())
    {
        case UIGInformationItemType_Element:
        {
            UIGInformationElement *pElement = pItem->toElement();
            InformationElementType type = pElement->elementType();
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

QList<UIGInformationItem*> UIGInformationSet::items(UIGInformationItemType type /* = UIGInformationItemType_Element */) const
{
    switch (type)
    {
        case UIGInformationItemType_Element: return m_elements.values();
        case UIGInformationItemType_Any: return items(UIGInformationItemType_Element);
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
    return QList<UIGInformationItem*>();
}

bool UIGInformationSet::hasItems(UIGInformationItemType type /* = UIGInformationItemType_Element */) const
{
    switch (type)
    {
        case UIGInformationItemType_Element: return !m_elements.isEmpty();
        case UIGInformationItemType_Any: return hasItems(UIGInformationItemType_Element);
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
    return false;
}

void UIGInformationSet::clearItems(UIGInformationItemType type /* = UIGInformationItemType_Element */)
{
    switch (type)
    {
        case UIGInformationItemType_Element:
        {
            foreach (int iKey, m_elements.keys())
                delete m_elements[iKey];
            AssertMsg(m_elements.isEmpty(), ("Set items cleanup failed!"));
            break;
        }
        case UIGInformationItemType_Any:
        {
            clearItems(UIGInformationItemType_Element);
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }
}

UIGInformationElement* UIGInformationSet::element(InformationElementType elementType) const
{
    UIGInformationItem *pItem = m_elements.value(elementType, 0);
    if (pItem)
        return pItem->toElement();
    return 0;
}

void UIGInformationSet::prepareSet()
{
    /* Setup size-policy: */
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}

void UIGInformationSet::prepareConnections()
{
    /* Global-events connections: */
    connect(gVBoxEvents, SIGNAL(sigMachineStateChange(QString, KMachineState)), this, SLOT(sltMachineStateChange(QString)));
    connect(gVBoxEvents, SIGNAL(sigMachineDataChange(QString)), this, SLOT(sltMachineAttributesChange(QString)));
    connect(gVBoxEvents, SIGNAL(sigSessionStateChange(QString, KSessionState)), this, SLOT(sltMachineAttributesChange(QString)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotTake(QString, QString)), this, SLOT(sltMachineAttributesChange(QString)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotDelete(QString, QString)), this, SLOT(sltMachineAttributesChange(QString)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotChange(QString, QString)), this, SLOT(sltMachineAttributesChange(QString)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotRestore(QString, QString)), this, SLOT(sltMachineAttributesChange(QString)));

    /* Meidum-enumeration connections: */
    connect(&vboxGlobal(), SIGNAL(sigMediumEnumerationStarted()), this, SLOT(sltUpdateAppearance()));
    connect(&vboxGlobal(), SIGNAL(sigMediumEnumerationFinished()), this, SLOT(sltUpdateAppearance()));
}

int UIGInformationSet::minimumWidthHint() const
{
    /* Zero if has no details: */
    if (!hasDetails())
        return 0;

    /* Prepare variables: */
    int iMargin = data(SetData_Margin).toInt();
    int iSpacing = data(SetData_Spacing).toInt();
    int iMinimumWidthHint = 0;

    /* Take into account all the elements: */
    foreach (UIGInformationItem *pItem, items())
    {
        /* Skip hidden: */
        if (!pItem->isVisible())
            continue;

        /* For each particular element: */
        UIGInformationElement *pElement = pItem->toElement();
        switch (pElement->elementType())
        {
            case InformationElementType_General:
            case InformationElementType_System:
            case InformationElementType_Display:
            case InformationElementType_Storage:
            case InformationElementType_Audio:
            case InformationElementType_Network:
            case InformationElementType_Serial:
#ifdef VBOX_WITH_PARALLEL_PORTS
            case InformationElementType_Parallel:
#endif /* VBOX_WITH_PARALLEL_PORTS */
            case InformationElementType_USB:
            case InformationElementType_SF:
            case InformationElementType_UI:
            case InformationElementType_Description:
            case InformationElementType_RuntimeAttributes:
            {
                iMinimumWidthHint = qMax(iMinimumWidthHint, pItem->minimumWidthHint());
                break;
            }
            case InformationElementType_Preview:
            {
                UIGInformationItem *pGeneralItem = element(InformationElementType_General);
                UIGInformationItem *pSystemItem = element(InformationElementType_System);
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

int UIGInformationSet::minimumHeightHint() const
{
    /* Zero if has no details: */
    if (!hasDetails())
        return 0;

    /* Prepare variables: */
    int iMargin = data(SetData_Margin).toInt();
    int iSpacing = data(SetData_Spacing).toInt();
    int iMinimumHeightHint = 0;

    /* Take into account all the elements: */
    foreach (UIGInformationItem *pItem, items())
    {
        /* Skip hidden: */
        if (!pItem->isVisible())
            continue;

        /* For each particular element: */
        UIGInformationElement *pElement = pItem->toElement();
        switch (pElement->elementType())
        {
            case InformationElementType_General:
            case InformationElementType_System:
            case InformationElementType_Display:
            case InformationElementType_Storage:
            case InformationElementType_Audio:
            case InformationElementType_Network:
            case InformationElementType_Serial:
#ifdef VBOX_WITH_PARALLEL_PORTS
            case InformationElementType_Parallel:
#endif /* VBOX_WITH_PARALLEL_PORTS */
            case InformationElementType_USB:
            case InformationElementType_SF:
            case InformationElementType_UI:
            case InformationElementType_Description:
            case InformationElementType_RuntimeAttributes:
            {
                iMinimumHeightHint += (pItem->minimumHeightHint() + iSpacing);
                break;
            }
            case InformationElementType_Preview:
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

void UIGInformationSet::updateLayout()
{
    /* Prepare variables: */
    int iMargin = data(SetData_Margin).toInt();
    int iSpacing = data(SetData_Spacing).toInt();
    int iMaximumWidth = geometry().size().toSize().width();
    int iVerticalIndent = iMargin;

    /* Layout all the elements: */
    foreach (UIGInformationItem *pItem, items())
    {
        /* Skip hidden: */
        if (!pItem->isVisible())
            continue;

        /* For each particular element: */
        UIGInformationElement *pElement = pItem->toElement();
        switch (pElement->elementType())
        {
            case InformationElementType_General:
            case InformationElementType_System:
            case InformationElementType_Display:
            case InformationElementType_Storage:
            case InformationElementType_Audio:
            case InformationElementType_Network:
            case InformationElementType_Serial:
#ifdef VBOX_WITH_PARALLEL_PORTS
            case InformationElementType_Parallel:
#endif /* VBOX_WITH_PARALLEL_PORTS */
            case InformationElementType_USB:
            case InformationElementType_SF:
            case InformationElementType_UI:
            case InformationElementType_Description:
            case InformationElementType_RuntimeAttributes:
            {
                /* Move element: */
                pElement->setPos(iMargin, iVerticalIndent);
                /* Calculate required width: */
                int iWidth = iMaximumWidth - 2 * iMargin;
                if (pElement->elementType() == InformationElementType_General ||
                    pElement->elementType() == InformationElementType_System)
                    if (UIGInformationElement *pPreviewElement = element(InformationElementType_Preview))
                        if (pPreviewElement->isVisible())
                            iWidth -= (iSpacing + pPreviewElement->minimumWidthHint());
                /* If element width is wrong: */
                if (pElement->geometry().width() != iWidth)
                {
                    /* Resize element to required width: */
                    pElement->resize(iWidth, pElement->geometry().height());
                }
                /* Acquire required height: */
                int iHeight = pElement->minimumHeightHint();
                /* If element height is wrong: */
                if (pElement->geometry().height() != iHeight)
                {
                    /* Resize element to required height: */
                    pElement->resize(pElement->geometry().width(), iHeight);
                }
                /* Layout element content: */
                pItem->updateLayout();
                /* Advance indent: */
                iVerticalIndent += (iHeight + iSpacing);
                break;
            }
            case InformationElementType_Preview:
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

void UIGInformationSet::rebuildSet()
{
    /* Make sure we have details: */
    if (!m_fHasDetails)
        return;

    /* Recache properties: */
    m_configurationAccessLevel = m_pMachineItem->configurationAccessLevel();

    /* Cleanup build-step: */
    delete m_pBuildStep;
    m_pBuildStep = 0;

    /* Generate new set-id: */
    m_strSetId = QUuid::createUuid().toString();

    /* Request to build first step: */
    emit sigBuildStep(m_strSetId, InformationElementType_General);
}

UIGInformationElement* UIGInformationSet::createElement(InformationElementType elementType, bool fOpen)
{
    /* Element factory: */
    switch (elementType)
    {
        case InformationElementType_General:     return new UIGInformationElementGeneral(this, fOpen);
        case InformationElementType_System:      return new UIGInformationElementSystem(this, fOpen);
        case InformationElementType_Preview:     return new UIGInformationElementPreview(this, fOpen);
        case InformationElementType_Display:     return new UIGInformationElementDisplay(this, fOpen);
        case InformationElementType_Storage:     return new UIGInformationElementStorage(this, fOpen);
        case InformationElementType_Audio:       return new UIGInformationElementAudio(this, fOpen);
        case InformationElementType_Network:     return new UIGInformationElementNetwork(this, fOpen);
        case InformationElementType_Serial:      return new UIGInformationElementSerial(this, fOpen);
#ifdef VBOX_WITH_PARALLEL_PORTS
        case InformationElementType_Parallel:    return new UIGInformationElementParallel(this, fOpen);
#endif /* VBOX_WITH_PARALLEL_PORTS */
        case InformationElementType_USB:         return new UIGInformationElementUSB(this, fOpen);
        case InformationElementType_SF:          return new UIGInformationElementSF(this, fOpen);
        case InformationElementType_UI:          return new UIGInformationElementUI(this, fOpen);
        case InformationElementType_Description: return new UIGInformationElementDescription(this, fOpen);
        case InformationElementType_RuntimeAttributes:         return new UIGInformationElementRuntimeAttributes(this, fOpen);
    }
    return 0;
}

