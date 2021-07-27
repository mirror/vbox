/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVD class implementation.
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
#include <QVariant>
#include <QPushButton>

/* GUI includes: */
#include "UICommon.h"
#include "UIMedium.h"
#include "UIMessageCenter.h"
#include "UINotificationCenter.h"
#include "UIWizardNewVD.h"
#include "UIWizardNewVDPageFileType.h"
#include "UIWizardNewVDPageVariant.h"
#include "UIWizardNewVDPageSizeLocation.h"
#include "UIWizardNewVDPageExpert.h"

/* COM includes: */
#include "CMediumFormat.h"


UIWizardNewVD::UIWizardNewVD(QWidget *pParent,
                             const QString &strDefaultName,
                             const QString &strDefaultPath,
                             qulonglong uDefaultSize,
                             WizardMode mode)
    : UINativeWizard(pParent, WizardType_NewVD, mode)
    , m_strDefaultName(strDefaultName)
    , m_strDefaultPath(strDefaultPath)
    , m_uDefaultSize(uDefaultSize)
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    setPixmapName(":/wizard_new_harddisk.png");
#else /* VBOX_WS_MAC */
    /* Assign background image: */
    setPixmapName(":/wizard_new_harddisk_bg.png");
#endif /* VBOX_WS_MAC */
}

void UIWizardNewVD::populatePages()
{
    switch (mode())
    {
        case WizardMode_Basic:
        case WizardMode_Expert:
        {
            addPage(new UIWizardNewVDPageFileType);
            addPage(new UIWizardNewVDPageVariant);
            addPage(new UIWizardNewVDPageSizeLocation(m_strDefaultName, m_strDefaultPath, m_uDefaultSize));
            break;
        }

        // {
        //     //addPage(new UIWizardNewVMPageExpert);
        //     break;
        // }
        default:
        {
            AssertMsgFailed(("Invalid mode: %d", mode()));
            break;
        }
    }
}

bool UIWizardNewVD::createVirtualDisk()
{
    /* Gather attributes: */
    // const CMediumFormat comMediumFormat = field("mediumFormat").value<CMediumFormat>();
    // const qulonglong uVariant = field("mediumVariant").toULongLong();
    // const QString strMediumPath = field("mediumPath").toString();
    // const qulonglong uSize = field("mediumSize").toULongLong();
    // /* Check attributes: */
    // AssertReturn(!strMediumPath.isNull(), false);
    // AssertReturn(uSize > 0, false);

    // /* Get VBox object: */
    // CVirtualBox comVBox = uiCommon().virtualBox();

    // /* Create new virtual disk image: */
    // CMedium comVirtualDisk = comVBox.CreateMedium(comMediumFormat.GetName(), strMediumPath, KAccessMode_ReadWrite, KDeviceType_HardDisk);
    // if (!comVBox.isOk())
    // {
    //     msgCenter().cannotCreateMediumStorage(comVBox, strMediumPath, this);
    //     return false;
    // }

    // /* Compose medium-variant: */
    // QVector<KMediumVariant> variants(sizeof(qulonglong) * 8);
    // for (int i = 0; i < variants.size(); ++i)
    // {
    //     qulonglong temp = uVariant;
    //     temp &= Q_UINT64_C(1) << i;
    //     variants[i] = (KMediumVariant)temp;
    // }

    // /* Copy medium: */
    // UINotificationProgressMediumCreate *pNotification = new UINotificationProgressMediumCreate(comVirtualDisk,
    //                                                                                            uSize,
    //                                                                                            variants);
    // connect(pNotification, &UINotificationProgressMediumCreate::sigMediumCreated,
    //         &uiCommon(), &UICommon::sltHandleMediumCreated);
    // notificationCenter().append(pNotification);

    /* Positive: */
    return true;
}

void UIWizardNewVD::retranslateUi()
{
    UINativeWizard::retranslateUi();
    setWindowTitle(tr("Create Virtual Hard Disk"));
}

void UIWizardNewVD::prepare()
{
    // /* Create corresponding pages: */
    // switch (mode())
    // {
    //     case WizardMode_Basic:
    //     {
    //         setPage(Page1, new UIWizardNewVDPageFileType);
    //         setPage(Page2, new UIWizardNewVDPageVariant);
    //         setPage(Page3, new UIWizardNewVDPageSizeLocation(m_strDefaultName, m_strDefaultPath, m_uDefaultSize));
    //         break;
    //     }
    //     case WizardMode_Expert:
    //     {
    //         setPage(PageExpert, new UIWizardNewVDPageExpert(m_strDefaultName, m_strDefaultPath, m_uDefaultSize));
    //         break;
    //     }
    //     default:
    //     {
    //         AssertMsgFailed(("Invalid mode: %d", mode()));
    //         break;
    //     }
    // }

}
