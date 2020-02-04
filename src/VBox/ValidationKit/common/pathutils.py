# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=too-many-lines

"""
Path Utility Functions.
"""

__copyright__ = \
"""
Copyright (C) 2012-2020 Oracle Corporation

This file is part of VirtualBox Open Source Edition (OSE), as
available from http://www.virtualbox.org. This file is free software;
you can redistribute it and/or modify it under the terms of the GNU
General Public License (GPL) as published by the Free Software
Foundation, in version 2 as it comes in the "COPYING" file of the
VirtualBox OSE distribution. VirtualBox OSE is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

The contents of this file may alternatively be used under the terms
of the Common Development and Distribution License Version 1.0
(CDDL) only, as it comes in the "COPYING.CDDL" file of the
VirtualBox OSE distribution, in which case the provisions of the
CDDL are applicable instead of those of the GPL.

You may elect to license modified versions of this file under the
terms and conditions of either the GPL or the CDDL or both.
"""
__version__ = "$Revision$"


# Standard Python imports.
import unittest;


## @name Path data keyed by fDosStyle bool value.
## @{
g_dsPathSlash       = { False: '/', True: '\\', };
g_dasPathSlashes    = { False: ('/',), True: ('\\', '/', ), };
g_dasPathSeparators = { False: ('/',), True: ('\\', '/', ':', ), };
## @}


def joinEx(fDosStyle, sBase, *asAppend):
    """
    Mimicking os.path.join, but where target system isn't the host.
    The code is very simple at present.
    """
    # Get the first non-None element and use it as base.
    i = 0;
    sRet = sBase;
    while sRet is None and i < len(asAppend):
        sRet = asAppend[i];
        i += 1;

    while i < len(asAppend):
        sAppend = asAppend[i];

        # Skip None elements.
        if sAppend is not None:
            # Strip leading slashes from sAppend:
            offSkip = 0;
            while offSkip < len(sAppend) and sAppend[offSkip] in g_dasPathSlashes[fDosStyle]:
                offSkip += 1;

            # Add separator if needed before appending the new bit:
            if not sRet or sRet[-1] not in g_dasPathSeparators[fDosStyle]:
                sRet += g_dsPathSlash[fDosStyle] + sAppend[offSkip:];
            else:
                sRet += sAppend[offSkip:];

        i += 1;

    return sRet;


#
# Unit testing.
#

# pylint: disable=missing-docstring,undefined-variable
class JoinExTestCase(unittest.TestCase):
    def testJoinEx(self):
        self.assertEqual(joinEx(True, None), None);
        self.assertEqual(joinEx(False, None), None);
        self.assertEqual(joinEx(True, ''), '');
        self.assertEqual(joinEx(False, ''), '');
        self.assertEqual(joinEx(True, '',''), '\\');
        self.assertEqual(joinEx(False, '',''), '/');
        self.assertEqual(joinEx(True, 'C:','dos'), 'C:dos');
        self.assertEqual(joinEx(True, 'C:/','dos'), 'C:/dos');
        self.assertEqual(joinEx(True, 'C:\\','dos'), 'C:\\dos');
        self.assertEqual(joinEx(True, 'C:\\dos','edlin.com'), 'C:\\dos\\edlin.com');
        self.assertEqual(joinEx(True, 'C:\\dos\\','edlin.com'), 'C:\\dos\\edlin.com');
        self.assertEqual(joinEx(True, 'C:\\dos/','edlin.com'), 'C:\\dos/edlin.com');
        self.assertEqual(joinEx(True, 'C:\\dos//','edlin.com'), 'C:\\dos//edlin.com');
        self.assertEqual(joinEx(True, 'C:\\dos','\\/edlin.com'), 'C:\\dos\\edlin.com');
        self.assertEqual(joinEx(True, 'C:\\dos', None, 'edlin.com'), 'C:\\dos\\edlin.com');
        self.assertEqual(joinEx(True, None, 'C:\\dos', None, 'edlin.com'), 'C:\\dos\\edlin.com');
        self.assertEqual(joinEx(True, None, None, 'C:\\dos', None, 'edlin.com', None), 'C:\\dos\\edlin.com');
        self.assertEqual(joinEx(False, '/', 'bin', 'ls'), '/bin/ls');
        self.assertEqual(joinEx(False, '/', '/bin', 'ls'), '/bin/ls');
        self.assertEqual(joinEx(False, '/', '/bin/', 'ls'), '/bin/ls');
        self.assertEqual(joinEx(False, '/', '/bin//', 'ls'), '/bin//ls');
        self.assertEqual(joinEx(False, '/', None, 'bin', None, 'ls', None), '/bin/ls');
        self.assertEqual(joinEx(False, None, '/', None, 'bin', None, 'ls', None), '/bin/ls');


if __name__ == '__main__':
    unittest.main();
    # not reached.

