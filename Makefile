# Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.

BASE := nvidia/base
BASE_DEPENDENCY := base-%

.PHONY: all

all: base ubuntu18.04 ubuntu16.04 debian9 centos7 amzn2 amzn1

verify: fmt lint vet

fmt:
	find . -not \( \( -wholename './.*' -o -wholename '*/vendor/*' \) -prune \) -name '*.go' \
		| sort -u | xargs gofmt -s -l

lint:
	find . -not \( \( -wholename './.*' -o -wholename '*/vendor/*' \) -prune \) -name '*.go' \
		| sort -u | xargs golint

vet:
	go list ./... | grep -v "vendor" | xargs go vet

runtime: runtime-ubuntu18.04 runtime-ubuntu16.04 runtime-debian9 runtime-centos7 runtime-amzn2 runtime-amzn1

toolkit: toolkit-ubuntu18.04 toolkit-ubuntu16.04 toolkit-debian9 toolkit-centos7 toolkit-amzn2 toolkit-amzn1

# Build all packages for a specific distribution.
ubuntu18.04: runtime-ubuntu18.04 toolkit-ubuntu18.04

ubuntu16.04: runtime-ubuntu16.04 toolkit-ubuntu16.04

ubuntu14.04: runtime-ubuntu14.04 toolkit-ubuntu14.04

debian9: runtime-debian9 toolkit-debian9

debian8: runtime-debian8 toolkit-debian8

centos7: runtime-centos7 toolkit-centos7

amzn2: runtime-amzn2 toolkit-amzn2

amzn1: runtime-amzn1 toolkit-amzn1

base:
	make -C $(CURDIR)/base BASE=${BASE}

base-%:
	make -C $(CURDIR)/base BASE=${BASE} $*

toolkit-%: 
	make -C $(CURDIR)/toolkit BASE=${BASE} $*

runtime-%: 
	make -C $(CURDIR)/runtime BASE=${BASE} $*

# Build nvidia-container-runtime for specific versions of docker.
%-runtime-ubuntu18.04: base-ubuntu18.04
	make -C $(CURDIR)/runtime $*-ubuntu18.04

%-runtime-ubuntu16.04: base-ubuntu16.04
	make -C $(CURDIR)/runtime $*-ubuntu16.04

%-runtime-ubuntu14.04: base-ubuntu14.04
	make -C $(CURDIR)/runtime $*-ubuntu14.04

%-runtime-debian9: base-debian9
	make -C $(CURDIR)/runtime $*-debian9

%-runtime-debian8: base-debian8
	make -C $(CURDIR)/runtime $*-debian8

%-runtime-centos7: base-centos7
	make -C $(CURDIR)/runtime $*-centos7

%-runtime-amzn2: base-amzn2
	make -C $(CURDIR)/runtime $*-amzn2

%-runtime-amzn1: base-amzn1
	make -C $(CURDIR)/runtime $*-amzn1
