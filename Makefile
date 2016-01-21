CURRENT_MAKEFILE_LIST := $(MAKEFILE_LIST)
TOPDIR_RELATIVE := $(dir $(firstword $(CURRENT_MAKEFILE_LIST)))
TOPDIR := $(shell cd "$(TOPDIR_RELATIVE)" && pwd)

include Makefile.inc

PACK_VERSION:=$(shell cd $(TOPDIR) && "$(SWIPL)" -g "['pack'],version(Version),write(Version),halt" -t 'halt(1)' 2>/dev/null)

all: lgp.$(SOEXT)

lgp.$(SOEXT): lg-source/$(LINK_GRAMMAR_BUILD_DIR)/lgp.$(SOEXT)
	@if ! cmp --quiet $< $@; then \
		echo cp $< $@; \
		cp $< $@; \
	fi

lg-source/$(LINK_GRAMMAR_BUILD_DIR)/lgp.pl: prolog/lgp.pl lg-source/$(LINK_GRAMMAR_BUILD_DIR)/
	@if ! cmp --quiet $< $@; then \
		echo cp $< $@; \
		cp $< $@; \
	fi

lg-source/$(LINK_GRAMMAR_BUILD_DIR)/lgp_lib_test.pl: tests/lgp_lib_test.pl lg-source/$(LINK_GRAMMAR_BUILD_DIR)/
	@if ! cmp --quiet $< $@; then \
		echo cp $< $@; \
		cp $< $@; \
	fi

lg-source/$(LINK_GRAMMAR_BUILD_DIR)/Makefile.swi-prolog-lg: src/Makefile.swi-prolog-lg-$(LINK_GRAMMAR_VERSION) lg-source/$(LINK_GRAMMAR_BUILD_DIR)/
	@if ! cmp --quiet $< $@; then \
		echo cp $< $@; \
		cp $< $@; \
	fi

lg-source/$(LINK_GRAMMAR_BUILD_DIR)/Makefile.topdir.inc:
	@echo "TOPDIR:=$(TOPDIR)" > $@

lg-source/$(LINK_GRAMMAR_BUILD_DIR)/lgp.c: src/lgp.c lg-source/$(LINK_GRAMMAR_BUILD_DIR)/
	@if ! cmp --quiet $< $@; then \
		echo cp $< $@; \
		cp $< $@; \
	fi

lg-source/$(LINK_GRAMMAR_BUILD_DIR)/lgp.h: src/lgp.h lg-source/$(LINK_GRAMMAR_BUILD_DIR)/
	@if ! cmp --quiet $< $@; then \
		echo cp $< $@; \
		cp $< $@; \
	fi

lg-source/$(LINK_GRAMMAR_BUILD_DIR)/lgp.$(SOEXT): lg-source/$(LINK_GRAMMAR_BUILD_DIR)/ patched-lg-source
	$(MAKE) -C lg-source/$(LINK_GRAMMAR_BUILD_DIR) -f Makefile.swi-prolog-lg lgp.$(SOEXT)

lg-source-archive-$(LINK_GRAMMAR_VERSION).tar.gz:
	@if ! wget "$(SRC_URL)" -O "lg-source-archive-$(LINK_GRAMMAR_VERSION).tar.gz"; then \
		echo "Could not download Link grammar sources from URL: \"$(SRC_URL)\". Please run make again or download this archive manually and save it into a file named lg-source-archive-$(LINK_GRAMMAR_VERSION).tar.gz" >&2; \
		exit 1; \
	fi

lg-source-$(LINK_GRAMMAR_VERSION)/: lg-source-archive-$(LINK_GRAMMAR_VERSION).tar.gz
	mkdir -p lg-source-$(LINK_GRAMMAR_VERSION)
	@if ! tar -C lg-source-$(LINK_GRAMMAR_VERSION) -xzf "lg-source-archive-$(LINK_GRAMMAR_VERSION).tar.gz"; then \
		rm -rf lg-source-$(LINK_GRAMMAR_VERSION); \
		echo "Could not extract \"lg-source-archive-$(LINK_GRAMMAR_VERSION).tar.gz\" into \"lg-source-$(LINK_GRAMMAR_VERSION)\"" >&2; \
	fi

patched-lg-source: lg-source-$(LINK_GRAMMAR_VERSION)/ lg-source/$(LINK_GRAMMAR_BUILD_DIR)/ patches/$(LINK_GRAMMAR_VERSION)/
	@echo "Checking files to copy and patches to apply"
	@EXP_MD5=`cat patches/$(LINK_GRAMMAR_VERSION)/*.patch 2>/dev/null | md5sum - | sed -e 's/^\([^[:blank:]][^[:blank:]]*\).*$$/\1/'`; \
	APPLIED_MD5=`cat $(LINK_GRAMMAR_APPLIED_PATCHES_DIR)/*.patch 2>/dev/null | md5sum - | sed -e 's/^\([^[:blank:]][^[:blank:]]*\).*$$/\1/'`; \
	if test -n "$$EXP_MD5" && test x"$$EXP_MD5" != x"$$APPLIED_MD5"; then \
		echo "Old sources applied patches differ from current patches, resetting source directory"; \
		rm -rf $(LINK_GRAMMAR_APPLIED_PATCHES_DIR) || exit 1; \
		$(MAKE) LINK_GRAMMAR_VERSION=$(LINK_GRAMMAR_VERSION) clean-source || exit 1; \
	fi
	$(MAKE) LINK_GRAMMAR_VERSION=$(LINK_GRAMMAR_VERSION) lg-source/$(LINK_GRAMMAR_BUILD_DIR)/Makefile.swi-prolog-lg \
	                                                     lg-source/$(LINK_GRAMMAR_BUILD_DIR)/Makefile.topdir.inc \
	                                                     lg-source/$(LINK_GRAMMAR_BUILD_DIR)/lgp.c \
	                                                     lg-source/$(LINK_GRAMMAR_BUILD_DIR)/lgp.h \
	                                                     lg-source/$(LINK_GRAMMAR_BUILD_DIR)/lgp.pl \
	                                                     lg-source/$(LINK_GRAMMAR_BUILD_DIR)/lgp_lib_test.pl
	$(MAKE) LINK_GRAMMAR_VERSION=$(LINK_GRAMMAR_VERSION) patch
	$(MAKE) LINK_GRAMMAR_VERSION=$(LINK_GRAMMAR_VERSION) $(LINK_GRAMMAR_APPLIED_PATCHES_DIR)

$(LINK_GRAMMAR_APPLIED_PATCHES_DIR): patches/$(LINK_GRAMMAR_VERSION)/
	@echo "Copying patch list"
	mkdir -p $(LINK_GRAMMAR_APPLIED_PATCHES_DIR)
	cp patches/$(LINK_GRAMMAR_VERSION)/*.patch $(LINK_GRAMMAR_APPLIED_PATCHES_DIR) || :

apply-patches: $(LINK_GRAMMAR_APPLIED_PATCHES_DIR)
	@for p in $(LINK_GRAMMAR_APPLIED_PATCHES_DIR)/*.patch; do \
		echo "Applying patch \"$$p\""; \
		patch -d "lg-source/$(LINK_GRAMMAR_BUILD_DIR)" -p1 < "$$p" || exit 1; \
	done;

patch:
	@if ! test -e $(LINK_GRAMMAR_APPLIED_PATCHES_DIR); then \
		echo "Applying all patches for version $(LINK_GRAMMAR_VERSION)"; \
		$(MAKE) LINK_GRAMMAR_VERSION=$(LINK_GRAMMAR_VERSION) apply-patches; \
	else \
		echo "Skipping patching (already applied)"; \
	fi

force-patch: clean-source patched-lg-source

lg-source/$(LINK_GRAMMAR_BUILD_DIR)/: lg-source-$(LINK_GRAMMAR_VERSION)/
	rm -rf lg-source || :
	ln -s lg-source-$(LINK_GRAMMAR_VERSION) lg-source

clean-source:
	rm -rf lg-source-$(LINK_GRAMMAR_VERSION)/

clean: clean-source
	rm -rf lg-source lg-source-archive-$(LINK_GRAMMAR_VERSION).*
	rm -f lgp.$(SOEXT)
ifneq ($(SWIPL_ARCH),)
	rm -f lib/$(SWIPL_ARCH)/lgp.$(SOEXT)
endif

check: lgp.$(SOEXT)
	$(MAKE) -C lg-source/$(LINK_GRAMMAR_BUILD_DIR) -f Makefile.swi-prolog-lg check

install: lgp.$(SOEXT)
	install -d $(LIB_TARGET_DIR)/
	install lgp.$(SOEXT) $(LIB_TARGET_DIR)/
	install -d data
	(DST=`pwd`/data/ && cd lg-source/$(LINK_GRAMMAR_DATA_DIR); cp -r * "$$DST")

ifeq ($(SWIPL_ARCH),)
$(TARGET_ZIP_PACKAGE):
	$(error Could not guess SWIPL_ARCH. Either force its value by setting variable SWIPL_ARCH or modify the PATH to be able to execute the swipl command (or provide its full path in variable SWIPL))
else
$(TARGET_ZIP_PACKAGE): lib/$(SWIPL_ARCH)/lgp.$(SOEXT)
	@echo Building package $(TARGET_ZIP_PACKAGE)
	find ./ -name '.git*' -prune -o -path './lg-source*' -prune -o -path './lgp.$(SOEXT)' -prune -o -type f -print0 | xargs -0 zip -u $@

lib/$(SWIPL_ARCH)/lgp.$(SOEXT): lgp.$(SOEXT)
	mkdir -p lib/$(SWIPL_ARCH)
	@if ! cmp --quiet $< $@; then \
		echo cp $< $@; \
		cp $< $@; \
	fi
endif

ifeq ($(PACK_VERSION),)
pack:
	$(error Could not retrieve PACK_VERSION from pack.pl file)
else
TARGET_ZIP_PACKAGE:=$(TOPDIR)/link_grammar_prolog-$(PACK_VERSION).zip
pack: $(TARGET_ZIP_PACKAGE)
endif

.PHONY: all patched-lg-source apply-patches patch force-patch clean-source clean check install pack
