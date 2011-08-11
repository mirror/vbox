/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UINetworkManager class implementation
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Local includes: */
#include "UINetworkManager.h"

/* static */
UINetworkManager* UINetworkManager::m_pInstance = 0;

/* static */
void UINetworkManager::create()
{
    /* Check that instance do NOT exist: */
    if (m_pInstance)
        return;

    /* Create instance: */
    new UINetworkManager;
}

/* static */
void UINetworkManager::destroy()
{
    /* Check that instance exists: */
    if (!m_pInstance)
        return;

    /* Destroy instance: */
    delete m_pInstance;
}

/* Constructor: */
UINetworkManager::UINetworkManager()
{
    /* Prepare instance: */
    m_pInstance = this;

    /* Prepare finally: */
    prepare();
}

/* Destructor: */
UINetworkManager::~UINetworkManager()
{
    /* Cleanup first: */
    cleanup();

    /* Cleanup instance: */
    m_pInstance = 0;
}

/* Prepare: */
void UINetworkManager::prepare()
{
}

/* Cleanup: */
void UINetworkManager::cleanup()
{
}

