CURRENT_MAKEFILE_LIST := $(MAKEFILE_LIST)

LINK_GRAMMAR_VERSION?=4.1b

UNAME       = $(shell uname -s 2>/dev/null | tr 'A-Z' 'a-z' | sed -e 's/^cygwin.*$$/cygwin/' || echo unknown)
ifeq ($(UNAME),linux)
LIBEXT      = so
LIBPL       ?= $(SWIHOME)/lib/i386/libpl.a
else
ifeq ($(UNAME),cygwin)
LIBEXT      = dll
LIBPL       ?= $(SWIHOME)/bin/LIBPL.DLL
else
$(error Unsupported platform)
endif
endif

ifeq ($(LINK_GRAMMAR_VERSION),4.1b)
SRC_URL?=http://www.link.cs.cmu.edu/link/ftp-site/link-grammar/link-4.1b/unix/link-4.1b.tar.gz
LINK_GRAMMAR_BUILD_DIR?=link-4.1b
endif

TOPDIR := $(dir $(firstword $(CURRENT_MAKEFILE_LIST)))

all: lgp.$(LIBEXT)

lgp.$(LIBEXT): patched_link_grammar
	$(MAKE) -C lg-source/$(LINK_GRAMMAR_BUILD_DIR) -f Makefile.swi-prolog-lg lgp.$(LIBEXT)
	cp lg-source/$(LINK_GRAMMAR_BUILD_DIR)/lgp.$(LIBEXT) .

lg-source/$(LINK_GRAMMAR_BUILD_DIR)/lgp_lib.pl: pl/lgp_lib.pl
	cp $^ $@

lg-source/$(LINK_GRAMMAR_BUILD_DIR)/lgp_lib_test.pl: pl/lgp_lib_test.pl
	cp $^ $@

patched_link_grammar: patches/$(LINK_GRAMMAR_VERSION)
	@EXP_MD5=`cat patches/$(LINK_GRAMMAR_VERSION)/*.patch 2>/dev/null | md5sum - | sed -e 's/^\([^[:blank:]][^[:blank:]]*\).*$$/\1/'`; \
	APPLIED_MD5=`cat .applied_patches/*.patch 2>/dev/null | md5sum - | sed -e 's/^\([^[:blank:]][^[:blank:]]*\).*$$/\1/'`; \
	if test -n "$$EXP_MD5" && test x"$$EXP_MD5" != x"$$APPLIED_MD5"; then \
		echo "Resetting patches to be applied to source"; \
		rm -rf .applied_patches/ 2>/dev/null; \
		mkdir .applied_patches/; \
		cp patches/$(LINK_GRAMMAR_VERSION)/*.patch .applied_patches/; \
		$(MAKE) LINK_GRAMMAR_VERSION=$(LINK_GRAMMAR_VERSION) force_patch  lg-source/$(LINK_GRAMMAR_BUILD_DIR)/lgp_lib.pl lg-source/$(LINK_GRAMMAR_BUILD_DIR)/lgp_lib_test.pl || rm -rf .applied_patches/ 2>/dev/null; \
	fi

lg-source-archive-$(LINK_GRAMMAR_VERSION).tar.gz:
	wget "$(SRC_URL)" -O "lg-source-archive-$(LINK_GRAMMAR_VERSION).tar.gz"

lg-source-$(LINK_GRAMMAR_VERSION): lg-source-archive-$(LINK_GRAMMAR_VERSION).tar.gz
	mkdir lg-source-$(LINK_GRAMMAR_VERSION)
	tar -C lg-source-$(LINK_GRAMMAR_VERSION) -xvzf "lg-source-archive-$(LINK_GRAMMAR_VERSION).tar.gz" || rm -rf lg-source-$(LINK_GRAMMAR_VERSION)

force_patch: clean-source link_dir
	for p in .applied_patches/*.patch; do \
		patch -d "lg-source/$(LINK_GRAMMAR_BUILD_DIR)" -p1 < "$$p"; \
	done

link_dir: lg-source-$(LINK_GRAMMAR_VERSION)
	rm -f lg-source 2>/dev/null
	ln -s lg-source-$(LINK_GRAMMAR_VERSION) lg-source

clean-source:
	rm -rf lg-source-$(LINK_GRAMMAR_VERSION)/

clean: clean-source
	rm -f lg-source lg-source-archive-$(LINK_GRAMMAR_VERSION).*
	rm -rf .applied_patches/
