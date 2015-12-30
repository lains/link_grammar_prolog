Link grammar binding for SWI-Prolog
===================================

This project provides a SWI-Prolog binding to the Link Grammar library

## Compilation instructions

### Dependencies and PATHs

First, you will need to install SWI-Prolog in order to use its static library and headers during compilation.
On Debian or Ubuntu, this can be done by running:
```
apt-get install swi-prolog
```

On Windows, you will have to install SWI-Prolog for Windows.

Once installed, look for the static library `libpl.a` or `libswipl.a`, on Debian/Ubuntu:
```
dpkg -S libpl.a;dpkg -S libswipl.a
```
Write down *the absolute PATH* to the .a file, and set the environment variable `SWILIBPL` to point to this file, for example:
```
export SWILIBPL=/usr/lib/libswipl.a
```
Now, look for the header file `SWI-Prolog.h`, on Debian/Ubuntu:
```
dpkg -S SWI-Prolog.h
```
Write down the *directory* containing the file `SWI-Prolog.h`, and set the environment variable `SWIINC` to point to this file, for example:
```
export SWIINC=/usr/lib/swi-prolog/include
```

Under Windows, setting the variable `SWIHOME` might be enough for the Makefile to automatically guess the value for `SWILIBPL` and `SWIINC`.

### Fetching the source code

Checkout a fresh copy if the link grammar binding for SWI-Prolog:
```git clone https://github.com/lains/swi-prolog-lg.git```

### Compilation

For Windows, you will have to install:
* either Cygwin (and always use the cygwin*.dll library altogether with the binding library)
* or MingWin and compile natively for Windows API

Once `SWILIBPL` and `SWIINC` environment variables are set properly, from the root of the sources, run:
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


### Installation of knowledge files

The Link grammar library requires several database files and directories in order to run.
You will find these files/directories wihin the sources (once make has completed successfully) in the directory `lg-source/link*/data`:

* `4.0.dict`
* `4.0.knowledge`
* `4.0.constituent-knowledge`
* `4.0.affix`
* `words`

You will have to either copy over theses files/directories and their content inside the folder `SWI-Prolog_home_dir/`, or create a symbolic link or provide a valid PATH to this file when running the lgp_lib:create_dictionary/5 command.

### Running unit tests

From the top directory, you can run unit tests:
```
make check
```

### Usage

See the file `Installation_notes_lgp_dll.txt` inside this repository.