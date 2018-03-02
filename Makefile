# Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.

.PHONY: all

all: xenial centos7 stretch amzn1

# Build all packages for a specific distribution.
xenial: runtime-xenial hook-xenial

centos7: runtime-centos7 hook-centos7

stretch: runtime-stretch hook-stretch

amzn1: runtime-amzn1 hook-amzn1

base-%: $(CURDIR)/base/Dockerfile.%
	make -C $(CURDIR)/base $*

hook-%: base-% $(CURDIR)/hook/Dockerfile.%
	make -C $(CURDIR)/hook $*

runtime-%: base-% $(CURDIR)/runtime/Dockerfile.%
	make -C $(CURDIR)/runtime $*

# Build nvidia-container-runtime for specific versions of docker.
%-runtime-xenial: base-xenial
	make -C $(CURDIR)/runtime $*-xenial

%-runtime-stretch: base-stretch
	make -C $(CURDIR)/runtime $*-stretch

%-runtime-centos7: base-centos7
	make -C $(CURDIR)/runtime $*-centos7

%-runtime-amzn1: base-amzn1
	make -C $(CURDIR)/runtime $*-amzn1
