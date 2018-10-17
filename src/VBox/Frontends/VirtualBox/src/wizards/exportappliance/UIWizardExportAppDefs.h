/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppDefs class declaration.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIWizardExportAppDefs_h___
#define ___UIWizardExportAppDefs_h___

/* Qt includes: */
#include <QMetaType>
#include <QPointer>
#include <QListWidget>

/* GUI includes: */
#include "UIApplianceExportEditorWidget.h"

/* Typedefs: */
typedef QPointer<UIApplianceExportEditorWidget> ExportAppliancePointer;
Q_DECLARE_METATYPE(ExportAppliancePointer);

/** QListWidgetItem subclass for Export Appliance wizard VM list. */
class UIVMListWidgetItem : public QListWidgetItem
{
public:

    /** Constructs VM list item passing @a pixIcon, @a strText and @a pParent to the base-class.
      * @param  strUuid       Brings the machine ID.
      * @param  fInSaveState  Brings whether machine is in Saved state. */
    UIVMListWidgetItem(QPixmap &pixIcon, QString &strText, QUuid aUuid, bool fInSaveState, QListWidget *pParent)
        : QListWidgetItem(pixIcon, strText, pParent)
        , m_uUuid(aUuid)
        , m_fInSaveState(fInSaveState)
    {}

    /** Returns whether this item is less than @a other. */
    bool operator<(const QListWidgetItem &other) const
    {
        return text().toLower() < other.text().toLower();
    }

    /** Returns the machine ID. */
    QUuid uuid() const { return m_uUuid; }
    /** Returns whether machine is in Saved state. */
    bool isInSaveState() const { return m_fInSaveState; }

private:

    /** Holds the machine ID. */
    QUuid    m_uUuid;
    /** Holds whether machine is in Saved state. */
    bool     m_fInSaveState;
};

#endif /* !___UIWizardExportAppDefs_h___ */
