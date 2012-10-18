#!/bin/sh
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

[ x"$1" = x"--keep-temp" ] && KEEP_TEMP=true

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

## Run a test in which VBoxHeadlessXOrg.sh is expected to complete within a
# certain time and call a function if it does which should check whether the
# test was successful and print status information.  The function takes the
# exit status as its single parameter.
run_expect_exit()
{
  CONF_FILE="$1"      ## The configuration file.
  TIME_OUT="$2"       ## The time-out before we terminate the process.
  TEST_FUNCTION="$3"  ## The function to call on exit to check the test result.
  ./VBoxHeadlessXOrg.sh -c "${CONF_FILE}" &
  PID=$!
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

# Get the directory where the script is located and change to the parent.
VBOX_FOLDER="$(dirname "$0")/.."
VBOX_FOLDER=$(cd "${VBOX_FOLDER}" && pwd)
[ -d "${VBOX_FOLDER}" ] ||
  abort "Failed to change to directory ${VBOX_FOLDER}.\n"
cd "${VBOX_FOLDER}"

# Get our name for output.
TEST_NAME="$(basename "$0" .sh)"

# Create a temporary directory for configuration and logging.
for i in 0 1 2 3 4 5 6 7 8 9; do
  TEST_FOLDER="/tmp/${TEST_NAME}${i}"
  mkdir -m 0700 "${TEST_FOLDER}" 2>/dev/null && break
done
[ -d "${TEST_FOLDER}" ] || abort "Failed to create a temporary folder\n"
# Clean up.  Small race here, but probably not important.
[ -z "${KEEP_TEMP}" ] &&
  trap "rm -r \"${TEST_FOLDER}\" 2>/dev/null" EXIT HUP INT QUIT ABRT TERM
# Server configuration folder.
XORG_FOLDER="${TEST_FOLDER}/xorg"
mkdir "${XORG_FOLDER}"

# Set up our basic configuration file.
cat > "${TEST_FOLDER}/conf" << EOF
HEADLESS_X_ORG_CONFIGURATION_FOLDER="${XORG_FOLDER}"
HEADLESS_X_ORG_LOG_FOLDER="${TEST_FOLDER}/log"
HEADLESS_X_ORG_LOG_FILE="log"
HEADLESS_X_ORG_SERVER_COMMAND="echo \\\${screen} \\\${conf_file} \\\${log_file}"
HEADLESS_X_ORG_SERVER_LOG_FILE_TEMPLATE="log.\\\${screen}"
EOF

# Simple start-up test.
print_line "simple start-up test"
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
run_expect_exit "${TEST_FOLDER}/conf" 5 test_simple_start_up
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
run_expect_exit "${TEST_FOLDER}/conf" 5 test_should_fail

# Bad configuration files.
print_line "bad configuration files"
touch "${XORG_FOLDER}/xorg.conf.2"
touch "${XORG_FOLDER}/xorg.conf.4"
touch "${XORG_FOLDER}/xorg.conf.other"
run_expect_exit "${TEST_FOLDER}/conf" 5 test_should_fail
rm "${XORG_FOLDER}/"xorg.conf.*

# Set up a configuration file for a long-running command.
cat > "${TEST_FOLDER}/conf" << EOF
HEADLESS_X_ORG_CONFIGURATION_FOLDER="${XORG_FOLDER}"
HEADLESS_X_ORG_LOG_FOLDER="${TEST_FOLDER}/log"
HEADLESS_X_ORG_LOG_FILE="log"
HEADLESS_X_ORG_SERVER_COMMAND="echo $$ > ${TEST_FOLDER}/pid.\\\${screen}; cat"
HEADLESS_X_ORG_SERVER_LOG_FILE_TEMPLATE="log.\\\${screen}"
EOF

# Long running server command.
print_line "long running server command"
touch "${XORG_FOLDER}/xorg.conf.1"
touch "${XORG_FOLDER}/xorg.conf.5"
FAILURE=""
./VBoxHeadlessXOrg.sh -c "${TEST_FOLDER}/conf"
PID="$!"
[ -r "${TEST_FOLDER}/pid.1" ] &&
  [ -r "${TEST_FOLDER}/pid.5" ] &&
  ps -p "$(cat "${TEST_FOLDER}/pid.1")" >/dev/null 2>&1 &&
  ps -p "$(cat "${TEST_FOLDER}/pid.5")" >/dev/null 2>&1 ||
  FAILURE="\nFAILED to start servers.\n"
[ -n "${PID}" ] && kill "${PID}"
{ [ -r "${TEST_FOLDER}/pid.1" ] &&
    ps -p "$(cat "${TEST_FOLDER}/pid.1")" >/dev/null 2>&1; } ||
{ [ -r "${TEST_FOLDER}/pid.5" ] &&
    ps -p "$(cat "${TEST_FOLDER}/pid.5")" >/dev/null 2>&1; } &&
  FAILURE="\nFAILED to stop servers.\n"  # To terminate or not to terminate?
if [ -z "${FAILURE}" ]; then
  printf "${FAILURE}"
else
  printf "SUCCESS.\n"
fi
rm "${XORG_FOLDER}/"xorg.conf.*
rm -f "${TEST_FOLDER}/pid.1" "${TEST_FOLDER}/pid.5"
