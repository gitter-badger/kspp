#!/usr/bin/env bash
rm -rf build bin lib
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_ROCKSDB=ON -DBUILD_SAMPLES=ON -DBUILD_TESTS=ON ..
make -j8 
cd ..

