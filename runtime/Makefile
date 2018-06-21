# Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.

DOCKER ?= docker

VERSION := 2.0.0
PKG_REV := 1

DIST_DIR  := $(CURDIR)/../dist

.NOTPARALLEL:
.PHONY: all

all: ubuntu18.04 ubuntu16.04 ubuntu14.04 debian9 debian8 centos7 amzn2 amzn1

ubuntu18.04: $(addsuffix -ubuntu18.04, 18.03.1 17.12.1)

ubuntu16.04: $(addsuffix -ubuntu16.04, 18.03.1 18.03.0 17.12.1 17.12.0 17.09.1 17.09.0 17.06.2 17.03.2 1.13.1 1.12.6)

ubuntu14.04: $(addsuffix -ubuntu14.04, 18.03.1 18.03.0 17.12.1 17.09.1 17.06.2 17.03.2)

debian9: $(addsuffix -debian9, 18.03.1 18.03.0 17.12.1 17.12.0 17.09.1 17.09.0 17.06.2 17.03.2)

debian8: $(addsuffix -debian8, 18.03.1 18.03.0 17.12.1 17.09.1 17.06.2)

centos7: $(addsuffix -centos7, 18.03.1 18.03.0 17.12.1 17.12.0 17.09.1 17.09.0 17.06.2 17.03.2 1.13.1 1.12.6)

amzn2: $(addsuffix -amzn2, 17.06.2)

amzn1: $(addsuffix -amzn1, 18.03.1 17.12.1 17.09.1 17.06.2 17.03.2)

18.03.1-%-runc:
	echo "4fc53a81fb7c994640722ac585fa9ca548971871"

18.03.0-%-runc:
	echo "4fc53a81fb7c994640722ac585fa9ca548971871"

17.12.1-%-runc:
	echo "9f9c96235cc97674e935002fc3d78361b696a69e"

17.12.0-%-runc:
	echo "b2567b37d7b75eb4cf325b77297b140ea686ce8f"

17.09.1-%-runc 17.09.0-%-runc:
	echo "3f2f8b84a77f73d38244dd690525642a72156c64"

17.06.2-%-runc:
	echo "810190ceaa507aa2727d7ae6f4790c76ec150bd2"

17.03.2-%-runc:
	echo "54296cf40ad8143b62dbcaa1d90e520a2136ddfe"

1.13.1-%-runc:
	echo "9df8b306d01f59d3a8029be411de015b7304dd8f"

1.12.6-%-runc:
	echo "50a19c6ff828c58e5dab13830bd3dacde268afe5"

%-ubuntu18.04:
	runc="$(shell $(MAKE) -s $@-runc)" && \
	$(DOCKER) build --build-arg VERSION_ID="18.04" \
                        --build-arg RUNC_COMMIT="$${runc}" \
                        --build-arg PKG_VERS="$(VERSION)+docker$*" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t "nvidia/runtime/ubuntu:18.04-docker$*" -f Dockerfile.ubuntu .
	$(DOCKER) run --rm -v $(DIST_DIR)/ubuntu18.04:/dist:Z "nvidia/runtime/ubuntu:18.04-docker$*"

%-ubuntu16.04:
	runc="$(shell $(MAKE) -s $@-runc)" && \
	$(DOCKER) build --build-arg VERSION_ID="16.04" \
                        --build-arg RUNC_COMMIT="$${runc}" \
                        --build-arg PKG_VERS="$(VERSION)+docker$*" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t "nvidia/runtime/ubuntu:16.04-docker$*" -f Dockerfile.ubuntu .
	$(DOCKER) run --rm -v $(DIST_DIR)/ubuntu16.04:/dist:Z "nvidia/runtime/ubuntu:16.04-docker$*"

%-ubuntu14.04:
	runc="$(shell $(MAKE) -s $@-runc)" && \
	$(DOCKER) build --build-arg VERSION_ID="14.04" \
                        --build-arg RUNC_COMMIT="$${runc}" \
                        --build-arg PKG_VERS="$(VERSION)+docker$*" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t "nvidia/runtime/ubuntu:14.04-docker$*" -f Dockerfile.ubuntu .
	$(DOCKER) run --rm -v $(DIST_DIR)/ubuntu14.04:/dist:Z "nvidia/runtime/ubuntu:14.04-docker$*"

%-debian9:
	runc="$(shell $(MAKE) -s $@-runc)" && \
	$(DOCKER) build --build-arg VERSION_ID="9" \
                        --build-arg RUNC_COMMIT="$${runc}" \
                        --build-arg PKG_VERS="$(VERSION)+docker$*" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t "nvidia/runtime/debian:9-docker$*" -f Dockerfile.debian .
	$(DOCKER) run --rm -v $(DIST_DIR)/debian9:/dist:Z "nvidia/runtime/debian:9-docker$*"

%-debian8:
	runc="$(shell $(MAKE) -s $@-runc)" && \
	$(DOCKER) build --build-arg VERSION_ID="8" \
                        --build-arg RUNC_COMMIT="$${runc}" \
                        --build-arg PKG_VERS="$(VERSION)+docker$*" \
                        --build-arg PKG_REV="$(PKG_REV)" \
                        -t "nvidia/runtime/debian:8-docker$*" -f Dockerfile.debian .
	$(DOCKER) run --rm -v $(DIST_DIR)/debian8:/dist:Z "nvidia/runtime/debian:8-docker$*"

%-centos7:
	runc="$(shell $(MAKE) -s $@-runc)" && \
	$(DOCKER) build --build-arg VERSION_ID="7" \
                        --build-arg RUNC_COMMIT="$${runc}" \
                        --build-arg PKG_VERS="$(VERSION)" \
                        --build-arg PKG_REV="$(PKG_REV).docker$*" \
                        -t "nvidia/runtime/centos:7-docker$*" -f Dockerfile.centos .
	$(DOCKER) run --rm -v $(DIST_DIR)/centos7:/dist:Z "nvidia/runtime/centos:7-docker$*"

%-amzn2:
	runc="$(shell $(MAKE) -s $@-runc)" && \
	$(DOCKER) build --build-arg VERSION_ID="2" \
                        --build-arg RUNC_COMMIT="$${runc}" \
                        --build-arg PKG_VERS="$(VERSION)" \
                        --build-arg PKG_REV="$(PKG_REV).docker$*.amzn2" \
                        -t "nvidia/runtime/amzn:2-docker$*" -f Dockerfile.amzn .
	$(DOCKER) run --rm -v $(DIST_DIR)/amzn2:/dist:Z "nvidia/runtime/amzn:2-docker$*"

%-amzn1:
	runc="$(shell $(MAKE) -s $@-runc)" && \
	$(DOCKER) build --build-arg VERSION_ID="1" \
                        --build-arg RUNC_COMMIT="$${runc}" \
                        --build-arg PKG_VERS="$(VERSION)" \
                        --build-arg PKG_REV="$(PKG_REV).docker$*.amzn1" \
                        -t "nvidia/runtime/amzn:1-docker$*" -f Dockerfile.amzn .
	$(DOCKER) run --rm -v $(DIST_DIR)/amzn1:/dist:Z "nvidia/runtime/amzn:1-docker$*"
