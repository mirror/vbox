/* $Id$ */
/** @file
 * VBox Qt GUI - UIMousePointerShapeData class implementation.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UIMousePointerShapeData.h"

UIMousePointerShapeData::UIMousePointerShapeData(bool fVisible /* = false */,
                                                 bool fAlpha /* = false */,
                                                 const QPoint &hotSpot /* = QPoint() */,
                                                 const QSize &shapeSize /* = QSize() */,
                                                 const QVector<BYTE> &shape /* = QVector<BYTE>() */)
    : m_fVisible(fVisible)
    , m_fAlpha(fAlpha)
    , m_hotSpot(hotSpot)
    , m_shapeSize(shapeSize)
    , m_shape(shape)
{
}

UIMousePointerShapeData::UIMousePointerShapeData(const UIMousePointerShapeData &another)
    : m_fVisible(another.isVisible())
    , m_fAlpha(another.hasAlpha())
    , m_hotSpot(another.hotSpot())
    , m_shapeSize(another.shapeSize())
    , m_shape(another.shape())
{
}

UIMousePointerShapeData &UIMousePointerShapeData::operator=(const UIMousePointerShapeData &another)
{
    m_fVisible = another.isVisible();
    m_fAlpha = another.hasAlpha();
    m_hotSpot = another.hotSpot();
    m_shapeSize = another.shapeSize();
    m_shape = another.shape();
    return *this;
}
