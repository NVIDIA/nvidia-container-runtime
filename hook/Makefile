# Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.

DOCKER ?= docker

VERSION := 1.4.0

DIST_DIR  := $(CURDIR)/../dist

.NOTPARALLEL:
.PHONY: all

all: ubuntu18.04 ubuntu16.04 ubuntu14.04 debian9 debian8 centos7 amzn2 amzn1

ubuntu%: $(CURDIR)/Dockerfile.ubuntu
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg PKG_VERS="$(VERSION)" \
	                --build-arg PKG_REV="1" \
	                -t "nvidia/hook/ubuntu:$*" -f Dockerfile.ubuntu .
	$(DOCKER) run --rm -v $(DIST_DIR)/$@:/dist:Z "nvidia/hook/ubuntu:$*"

debian%: $(CURDIR)/Dockerfile.debian
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg PKG_VERS="$(VERSION)" \
	                --build-arg PKG_REV="1" \
	                -t "nvidia/hook/debian:$*" -f Dockerfile.debian .
	$(DOCKER) run --rm -v $(DIST_DIR)/$@:/dist:Z "nvidia/hook/debian:$*"

centos%: $(CURDIR)/Dockerfile.centos
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg PKG_VERS="$(VERSION)" \
	                --build-arg PKG_REV="2" \
	                -t "nvidia/hook/centos:$*" -f Dockerfile.centos .
	$(DOCKER) run --rm -v $(DIST_DIR)/$@:/dist:Z "nvidia/hook/centos:$*"

amzn%: $(CURDIR)/Dockerfile.amzn
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg PKG_VERS="$(VERSION)" \
	                --build-arg PKG_REV="2.amzn$*" \
	                -t "nvidia/hook/amzn:$*" -f Dockerfile.amzn .
	$(DOCKER) run --rm -v $(DIST_DIR)/$@:/dist:Z "nvidia/hook/amzn:$*"
