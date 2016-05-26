# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager Web-UI - Form Helpers.
"""

__copyright__ = \
"""
Copyright (C) 2012-2015 Oracle Corporation

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

# Standard python imports.
import copy;

# Validation Kit imports.
from common                         import utils;
from common.webutils                import escapeAttr, escapeElem;
from testmanager                    import config;
from testmanager.core.schedgroup    import SchedGroupMemberData, SchedGroupDataEx;
from testmanager.core.testcaseargs  import TestCaseArgsData;
from testmanager.core.testgroup     import TestGroupMemberData, TestGroupDataEx;


class WuiHlpForm(object):
    """
    Helper for constructing a form.
    """

    ksItemsList = 'ksItemsList'

    ksOnSubmit_AddReturnToFieldWithCurrentUrl = '+AddReturnToFieldWithCurrentUrl+';

    def __init__(self, sId, sAction, dErrors = None, fReadOnly = False, sOnSubmit = None):
        self._fFinalized = False;
        self._fReadOnly  = fReadOnly;
        self._dErrors    = dErrors if dErrors is not None else dict();

        if sOnSubmit == self.ksOnSubmit_AddReturnToFieldWithCurrentUrl:
            sOnSubmit = 'return addRedirectToInputFieldWithCurrentUrl(this)';
        if sOnSubmit is None:   sOnSubmit = u'';
        else:                   sOnSubmit = u' onsubmit=\"%s\"' % (escapeAttr(sOnSubmit),);

        self._sBody      = u'\n' \
                           u'<div id="%s" class="tmform">\n' \
                           u'  <form action="%s" method="post"%s>\n' \
                           u'    <ul>\n' \
                         % (sId, sAction, sOnSubmit);

    def _add(self, sText):
        """Internal worker for appending text to the body."""
        assert not self._fFinalized;
        if not self._fFinalized:
            self._sBody += unicode(sText, errors='ignore') if isinstance(sText, str) else sText;
            return True;
        return False;

    def _escapeErrorText(self, sText):
        """Escapes error text, preserving some predefined HTML tags."""
        if sText.find('<br>') >= 0:
            asParts = sText.split('<br>');
            for i, _ in enumerate(asParts):
                asParts[i] = escapeElem(asParts[i].strip());
            sText = '<br>\n'.join(asParts);
        else:
            sText = escapeElem(sText);
        return sText;

    def _addLabel(self, sName, sLabel, sDivSubClass = 'normal'):
        """Internal worker for adding a label."""
        if sName in self._dErrors:
            sError = self._dErrors[sName];
            if utils.isString(sError):          # List error trick (it's an associative array).
                return self._add('      <li>\n'
                                 '        <div class="tmform-field"><div class="tmform-field-%s">\n'
                                 '          <label for="%s" class="tmform-error-label">%s\n'
                                 '              <span class="tmform-error-desc">%s</span>\n'
                                 '          </label>\n'
                                 % (escapeAttr(sDivSubClass), escapeAttr(sName), escapeElem(sLabel),
                                    self._escapeErrorText(sError), ) );
        return self._add('      <li>\n'
                         '        <div class="tmform-field"><div class="tmform-field-%s">\n'
                         '          <label  for="%s">%s</label>\n'
                         % (escapeAttr(sDivSubClass), escapeAttr(sName), escapeElem(sLabel)) );


    def finalize(self):
        """
        Finalizes the form and returns the body.
        """
        if not self._fFinalized:
            self._add('    </ul>\n'
                      '  </form>\n'
                      '</div>\n'
                      '<div class="clear"></div>\n'
                     );
        return self._sBody;

    def addTextHidden(self, sName, sValue, sExtraAttribs = ''):
        """Adds a hidden text input."""
        return self._add('      <div class="tmform-field-hidden">\n'
                         '        <input name="%s" id="%s" type="text" hidden%s value="%s" class="tmform-hidden">\n'
                         '      </div>\n'
                         '    </li>\n'
                         % ( escapeAttr(sName), escapeAttr(sName), sExtraAttribs, escapeElem(str(sValue)) ));
    #
    # Non-input stuff.
    #
    def addNonText(self, sValue, sLabel, sPostHtml = ''):
        """Adds a read-only text input."""
        self._addLabel('non-text', sLabel, 'string');
        if sValue is None: sValue = '';
        return self._add('          <p>%s</p>%s\n'
                         '        </div></div>\n'
                         '      </li>\n'
                         % (escapeElem(str(sValue)), sPostHtml ));


    #
    # Text input fields.
    #
    def addText(self, sName, sValue, sLabel, sSubClass = 'string', sExtraAttribs = '', sPostHtml = ''):
        """Adds a text input."""
        if self._fReadOnly:
            return self.addTextRO(sName, sValue, sLabel, sSubClass, sExtraAttribs);
        if sSubClass not in ('int', 'long', 'string', 'uuid', 'timestamp', 'wide'): raise Exception(sSubClass);
        self._addLabel(sName, sLabel, sSubClass);
        if sValue is None: sValue = '';
        return self._add('          <input name="%s" id="%s" type="text"%s value="%s">%s\n'
                         '        </div></div>\n'
                         '      </li>\n'
                         % ( escapeAttr(sName), escapeAttr(sName), sExtraAttribs, escapeElem(sValue), sPostHtml ));

    def addTextRO(self, sName, sValue, sLabel, sSubClass = 'string', sExtraAttribs = '', sPostHtml = ''):
        """Adds a read-only text input."""
        if sSubClass not in ('int', 'long', 'string', 'uuid', 'timestamp', 'wide'): raise Exception(sSubClass);
        self._addLabel(sName, sLabel, sSubClass);
        if sValue is None: sValue = '';
        return self._add('          <input name="%s" id="%s" type="text" readonly%s value="%s" class="tmform-input-readonly">%s\n'
                         '        </div></div>\n'
                         '      </li>\n'
                         % ( escapeAttr(sName), escapeAttr(sName), sExtraAttribs, escapeElem(str(sValue)), sPostHtml ));

    def addWideText(self, sName, sValue, sLabel, sExtraAttribs = ''):
        """Adds a wide text input."""
        return self.addText(sName, sValue, sLabel, 'wide', sExtraAttribs);

    def addWideTextRO(self, sName, sValue, sLabel, sExtraAttribs = ''):
        """Adds a wide read-only text input."""
        return self.addTextRO(sName, sValue, sLabel, 'wide', sExtraAttribs);


    def addMultilineText(self, sName, sValue, sLabel, sSubClass = 'string', sExtraAttribs = ''):
        """Adds a multiline text input."""
        if self._fReadOnly:
            return self.addMultilineTextRO(sName, sValue, sLabel, sSubClass, sExtraAttribs);
        if sSubClass not in ('int', 'long', 'string', 'uuid', 'timestamp'): raise Exception(sSubClass)
        self._addLabel(sName, sLabel, sSubClass)
        if sValue is None: sValue = '';
        sNewValue = str(sValue) if not isinstance(sValue, list) else '\n'.join(sValue)
        return self._add('          <textarea name="%s" id="%s" %s>%s</textarea>\n'
                         '        </div></div>\n'
                         '      </li>\n'
                         % ( escapeAttr(sName), escapeAttr(sName), sExtraAttribs, escapeElem(sNewValue)))

    def addMultilineTextRO(self, sName, sValue, sLabel, sSubClass = 'string', sExtraAttribs = ''):
        """Adds a multiline read-only text input."""
        if sSubClass not in ('int', 'long', 'string', 'uuid', 'timestamp'): raise Exception(sSubClass)
        self._addLabel(sName, sLabel, sSubClass)
        if sValue is None: sValue = '';
        sNewValue = str(sValue) if not isinstance(sValue, list) else '\n'.join(sValue)
        return self._add('          <textarea name="%s" id="%s" readonly %s>%s</textarea>\n'
                         '        </div></div>\n'
                         '      </li>\n'
                         % ( escapeAttr(sName), escapeAttr(sName), sExtraAttribs, escapeElem(sNewValue)))

    def addInt(self, sName, iValue, sLabel, sExtraAttribs = ''):
        """Adds an integer input."""
        return self.addText(sName, str(iValue), sLabel, 'int', sExtraAttribs);

    def addIntRO(self, sName, iValue, sLabel, sExtraAttribs = ''):
        """Adds an integer input."""
        return self.addTextRO(sName, str(iValue), sLabel, 'int', sExtraAttribs);

    def addLong(self, sName, lValue, sLabel, sExtraAttribs = ''):
        """Adds a long input."""
        return self.addText(sName, str(lValue), sLabel, 'long', sExtraAttribs);

    def addLongRO(self, sName, lValue, sLabel, sExtraAttribs = ''):
        """Adds a long input."""
        return self.addTextRO(sName, str(lValue), sLabel, 'long', sExtraAttribs);

    def addUuid(self, sName, uuidValue, sLabel, sExtraAttribs = ''):
        """Adds an UUID input."""
        return self.addText(sName, str(uuidValue), sLabel, 'uuid', sExtraAttribs);

    def addUuidRO(self, sName, uuidValue, sLabel, sExtraAttribs = ''):
        """Adds a read-only UUID input."""
        return self.addTextRO(sName, str(uuidValue), sLabel, 'uuid', sExtraAttribs);

    def addTimestampRO(self, sName, sTimestamp, sLabel, sExtraAttribs = ''):
        """Adds a read-only database string timstamp input."""
        return self.addTextRO(sName, sTimestamp, sLabel, 'timestamp', sExtraAttribs);


    #
    # Text areas.
    #


    #
    # Combo boxes.
    #
    def addComboBox(self, sName, sSelected, sLabel, aoOptions, sExtraAttribs = ''):
        """Adds a combo box."""
        if self._fReadOnly:
            return self.addComboBoxRO(sName, sSelected, sLabel, aoOptions, sExtraAttribs);
        self._addLabel(sName, sLabel, 'combobox');
        self._add('          <select name="%s" id="%s" class="tmform-combobox"%s>\n'
                  % (escapeAttr(sName), escapeAttr(sName), sExtraAttribs));
        sSelected = str(sSelected);
        for iValue, sText, _ in aoOptions:
            sValue = str(iValue);
            self._add('            <option value="%s"%s>%s</option>\n'
                      % (escapeAttr(sValue), ' selected' if sValue == sSelected else '',
                         escapeElem(sText)));
        return self._add('          </select>\n'
                         '        </div></div>\n'
                         '      </li>\n'
                        );

    def addComboBoxRO(self, sName, sSelected, sLabel, aoOptions, sExtraAttribs = ''):
        """Adds a read-only combo box."""
        self.addTextHidden(sName, sSelected);
        self._addLabel(sName, sLabel, 'combobox-readonly');
        self._add('          <select name="%s" id="%s" disabled class="tmform-combobox"%s>\n'
                  % (escapeAttr(sName), escapeAttr(sName), sExtraAttribs));
        sSelected = str(sSelected);
        for iValue, sText, _ in aoOptions:
            sValue = str(iValue);
            self._add('            <option value="%s"%s>%s</option>\n'
                      % (escapeAttr(sValue), ' selected' if sValue == sSelected else '',
                         escapeElem(sText)));
        return self._add('          </select>\n'
                         '        </div></div>\n'
                         '      </li>\n'
                        );

    #
    # Check boxes.
    #
    @staticmethod
    def _reinterpretBool(fValue):
        """Reinterprets a value as a boolean type."""
        if fValue is not type(True):
            if fValue is None:
                fValue = False;
            elif str(fValue) in ('True', 'true', '1'):
                fValue = True;
            else:
                fValue = False;
        return fValue;

    def addCheckBox(self, sName, fChecked, sLabel, sExtraAttribs = ''):
        """Adds an check box."""
        if self._fReadOnly:
            return self.addCheckBoxRO(sName, fChecked, sLabel, sExtraAttribs);
        self._addLabel(sName, sLabel, 'checkbox');
        fChecked = self._reinterpretBool(fChecked);
        return self._add('          <input name="%s" id="%s" type="checkbox"%s%s value="1" class="tmform-checkbox">\n'
                         '        </div></div>\n'
                         '      </li>\n'
                         % (escapeAttr(sName), escapeAttr(sName), ' checked' if fChecked else '', sExtraAttribs));

    def addCheckBoxRO(self, sName, fChecked, sLabel, sExtraAttribs = ''):
        """Adds an readonly check box."""
        self._addLabel(sName, sLabel, 'checkbox');
        fChecked = self._reinterpretBool(fChecked);
        # Hack Alert! The onclick and onkeydown are for preventing editing and fake readonly/disabled.
        return self._add('          <input name="%s" id="%s" type="checkbox"%s readonly%s value="1" class="readonly"\n'
                         '              onclick="return false" onkeydown="return false">\n'
                         '        </div></div>\n'
                         '      </li>\n'
                         % (escapeAttr(sName), escapeAttr(sName), ' checked' if fChecked else '', sExtraAttribs));

    #
    # List of items to check
    #
    def _addList(self, sName, aoRows, sLabel, fUseTable = False, sId = 'dummy', sExtraAttribs = ''):
        """
        Adds a list of items to check.

        @param sName  Name of HTML form element
        @param aoRows  List of [sValue, fChecked, sName] sub-arrays.
        @param sLabel Label of HTML form element
        """
        fReadOnly = self._fReadOnly; ## @todo add this as a parameter.
        if fReadOnly:
            sExtraAttribs += ' readonly onclick="return false" onkeydown="return false"';

        self._addLabel(sName, sLabel, 'list');
        if len(aoRows) == 0:
            return self._add('No items</div></div></li>')
        sNameEscaped = escapeAttr(sName);

        self._add('          <div class="tmform-checkboxes-container" id="%s">\n' % (escapeAttr(sId),));
        if fUseTable:
            self._add('          <table>\n');
            for asRow in aoRows:
                assert len(asRow) == 3; # Don't allow sloppy input data!
                fChecked = self._reinterpretBool(asRow[1])
                self._add('            <tr>\n'
                          '              <td><input type="checkbox" name="%s" value="%s"%s%s></td>\n'
                          '              <td>%s</td>\n'
                          '            </tr>\n'
                          % ( sNameEscaped, escapeAttr(str(asRow[0])), ' checked' if fChecked else '', sExtraAttribs,
                              escapeElem(str(asRow[2])), ));
            self._add('          </table>\n');
        else:
            for asRow in aoRows:
                assert len(asRow) == 3; # Don't allow sloppy input data!
                fChecked = self._reinterpretBool(asRow[1])
                self._add('            <div class="tmform-checkbox-holder">'
                          '<input type="checkbox" name="%s" value="%s"%s%s> %s</input></div>\n'
                          % ( sNameEscaped, escapeAttr(str(asRow[0])), ' checked' if fChecked else '', sExtraAttribs,
                              escapeElem(str(asRow[2])),));
        return self._add('        </div></div></div>\n'
                         '      </li>\n');


    def addListOfOsArches(self, sName, aoOsArches, sLabel, sExtraAttribs = ''):
        """
        List of checkboxes for OS/ARCH selection.
        asOsArches is a list of [sValue, fChecked, sName] sub-arrays.
        """
        return self._addList(sName, aoOsArches, sLabel, fUseTable = False, sId = 'tmform-checkbox-list-os-arches',
                             sExtraAttribs = sExtraAttribs);

    def addListOfTypes(self, sName, aoTypes, sLabel, sExtraAttribs = ''):
        """
        List of checkboxes for build type selection.
        aoTypes is a list of [sValue, fChecked, sName] sub-arrays.
        """
        return self._addList(sName, aoTypes, sLabel, fUseTable = False, sId = 'tmform-checkbox-list-build-types',
                             sExtraAttribs = sExtraAttribs);

    def addListOfTestCases(self, sName, aoTestCases, sLabel, sExtraAttribs = ''):
        """
        List of checkboxes for test box (dependency) selection.
        aoTestCases is a list of [sValue, fChecked, sName] sub-arrays.
        """
        return self._addList(sName, aoTestCases, sLabel, fUseTable = False, sId = 'tmform-checkbox-list-testcases',
                             sExtraAttribs = sExtraAttribs);

    def addListOfResources(self, sName, aoTestCases, sLabel, sExtraAttribs = ''):
        """
        List of checkboxes for resource selection.
        aoTestCases is a list of [sValue, fChecked, sName] sub-arrays.
        """
        return self._addList(sName, aoTestCases, sLabel, fUseTable = False, sId = 'tmform-checkbox-list-resources',
                             sExtraAttribs = sExtraAttribs);

    def addListOfTestGroups(self, sName, aoTestGroups, sLabel, sExtraAttribs = ''):
        """
        List of checkboxes for test group selection.
        aoTestGroups is a list of [sValue, fChecked, sName] sub-arrays.
        """
        return self._addList(sName, aoTestGroups, sLabel, fUseTable = False, sId = 'tmform-checkbox-list-testgroups',
                             sExtraAttribs = sExtraAttribs);

    def addListOfTestCaseArgs(self, sName, aoVariations, sLabel): # pylint: disable=R0915
        """
        Adds a list of test case argument variations to the form.

        @param sName        Name of HTML form element
        @param aoVariations List of TestCaseArgsData instances.
        @param sLabel       Label of HTML form element
        """
        self._addLabel(sName, sLabel);

        sTableId = 'TestArgsExtendingListRoot';
        fReadOnly = self._fReadOnly;  ## @todo argument?
        sReadOnlyAttr = ' readonly class="tmform-input-readonly"' if fReadOnly else '';

        sHtml  = '<li>\n'

        #
        # Define javascript function for extending the list of test case
        # variations.  Doing it here so we can use the python constants. This
        # also permits multiple argument lists on one page should that ever be
        # required...
        #
        if not fReadOnly:
            sHtml += '<script type="text/javascript">\n'
            sHtml += '\n';
            sHtml += 'g_%s_aItems = { %s };\n' % (sName, ', '.join(('%s: 1' % (i,)) for i in range(len(aoVariations))),);
            sHtml += 'g_%s_cItems = %s;\n' % (sName, len(aoVariations),);
            sHtml += 'g_%s_iIdMod = %s;\n' % (sName, len(aoVariations) + 32);
            sHtml += '\n';
            sHtml += 'function %s_removeEntry(sId)\n' % (sName,);
            sHtml += '{\n';
            sHtml += '    if (g_%s_cItems > 1)\n' % (sName,);
            sHtml += '    {\n';
            sHtml += '        g_%s_cItems--;\n' % (sName,);
            sHtml += '        delete g_%s_aItems[sId];\n' % (sName,);
            sHtml += '        setElementValueToKeyList(\'%s\', g_%s_aItems);\n' % (sName, sName);
            sHtml += '\n';
            for iInput in range(8):
                sHtml += '        removeHtmlNode(\'%s[\' + sId + \'][%s]\');\n' % (sName, iInput,);
            sHtml += '    }\n';
            sHtml += '}\n';
            sHtml += '\n';
            sHtml += 'function %s_extendListEx(cGangMembers, cSecTimeout, sArgs, sTestBoxReqExpr, sBuildReqExpr)\n' % (sName,);
            sHtml += '{\n';
            sHtml += '    var oElement = document.getElementById(\'%s\');\n' % (sTableId,);
            sHtml += '    var oTBody   = document.createElement(\'tbody\');\n';
            sHtml += '    var sHtml    = \'\';\n';
            sHtml += '    var sId;\n';
            sHtml += '\n';
            sHtml += '    g_%s_iIdMod += 1;\n' % (sName,);
            sHtml += '    sId = g_%s_iIdMod.toString();\n' % (sName,);

            oVarDefaults = TestCaseArgsData();
            oVarDefaults.convertToParamNull();
            sHtml += '\n';
            sHtml += '    sHtml += \'<tr class="tmform-testcasevars-first-row">\';\n';
            sHtml += '    sHtml += \'  <td>Gang Members:</td>\';\n';
            sHtml += '    sHtml += \'  <td class="tmform-field-tiny-int">' \
                     '<input name="%s[\' + sId + \'][%s]" id="%s[\' + sId + \'][0]" value="\' + cGangMembers + \'"></td>\';\n' \
                   % (sName, TestCaseArgsData.ksParam_cGangMembers, sName,);
            sHtml += '    sHtml += \'  <td>Timeout:</td>\';\n';
            sHtml += '    sHtml += \'  <td class="tmform-field-int">' \
                     '<input name="%s[\' + sId + \'][%s]" id="%s[\' + sId + \'][1]" value="\'+ cSecTimeout + \'"></td>\';\n' \
                   % (sName, TestCaseArgsData.ksParam_cSecTimeout, sName,);
            sHtml += '    sHtml += \'  <td><a href="#" onclick="%s_removeEntry(\\\'\' + sId + \'\\\');"> Remove</a></td>\';\n' \
                   % (sName, );
            sHtml += '    sHtml += \'  <td></td>\';\n';
            sHtml += '    sHtml += \'</tr>\';\n'
            sHtml += '\n';
            sHtml += '    sHtml += \'<tr class="tmform-testcasevars-inner-row">\';\n';
            sHtml += '    sHtml += \'  <td>Arguments:</td>\';\n';
            sHtml += '    sHtml += \'  <td class="tmform-field-wide100" colspan="4">' \
                     '<input name="%s[\' + sId + \'][%s]" id="%s[\' + sId + \'][2]" value="\' + sArgs + \'"></td>\';\n' \
                   % (sName, TestCaseArgsData.ksParam_sArgs, sName,);
            sHtml += '    sHtml += \'  <td></td>\';\n';
            sHtml += '    sHtml += \'</tr>\';\n'
            sHtml += '\n';
            sHtml += '    sHtml += \'<tr class="tmform-testcasevars-inner-row">\';\n';
            sHtml += '    sHtml += \'  <td>TestBox Reqs:</td>\';\n';
            sHtml += '    sHtml += \'  <td class="tmform-field-wide100" colspan="4">' \
                     '<input name="%s[\' + sId + \'][%s]" id="%s[\' + sId + \'][2]" value="\' + sTestBoxReqExpr' \
                     ' + \'"></td>\';\n' \
                   % (sName, TestCaseArgsData.ksParam_sTestBoxReqExpr, sName,);
            sHtml += '    sHtml += \'  <td></td>\';\n';
            sHtml += '    sHtml += \'</tr>\';\n'
            sHtml += '\n';
            sHtml += '    sHtml += \'<tr class="tmform-testcasevars-final-row">\';\n';
            sHtml += '    sHtml += \'  <td>Build Reqs:</td>\';\n';
            sHtml += '    sHtml += \'  <td class="tmform-field-wide100" colspan="4">' \
                     '<input name="%s[\' + sId + \'][%s]" id="%s[\' + sId + \'][2]" value="\' + sBuildReqExpr + \'"></td>\';\n' \
                   % (sName, TestCaseArgsData.ksParam_sBuildReqExpr, sName,);
            sHtml += '    sHtml += \'  <td></td>\';\n';
            sHtml += '    sHtml += \'</tr>\';\n'
            sHtml += '\n';
            sHtml += '    oTBody.id = \'%s[\' + sId + \'][6]\';\n' % (sName,);
            sHtml += '    oTBody.innerHTML = sHtml;\n';
            sHtml += '\n';
            sHtml += '    oElement.appendChild(oTBody);\n';
            sHtml += '\n';
            sHtml += '    g_%s_aItems[sId] = 1;\n' % (sName,);
            sHtml += '    g_%s_cItems++;\n' % (sName,);
            sHtml += '    setElementValueToKeyList(\'%s\', g_%s_aItems);\n' % (sName, sName);
            sHtml += '}\n';
            sHtml += 'function %s_extendList()\n' % (sName,);
            sHtml += '{\n';
            sHtml += '    %s_extendListEx("%s", "%s", "%s", "%s", "%s");\n' % (sName,
                escapeAttr(str(oVarDefaults.cGangMembers)), escapeAttr(str(oVarDefaults.cSecTimeout)),
                escapeAttr(oVarDefaults.sArgs), escapeAttr(oVarDefaults.sTestBoxReqExpr),
                escapeAttr(oVarDefaults.sBuildReqExpr), );
            sHtml += '}\n';
            if config.g_kfVBoxSpecific:
                sSecTimeoutDef = escapeAttr(str(oVarDefaults.cSecTimeout));
                sHtml += 'function vbox_%s_add_uni()\n' % (sName,);
                sHtml += '{\n';
                sHtml += '    %s_extendListEx("1", "%s", "--cpu-counts 1 --virt-modes raw", ' \
                         ' "", "");\n' % (sName, sSecTimeoutDef);
                sHtml += '    %s_extendListEx("1", "%s", "--cpu-counts 1 --virt-modes hwvirt", ' \
                         ' "fCpuHwVirt is True", "");\n' % (sName, sSecTimeoutDef);
                sHtml += '    %s_extendListEx("1", "%s", "--cpu-counts 1 --virt-modes hwvirt-np", ' \
                         ' "fCpuNestedPaging is True", "");\n' % (sName, sSecTimeoutDef);
                sHtml += '}\n';
                sHtml += 'function vbox_%s_add_uni_amd64()\n' % (sName,);
                sHtml += '{\n';
                sHtml += '    %s_extendListEx("1", "%s", "--cpu-counts 1 --virt-modes hwvirt", ' \
                         ' "fCpuHwVirt is True", "");\n' % (sName, sSecTimeoutDef);
                sHtml += '    %s_extendListEx("1", "%s", "--cpu-counts 1 --virt-modes hwvirt-np", ' \
                         ' "fCpuNestedPaging is True", "");\n' % (sName, sSecTimeoutDef);
                sHtml += '}\n';
                sHtml += 'function vbox_%s_add_smp()\n' % (sName,);
                sHtml += '{\n';
                sHtml += '    %s_extendListEx("1", "%s", "--cpu-counts 2 --virt-modes hwvirt",' \
                         ' "fCpuHwVirt is True and cCpus >= 2", "");\n' % (sName, sSecTimeoutDef);
                sHtml += '    %s_extendListEx("1", "%s", "--cpu-counts 2 --virt-modes hwvirt-np",' \
                         ' "fCpuNestedPaging is True and cCpus >= 2", "");\n' % (sName, sSecTimeoutDef);
                sHtml += '    %s_extendListEx("1", "%s", "--cpu-counts 3 --virt-modes hwvirt",' \
                         ' "fCpuHwVirt is True and cCpus >= 3", "");\n' % (sName, sSecTimeoutDef);
                sHtml += '    %s_extendListEx("1", "%s", "--cpu-counts 4 --virt-modes hwvirt-np ",' \
                         ' "fCpuNestedPaging is True and cCpus >= 4", "");\n' % (sName, sSecTimeoutDef);
                #sHtml += '    %s_extendListEx("1", "%s", "--cpu-counts 6 --virt-modes hwvirt",' \
                #         ' "fCpuHwVirt is True and cCpus >= 6", "");\n' % (sName, sSecTimeoutDef);
                #sHtml += '    %s_extendListEx("1", "%s", "--cpu-counts 8 --virt-modes hwvirt-np",' \
                #         ' "fCpuNestedPaging is True and cCpus >= 8", "");\n' % (sName, sSecTimeoutDef);
                sHtml += '}\n';
            sHtml += '</script>\n';


        #
        # List current entries.
        #
        sHtml += '<input type="hidden" name="%s" id="%s" value="%s">\n' \
               % (sName, sName, ','.join(str(i) for i in range(len(aoVariations))), );
        sHtml += '  <table id="%s" class="tmform-testcasevars">\n' % (sTableId,)
        if not fReadOnly:
            sHtml += '  <caption>\n' \
                     '    <a href="#" onClick="%s_extendList()">Add</a>\n' % (sName,);
            if config.g_kfVBoxSpecific:
                sHtml += '    [<a href="#" onClick="vbox_%s_add_uni()">Single CPU Variations</a>\n' % (sName,);
                sHtml += '    <a href="#" onClick="vbox_%s_add_uni_amd64()">amd64</a>]\n' % (sName,);
                sHtml += '    [<a href="#" onClick="vbox_%s_add_smp()">SMP Variations</a>]\n' % (sName,);
            sHtml += '  </caption>\n';

        dSubErrors = {};
        if sName in self._dErrors  and  isinstance(self._dErrors[sName], dict):
            dSubErrors = self._dErrors[sName];

        for iVar, _ in enumerate(aoVariations):
            oVar = copy.copy(aoVariations[iVar]);
            oVar.convertToParamNull();

            sHtml += '<tbody id="%s[%s][6]">\n' % (sName, iVar,)
            sHtml += '  <tr class="tmform-testcasevars-first-row">\n' \
                     '    <td>Gang Members:</td>' \
                     '    <td class="tmform-field-tiny-int"><input name="%s[%s][%s]" id="%s[%s][1]" value="%s"%s></td>\n' \
                     '    <td>Timeout:</td>' \
                     '    <td class="tmform-field-int"><input name="%s[%s][%s]" id="%s[%s][2]" value="%s"%s></td>\n' \
                   % ( sName, iVar, TestCaseArgsData.ksParam_cGangMembers, sName, iVar, oVar.cGangMembers, sReadOnlyAttr,
                       sName, iVar, TestCaseArgsData.ksParam_cSecTimeout,  sName, iVar,
                       utils.formatIntervalSeconds2(oVar.cSecTimeout), sReadOnlyAttr, );
            if not fReadOnly:
                sHtml += '    <td><a href="#" onclick="%s_removeEntry(\'%s\');">Remove</a></td>\n' \
                       % (sName, iVar);
            else:
                sHtml +=  '    <td></td>\n';
            sHtml += '    <td class="tmform-testcasevars-stupid-border-column"></td>\n' \
                     '  </tr>\n';

            sHtml += '  <tr class="tmform-testcasevars-inner-row">\n' \
                     '    <td>Arguments:</td>' \
                     '    <td class="tmform-field-wide100" colspan="4">' \
                     '<input name="%s[%s][%s]" id="%s[%s][3]" value="%s"%s></td>\n' \
                     '    <td></td>\n' \
                     '  </tr>\n' \
                   % ( sName, iVar, TestCaseArgsData.ksParam_sArgs, sName, iVar, escapeAttr(oVar.sArgs), sReadOnlyAttr)

            sHtml += '  <tr class="tmform-testcasevars-inner-row">\n' \
                     '    <td>TestBox Reqs:</td>' \
                     '    <td class="tmform-field-wide100" colspan="4">' \
                     '<input name="%s[%s][%s]" id="%s[%s][4]" value="%s"%s></td>\n' \
                     '    <td></td>\n' \
                     '  </tr>\n' \
                   % ( sName, iVar, TestCaseArgsData.ksParam_sTestBoxReqExpr, sName, iVar,
                       escapeAttr(oVar.sTestBoxReqExpr), sReadOnlyAttr)

            sHtml += '  <tr class="tmform-testcasevars-final-row">\n' \
                     '    <td>Build Reqs:</td>' \
                     '    <td class="tmform-field-wide100" colspan="4">' \
                     '<input name="%s[%s][%s]" id="%s[%s][5]" value="%s"%s></td>\n' \
                     '    <td></td>\n' \
                     '  </tr>\n' \
                   % ( sName, iVar, TestCaseArgsData.ksParam_sBuildReqExpr, sName, iVar,
                       escapeAttr(oVar.sBuildReqExpr), sReadOnlyAttr)


            if iVar in dSubErrors:
                sHtml += '  <tr><td colspan="4"><p align="left" class="tmform-error-desc">%s</p></td></tr>\n' \
                       % (self._escapeErrorText(dSubErrors[iVar]),);

            sHtml += '</tbody>\n';
        sHtml += '  </table>\n'
        sHtml += '</li>\n'

        return self._add(sHtml)

    def addListOfTestGroupMembers(self, sName, aoTestGroupMembers, aoAllTestCases, sLabel,  # pylint: disable=R0914
                                  fReadOnly = True):
        """
        For WuiTestGroup.
        """
        assert len(aoTestGroupMembers) <= len(aoAllTestCases);
        self._addLabel(sName, sLabel);
        if len(aoAllTestCases) == 0:
            return self._add('<li>No testcases available.</li>\n')

        self._add('<input name="%s" type="hidden" value="%s">\n'
                  % ( TestGroupDataEx.ksParam_aidTestCases,
                      ','.join([str(oTestCase.idTestCase) for oTestCase in aoAllTestCases]), ));

        self._add('<table class="tmformtbl">\n'
                  ' <thead>\n'
                  '  <tr>\n'
                  '    <th rowspan="2"></th>\n'
                  '    <th rowspan="2">Test Case</th>\n'
                  '    <th rowspan="2">All Vars</th>\n'
                  '    <th rowspan="2">Priority [0..31]</th>\n'
                  '    <th colspan="4" align="center">Variations</th>\n'
                  '  </tr>\n'
                  '  <tr>\n'
                  '    <th>Included</th>\n'
                  '    <th>Gang size</th>\n'
                  '    <th>Timeout</th>\n'
                  '    <th>Arguments</th>\n'
                  '  </tr>\n'
                  ' </thead>\n'
                  ' <tbody>\n'
                  );

        if self._fReadOnly:
            fReadOnly = True;
        sCheckBoxAttr = ' readonly onclick="return false" onkeydown="return false"' if fReadOnly else '';

        oDefMember = TestGroupMemberData();
        aoTestGroupMembers = list(aoTestGroupMembers); # Copy it so we can pop.
        for iTestCase, _ in enumerate(aoAllTestCases):
            oTestCase = aoAllTestCases[iTestCase];

            # Is it a member?
            oMember = None;
            for i, _ in enumerate(aoTestGroupMembers):
                if aoTestGroupMembers[i].oTestCase.idTestCase == oTestCase.idTestCase:
                    oMember = aoTestGroupMembers.pop(i);
                    break;

            # Start on the rows...
            sPrefix = '%s[%d]' % (sName, oTestCase.idTestCase,);
            self._add('  <tr class="%s">\n'
                      '    <td rowspan="%d">\n'
                      '      <input name="%s[%s]" type="hidden" value="%s">\n' # idTestCase
                      '      <input name="%s[%s]" type="hidden" value="%s">\n' # idTestGroup
                      '      <input name="%s[%s]" type="hidden" value="%s">\n' # tsExpire
                      '      <input name="%s[%s]" type="hidden" value="%s">\n' # tsEffective
                      '      <input name="%s[%s]" type="hidden" value="%s">\n' # uidAuthor
                      '      <input name="%s" type="checkbox"%s%s value="%d" class="tmform-checkbox" title="#%d - %s">\n' # (list)
                      '    </td>\n'
                      % ( 'tmodd' if iTestCase & 1 else 'tmeven',
                          len(oTestCase.aoTestCaseArgs),
                          sPrefix, TestGroupMemberData.ksParam_idTestCase,  oTestCase.idTestCase,
                          sPrefix, TestGroupMemberData.ksParam_idTestGroup, -1 if oMember is None else oMember.idTestGroup,
                          sPrefix, TestGroupMemberData.ksParam_tsExpire,    '' if oMember is None else oMember.tsExpire,
                          sPrefix, TestGroupMemberData.ksParam_tsEffective, '' if oMember is None else oMember.tsEffective,
                          sPrefix, TestGroupMemberData.ksParam_uidAuthor,   '' if oMember is None else oMember.uidAuthor,
                          TestGroupDataEx.ksParam_aoMembers, '' if oMember is None else ' checked', sCheckBoxAttr,
                          oTestCase.idTestCase, oTestCase.idTestCase, escapeElem(oTestCase.sName),
                          ));
            self._add('    <td rowspan="%d" align="left">%s</td>\n'
                      % ( len(oTestCase.aoTestCaseArgs), escapeElem(oTestCase.sName), ));

            self._add('    <td rowspan="%d" title="Include all variations (checked) or choose a set?">\n'
                      '      <input name="%s[%s]" type="checkbox"%s%s value="-1">\n'
                      '    </td>\n'
                      % ( len(oTestCase.aoTestCaseArgs),
                          sPrefix, TestGroupMemberData.ksParam_aidTestCaseArgs,
                          ' checked' if oMember is None  or  oMember.aidTestCaseArgs is None else '', sCheckBoxAttr, ));

            self._add('    <td rowspan="%d" align="center">\n'
                      '      <input name="%s[%s]" type="text" value="%s" style="max-width:3em;" %s>\n'
                      '    </td>\n'
                      % ( len(oTestCase.aoTestCaseArgs),
                          sPrefix, TestGroupMemberData.ksParam_iSchedPriority,
                          (oMember if oMember is not None else oDefMember).iSchedPriority,
                          ' readonly class="tmform-input-readonly"' if fReadOnly else '', ));

            # Argument variations.
            aidTestCaseArgs = [] if oMember is None or oMember.aidTestCaseArgs is None else oMember.aidTestCaseArgs;
            for iVar in range(len(oTestCase.aoTestCaseArgs)):
                oVar = oTestCase.aoTestCaseArgs[iVar];
                if iVar > 0:
                    self._add('  <tr class="%s">\n' % ('tmodd' if iTestCase & 1 else 'tmeven',));
                self._add('   <td align="center">\n'
                          '     <input name="%s[%s]" type="checkbox"%s%s value="%d">'
                          '   </td>\n'
                          % ( sPrefix, TestGroupMemberData.ksParam_aidTestCaseArgs,
                              ' checked' if oVar.idTestCaseArgs in aidTestCaseArgs else '', sCheckBoxAttr, oVar.idTestCaseArgs,
                              ));
                self._add('   <td align="center">%s</td>\n'
                          '   <td align="center">%s</td>\n'
                          '   <td align="left">%s</td>\n'
                          % ( oVar.cGangMembers,
                              'Default' if oVar.cSecTimeout is None else oVar.cSecTimeout,
                              escapeElem(oVar.sArgs) ));

                self._add('  </tr>\n');



            if len(oTestCase.aoTestCaseArgs) == 0:
                self._add('    <td></td> <td></td> <td></td> <td></td>\n'
                          '  </tr>\n');
        return self._add(' </tbody>\n'
                         '</table>\n');

    def addListOfSchedGroupMembers(self, sName, aoSchedGroupMembers, aoAllTestGroups,  # pylint: disable=R0914
                                   sLabel, fReadOnly = True):
        """
        For WuiAdminSchedGroup.
        """
        if self._fReadOnly:
            fReadOnly = True;
        assert len(aoSchedGroupMembers) <= len(aoAllTestGroups);
        self._addLabel(sName, sLabel);
        if len(aoAllTestGroups) == 0:
            return self._add('<li>No test groups available.</li>\n')

        self._add('<input name="%s" type="hidden" value="%s">\n'
                  % ( SchedGroupDataEx.ksParam_aidTestGroups,
                      ','.join([str(oTestGroup.idTestGroup) for oTestGroup in aoAllTestGroups]), ));

        self._add('<table class="tmformtbl">\n'
                  ' <thead>\n'
                  '  <tr>\n'
                  '    <th></th>\n'
                  '    <th>Test Group</th>\n'
                  '    <th>Priority [0..31]</th>\n'
                  '    <th>Prerequisite Test Group</th>\n'
                  '    <th>Weekly schedule</th>\n'
                  '  </tr>\n'
                  ' </thead>\n'
                  ' <tbody>\n'
                  );

        sCheckBoxAttr = ' readonly onclick="return false" onkeydown="return false"' if fReadOnly else '';
        sComboBoxAttr = ' disabled' if fReadOnly else '';

        oDefMember = SchedGroupMemberData();
        aoSchedGroupMembers = list(aoSchedGroupMembers); # Copy it so we can pop.
        for iTestGroup, _ in enumerate(aoAllTestGroups):
            oTestGroup = aoAllTestGroups[iTestGroup];

            # Is it a member?
            oMember = None;
            for i, _ in enumerate(aoSchedGroupMembers):
                if aoSchedGroupMembers[i].oTestGroup.idTestGroup == oTestGroup.idTestGroup:
                    oMember = aoSchedGroupMembers.pop(i);
                    break;

            # Start on the rows...
            sPrefix = '%s[%d]' % (sName, oTestGroup.idTestGroup,);
            self._add('  <tr class="%s">\n'
                      '    <td>\n'
                      '      <input name="%s[%s]" type="hidden" value="%s">\n' # idTestGroup
                      '      <input name="%s[%s]" type="hidden" value="%s">\n' # idSchedGroup
                      '      <input name="%s[%s]" type="hidden" value="%s">\n' # tsExpire
                      '      <input name="%s[%s]" type="hidden" value="%s">\n' # tsEffective
                      '      <input name="%s[%s]" type="hidden" value="%s">\n' # uidAuthor
                      '      <input name="%s" type="checkbox"%s%s value="%d" class="tmform-checkbox" title="#%d - %s">\n' # (list)
                      '    </td>\n'
                      % ( 'tmodd' if iTestGroup & 1 else 'tmeven',
                          sPrefix, SchedGroupMemberData.ksParam_idTestGroup,    oTestGroup.idTestGroup,
                          sPrefix, SchedGroupMemberData.ksParam_idSchedGroup,   -1 if oMember is None else oMember.idSchedGroup,
                          sPrefix, SchedGroupMemberData.ksParam_tsExpire,       '' if oMember is None else oMember.tsExpire,
                          sPrefix, SchedGroupMemberData.ksParam_tsEffective,    '' if oMember is None else oMember.tsEffective,
                          sPrefix, SchedGroupMemberData.ksParam_uidAuthor,      '' if oMember is None else oMember.uidAuthor,
                          SchedGroupDataEx.ksParam_aoMembers, '' if oMember is None else ' checked', sCheckBoxAttr,
                          oTestGroup.idTestGroup, oTestGroup.idTestGroup, escapeElem(oTestGroup.sName),
                          ));
            self._add('    <td align="left">%s</td>\n' % ( escapeElem(oTestGroup.sName), ));

            self._add('    <td align="center">\n'
                      '      <input name="%s[%s]" type="text" value="%s" style="max-width:3em;" %s>\n'
                      '    </td>\n'
                      % ( sPrefix, SchedGroupMemberData.ksParam_iSchedPriority,
                          (oMember if oMember is not None else oDefMember).iSchedPriority,
                          ' readonly class="tmform-input-readonly"' if fReadOnly else '', ));

            self._add('    <td align="center">\n'
                      '      <select name="%s[%s]" id="%s[%s]" class="tmform-combobox"%s>\n'
                      '        <option value="-1"%s>None</option>\n'
                      % ( sPrefix, SchedGroupMemberData.ksParam_idTestGroupPreReq,
                          sPrefix, SchedGroupMemberData.ksParam_idTestGroupPreReq,
                          sComboBoxAttr,
                          ' selected' if oMember is None or oMember.idTestGroupPreReq is None else '',
                          ));
            for oTestGroup2 in aoAllTestGroups:
                if oTestGroup2 != oTestGroup:
                    fSelected = oMember is not None and oTestGroup2.idTestGroup == oMember.idTestGroupPreReq;
                    self._add('        <option value="%s"%s>%s</option>\n'
                              % ( oTestGroup2.idTestGroup, ' selected' if fSelected else '', escapeElem(oTestGroup2.sName), ));
            self._add('      </select>\n'
                      '    </td>\n');

            self._add('    <td align="left">\n'
                      '      Todo<input name="%s[%s]" type="hidden" value="%s">\n'
                      '    </td>\n'
                      % ( sPrefix, SchedGroupMemberData.ksParam_bmHourlySchedule,
                          '' if oMember is None else oMember.bmHourlySchedule, ));

            self._add('  </tr>\n');
        return self._add(' </tbody>\n'
                         '</table>\n');

    #
    # Buttons.
    #
    def addSubmit(self, sLabel = 'Submit'):
        """Adds the submit button to the form."""
        if self._fReadOnly:
            return True;
        return self._add('      <li>\n'
                         '        <br>\n'
                         '        <div class="tmform-field"><div class="tmform-field-submit">\n'
                         '           <label>&nbsp;</label>\n'
                         '           <input type="submit" value="%s">\n'
                         '        </div></div>\n'
                         '      </li>\n'
                         % (escapeElem(sLabel),));

    def addReset(self):
        """Adds a reset button to the form."""
        if self._fReadOnly:
            return True;
        return self._add('      <li>\n'
                         '        <div class="tmform-button"><div class="tmform-button-reset">\n'
                         '          <input type="reset" value="%s">\n'
                         '        </div></div>\n'
                         '      </li>\n'
                        );

