/* $Id$ */
/** @file
 * VBox Qt GUI - UITextTable class implementation.
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

/* GUI includes: */
#include "UITextTable.h"


UITextTableLine::UITextTableLine(const QString &str1, const QString &str2, QObject *pParent /* = 0 */)
    : QObject(pParent)
    , m_str1(str1)
    , m_str2(str2)
{}

UITextTableLine::UITextTableLine(const UITextTableLine &other)
    : QObject(other.parent())
    , m_str1(other.string1())
    , m_str2(other.string2())
{}

UITextTableLine &UITextTableLine::operator=(const UITextTableLine &other)
{
    setParent(other.parent());
    set1(other.string1());
    set2(other.string2());
    return *this;
}

bool UITextTableLine::operator==(const UITextTableLine &other) const
{
    return    string1() == other.string1()
           && string2() == other.string2();
}
