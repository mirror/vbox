#!/bin/sh
# $Id$
#
# VirtualBox X Server auto-start service unit test.
#
# Copyright (C) 2012 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

## The function definition at the start of every non-trivial shell script!
abort()
{
  ## $@, ... Error text to output to standard error in printf format.
  format="$1"
  shift
  printf "${TEST_NAME}: ${format}" "$@" >&2
  exit 1
}

## Print a TESTING line.  Takes printf arguments but without a '\n'.
print_line()
{
  format="$1"
  shift
  printf "${TEST_NAME}: TESTING ${format}... " "$@"
}

## Expected a process to complete within a certain time and call a function if
# it does which should check whether the test was successful and print status
# information.  The function takes the exit status as its single parameter.
expect_exit()
{
  PID="$1"            ## The PID we are waiting for.
  TIME_OUT="$2"       ## The time-out before we terminate the process.
  TEST_FUNCTION="$3"  ## The function to call on exit to check the test result.

  # Give it time to complete.
  { sleep "${TIME_OUT}"; kill "${PID}" 2>/dev/null; } &

  wait "${PID}"
  STATUS="$?"
  case "${STATUS}" in
  143) # SIGTERM
    printf "\nFAILED: time-out.\n"
    ;;
  *)
    ${TEST_FUNCTION} "${STATUS}"
esac
}

## Create a simple configuration file.  Add items onto the end to override them
# on an item-by-item basis.
create_basic_configuration_file()
{
  FILE_NAME="$1"    ## The name of the configuration file to create.
  BASE_FOLDER="$2"  ## The basic folder for creating things under.
  cat > "${FILE_NAME}" << EOF
HEADLESS_X_ORG_CONFIGURATION_FOLDER="${BASE_FOLDER}/xorg"
HEADLESS_X_ORG_LOG_FOLDER="${BASE_FOLDER}/log"
HEADLESS_X_ORG_LOG_FILE="log"
HEADLESS_X_ORG_RUN_FOLDER="${BASE_FOLDER}/run"
HEADLESS_X_ORG_CHECK_PREREQUISITES=
HEADLESS_X_ORG_SERVER_PRE_COMMAND=
HEADLESS_X_ORG_SERVER_COMMAND="echo \\\${screen} \\\${conf_file} \\\${log_file}"
HEADLESS_X_ORG_SERVER_LOG_FILE_TEMPLATE="log.\\\${screen}"
EOF

}

# Get the directory where the script is located and the parent.
OUR_FOLDER="$(dirname "$0")"
OUR_FOLDER=$(cd "${OUR_FOLDER}" && pwd)
VBOX_FOLDER=$(cd "${OUR_FOLDER}/.." && pwd)
[ -d "${VBOX_FOLDER}" ] ||
  abort "Failed to change to directory ${VBOX_FOLDER}.\n"
cd "${VBOX_FOLDER}"

# Get our name for output.
TEST_NAME="$(basename "$0" .sh)"

# And remember the full path.
TEST_NAME_FULL="${OUR_FOLDER}/$(basename "$0")"

# We use this to test a long-running process
[ x"$1" = "x--test-sleep" ] &&
  while true; do true; done

# Create a temporary directory for configuration and logging.
for i in 0 1 2 3 4 5 6 7 8 9; do
  TEST_FOLDER="/tmp/${TEST_NAME}${i}"
  mkdir -m 0700 "${TEST_FOLDER}" 2>/dev/null && break
done
[ -d "${TEST_FOLDER}" ] || abort "Failed to create a temporary folder\n"
# Clean up.  Small race here, but probably not important.
trap "rm -r \"${TEST_FOLDER}\" 2>/dev/null" EXIT HUP INT QUIT ABRT TERM
# Server configuration folder.
XORG_FOLDER="${TEST_FOLDER}/xorg"
mkdir -p "${XORG_FOLDER}"

# Simple start-up test.
print_line "simple start-up test"
create_basic_configuration_file "${TEST_FOLDER}/conf" "${TEST_FOLDER}"
touch "${XORG_FOLDER}/xorg.conf.2"
touch "${XORG_FOLDER}/xorg.conf.4"

test_simple_start_up()
{
  STATUS="$1"
  case "${STATUS}" in
  0)
    LOG_FOLDER="${TEST_FOLDER}/log"
    LOG="${LOG_FOLDER}/log"
    if grep -q "2 ${XORG_FOLDER}/xorg.conf.2 ${LOG_FOLDER}/log.2" "${LOG}" &&
      grep -q "4 ${XORG_FOLDER}/xorg.conf.4 ${LOG_FOLDER}/log.4" "${LOG}"; then
      printf "SUCCESS.\n"
    else
      printf "\nFAILED: incorrect log output.\n"
    fi
    ;;
  *)
    printf "\nFAILED: exit status ${STATUS}.\n"
  esac
}

./VBoxHeadlessXOrg.sh -c "${TEST_FOLDER}/conf" &
PID=$!
expect_exit "${PID}" 5 test_simple_start_up
rm "${XORG_FOLDER}"/xorg.conf.*

# No configuration files.
print_line "no configuration files"

test_should_fail()
{
  STATUS="$1"
  case "${STATUS}" in
  0)
    printf "\nFAILED: successful exit when an error was expected.\n"
    ;;
  *)
    printf "SUCCESS.\n"  # At least it behaved the way we wanted.
  esac
}

./VBoxHeadlessXOrg.sh -c "${TEST_FOLDER}/conf" &
PID=$!
expect_exit "${PID}" 5 test_should_fail

# Bad configuration files.
print_line "bad configuration files"
touch "${XORG_FOLDER}/xorg.conf.2"
touch "${XORG_FOLDER}/xorg.conf.4"
touch "${XORG_FOLDER}/xorg.conf.other"
./VBoxHeadlessXOrg.sh -c "${TEST_FOLDER}/conf" &
PID=$!
expect_exit "${PID}" 5 test_should_fail
rm "${XORG_FOLDER}/"xorg.conf.*

# Set up a configuration file for a long-running command.
create_basic_configuration_file "${TEST_FOLDER}/conf" "${TEST_FOLDER}"
cat >> "${TEST_FOLDER}/conf" << EOF
HEADLESS_X_ORG_SERVER_COMMAND="\"${TEST_NAME_FULL}\" --test-sleep \\\${screen}"
EOF

# Long running server command.
print_line "long running server command (sleeps)"
touch "${XORG_FOLDER}/xorg.conf.1"
touch "${XORG_FOLDER}/xorg.conf.5"
FAILURE=""
./VBoxHeadlessXOrg.sh -c "${TEST_FOLDER}/conf" &
PID="$!"
STARTED=""
for i in 1 2 3 4 5; do
  sleep 1  # Make sure it runs for at least one second.
  if ps -a -f | grep "${TEST_NAME}.*1" | grep -q -v grep &&
    ps -a -f | grep "${TEST_NAME}.*5" | grep -q -v grep; then
    STARTED="true"
    break
  fi
done
[ -n "${STARTED}" ] || FAILURE="\nFAILED to start servers.\n"
[ -n "${PID}" ] && kill "${PID}" 2>/dev/null
STOPPED=""
if [ -z "${FAILURE}" ]; then
  for i in 1 2 3 4 5; do
    if ! ps -a -f | grep "${TEST_NAME}.*1" | grep -q -v grep &&
      ! ps -a -f | grep "${TEST_NAME}.*5" | grep -q -v grep; then
      STOPPED="true"
      break;
    fi
    sleep 1
  done
  [ -n "${STOPPED}" ] ||
    FAILURE="\nFAILED to stop servers.\n"  # To terminate or not to terminate?
fi
if [ -n "${FAILURE}" ]; then
  printf "${FAILURE}"
else
  printf "SUCCESS.\n"
fi
rm "${XORG_FOLDER}/"xorg.conf.*

# Set up a configuration file with a pre-requisite.
create_basic_configuration_file "${TEST_FOLDER}/conf" "${TEST_FOLDER}"
cat >> "${TEST_FOLDER}/conf" << EOF
HEADLESS_X_ORG_CHECK_PREREQUISITES="[ -e \\"${TEST_FOLDER}/run/prereq\\" ]"
EOF

# Pre-requisite test.
print_line "configuration file with pre-requisite (sleeps)"
touch "${XORG_FOLDER}/xorg.conf.2"
touch "${XORG_FOLDER}/xorg.conf.4"
FAILURE=""
./VBoxHeadlessXOrg.sh -c "${TEST_FOLDER}/conf" &
PID="$!"
sleep 1
ps -p "${PID}" > /dev/null 2>&1 || FAILURE="\nFAILED to wait for pre-requisite.\n"
touch "${TEST_FOLDER}/run/prereq"
if [ -z "${FAILURE}" ]; then
  expect_exit "${PID}" 10 test_simple_start_up
else
  printf "${FAILURE}"
fi
rm -r "${XORG_FOLDER}"/xorg.conf.* "${TEST_FOLDER}/run"

# Set up our pre-command test configuration file.
create_basic_configuration_file "${TEST_FOLDER}/conf" "${TEST_FOLDER}"
cat >> "${TEST_FOLDER}/conf" << EOF
HEADLESS_X_ORG_SERVER_PRE_COMMAND="touch \"${TEST_FOLDER}/run/pre\""
HEADLESS_X_ORG_SERVER_COMMAND="cp \"${TEST_FOLDER}/run/pre\" \"${TEST_FOLDER}/run/pre2\""
EOF

# Pre-command test.
print_line "pre-command test"
touch "${XORG_FOLDER}/xorg.conf.2"

test_pre_command()
{
  STATUS="$1"
  case "${STATUS}" in
  0)
    LOG_FOLDER="${TEST_FOLDER}/log"
    LOG="${LOG_FOLDER}/log"
    if [ -e "${TEST_FOLDER}/run/pre" ] && [ -e "${TEST_FOLDER}/run/pre2" ]; then
      printf "SUCCESS.\n"
    else
      printf "\nFAILED: pre-command not executed.\n"
    fi
    ;;
  *)
    printf "\nFAILED: exit status ${STATUS}.\n"
  esac
}

rm -f "${TEST_FOLDER}/run/pre"
./VBoxHeadlessXOrg.sh -c "${TEST_FOLDER}/conf" &
PID=$!
expect_exit "${PID}" 5 test_pre_command
rm -f "${XORG_FOLDER}"/xorg.conf.* "${TEST_FOLDER}"/run/pre*

# Set up our post-command test configuration file.
create_basic_configuration_file "${TEST_FOLDER}/conf" "${TEST_FOLDER}"
cat >> "${TEST_FOLDER}/conf" << EOF
HEADLESS_X_ORG_SERVER_COMMAND="rm -f \"${TEST_FOLDER}/run/post\""
HEADLESS_X_ORG_SERVER_POST_COMMAND="touch \"${TEST_FOLDER}/run/post\""
EOF

# Post-command test.
print_line "post-command test"
touch "${XORG_FOLDER}/xorg.conf.2"
touch "${XORG_FOLDER}/xorg.conf.4"

test_post_command()
{
  STATUS="$1"
  case "${STATUS}" in
  0)
    LOG_FOLDER="${TEST_FOLDER}/log"
    LOG="${LOG_FOLDER}/log"
    if [ -e "${TEST_FOLDER}/run/post" ]; then
      printf "SUCCESS.\n"
    else
      printf "\nFAILED: post-command not executed.\n"
    fi
    ;;
  *)
    printf "\nFAILED: exit status ${STATUS}.\n"
  esac
}

rm -f "${TEST_FOLDER}/run/post"
./VBoxHeadlessXOrg.sh -c "${TEST_FOLDER}/conf" &
PID=$!
expect_exit "${PID}" 5 test_post_command
rm -f "${XORG_FOLDER}"/xorg.conf.* "${TEST_FOLDER}/run/post"
