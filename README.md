
zBasic
======

zBasic is a minimal BASIC interpreter for embedded environments, compiling to
under 4kB code on ARM THUMB. It is easily extended by writing C functions that
can be called from BASIC code.

The core variable type can be configured to fit the host system, zBasic
supports signed and unsigned integer or float/double variables.

The BASIC source is parsed and converted into byte code resulting in pretty
decent performance.

zBasic currently supports:

- direct execution from REPL or running from program lines
- flow control with if/then/else, for/next, goto, gosub/return

Strings and arrays are not yet supported.


Usage
=====

````
$ make
$ make test
$ ./zbasic < basic/mandel.bas
````

