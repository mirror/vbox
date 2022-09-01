#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=line-too-long

"""
A cronjob that maintains archived testsets.
"""

from __future__ import print_function;

__copyright__ = \
"""
Copyright (C) 2012-2022 Oracle and/or its affiliates.

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

The contents of this file may alternatively be used under the terms
of the Common Development and Distribution License Version 1.0
(CDDL), a copy of it is provided in the "COPYING.CDDL" file included
in the VirtualBox distribution, in which case the provisions of the
CDDL are applicable instead of those of the GPL.

You may elect to license modified versions of this file under the
terms and conditions of either the GPL or the CDDL or both.

SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
"""
__version__ = "$Revision$"

# Standard python imports
from datetime import datetime, timedelta
import sys
import os
from optparse import OptionParser, OptionGroup;  # pylint: disable=deprecated-module
import shutil
import tempfile;
import time;
import zipfile;

# Add Test Manager's modules path
g_ksTestManagerDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(g_ksTestManagerDir)

# Test Manager imports
from common                     import utils;
from testmanager                import config;


class ArchiveDelFilesBatchJob(object): # pylint: disable=too-few-public-methods
    """
    Log+files comp
    """

    def __init__(self, sCmd, oOptions):
        """
        Parse command line
        """
        self.fVerbose       = oOptions.fVerbose;
        self.sCmd           = sCmd;
        self.sSrcDir        = oOptions.sSrcDir;
        if not self.sSrcDir :
            self.sSrcDir    = config.g_ksFileAreaRootDir;
        self.sDstDir        = config.g_ksZipFileAreaRootDir;
        self.sTempDir       = oOptions.sTempDir;
        if not self.sTempDir:
            self.sTempDir   = tempfile.gettempdir();
        #self.oTestSetLogic = TestSetLogic(TMDatabaseConnection(self.dprint if self.fVerbose else None));
        #self.oTestSetLogic = TestSetLogic(TMDatabaseConnection(None));
        self.fDryRun        = oOptions.fDryRun;
        self.asFileExt      = [];
        self.asFileExt      = oOptions.asFileExt and oOptions.asFileExt.split(',');
        self.cOlderThanDays = oOptions.cOlderThanDays;
        self.cbBiggerThan   = oOptions.uBiggerThanKb * 1024; # Kilobyte (kB) to bytes.
        self.fForce         = oOptions.fForce;

    def dprint(self, sText):
        """ Verbose output. """
        if self.fVerbose:
            print(sText);
        return True;

    def warning(self, sText):
        """Prints a warning."""
        print(sText);
        return True;

    def _processTestSetZip(self, idTestSet, sFile, sCurDir):
        """
        Worker for processDir.
        Same return codes as processDir.
        """

        _ = idTestSet

        sSrcZipFileAbs = os.path.join(sCurDir, sFile);
        print('Processing ZIP archive "%s" ...' % (sSrcZipFileAbs));

        with tempfile.NamedTemporaryFile(dir=self.sTempDir, delete=False) as tmpfile:
            sDstZipFileAbs = tmpfile.name
        self.dprint('Using temporary ZIP archive "%s"' % (sDstZipFileAbs));

        fRc = True;

        try:
            oSrcZipFile = zipfile.ZipFile(sSrcZipFileAbs, 'r');                            # pylint: disable=consider-using-with
            try:
                if not self.fDryRun:
                    oDstZipFile = zipfile.ZipFile(sDstZipFileAbs, 'w');
                try:
                    for oCurFile in oSrcZipFile.infolist():

                        self.dprint('Handling File "%s" ...' % (oCurFile.filename))
                        sFileExt = os.path.splitext(oCurFile.filename)[1];

                        fDoRepack = True; # Re-pack all unless told otherwise.

                        if  sFileExt \
                        and sFileExt[1:] in self.asFileExt:
                            self.dprint('\tMatches excluded extensions')
                            fDoRepack = False;

                        if  self.cbBiggerThan \
                        and oCurFile.file_size > self.cbBiggerThan:
                            self.dprint('\tIs bigger than %d bytes (%d bytes)' % (self.cbBiggerThan, oCurFile.file_size))
                            fDoRepack = False;

                        if fDoRepack \
                           and self.cOlderThanDays:
                            tsMaxAge  = datetime.now() - timedelta(days = self.cOlderThanDays);
                            tsFile    = datetime(year   = oCurFile.date_time[0], \
                                                 month  = oCurFile.date_time[1], \
                                                 day    = oCurFile.date_time[2],
                                                 hour   = oCurFile.date_time[3],
                                                 minute = oCurFile.date_time[4],
                                                 second = oCurFile.date_time[5],);
                            if tsFile < tsMaxAge:
                                self.dprint('\tIs older than %d days (%s)' % (self.cOlderThanDays, tsFile))
                                fDoRepack = False;

                        if fDoRepack:
                            self.dprint('Re-packing file "%s"' % (oCurFile.filename,))
                            if not self.fDryRun:
                                oBuf = oSrcZipFile.read(oCurFile);
                                oDstZipFile.writestr(oCurFile, oBuf);
                        else:
                            print('Deleting file "%s"' % (oCurFile.filename,))
                    if not self.fDryRun:
                        oDstZipFile.close();
                except Exception as oXcpt4:
                    print(oXcpt4);
                    return (None, 'Error handling file "%s" of archive "%s": %s' \
                            % (oCurFile.filename, sSrcZipFileAbs, oXcpt4,), None);

                oSrcZipFile.close();

                try:
                    self.dprint('Deleting ZIP file "%s"' % (sSrcZipFileAbs));
                    if not self.fDryRun:
                        os.remove(sSrcZipFileAbs);
                except Exception as oXcpt:
                    return (None, 'Error deleting ZIP file "%s": %s' % (sSrcZipFileAbs, oXcpt,), None);
                try:
                    self.dprint('Moving ZIP "%s" to "%s"' % (sDstZipFileAbs, sSrcZipFileAbs));
                    if not self.fDryRun:
                        shutil.move(sDstZipFileAbs, sSrcZipFileAbs);
                except Exception as oXcpt5:
                    return (None, 'Error moving temporary ZIP "%s" to original ZIP file "%s": %s' \
                            % (sDstZipFileAbs, sSrcZipFileAbs, oXcpt5,), None);
            except Exception as oXcpt3:
                return (None, 'Error creating temporary ZIP archive "%s": %s' % (sDstZipFileAbs, oXcpt3,), None);
        except Exception as oXcpt1:
            # Construct a meaningful error message.
            try:
                if os.path.exists(sSrcZipFileAbs):
                    return (None, 'Error opening "%s": %s' % (sSrcZipFileAbs, oXcpt1), None);
                if not os.path.exists(sFile):
                    return (None, 'File "%s" not found. [%s]' % (sSrcZipFileAbs, sFile), None);
                return (None, 'Error opening "%s" inside "%s": %s' % (sSrcZipFileAbs, sFile, oXcpt1), None);
            except Exception as oXcpt2:
                return (None, 'WTF? %s; %s' % (oXcpt1, oXcpt2,), None);

        return fRc;


    def processDir(self, sCurDir):
        """
        Process the given directory (relative to sSrcDir and sDstDir).
        Returns success indicator.
        """

        if not self.asFileExt:
            print('Must specify at least one file extension to delete.');
            return False;

        if self.fVerbose:
            self.dprint('Processing directory: %s' % (sCurDir,));

        #
        # Sift thought the directory content, collecting subdirectories and
        # sort relevant files by test set.
        # Generally there will either be subdirs or there will be files.
        #
        asSubDirs = [];
        dTestSets = {};
        sCurPath = os.path.abspath(os.path.join(self.sSrcDir, sCurDir));
        for sFile in os.listdir(sCurPath):
            if os.path.isdir(os.path.join(sCurPath, sFile)):
                if sFile not in [ '.', '..' ]:
                    asSubDirs.append(sFile);
            elif sFile.startswith('TestSet-') \
            and  sFile.endswith('zip'):
                # Parse the file name. ASSUMES 'TestSet-%d-filename' format.
                iSlash1 = sFile.find('-');
                iSlash2 = sFile.find('-', iSlash1 + 1);
                if iSlash2 <= iSlash1:
                    self.warning('Bad filename (1): "%s"' % (sFile,));
                    continue;

                try:    idTestSet = int(sFile[(iSlash1 + 1):iSlash2]);
                except:
                    self.warning('Bad filename (2): "%s"' % (sFile,));
                    if self.fVerbose:
                        self.dprint('\n'.join(utils.getXcptInfo(4)));
                    continue;

                if idTestSet <= 0:
                    self.warning('Bad filename (3): "%s"' % (sFile,));
                    continue;

                if iSlash2 + 2 >= len(sFile):
                    self.warning('Bad filename (4): "%s"' % (sFile,));
                    continue;
                sName = sFile;

                # Add it.
                if idTestSet not in dTestSets:
                    dTestSets[idTestSet] = [];
                asTestSet = dTestSets[idTestSet];
                asTestSet.append(sName);

        #
        # Test sets.
        #
        fRc = True;
        for idTestSet, oTestSet in dTestSets.items():
            try:
                if self._processTestSetZip(idTestSet, oTestSet[0], sCurDir) is not True:
                    fRc = False;
            except:
                self.warning('TestSet %d: Exception in _processTestSetZip:\n%s' % (idTestSet, '\n'.join(utils.getXcptInfo()),));
                fRc = False;

        #
        # Sub dirs.
        #
        self.dprint('Processing sub-directories');
        for sSubDir in asSubDirs:
            if self.processDir(os.path.join(sCurDir, sSubDir)) is not True:
                fRc = False;

        #
        # Try Remove the directory iff it's not '.' and it's been unmodified
        # for the last 24h (race protection).
        #
        if sCurDir != '.':
            try:
                fpModTime = float(os.path.getmtime(sCurPath));
                if fpModTime + (24*3600) <= time.time():
                    if utils.noxcptRmDir(sCurPath) is True:
                        self.dprint('Removed "%s".' % (sCurPath,));
            except:
                pass;

        return fRc;

    @staticmethod
    def main():
        """ C-style main(). """
        #
        # Parse options.
        #

        if len(sys.argv) < 2:
            print('Must specify a main command!\n');
            return 1;

        sCommand = sys.argv[1];

        asCmds = [ 'delete-files' ];
        if sCommand not in asCmds:
            print('Unknown main command! Must be one of: %s\n' % ', '.join(asCmds));
            return 1;

        oParser = OptionParser();

        # Generic options.
        oParser.add_option('-v', '--verbose', dest = 'fVerbose', action = 'store_true',  default = False,
                           help = 'Verbose output.');
        oParser.add_option('-q', '--quiet',   dest = 'fVerbose', action = 'store_false', default = False,
                           help = 'Quiet operation.');
        oParser.add_option('-d', '--dry-run', dest = 'fDryRun',  action = 'store_true',  default = False,
                           help = 'Dry run, do not make any changes.');
        oParser.add_option('--source-dir', type = 'string', dest = 'sSrcDir',
                           help = 'Specifies the source directory to process.');
        oParser.add_option('--temp-dir', type = 'string', dest = 'sTempDir',
                           help = 'Specifies the temp directory to use.');
        oParser.add_option('--force', dest = 'fForce', action = 'store_true', default = False,
                           help = 'Forces the operation.');

        if sCommand == 'delete-files':
            oGroup = OptionGroup(oParser, "File deletion options", "Deletes files from testset archives.");
            oGroup.add_option('--file-ext', type = 'string', dest = 'asFileExt',
                              help = 'Selects files with the given extensions.');
            oGroup.add_option('--older-than-days', type = 'int', dest = 'cOlderThanDays', default = 0,
                              help = 'Selects all files which are older than NUM days.');
            oGroup.add_option('--bigger-than-kb', type = 'int', dest = 'uBiggerThanKb', default = 0,
                              help = 'Selects all files which are bigger than (kB).\nA kilobyte is 1024 bytes.');
            oParser.add_option_group(oGroup);

        (oOptions, asArgs) = oParser.parse_args(sys.argv[2:]);
        if asArgs != []:
            oParser.print_help();
            return 1;

        if oOptions.fDryRun:
            print('***********************************');
            print('*** DRY RUN - NO FILES MODIFIED ***');
            print('***********************************');

        #
        # Do the work.
        #
        fRc = False;

        if sCommand == 'delete-files':
            print('Job: Deleting files from archive');
            oBatchJob = ArchiveDelFilesBatchJob(sCommand, oOptions);
            fRc = oBatchJob.processDir(oBatchJob.sSrcDir);

        if oOptions.fVerbose:
            print('SUCCESS' if fRc else 'FAILURE');

        return 0 if fRc is True else 1;

if __name__ == '__main__':
    sys.exit(ArchiveDelFilesBatchJob.main());
