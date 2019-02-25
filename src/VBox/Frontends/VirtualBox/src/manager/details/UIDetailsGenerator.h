/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsGenerator declaration.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_details_UIDetailsGenerator_h
#define FEQT_INCLUDED_SRC_manager_details_UIDetailsGenerator_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIExtraDataDefs.h"
#include "UITextTable.h"

/* Forward declarations: */
class CMachine;

/** Details generation namespace. */
namespace UIDetailsGenerator
{
    UITextTable generateMachineInformationGeneral(CMachine &comMachine,
                                                  const UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral &fOptions);

    UITextTable generateMachineInformationSystem(CMachine &comMachine,
                                                 const UIExtraDataMetaDefs::DetailsElementOptionTypeSystem &fOptions);

    UITextTable generateMachineInformationDisplay(CMachine &comMachine,
                                                  const UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay &fOptions);

    UITextTable generateMachineInformationStorage(CMachine &comMachine,
                                                  const UIExtraDataMetaDefs::DetailsElementOptionTypeStorage &fOptions);

    UITextTable generateMachineInformationAudio(CMachine &comMachine,
                                                const UIExtraDataMetaDefs::DetailsElementOptionTypeAudio &fOptions);

    UITextTable generateMachineInformationNetwork(CMachine &comMachine,
                                                  const UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork &fOptions);

    UITextTable generateMachineInformationSerial(CMachine &comMachine,
                                                 const UIExtraDataMetaDefs::DetailsElementOptionTypeSerial &fOptions);

    UITextTable generateMachineInformationUSB(CMachine &comMachine,
                                              const UIExtraDataMetaDefs::DetailsElementOptionTypeUsb &fOptions);

    UITextTable generateMachineInformationSharedFolders(CMachine &comMachine,
                                                        const UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders &fOptions);

    UITextTable generateMachineInformationUI(CMachine &comMachine,
                                             const UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface &fOptions);

    UITextTable generateMachineInformationDetails(CMachine &comMachine,
                                                  const UIExtraDataMetaDefs::DetailsElementOptionTypeDescription &fOptions);
}

#endif /* !FEQT_INCLUDED_SRC_manager_details_UIDetailsGenerator_h */
