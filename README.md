# **miniasync: Mini Asynchronous Library**

[![GHA build status](https://github.com/pmem/miniasync/workflows/On_Pull_Request/badge.svg?branch=master)](https://github.com/pmem/miniasync/actions)
[![Coverage Status](https://codecov.io/github/pmem/miniasync/coverage.svg?branch=master)](https://codecov.io/gh/pmem/miniasync/branch/master)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/pmem/miniasync.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/pmem/miniasync/context:cpp)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/24119/badge.svg)](https://scan.coverity.com/projects/pmem-miniasync)

The **Mini Library for Asynchronous Programming in C** is a C low-level concurrency library for asynchronous functions.
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

## Contact Us

For more information on this library, contact
Piotr Balcer (piotr.balcer@intel.com),
[Google group](https://groups.google.com/group/pmem).
