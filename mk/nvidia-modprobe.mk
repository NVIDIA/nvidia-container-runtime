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

LIB_STATIC     := $(MODPROBE_UTILS)/libnvidia-modprobe-utils.a
LIB_INCS       := $(MODPROBE_UTILS)/nvidia-modprobe-utils.h \
                  $(MODPROBE_UTILS)/pci-enum.h
LIB_SRCS       := $(MODPROBE_UTILS)/nvidia-modprobe-utils.c \
                  $(MODPROBE_UTILS)/pci-sysfs.c

##### Flags definitions #####

ARFLAGS  := -rU
CPPFLAGS += -D_FORTIFY_SOURCE=2 -DNV_LINUX
CFLAGS   += -fdata-sections -ffunction-sections -fstack-protector -fPIC -O2

##### Private rules #####

LIB_OBJS := $(LIB_SRCS:.c=.o)

$(SRCS_DIR)/.download_stamp:
	$(MKDIR) -p $(SRCS_DIR)
	$(CURL) --progress-bar -fSL $(URL) | \
	$(TAR) -C $(SRCS_DIR) --strip-components=1 -xz $(PREFIX)/modprobe-utils
	@touch $@

$(LIB_SRCS): $(SRCS_DIR)/.download_stamp

##### Public rules #####

.PHONY: all install clean

all: $(LIB_STATIC)($(LIB_OBJS))

install: all
	$(INSTALL) -d -m 755 $(addprefix $(DESTDIR),$(includedir) $(libdir))
	$(INSTALL) -m 644 $(LIB_INCS) $(DESTDIR)$(includedir)
	$(INSTALL) -m 644 $(LIB_STATIC) $(DESTDIR)$(libdir)

clean:
	$(RM) $(LIB_OBJS) $(LIB_STATIC)
