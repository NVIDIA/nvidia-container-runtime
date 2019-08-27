# Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.

DOCKER ?= docker

MKDIR ?= mkdir

VERSION := 1.0.3

DIST_DIR  := $(CURDIR)/../dist

BASE := nvidia/base

.NOTPARALLEL:
.PHONY: all

all: ubuntu18.04 ubuntu16.04 debian10 debian9 centos7 amzn2 amzn1 opensuse-leap15.1

ubuntu%: ARCH := amd64
ubuntu%: $(CURDIR)/Dockerfile.ubuntu
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg PKG_VERS="$(VERSION)" \
	                --build-arg PKG_REV="1" \
			--build-arg BASE="$(BASE)" \
	                -t "nvidia/toolkit/ubuntu:$*" -f Dockerfile.ubuntu .
	$(MKDIR) -p $(DIST_DIR)/$@/$(ARCH)
	$(DOCKER) run --cidfile $@.cid "nvidia/toolkit/ubuntu:$*"
	$(DOCKER) cp $$(cat $@.cid):/dist/. $(DIST_DIR)/$@/$(ARCH)/
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

debian%: ARCH := amd64
debian%: $(CURDIR)/Dockerfile.debian
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg PKG_VERS="$(VERSION)" \
	                --build-arg PKG_REV="1" \
			--build-arg BASE="$(BASE)" \
	                -t "nvidia/toolkit/debian:$*" -f Dockerfile.debian .
	$(MKDIR) -p $(DIST_DIR)/$@/$(ARCH)
	$(DOCKER) run --cidfile $@.cid "nvidia/toolkit/debian:$*"
	$(DOCKER) cp $$(cat $@.cid):/dist/. $(DIST_DIR)/$@/$(ARCH)/
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

centos%: ARCH := x86_64
centos%: $(CURDIR)/Dockerfile.centos
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg PKG_VERS="$(VERSION)" \
	                --build-arg PKG_REV="2" \
			--build-arg BASE="$(BASE)" \
	                -t "nvidia/toolkit/centos:$*" -f Dockerfile.centos .
	$(MKDIR) -p $(DIST_DIR)/$@/$(ARCH)
	$(DOCKER) run --cidfile $@.cid "nvidia/toolkit/centos:$*"
	$(DOCKER) cp $$(cat $@.cid):/dist/. $(DIST_DIR)/$@/$(ARCH)/
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

amzn%: ARCH := x86_64
amzn%: $(CURDIR)/Dockerfile.amzn
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg PKG_VERS="$(VERSION)" \
	                --build-arg PKG_REV="2.amzn$*" \
			--build-arg BASE="$(BASE)" \
	                -t "nvidia/toolkit/amzn:$*" -f Dockerfile.amzn .
	$(MKDIR) -p $(DIST_DIR)/$@/$(ARCH)
	$(DOCKER) run --cidfile $@.cid "nvidia/toolkit/amzn:$*"
	$(DOCKER) cp $$(cat $@.cid):/dist/. $(DIST_DIR)/$@/$(ARCH)/
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

opensuse-leap%: ARCH := x86_64
opensuse-leap%: $(CURDIR)/Dockerfile.opensuse-leap
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg PKG_VERS="$(VERSION)" \
	                --build-arg PKG_REV="1" \
	                --build-arg BASE="$(BASE)" \
	                -t "nvidia/toolkit/opensuse-leap:$*" -f Dockerfile.opensuse-leap .
	$(MKDIR) -p $(DIST_DIR)/$@/$(ARCH)
	$(DOCKER) run --cidfile $@.cid "nvidia/toolkit/opensuse-leap:$*"
	$(DOCKER) cp $$(cat $@.cid):/dist/. $(DIST_DIR)/$@/$(ARCH)/
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid
