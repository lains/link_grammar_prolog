LINK_GRAMMAR_VERSION?=4.1b

UNAME       = $(shell uname -s 2>/dev/null | tr 'A-Z' 'a-z' | sed -e 's/^cygwin.*$$/cygwin/' || echo unknown)
ifeq ($(UNAME),linux)
SOEXT              = so
SWIHOME            ?= /usr/lib/swi-prolog
SWIPL_LIBS         ?= -lpl
SWIPL_LDFLAGS      ?= -L$(SWIHOME)/lib
STATIC_LIBSWIPL    ?= $(SWIHOME)/lib/i386/libpl.a
else
ifeq ($(UNAME),cygwin)
SOEXT              = dll
SWIHOME            ?= /cygdrive/c/Program\ Files/swipl/
SWIPL_LIBS         ?= -lswipl
SWIPL_LDFLAGS      ?= -L$(SWIHOME)/lib
STATIC_LIBSWIPL    ?= $(SWIHOME)/lib/libswipl.dll.a
else
ifneq (,$(filter mingw%,$(UNAME)))
SOEXT              = dll
SWIHOME            ?= /c/Program\ Files/swipl/
SWIPL_LIBS         ?= -lswipl
SWIPL_LDFLAGS      ?= -L$(SWIHOME)/lib
STATIC_LIBSWIPL    ?= $(SWIHOME)/lib/libswipl.dll.a
else
$(error Unsupported platform $(UNAME))
endif
endif
endif

ifeq ($(TOPDIR),)
$(error Missing TOPDIR variable (required by Makefile.inc))
endif

SWIPL?=swipl
SWIPL_ARCH:=$(shell $(SWIPL) -g "current_prolog_flag(arch, Arch),write(Arch),halt" -t 'halt(1)' 2>/dev/null)

ifeq ($(LINK_GRAMMAR_VERSION),4.1b)
SRC_URL?=http://www.link.cs.cmu.edu/link/ftp-site/link-grammar/link-4.1b/unix/link-4.1b.tar.gz
LINK_GRAMMAR_BUILD_DIR?=link-4.1b
LINK_GRAMMAR_DATA_DIR?=$(LINK_GRAMMAR_BUILD_DIR)/data
endif
ifeq ($(LINK_GRAMMAR_VERSION),5.3.2)
SRC_URL?=https://github.com/opencog/link-grammar/archive/link-grammar-5.3.2.tar.gz
LINK_GRAMMAR_BUILD_DIR?=link-grammar-link-grammar-5.3.2
LINK_GRAMMAR_DATA_DIR?=$(LINK_GRAMMAR_BUILD_DIR)/data
endif

LINK_GRAMMAR_APPLIED_PATCHES_DIR=lg-source-$(LINK_GRAMMAR_VERSION)/.applied-patches/

ifeq ($(PACKSODIR),)
LIB_TARGET_DIR:=$(TOPDIR)/lib
else
LIB_TARGET_DIR?=$(PACKSODIR)
endif
