TBB_VERSION=2022.1.0

# get source code
fetch-src https://github.com/oneapi-src/oneTBB/archive/refs/tags/v${TBB_VERSION}.tar.gz

cmake-default \
    -DTBB_TEST=OFF
