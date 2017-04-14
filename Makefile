#
# Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
#

.DEFAULT_GOAL := all

##### Global variables #####

WITH_TIRPC   ?= 0
WITH_SECCOMP ?= 1

##### Global definitions #####

export CUDA_DIR  ?= /usr/local/cuda
export DEPS_DIR  ?= $(CURDIR)/deps
export DIST_DIR  ?= $(CURDIR)/dist
export MAKE_DIR  ?= $(CURDIR)/mk
export DEBUG_DIR ?= $(CURDIR)/.debug

include $(MAKE_DIR)/common.mk

##### Source definitions #####

TARGET         := libnvidia-container
ARCH           := x86_64
MAJOR          := $(call getdef,NVC_MAJOR,nvc.h)
MINOR          := $(call getdef,NVC_MINOR,nvc.h)
REVISION       := $(call getdef,NVC_REVISION,nvc.h)
VERSION        := $(MAJOR).$(MINOR).$(REVISION)

STATIC_LIB     := $(TARGET).a
SHARED_LIB     := $(TARGET).so.$(VERSION)
SONAME         := $(TARGET).so.$(MAJOR)

VERSION_SCRIPT := nvc.sym
BUILD_INFO     := build.info
DRIVER_RPC     := driver_rpc.x

SRCS := driver.c        \
        elftool.c       \
        error.c         \
        ldcache.c       \
        nvc.c           \
        nvc_entry.c     \
        nvc_exec.c      \
        nvc_info.c      \
        nvc_mount.c     \
        nvc_container.c \
        options.c       \
        utils.c

RPCS := driver_rpc.h \
        driver_xdr.c \
        driver_svc.c \
        driver_clt.c

##### Flags definitions #####

ARFLAGS  := -rU
CPPFLAGS += -D_GNU_SOURCE -D_FORTIFY_SOURCE=2 -DNV_LINUX -MMD -MF .$*.d
CFLAGS   += -std=gnu11 -O2 -g -Wall -Wextra -Wcast-align -Wpointer-arith -Wmissing-prototypes -Wnonnull \
            -Wwrite-strings -Wlogical-op -Wformat=2 -Wmissing-format-attribute -Winit-self -Wshadow \
            -Wstrict-prototypes -Wunreachable-code -Wconversion -Wsign-conversion \
            -Wno-unknown-warning-option -Wno-format-extra-args -Wno-gnu \
            -fdata-sections -ffunction-sections -fstack-protector -fstrict-aliasing -fvisibility=hidden -fPIC
LDFLAGS  += -shared -Wl,-zrelro -Wl,-znow -Wl,-zdefs -Wl,--gc-sections -Wl,-soname=$(SONAME) -Wl,--version-script=$(VERSION_SCRIPT)

CPPFLAGS       += -isystem $(CUDA_DIR)/include -isystem $(DEPS_DIR)/usr/local/include
LDFLAGS        += -Wl,--entry=nvc_entry -Wl,--undefined=nvc_interp -L$(DEPS_DIR)/usr/local/lib
STATIC_LDLIBS  += -l:libelf.a -l:libnvidia-modprobe-utils.a
SHARED_LDLIBS  += -ldl -lcap
ifeq ($(WITH_TIRPC), 1)
CPPFLAGS       += -isystem $(DEPS_DIR)/usr/local/include/tirpc -DWITH_TIRPC
STATIC_LDLIBS  += -l:libtirpc.a
SHARED_LDLIBS  += -lpthread
endif
ifeq ($(WITH_SECCOMP), 1)
CPPFLAGS       += -DWITH_SECCOMP
SHARED_LDLIBS  += -lseccomp
endif
LDLIBS         += $(STATIC_LDLIBS) $(SHARED_LDLIBS)

$(word 1,$(RPCS)): RPCGENFLAGS=-h
$(word 2,$(RPCS)): RPCGENFLAGS=-c
$(word 3,$(RPCS)): RPCGENFLAGS=-m
$(word 4,$(RPCS)): RPCGENFLAGS=-l

##### Private targets #####

OBJS           := $(SRCS:.c=.o) $(patsubst %.c,%.o,$(filter %.c,$(RPCS)))
STATIC_LIB_OBJ := $(STATIC_LIB:.a=.o)

nvc.o nvc_entry.o: info
nvc.o nvc_entry.o: CPPFLAGS += -include $(BUILD_INFO)

$(RPCS): $(DRIVER_RPC)
	$(RM) $@
	$(RPCGEN) $(RPCGENFLAGS) -C -M -N -o $@ $(DRIVER_RPC)

$(OBJS): %.o: %.c | deps
	$(COMPILE.c) $(OUTPUT_OPTION) $<

-include $(OBJS:%.o=.%.d)

$(SHARED_LIB): $(OBJS)
	$(MKDIR) -p $(DEBUG_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	$(OBJCPY) --only-keep-debug $@ $(SONAME)
	$(OBJCPY) --add-gnu-debuglink=$(SONAME) $@
	$(MV) $(SONAME) $(DEBUG_DIR)
	$(STRIP) --strip-unneeded $@

$(STATIC_LIB_OBJ): $(OBJS)
	$(LD) -d -r --exclude-libs ALL -L$(DEPS_DIR)/usr/local/lib -o $@ $^ $(STATIC_LDLIBS)
	$(OBJCPY) --localize-hidden $@
	$(STRIP) --strip-unneeded $@

##### Public targets #####

.PHONY: all shared static deps info install uninstall dist depsclean mostlyclean clean distclean

all: release

debug: shared static

release: CPPFLAGS += -DNDEBUG
release: shared static

shared: $(SHARED_LIB)

static: $(STATIC_LIB)($(STATIC_LIB_OBJ))

deps: export DESTDIR=$(DEPS_DIR)
deps: $(RPCS)
	$(MKDIR) -p $(DEPS_DIR)
	$(MAKE) -f $(MAKE_DIR)/elftoolchain.mk install
	$(MAKE) -f $(MAKE_DIR)/nvidia-modprobe.mk install
ifeq ($(WITH_TIRPC), 1)
	$(MAKE) -f $(MAKE_DIR)/libtirpc.mk install
endif

info:
	@printf '#define BUILD_DATE   "%s"\n' '$(shell date -u --iso-8601=minutes)' >$(BUILD_INFO)
	@printf '#define SCM_REVISION "%s"\n' '$(shell git rev-parse HEAD)' >>$(BUILD_INFO)
	@printf '#define COMPILE_OPTS "%s"\n' '$(CFLAGS)' >>$(BUILD_INFO)

install: all
	$(INSTALL) -D -m 644 nvc.h $(DESTDIR)$(includedir)/nvc.h
	$(INSTALL) -D -m 644 $(STATIC_LIB) $(DESTDIR)$(libdir)/$(STATIC_LIB)
	$(INSTALL) -D -m 755 $(SHARED_LIB) $(DESTDIR)$(libdir)/$(SHARED_LIB)
	$(INSTALL) -D -m 755 $(DEBUG_DIR)/$(SONAME) $(DESTDIR)$(libdbgdir)/$(DEBUG_DIR)/$(SONAME)
	$(INSTALL) -d -m 755 $(DESTDIR)$(pkgconfdir)
	$(LDCONFIG) -n $(DESTDIR)$(libdir)
	$(LN) -sf $(SONAME) $(DESTDIR)$(libdir)/$(TARGET).so
	$(M4) -D'$$VERSION=$(VERSION)' -D'$$PRIVATE_LIBS=$(SHARED_LDLIBS)' $(MAKE_DIR)/$(TARGET).pc.m4 > $(DESTDIR)$(pkgconfdir)/$(TARGET).pc

uninstall:
	$(RM) $(addprefix $(DESTDIR)$(includedir)/,nvc.h)
	$(RM) $(addprefix $(DESTDIR)$(libdir)/,$(SHARED_LIB) $(STATIC_LIB) $(SONAME) $(TARGET).so)
	$(RM) $(DESTDIR)$(pkgconfdir)/$(TARGET).pc

dist: DESTDIR=$(DIST_DIR)/$(TARGET)_$(VERSION)
dist: install
	$(TAR) -C $(dir $(DESTDIR)) -caf $(DESTDIR)_$(ARCH).tar.xz $(notdir $(DESTDIR))
	$(RM) -r $(DESTDIR)

depsclean:
	-$(MAKE) -f $(MAKE_DIR)/elftoolchain.mk clean
	-$(MAKE) -f $(MAKE_DIR)/nvidia-modprobe.mk clean
ifeq ($(WITH_TIRPC), 1)
	-$(MAKE) -f $(MAKE_DIR)/libtirpc.mk clean
endif

mostlyclean:
	$(RM) -r $(DEBUG_DIR)
	$(RM) $(OBJS) $(STATIC_LIB_OBJ) $(OBJS:%.o=.%.d)

clean: mostlyclean depsclean

distclean: clean
	$(RM) -r $(DEPS_DIR) $(DIST_DIR) $(DEBUG_DIR)
	$(RM) $(RPCS) $(BUILD_INFO) $(STATIC_LIB) $(SHARED_LIB)
