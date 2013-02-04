#!/bin/sh
# $Id$
#
# VirtualBox init file creator unit test.
#
# Copyright (C) 2012-2013 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# This will initially be a skeleton with a couple of tests - add more for quick
# debugging when you suspect that something isn't working as specified.

tab="	"
tmpbase="/tmp/tstInstallInit 99"  # Space in the name for a little stress...

## The function definition at the start of every non-trivial shell script!
abort()
{
  ## $1 Error text to output to standard error in printf format.
    cat >&2 << EOF
${TEST_NAME}: $1
EOF
    exit 1
}

## Print a TESTING line.
print_line()
{
    cat >&2 << EOF
${TEST_NAME}: TESTING $1
EOF
}

## Set the failure message if it is not yet set.
fail_msg()
{
    test -z "${failed}" && failed="FAILED: ${1}"
}

# Get the directory where the script is located and the parent.
OUR_FOLDER=`dirname "$0"`
OUR_FOLDER=`cd "${OUR_FOLDER}" && pwd`
VBOX_FOLDER=`cd "${OUR_FOLDER}/.." && pwd`
[ -d "${VBOX_FOLDER}" ] ||
  abort "Failed to change to directory ${VBOX_FOLDER}.\n"
cd "${VBOX_FOLDER}"

# Get our name for output.
TEST_NAME="$(basename "$0" .sh)"

# Create a trivial test service in temporary directory $1 with name $2.
test_service()
{
    cat > "${1}/${2}" << EOF
#!/bin/sh
trap "touch \"${1}/stopped\"; exit" TERM 
echo "1: \${1} 2: \${2} 3: \${3}" > "${1}/started"
while true; do true; done
EOF
chmod u+x "${1}/${2}"
}

# Create a trivial test command in temporary directory $1 with name $2.
test_oneshot()
{
    cat > "${1}/${2}" << EOF
#!/bin/sh
if test "\${1}" = start; then
    touch "${1}/started"
else
    rm "${1}/started"
fi
exit 0
EOF
chmod u+x "${1}/${2}"
}

# Test some dodgy input values against generate_service_file.
# Make sure there is a substitution pattern at the end too.
print_line "generation of shell script from template."
input='TEST1%DESCRIPTION%%%%SERVICE_NAME%%STOP_COMMAND% TST2 TEST  %ARGUMENTS%%COMMAND%'
out=`echo "${input}" |
    helpers/generate_service_file --command '/usr/bin
aries/hello
world' --arguments 'p\x0a0\n\ \t' --format shell --description ''`
expected='TEST1%hello
world TST2 TEST  '\''p
0
 '"${tab}"\'\''/usr/bin
aries/hello
world'\'
case "${out}" in ${expected})
echo "SUCCESS (1)";;
*)
cat << EOF
FAILED: expected
${expected}
but got
${out}
EOF
esac
input='TEST%HAVE_STOP_COMMAND%%SERVICE_NAME%%STOP_COMMAND% TST2
 TEST  %COMMAND%'
out=`echo "${input}" |
    helpers/generate_service_file --command '/usr/bin/hello' --format shell --description ''`
expected='TEST TEST  '\''/usr/bin/hello'\'''
case "${out}" in ${expected})
echo "SUCCESS (2)";;
*)
cat << EOF
FAILED: expected
${expected}
but got
${out}
EOF
esac
input='TEST%HAVE_STOP_COMMAND%%SERVICE_NAME%%STOP_COMMAND% %STOP_ARGUMENTS% TST2
 TEST  %COMMAND%'
out=`echo "${input}" |
    helpers/generate_service_file --command '/usr/bin/hello' --format shell --description '' --stop-command /usr/bin/stop --stop-arguments hello`
expected='TESThello'\''/usr/bin/stop'\'' '\''hello'\'' TST2
 TEST  '\''/usr/bin/hello'\'''
case "${out}" in ${expected})
echo "SUCCESS (3)";;
*)
cat << EOF
FAILED: expected
${expected}
but got
${out}
EOF
esac

# Create a simulated init system layout.
create_simulated_init_tree()
{
    tmpdir="${1}"
    rm -rf "${tmpdir}"
    mkdir -m 0700 "${tmpdir}" || abort "Failed to create a temporary folder."
    mkdir -p "${tmpdir}/init.d/" "${tmpdir}/rc.d/init.d/"
    for i in 0 1 2 3 4 5 6; do
        mkdir "${tmpdir}/rc${i}.d/" "${tmpdir}/rc.d/rc${i}.d/"
    done
    mkdir -p "${tmpdir}/runlevel/default" "${tmpdir}/runlevel/boot"
    mkdir "${tmpdir}/run"
}

# Test an init script installation.
print_line "installing an init script."
failed=""
# Create a simulated init system layout.
tmpdir="${tmpbase}0"
create_simulated_init_tree "${tmpdir}"
# Create the service binary.
test_service "${tmpdir}" "service"
# And install it.
scripts/install_service  --prefix "${tmpdir}" --enable -- --command "${tmpdir}/service" --arguments "test of my\ arguments" --description "My description" ||
    fail_msg "\"scripts/install_service\" failed."
# Check that the main service file was created as specified.
if test -x "${tmpdir}/init.d/service"; then
    grep "Short-Description: My description" "${tmpdir}/init.d/service" >/dev/null ||
        fail_msg "Description not set in \"${tmpdir}/init.d/service\""
else
    fail_msg "\"${tmpdir}/init.d/service\" not correctly created."
fi
test -x "${tmpdir}/init.d/rc.d/service" &&
    fail_msg "\"${tmpdir}/init.d/rc.d/service\" created but shouldn't have been."
# Try to start the service using the symbolic links which should have been
# created.
if "${tmpdir}/rc3.d/S20service" --prefix "${tmpdir}" --lsb-functions "" start >/dev/null 2>&1; then
    if grep "1: test 2: of 3: my arguments" "${tmpdir}/started" >/dev/null; then
        test -f "${tmpdir}/stopped" &&
            fail_msg "\"${tmpdir}/rc3.d/S20service\" stopped immediately."
    else
        fail_msg "\"${tmpdir}/rc3.d/S20service\" did not start correctly."
    fi
else
    fail_msg "could not start \"${tmpdir}/rc3.d/S20service\"."
fi
# Check the status.
"${tmpdir}/rc.d/rc5.d/S20service" --prefix "${tmpdir}" --lsb-functions "" status >/dev/null 2>&1 ||
    fail_msg "\"${tmpdir}/rc.d/rc5.d/S20service\" reported the wrong status."
# Try to stop the service using the symbolic links which should have been
# created.
if "${tmpdir}/rc.d/rc6.d/K80service" --prefix "${tmpdir}" --lsb-functions "" stop >/dev/null 2>&1; then
    test -f "${tmpdir}/stopped" ||
        echo "\"${tmpdir}/rc.d/rc6.d/K80service\" did not stop correctly."
else
    fail_msg "could not stop \"${tmpdir}/rc.d/rc6.d/K80service\"."
fi
# Check the status again - now it should be stopped.
"${tmpdir}/runlevel/service" --prefix "${tmpdir}" --lsb-functions "" status >/dev/null 2>&1 &&
    fail_msg "\"${tmpdir}/runlevel/service\" reported the wrong status."
# Final summary.
if test -n "${failed}"; then
    echo "${failed}"
else
    echo SUCCESS
fi

# Test an one shot init script installation.
print_line "installing a one shot init script."
failed=""
# Create a simulated init system layout.
tmpdir="${tmpbase}0"
create_simulated_init_tree "${tmpdir}"
# Create the command binary.
test_oneshot "${tmpdir}" "command"
# And install the script.
scripts/install_service --prefix "${tmpdir}" --enable -- --command "${tmpdir}/command" --arguments "start" --description "My description" --stop-command "${tmpdir}/command" --stop-arguments "stop" --one-shot ||
    fail_msg "\"scripts/install_service\" failed."
# Sanity test.
test -f "${tmpdir}/started" &&
    fail_msg "\"${tmpdir}/started\" already exists!"
# Try to start the service using the symbolic links which should have been
# created.
if "${tmpdir}/rc3.d/S20command" --prefix "${tmpdir}" --lsb-functions "" start >/dev/null 2>&1; then
    test -f "${tmpdir}/started" ||
        fail_msg "\"${tmpdir}/rc3.d/S20command\" did not start correctly."
else
    fail_msg "could not start \"${tmpdir}/rc3.d/S20command\"."
fi
# Try to stop the service using the symbolic links which should have been
# created.
if "${tmpdir}/rc.d/rc6.d/K80command" --prefix "${tmpdir}" --lsb-functions "" stop >/dev/null 2>&1; then
    test -f "${tmpdir}/started" &&
        echo "\"${tmpdir}/rc.d/rc6.d/K80command\" did not stop correctly."
else
    fail_msg "could not stop \"${tmpdir}/rc.d/rc6.d/K80command\"."
fi
# Final summary.
if test -n "${failed}"; then
    echo "${failed}"
else
    echo SUCCESS
fi

# Test an init script removal.
print_line "removing an init script."
failed=""
# Create a simulated init system layout.
tmpdir="${tmpbase}0"
create_simulated_init_tree "${tmpdir}"
# Create the service binary.
test_service "${tmpdir}" "service"
# Install it.
scripts/install_service --prefix "${tmpdir}" --enable -- --command "${tmpdir}/service" --arguments "test of my\ arguments" --description "My description" ||
    fail_msg "\"scripts/install_service\" failed."
# And remove it again.
scripts/install_service --prefix "${tmpdir}" --remove -- --command "${tmpdir}/service" ||
    fail_msg "\"scripts/install_service\" failed."
# After uninstallation this should be the only file left in the init tree.
rm "${tmpdir}/service"
test "x`find "${tmpdir}" -type f -o -type l`" = "x" ||
    fail_msg "not all files were removed."
# Final summary.
if test -n "${failed}"; then
    echo "${failed}"
else
    echo SUCCESS
fi

# Test an enabled init script update with --disable.
print_line "updating an enabled init script with --disable."
failed=""
# Create a simulated init system layout.
tmpdir="${tmpbase}1"
create_simulated_init_tree "${tmpdir}"
# Create the service binary.
test_service "${tmpdir}" "service"
# Install it.
scripts/install_service --prefix "${tmpdir}" --enable -- --command "${tmpdir}/service" --arguments "test of my\ arguments" --description "My description" ||
    fail_msg "\"scripts/install_service\" failed."
# Install it disabled without forcing.
scripts/install_service --prefix "${tmpdir}" --disable -- --command "${tmpdir}/service" --arguments "test of my\ arguments" --description "My description" ||
    fail_msg "\"scripts/install_service\" failed."
test "x`find "${tmpdir}"/rc*.d "${tmpdir}/runlevel" -type l | wc -l`" = "x15" ||
    fail_msg "links were removed on non-forced disable."
# Final summary.
if test -n "${failed}"; then
    echo "${failed}"
else
    echo SUCCESS
fi

# Test updating a disabled init script with --enable.
print_line "updating a disabled init script with --enable."
failed=""
# Create a simulated init system layout.
tmpdir="${tmpbase}2"
create_simulated_init_tree "${tmpdir}"
# Create the service binary.
test_service "${tmpdir}" "service"
# Install it.
scripts/install_service --prefix "${tmpdir}" --disable -- --command "${tmpdir}/service" --arguments "test of my\ arguments" --description "My description" ||
    fail_msg "\"scripts/install_service\" failed."
# Install it disabled without forcing.
scripts/install_service --prefix "${tmpdir}" --enable -- --command "${tmpdir}/service" --arguments "test of my\ arguments" --description "My description" ||
    fail_msg "\"scripts/install_service\" failed."
test "x`find "${tmpdir}"/rc*.d "${tmpdir}/runlevel" -type l`" = "x" ||
    fail_msg "files were installed on non-forced enable."
# Final summary.
if test -n "${failed}"; then
    echo "${failed}"
else
    echo SUCCESS
fi

# Test an enabled init script update with --force-disable.
print_line "updating an enabled init script with --force-disable."
failed=""
# Create a simulated init system layout.
tmpdir="${tmpbase}3"
create_simulated_init_tree "${tmpdir}"
# Create the service binary.
test_service "${tmpdir}" "service"
# Install it.
scripts/install_service --prefix "${tmpdir}" --enable -- --command "${tmpdir}/service" --arguments "test of my\ arguments" --description "My description" ||
    fail_msg "\"scripts/install_service\" failed."
# Install it disabled without forcing.
scripts/install_service --prefix "${tmpdir}" --force-disable -- --command "${tmpdir}/service" --arguments "test of my\ arguments" --description "My description" ||
    fail_msg "\"scripts/install_service\" failed."
test "x`find "${tmpdir}"/rc*.d "${tmpdir}/runlevel" -type l`" = "x" ||
    fail_msg "links were not removed on forced disable."
# Final summary.
if test -n "${failed}"; then
    echo "${failed}"
else
    echo SUCCESS
fi

# Test updating a disabled init script with --force-enable.
print_line "updating a disabled init script with --force-enable."
failed=""
# Create a simulated init system layout.
tmpdir="${tmpbase}4"
create_simulated_init_tree "${tmpdir}"
# Create the service binary.
test_service "${tmpdir}" "service"
# Install it.
scripts/install_service --prefix "${tmpdir}" --disable -- --command "${tmpdir}/service" --arguments "test of my\ arguments" --description "My description" ||
    fail_msg "\"scripts/install_service\" failed."
# Install it disabled without forcing.
scripts/install_service --prefix "${tmpdir}" --force-enable -- --command "${tmpdir}/service" --arguments "test of my\ arguments" --description "My description" ||
    fail_msg "\"scripts/install_service\" failed."
test "x`find "${tmpdir}"/rc*.d "${tmpdir}/runlevel" -type l | wc -l`" = "x15" ||
    fail_msg "files were not installed on forced enable."
# Final summary.
if test -n "${failed}"; then
    echo "${failed}"
else
    echo SUCCESS
fi
