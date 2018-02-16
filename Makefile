# Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.

DOCKER ?= docker

VERSION := 1.1.1
PKG_REV := 1

DIST_DIR  := $(CURDIR)/dist

.NOTPARALLEL:
.PHONY: all

all: xenial centos7 stretch

xenial: 17.12.0-xenial 17.09.1-xenial 17.09.0-xenial 17.06.2-xenial 17.03.2-xenial 1.13.1-xenial 1.12.6-xenial

centos7: 17.12.0-centos7 17.09.1-centos7 17.09.0-centos7 17.06.2-centos7 17.03.2-centos7 1.13.1-centos7 1.12.6-centos7

stretch: 17.12.0-stretch 17.09.1-stretch 17.09.0-stretch 17.06.2-stretch 17.03.2-stretch

17.12.0-xenial:
	$(DOCKER) build --build-arg RUNC_COMMIT="b2567b37d7b75eb4cf325b77297b140ea686ce8f" \
                        --build-arg PKG_VERS="$(VERSION)+docker17.12.0" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.xenial .
	$(DOCKER) run --rm -v $(DIST_DIR)/xenial:/dist:Z nvidia-container-runtime:$@

17.09.1-xenial:
	$(DOCKER) build --build-arg RUNC_COMMIT="3f2f8b84a77f73d38244dd690525642a72156c64" \
                        --build-arg PKG_VERS="$(VERSION)+docker17.09.1" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.xenial .
	$(DOCKER) run --rm -v $(DIST_DIR)/xenial:/dist:Z nvidia-container-runtime:$@

17.09.0-xenial:
	$(DOCKER) build --build-arg RUNC_COMMIT="3f2f8b84a77f73d38244dd690525642a72156c64" \
                        --build-arg PKG_VERS="$(VERSION)+docker17.09.0" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.xenial .
	$(DOCKER) run --rm -v $(DIST_DIR)/xenial:/dist:Z nvidia-container-runtime:$@

17.06.2-xenial:
	$(DOCKER) build --build-arg RUNC_COMMIT="810190ceaa507aa2727d7ae6f4790c76ec150bd2" \
                        --build-arg PKG_VERS="$(VERSION)+docker17.06.2" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.xenial .
	$(DOCKER) run --rm -v $(DIST_DIR)/xenial:/dist:Z nvidia-container-runtime:$@

17.03.2-xenial:
	$(DOCKER) build --build-arg RUNC_COMMIT="54296cf40ad8143b62dbcaa1d90e520a2136ddfe" \
                        --build-arg PKG_VERS="$(VERSION)+docker17.03.2" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.xenial .
	$(DOCKER) run --rm -v $(DIST_DIR)/xenial:/dist:Z nvidia-container-runtime:$@

1.13.1-xenial:
	$(DOCKER) build --build-arg RUNC_COMMIT="9df8b306d01f59d3a8029be411de015b7304dd8f" \
                        --build-arg PKG_VERS="$(VERSION)+docker1.13.1" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.xenial .
	$(DOCKER) run --rm -v $(DIST_DIR)/xenial:/dist:Z nvidia-container-runtime:$@

1.12.6-xenial:
	$(DOCKER) build --build-arg RUNC_COMMIT="50a19c6ff828c58e5dab13830bd3dacde268afe5" \
                        --build-arg PKG_VERS="$(VERSION)+docker1.12.6" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.xenial .
	$(DOCKER) run --rm -v $(DIST_DIR)/xenial:/dist:Z nvidia-container-runtime:$@

17.12.0-centos7:
	$(DOCKER) build --build-arg RUNC_COMMIT="b2567b37d7b75eb4cf325b77297b140ea686ce8f" \
                        --build-arg PKG_VERS="$(VERSION)" \
                        --build-arg PKG_REV="$(PKG_REV).docker17.12.0" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.centos7 .
	$(DOCKER) run --rm -v $(DIST_DIR)/centos7:/dist:Z nvidia-container-runtime:$@

17.09.1-centos7:
	$(DOCKER) build --build-arg RUNC_COMMIT="3f2f8b84a77f73d38244dd690525642a72156c64" \
                        --build-arg PKG_VERS="$(VERSION)" \
                        --build-arg PKG_REV="$(PKG_REV).docker17.09.1" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.centos7 .
	$(DOCKER) run --rm -v $(DIST_DIR)/centos7:/dist:Z nvidia-container-runtime:$@

17.09.0-centos7:
	$(DOCKER) build --build-arg RUNC_COMMIT="3f2f8b84a77f73d38244dd690525642a72156c64" \
                        --build-arg PKG_VERS="$(VERSION)" \
                        --build-arg PKG_REV="$(PKG_REV).docker17.09.0" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.centos7 .
	$(DOCKER) run --rm -v $(DIST_DIR)/centos7:/dist:Z nvidia-container-runtime:$@

17.06.2-centos7:
	$(DOCKER) build --build-arg RUNC_COMMIT="810190ceaa507aa2727d7ae6f4790c76ec150bd2" \
                        --build-arg PKG_VERS="$(VERSION)" \
                        --build-arg PKG_REV="$(PKG_REV).docker17.06.2" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.centos7 .
	$(DOCKER) run --rm -v $(DIST_DIR)/centos7:/dist:Z nvidia-container-runtime:$@

17.03.2-centos7:
	$(DOCKER) build --build-arg RUNC_COMMIT="54296cf40ad8143b62dbcaa1d90e520a2136ddfe" \
                        --build-arg PKG_VERS="$(VERSION)" \
                        --build-arg PKG_REV="$(PKG_REV).docker17.03.2" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.centos7 .
	$(DOCKER) run --rm -v $(DIST_DIR)/centos7:/dist:Z nvidia-container-runtime:$@

1.13.1-centos7:
	$(DOCKER) build --build-arg RUNC_COMMIT="9df8b306d01f59d3a8029be411de015b7304dd8f" \
                        --build-arg PKG_VERS="$(VERSION)" \
                        --build-arg PKG_REV="$(PKG_REV).docker1.13.1" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.centos7 .
	$(DOCKER) run --rm -v $(DIST_DIR)/centos7:/dist:Z nvidia-container-runtime:$@

1.12.6-centos7:
	$(DOCKER) build --build-arg RUNC_COMMIT="50a19c6ff828c58e5dab13830bd3dacde268afe5" \
                        --build-arg PKG_VERS="$(VERSION)" \
                        --build-arg PKG_REV="$(PKG_REV).docker1.12.6" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.centos7 .
	$(DOCKER) run --rm -v $(DIST_DIR)/centos7:/dist:Z nvidia-container-runtime:$@

17.12.0-stretch:
	$(DOCKER) build --build-arg RUNC_COMMIT="b2567b37d7b75eb4cf325b77297b140ea686ce8f" \
                        --build-arg PKG_VERS="$(VERSION)+docker17.12.0" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.stretch .
	$(DOCKER) run --rm -v $(DIST_DIR)/stretch:/dist:Z nvidia-container-runtime:$@

17.09.1-stretch:
	$(DOCKER) build --build-arg RUNC_COMMIT="3f2f8b84a77f73d38244dd690525642a72156c64" \
                        --build-arg PKG_VERS="$(VERSION)+docker17.09.1" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.stretch .
	$(DOCKER) run --rm -v $(DIST_DIR)/stretch:/dist:Z nvidia-container-runtime:$@

17.09.0-stretch:
	$(DOCKER) build --build-arg RUNC_COMMIT="3f2f8b84a77f73d38244dd690525642a72156c64" \
                        --build-arg PKG_VERS="$(VERSION)+docker17.09.0" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.stretch .
	$(DOCKER) run --rm -v $(DIST_DIR)/stretch:/dist:Z nvidia-container-runtime:$@

17.06.2-stretch:
	$(DOCKER) build --build-arg RUNC_COMMIT="810190ceaa507aa2727d7ae6f4790c76ec150bd2" \
                        --build-arg PKG_VERS="$(VERSION)+docker17.06.2" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.stretch .
	$(DOCKER) run --rm -v $(DIST_DIR)/stretch:/dist:Z nvidia-container-runtime:$@

17.03.2-stretch:
	$(DOCKER) build --build-arg RUNC_COMMIT="54296cf40ad8143b62dbcaa1d90e520a2136ddfe" \
                        --build-arg PKG_VERS="$(VERSION)+docker17.03.2" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t nvidia-container-runtime:$@ -f Dockerfile.stretch .
	$(DOCKER) run --rm -v $(DIST_DIR)/stretch:/dist:Z nvidia-container-runtime:$@
