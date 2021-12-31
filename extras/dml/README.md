This directory contains DML implementation *miniasync* library (*miniasync-dml*
library).

The DML library is required to compile *miniasync-dml* library.
By default DML compiles with software path only. If you want to make use of
hardware path, make sure that DML is installed with ```DML_HW``` option.

To compile *miniasync-dml* with hardware path, set ```DML_HW_PATH``` flag to
*ON*. By default, *miniasync-dml* compiles with software path.

Compiling *miniasync-dml* with default, software path:
```shell
$ cmake .. -DCOMPILE_DML=ON
```

Compiling *miniasync-dml* with hardware path:
```shell
$ cmake .. -DCOMPILE_DML=ON -DDML_HW_PATH=ON
```
