# **libuasync: Micro Asynchronous Library**

[![GHA build status](https://github.com/wlemkows/libuasync/workflows/On_Pull_Request/badge.svg?branch=master)](https://github.com/wlemkows/libuasync/actions)
[![Coverage Status](https://codecov.io/github/wlemkows/libuasync/coverage.svg?branch=master)](https://codecov.io/gh/wlemkows/libuasync/branch/master)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/wlemkows/libuasync.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/wlemkows/libuasync/context:cpp)

The **Micro Asynchronous Library** is a C low-level concurrency library for asynchronous functions.
For more information, see [pmem.io](https://pmem.io).

## Building

Requirements:
- C compiler
- cmake >= 3.3
- pkg_config

First, you have to create a `build` directory.
From there you have to prepare the compilation using CMake.
The final build step is just a `make` command.

```shell
$ mkdir build && cd build
$ cmake ..
$ make -j
```

### Building packages

In order to build 'rpm' or 'deb' packages you should issue the following commands:

```shell
$ mkdir build && cd build
$ cmake .. -DCPACK_GENERATOR="$GEN" -DCMAKE_INSTALL_PREFIX=/usr
$ make package
```

where $GEN is a type of package generator: RPM or DEB.

CMAKE_INSTALL_PREFIX must be set to a destination were packages will be installed

## Contact Us

For more information on this library, contact
Piotr Balcer (piotr.balcer@intel.com),
[Google group](https://groups.google.com/group/pmem).
