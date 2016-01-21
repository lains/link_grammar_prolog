Link grammar binding for SWI-Prolog
===================================

This library provides a SWI-Prolog binding to the Link Grammar library

The Link Grammar library is a syntactic parser based on link grammar.
Given a sentence, the system assigns to it a syntactic structure, which consists of a set of labeled links connecting pairs of words.
The parser also produces a "constituent" representation of a sentence (showing noun phrases, verb phrases, etc.).

This document details how to build from sources

## Prerequisites

First, you will need to install SWI-Prolog in order to use its static library and headers during compilation.
On Debian or Ubuntu, this can be done by running:
```
apt-get install swi-prolog
```

On Windows, you will have to install SWI-Prolog for Windows.

## Compiling the foreign library from the sources

On Linux, you will need the standard build tools (make, gcc etc...)

On Windows, you will have to install:
* either Cygwin (and always use the cygwin*.dll library altogether with the binding library)
* or MingWin and compile natively for Windows API

### Fetching the source code

Checkout a fresh copy if the link grammar binding for SWI-Prolog:
```
git clone https://github.com/lains/link_grammar_prolog.git
```

### Compilation and installation from the SWI-Prolog CLI

Once the sources have been extracted, you can run SWI-Prolog's built-in packager, from the root of the sources:
```
swipl -g "pack_install('.'),halt" -t 'halt(1)'
```

### Compilation from the shell

Once the sources have been extracted, you can run build the foreign library, from the root of the sources:
```
make
```

### Alternative compilation using static linking to the libpl.a library

Once installed, look for the static library `libpl.a` or `libswipl.a`, on Debian/Ubuntu:
```
dpkg -S libpl.a;dpkg -S libswipl.a
```
Write down *the absolute PATH* to the .a file, and set the environment variable `STATIC_SWILIBPL` to point to this file, for example:
```
export STATIC_SWILIBPL=/usr/lib/libswipl.a
```
Now, look for the header file `SWI-Prolog.h`, on Debian/Ubuntu:
```
dpkg -S SWI-Prolog.h
```
Write down the *directory* containing the file `SWI-Prolog.h`, and set the environment variable `SWIINC` to point to this file, for example:
```
export SWIINC=/usr/lib/swi-prolog/include
```

Under Windows, setting the variable `SWIHOME` might be enough for the Makefile to automatically guess the value for `STATIC_SWILIBPL` and `SWIINC`.

Once `STATIC_SWILIBPL` and `SWIINC` environment variables are set properly, from the root of the sources, run:
```
make
```

This will lead to the creation of the shared library `lgp.so` or `lgp.dll` in the root of the repository.
This file is the C-library part of the binding (the foreign library in SWI-Prolog terms).
This shared library will be needed by SWI-Prolog at run time (it is used by the Prolog engine when loading the Prolog binding module "lgp_lib.pl")

You will have to copy this file into a folder where SWI-Prolog will search for shared libraries.
In case Cygwin is in used, you may also have to copy over the Cygwin library (`cygwin*.dll`)
The `lgp_lib.pl` file (provided in the folder `SWI-Prolog_home_dir/` within the source) will also probably needs to be moved, or swpil has to be started from this exact folder in order to find `lgp_lib.pl`.
Typically, this is the SWI-Prolog's home directory.

## Running unit tests

From the top directory, you can run unit tests:
```
make check
```

## Creating an archive that can be distributed

You can create a .zip file that contains all the library (foreign library + prolog code), by running the following command, from the root of the sources:
```
make pack
```
