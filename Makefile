# Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.

DOCKER ?= docker
MKDIR  ?= mkdir

VERSION := 3.1.4
PKG_REV := 1

TOOLKIT_VERSION := 1.0.5
GOLANG_VERSION  := 1.10.3

DIST_DIR := $(CURDIR)/dist
REGISTRY := nvidia

.NOTPARALLEL:
.PHONY: all

all: ubuntu18.04 ubuntu16.04 debian10 debian9 centos7 amzn2 amzn1 opensuse-leap15.1

push%:
	$(DOCKER) push "$(REGISTRY)/runtime/$*"

ubuntu%: ARCH := amd64
ubuntu%:
	$(DOCKER) build --pull \
			--build-arg VERSION_ID="$*" \
			--build-arg GOLANG_VERSION="$(GOLANG_VERSION)" \
			--build-arg PKG_VERS="$(VERSION)" \
			--build-arg PKG_REV="$(PKG_REV)" \
			--build-arg TOOLKIT_VERSION="$(TOOLKIT_VERSION)" \
			--cache-from "$(REGISTRY)/runtime/ubuntu$*" \
			--tag "$(REGISTRY)/runtime/ubuntu$*" \
			--file docker/Dockerfile.ubuntu .
	$(MKDIR) -p "$(DIST_DIR)/ubuntu$*/$(ARCH)"
	$(DOCKER) run --cidfile $@.cid "$(REGISTRY)/runtime/ubuntu$*"
	$(DOCKER) cp $$(cat $@.cid):/dist/. "$(DIST_DIR)/ubuntu$*/$(ARCH)/"
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

debian%: ARCH := amd64
debian%:
	$(DOCKER) build --pull \
			--build-arg VERSION_ID="$*" \
			--build-arg GOLANG_VERSION="$(GOLANG_VERSION)" \
			--build-arg PKG_VERS="$(VERSION)" \
			--build-arg PKG_REV="$(PKG_REV)" \
			--build-arg TOOLKIT_VERSION="$(TOOLKIT_VERSION)" \
			--cache-from "$(REGISTRY)/runtime/debian$*" \
			--tag "$(REGISTRY)/runtime/debian$*" \
			--file docker/Dockerfile.debian .
	$(MKDIR) -p "$(DIST_DIR)/debian$*/$(ARCH)"
	$(DOCKER) run --cidfile $@.cid "$(REGISTRY)/runtime/debian$*"
	$(DOCKER) cp $$(cat $@.cid):/dist/. "$(DIST_DIR)/debian$*/$(ARCH)/"
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

centos%: ARCH := x86_64
centos%:
	$(DOCKER) build --pull \
			--build-arg VERSION_ID="$*" \
			--build-arg GOLANG_VERSION="$(GOLANG_VERSION)" \
			--build-arg PKG_VERS="$(VERSION)" \
			--build-arg PKG_REV="$(PKG_REV)" \
			--build-arg TOOLKIT_VERSION="$(TOOLKIT_VERSION)" \
			--cache-from "$(REGISTRY)/runtime/centos$*" \
			--tag "$(REGISTRY)/runtime/centos$*" \
			--file docker/Dockerfile.centos .
	$(MKDIR) -p "$(DIST_DIR)/centos$*/$(ARCH)"
	$(DOCKER) run --cidfile $@.cid "$(REGISTRY)/runtime/centos$*"
	$(DOCKER) cp $$(cat $@.cid):/dist/. "$(DIST_DIR)/centos$*/$(ARCH)/"
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

amzn%: ARCH := x86_64
amzn%:
	$(DOCKER) build --pull \
			--build-arg VERSION_ID="$*" \
			--build-arg GOLANG_VERSION="$(GOLANG_VERSION)" \
			--build-arg PKG_VERS="$(VERSION)" \
			--build-arg PKG_REV="$(PKG_REV)" \
			--build-arg TOOLKIT_VERSION="$(TOOLKIT_VERSION)" \
			--cache-from "$(REGISTRY)/runtime/amzn$*" \
			--tag "$(REGISTRY)/runtime/amzn$*" \
			--file docker/Dockerfile.amzn .
	$(MKDIR) -p "$(DIST_DIR)/amzn$*/$(ARCH)"
	$(DOCKER) run --cidfile $@.cid "$(REGISTRY)/runtime/amzn$*"
	$(DOCKER) cp $$(cat $@.cid):/dist/. "$(DIST_DIR)/amzn$*/$(ARCH)/"
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid

opensuse-leap%: ARCH := x86_64
opensuse-leap%:
	$(DOCKER) build --pull \
			--build-arg VERSION_ID="$*" \
			--build-arg GOLANG_VERSION="$(GOLANG_VERSION)" \
			--build-arg PKG_VERS="$(VERSION)" \
			--build-arg PKG_REV="$(PKG_REV)" \
			--build-arg TOOLKIT_VERSION="$(TOOLKIT_VERSION)" \
			--cache-from "$(REGISTRY)/runtime/opensuse-leap$*" \
			--tag "$(REGISTRY)/runtime/opensuse-leap$*" \
			--file docker/Dockerfile.opensuse-leap .
	$(MKDIR) -p $(DIST_DIR)/opensuse-leap$*/$(ARCH)
	$(DOCKER) run --cidfile $@.cid "$(REGISTRY)/runtime/opensuse-leap$*"
	$(DOCKER) cp $$(cat $@.cid):/dist/. $(DIST_DIR)/opensuse-leap$*/$(ARCH)/
	$(DOCKER) rm $$(cat $@.cid) && rm $@.cid
