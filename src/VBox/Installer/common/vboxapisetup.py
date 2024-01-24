"""
Copyright (C) 2009-2023 Oracle and/or its affiliates.

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

#
# For "modern" Python packages setuptools only is one of many ways
# for installing a package on a system and acts as a pure build backend now.
# Using a build backend is controlled by the accompanied pyproject.toml file.
#
# See: https://packaging.python.org/en/latest/discussions/setup-py-deprecated/#setup-py-deprecated
#
# PEP 518 [1] introduced pyproject.toml.
# PEP 632 [2], starting with Python 3.10, the distutils module is marked as being deprecated.
# Python 3.12 does not ship with distutils anymore, but we have to still support Python < 3.12.
#
# [1] https://peps.python.org/pep-0518/
# [2] https://peps.python.org/pep-0632/

import atexit
import io
import os
import platform
import sys

g_fVerbose = True

def cleanupWinComCacheDir(sPath):
    """
    Cleans up a Windows COM cache directory by deleting it.
    """
    if not sPath:
        return
    import shutil
    sDirCache = os.path.join(sPath, 'win32com', 'gen_py')
    print("Cleaning COM cache at '%s'" % (sDirCache))
    shutil.rmtree(sDirCache, True)

def cleanupWinComCache():
    """
    Cleans up various Windows COM cache directories by deleting them.
    """
    if sys.version_info >= (3, 2): # Since Python 3.2 we use the site module.
        import site
        for sSiteDir in site.getsitepackages():
            cleanupWinComCacheDir(sSiteDir)
    else:
        from distutils.sysconfig import get_python_lib # pylint: disable=deprecated-module
        cleanupWinComCacheDir(get_python_lib())

    # @todo r=andy Do still need/want this? Was there forever. Probably a leftover.
    sDirTemp = os.path.join(os.environ.get("TEMP", "c:\\tmp"), 'gen_py')
    cleanupWinComCacheDir(sDirTemp)

def patchWith(sFile, sVBoxInstallPath, sVBoxSdkPath):
    """
    Patches a given file with the VirtualBox install path + SDK path.
    """
    sFileTemp = sFile + ".new"
    sVBoxInstallPath = sVBoxInstallPath.replace("\\", "\\\\")
    try:
        os.remove(sFileTemp)
    except Exception as _:
        pass
    # Note: We need io.open() to specify the file encoding on Python <= 2.7.
    try:
        with io.open(sFile, 'r', encoding='utf-8') as fileSrc:
            with io.open(sFileTemp, 'w', encoding='utf-8') as fileDst:
                for line in fileSrc:
                    line = line.replace("%VBOX_INSTALL_PATH%", sVBoxInstallPath)
                    line = line.replace("%VBOX_SDK_PATH%", sVBoxSdkPath)
                    fileDst.write(line)
                fileDst.close()
            fileSrc.close()
    except IOError as exc:
        print("ERROR: Opening VirtualBox Python source file '%s' failed: %s" % (sFile, exc))
        return False
    try:
        os.remove(sFile)
    except Exception as _:
        pass
    os.rename(sFileTemp, sFile)
    return True

def testVBoxAPI():
    """
    Performs various VirtualBox API tests.
    """

    # Give the user a hint where we gonna install stuff into.
    if  g_fVerbose \
    and sys.version_info.major >= 3:
        import site
        print("Global site packages directories are:")
        for sPath in site.getsitepackages():
            print("\t%s" % (sPath))
        print("User site packages directories are:")
        print("\t%s" % (site.getusersitepackages()))
        print("Module search path is:")
        for sPath in sys.path:
            print("\t%s" % (sPath))

    #
    # Test using the just installed VBox API module by calling some (simpler) APIs
    # where now kernel drivers are other fancy stuff is needed.
    #
    try:
        from vboxapi import VirtualBoxManager
        oVBoxMgr = VirtualBoxManager()
        oVBox    = oVBoxMgr.getVirtualBox()
        oHost    = oVBox.host
        if oHost.architecture not in (oVBoxMgr.constants.PlatformArchitecture_x86, \
                                      oVBoxMgr.constants.PlatformArchitecture_ARM):
            raise Exception('Host platform invalid!')
        print("Testing VirtualBox Python bindings successful: Detected VirtualBox %s (%d)" % (oVBox.version, oHost.architecture))
        _ = oVBox.getMachines()
        oVBoxMgr.deinit()
        del oVBoxMgr
    except ImportError as exc:
        print("ERROR: Testing VirtualBox Python bindings failed: %s" % (exc))
        return False

    print("Installation of VirtualBox Python bindings for Python %d.%d successful." \
          % (sys.version_info.major, sys.version_info.minor))
    return True

def findModulePathHelper(sModule = 'vboxapi', aDir = sys.path):
    """
    Helper function for findModulePath.

    Returns the path found, or None if not found.
    """
    for sPath in aDir:
        if g_fVerbose:
            print('Searching for "%s" in path "%s" ...' % (sModule, sPath))
        if os.path.isdir(sPath):
            aDirEntries = os.listdir(sPath)
            if g_fVerbose:
                print(aDirEntries)
            if sModule in aDirEntries:
                return os.path.join(sPath, sModule)
    return None

def findModulePath(sModule = 'vboxapi'):
    """
    Finds a module in the system path.

    Returns the path found, or None if not found.
    """
    sPath = findModulePathHelper(sModule)
    if not sPath:
        try:
            import site # Might not available everywhere.
            sPath = findModulePathHelper(sModule, site.getsitepackages())
        except:
            pass
    return sPath

try:
    from distutils.command.install import install # Only for < Python 3.12.
except:
    pass

class setupInstallClass(install):
    """
    Class which overrides the "install" command of the setup so that we can
    run post-install actions.
    """

    def run(self):
        def _post_install():
            if findModulePath():
                testVBoxAPI()
        atexit.register(_post_install)
        install.run(self)

def main():
    """
    Main function for the setup script.
    """

    print("Installing VirtualBox bindings for Python %d.%d ..." % (sys.version_info.major, sys.version_info.minor))

    # Deprecation warning for older Python stuff (< Python 3.x).
    if sys.version_info.major < 3:
        print("\nWarning: Running VirtualBox with Python %d.%d is marked as being deprecated.\n" \
              "Please upgrade your Python installation to avoid breakage.\n" \
              % (sys.version_info.major, sys.version_info.minor))

    sVBoxInstallPath = os.environ.get("VBOX_MSI_INSTALL_PATH", None)
    if sVBoxInstallPath is None:
        sVBoxInstallPath = os.environ.get('VBOX_INSTALL_PATH', None)
        if sVBoxInstallPath is None:
            print("No VBOX_INSTALL_PATH defined, exiting")
            return 1

    sVBoxVersion = os.environ.get("VBOX_VERSION", None)
    if sVBoxVersion is None:
        # Should we use VBox version for binding module versioning?
        sVBoxVersion = "1.0"

    if g_fVerbose:
        print("VirtualBox installation directory is: %s" % (sVBoxInstallPath))

    if platform.system() == 'Windows':
        cleanupWinComCache()

    # Make sure that we always are in the directory where this script resides.
    # Otherwise installing packages below won't work.
    sCurDir = os.path.dirname(os.path.abspath(__file__))
    if g_fVerbose:
        print("Current directory is: %s" % (sCurDir))
    try:
        os.chdir(sCurDir)
    except OSError as exc:
        print("Changing to current directory failed: %s" % (exc))

    # Darwin: Patched before installation. Modifying bundle is not allowed, breaks signing and upsets gatekeeper.
    if platform.system() != 'Darwin':
        # @todo r=andy This *will* break the script if VirtualBox installation files will be moved.
        #              Better would be patching the *installed* module instead of the original module.
        sVBoxSdkPath = os.path.join(sVBoxInstallPath, "sdk")
        fRc = patchWith(os.path.join(sCurDir, 'src', 'vboxapi', '__init__.py'), \
                        sVBoxInstallPath, sVBoxSdkPath)
        if not fRc:
            return 1

    try:
        #
        # Detect which installation method is being used.
        #
        # This is a bit messy due the fact that we want to support a broad range of older and newer
        # Python versions, along with distributions which maintain their own Python packages (e.g. newer Ubuntus).
        #
        fInvokeSetupTools = False
        if sys.version_info >= (3, 12): # Since Python 3.12 there are no distutils anymore. See PEP632.
            try:
                from setuptools import setup
            except ImportError:
                print("ERROR: setuptools package not installed, can't continue. Exiting.")
                return 1
            setup(cmdclass={"install": setupInstallClass})
        else:
            if sys.version_info >= (3, 3): # Starting with Python 3.3 we use pyproject.toml by using setup().
                try:
                    from distutils.core import setup # pylint: disable=deprecated-module
                    setup(cmdclass={"install": setupInstallClass})
                except ImportError:
                    print("distutils[.core] package not installed/available, falling back to legacy setuptools ...")
                    fInvokeSetupTools = True # Invoke setuptools as a last resort.
            else: # Python 2.7.x + Python < 3.6 legacy cruft.
                fInvokeSetupTools = True

            if fInvokeSetupTools:
                if g_fVerbose:
                    print("Invoking setuptools directly ...")
                setupTool = setup(name='vboxapi',
                            version=sVBoxVersion,
                            description='Python interface to VirtualBox',
                            author='Oracle Corp.',
                            author_email='vbox-dev@virtualbox.org',
                            url='https://www.virtualbox.org',
                            package_dir={'': 'src'},
                            packages=['vboxapi'])
                if setupTool:
                    sPathInstalled = setupTool.command_obj['install'].install_lib
                    if sPathInstalled not in sys.path:
                        print("\nWARNING: Installation path is not in current module search path!")
                        print("         This might happen on OSes / distributions which only maintain packages by")
                        print("         a vendor-specific method.")
                        print("Hints:")
                        print("- Check how the distribution handles user-installable Python packages.")
                        print("- Using setuptools directly might be deprecated on the distribution.")
                        print("- Using \"pip install ./vboxapi\" within a virtual environment (virtualenv)")
                        print("  might fix this.\n")
                        sys.path.append(sPathInstalled)

                print("Installed to: %s" % (sPathInstalled))

    except RuntimeError as exc:
        print("ERROR: Installation of VirtualBox Python bindings failed: %s" % (exc))
        return 1

    if  fInvokeSetupTools \
    and not testVBoxAPI():
        return 1
    return 0

if __name__ == '__main__':
    sys.exit(main())
