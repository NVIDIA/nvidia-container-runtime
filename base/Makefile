# Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.

DOCKER ?= docker

GOLANG_VERSION := 1.9.4

.NOTPARALLEL:
.PHONY: all

all: xenial centos7 stretch

%: $(CURDIR)/Dockerfile.%
	$(DOCKER) build --build-arg GOLANG_VERSION="$(GOLANG_VERSION)" \
	                -t nvidia/build-base:$@ -f Dockerfile.$@ .
