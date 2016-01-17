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

### Installation of the library (prolog module and foreign library)

You can either download the sources and install the package (library) manually or install the package online from Github:

#### Installation from Github

This is the easiest way to install.
First, check the latest release of the package by [looking at the release page](https://github.com/lains/link_grammar_prolog/releases)

Write down the URL of the archive download (not the source code archive, but directly package file named `link_grammar_prolog-*.zip`)

Open the SWI-Prolog command-line interface and type:
```
?- pack_install('https://github.com/lains/link_grammar_prolog/releases/download/link_grammar_prolog-*/link_grammar_prolog-*.zip').
```

Note: replace the above line by the correct URL you wrote down earlier.

#### Installation from the sources

For Windows, you will have to install:
* either Cygwin (and always use the cygwin*.dll library altogether with the binding library)
* or MingWin and compile natively for Windows API

##### Fetching the source code

Checkout a fresh copy if the link grammar binding for SWI-Prolog:
```git clone https://github.com/lains/swi-prolog-lg.git```

##### Installation from the sources

Once the sources have been extracted, you can run SWI-Prolog's built-in packager, from the root of the sources:
```
swipl -g "pack_install('.'),halt" -t 'halt(1)'
```

Once the compilation succeeds, the library should be installed. You can check this from the Prolog prompt:
```
?- pack_list_installed.
```
This should return (among other possible packages), the link-grammar-binding package.

### Installation of knowledge files

The Link grammar library requires several database files and directories in order to run.
You will find these files/directories wihin the sources (once make has completed successfully) in the directory `lg-source/link*/data`, but more conveniently, they are also copied in the `data/` folder during the `make install` stage.

* `4.0.dict`
* `4.0.knowledge`
* `4.0.constituent-knowledge`
* `4.0.affix`
* `words`

You will have to either copy over theses files/directories and their content inside the current directory at the moment the lgp_lib:create_dictionary/5 predicate is callde, or create a symbolic link or provide valid PATHs (relative/absolute) to these files when running the lgp_lib:create_dictionary/5 predicate.

### Using the library

In order to load the library, just do:
```
?- use_module(library(lgp)).
```

In order to load the provided data (dictonary, rules, knownledge etc...), you will have to specify in which directory the database files are located.
During the installation of the package, they have been copied over to `~/lib/swipl/pack/link_grammar_prolog/data`. You can either move to the data directory using the command below, or you will have to us absolute/relative path to these files when using lgp:create_dictionary/5:
```
?- cd('/path/to/link/grammar/data').
```

Now, you can create a dictionary and start parsing sentences:
```
?- lgp:create_dictionary('4.0.dict', '4.0.knowledge', '4.0.constituent-knowledge', '4.0.affix', Dictionary_handle).
```

For detailed instructions, on how to use the current Link Grammar (v4.1b) please refer to
[the documentation embedded inside the sources](patches/4.1b//README.md)

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

### Running unit tests

From the top directory, you can run unit tests:
```
make check
```

### Usage

See the file `Installation_notes_lgp_dll.txt` inside this repository.