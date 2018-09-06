#
# Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
#

include $(MAKE_DIR)/common.mk

##### Source definitions #####

VERSION  := 1.1.4
PREFIX   := libtirpc-$(VERSION)
URL      := https://downloads.sourceforge.net/project/libtirpc/libtirpc/$(VERSION)/$(PREFIX).tar.bz2

SRCS_DIR := $(DEPS_DIR)/src/$(PREFIX)
LIBTIRPC := $(SRCS_DIR)/libtirpc

##### Flags definitions #####

export CPPFLAGS := -D_FORTIFY_SOURCE=2
export CFLAGS   := -O2 -g -fdata-sections -ffunction-sections -fstack-protector -fno-strict-aliasing -fPIC

##### Private rules #####

$(SRCS_DIR)/.download_stamp:
	$(MKDIR) -p $(SRCS_DIR)
	$(CURL) --progress-bar -fSL $(URL) | \
	$(TAR) -C $(SRCS_DIR) --strip-components=1 -xj
	@touch $@

$(SRCS_DIR)/.build_stamp: $(SRCS_DIR)/.download_stamp
	cd $(SRCS_DIR) && ./configure --prefix=$(DESTDIR)$(prefix) --enable-static --disable-shared --disable-gssapi --with-pic
	$(MAKE) -C $(SRCS_DIR)
	@touch $@

##### Public rules #####

.PHONY: all install clean

all: $(SRCS_DIR)/.build_stamp

unexport DESTDIR
install: all
	$(MAKE) -C $(SRCS_DIR) install

clean:
	$(RM) $(SRCS_DIR)/.build_stamp
	$(MAKE) -C $(SRCS_DIR) clean
