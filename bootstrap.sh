#!/bin/bash

#
#  bootstrap.sh
#  OpenFirmwareManager
#

#
#  Released under "The GNU General Public License (GPL-2.0)"
#
#  Copyright (c) 2021 cjiang. All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License as published by the
#  Free Software Foundation; either version 2 of the License, or (at your
#  option) any later version.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
#  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
#  for more details.
#
#  You should have received a copy of the GNU General Public License along
#  with this program; if not, write to the Free Software Foundation, Inc.,
#  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#

#
#  This script is supposed to quickly bootstrap OpenFirmwareManger for plugin building.
#  A compiled OpenFirmwareManager release will be bootstrapped in the working directory.
#
#  Latest version available at:
#  https://raw.githubusercontent.com/AppleBluetooth/OpenFirmwareManager/master/bootstrap.sh
#
#  Example usage:
#  src=$(/usr/bin/curl -Lfs https://raw.githubusercontent.com/AppleBluetooth/OpenFirmwareManager/master/bootstrap.sh) && eval "$src" || exit 1
#

REPO_PATH="AppleBluetooth/OpenFirmwareManager"
SDK_PATH="OpenFirmwareManager.kext"
SDK_CHECK_PATH="${SDK_PATH}/Contents/Resources/OpenFirmwareManager.h"

PROJECT_PATH="$(pwd)"
if [ $? -ne 0 ] || [ ! -d "${PROJECT_PATH}" ]; then
  echo "ERROR: Failed to determine working directory!"
  exit 1
fi

# Avoid conflicts with PATH overrides.
CURL="/usr/bin/curl"
GIT="/usr/bin/git"
GREP="/usr/bin/grep"
MKDIR="/bin/mkdir"
MV="/bin/mv"
RM="/bin/rm"
SED="/usr/bin/sed"
UNAME="/usr/bin/uname"
UNZIP="/usr/bin/unzip"
UUIDGEN="/usr/bin/uuidgen"
XCODEBUILD="/usr/bin/xcodebuild"

TOOLS=(
  "${CURL}"
  "${GIT}"
  "${GREP}"
  "${MKDIR}"
  "${MV}"
  "${RM}"
  "${SED}"
  "${UNAME}"
  "${UNZIP}"
  "${UUIDGEN}"
  "${XCODEBUILD}"
)

for tool in "${TOOLS[@]}"; do
  if [ ! -x "${tool}" ]; then
    echo "ERROR: Missing ${tool}!"
    exit 1
  fi
done

# Prepare temporary directory to avoid conflicts with other scripts.
# Sets TMP_PATH.
prepare_environment() {
  local ret=0

  local sys=$("${UNAME}") || ret=$?
  if [ $ret -ne 0 ] || [ "$sys" != "Darwin" ]; then
    echo "ERROR: This script is only meant to be used on Darwin systems!"
    return 1
  fi

  if [ -e "${SDK_PATH}" ]; then
    echo "ERROR: Found existing SDK directory ${SDK_PATH}, aborting!"
    return 1
  fi

  local uuid=$("${UUIDGEN}") || ret=$?
  if [ $ret -ne 0 ]; then
    echo "ERROR: Failed to generate temporary UUID with code ${ret}!"
    return 1
  fi

  TMP_PATH="/tmp/ofmtmp.${uuid}"
  if [ -e "${TMP_PATH}" ]; then
    echo "ERROR: Found existing temporary directory ${TMP_PATH}, aborting!"
    return 1
  fi

  "${MKDIR}" "${TMP_PATH}" || ret=$?
  if [ $ret -ne 0 ]; then
    echo "ERROR: Failed to create temporary directory ${TMP_PATH} with code ${ret}!"
    return 1
  fi

  cd "${TMP_PATH}" || ret=$?
  if [ $ret -ne 0 ]; then
    echo "ERROR: Failed to cd to temporary directory ${TMP_PATH} with code ${ret}!"
    "${RM}" -rf "${TMP_PATH}"
    return 1
  fi

  return 0
}

# Install manually compiled SDK for development builds.
install_compiled_sdk() {
  local ret=0

  echo "Installing compiled SDK..."

  echo "-> Cloning the latest version from master..."

  local url="https://github.com/${REPO_PATH}"
  "${GIT}" clone "${url}" -b "master" --depth=1 "tmp" || ret=$?
  if [ $ret -ne 0 ]; then
    echo "ERROR: Failed to clone repository with code ${ret}!"
    return 1
  fi

  echo "-> Building the latest SDK..."

  cd "tmp" || ret=$?
  if [ $ret -ne 0 ]; then
    echo "ERROR: Failed to cd to temporary directory tmp with code ${ret}!"
    return 1
  fi

  "${GIT}" clone "https://github.com/acidanthera/MacKernelSDK" -b "master" --depth=1 || ret=$?
  if [ $ret -ne 0 ]; then
    echo "ERROR: Failed to clone MacKernelSDK with code ${ret}!"
    return 1
  fi

  "${XCODEBUILD}" -configuration Debug -arch x86_64 || ret=$?

  if [ $ret -ne 0 ]; then
    echo "ERROR: Failed to compile the latest version with code ${ret}!"
    return 1
  fi

  echo "-> Installing compiled SDK..."

  if [ ! -d "build/Debug/${SDK_PATH}" ] || [ ! -f "build/Debug/${SDK_CHECK_PATH}" ]; then
    echo "ERROR: Failed to find the built SDK!"
    return 1
  fi

  "${MV}" "build/Debug/${SDK_PATH}" "${PROJECT_PATH}/${SDK_PATH}" || ret=$?
  if [ $ret -ne 0 ]; then
    echo "ERROR: Failed to install SDK with code ${ret}!"
    return 1
  fi

  echo "Installed compiled SDK from master!"
}

prepare_environment || exit 1

ret=0
install_compiled_sdk || ret=$?

cd "${PROJECT_PATH}" || ret=$?

"${RM}" -rf "${TMP_PATH}"

if [ $ret -ne 0 ]; then
  echo "ERROR: Failed to bootstrap SDK with code ${ret}!"
  exit 1
fi
