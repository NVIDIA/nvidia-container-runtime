# Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.

DOCKER ?= docker

VERSION := 1.2.1
PKG_REV := 1

DIST_DIR  := $(CURDIR)/../dist

.NOTPARALLEL:
.PHONY: all

all: xenial centos7 stretch amzn1

%: $(CURDIR)/Dockerfile.%
	$(DOCKER) build --build-arg PKG_VERS="$(VERSION)" \
	                --build-arg PKG_REV="$(PKG_REV)" \
	                -t nvidia-container-runtime-hook:$@ -f Dockerfile.$@ .
	$(DOCKER) run --rm -v $(DIST_DIR)/$@:/dist:Z nvidia-container-runtime-hook:$@
