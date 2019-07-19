# Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.

DOCKER ?= docker

MKDIR  ?= mkdir

VERSION := 1.4.0

DIST_DIR  := $(CURDIR)/../dist

.NOTPARALLEL:
.PHONY: all

all: ubuntu18.04 ubuntu16.04 ubuntu14.04 debian9 debian8 centos7 amzn2 amzn1

ubuntu%: ARCH := amd64
ubuntu%: $(CURDIR)/Dockerfile.ubuntu
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg PKG_VERS="$(VERSION)" \
	                --build-arg PKG_REV="1" \
	                -t "nvidia/hook/ubuntu:$*" -f Dockerfile.ubuntu .
	$(MKDIR) -p $(DIST_DIR)/$@/$(ARCH)
	$(DOCKER) run --cidfile $@.cid "nvidia/hook/ubuntu:$*"
	$(DOCKER) cp $$(cat $@.cid):/dist/. $(DIST_DIR)/$@/$(ARCH)/
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

debian%: ARCH := amd64
debian%: $(CURDIR)/Dockerfile.debian
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg PKG_VERS="$(VERSION)" \
	                --build-arg PKG_REV="1" \
	                -t "nvidia/hook/debian:$*" -f Dockerfile.debian .
	$(MKDIR) -p $(DIST_DIR)/$@/$(ARCH)
	$(DOCKER) run --cidfile $@.cid "nvidia/hook/debian:$*"
	$(DOCKER) cp $$(cat $@.cid):/dist/. $(DIST_DIR)/$@/$(ARCH)/
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

centos%: ARCH := x86_64
centos%: $(CURDIR)/Dockerfile.centos
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg PKG_VERS="$(VERSION)" \
	                --build-arg PKG_REV="2" \
	                -t "nvidia/hook/centos:$*" -f Dockerfile.centos .
	$(MKDIR) -p $(DIST_DIR)/$@/$(ARCH)
	$(DOCKER) run --cidfile $@.cid "nvidia/hook/centos:$*"
	$(DOCKER) cp $$(cat $@.cid):/dist/. $(DIST_DIR)/$@/$(ARCH)/
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

amzn%: ARCH := x86_64
amzn%: $(CURDIR)/Dockerfile.amzn
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg PKG_VERS="$(VERSION)" \
	                --build-arg PKG_REV="2.amzn$*" \
	                -t "nvidia/hook/amzn:$*" -f Dockerfile.amzn .
	$(MKDIR) -p $(DIST_DIR)/$@/$(ARCH)
	$(DOCKER) run --cidfile $@.cid "nvidia/hook/amzn:$*"
	$(DOCKER) cp $$(cat $@.cid):/dist/. $(DIST_DIR)/$@/$(ARCH)/
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid
