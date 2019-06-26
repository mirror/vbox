/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsElement[Name] classes implementation.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
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
#include <QDir>
#include <QTimer>
#include <QGraphicsLinearLayout>

/* GUI includes: */
#include "UIDetailsElements.h"
#include "UIDetailsGenerator.h"
#include "UIDetailsModel.h"
#include "UIMachinePreview.h"
#include "UIGraphicsRotatorButton.h"
#include "UICommon.h"
#include "UIIconPool.h"
#include "UIConverter.h"
#include "UIGraphicsTextPane.h"
#include "UIErrorString.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"
#include "CSystemProperties.h"
#include "CVRDEServer.h"
#include "CStorageController.h"
#include "CMediumAttachment.h"
#include "CAudioAdapter.h"
#include "CRecordingSettings.h"
#include "CRecordingScreenSettings.h"
#include "CNetworkAdapter.h"
#include "CSerialPort.h"
#include "CUSBController.h"
#include "CUSBDeviceFilters.h"
#include "CUSBDeviceFilter.h"
#include "CSharedFolder.h"
#include "CMedium.h"

UIDetailsUpdateTask::UIDetailsUpdateTask(const CMachine &machine)
    : UITask(UITask::Type_DetailsPopulation)
{
    /* Store machine as property: */
    setProperty("machine", QVariant::fromValue(machine));
}

UIDetailsElementInterface::UIDetailsElementInterface(UIDetailsSet *pParent, DetailsElementType type, bool fOpened)
    : UIDetailsElement(pParent, type, fOpened)
    , m_pTask(0)
{
    /* Listen for the global thread-pool: */
    connect(uiCommon().threadPool(), SIGNAL(sigTaskComplete(UITask*)),
            this, SLOT(sltUpdateAppearanceFinished(UITask*)));

    /* Translate finally: */
    retranslateUi();
}

void UIDetailsElementInterface::retranslateUi()
{
    /* Assign corresponding name: */
    setName(gpConverter->toString(elementType()));
}

void UIDetailsElementInterface::updateAppearance()
{
    /* Call to base-class: */
    UIDetailsElement::updateAppearance();

    /* Prepare/start update task: */
    if (!m_pTask)
    {
        /* Prepare update task: */
        m_pTask = createUpdateTask();
        /* Post task into global thread-pool: */
        uiCommon().threadPool()->enqueueTask(m_pTask);
    }
}

void UIDetailsElementInterface::sltUpdateAppearanceFinished(UITask *pTask)
{
    /* Make sure that is one of our tasks: */
    if (pTask->type() != UITask::Type_DetailsPopulation)
        return;

    /* Skip unrelated tasks: */
    if (m_pTask != pTask)
        return;

    /* Assign new text if changed: */
    const UITextTable newText = pTask->property("table").value<UITextTable>();
    if (text() != newText)
        setText(newText);

    /* Mark task processed: */
    m_pTask = 0;

    /* Notify listeners about update task complete: */
    emit sigBuildDone();
}


UIDetailsElementPreview::UIDetailsElementPreview(UIDetailsSet *pParent, bool fOpened)
    : UIDetailsElement(pParent, DetailsElementType_Preview, fOpened)
{
    /* Create preview: */
    m_pPreview = new UIMachinePreview(this);
    AssertPtr(m_pPreview);
    {
        /* Configure preview: */
        connect(m_pPreview, SIGNAL(sigSizeHintChanged()),
                this, SLOT(sltPreviewSizeHintChanged()));
    }

    /* Translate finally: */
    retranslateUi();
}

void UIDetailsElementPreview::sltPreviewSizeHintChanged()
{
    /* Recursively update size-hints: */
    updateGeometry();
    /* Update whole model layout: */
    model()->updateLayout();
}

void UIDetailsElementPreview::retranslateUi()
{
    /* Assign corresponding name: */
    setName(gpConverter->toString(elementType()));
}

int UIDetailsElementPreview::minimumWidthHint() const
{
    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();

    /* Calculating proposed width: */
    int iProposedWidth = 0;

    /* Maximum between header width and preview width: */
    iProposedWidth += qMax(minimumHeaderWidth(), m_pPreview->minimumSizeHint().toSize().width());

    /* Two margins: */
    iProposedWidth += 2 * iMargin;

    /* Return result: */
    return iProposedWidth;
}

int UIDetailsElementPreview::minimumHeightHintForElement(bool fClosed) const
{
    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();

    /* Calculating proposed height: */
    int iProposedHeight = 0;

    /* Two margins: */
    iProposedHeight += 2 * iMargin;

    /* Header height: */
    iProposedHeight += minimumHeaderHeight();

    /* Element is opened? */
    if (!fClosed)
    {
        iProposedHeight += iMargin;
        iProposedHeight += m_pPreview->minimumSizeHint().toSize().height();
    }
    else
    {
        /* Additional height during animation: */
        if (button()->isAnimationRunning())
            iProposedHeight += additionalHeight();
    }

    /* Return result: */
    return iProposedHeight;
}

void UIDetailsElementPreview::updateLayout()
{
    /* Call to base-class: */
    UIDetailsElement::updateLayout();

    /* Show/hide preview: */
    if (isClosed() && m_pPreview->isVisible())
        m_pPreview->hide();
    if (isOpened() && !m_pPreview->isVisible() && !isAnimationRunning())
        m_pPreview->show();

    /* And update preview layout itself: */
    const int iMargin = data(ElementData_Margin).toInt();
    m_pPreview->setPos(iMargin, 2 * iMargin + minimumHeaderHeight());
    m_pPreview->resize(m_pPreview->minimumSizeHint());
}

void UIDetailsElementPreview::updateAppearance()
{
    /* Call to base-class: */
    UIDetailsElement::updateAppearance();

    /* Set new machine attribute directly: */
    m_pPreview->setMachine(machine());
    m_pPreview->resize(m_pPreview->minimumSizeHint());
    emit sigBuildDone();
}


void UIDetailsUpdateTaskGeneral::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Generate details table: */
    UITextTable table = UIDetailsGenerator::generateMachineInformationGeneral(comMachine, m_fOptions);
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementGeneral::createUpdateTask()
{
    return new UIDetailsUpdateTaskGeneral(machine(), model()->optionsGeneral());
}


void UIDetailsUpdateTaskSystem::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Generate details table: */
    UITextTable table = UIDetailsGenerator::generateMachineInformationSystem(comMachine, m_fOptions);
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementSystem::createUpdateTask()
{
    return new UIDetailsUpdateTaskSystem(machine(), model()->optionsSystem());
}


void UIDetailsUpdateTaskDisplay::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Generate details table: */
    UITextTable table = UIDetailsGenerator::generateMachineInformationDisplay(comMachine, m_fOptions);
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementDisplay::createUpdateTask()
{
    return new UIDetailsUpdateTaskDisplay(machine(), model()->optionsDisplay());
}


void UIDetailsUpdateTaskStorage::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Generate details table: */
    UITextTable table = UIDetailsGenerator::generateMachineInformationStorage(comMachine, m_fOptions);
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementStorage::createUpdateTask()
{
    return new UIDetailsUpdateTaskStorage(machine(), model()->optionsStorage());
}


void UIDetailsUpdateTaskAudio::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Generate details table: */
    UITextTable table = UIDetailsGenerator::generateMachineInformationAudio(comMachine, m_fOptions);
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementAudio::createUpdateTask()
{
    return new UIDetailsUpdateTaskAudio(machine(), model()->optionsAudio());
}

void UIDetailsUpdateTaskNetwork::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Generate details table: */
    UITextTable table = UIDetailsGenerator::generateMachineInformationNetwork(comMachine, m_fOptions);
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementNetwork::createUpdateTask()
{
    return new UIDetailsUpdateTaskNetwork(machine(), model()->optionsNetwork());
}

void UIDetailsUpdateTaskSerial::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Generate details table: */
    UITextTable table = UIDetailsGenerator::generateMachineInformationSerial(comMachine, m_fOptions);
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementSerial::createUpdateTask()
{
    return new UIDetailsUpdateTaskSerial(machine(), model()->optionsSerial());
}

void UIDetailsUpdateTaskUSB::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Generate details table: */
    UITextTable table = UIDetailsGenerator::generateMachineInformationUSB(comMachine, m_fOptions);
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementUSB::createUpdateTask()
{
    return new UIDetailsUpdateTaskUSB(machine(), model()->optionsUsb());
}


void UIDetailsUpdateTaskSF::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Generate details table: */
    UITextTable table = UIDetailsGenerator::generateMachineInformationSharedFolders(comMachine, m_fOptions);
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementSF::createUpdateTask()
{
    return new UIDetailsUpdateTaskSF(machine(), model()->optionsSharedFolders());
}


void UIDetailsUpdateTaskUI::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Generate details table: */
    UITextTable table = UIDetailsGenerator::generateMachineInformationUI(comMachine, m_fOptions);
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementUI::createUpdateTask()
{
    return new UIDetailsUpdateTaskUI(machine(), model()->optionsUserInterface());
}


void UIDetailsUpdateTaskDescription::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Generate details table: */
    UITextTable table = UIDetailsGenerator::generateMachineInformationDetails(comMachine, m_fOptions);
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementDescription::createUpdateTask()
{
    return new UIDetailsUpdateTaskDescription(machine(), model()->optionsDescription());
}
