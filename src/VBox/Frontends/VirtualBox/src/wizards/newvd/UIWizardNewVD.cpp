/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVD class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes: */
#include <QVariant>

/* Local includes: */
#include "UIWizardNewVD.h"
#include "UIWizardNewVDPageBasic1.h"
#include "UIWizardNewVDPageBasic2.h"
#include "UIWizardNewVDPageBasic3.h"
#include "UIWizardNewVDPageBasic4.h"
#include "UIWizardNewVDPageExpert.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

UIWizardNewVD::UIWizardNewVD(QWidget *pParent, const QString &strDefaultName, const QString &strDefaultPath, qulonglong uDefaultSize)
    : UIWizard(pParent, UIWizardType_NewVD)
    , m_strDefaultName(strDefaultName)
    , m_strDefaultPath(strDefaultPath)
    , m_uDefaultSize(uDefaultSize)
{
#ifndef Q_WS_MAC
    /* Assign watermark: */
    assignWatermark(":/vmw_new_harddisk.png");
#else /* Q_WS_MAC */
    /* Assign background image: */
    assignBackground(":/vmw_new_harddisk_bg.png");
#endif /* Q_WS_MAC */
}

bool UIWizardNewVD::createVirtualDisk()
{
    /* Gather attributes: */
    CMediumFormat mediumFormat = field("mediumFormat").value<CMediumFormat>();
    qulonglong uVariant = field("mediumVariant").toULongLong();
    QString strMediumPath = field("mediumPath").toString();
    qulonglong uSize = field("mediumSize").toULongLong();
    /* Check attributes: */
    AssertReturn(!strMediumPath.isNull(), false);
    AssertReturn(uSize > 0, false);

    /* Get vbox object: */
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* Create new virtual disk: */
    CMedium virtualDisk = vbox.CreateHardDisk(mediumFormat.GetName(), strMediumPath);
    CProgress progress;
    if (!vbox.isOk())
    {
        msgCenter().cannotCreateHardDiskStorage(this, vbox, strMediumPath, virtualDisk, progress);
        return false;
    }

    /* Create base storage for the new hard disk: */
    progress = virtualDisk.CreateBaseStorage(uSize, uVariant);
    if (!virtualDisk.isOk())
    {
        msgCenter().cannotCreateHardDiskStorage(this, vbox, strMediumPath, virtualDisk, progress);
        return false;
    }

    /* Show creation progress: */
    msgCenter().showModalProgressDialog(progress, windowTitle(), ":/progress_media_create_90px.png", this, true);
    if (progress.GetCanceled())
        return false;
    if (!progress.isOk() || progress.GetResultCode() != 0)
    {
        msgCenter().cannotCreateHardDiskStorage(this, vbox, strMediumPath, virtualDisk, progress);
        return false;
    }

    /* Remember created virtual-disk: */
    m_virtualDisk = virtualDisk;

    /* Inform everybody there is a new medium: */
    vboxGlobal().addMedium(VBoxMedium(m_virtualDisk, VBoxDefs::MediumType_HardDisk, KMediumState_Created));

    return true;
}

void UIWizardNewVD::retranslateUi()
{
    /* Call to base-class: */
    UIWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Create New Virtual Disk"));
    setButtonText(QWizard::FinishButton, tr("Create"));
}

void UIWizardNewVD::prepare()
{
    /* Create corresponding pages: */
    switch (mode())
    {
        case UIWizardMode_Basic:
        {
            setPage(Page1, new UIWizardNewVDPageBasic1);
            setPage(Page2, new UIWizardNewVDPageBasic2);
            setPage(Page3, new UIWizardNewVDPageBasic3(m_strDefaultName, m_strDefaultPath, m_uDefaultSize));
            setPage(Page4, new UIWizardNewVDPageBasic4);
            break;
        }
        case UIWizardMode_Expert:
        {
            setPage(PageExpert, new UIWizardNewVDPageExpert(m_strDefaultName, m_strDefaultPath, m_uDefaultSize));
            break;
        }
    }
    /* Call to base-class: */
    UIWizard::prepare();
}

