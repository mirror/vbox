/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestRAMSlider class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIGuestRAMSlider.h"

/* COM includes: */
# include "CSystemProperties.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIGuestRAMSlider::UIGuestRAMSlider(QWidget *pParent /* = 0 */)
  : QIAdvancedSlider(pParent)
  , m_uMinRAM(0)
  , m_uMaxRAMOpt(0)
  , m_uMaxRAMAlw(0)
  , m_uMaxRAM(0)
{
    /* Prepare: */
    prepare();
}

UIGuestRAMSlider::UIGuestRAMSlider(Qt::Orientation enmOrientation, QWidget *pParent /* = 0 */)
  : QIAdvancedSlider(enmOrientation, pParent)
  , m_uMinRAM(0)
  , m_uMaxRAMOpt(0)
  , m_uMaxRAMAlw(0)
  , m_uMaxRAM(0)
{
    /* Prepare: */
    prepare();
}

uint UIGuestRAMSlider::minRAM() const
{
    return m_uMinRAM;
}

uint UIGuestRAMSlider::maxRAMOpt() const
{
    return m_uMaxRAMOpt;
}

uint UIGuestRAMSlider::maxRAMAlw() const
{
    return m_uMaxRAMAlw;
}

uint UIGuestRAMSlider::maxRAM() const
{
    return m_uMaxRAM;
}

void UIGuestRAMSlider::prepare()
{
    ulong uFullSize = vboxGlobal().host().GetMemorySize();
    CSystemProperties sys = vboxGlobal().virtualBox().GetSystemProperties();
    m_uMinRAM = sys.GetMinGuestRAM();
    m_uMaxRAM = RT_MIN(RT_ALIGN(uFullSize, _1G / _1M), sys.GetMaxGuestRAM());

    /* Come up with some nice round percent boundaries relative to
     * the system memory. A max of 75% on a 256GB config is ridiculous,
     * even on an 8GB rig reserving 2GB for the OS is way to conservative.
     * The max numbers can be estimated using the following program:
     *
     *      double calcMaxPct(uint64_t cbRam)
     *      {
     *          double cbRamOverhead = cbRam * 0.0390625; // 160 bytes per page.
     *          double cbRamForTheOS = RT_MAX(RT_MIN(_512M, cbRam * 0.25), _64M);
     *          double OSPct  = (cbRamOverhead + cbRamForTheOS) * 100.0 / cbRam;
     *          double MaxPct = 100 - OSPct;
     *          return MaxPct;
     *      }
     *
     *      int main()
     *      {
     *          uint64_t cbRam = _1G;
     *          for (; !(cbRam >> 33); cbRam += _1G)
     *              printf("%8lluGB %.1f%% %8lluKB\n", cbRam >> 30, calcMaxPct(cbRam),
     *                     (uint64_t)(cbRam * calcMaxPct(cbRam) / 100.0) >> 20);
     *          for (; !(cbRam >> 51); cbRam <<= 1)
     *              printf("%8lluGB %.1f%% %8lluKB\n", cbRam >> 30, calcMaxPct(cbRam),
     *                     (uint64_t)(cbRam * calcMaxPct(cbRam) / 100.0) >> 20);
     *          return 0;
     *      }
     *
     * Note. We might wanna put these calculations somewhere global later. */

    /* System RAM amount test: */
    m_uMaxRAMAlw = (uint)(0.75 * uFullSize);
    m_uMaxRAMOpt = (uint)(0.50 * uFullSize);
    if (uFullSize < 3072)
        /* done */;
    else if (uFullSize < 4096)   /* 3GB */
        m_uMaxRAMAlw = (uint)(0.80 * uFullSize);
    else if (uFullSize < 6144)   /* 4-5GB */
    {
        m_uMaxRAMAlw = (uint)(0.84 * uFullSize);
        m_uMaxRAMOpt = (uint)(0.60 * uFullSize);
    }
    else if (uFullSize < 8192)   /* 6-7GB */
    {
        m_uMaxRAMAlw = (uint)(0.88 * uFullSize);
        m_uMaxRAMOpt = (uint)(0.65 * uFullSize);
    }
    else if (uFullSize < 16384)  /* 8-15GB */
    {
        m_uMaxRAMAlw = (uint)(0.90 * uFullSize);
        m_uMaxRAMOpt = (uint)(0.70 * uFullSize);
    }
    else if (uFullSize < 32768)  /* 16-31GB */
    {
        m_uMaxRAMAlw = (uint)(0.93 * uFullSize);
        m_uMaxRAMOpt = (uint)(0.75 * uFullSize);
    }
    else if (uFullSize < 65536)  /* 32-63GB */
    {
        m_uMaxRAMAlw = (uint)(0.94 * uFullSize);
        m_uMaxRAMOpt = (uint)(0.80 * uFullSize);
    }
    else if (uFullSize < 131072) /* 64-127GB */
    {
        m_uMaxRAMAlw = (uint)(0.95 * uFullSize);
        m_uMaxRAMOpt = (uint)(0.85 * uFullSize);
    }
    else                        /* 128GB- */
    {
        m_uMaxRAMAlw = (uint)(0.96 * uFullSize);
        m_uMaxRAMOpt = (uint)(0.90 * uFullSize);
    }
    /* Now check the calculated maximums are out of the range for the guest
     * RAM. If so change it accordingly. */
    m_uMaxRAMAlw = RT_MIN(m_uMaxRAMAlw, m_uMaxRAM);
    m_uMaxRAMOpt = RT_MIN(m_uMaxRAMOpt, m_uMaxRAM);

    setPageStep(calcPageStep(m_uMaxRAM));
    setSingleStep(pageStep() / 4);
    setTickInterval(pageStep());
    /* Setup the scale so that ticks are at page step boundaries */
    setMinimum((m_uMinRAM / pageStep()) * pageStep());
    setMaximum(m_uMaxRAM);
    setSnappingEnabled(true);
    setOptimalHint(m_uMinRAM, m_uMaxRAMOpt);
    setWarningHint(m_uMaxRAMOpt, m_uMaxRAMAlw);
    setErrorHint(m_uMaxRAMAlw, m_uMaxRAM);
}

int UIGuestRAMSlider::calcPageStep(int iMaximum) const
{
    /* Calculate a suitable page step size for the given max value.
     * The returned size is so that there will be no more than 32
     * pages. The minimum returned page size is 4. */

    /* Reasonable max. number of page steps is 32: */
    uint uPage = ((uint)iMaximum + 31) / 32;
    /* Make it a power of 2: */
    uint p = uPage, p2 = 0x1;
    while ((p >>= 1))
        p2 <<= 1;
    if (uPage != p2)
        p2 <<= 1;
    if (p2 < 4)
        p2 = 4;
    return (int) p2;
}
