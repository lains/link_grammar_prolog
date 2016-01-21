Link grammar binding for SWI-Prolog
===================================

This library provides a SWI-Prolog binding to the Link Grammar library

The Link Grammar library is a syntactic parser based on link grammar.
Given a sentence, the system assigns to it a syntactic structure, which consists of a set of labeled links connecting pairs of words.
The parser also produces a "constituent" representation of a sentence (showing noun phrases, verb phrases, etc.).

## Installation of the library (prolog module and foreign library)

You can either download the sources and install the package (library) manually or install the package online from Github:

### Online installation from the SWI-Prolog official known pack list

This is the easiest way to install.

Open the SWI-Prolog command-line interface and type:
```
?- pack_install('link_grammar_prolog').
```

If this fails, you can try to install from a Github archive (see below)

### Online installation from Github

First, check the latest release of the package by [looking at the release page](https://github.com/lains/link_grammar_prolog/releases)

Write down the URL of the archive download (not the source code archive, but directly package file named `link_grammar_prolog-*.zip`)

Open the SWI-Prolog command-line interface and type:
```
?- pack_install('https://github.com/lains/link_grammar_prolog/releases/download/link_grammar_prolog-*/link_grammar_prolog-*.zip').
```

Note: replace the above line by the correct URL you wrote down at the previous step.

### Offline installation or compilation from sources

To install from sources, please refer to [the build instructions](BUILD.md)

### Checking the pack has been installed

Once the compilation succeeds, the library should be installed. You can check this from the Prolog prompt:
```
?- pack_list_installed.
```
This should return (among other possible packages), the link-grammar-binding package.

### Knowledge files

The Link grammar library requires several database files and directories in order to run.
This data directory contains:
* `4.0.dict`
* `4.0.knowledge`
* `4.0.constituent-knowledge`
* `4.0.affix`
* `words`

In order to load the provided data (dictonary, rules, knowledge etc...), you will have to specify in which directory the database files are located.
You will have to either copy over theses files/directories and their content inside the current directory at the moment the lgp_lib:create_dictionary/5 predicate is called, or create a symbolic link or provide valid PATHs (relative/absolute) to these files when running the create_dictionary/5 predicate.

#### `data` files when compiling from sources

If you recompile the package from source, the data files are downloaded as part of the build process and the `data` directory is populated in the pack.

you will find these files/directories in the subdirectory `data/` under the directory where the pack has been installed.

To find out the location of this directory, just type the following on the SWI-Prolog prompt:
```
?- pack_property('link_grammar_prolog', directory(Pack_dir)),atom_concat(Pack_dir, '/data', Data_dir).
```

The result will be displayed as `Data_dir`

#### Getting `data` files when doing an online installation

When using the online installation, you will have to manually download [the data directory directly from the original sources of the Link Grammar library](http://www.link.cs.cmu.edu/link/ftp-site/link-grammar/link-4.1b/unix/link-4.1b/data/) and save it locally before you can use create_dictionary/5 below.

## Using the library

In order to load the library, just do:
```
?- use_module(library(lgp)).
```

You can now either move to the `data` directory or you will have to use an absolute/relative path to the 4 files provided as argument of create_dictionary/5:

Now, you can create a dictionary and start parsing sentences:
```
?- lgp:create_dictionary('4.0.dict', '4.0.knowledge', '4.0.constituent-knowledge', '4.0.affix', Dictionary_handle).
```

For detailed instructions, on how to use the current Link Grammar (v4.1b) please refer to
[the documentation embedded inside the sources](patches/4.1b//README.md)
