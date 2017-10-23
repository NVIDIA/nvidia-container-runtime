# Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.

DOCKER ?= docker

VERSION := 1.0.0
PKG_REV := 1

DIST_DIR  := $(CURDIR)/dist

.NOTPARALLEL:
.PHONY: all

all: xenial centos7

xenial: 17.09.0-xenial 17.06.2-xenial 17.06.1-xenial 17.03.2-xenial 1.13.1-xenial 1.12.6-xenial

centos7: 17.09.0-centos7 17.06.2-centos7 17.06.1-centos7 17.03.2-centos7 1.12.6-centos7

ifeq ($(shell uname -p),ppc64le)
ARCH    ?= ppc64le
PKG_ARCH ?= ppc64le
else
ARCH    ?= amd64
PKG_ARCH ?= x86_64
endif

17.09.0-xenial:
	$(DOCKER) build --build-arg RUNC_COMMIT="3f2f8b84a77f73d38244dd690525642a72156c64" \
                        --build-arg PKG_VERS="$(VERSION)+docker17.09.0" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        --build-arg GO_ARCH="$(ARCH)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.xenial .
	$(DOCKER) run --rm -v $(DIST_DIR)/xenial:/dist:Z nvidia-container-runtime:$@

17.06.2-xenial:
	$(DOCKER) build --build-arg RUNC_COMMIT="810190ceaa507aa2727d7ae6f4790c76ec150bd2" \
                        --build-arg PKG_VERS="$(VERSION)+docker17.06.2" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        --build-arg GO_ARCH="$(ARCH)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.xenial .
	$(DOCKER) run --rm -v $(DIST_DIR)/xenial:/dist:Z nvidia-container-runtime:$@

17.06.1-xenial:
	$(DOCKER) build --build-arg RUNC_COMMIT="810190ceaa507aa2727d7ae6f4790c76ec150bd2" \
                        --build-arg PKG_VERS="$(VERSION)+docker17.06.1" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        --build-arg GO_ARCH="$(ARCH)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.xenial .
	$(DOCKER) run --rm -v $(DIST_DIR)/xenial:/dist:Z nvidia-container-runtime:$@

17.03.2-xenial:
	$(DOCKER) build --build-arg RUNC_COMMIT="54296cf40ad8143b62dbcaa1d90e520a2136ddfe" \
                        --build-arg PKG_VERS="$(VERSION)+docker17.03.2" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        --build-arg GO_ARCH="$(ARCH)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.xenial .
	$(DOCKER) run --rm -v $(DIST_DIR)/xenial:/dist:Z nvidia-container-runtime:$@

1.13.1-xenial:
	$(DOCKER) build --build-arg RUNC_COMMIT="9df8b306d01f59d3a8029be411de015b7304dd8f" \
                        --build-arg PKG_VERS="$(VERSION)+docker1.13.1" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        --build-arg GO_ARCH="$(ARCH)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.xenial .
	$(DOCKER) run --rm -v $(DIST_DIR)/xenial:/dist:Z nvidia-container-runtime:$@

1.12.6-xenial:
	$(DOCKER) build --build-arg RUNC_COMMIT="50a19c6ff828c58e5dab13830bd3dacde268afe5" \
                        --build-arg PKG_VERS="$(VERSION)+docker1.12.6" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        --build-arg GO_ARCH="$(ARCH)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.xenial .
	$(DOCKER) run --rm -v $(DIST_DIR)/xenial:/dist:Z nvidia-container-runtime:$@

17.09.0-centos7:
	$(DOCKER) build --build-arg PKG_ARCH="$(PKG_ARCH)" \
                        --build-arg RUNC_COMMIT="3f2f8b84a77f73d38244dd690525642a72156c64" \
                        --build-arg PKG_VERS="$(VERSION)" \
                        --build-arg PKG_REV="$(PKG_REV).docker17.09.0" \
                        --build-arg GO_ARCH="$(ARCH)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.centos7 .
	$(DOCKER) run --rm -v $(DIST_DIR)/centos7:/dist:Z nvidia-container-runtime:$@

17.06.2-centos7:
	$(DOCKER) build --build-arg PKG_ARCH="$(PKG_ARCH)" \
                        --build-arg RUNC_COMMIT="810190ceaa507aa2727d7ae6f4790c76ec150bd2" \
                        --build-arg PKG_VERS="$(VERSION)" \
                        --build-arg PKG_REV="$(PKG_REV).docker17.06.2" \
                        --build-arg GO_ARCH="$(ARCH)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.centos7 .
	$(DOCKER) run --rm -v $(DIST_DIR)/centos7:/dist:Z nvidia-container-runtime:$@

17.06.1-centos7:
	$(DOCKER) build --build-arg PKG_ARCH="$(PKG_ARCH)" \
                        --build-arg RUNC_COMMIT="810190ceaa507aa2727d7ae6f4790c76ec150bd2" \
                        --build-arg PKG_VERS="$(VERSION)" \
                        --build-arg PKG_REV="$(PKG_REV).docker17.06.1" \
                        --build-arg GO_ARCH="$(ARCH)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.centos7 .
	$(DOCKER) run --rm -v $(DIST_DIR)/centos7:/dist:Z nvidia-container-runtime:$@

17.03.2-centos7:
	$(DOCKER) build --build-arg PKG_ARCH="$(PKG_ARCH)" \
                        --build-arg RUNC_COMMIT="54296cf40ad8143b62dbcaa1d90e520a2136ddfe" \
                        --build-arg PKG_VERS="$(VERSION)" \
                        --build-arg PKG_REV="$(PKG_REV).docker17.03.2" \
                        --build-arg GO_ARCH="$(ARCH)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.centos7 .
	$(DOCKER) run --rm -v $(DIST_DIR)/centos7:/dist:Z nvidia-container-runtime:$@

1.12.6-centos7:
	$(DOCKER) build --build-arg PKG_ARCH="$(PKG_ARCH)" \
                        --build-arg RUNC_COMMIT="50a19c6ff828c58e5dab13830bd3dacde268afe5" \
                        --build-arg PKG_VERS="$(VERSION)" \
                        --build-arg PKG_REV="$(PKG_REV).docker1.12.6" \
                        --build-arg GO_ARCH="$(ARCH)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.centos7 .
	$(DOCKER) run --rm -v $(DIST_DIR)/centos7:/dist:Z nvidia-container-runtime:$@
