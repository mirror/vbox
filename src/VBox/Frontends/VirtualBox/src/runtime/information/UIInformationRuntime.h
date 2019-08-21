/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationRuntime class declaration.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_runtime_information_UIInformationRuntime_h
#define FEQT_INCLUDED_SRC_runtime_information_UIInformationRuntime_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"
#include "CConsole.h"

/* GUI includes: */
#include "UIInformationWidget.h"

/* Forward declarations: */
class QVBoxLayout;
class UIInformationView;
class UIInformationModel;
class UIVMItem;


class UIInformationRuntime : public UIInformationWidget
{
    Q_OBJECT;

public:

    UIInformationRuntime(QWidget *pParent, const CMachine &machine, const CConsole &console);

protected:

    void retranslateUi() /* override */;
    void createTableItems() /* override */;

private:
    UITextTable runTimeAttributes();

    /** @name Cached translated string.
     * @{ */
       QString m_strRuntimeTitle;
    /** @} */

};

#endif /* !FEQT_INCLUDED_SRC_runtime_information_UIInformationRuntime_h */
