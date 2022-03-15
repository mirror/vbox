# !kmk_ash
# $Id$
## @file
# Script for generating a Visual Studio Code (vscode) workspace.
#

#
# Copyright (C) 2022 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

#
# Some constants.
#
MY_CAT="kmk_cat"
MY_CP="kmk_cp"
MY_MKDIR="kmk_mkdir"
MY_MV="kmk_mv"
MY_SED="kmk_sed"
MY_RM="kmk_rm"
MY_SLEEP="kmk_sleep"
MY_EXPR="kmk_expr"
MY_SVN="svn"

#MY_SORT="kmk_cat"
MY_SORT="sort"

#
# Globals.
#
MY_PROJECT_FILES=""
MY_OUT_DIRS="\
out/${KBUILD_TARGET}.${KBUILD_TARGET_ARCH}/${KBUILD_TYPE} \
out/${KBUILD_TARGET}.${KBUILD_TARGET_ARCH}/debug \
out/${KBUILD_TARGET}.${KBUILD_TARGET_ARCH}/release \
out/linux.amd64/debug \
out/linux.x86/debug \
out/win.amd64/debug \
out/win.x86/debug \
out/darwin.amd64/debug \
out/darwin.x86/debug \
out/haiku.amd64/debug \
out/haiku.x86/debug \
out/solaris.amd64/debug \
out/solaris.x86/debug";


#
# Parameters w/ defaults.
#
MY_ROOT_DIR=".."
MY_VSCODE_DIR=".vscode"
MY_VSCODE_FILE_DOT_EXT=".json"
MY_OUT_DIR=${MY_VSCODE_DIR}
MY_PRJ_PRF="VBox-"
MY_WS_NAME="virtualbox.code-workspace"
MY_DBG=""
MY_WINDOWS_HOST=""
MY_OPT_MINIMAL=""
MY_OPT_USE_WILDCARDS="yes"


##
# Gets the absolute path to an existing directory.
#
# @param    $1      The path.
my_abs_dir()
{
    if test -n "${PWD}"; then
        MY_ABS_DIR=`cd ${MY_ROOT_DIR}/${1} && echo ${PWD}`

    else
        # old cygwin shell has no PWD and need adjusting.
        MY_ABS_DIR=`cd ${MY_ROOT_DIR}/${1} && pwd | ${MY_SED} -e 's,^/cygdrive/\(.\)/,\1:/,'`
    fi
    if test -z "${MY_ABS_DIR}"; then
        MY_ABS_DIR="${1}"
    fi
}

##
# Gets the file name part of a path.
#
# @param    $1      The path.
my_get_name()
{
    SAVED_IFS=$IFS
    IFS=":/ "
    set $1
    while test $# -gt 1  -a  -n "${2}";
    do
        shift;
    done

    IFS=$SAVED_IFS
    #echo "$1"
    export MY_GET_NAME=$1
}

##
# Gets the newest version of a library (like openssl).
#
# @param    $1      The library base path relative to root.
my_get_newest_ver()
{
    cd "${MY_ROOT_DIR}" > /dev/null
    latest=
    for ver in "$1"*;
    do
        if test -z "${latest}" || "${MY_EXPR}" "${ver}" ">" "${latest}" > /dev/null; then
            latest="${ver}"
        fi
    done
    if test -z "${latest}"; then
        echo "error: could not find any version of: $1" >&2;
        exit 1;
    fi
    echo "${latest}"
    return 0;
}


##
# Generate file entry for the specified file if it was found to be of interest.
#
# @param    $1      The output file name base.
# @param    $2      The file name.
# @param    $3      Optional folder override.
my_file()
{
    # sort and filter by file type.
    case "$2" in
        # drop these.
        *.kup|*~|*.pyc|*.exe|*.sys|*.dll|*.o|*.obj|*.lib|*.a|*.ko)
            return 0
            ;;

        # by prefix or directory component.
        tst*|*/testcase/*)
            MY_FOLDER="$1-Testcases.lst"
            ;;

        # by extension.
        *.c|*.cpp|*.m|*.mm|*.pl|*.py|*.as|*.c.h|*.cpp.h|*.java)
            MY_FOLDER="$1-Sources.lst"
            ;;

        *.h|*.hpp|*.mm)
            MY_FOLDER="$1-Headers.lst"
            ;;

        *.asm|*.s|*.S|*.inc|*.mac)
            MY_FOLDER="$1-Assembly.lst"
            ;;

        *)
            MY_FOLDER="$1-Others.lst"
            ;;
    esac
    if test -n "$3";
    then
        MY_FOLDER="$1-$3.lst"
    fi

    ## @todo only files which are in subversion.
#    if ${MY_SVN} info "${2}" > /dev/null 2>&1; then
        my_get_name "${2}"
        echo ' <!-- sortkey: '"${MY_GET_NAME}"' --> <F N="'"${2}"'"/>' >> "${MY_FOLDER}"
#    fi
}

##
# Generate file entry for the specified file if it was found to be of interest.
#
# @param    $1      The output file name base.
# @param    $2      The wildcard spec.
# @param    $3      Optional folder override.
my_wildcard()
{
    if test -n "$3"; then
        MY_FOLDER="$1-$3.lst"
    else
        MY_FOLDER="$1-All.lst"
    fi
    EXCLUDES="*.log;*.kup;*~;*.bak;*.bak?;*.pyc;*.exe;*.sys;*.dll;*.o;*.obj;*.lib;*.a;*.ko;*.class;*.cvsignore;*.done;*.project;*.actionScriptProperties;*.scm-settings;*.svnpatch.rej;*.svn-base;.svn/*;*.gitignore;*.gitattributes;*.gitmodules;*.swagger-codegen-ignore;*.png;*.bmp;*.jpg"
    echo '        <F N="'"${2}"'/*" Recurse="1" Excludes="'"${EXCLUDES}"'"/>' >> "${MY_FOLDER}"
}

##
# Generate file entries for the specified sub-directory tree.
#
# @param    $1      The output filename.
# @param    $2      The sub-directory.
# @param    $3      Optional folder override.
my_sub_tree()
{
    if test -n "$MY_DBG"; then echo "dbg: my_sub_tree: ${2}"; fi

    # Skip .svn directories.
    case "$2" in
        */.svn|*/.svn)
            return 0
            ;;
    esac

    # Process the files in the directory.
    for f in $2/*;
    do
        if test -d "${f}";
        then
            my_sub_tree "${1}" "${f}" "${3}"
        else
            my_file "${1}" "${f}" "${3}"
        fi
    done
    return 0;
}

##
# Function generating an (intermediate) project task.
#
# @param    $1      The project file name.
# @param    $2      Build config name.
# @param    $3      Task group to assign task to.
# @param    $4      kBuild extra args.
# @param    $5      kBuild working directory to set.
#                   If empty, the file's directory will be used.
# @param    $6      kBuild per-task options.
#                   Leave empty if not being used.
my_generate_project_task()
{
    MY_FILE="${1}";
    MY_CFG_NAME="${2}";
    MY_TASK_GROUP="${3}";
    MY_KMK_EXTRAS="${4}";
    MY_KMK_CWD="${5}";
    MY_KMK_TASK_ARGS="${6}";
    shift; shift; shift; shift; shift; shift;

    if [ -z "$MY_KMK_CWD" ]; then
        MY_KMK_CWD='${fileDirname}'
    fi

    MY_TASK_LABEL="${MY_TASK_GROUP}: ${MY_CFG_NAME}"

    echo '        {'                                                            >> "${MY_FILE}"
    echo '            "type": "shell",'                                         >> "${MY_FILE}"
    echo '            "label": "'${MY_TASK_LABEL}'",'                           >> "${MY_FILE}"
    echo '            "command": "'${MY_KMK_CMD}'",'                            >> "${MY_FILE}"
    echo '            "args": ['                                                >> "${MY_FILE}"
    echo '                ' ${MY_KMK_ARGS} ','                                  >> "${MY_FILE}"
    if [ -n "${MY_KMK_EXTRAS}" ]; then
        echo '                '${MY_KMK_EXTRAS}' ,'                             >> "${MY_FILE}"
    fi
    echo '                "-C",'                                                >> "${MY_FILE}"
    echo '                "'${MY_KMK_CWD}'"'                                    >> "${MY_FILE}"
    if [ -n "${MY_KMK_TASK_ARGS}" ]; then
    echo '              ', ${MY_KMK_TASK_ARGS}                                  >> "${MY_FILE}"
    fi
    echo '            ],'                                                       >> "${MY_FILE}"
    echo '            "options": {'                                             >> "${MY_FILE}"
    echo '                "cwd": "'${MY_KMK_CWD}'"'                             >> "${MY_FILE}"
    echo '            },'                                                       >> "${MY_FILE}"
    echo '            "problemMatcher": ['                                      >> "${MY_FILE}"
    echo '                "$gcc"'                                               >> "${MY_FILE}"
    echo '            ],'                                                       >> "${MY_FILE}"
    echo '            "presentation": {'                                        >> "${MY_FILE}"
    echo '                "reveal": "always",'                                  >> "${MY_FILE}"
    echo '                "clear": true,'                                       >> "${MY_FILE}"
    echo '                "panel": "dedicated"'                                 >> "${MY_FILE}"
    echo '            },'                                                       >> "${MY_FILE}"
    echo '            "detail": "compiler: /bin/clang++-9"'                     >> "${MY_FILE}"
    echo '        },'                                                           >> "${MY_FILE}"
}

##
# Function generating a project build config.
#
# @param    $1      The project file name.
# @param    $2      Build config name.
# @param    $3      Extra kBuild command line options, variant 1.
# @param    $4      Extra kBuild command line options, variant 2.
# @param    $4+     Include directories.
# @param    $N      --end-includes
my_generate_project_config()
{
    MY_FILE="${1}";
    MY_CFG_NAME="${2}";
    MY_KMK_EXTRAS1="${3}";
    MY_KMK_EXTRAS2="${4}";
    MY_KMK_EXTRAS3="${5}";
    MY_KMK_EXTRAS4="${6}";
    shift; shift; shift; shift; shift; shift;

    ## @todo Process includes.
    while test $# -ge 1  -a  "${1}" != "--end-includes";
    do
        for f in $1;
        do
            my_abs_dir ${f}
            #echo "Includes: ${MY_ABS_DIR}/"
        done
        shift
    done
    shift

    #
    # Build tasks.
    #
    MY_TASK_CWD='${fileDirname}'
    MY_TASK_ARGS='"-o", "${file}"'
    my_generate_project_task "${MY_FILE}" "${MY_CFG_NAME}" "Compile1" "${MY_KMK_EXTRAS1}" \
                             "${MY_TASK_CWD}" "${MY_TASK_ARGS}"
    if test -n "${MY_KMK_EXTRAS2}"; then
        my_generate_project_task "${MY_FILE}" "${MY_CFG_NAME}" "Compile2" "${MY_KMK_EXTRAS2}" \
                                 "${MY_TASK_CWD}" "${MY_TASK_ARGS}"
    fi
    if test -n "${MY_KMK_EXTRAS3}"; then
        my_generate_project_task "${MY_FILE}" "${MY_CFG_NAME}" "Compile3" "${MY_KMK_EXTRAS3}" \
                                 "${MY_TASK_CWD}" "${MY_TASK_ARGS}"
    fi
    if test -n "${MY_KMK_EXTRAS4}"; then
        my_generate_project_task "${MY_FILE}" "${MY_CFG_NAME}" "Compile4" "${MY_KMK_EXTRAS4}" \
                                 "${MY_TASK_CWD}" "${MY_TASK_ARGS}"
    fi
    MY_TASK_CWD='${workspaceFolder}'
    MY_TASK_ARGS=""
    my_generate_project_task "${MY_FILE}" "${MY_CFG_NAME}" "Build1" "${MY_KMK_EXTRAS1}" \
                             "${MY_TASK_CWD}" "${MY_TASK_ARGS}"
    if test -n "${MY_KMK_EXTRAS2}"; then
        my_generate_project_task "${MY_FILE}" "${MY_CFG_NAME}" "Build2" "${MY_KMK_EXTRAS2}" \
                                 "${MY_TASK_CWD}" "${MY_TASK_ARGS}"
    fi
    if test -n "${MY_KMK_EXTRAS3}"; then
        my_generate_project_task "${MY_FILE}" "${MY_CFG_NAME}" "Build3" "${MY_KMK_EXTRAS3}" \
                                 "${MY_TASK_CWD}" "${MY_TASK_ARGS}"
    fi
    if test -n "${MY_KMK_EXTRAS4}"; then
        my_generate_project_task "${MY_FILE}" "${MY_CFG_NAME}" "Build4" "${MY_KMK_EXTRAS4}" \
                                 "${MY_TASK_CWD}" "${MY_TASK_ARGS}"
    fi

    MY_TASK_CWD='${workspaceFolder}'
    MY_TASK_ARGS='"rebuild"'
    my_generate_project_task "${MY_FILE}" "${MY_CFG_NAME}" "Rebuild1" "${MY_KMK_EXTRAS1}" \
                             "${MY_TASK_CWD}" "${MY_TASK_ARGS}"
    if test -n "${MY_KMK_EXTRAS2}"; then
        my_generate_project_task "${MY_FILE}" "${MY_CFG_NAME}" "Rebuild2" "${MY_KMK_EXTRAS2}" \
                                 "${MY_TASK_CWD}" "${MY_TASK_ARGS}"
    fi
    if test -n "${MY_KMK_EXTRAS3}"; then
        my_generate_project_task "${MY_FILE}" "${MY_CFG_NAME}" "Rebuild3" "${MY_KMK_EXTRAS3}" \
                                 "${MY_TASK_CWD}" "${MY_TASK_ARGS}"
    fi
    if test -n "${MY_KMK_EXTRAS4}"; then
        my_generate_project_task "${MY_FILE}" "${MY_CFG_NAME}" "Rebuild4" "${MY_KMK_EXTRAS4}" \
                                 "${MY_TASK_CWD}" "${MY_TASK_ARGS}"
    fi

    #
    # Generate compound tasks that invokes all needed sub tasks.
    #
    # Note: We include "VBox" in the label so that the command is easier to find
    #       in the command pallette.
    #
    echo '              {'                                                      >> "${MY_FILE}"
    echo '                  "label": "VBox Compile: '${MY_CFG_NAME}'",'         >> "${MY_FILE}"
    echo '                  "dependsOrder": "sequence",'                        >> "${MY_FILE}"
    echo '                  "dependsOn": [ "Compile1: '${MY_CFG_NAME}'"'        >> "${MY_FILE}"
    if test -n "${MY_KMK_EXTRAS2}"; then
        echo '                        , "Compile2: '${MY_CFG_NAME}'"'           >> "${MY_FILE}"
    fi
    if test -n "${MY_KMK_EXTRAS3}"; then
       echo '                         , "Compile3: '${MY_CFG_NAME}'"'           >> "${MY_FILE}"
    fi
    if test -n "${MY_KMK_EXTRAS4}"; then
       echo '                         , "Compile4: '${MY_CFG_NAME}'"'           >> "${MY_FILE}"
    fi
    echo '                       ]'                                             >> "${MY_FILE}"
    echo '              },'                                                     >> "${MY_FILE}"
    echo '              {'                                                      >> "${MY_FILE}"
    echo '                      "label": "VBox Build: '${MY_CFG_NAME}'",'       >> "${MY_FILE}"
    echo '                      "dependsOrder": "sequence",'                    >> "${MY_FILE}"
    echo '                      "dependsOn": [ "Build1: '${MY_CFG_NAME}'"'      >> "${MY_FILE}"
    if test -n "${MY_KMK_EXTRAS2}"; then
        echo '                         , "Build2: '${MY_CFG_NAME}'"'            >> "${MY_FILE}"
    fi
    if test -n "${MY_KMK_EXTRAS3}"; then
        echo '                         , "Build3: '${MY_CFG_NAME}'"'            >> "${MY_FILE}"
    fi
    if test -n "${MY_KMK_EXTRAS4}"; then
        echo '                         , "Build4: '${MY_CFG_NAME}'"'            >> "${MY_FILE}"
    fi
    echo '                      ],'                                             >> "${MY_FILE}"
    echo '                      "group": {'                                     >> "${MY_FILE}"
    echo '                          "kind": "build",'                           >> "${MY_FILE}"
    echo '                          "isDefault": true'                          >> "${MY_FILE}"
    echo '                      }'                                              >> "${MY_FILE}"
    echo '              },'                                                     >> "${MY_FILE}"
    echo '              {'                                                      >> "${MY_FILE}"
    echo '                      "label": "VBox Rebuild: '${MY_CFG_NAME}'",'     >> "${MY_FILE}"
    echo '                      "dependsOrder": "sequence",'                    >> "${MY_FILE}"
    echo '                      "dependsOn": [ "Rebuild1: '${MY_CFG_NAME}'"'    >> "${MY_FILE}"
    if test -n "${MY_KMK_EXTRAS2}"; then
        echo '                         , "Rebuild2: '${MY_CFG_NAME}'"'          >> "${MY_FILE}"
    fi
    if test -n "${MY_KMK_EXTRAS3}"; then
        echo '                         , "Rebuild3: '${MY_CFG_NAME}'"'          >> "${MY_FILE}"
    fi
    if test -n "${MY_KMK_EXTRAS4}"; then
        echo '                         , "Rebuild4: '${MY_CFG_NAME}'"'          >> "${MY_FILE}"
    fi
    echo '                       ]'                                             >> "${MY_FILE}"
    echo '              },'                                                     >> "${MY_FILE}"
}


##
# Function generating a project.
#
# @param    $1      The project file name.
# @param    $2      The project working directory.
# @param    $3      Dummy separator.
# @param    $4+     Include directories.
# @param    $N      --end-includes
# @param    $N+1    Directory sub-trees and files to include in the project.
#
my_generate_project()
{
    MY_PRJ_NAME=${1}
    MY_WRK_DIR="${MY_FILE_ROOT_DIR}/${2}"
    MY_FILE_PATH="${MY_WRK_DIR}/.vscode"
    shift
    shift
    shift

    # Make sure that the .vscode project dir exists.
    mkdir -p "$MY_FILE_PATH"

    MY_FILE="${MY_FILE_PATH}/c_cpp_properties${MY_VSCODE_FILE_DOT_EXT}";
    echo "Generating ${MY_FILE}..."

    # Add it to the project list for workspace construction later on.
    MY_PROJECT_FILES="${MY_PROJECT_FILES} ${MY_PRJ_NAME}:${MY_WRK_DIR}"

    #
    # Generate the C/C++ bits.
    ## @todo Might needs tweaking a bit more as stuff evolves.
    #
    echo '{'                                                                    >  "${MY_FILE}"
    echo '    "configurations": ['                                              >> "${MY_FILE}"
    echo '        {'                                                            >> "${MY_FILE}"
    echo '            "name": "Linux",'                                         >> "${MY_FILE}"
    echo '            "includePath": ['                                         >> "${MY_FILE}"
    echo '                "${workspaceFolder}/**"'                              >> "${MY_FILE}"
    echo '            ],'                                                       >> "${MY_FILE}"
    echo '            "defines": [],'                                           >> "${MY_FILE}"
    echo '            "cStandard": "c17",'                                      >> "${MY_FILE}"
    echo '            "cppStandard": "c++14",'                                  >> "${MY_FILE}"
    echo '            "intelliSenseMode": "linux-gcc-x64",'                     >> "${MY_FILE}"
    echo '            "compilerPath": "/usr/bin/gcc"'                           >> "${MY_FILE}"
    echo '        }'                                                            >> "${MY_FILE}"
    echo '    ],'                                                               >> "${MY_FILE}"
    echo '    "version": 4'                                                     >> "${MY_FILE}"
    echo '}'                                                                    >> "${MY_FILE}"

    MY_FILE="${MY_FILE_PATH}/tasks${MY_VSCODE_FILE_DOT_EXT}";
    echo "Generating ${MY_FILE}..."

    #
    # Tasks header.
    #
    echo '{'                                                                    >  "${MY_FILE}"
    echo '    "version": "2.0.0",'                                              >> "${MY_FILE}"
    echo '    "tasks": ['                                                       >> "${MY_FILE}"

    my_generate_project_config "${MY_FILE}" "Default" "" "" "" "" $*
    my_generate_project_config "${MY_FILE}" "Debug + hardening" \
        '"KBUILD_TYPE=debug", "VBOX_WITH_HARDENING=1"' \
        "" "" "" $*
    my_generate_project_config "${MY_FILE}" "Release + hardening" \
        '"KBUILD_TYPE=release", "VBOX_WITH_HARDENING=1"' \
        "" "" "" $*
    my_generate_project_config "${MY_FILE}" "Debug+Release + hardening" \
        '"KBUILD_TYPE=debug", "VBOX_WITH_HARDENING=1"' \
        '"KBUILD_TYPE=release", "VBOX_WITH_HARDENING=1"' \
        "" "" $*
    my_generate_project_config "${MY_FILE}" "Debug w/o hardening" \
        '"KBUILD_TYPE=debug", "VBOX_WITHOUT_HARDENING=1"' \
        "" "" "" $*
    my_generate_project_config "${MY_FILE}" "Release w/o hardening" \
        '"KBUILD_TYPE=release", "VBOX_WITHOUT_HARDENING=1"' \
        "" "" "" $*
    my_generate_project_config "${MY_FILE}" "Debug+Release w/o hardening" \
        '"KBUILD_TYPE=debug", "VBOX_WITHOUT_HARDENING=1"' \
        '"KBUILD_TYPE=release", "VBOX_WITHOUT_HARDENING=1"' \
        "" "" $*
    my_generate_project_config "${MY_FILE}" "Debug+Release with and without hardening" \
        '"KBUILD_TYPE=debug", "VBOX_WITH_HARDENING=1"' \
        '"KBUILD_TYPE=release", "VBOX_WITH_HARDENING=1"' \
        '"KBUILD_TYPE=debug", "VBOX_WITHOUT_HARDENING=1"' \
        '"KBUILD_TYPE=release", "VBOX_WITHOUT_HARDENING=1"' \
        $*

    #
    # Tasks footer.
    #
    echo '    ]'                                                                >> "${MY_FILE}"
    echo '}'                                                                    >> "${MY_FILE}"

    while test $# -ge 1  -a  "${1}" != "--end-includes";
    do
        shift;
    done;
    shift;

    return 0
}


##
# Generate the workspace
#
my_generate_workspace()
{
    MY_FILE="${MY_FILE_ROOT_DIR}/${MY_WS_NAME}"
    echo "Generating ${MY_FILE}..."
    echo '{'                                                                    >  "${MY_FILE}"
    echo '    "folders": ['                                                     >> "${MY_FILE}"
    for i in ${MY_PROJECT_FILES};
    do
        MY_PRJ_NAME=$(echo $i | cut -d ":" -f 1)
        MY_PRJ_PATH=$(echo $i | cut -d ":" -f 2)
        echo '        {'                                                        >> "${MY_FILE}"
        echo '            "name": "'"${MY_PRJ_NAME}"'",'                        >> "${MY_FILE}"
        echo '            "path": "'"${MY_PRJ_PATH}"'",'                        >> "${MY_FILE}"
        echo '        },'                                                       >> "${MY_FILE}"
    done
    echo '    ],'                                                               >> "${MY_FILE}"
    echo '    "settings": {},'                                                  >> "${MY_FILE}"
    echo '    "extensions": {'                                                  >> "${MY_FILE}"
    echo '        "recommendations": ["ms-vscode.cpptools"]'                    >> "${MY_FILE}"
    echo '    },'                                                               >> "${MY_FILE}"
    echo '}'                                                                    >> "${MY_FILE}"
    return 0
}


#
# Parse arguments.
#
while test $# -ge 1;
do
    ARG=$1
    shift
    case "$ARG" in

        --rootdir)
            if test $# -eq 0; then
                echo "error: missing --rootdir argument." 1>&2
                exit 1;
            fi
            MY_ROOT_DIR="$1"
            shift
            ;;

        --outdir)
            if test $# -eq 0; then
                echo "error: missing --outdir argument." 1>&2
                exit 1;
            fi
            MY_OUT_DIR="$1"
            shift
            ;;

        --project-base)
            if test $# -eq 0; then
                echo "error: missing --project-base argument." 1>&2
                exit 1;
            fi
            MY_PRJ_PRF="$1"
            shift
            ;;

        --workspace)
            if test $# -eq 0; then
                echo "error: missing --workspace argument." 1>&2
                exit 1;
            fi
            MY_WS_NAME="$1"
            shift
            ;;

        --windows-host)
            MY_WINDOWS_HOST=1
            ;;

        --minimal)
            MY_OPT_MINIMAL=1
            ;;

        # usage
        --h*|-h*|-?|--?)
            echo "usage: $0 [--rootdir <rootdir>] [--outdir <outdir>] [--project-base <prefix>] [--workspace <name>] [--minimal]"
            echo ""
            echo "If --outdir is specified, you must specify a --rootdir relative to it as well."
            exit 1;
            ;;

        # default
        *)
            echo "error: Invalid parameter '$ARG'" 1>&2
            exit 1;

    esac
done


#
# From now on everything *MUST* succeed.
#
set -e


#
# Save the absolute path to the root directory.
#
cd "${MY_OUT_DIR}"
my_abs_dir "."
MY_FILE_ROOT_DIR=${MY_ABS_DIR}


#
# Make sure the output directory exists, is valid and clean.
#
${MY_RM} -f \
    "${MY_OUT_DIR}"*.json \
    "${MY_FILE_ROOT_DIR}/${MY_WS_NAME}"
${MY_MKDIR} -p "${MY_OUT_DIR}"


#
# Determine the invocation to conjure up kmk.
#
my_abs_dir "tools"
if test -n "${MY_WINDOWS_HOST}"; then
    MY_KMK_CMD="cscript.exe"
    MY_KMK_ARGS='"/Nologo", "'${MY_ABS_DIR}/envSub.vbs'", "--quiet", "--", "kmk.exe"'
else
    MY_KMK_CMD="/usr/bin/env"
    MY_KMK_ARGS='"LANG=C", "'${MY_ABS_DIR}/env.sh'", "--quiet", "--no-wine", "kmk"'
fi

#
# Generate the projects and workspace.
#
# Note! The configs aren't optimal yet, lots of adjustment wrt headers left to be done.
#

# src/VBox/Runtime
my_generate_project "IPRT"          "src/VBox/Runtime"                      --begin-incs "include" "src/VBox/Runtime/include"               --end-includes "include/iprt" "src/VBox/Runtime"

# src/VBox/VMM
my_generate_project "VMM"           "src/VBox/VMM"                          --begin-incs "include" "src/VBox/VMM"                           --end-includes "src/VBox/VMM" \
    "include/VBox/vmm/cfgm.h" \
    "include/VBox/vmm/cpum.*" \
    "include/VBox/vmm/dbgf.h" \
    "include/VBox/vmm/em.h" \
    "include/VBox/vmm/gim.h" \
    "include/VBox/vmm/apic.h" \
    "include/VBox/vmm/gmm.*" \
    "include/VBox/vmm/gvm.*" \
    "include/VBox/vmm/hm*.*" \
    "include/VBox/vmm/iom.h" \
    "include/VBox/vmm/mm.h" \
    "include/VBox/vmm/patm.*" \
    "include/VBox/vmm/pdm*.h" \
    "include/VBox/vmm/pgm.*" \
    "include/VBox/vmm/selm.*" \
    "include/VBox/vmm/ssm.h" \
    "include/VBox/vmm/stam.*" \
    "include/VBox/vmm/tm.h" \
    "include/VBox/vmm/trpm.*" \
    "include/VBox/vmm/vm.*" \
    "include/VBox/vmm/vmm.*"

# src/VBox/Additions
my_generate_project "Add-darwin"    "src/VBox/Additions/darwin"             --begin-incs "include" "src/VBox/Additions/darwin"              --end-includes "src/VBox/Additions/darwin"
my_generate_project "Add-freebsd"   "src/VBox/Additions/freebsd"            --begin-incs "include" "src/VBox/Additions/freebsd"             --end-includes "src/VBox/Additions/freebsd"
my_generate_project "Add-haiku"     "src/VBox/Additions/haiku"              --begin-incs "include" "src/VBox/Additions/haiku"               --end-includes "src/VBox/Additions/haiku"
my_generate_project "Add-linux"     "src/VBox/Additions/linux"              --begin-incs "include" "src/VBox/Additions/linux"               --end-includes "src/VBox/Additions/linux"
my_generate_project "Add-os2"       "src/VBox/Additions/os2"                --begin-incs "include" "src/VBox/Additions/os2"                 --end-includes "src/VBox/Additions/os2"
my_generate_project "Add-solaris"   "src/VBox/Additions/solaris"            --begin-incs "include" "src/VBox/Additions/solaris"             --end-includes "src/VBox/Additions/solaris"
my_generate_project "Add-win"       "src/VBox/Additions/WINNT"              --begin-incs "include" "src/VBox/Additions/WINNT"               --end-includes "src/VBox/Additions/WINNT"
if test -z "$MY_OPT_MINIMAL"; then
    my_generate_project "Add-x11"   "src/VBox/Additions/x11"                --begin-incs "include" "src/VBox/Additions/x11"                 --end-includes "src/VBox/Additions/x11"
fi
my_generate_project "Add-Control"   "src/VBox/Additions/common/VBoxControl"   --begin-incs "include" "src/VBox/Additions/common/VBoxControl"  --end-includes "src/VBox/Additions/common/VBoxControl"
my_generate_project "Add-GuestDrv"  "src/VBox/Additions/common/VBoxGuest"     --begin-incs "include" "src/VBox/Additions/common/VBoxGuest"    --end-includes "src/VBox/Additions/common/VBoxGuest"      "include/VBox/VBoxGuest*.*"
my_generate_project "Add-Lib"       "src/VBox/Additions/common/VBoxGuest/lib" --begin-incs "include" "src/VBox/Additions/common/VBoxGuest/lib" --end-includes "src/VBox/Additions/common/VBoxGuest/lib" "include/VBox/VBoxGuest/lib/*.*"
my_generate_project "Add-Service"   "src/VBox/Additions/common/VBoxService"   --begin-incs "include" "src/VBox/Additions/common/VBoxService"  --end-includes "src/VBox/Additions/common/VBoxService"
my_generate_project "Add-VBoxVideo" "src/VBox/Additions/common/VBoxVideo"     --begin-incs "include" "src/VBox/Additions/common/VBoxVideo"    --end-includes "src/VBox/Additions/common/VBoxVideo"
if test -z "$MY_OPT_MINIMAL"; then
    my_generate_project "Add-pam"       "src/VBox/Additions/common/pam"         --begin-incs "include" "src/VBox/Additions/common/pam"          --end-includes "src/VBox/Additions/common/pam"
    my_generate_project "Add-cmn-test"  "src/VBox/Additions/common/testcase"    --begin-incs "include" "src/VBox/Additions/common/testcase"     --end-includes "src/VBox/Additions/common/testcase"
    my_generate_project "Add-CredProv"  "src/VBox/Additions/WINNT/VBoxCredProv" --begin-incs "include" "src/VBox/Additions/WINNT/VBoxCredProv"  --end-includes "src/VBox/Additions/WINNT/VBoxCredProv"
    my_generate_project "Add-GINA"      "src/VBox/Additions/WINNT/VBoxGINA"     --begin-incs "include" "src/VBox/Additions/WINNT/VBoxGINA"      --end-includes "src/VBox/Additions/WINNT/VBoxGINA"
fi

# src/VBox/Debugger
my_generate_project "Debugger"      "src/VBox/Debugger"                     --begin-incs "include" "src/VBox/Debugger"                      --end-includes "src/VBox/Debugger" "include/VBox/dbggui.h" "include/VBox/dbg.h"

# src/VBox/Devices
my_generate_project "Devices"       "src/VBox/Devices"                      --begin-incs "include" "src/VBox/Devices"                       --end-includes "src/VBox/Devices" "include/VBox/pci.h" "include/VBox/pdm*.h"
## @todo split this up.

# src/VBox/Disassembler
my_generate_project "DIS"           "src/VBox/Disassembler"                 --begin-incs "include" "src/VBox/Disassembler"                  --end-includes "src/VBox/Disassembler" "include/VBox/dis*.h"

# src/VBox/Frontends
if test -z "$MY_OPT_MINIMAL"; then
    my_generate_project "FE-VBoxBalloonCtrl" "src/VBox/Frontends/VBoxBalloonCtrl" --begin-incs "include" "src/VBox/Frontends/VBoxBalloonCtrl"     --end-includes "src/VBox/Frontends/VBoxBalloonCtrl"
fi
my_generate_project "FE-VBoxManage"      "src/VBox/Frontends/VBoxManage"      --begin-incs "include" "src/VBox/Frontends/VBoxManage"          --end-includes "src/VBox/Frontends/VBoxManage"
my_generate_project "FE-VBoxHeadless"    "src/VBox/Frontends/VBoxHeadless"    --begin-incs "include" "src/VBox/Frontends/VBoxHeadless"        --end-includes "src/VBox/Frontends/VBoxHeadless"
my_generate_project "FE-VBoxSDL"         "src/VBox/Frontends/VBoxSDL"         --begin-incs "include" "src/VBox/Frontends/VBoxSDL"             --end-includes "src/VBox/Frontends/VBoxSDL"
my_generate_project "FE-VBoxShell"       "src/VBox/Frontends/VBoxShell"       --begin-incs "include" "src/VBox/Frontends/VBoxShell"           --end-includes "src/VBox/Frontends/VBoxShell"
# noise - my_generate_project "FE-VBoxBFE"      "src/VBox/Frontends/VBoxBFE"      --begin-incs "include" "src/VBox/Frontends/VBoxBFE"            --end-includes "src/VBox/Frontends/VBoxBFE"
FE_VBOX_WRAPPERS=""
for d in ${MY_OUT_DIRS};
do
    if test -d "${MY_ROOT_DIR}/${d}/obj/VirtualBox/include"; then
        FE_VBOX_WRAPPERS="${d}/obj/VirtualBox/include"
        break
    fi
done
if test -n "${FE_VBOX_WRAPPERS}"; then
    my_generate_project "FE-VirtualBox" "src/VBox/Frontends/VirtualBox"     --begin-incs "include" "${FE_VBOX_WRAPPERS}"                    --end-includes "src/VBox/Frontends/VirtualBox" "${FE_VBOX_WRAPPERS}/COMWrappers.cpp" "${FE_VBOX_WRAPPERS}/COMWrappers.h"
else
    my_generate_project "FE-VirtualBox" "src/VBox/Frontends/VirtualBox"     --begin-incs "include"                                          --end-includes "src/VBox/Frontends/VirtualBox"
fi

# src/VBox/GuestHost
my_generate_project "HGSMI-GH"      "src/VBox/GuestHost/HGSMI"              --begin-incs "include"                                          --end-includes "src/VBox/GuestHost/HGSMI"
if test -z "$MY_OPT_MINIMAL"; then
    my_generate_project "DnD-GH"    "src/VBox/GuestHost/DragAndDrop"        --begin-incs "include"                                          --end-includes "src/VBox/GuestHost/DragAndDrop"
fi
my_generate_project "ShClip-GH"     "src/VBox/GuestHost/SharedClipboard"    --begin-incs "include"                                          --end-includes "src/VBox/GuestHost/SharedClipboard"

# src/VBox/HostDrivers
my_generate_project "SUP"           "src/VBox/HostDrivers/Support"          --begin-incs "include" "src/VBox/HostDrivers/Support"           --end-includes "src/VBox/HostDrivers/Support" "include/VBox/sup.h" "include/VBox/sup.mac"
my_generate_project "VBoxNetAdp"    "src/VBox/HostDrivers/VBoxNetAdp"       --begin-incs "include" "src/VBox/HostDrivers/VBoxNetAdp"        --end-includes "src/VBox/HostDrivers/VBoxNetAdp" "include/VBox/intnet.h"
my_generate_project "VBoxNetFlt"    "src/VBox/HostDrivers/VBoxNetFlt"       --begin-incs "include" "src/VBox/HostDrivers/VBoxNetFlt"        --end-includes "src/VBox/HostDrivers/VBoxNetFlt" "include/VBox/intnet.h"
my_generate_project "VBoxUSB"       "src/VBox/HostDrivers/VBoxUSB"          --begin-incs "include" "src/VBox/HostDrivers/VBoxUSB"           --end-includes "src/VBox/HostDrivers/VBoxUSB" "include/VBox/usblib*.h" "include/VBox/usbfilter.h"
my_generate_project "AdpCtl"        "src/VBox/HostDrivers/adpctl"           --begin-incs "include"                                          --end-includes "src/VBox/HostDrivers/adpctl"

# src/VBox/HostServices
my_generate_project "GuestCntl"     "src/VBox/HostServices/GuestControl"    --begin-incs "include" "src/VBox/HostServices/GuestControl"     --end-includes "src/VBox/HostServices/GuestControl"
my_generate_project "DragAndDrop"   "src/VBox/HostServices/DragAndDrop"     --begin-incs "include" "src/VBox/HostServices/DragAndDrop"      --end-includes "src/VBox/HostServices/DragAndDrop"
my_generate_project "GuestProps"    "src/VBox/HostServices/GuestProperties" --begin-incs "include" "src/VBox/HostServices/GuestProperties"  --end-includes "src/VBox/HostServices/GuestProperties"
my_generate_project "ShClip-HS"     "src/VBox/HostServices/SharedClipboard" --begin-incs "include" "src/VBox/HostServices/SharedClipboard"  --end-includes "src/VBox/HostServices/SharedClipboard"
my_generate_project "SharedFolders" "src/VBox/HostServices/SharedFolders"   --begin-incs "include" "src/VBox/HostServices/SharedFolders"    --end-includes "src/VBox/HostServices/SharedFolders" "include/VBox/shflsvc.h"

# src/VBox/ImageMounter
my_generate_project "ImageMounter"  "src/VBox/ImageMounter"                 --begin-incs "include" "src/VBox/ImageMounter"                  --end-includes "src/VBox/ImageMounter"

# src/VBox/Installer
my_generate_project "Installers"    "src/VBox/Installer"                    --begin-incs "include"                                          --end-includes "src/VBox/Installer"

# src/VBox/Main
my_generate_project "Main"          "src/VBox/Main"                         --begin-incs "include" "src/VBox/Main/include"                  --end-includes "src/VBox/Main" "include/VBox/com" "include/VBox/settings.h"
## @todo seperate webservices and Main. pick the right headers. added generated headers.

# src/VBox/Network
my_generate_project "Net-DHCP"      "src/VBox/NetworkServices/Dhcpd"        --begin-incs "include" "src/VBox/NetworkServices/NetLib"        --end-includes "src/VBox/NetworkServices/Dhcpd"
my_generate_project "Net-NAT"       "src/VBox/NetworkServices/NAT"          --begin-incs "include" "src/VBox/NetworkServices/NAT"           --end-includes "src/VBox/NetworkServices/NAT" "src/VBox/Devices/Network/slirp"
my_generate_project "Net-NetLib"    "src/VBox/NetworkServices/NetLib"       --begin-incs "include" "src/VBox/NetworkServices/NetLib"        --end-includes "src/VBox/NetworkServices/NetLib"

# src/VBox/RDP
my_generate_project "RDP-Client"    "src/VBox/RDP/client-1.8.4"             --begin-incs "include" "src/VBox/RDP/client-1.8.4"              --end-includes "src/VBox/RDP/client-1.8.4"
my_generate_project "RDP-Server"    "src/VBox/RDP/server"                   --begin-incs "include" "src/VBox/RDP/server"                    --end-includes "src/VBox/RDP/server"
my_generate_project "RDP-WebClient" "src/VBox/RDP/webclient"                --begin-incs "include" "src/VBox/RDP/webclient"                 --end-includes "src/VBox/RDP/webclient"
my_generate_project "RDP-Misc"      "src/VBox/RDP"                          --begin-incs "include"                                          --end-includes "src/VBox/RDP/auth" "src/VBox/RDP/tscpasswd" "src/VBox/RDP/x11server"

# src/VBox/Storage
my_generate_project "Storage"       "src/VBox/Storage"                      --begin-incs "include" "src/VBox/Storage"                       --end-includes "src/VBox/Storage"

# src/VBox/ValidationKit
my_generate_project "ValidationKit" "src/VBox/ValidationKit"                --begin-incs "include"                                          --end-includes "src/VBox/ValidationKit"

# src/VBox/ExtPacks
my_generate_project "ExtPacks"      "src/VBox/ExtPacks"                     --begin-incs "include"                                          --end-includes "src/VBox/ExtPacks"

# src/bldprogs
my_generate_project "bldprogs"      "src/bldprogs"                          --begin-incs "include"                                          --end-includes "src/bldprogs"

# A few things from src/lib
lib=$(my_get_newest_ver src/libs/zlib)
my_generate_project "zlib"          "${lib}"                                --begin-incs "include"                                          --end-includes "${lib}/*.c" "${lib}/*.h"
lib=$(my_get_newest_ver src/libs/liblzf)
my_generate_project "liblzf"        "${lib}"                                --begin-incs "include"                                          --end-includes "${lib}"
lib=$(my_get_newest_ver src/libs/libpng)
my_generate_project "libpng"        "${lib}"                                --begin-incs "include"                                          --end-includes "${lib}/*.c" "${lib}/*.h"
lib=$(my_get_newest_ver src/libs/openssl)
my_generate_project "openssl"       "${lib}"                                --begin-incs "include" "${lib}/crypto"                          --end-includes "${lib}"
lib=$(my_get_newest_ver src/libs/curl)
my_generate_project "curl"          "${lib}"                                --begin-incs "include" "${lib}/include"                         --end-includes "${lib}"

# webtools
my_generate_project "webtools"      "webtools"                              --begin-incs "include" "webtools/tinderbox/server/Tinderbox3"   --end-includes "webtools"

# include/VBox
my_generate_project "VBoxHeaders"   "include"                               --begin-incs "include"                                          --end-includes "include/VBox"

# Misc
my_generate_project "misc"          "."                                     --begin-incs "include"                                          --end-includes \
    "configure" \
    "configure.vbs" \
    "Config.kmk" \
    "Makefile.kmk" \
    "src/Makefile.kmk" \
    "src/VBox/Makefile.kmk" \
    "tools/env.sh" \
    "tools/env.cmd" \
    "tools/envSub.vbs" \
    "tools/envSub.cmd" \
    "tools/win/vbscript"


# out/x.y/z/bin/sdk/bindings/xpcom
XPCOM_INCS="src/libs/xpcom18a4"
for d in \
    "out/${KBUILD_TARGET}.${KBUILD_TARGET_ARCH}/${KBUILD_TYPE}/dist/sdk/bindings/xpcom" \
    "out/${KBUILD_TARGET}.${KBUILD_TARGET_ARCH}/${KBUILD_TYPE}/bin/sdk/bindings/xpcom" \
    "out/linux.amd64/debug/bin/sdk/bindings/xpcom" \
    "out/linux.x86/debug/bin/sdk/bindings/xpcom" \
    "out/darwin.amd64/debug/dist/sdk/bindings/xpcom" \
    "out/darwin.x86/debug/bin/dist/bindings/xpcom" \
    "out/haiku.amd64/debug/bin/sdk/bindings/xpcom" \
    "out/haiku.x86/debug/bin/sdk/bindings/xpcom" \
    "out/solaris.amd64/debug/bin/sdk/bindings/xpcom" \
    "out/solaris.x86/debug/bin/sdk/bindings/xpcom";
do
    if test -d "${MY_ROOT_DIR}/${d}"; then
        my_generate_project "SDK-xpcom" "${d}"  --begin-incs "include" "${d}/include" --end-includes "${d}"
        XPCOM_INCS="${d}/include"
        break
    fi
done

# lib/xpcom
my_generate_project "xpcom"         "src/libs/xpcom18a4"                    --begin-incs "include" "${XPCOM_INCS}"                          --end-includes "src/libs/xpcom18a4"

my_generate_workspace




echo "done"
