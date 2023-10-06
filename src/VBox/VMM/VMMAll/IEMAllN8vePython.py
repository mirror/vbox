#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=invalid-name

"""
Native recompiler side-kick for IEMAllThrdPython.py.

Analyzes the each threaded function variant to see if we can we're able to
recompile it, then provides modifies MC block code for doing so.
"""

from __future__ import print_function;

__copyright__ = \
"""
Copyright (C) 2023 Oracle and/or its affiliates.

This file is part of VirtualBox base platform packages, as
available from https://www.virtualbox.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, in version 3 of the
License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <https://www.gnu.org/licenses>.

SPDX-License-Identifier: GPL-3.0-only
"""
__version__ = "$Revision$"

# Standard python imports:
#import sys;

# Out python imports:
import IEMAllInstPython as iai;


class NativeRecompFunctionVariation(object):
    """
    Class that deals with transforming a threaded function variation into a
    native recompiler function.

    This base class doesn't do any transforming and just renders the same
    code as for the threaded function.
    """

    def __init__(self, oVariation, sHostArch):
        self.oVariation = oVariation # type: ThreadedFunctionVariation
        self.sHostArch  = sHostArch;

    def isRecompilable(self):
        """
        Predicate that returns whether the variant can be recompiled natively
        (for the selected host architecture).
        """
        return True;

    def renderCode(self, cchIndent):
        """
        Returns the native recompiler function body for this threaded variant.
        """
        aoStmts = self.oVariation.aoStmtsForThreadedFunction # type: list(McStmt)
        return iai.McStmt.renderCodeForList(aoStmts, cchIndent);



def analyzeVariantForNativeRecomp(oVariation,
                                  sHostArch): # type: (ThreadedFunctionVariation, str) -> NativeRecompFunctionVariation
    """
    This function analyzes the threaded function variant and returns an
    NativeRecompFunctionVariation instance for it, unless it's not
    possible to recompile at present.

    Returns NativeRecompFunctionVariation or None.
    """

    #
    # Analyze the statements.
    #
    aoStmts = oVariation.aoStmtsForThreadedFunction # type: list(McStmt)

    # The simplest case are the IEM_MC_DEFER_TO_CIMPL_*_RET_THREADED ones, just pass them thru:
    if (    len(aoStmts) == 1
        and aoStmts[0].sName.startswith('IEM_MC_DEFER_TO_CIMPL_')
        and aoStmts[0].sName.endswith('_RET_THREADED')):
        return NativeRecompFunctionVariation(oVariation, sHostArch);

    return None;
