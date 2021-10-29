# **libuasync: Micro Asynchronous Library**

[![GHA build status](https://github.com/pmem/libuasync/workflows/libuasync/badge.svg?branch=master)](https://github.com/pmem/libuasync/actions)
[![Coverage Status](https://codecov.io/github/pmem/libuasync/coverage.svg?branch=master)](https://codecov.io/gh/wlemkows/libuasync/branch/master)

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

## Contact Us

For more information on this library, contact
Piotr Balcer (piotr.balcer@intel.com),
[Google group](https://groups.google.com/group/pmem).
