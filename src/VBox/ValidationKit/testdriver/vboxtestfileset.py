# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=too-many-lines

"""
VirtualBox Test File Set
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

# Validation Kit imports.
from common     import utils;
from testdriver import reporter;

# Python 3 hacks:
if sys.version_info[0] >= 3:
    xrange = range; # pylint: disable=redefined-builtin,invalid-name


class TestFileSet(object):
    """
    Generates a set of files and directories for use in a test.

    Uploaded as a tarball and expanded via TXS (if new enough) or uploaded vts_tar
    utility from the validation kit.
    """

    class GstFsObj(object):
        """ A file system object we created in the guest for test purposes. """
        def __init__(self, oParent, sPath):
            self.oParent   = oParent    # type: GstDir
            self.sPath     = sPath      # type: str
            self.sName     = sPath      # type: str
            if oParent:
                assert sPath.startswith(oParent.sPath);
                self.sName = sPath[len(oParent.sPath) + 1:];
                # Add to parent.
                oParent.aoChildren.append(self);
                oParent.dChildrenUpper[self.sName.upper()] = self;

    class GstFile(GstFsObj):
        """ A file object in the guest. """
        def __init__(self, oParent, sPath, abContent):
            TestFileSet.GstFsObj.__init__(self, oParent, sPath);
            self.abContent = abContent          # type: bytearray
            self.cbContent = len(abContent);
            self.off       = 0;

        def read(self, cbToRead):
            assert self.off <= self.cbContent;
            cbLeft = self.cbContent - self.off;
            if cbLeft < cbToRead:
                cbToRead = cbLeft;
            abRet = self.abContent[self.off:(self.off + cbToRead)];
            assert len(abRet) == cbToRead;
            self.off += cbToRead;
            return abRet;

    class GstDir(GstFsObj):
        """ A file object in the guest. """
        def __init__(self, oParent, sPath):
            TestFileSet.GstFsObj.__init__(self, oParent, sPath);
            self.aoChildren     = []  # type: list(GsFsObj)
            self.dChildrenUpper = {}  # type: dict(str,GsFsObj)

        def contains(self, sName):
            """ Checks if the directory contains the given name. """
            return sName.upper() in self.dChildrenUpper


    ksReservedWinOS2         = '/\\"*:<>?|\t\v\n\r\f\a\b';
    ksReservedUnix           = '/';
    ksReservedTrailingWinOS2 = ' .';
    ksReservedTrailingUnix   = '';

    def __init__(self, oTestVm, sBasePath, sSubDir, # pylint: disable=too-many-arguments
                 oRngFileSizes = xrange(0, 16384),
                 oRngManyFiles = xrange(128, 512),
                 oRngTreeFiles = xrange(128, 384),
                 oRngTreeDepth = xrange(92, 256),
                 oRngTreeDirs  = xrange(2, 16),
                 cchMaxPath    = 230,
                 cchMaxName    = 230):
        ## @name Parameters
        ## @{
        self.oTestVm       = oTestVm;
        self.sBasePath     = sBasePath;
        self.sSubDir       = sSubDir;
        self.oRngFileSizes = oRngFileSizes;
        self.oRngManyFiles = oRngManyFiles;
        self.oRngTreeFiles = oRngTreeFiles;
        self.oRngTreeDepth = oRngTreeDepth;
        self.oRngTreeDirs  = oRngTreeDirs;
        self.cchMaxPath    = cchMaxPath;
        self.cchMaxName    = cchMaxName
        ## @}

        ## @name Charset stuff
        ## @todo allow more chars for unix hosts + guests.
        ## @{
        ## The filename charset.
        self.sFileCharset             = string.printable;
        ## The filename charset common to host and guest.
        self.sFileCharsetCommon       = string.printable;
        ## Set of characters that should not trail a guest filename.
        self.sReservedTrailing        = self.ksReservedTrailingWinOS2;
        ## Set of characters that should not trail a host or guest filename.
        self.sReservedTrailingCommon  = self.ksReservedTrailingWinOS2;
        if oTestVm.isWindows() or oTestVm.isOS2():
            for ch in self.ksReservedWinOS2:
                self.sFileCharset     = self.sFileCharset.replace(ch, '');
        else:
            self.sReservedTrailing    = self.ksReservedTrailingUnix;
            for ch in self.ksReservedUnix:
                self.sFileCharset     = self.sFileCharset.replace(ch, '');
        self.sFileCharset            += '   ...';

        for ch in self.ksReservedWinOS2:
            self.sFileCharsetCommon   = self.sFileCharset.replace(ch, '');
        self.sFileCharsetCommon      += '   ...';
        ## @}

        ## The root directory.
        self.oRoot      = None      # type: GstDir;
        ## An empty directory (under root).
        self.oEmptyDir  = None      # type: GstDir;

        ## A directory with a lot of files in it.
        self.oManyDir   = None      # type: GstDir;

        ## A directory with a mixed tree structure under it.
        self.oTreeDir   = None      # type: GstDir;
        ## Number of files in oTreeDir.
        self.cTreeFiles = 0;
        ## Number of directories under oTreeDir.
        self.cTreeDirs  = 0;

        ## All directories in creation order.
        self.aoDirs     = []        # type: list(GstDir);
        ## All files in creation order.
        self.aoFiles    = []        # type: list(GstFile);
        ## Path to object lookup.
        self.dPaths     = {}        # type: dict(str, GstFsObj);

        #
        # Do the creating.
        #
        self.uSeed   = utils.timestampMilli();
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

    def generateFilenameEx(self, cchMax = -1, cchMin = -1, fCommonCharset = True):
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

        if fCommonCharset:
            sCharset            = self.sFileCharsetCommon;
            sReservedTrailing   = self.sReservedTrailingCommon;
        else:
            sCharset            = self.sFileCharset;
            sReservedTrailing   = self.sReservedTrailing;

        while True:
            cchName = self.oRandom.randrange(cchMin, cchMax + 1);
            sName = ''.join(self.oRandom.choice(sCharset) for _ in xrange(cchName));
            if sName[-1] not in sReservedTrailing:
                if sName not in ('.', '..',):
                    return sName;
        return ''; # never reached, but makes pylint happy.

    def __createTestDir(self, oParent, sDir):
        """
        Creates a test directory.
        """
        oDir = TestFileSet.GstDir(oParent, sDir);
        self.aoDirs.append(oDir);
        self.dPaths[sDir] = oDir;
        return oDir;

    def __createTestFile(self, oParent, sFile):
        """
        Creates a test file with random size up to cbMaxContent and random content.
        """
        cbFile = self.oRandom.choice(self.oRngFileSizes);
        abContent = bytearray(self.oRandom.getrandbits(8) for _ in xrange(cbFile));

        oFile = TestFileSet.GstFile(oParent, sFile, abContent);
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
        sRoot = self.oTestVm.pathJoin(self.sBasePath, self.sSubDir);
        self.oRoot     = self.__createTestDir(None, sRoot);
        self.oEmptyDir = self.__createTestDir(self.oRoot, self.oTestVm.pathJoin(sRoot, 'empty'));

        #
        # Create a directory with about files in it using the guest specific charset:
        #
        oDir = self.__createTestDir(self.oRoot, self.oTestVm.pathJoin(sRoot, 'many'));
        self.oManyDir = oDir;
        cManyFiles = self.oRandom.choice(self.oRngManyFiles);
        for _ in xrange(cManyFiles):
            sName = self.__createFilename(oDir, self.sFileCharset, self.sReservedTrailing);
            self.__createTestFile(oDir, self.oTestVm.pathJoin(oDir.sPath, sName));

        #
        # Generate a tree of files and dirs. Portable character set.
        #
        oDir = self.__createTestDir(self.oRoot, self.oTestVm.pathJoin(sRoot, 'tree'));
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
                sName = self.__createFilename(oDir, self.sFileCharsetCommon, self.sReservedTrailingCommon);
                self.__createTestFile(oDir, self.oTestVm.pathJoin(oDir.sPath, sName));
                self.cTreeFiles += 1;
            # 1: Add a subdirector and descend into it:
            elif iAction == 1 and self.cTreeDirs < cMaxDirs and uDepth < uMaxDepth and len(oDir.sPath) < 220:
                sName = self.__createFilename(oDir, self.sFileCharsetCommon, self.sReservedTrailingCommon);
                oDir  = self.__createTestDir(oDir, self.oTestVm.pathJoin(oDir.sPath, sName));
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

        chOtherSlash = '\\' if self.oTestVm.isWindows() or self.oTestVm.isOS2() else '/';
        cchSkip      = len(self.sBasePath) + 1;

        # Open the tarball:
        try:
            oTarFile = tarfile.open(sTarFileHst, 'w:gz');
        except:
            return reporter.errorXcpt('Failed to open new tar file: %s' % (sTarFileHst,));

        # Directories:
        for oDir in self.aoDirs:
            oTarInfo = tarfile.TarInfo(oDir.sPath[cchSkip:].replace(chOtherSlash, '/') + '/');
            oTarInfo.mode = 0o777;
            oTarInfo.type = tarfile.DIRTYPE;
            try:
                oTarFile.addfile(oTarInfo);
            except:
                return reporter.errorXcpt('Failed adding directory tarfile: %s' % (oDir.sPath,));

        # Files:
        for oFile in self.aoFiles:
            oTarInfo = tarfile.TarInfo(oFile.sPath[cchSkip:].replace(chOtherSlash, '/'));
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

    def __uploadFallback(self, oTxsSession, sTarFileGst, oTstDrv):
        """
        Fallback upload method.
        """

        ## Directories:
        #for oDir in self.aoDirs:
        #    if oTxsSession.syncMkDirPath(oDir.sPath, 0o777) is not True:
        #        return reporter.error('Failed to create directory "%s"!' % (oDir.sPath,));
        #
        ## Files:
        #for oFile in self.aoFiles:
        #    if oTxsSession.syncUploadString(oFile.abContent, oFile.sPath) is not True:
        #        return reporter.error('Failed to create file "%s" with %s content bytes!' % (oFile.sPath, oFile.cbContent));

        sVtsTarExe = 'vts_tar' + self.oTestVm.getGuestExeSuff();
        sVtsTarHst = os.path.join(oTstDrv.sVBoxValidationKit, self.oTestVm.getGuestOs(),
                                  self.oTestVm.getGuestArch(), sVtsTarExe);
        sVtsTarGst = self.oTestVm.pathJoin(self.sBasePath, sVtsTarExe);

        if oTxsSession.syncUploadFile(sVtsTarHst, sVtsTarGst) is not True:
            return reporter.error('Failed to upload "%s" to the guest as "%s"!' % (sVtsTarHst, sVtsTarGst,));

        fRc = oTxsSession.syncExec(sVtsTarGst, [sVtsTarGst, '-xzf', sTarFileGst, '-C', self.sBasePath,], fWithTestPipe = False);
        if fRc is not True:
            return reporter.error('vts_tar failed!');
        return True;

    def upload(self, oTxsSession, oTstDrv):
        """
        Uploads the files into the guest via the given TXS session.

        Returns True / False.
        """

        #
        # Create a tarball.
        #
        sTarFileHst = os.path.join(oTstDrv.sScratchPath, 'tdAddGuestCtrl-1-Stuff.tar.gz');
        sTarFileGst = self.oTestVm.pathJoin(self.sBasePath, 'tdAddGuestCtrl-1-Stuff.tar.gz');
        if self.createTarball(sTarFileHst) is not True:
            return False;

        #
        # Upload it.
        #
        reporter.log('Uploading tarball "%s" to the guest as "%s"...' % (sTarFileHst, sTarFileGst));
        if oTxsSession.syncUploadFile(sTarFileHst, sTarFileGst) is not True:
            return reporter.error('Failed upload tarball "%s" as "%s"!' % (sTarFileHst, sTarFileGst,));

        #
        # Try unpack it.
        #
        reporter.log('Unpacking "%s" into "%s"...' % (sTarFileGst, self.sBasePath));
        if oTxsSession.syncUnpackFile(sTarFileGst, self.sBasePath, fIgnoreErrors = True) is not True:
            reporter.log('Failed to expand tarball "%s" into "%s", falling back on individual directory and file creation...'
                         % (sTarFileGst, self.sBasePath,));
            if self.__uploadFallback(oTxsSession, sTarFileGst, oTstDrv) is not True:
                return False;
        reporter.log('Successfully placed test files and directories in the VM.');
        return True;


