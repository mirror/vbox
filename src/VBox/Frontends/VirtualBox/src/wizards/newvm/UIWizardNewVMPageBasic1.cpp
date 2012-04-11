/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVMPageBasic1 class implementation
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
#include <QVBoxLayout>

/* Local includes: */
#include "UIWizardNewVMPageBasic1.h"
#include "UIWizardNewVM.h"
#include "UIIconPool.h"
#include "QIRichTextLabel.h"

UIWizardNewVMPageBasic1::UIWizardNewVMPageBasic1()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        m_pLabel1 = new QIRichTextLabel(this);
            /* Register 'message-box warning-icon' image in m_pLabel1 as 'image': */
            QSize wSize(64, 64);
            QPixmap wPixmap = UIIconPool::defaultIcon(UIIconPool::MessageBoxWarningIcon).pixmap(wSize);
            if (wPixmap.width() != wSize.width())
                wPixmap = wPixmap.scaled(wSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QImage wImage = wPixmap.toImage();
            m_pLabel1->registerImage(wImage, "image");
        m_pLabel2 = new QIRichTextLabel(this);
    pMainLayout->addWidget(m_pLabel1);
    pMainLayout->addWidget(m_pLabel2);
    pMainLayout->addStretch();
}

void UIWizardNewVMPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVM::tr("Welcome to the New Virtual Machine Wizard!"));

    /* Translate widgets: */
    m_pLabel1->setText(tableTemplate().arg("<b>STOP!</b> Before continuing, please understand what you are doing. "
                                           "Computer system virtualization involves complex concepts, "
                                           "which no user interface can hide. Please make sure that you have read "
                                           "the first section of the user manual before continuing "
                                           "or seeking out online assistance!"));
    m_pLabel2->setText(UIWizardNewVM::tr("<p>This wizard will guide you through the steps that are necessary "
                                         "to create a new virtual machine for VirtualBox.</p><p>%1</p>").arg(standardHelpText()));
}

void UIWizardNewVMPageBasic1::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

/* static */
QString UIWizardNewVMPageBasic1::tableTemplate()
{
    /* Prepare table template: */
    QString strTable("<table cellspacing=0 cellpadding=0>%1</table>");
    QString strTableRow("<tr>%1</tr>");
    QString strTableData("<td>%1</td>");
    QString strTableDataCentered("<td valign=middle>%1</td>");
    QString strImage("<img src=\"image\"/>");
    QString strSpacing("&nbsp;&nbsp;&nbsp;");
    QString strTableDataImage(strTableDataCentered.arg(strImage));
    QString strTableDataSpacing(strTableData.arg(strSpacing));
    return strTable.arg(strTableRow.arg(strTableDataImage + strTableDataSpacing + strTableData));
}

