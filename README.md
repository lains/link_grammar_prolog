Link grammar binding for SWI-Prolog
===================================

This project provides a SWI-Prolog binding to the Link Grammar library

## Compilation instructions

### Dependencies and PATHs

First, you will need to install SWI-Prolog in order to use its static library and headers during compilation.
On Debian or Ubuntu, this can be done by running:
```apt-get install swi-prolog```

Once installed, look for the static library `libpl.a` or `libswipl.a`, on Debian/Ubuntu:
```dpkg -S libpl.a;dpkg -S libswipl.a```
Write down *the absolute PATH* to the .a file, and set the environment variable `SWILIBPL` to point to this file, for example:
```export SWILIBPL=/usr/lib/libswipl.a```
Now, look for the header file `SWI-Prolog.h`, on Debian/Ubuntu:
```dpkg -S SWI-Prolog.h```
Write down the *directory* containing the file `SWI-Prolog.h`, and set the environment variable `SWIINC` to point to this file, for example:
```export SWIINC=/usr/lib/swi-prolog/include```

Under Windows, setting the variable `SWIHOME` might be enough for the Makefile to automatically guess the value for `SWILIBPL` and `SWIINC`.

### Fetching the source code

Checkout a fresh copy if the link grammar binding for SWI-Prolog:
```git clone https://github.com/lains/swi-prolog-lg.git```

### Compilation

Once `SWILIBPL` and `SWIINC` environment variables are set properly, from the root of the sources, run:
```make```

This will lead to the creation of the shared library `lgp.so` or `lgp.dll` in the root of the sources.
This file is the C-library part of the binding (the foreign library in SWI-Prolog terms).

You will have to copy this file into the same folder as the `lgp_lib.pl` file provided in the folder `SWI-Prolog_home_dir/` within the source, and start prolog from there.

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

Once all database files have been setup, you can run unit tests:
```swipl <options> -g go,halt -t 'halt(1)'```

### Usage

See the file `Installation_notes_lgp_dll.txt` inside this repository.