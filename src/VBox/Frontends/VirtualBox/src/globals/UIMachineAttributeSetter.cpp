/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineAttributeSetter namespace implementation.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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

/* GUI includes: */
#include "UICommon.h"
#include "UIMachineAttributeSetter.h"
#include "UIMessageCenter.h"


void UIMachineAttributeSetter::setMachineAttribute(const CMachine &comConstMachine,
                                                   const MachineAttribute &enmType,
                                                   const QVariant &guiAttribute)
{
    /* Get editable machine & session: */
    CMachine comMachine = comConstMachine;
    CSession comSession = uiCommon().tryToOpenSessionFor(comMachine);

    /* Main API block: */
    do
    {
        /* Assign attribute depending on passed type: */
        switch (enmType)
        {
            case MachineAttribute_Name: comMachine.SetName(guiAttribute.toString()); break;
            default: break;
        }
        /* Change machine name: */
        if (!comMachine.isOk())
        {
            msgCenter().cannotChangeMachineAttribute(comMachine);
            break;
        }

        /* Save machine settings: */
        comMachine.SaveSettings();
        if (!comMachine.isOk())
        {
            msgCenter().cannotSaveMachineSettings(comMachine);
            break;
        }
    }
    while (0);

    /* Close session to editable comMachine if necessary: */
    if (!comSession.isNull())
        comSession.UnlockMachine();
}
