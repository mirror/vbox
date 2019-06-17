# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=too-many-lines

"""
Test File Set
"""

__copyright__ = \
"""
Copyright (C) 2010-2019 Oracle Corporation

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
import os;
import random;
import string;
import sys;
import tarfile;
import unittest;

# Validation Kit imports.
from common     import utils;
from common     import pathutils;
from testdriver import reporter;

# Python 3 hacks:
if sys.version_info[0] >= 3:
    xrange = range; # pylint: disable=redefined-builtin,invalid-name



class TestFsObj(object):
    """ A file system object we created in for test purposes. """
    def __init__(self, oParent, sPath):
        self.oParent   = oParent    # type: TestDir
        self.sPath     = sPath      # type: str
        self.sName     = sPath      # type: str
        if oParent:
            assert sPath.startswith(oParent.sPath);
            self.sName = sPath[len(oParent.sPath) + 1:];
            # Add to parent.
            oParent.aoChildren.append(self);
            oParent.dChildrenUpper[self.sName.upper()] = self;

class TestFile(TestFsObj):
    """ A file object in the guest. """
    def __init__(self, oParent, sPath, abContent):
        TestFsObj.__init__(self, oParent, sPath);
        self.abContent = abContent          # type: bytearray
        self.cbContent = len(abContent);
        self.off       = 0;

    def read(self, cbToRead):
        """ read() emulation. """
        assert self.off <= self.cbContent;
        cbLeft = self.cbContent - self.off;
        if cbLeft < cbToRead:
            cbToRead = cbLeft;
        abRet = self.abContent[self.off:(self.off + cbToRead)];
        assert len(abRet) == cbToRead;
        self.off += cbToRead;
        if sys.version_info[0] < 3:
            return bytes(abRet);
        return abRet;

class TestDir(TestFsObj):
    """ A file object in the guest. """
    def __init__(self, oParent, sPath):
        TestFsObj.__init__(self, oParent, sPath);
        self.aoChildren     = []  # type: list(TestFsObj)
        self.dChildrenUpper = {}  # type: dict(str, TestFsObj)

    def contains(self, sName):
        """ Checks if the directory contains the given name. """
        return sName.upper() in self.dChildrenUpper


class TestFileSet(object):
    """
    A generated set of files and directories for use in a test.

    Can be wrapped up into a tarball or written directly to the file system.
    """

    ksReservedWinOS2         = '/\\"*:<>?|\t\v\n\r\f\a\b';
    ksReservedUnix           = '/';
    ksReservedTrailingWinOS2 = ' .';
    ksReservedTrailingUnix   = '';

    ## @name Path style.
    ## @{

    ## @}

    def __init__(self, fDosStyle, sBasePath, sSubDir, # pylint: disable=too-many-arguments
                 asCompatibleWith = None,             # List of getHostOs values to the names must be compatible with.
                 oRngFileSizes = xrange(0, 16384),
                 oRngManyFiles = xrange(128, 512),
                 oRngTreeFiles = xrange(128, 384),
                 oRngTreeDepth = xrange(92, 256),
                 oRngTreeDirs  = xrange(2, 16),
                 cchMaxPath    = 230,
                 cchMaxName    = 230,
                 uSeed         = None):
        ## @name Parameters
        ## @{
        self.fDosStyle          = fDosStyle;
        self.sMinStyle          = 'win' if fDosStyle else 'linux';
        if asCompatibleWith is not None:
            for sOs in asCompatibleWith:
                assert sOs in ('win', 'os2', 'darwin', 'linux', 'solaris',), sOs;
            if 'os2' in asCompatibleWith:
                self.sMinStyle      = 'os2';
            elif 'win' in asCompatibleWith:
                self.sMinStyle      = 'win';
        self.sBasePath          = sBasePath;
        self.sSubDir            = sSubDir;
        self.oRngFileSizes      = oRngFileSizes;
        self.oRngManyFiles      = oRngManyFiles;
        self.oRngTreeFiles      = oRngTreeFiles;
        self.oRngTreeDepth      = oRngTreeDepth;
        self.oRngTreeDirs       = oRngTreeDirs;
        self.cchMaxPath         = cchMaxPath;
        self.cchMaxName         = cchMaxName
        ## @}

        ## @name Charset stuff
        ## @todo allow more chars for unix hosts + guests.
        ## @todo include unicode stuff, except on OS/2 and DOS.
        ## @{
        ## The filename charset.
        self.sFileCharset             = string.printable;
        ## Set of characters that should not trail a guest filename.
        self.sReservedTrailing        = self.ksReservedTrailingWinOS2;
        if self.sMinStyle in ('win', 'os2'):
            for ch in self.ksReservedWinOS2:
                self.sFileCharset     = self.sFileCharset.replace(ch, '');
        else:
            self.sReservedTrailing    = self.ksReservedTrailingUnix;
            for ch in self.ksReservedUnix:
                self.sFileCharset     = self.sFileCharset.replace(ch, '');
        # More spaces and dot:
        self.sFileCharset            += '   ...';
        ## @}

        ## The root directory.
        self.oRoot      = None      # type: TestDir;
        ## An empty directory (under root).
        self.oEmptyDir  = None      # type: TestDir;

        ## A directory with a lot of files in it.
        self.oManyDir   = None      # type: TestDir;

        ## A directory with a mixed tree structure under it.
        self.oTreeDir   = None      # type: TestDir;
        ## Number of files in oTreeDir.
        self.cTreeFiles = 0;
        ## Number of directories under oTreeDir.
        self.cTreeDirs  = 0;
        ## Number of other file types under oTreeDir.
        self.cTreeOthers = 0;

        ## All directories in creation order.
        self.aoDirs     = []        # type: list(TestDir);
        ## All files in creation order.
        self.aoFiles    = []        # type: list(TestFile);
        ## Path to object lookup.
        self.dPaths     = {}        # type: dict(str, TestFsObj);

        #
        # Do the creating.
        #
        self.uSeed   = uSeed if uSeed is not None else utils.timestampMilli();
        self.oRandom = random.Random();
        self.oRandom.seed(self.uSeed);
        reporter.log('prepareGuestForTesting: random seed %s' % (self.uSeed,));

        self.__createTestStuff();

    def __createFilename(self, oParent, sCharset, sReservedTrailing):
        """
        Creates a filename contains random characters from sCharset and together
        with oParent.sPath doesn't exceed the given max chars in length.
        """
        ## @todo Consider extending this to take UTF-8 and UTF-16 encoding so we
        ##       can safely use the full unicode range.  Need to check how
        ##       RTZipTarCmd handles file name encoding in general...

        if oParent:
            cchMaxName = self.cchMaxPath - len(oParent.sPath) - 1;
        else:
            cchMaxName = self.cchMaxPath - 4;
        if cchMaxName > self.cchMaxName:
            cchMaxName = self.cchMaxName;
        if cchMaxName <= 1:
            cchMaxName = 2;

        while True:
            cchName = self.oRandom.randrange(1, cchMaxName);
            sName = ''.join(self.oRandom.choice(sCharset) for _ in xrange(cchName));
            if oParent is None or not oParent.contains(sName):
                if sName[-1] not in sReservedTrailing:
                    if sName not in ('.', '..',):
                        return sName;
        return ''; # never reached, but makes pylint happy.

    def generateFilenameEx(self, cchMax = -1, cchMin = -1):
        """
        Generates a filename according to the given specs.

        This is for external use, whereas __createFilename is for internal.

        Returns generated filename.
        """
        assert cchMax == -1 or (cchMax >= 1 and cchMax > cchMin);
        if cchMin <= 0:
            cchMin = 1;
        if cchMax < cchMin:
            cchMax = self.cchMaxName;

        while True:
            cchName = self.oRandom.randrange(cchMin, cchMax + 1);
            sName = ''.join(self.oRandom.choice(self.sFileCharset) for _ in xrange(cchName));
            if sName[-1] not in self.sReservedTrailing:
                if sName not in ('.', '..',):
                    return sName;
        return ''; # never reached, but makes pylint happy.

    def __createTestDir(self, oParent, sDir):
        """
        Creates a test directory.
        """
        oDir = TestDir(oParent, sDir);
        self.aoDirs.append(oDir);
        self.dPaths[sDir] = oDir;
        return oDir;

    def __createTestFile(self, oParent, sFile):
        """
        Creates a test file with random size up to cbMaxContent and random content.
        """
        cbFile = self.oRandom.choice(self.oRngFileSizes);
        abContent = bytearray(self.oRandom.getrandbits(8) for _ in xrange(cbFile));

        oFile = TestFile(oParent, sFile, abContent);
        self.aoFiles.append(oFile);
        self.dPaths[sFile] = oFile;
        return oFile;

    def __createTestStuff(self):
        """
        Create a random file set that we can work on in the tests.
        Returns True/False.
        """

        #
        # Create the root test dir.
        #
        sRoot = pathutils.joinEx(self.fDosStyle, self.sBasePath, self.sSubDir);
        self.oRoot     = self.__createTestDir(None, sRoot);
        self.oEmptyDir = self.__createTestDir(self.oRoot, pathutils.joinEx(self.fDosStyle, sRoot, 'empty'));

        #
        # Create a directory with lots of files in it:
        #
        oDir = self.__createTestDir(self.oRoot, pathutils.joinEx(self.fDosStyle, sRoot, 'many'));
        self.oManyDir = oDir;
        cManyFiles = self.oRandom.choice(self.oRngManyFiles);
        for _ in xrange(cManyFiles):
            sName = self.__createFilename(oDir, self.sFileCharset, self.sReservedTrailing);
            self.__createTestFile(oDir, pathutils.joinEx(self.fDosStyle, oDir.sPath, sName));

        #
        # Generate a tree of files and dirs.
        #
        oDir = self.__createTestDir(self.oRoot, pathutils.joinEx(self.fDosStyle, sRoot, 'tree'));
        uMaxDepth       = self.oRandom.choice(self.oRngTreeDepth);
        cMaxFiles       = self.oRandom.choice(self.oRngTreeFiles);
        cMaxDirs        = self.oRandom.choice(self.oRngTreeDirs);
        self.oTreeDir   = oDir;
        self.cTreeFiles = 0;
        self.cTreeDirs  = 0;
        uDepth          = 0;
        while self.cTreeFiles < cMaxFiles and self.cTreeDirs < cMaxDirs:
            iAction = self.oRandom.randrange(0, 2+1);
            # 0: Add a file:
            if iAction == 0 and self.cTreeFiles < cMaxFiles and len(oDir.sPath) < 230 - 2:
                sName = self.__createFilename(oDir, self.sFileCharset, self.sReservedTrailing);
                self.__createTestFile(oDir, pathutils.joinEx(self.fDosStyle, oDir.sPath, sName));
                self.cTreeFiles += 1;
            # 1: Add a subdirector and descend into it:
            elif iAction == 1 and self.cTreeDirs < cMaxDirs and uDepth < uMaxDepth and len(oDir.sPath) < 220:
                sName = self.__createFilename(oDir, self.sFileCharset, self.sReservedTrailing);
                oDir  = self.__createTestDir(oDir, pathutils.joinEx(self.fDosStyle, oDir.sPath, sName));
                self.cTreeDirs  += 1;
                uDepth += 1;
            # 2: Ascend to parent dir:
            elif iAction == 2 and uDepth > 0:
                oDir = oDir.oParent;
                uDepth -= 1;

        return True;

    def createTarball(self, sTarFileHst):
        """
        Creates a tarball on the host.
        Returns success indicator.
        """
        reporter.log('Creating tarball "%s" with test files for the guest...' % (sTarFileHst,));

        cchSkip = len(self.sBasePath) + 1;

        # Open the tarball:
        try:
            oTarFile = tarfile.open(sTarFileHst, 'w:gz');
        except:
            return reporter.errorXcpt('Failed to open new tar file: %s' % (sTarFileHst,));

        # Directories:
        for oDir in self.aoDirs:
            sPath = oDir.sPath[cchSkip:];
            if self.fDosStyle:
                sPath = sPath.replace('\\', '/');
            oTarInfo = tarfile.TarInfo(sPath + '/');
            oTarInfo.mode = 0o777;
            oTarInfo.type = tarfile.DIRTYPE;
            try:
                oTarFile.addfile(oTarInfo);
            except:
                return reporter.errorXcpt('Failed adding directory tarfile: %s' % (oDir.sPath,));

        # Files:
        for oFile in self.aoFiles:
            sPath = oFile.sPath[cchSkip:];
            if self.fDosStyle:
                sPath = sPath.replace('\\', '/');
            oTarInfo = tarfile.TarInfo(sPath);
            oTarInfo.size = len(oFile.abContent);
            oFile.off = 0;
            try:
                oTarFile.addfile(oTarInfo, oFile);
            except:
                return reporter.errorXcpt('Failed adding directory tarfile: %s' % (oFile.sPath,));

        # Complete the tarball.
        try:
            oTarFile.close();
        except:
            return reporter.errorXcpt('Error closing new tar file: %s' % (sTarFileHst,));
        return True;

    def writeToDisk(self, sAltBase = None):
        """
        Writes out the files to disk.
        Returns True on success, False + error logging on failure.
        """

        # The directories:
        for oDir in self.aoDirs:
            sPath = oDir.sPath;
            if sAltBase:
                sPath = sAltBase + sPath[len(self.sBasePath):];

            try:
                os.mkdir(sPath, 0o770);
            except:
                return reporter.errorXcpt('mkdir(%s) failed' % (sPath,));

        # The files:
        for oFile in self.aoFiles:
            sPath = oFile.sPath;
            if sAltBase:
                sPath = sAltBase + sPath[len(self.sBasePath):];

            try:
                oFile = open(sPath, 'wb');
            except:
                return reporter.errorXcpt('open(%s, "wb") failed' % (sPath,));
            try:
                if sys.version_info[0] < 3:
                    oFile.write(bytes(oFile.abContent));
                else:
                    oFile.write(oFile.abContent);
            except:
                try:    oFile.close();
                except: pass;
                return reporter.errorXcpt('%s: write(%s bytes) failed' % (sPath, oFile.cbContent,));
            try:
                oFile.close();
            except:
                return reporter.errorXcpt('%s: close() failed' % (sPath,));

        return True;


    def chooseRandomFile(self):
        """
        Returns a random file.
        """
        return self.aoFiles[self.oRandom.choice(xrange(len(self.aoFiles)))];

    def chooseRandomDirFromTree(self, fLeaf = False, fNonEmpty = False):
        """
        Returns a random directory from the tree (self.oTreeDir).
        """
        while True:
            oDir = self.aoDirs[self.oRandom.choice(xrange(len(self.aoDirs)))];
            # Check fNonEmpty requirement:
            if not fNonEmpty or oDir.aoChildren:
                # Check leaf requirement:
                if not fLeaf:
                    for oChild in oDir.aoChildren:
                        if isinstance(oChild, TestDir):
                            continue; # skip it.

                # Return if in the tree:
                oParent = oDir.oParent;
                while oParent is not None:
                    if oParent is self.oTreeDir:
                        return oDir;
                    oParent = oParent.oParent;

#
# Unit testing.
#

# pylint: disable=missing-docstring
# pylint: disable=undefined-variable
class TestFileSetUnitTests(unittest.TestCase):
    def testGeneral(self):
        oSet = TestFileSet(False, '/tmp', 'unittest');
        self.assertTrue(isinstance(oSet.chooseRandomDirFromTree(), TestDir));
        self.assertTrue(isinstance(oSet.chooseRandomFile(), TestFile));


if __name__ == '__main__':
    unittest.main();
    # not reached.

