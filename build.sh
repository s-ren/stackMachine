#!/bin/bash
set -e
cmake -B build
cmake --build build --target install
ctest --test-dir build --output-on-failure
