#! /bin/bash

curl -L https://github.com/ospray/ospray/archive/v2.8.0.tar.gz | tar xz --strip-components 1

cmake -S . -B build \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DOSPRAY_ENABLE_APPS=OFF \
    ..

cmake --build build
cmake --install build
