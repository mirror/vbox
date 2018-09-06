/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsSet class implementation.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
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
# include <QStyle>

/* GUI includes: */
# include "UIDetailsElements.h"
# include "UIDetailsModel.h"
# include "UIDetailsSet.h"
# include "UIVirtualBoxEventHandler.h"
# include "UIVirtualMachineItem.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CUSBController.h"
# include "CUSBDeviceFilters.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIDetailsSet::UIDetailsSet(UIDetailsItem *pParent)
    : UIDetailsItem(pParent)
    , m_pMachineItem(0)
    , m_fFullSet(true)
    , m_fHasDetails(false)
    , m_configurationAccessLevel(ConfigurationAccessLevel_Null)
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

UIDetailsSet::~UIDetailsSet()
{
    /* Cleanup items: */
    clearItems();

    /* Remove set from the parent group: */
    parentItem()->removeItem(this);
}

void UIDetailsSet::buildSet(UIVirtualMachineItem *pMachineItem, bool fFullSet, const QMap<DetailsElementType, bool> &settings)
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
        int iFirstItem = m_fHasDetails ? DetailsElementType_Display : DetailsElementType_General;
        int iLastItem = DetailsElementType_Description;
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
    m_iLastStepNumber = m_fFullSet ? DetailsElementType_Description : DetailsElementType_Preview;

    /* Fetch USB controller restrictions: */
    const CUSBDeviceFilters &filters = m_machine.GetUSBDeviceFilters();
    if (filters.isNull() || !m_machine.GetUSBProxyAvailable())
        m_settings.remove(DetailsElementType_USB);

    /* Start building set: */
    rebuildSet();
}

void UIDetailsSet::sltBuildStep(QString strStepId, int iStepNumber)
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
        DetailsElementType enmElementType = (DetailsElementType)iStepNumber;
        /* Should the element be visible? */
        bool fVisible = m_settings.contains(enmElementType);
        /* Should the element be opened? */
        bool fOpen = fVisible && m_settings[enmElementType];

        /* Check if element is present already: */
        UIDetailsElement *pElement = element(enmElementType);
        if (pElement && fOpen)
            pElement->open(false);
        /* Create element if necessary: */
        bool fJustCreated = false;
        if (!pElement)
        {
            fJustCreated = true;
            pElement = createElement(enmElementType, fOpen);
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
            m_pBuildStep = new UIPrepareStep(this, pElement, strStepId, iStepNumber + 1);

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
        foreach (UIDetailsItem *pItem, items())
            pItem->update();
        /* Notify listener about build done: */
        emit sigBuildDone();
    }
}

QString UIDetailsSet::description() const
{
    return tr("Contains the details of virtual machine '%1'").arg(m_pMachineItem->name());
}

void UIDetailsSet::addItem(UIDetailsItem *pItem)
{
    switch (pItem->type())
    {
        case UIDetailsItemType_Element:
        {
            UIDetailsElement *pElement = pItem->toElement();
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

void UIDetailsSet::removeItem(UIDetailsItem *pItem)
{
    switch (pItem->type())
    {
        case UIDetailsItemType_Element:
        {
            UIDetailsElement *pElement = pItem->toElement();
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

QList<UIDetailsItem*> UIDetailsSet::items(UIDetailsItemType enmType /* = UIDetailsItemType_Element */) const
{
    switch (enmType)
    {
        case UIDetailsItemType_Element: return m_elements.values();
        case UIDetailsItemType_Any: return items(UIDetailsItemType_Element);
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
    return QList<UIDetailsItem*>();
}

bool UIDetailsSet::hasItems(UIDetailsItemType enmType /* = UIDetailsItemType_Element */) const
{
    switch (enmType)
    {
        case UIDetailsItemType_Element: return !m_elements.isEmpty();
        case UIDetailsItemType_Any: return hasItems(UIDetailsItemType_Element);
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
    return false;
}

void UIDetailsSet::clearItems(UIDetailsItemType enmType /* = UIDetailsItemType_Element */)
{
    switch (enmType)
    {
        case UIDetailsItemType_Element:
        {
            foreach (int iKey, m_elements.keys())
                delete m_elements[iKey];
            AssertMsg(m_elements.isEmpty(), ("Set items cleanup failed!"));
            break;
        }
        case UIDetailsItemType_Any:
        {
            clearItems(UIDetailsItemType_Element);
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }
}

UIDetailsElement *UIDetailsSet::element(DetailsElementType enmElementType) const
{
    UIDetailsItem *pItem = m_elements.value(enmElementType, 0);
    if (pItem)
        return pItem->toElement();
    return 0;
}

void UIDetailsSet::updateLayout()
{
    /* Prepare variables: */
    const int iMargin = data(SetData_Margin).toInt();
    const int iSpacing = data(SetData_Spacing).toInt();
    const int iMaximumWidth = geometry().size().toSize().width();
    int iVerticalIndent = 0;

    /* Layout all the elements: */
    foreach (UIDetailsItem *pItem, items())
    {
        /* Skip hidden: */
        if (!pItem->isVisible())
            continue;

        /* For each particular element: */
        UIDetailsElement *pElement = pItem->toElement();
        switch (pElement->elementType())
        {
            case DetailsElementType_General:
            case DetailsElementType_System:
            case DetailsElementType_Display:
            case DetailsElementType_Storage:
            case DetailsElementType_Audio:
            case DetailsElementType_Network:
            case DetailsElementType_Serial:
            case DetailsElementType_USB:
            case DetailsElementType_SF:
            case DetailsElementType_UI:
            case DetailsElementType_Description:
            {
                /* Move element: */
                pElement->setPos(0, iVerticalIndent);
                /* Calculate required width: */
                int iWidth = iMaximumWidth;
                if (pElement->elementType() == DetailsElementType_General ||
                    pElement->elementType() == DetailsElementType_System)
                    if (UIDetailsElement *pPreviewElement = element(DetailsElementType_Preview))
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
                if (pElement->elementType() == DetailsElementType_System && !pElement->isAnimationRunning() && !pElement->isClosed())
                    if (UIDetailsElement *pPreviewElement = element(DetailsElementType_Preview))
                        if (pPreviewElement->isVisible())
                        {
                            int iSpareHeight = pPreviewElement->minimumHeightHint();
                            if (UIDetailsElement *pGeneralElement = element(DetailsElementType_General))
                                if (pGeneralElement->isVisible())
                                    iSpareHeight -= (iSpacing + pGeneralElement->minimumHeightHint());
                            iHeight = qMax(iHeight, iSpareHeight);
                        }
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
            case DetailsElementType_Preview:
            {
                /* Prepare variables: */
                int iWidth = pElement->minimumWidthHint();
                int iHeight = pElement->minimumHeightHint();
                /* Move element: */
                pElement->setPos(iMaximumWidth - iWidth, iMargin);
                /* Resize element: */
                pElement->resize(iWidth, iHeight);
                /* Layout element content: */
                pItem->updateLayout();
                /* Advance indent: */
                iVerticalIndent = qMax(iVerticalIndent, iHeight + iSpacing);
                break;
            }
            case DetailsElementType_Invalid: AssertFailed(); break; /* Shut up, MSC! */
        }
    }
}

int UIDetailsSet::minimumWidthHint() const
{
    /* Zero if has no details: */
    if (!hasDetails())
        return 0;

    /* Prepare variables: */
    const int iSpacing = data(SetData_Spacing).toInt();
    int iMinimumWidthHint = 0;

    /* Take into account all the elements: */
    foreach (UIDetailsItem *pItem, items())
    {
        /* Skip hidden: */
        if (!pItem->isVisible())
            continue;

        /* For each particular element: */
        UIDetailsElement *pElement = pItem->toElement();
        switch (pElement->elementType())
        {
            case DetailsElementType_General:
            case DetailsElementType_System:
            case DetailsElementType_Display:
            case DetailsElementType_Storage:
            case DetailsElementType_Audio:
            case DetailsElementType_Network:
            case DetailsElementType_Serial:
            case DetailsElementType_USB:
            case DetailsElementType_SF:
            case DetailsElementType_UI:
            case DetailsElementType_Description:
            {
                iMinimumWidthHint = qMax(iMinimumWidthHint, pItem->minimumWidthHint());
                break;
            }
            case DetailsElementType_Preview:
            {
                UIDetailsItem *pGeneralItem = element(DetailsElementType_General);
                UIDetailsItem *pSystemItem = element(DetailsElementType_System);
                int iGeneralElementWidth = pGeneralItem ? pGeneralItem->minimumWidthHint() : 0;
                int iSystemElementWidth = pSystemItem ? pSystemItem->minimumWidthHint() : 0;
                int iFirstColumnWidth = qMax(iGeneralElementWidth, iSystemElementWidth);
                iMinimumWidthHint = qMax(iMinimumWidthHint, iFirstColumnWidth + iSpacing + pItem->minimumWidthHint());
                break;
            }
            case DetailsElementType_Invalid: AssertFailed(); break; /* Shut up, MSC! */
        }
    }

    /* Return result: */
    return iMinimumWidthHint;
}

int UIDetailsSet::minimumHeightHint() const
{
    /* Zero if has no details: */
    if (!hasDetails())
        return 0;

    /* Prepare variables: */
    int iMargin = data(SetData_Margin).toInt();
    int iSpacing = data(SetData_Spacing).toInt();
    int iMinimumHeightHint = 0;

    /* Take into account all the elements: */
    foreach (UIDetailsItem *pItem, items())
    {
        /* Skip hidden: */
        if (!pItem->isVisible())
            continue;

        /* For each particular element: */
        UIDetailsElement *pElement = pItem->toElement();
        switch (pElement->elementType())
        {
            case DetailsElementType_General:
            case DetailsElementType_System:
            case DetailsElementType_Display:
            case DetailsElementType_Storage:
            case DetailsElementType_Audio:
            case DetailsElementType_Network:
            case DetailsElementType_Serial:
            case DetailsElementType_USB:
            case DetailsElementType_SF:
            case DetailsElementType_UI:
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
            case DetailsElementType_Invalid: AssertFailed(); break; /* Shut up, MSC! */
        }
    }

    /* Minus last spacing: */
    iMinimumHeightHint -= iSpacing;

    /* And two margins finally: */
    iMinimumHeightHint += 2 * iMargin;

    /* Return result: */
    return iMinimumHeightHint;
}

void UIDetailsSet::sltMachineStateChange(QString strId)
{
    /* Is this our VM changed? */
    if (m_machine.GetId() != strId)
        return;

    /* Update appearance: */
    rebuildSet();
}

void UIDetailsSet::sltMachineAttributesChange(QString strId)
{
    /* Is this our VM changed? */
    if (m_machine.GetId() != strId)
        return;

    /* Update appearance: */
    rebuildSet();
}

void UIDetailsSet::sltUpdateAppearance()
{
    /* Update appearance: */
    rebuildSet();
}

void UIDetailsSet::prepareSet()
{
    /* Setup size-policy: */
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}

void UIDetailsSet::prepareConnections()
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

QVariant UIDetailsSet::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case SetData_Margin: return 0;
        case SetData_Spacing: return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 6;
        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIDetailsSet::rebuildSet()
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
    emit sigBuildStep(m_strSetId, DetailsElementType_General);
}

UIDetailsElement *UIDetailsSet::createElement(DetailsElementType enmElementType, bool fOpen)
{
    /* Element factory: */
    switch (enmElementType)
    {
        case DetailsElementType_General:     return new UIDetailsElementGeneral(this, fOpen);
        case DetailsElementType_System:      return new UIDetailsElementSystem(this, fOpen);
        case DetailsElementType_Preview:     return new UIDetailsElementPreview(this, fOpen);
        case DetailsElementType_Display:     return new UIDetailsElementDisplay(this, fOpen);
        case DetailsElementType_Storage:     return new UIDetailsElementStorage(this, fOpen);
        case DetailsElementType_Audio:       return new UIDetailsElementAudio(this, fOpen);
        case DetailsElementType_Network:     return new UIDetailsElementNetwork(this, fOpen);
        case DetailsElementType_Serial:      return new UIDetailsElementSerial(this, fOpen);
        case DetailsElementType_USB:         return new UIDetailsElementUSB(this, fOpen);
        case DetailsElementType_SF:          return new UIDetailsElementSF(this, fOpen);
        case DetailsElementType_UI:          return new UIDetailsElementUI(this, fOpen);
        case DetailsElementType_Description: return new UIDetailsElementDescription(this, fOpen);
        case DetailsElementType_Invalid:     AssertFailed(); break; /* Shut up, MSC! */
    }
    return 0;
}
