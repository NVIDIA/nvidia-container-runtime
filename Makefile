# Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.

.PHONY: all

all: ubuntu16.04 debian9 centos7 amzn1

# Build all packages for a specific distribution.
ubuntu16.04: runtime-ubuntu16.04 hook-ubuntu16.04

debian9: runtime-debian9 hook-debian9

centos7: runtime-centos7 hook-centos7

amzn1: runtime-amzn1 hook-amzn1

base-%:
	make -C $(CURDIR)/base $*

hook-%: base-%
	make -C $(CURDIR)/hook $*

runtime-%: base-%
	make -C $(CURDIR)/runtime $*

# Build nvidia-container-runtime for specific versions of docker.
%-runtime-ubuntu16.04: base-ubuntu16.04
	make -C $(CURDIR)/runtime $*-ubuntu16.04

%-runtime-debian9: base-debian9
	make -C $(CURDIR)/runtime $*-debian9

%-runtime-centos7: base-centos7
	make -C $(CURDIR)/runtime $*-centos7

%-runtime-amzn1: base-amzn1
	make -C $(CURDIR)/runtime $*-amzn1
