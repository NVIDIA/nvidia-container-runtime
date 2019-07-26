# Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.

DOCKER ?= docker

GOLANG_VERSION := 1.10.3

BASE := nvidia/base

.NOTPARALLEL:
.PHONY: all

all: ubuntu18.04 ubuntu16.04 ubuntu14.04 debian9 debian8 centos7 amzn2 amzn1

# Build and push base images to gitlab registry. Used in CI
base-push: all
	for i in $(shell $(DOCKER) images --format "{{.Repository}}:{{.Tag}}" | grep "${BASE}"); do \
		$(DOCKER) push $$i; \
	done

ubuntu%: $(CURDIR)/Dockerfile.ubuntu
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg GOLANG_VERSION="$(GOLANG_VERSION)" \
	                -t "$(BASE)/ubuntu:$*" -f Dockerfile.ubuntu .

debian%: $(CURDIR)/Dockerfile.debian
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg GOLANG_VERSION="$(GOLANG_VERSION)" \
	                -t "$(BASE)/debian:$*" -f Dockerfile.debian .

centos%: $(CURDIR)/Dockerfile.centos
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg GOLANG_VERSION="$(GOLANG_VERSION)" \
	                -t "$(BASE)/centos:$*" -f Dockerfile.centos .

amzn%: $(CURDIR)/Dockerfile.amzn
	$(DOCKER) build --build-arg VERSION_ID="$*" \
	                --build-arg GOLANG_VERSION="$(GOLANG_VERSION)" \
	                -t "$(BASE)/amzn:$*" -f Dockerfile.amzn .
