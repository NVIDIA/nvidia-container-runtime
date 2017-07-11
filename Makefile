#
# Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
#

.PHONY: all utils shared static deps install uninstall dist depsclean mostlyclean clean distclean
.DEFAULT_GOAL := all

##### Global variables #####

WITH_TIRPC   ?= 0
WITH_SECCOMP ?= 1

##### Global definitions #####

export CUDA_DIR  ?= /usr/local/cuda-8.0
export SRCS_DIR  ?= $(CURDIR)/src
export DEPS_DIR  ?= $(CURDIR)/deps
export DIST_DIR  ?= $(CURDIR)/dist
export MAKE_DIR  ?= $(CURDIR)/mk
export DEBUG_DIR ?= $(CURDIR)/.debug

include $(MAKE_DIR)/common.mk

DATE     := $(shell date -u --iso-8601=minutes)
REVISION := $(shell git rev-parse HEAD)

ifeq ($(DATE),)
$(error Invalid date format)
endif
ifeq ($(REVISION),)
$(error Invalid commit hash)
endif

##### Source definitions #####

BUILD_DEFS   := $(SRCS_DIR)/build.h

LIB_INCS     := $(SRCS_DIR)/nvc.h
LIB_SRCS     := $(SRCS_DIR)/driver.c        \
                $(SRCS_DIR)/elftool.c       \
                $(SRCS_DIR)/error_generic.c \
                $(SRCS_DIR)/error.c         \
                $(SRCS_DIR)/ldcache.c       \
                $(SRCS_DIR)/nvc.c           \
                $(SRCS_DIR)/nvc_ldcache.c   \
                $(SRCS_DIR)/nvc_info.c      \
                $(SRCS_DIR)/nvc_mount.c     \
                $(SRCS_DIR)/nvc_container.c \
                $(SRCS_DIR)/options.c       \
                $(SRCS_DIR)/utils.c

# Order sensitive (see flags definitions)
LIB_RPC_SPEC := $(SRCS_DIR)/driver_rpc.x
LIB_RPC_SRCS := $(SRCS_DIR)/driver_rpc.h \
                $(SRCS_DIR)/driver_xdr.c \
                $(SRCS_DIR)/driver_svc.c \
                $(SRCS_DIR)/driver_clt.c

BIN_SRCS     := $(SRCS_DIR)/nvc_cli.c \
                $(SRCS_DIR)/error_generic.c \
                $(SRCS_DIR)/dsl.c \
                $(SRCS_DIR)/utils.c

LIB_SCRIPT   = $(SRCS_DIR)/$(LIB_NAME).lds
BIN_SCRIPT   = $(SRCS_DIR)/$(BIN_UTILS).lds

##### Target definitions #####

ARCH    := x86_64
MAJOR   := $(call getdef,NVC_MAJOR,$(LIB_INCS))
MINOR   := $(call getdef,NVC_MINOR,$(LIB_INCS))
PATCH   := $(call getdef,NVC_PATCH,$(LIB_INCS))
VERSION := $(MAJOR).$(MINOR).$(PATCH)

ifeq ($(MAJOR),)
$(error Invalid major version)
endif
ifeq ($(MINOR),)
$(error Invalid minor version)
endif
ifeq ($(PATCH),)
$(error Invalid patch version)
endif

BIN_UTILS   := nvidia-container-cli
LIB_NAME    := libnvidia-container
LIB_STATIC  := $(LIB_NAME).a
LIB_SHARED  := $(LIB_NAME).so.$(VERSION)
LIB_SONAME  := $(LIB_NAME).so.$(MAJOR)
LIB_SYMLINK := $(LIB_NAME).so
LIB_PKGCFG  := $(LIB_NAME).pc

##### Flags definitions #####

ARFLAGS  := -rU
CPPFLAGS += -D_GNU_SOURCE -D_FORTIFY_SOURCE=2
CFLAGS   += -std=gnu11 -O2 -g -fdata-sections -ffunction-sections -fstack-protector -fstrict-aliasing -fvisibility=hidden \
            -Wall -Wextra -Wcast-align -Wpointer-arith -Wmissing-prototypes -Wnonnull \
            -Wwrite-strings -Wlogical-op -Wformat=2 -Wmissing-format-attribute -Winit-self -Wshadow \
            -Wstrict-prototypes -Wunreachable-code -Wconversion -Wsign-conversion \
            -Wno-unknown-warning-option -Wno-format-extra-args -Wno-gnu-alignof-expression
LDFLAGS  += -Wl,-zrelro -Wl,-znow -Wl,-zdefs -Wl,--gc-sections

LIB_CPPFLAGS       = $(CPPFLAGS) -DNV_LINUX -isystem $(CUDA_DIR)/include -isystem $(DEPS_DIR)/usr/local/include -include $(BUILD_DEFS)
LIB_CFLAGS         = $(CFLAGS) -fPIC
LIB_LDLIBS_STATIC  = $(LDLIBS) -l:libelf.a -l:libnvidia-modprobe-utils.a
LIB_LDLIBS_SHARED  = $(LDLIBS) -ldl -lcap
LIB_LDFLAGS        = $(LDFLAGS) -L$(DEPS_DIR)/usr/local/lib -shared -Wl,-soname=$(LIB_SONAME)

ifeq ($(WITH_TIRPC), 1)
LIB_CPPFLAGS       += -isystem $(DEPS_DIR)/usr/local/include/tirpc -DWITH_TIRPC
LIB_LDLIBS_STATIC  += -l:libtirpc.a
LIB_LDLIBS_SHARED  += -lpthread
endif
ifeq ($(WITH_SECCOMP), 1)
LIB_CPPFLAGS       += -DWITH_SECCOMP
LIB_LDLIBS_SHARED  += -lseccomp
endif
LIB_LDLIBS         = $(LIB_LDLIBS_STATIC) $(LIB_LDLIBS_SHARED)

BIN_CPPFLAGS       = $(CPPFLAGS) -include $(BUILD_DEFS)
BIN_CFLAGS         = $(CFLAGS) -fPIE -flto
BIN_LDFLAGS        = $(LDFLAGS) -L. -pie
BIN_LDLIBS         = $(LDLIBS) -l:$(LIB_SHARED)

$(word 1,$(LIB_RPC_SRCS)): RPCGENFLAGS=-h
$(word 2,$(LIB_RPC_SRCS)): RPCGENFLAGS=-c
$(word 3,$(LIB_RPC_SRCS)): RPCGENFLAGS=-m
$(word 4,$(LIB_RPC_SRCS)): RPCGENFLAGS=-l

##### Private rules #####

BIN_OBJS       := $(BIN_SRCS:.c=.o)
LIB_OBJS       := $(LIB_SRCS:.c=.lo) $(patsubst %.c,%.lo,$(filter %.c,$(LIB_RPC_SRCS)))
LIB_STATIC_OBJ := $(SRCS_DIR)/$(LIB_STATIC:.a=.lo)
DEPENDENCIES   := $(BIN_OBJS:%.o=%.d) $(LIB_OBJS:%.lo=%.d)

$(BUILD_DEFS):
	@printf '#define BUILD_DATE     "%s"\n' '$(DATE)' >$(BUILD_DEFS)
	@printf '#define BUILD_FLAGS    "%s"\n' '$(CPPFLAGS) $(CFLAGS) $(LDFLAGS)' >>$(BUILD_DEFS)
	@printf '#define BUILD_REVISION "%s"\n' '$(REVISION)' >>$(BUILD_DEFS)

$(LIB_RPC_SRCS): $(LIB_RPC_SPEC)
	$(RM) $@
	cd $(dir $@) && $(RPCGEN) $(RPCGENFLAGS) -C -M -N -o $(notdir $@) $(LIB_RPC_SPEC)

$(LIB_OBJS): %.lo: %.c | deps
	$(CC) $(LIB_CFLAGS) $(LIB_CPPFLAGS) -MMD -MF $*.d -c $(OUTPUT_OPTION) $<

$(BIN_OBJS): %.o: %.c | shared
	$(CC) $(BIN_CFLAGS) $(BIN_CPPFLAGS) -MMD -MF $*.d -c $(OUTPUT_OPTION) $<

-include $(DEPENDENCIES)

$(LIB_SHARED): $(LIB_OBJS)
	$(MKDIR) -p $(DEBUG_DIR)
	$(CC) $(LIB_CFLAGS) $(LIB_CPPFLAGS) $(LIB_LDFLAGS) $(OUTPUT_OPTION) $^ $(LIB_SCRIPT) $(LIB_LDLIBS)
	$(OBJCPY) --only-keep-debug $@ $(LIB_SONAME)
	$(OBJCPY) --add-gnu-debuglink=$(LIB_SONAME) $@
	$(MV) $(LIB_SONAME) $(DEBUG_DIR)
	$(STRIP) --strip-unneeded $@

$(LIB_STATIC_OBJ): $(LIB_OBJS)
	$(LD) -d -r --exclude-libs ALL -L$(DEPS_DIR)/usr/local/lib $(OUTPUT_OPTION) $^ $(LIB_LDLIBS_STATIC)
	$(OBJCPY) --localize-hidden $@
	$(STRIP) --strip-unneeded $@

$(BIN_UTILS): $(BIN_OBJS)
	$(CC) $(BIN_CFLAGS) $(BIN_CPPFLAGS) $(BIN_LDFLAGS) $(OUTPUT_OPTION) $^ $(BIN_SCRIPT) $(BIN_LDLIBS)

##### Public rules #####

all: release

debug: CFLAGS += -pedantic -fsanitize=undefined -fno-omit-frame-pointer -fno-common
debug: STRIP  := @echo skipping: strip
debug: shared static utils

release: CPPFLAGS += -DNDEBUG
release: BIN_LDFLAGS += -Wl,--strip-all
release: shared static utils

utils: $(BIN_UTILS)

shared: $(LIB_SHARED)

static: $(LIB_STATIC)($(LIB_STATIC_OBJ))

deps: export DESTDIR=$(DEPS_DIR)
deps: $(LIB_RPC_SRCS) $(BUILD_DEFS)
	$(MKDIR) -p $(DEPS_DIR)
	$(MAKE) -f $(MAKE_DIR)/elftoolchain.mk install
	$(MAKE) -f $(MAKE_DIR)/nvidia-modprobe.mk install
ifeq ($(WITH_TIRPC), 1)
	$(MAKE) -f $(MAKE_DIR)/libtirpc.mk install
endif

install: all
	$(INSTALL) -d -m 755 $(addprefix $(DESTDIR),$(includedir) $(bindir) $(libdir) $(libdbgdir) $(pkgconfdir))
	# Install header files
	$(INSTALL) -m 644 $(LIB_INCS) $(DESTDIR)$(includedir)
	# Install library files
	$(INSTALL) -m 644 $(LIB_STATIC) $(DESTDIR)$(libdir)
	$(INSTALL) -m 755 $(LIB_SHARED) $(DESTDIR)$(libdir)
	$(LN) -sf $(LIB_SONAME) $(DESTDIR)$(libdir)/$(LIB_SYMLINK)
	$(LDCONFIG) -n $(DESTDIR)$(libdir)
	# Install debugging symbols
	$(INSTALL) -m 644 $(DEBUG_DIR)/$(LIB_SONAME) $(DESTDIR)$(libdbgdir)
	# Install configuration files
	$(M4) -D'$$VERSION=$(VERSION)' -D'$$PRIVATE_LIBS=$(LIB_LDLIBS_SHARED)' $(MAKE_DIR)/$(LIB_PKGCFG).m4 > $(DESTDIR)$(pkgconfdir)/$(LIB_PKGCFG)
	# Install binary files
	$(INSTALL) -m 755 $(BIN_UTILS) $(DESTDIR)$(bindir)

uninstall:
	# Uninstall header files
	$(RM) $(addprefix $(DESTDIR)$(includedir)/,$(notdir $(LIB_INCS)))
	# Uninstall library files
	$(RM) $(addprefix $(DESTDIR)$(libdir)/,$(LIB_STATIC) $(LIB_SHARED) $(LIB_SONAME) $(LIB_SYMLINK))
	# Uninstall debugging symbols
	$(RM) $(DESTDIR)$(libdbgdir)/$(LIB_SONAME)
	# Uninstall configuration files
	$(RM) $(DESTDIR)$(pkgconfdir)/$(LIB_PKGCFG)
	# Uninstall binary files
	$(RM) $(DESTDIR)$(bindir)/$(BIN_UTILS)

dist: DESTDIR=$(DIST_DIR)/$(LIB_NAME)_$(VERSION)
dist: install
	$(TAR) -C $(dir $(DESTDIR)) -caf $(DESTDIR)_$(ARCH).tar.xz $(notdir $(DESTDIR))
	$(RM) -r $(DESTDIR)

depsclean:
	$(RM) $(BUILD_DEFS)
	-$(MAKE) -f $(MAKE_DIR)/elftoolchain.mk clean
	-$(MAKE) -f $(MAKE_DIR)/nvidia-modprobe.mk clean
ifeq ($(WITH_TIRPC), 1)
	-$(MAKE) -f $(MAKE_DIR)/libtirpc.mk clean
endif

mostlyclean:
	$(RM) $(LIB_OBJS) $(LIB_STATIC_OBJ) $(BIN_OBJS) $(DEPENDENCIES)

clean: mostlyclean depsclean

distclean: clean
	$(RM) -r $(DEPS_DIR) $(DIST_DIR) $(DEBUG_DIR)
	$(RM) $(LIB_RPC_SRCS) $(LIB_STATIC) $(LIB_SHARED) $(BIN_UTILS)
