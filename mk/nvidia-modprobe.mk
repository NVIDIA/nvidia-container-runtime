#
# Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
#

include $(MAKE_DIR)/common.mk

##### Source definitions #####

VERSION        := 381.22
PREFIX         := nvidia-modprobe-$(VERSION)
URL            := https://github.com/NVIDIA/nvidia-modprobe/archive/$(VERSION).tar.gz

SRCS_DIR       := $(DEPS_DIR)/src/$(PREFIX)
MODPROBE_UTILS := $(SRCS_DIR)/modprobe-utils

STATIC_LIB     := $(MODPROBE_UTILS)/libnvidia-modprobe-utils.a
SRCS           := $(MODPROBE_UTILS)/nvidia-modprobe-utils.c \
                  $(MODPROBE_UTILS)/pci-sysfs.c

##### Flags definitions #####

ARFLAGS  := -rU
CPPFLAGS += -D_FORTIFY_SOURCE=2 -DNV_LINUX
CFLAGS   += -fdata-sections -ffunction-sections -fstack-protector -fPIC -O2

##### Private targets #####

OBJS := $(SRCS:.c=.o)

$(SRCS_DIR)/.download_stamp:
	$(MKDIR) -p $(SRCS_DIR)
	$(CURL) --progress-bar -fSL $(URL) | \
	$(TAR) -C $(SRCS_DIR) --strip-components=1 -xz $(PREFIX)/modprobe-utils
	@touch $@

$(SRCS): $(SRCS_DIR)/.download_stamp

##### Public targets #####

.PHONY: all install clean

all: $(STATIC_LIB)($(OBJS))

install: all
	$(INSTALL) -D -m 644 -t $(DESTDIR)$(includedir) $(MODPROBE_UTILS)/nvidia-modprobe-utils.h
	$(INSTALL) -D -m 644 -t $(DESTDIR)$(includedir) $(MODPROBE_UTILS)/pci-enum.h
	$(INSTALL) -D -m 644 -t $(DESTDIR)$(libdir) $(STATIC_LIB)

clean:
	$(RM) $(OBJS) $(STATIC_LIB)
