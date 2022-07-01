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
MY_WS_NAME="virtualbox.vscode-workspace"
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
# Save the absolute path to the root directory.
#
cd "${MY_OUT_DIR}"
my_abs_dir "."
MY_FILE_ROOT_DIR=${MY_ABS_DIR}


#
# From now on everything *MUST* succeed.
#
set -e


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
my_abs_dir "tools/bin"
MY_INCLUDE_DIR=${MY_ABS_DIR}
. "$MY_INCLUDE_DIR/include-workspace-projects.sh"

my_generate_workspace

echo "done"
