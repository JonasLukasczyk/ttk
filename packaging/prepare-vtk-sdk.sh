#!/usr/bin/env bash
set -euxo pipefail

VTK_VERSION=9.6.2
PYTHON_TAG=cp312-cp312
PLATFORM_TAG=manylinux2014_x86_64.manylinux_2_17_x86_64

VTK_PREFIX=/opt/vtk-sdk

SDK_NAME="vtk-wheel-sdk-${VTK_VERSION}-${PYTHON_TAG}-${PLATFORM_TAG}"
SDK_URL="https://vtk.org/files/release/9.6/${SDK_NAME}.tar.xz"

rm -rf "${VTK_PREFIX}"
rm -rf /tmp/vtk-sdk-extract
rm -f /tmp/vtk-sdk.tar.xz

mkdir -p "${VTK_PREFIX}"
mkdir -p /tmp/vtk-sdk-extract

curl -fL \
  "${SDK_URL}" \
  -o /tmp/vtk-sdk.tar.xz

tar -xJf /tmp/vtk-sdk.tar.xz \
  -C /tmp/vtk-sdk-extract

TOPDIR="$(
  find /tmp/vtk-sdk-extract \
    -mindepth 1 \
    -maxdepth 1 \
    -type d \
    -print \
    -quit
)"

cp -a "${TOPDIR}/." "${VTK_PREFIX}/"

echo "VTK CMake directory:"
find "${VTK_PREFIX}" \
  -type d \
  -path '*/headers/cmake' \
  -print
