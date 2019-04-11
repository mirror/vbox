/* $Id$ */
/** @file
 * VBox Qt GUI - UIApplianceUnverifiedCertificateViewer class declaration.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_importappliance_UIApplianceUnverifiedCertificateViewer_h
#define FEQT_INCLUDED_SRC_wizards_importappliance_UIApplianceUnverifiedCertificateViewer_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QLabel;
class QTextBrowser;
class CCertificate;

/** QIDialog extension
  * asking for consent to continue with unverifiable certificate. */
class UIApplianceUnverifiedCertificateViewer : public QIWithRetranslateUI<QIDialog>
{
    Q_OBJECT;

public:

    /** Constructs appliance @a comCertificate viewer for passed @a pParent. */
    UIApplianceUnverifiedCertificateViewer(QWidget *pParent, const CCertificate &comCertificate);

protected:

    /** Prepares all. */
    void prepare();

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private:

    /** Holds the certificate reference. */
    const CCertificate &m_comCertificate;

    /** Holds the text-label instance. */
    QLabel       *m_pTextLabel;
    /** Holds the text-browser instance. */
    QTextBrowser *m_pTextBrowser;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_importappliance_UIApplianceUnverifiedCertificateViewer_h */
