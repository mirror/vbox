/* $Id$ */
/** @file
 * VirtualBox audio base class for Main audio drivers.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_AUDIODRIVER
#define ____H_AUDIODRIVER

#include <VBox/com/ptr.h>
#include <VBox/com/string.h>

using namespace com;

/**
 * Audio driver configuration for audio drivers implemented
 * in Main.
 */
struct AudioDriverCfg
{
    AudioDriverCfg(Utf8Str a_strDev = "", unsigned a_uInst = 0, Utf8Str a_strName = "")
        : strDev(a_strDev)
        , uInst(a_uInst)
        , strName(a_strName) { }

    AudioDriverCfg& operator=(AudioDriverCfg that)
    {
        this->strDev  = that.strDev;
        this->uInst   = that.uInst;
        this->strName = that.strName;

        return *this;
    }

    /** The device name. */
    Utf8Str              strDev;
    /** The device instance. */
    unsigned             uInst;
    /** The driver name. */
    Utf8Str              strName;
};

class Console;

/**
 * Base class for all audio drivers implemented in Main.
 */
class AudioDriver
{

public:

    AudioDriver(Console *pConsole);

    virtual ~AudioDriver();

    AudioDriverCfg *GetConfig(void) { return &mCfg; }

    Console *GetParent(void) { return mpConsole; }

    int Initialize(AudioDriverCfg *pCfg);

    bool IsAttached(void) { return mfAttached; }

    static DECLCALLBACK(int) Attach(AudioDriver *pThis);

    static DECLCALLBACK(int) Detach(AudioDriver *pThis);

protected:

    int configure(unsigned uLUN, bool fAttach);

    /**
     * Optional (virtual) function to give the derived audio driver
     * class the ability to add more driver configuration entries when
     * setting up.
     *
     * @param pLunCfg           CFGM configuration node of the driver.
     */
    virtual void configureDriver(PCFGMNODE pLunCfg) { RT_NOREF(pLunCfg); }

    unsigned getFreeLUN(void);

protected:

    /** Pointer to parent. */
    Console             *mpConsole;
    /** The driver's configuration. */
    AudioDriverCfg       mCfg;
    /** Whether the driver is attached or not. */
    bool                 mfAttached;
    /** The LUN the driver is attached to.
     *  Set the UINT8_MAX if not attached. */
    unsigned             muLUN;
};

#endif /* ____H_AUDIODRIVER */

