/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardFirstRun class implementation.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
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
# include "UIWizardFirstRun.h"
# include "UIWizardFirstRunPageBasic.h"
# include "VBoxGlobal.h"
# include "UIMessageCenter.h"
# include "UIMedium.h"

/* COM includes: */
# include "CStorageController.h"
# include "CMediumAttachment.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIWizardFirstRun::UIWizardFirstRun(QWidget *pParent, const CMachine &machine)
    : UIWizard(pParent, WizardType_FirstRun)
    , m_machine(machine)
    , m_fHardDiskWasSet(isBootHardDiskAttached(m_machine))
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    assignWatermark(":/vmw_first_run.png");
#else /* VBOX_WS_MAC */
    /* Assign background image: */
    assignBackground(":/vmw_first_run_bg.png");
#endif /* VBOX_WS_MAC */
}

bool UIWizardFirstRun::insertMedium()
{
    /* Get 'vbox' global object: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    /* Determine machine 'OS type': */
    const CGuestOSType &osType = vbox.GetGuestOSType(m_machine.GetOSTypeId());
    /* Determine recommended controller's 'bus' & 'type': */
    KStorageBus dvdCtrBus = osType.GetRecommendedDVDStorageBus();
    KStorageControllerType dvdCtrType = osType.GetRecommendedDVDStorageController();
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
    UIMedium vmedium = vboxGlobal().medium(mediumId);
    CMedium medium = vmedium.medium(); /// @todo r=dj can this be cached somewhere?
    /* Mount medium to the predefined port/device: */
    m_machine.MountMedium(cda.GetController(), cda.GetPort(), cda.GetDevice(), medium, false /* force */);
    if (m_machine.isOk())
        return true;
    else
    {
        msgCenter().cannotRemountMedium(m_machine, vmedium, true /* mount? */, false /* retry? */, this);
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
        case WizardMode_Basic:
        {
            setPage(Page, new UIWizardFirstRunPageBasic(m_machine.GetId(), m_fHardDiskWasSet));
            break;
        }
        case WizardMode_Expert:
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
    KStorageBus hdCtrBus = osType.GetRecommendedHDStorageBus();
    KStorageControllerType hdCtrType = osType.GetRecommendedHDStorageController();
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

