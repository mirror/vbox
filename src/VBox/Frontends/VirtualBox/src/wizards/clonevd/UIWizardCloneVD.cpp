/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVD class implementation
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
#include "UIWizardCloneVD.h"
#include "UIWizardCloneVDPageBasic1.h"
#include "UIWizardCloneVDPageBasic2.h"
#include "UIWizardCloneVDPageBasic3.h"
#include "UIWizardCloneVDPageBasic4.h"
#include "UIWizardCloneVDPageBasic5.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

UIWizardCloneVD::UIWizardCloneVD(QWidget *pParent, const CMedium &sourceVirtualDisk)
    : UIWizard(pParent)
{
#ifdef Q_WS_WIN
    /* Hide window icon: */
    setWindowIcon(QIcon());
#endif /* Q_WS_WIN */

    /* Create & add pages: */
    setPage(Page1, new UIWizardCloneVDPageBasic1(sourceVirtualDisk));
    setPage(Page2, new UIWizardCloneVDPageBasic2);
    setPage(Page3, new UIWizardCloneVDPageBasic3);
    setPage(Page4, new UIWizardCloneVDPageBasic4);
    setPage(Page5, new UIWizardCloneVDPageBasic5);

    /* Translate wizard: */
    retranslateUi();

    /* Translate wizard pages: */
    retranslateAllPages();

#ifndef Q_WS_MAC
    /* Assign watermark: */
    assignWatermark(":/vmw_new_harddisk.png");
#else /* Q_WS_MAC */
    /* Assign background image: */
    assignBackground(":/vmw_new_harddisk_bg.png");
#endif /* Q_WS_MAC */

    /* Resize wizard to 'golden ratio': */
    resizeToGoldenRatio(UIWizardType_CloneVD);
}

/* static */
QString UIWizardCloneVD::fullFormatName(const QString &strBaseFormatName)
{
    if (strBaseFormatName == "VDI")
        return QApplication::translate("UIWizardCloneVD", "&VDI (VirtualBox Disk Image)");
    else if (strBaseFormatName == "VMDK")
        return QApplication::translate("UIWizardCloneVD", "V&MDK (Virtual Machine Disk)");
    else if (strBaseFormatName == "VHD")
        return QApplication::translate("UIWizardCloneVD", "V&HD (Virtual Hard Disk)");
    else if (strBaseFormatName == "Parallels")
        return QApplication::translate("UIWizardCloneVD", "H&DD (Parallels Hard Disk)");
    else if (strBaseFormatName == "QED")
        return QApplication::translate("UIWizardCloneVD", "Q&ED (QEMU enhanced disk)");
    else if (strBaseFormatName == "QCOW")
        return QApplication::translate("UIWizardCloneVD", "&QCOW (QEMU Copy-On-Write)");
    return strBaseFormatName;
}

bool UIWizardCloneVD::copyVirtualDisk()
{
    /* Gather attributes: */
    CMedium sourceVirtualDisk = field("sourceVirtualDisk").value<CMedium>();
    CMediumFormat mediumFormat = field("mediumFormat").value<CMediumFormat>();
    qulonglong uVariant = field("mediumVariant").toULongLong();
    QString strMediumPath = field("mediumPath").toString();
    qulonglong uSize = field("mediumSize").toULongLong();
    /* Check attributes: */
    AssertReturn(!strMediumPath.isNull(), false);
    AssertReturn(uSize > 0, false);

    /* Get VBox object: */
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* Create new virtual-disk: */
    CMedium virtualDisk = vbox.CreateHardDisk(mediumFormat.GetName(), strMediumPath);
    CProgress progress;
    if (!vbox.isOk())
    {
        msgCenter().cannotCreateHardDiskStorage(this, vbox, strMediumPath, virtualDisk, progress);
        return false;
    }

    /* Copy existing virtual-disk to the new virtual-disk: */
    progress = sourceVirtualDisk.CloneTo(virtualDisk, uVariant, CMedium());
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

    /* Just close the created medium, it is not necessary yet: */
    m_virtualDisk.Close();

    return true;
}

void UIWizardCloneVD::retranslateUi()
{
    /* Translate wizard: */
    setWindowTitle(tr("Copy Virtual Disk"));
    setButtonText(QWizard::FinishButton, tr("Copy"));
}

