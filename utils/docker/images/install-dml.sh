#!/usr/bin/env bash
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

#
# install-dml.sh - installs DML library
#

set -e

git clone https://github.com/intel/DML
cd DML

# DML v0.1.6-beta
git checkout v0.1.6-beta

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DDML_HW=ON ..
cmake --build . --target install

cd ../../
rm -rf DML
