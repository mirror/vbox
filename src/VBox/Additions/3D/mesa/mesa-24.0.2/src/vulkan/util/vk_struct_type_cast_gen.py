# Copyright © 2023 Igalia S.L.
# SPDX-License-Identifier: MIT

"""Create shortcuts for casting Vulkan structs when knowing their stype."""

import argparse
import functools
import os
import re
import textwrap
import xml.etree.ElementTree as et

from mako.template import Template
from vk_extensions import Extension, filter_api, get_all_required

COPYRIGHT = textwrap.dedent(u"""\
     * Copyright © 2023 Igalia S.L.
     * SPDX-License-Identifier: MIT
     """)

H_TEMPLATE = Template(textwrap.dedent(u"""\
    /* Autogenerated file -- do not edit
     * generated by ${file}
     *
     ${copyright}
     */

    #ifndef MESA_VK_STRUCT_CASTS_H
    #define MESA_VK_STRUCT_CASTS_H

    #include <vulkan/vulkan.h>

    #ifdef __cplusplus
    extern "C" {
    #endif

    % for s in structs:
    #define ${s.stype}_cast ${s.name}
    % endfor

    #ifdef __cplusplus
    } /* extern "C" */
    #endif

    #endif"""))


class VkStruct(object):
    """Simple struct-like class representing a single Vulkan struct identified with a VkStructureType"""
    def __init__(self, name, stype):
        self.name = name
        self.stype = stype


def struct_get_stype(xml_node):
    for member in xml_node.findall('./member'):
        name = member.findall('./name')
        if len(name) > 0 and name[0].text == "sType":
            return member.get('values')
    return None


def parse_xml(filename, structs, beta):
    xml = et.parse(filename)
    api = 'vulkan'

    required_types = get_all_required(xml, 'type', api, beta)

    for struct_type in xml.findall('./types/type[@category="struct"]'):
        if not filter_api(struct_type, api):
            continue

        name = struct_type.attrib['name']
        if name not in required_types:
            continue

        stype = struct_get_stype(struct_type)
        if stype is not None:
            structs.append(VkStruct(name, stype))

    for struct_type in xml.findall('.//enum[@alias][@extends=\'VkStructureType\']'):
        name = struct_type.attrib['name']
        alias = struct_type.attrib['alias']
        structs.append(VkStruct(alias + "_cast", name))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--xml', required=True,
                        help='Vulkan API XML files',
                        action='append',
                        dest='xml_files')
    parser.add_argument('--outdir',
                        help='Directory to put the generated files in',
                        required=True)
    parser.add_argument('--beta', required=True, help='Enable beta extensions.')

    args = parser.parse_args()

    structs = []

    for filename in args.xml_files:
        parse_xml(filename, structs, args.beta)

    structs = sorted(structs, key=lambda s: s.name)

    for template, file_ in [(H_TEMPLATE, os.path.join(args.outdir, 'vk_struct_type_cast.h'))]:
        with open(file_, 'w', encoding='utf-8') as f:
            f.write(template.render(
                file=os.path.basename(__file__),
                structs=structs,
                copyright=COPYRIGHT))


if __name__ == '__main__':
    main()
