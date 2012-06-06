/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardFirstRun class implementation
 */

/*
 * Copyright (C) 2008-2012 Oracle Corporation
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
#include "UIWizardFirstRun.h"
#include "UIWizardFirstRunPageBasic.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CStorageController.h"
#include "CMediumAttachment.h"

UIWizardFirstRun::UIWizardFirstRun(QWidget *pParent, const CMachine &machine)
    : UIWizard(pParent, UIWizardType_FirstRun)
    , m_machine(machine)
    , m_fHardDiskWasSet(isBootHardDiskAttached(m_machine))
{
#ifndef Q_WS_MAC
    /* Assign watermark: */
    assignWatermark(":/vmw_first_run.png");
#else /* Q_WS_MAC */
    /* Assign background image: */
    assignBackground(":/vmw_first_run_bg.png");
#endif /* Q_WS_MAC */
}

bool UIWizardFirstRun::insertMedium()
{
    /* Get 'vbox' global object: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    /* Determine machine 'OS type': */
    const CGuestOSType &osType = vbox.GetGuestOSType(m_machine.GetOSTypeId());
    /* Determine recommended controller's 'bus' & 'type': */
    KStorageBus dvdCtrBus = osType.GetRecommendedDvdStorageBus();
    KStorageControllerType dvdCtrType = osType.GetRecommendedDvdStorageController();
    /* Declare null 'dvd' attachment: */
    CMediumAttachment cda;
    /* Enumerate attachments vector: */
    const CMediumAttachmentVector &attachments = m_machine.GetMediumAttachments();
    for (int i = 0; i < attachments.size(); ++i)
    {
        /* Get current attachment: */
        const CMediumAttachment &attachment = attachments[i];
        /* Determine attachment's controller: */
        const CStorageController &controller = m_machine.GetStorageControllerByName(attachment.GetController());
        /* If controller's 'bus' & 'type' are recommended and attachment's 'type' is 'dvd': */
        if (controller.GetBus() == dvdCtrBus &&
            controller.GetControllerType() == dvdCtrType &&
            attachment.GetType() == KDeviceType_DVD)
        {
            /* Remember attachment: */
            cda = attachment;
            break;
        }
    }
    AssertMsg(!cda.isNull(), ("Storage Controller is NOT properly configured!\n"));
    /* Get chosen 'dvd' medium to mount: */
    QString mediumId = field("id").toString();
    VBoxMedium vmedium = vboxGlobal().findMedium(mediumId);
    CMedium medium = vmedium.medium(); // @todo r=dj can this be cached somewhere?
    /* Mount medium to the predefined port/device: */
    m_machine.MountMedium(cda.GetController(), cda.GetPort(), cda.GetDevice(), medium, false /* force */);
    if (m_machine.isOk())
        return true;
    else
    {
        msgCenter().cannotRemountMedium(this, m_machine, vmedium, true /* mount? */, false /* retry? */);
        return false;
    }
}

void UIWizardFirstRun::retranslateUi()
{
    /* Call to base-class: */
    UIWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Select start-up disk"));
    setButtonText(QWizard::FinishButton, tr("Start"));
}

void UIWizardFirstRun::prepare()
{
    /* Create corresponding pages: */
    switch (mode())
    {
        case UIWizardMode_Basic:
        {
            setPage(Page, new UIWizardFirstRunPageBasic(m_machine.GetId(), m_fHardDiskWasSet));
            break;
        }
        case UIWizardMode_Expert:
        {
            AssertMsgFailed(("First-run wizard has no expert-mode!"));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid mode: %d", mode()));
            break;
        }
    }
    /* Call to base-class: */
    UIWizard::prepare();
}

/* static */
bool UIWizardFirstRun::isBootHardDiskAttached(const CMachine &machine)
{
    /* Result is 'false' initially: */
    bool fIsBootHardDiskAttached = false;
    /* Get 'vbox' global object: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    /* Determine machine 'OS type': */
    const CGuestOSType &osType = vbox.GetGuestOSType(machine.GetOSTypeId());
    /* Determine recommended controller's 'bus' & 'type': */
    KStorageBus hdCtrBus = osType.GetRecommendedHdStorageBus();
    KStorageControllerType hdCtrType = osType.GetRecommendedHdStorageController();
    /* Enumerate attachments vector: */
    const CMediumAttachmentVector &attachments = machine.GetMediumAttachments();
    for (int i = 0; i < attachments.size(); ++i)
    {
        /* Get current attachment: */
        const CMediumAttachment &attachment = attachments[i];
        /* Determine attachment's controller: */
        const CStorageController &controller = machine.GetStorageControllerByName(attachment.GetController());
        /* If controller's 'bus' & 'type' are recommended and attachment's 'type' is 'hard disk': */
        if (controller.GetBus() == hdCtrBus &&
            controller.GetControllerType() == hdCtrType &&
            attachment.GetType() == KDeviceType_HardDisk)
        {
            /* Set the result to 'true': */
            fIsBootHardDiskAttached = true;
            break;
        }
    }
    /* Return result: */
    return fIsBootHardDiskAttached;
}

