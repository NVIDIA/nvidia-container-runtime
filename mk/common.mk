#
# Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
#

MV       ?= mv -f
CP       ?= cp -a
LN       ?= ln
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
