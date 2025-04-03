RKCOMMON_VERSION=1.8.0

require-pkgs \
    libtbb-dev

fetch-src https://github.com/ospray/rkcommon/archive/v${RKCOMMON_VERSION}.tar.gz

cmake-default \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DBUILD_TESTING=OFF                 \
    -DRKCOMMON_TASKING_SYSTEM=TBB
