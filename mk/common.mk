#
# Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
#

export prefix      ?= /usr/local
export exec_prefix ?= $(prefix)
export libdir      ?= $(exec_prefix)/lib
export libdbgdir   ?= $(libdir)/debug/$(libdir)
export includedir  ?= $(prefix)/include
export pkgconfdir  ?= $(libdir)/pkgconfig

MV       ?= mv -f
LN       ?= ln
M4       ?= m4
TAR      ?= tar
CURL     ?= curl
MKDIR    ?= mkdir
LDCONFIG ?= ldconfig
INSTALL  ?= install
STRIP    ?= strip
OBJCPY   ?= objcopy
RPCGEN   ?= rpcgen
BMAKE    ?= MAKEFLAGS= bmake

getdef = $(shell sed -n "0,/$(1)/s/\#define\s\+$(1)\s\+\(\w*\)/\1/p" $(2))
