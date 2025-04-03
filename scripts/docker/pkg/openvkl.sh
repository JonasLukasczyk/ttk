require-pkgs \
    libtbb-dev

fetch-src https://github.com/openvkl/openvkl/archive/v1.1.0.tar.gz

cmake-default \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DBUILD_BENCHMARKS=OFF        \
    -DBUILD_EXAMPLES=OFF          \
    -DBUILD_TESTING=OFF
